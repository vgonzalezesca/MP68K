/*
 * audio — ver audio.h.
 */
#include "audio.h"

#include "memblock.h"
#include "pcmconv.h"
#include "ring.h"
#include "sndver.h"

#include <Memory.h>
#include <Sound.h>

#include <stddef.h>
#include <string.h>

/*
 * Constantes que las Multiversal Interfaces no traen (valores de Sound.h de Apple,
 * por verificar con la salida de audiotest).
 */
enum {
    MP_initMono = 0x0080,
    MP_initStereo = 0x00C0,
    MP_siSampleSizeAvailable = 'ssav'
};

#define BUF_FRAMES 2048     /* 46 ms a 44,1 kHz; el retardo de pausa es de hasta 2 buffers */
#define DEFAULT_RING_MS 1000
#define CONVERT_CHUNK 256

typedef struct {
    Ring ring;
    volatile Boolean paused;
    volatile Boolean draining;
    volatile Boolean done;
    volatile unsigned long underruns;
    volatile unsigned long frames_played;
    unsigned frame_bytes;
    unsigned buf_frames;
    uint8_t silence;
} AudioCtx;

static AudioCtx gCtx;
static SndChannelPtr gChan;
static SndDoubleBufferHeader gHeader;
static MemBlock gRingMem, gBufMem[2];
static unsigned gBits, gChannels;
static Boolean gOpen, gStarted;
static unsigned gGainL = 256, gGainR = 256;

/*
 * Callback del doble buffer: convención Pascal en el 68k. Se ejecuta en tiempo de
 * interrupción: solo memcpy/memset y contadores. El contexto llega por chan->userInfo
 * (las variables globales de Retro68 son direcciones absolutas, pero así no dependemos
 * de eso).
 */
static pascal void audio_double_back(SndChannelPtr chan, SndDoubleBufferPtr buf)
{
    AudioCtx *c = (AudioCtx *)chan->userInfo;
    unsigned long want = (unsigned long)c->buf_frames * c->frame_bytes;
    uint8_t *dst = (uint8_t *)&buf->dbSoundData[0];
    unsigned long got = 0;
    long flags = dbBufferReady;
    long frames = c->buf_frames;

    if (!c->paused)
        got = ring_read(&c->ring, dst, want);
    if (got < want) {
        memset(dst + got, c->silence, want - got);
        if (c->draining && !c->paused) {
            /* Fin: este es el último buffer; se entrega solo lo que había. */
            frames = got / c->frame_bytes;
            if (frames < 1)
                frames = 1;             /* un frame de silencio */
            flags |= dbLastBuffer;
            c->done = true;
        } else if (!c->paused) {
            c->underruns++;
        }
    }
    c->frames_played += got / c->frame_bytes;
    buf->dbNumFrames = frames;
    buf->dbFlags |= flags;
}

static unsigned be16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

/* Tamaños de muestra que ofrece la salida: bit 0 = 8 bits, bit 1 = 16 bits; 0 = no se pudo saber. */
static unsigned query_sample_sizes(SndChannelPtr chan)
{
    union { unsigned char b[64]; long align; } u;
    Handle h;
    unsigned mask = 0;
    long i, n;

    memset(u.b, 0, sizeof u.b);
    if (SndGetInfo(chan, MP_siSampleSizeAvailable, u.b) != noErr)
        return 0;
    memcpy(&h, u.b + 2, sizeof h);
    if (h == NULL)
        return 0;
    n = GetHandleSize(h) / 2;
    HLock(h);
    for (i = 0; i < n; i++) {
        unsigned bits = be16((const unsigned char *)*h + i * 2);
        if (bits == 8)
            mask |= 1;
        else if (bits == 16)
            mask |= 2;
    }
    HUnlock(h);
    DisposeHandle(h);
    return mask;
}

OSErr audio_open(unsigned rate, unsigned channels, unsigned want_bits, unsigned ring_ms,
                 AudioInfo *info)
{
    uint32_t sm_version;
    unsigned long ring_bytes;
    long buf_bytes;
    OSErr err;
    int i;

    if (gOpen)
        return paramErr;
    if (channels < 1 || channels > 2 || rate < 8000 || rate > 48000)
        return paramErr;

    sm_version = sndver_get();
    if ((sm_version >> 24) < 3)
        return AUDIO_ERR_NO_DOUBLEBUFFER;

    {
        unsigned long keep = gCtx.underruns;    /* el contador acumula durante toda la ejecución */

        memset(&gCtx, 0, sizeof gCtx);
        gCtx.underruns = keep;
    }
    err = SndNewChannel(&gChan, sampledSynth, channels == 2 ? MP_initStereo : MP_initMono, NULL);
    if (err != noErr)
        return err;

    info->sm_major = (unsigned)(sm_version >> 24);
    info->sm_minor = (unsigned)((sm_version >> 20) & 0xF);
    info->hw_sizes = query_sample_sizes(gChan);
    if (want_bits == 8 || want_bits == 16)
        gBits = want_bits;
    else
        gBits = (info->hw_sizes & 2) ? 16 : ((info->hw_sizes & 1) ? 8 : 16);
    gChannels = channels;

    gCtx.frame_bytes = channels * (gBits / 8);
    gCtx.buf_frames = BUF_FRAMES;
    gCtx.silence = gBits == 8 ? 0x80 : 0x00;

    if (ring_ms == 0)
        ring_ms = DEFAULT_RING_MS;
    ring_bytes = (unsigned long)rate * ring_ms / 1000 * gCtx.frame_bytes;
    buf_bytes = (long)offsetof(SndDoubleBuffer, dbSoundData) + (long)BUF_FRAMES * gCtx.frame_bytes;

    err = memblock_alloc(&gRingMem, (long)ring_bytes);
    for (i = 0; i < 2 && err == noErr; i++)
        err = memblock_alloc(&gBufMem[i], buf_bytes);
    if (err != noErr) {
        memblock_free(&gRingMem);
        memblock_free(&gBufMem[0]);
        memblock_free(&gBufMem[1]);
        SndDisposeChannel(gChan, true);
        gChan = NULL;
        return err;
    }
    ring_init(&gCtx.ring, (uint8_t *)gRingMem.data, ring_bytes);

    memset(&gHeader, 0, sizeof gHeader);
    gHeader.dbhNumChannels = (short)channels;
    gHeader.dbhSampleSize = (short)gBits;
    gHeader.dbhCompressionID = 0;               /* sin compresión */
    gHeader.dbhPacketSize = 0;
    gHeader.dbhSampleRate = (Fixed)((unsigned long)rate << 16);
    gHeader.dbhBufferPtr[0] = (SndDoubleBufferPtr)gBufMem[0].data;
    gHeader.dbhBufferPtr[1] = (SndDoubleBufferPtr)gBufMem[1].data;
    gHeader.dbhDoubleBack = NewSndDoubleBackUPP(audio_double_back);
    gChan->userInfo = (long)&gCtx;

    info->rate = rate;
    info->channels = channels;
    info->bits = gBits;
    info->ring_bytes = ring_bytes;
    info->buf_frames = BUF_FRAMES;
    gOpen = true;
    gStarted = false;
    return noErr;
}

void audio_close(void)
{
    if (!gOpen)
        return;
    SndDisposeChannel(gChan, true);        /* detiene y descarta lo pendiente */
    gChan = NULL;
    memblock_free(&gRingMem);
    memblock_free(&gBufMem[0]);
    memblock_free(&gBufMem[1]);
    gOpen = false;
    gStarted = false;
}

long audio_write(const int16_t *pcm, long count)
{
    long room, done = 0;
    int unity = gGainL == 256 && gGainR == 256;

    if (!gOpen)
        return 0;
    room = (long)(ring_space(&gCtx.ring) / (gBits / 8));
    if (count > room)
        count = room;
    count -= count % (long)gChannels;               /* solo frames completos */
    if (count <= 0)
        return 0;

    if (gBits == 16 && unity) {
        ring_write(&gCtx.ring, pcm, (unsigned long)count * 2);
        return count;
    }
    while (done < count) {
        int16_t gained[CONVERT_CHUNK];
        uint8_t tmp[CONVERT_CHUNK];
        const int16_t *src = pcm + done;
        long n = count - done;

        if (n > CONVERT_CHUNK)
            n = CONVERT_CHUNK;                      /* par: no parte frames estéreo */
        if (!unity) {
            pcm_apply_gain16(src, gained, (unsigned long)n, gChannels, gGainL, gGainR);
            src = gained;
        }
        if (gBits == 16) {
            ring_write(&gCtx.ring, src, (unsigned long)n * 2);
        } else {
            pcm_s16_to_u8(src, tmp, (unsigned long)n);
            ring_write(&gCtx.ring, tmp, (unsigned long)n);
        }
        done += n;
    }
    return count;
}

void audio_set_gain(unsigned left, unsigned right)
{
    gGainL = left > 256 ? 256 : left;
    gGainR = right > 256 ? 256 : right;
}

OSErr audio_restart(void)
{
    OSErr err;

    if (!gOpen)
        return paramErr;
    SndDisposeChannel(gChan, true);        /* detiene los callbacks antes de tocar el anillo */
    gChan = NULL;

    ring_reset(&gCtx.ring);
    gCtx.paused = false;
    gCtx.draining = false;
    gCtx.done = false;
    gCtx.frames_played = 0;

    err = SndNewChannel(&gChan, sampledSynth, gChannels == 2 ? MP_initStereo : MP_initMono, NULL);
    if (err != noErr) {
        gChan = NULL;
        gOpen = false;                      /* sin canal no se puede seguir */
        memblock_free(&gRingMem);
        memblock_free(&gBufMem[0]);
        memblock_free(&gBufMem[1]);
        return err;
    }
    gChan->userInfo = (long)&gCtx;
    gStarted = false;
    return noErr;
}

long audio_space_frames(void)
{
    return gOpen ? (long)(ring_space(&gCtx.ring) / gCtx.frame_bytes) : 0;
}

long audio_buffered_frames(void)
{
    return gOpen ? (long)(ring_used(&gCtx.ring) / gCtx.frame_bytes) : 0;
}

OSErr audio_start(void)
{
    int i;

    if (!gOpen || gStarted)
        return paramErr;
    /* Los dos buffers deben ir llenos y marcados como listos antes de SndPlayDoubleBuffer. */
    for (i = 0; i < 2; i++) {
        SndDoubleBufferPtr b = gHeader.dbhBufferPtr[i];
        uint8_t *dst = (uint8_t *)&b->dbSoundData[0];
        unsigned long want = (unsigned long)gCtx.buf_frames * gCtx.frame_bytes;
        unsigned long got = ring_read(&gCtx.ring, dst, want);

        if (got < want)
            memset(dst + got, gCtx.silence, want - got);
        b->dbNumFrames = gCtx.buf_frames;
        b->dbFlags = dbBufferReady;
        b->dbUserInfo[0] = 0;
        b->dbUserInfo[1] = 0;
        gCtx.frames_played += got / gCtx.frame_bytes;
    }
    gStarted = true;
    return SndPlayDoubleBuffer(gChan, &gHeader);
}

void audio_set_paused(Boolean paused)
{
    gCtx.paused = paused;
}

void audio_finish(void)
{
    gCtx.draining = true;
}

Boolean audio_is_done(void)
{
    return gCtx.done;
}

unsigned long audio_underruns(void)
{
    return gCtx.underruns;
}

unsigned long audio_frames_played(void)
{
    return gCtx.frames_played;
}
