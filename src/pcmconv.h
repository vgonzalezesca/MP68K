/*
 * pcmconv — conversión del PCM de 16 bits del decodificador al formato de salida.
 */
#ifndef MP368K_PCMCONV_H
#define MP368K_PCMCONV_H

#include <stdint.h>

/*
 * 16 bits con signo -> 8 bits sin signo (binario con desplazamiento: 0x80 = silencio),
 * con redondeo y saturación, sin dither. Para salida de hardware de 8 bits.
 */
void pcm_s16_to_u8(const int16_t *src, uint8_t *dst, unsigned long n);

/*
 * Aplica ganancia por canal (256 = unidad) a PCM intercalado de 16 bits. gl/gr se
 * limitan a 256. En mono se usa solo gl. Con 256 en ambos canales el resultado es
 * idéntico a la entrada. src y dst pueden ser el mismo bloque.
 */
void pcm_apply_gain16(const int16_t *src, int16_t *dst, unsigned long n,
                      unsigned channels, unsigned gl, unsigned gr);

#endif
