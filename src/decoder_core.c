/*
 * decoder_core — ver decoder_core.h.
 */
#include "decoder_core.h"

#include <mad.h>

#include <string.h>

/*
 * Todo el estado es estático: libmad pide dos bloques dinámicos (main_data con
 * malloc y overlap con calloc) pero solo cuando el puntero es NULL, así que se
 * los damos nosotros y no se toca el código de libmad. Por eso tampoco se llama
 * a mad_stream_finish() ni a mad_frame_finish(): liberarían estos bloques.
 */
#ifdef MP368K_PROFILE
/* Definidos en third_party/patches/mp368k_prof.c (biblioteca parcheada). */
extern unsigned long long (*mp368k_prof_clock)(void);
extern unsigned long mp368k_prof_acc[PROF_LIBMAD_BUCKETS];
extern unsigned long mp368k_prof_marks;
#endif

static struct mad_stream sStream;
static struct mad_frame sFrame;
static struct mad_synth sSynth;
static unsigned char sMainData[MAD_BUFFER_MDLEN];
static mad_fixed_t sOverlap[2][32][18];

static int16_t sPcm[DEC_PCM_MAX];

static uint32_t sCrcTable[16];

static const char *const kProfileNames[DEC_PROFILE_COUNT] = {
    "FULL", "HALF", "MONO", "HALF_MONO"
};

const char *dec_profile_name(DecProfile p)
{
    return (unsigned)p < DEC_PROFILE_COUNT ? kProfileNames[p] : "?";
}

const char *dec_library_info(void)
{
    static char info[160];

    if (info[0] == '\0') {
        /* Sin snprintf: este módulo no depende de la libc de E/S. */
        size_t n = 0;
        const char *parts[] = { "libmad ", mad_version, " / ", mad_build };
        unsigned i;

        for (i = 0; i < sizeof parts / sizeof parts[0]; i++) {
            size_t len = strlen(parts[i]);
            if (n + len >= sizeof info)
                len = sizeof info - 1 - n;
            memcpy(info + n, parts[i], len);
            n += len;
        }
        info[n] = '\0';
    }
    return info;
}

/* CRC-32 (polinomio 0xEDB88320) por nibbles; resultado estándar, p. ej. "123456789" -> CBF43926. */
static void crc_init_table(void)
{
    unsigned i, k;

    for (i = 0; i < 16; i++) {
        uint32_t c = i;
        for (k = 0; k < 4; k++)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
        sCrcTable[i] = c;
    }
}

static uint32_t crc_byte(uint32_t crc, uint8_t b)
{
    crc = (crc >> 4) ^ sCrcTable[(crc ^ b) & 0xF];
    crc = (crc >> 4) ^ sCrcTable[(crc ^ (b >> 4)) & 0xF];
    return crc;
}

uint32_t dec_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xFFFFFFFFUL;
    size_t i;

    crc_init_table();
    for (i = 0; i < size; i++)
        crc = crc_byte(crc, data[i]);
    return crc ^ 0xFFFFFFFFUL;
}

/* Redondeo y saturación a 16 bits, igual que scale() de minimad.c de libmad. */
static int16_t scale16(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));

    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return (int16_t)(sample >> (MAD_F_FRACBITS + 1 - 16));
}

/* Longitud total de una etiqueta ID3v2 cuyos primeros 10 bytes son `d` (0 si no hay). */
static size_t id3v2_total(const uint8_t *d)
{
    size_t len;

    if (d[0] != 'I' || d[1] != 'D' || d[2] != '3')
        return 0;
    if ((d[6] | d[7] | d[8] | d[9]) & 0x80)
        return 0;               /* tamaño "synchsafe" inválido */
    len = ((size_t)d[6] << 21) | ((size_t)d[7] << 14) | ((size_t)d[8] << 7) | d[9];
    len += 10;
    if (d[5] & 0x10)
        len += 10;              /* pie de etiqueta */
    return len;
}

/* Igual, acotada al tamaño del bloque en memoria. */
static size_t id3v2_length(const uint8_t *d, size_t size)
{
    size_t len = size < 10 ? 0 : id3v2_total(d);

    return len < size ? len : size;
}

/* Mezcla L+R en las subbandas y deja el frame como de un solo canal. */
static void mix_to_mono(struct mad_frame *frame);

/* Pasos de un frame ya decodificado en sFrame; comparten dec_run y el flujo. */
static void step_mix(int want_mono)
{
    if (want_mono && MAD_NCHANNELS(&sFrame.header) == 2)
        mix_to_mono(&sFrame);
}

static unsigned step_convert(void)
{
    unsigned nch = sSynth.pcm.channels, len = sSynth.pcm.length;
    unsigned count = 0, i, ch;

    for (i = 0; i < len; i++)
        for (ch = 0; ch < nch; ch++)
            sPcm[count++] = scale16(sSynth.pcm.samples[ch][i]);
    return count;
}

static void mix_to_mono(struct mad_frame *frame)
{
    unsigned ns = MAD_NSBSAMPLES(&frame->header);
    unsigned s, sb;

    for (s = 0; s < ns; s++)
        for (sb = 0; sb < 32; sb++)
            frame->sbsample[0][s][sb] =
                (frame->sbsample[0][s][sb] + frame->sbsample[1][s][sb]) >> 1;

    frame->header.mode = MAD_MODE_SINGLE_CHANNEL;
}

int dec_run(const uint8_t *data, size_t size, const DecConfig *cfg, DecResult *out)
{
    unsigned want_half = cfg->profile == DEC_PROFILE_HALF || cfg->profile == DEC_PROFILE_HALF_MONO;
    unsigned want_mono = cfg->profile == DEC_PROFILE_MONO || cfg->profile == DEC_PROFILE_HALF_MONO;
    uint32_t crc = 0xFFFFFFFFUL;
    size_t skip = id3v2_length(data, size);
    DecClock clk = cfg->clock;
    unsigned long long decode_total = 0;

    memset(out, 0, sizeof *out);
#ifdef MP368K_PROFILE
    mp368k_prof_clock = clk;
    memset(mp368k_prof_acc, 0, sizeof mp368k_prof_acc);
    mp368k_prof_marks = 0;
#endif
    if (cfg->want_crc)
        crc_init_table();

    mad_stream_init(&sStream);
    mad_frame_init(&sFrame);
    mad_synth_init(&sSynth);
    memset(sOverlap, 0, sizeof sOverlap);
    sStream.main_data = &sMainData;
    sFrame.overlap = &sOverlap;
    if (want_half)
        sStream.options |= MAD_OPTION_HALFSAMPLERATE;

    mad_stream_buffer(&sStream, data + skip, size - skip);

    for (;;) {
        unsigned nch, len, i, count;
        unsigned long long t0 = 0, t1 = 0, t2 = 0, t3 = 0, t4 = 0;

        if (clk)
            t0 = clk();
        if (mad_frame_decode(&sFrame, &sStream) != 0) {
            if (MAD_RECOVERABLE(sStream.error)) {
                out->errors++;
                continue;
            }
            if (sStream.error != MAD_ERROR_BUFLEN) {
                out->errors++;
                out->fatal = 1;
            }
            break;
        }

        if (clk)
            t1 = clk();

        if (out->frames == 0) {
            out->bitrate = sFrame.header.bitrate;
            out->in_rate = sFrame.header.samplerate;
            out->in_channels = MAD_NCHANNELS(&sFrame.header);
            out->layer = sFrame.header.layer;
        }

        step_mix(want_mono);

        if (clk)
            t2 = clk();
        mad_synth_frame(&sSynth, &sFrame);
        if (clk)
            t3 = clk();

        nch = sSynth.pcm.channels;
        len = sSynth.pcm.length;
        count = step_convert();

        if (clk) {
            t4 = clk();
            decode_total += t1 - t0;
            out->prof_us[PROF_MONOMIX] += t2 - t1;
            out->prof_us[PROF_SYNTH] += t3 - t2;
            out->prof_us[PROF_PCM] += t4 - t3;
        }

        if (cfg->want_crc)
            for (i = 0; i < count; i++) {
                uint16_t v = (uint16_t)sPcm[i];
                crc = crc_byte(crc, (uint8_t)(v >> 8));
                crc = crc_byte(crc, (uint8_t)v);
            }
        if (cfg->pcm_sink)
            cfg->pcm_sink(sPcm, count, cfg->sink_ctx);

        out->channels = nch;
        out->out_rate = sSynth.pcm.samplerate;
        out->samples_per_ch += len;
        out->frames++;

        if (cfg->max_frames != 0 && out->frames >= cfg->max_frames)
            break;
    }

    if (clk) {
        unsigned long long inside = 0;
        int b;

#ifdef MP368K_PROFILE
        for (b = 0; b < PROF_LIBMAD_BUCKETS; b++) {
            out->prof_us[b] = mp368k_prof_acc[b];
            inside += mp368k_prof_acc[b];
        }
        out->prof_marks = mp368k_prof_marks;
        mp368k_prof_clock = 0;
#endif
        (void) b;
        out->prof_us[PROF_OTHER] = decode_total > inside ? decode_total - inside : 0;
    }
    if (cfg->want_crc)
        out->crc = crc ^ 0xFFFFFFFFUL;
    return out->frames > 0 ? 0 : -1;
}

/* ------------------------------------------------------------------------ */
/* Decodificación en flujo                                                   */
/* ------------------------------------------------------------------------ */

static DecReadFn sRead;
static DecSeekFn sSeek;
static void *sCtx;
static uint8_t *sBuf;
static size_t sBufSize;
static long sBufOffset;             /* posición en el archivo de sBuf[0] */
static int sNeedData, sEof, sWantMono;
static unsigned long sStreamErrors;
static long sAudioStart;

static void stream_reset_libmad(DecProfile profile)
{
    mad_stream_init(&sStream);
    mad_frame_init(&sFrame);
    mad_synth_init(&sSynth);
    memset(sOverlap, 0, sizeof sOverlap);
    sStream.main_data = &sMainData;
    sFrame.overlap = &sOverlap;
    if (profile == DEC_PROFILE_HALF || profile == DEC_PROFILE_HALF_MONO)
        sStream.options |= MAD_OPTION_HALFSAMPLERATE;
    sWantMono = profile == DEC_PROFILE_MONO || profile == DEC_PROFILE_HALF_MONO;
}

int dec_stream_open(DecProfile profile, DecReadFn read, DecSeekFn seek, void *ctx,
                    uint8_t *buf, size_t bufsize)
{
    uint8_t head[10];
    long got = 0;
    size_t skip = 0;

    if (bufsize < DEC_STREAM_MIN_BUF || read == 0 || seek == 0)
        return -1;
    sRead = read;
    sSeek = seek;
    sCtx = ctx;
    sBuf = buf;
    sBufSize = bufsize;
    sStreamErrors = 0;

    /* ¿Etiqueta ID3v2 al inicio? Se mira la cabecera y se salta con seek. */
    if (seek(ctx, 0) != 0 || read(ctx, head, sizeof head, &got) != 0)
        return -1;
    if (got == (long)sizeof head)
        skip = id3v2_total(head);
    if (seek(ctx, (long)skip) != 0)
        return -1;
    sBufOffset = (long)skip;
    sAudioStart = (long)skip;

    stream_reset_libmad(profile);
    mad_stream_buffer(&sStream, sBuf, 0);
    sNeedData = 1;
    sEof = 0;
    return 0;
}

/* Rellena el buffer conservando los bytes sin consumir. Devuelve 0 si estuvo bien. */
static int stream_refill(void)
{
    size_t remaining = 0;
    long got = 0;

    if (sStream.next_frame != 0) {
        remaining = (size_t)(sStream.bufend - sStream.next_frame);
        sBufOffset += (long)(sStream.next_frame - sBuf);
        memmove(sBuf, sStream.next_frame, remaining);
    }
    if (sRead(sCtx, sBuf + remaining, (long)(sBufSize - remaining - MAD_BUFFER_GUARD), &got) != 0)
        return -1;
    if (got == 0) {
        /* Fin de archivo: libmad exige MAD_BUFFER_GUARD bytes de cola para el último frame. */
        sEof = 1;
        memset(sBuf + remaining, 0, MAD_BUFFER_GUARD);
        got = MAD_BUFFER_GUARD;
    }
    mad_stream_buffer(&sStream, sBuf, remaining + (size_t)got);
    return 0;
}

int dec_stream_next(DecFrame *out)
{
    unsigned count;

    for (;;) {
        if (sNeedData) {
            if (sEof)
                return 0;
            if (stream_refill() != 0)
                return -1;
            sNeedData = 0;
        }
        if (mad_frame_decode(&sFrame, &sStream) != 0) {
            /* stream.error solo vale justo después de un fallo. */
            if (sStream.error == MAD_ERROR_BUFLEN) {
                sNeedData = 1;
                continue;
            }
            if (MAD_RECOVERABLE(sStream.error)) {
                sStreamErrors++;
                continue;
            }
            sStreamErrors++;
            return -1;
        }
        break;
    }

    step_mix(sWantMono);
    mad_synth_frame(&sSynth, &sFrame);
    count = step_convert();

    out->pcm = sPcm;
    out->count = count;
    out->frames = sSynth.pcm.length;
    out->channels = sSynth.pcm.channels;
    out->rate = sSynth.pcm.samplerate;
    out->bitrate = sFrame.header.bitrate;
    out->in_rate = sFrame.header.samplerate;
    out->in_channels = MAD_NCHANNELS(&sFrame.header);
    out->layer = sFrame.header.layer;
    out->file_pos = sBufOffset + (long)(sStream.next_frame - sBuf);
    return 1;
}

int dec_stream_seek(long offset)
{
    if (sSeek(sCtx, offset) != 0)
        return -1;
    sBufOffset = offset;
    sEof = 0;
    mad_stream_buffer(&sStream, sBuf, 0);
    sStream.md_len = 0;             /* se descarta la reserva de bits anterior */
    mad_frame_mute(&sFrame);
    mad_synth_mute(&sSynth);
    sNeedData = 1;
    return 0;
}

unsigned long dec_stream_errors(void)
{
    return sStreamErrors;
}

long dec_stream_audio_start(void)
{
    return sAudioStart;
}

void dec_stream_close(void)
{
    /* Sin mad_*_finish: liberarían los bloques estáticos (ver arriba). */
    sRead = 0;
    sSeek = 0;
    sBuf = 0;
}
