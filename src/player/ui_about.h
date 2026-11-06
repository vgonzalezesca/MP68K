/*
 * ui_about — ventana "About MP368K" con el logo. Es una ventana más del bucle de eventos
 * (no un diálogo modal), así que el audio sigue alimentándose mientras está abierta.
 *
 * El logo es el recurso 'LOGO' (128): 96x96 bytes de 8 bits, índices de la tabla de colores
 * estándar del sistema (clut 8). Lo genera tools/make_logo.py.
 */
#ifndef MP368K_UI_ABOUT_H
#define MP368K_UI_ABOUT_H

#include <MacTypes.h>
#include <Windows.h>

/* Abre la ventana (o la trae al frente si ya existe). */
OSErr ui_about_show(void);
void ui_about_close(void);
WindowPtr ui_about_window(void);   /* NULL si no está abierta */
void ui_about_draw(void);          /* evento de actualización */

#endif
