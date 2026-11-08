/*
 * eq_kernels — núcleo del ecualizador por coeficientes, con referencia en C y kernel asm (ADR-014).
 *
 *   mulg_range   para i = 0..n-1: si gain[i] != 2^24 (ganancia 1,0), xr[i] = (x*g + 2^23) >> 24 con g en Q24, saturado a 32 bits.
 *                Es el bucle de bloques largos de eq_apply_granule (src/eq.c); los bloques cortos y mixtos siguen en C.
 *
 * mp368k_eq_mulg_range_ref es C portable (src/eq_ref.c); mp368k_eq_mulg_range_040 es asm 68040 (targets/asm/eq_040.s, solo rama 68k).
 * Mismo resultado bit a bit. El build elige con la macro MP368K_EQ_MULG_RANGE (por omisión, la referencia en C).
 */
#ifndef MP368K_EQ_KERNELS_H
#define MP368K_EQ_KERNELS_H

#include <stdint.h>

#define EQ_UNITY_GAIN (1L << 24)

void mp368k_eq_mulg_range_ref(int32_t *xr, const int32_t *gain, unsigned int n);
void mp368k_eq_mulg_range_040(int32_t *xr, const int32_t *gain, unsigned int n);

#ifndef MP368K_EQ_MULG_RANGE
#define MP368K_EQ_MULG_RANGE mp368k_eq_mulg_range_ref
#endif

#endif
