/*
 * refcrc — lee REFERENCE.txt (generado por tools/host-ref.sh) junto a la app.
 *
 * Cada línea de datos es:
 *   kind,file,file_crc,profile,frames,errors,samples_per_ch,channels,out_rate,pcm_crc
 * kind = "bench" (decbench) o "prof" (decprofile). Acepta fin de línea LF o CR.
 */
#ifndef MP368K_REFCRC_H
#define MP368K_REFCRC_H

#include <MacTypes.h>

/* Carga REFERENCE.txt; devuelve noErr si existe. */
OSErr refcrc_load(void);

/* Busca (kind, file, profile). Devuelve 1 y los CRC si la encuentra. */
int refcrc_find(const char *kind, const char *file, const char *profile,
                unsigned long *pcm_crc, unsigned long *file_crc);

void refcrc_free(void);

#endif
