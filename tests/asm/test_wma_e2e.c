/*
 * test_wma_e2e — decodifica WMA completos con el códec de MP368K (src/codec_wma.c: lector ASF + libwma) y escribe el CRC-32
 * del PCM de 16 bits. Se compila para el 68k (bajo qemu-m68k, big-endian) y para el PC (-DMP368K_HOSTREF, little-endian):
 * targets/asm-test.sh compara las líneas "archivo frames=N crc=XXXXXXXX". El decodificador es entero: deben coincidir.
 */
#include <stdint.h>
#include <string.h>

#include "codec.h"

#ifdef MP368K_HOSTREF
#include <stdio.h>
#include <stdlib.h>
#else
#include "asmrt.h"
#endif

#define MAX_FILE (2 * 1024 * 1024)
static unsigned char filebuf[MAX_FILE];
static long filelen, filepos;
static Arena arena;
static unsigned char arenamem[256 * 1024];
static uint8_t workbuf[16384];
static uint32_t crc_table[256];
static unsigned long max_frames;       /* --frames=N: tope de cuadros por archivo (0 = todo), para perfilar */

Arena *codec_arena(void) { return &arena; }

static int io_read(void *ctx, uint8_t *dst, long max, long *got)
{
    long n = max < filelen - filepos ? max : filelen - filepos;

    (void)ctx;
    if (n < 0)
        n = 0;
    memcpy(dst, filebuf + filepos, (size_t)n);
    filepos += n;
    *got = n;
    return 0;
}

static int io_seek(void *ctx, long off)
{
    (void)ctx;
    if (off < 0 || off > filelen)
        return -1;
    filepos = off;
    return 0;
}

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
    register long d0 __asm__("d0") = 5;
    register long d1 __asm__("d1") = (long)path;
    register long d2 __asm__("d2") = 0;
    long fd, total = 0, r;

    __asm__ volatile("trap #0" : "+r"(d0) : "r"(d1), "r"(d2) : "memory");
    fd = d0;
    if (fd < 0)
        return -1;
    for (;;) {
        register long e0 __asm__("d0") = 3;
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
        register long c0 __asm__("d0") = 6;
        register long c1 __asm__("d1") = fd;

        __asm__ volatile("trap #0" : "+r"(c0) : "r"(c1) : "memory");
    }
    return total;
#endif
}

static void report(const char *name, unsigned long frames, uint32_t crc, long rc)
{
#ifdef MP368K_HOSTREF
    printf("%s frames=%lu crc=%08X rc=%ld\n", name, frames, (unsigned)crc, rc);
#else
    char b[9];
    int i;
    uint32_t v = crc;

    say(name);
    say(" frames=");
    say_dec((uint32_t)frames);
    say(" crc=");
    for (i = 7; i >= 0; i--, v >>= 4)
        b[i] = "0123456789ABCDEF"[v & 15];
    b[8] = 0;
    say(b);
    say(" rc=");
    say_dec((uint32_t)(rc < 0 ? -rc : rc));
    say("\n");
#endif
}

static int decode_one(const char *path)
{
    CodecIO io;
    CodecInfo ci;
    int err, rc;
    unsigned long frames = 0;
    uint32_t crc = 0xFFFFFFFFu;
    const Codec *c;
    DecFrame f;

    filelen = read_file(path, filebuf, MAX_FILE);
    if (filelen <= 0)
        return 1;
    filepos = 0;
    memset(&io, 0, sizeof io);
    io.read = io_read;
    io.seek = io_seek;
    io.size = filelen;
    arena_init(&arena, arenamem, sizeof arenamem);
    memset(&ci, 0, sizeof ci);
    c = &codec_wma;
    err = c->probe(&io, workbuf, sizeof workbuf, &ci);
    if (err != CODEC_OK) {
        report(path, 0, 0, err);
        return 1;
    }
    rc = c->open(&io, DEC_PROFILE_FULL, workbuf, sizeof workbuf, &ci);
    if (rc != CODEC_OK) {
        report(path, 0, 0, rc);
        return 1;
    }
    while ((!max_frames || frames < max_frames) && c->next(&f) == 1) {
        unsigned i;

        for (i = 0; i < f.count; i++) {
            uint32_t v = (uint32_t)(uint16_t)f.pcm[i];
            int k;

            for (k = 8; k >= 0; k -= 8)
                crc = crc_table[(crc ^ (v >> k)) & 0xFF] ^ (crc >> 8);
        }
        frames += f.frames;
    }
    c->close();
    report(path, frames, ~crc, 0);
    return 0;
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
            }
            continue;
        }
        rc |= decode_one(argv[i]);
    }
    return rc;
}
