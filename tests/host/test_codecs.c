/*
 * test_codecs — prueba en el PC la capa de códecs (src/codec*.c) con los mismos fuentes que la app.
 *
 *   test_codecs info   ARCHIVO
 *   test_codecs decode ARCHIVO SALIDA.raw [perfil 0-3] [arena_kb]
 *   test_codecs seek   ARCHIVO MS SALIDA.raw CUADROS [perfil]
 *
 * "info" imprime clave=valor (las etiquetas como bytes en hexadecimal, ya en MacRoman).
 * "decode" escribe todo el PCM como int16 little-endian y reporta uso del arena. "seek" abre,
 * busca a MS y escribe los primeros CUADROS cuadros. check_codecs.py compara con las referencias.
 */
#include "codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *f;
} HostFile;

static int host_read(void *ctx, uint8_t *dst, long max, long *got)
{
    HostFile *h = ctx;

    *got = (long)fread(dst, 1, (size_t)max, h->f);
    return 0;
}

static int host_seek(void *ctx, long offset)
{
    HostFile *h = ctx;

    return fseek(h->f, offset, SEEK_SET) == 0 ? 0 : -1;
}

static void print_hex(const char *key, const char *s)
{
    printf("%s=", key);
    while (*s)
        printf("%02X", (unsigned char)*s++);
    printf("\n");
}

static int open_file(const char *path, HostFile *h, CodecIO *io)
{
    long size;

    h->f = fopen(path, "rb");
    if (!h->f)
        return -1;
    fseek(h->f, 0, SEEK_END);
    size = ftell(h->f);
    fseek(h->f, 0, SEEK_SET);
    io->read = host_read;
    io->seek = host_seek;
    io->ctx = h;
    io->size = size;
    io->audio_start = 0;
    return 0;
}

static void print_info(const Codec *c, const CodecInfo *ci)
{
    printf("codec=%s\nformat=%s\nrate=%u\nchannels=%u\nbits=%u\nbitrate=%u\nvbr=%d\nduration_ms=%lu\n"
           "profiles=%d\neq=%d\n",
           c->name, ci->format, ci->rate, ci->channels, ci->bits_src, ci->bitrate, ci->vbr, ci->duration_ms,
           ci->has_profiles, ci->eq_capable);
    print_hex("title", ci->tags.title);
    print_hex("artist", ci->tags.artist);
    print_hex("album", ci->tags.album);
}

int main(int argc, char **argv)
{
    static uint8_t probe_buf[32768];
    static uint8_t read_buf[32768];
    HostFile hf;
    CodecIO io;
    CodecInfo ci;
    const Codec *c;
    int err = 0, rc;
    unsigned arena_kb = 768;
    DecProfile prof = DEC_PROFILE_FULL;
    FILE *out = NULL;
    void *arena_mem;
    unsigned long frames = 0, want = 0;
    const char *mode;

    if (argc < 3) {
        fprintf(stderr, "uso: test_codecs info|decode|seek ARCHIVO ...\n");
        return 2;
    }
    mode = argv[1];
    if (!strcmp(mode, "decode")) {
        if (argc < 4)
            return 2;
        if (argc > 4)
            prof = (DecProfile)atoi(argv[4]);
        if (argc > 5)
            arena_kb = (unsigned)atoi(argv[5]);
    } else if (!strcmp(mode, "seek")) {
        if (argc < 6)
            return 2;
        if (argc > 6)
            prof = (DecProfile)atoi(argv[6]);
    }

    arena_mem = malloc((size_t)arena_kb * 1024);
    codec_init(arena_mem, (size_t)arena_kb * 1024);

    if (open_file(argv[2], &hf, &io) != 0) {
        printf("error=nofile\n");
        return 1;
    }
    c = codec_probe_any(&io, probe_buf, sizeof probe_buf, &ci, &err);
    if (c == NULL) {
        printf("error=%d\n", err);
        return 1;
    }
    if (!strcmp(mode, "info")) {
        print_info(c, &ci);
        return 0;
    }

    out = fopen(argv[!strcmp(mode, "seek") ? 4 : 3], "wb");
    if (!out)
        return 2;
    rc = c->open(&io, prof, read_buf, sizeof read_buf, &ci);
    if (rc != CODEC_OK) {
        printf("error=open:%d\n", rc);
        return 1;
    }
    if (!strcmp(mode, "seek")) {
        want = strtoul(argv[5], NULL, 10);
        if (c->seek(strtoul(argv[3], NULL, 10)) != CODEC_OK) {
            printf("error=seek\n");
            return 1;
        }
    }
    for (;;) {
        DecFrame f;

        rc = c->next(&f);
        if (rc != 1)
            break;
        if (want && frames + f.frames > want)
            f.frames = (unsigned)(want - frames), f.count = f.frames * f.channels;
        fwrite(f.pcm, sizeof(int16_t), f.count, out);
        frames += f.frames;
        if (want && frames >= want)
            break;
        if (f.frames > CODEC_MAX_FRAMES) {
            printf("error=slice_too_big:%u\n", f.frames);
            return 1;
        }
    }
    c->close();
    fclose(out);
    printf("frames=%lu\nend=%d\narena_peak=%lu\narena_size=%lu\n", frames, rc,
           (unsigned long)codec_arena()->peak, (unsigned long)codec_arena()->size);
    return 0;
}
