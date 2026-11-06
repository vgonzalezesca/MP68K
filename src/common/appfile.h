/*
 * appfile — localiza la carpeta de la aplicación y carga archivos a memoria.
 *
 * Solo para el banco de pruebas y la inicialización: usa el File Manager y
 * asigna memoria, así que NO llamar desde el callback de audio ni en el bucle
 * de decodificación.
 */
#ifndef MP368K_APPFILE_H
#define MP368K_APPFILE_H

#include <MacTypes.h>
#include <Files.h>

#include "memblock.h"

/* FSSpec de la aplicación en ejecución (Process Manager, System 7+). */
OSErr appfile_spec(FSSpec *spec);

typedef struct {
    void *data;     /* contenido del archivo (NULL si no hay nada cargado) */
    long size;      /* bytes */
    MemBlock mem;   /* memoria que lo contiene */
} FileBlob;

/*
 * Carga completo `name` (cadena C, carpeta de la app) en memoria (ver memblock.h).
 */
OSErr appfile_load(const char *name, FileBlob *out);

/* Carga el archivo `spec`; si max_bytes > 0 y el archivo es mayor, carga solo los primeros max_bytes. */
OSErr appfile_load_spec(FSSpec *spec, long max_bytes, FileBlob *out);

void appfile_free(FileBlob *f);

#endif
