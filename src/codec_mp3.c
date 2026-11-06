/*
 * codec_mp3 — MP3 como códec: envuelve dec_stream_* (libmad), id3 y mp3info.
 * El PCM es el mismo de siempre: los CRC de tests/data/REFERENCE.txt no pueden cambiar.
 */
#include "codec.h"

#include <string.h>

#include "mp3info.h"

_Static_assert(sizeof(Mp3Info) <= sizeof(((CodecInfo *)0)->priv), "CodecInfo.priv es chico para Mp3Info");

static Mp3Info sMi;     /* datos del archivo abierto (para buscar) */

static int read_exact(const CodecIO *io, long offset, uint8_t *dst, long want, long *got)
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
    *got = total;
    return 0;
}

static int mp3_sniff(const uint8_t *head, size_t n)
{
    (void)head;
    (void)n;
    return 1;           /* último recurso: mp3info_parse decide si hay un frame válido */
}

static int mp3_probe(const CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci)
{
    uint8_t v1[128];
    Mp3Info mi;
    long got = 0, v2, size = io->size;
    long want = cap < 8192 ? cap : 8192;
    int has_v1 = 0;

    if (read_exact(io, 0, buf, want, &got) != 0)
        return CODEC_ERR_IO;
    v2 = id3_parse_v2(buf, (size_t)got, &ci->tags);     /* 0 si no hay; tolera etiquetas truncadas */
    if (size >= 128) {
        long n = 0;

        if (read_exact(io, size - 128, v1, 128, &n) == 0 && n == 128)
            has_v1 = id3_parse_v1(v1, &ci->tags);
    }
    if (v2 >= size || read_exact(io, v2, buf, want, &got) != 0)
        return CODEC_ERR_IO;
    if (mp3info_parse(buf, (size_t)got, v2, size, has_v1, &mi) != 0)
        return CODEC_ERR_UNSUPPORTED;

    strcpy(ci->format, "MP3");
    ci->rate = mi.sample_rate;
    ci->channels = mi.channels;
    ci->bits_src = 0;
    ci->vbr = mi.is_vbr != 0;
    ci->duration_ms = mi.duration_ms;
    /* En VBR el bitrate del primer frame no dice nada: se muestra el promedio. */
    ci->bitrate = (ci->vbr && mi.duration_ms)
        ? (unsigned)((unsigned long long)mi.audio_bytes * 8000ULL / mi.duration_ms)
        : mi.bitrate;
    ci->has_profiles = 1;
    ci->eq_capable = 1;
    memcpy(ci->priv, &mi, sizeof mi);
    return CODEC_OK;
}

static int mp3_open(const CodecIO *io, DecProfile prof, uint8_t *buf, size_t bufsize, const CodecInfo *ci)
{
    memcpy(&sMi, ci->priv, sizeof sMi);
    return dec_stream_open(prof, io->read, io->seek, io->ctx, buf, bufsize) == 0 ? CODEC_OK : CODEC_ERR_IO;
}

static int mp3_next(DecFrame *out)
{
    return dec_stream_next(out);
}

/* Byte donde está el instante ms: tabla de Xing si hay; si no, lineal (exacta en CBR). */
static int mp3_seek(unsigned long ms)
{
    return dec_stream_seek(mp3info_offset_for_ms(&sMi, ms)) == 0 ? CODEC_OK : CODEC_ERR_IO;
}

static void mp3_close(void)
{
    dec_stream_close();
}

const Codec codec_mp3 = { "MP3", mp3_sniff, mp3_probe, mp3_open, mp3_next, mp3_seek, mp3_close };
