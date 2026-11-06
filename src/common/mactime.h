/*
 * mactime — reloj de microsegundos del Time Manager.
 *
 * Las Multiversal Interfaces de Retro68 solo declaran Microseconds() para
 * Executor, así que aquí va la glue del trap _Microseconds (A193) a mano.
 * Requiere el Time Manager extendido (System 7+; Gestalt 'tmgr' >= 3).
 */
#ifndef MP368K_MACTIME_H
#define MP368K_MACTIME_H

#include <stdint.h>

/* Contador de 64 bits en microsegundos desde el arranque. */
uint64_t mactime_usec(void);

/*
 * Resolución efectiva de mactime_usec(): menor salto no nulo del contador sobre
 * 8 cambios. Si es de ~1000 us, los tiempos menores a ~100 ms quedan cuantizados.
 */
unsigned long mactime_resolution_usec(void);

#endif
