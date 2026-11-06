/*
 * ui_draw — primitivas de dibujo con aspecto Platinum (Mac OS 8), con QuickDraw en color.
 *
 * Todo se dibuja en el puerto actual (el llamador hace SetPort). Sin recursos gráficos:
 * biseles, pozos hundidos, glifos y dígitos LCD se trazan con rectángulos. El texto se
 * pinta opaco (srcCopy) con recorte, de modo que redibujarlo no parpadea.
 *
 * Las Multiversal Interfaces no traen las APIs de temas del Appearance Manager, así que el
 * aspecto se imita con colores y biseles propios; funciona en cualquier System 7 o posterior.
 */
#ifndef MP368K_UI_DRAW_H
#define MP368K_UI_DRAW_H

#include <Quickdraw.h>

typedef enum { GLYPH_PREV, GLYPH_PLAY, GLYPH_PAUSE, GLYPH_STOP, GLYPH_NEXT, GLYPH_EJECT } UiGlyph;

/* Paleta Platinum. */
extern const RGBColor kPlatBg;      /* fondo de ventana, #DDDDDD */
extern const RGBColor kPlatWhite;
extern const RGBColor kPlatBlack;
extern const RGBColor kPlatText;    /* texto normal */
extern const RGBColor kPlatDim;     /* texto atenuado */
extern const RGBColor kPlatAccent;  /* azul de selección / glifo activo */
extern const RGBColor kLcdBg, kLcdOn, kLcdOff;

/* Fuentes: la del sistema (Charcoal bajo Platinum) y Geneva para texto chico. */
#define UI_FONT_SYSTEM 0
#define UI_FONT_SMALL  3

/* Reserva la región auxiliar para recortes. Llamar una vez, tras InitGraf. */
void ui_draw_init(void);
void ui_draw_shutdown(void);

void ui_fill(const Rect *r, const RGBColor *c);

/* Pozo hundido (pantallas, listas, ranuras) con el interior pintado de `fill`. */
void ui_well(const Rect *r, const RGBColor *fill);

/* Botón Platinum: contorno negro de esquinas recortadas, degradado suave y bisel. */
void ui_push_frame(const Rect *r, Boolean pressed);

/* Pantalla LCD "MM:SS" dentro de `box` (ya pintada como pozo); valid = falso muestra "--:--". */
void ui_lcd_time(const Rect *box, unsigned long ms, Boolean valid);

/* Botón con glifo; lit = el glifo se pinta con el azul de acento. */
void ui_glyph_button(const Rect *r, UiGlyph g, Boolean pressed, Boolean lit);
/* Botón de texto; on = botón alternable activado (se ve hundido). */
void ui_text_button(const Rect *r, const char *label, Boolean pressed, Boolean on, Boolean enabled);

/* Control deslizante horizontal: ranura hundida y perilla en pos_x1000 (0..1000). */
void ui_slider(const Rect *track, unsigned pos_x1000, Boolean enabled);
/* Posición (0..1000) que corresponde a la coordenada x del ratón sobre `track`. */
unsigned ui_slider_pos(const Rect *track, short x);

/* Marcador de "pista actual" (triángulo) con la esquina superior izquierda en (x, y). */
void ui_play_marker(short x, short y, const RGBColor *c);

/*
 * Texto opaco: dibuja `s` con la línea base en `baseline`, empezando en x, recortado a
 * `clip` y con fondo `bg`. Devuelve el ancho del texto en píxeles.
 */
short ui_text(const Rect *clip, short x, short baseline, const char *s, const RGBColor *fg,
              const RGBColor *bg, short font, short size);
short ui_text_width(const char *s, short font, short size);

#endif
