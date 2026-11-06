/*
 * decoder_core — decodificación MP3 (libmad) con perfiles de calidad.
 *
 * C portable: sin Toolbox, sin FPU y sin asignación dinámica. Se compila igual
 * para el Mac 68k y para el PC (tests/host), de modo que el CRC del PCM de
 * ambos se pueda comparar bit a bit.
 *
 * Perfiles (SPECS §4):
 *   FULL       estéreo a la tasa del archivo.
 *   HALF       salida a mitad de tasa (synth_half de libmad).
 *   MONO       mezcla L+R en el dominio de subbandas y síntesis de un solo canal
 *              (la síntesis es lineal, así que equivale a mezclar el PCM salvo
 *              por redondeo).
 *   HALF_MONO  ambas.
 * En un archivo mono, MONO equivale a FULL y HALF_MONO a HALF.
 */
#ifndef MP368K_DECODER_CORE_H
#define MP368K_DECODER_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "prof_buckets.h"

typedef enum {
    DEC_PROFILE_FULL = 0,
    DEC_PROFILE_HALF,
    DEC_PROFILE_MONO,
    DEC_PROFILE_HALF_MONO
} DecProfile;

#define DEC_PROFILE_COUNT 4

/* Muestras PCM intercaladas máximas por frame: 1152 por canal x 2 canales. */
#define DEC_PCM_MAX 2304

/* Reloj en microsegundos (unsigned long long, igual que mp368k_prof_clock). */
typedef unsigned long long (*DecClock)(void);

typedef struct {
    DecProfile profile;
    unsigned long max_frames;   /* 0 = todo el archivo */
    int want_crc;               /* calcular CRC-32 del PCM (más lento: no cronometrar) */
    /* Recibe el PCM de cada frame (count = muestras intercaladas); NULL = descartar. */
    void (*pcm_sink)(const int16_t *pcm, unsigned count, void *ctx);
    void *sink_ctx;
    /*
     * Perfilado (etapa 3): si no es NULL, dec_run cronometra decodificación, mezcla,
     * síntesis y PCM de cada frame y llena DecResult.prof_us. Las cubetas internas de
     * libmad (PROF_SIDE..PROF_IMDCT) solo se llenan si se enlaza la biblioteca
     * parcheada (MP368K_PROFILE). NULL = sin cronometraje ni costo extra.
     */
    DecClock clock;
} DecConfig;

typedef struct {
    unsigned long frames;       /* frames decodificados correctamente */
    unsigned long errors;       /* errores de libmad (recuperables o no) */
    unsigned long samples_per_ch; /* muestras PCM por canal producidas */
    unsigned channels;          /* canales de salida (ya con el perfil aplicado) */
    unsigned out_rate;          /* Hz de salida */
    unsigned in_rate;           /* Hz del archivo */
    unsigned in_channels;       /* canales del archivo */
    unsigned bitrate;           /* bps del primer frame (0 si no hubo ninguno) */
    unsigned layer;             /* 1, 2 o 3 */
    uint32_t crc;               /* CRC-32 del PCM (big-endian por muestra); 0 si no se pidió */
    int fatal;                  /* distinto de 0 si se abortó por un error no recuperable */
    unsigned long long prof_us[PROF_COUNT];  /* us por cubeta (solo con cfg->clock) */
    unsigned long prof_marks;   /* marcas de tiempo ejecutadas dentro de libmad */
} DecResult;

const char *dec_profile_name(DecProfile p);

/* Cadena "libmad <versión> / <opciones de compilación>" (mad_version, mad_build). */
const char *dec_library_info(void);

/* CRC-32 estándar de un bloque de bytes (sirve para comprobar que un archivo no se corrompió al transferirlo). */
uint32_t dec_crc32(const uint8_t *data, size_t size);

/*
 * Decodifica un MP3 completo en memoria. Reinicia todo el estado interno en cada
 * llamada. Salta una etiqueta ID3v2 inicial si existe. Devuelve 0 si decodificó
 * al menos un frame, -1 en otro caso.
 */
int dec_run(const uint8_t *data, size_t size, const DecConfig *cfg, DecResult *out);

/* ------------------------------------------------------------------------ */
/* Decodificación en flujo (reproductor): el MP3 se lee por bloques            */
/* ------------------------------------------------------------------------ */

/* Lee hasta `max` bytes en `dst`. Devuelve 0 si estuvo bien; *got = 0 indica fin de archivo. */
typedef int (*DecReadFn)(void *ctx, uint8_t *dst, long max, long *got);
/* Reposiciona la lectura en `offset` bytes desde el inicio. Devuelve 0 si estuvo bien. */
typedef int (*DecSeekFn)(void *ctx, long offset);

typedef struct {
    const int16_t *pcm;     /* PCM intercalado del frame; vale hasta la próxima llamada */
    unsigned count;         /* muestras intercaladas (frames x canales) */
    unsigned frames;        /* muestras por canal */
    unsigned channels;      /* canales de salida (con el perfil aplicado) */
    unsigned rate;          /* Hz de salida */
    unsigned bitrate;       /* bps del frame */
    unsigned in_rate;       /* Hz del archivo */
    unsigned in_channels;   /* canales del archivo */
    unsigned layer;
    long file_pos;          /* byte del archivo donde empieza el frame siguiente */
} DecFrame;

/*
 * Abre un flujo. `buf` (>= DEC_STREAM_MIN_BUF bytes) es el buffer de lectura y debe
 * seguir vivo hasta dec_stream_close. Salta una etiqueta ID3v2 inicial. Solo hay un
 * flujo a la vez, y no se puede mezclar con dec_run. Devuelve 0 si estuvo bien.
 */
#define DEC_STREAM_MIN_BUF 4096
int dec_stream_open(DecProfile profile, DecReadFn read, DecSeekFn seek, void *ctx,
                    uint8_t *buf, size_t bufsize);

/*
 * Decodifica el siguiente frame. Los frames corruptos se saltan (se cuentan en
 * dec_stream_errors). Devuelve 1 con un frame, 0 al terminar el archivo y -1 si hubo un
 * error fatal de lectura o de libmad.
 */
int dec_stream_next(DecFrame *out);

/* Salta a `offset` bytes del archivo. Los primeros frames pueden salir silenciados. */
int dec_stream_seek(long offset);

unsigned long dec_stream_errors(void);

/* Byte donde empieza el audio (tras la etiqueta ID3v2 inicial, si había). */
long dec_stream_audio_start(void);

void dec_stream_close(void);

#endif
