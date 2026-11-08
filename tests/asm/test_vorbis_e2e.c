/*
 * test_vorbis_e2e — decodifica Ogg Vorbis completos con Tremor (copia parcheada) y escribe el CRC-32 del PCM de 16 bits
 * (big-endian por muestra). Se compila para el 68k (bajo qemu-m68k, que es big-endian) y para el PC (-DMP368K_HOSTREF,
 * little-endian): targets/asm-test.sh compara las líneas "archivo frames=N crc=XXXXXXXX" de ambas. El decodificador es
 * entero, así que deben coincidir exactamente.
 */
#include <stdint.h>
#include <string.h>

#include "heap.h"
#include "ivorbisfile.h"

#ifdef MP368K_HOSTREF
#include <stdio.h>
#include <stdlib.h>
#else
#include "asmrt.h"
#endif

#define MAX_FILE (2 * 1024 * 1024)
static unsigned char filebuf[MAX_FILE];
static long filelen, filepos;
static Heap heap;
static unsigned char heapmem[512 * 1024];
static uint32_t crc_table[256];
static unsigned long max_frames;       /* --frames=N: tope de cuadros por archivo (0 = todo), para perfilar */

void *mp368k_tremor_malloc(size_t n) { return heap_alloc(&heap, n); }
void *mp368k_tremor_calloc(size_t c, size_t s) { return heap_calloc(&heap, c, s); }
void *mp368k_tremor_realloc(void *p, size_t n) { return heap_realloc(&heap, p, n); }
void mp368k_tremor_free(void *p) { heap_free(&heap, p); }

static size_t cb_read(void *ptr, size_t size, size_t nmemb, void *ds)
{
    long want = (long)(size * nmemb);

    (void)ds;
    if (want > filelen - filepos)
        want = filelen - filepos;
    if (want <= 0 || size == 0)
        return 0;
    memcpy(ptr, filebuf + filepos, (size_t)want);
    filepos += want;
    return (size_t)want / size;
}

static int cb_seek(void *ds, ogg_int64_t off, int whence)
{
    long t = whence == 0 ? (long)off : whence == 1 ? filepos + (long)off : filelen + (long)off;

    (void)ds;
    if (t < 0 || t > filelen)
        return -1;
    filepos = t;
    return 0;
}

static int cb_close(void *ds) { (void)ds; return 0; }
static long cb_tell(void *ds) { (void)ds; return filepos; }

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

static void report(const char *name, unsigned long frames, uint32_t crc, unsigned long peak)
{
#ifdef MP368K_HOSTREF
    printf("%s frames=%lu crc=%08X heap=%lu\n", name, frames, (unsigned)crc, peak);
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
    say(" heap=");
    say_dec((uint32_t)peak);
    say("\n");
#endif
}

static int decode_one(const char *path)
{
    static const ov_callbacks cbs = { cb_read, cb_seek, cb_close, cb_tell };
    static OggVorbis_File vf;
    static int16_t pcm[4096];
    unsigned long frames = 0;
    uint32_t crc = 0xFFFFFFFFu;
    int bs, ch, rc;
    long n;

    filelen = read_file(path, filebuf, MAX_FILE);
    if (filelen <= 0)
        return 1;
    filepos = 0;
    heap_init(&heap, heapmem, sizeof heapmem);
    memset(&vf, 0, sizeof vf);
    rc = ov_open_callbacks(&filebuf, &vf, NULL, 0, cbs);
    if (rc != 0) {
        report(path, 0, 0, 0);
        return 1;
    }
    ch = ov_info(&vf, -1)->channels;
    for (;;) {
        long i;

        if (max_frames && frames >= max_frames)
            break;
        n = ov_read(&vf, pcm, (int)sizeof pcm, &bs);
        if (n <= 0)
            break;
        for (i = 0; i < n / 2; i++) {
            uint32_t v = (uint32_t)(uint16_t)pcm[i];
            int k;

            for (k = 8; k >= 0; k -= 8)
                crc = crc_table[(crc ^ (v >> k)) & 0xFF] ^ (crc >> 8);
        }
        frames += (unsigned long)(n / (2 * ch));
    }
    ov_clear(&vf);
    report(path, frames, ~crc, heap.peak);
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
