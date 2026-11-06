/*
 * vorbiscomment — etiquetas "CLAVE=valor" en UTF-8 de FLAC y Ogg Vorbis, convertidas a MacRoman.
 *
 * Portable (sin Toolbox). Solo reconoce TITLE, ARTIST y ALBUM (sin distinguir mayúsculas);
 * completa únicamente campos vacíos, como id3_parse_v2. Lo que MacRoman no puede representar
 * queda como '?'.
 */
#ifndef MP368K_VORBISCOMMENT_H
#define MP368K_VORBISCOMMENT_H

#include <stddef.h>
#include <stdint.h>

#include "id3.h"

/* `e` es una entrada completa "CLAVE=valor" de `n` bytes (sin terminador). Devuelve 1 si la usó. */
int vorbiscomment_entry(const uint8_t *e, size_t n, Id3Tags *tg);

/* UTF-8 -> MacRoman en `dst` (terminada en cero, tope `cap`). */
void vorbiscomment_utf8_to_macroman(const uint8_t *s, size_t n, char *dst, size_t cap);

#endif
