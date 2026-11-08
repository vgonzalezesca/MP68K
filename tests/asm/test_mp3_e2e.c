/*
 * test_mp3_e2e — decodifica MP3 completos con libmad (copia parcheada) y escribe el CRC-32 de las
 * muestras de salida de la síntesis (mad_fixed_t crudo, big-endian). Se compila de dos maneras:
 *   - para el 68k (bajo qemu-m68k), con el kernel asm o con la referencia en C;
 *   - para el PC (-DMP368K_HOSTREF), con la síntesis ORIGINAL de libmad: es la referencia.
 * targets/asm-test.sh compara las líneas "archivo frames=N crc=XXXXXXXX" de ambas.
 */
#include <stdint.h>
#include "mad.h"
#include "eq.h"

#ifdef MP368K_HOSTREF
#include <stdio.h>
#include <stdlib.h>
#else
#include "asmrt.h"
#endif

#define MAX_FILE (2 * 1024 * 1024)
static unsigned char filebuf[MAX_FILE + MAD_BUFFER_GUARD];
static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;
static uint32_t crc_table[256];
static int only_half;                   /* --onlyhalf: solo el perfil de media tasa (para perfilar) */
static int no_half;                     /* --nohalf: solo el perfil completo (para perfilar) */
static unsigned long max_frames;       /* --frames=N: tope de cuadros por archivo (0 = todo), para perfilar */

static void crc_init(void)
{
    uint32_t n, k, c;

    for (n = 0; n < 256; n++) {
        c = n;
        for (k = 0; k < 8; k++)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}

static long read_file(const char *path, unsigned char *buf, long cap)
{
#ifdef MP368K_HOSTREF
    FILE *f = fopen(path, "rb");
    long n;

    if (!f)
        return -1;
    n = (long)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
#else
    register long d0 __asm__("d0") = 5;                 /* open */
    register long d1 __asm__("d1") = (long)path;
    register long d2 __asm__("d2") = 0;                 /* O_RDONLY */
    long fd, total = 0, r;

    __asm__ volatile("trap #0" : "+r"(d0) : "r"(d1), "r"(d2) : "memory");
    fd = d0;
    if (fd < 0)
        return -1;
    for (;;) {
        register long e0 __asm__("d0") = 3;             /* read */
        register long e1 __asm__("d1") = fd;
        register long e2 __asm__("d2") = (long)(buf + total);
        register long e3 __asm__("d3") = cap - total;

        __asm__ volatile("trap #0" : "+r"(e0) : "r"(e1), "r"(e2), "r"(e3) : "memory");
        r = e0;
        if (r <= 0)
            break;
        total += r;
    }
    {
        register long c0 __asm__("d0") = 6;             /* close */
        register long c1 __asm__("d1") = fd;

        __asm__ volatile("trap #0" : "+r"(c0) : "r"(c1) : "memory");
    }
    return total;
#endif
}

static void report(const char *name, unsigned frames, uint32_t crc)
{
#ifdef MP368K_HOSTREF
    printf("%s frames=%u crc=%08X\n", name, frames, (unsigned)crc);
#else
    say(name);
    say(" frames=");
    say_dec(frames);
    say(" crc=");
    {
        char b[9];
        int i;
        uint32_t v = crc;

        for (i = 7; i >= 0; i--, v >>= 4)
            b[i] = "0123456789ABCDEF"[v & 15];
        b[8] = 0;
        say(b);
    }
    say("\n");
#endif
}

static int decode_one(const char *path, int half)
{
    long n = read_file(path, filebuf, MAX_FILE);
    unsigned frames = 0, c, i;
    uint32_t crc = 0xFFFFFFFFu;

    if (n <= 0)
        return 1;
    mad_stream_init(&stream);
    mad_frame_init(&frame);
    mad_synth_init(&synth);
    stream.options = half ? MAD_OPTION_HALFSAMPLERATE : 0;
    mad_stream_buffer(&stream, filebuf, (unsigned long)n);
    for (;;) {
        if (max_frames && frames >= max_frames)
            break;
        if (mad_frame_decode(&frame, &stream)) {
            if (MAD_RECOVERABLE(stream.error))
                continue;
            break;
        }
        mad_synth_frame(&synth, &frame);
        frames++;
        for (c = 0; c < synth.pcm.channels; c++)
            for (i = 0; i < synth.pcm.length; i++) {
                uint32_t v = (uint32_t)synth.pcm.samples[c][i];
                int k;

                for (k = 24; k >= 0; k -= 8)
                    crc = crc_table[(crc ^ (v >> k)) & 0xFF] ^ (crc >> 8);
            }
    }
    mad_synth_finish(&synth);
    mad_frame_finish(&frame);
    mad_stream_finish(&stream);
    report(half ? "half" : "full", frames, ~crc);
    (void)path;
    return 0;
}

/* Los mismos ajustes que kBenchEq de bench/codecbench.c y EQ_BENCH de tools/host-codec-ref.sh. */
static void eq_on(void)
{
    static const int8_t v[EQ_BANDS + 1] = { 12, -6, 4, 0, 10, -12, 8, 0, -4, 12, 0 };
    EqSettings es;
    int i;

    es.enabled = 1;
    for (i = 0; i < EQ_BANDS; i++)
        es.band[i] = v[i];
    es.preamp = v[EQ_BANDS];
    eq_set(&es);
}

int main(int argc, char **argv)
{
    int i, rc = 0;

    crc_init();
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            if (argv[i][2] == 'f') {                    /* --frames=N */
                const char *s = argv[i] + 9;

                max_frames = 0;
                while (*s >= '0' && *s <= '9')
                    max_frames = max_frames * 10 + (unsigned long)(*s++ - '0');
            } else if (argv[i][2] == 'e') {             /* --eq */
                eq_on();
            } else if (argv[i][2] == 'n') {             /* --nohalf */
                no_half = 1;
            } else if (argv[i][2] == 'o') {             /* --onlyhalf */
                only_half = 1;
            }
            continue;
        }
        report(argv[i], 0, 0);
        if (!only_half)
            rc |= decode_one(argv[i], 0);
        if (!no_half)
            rc |= decode_one(argv[i], 1);
    }
    return rc;
}
