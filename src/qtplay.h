/*
 * qtplay — reproducción de un archivo por QuickTime (WAV, AIFF, AIFC y AU; ver qtfmt.h).
 *
 * QuickTime abre su propio canal del Sound Manager y mezcla por su cuenta: el motor (player.c) cierra el suyo mientras
 * suena una película y llama a qtplay_task() en cada vuelta del bucle principal. Nunca desde interrupción.
 *
 * Solo hay implementación donde se compilen las declaraciones de QuickTime del equipo (68k: targets/qtplay.c, con las
 * Multiversal Interfaces). Sin ella (MP368K_HAVE_QT sin definir) quedan los reemplazos de abajo: QuickTime "no está
 * instalado" y el reproductor se comporta como antes.
 *
 * Qué NO hay, a propósito: duración, posición, búsqueda y volumen de la película. Esos selectores del trap 0xAAAA no
 * están en las Multiversal y no se inventan (ADR-010): el motor usa la duración de la cabecera (qtfmt) y estima la
 * posición con TickCount.
 */
#ifndef MP368K_QTPLAY_H
#define MP368K_QTPLAY_H

#include <MacTypes.h>
#include <Files.h>

#ifdef MP368K_HAVE_QT

/* Gestalt('qtim') y EnterMovies, una sola vez. Verdadero si QuickTime está y se pudo iniciar. */
Boolean qtplay_available(void);
/* Versión de QuickTime (palabra alta de Gestalt('qtim'), p. ej. 0x0250 = 2.5); 0 si no hay. */
unsigned qtplay_version(void);
/* Abre el archivo como película y la deja lista (PrerollMovie), detenida al comienzo. */
OSErr qtplay_open(const FSSpec *spec);
void qtplay_start(void);                /* inicia o reanuda */
void qtplay_stop(void);                 /* pausa (mantiene la posición) */
OSErr qtplay_rewind(void);              /* vuelve al comienzo (queda detenida) */
void qtplay_task(void);                 /* MoviesTask: llamar seguido mientras suena */
Boolean qtplay_is_done(void);
void qtplay_close(void);                /* detiene y descarta la película */
void qtplay_shutdown(void);             /* ExitMovies, al salir de la aplicación */

#else

static inline Boolean qtplay_available(void) { return 0; }
static inline unsigned qtplay_version(void) { return 0; }
static inline OSErr qtplay_open(const FSSpec *spec) { (void)spec; return paramErr; }
static inline void qtplay_start(void) {}
static inline void qtplay_stop(void) {}
static inline OSErr qtplay_rewind(void) { return paramErr; }
static inline void qtplay_task(void) {}
static inline Boolean qtplay_is_done(void) { return 1; }
static inline void qtplay_close(void) {}
static inline void qtplay_shutdown(void) {}

#endif

#endif
