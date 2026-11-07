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
