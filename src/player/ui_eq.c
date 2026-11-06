/*
 * ui_eq — ver ui_eq.h. Todo texto visible va en inglés de EE. UU. (regla del proyecto).
 */
#include "ui_eq.h"

#include <Events.h>
#include <Quickdraw.h>

#include <stdio.h>
#include <string.h>

#include "eq.h"
#include "player.h"
#include "ui_draw.h"

#define WIN_W 330
#define WIN_H 206
#define NCOL  (EQ_BANDS + 1)            /* columna 0 = preamp, 1..10 = bandas */
#define COL_TOP    42
#define COL_BOTTOM 162
#define FILL_TARGET_MS 600

static const Rect rOnBox   = {  10,  10,  22,  22 };
static const Rect rOnLabel = {   6,   8,  28,  58 };
static const Rect rPreset[4] = {
    {  6,  64,  28, 110 }, {  6, 114,  28, 160 }, {  6, 164,  28, 210 }, {  6, 214,  28, 260 }
};
static const char *const kPresetName[4] = { "Flat", "Bass", "Treble", "Vocal" };
static const Rect rInfo    = {  30,  60, 40, 324 };     /* valor de la banda que se arrastra */
static const Rect rMsg     = { 182,   6, 202, 324 };

/*
 * Presets propios (no son los de Winamp), en pasos de 0,5 dB: bandas 70 180 320 600 1K 3K 6K 12K 14K 16K y preamp.
 */
static const int8_t kPresets[4][NCOL] = {
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0 },                  /* Flat */
    { 12, 10,  8,  4,  0,  0,  0,  0,  0,  0, 0 },                  /* Bass */
    {  0,  0,  0,  0,  0,  2,  4,  8, 10, 12, 0 },                  /* Treble */
    { -4, -2,  0,  4,  8,  8,  4,  0, -2, -4, 0 },                  /* Vocal */
};

static WindowPtr gWin;
static Boolean gVisible;
static Boolean gAvail = true;
static char gFormat[8];
static Boolean gHasFile;
static char gInfo[40];                                              /* texto actual de rInfo */

static const char *const kLabel[NCOL] = { "Preamp", "70", "180", "320", "600", "1K", "3K", "6K", "12K", "14K", "16K" };
static const char *const kFullName[NCOL] = {
    "Preamp", "70 Hz", "180 Hz", "320 Hz", "600 Hz", "1 kHz", "3 kHz", "6 kHz", "12 kHz", "14 kHz", "16 kHz"
};

static void column_rect(int col, Rect *r)
{
    short cx = col == 0 ? 26 : (short)(80 + 24 * (col - 1));

    SetRect(r, (short)(cx - 9), COL_TOP, (short)(cx + 9), COL_BOTTOM);
}

/* Valor (pasos de 0,5 dB) de la columna `c` en los ajustes. */
static int step_of(const EqSettings *s, int c)
{
    return c == 0 ? s->preamp : s->band[c - 1];
}

static void set_step(EqSettings *s, int c, int v)
{
    if (c == 0)
        s->preamp = (int8_t)v;
    else
        s->band[c - 1] = (int8_t)v;
}

static unsigned pos_of_step(int v)
{
    return (unsigned)((EQ_STEP_MAX - v) * 1000 / (2 * EQ_STEP_MAX));
}

/* Paso (-24..+24) que corresponde a una posición 0..1000, con detente en 0 dB (±0,5 dB). */
static int step_of_pos(unsigned pos)
{
    int v = EQ_STEP_MAX - (int)((pos * (2 * EQ_STEP_MAX) + 500) / 1000);

    return (v >= -1 && v <= 1) ? 0 : v;
}

static void format_db(char *out, size_t n, int v)
{
    int a = v < 0 ? -v : v;

    snprintf(out, n, "%s%d.%d dB", v < 0 ? "-" : "+", a / 2, (a % 2) * 5);
}

/* ------------------------------------------------------------------------ */

static void draw_column(int c, Boolean enabled)
{
    Rect r, lab;
    short cx;
    short w;

    column_rect(c, &r);
    ui_vslider(&r, pos_of_step(step_of(player_get_eq(), c)), enabled);
    cx = (short)((r.left + r.right) / 2);
    w = ui_text_width(kLabel[c], UI_FONT_SMALL, 9);
    SetRect(&lab, (short)(cx - 18), (short)(COL_BOTTOM + 4), (short)(cx + 18), (short)(COL_BOTTOM + 18));
    ui_text(&lab, (short)(cx - w / 2), (short)(COL_BOTTOM + 14), kLabel[c], enabled ? &kPlatText : &kPlatDim,
            &kPlatBg, UI_FONT_SMALL, 9);
}

static void draw_on(void)
{
    ui_checkbox(&rOnBox, &rOnLabel, "On", player_get_eq()->enabled != 0, gAvail);
}

static void draw_info(const char *text)
{
    if (strcmp(text, gInfo) == 0)
        return;
    ui_fill(&rInfo, &kPlatBg);
    ui_text(&rInfo, rInfo.left, (short)(rInfo.bottom - 2), text, &kPlatText, &kPlatBg, UI_FONT_SMALL, 9);
    snprintf(gInfo, sizeof gInfo, "%s", text);
}

/* Escala de dB a la derecha de las bandas: +12, 0 y -12. */
static void draw_scale(void)
{
    static const char *const kTick[3] = { "+12", "0", "-12" };
    Rect clip;
    short t;

    SetRect(&clip, (short)(COL_TOP - 2), 302, (short)(COL_BOTTOM + 2), WIN_W);
    ui_fill(&clip, &kPlatBg);
    for (t = 0; t < 3; t++) {
        short y = (short)(COL_TOP + 6 + (long)(COL_BOTTOM - COL_TOP - 13) * t / 2);

        ui_text(&clip, 306, (short)(y + 3), kTick[t], &kPlatDim, &kPlatBg, UI_FONT_SMALL, 9);
    }
}

static void draw_message(void)
{
    char msg[64];

    msg[0] = '\0';
    if (!gAvail)
        snprintf(msg, sizeof msg, "The equalizer is not available for %s files.", gFormat);
    ui_fill(&rMsg, &kPlatBg);
    if (msg[0] != '\0')
        ui_text(&rMsg, rMsg.left, (short)(rMsg.bottom - 6), msg, &kPlatDim, &kPlatBg, UI_FONT_SMALL, 9);
}

static void draw_presets(void)
{
    int i;

    for (i = 0; i < 4; i++)
        ui_text_button(&rPreset[i], kPresetName[i], false, false, gAvail);
}

static void refresh_availability(void)
{
    const PlayerStatus *st = player_status();

    gHasFile = st->has_file;
    gAvail = !st->has_file || st->eq_available;
    snprintf(gFormat, sizeof gFormat, "%s", st->has_file ? st->format : "");
}

OSErr ui_eq_create(void)
{
    Rect r;
    Str255 title;

    SetRect(&r, 290, 90, 290 + WIN_W, 90 + WIN_H);
    title[0] = 9;
    memcpy(title + 1, "Equalizer", 9);
    gWin = NewCWindow(NULL, &r, title, false, noGrowDocProc, (WindowPtr)-1, true, 0);
    if (gWin == NULL)
        return memFullErr;
    gVisible = false;
    gInfo[0] = '\0';
    return noErr;
}

void ui_eq_destroy(void)
{
    if (gWin != NULL) {
        DisposeWindow(gWin);
        gWin = NULL;
    }
}

WindowPtr ui_eq_window(void)
{
    return gWin;
}

void ui_eq_show(Boolean show)
{
    if (gWin == NULL)
        return;
    gVisible = show;
    ShowHide(gWin, show);
    if (show)
        SelectWindow(gWin);
    player_set_fill_target_ms(show ? FILL_TARGET_MS : 0);
}

Boolean ui_eq_visible(void)
{
    return gVisible;
}

void ui_eq_draw_all(void)
{
    GrafPtr saved;
    Rect all, sep;
    int c;

    if (gWin == NULL)
        return;
    GetPort(&saved);
    SetPort(gWin);
    refresh_availability();
    SetRect(&all, 0, 0, WIN_W, WIN_H);
    ui_fill(&all, &kPlatBg);
    draw_on();
    draw_presets();
    gInfo[0] = '\0';
    draw_info("");
    for (c = 0; c < NCOL; c++)
        draw_column(c, gAvail);
    SetRect(&sep, 51, COL_TOP, 52, COL_BOTTOM + 16);               /* separa el preamp de las bandas */
    ui_fill(&sep, &kPlatDim);
    draw_scale();
    draw_message();
    SetPort(saved);
}

void ui_eq_tick(void)
{
    GrafPtr saved;
    Boolean was_avail = gAvail;
    char was_format[8];

    if (gWin == NULL || !gVisible)
        return;
    memcpy(was_format, gFormat, sizeof was_format);
    refresh_availability();
    if (was_avail != gAvail || strcmp(was_format, gFormat) != 0) {
        GetPort(&saved);
        SetPort(gWin);
        ui_eq_draw_all();
        SetPort(saved);
    }
}

/* ------------------------------------------------------------------------ */
/* Ratón                                                                     */
/* ------------------------------------------------------------------------ */

static void apply(const EqSettings *s)
{
    player_set_eq(s);
}

static void track_column(int c)
{
    EqSettings s = *player_get_eq();
    Rect r;
    Point pt;
    int last = step_of(&s, c) + 1000;           /* fuerza el primer redibujado */
    char txt[40];

    column_rect(c, &r);
    while (StillDown()) {
        int v;

        GetMouse(&pt);
        v = step_of_pos(ui_vslider_pos(&r, pt.v));
        if (v != last) {
            char db[16];

            last = v;
            set_step(&s, c, v);
            apply(&s);
            draw_column(c, true);
            format_db(db, sizeof db, v);
            snprintf(txt, sizeof txt, "%s  %s", kFullName[c], db);
            draw_info(txt);
        }
        player_idle();                          /* el audio no espera al ratón */
    }
    FlushEvents(mUpMask, 0);
}

static void track_preset(int i)
{
    Boolean inside = true;
    Point pt;

    ui_text_button(&rPreset[i], kPresetName[i], true, false, true);
    while (StillDown()) {
        Boolean in;

        GetMouse(&pt);
        in = PtInRect(pt, (Rect *)&rPreset[i]);
        if (in != inside) {
            inside = in;
            ui_text_button(&rPreset[i], kPresetName[i], inside, false, true);
        }
        player_idle();
    }
    FlushEvents(mUpMask, 0);
    ui_text_button(&rPreset[i], kPresetName[i], false, false, true);
    if (inside) {
        EqSettings s = *player_get_eq();
        int c;

        for (c = 0; c < NCOL; c++)
            set_step(&s, c, kPresets[i][c]);
        apply(&s);
        for (c = 0; c < NCOL; c++)
            draw_column(c, true);
        gInfo[0] = '\0';
        draw_info(kPresetName[i]);
    }
}

void ui_eq_click(Point where)
{
    int c;

    if (!gAvail)
        return;                                 /* atenuado: sin efecto con este formato */
    if (PtInRect(where, (Rect *)&rOnLabel) || PtInRect(where, (Rect *)&rOnBox)) {
        EqSettings s = *player_get_eq();

        s.enabled = !s.enabled;
        apply(&s);
        draw_on();
        return;
    }
    for (c = 0; c < 4; c++)
        if (PtInRect(where, (Rect *)&rPreset[c])) {
            track_preset(c);
            return;
        }
    for (c = 0; c < NCOL; c++) {
        Rect r;

        column_rect(c, &r);
        if (PtInRect(where, &r)) {
            track_column(c);
            return;
        }
    }
}
