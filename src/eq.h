/*
 * eq — ecualizador de 10 bandas que actúa sobre los coeficientes de frecuencia del MP3.
 *
 * En vez de filtrar el PCM muestra a muestra (≈ 267 % de la CPU de un 68LC040 a 44,1 kHz estéreo,
 * aritmética, sin medir), multiplica cada coeficiente por una ganancia justo antes de la IMDCT, con
 * el gancho mp368k_eq_hook de la libmad parcheada (third_party/patches/libmad-0.15.1b-eq.patch). Cuesta
 * a lo sumo 576 multiplicaciones por granulo y canal (≈ 2 304 por frame). Solo enteros.
 *
 * Con el EQ apagado o plano el gancho vale NULL y el PCM es idéntico bit a bit al de libmad sin parchear.
 * Límite conocido: a 44,1 kHz una línea mide ~38 Hz, así que las bandas de 70, 180 y 320 Hz tienen
 * muy poca resolución. Portable (sin Toolbox).
 */
#ifndef MP368K_EQ_H
#define MP368K_EQ_H

#include <stdint.h>

#define EQ_BANDS 10
#define EQ_STEP_MAX 24          /* pasos de 0,5 dB: ±12 dB */

typedef struct {
    int enabled;
    int8_t band[EQ_BANDS];      /* pasos de 0,5 dB, de -24 a +24 */
    int8_t preamp;              /* pasos de 0,5 dB, de -24 a +24; se suma a cada banda */
} EqSettings;

/* Frecuencias centrales: 70, 180, 320, 600, 1K, 3K, 6K, 12K, 14K, 16K Hz (por confirmar contra un Winamp real). */
extern const unsigned short kEqBandHz[EQ_BANDS];

void eq_flat(EqSettings *s);                    /* todo en 0 (conserva `enabled`) */
/* Aplica los ajustes (los recorta a ±24) e instala o quita el gancho de libmad. */
void eq_set(const EqSettings *s);
const EqSettings *eq_get(void);
/* Verdadero si el gancho está instalado: activo y con alguna ganancia distinta de 0 dB. */
int eq_is_active(void);

/* Lo llama libmad por el gancho; expuesto para pruebas. xr es mad_fixed_t (signed int, Q28). */
void eq_apply_granule(signed int xr[576], unsigned int samplerate, int block);

#ifdef MP368K_EQ_STATS
/* Solo en las pruebas: granulos procesados con bloque largo, corto y mixto. */
void eq_stats(unsigned long out[3]);
#endif

#endif
