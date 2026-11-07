/*
 * minilibc — lo mínimo de libc que pide libmad, para correrlo bajo qemu-m68k sin libc.
 * Solo se usa en los bancos de tests/asm.
 */
#include <stddef.h>

#define NOOPT __attribute__((optimize("no-tree-loop-distribute-patterns")))

NOOPT void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dd = d;
    const unsigned char *ss = s;

    while (n--)
        *dd++ = *ss++;
    return d;
}

NOOPT void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *dd = d;
    const unsigned char *ss = s;

    if (dd < ss) {
        while (n--)
            *dd++ = *ss++;
    } else {
        dd += n;
        ss += n;
        while (n--)
            *--dd = *--ss;
    }
    return d;
}

NOOPT void *memset(void *d, int c, size_t n)
{
    unsigned char *dd = d;

    while (n--)
        *dd++ = (unsigned char)c;
    return d;
}

NOOPT int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;

    for (; n; n--, x++, y++)
        if (*x != *y)
            return *x - *y;
    return 0;
}

/* mad_timer_string (timer.c) la referencia, pero el banco no la llama. */
int sprintf(char *s, const char *fmt, ...)
{
    (void)fmt;
    s[0] = 0;
    return 0;
}

/* libmad pide main_data (MAD_BUFFER_MDLEN) con malloc; un reservorio estático basta: el banco decodifica de a un archivo. */
static unsigned char pool[64 * 1024];
static size_t pool_used;

void *malloc(size_t n)
{
    void *p;

    n = (n + 7u) & ~7u;
    if (pool_used + n > sizeof pool)
        return 0;
    p = pool + pool_used;
    pool_used += n;
    return p;
}

void free(void *p)
{
    (void)p;
}

void *calloc(size_t a, size_t b)
{
    void *p = malloc(a * b);

    if (p)
        memset(p, 0, a * b);
    return p;
}

void *realloc(void *p, size_t n)
{
    void *q = malloc(n);

    if (q && p)
        memcpy(q, p, n);
    return q;
}

/* Lo que pide Tremor (vorbisfile.c) aunque el banco no lo ejecute: ov_open/ov_test con FILE, mensajes, ctype. */
int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

size_t strlen(const char *s)
{
    size_t n = 0;

    while (s[n])
        n++;
    return n;
}

char *strcpy(char *d, const char *s)
{
    char *r = d;

    while ((*d++ = *s++))
        ;
    return r;
}

char *strcat(char *d, const char *s)
{
    strcpy(d + strlen(d), s);
    return d;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;

    for (; n; n--, p++)
        if (*p == (unsigned char)c)
            return (void *)p;
    return 0;
}

int fclose(void *f) { (void)f; return -1; }
int fseek(void *f, long off, int whence) { (void)f; (void)off; (void)whence; return -1; }
long ftell(void *f) { (void)f; return -1; }
size_t fread(void *p, size_t s, size_t n, void *f) { (void)p; (void)s; (void)n; (void)f; return 0; }
int fprintf(void *f, const char *fmt, ...) { (void)f; (void)fmt; return 0; }

const char _ctype_[257];
void *_impure_ptr;
int *__errno(void)
{
    static int e;

    return &e;
}

/* qsort de libwma (init_vlc ordena los códigos): ordenación de Shell, sin memoria extra. */
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    unsigned char *b = base;
    size_t gap, i, j, k;

    for (gap = n / 2; gap > 0; gap /= 2)
        for (i = gap; i < n; i++)
            for (j = i; j >= gap && cmp(b + (j - gap) * size, b + j * size) > 0; j -= gap)
                for (k = 0; k < size; k++) {
                    unsigned char t = b[(j - gap) * size + k];

                    b[(j - gap) * size + k] = b[j * size + k];
                    b[j * size + k] = t;
                }
}
