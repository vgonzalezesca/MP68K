/*
 * qtfmt — ver qtfmt.h.
 */
#include "qtfmt.h"

#include <string.h>

#define MAX_CHUNKS 64               /* tope de bloques que se recorren: un archivo dañado no cuelga la búsqueda */

static unsigned long le16(const uint8_t *p) { return (unsigned long)p[0] | ((unsigned long)p[1] << 8); }
static unsigned long le32(const uint8_t *p) { return le16(p) | (le16(p + 2) << 16); }
static unsigned long be16(const uint8_t *p) { return ((unsigned long)p[0] << 8) | p[1]; }
static unsigned long be32(const uint8_t *p) { return (be16(p) << 16) | be16(p + 2); }

/* Lee `want` bytes en `offset`; devuelve cuántos leyó (puede ser menos al final del archivo) o -1 si falla. */
static long rd(const CodecIO *io, long offset, uint8_t *dst, long want)
{
    long total = 0, n;

    if (io->seek(io->ctx, offset) != 0)
        return -1;
    while (total < want) {
        n = 0;
        if (io->read(io->ctx, dst + total, want - total, &n) != 0)
            return -1;
        if (n <= 0)
            break;
        total += n;
    }
    return total;
}

/* ms = frames * 1000 / rate sin pasar de 32 bits. */
static unsigned long frames_to_ms(unsigned long frames, unsigned long rate)
{
    if (rate == 0)
        return 0;
    return (frames / rate) * 1000UL + (frames % rate) * 1000UL / rate;
}

int qtfmt_sniff(const uint8_t *head, size_t n)
{
    if (n >= 12 && memcmp(head, "RIFF", 4) == 0 && memcmp(head + 8, "WAVE", 4) == 0)
        return 1;
    if (n >= 12 && memcmp(head, "FORM", 4) == 0 && (memcmp(head + 8, "AIFF", 4) == 0 || memcmp(head + 8, "AIFC", 4) == 0))
        return 1;
    if (n >= 4 && memcmp(head, ".snd", 4) == 0)
        return 1;
    return 0;
}

/* ------------------------------------------------------------------------ */
/* WAV                                                                       */
/* ------------------------------------------------------------------------ */

static int probe_wav(const CodecIO *io, QtInfo *qi)
{
    uint8_t h[8], fmt[40];
    long pos = 12, size = io->size, datapos = 0;
    unsigned long datalen = 0, blockalign = 0, tag = 0;
    int have_fmt = 0, have_data = 0, i;

    memset(qi, 0, sizeof *qi);
    for (i = 0; i < MAX_CHUNKS && pos + 8 <= size && !(have_fmt && have_data); i++) {
        unsigned long len;
        long got = rd(io, pos, h, 8);

        if (got < 0)
            return CODEC_ERR_IO;
        if (got < 8)
            break;
        len = le32(h + 4);
        if (memcmp(h, "fmt ", 4) == 0) {
            long want = len < sizeof fmt ? (long)len : (long)sizeof fmt;

            if (len < 16)
                return CODEC_ERR_UNSUPPORTED;
            got = rd(io, pos + 8, fmt, want);
            if (got < 0)
                return CODEC_ERR_IO;
            if (got < 16)
                return CODEC_ERR_UNSUPPORTED;
            tag = le16(fmt);
            qi->channels = (unsigned)le16(fmt + 2);
            qi->rate = (unsigned)le32(fmt + 4);
            blockalign = le16(fmt + 12);
            qi->bits = (unsigned)le16(fmt + 14);
            if (tag == 0xFFFEUL && got >= 26)           /* WAVE_FORMAT_EXTENSIBLE: el formato real va en el GUID */
                tag = le16(fmt + 24);
            have_fmt = 1;
        } else if (memcmp(h, "data", 4) == 0) {
            datapos = pos + 8;
            datalen = len;
            have_data = 1;
        }
        {
            unsigned long adv = len + (len & 1UL);

            if (adv > (unsigned long)(size - pos))      /* el bloque se pasa del archivo: no hay más que leer */
                break;
            pos += 8 + (long)adv;
        }
    }
    if (!have_fmt || !have_data)
        return CODEC_ERR_UNSUPPORTED;
    if (qi->channels == 0 || qi->rate == 0)
        return CODEC_ERR_PARAMS;
    /* "data" de largo 0 o 0xFFFFFFFF (WAV en flujo) o mayor que el archivo: vale lo que queda. */
    if (datalen == 0 || datalen > (unsigned long)(size - datapos))
        datalen = size > datapos ? (unsigned long)(size - datapos) : 0;
    if (blockalign == 0)
        blockalign = (unsigned long)((qi->bits + 7) / 8) * qi->channels;
    strcpy(qi->format, "WAV");
    qi->pcm = (tag == 1);
    qi->duration_ms = blockalign ? frames_to_ms(datalen / blockalign, qi->rate) : 0;
    return CODEC_OK;
}

/* ------------------------------------------------------------------------ */
/* AIFF y AIFC                                                               */
/* ------------------------------------------------------------------------ */

/* Tasa de un número de 80 bits (IEEE extendido) de AIFF; 0 si no es un entero razonable. */
static unsigned long ext80_to_rate(const uint8_t *b)
{
    unsigned long exp = ((b[0] & 0x7FUL) << 8) | b[1];
    unsigned long mant = be32(b + 2);               /* parte alta de la mantisa (bit 31 = 1 explícito) */
    long e;

    if (exp < 16383UL || mant == 0)
        return 0;
    e = (long)exp - 16383L;
    if (e > 31)
        return 0;
    return e >= 31 ? mant : (mant >> (31 - e));
}

static int aifc_is_pcm(const uint8_t *t)
{
    return memcmp(t, "NONE", 4) == 0 || memcmp(t, "sowt", 4) == 0 || memcmp(t, "twos", 4) == 0 ||
           memcmp(t, "raw ", 4) == 0 || memcmp(t, "in24", 4) == 0 || memcmp(t, "42ni", 4) == 0 ||
           memcmp(t, "in32", 4) == 0 || memcmp(t, "23ni", 4) == 0;
}

static int probe_aiff(const CodecIO *io, int is_aifc, QtInfo *qi)
{
    uint8_t h[8], comm[26];
    long pos = 12, size = io->size;
    unsigned long frames = 0, ssnd_len = 0;
    int have_comm = 0, have_ssnd = 0, i;

    memset(qi, 0, sizeof *qi);
    for (i = 0; i < MAX_CHUNKS && pos + 8 <= size && !(have_comm && have_ssnd); i++) {
        unsigned long len;
        long got = rd(io, pos, h, 8);

        if (got < 0)
            return CODEC_ERR_IO;
        if (got < 8)
            break;
        len = be32(h + 4);
        if (memcmp(h, "COMM", 4) == 0) {
            if (len < 18)
                return CODEC_ERR_UNSUPPORTED;
            got = rd(io, pos + 8, comm, (long)(len < sizeof comm ? len : sizeof comm));
            if (got < 0)
                return CODEC_ERR_IO;
            if (got < 18)
                return CODEC_ERR_UNSUPPORTED;
            qi->channels = (unsigned)be16(comm);
            frames = be32(comm + 2);
            qi->bits = (unsigned)be16(comm + 6);
            qi->rate = (unsigned)ext80_to_rate(comm + 8);
            qi->pcm = 1;
            if (is_aifc && got >= 22)
                qi->pcm = aifc_is_pcm(comm + 18);
            have_comm = 1;
        } else if (memcmp(h, "SSND", 4) == 0) {
            ssnd_len = len >= 8 ? len - 8 : 0;      /* descuenta offset y blockSize */
            have_ssnd = 1;
        }
        {
            unsigned long adv = len + (len & 1UL);

            if (adv > (unsigned long)(size - pos))
                break;
            pos += 8 + (long)adv;
        }
    }
    if (!have_comm)
        return CODEC_ERR_UNSUPPORTED;
    if (qi->channels == 0 || qi->rate == 0)
        return CODEC_ERR_PARAMS;
    if (have_ssnd && qi->pcm && qi->bits != 0) {
        /* el archivo puede estar cortado: no se cuenta más de lo que hay */
        unsigned long bpf = (unsigned long)((qi->bits + 7) / 8) * qi->channels;

        if (bpf != 0 && frames > ssnd_len / bpf)
            frames = ssnd_len / bpf;
    }
    strcpy(qi->format, is_aifc ? "AIFC" : "AIFF");
    qi->duration_ms = frames_to_ms(frames, qi->rate);
    return CODEC_OK;
}

/* ------------------------------------------------------------------------ */
/* AU de Sun (".snd")                                                        */
/* ------------------------------------------------------------------------ */

static int probe_au(const CodecIO *io, QtInfo *qi)
{
    uint8_t h[24];
    unsigned long off, datalen, enc, bps;
    long got, size = io->size;

    memset(qi, 0, sizeof *qi);
    got = rd(io, 0, h, 24);
    if (got < 0)
        return CODEC_ERR_IO;
    if (got < 24)
        return CODEC_ERR_UNSUPPORTED;
    off = be32(h + 4);
    datalen = be32(h + 8);
    enc = be32(h + 12);
    qi->rate = (unsigned)be32(h + 16);
    qi->channels = (unsigned)be32(h + 20);
    if (off < 24 || (long)off > size)
        return CODEC_ERR_UNSUPPORTED;
    if (qi->channels == 0 || qi->rate == 0)
        return CODEC_ERR_PARAMS;
    switch (enc) {
    case 1:  bps = 1; qi->pcm = 0; break;           /* mu-law de 8 bits */
    case 2:  bps = 1; qi->pcm = 1; break;
    case 3:  bps = 2; qi->pcm = 1; break;
    case 4:  bps = 3; qi->pcm = 1; break;
    case 5:  bps = 4; qi->pcm = 1; break;
    case 27: bps = 1; qi->pcm = 0; break;           /* A-law de 8 bits */
    default: bps = 0; qi->pcm = 0; break;           /* flotante, ADPCM...: no se sabe la duración */
    }
    qi->bits = (unsigned)(bps * 8);
    if (datalen == 0 || datalen > (unsigned long)(size - (long)off))     /* 0xFFFFFFFF = tamaño desconocido */
        datalen = (unsigned long)(size - (long)off);
    strcpy(qi->format, "AU");
    qi->duration_ms = bps ? frames_to_ms(datalen / (bps * qi->channels), qi->rate) : 0;
    return CODEC_OK;
}

int qtfmt_probe(const CodecIO *io, QtInfo *qi)
{
    uint8_t head[12];
    long got = rd(io, 0, head, 12);

    if (got < 0)
        return CODEC_ERR_IO;
    if (got < 4 || !qtfmt_sniff(head, (size_t)got))
        return CODEC_ERR_UNSUPPORTED;
    if (memcmp(head, "RIFF", 4) == 0)
        return probe_wav(io, qi);
    if (memcmp(head, "FORM", 4) == 0)
        return probe_aiff(io, memcmp(head + 8, "AIFC", 4) == 0, qi);
    return probe_au(io, qi);
}

/* ------------------------------------------------------------------------ */
/* Nombres                                                                   */
/* ------------------------------------------------------------------------ */

static int ends_with_ci(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix), i;

    if (n < m)
        return 0;
    for (i = 0; i < m; i++) {
        char c = s[n - m + i];

        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != suffix[i])
            return 0;
    }
    return 1;
}

int qtfmt_name_matches(const char *name, unsigned long mac_type)
{
    static const char *const kExt[] = { ".wav", ".aif", ".aiff", ".aifc", ".au", ".snd" };
    static const unsigned long kTypes[] = {
        0x57415645UL /* WAVE */, 0x41494646UL /* AIFF */, 0x41494643UL /* AIFC */, 0x554C4157UL /* ULAW */
    };
    size_t i;

    for (i = 0; i < sizeof kExt / sizeof kExt[0]; i++)
        if (ends_with_ci(name, kExt[i]))
            return 1;
    for (i = 0; i < sizeof kTypes / sizeof kTypes[0]; i++)
        if (mac_type == kTypes[i])
            return 1;
    return 0;
}
