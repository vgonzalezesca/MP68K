/*
 * trackinfo — ver trackinfo.h.
 */
#include "trackinfo.h"

#include <stdio.h>
#include <string.h>

OSErr trackinfo_probe(FileReader *fr, uint8_t *buf, long cap, Id3Tags *tg, Mp3Info *mi)
{
    uint8_t v1[128];
    long got = 0, v2 = 0, size = fr->size;
    long want = cap < 8192 ? cap : 8192;
    int has_v1 = 0;

    memset(tg, 0, sizeof *tg);
    if (fileread_seek(fr, 0) != 0 || fileread_read(fr, buf, want, &got) != 0)
        return ioErr;
    v2 = id3_parse_v2(buf, (size_t)got, tg);        /* 0 si no hay; tolera etiquetas truncadas */
    if (size >= 128 && fileread_seek(fr, size - 128) == 0) {
        long n = 0;

        if (fileread_read(fr, v1, 128, &n) == 0 && n == 128)
            has_v1 = id3_parse_v1(v1, tg);
    }
    if (v2 >= size || fileread_seek(fr, v2) != 0 || fileread_read(fr, buf, want, &got) != 0)
        return ioErr;
    return mp3info_parse(buf, (size_t)got, v2, size, has_v1, mi) == 0 ? noErr : TRACKINFO_ERR_NOT_MP3;
}

void trackinfo_name_without_extension(const char *name, char *out, size_t cap)
{
    size_t n = strlen(name);
    const char *dot = strrchr(name, '.');

    if (dot != NULL && dot != name && n - (size_t)(dot - name) <= 5)
        n = (size_t)(dot - name);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, name, n);
    out[n] = '\0';
}

void trackinfo_display_name(const char *filename, const Id3Tags *tg, char *out, size_t cap)
{
    if (tg->title[0] != '\0' && tg->artist[0] != '\0')
        snprintf(out, cap, "%s - %s", tg->artist, tg->title);
    else if (tg->title[0] != '\0')
        snprintf(out, cap, "%s", tg->title);
    else
        trackinfo_name_without_extension(filename, out, cap);
}
