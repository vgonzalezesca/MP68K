/*
 * ui_list — ver ui_list.h.
 */
#include "ui_list.h"

#include <Events.h>
#include <Fonts.h>
#include <LowMem.h>

#include <stdio.h>
#include <string.h>

#include "playlist.h"
#include "player.h"
#include "ui_draw.h"

#define WIN_W 330
#define WIN_H 222
#define ROW_H 16
#define VISIBLE_ROWS 10

/* Rect: top, left, bottom, right */
static const Rect rList   = {  10,  10, 180, 304 };
static const Rect rScroll = {  10, 304, 180, 320 };
static const Rect rAdd    = { 190,  10, 210,  70 };
static const Rect rFolder = { 190,  74, 210, 176 };
static const Rect rRemove = { 190, 180, 210, 244 };
static const Rect rClear  = { 190, 248, 210, 304 };

static WindowPtr gWin;
static ControlHandle gScrollCtl;
static Boolean gVisible;
static int gSel = -1;
static int gTop;                        /* primera fila visible */
static int gScrollMax;
static unsigned long gLastClick;
static int gLastRow = -1;

static int count(void)
{
    return (int)playlist_count();
}

static void clamp_top(void)
{
    int max = count() > VISIBLE_ROWS ? count() - VISIBLE_ROWS : 0;

    if (gTop > max)
        gTop = max;
    if (gTop < 0)
        gTop = 0;
}

static void format_time(unsigned long ms, char *out, size_t n)
{
    unsigned long s = ms / 1000;

    if (ms == 0)
        out[0] = '\0';
    else
        snprintf(out, n, "%lu:%02lu", s / 60, s % 60);
}

static void row_rect(int visible_index, Rect *r)
{
    SetRect(r, (short)(rList.left + 2), (short)(rList.top + 2 + visible_index * ROW_H),
            (short)(rList.right - 2), (short)(rList.top + 2 + (visible_index + 1) * ROW_H));
}

static void draw_row(int row)
{
    Rect rr, textClip;
    int vis = row - gTop;

    if (vis < 0 || vis >= VISIBLE_ROWS)
        return;
    row_rect(vis, &rr);
    ui_fill(&rr, &kPlatWhite);
    if (row < count()) {
        const PlaylistEntry *e = playlist_get((unsigned)row);
        char text[96], tm[16];
        short w;

        snprintf(text, sizeof text, "%d  %s", row + 1, e->label);
        format_time(e->duration_ms, tm, sizeof tm);
        w = tm[0] ? ui_text_width(tm, UI_FONT_SYSTEM, 12) : 0;
        textClip = rr;
        textClip.left = (short)(rr.left + 14);
        textClip.right = (short)(rr.right - (w ? w + 12 : 4));
        ui_text(&textClip, (short)(rr.left + 16), (short)(rr.top + 12), text, &kPlatText, &kPlatWhite,
                UI_FONT_SYSTEM, 12);
        if (w) {
            Rect tr = rr;

            tr.left = (short)(rr.right - w - 8);
            ui_text(&tr, (short)(rr.right - w - 6), (short)(rr.top + 12), tm, &kPlatText, &kPlatWhite,
                    UI_FONT_SYSTEM, 12);
        }
        if (row == order_current(playlist_order()))
            ui_play_marker((short)(rr.left + 3), (short)(rr.top + 4), &kPlatBlack);
    }
    if (row == gSel && row < count()) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"    /* variable global del sistema en una dirección baja */
#pragma GCC diagnostic ignored "-Warray-bounds"
        LMSetHiliteMode((Byte)(LMGetHiliteMode() & 0x7F));   /* resaltado con el color del sistema */
#pragma GCC diagnostic pop
        InvertRect(&rr);
    }
}

static void draw_rows(void)
{
    int i;

    for (i = 0; i < VISIBLE_ROWS; i++)
        draw_row(gTop + i);
}

static void update_scrollbar(void)
{
    int max = count() > VISIBLE_ROWS ? count() - VISIBLE_ROWS : 0;

    if (gScrollCtl == NULL)
        return;
    gScrollMax = max;
    SetControlMaximum(gScrollCtl, max);
    SetControlValue(gScrollCtl, gTop);
    HiliteControl(gScrollCtl, max > 0 ? 0 : 255);           /* inactiva si todo cabe */
}

static void draw_buttons(void)
{
    Boolean any = count() > 0;

    ui_text_button(&rAdd, "Add\311", false, false, true);
    ui_text_button(&rFolder, "Add Folder\311", false, false, true);
    ui_text_button(&rRemove, "Remove", false, false, any && gSel >= 0);
    ui_text_button(&rClear, "Clear", false, false, any);
}

/* ------------------------------------------------------------------------ */

OSErr ui_list_create(void)
{
    Rect r;
    Str255 title;
    GrafPtr saved;

    SetRect(&r, 30, 232, 30 + WIN_W, 232 + WIN_H);
    title[0] = 8;
    memcpy(title + 1, "Playlist", 8);
    gWin = NewCWindow(NULL, &r, title, false, noGrowDocProc, (WindowPtr)-1, true, 0);
    if (gWin == NULL)
        return memFullErr;
    GetPort(&saved);
    SetPort(gWin);
    title[0] = 0;
    gScrollCtl = NewControl(gWin, (Rect *)&rScroll, title, true, 0, 0, 0, scrollBarProc, 0);
    SetPort(saved);
    gVisible = false;
    return noErr;
}

void ui_list_destroy(void)
{
    if (gWin != NULL) {
        DisposeWindow(gWin);                                /* también libera la barra */
        gWin = NULL;
        gScrollCtl = NULL;
    }
}

WindowPtr ui_list_window(void)
{
    return gWin;
}

void ui_list_show(Boolean show)
{
    if (gWin == NULL)
        return;
    gVisible = show;
    ShowHide(gWin, show);
    if (show)
        SelectWindow(gWin);
}

Boolean ui_list_visible(void)
{
    return gVisible;
}

void ui_list_draw_all(void)
{
    GrafPtr saved;
    Rect all;

    if (gWin == NULL)
        return;
    GetPort(&saved);
    SetPort(gWin);
    SetRect(&all, 0, 0, WIN_W, WIN_H);
    ui_fill(&all, &kPlatBg);
    ui_well(&rList, &kPlatWhite);
    clamp_top();
    draw_rows();
    update_scrollbar();
    draw_buttons();
    DrawControls(gWin);
    SetPort(saved);
}

void ui_list_refresh(void)
{
    GrafPtr saved;

    if (gWin == NULL)
        return;
    if (gSel >= count())
        gSel = count() - 1;
    clamp_top();
    if (!gVisible)
        return;
    GetPort(&saved);
    SetPort(gWin);
    draw_rows();
    update_scrollbar();
    draw_buttons();
    SetPort(saved);
}

void ui_list_activate(Boolean active)
{
    if (gScrollCtl != NULL)
        HiliteControl(gScrollCtl, (active && gScrollMax > 0) ? 0 : 255);
}

int ui_list_selected(void)
{
    return gSel;
}

void ui_list_set_selected(int index)
{
    if (index >= count())
        index = count() - 1;
    gSel = index;
    ui_list_refresh();
}

void ui_list_show_row(int index)
{
    if (index < 0 || index >= count())
        return;
    if (index < gTop)
        gTop = index;
    else if (index >= gTop + VISIBLE_ROWS)
        gTop = index - VISIBLE_ROWS + 1;
    ui_list_refresh();
}

/* ------------------------------------------------------------------------ */
/* Ratón y teclado                                                           */
/* ------------------------------------------------------------------------ */

static pascal void scroll_action(ControlHandle c, short part)
{
    int v = GetControlValue(c), old = v;

    switch (part) {
    case inUpButton:   v--; break;
    case inDownButton: v++; break;
    case inPageUp:     v -= VISIBLE_ROWS - 1; break;
    case inPageDown:   v += VISIBLE_ROWS - 1; break;
    default:           break;
    }
    if (v < 0)
        v = 0;
    if (v > gScrollMax)
        v = gScrollMax;
    if (v != old) {
        SetControlValue(c, v);
        gTop = v;
        draw_rows();
    }
    player_idle();                                          /* el audio no espera al ratón */
}

static Boolean track_button(const Rect *r, const char *label, Boolean enabled)
{
    Boolean inside = true;
    Point pt;

    if (!enabled)
        return false;
    ui_text_button(r, label, true, false, true);
    while (StillDown()) {
        Boolean in;

        GetMouse(&pt);
        in = PtInRect(pt, r);
        if (in != inside) {
            inside = in;
            ui_text_button(r, label, inside, false, true);
        }
        player_idle();
    }
    FlushEvents(mUpMask, 0);
    ui_text_button(r, label, false, false, true);
    return inside;
}

void ui_list_click(Point where, unsigned long when)
{
    GrafPtr saved;
    ControlHandle c;
    short part;

    if (gWin == NULL)
        return;
    GetPort(&saved);
    SetPort(gWin);

    part = FindControl(where, gWin, &c);
    if (part != 0 && c == gScrollCtl) {
        if (part == inThumb) {
            TrackControl(c, where, NULL);
            gTop = GetControlValue(c);
            draw_rows();
        } else {
            TrackControl(c, where, (ControlActionUPP)scroll_action);
        }
    } else if (PtInRect(where, &rAdd)) {
        if (track_button(&rAdd, "Add\311", true))
            app_list_command(LIST_ADD, 0);
    } else if (PtInRect(where, &rFolder)) {
        if (track_button(&rFolder, "Add Folder\311", true))
            app_list_command(LIST_ADD_FOLDER, 0);
    } else if (PtInRect(where, &rRemove)) {
        if (track_button(&rRemove, "Remove", count() > 0 && gSel >= 0))
            app_list_command(LIST_REMOVE, gSel);
    } else if (PtInRect(where, &rClear)) {
        if (track_button(&rClear, "Clear", count() > 0))
            app_list_command(LIST_CLEAR, 0);
    } else if (PtInRect(where, &rList)) {
        int row = gTop + (where.v - rList.top - 2) / ROW_H;

        if (row >= 0 && row < count()) {
            int old = gSel;

            gSel = row;
            if (old >= 0)
                draw_row(old);
            draw_row(row);
            if (row == gLastRow && (long)(when - gLastClick) <= LMGetDoubleTime())
                app_list_command(LIST_PLAY, row);           /* doble clic */
            gLastRow = row;
            gLastClick = when;
        }
    }
    SetPort(saved);
}

void ui_list_key(char c)
{
    int sel = gSel;

    if (count() == 0)
        return;
    switch (c) {
    case 0x1E:                                              /* flecha arriba */
        sel = sel < 0 ? 0 : sel - 1;
        break;
    case 0x1F:                                              /* flecha abajo */
        sel = sel < 0 ? 0 : sel + 1;
        break;
    case 0x0D:
    case 0x03:                                              /* Return / Enter */
        if (gSel >= 0)
            app_list_command(LIST_PLAY, gSel);
        return;
    case 0x08:
    case 0x7F:                                              /* Delete */
        if (gSel >= 0)
            app_list_command(LIST_REMOVE, gSel);
        return;
    default:
        return;
    }
    if (sel < 0)
        sel = 0;
    if (sel >= count())
        sel = count() - 1;
    gSel = sel;
    ui_list_show_row(sel);
    ui_list_refresh();
}
