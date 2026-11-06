/*
 * mp368k_eq.h — gancho del ecualizador para layer3.c (parte de MP368K, GPL).
 * Solo se incluye en la copia parcheada de libmad (tools/prepare-libmad-eq.sh); el árbol
 * third_party/libmad/ no se modifica.
 *
 * mp368k_eq_hook, si no es NULL, se llama una vez por granulo y canal con los 576 coeficientes de
 * frecuencia ya decodificados (tras la estereofonía conjunta) y ANTES de la alias-reduction y la IMDCT.
 *   samplerate  Hz del frame
 *   block       0 = bloque largo; 1 = bloque corto (ya reordenado: 18*sb + 6*ventana + k);
 *               2 = bloque mixto (36 coeficientes largos y el resto cortos)
 * Con el gancho en NULL el resultado es idéntico bit a bit al de libmad sin parchear.
 */
#ifndef MP368K_EQ_HOOK_H
#define MP368K_EQ_HOOK_H

#include "fixed.h"

extern void (*mp368k_eq_hook)(mad_fixed_t xr[576], unsigned int samplerate, int block);

#endif
