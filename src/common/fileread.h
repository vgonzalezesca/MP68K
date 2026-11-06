/*
 * fileread — lectura secuencial de un archivo por bloques (para DecReadFn/DecSeekFn).
 *
 * Lectura SÍNCRONA con FSRead desde el bucle principal. Es una simplificación
 * deliberada de la primera versión (la lectura asíncrona con PBReadAsync queda para
 * cuando las mediciones muestren que hace falta). Nunca llamar desde interrupción.
 */
#ifndef MP368K_FILEREAD_H
#define MP368K_FILEREAD_H

#include <MacTypes.h>
#include <Files.h>

#include <stdint.h>

typedef struct {
    short ref;      /* 0 = cerrado */
    long size;
} FileReader;

/* Abre `name` (cadena C) en la carpeta de la aplicación. */
OSErr fileread_open_app_file(FileReader *f, const char *name);
OSErr fileread_open_spec(FileReader *f, FSSpec *spec);
void fileread_close(FileReader *f);

/* DecReadFn / DecSeekFn: ctx es un FileReader*. */
int fileread_read(void *ctx, uint8_t *dst, long max, long *got);
int fileread_seek(void *ctx, long offset);

#endif
