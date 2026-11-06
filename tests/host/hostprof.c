/*
 * hostprof — comprueba en el PC la biblioteca parcheada con marcas de perfilado.
 *
 * Verifica que las marcas NO alteran el audio (CRC con reloj == CRC sin reloj) y
 * que las cubetas suman el tiempo medido. Las cifras son del PC: solo sirven para
 * validar la instrumentación, no dicen nada del 68k.
 *
 * Salida: líneas "prof,..." con el mismo esquema que hostdec (clock = NULL) y
 * líneas "#buckets,..." informativas. Uso: hostprof [-f FRAMES] archivo.mp3...
 */
#include "decoder_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned long long host_clock(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}

static const char *const kNames[PROF_COUNT] = {
    "side", "scalef", "huff_requant", "stereo", "reorder", "alias",
    "imdct_overlap", "other_decode", "monomix", "synth", "pcm"
};

int main(int argc, char **argv)
{
    unsigned long frames = 700;
    int i, rc = 0;

    for (i = 1; i < argc && argv[i][0] == '-'; i++)
        if (!strcmp(argv[i], "-f") && i + 1 < argc)
            frames = strtoul(argv[++i], NULL, 10);

    for (; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        unsigned char *data;
        long n;
        unsigned long fcrc;
        const char *base = strrchr(argv[i], '/');
        int p;

        if (!f) {
            fprintf(stderr, "no puedo leer %s\n", argv[i]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        data = malloc(n);
        if (fread(data, 1, n, f) != (size_t)n)
            return 1;
        fclose(f);
        base = base ? base + 1 : argv[i];
        fcrc = dec_crc32(data, n);

        for (p = 0; p < DEC_PROFILE_COUNT; p++) {
            DecConfig cfg;
            DecResult a, b;
            unsigned long long sum = 0;
            int k;

            memset(&cfg, 0, sizeof cfg);
            cfg.profile = (DecProfile)p;
            cfg.max_frames = frames;
            cfg.want_crc = 1;
            dec_run(data, n, &cfg, &a);                 /* sin reloj */
            printf("prof,%s,%08lX,%s,%lu,%lu,%lu,%u,%u,%08lX\n", base, fcrc,
                   dec_profile_name((DecProfile)p), a.frames, a.errors, a.samples_per_ch,
                   a.channels, a.out_rate, (unsigned long)a.crc);

            cfg.clock = host_clock;
            dec_run(data, n, &cfg, &b);                 /* con reloj */
            if (b.crc != a.crc) {
                fprintf(stderr, "%s %s: LAS MARCAS ALTERAN EL AUDIO (%08lX != %08lX)\n", base,
                        dec_profile_name((DecProfile)p), (unsigned long)b.crc, (unsigned long)a.crc);
                rc = 1;
            }
            for (k = 0; k < PROF_COUNT; k++)
                sum += b.prof_us[k];
            printf("#buckets,%s,%s,marks=%lu,sum_us=%llu", base, dec_profile_name((DecProfile)p),
                   b.prof_marks, sum);
            for (k = 0; k < PROF_COUNT; k++)
                printf(",%s=%llu", kNames[k], b.prof_us[k]);
            printf("\n");
        }
        free(data);
    }
    return rc;
}
