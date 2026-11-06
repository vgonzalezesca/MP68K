/*
 * trackinfo — lee de un MP3 su etiqueta (ID3v2/ID3v1) y su cabecera de duración, sin decodificar.
 * Lo usan el motor (al abrir una pista) y la lista (al agregar archivos).
 */
#ifndef MP368K_TRACKINFO_H
#define MP368K_TRACKINFO_H

#include <MacTypes.h>

#include "fileread.h"
#include "id3.h"
#include "mp3info.h"

#define TRACKINFO_ERR_NOT_MP3 (-4101)

/*
 * Lee la etiqueta y la cabecera del archivo abierto en `fr`. `buf` (de `cap` >= 8192 bytes) es
 * memoria de trabajo. Devuelve noErr, TRACKINFO_ERR_NOT_MP3 si no hay un frame MP3 válido, o un
 * error de lectura.
 */
OSErr trackinfo_probe(FileReader *fr, uint8_t *buf, long cap, Id3Tags *tg, Mp3Info *mi);

/* Nombre de archivo sin extensión (solo si es corta, como ".mp3"). */
void trackinfo_name_without_extension(const char *name, char *out, size_t cap);

/* "Artista - Título" si hay etiqueta; si no, el nombre del archivo sin extensión. */
void trackinfo_display_name(const char *filename, const Id3Tags *tg, char *out, size_t cap);

#endif
