/*
 * ring — anillo de bytes de un productor y un consumidor, sin bloqueos.
 *
 * El hilo principal escribe (solo toca wpos) y el callback del Sound Manager lee
 * (solo toca rpos). Los contadores son de 32 bits alineados, así que su lectura y
 * escritura es atómica en el 68k, y corren libres (módulo 2^32): usados = w - r.
 * Portable (sin Toolbox) para poder probarlo en el PC.
 *
 * ring_reset NO es seguro con un consumidor activo: úselo solo con el audio detenido.
 */
#ifndef MP368K_RING_H
#define MP368K_RING_H

#include <stdint.h>

typedef struct {
    uint8_t *buf;
    unsigned long size;
    volatile unsigned long wpos;
    volatile unsigned long rpos;
} Ring;

void ring_init(Ring *r, uint8_t *buf, unsigned long size);
void ring_reset(Ring *r);
unsigned long ring_used(const Ring *r);
unsigned long ring_space(const Ring *r);
/* Escriben/leen hasta n bytes; devuelven los que hubo lugar/datos. */
unsigned long ring_write(Ring *r, const void *src, unsigned long n);
unsigned long ring_read(Ring *r, void *dst, unsigned long n);

#endif
