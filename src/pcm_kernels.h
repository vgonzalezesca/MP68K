/*
 * pcm_kernels — conversiones de PCM en la ruta de decodificación y de salida, con referencia en C y kernel asm.
 *
 *   scale16   muestras de la síntesis (mad_fixed_t, 28 bits fraccionarios) -> int16 con redondeo y saturación,
 *             intercaladas por canal. Es scale16() de libmad (minimad.c): v = (m + 2^12) >> 13, acotado a 16 bits.
 *   u8        int16 -> uint8 con desplazamiento 0x80, redondeo y saturación (pcm_s16_to_u8, src/pcmconv.h)
 *   gain16    ganancia por canal (256 = unidad) sobre PCM intercalado de 16 bits (pcm_apply_gain16)
 *
 *   wma16     salida de libwma (fixed32 de 29 bits con polaridad invertida) -> int16: clamp((8192 - v) >> 14), intercalado
 *
 * mp368k_*_ref son C portable (src/pcm_ref.c); mp368k_*_040 son asm 68040 (targets/asm/pcm_040.s, solo rama 68k).
 * Mismo resultado bit a bit. El build elige cuál usa cada llamador con las macros MP368K_PCM_SCALE16, MP368K_PCM_U8 y
 * MP368K_PCM_GAIN16 (por omisión, la referencia en C).
 */
#ifndef MP368K_PCM_KERNELS_H
#define MP368K_PCM_KERNELS_H

#include <stdint.h>

/* nch = 1: out[i] = scale(s0[i]); nch = 2: out[2i] = scale(s0[i]), out[2i+1] = scale(s1[i]). len = muestras por canal. */
void mp368k_pcm_scale16_ref(const int32_t *s0, const int32_t *s1, int16_t *out, unsigned int len, unsigned int nch);
void mp368k_pcm_scale16_040(const int32_t *s0, const int32_t *s1, int16_t *out, unsigned int len, unsigned int nch);

/* nch = 1: out[i] = wma16(s0[i]); nch = 2: out[2i] = wma16(s0[i]), out[2i+1] = wma16(s1[i]). */
void mp368k_pcm_wma16_ref(const int32_t *s0, const int32_t *s1, int16_t *out, unsigned int len, unsigned int nch);
void mp368k_pcm_wma16_040(const int32_t *s0, const int32_t *s1, int16_t *out, unsigned int len, unsigned int nch);

void mp368k_pcm_u8_ref(const int16_t *src, uint8_t *dst, unsigned long n);
void mp368k_pcm_u8_040(const int16_t *src, uint8_t *dst, unsigned long n);

void mp368k_pcm_gain16_ref(const int16_t *src, int16_t *dst, unsigned long n, unsigned int channels,
                           unsigned int gl, unsigned int gr);
void mp368k_pcm_gain16_040(const int16_t *src, int16_t *dst, unsigned long n, unsigned int channels,
                           unsigned int gl, unsigned int gr);

#ifndef MP368K_PCM_SCALE16
#define MP368K_PCM_SCALE16 mp368k_pcm_scale16_ref
#endif
#ifndef MP368K_PCM_WMA16
#define MP368K_PCM_WMA16 mp368k_pcm_wma16_ref
#endif
#ifndef MP368K_PCM_U8
#define MP368K_PCM_U8 mp368k_pcm_u8_ref
#endif
#ifndef MP368K_PCM_GAIN16
#define MP368K_PCM_GAIN16 mp368k_pcm_gain16_ref
#endif

#endif
