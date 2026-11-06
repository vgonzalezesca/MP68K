/*
 * mp3info — ver mp3info.h.
 */
#include "mp3info.h"

#include <string.h>

static const unsigned short kBrV1L1[15] = { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 };
static const unsigned short kBrV1L2[15] = { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 };
static const unsigned short kBrV1L3[15] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
static const unsigned short kBrV2L1[15] = { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 };
static const unsigned short kBrV2L23[15] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 };

static unsigned long be32(const uint8_t *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

typedef struct {
    unsigned version, layer, bitrate, rate, channels, spf, protection, padding;
    unsigned long length;       /* bytes del frame */
} Hdr;

/* Interpreta una cabecera de 4 bytes. Devuelve 0 si es válida. */
static int parse_header(const uint8_t *p, Hdr *h)
{
    unsigned long w = be32(p);
    unsigned vers = (unsigned)((w >> 19) & 3), layer = (unsigned)((w >> 17) & 3);
    unsigned bri = (unsigned)((w >> 12) & 15), sri = (unsigned)((w >> 10) & 3);
    static const unsigned kRate[3] = { 44100, 48000, 32000 };

    if ((w & 0xFFE00000UL) != 0xFFE00000UL || vers == 1 || layer == 0 || bri == 0 || bri == 15 || sri == 3)
        return -1;
    h->version = vers == 3 ? 1 : (vers == 2 ? 2 : 25);
    h->layer = 4 - layer;
    h->rate = kRate[sri] / (vers == 3 ? 1 : (vers == 2 ? 2 : 4));
    if (h->layer == 1)
        h->bitrate = (h->version == 1 ? kBrV1L1[bri] : kBrV2L1[bri]) * 1000u;
    else if (h->layer == 2)
        h->bitrate = (h->version == 1 ? kBrV1L2[bri] : kBrV2L23[bri]) * 1000u;
    else
        h->bitrate = (h->version == 1 ? kBrV1L3[bri] : kBrV2L23[bri]) * 1000u;
    h->protection = (unsigned)((w >> 16) & 1) ? 0 : 1;      /* 1 = hay CRC de 2 bytes */
    h->padding = (unsigned)((w >> 9) & 1);
    h->channels = ((w >> 6) & 3) == 3 ? 1 : 2;
    h->spf = h->layer == 1 ? 384 : (h->layer == 3 && h->version != 1 ? 576 : 1152);
    if (h->layer == 1)
        h->length = (12UL * h->bitrate / h->rate + h->padding) * 4;
    else
        h->length = (h->layer == 3 && h->version != 1 ? 72UL : 144UL) * h->bitrate / h->rate + h->padding;
    return 0;
}

int mp3info_parse(const uint8_t *buf, size_t len, long audio_start, long file_size, int has_id3v1,
                  Mp3Info *out)
{
    Hdr h;
    size_t i, limit = len > 4096 ? 4096 : len, tag;
    unsigned side;

    memset(out, 0, sizeof *out);
    for (i = 0; i + 4 <= limit; i++) {
        Hdr n;

        if (buf[i] != 0xFF || parse_header(buf + i, &h) != 0)
            continue;
        /* Confirma con la cabecera del frame siguiente si está en el buffer (evita falsos positivos). */
        if (i + h.length + 4 <= len && parse_header(buf + i + h.length, &n) != 0)
            continue;
        break;
    }
    if (i + 4 > limit)
        return -1;

    out->version = h.version;
    out->layer = h.layer;
    out->bitrate = h.bitrate;
    out->sample_rate = h.rate;
    out->channels = h.channels;
    out->samples_per_frame = h.spf;
    out->audio_start = audio_start;
    out->first_frame_offset = (long)i;
    out->audio_bytes = file_size - audio_start - (has_id3v1 ? 128 : 0);
    if (out->audio_bytes < 0)
        out->audio_bytes = 0;

    /* Cabecera Xing/Info: tras los 4 bytes de cabecera, el CRC opcional y la información lateral. */
    side = h.version == 1 ? (h.channels == 1 ? 17 : 32) : (h.channels == 1 ? 9 : 17);
    tag = i + 4 + (h.protection ? 2 : 0) + side;
    if (h.layer == 3 && tag + 12 <= len && (!memcmp(buf + tag, "Xing", 4) || !memcmp(buf + tag, "Info", 4))) {
        unsigned long flags = be32(buf + tag + 4);
        size_t p = tag + 8;

        out->has_vbr_tag = 1;
        out->is_vbr = buf[tag] == 'X';
        if ((flags & 1) && p + 4 <= len) {
            out->frames = be32(buf + p);
            p += 4;
        }
        if ((flags & 2) && p + 4 <= len) {
            out->tag_bytes = be32(buf + p);
            p += 4;
        }
        if ((flags & 4) && p + 100 <= len) {
            memcpy(out->toc, buf + p, 100);
            out->has_toc = 1;
        }
    } else if (h.layer == 3 && i + 36 + 18 <= len && !memcmp(buf + i + 36, "VBRI", 4)) {
        out->has_vbr_tag = 1;
        out->is_vbr = 1;
        out->tag_bytes = be32(buf + i + 36 + 10);
        out->frames = be32(buf + i + 36 + 14);
    }

    if (out->frames > 0 && h.rate > 0) {
        out->duration_ms = (unsigned long)((unsigned long long)out->frames * h.spf * 1000ULL / h.rate);
        out->duration_exact = 1;
    } else if (h.bitrate > 0) {
        out->duration_ms = (unsigned long)((unsigned long long)out->audio_bytes * 8000ULL / h.bitrate);
        out->duration_exact = 0;
    }
    return 0;
}

long mp3info_offset_for_ms(const Mp3Info *m, unsigned long ms)
{
    unsigned long long bytes;

    if (m->duration_ms == 0)
        return m->audio_start;
    if (ms > m->duration_ms)
        ms = m->duration_ms;

    if (m->has_toc && m->is_vbr) {
        unsigned long long pos = (unsigned long long)ms * 100000ULL / m->duration_ms;     /* porcentaje x 1000 */
        unsigned idx = (unsigned)(pos / 1000), a, b;
        unsigned long rem = (unsigned long)(pos % 1000), fx;

        if (idx >= 100) {
            idx = 99;
            rem = 1000;
        }
        a = m->toc[idx];
        b = idx < 99 ? m->toc[idx + 1] : 256;
        if (b < a)
            b = a;
        fx = a * 1000UL + (b - a) * rem;                    /* posición x 256 x 1000 */
        bytes = m->tag_bytes ? m->tag_bytes : (unsigned long)m->audio_bytes;
        return m->audio_start + (long)(bytes * fx / 256000ULL);
    }
    return m->audio_start + (long)((unsigned long long)m->audio_bytes * ms / m->duration_ms);
}
