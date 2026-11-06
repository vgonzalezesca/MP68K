/*
 * player — motor del reproductor: archivo -> códec (codec.h) -> audio.
 *
 * Un solo archivo abierto a la vez. Todo corre en el bucle principal: la aplicación
 * llama a player_idle() en cada vuelta del bucle de eventos (también dentro de sus
 * bucles de ratón) y esta decodifica lo que falte para mantener lleno el anillo de
 * audio. Nunca se llama desde interrupción.
 */
#ifndef MP368K_PLAYER_H
#define MP368K_PLAYER_H

#include <MacTypes.h>
#include <Files.h>

#include "decoder_core.h"
#include "trackinfo.h"

typedef enum { PLAYER_STOPPED = 0, PLAYER_PLAYING, PLAYER_PAUSED } PlayerState;

typedef enum {
    QUALITY_AUTO = 0,       /* elige según la velocidad medida (ver player.c) */
    QUALITY_FULL,
    QUALITY_HALF,
    QUALITY_MONO,
    QUALITY_HALF_MONO,
    QUALITY_COUNT
} PlayerQuality;

#define PLAYER_ERR_UNSUPPORTED TRACKINFO_ERR_UNSUPPORTED
#define PLAYER_ERR_PARAMS      TRACKINFO_ERR_PARAMS
#define PLAYER_ERR_NO_FILE (-4102)

typedef struct {
    PlayerState state;
    Boolean has_file;
    Boolean ended;                  /* la última pista llegó al final */
    char name[64];                  /* nombre del archivo */
    char title[64];                 /* título de la etiqueta ID3 (MacRoman), o el nombre sin extensión */
    char artist[64];                /* artista de la etiqueta ID3 (MacRoman); vacío si no hay */
    char format[8];                 /* "MP3", "FLAC"... */
    Boolean has_profiles;           /* admite FULL/HALF/MONO (solo MP3) */
    Boolean eq_available;           /* el ecualizador actúa sobre este formato */
    Boolean may_stutter;            /* la velocidad medida al empezar no alcanza para tiempo real (sin medir en el LC 580) */
    unsigned bits_src;              /* bits por muestra del archivo (0 si no aplica) */
    Boolean vbr;                    /* la cabecera dice que es VBR: bitrate es el promedio */
    unsigned bitrate;               /* bps (primer frame en CBR; promedio en VBR) */
    unsigned in_rate, in_channels;  /* del archivo */
    unsigned out_rate, out_channels, out_bits;  /* de la salida (0 si el audio no está abierto) */
    DecProfile profile;             /* perfil en uso */
    PlayerQuality quality;          /* selección del usuario */
    unsigned long position_ms;
    unsigned long duration_ms;      /* de la cabecera Xing/VBRI si hay; si no, estimada (CBR); 0 si se desconoce */
    long buffered_ms;
    unsigned long underruns;        /* buffers de silencio por falta de datos, de esta pista */
    unsigned long frames_decoded;
    OSErr last_error;
    Boolean auto_degraded;          /* el modo automático bajó de perfil en esta pista */
    unsigned long preroll_rtf_x100; /* velocidad medida al elegir el perfil automático; 0 = no medida */
    unsigned long max_gap_ms;       /* mayor intervalo entre llamadas a player_idle() mientras suena (diagnóstico) */
    unsigned long arena_peak;       /* mayor uso del arena de los códecs, en bytes (diagnóstico) */
} PlayerStatus;

OSErr player_init(void);
void player_shutdown(void);

/* Abre un archivo y lo deja detenido en 0:00. Cierra el anterior. */
OSErr player_open(FSSpec *spec);

/* Desde detenido empieza; desde pausa reanuda. */
OSErr player_play(void);
void player_pause(void);
void player_stop(void);

/* Salta a fraction_x1000/1000 de la duración (0..1000). Solo suena si está reproduciendo o en pausa. */
OSErr player_seek(unsigned fraction_x1000);

void player_set_quality(PlayerQuality q);
/* 0 = automático (16 bits si el hardware lo ofrece), 8 o 16. Si está sonando, reabre la salida en la posición actual. */
void player_set_output_bits(unsigned bits);
void player_set_volume(unsigned volume);    /* 0..100 */
void player_set_balance(int balance);       /* -100 (izquierda) .. 100 (derecha) */

void player_idle(void);
/* Verdadero si conviene volver al bucle de inmediato (falta decodificar). */
Boolean player_wants_cpu(void);

const PlayerStatus *player_status(void);
const char *player_quality_name(PlayerQuality q);

#endif
