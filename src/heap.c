/*
 * heap — ver heap.h.
 *
 * Cada bloque lleva una cabecera de 16 bytes con su tamaño total. Los libres forman una lista ordenada por dirección,
 * así heap_free fusiona con el vecino anterior y el siguiente en una sola pasada.
 */
#include "heap.h"

#include <string.h>

#define ALIGN 16
#define HDR   16

struct HeapBlock {
    size_t size;                /* tamaño total del bloque, cabecera incluida, múltiplo de ALIGN */
    HeapBlock *next;            /* solo en bloques libres */
};

static size_t round_up(size_t n)
{
    return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1);
}

void heap_init(Heap *h, void *mem, size_t size)
{
    uintptr_t p = (uintptr_t)mem;
    size_t skew = (size_t)((ALIGN - (p & (ALIGN - 1))) & (ALIGN - 1));
    HeapBlock *b;

    h->base = (uint8_t *)mem + skew;
    h->size = size > skew ? (size - skew) & ~(size_t)(ALIGN - 1) : 0;
    h->free_list = NULL;
    h->used = 0;
    h->peak = 0;
    h->failed = 0;
    if (h->size >= HDR + ALIGN) {
        b = (HeapBlock *)h->base;
        b->size = h->size;
        b->next = NULL;
        h->free_list = b;
    }
}

void *heap_alloc(Heap *h, size_t n)
{
    size_t need = HDR + round_up(n ? n : 1);
    HeapBlock *b, *prev = NULL;

    if (need < n) {
        h->failed = 1;
        return NULL;
    }
    for (b = h->free_list; b; prev = b, b = b->next) {
        if (b->size < need)
            continue;
        if (b->size - need >= HDR + ALIGN) {                 /* partir: el resto sigue libre */
            HeapBlock *rest = (HeapBlock *)((uint8_t *)b + need);

            rest->size = b->size - need;
            rest->next = b->next;
            b->size = need;
            if (prev)
                prev->next = rest;
            else
                h->free_list = rest;
        } else {
            if (prev)
                prev->next = b->next;
            else
                h->free_list = b->next;
        }
        h->used += b->size;
        if (h->used > h->peak)
            h->peak = h->used;
        return (uint8_t *)b + HDR;
    }
    h->failed = 1;
    return NULL;
}

void *heap_calloc(Heap *h, size_t count, size_t size)
{
    size_t n = count * size;
    void *p;

    if (size && n / size != count) {
        h->failed = 1;
        return NULL;
    }
    p = heap_alloc(h, n);
    if (p)
        memset(p, 0, n);
    return p;
}

void heap_free(Heap *h, void *p)
{
    HeapBlock *b, *cur, *prev = NULL;

    if (p == NULL)
        return;
    b = (HeapBlock *)((uint8_t *)p - HDR);
    h->used -= b->size;
    for (cur = h->free_list; cur && cur < b; prev = cur, cur = cur->next)
        ;
    b->next = cur;
    if (prev)
        prev->next = b;
    else
        h->free_list = b;
    if (cur && (uint8_t *)b + b->size == (uint8_t *)cur) {      /* fusionar con el siguiente */
        b->size += cur->size;
        b->next = cur->next;
    }
    if (prev && (uint8_t *)prev + prev->size == (uint8_t *)b) { /* y con el anterior */
        prev->size += b->size;
        prev->next = b->next;
    }
}

void *heap_realloc(Heap *h, void *p, size_t n)
{
    HeapBlock *b;
    size_t have, keep;
    void *q;

    if (p == NULL)
        return heap_alloc(h, n);
    b = (HeapBlock *)((uint8_t *)p - HDR);
    have = b->size - HDR;
    if (n <= have)
        return p;                                   /* ya cabe (no se devuelve el sobrante) */
    q = heap_alloc(h, n);
    if (q == NULL)
        return NULL;
    keep = have < n ? have : n;
    memcpy(q, p, keep);
    heap_free(h, p);
    return q;
}

int heap_check(const Heap *h)
{
    const HeapBlock *b, *prev = NULL;
    size_t free_total = 0;

    for (b = h->free_list; b; prev = b, b = b->next) {
        if ((const uint8_t *)b < h->base || (const uint8_t *)b + b->size > h->base + h->size)
            return 1;
        if (b->size < HDR + ALIGN || (b->size & (ALIGN - 1)))
            return 2;
        if (prev && (const uint8_t *)prev + prev->size > (const uint8_t *)b)
            return 3;                               /* se solapan o no están ordenados */
        if (prev && (const uint8_t *)prev + prev->size == (const uint8_t *)b)
            return 4;                               /* adyacentes sin fusionar */
        free_total += b->size;
    }
    return free_total + h->used == h->size ? 0 : 5;
}

size_t heap_largest_free(const Heap *h)
{
    const HeapBlock *b;
    size_t best = 0;

    for (b = h->free_list; b; b = b->next)
        if (b->size - HDR > best)
            best = b->size - HDR;
    return best;
}
