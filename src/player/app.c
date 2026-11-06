/*
 * app — MP368K: inicialización del Toolbox, menús, bucle de eventos, lista y comandos.
 *
 * El bucle llama a player_idle() en cada vuelta (y la interfaz lo hace dentro de sus bucles de
 * ratón) para mantener lleno el anillo de audio. Los diálogos modales del sistema bloquean la
 * decodificación, por eso se pausa antes de abrirlos. Todo texto visible va en inglés de EE. UU.
 */
#include <Dialogs.h>
#include <Events.h>
#include <Fonts.h>
#include <Memory.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <StandardFile.h>
#include <TextEdit.h>
#include <ToolUtils.h>
#include <Windows.h>

#include <stdio.h>
#include <string.h>

#include "macenv.h"
#include "platform.h"
#include "player.h"
#include "playlist.h"
#include "ui_list.h"
#include "ui_main.h"

enum { MENU_APPLE = 128, MENU_FILE, MENU_CONTROLS, MENU_QUALITY, MENU_OPTIONS, MENU_WINDOW };

/* Posiciones de los ítems de menú (ver player.r). */
enum {
    FILE_OPEN = 1, FILE_ADD, FILE_ADD_FOLDER, FILE_OPEN_PLAYLIST, FILE_CLOSE = 6, FILE_QUIT = 8
};
enum {
    CTL_PLAY_PAUSE = 1, CTL_STOP, CTL_PREV = 4, CTL_NEXT, CTL_SHUFFLE = 7, CTL_REPEAT_OFF = 9,
    CTL_REPEAT_ALL, CTL_REPEAT_ONE
};
enum { OPT_OUT_AUTO = 1, OPT_OUT_8, OPT_OUT_16, OPT_TECH_INFO = 5 };
enum { ALRT_ABOUT = 128, ALRT_MESSAGE = 129 };

#define EXTRA_STACK  (48L * 1024)
#define MIN_STACK    (24L * 1024)

static Boolean gQuit;
static Boolean gDebug;
static unsigned gOutBits;               /* 0 = automático, 8, 16 */
static Boolean gEndedSeen;

void app_hook_idle(void)
{
    static Boolean inside;

    if (inside)
        return;
    inside = true;
    player_idle();
    inside = false;
}

/* ------------------------------------------------------------------------ */
/* Mensajes                                                                  */
/* ------------------------------------------------------------------------ */

static void pstr(Str255 dst, const char *src)
{
    size_t n = strlen(src);

    if (n > 255)
        n = 255;
    dst[0] = (unsigned char)n;
    memcpy(dst + 1, src, n);
}

static void alert_text(const char *text, Boolean is_error)
{
    Str255 p0, p1;

    pstr(p0, text);
    p1[0] = 0;
    ParamText(p0, p1, p1, p1);
    if (is_error)
        StopAlert(ALRT_MESSAGE, NULL);
    else
        NoteAlert(ALRT_MESSAGE, NULL);
}

static void show_error(const char *what, OSErr err)
{
    char buf[200];

    snprintf(buf, sizeof buf, "%s (error %d)", what, (int)err);
    alert_text(buf, true);
}

static const char *play_error_text(OSErr err)
{
    return err == PLAYER_ERR_NOT_MP3 ? "This does not look like a valid MP3 file."
                                     : "The file could not be played.";
}

/* ------------------------------------------------------------------------ */
/* Lista y reproducción                                                      */
/* ------------------------------------------------------------------------ */

static void sync_modes(void)
{
    Order *o = playlist_order();

    ui_main_set_modes(o->shuffle != 0, o->repeat, ui_list_visible());
}

/* Carga la pista `idx` de la lista en el motor; con play, la reproduce. */
static OSErr load_index(int idx, Boolean play, Boolean quiet)
{
    const PlaylistEntry *e = playlist_get((unsigned)idx);
    FSSpec spec;
    OSErr err;

    if (e == NULL)
        return paramErr;
    spec = e->spec;
    err = player_open(&spec);
    if (err == noErr) {
        order_set_current(playlist_order(), (unsigned)idx);
        if (play)
            err = player_play();
    }
    gEndedSeen = false;
    if (err != noErr && !quiet)
        show_error(play_error_text(err), err);
    ui_list_show_row(idx);
    ui_list_refresh();
    return err;
}

/* Siguiente pista según el orden; si no se puede cargar una, prueba la siguiente (sin alertas). */
static void go_next(Boolean automatic)
{
    Order *o = playlist_order();
    unsigned tries = playlist_count();

    while (tries-- > 0) {
        int idx = order_next(o, automatic);

        if (idx < 0)
            return;                                 /* se acabó la lista */
        if (load_index(idx, true, true) == noErr)
            return;
    }
}

static void go_prev(void)
{
    const PlayerStatus *st = player_status();
    int idx;

    if (st->state != PLAYER_STOPPED && st->position_ms > 3000) {
        player_seek(0);                             /* como en un CD: la primera vez reinicia */
        return;
    }
    idx = order_prev(playlist_order());
    if (idx >= 0)
        load_index(idx, true, false);
}

static void cycle_repeat(void)
{
    Order *o = playlist_order();

    order_set_repeat(o, o->repeat == REPEAT_OFF ? REPEAT_ALL : (o->repeat == REPEAT_ALL ? REPEAT_ONE : REPEAT_OFF));
    sync_modes();
}

static void toggle_shuffle(void)
{
    Order *o = playlist_order();

    order_set_shuffle(o, !o->shuffle);
    sync_modes();
}

static void toggle_list_window(void)
{
    ui_list_show(!ui_list_visible());
    sync_modes();
}

/* ------------------------------------------------------------------------ */
/* Diálogos y archivos                                                       */
/* ------------------------------------------------------------------------ */

/* Los diálogos modales bloquean el bucle: se pausa mientras están abiertos. */
static Boolean pause_for_modal(void)
{
    if (player_status()->state == PLAYER_PLAYING) {
        player_pause();
        return true;
    }
    return false;
}

static void resume_after_modal(Boolean was_playing)
{
    if (was_playing && player_status()->state == PLAYER_PAUSED)
        player_play();
}

static Boolean choose_file(StandardFileReply *reply)
{
    StandardGetFile(NULL, -1, NULL, reply);         /* todos los tipos: los MP3 de PC traen tipos variables */
    return reply->sfGood;
}

/* Agrega el archivo, la carpeta o la lista .m3u elegida. Devuelve cuántas pistas agregó. */
static unsigned add_from_reply(StandardFileReply *reply, Boolean folder)
{
    unsigned added = 0, skipped = 0;
    char msg[120];

    SetCursor(*GetCursor(watchCursor));
    if (folder) {
        added = playlist_add_folder_of(&reply->sfFile, &skipped);
    } else if (playlist_is_m3u(reply->sfFile.name)) {
        added = playlist_add_m3u(&reply->sfFile, &skipped);
    } else {
        added = playlist_add_file(&reply->sfFile) >= 0 ? 1 : 0;
        skipped = added ? 0 : 1;
    }
    InitCursor();

    if (added == 0) {
        alert_text(folder ? "No playable MP3 files were found in that folder."
                          : "No playable MP3 files were found.", true);
    } else if (skipped > 0) {
        snprintf(msg, sizeof msg, "%u of the entries could not be added because the files were not found or are not valid MP3 files.", skipped);
        alert_text(msg, false);
    }
    return added;
}

/*
 * Open...: reemplaza la lista y reproduce. Primero se agrega lo nuevo y solo si salió bien se
 * quita lo anterior, así abrir un archivo equivocado no destruye la lista.
 */
static void do_open(Boolean m3u_only)
{
    StandardFileReply reply;
    Boolean was = pause_for_modal();
    unsigned old_count;

    if (!choose_file(&reply)) {
        resume_after_modal(was);
        return;
    }
    if (m3u_only && !playlist_is_m3u(reply.sfFile.name)) {
        alert_text("That is not a playlist (.m3u) file.", true);
        resume_after_modal(was);
        return;
    }
    old_count = playlist_count();
    if (add_from_reply(&reply, false) == 0) {
        resume_after_modal(was);                    /* no cambió nada */
        return;
    }
    player_stop();
    while (old_count-- > 0)
        playlist_remove(0);                         /* quita lo anterior */
    load_index(0, true, false);
    ui_list_set_selected(-1);
    if (m3u_only || playlist_count() > 1) {
        ui_list_show(true);
        sync_modes();
    }
}

/* Add...: agrega a la lista sin interrumpir lo que suena. */
static void do_add(Boolean folder)
{
    StandardFileReply reply;
    Boolean was = pause_for_modal();

    if (!choose_file(&reply)) {
        resume_after_modal(was);
        return;
    }
    if (add_from_reply(&reply, folder) > 0 && !player_status()->has_file)
        load_index(0, false, true);                 /* deja la primera lista para Play */
    ui_list_refresh();
    ui_list_show(true);
    sync_modes();
    resume_after_modal(was);
}

static void do_about(void)
{
    Boolean was = pause_for_modal();

    NoteAlert(ALRT_ABOUT, NULL);
    resume_after_modal(was);
}

static void toggle_play_pause(void)
{
    const PlayerStatus *st = player_status();
    OSErr err = noErr;

    if (!st->has_file) {
        if (playlist_count() > 0)
            err = load_index(0, true, false);
        else
            do_open(false);
        return;
    }
    if (st->state == PLAYER_PLAYING)
        player_pause();
    else
        err = player_play();
    if (err != noErr)
        show_error(play_error_text(err), err);
    gEndedSeen = false;
}

void app_command(UiCommand cmd)
{
    const PlayerStatus *st = player_status();
    OSErr err;

    switch (cmd) {
    case UI_CMD_OPEN:
        do_open(false);
        break;
    case UI_CMD_PLAY:
        if (!st->has_file) {
            toggle_play_pause();
        } else if (st->state != PLAYER_PLAYING) {
            err = player_play();
            if (err != noErr)
                show_error(play_error_text(err), err);
            gEndedSeen = false;
        }
        break;
    case UI_CMD_PAUSE:
        if (st->state == PLAYER_PLAYING)
            player_pause();
        else if (st->state == PLAYER_PAUSED)
            player_play();
        break;
    case UI_CMD_STOP:
        player_stop();
        break;
    case UI_CMD_PREV:
        go_prev();
        break;
    case UI_CMD_NEXT:
        go_next(false);
        break;
    case UI_CMD_SHUFFLE:
        toggle_shuffle();
        break;
    case UI_CMD_REPEAT:
        cycle_repeat();
        break;
    case UI_CMD_PLAYLIST:
        toggle_list_window();
        break;
    default:
        break;
    }
}

void app_list_command(ListCommand cmd, int arg)
{
    switch (cmd) {
    case LIST_ADD:
        do_add(false);
        break;
    case LIST_ADD_FOLDER:
        do_add(true);
        break;
    case LIST_REMOVE:
        if (arg >= 0 && arg < (int)playlist_count()) {
            if (arg == order_current(playlist_order()))
                player_stop();
            playlist_remove((unsigned)arg);
            ui_list_set_selected(arg);
        }
        break;
    case LIST_CLEAR:
        player_stop();
        playlist_clear();
        ui_list_set_selected(-1);
        break;
    case LIST_PLAY:
        load_index(arg, true, false);
        break;
    }
}

/* ------------------------------------------------------------------------ */
/* Menús                                                                     */
/* ------------------------------------------------------------------------ */

static void enable(MenuHandle m, short item, Boolean on)
{
    if (on)
        EnableItem(m, item);
    else
        DisableItem(m, item);
}

static void update_menus(void)
{
    const PlayerStatus *st = player_status();
    Order *o = playlist_order();
    MenuHandle mf = GetMenuHandle(MENU_FILE);
    MenuHandle mc = GetMenuHandle(MENU_CONTROLS);
    MenuHandle mq = GetMenuHandle(MENU_QUALITY);
    MenuHandle mo = GetMenuHandle(MENU_OPTIONS);
    MenuHandle mw = GetMenuHandle(MENU_WINDOW);
    Boolean any = st->has_file || playlist_count() > 0;
    short i;

    (void)mf;
    enable(mc, CTL_PLAY_PAUSE, any);
    enable(mc, CTL_STOP, st->has_file);
    enable(mc, CTL_PREV, playlist_count() > 0);
    enable(mc, CTL_NEXT, playlist_count() > 0);
    CheckItem(mc, CTL_SHUFFLE, o->shuffle != 0);
    CheckItem(mc, CTL_REPEAT_OFF, o->repeat == REPEAT_OFF);
    CheckItem(mc, CTL_REPEAT_ALL, o->repeat == REPEAT_ALL);
    CheckItem(mc, CTL_REPEAT_ONE, o->repeat == REPEAT_ONE);
    for (i = 1; i <= QUALITY_COUNT; i++)
        CheckItem(mq, i, st->quality == (PlayerQuality)(i - 1));
    CheckItem(mo, OPT_OUT_AUTO, gOutBits == 0);
    CheckItem(mo, OPT_OUT_8, gOutBits == 8);
    CheckItem(mo, OPT_OUT_16, gOutBits == 16);
    CheckItem(mo, OPT_TECH_INFO, gDebug);
    CheckItem(mw, 1, ui_list_visible());
}

static void close_front_window(void)
{
    if (ui_list_visible() && FrontWindow() == ui_list_window()) {
        ui_list_show(false);
        sync_modes();
    } else {
        gQuit = true;
    }
}

static void do_menu(long result)
{
    short menu = (short)((result >> 16) & 0xFFFF);
    short item = (short)(result & 0xFFFF);

    switch (menu) {
    case MENU_APPLE:
        if (item == 1)
            do_about();
        break;
    case MENU_FILE:
        if (item == FILE_OPEN)
            do_open(false);
        else if (item == FILE_ADD)
            do_add(false);
        else if (item == FILE_ADD_FOLDER)
            do_add(true);
        else if (item == FILE_OPEN_PLAYLIST)
            do_open(true);
        else if (item == FILE_CLOSE)
            close_front_window();
        else if (item == FILE_QUIT)
            gQuit = true;
        break;
    case MENU_CONTROLS:
        if (item == CTL_PLAY_PAUSE)
            toggle_play_pause();
        else if (item == CTL_STOP)
            player_stop();
        else if (item == CTL_PREV)
            go_prev();
        else if (item == CTL_NEXT)
            go_next(false);
        else if (item == CTL_SHUFFLE)
            toggle_shuffle();
        else if (item >= CTL_REPEAT_OFF && item <= CTL_REPEAT_ONE) {
            order_set_repeat(playlist_order(), item == CTL_REPEAT_OFF ? REPEAT_OFF
                                                 : (item == CTL_REPEAT_ALL ? REPEAT_ALL : REPEAT_ONE));
            sync_modes();
        }
        break;
    case MENU_QUALITY:
        if (item >= 1 && item <= QUALITY_COUNT)
            player_set_quality((PlayerQuality)(item - 1));
        break;
    case MENU_OPTIONS:
        if (item >= OPT_OUT_AUTO && item <= OPT_OUT_16) {
            gOutBits = item == OPT_OUT_AUTO ? 0 : (item == OPT_OUT_8 ? 8 : 16);
            player_set_output_bits(gOutBits);
        } else if (item == OPT_TECH_INFO) {
            gDebug = !gDebug;
            ui_main_set_debug(gDebug);
        }
        break;
    case MENU_WINDOW:
        if (item == 1)
            toggle_list_window();
        break;
    }
    HiliteMenu(0);
}

/* ------------------------------------------------------------------------ */
/* Eventos                                                                   */
/* ------------------------------------------------------------------------ */

static void do_key(EventRecord *evt)
{
    char c = (char)(evt->message & charCodeMask);
    const PlayerStatus *st = player_status();

    if (evt->modifiers & cmdKey) {
        long m;

        update_menus();
        m = MenuKey(c);
        if ((m >> 16) != 0)
            do_menu(m);
        return;
    }
    if (c == ' ') {
        toggle_play_pause();
        return;
    }
    if (ui_list_visible() && FrontWindow() == ui_list_window()) {
        ui_list_key(c);
        return;
    }
    if ((c == 0x1C || c == 0x1D) && st->has_file && st->state != PLAYER_STOPPED && st->duration_ms) {
        long pos = (long)st->position_ms + (c == 0x1D ? 5000L : -5000L);     /* flechas: -5 s / +5 s */

        if (pos < 0)
            pos = 0;
        if ((unsigned long)pos > st->duration_ms)
            pos = (long)st->duration_ms;
        player_seek((unsigned)((unsigned long long)pos * 1000ULL / st->duration_ms));
    }
}

static void do_mouse_down(EventRecord *evt)
{
    WindowPtr win;
    short part = FindWindow(evt->where, &win);

    switch (part) {
    case inMenuBar:
        update_menus();
        do_menu(platform_menu_select(evt->where));
        break;
    case inDrag:
        platform_drag_window(win, evt->where);
        break;
    case inGoAway:
        if (TrackGoAway(win, evt->where)) {
            if (win == ui_list_window()) {
                ui_list_show(false);
                sync_modes();
            } else {
                gQuit = true;
            }
        }
        break;
    case inContent:
        if (win != FrontWindow()) {
            SelectWindow(win);
        } else {
            Point p = evt->where;

            SetPort(win);
            GlobalToLocal(&p);
            if (win == ui_main_window())
                ui_main_click(p);
            else if (win == ui_list_window())
                ui_list_click(p, evt->when);
        }
        break;
    default:
        break;
    }
}

static void handle_event(EventRecord *evt)
{
    switch (evt->what) {
    case mouseDown:
        do_mouse_down(evt);
        break;
    case keyDown:
    case autoKey:
        do_key(evt);
        break;
    case updateEvt: {
        WindowPtr w = (WindowPtr)evt->message;

        BeginUpdate(w);
        SetPort(w);
        if (w == ui_main_window())
            ui_main_draw_all();
        else if (w == ui_list_window())
            ui_list_draw_all();
        EndUpdate(w);
        break;
    }
    case activateEvt:
        if ((WindowPtr)evt->message == ui_list_window())
            ui_list_activate((evt->modifiers & activeFlag) != 0);
        break;
    default:
        break;
    }
}

static void init_toolbox(void)
{
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();
    FlushEvents(everyEvent, 0);
}

int main(void)
{
    EventRecord evt;
    Handle mbar;
    long stack;
    OSErr err;

    init_toolbox();
    stack = macenv_grow_stack(EXTRA_STACK);
    if (stack < MIN_STACK) {
        show_error("There is not enough memory for the stack.", (OSErr)stack);
        return 1;
    }

    mbar = GetNewMBar(128);
    if (mbar == NULL) {
        show_error("The menu resources are missing.", resNotFound);
        return 1;
    }
    SetMenuBar(mbar);
    DisposeHandle(mbar);
    DrawMenuBar();

    err = player_init();
    if (err == noErr)
        err = playlist_init();
    if (err != noErr) {
        show_error("The player could not be started.", err);
        return 1;
    }
    err = ui_main_create();
    if (err == noErr)
        err = ui_list_create();
    if (err != noErr) {
        show_error("The windows could not be created.", err);
        player_shutdown();
        return 1;
    }
    sync_modes();

    while (!gQuit) {
        const PlayerStatus *st;
        long sleep;

        sleep = player_wants_cpu() ? 0 : (player_status()->state == PLAYER_STOPPED ? 30 : 3);
        if (WaitNextEvent(everyEvent, &evt, sleep, NULL))
            handle_event(&evt);

        player_idle();
        ui_main_tick();

        /* Fin de pista: seguir con la siguiente según el orden y el modo de repetición. */
        st = player_status();
        if (st->state != PLAYER_STOPPED)
            gEndedSeen = false;
        if (st->ended && st->state == PLAYER_STOPPED && !gEndedSeen) {
            gEndedSeen = true;
            go_next(true);
        }
    }

    player_shutdown();
    playlist_shutdown();
    ui_list_destroy();
    ui_main_destroy();
    return 0;
}
