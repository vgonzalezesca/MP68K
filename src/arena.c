/*
 * arena — ver arena.h.
 */
#include "arena.h"

#include <string.h>

#define ALIGN 16
#define HDR   16            /* cabecera de cada bloque: su tamaño, rellenado a ALIGN */

static size_t round_up(size_t n)
{
    return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1);
}

void arena_init(Arena *a, void *mem, size_t size)
{
    uintptr_t p = (uintptr_t)mem;
    size_t skew = (size_t)((ALIGN - (p & (ALIGN - 1))) & (ALIGN - 1));

    a->base = (uint8_t *)mem + skew;
    a->size = size > skew ? size - skew : 0;
    a->used = 0;
    a->peak = 0;
    a->failed = 0;
}

void arena_reset(Arena *a)
{
    a->used = 0;
    a->failed = 0;
}

static size_t block_size(const void *p)
{
    size_t n;

    memcpy(&n, (const uint8_t *)p - HDR, sizeof n);
    return n;
}

static int is_top(const Arena *a, const void *p)
{
    return (const uint8_t *)p + round_up(block_size(p)) == a->base + a->used;
}

void *arena_alloc(Arena *a, size_t n)
{
    size_t need = HDR + round_up(n ? n : 1);
    uint8_t *blk;

    if (need < n || need > a->size - a->used) {     /* desborde o no cabe */
        a->failed = 1;
        return NULL;
    }
    blk = a->base + a->used;
    a->used += need;
    if (a->used > a->peak)
        a->peak = a->used;
    memcpy(blk, &n, sizeof n);
    return blk + HDR;
}

size_t arena_remaining(const Arena *a)
{
    size_t left = a->size - a->used;

    return left > HDR ? (left - HDR) & ~(size_t)(ALIGN - 1) : 0;
}

void arena_free(Arena *a, void *p)
{
    if (p == NULL)
        return;
    if (is_top(a, p))
        a->used -= HDR + round_up(block_size(p));
}

void *arena_realloc(Arena *a, void *p, size_t n)
{
    size_t old;
    void *q;

    if (p == NULL)
        return arena_alloc(a, n);
    old = block_size(p);
    if (is_top(a, p)) {
        size_t base_used = (size_t)((uint8_t *)p - HDR - a->base);
        size_t need = HDR + round_up(n ? n : 1);

        if (need < n || need > a->size - base_used) {
            a->failed = 1;
            return NULL;
        }
        a->used = base_used + need;
        if (a->used > a->peak)
            a->peak = a->used;
        memcpy((uint8_t *)p - HDR, &n, sizeof n);
        return p;
    }
    q = arena_alloc(a, n);
    if (q != NULL)
        memcpy(q, p, old < n ? old : n);
    return q;
}
