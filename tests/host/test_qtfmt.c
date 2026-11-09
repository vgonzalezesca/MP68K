/*
 * test_qtfmt — prueba de src/qtfmt.c contra los valores de ffprobe/sox (los pasa tools/host-test.sh).
 *
 *   test_qtfmt names                               prueba qtfmt_name_matches (extensiones y tipos de Mac)
 *   test_qtfmt ok  <archivo> <FORMATO> <tasa> <canales> <ms> <pcm>   debe reconocerlo con esos datos (ms con ±1)
 *   test_qtfmt err <archivo> <UNSUPPORTED|PARAMS>    debe rechazarlo con ese código
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qtfmt.h"

static int rd(void *ctx, uint8_t *dst, long max, long *got)
{
    *got = (long)fread(dst, 1, (size_t)max, (FILE *)ctx);
    return 0;
}

static int sk(void *ctx, long off)
{
    return fseek((FILE *)ctx, off, SEEK_SET);
}

static int names(void)
{
    static const struct { const char *n; unsigned long t; int want; } c[] = {
        { "a.wav", 0, 1 }, { "A.WAV", 0, 1 }, { "a.aif", 0, 1 }, { "a.aiff", 0, 1 }, { "a.aifc", 0, 1 }, { "a.au", 0, 1 },
        { "a.snd", 0, 1 }, { "sin extension", 0x57415645UL, 1 }, { "x", 0x41494646UL, 1 }, { "x", 0x41494643UL, 1 },
        { "x", 0x554C4157UL, 1 },
        { "a.mp3", 0, 0 }, { "a.flac", 0, 0 }, { "a.ogg", 0, 0 }, { "a.wma", 0, 0 }, { "a.txt", 0, 0 },
        { "x", 0x4D504733UL, 0 } /* MPG3 */, { "x", 0x464C4143UL, 0 } /* FLAC */, { "wav", 0, 0 }, { "", 0, 0 },
    };
    unsigned i;
    int bad = 0;

    for (i = 0; i < sizeof c / sizeof c[0]; i++)
        if (qtfmt_name_matches(c[i].n, c[i].t) != c[i].want) {
            printf("FALLA names: '%s' tipo %08lX esperado %d\n", c[i].n, c[i].t, c[i].want);
            bad++;
        }
    printf("names: %s\n", bad ? "FALLA" : "OK");
    return bad != 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    CodecIO io;
    QtInfo qi;
    long size;
    int rc;

    if (argc >= 2 && strcmp(argv[1], "names") == 0)
        return names();
    if (argc < 4) {
        fprintf(stderr, "uso: ver el comentario del archivo\n");
        return 2;
    }
    f = fopen(argv[2], "rb");
    if (!f) {
        perror(argv[2]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    memset(&io, 0, sizeof io);
    io.read = rd;
    io.seek = sk;
    io.ctx = f;
    io.size = size;
    rc = qtfmt_probe(&io, &qi);
    fclose(f);

    if (strcmp(argv[1], "err") == 0) {
        int want = strcmp(argv[3], "PARAMS") == 0 ? CODEC_ERR_PARAMS : CODEC_ERR_UNSUPPORTED;

        if (rc != want) {
            printf("FALLA %s: rc=%d esperado %d\n", argv[2], rc, want);
            return 1;
        }
        printf("OK    %s: rechazado (%s)\n", argv[2], argv[3]);
        return 0;
    }
    if (argc < 8) {
        fprintf(stderr, "faltan argumentos\n");
        return 2;
    }
    {
        unsigned long ms = strtoul(argv[6], NULL, 10);
        long d = (long)qi.duration_ms - (long)ms;
        int bad = rc != CODEC_OK || strcmp(qi.format, argv[3]) != 0 || qi.rate != (unsigned)atoi(argv[4]) ||
                  qi.channels != (unsigned)atoi(argv[5]) || d < -1 || d > 1 || qi.pcm != atoi(argv[7]);

        printf("%s %s: rc=%d %s %u Hz %u ch %u bits %lu ms pcm=%d (esperado %s %s Hz %s ch %lu ms pcm=%s)\n",
               bad ? "FALLA" : "OK   ", argv[2], rc, qi.format, qi.rate, qi.channels, qi.bits, qi.duration_ms, qi.pcm,
               argv[3], argv[4], argv[5], ms, argv[7]);
        return bad;
    }
}
