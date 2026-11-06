/*
 * playlist — lista de reproducción: archivos, etiquetas y orden (shuffle/repeat vía order.h).
 *
 * Hasta PLAYLIST_MAX entradas en memoria reservada una vez. Los métodos que agregan archivos leen
 * la cabecera de cada MP3 (unos pocos KB por archivo) y descartan lo que no sea un MP3 válido.
 * Todo corre en el bucle principal; nunca desde interrupción.
 */
#ifndef MP368K_PLAYLIST_H
#define MP368K_PLAYLIST_H

#include <MacTypes.h>
#include <Files.h>

#include "order.h"

#define PLAYLIST_MAX ORDER_MAX

typedef struct {
    FSSpec spec;
    char label[64];             /* "Artista - Título" o el nombre sin extensión (MacRoman) */
    unsigned long duration_ms;  /* 0 si se desconoce */
} PlaylistEntry;

OSErr playlist_init(void);
void playlist_shutdown(void);

Order *playlist_order(void);
unsigned playlist_count(void);
const PlaylistEntry *playlist_get(unsigned index);

/* Agrega un archivo. Devuelve su índice, o -1 si no es un MP3 válido o la lista está llena. */
int playlist_add_file(FSSpec *spec);

/*
 * Agrega todos los MP3 de la carpeta donde está `any_file` (por extensión .mp3 o tipo MPG3/MP3),
 * ordenados por nombre. Devuelve cuántos agregó; *skipped = los que no se pudieron leer.
 */
unsigned playlist_add_folder_of(FSSpec *any_file, unsigned *skipped);

/*
 * Agrega los archivos de una lista .m3u. Las rutas relativas se resuelven desde la carpeta del
 * .m3u; las absolutas o de Windows se buscan por nombre en esa misma carpeta. Devuelve cuántos
 * agregó; *skipped = entradas que no se encontraron o no son MP3.
 */
unsigned playlist_add_m3u(FSSpec *m3u, unsigned *skipped);

void playlist_remove(unsigned index);
void playlist_clear(void);

/* ¿El nombre (cadena Pascal) termina en .m3u o .m3u8? */
Boolean playlist_is_m3u(const unsigned char *pname);

#endif
