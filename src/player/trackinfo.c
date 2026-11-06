/*
 * trackinfo — ver trackinfo.h.
 */
#include "trackinfo.h"

#include <stdio.h>
#include <string.h>

OSErr trackinfo_probe(FileReader *fr, CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci, const Codec **codec)
{
    int err = 0;

    io->read = fileread_read;
    io->seek = fileread_seek;
    io->ctx = fr;
    io->size = fr->size;
    io->audio_start = 0;
    *codec = codec_probe_any(io, buf, cap, ci, &err);
    if (*codec != NULL)
        return noErr;
    switch (err) {
    case CODEC_ERR_IO:     return ioErr;
    case CODEC_ERR_PARAMS: return TRACKINFO_ERR_PARAMS;
    case CODEC_ERR_MEMORY: return memFullErr;
    default:               return TRACKINFO_ERR_UNSUPPORTED;
    }
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
