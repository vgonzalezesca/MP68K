/*
 * asmrt — runtime mínimo para los bancos de asm bajo qemu-m68k (Linux, sin libc).
 * Solo corrección: QEMU no modela tiempos ni cachés.
 */
#ifndef MP368K_ASMRT_H
#define MP368K_ASMRT_H

#include <stdint.h>

long call_checked(void *fn, long a1, long a2, long a3, long a4, long a5, long a6, long a7);

static inline long sys_write(long fd, const void *buf, long n)
{
    register long d0 __asm__("d0") = 4;
    register long d1 __asm__("d1") = fd;
    register long d2 __asm__("d2") = (long)buf;
    register long d3 __asm__("d3") = n;

    __asm__ volatile("trap #0" : "+r"(d0) : "r"(d1), "r"(d2), "r"(d3) : "memory");
    return d0;
}

static inline void say(const char *s)
{
    long n = 0;

    while (s[n])
        n++;
    sys_write(1, s, n);
}

static inline void say_hex(uint32_t v)
{
    char b[9];
    int i;

    for (i = 7; i >= 0; i--, v >>= 4)
        b[i] = "0123456789abcdef"[v & 15];
    b[8] = 0;
    say(b);
}

static inline void say_dec(uint32_t v)
{
    char b[11];
    int i = 10;

    b[i] = 0;
    do {
        b[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    say(b + i);
}

/* Generador congruencial de 32 bits (semilla fija: pruebas reproducibles). */
static inline uint32_t rnd(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s ^ (*s >> 15);
}

#endif
