/*
 * id3 — etiquetas ID3v2 (2.2, 2.3, 2.4) e ID3v1: título, artista y álbum en MacRoman.
 *
 * Portable (sin Toolbox) para probarlo en el PC. El texto de cualquier codificación
 * (ISO-8859-1, UTF-16 con o sin BOM, UTF-8) se convierte a MacRoman, el juego de caracteres
 * de Mac OS 8; lo que MacRoman no puede representar se aproxima a ASCII o queda como '?'.
 *
 * Límites conocidos: la desincronización de TODA la etiqueta en ID3v2.3 no está soportada (es
 * muy rara); sí la de cada frame en ID3v2.4. Los frames comprimidos o cifrados se saltan.
 */
#ifndef MP368K_ID3_H
#define MP368K_ID3_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char title[64];
    char artist[64];
    char album[64];
} Id3Tags;

/* Punto de código Unicode -> MacRoman ('?' si no hay equivalente). */
unsigned char id3_cp_to_macroman(unsigned long cp);

/* Byte ISO-8859-1 -> MacRoman. Los 0x80..0x9F se leen como Windows-1252 (así vienen casi siempre). */
unsigned char id3_latin1_to_macroman(unsigned char c);

/*
 * Tamaño total (cabecera + cuerpo) de la etiqueta ID3v2 que empieza en `b`, o 0 si no hay.
 * Necesita al menos 10 bytes.
 */
long id3_v2_total_size(const uint8_t *b, size_t len);

/*
 * Lee la etiqueta ID3v2 de `b` (puede estar truncada: `len` es lo que hay). Completa solo los
 * campos que encuentre y que estén vacíos. Devuelve el tamaño total de la etiqueta (0 si no hay).
 */
long id3_parse_v2(const uint8_t *b, size_t len, Id3Tags *out);

/* Lee un bloque ID3v1 de 128 bytes. Completa solo los campos vacíos. Devuelve 1 si hay "TAG". */
int id3_parse_v1(const uint8_t *block128, Id3Tags *out);

#endif
