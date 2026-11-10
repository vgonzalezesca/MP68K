/*
 * mp368k_l3.h — opciones de la capa III y de la síntesis para la copia parcheada de libmad (parte de MP368K, GPL; ver
 * third_party/patches/libmad-0.15.1b-l3.patch). El árbol third_party/libmad/ no se modifica.
 *
 * Siempre activo (sin cambiar un solo bit del PCM): la alias-reduction y el estéreo M/S solo recorren la zona de
 * líneas no nulas que deja la decodificación Huffman, y la búsqueda de la última subbanda no nula empieza ahí.
 *
 * mp368k_l3_mono   distinto de 0: un archivo estéreo sale en mono, (L+R)/2, mezclado ANTES de la IMDCT (la IMDCT y
 *                  la síntesis son lineales) y con un solo canal en la IMDCT y la síntesis. En estéreo M/S puro,
 *                  (L+R)/2 = M/sqrt(2): el canal S ni se decodifica. Si L y R tienen bloques distintos (solo en
 *                  estéreo simple) se procesan los dos y se mezclan las subbandas. Tras decodificar, la cabecera
 *                  del frame queda como de un solo canal.
 * mp368k_l3_maxsb  subbandas que pasan por la alias-reduction, la IMDCT y la síntesis (32 = todas). Con 16 y la síntesis
 *                  de media tasa, la salida a fs/2 queda limitada a fs/4 sin plegar las subbandas altas encima.
 * mp368k_synth_lowprec  distinto de 0: la síntesis usa la ventana de 16x16 bits (src/synth_kernels.h,
 *                  mp368k_synth_window_half16_* y _full16_*), si el build las define con MP368K_SYNTH_WINDOW_HALF16 y _FULL16.
 *
 * Con los valores por omisión (0, 32 y 0) el PCM es idéntico bit a bit al de libmad sin parchear.
 */
#ifndef MP368K_L3_H
#define MP368K_L3_H

extern int mp368k_l3_mono;
extern unsigned int mp368k_l3_maxsb;
extern int mp368k_synth_lowprec;

#endif
