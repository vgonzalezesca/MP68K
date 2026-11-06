/*
 * memblock — bloques de memoria grandes para la inicialización del reproductor.
 *
 * La partición de la aplicación es chica (1 MB por defecto y casi toda ocupada), así
 * que primero se prueba NewPtr y, si no cabe, memoria temporal del sistema. El dato
 * queda alineado a 16 bytes (línea de caché del 040, requisito de MOVE16).
 * Reservar en la inicialización; NO llamar en el bucle de decodificación ni en
 * interrupción.
 */
#ifndef MP368K_MEMBLOCK_H
#define MP368K_MEMBLOCK_H

#include <MacTypes.h>

typedef struct {
    void *data;     /* NULL si no hay nada reservado; alineado a 16 bytes */
    long size;
    Ptr ptr;        /* bloque de NewPtr, o NULL */
    Handle handle;  /* memoria temporal bloqueada, o NULL */
} MemBlock;

OSErr memblock_alloc(MemBlock *b, long size);
void memblock_free(MemBlock *b);

#endif
