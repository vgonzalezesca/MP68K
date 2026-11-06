/*
 * hostdec — decodificador de referencia en el PC (oráculo para el 68k).
 *
 * Usa exactamente el mismo src/decoder_core.c que la aplicación Mac. Para cada
 * archivo y cada perfil imprime una línea CSV con el CRC-32 del PCM:
 *   kind,file,file_crc,profile,frames,errors,samples_per_ch,channels,out_rate,pcm_crc
 * kind = "bench" (primeros FRAMES frames, como decbench) o "full" (archivo entero).
 *
 * Uso: hostdec [-f FRAMES] [-d DIR_PCM] archivo.mp3...
 *   -d DIR   además escribe DIR/<archivo>.<perfil>.raw (int16 little-endian,
 *            archivo entero) para compararlo con otras herramientas.
 */
#include "decoder_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long n;

    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(n > 0 ? n : 1);
    if (buf && fread(buf, 1, n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    *size = n;
    return buf;
}

static void sink_file(const int16_t *pcm, unsigned count, void *ctx)
{
    fwrite(pcm, sizeof(int16_t), count, (FILE *)ctx);
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

int main(int argc, char **argv)
{
    unsigned long bench_frames = 300;
    const char *pcm_dir = NULL;
    int i, rc = 0;

    for (i = 1; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc)
            bench_frames = strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "-d") && i + 1 < argc)
            pcm_dir = argv[++i];
        else {
            fprintf(stderr, "uso: hostdec [-f FRAMES] [-d DIR_PCM] archivo.mp3...\n");
            return 2;
        }
    }

    printf("# %s\n", dec_library_info());

    for (; i < argc; i++) {
        size_t size;
        uint8_t *data = read_file(argv[i], &size);
        unsigned long fcrc;
        int p, pass;

        if (!data) {
            fprintf(stderr, "no puedo leer %s\n", argv[i]);
            rc = 1;
            continue;
        }
        fcrc = dec_crc32(data, size);

        for (p = 0; p < DEC_PROFILE_COUNT; p++)
            for (pass = 0; pass < 2; pass++) {
                DecConfig cfg;
                DecResult res;
                FILE *raw = NULL;

                memset(&cfg, 0, sizeof cfg);
                cfg.profile = (DecProfile)p;
                cfg.want_crc = 1;
                cfg.max_frames = pass == 0 ? bench_frames : 0;

                if (pass == 1 && pcm_dir) {
                    char path[1024];
                    snprintf(path, sizeof path, "%s/%s.%s.raw", pcm_dir,
                             base_name(argv[i]), dec_profile_name((DecProfile)p));
                    raw = fopen(path, "wb");
                    if (raw) {
                        cfg.pcm_sink = sink_file;
                        cfg.sink_ctx = raw;
                    }
                }

                if (dec_run(data, size, &cfg, &res) != 0) {
                    fprintf(stderr, "%s: no se pudo decodificar\n", argv[i]);
                    rc = 1;
                }
                if (raw)
                    fclose(raw);

                printf("%s,%s,%08lX,%s,%lu,%lu,%lu,%u,%u,%08lX\n",
                       pass == 0 ? "bench" : "full", base_name(argv[i]), fcrc,
                       dec_profile_name((DecProfile)p), res.frames, res.errors,
                       res.samples_per_ch, res.channels, res.out_rate,
                       (unsigned long)res.crc);
            }
        free(data);
    }
    return rc;
}
