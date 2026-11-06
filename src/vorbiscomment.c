/*
 * vorbiscomment — ver vorbiscomment.h.
 */
#include "vorbiscomment.h"

#include <string.h>

/* Cuántas continuaciones lleva el byte inicial `c` y su parte del punto de código; -1 si no es inicio válido. */
static int utf8_lead(uint8_t c, unsigned long *cp)
{
    if (c < 0x80) {
        *cp = c;
        return 0;
    }
    if (c >= 0xC2 && c < 0xE0) {
        *cp = c & 0x1F;
        return 1;
    }
    if (c >= 0xE0 && c < 0xF0) {
        *cp = c & 0x0F;
        return 2;
    }
    if (c >= 0xF0 && c < 0xF5) {
        *cp = c & 0x07;
        return 3;
    }
    return -1;
}

void vorbiscomment_utf8_to_macroman(const uint8_t *s, size_t n, char *dst, size_t cap)
{
    size_t i = 0, o = 0;

    if (cap == 0)
        return;
    while (i < n && o + 1 < cap) {
        unsigned long cp = '?';
        int extra = utf8_lead(s[i], &cp);

        if (extra < 0) {                    /* byte suelto inválido */
            cp = '?';
            i++;
        } else if (i + (size_t)extra >= n) {    /* secuencia cortada al final */
            cp = '?';
            i = n;
        } else {
            int k, ok = 1;

            for (k = 1; k <= extra; k++) {
                if ((s[i + (size_t)k] & 0xC0) != 0x80) {
                    ok = 0;
                    break;
                }
                cp = (cp << 6) | (s[i + (size_t)k] & 0x3F);
            }
            if (ok) {
                i += 1 + (size_t)extra;
            } else {
                cp = '?';
                i++;                        /* se salta solo el byte inicial */
            }
        }
        dst[o++] = cp < 0x80 ? (char)cp : (char)id3_cp_to_macroman(cp);
    }
    dst[o] = '\0';
}

static int key_is(const uint8_t *e, size_t keylen, const char *want)
{
    size_t i;

    if (strlen(want) != keylen)
        return 0;
    for (i = 0; i < keylen; i++) {
        char c = (char)e[i];

        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (c != want[i])
            return 0;
    }
    return 1;
}

int vorbiscomment_entry(const uint8_t *e, size_t n, Id3Tags *tg)
{
    size_t eq;
    char *dst = NULL;
    size_t cap = 0;

    for (eq = 0; eq < n && e[eq] != '='; eq++)
        ;
    if (eq == n)
        return 0;
    if (key_is(e, eq, "TITLE") && tg->title[0] == '\0') {
        dst = tg->title;
        cap = sizeof tg->title;
    } else if (key_is(e, eq, "ARTIST") && tg->artist[0] == '\0') {
        dst = tg->artist;
        cap = sizeof tg->artist;
    } else if (key_is(e, eq, "ALBUM") && tg->album[0] == '\0') {
        dst = tg->album;
        cap = sizeof tg->album;
    }
    if (dst == NULL)
        return 0;
    vorbiscomment_utf8_to_macroman(e + eq + 1, n - eq - 1, dst, cap);
    return 1;
}
