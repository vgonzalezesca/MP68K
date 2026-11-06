/*
 * ui_main — ventana del reproductor (aspecto Platinum): pantalla LCD de tiempo, información de
 * la pista, barra de posición, botones de transporte, volumen, balance y alternables.
 * Diseño de 330x144 px (ver docs/UI.md).
 *
 * Solo dibuja y atiende el ratón; las acciones las ejecuta app.c (app_command) y el motor
 * (player_*). Los bucles de seguimiento del ratón siguen llamando a player_idle() para no
 * cortar el audio.
 */
#ifndef MP368K_UI_MAIN_H
#define MP368K_UI_MAIN_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>

#include "order.h"

typedef enum {
    UI_CMD_NONE = 0,
    UI_CMD_OPEN,
    UI_CMD_PLAY,
    UI_CMD_PAUSE,
    UI_CMD_STOP,
    UI_CMD_PREV,
    UI_CMD_NEXT,
    UI_CMD_SHUFFLE,
    UI_CMD_REPEAT,
    UI_CMD_PLAYLIST
} UiCommand;

/* La implementa app.c. */
void app_command(UiCommand cmd);

OSErr ui_main_create(void);
void ui_main_destroy(void);
WindowPtr ui_main_window(void);

/* Redibuja toda la ventana (evento de actualización). */
void ui_main_draw_all(void);
/* Redibuja solo lo que cambió desde la última vez; llamar en cada vuelta del bucle. */
void ui_main_tick(void);
/* Clic en el contenido; `where` en coordenadas locales de la ventana. */
void ui_main_click(Point where);

/* Estado de los botones alternables (lo gobierna la aplicación). */
void ui_main_set_modes(Boolean shuffle, RepeatMode repeat, Boolean playlist_visible);
void ui_main_set_debug(Boolean on);

#endif
