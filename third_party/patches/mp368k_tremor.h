/*
 * mp368k_tremor.h — asignaciones de Tremor sobre el heap del códec (parte de MP368K, GPL).
 * Solo se incluye en la copia parcheada de Tremor (tools/prepare-tremor.sh); third_party/tremor/ no se modifica.
 * Las cuatro funciones las define src/codec_vorbis.c sobre el heap que reserva el códec al abrir el archivo.
 */
#ifndef MP368K_TREMOR_H
#define MP368K_TREMOR_H

#include <stddef.h>

void *mp368k_tremor_malloc(size_t n);
void *mp368k_tremor_calloc(size_t count, size_t size);
void *mp368k_tremor_realloc(void *p, size_t n);
void mp368k_tremor_free(void *p);

#endif
