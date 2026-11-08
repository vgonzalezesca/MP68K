/*
 * codec_wma — WMA v1 y v2 con libwma de Rockbox (derivada del decodificador WMA de FFmpeg; LGPL 2.1 o posterior y GPL 2 o
 * posterior), en punto fijo y sin malloc: libwma usa arreglos estáticos y el único buffer dinámico es el del paquete ASF
 * (del arena del códec). El único float de libwma (pow_m1_4) se reemplaza por enteros: third_party/patches/libwma-mp368k.patch.
 *
 * El contenedor se lee con src/asfparse.c. El reconocimiento, las etiquetas y la duración (probe) salen solo de la cabecera.
 * Límites declarados: WMA v1 y v2 (no Pro, Voice, Lossless ni DRM), hasta 2 canales y de 8 a 48 kHz.
 */
#include "codec.h"

#include <string.h>

#include "asfparse.h"
#include "pcm_kernels.h"
#include "wmadec.h"

#define MIN_RATE 8000
#define MAX_RATE 48000
#define MAX_ERRORS 8                /* errores de decodificación seguidos antes de rendirse */
#define OUT_SHIFT 14                /* salida de libwma (29 bits con signo) -> 16 bits */
/*
 * Dos particularidades medidas contra ffmpeg: la salida de libwma tiene la polaridad invertida (inaudible; se corrige para
 * que coincida con el decodificador de referencia), y los primeros 2 cuadros de MDCT (2 * frame_len muestras) son el arranque
 * del decodificador, que ffmpeg descarta.
 */
#define DELAY_FRAMES 2

static WMADecodeContext sDec;
static AsfInfo sInfo;
static AsfReader sRd;
static CodecIO sIo;
static int sOpen;
static int16_t sPcm[CODEC_MAX_FRAMES * 2];
static unsigned sCh, sRate;

/* superframe en curso */
static const uint8_t *sObj;         /* objeto de medios actual (puede traer varios superframes) */
static long sObjLen, sObjOff;
static const uint8_t *sSf;          /* superframe en decodificación */
static long sSfLen;
static int sFrame, sNbFrames;
/* trozo decodificado pendiente de entregar */
static unsigned sOutN, sOutPos;
static unsigned long sSkip;         /* muestras por canal que se descartan tras una búsqueda */
static unsigned long sSeekTarget;   /* ms + preroll tras una búsqueda */
static int sSeekPending;
static int sErrors;
static long sPos;

static const uint8_t kGuidHeader[16] = { 0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C };

static int wma_sniff(const uint8_t *head, size_t n)
{
    return n >= 16 && memcmp(head, kGuidHeader, 16) == 0;
}

static int check_params(const AsfInfo *ai)
{
    if (ai->codec_id != ASF_CODEC_WMAV1 && ai->codec_id != ASF_CODEC_WMAV2)
        return CODEC_ERR_UNSUPPORTED;               /* WMA Pro, Voice, Lossless... */
    if (ai->channels < 1 || ai->channels > 2 || ai->rate < MIN_RATE || ai->rate > MAX_RATE)
        return CODEC_ERR_PARAMS;
    if (ai->blockalign == 0)
        return CODEC_ERR_UNSUPPORTED;
    return CODEC_OK;
}

static int wma_probe(const CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci)
{
    AsfInfo ai;
    int rc = asf_parse_header(io, buf, cap, &ai);

    if (rc != CODEC_OK)
        return rc;
    rc = check_params(&ai);
    if (rc != CODEC_OK)
        return rc;
    strcpy(ci->format, "WMA");
    ci->rate = ai.rate;
    ci->channels = ai.channels;
    ci->bits_src = 0;
    ci->tags = ai.tags;
    ci->duration_ms = ai.duration_ms;
    ci->bitrate = ai.duration_ms
        ? (unsigned)((unsigned long long)(io->size - ai.data_offset) * 8000ULL / ai.duration_ms) : ai.bitrate;
    ci->vbr = 0;
    ci->has_profiles = 0;
    ci->eq_capable = 0;
    return CODEC_OK;
}

/* ------------------------------------------------------------------------ */
/* Reproducción                                                              */
/* ------------------------------------------------------------------------ */

static void reset_decoder_state(void)
{
    sDec.last_superframe_len = 0;
    sDec.last_bitoffset = 0;
    memset(*sDec.frame_out, 0, sizeof(fixed32) * MAX_CHANNELS * BLOCK_MAX_SIZE * 2);
    sObj = sSf = NULL;
    sObjLen = sObjOff = sSfLen = 0;
    sFrame = sNbFrames = 0;
    sOutN = sOutPos = 0;
    sErrors = 0;
}

static int wma_open(const CodecIO *io, DecProfile prof, uint8_t *buf, size_t bufsize, const CodecInfo *ci)
{
    Arena *a = codec_arena();
    asf_waveformatex_t wfx;
    uint8_t *pkt, *obj;
    int rc;

    (void)prof;
    (void)ci;
    arena_reset(a);
    sIo = *io;
    sPos = 0;
    rc = asf_parse_header(io, buf, (long)bufsize, &sInfo);
    if (rc != CODEC_OK)
        return rc;
    rc = check_params(&sInfo);
    if (rc != CODEC_OK)
        return rc;
    pkt = arena_alloc(a, sInfo.packet_size);
    obj = arena_alloc(a, MAX_CODED_SUPERFRAME_SIZE);
    if (pkt == NULL || obj == NULL)
        return CODEC_ERR_MEMORY;

    memset(&wfx, 0, sizeof wfx);
    wfx.packet_size = sInfo.packet_size;
    wfx.audiostream = sInfo.audiostream;
    wfx.codec_id = sInfo.codec_id;
    wfx.channels = sInfo.channels;
    wfx.rate = sInfo.rate;
    wfx.bitrate = sInfo.bitrate;
    wfx.blockalign = sInfo.blockalign;
    wfx.bitspersample = sInfo.bitspersample;
    wfx.datalen = sInfo.extra_len;
    wfx.numpackets = sInfo.num_packets;
    memcpy(wfx.data, sInfo.extra, sInfo.extra_len);
    memset(&sDec, 0, sizeof sDec);
    if (wma_decode_init(&sDec, &wfx) < 0)
        return CODEC_ERR_UNSUPPORTED;

    sCh = sInfo.channels;
    sRate = sInfo.rate;
    asf_reader_init(&sRd, &sIo, &sInfo, pkt, obj, MAX_CODED_SUPERFRAME_SIZE);
    reset_decoder_state();
    sSkip = (unsigned long)DELAY_FRAMES * (unsigned long)sDec.frame_len;      /* arranque del decodificador */
    sSeekPending = 0;
    sOpen = 1;
    return CODEC_OK;
}

static int wma_next(DecFrame *out)
{
    for (;;) {
        unsigned n;

        if (!sOpen)
            return -1;
        if (sOutPos < sOutN) {                          /* entregar un trozo de lo ya decodificado */
            n = sOutN - sOutPos;
            if (n > CODEC_MAX_FRAMES)
                n = CODEC_MAX_FRAMES;
            /* núcleo aparte (referencia en C o asm 68040, src/pcm_kernels.h): clamp((8192 - v) >> 14), intercalado */
            MP368K_PCM_WMA16((const int32_t *)&(*sDec.frame_out)[0][sOutPos],
                             (const int32_t *)&(*sDec.frame_out)[sCh > 1 ? 1 : 0][sOutPos], sPcm, n, sCh);
            sOutPos += n;
            out->pcm = sPcm;
            out->frames = n;
            out->count = n * sCh;
            out->channels = sCh;
            out->rate = sRate;
            out->bitrate = sInfo.bitrate;
            out->in_rate = sRate;
            out->in_channels = sCh;
            out->layer = 0;
            out->file_pos = sPos;
            return 1;
        }
        if (sFrame < sNbFrames) {                       /* siguiente cuadro del superframe */
            int got = wma_decode_superframe_frame(&sDec, sSf, (int)sSfLen);

            sFrame++;
            if (got < 0) {
                sFrame = sNbFrames;                     /* se abandona el resto de este superframe */
                if (++sErrors > MAX_ERRORS)
                    return -1;
                continue;
            }
            sErrors = 0;
            if (got > 0) {
                sOutN = (unsigned)got;
                sOutPos = 0;
                if (sSkip) {                            /* descartar el comienzo tras una búsqueda */
                    unsigned drop = sSkip < sOutN ? (unsigned)sSkip : sOutN;

                    sOutPos = drop;
                    sSkip -= drop;
                }
            }
            continue;
        }
        if (sObjOff >= sObjLen) {                       /* hace falta otro objeto de medios */
            uint32_t pts = 0;
            int rc = asf_next_object(&sRd, &sObj, &sObjLen, &pts);

            if (rc <= 0)
                return rc;
            sObjOff = 0;
            sPos = sInfo.data_offset + (long)((unsigned long long)sInfo.packet_size * sRd.next_packet);
            if (sSeekPending) {                         /* primera marca de tiempo tras una búsqueda */
                /* se aterriza antes del destino: se descarta la diferencia más el arranque del decodificador */
                sSkip = (pts < sSeekTarget ? (unsigned long)((unsigned long long)(sSeekTarget - pts) * sRate / 1000ULL) : 0)
                        + (unsigned long)DELAY_FRAMES * (unsigned long)sDec.frame_len;
                sSeekPending = 0;
            }
        }
        {
            long bl = sInfo.blockalign;

            sSfLen = (bl > 0 && sObjLen > bl && sObjLen % bl == 0) ? bl : sObjLen - sObjOff;
        }
        sSf = sObj + sObjOff;
        sObjOff += sSfLen;
        wma_decode_superframe_init(&sDec, sSf, (int)sSfLen);
        sNbFrames = sDec.nb_frames;
        sFrame = 0;
    }
}

static int wma_seek(unsigned long ms)
{
    if (!sOpen)
        return CODEC_ERR_IO;
    if (asf_seek_ms(&sRd, ms) != 0)
        return CODEC_ERR_IO;
    reset_decoder_state();
    sSkip = 0;
    sSeekTarget = ms + sInfo.preroll_ms;
    sSeekPending = 1;
    return CODEC_OK;
}

static void wma_close(void)
{
    sOpen = 0;
}

const Codec codec_wma = { "WMA", wma_sniff, wma_probe, wma_open, wma_next, wma_seek, wma_close, NULL };
