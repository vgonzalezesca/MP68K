/*
 * ui_main — ver ui_main.h.
 */
#include "ui_main.h"

#include <Events.h>
#include <Fonts.h>

#include <stdio.h>
#include <string.h>

#include "player.h"
#include "ui_draw.h"

#define WIN_W 330
#define WIN_H 144

/* Rect: top, left, bottom, right */
static const Rect rLcd      = {  10,  10,  54, 120 };
static const Rect rInfo     = {  10, 126,  54, 320 };
static const Rect rTitle    = {  13, 130,  29, 316 };
static const Rect rArtist   = {  29, 130,  41, 316 };
static const Rect rFormat   = {  41, 130,  52, 316 };
static const Rect rSeek     = {  62,  10,  78, 320 };
static const Rect rVolLabel = {  90, 222, 106, 248 };
static const Rect rVol      = {  88, 250, 104, 320 };
static const Rect rBalLabel = { 116, 222, 132, 248 };
static const Rect rBal      = { 116, 250, 132, 320 };

typedef struct {
    Rect r;
    UiGlyph glyph;
    UiCommand cmd;
} Btn;

static const Btn kBtn[] = {
    { {  86,  10, 108,  40 }, GLYPH_PREV,  UI_CMD_PREV },
    { {  86,  43, 108,  73 }, GLYPH_PLAY,  UI_CMD_PLAY },
    { {  86,  76, 108, 106 }, GLYPH_PAUSE, UI_CMD_PAUSE },
    { {  86, 109, 108, 139 }, GLYPH_STOP,  UI_CMD_STOP },
    { {  86, 142, 108, 172 }, GLYPH_NEXT,  UI_CMD_NEXT },
    { {  86, 180, 108, 210 }, GLYPH_EJECT, UI_CMD_OPEN },
};
#define NBTN (sizeof kBtn / sizeof kBtn[0])
static const Rect rShuffle  = { 114,  10, 134,  70 };
static const Rect rRepeat   = { 114,  74, 134, 152 };
static const Rect rPlaylist = { 114, 156, 134, 218 };

static WindowPtr gWin;
static Boolean gShuffle, gListVisible, gDebug;
static RepeatMode gRepeat;
static unsigned gVolume = 100;          /* 0..100 */
static int gBalance = 0;                /* -100..100 */

/* Lo último dibujado, para redibujar solo lo que cambia. */
static struct {
    Boolean valid;
    PlayerState state;
    Boolean has_file;
    unsigned long secs;
    unsigned seek_pos;
    char artist[96];
    char format[96];
    char title[96];
    short tick_x;
    unsigned long tick_time;
    Boolean ticking;
} C;

/* ------------------------------------------------------------------------ */

static void dyn_invalidate(void)
{
    C.valid = false;
    C.title[0] = '\0';
    C.artist[0] = '\0';
    C.format[0] = '\0';
    C.tick_x = 0;
}

static unsigned seek_pos_now(const PlayerStatus *st)
{
    if (!st->has_file || st->duration_ms == 0 || st->state == PLAYER_STOPPED)
        return 0;
    return (unsigned)((unsigned long long)st->position_ms * 1000ULL / st->duration_ms);
}

static const char *repeat_label(void)
{
    return gRepeat == REPEAT_ONE ? "Repeat 1" : (gRepeat == REPEAT_ALL ? "Repeat All" : "Repeat");
}

static void draw_toggles(void)
{
    ui_text_button(&rShuffle, "Shuffle", false, gShuffle, true);
    ui_text_button(&rRepeat, repeat_label(), false, gRepeat != REPEAT_OFF, true);
    ui_text_button(&rPlaylist, "Playlist", false, gListVisible, true);
}

static void draw_transport(const PlayerStatus *st)
{
    unsigned i;

    for (i = 0; i < NBTN; i++) {
        Boolean lit = (kBtn[i].cmd == UI_CMD_PLAY && st->state == PLAYER_PLAYING) ||
                      (kBtn[i].cmd == UI_CMD_PAUSE && st->state == PLAYER_PAUSED);
        ui_glyph_button(&kBtn[i].r, kBtn[i].glyph, false, lit);
    }
}

static void draw_volume_balance(void)
{
    ui_text(&rVolLabel, rVolLabel.left, (short)(rVolLabel.bottom - 5), "Vol", &kPlatText, &kPlatBg,
            UI_FONT_SMALL, 9);
    ui_slider(&rVol, gVolume * 10, true);
    ui_text(&rBalLabel, rBalLabel.left, (short)(rBalLabel.bottom - 5), "Bal", &kPlatText, &kPlatBg,
            UI_FONT_SMALL, 9);
    ui_slider(&rBal, (unsigned)((gBalance + 100) * 5), true);
}

static void build_format(const PlayerStatus *st, char *out, size_t n)
{
    if (!st->has_file) {
        out[0] = '\0';
        return;
    }
    if (gDebug && st->state != PLAYER_STOPPED) {
        snprintf(out, n, "buf %ld  und %lu  gap %lu  rtf %lu.%02lu  mem %luK", st->buffered_ms, st->underruns,
                 st->max_gap_ms, st->preroll_rtf_x100 / 100, st->preroll_rtf_x100 % 100,
                 st->arena_peak / 1024);
        return;
    }
    {
        const char *ch = st->out_channels ? (st->out_channels == 2 ? "Stereo" : "Mono")
                                          : (st->in_channels == 2 ? "Stereo" : "Mono");
        unsigned rate = st->out_rate ? st->out_rate : st->in_rate;

        if (st->has_profiles) {
            char kbps[24];

            if (st->vbr)
                snprintf(kbps, sizeof kbps, "VBR %u kbps", st->bitrate / 1000);
            else
                snprintf(kbps, sizeof kbps, "%u kbps", st->bitrate / 1000);
            snprintf(out, n, "%s  %u.%u kHz  %s  %s%s", kbps, rate / 1000, (rate % 1000) / 100, ch,
                     dec_profile_name(st->profile), st->auto_degraded ? "*" : "");
        } else if (st->bits_src != 0) {
            snprintf(out, n, "%s  %u-bit  %u.%u kHz  %s", st->format, st->bits_src, rate / 1000,
                     (rate % 1000) / 100, ch);
        } else if (st->bitrate != 0) {
            snprintf(out, n, "%s  %u kbps  %u.%u kHz  %s", st->format, st->bitrate / 1000, rate / 1000,
                     (rate % 1000) / 100, ch);
        } else {
            snprintf(out, n, "%s  %u.%u kHz  %s", st->format, rate / 1000, (rate % 1000) / 100, ch);
        }
    }
}

static void draw_info_lines(const PlayerStatus *st)
{
    char fmt[96];
    const char *artist = st->has_file ? st->artist : "Choose File > Open to begin";

    if (st->has_file && st->may_stutter)
        artist = "May not play in real time";

    build_format(st, fmt, sizeof fmt);
    if (!C.valid || strcmp(artist, C.artist) != 0) {
        ui_fill(&rArtist, &kLcdBg);
        ui_text(&rArtist, rArtist.left, (short)(rArtist.bottom - 2), artist, &kLcdOn, &kLcdBg, UI_FONT_SMALL, 9);
        strcpy(C.artist, artist);
    }
    if (!C.valid || strcmp(fmt, C.format) != 0) {
        ui_fill(&rFormat, &kLcdBg);
        ui_text(&rFormat, rFormat.left, (short)(rFormat.bottom - 2), fmt, &kLcdOn, &kLcdBg, UI_FONT_SMALL, 9);
        strcpy(C.format, fmt);
    }
}

/* Título: fijo si cabe; si no, se desplaza 2 px cada ~100 ms con dos copias seguidas. */
static void draw_title(const PlayerStatus *st, Boolean force)
{
    char t[96], padded[110];
    short w, clipw = (short)(rTitle.right - rTitle.left);
    unsigned long now = TickCount();

    snprintf(t, sizeof t, "%s", st->has_file ? st->title : "MP368K");
    if (force || strcmp(t, C.title) != 0) {
        strcpy(C.title, t);
        C.tick_x = 0;
        C.tick_time = now;
        ui_fill(&rTitle, &kLcdBg);
        C.ticking = ui_text_width(t, UI_FONT_SYSTEM, 12) > clipw;
        if (!C.ticking)
            ui_text(&rTitle, rTitle.left, (short)(rTitle.bottom - 3), t, &kLcdOn, &kLcdBg, UI_FONT_SYSTEM, 12);
        else
            C.tick_time = 0;                /* fuerza el primer cuadro */
    }
    if (C.ticking && now - C.tick_time >= 6) {
        snprintf(padded, sizeof padded, "%s      ", t);       /* los espacios borran el hueco */
        w = ui_text_width(padded, UI_FONT_SYSTEM, 12);
        C.tick_x = (short)((C.tick_x + 2) % w);
        ui_text(&rTitle, (short)(rTitle.left - C.tick_x), (short)(rTitle.bottom - 3), padded, &kLcdOn, &kLcdBg,
                UI_FONT_SYSTEM, 12);
        ui_text(&rTitle, (short)(rTitle.left - C.tick_x + w), (short)(rTitle.bottom - 3), padded, &kLcdOn,
                &kLcdBg, UI_FONT_SYSTEM, 12);
        C.tick_time = now;
    }
}

/* ------------------------------------------------------------------------ */

OSErr ui_main_create(void)
{
    Rect r;
    Str255 title;

    SetRect(&r, 30, 60, 30 + WIN_W, 60 + WIN_H);
    title[0] = 6;
    memcpy(title + 1, "MP368K", 6);
    gWin = NewCWindow(NULL, &r, title, true, noGrowDocProc, (WindowPtr)-1, true, 0);
    if (gWin == NULL)
        return memFullErr;
    ui_draw_init();
    dyn_invalidate();
    return noErr;
}

void ui_main_destroy(void)
{
    if (gWin != NULL) {
        DisposeWindow(gWin);
        gWin = NULL;
    }
    ui_draw_shutdown();
}

WindowPtr ui_main_window(void)
{
    return gWin;
}

void ui_main_set_debug(Boolean on)
{
    gDebug = on;
    C.format[0] = '\0';
}

void ui_main_set_modes(Boolean shuffle, RepeatMode repeat, Boolean playlist_visible)
{
    GrafPtr saved;

    if (shuffle == gShuffle && repeat == gRepeat && playlist_visible == gListVisible)
        return;
    gShuffle = shuffle;
    gRepeat = repeat;
    gListVisible = playlist_visible;
    if (gWin != NULL) {
        GetPort(&saved);
        SetPort(gWin);
        draw_toggles();
        SetPort(saved);
    }
}

void ui_main_draw_all(void)
{
    GrafPtr saved;
    Rect all;
    const PlayerStatus *st = player_status();

    if (gWin == NULL)
        return;
    GetPort(&saved);
    SetPort(gWin);
    SetRect(&all, 0, 0, WIN_W, WIN_H);
    ui_fill(&all, &kPlatBg);

    ui_well(&rLcd, &kLcdBg);
    ui_lcd_time(&rLcd, st->position_ms, st->has_file);
    ui_well(&rInfo, &kLcdBg);
    dyn_invalidate();
    draw_title(st, true);
    draw_info_lines(st);
    ui_slider(&rSeek, seek_pos_now(st), st->has_file && st->state != PLAYER_STOPPED);
    draw_transport(st);
    draw_volume_balance();
    draw_toggles();

    C.valid = true;
    C.state = st->state;
    C.has_file = st->has_file;
    C.secs = st->position_ms / 1000;
    C.seek_pos = seek_pos_now(st);
    SetPort(saved);
}

void ui_main_tick(void)
{
    GrafPtr saved;
    const PlayerStatus *st;
    unsigned seek;

    if (gWin == NULL || !C.valid)
        return;
    st = player_status();
    GetPort(&saved);
    SetPort(gWin);

    if (st->state != C.state || st->has_file != C.has_file) {
        draw_transport(st);
        C.state = st->state;
        C.has_file = st->has_file;
        C.secs = (unsigned long)-1;                 /* repinta el tiempo */
        C.seek_pos = (unsigned)-1;
    }
    if (st->position_ms / 1000 != C.secs) {
        C.secs = st->position_ms / 1000;
        ui_lcd_time(&rLcd, st->position_ms, st->has_file);
    }
    seek = seek_pos_now(st);
    if (seek / 4 != C.seek_pos / 4 || C.seek_pos == (unsigned)-1) {
        C.seek_pos = seek;
        ui_slider(&rSeek, seek, st->has_file && st->state != PLAYER_STOPPED);
    }
    draw_title(st, false);
    draw_info_lines(st);
    SetPort(saved);
}

/* ------------------------------------------------------------------------ */
/* Ratón                                                                     */
/* ------------------------------------------------------------------------ */

static void track_button(unsigned i)
{
    Rect r = kBtn[i].r;
    Boolean inside = true;
    Point pt;

    ui_glyph_button(&r, kBtn[i].glyph, true, false);
    while (StillDown()) {
        Boolean in;

        GetMouse(&pt);
        in = PtInRect(pt, &r);
        if (in != inside) {
            inside = in;
            ui_glyph_button(&r, kBtn[i].glyph, inside, false);
        }
        player_idle();                              /* el audio no espera al ratón */
    }
    FlushEvents(mUpMask, 0);
    draw_transport(player_status());
    if (inside)
        app_command(kBtn[i].cmd);
}

/* Botón de texto alternable: al soltar dentro se manda el comando; la app actualiza el estado. */
static void track_toggle(const Rect *r, const char *label, Boolean on, UiCommand cmd)
{
    Boolean inside = true;
    Point pt;

    ui_text_button(r, label, true, on, true);
    while (StillDown()) {
        Boolean in;

        GetMouse(&pt);
        in = PtInRect(pt, r);
        if (in != inside) {
            inside = in;
            ui_text_button(r, label, inside, on, true);
        }
        player_idle();
    }
    FlushEvents(mUpMask, 0);
    ui_text_button(r, label, false, on, true);
    if (inside)
        app_command(cmd);
}

static void track_seek(void)
{
    const PlayerStatus *st = player_status();
    unsigned pos = 0, last = (unsigned)-1;
    Point pt;

    if (!st->has_file || st->state == PLAYER_STOPPED)
        return;
    while (StillDown()) {
        GetMouse(&pt);
        pos = ui_slider_pos(&rSeek, pt.h);
        if (pos != last) {
            last = pos;
            ui_slider(&rSeek, pos, true);
            ui_lcd_time(&rLcd, (unsigned long)((unsigned long long)st->duration_ms * pos / 1000), true);
        }
        player_idle();
    }
    FlushEvents(mUpMask, 0);
    if (last != (unsigned)-1)
        player_seek(pos);
    C.secs = (unsigned long)-1;
    C.seek_pos = (unsigned)-1;
}

static void track_vol(void)
{
    Point pt;
    unsigned pos, last = (unsigned)-1;

    while (StillDown()) {
        GetMouse(&pt);
        pos = ui_slider_pos(&rVol, pt.h);
        if (pos != last) {
            last = pos;
            gVolume = (pos + 5) / 10;
            player_set_volume(gVolume);
            ui_slider(&rVol, gVolume * 10, true);
        }
        player_idle();
    }
    FlushEvents(mUpMask, 0);
}

static void track_balance(void)
{
    Point pt;
    unsigned pos, last = (unsigned)-1;

    while (StillDown()) {
        GetMouse(&pt);
        pos = ui_slider_pos(&rBal, pt.h);
        if (pos != last) {
            last = pos;
            gBalance = (int)pos / 5 - 100;
            if (gBalance > -8 && gBalance < 8)
                gBalance = 0;                       /* detente en el centro */
            player_set_balance(gBalance);
            ui_slider(&rBal, (unsigned)((gBalance + 100) * 5), true);
        }
        player_idle();
    }
    FlushEvents(mUpMask, 0);
}

void ui_main_click(Point where)
{
    GrafPtr saved;
    Rect seek = rSeek, vol = rVol, bal = rBal;
    unsigned i;

    if (gWin == NULL)
        return;
    GetPort(&saved);
    SetPort(gWin);

    for (i = 0; i < NBTN; i++)
        if (PtInRect(where, &kBtn[i].r)) {
            track_button(i);
            goto done;
        }
    if (PtInRect(where, &rShuffle)) {
        track_toggle(&rShuffle, "Shuffle", gShuffle, UI_CMD_SHUFFLE);
        goto done;
    }
    if (PtInRect(where, &rRepeat)) {
        track_toggle(&rRepeat, repeat_label(), gRepeat != REPEAT_OFF, UI_CMD_REPEAT);
        goto done;
    }
    if (PtInRect(where, &rPlaylist)) {
        track_toggle(&rPlaylist, "Playlist", gListVisible, UI_CMD_PLAYLIST);
        goto done;
    }
    InsetRect(&seek, -2, -2);
    InsetRect(&vol, -2, -2);
    InsetRect(&bal, -2, -2);
    if (PtInRect(where, &seek))
        track_seek();
    else if (PtInRect(where, &vol))
        track_vol();
    else if (PtInRect(where, &bal))
        track_balance();
done:
    SetPort(saved);
}
