/*
 * codec — interfaz común de los decodificadores de audio (MP3, FLAC, ...).
 *
 * Un solo flujo abierto a la vez, con estado estático (como dec_stream_*). Los códecs entregan
 * el PCM de 16 bits intercalado en rebanadas de a lo sumo CODEC_MAX_FRAMES muestras por canal,
 * aunque su bloque nativo sea mayor, de modo que el bucle del motor no cambie.
 *
 * Portable (sin Toolbox): la E/S llega por los callbacks de CodecIO y la memoria de los códecs
 * de terceros por un arena que reserva la aplicación (codec_init). Se prueba en el PC.
 */
#ifndef MP368K_CODEC_H
#define MP368K_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "decoder_core.h"
#include "id3.h"

#define CODEC_MAX_FRAMES 1152       /* muestras por canal en cada DecFrame */

/* Códigos de error (negativos). */
#define CODEC_OK              0
#define CODEC_ERR_IO         (-1)   /* lectura o posicionamiento fallidos */
#define CODEC_ERR_UNSUPPORTED (-2)  /* no es un formato conocido, o está dañado */
#define CODEC_ERR_PARAMS     (-3)   /* formato conocido, pero con canales o tasa que no se pueden reproducir */
#define CODEC_ERR_MEMORY     (-4)   /* no cupo en el arena */

/* Entrada de datos: callbacks de decoder_core.h más el tamaño total del archivo. */
typedef struct {
    DecReadFn read;
    DecSeekFn seek;
    void *ctx;
    long size;
    long audio_start;       /* byte tras una etiqueta ID3v2 inicial (0 si no hay); lo llena codec_probe_any */
} CodecIO;

typedef struct {
    char format[8];                 /* "MP3", "FLAC" */
    unsigned rate;                  /* Hz del archivo */
    unsigned channels;              /* 1 o 2 */
    unsigned bits_src;              /* bits por muestra del archivo (0 si no aplica) */
    unsigned bitrate;               /* bps; en VBR, el promedio; 0 si se desconoce */
    int vbr;
    unsigned long duration_ms;      /* 0 si se desconoce */
    Id3Tags tags;                   /* título, artista y álbum en MacRoman */
    int has_profiles;               /* admite FULL/HALF/MONO (solo MP3) */
    int eq_capable;                 /* el ecualizador actúa sobre este formato */
    unsigned char priv[320];        /* estado del códec entre probe y open (no tocar) */
} CodecInfo;

typedef struct Codec {
    const char *name;
    /* `head` son los primeros bytes tras una etiqueta ID3v2. Devuelve 1 si reconoce el contenido. */
    int (*sniff)(const uint8_t *head, size_t n);
    /* Lee etiquetas y duración sin decodificar. `buf` (>= 8192 bytes) es memoria de trabajo. */
    int (*probe)(const CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci);
    /* Abre el flujo. `buf` (>= DEC_STREAM_MIN_BUF) debe seguir vivo hasta close. `prof` solo cuenta en MP3. */
    int (*open)(const CodecIO *io, DecProfile prof, uint8_t *buf, size_t bufsize, const CodecInfo *ci);
    /* Siguiente rebanada: 1 con datos, 0 al terminar el archivo, -1 en error fatal. */
    int (*next)(DecFrame *out);
    int (*seek)(unsigned long ms);
    void (*close)(void);
    /* Opcional (NULL = el pico del arena): bytes de memoria usados como máximo desde open. */
    size_t (*mem_peak)(void);
} Codec;

/* Reserva para los códecs que asignan memoria. Llamar una vez, antes de abrir nada. */
void codec_init(void *arena_mem, size_t size);
Arena *codec_arena(void);

/*
 * Reconoce el archivo por su contenido y lee su información. Devuelve el códec o NULL, con el
 * código de error en *err. Llena io->audio_start.
 */
const Codec *codec_probe_any(CodecIO *io, uint8_t *buf, long cap, CodecInfo *ci, int *err);

/* ¿La extensión o el tipo de archivo de Mac sugieren un formato que sabemos abrir? */
int codec_name_matches(const char *name, unsigned long mac_type);

/* Los códecs registrados (para pruebas). */
extern const Codec codec_mp3;
extern const Codec codec_flac;
extern const Codec codec_vorbis;

#endif
