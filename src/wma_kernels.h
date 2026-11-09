/*
 * wma_kernels — núcleos de la decodificación de WMA (libwma), con referencia en C y kernel asm 68040 (ADR-008, ADR-014).
 *
 *   mp368k_wma_fmul_add_add_*   dst[i] += fixmul32b(src0[i], src1[i])                 (ventana izquierda de wma_window)
 *   mp368k_wma_fmul_reverse_*   dst[i]  = fixmul32b(src0[i], src1[len - 1 - i])        (ventana derecha, en orden inverso)
 *
 * fixmul32b(x, y) = (int32)(((int64)x * y) >> 31), truncado (third_party/libwma/mdct.h); la suma es de 32 bits con
 * envoltura. mp368k_*_ref es C portable (src/wma_ref.c); mp368k_*_040 es asm (targets/asm/wma_window_040.s, solo rama 68k).
 * Mismo resultado bit a bit. libwma los usa a través de las macros MP368K_WMA_FMUL_ADD_ADD y MP368K_WMA_FMUL_REVERSE,
 * que define el build (third_party/patches/libwma-mp368k.patch); sin ellas libwma usa su código original.
 * len > 0 y len <= 65536 (el asm cuenta con dbra).
 */
#ifndef MP368K_WMA_KERNELS_H
#define MP368K_WMA_KERNELS_H

#include <stdint.h>

void mp368k_wma_fmul_add_add_ref(int32_t *dst, const int32_t *src0, const int32_t *src1, int len);
void mp368k_wma_fmul_add_add_040(int32_t *dst, const int32_t *src0, const int32_t *src1, int len);
void mp368k_wma_fmul_reverse_ref(int32_t *dst, const int32_t *src0, const int32_t *src1, int len);
void mp368k_wma_fmul_reverse_040(int32_t *dst, const int32_t *src0, const int32_t *src1, int len);

#endif
