/*
 * synth_kernels — núcleos de la síntesis polifásica del MP3 (ventana D, un "slot" de 32 muestras).
 *
 * Dos implementaciones con la MISMA firma y el MISMO resultado bit a bit:
 *   mp368k_synth_window_full_ref   C portable (src/synth_ref.c): la referencia.
 *   mp368k_synth_window_full_040   ensamblador 68040 (targets/asm/synth_window_040.s), solo rama 68k.
 * La copia parcheada de libmad (third_party/patches/libmad-0.15.1b-synth.patch) llama a una u otra
 * según MP368K_SYNTH_WINDOW, que define el build (selector C/asm, ADR-008).
 *
 * Qué calcula (idéntico a synth_full de libmad con FPM_64BIT, sin OPT_*): cada producto es
 * (int32)(((int64)x*y) >> 16), truncado, y se acumula en 32 bits con envoltura. El 16 viene de que, en
 * synth.c, la tabla D está pre-desplazada 12 bits (PRESHIFT) y MAD_F_SCALEBITS pasa a 28 - 12. (El DCT32,
 * que está antes en el mismo archivo, usa 28.) Cada salida es una suma de 16 productos de la fila de salida
 * del DCT32 por coeficientes de la ventana D; algunas van negadas.
 *
 *   fx, fe, fo  primera fila de cada matriz [16][8] del filtro: (*filter)[0][~phase&1],
 *               (*filter)[0][phase&1] y (*filter)[1][~phase&1] (filas contiguas de 8 palabras)
 *   D           &D[0][0] de la tabla de la ventana ya desplazada, 17 filas de 32 palabras
 *   pe, po      desplazamientos en palabras (pe par, po impar), como en libmad
 *   pcm         salida: 32 palabras, pcm[0..31]
 */
#ifndef MP368K_SYNTH_KERNELS_H
#define MP368K_SYNTH_KERNELS_H

#include <stdint.h>

void mp368k_synth_window_full_ref(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                  const int32_t *D, unsigned int pe, unsigned int po, int32_t *pcm);

void mp368k_synth_window_full_040(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                  const int32_t *D, unsigned int pe, unsigned int po, int32_t *pcm);

#endif
