/*
 * ui_list — ventana de la lista de reproducción (aspecto Platinum): filas con "n  Artista - Título"
 * y duración, marcador de pista actual, selección con el color de resaltado del sistema, barra
 * de desplazamiento nativa (Platinum bajo el Appearance Manager) y botones.
 */
#ifndef MP368K_UI_LIST_H
#define MP368K_UI_LIST_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>

typedef enum { LIST_ADD, LIST_ADD_FOLDER, LIST_REMOVE, LIST_CLEAR, LIST_PLAY } ListCommand;

/* La implementa app.c. `arg` es el índice de fila para LIST_REMOVE y LIST_PLAY. */
void app_list_command(ListCommand cmd, int arg);

OSErr ui_list_create(void);
void ui_list_destroy(void);
WindowPtr ui_list_window(void);

void ui_list_show(Boolean show);
Boolean ui_list_visible(void);

void ui_list_draw_all(void);
/* Redibuja las filas y la barra después de cambiar la lista o la pista actual. */
void ui_list_refresh(void);
void ui_list_activate(Boolean active);

/* Clic en el contenido; `when` = instante del evento (para detectar el doble clic). */
void ui_list_click(Point where, unsigned long when);
/* Teclas con la ventana al frente: flechas, Return/Enter (reproducir), Delete (quitar). */
void ui_list_key(char c);

int ui_list_selected(void);
void ui_list_set_selected(int index);
/* Se asegura de que la fila `index` esté a la vista. */
void ui_list_show_row(int index);

#endif
