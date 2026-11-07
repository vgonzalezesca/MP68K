/*
 * codec_flac — FLAC con dr_flac (dominio público o MIT-0), sobre un arena sin malloc.
 *
 * Las etiquetas y la duración se leen con un lector propio (probe): así una carátula de varios
 * cientos de KB en el archivo no pasa por la memoria del decodificador. Para reproducir se usa
 * drflac_open sin callback de metadatos, que se salta los bloques que no necesita.
 *
 * Límites declarados: hasta 2 canales y de 8 a 48 kHz (el audio de hoy no remuestrea); 24 bits
 * se reducen a 16 y 8 bits se amplían, tal como lo hace dr_flac (sin redondeo).
 */
#include "codec.h"

#include <string.h>

#include "dr_flac_config.h"
#include "dr_flac.h"
#include "vorbiscomment.h"

#define MIN_RATE 8000
#define MAX_RATE 48000
#define MAX_BLOCKS 256              /* tope de bloques de metadatos que se recorren */
#define MAX_COMMENT 4096            /* tope de entradas de comentario */
#define MAX_ENTRY 512               /* entradas más largas se saltan (carátulas en base64, etc.) */

static drflac *sF;
static CodecIO sIo;
static long sPos;                   /* posición de lectura actual, para las búsquedas relativas */
static int16_t sPcm[CODEC_MAX_FRAMES * 2];
static unsigned sRate, sCh;

/* ------------------------------------------------------------------------ */
/* Memoria y E/S para dr_flac                                                */
/* ------------------------------------------------------------------------ */

static void *a_malloc(size_t sz, void *ud)
{
    (void)ud;
    return arena_alloc(codec_arena(), sz);
}

static void *a_realloc(void *p, size_t sz, void *ud)
{
    (void)ud;
    return arena_realloc(codec_arena(), p, sz);
}

static void a_free(void *p, void *ud)
{
    (void)ud;
    arena_free(codec_arena(), p);
}

static size_t on_read(void *ud, void *dst, size_t n)
{
    long got = 0;

    (void)ud;
    if (sIo.read(sIo.ctx, dst, (long)n, &got) != 0 || got < 0)
        return 0;
    sPos += got;
    return (size_t)got;
}

static drflac_bool32 on_seek(void *ud, int offset, drflac_seek_origin origin)
{
    long target = origin == DRFLAC_SEEK_SET ? (long)offset : sPos + (long)offset;

    (void)ud;
    if (target < 0 || sIo.seek(sIo.ctx, target) != 0)
        return DRFLAC_FALSE;
    sPos = target;
    return DRFLAC_TRUE;
}

/* ------------------------------------------------------------------------ */
/* Reconocimiento y lectura de la cabecera                                   */
/* ------------------------------------------------------------------------ */

static int flac_sniff(const uint8_t *head, size_t n)
{
    return n >= 4 && memcmp(head, "fLaC", 4) == 0;
}

static unsigned long le32(const uint8_t *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static unsigned long be32(const uint8_t *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

/* Lee exactamente `want` bytes desde `offset`; 0 si lo logró. */
static int rd(const CodecIO *io, long offset, uint8_t *dst, long want)
{
    long total = 0, n;

    if (io->seek(io->ctx, offset) != 0)
        return -1;
    while (total < want) {
        n = 0;
        if (io->read(io->ctx, dst + total, want - total, &n) != 0 || n <= 0)
            return -1;
        total += n;
    }
    return 0;
}

/* Recorre el bloque VORBIS_COMMENT (que empieza en `pos` y mide `len`). Tolera bloques dañados. */
static void read_comments(const CodecIO *io, long pos, long len, uint8_t *buf, long cap, Id3Tags *tg)
{
    uint8_t w[4];
    long end = pos + len, cur = pos;
    unsigned long vendor, count, i;

    if (len < 8 || rd(io, cur, w, 4) != 0)
        return;
    vendor = le32(w);
    cur += 4 + (long)vendor;
    if (cur + 4 > end || rd(io, cur, w, 4) != 0)
        return;
    count = le32(w);
    cur += 4;
    if (count > MAX_COMMENT)
        count = MAX_COMMENT;
    for (i = 0; i < count && cur + 4 <= end; i++) {
        unsigned long n;

        if (rd(io, cur, w, 4) != 0)
            return;
        n = le32(w);
        cur += 4;
        if (cur + (long)n > end)
            return;
        if (n <= MAX_ENTRY && (long)n <= cap && rd(io, cur, buf, (long)n) == 0)
            vorbiscomment_entry(buf, (size_t)n, tg);
        cur += (long)n;
    }
}

static int flac_probe(const CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci)
{
    uint8_t h[4], si[34];
    long pos = io->audio_start + 4;
    int have_info = 0, k;
    unsigned long long total = 0;

    if (cap < MAX_ENTRY)
        return CODEC_ERR_PARAMS;
    if (rd(io, io->audio_start, h, 4) != 0)
        return CODEC_ERR_IO;
    if (memcmp(h, "fLaC", 4) != 0)
        return CODEC_ERR_UNSUPPORTED;

    for (k = 0; k < MAX_BLOCKS; k++) {
        unsigned type, last;
        long len;

        if (rd(io, pos, h, 4) != 0)
            break;
        last = h[0] >> 7;
        type = h[0] & 0x7F;
        len = (long)(((unsigned long)h[1] << 16) | ((unsigned long)h[2] << 8) | h[3]);
        pos += 4;
        if (type == 0 && len >= 34) {                       /* STREAMINFO */
            if (rd(io, pos, si, 34) != 0)
                return CODEC_ERR_UNSUPPORTED;
            ci->rate = ((unsigned)si[10] << 12) | ((unsigned)si[11] << 4) | (si[12] >> 4);
            ci->channels = (unsigned)((si[12] >> 1) & 7) + 1;
            ci->bits_src = (((unsigned)(si[12] & 1)) << 4 | (si[13] >> 4)) + 1u;
            total = ((unsigned long long)(si[13] & 0x0F) << 32) | be32(si + 14);
            have_info = 1;
        } else if (type == 4) {                             /* VORBIS_COMMENT */
            read_comments(io, pos, len, buf, cap, &ci->tags);
        }
        pos += len;
        if (last)
            break;
    }
    if (!have_info || ci->rate == 0)
        return CODEC_ERR_UNSUPPORTED;
    if (ci->channels > 2 || ci->rate < MIN_RATE || ci->rate > MAX_RATE)
        return CODEC_ERR_PARAMS;

    strcpy(ci->format, "FLAC");
    ci->vbr = 1;
    ci->duration_ms = total ? (unsigned long)((total / ci->rate) * 1000ULL +
                                              ((total % ci->rate) * 1000ULL) / ci->rate) : 0;
    ci->bitrate = ci->duration_ms
        ? (unsigned)((unsigned long long)(io->size - io->audio_start) * 8000ULL / ci->duration_ms) : 0;
    ci->has_profiles = 0;
    ci->eq_capable = 0;
    return CODEC_OK;
}

/* ------------------------------------------------------------------------ */
/* Reproducción                                                              */
/* ------------------------------------------------------------------------ */

static int flac_open(const CodecIO *io, DecProfile prof, uint8_t *buf, size_t bufsize, const CodecInfo *ci)
{
    static drflac_allocation_callbacks cb;
    Arena *a = codec_arena();

    (void)prof;
    (void)buf;
    (void)bufsize;
    (void)ci;
    arena_reset(a);
    sIo = *io;
    sPos = 0;
    if (sIo.seek(sIo.ctx, 0) != 0)
        return CODEC_ERR_IO;
    cb.pUserData = NULL;
    cb.onMalloc = a_malloc;
    cb.onRealloc = a_realloc;
    cb.onFree = a_free;
    sF = drflac_open(on_read, on_seek, NULL, NULL, &cb);
    if (sF == NULL)
        return a->failed ? CODEC_ERR_MEMORY : CODEC_ERR_UNSUPPORTED;
    sRate = sF->sampleRate;
    sCh = sF->channels;
    if (sCh < 1 || sCh > 2 || sRate < MIN_RATE || sRate > MAX_RATE) {
        drflac_close(sF);
        sF = NULL;
        return CODEC_ERR_PARAMS;
    }
    return CODEC_OK;
}

static int flac_next(DecFrame *out)
{
    drflac_uint64 n;

    if (sF == NULL)
        return -1;
    n = drflac_read_pcm_frames_s16(sF, CODEC_MAX_FRAMES, sPcm);
    if (n == 0)
        return 0;
    out->pcm = sPcm;
    out->frames = (unsigned)n;
    out->count = (unsigned)n * sCh;
    out->channels = sCh;
    out->rate = sRate;
    out->bitrate = 0;
    out->in_rate = sRate;
    out->in_channels = sCh;
    out->layer = 0;
    out->file_pos = sPos;
    return 1;
}

static int flac_seek(unsigned long ms)
{
    drflac_uint64 frame;

    if (sF == NULL)
        return CODEC_ERR_IO;
    frame = (drflac_uint64)ms * sRate / 1000ULL;
    return drflac_seek_to_pcm_frame(sF, frame) ? CODEC_OK : CODEC_ERR_IO;
}

static void flac_close(void)
{
    if (sF != NULL) {
        drflac_close(sF);
        sF = NULL;
    }
}

const Codec codec_flac = { "FLAC", flac_sniff, flac_probe, flac_open, flac_next, flac_seek, flac_close, NULL };
