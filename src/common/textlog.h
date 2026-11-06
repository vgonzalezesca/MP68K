/*
 * textlog — bitácora de texto para programas de prueba de MP368K.
 *
 * Acumula texto en un buffer estático, lo muestra en la consola de Retro68 y
 * al final lo guarda en un archivo junto a la aplicación. Pensado para sysprobe
 * y el banco de pruebas: NO usar en el camino caliente ni en interrupciones
 * (usa vsnprintf y el File Manager).
 */
#ifndef MP368K_TEXTLOG_H
#define MP368K_TEXTLOG_H

#include <MacTypes.h>

/* Vacía el buffer. Llamar una vez al inicio. */
void textlog_init(void);

/* Agrega texto con formato printf; también lo imprime en la consola. */
void textlog_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Verdadero si algún texto no cupo en el buffer. */
Boolean textlog_truncated(void);

/*
 * Guarda el buffer en `name` (cadena C) en la carpeta de la aplicación.
 * Convierte '\n' a '\r' (fin de línea de Mac OS clásico) y crea el archivo con
 * creador 'ttxt' (SimpleText). Reemplaza el archivo si ya existe.
 */
OSErr textlog_save_next_to_app(const char *name);

#endif
