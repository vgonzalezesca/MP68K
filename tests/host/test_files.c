/*
 * test_files — pruebas del PC de mp3info e id3 sobre archivos reales.
 *
 * Uso: test_files archivo.mp3 duracion_esperada_ms [toc|info|none] [tags]
 *   duracion_esperada_ms: de ffprobe.   toc = cabecera Xing con tabla; info = cabecera Info;
 *   none = sin cabecera.   tags = debe traer las etiquetas de prueba.
 *
 * La correspondencia tiempo -> byte se comprueba decodificando el archivo entero: se registra
 * en qué byte empieza cada frame y su instante, y se compara con mp3info_offset_for_ms.
 */
#include "decoder_core.h"
#include "id3.h"
#include "mp3info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { const uint8_t *d; long size, pos; } Mem;

static int mem_read(void *ctx, uint8_t *dst, long max, long *got)
{
    Mem *m = ctx;
    long n = m->size - m->pos;

    if (n > max) n = max;
    if (n < 0) n = 0;
    memcpy(dst, m->d + m->pos, n);
    m->pos += n;
    *got = n;
    return 0;
}

static int mem_seek(void *ctx, long off)
{
    Mem *m = ctx;
    if (off < 0 || off > m->size) return -1;
    m->pos = off;
    return 0;
}

int main(int argc, char **argv)
{
    FILE *fp;
    uint8_t *data;
    long size, v2, audio_start;
    int has_v1, expect_tags = 0;
    unsigned long want_ms, tol;
    const char *expect = argc > 3 ? argv[3] : "none";
    Mp3Info mi;
    Id3Tags tg;
    int i;

    if (argc < 3) { fprintf(stderr, "uso: test_files archivo ms [toc|info|none] [tags]\n"); return 2; }
    for (i = 4; i < argc; i++) if (!strcmp(argv[i], "tags")) expect_tags = 1;
    want_ms = strtoul(argv[2], NULL, 10);

    fp = fopen(argv[1], "rb");
    if (!fp) { fprintf(stderr, "no puedo abrir %s\n", argv[1]); return 2; }
    fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 0, SEEK_SET);
    data = malloc(size);
    if (fread(data, 1, size, fp) != (size_t)size) return 2;
    fclose(fp);

    v2 = id3_v2_total_size(data, size);
    has_v1 = size >= 128 && !memcmp(data + size - 128, "TAG", 3);
    audio_start = v2;

    CHECK(mp3info_parse(data + audio_start, (size_t)(size - audio_start) > 8192 ? 8192 : (size_t)(size - audio_start),
                        audio_start, size, has_v1, &mi) == 0, "mp3info_parse fallo");
    printf("  %s: v%u L%u %u kbps %u Hz %uch, tag=%s toc=%d frames=%lu, duracion %lu ms (%s), inicio %ld\n", argv[1],
           mi.version, mi.layer, mi.bitrate / 1000, mi.sample_rate, mi.channels,
           mi.has_vbr_tag ? (mi.is_vbr ? "Xing/VBR" : "Info") : "ninguna", mi.has_toc, mi.frames, mi.duration_ms,
           mi.duration_exact ? "exacta" : "estimada", mi.audio_start);

    if (!strcmp(expect, "toc"))
        CHECK(mi.has_vbr_tag && mi.is_vbr && mi.has_toc, "se esperaba Xing con tabla");
    else if (!strcmp(expect, "info"))
        CHECK(mi.has_vbr_tag && !mi.is_vbr && mi.frames > 0, "se esperaba cabecera Info");
    else
        CHECK(!mi.has_vbr_tag, "no se esperaba cabecera");

    tol = mi.duration_exact ? 60 : 150;
    CHECK(mi.duration_ms + tol >= want_ms && mi.duration_ms <= want_ms + tol,
          "duracion %lu ms vs ffprobe %lu ms (tolerancia %lu)", mi.duration_ms, want_ms, tol);

    if (expect_tags) {
        memset(&tg, 0, sizeof tg);
        id3_parse_v2(data, size, &tg);
        id3_parse_v1(data + size - 128, &tg);
        CHECK(!strcmp(tg.title, "Canci\x97n de prueba"), "titulo '%s'", tg.title);
        CHECK(!strcmp(tg.artist, "\x84" "and\x9C"), "artista '%s'", tg.artist);
        CHECK(!strcmp(tg.album, "\xE7" "lbum"), "album '%s'", tg.album);
        printf("  etiquetas: '%s' / '%s' / '%s'\n", tg.title, tg.artist, tg.album);
    }

    /* tiempo -> byte contra el audio real */
    {
        Mem m = { data, size, 0 };
        uint8_t *buf = malloc(16384);
        DecFrame f;
        long cap = 4096, n = 0, start_prev = audio_start;
        long *start = malloc(cap * sizeof *start);
        unsigned long long *t_ms = malloc(cap * sizeof *t_ms);
        unsigned long long samples = 0;
        static const int pct[] = { 5, 25, 50, 75, 95 };
        unsigned long long span = (unsigned long long)mi.audio_bytes;
        double worst = 0;

        dec_stream_open(DEC_PROFILE_FULL, mem_read, mem_seek, &m, buf, 16384);
        while (dec_stream_next(&f) == 1) {
            if (n == cap) { cap *= 2; start = realloc(start, cap * sizeof *start); t_ms = realloc(t_ms, cap * sizeof *t_ms); }
            start[n] = start_prev;
            t_ms[n] = samples * 1000ULL / f.rate;
            samples += f.frames;
            start_prev = f.file_pos;
            n++;
        }
        dec_stream_close();
        for (i = 0; i < 5; i++) {
            unsigned long ms = (unsigned long)((unsigned long long)mi.duration_ms * pct[i] / 100);
            long k, actual, est = mp3info_offset_for_ms(&mi, ms);
            double err;

            for (k = 0; k < n && t_ms[k] < ms; k++) ;
            actual = k < n ? start[k] : size;
            err = (double)(est > actual ? est - actual : actual - est) * 100.0 / (double)span;
            if (err > worst) worst = err;
            printf("  %d%%: ms=%lu estimado=%ld real=%ld error=%.3f%% del archivo\n", pct[i], ms, est, actual, err);
        }
        CHECK(worst < (mi.has_toc ? 1.2 : 0.15), "error de busqueda maximo %.3f%%", worst);
        CHECK(mp3info_offset_for_ms(&mi, 0) <= audio_start + mi.first_frame_offset + 1, "ms=0 cae en el inicio");
        CHECK(mp3info_offset_for_ms(&mi, mi.duration_ms) <= size, "ms=duracion no pasa del final");
        free(buf); free(start); free(t_ms);
    }
    printf(failures ? "  RESULTADO: %d FALLAS\n" : "  RESULTADO: todo OK (%d fallas)\n", failures);
    free(data);
    return failures ? 1 : 0;
}
