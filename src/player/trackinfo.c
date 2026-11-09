/*
 * trackinfo — ver trackinfo.h.
 */
#include "trackinfo.h"

#include <stdio.h>
#include <string.h>

#include "qtfmt.h"
#include "qtplay.h"

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

OSErr trackinfo_probe_ex(FileReader *fr, CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci, const Codec **codec,
                         Boolean *via_qt)
{
    uint8_t head[12];
    long got = 0;

    *via_qt = false;
    *codec = NULL;
    if (fileread_seek(fr, 0) == 0 && fileread_read(fr, head, sizeof head, &got) == 0 && got >= 4 &&
        qtfmt_sniff(head, (size_t)got)) {
        QtInfo qi;
        int rc;

        if (!qtplay_available())
            return TRACKINFO_ERR_NO_QUICKTIME;
        io->read = fileread_read;
        io->seek = fileread_seek;
        io->ctx = fr;
        io->size = fr->size;
        io->audio_start = 0;
        rc = qtfmt_probe(io, &qi);
        if (rc != CODEC_OK)
            return rc == CODEC_ERR_IO ? ioErr : (rc == CODEC_ERR_PARAMS ? TRACKINFO_ERR_PARAMS : TRACKINFO_ERR_UNSUPPORTED);
        memset(ci, 0, sizeof *ci);
        strcpy(ci->format, qi.format);
        ci->rate = qi.rate;
        ci->channels = qi.channels;
        ci->bits_src = qi.bits;
        ci->duration_ms = qi.duration_ms;
        *via_qt = true;
        return noErr;
    }
    return trackinfo_probe(fr, io, buf, cap, ci, codec);
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
