/*
 * mactime — ver mactime.h.
 */
#include "mactime.h"

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
