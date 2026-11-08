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

/*
 * Ventana de la síntesis de MEDIA TASA (synth_half de libmad): las mismas cuentas que la completa pero solo para las subbandas pares
 * (sb = 0, 2, ..., 14 y 16); salen 16 muestras en pcm[0..15]: pcm[0], luego pcm[sb/2] y pcm[16 - sb/2] por cada sb par de 2 a 14, y pcm[8].
 * Mismos argumentos y misma aritmética (>> 16, suma de 32 bits) que la completa.
 */
void mp368k_synth_window_half_ref(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                  const int32_t *D, unsigned int pe, unsigned int po, int32_t *pcm);
void mp368k_synth_window_half_040(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                  const int32_t *D, unsigned int pe, unsigned int po, int32_t *pcm);

/*
 * DCT de 32 puntos (el dct32 de libmad): in[32] -> lo[i][slot] y hi[i][slot], i = 0..15, con lo/hi de [16][8].
 * Aritmética de 32 bits con envoltura; cada producto es (int32)(((int64)x*y) >> 28). Referencia GENERADA
 * (src/dct32_ref.c) y kernel asm GENERADO (targets/asm/dct32_040.s) desde el mismo texto de libmad.
 */
void mp368k_dct32_ref(const int32_t in[32], unsigned int slot, int32_t lo[16][8], int32_t hi[16][8]);
void mp368k_dct32_040(const int32_t in[32], unsigned int slot, int32_t lo[16][8], int32_t hi[16][8]);

/*
 * IMDCT de una subbanda de un bloque LARGO normal (block_type 0), con ventana, solapado y, si invert != 0, inversión
 * de frecuencias (muestras impares negadas): lo que hace libmad con III_imdct_l + III_overlap (+ III_freqinver).
 *   X        18 coeficientes de la subbanda (xr[ch][18*sb])
 *   overlap  18 palabras: se suman a la salida y se reemplazan por la segunda mitad
 *   sample   &sample[0][sb]: la muestra i va en sample[32*i]
 * Referencia GENERADA (src/imdct_ref.c) y kernel asm GENERADO (targets/asm/imdct_l_040.s) del mismo texto de libmad.
 */
void mp368k_imdct_l_ref(const int32_t X[18], int32_t overlap[18], int32_t *sample, int invert);
void mp368k_imdct_l_040(const int32_t X[18], int32_t overlap[18], int32_t *sample, int invert);

#endif
