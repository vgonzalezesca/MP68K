/*
 * ui_draw — ver ui_draw.h.
 */
#include "ui_draw.h"

#include <Fonts.h>
#include <Memory.h>

#include <string.h>

#define G(v) { (v) * 0x0101, (v) * 0x0101, (v) * 0x0101 }

const RGBColor kPlatBg     = G(0xDD);
const RGBColor kPlatWhite  = G(0xFF);
const RGBColor kPlatBlack  = G(0x00);
const RGBColor kPlatText   = G(0x00);
const RGBColor kPlatDim    = G(0x66);
const RGBColor kPlatAccent = { 0x3C3C, 0x5A5A, 0xA6A6 };
const RGBColor kLcdBg      = { 0xC8C8, 0xD2D2, 0xBCBC };    /* gris verdoso, como el Apple CD Player */
const RGBColor kLcdOn      = { 0x1C1C, 0x2424, 0x1C1C };
const RGBColor kLcdOff     = { 0xB8B8, 0xC3C3, 0xACAC };

static const RGBColor kFaceTop    = G(0xEE);
static const RGBColor kFaceMid    = G(0xDD);
static const RGBColor kFaceBottom = G(0xCC);
static const RGBColor kPressedFace = G(0xAA);
static const RGBColor kHi1        = G(0xFF);    /* bisel claro exterior */
static const RGBColor kHi2        = G(0xF2);
static const RGBColor kSh1        = G(0x7F);    /* bisel oscuro exterior */
static const RGBColor kSh2        = G(0xBB);
static const RGBColor kDeep       = G(0x55);

static RgnHandle gSavedClip;

void ui_draw_init(void)
{
    gSavedClip = NewRgn();
}

void ui_draw_shutdown(void)
{
    if (gSavedClip != NULL) {
        DisposeRgn(gSavedClip);
        gSavedClip = NULL;
    }
}

/*
 * Las Multiversal Interfaces declaran estas rutinas con punteros no constantes aunque
 * no modifican su argumento; los casts de aquí son por esa razón.
 */
static void fore(const RGBColor *c)
{
    RGBForeColor((RGBColor *)c);
}

static void back(const RGBColor *c)
{
    RGBBackColor((RGBColor *)c);
}

void ui_fill(const Rect *r, const RGBColor *c)
{
    fore(c);
    PaintRect((Rect *)r);
}

/* Línea de 1 px como rectángulo (el extremo derecho/inferior no se incluye). */
static void hline(short l, short r, short y, const RGBColor *c)
{
    Rect q;

    if (r <= l)
        return;
    SetRect(&q, l, y, r, (short)(y + 1));
    ui_fill(&q, c);
}

static void vline(short x, short t, short b, const RGBColor *c)
{
    Rect q;

    if (b <= t)
        return;
    SetRect(&q, x, t, (short)(x + 1), b);
    ui_fill(&q, c);
}

/* ------------------------------------------------------------------------ */

void ui_well(const Rect *r, const RGBColor *fill)
{
    Rect in = *r;

    InsetRect(&in, 2, 2);
    ui_fill(&in, fill);
    /* borde exterior: oscuro arriba/izquierda, blanco abajo/derecha */
    hline(r->left, r->right, r->top, &kSh1);
    vline(r->left, r->top, r->bottom, &kSh1);
    hline(r->left, r->right, (short)(r->bottom - 1), &kPlatWhite);
    vline((short)(r->right - 1), r->top, r->bottom, &kPlatWhite);
    /* borde interior */
    hline((short)(r->left + 1), (short)(r->right - 1), (short)(r->top + 1), &kDeep);
    vline((short)(r->left + 1), (short)(r->top + 1), (short)(r->bottom - 1), &kDeep);
    hline((short)(r->left + 1), (short)(r->right - 1), (short)(r->bottom - 2), &kSh2);
    vline((short)(r->right - 2), (short)(r->top + 1), (short)(r->bottom - 1), &kSh2);
}

void ui_push_frame(const Rect *r, Boolean pressed)
{
    short l = r->left, t = r->top, rr = r->right, b = r->bottom;
    short h = (short)(b - t - 2), band = (short)(h / 3);
    Rect q;

    /* cara: tres bandas suaves, o plana y más oscura si está pulsado */
    if (pressed) {
        SetRect(&q, (short)(l + 1), (short)(t + 1), (short)(rr - 1), (short)(b - 1));
        ui_fill(&q, &kPressedFace);
    } else {
        SetRect(&q, (short)(l + 1), (short)(t + 1), (short)(rr - 1), (short)(t + 1 + band));
        ui_fill(&q, &kFaceTop);
        SetRect(&q, (short)(l + 1), (short)(t + 1 + band), (short)(rr - 1), (short)(t + 1 + 2 * band));
        ui_fill(&q, &kFaceMid);
        SetRect(&q, (short)(l + 1), (short)(t + 1 + 2 * band), (short)(rr - 1), (short)(b - 1));
        ui_fill(&q, &kFaceBottom);
    }
    /* contorno negro con las cuatro esquinas recortadas */
    hline((short)(l + 1), (short)(rr - 1), t, &kPlatBlack);
    hline((short)(l + 1), (short)(rr - 1), (short)(b - 1), &kPlatBlack);
    vline(l, (short)(t + 1), (short)(b - 1), &kPlatBlack);
    vline((short)(rr - 1), (short)(t + 1), (short)(b - 1), &kPlatBlack);
    /* bisel interior */
    if (pressed) {
        hline((short)(l + 1), (short)(rr - 1), (short)(t + 1), &kDeep);
        vline((short)(l + 1), (short)(t + 1), (short)(b - 1), &kDeep);
        hline((short)(l + 2), (short)(rr - 1), (short)(b - 2), &kHi2);
        vline((short)(rr - 2), (short)(t + 2), (short)(b - 1), &kHi2);
    } else {
        hline((short)(l + 1), (short)(rr - 2), (short)(t + 1), &kHi1);
        vline((short)(l + 1), (short)(t + 2), (short)(b - 2), &kHi1);
        hline((short)(l + 2), (short)(rr - 1), (short)(b - 2), &kSh1);
        vline((short)(rr - 2), (short)(t + 2), (short)(b - 1), &kSh1);
        hline((short)(l + 2), (short)(rr - 3), (short)(t + 2), &kHi2);
        vline((short)(l + 2), (short)(t + 3), (short)(b - 3), &kHi2);
        hline((short)(l + 3), (short)(rr - 2), (short)(b - 3), &kSh2);
        vline((short)(rr - 3), (short)(t + 3), (short)(b - 2), &kSh2);
    }
}

/* ------------------------------------------------------------------------ */
/* Pantalla LCD de 7 segmentos                                                */
/* ------------------------------------------------------------------------ */

/* bits: a=1 b=2 c=4 d=8 e=16 f=32 g=64 */
static const unsigned char kSeg[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

static void seg_rect(short l, short t, short r, short b, Boolean on)
{
    Rect q;

    SetRect(&q, l, t, r, b);
    ui_fill(&q, on ? &kLcdOn : &kLcdOff);
}

#define DW 16       /* ancho de un dígito */
#define DH 30       /* alto */
#define DT 4        /* grosor de segmento */

static void lcd_digit(short x, short y, int digit)
{
    const short w = DW, h = DH, t = DT, m = h / 2;
    unsigned char s = kSeg[digit];

    seg_rect(x + t, y,         x + w - t, y + t,     s & 1);       /* a */
    seg_rect(x + w - t, y + t, x + w,     y + m,     s & 2);       /* b */
    seg_rect(x + w - t, y + m, x + w,     y + h - t, s & 4);       /* c */
    seg_rect(x + t, y + h - t, x + w - t, y + h,     s & 8);       /* d */
    seg_rect(x, y + m,         x + t,     y + h - t, s & 16);      /* e */
    seg_rect(x, y + t,         x + t,     y + m,     s & 32);      /* f */
    seg_rect(x + t, y + m - 2, x + w - t, y + m + 2, s & 64);      /* g */
}

void ui_lcd_time(const Rect *box, unsigned long ms, Boolean valid)
{
    unsigned long total = ms / 1000, min = total / 60, sec = total % 60;
    short x0 = (short)(box->left + 10), y = (short)(box->top + 8);
    Rect dot, inner = *box;

    InsetRect(&inner, 2, 2);
    ui_fill(&inner, &kLcdBg);
    if (!valid) {
        min = 0;
        sec = 0;
    }
    if (min > 99) {
        min = 99;
        sec = 59;
    }
    if (valid) {
        lcd_digit(x0,      y, (int)(min / 10));
        lcd_digit(x0 + 20, y, (int)(min % 10));
        lcd_digit(x0 + 54, y, (int)(sec / 10));
        lcd_digit(x0 + 74, y, (int)(sec % 10));
    } else {
        short xs[4], i;

        xs[0] = x0; xs[1] = (short)(x0 + 20); xs[2] = (short)(x0 + 54); xs[3] = (short)(x0 + 74);
        for (i = 0; i < 4; i++)
            seg_rect((short)(xs[i] + DT), (short)(y + DH / 2 - 2), (short)(xs[i] + DW - DT), (short)(y + DH / 2 + 2), true);
    }
    SetRect(&dot, (short)(x0 + 41), (short)(y + 8), (short)(x0 + 45), (short)(y + 12));
    ui_fill(&dot, &kLcdOn);
    OffsetRect(&dot, 0, 10);
    ui_fill(&dot, &kLcdOn);
}

/* ------------------------------------------------------------------------ */
/* Botones                                                                    */
/* ------------------------------------------------------------------------ */

static void bar(short l, short t, short r, short b)
{
    Rect q;

    SetRect(&q, l, t, r, b);
    PaintRect(&q);
}

static void tri_right(short x, short cy, short w, short hh)
{
    short k;

    for (k = 0; k < w; k++) {
        short half = (short)((long)(w - k) * hh / w);
        bar((short)(x + k), (short)(cy - half), (short)(x + k + 1), (short)(cy + half + 1));
    }
}

static void tri_left(short x, short cy, short w, short hh)
{
    short k;

    for (k = 0; k < w; k++) {
        short half = (short)((long)(w - k) * hh / w);
        bar((short)(x + w - 1 - k), (short)(cy - half), (short)(x + w - k), (short)(cy + half + 1));
    }
}

void ui_glyph_button(const Rect *r, UiGlyph g, Boolean pressed, Boolean lit)
{
    short cx = (short)((r->left + r->right) / 2 + (pressed ? 1 : 0));
    short cy = (short)((r->top + r->bottom) / 2 + (pressed ? 1 : 0));

    ui_push_frame(r, pressed);
    fore(lit ? &kPlatAccent : &kPlatBlack);
    switch (g) {
    case GLYPH_PLAY:  tri_right((short)(cx - 4), cy, 9, 5); break;
    case GLYPH_PAUSE: bar((short)(cx - 4), (short)(cy - 5), (short)(cx - 1), (short)(cy + 6));
                      bar((short)(cx + 2), (short)(cy - 5), (short)(cx + 5), (short)(cy + 6)); break;
    case GLYPH_STOP:  bar((short)(cx - 4), (short)(cy - 4), (short)(cx + 5), (short)(cy + 5)); break;
    case GLYPH_PREV:  bar((short)(cx - 5), (short)(cy - 5), (short)(cx - 3), (short)(cy + 6));
                      tri_left((short)(cx - 3), cy, 8, 5); break;
    case GLYPH_NEXT:  tri_right((short)(cx - 5), cy, 8, 5);
                      bar((short)(cx + 3), (short)(cy - 5), (short)(cx + 5), (short)(cy + 6)); break;
    case GLYPH_EJECT: {
        short k;

        for (k = 0; k < 7; k++)                     /* triángulo hacia arriba */
            bar((short)(cx - k), (short)(cy - 5 + k), (short)(cx + k + 1), (short)(cy - 4 + k));
        bar((short)(cx - 6), (short)(cy + 3), (short)(cx + 7), (short)(cy + 6));
        break;
    }
    }
}

void ui_text_button(const Rect *r, const char *label, Boolean pressed, Boolean on, Boolean enabled)
{
    Boolean down = pressed || on;
    short w = ui_text_width(label, UI_FONT_SYSTEM, 12);
    short x = (short)(r->left + ((r->right - r->left) - w) / 2 + (down ? 1 : 0));

    ui_push_frame(r, down);
    ui_text(r, x, (short)(r->bottom - 7 + (down ? 1 : 0)), label, enabled ? &kPlatBlack : &kPlatDim,
            down ? &kPressedFace : &kFaceMid, UI_FONT_SYSTEM, 12);
}

/* ------------------------------------------------------------------------ */
/* Deslizador                                                                 */
/* ------------------------------------------------------------------------ */

#define THUMB_W 12

void ui_slider(const Rect *track, unsigned pos_x1000, Boolean enabled)
{
    Rect groove, thumb;
    short span = (short)(track->right - track->left - THUMB_W);
    short cy = (short)((track->top + track->bottom) / 2);
    short x, gx;

    /* se repinta el fondo de la franja: la perilla se mueve */
    ui_fill(track, &kPlatBg);
    SetRect(&groove, track->left, (short)(cy - 3), track->right, (short)(cy + 3));
    ui_well(&groove, enabled ? &kPlatWhite : &kFaceTop);
    if (pos_x1000 > 1000)
        pos_x1000 = 1000;
    x = (short)(track->left + (long)span * pos_x1000 / 1000);
    SetRect(&thumb, x, track->top, (short)(x + THUMB_W), track->bottom);
    ui_push_frame(&thumb, false);
    /* tres rayas de agarre */
    for (gx = (short)(x + 4); gx <= (short)(x + 8); gx += 2) {
        vline(gx, (short)(thumb.top + 4), (short)(thumb.bottom - 4), &kSh1);
        vline((short)(gx + 1), (short)(thumb.top + 4), (short)(thumb.bottom - 4), &kPlatWhite);
    }
}

unsigned ui_slider_pos(const Rect *track, short x)
{
    long span = track->right - track->left - THUMB_W;
    long p = x - track->left - THUMB_W / 2;

    if (span <= 0 || p <= 0)
        return 0;
    if (p >= span)
        return 1000;
    return (unsigned)(p * 1000 / span);
}

#define THUMB_H 12

void ui_vslider(const Rect *col, unsigned pos_x1000, Boolean enabled)
{
    Rect groove, thumb;
    short cx = (short)((col->left + col->right) / 2);
    short span = (short)(col->bottom - col->top - THUMB_H);
    short y, gy, tick;

    ui_fill(col, &kPlatBg);
    SetRect(&groove, (short)(cx - 3), (short)(col->top + THUMB_H / 2), (short)(cx + 3), (short)(col->bottom - THUMB_H / 2));
    ui_well(&groove, enabled ? &kPlatWhite : &kFaceTop);
    /* marcas en +12 dB (arriba), 0 y -12 dB (abajo) */
    for (tick = 0; tick < 3; tick++) {
        short ty = (short)(groove.top + (long)(groove.bottom - groove.top - 1) * tick / 2);

        hline(col->left, (short)(cx - 5), ty, &kSh1);
        hline((short)(cx + 5), col->right, ty, &kSh1);
    }
    if (pos_x1000 > 1000)
        pos_x1000 = 1000;
    y = (short)(col->top + (long)span * pos_x1000 / 1000);
    SetRect(&thumb, col->left, y, col->right, (short)(y + THUMB_H));
    ui_push_frame(&thumb, false);
    if (enabled) {
        for (gy = (short)(y + 4); gy <= (short)(y + 7); gy += 2) {
            hline((short)(col->left + 4), (short)(col->right - 4), gy, &kSh1);
            hline((short)(col->left + 4), (short)(col->right - 4), (short)(gy + 1), &kPlatWhite);
        }
    }
}

unsigned ui_vslider_pos(const Rect *col, short y)
{
    long span = col->bottom - col->top - THUMB_H;
    long p = y - col->top - THUMB_H / 2;

    if (span <= 0 || p <= 0)
        return 0;
    if (p >= span)
        return 1000;
    return (unsigned)(p * 1000 / span);
}

void ui_checkbox(const Rect *box, const Rect *label_clip, const char *label, Boolean on, Boolean enabled)
{
    short x;

    ui_fill(label_clip, &kPlatBg);
    ui_well(box, enabled ? &kPlatWhite : &kFaceTop);
    if (on) {
        fore(enabled ? &kPlatBlack : &kPlatDim);
        PenSize(2, 2);
        MoveTo((short)(box->left + 3), (short)(box->top + 3));
        LineTo((short)(box->right - 5), (short)(box->bottom - 5));
        MoveTo((short)(box->right - 5), (short)(box->top + 3));
        LineTo((short)(box->left + 3), (short)(box->bottom - 5));
        PenNormal();
    }
    x = (short)(box->right + 6);
    ui_text(label_clip, x, (short)(box->bottom - 1), label, enabled ? &kPlatText : &kPlatDim, &kPlatBg, UI_FONT_SYSTEM, 12);
}

void ui_play_marker(short x, short y, const RGBColor *c)
{
    fore(c);
    tri_right(x, (short)(y + 4), 6, 4);
}

/* ------------------------------------------------------------------------ */
/* Texto                                                                      */
/* ------------------------------------------------------------------------ */

static void to_pascal(Str255 dst, const char *src)
{
    size_t n = strlen(src);

    if (n > 255)
        n = 255;
    dst[0] = (unsigned char)n;
    memcpy(dst + 1, src, n);
}

short ui_text_width(const char *s, short font, short size)
{
    Str255 p;

    to_pascal(p, s);
    TextFont(font);
    TextSize(size);
    return StringWidth(p);
}

short ui_text(const Rect *clip, short x, short baseline, const char *s, const RGBColor *fg,
              const RGBColor *bg, short font, short size)
{
    Str255 p;
    short w;

    to_pascal(p, s);
    TextFont(font);
    TextSize(size);
    w = StringWidth(p);
    GetClip(gSavedClip);
    ClipRect((Rect *)clip);
    fore(fg);
    back(bg);
    TextMode(srcCopy);
    MoveTo(x, baseline);
    DrawString(p);
    TextMode(srcOr);
    back(&kPlatWhite);
    SetClip(gSavedClip);
    return w;
}
