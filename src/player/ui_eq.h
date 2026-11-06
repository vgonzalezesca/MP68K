/*
 * ui_eq — ventana del ecualizador (aspecto Platinum): casilla On, cuatro presets, deslizador de
 * preamp y 10 deslizadores verticales (70, 180, 320, 600, 1K, 3K, 6K, 12K, 14K, 16K Hz).
 * Diseño de 330x206 px (ver docs/UI.md). Se muestra desde Window > Equalizer.
 *
 * Solo dibuja y atiende el ratón; el sonido lo cambia el motor (player_set_eq). Los bucles de
 * seguimiento del ratón llaman a player_idle() para no cortar el audio.
 */
#ifndef MP368K_UI_EQ_H
#define MP368K_UI_EQ_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>

OSErr ui_eq_create(void);
void ui_eq_destroy(void);
WindowPtr ui_eq_window(void);

/* Al mostrarla se baja el colchón de audio (para que los cambios se oigan pronto); al ocultarla vuelve a 4 s. */
void ui_eq_show(Boolean show);
Boolean ui_eq_visible(void);

void ui_eq_draw_all(void);
/* Redibuja lo que cambió (disponibilidad según el formato); llamar en cada vuelta del bucle. */
void ui_eq_tick(void);
/* Clic en el contenido; `where` en coordenadas locales de la ventana. */
void ui_eq_click(Point where);

#endif
