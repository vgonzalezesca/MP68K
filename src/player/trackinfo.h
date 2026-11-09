/*
 * trackinfo — reconoce el formato de un archivo de audio y lee su etiqueta y su duración, sin
 * decodificar. Lo usan el motor (al abrir una pista) y la lista (al agregar archivos). Es el
 * puente entre el File Manager (FileReader) y la capa de códecs (codec.h).
 */
#ifndef MP368K_TRACKINFO_H
#define MP368K_TRACKINFO_H

#include <MacTypes.h>

#include "codec.h"
#include "fileread.h"

#define TRACKINFO_ERR_UNSUPPORTED (-4101)   /* no es un formato conocido, o está dañado */
#define TRACKINFO_ERR_PARAMS      (-4103)   /* formato conocido con canales o tasa no admitidos */
#define TRACKINFO_ERR_NO_QUICKTIME (-4104)  /* es WAV/AIFF/AU, que se reproduce por QuickTime, y no está instalado */

/*
 * Reconoce el archivo abierto en `fr` y llena `io` (para abrir el flujo después) y `ci`. `buf` (de
 * `cap` >= 8192 bytes) es memoria de trabajo. Devuelve noErr y el códec en *codec; si no, un error:
 * TRACKINFO_ERR_UNSUPPORTED, TRACKINFO_ERR_PARAMS, memFullErr o ioErr.
 */
OSErr trackinfo_probe(FileReader *fr, CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci, const Codec **codec);

/*
 * Como trackinfo_probe, pero antes mira si el archivo es de los que se reproducen por QuickTime (WAV, AIFF, AIFC, AU;
 * qtfmt.h). En ese caso devuelve noErr con *codec = NULL y *via_qt = verdadero, y `ci` con formato, tasa, canales, bits y
 * duración sacados de la cabecera (sin etiquetas). Si es de esos formatos y QuickTime no está, TRACKINFO_ERR_NO_QUICKTIME.
 */
OSErr trackinfo_probe_ex(FileReader *fr, CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci, const Codec **codec,
                         Boolean *via_qt);

/* Nombre de archivo sin extensión (solo si es corta, como ".mp3"). */
void trackinfo_name_without_extension(const char *name, char *out, size_t cap);

/* "Artista - Título" si hay etiqueta; si no, el nombre del archivo sin extensión. */
void trackinfo_display_name(const char *filename, const Id3Tags *tg, char *out, size_t cap);

#endif
