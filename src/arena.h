/*
 * arena — asignador de bloques sobre una memoria reservada una sola vez.
 *
 * Los códecs de terceros (dr_flac, Tremor, libwma) piden memoria con malloc/realloc/free, y la
 * regla 6 de CLAUDE.md prohíbe asignar en el bucle de decodificación. Se les da este asignador
 * sobre un bloque que la aplicación reserva al arrancar. Es un asignador de pila: liberar el
 * último bloque devuelve su espacio; liberar otro no hace nada hasta arena_reset().
 *
 * Portable (sin Toolbox): se prueba en el PC.
 */
#ifndef MP368K_ARENA_H
#define MP368K_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *base;
    size_t size;
    size_t used;
    size_t peak;        /* mayor uso desde arena_init (diagnóstico: Show Technical Info) */
    int failed;         /* una asignación no cupo desde el último arena_reset */
} Arena;

void arena_init(Arena *a, void *mem, size_t size);
/* Libera todo (conserva `peak`). */
void arena_reset(Arena *a);
/* Bloque alineado a 16 bytes, o NULL si no cabe. */
void *arena_alloc(Arena *a, size_t n);
void *arena_realloc(Arena *a, void *p, size_t n);
void arena_free(Arena *a, void *p);
/* Mayor n con el que arena_alloc(a, n) aún cabe (0 si no cabe nada). Sirve para ceder todo el resto a un heap. */
size_t arena_remaining(const Arena *a);

#endif
