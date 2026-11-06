/*
 * test_units — pruebas del PC para el anillo, la conversión a 8 bits y la
 * decodificación en flujo. Uso: test_units archivo.mp3 referencia.csv
 *   referencia.csv: salida de hostdec (líneas "full,..." con el CRC de archivo entero).
 */
#include "decoder_core.h"
#include "pcmconv.h"
#include "ring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- anillo: escritura/lectura intercaladas con tamaños aleatorios ---- */
static void test_ring(void)
{
    enum { SIZE = 1000, TOTAL = 200000 };
    static uint8_t store[SIZE], src[TOTAL], dst[TOTAL];
    Ring r;
    unsigned long w = 0, rd = 0, i;

    for (i = 0; i < TOTAL; i++)
        src[i] = (uint8_t)(i * 7 + (i >> 8));
    ring_init(&r, store, SIZE);
    srand(1234);
    while (rd < TOTAL) {
        unsigned long n = (unsigned long)(rand() % 700) + 1;
        if (rand() & 1) {
            if (n > TOTAL - w) n = TOTAL - w;
            w += ring_write(&r, src + w, n);
        } else
            rd += ring_read(&r, dst + rd, n > TOTAL - rd ? TOTAL - rd : n);
        CHECK(ring_used(&r) <= SIZE, "ring excede su tamano");
        if (failures) return;
    }
    CHECK(memcmp(src, dst, TOTAL) == 0, "ring: los datos leidos no coinciden");
    CHECK(ring_used(&r) == 0 && ring_space(&r) == SIZE, "ring: debe quedar vacio");

    /* contadores que dan la vuelta a 2^32 */
    ring_init(&r, store, SIZE);
    r.wpos = r.rpos = 0xFFFFFF00UL;
    CHECK(ring_write(&r, src, 600) == 600, "ring: escritura cerca del desbordamiento");
    CHECK(ring_read(&r, dst, 600) == 600 && memcmp(src, dst, 600) == 0, "ring: lectura tras la vuelta");
}

/* ---- conversión a 8 bits ---- */
static void test_pcmconv(void)
{
    static const int16_t in[] = { -32768, -32767, -129, -128, -1, 0, 127, 128, 32383, 32384, 32767 };
    uint8_t out[sizeof in / sizeof in[0]];
    unsigned i;

    pcm_s16_to_u8(in, out, sizeof out);
    CHECK(out[0] == 0, "min -> 0 (obtuve %u)", out[0]);
    CHECK(out[5] == 128, "0 -> 128 (silencio)");
    CHECK(out[10] == 255, "max -> 255 (obtuve %u)", out[10]);
    for (i = 1; i < sizeof out; i++)
        CHECK(out[i] >= out[i - 1], "monotonia rota en %u", i);
    CHECK(out[8] == 254 && out[9] == 255, "redondeo: 32383->254, 32384->255 (obtuve %u,%u)", out[8], out[9]);
}

/* ---- ganancia / balance ---- */
static void test_gain(void)
{
    static const int16_t in[] = { 0, 0, 1000, -1000, 32767, -32768, -1, 1, 12345, -12345 };
    int16_t out[10], buf[10];
    unsigned i;

    pcm_apply_gain16(in, out, 10, 2, 256, 256);
    CHECK(memcmp(in, out, sizeof in) == 0, "ganancia unidad debe ser identica");

    pcm_apply_gain16(in, out, 10, 2, 0, 0);
    for (i = 0; i < 10; i++)
        CHECK(out[i] == 0 || (out[i] == -1 && in[i] < 0 && 0), "ganancia 0 -> silencio (i=%u: %d)", i, out[i]);

    pcm_apply_gain16(in, out, 10, 2, 128, 128);
    CHECK(out[2] == 500 && out[3] == -500 && out[4] == 16383 && out[5] == -16384,
          "ganancia 1/2: %d %d %d %d", out[2], out[3], out[4], out[5]);

    pcm_apply_gain16(in, out, 10, 2, 256, 0);       /* balance todo a la izquierda */
    for (i = 0; i < 10; i += 2)
        CHECK(out[i] == in[i] && out[i + 1] == 0, "balance izquierda, par %u", i / 2);
    pcm_apply_gain16(in, out, 10, 2, 0, 256);
    for (i = 0; i < 10; i += 2)
        CHECK(out[i] == 0 && out[i + 1] == in[i + 1], "balance derecha, par %u", i / 2);

    pcm_apply_gain16(in, out, 10, 1, 128, 0);       /* mono: solo gl */
    CHECK(out[2] == 500 && out[3] == -500, "mono usa gl");

    pcm_apply_gain16(in, out, 10, 2, 9999, 9999);   /* se limita a 256 */
    CHECK(memcmp(in, out, sizeof in) == 0, "ganancia > 256 se limita a la unidad");

    memcpy(buf, in, sizeof in);
    pcm_apply_gain16(buf, buf, 10, 2, 128, 128);    /* in situ */
    pcm_apply_gain16(in, out, 10, 2, 128, 128);
    CHECK(memcmp(buf, out, sizeof buf) == 0, "in situ == fuera de lugar");
}

/* ---- flujo: lector en memoria con un prefijo opcional (etiqueta ID3v2 simulada) ---- */
typedef struct { const uint8_t *prefix; long plen; const uint8_t *data; long size; long pos; } MemSrc;

static int mem_read(void *ctx, uint8_t *dst, long max, long *got)
{
    MemSrc *m = ctx;
    long total = m->plen + m->size, n = total - m->pos;
    long i;

    if (n > max) n = max;
    if (n < 0) n = 0;
    for (i = 0; i < n; i++, m->pos++)
        dst[i] = m->pos < m->plen ? m->prefix[m->pos] : m->data[m->pos - m->plen];
    *got = n;
    return 0;
}

static int mem_seek(void *ctx, long off)
{
    MemSrc *m = ctx;
    if (off < 0 || off > m->plen + m->size) return -1;
    m->pos = off;
    return 0;
}

static uint32_t crc_feed(uint32_t crc, const int16_t *pcm, unsigned count)
{
    static uint32_t tab[256];
    unsigned i;

    if (!tab[1])
        for (i = 0; i < 256; i++) {
            uint32_t c = i; int k;
            for (k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
            tab[i] = c;
        }
    for (i = 0; i < count; i++) {
        uint16_t v = (uint16_t)pcm[i];
        crc = tab[(crc ^ (v >> 8)) & 0xFF] ^ (crc >> 8);
        crc = tab[(crc ^ (v & 0xFF)) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

/*
 * CRC de los primeros `limit` frames (0 = todos). *frames cuenta TODOS los del flujo:
 * dec_run recibe el archivo sin la cola de guarda de libmad y pierde el ultimo frame;
 * el flujo la agrega y lo recupera.
 */
static uint32_t stream_crc(MemSrc *m, DecProfile p, size_t bufsize, unsigned long limit,
                           unsigned long *frames, unsigned long *errors)
{
    uint8_t *buf = malloc(bufsize);
    DecFrame f;
    uint32_t crc = 0xFFFFFFFFUL;
    int rc;

    *frames = 0;
    if (dec_stream_open(p, mem_read, mem_seek, m, buf, bufsize) != 0) {
        free(buf);
        return 0;
    }
    while ((rc = dec_stream_next(&f)) == 1) {
        if (limit == 0 || *frames < limit)
            crc = crc_feed(crc, f.pcm, f.count);
        (*frames)++;
    }
    CHECK(rc == 0, "flujo termino con error %d", rc);
    *errors = dec_stream_errors();
    dec_stream_close();
    free(buf);
    return crc ^ 0xFFFFFFFFUL;
}

static void test_stream(const char *mp3, const char *csv)
{
    FILE *fp = fopen(mp3, "rb");
    uint8_t *data, tag[10 + 3000];
    long size;
    static const size_t bufs[] = { 4096, 4097, 5000, 16384, 65536 };
    char line[512];
    const char *base = strrchr(mp3, '/');
    unsigned b;
    DecProfile p;

    if (!fp) { printf("no puedo abrir %s\n", mp3); failures++; return; }
    fseek(fp, 0, SEEK_END); size = ftell(fp); fseek(fp, 0, SEEK_SET);
    data = malloc(size);
    if (fread(data, 1, size, fp) != (size_t)size) { failures++; return; }
    fclose(fp);
    base = base ? base + 1 : mp3;

    memset(tag, 0, sizeof tag);
    memcpy(tag, "ID3\x03\x00\x00", 6);
    tag[6] = 0; tag[7] = 0; tag[8] = 0x17; tag[9] = 0x38;      /* 3000 = 23*128 + 56 */

    for (p = DEC_PROFILE_FULL; p < DEC_PROFILE_COUNT; p++) {
        unsigned long want_frames = 0;
        uint32_t want_crc = 0;
        FILE *ref = fopen(csv, "r");

        while (ref && fgets(line, sizeof line, ref)) {
            char kind[16], file[64], fcrc[16], prof[16];
            unsigned long frames, errors, spc, ch, rate;
            unsigned crc;
            if (sscanf(line, "%15[^,],%63[^,],%15[^,],%15[^,],%lu,%lu,%lu,%lu,%lu,%x", kind, file, fcrc, prof,
                       &frames, &errors, &spc, &ch, &rate, &crc) == 10 &&
                !strcmp(kind, "full") && !strcmp(file, base) && !strcmp(prof, dec_profile_name(p))) {
                want_frames = frames; want_crc = crc;
            }
        }
        if (ref) fclose(ref);
        CHECK(want_frames > 0, "%s %s: sin referencia 'full' en el CSV", base, dec_profile_name(p));

        for (b = 0; b < sizeof bufs / sizeof bufs[0]; b++) {
            MemSrc m = { NULL, 0, data, size, 0 };
            unsigned long frames, errors;
            uint32_t crc = stream_crc(&m, p, bufs[b], want_frames, &frames, &errors);
            CHECK(crc == want_crc && frames == want_frames + 1,
                  "%s %s buf=%zu: crc %08X frames %lu, esperado %08X frames %lu+1", base,
                  dec_profile_name(p), bufs[b], crc, frames, want_crc, want_frames);
        }
        /* con una etiqueta ID3v2 de 3000+10 bytes por delante el audio debe ser el mismo */
        {
            MemSrc m = { tag, 3010, data, size, 0 };
            unsigned long frames, errors;
            uint32_t crc = stream_crc(&m, p, 8192, want_frames, &frames, &errors);
            CHECK(crc == want_crc && frames == want_frames + 1, "%s %s con ID3v2: crc %08X frames %lu", base,
                  dec_profile_name(p), crc, frames);
        }
    }

    /* seek: a mitad del archivo debe seguir decodificando hasta el final sin error fatal */
    {
        MemSrc m = { NULL, 0, data, size, 0 };
        uint8_t *buf = malloc(8192);
        DecFrame f;
        long last = -1, n = 0;
        int rc;

        CHECK(dec_stream_open(DEC_PROFILE_FULL, mem_read, mem_seek, &m, buf, 8192) == 0, "open para seek");
        CHECK(dec_stream_next(&f) == 1, "primer frame");
        CHECK(dec_stream_seek(size / 2) == 0, "seek");
        while ((rc = dec_stream_next(&f)) == 1) {
            CHECK(f.file_pos > last, "file_pos no avanza");
            last = f.file_pos;
            n++;
        }
        CHECK(rc == 0, "tras seek termino con error %d", rc);
        CHECK(n > 200 && last >= size - 2000, "tras seek: %ld frames, ultima pos %ld de %ld", n, last, size);
        dec_stream_close();
        free(buf);
    }
    free(data);
}

int main(int argc, char **argv)
{
    test_ring();
    test_pcmconv();
    test_gain();
    if (argc >= 3)
        test_stream(argv[1], argv[2]);
    printf(failures ? "RESULTADO: %d FALLAS\n" : "RESULTADO: todo OK (%d fallas)\n", failures);
    return failures ? 1 : 0;
}
