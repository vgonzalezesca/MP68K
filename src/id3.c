/*
 * id3 — ver id3.h. Las tablas de conversión se generaron con el códec mac_roman de Python
 * (con aproximación a ASCII para lo que MacRoman no tiene).
 */
#include "id3.h"

#include <string.h>

/* ISO-8859-1 0xA0..0xFF -> MacRoman */
static const unsigned char kLatin1High[96] = {
    0xCA, 0xC1, 0xA2, 0xA3, 0x3F, 0xB4, 0x7C, 0xA4, 0xAC, 0xA9, 0xBB, 0xC7,
    0xC2, 0x2D, 0xA8, 0xF8, 0xA1, 0xB1, 0x32, 0x33, 0xAB, 0xB5, 0xA6, 0xE1,
    0xFC, 0x31, 0xBC, 0xC8, 0x31, 0x31, 0x33, 0xC0, 0xCB, 0xE7, 0xE5, 0xCC,
    0x80, 0x81, 0xAE, 0x82, 0xE9, 0x83, 0xE6, 0xE8, 0xED, 0xEA, 0xEB, 0xEC,
    0x3F, 0x84, 0xF1, 0xEE, 0xEF, 0xCD, 0x85, 0x3F, 0xAF, 0xF4, 0xF2, 0xF3,
    0x86, 0x59, 0x3F, 0xA7, 0x88, 0x87, 0x89, 0x8B, 0x8A, 0x8C, 0xBE, 0x8D,
    0x8F, 0x8E, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95, 0x3F, 0x96, 0x98, 0x97,
    0x99, 0x9B, 0x9A, 0xD6, 0xBF, 0x9D, 0x9C, 0x9E, 0x9F, 0x79, 0x3F, 0xD8,
};

/* Windows-1252 0x80..0x9F -> MacRoman */
static const unsigned char kCp1252[32] = {
    0xDB, 0x3F, 0xE2, 0xC4, 0xE3, 0xC9, 0xA0, 0xE0, 0xF6, 0xE4, 0x53, 0xDC,
    0xCE, 0x3F, 0x5A, 0x3F, 0x3F, 0xD4, 0xD5, 0xD2, 0xD3, 0xA5, 0xD0, 0xD1,
    0xF7, 0xAA, 0x73, 0xDD, 0xCF, 0x3F, 0x7A, 0xD9,
};

/* Puntos de código fuera de Latin-1 que sí existen en MacRoman (o se aproximan). */
static const struct { unsigned short cp; unsigned char mac; } kExtra[] = {
    { 0x2013, 0xD0 },
    { 0x2014, 0xD1 },
    { 0x2018, 0xD4 },
    { 0x2019, 0xD5 },
    { 0x201A, 0xE2 },
    { 0x201C, 0xD2 },
    { 0x201D, 0xD3 },
    { 0x201E, 0xE3 },
    { 0x2020, 0xA0 },
    { 0x2022, 0xA5 },
    { 0x2026, 0xC9 },
    { 0x2030, 0xE4 },
    { 0x2039, 0xDC },
    { 0x203A, 0xDD },
    { 0x20AC, 0xDB },
    { 0x2122, 0xAA },
    { 0x0152, 0xCE },
    { 0x0153, 0xCF },
    { 0x0160, 0x53 },
    { 0x0161, 0x73 },
    { 0x0178, 0xD9 },
    { 0x017D, 0x5A },
    { 0x017E, 0x7A },
    { 0x0192, 0xC4 },
};

unsigned char id3_latin1_to_macroman(unsigned char c)
{
    if (c < 0x80)
        return c;
    if (c < 0xA0)
        return kCp1252[c - 0x80];
    return kLatin1High[c - 0xA0];
}

unsigned char id3_cp_to_macroman(unsigned long cp)
{
    unsigned i;

    if (cp < 0x100)
        return id3_latin1_to_macroman((unsigned char)cp);
    if (cp == 0x2212)
        return '-';                         /* signo menos */
    for (i = 0; i < sizeof kExtra / sizeof kExtra[0]; i++)
        if (kExtra[i].cp == cp)
            return kExtra[i].mac;
    return '?';
}

/* ------------------------------------------------------------------------ */

static unsigned long be32(const uint8_t *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static unsigned long synchsafe(const uint8_t *p)
{
    return ((unsigned long)(p[0] & 0x7F) << 21) | ((unsigned long)(p[1] & 0x7F) << 14) |
           ((unsigned long)(p[2] & 0x7F) << 7) | (p[3] & 0x7F);
}

long id3_v2_total_size(const uint8_t *b, size_t len)
{
    unsigned long size;
    long total;

    if (len < 10 || b[0] != 'I' || b[1] != 'D' || b[2] != '3')
        return 0;
    if (b[3] < 2 || b[3] > 4 || ((b[6] | b[7] | b[8] | b[9]) & 0x80))
        return 0;
    size = synchsafe(b + 6);
    total = (long)(10 + size);
    if (b[3] == 4 && (b[5] & 0x10))
        total += 10;                        /* pie de etiqueta */
    return total;
}

/* ------------------------------------------------------------------------ */
/* Decodificación de texto                                                   */
/* ------------------------------------------------------------------------ */

static void put_char(char *dst, size_t cap, size_t *n, unsigned char c)
{
    if (c < 0x20)
        c = ' ';
    if (*n + 1 < cap)
        dst[(*n)++] = (char)c;
}

static void finish(char *dst, size_t n)
{
    while (n > 0 && dst[n - 1] == ' ')
        n--;                                /* sin espacios al final */
    dst[n] = '\0';
}

/* d[0] = codificación; el resto es el texto. Se queda con el primer valor (hasta el primer NUL). */
static void decode_text(const uint8_t *d, size_t n, char *dst, size_t cap)
{
    size_t i = 1, out = 0;
    unsigned enc;

    if (n == 0 || cap == 0)
        return;
    enc = d[0];
    if (enc == 0) {                         /* ISO-8859-1 */
        for (; i < n && d[i] != 0; i++)
            put_char(dst, cap, &out, id3_latin1_to_macroman(d[i]));
    } else if (enc == 3) {                  /* UTF-8 */
        while (i < n && d[i] != 0) {
            unsigned long cp;
            unsigned char c = d[i++];
            int extra;

            if (c < 0x80) {
                cp = c;
                extra = 0;
            } else if ((c & 0xE0) == 0xC0) {
                cp = c & 0x1F;
                extra = 1;
            } else if ((c & 0xF0) == 0xE0) {
                cp = c & 0x0F;
                extra = 2;
            } else if ((c & 0xF8) == 0xF0) {
                cp = c & 0x07;
                extra = 3;
            } else {
                put_char(dst, cap, &out, '?');
                continue;
            }
            while (extra-- > 0 && i < n && (d[i] & 0xC0) == 0x80)
                cp = (cp << 6) | (d[i++] & 0x3F);
            put_char(dst, cap, &out, id3_cp_to_macroman(cp));
        }
    } else if (enc == 1 || enc == 2) {      /* UTF-16 con BOM (1) o sin BOM, big-endian (2) */
        int little = 0;

        if (enc == 1 && n >= 3) {
            if (d[1] == 0xFF && d[2] == 0xFE) {
                little = 1;
                i = 3;
            } else if (d[1] == 0xFE && d[2] == 0xFF) {
                i = 3;
            }
        }
        while (i + 1 < n) {
            unsigned long cp = little ? (unsigned long)(d[i] | (d[i + 1] << 8))
                                      : (unsigned long)((d[i] << 8) | d[i + 1]);

            i += 2;
            if (cp == 0)
                break;
            if (cp >= 0xD800 && cp <= 0xDFFF) {         /* par sustituto: no representable */
                if (cp < 0xDC00 && i + 1 < n)
                    i += 2;
                put_char(dst, cap, &out, '?');
                continue;
            }
            put_char(dst, cap, &out, id3_cp_to_macroman(cp));
        }
    }
    finish(dst, out);
}

/* Quita los 0x00 insertados tras 0xFF (desincronización) de src a dst. Devuelve el largo nuevo. */
static size_t deunsync(const uint8_t *src, size_t n, uint8_t *dst, size_t cap)
{
    size_t i, o = 0;

    for (i = 0; i < n && o < cap; i++) {
        dst[o++] = src[i];
        if (src[i] == 0xFF && i + 1 < n && src[i + 1] == 0x00)
            i++;
    }
    return o;
}

long id3_parse_v2(const uint8_t *b, size_t len, Id3Tags *out)
{
    long total = id3_v2_total_size(b, len);
    unsigned ver, flags;
    size_t pos = 10, end, hdr;

    if (total == 0)
        return 0;
    ver = b[3];
    flags = b[5];
    end = (size_t)total;
    if (ver == 4 && (flags & 0x10))
        end -= 10;
    if (end > len)
        end = len;                          /* etiqueta truncada: lo que haya */
    hdr = ver == 2 ? 6 : 10;

    if (ver >= 3 && (flags & 0x40)) {       /* cabecera extendida */
        if (pos + 4 > end)
            return total;
        pos += ver == 4 ? synchsafe(b + pos) : be32(b + pos) + 4;
    }

    while (pos + hdr <= end && b[pos] != 0) {
        const uint8_t *f = b + pos;
        unsigned long size;
        unsigned fflags = 0;
        char *dst = 0;
        size_t skip = 0;

        if (ver == 2)
            size = ((unsigned long)f[3] << 16) | ((unsigned long)f[4] << 8) | f[5];
        else {
            size = ver == 4 ? synchsafe(f + 4) : be32(f + 4);
            fflags = (unsigned)((f[8] << 8) | f[9]);
        }
        if (size == 0 || pos + hdr + size > end)
            break;

        if (ver == 2) {
            if (!memcmp(f, "TT2", 3)) dst = out->title;
            else if (!memcmp(f, "TP1", 3)) dst = out->artist;
            else if (!memcmp(f, "TAL", 3)) dst = out->album;
        } else {
            if (!memcmp(f, "TIT2", 4)) dst = out->title;
            else if (!memcmp(f, "TPE1", 4)) dst = out->artist;
            else if (!memcmp(f, "TALB", 4)) dst = out->album;
        }

        if (dst != 0 && dst[0] == '\0') {
            const uint8_t *d = f + hdr;
            size_t n = (size_t)size;
            uint8_t tmp[300];
            int skipframe = 0, unsync = 0;

            if (ver == 3) {
                if (fflags & 0x00C0) skipframe = 1;         /* comprimido o cifrado */
                if (fflags & 0x0020) skip += 1;             /* identificador de grupo */
            } else if (ver == 4) {
                if (fflags & 0x000C) skipframe = 1;
                if (fflags & 0x0040) skip += 1;
                if (fflags & 0x0001) skip += 4;             /* indicador de largo de datos */
                if (fflags & 0x0002) unsync = 1;
            }
            if (!skipframe && skip < n) {
                d += skip;
                n -= skip;
                if (unsync) {
                    n = deunsync(d, n > sizeof tmp ? sizeof tmp : n, tmp, sizeof tmp);
                    d = tmp;
                } else if (n > 256) {
                    n = 256;                                /* de sobra para 63 caracteres */
                }
                decode_text(d, n, dst, sizeof out->title);
            }
        }
        pos += hdr + size;
    }
    return total;
}

int id3_parse_v1(const uint8_t *block, Id3Tags *out)
{
    static const size_t kOff[3] = { 3, 33, 63 };      /* título, artista, álbum */
    int k;

    if (block[0] != 'T' || block[1] != 'A' || block[2] != 'G')
        return 0;
    for (k = 0; k < 3; k++) {
        char *dst = k == 0 ? out->title : (k == 1 ? out->artist : out->album);
        size_t n = 0, i;

        if (dst[0] != '\0')
            continue;
        for (i = 0; i < 30 && block[kOff[k] + i] != 0; i++)
            put_char(dst, sizeof out->title, &n, id3_latin1_to_macroman(block[kOff[k] + i]));
        finish(dst, n);
    }
    return 1;
}
