/*
 * audio — salida de sonido con el Sound Manager (doble buffer) y anillo de PCM.
 *
 * Flujo (skill mac-sound-playback):
 *   bucle principal --audio_write()--> anillo --callback del Sound Manager--> hardware
 * El callback corre en tiempo de interrupción: solo copia del anillo a los buffers,
 * rellena con silencio si falta y actualiza contadores. No asigna memoria, no llama al
 * File Manager y no toca el Toolbox.
 *
 * Requiere Sound Manager 3.0 o superior (SndPlayDoubleBuffer). El formato de salida
 * (8 o 16 bits) se negocia con el hardware al abrir; el PCM del decodificador siempre
 * es de 16 bits y se convierte aquí al escribir en el anillo, no en el callback.
 * Un único canal abierto a la vez.
 */
#ifndef MP368K_AUDIO_H
#define MP368K_AUDIO_H

#include <MacTypes.h>

#include <stdint.h>

#define AUDIO_ERR_NO_DOUBLEBUFFER (-4001)   /* Sound Manager < 3.0 */

typedef struct {
    unsigned sm_major, sm_minor;    /* versión del Sound Manager */
    unsigned rate;                  /* Hz */
    unsigned channels;
    unsigned bits;                  /* 8 o 16: lo que se envía al Sound Manager */
    unsigned hw_sizes;              /* bit 0 = 8 bits disponible, bit 1 = 16 bits; 0 = desconocido */
    unsigned long ring_bytes;
    unsigned buf_frames;            /* frames por buffer del doble buffer */
} AudioInfo;

/*
 * Abre el canal y reserva memoria (anillo de ring_ms milisegundos y dos buffers; 0 =
 * 1000). want_bits: 0 = automático (16 si el hardware lo ofrece; si no, 8), 8 o 16 para
 * forzar. Un anillo más largo tolera más tiempo con el bucle principal bloqueado (menús,
 * arrastre de ventanas).
 */
OSErr audio_open(unsigned rate, unsigned channels, unsigned want_bits, unsigned ring_ms,
                 AudioInfo *info);

/*
 * Detiene el canal, vacía el anillo y crea un canal nuevo con el mismo formato, sin
 * liberar memoria (para buscar o reiniciar). frames_played vuelve a 0; underruns sigue
 * acumulando. Luego hay que volver a llenar el anillo y llamar a audio_start().
 */
OSErr audio_restart(void);

/*
 * Ganancia por software por canal, 0..256 (256 = unidad, sin costo de CPU). En mono se
 * usa solo `left`. Se aplica a lo que se escribe desde ahora (el anillo ya lleno conserva
 * la ganancia anterior: el cambio se oye con el retardo del anillo).
 */
void audio_set_gain(unsigned left, unsigned right);

/* Para el canal (descartando lo pendiente) y libera la memoria. */
void audio_close(void);

/*
 * Escribe `count` muestras intercaladas de 16 bits (frames x canales) en el anillo,
 * convirtiéndolas al formato de salida. Devuelve cuántas aceptó (puede ser menos si el
 * anillo no tiene lugar; siempre un número entero de frames).
 */
long audio_write(const int16_t *pcm, long count);

/* Frames que caben ahora mismo en el anillo. */
long audio_space_frames(void);
long audio_buffered_frames(void);

/* Arranca la reproducción con lo que haya en el anillo (llenar antes: pre-roll). */
OSErr audio_start(void);

/* Pausa: el canal sigue vivo y suena silencio sin consumir el anillo. */
void audio_set_paused(Boolean paused);

/* Avisa que no habrá más datos: al vaciarse el anillo termina la reproducción. */
void audio_finish(void);
Boolean audio_is_done(void);

/* Buffers del doble buffer que salieron con silencio por falta de datos (acumulado desde el inicio del programa). */
unsigned long audio_underruns(void);
/* Frames reproducidos de verdad (no cuenta silencio de pausa ni de falta de datos). */
unsigned long audio_frames_played(void);

#endif
