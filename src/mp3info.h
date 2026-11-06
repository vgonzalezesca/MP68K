/*
 * mp3info — datos de un MP3 sin decodificarlo: formato del primer frame, cabecera Xing/Info/
 * VBRI, duración y correspondencia tiempo -> byte para buscar.
 *
 * Portable (sin Toolbox). Límites conocidos: de VBRI solo se leen frames y bytes (la búsqueda
 * es lineal); el contador de frames de la cabecera se toma tal cual (no resta el retardo del
 * codificador, ~25 ms).
 */
#ifndef MP368K_MP3INFO_H
#define MP368K_MP3INFO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned version;           /* 1 = MPEG-1, 2 = MPEG-2, 25 = MPEG-2.5 */
    unsigned layer;             /* 1, 2 o 3 */
    unsigned bitrate;           /* bps del primer frame */
    unsigned sample_rate;       /* Hz */
    unsigned channels;          /* 1 o 2 */
    unsigned samples_per_frame;
    int has_vbr_tag;            /* 1 si hay cabecera Xing, Info o VBRI */
    int is_vbr;                 /* 1 si la cabecera dice VBR ("Xing" o VBRI) */
    int has_toc;                /* 1 si hay tabla de 100 entradas (Xing) */
    uint8_t toc[100];
    unsigned long frames;       /* de la cabecera; 0 si no hay */
    unsigned long tag_bytes;    /* bytes de audio según la cabecera; 0 si no hay */
    unsigned long duration_ms;  /* 0 si no se pudo calcular */
    int duration_exact;         /* 1 si sale de la cabecera, 0 si es una estimación CBR */
    long audio_start;           /* byte del primer frame (tras la etiqueta ID3v2) */
    long audio_bytes;           /* bytes de audio: tamaño - inicio - ID3v1 */
    long first_frame_offset;    /* posición del primer frame dentro de `buf` */
} Mp3Info;

/*
 * `buf` son los bytes del archivo desde `audio_start` (conviene >= 4096). `file_size` es el
 * tamaño total. Devuelve 0 si encontró un frame válido, -1 si no.
 */
int mp3info_parse(const uint8_t *buf, size_t len, long audio_start, long file_size, int has_id3v1,
                  Mp3Info *out);

/* Byte del archivo donde está el instante `ms` (con TOC de Xing si hay; si no, lineal). */
long mp3info_offset_for_ms(const Mp3Info *m, unsigned long ms);

#endif
