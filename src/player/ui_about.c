/*
 * ui_about — ver ui_about.h. Texto visible en inglés de EE. UU. (regla del proyecto).
 */
#include "ui_about.h"

#include <Memory.h>
#include <Quickdraw.h>
#include <Resources.h>

#include <string.h>

#include "ui_draw.h"

#define LOGO_SIZE  96
#define WIN_W      360
#define WIN_H      136

static WindowPtr gWin;

static const char *const kLines[] = {
    "An audio player for classic Mac OS.",
    "Decoder: libmad (GPL). Personal use.",
    "",
    "Click or press Return to close.",
};

OSErr ui_about_show(void)
{
    Rect r;
    Str255 title;

    if (gWin != NULL) {
        SelectWindow(gWin);
        return noErr;
    }
    SetRect(&r, 80, 90, 80 + WIN_W, 90 + WIN_H);
    title[0] = 12;
    memcpy(title + 1, "About MP368K", 12);
    gWin = NewCWindow(NULL, &r, title, true, noGrowDocProc, (WindowPtr)-1, true, 0);
    if (gWin == NULL)
        return memFullErr;
    return noErr;
}

void ui_about_close(void)
{
    if (gWin != NULL) {
        DisposeWindow(gWin);
        gWin = NULL;
    }
}

WindowPtr ui_about_window(void)
{
    return gWin;
}

/* Dibuja el logo con la tabla de colores del sistema; el blanco puro (índice 0) es transparente. */
static void draw_logo(short left, short top)
{
    Handle h = GetResource('LOGO', 128);
    PixMap pm;
    Rect src, dst;
    CTabHandle clut;
    RGBColor white, bg;

    if (h == NULL || GetHandleSize(h) < (Size)LOGO_SIZE * LOGO_SIZE)
        return;
    clut = GetCTable(8);
    if (clut == NULL)
        return;
    HLock(h);
    memset(&pm, 0, sizeof pm);
    pm.baseAddr = *h;
    pm.rowBytes = (short)(LOGO_SIZE | 0x8000);
    SetRect(&pm.bounds, 0, 0, LOGO_SIZE, LOGO_SIZE);
    pm.pmVersion = 0;
    pm.packType = 0;
    pm.hRes = pm.vRes = 0x00480000;
    pm.pixelType = 0;
    pm.pixelSize = 8;
    pm.cmpCount = 1;
    pm.cmpSize = 8;
    pm.pmTable = clut;

    SetRect(&src, 0, 0, LOGO_SIZE, LOGO_SIZE);
    SetRect(&dst, left, top, left + LOGO_SIZE, top + LOGO_SIZE);
    white = kPlatWhite;                      /* RGBBackColor no recibe const */
    bg = kPlatBg;
    RGBBackColor(&white);
    CopyBits((BitMap *)&pm, (BitMap *)*((CGrafPtr)gWin)->portPixMap, &src, &dst, transparent, NULL);
    RGBBackColor(&bg);
    HUnlock(h);
    DisposeCTable(clut);
}

void ui_about_draw(void)
{
    Rect all, clip;
    short y;
    unsigned i;

    if (gWin == NULL)
        return;
    SetPort(gWin);
    SetRect(&all, 0, 0, WIN_W, WIN_H);
    ui_fill(&all, &kPlatBg);
    draw_logo(16, 20);

    SetRect(&clip, 124, 12, WIN_W - 8, WIN_H - 8);
    ui_text(&clip, 124, 32, "MP368K", &kPlatText, &kPlatBg, UI_FONT_SYSTEM, 14);
    ui_text(&clip, 124, 50, "Version 0.2", &kPlatDim, &kPlatBg, UI_FONT_SMALL, 9);
    y = 72;
    for (i = 0; i < sizeof kLines / sizeof kLines[0]; i++) {
        if (kLines[i][0] != '\0')
            ui_text(&clip, 124, y, kLines[i], &kPlatText, &kPlatBg, UI_FONT_SMALL, 9);
        y += 14;
    }
}
