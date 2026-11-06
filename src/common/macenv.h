/*
 * macenv — preparación del entorno de la aplicación Mac (pila y heap).
 */
#ifndef MP368K_MACENV_H
#define MP368K_MACENV_H

/*
 * Reserva `extra` bytes más de pila bajando ApplLimit y expandiendo el heap
 * (procedimiento clásico: SetApplLimit + MaxApplZone). Hay que llamarla al
 * principio de main(), antes de cualquier asignación grande. No garantiza el
 * resultado (no puede bajar del final actual del heap): devuelve StackSpace(),
 * los bytes de pila disponibles, para que el llamador lo compruebe y lo registre.
 */
long macenv_grow_stack(long extra);

#endif
