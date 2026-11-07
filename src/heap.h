/*
 * heap — asignador de uso general (primer ajuste, con fusión de bloques libres) sobre memoria reservada una sola vez.
 *
 * Para los códecs de terceros que asignan y liberan en orden arbitrario (Tremor, libwma): el arena de arena.h solo
 * libera lo último. La memoria se reserva al abrir el archivo (regla 6 de CLAUDE.md: nada de NewPtr/malloc en el
 * bucle de decodificación); este asignador reparte ese bloque. Portable (sin Toolbox): se prueba en el PC.
 * No es seguro entre hilos ni desde una interrupción.
 */
#ifndef MP368K_HEAP_H
#define MP368K_HEAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct HeapBlock HeapBlock;

typedef struct {
    uint8_t *base;
    size_t size;
    HeapBlock *free_list;       /* por dirección creciente */
    size_t used;                /* bytes en bloques asignados (con cabecera) */
    size_t peak;                /* mayor `used` desde heap_init */
    int failed;                 /* una asignación no cupo */
} Heap;

void heap_init(Heap *h, void *mem, size_t size);
/* Bloque alineado a 16 bytes, o NULL si no cabe (y marca `failed`). */
void *heap_alloc(Heap *h, size_t n);
void *heap_calloc(Heap *h, size_t count, size_t size);
void *heap_realloc(Heap *h, void *p, size_t n);
void heap_free(Heap *h, void *p);

/* Comprobación de consistencia para las pruebas: 0 si está bien. */
int heap_check(const Heap *h);
/* Mayor bloque libre contiguo, en bytes de carga útil. */
size_t heap_largest_free(const Heap *h);

#endif
