/*
 * sndver en 68k: el número viaja por la pila de Pascal y la llamada de las Multiversal sirve.
 * Ver sndver.h.
 */
#include "sndver.h"

#include <Sound.h>

uint32_t sndver_get(void)
{
    NumVersion nv = SndSoundManagerVersion();

    return (uint32_t)nv;
}
