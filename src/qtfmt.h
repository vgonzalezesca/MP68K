/*
 * qtfmt — reconoce los archivos de audio que se reproducen por QuickTime (WAV, AIFF, AIFC y AU de Sun) y lee su
 * duración, tasa, canales y bits SOLO de la cabecera, sin decodificar.
 *
 * Por qué existe: el reproductor no decodifica estos formatos (los abre QuickTime), y en 68k no tenemos los selectores
 * del trap de QuickTime para pedirle la duración a la película (ver docs/DECISIONS.md, ADR-010). La cabecera de estos
 * contenedores es trivial y da la duración exacta sin depender de QuickTime. Tampoco sirve para decidir si QuickTime
 * abrirá el archivo: eso solo lo sabe QuickTime (qttest en QuickTime 2.5 abrió WAV PCM, AIFF, AIFC y AU, y rechazó
 * WAV mu-law, CAF y M4A). `pcm` solo informa si el contenido es PCM lineal.
 *
 * Portable (sin Toolbox), solo enteros; se prueba en el PC (tests/host/test_qtfmt.c). No reconoce MP3, FLAC, Ogg ni WMA:
 * esos son de los códecs propios (codec.h).
 */
#ifndef MP368K_QTFMT_H
#define MP368K_QTFMT_H

#include <stddef.h>
#include <stdint.h>

#include "codec.h"

typedef struct {
    char format[8];                 /* "WAV", "AIFF", "AIFC" o "AU" */
    unsigned rate;                  /* Hz */
    unsigned channels;
    unsigned bits;                  /* bits por muestra; 0 si no se sabe */
    unsigned long duration_ms;      /* 0 si se desconoce */
    int pcm;                        /* 1 si es PCM lineal (entero), 0 si es otra codificación (mu-law, ADPCM...) */
} QtInfo;

/* ¿Los primeros bytes del archivo son una cabecera RIFF/WAVE, FORM/AIFF|AIFC o ".snd"? (necesita n >= 12) */
int qtfmt_sniff(const uint8_t *head, size_t n);

/*
 * Lee la cabecera por io->seek/io->read. Devuelve CODEC_OK, CODEC_ERR_UNSUPPORTED (cabecera dañada o sin los
 * bloques necesarios), CODEC_ERR_PARAMS (tasa o canales en cero) o CODEC_ERR_IO.
 */
int qtfmt_probe(const CodecIO *io, QtInfo *qi);

/* ¿La extensión (.wav .aif .aiff .aifc .au .snd) o el tipo de Mac (WAVE AIFF AIFC ULAW) sugieren uno de estos formatos? */
int qtfmt_name_matches(const char *name, unsigned long mac_type);

#endif
