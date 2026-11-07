/*
 * test_heap — src/heap.c: asignar, liberar y redimensionar al azar con comprobación de contenido, alineación, no
 * solapamiento y consistencia de la lista libre; al liberar todo debe volver a quedar un solo bloque.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "heap.h"

#define SLOTS 400
#define POOL (256 * 1024)

static uint32_t seed = 368;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

typedef struct { uint8_t *p; size_t n; uint8_t tag; } Slot;

static unsigned fails;
#define CHECK(c, msg) do { if (!(c)) { if (fails < 10) printf("  FALLA: %s (línea %d)\n", msg, __LINE__); fails++; } } while (0)

static void fill(Slot *s)
{
    memset(s->p, s->tag, s->n);
}

static int verify(const Slot *s)
{
    size_t i;

    for (i = 0; i < s->n; i++)
        if (s->p[i] != s->tag)
            return 0;
    return 1;
}

int main(void)
{
    static uint8_t mem[POOL + 64];
    Heap h;
    Slot slots[SLOTS];
    unsigned i, round;
    size_t whole;

    memset(slots, 0, sizeof slots);
    heap_init(&h, mem + 5, POOL);                       /* base desalineada a propósito */
    CHECK(heap_check(&h) == 0, "estado inicial");
    whole = heap_largest_free(&h);

    for (round = 0; round < 200000; round++) {
        Slot *s = &slots[rnd() % SLOTS];
        unsigned op = rnd() % 10;
        size_t n = (rnd() % 8 == 0) ? rnd() % 6000 : rnd() % 300;

        if (!s->p) {
            s->p = heap_alloc(&h, n);
            if (s->p) { s->n = n; s->tag = (uint8_t)rnd(); fill(s); CHECK(((uintptr_t)s->p & 15) == 0, "alineación"); }
        } else if (op < 5) {
            CHECK(verify(s), "contenido alterado antes de liberar");
            heap_free(&h, s->p);
            s->p = NULL;
        } else if (op < 8) {
            uint8_t *q = heap_realloc(&h, s->p, n);

            if (q) {
                size_t keep = s->n < n ? s->n : n;
                size_t k;

                for (k = 0; k < keep; k++)
                    CHECK(q[k] == s->tag, "realloc perdió datos");
                s->p = q; s->n = n; s->tag = (uint8_t)rnd(); fill(s);
            } else {
                CHECK(verify(s), "realloc fallido alteró el bloque");
            }
        }
        if (round % 997 == 0)
            CHECK(heap_check(&h) == 0, "consistencia durante el uso");
        if (fails > 20)
            break;
    }
    for (i = 0; i < SLOTS; i++)
        if (slots[i].p) {
            CHECK(verify(&slots[i]), "contenido alterado al final");
            heap_free(&h, slots[i].p);
        }
    CHECK(heap_check(&h) == 0, "estado final");
    CHECK(h.used == 0, "quedan bytes asignados");
    CHECK(heap_largest_free(&h) == whole, "no volvió a un solo bloque libre");
    CHECK(heap_alloc(&h, POOL * 2) == NULL && h.failed, "asignación imposible debe fallar");
    printf("test_heap: pico %zu bytes; %s\n", h.peak, fails ? "FALLAS" : "todo OK");
    return fails != 0;
}
