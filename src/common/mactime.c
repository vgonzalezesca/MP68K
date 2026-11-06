/*
 * mactime — ver mactime.h.
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

unsigned long mactime_resolution_usec(void)
{
    uint64_t best = 0, prev = mactime_usec(), now;
    int changes = 0;
    long spins = 0;

    while (changes < 8 && spins < 50000000L) {
        now = mactime_usec();
        spins++;
        if (now != prev) {
            if (best == 0 || now - prev < best)
                best = now - prev;
            prev = now;
            changes++;
        }
    }
    return (unsigned long)best;
}
