/*
 * sndver — versión del Sound Manager como número.
 *
 * SndSoundManagerVersion devuelve NumVersion, que en Apple es un struct y en las Multiversal
 * un LONGINT. En 68k da lo mismo; en PowerPC el struct viaja por un puntero oculto en r3, y
 * llamarla como si devolviera un número escribe 4 bytes sobre lo que r3 tuviera. Por eso cada
 * equipo la llama a su manera, en targets/sndver.c. Nadie más debe llamar a
 * SndSoundManagerVersion ni usar NumVersion (tools/check-numversion.sh).
 */
#ifndef MP368K_SNDVER_H
#define MP368K_SNDVER_H

#include <stdint.h>

/* Bytes de NumVersion como número big-endian: 0x03218000 = 3.2.1 estable. */
uint32_t sndver_get(void);

#endif
