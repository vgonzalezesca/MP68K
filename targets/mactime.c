/*
 * mactime en 68k: el trap _Microseconds, sin glue en las Multiversal Interfaces.
 */
#include "mactime.h"

uint64_t mactime_usec(void)
{
    /*
     * _Microseconds devuelve la parte alta en A0 y la baja en D0 (misma
     * secuencia que el FOURWORDINLINE(0xA193, ...) de las Universal Interfaces).
     * Un trap del Toolbox puede alterar D0-D2/A0-A1.
     */
    register uint32_t lo __asm__("d0");
    register uint32_t hi __asm__("a0");

    __asm__ volatile (".short 0xA193"
                      : "=r"(lo), "=r"(hi)
                      :
                      : "d1", "d2", "a1", "cc", "memory");
    return ((uint64_t)hi << 32) | lo;
}
