/*
 * qttest — Fase 6 de MP368K: ¿hay QuickTime y qué audio abre?
 *
 * Diagnóstico para el equipo donde se corra (Infinite Mac, Basilisk II, LC 580). Usa SOLO funciones que las
 * Multiversal Interfaces ya declaran para 68k (EnterMovies, OpenMovieFile, NewMovieFromFile, StartMovie,
 * MoviesTask, IsMovieDone...). No inventa ningún selector del trap 0xAAAA: lo que falte (duración, posición,
 * volumen, importadores) se declara más tarde y solo con fuente anotada.
 *
 * Qué hace:
 *   1. Gestalt: versión del sistema, ¿QuickTime ('qtim')? ¿versión? Si no hay, lo dice y termina sin tocar el trap.
 *   2. EnterMovies.
 *   3. Para cada archivo de la lista que exista junto a la aplicación: OpenMovieFile + NewMovieFromFile; si abre,
 *      informa la velocidad preferida y el rectángulo, y lo reproduce unos segundos con MoviesTask. Escucha y
 *      anota en la bitácora si sonó: el programa no puede oírlo.
 *   4. Escribe qttest.txt junto a la aplicación.
 *
 * Sin medir en un Mac real: lo que salga solo vale para el sistema donde se corrió.
 */
#include <Events.h>
#include <Files.h>
#include <Gestalt.h>
#include <MacTypes.h>
#include <Quickdraw.h>
/* Las declaraciones de QuickTime (EnterMovies, NewMovieFromFile...) vienen en Multiverse.h, que cada header de las Multiversal incluye;
 * no existe un QuickTime.h suelto en este toolchain. */
#include <TextUtils.h>

#include <stdio.h>
#include <string.h>

#include "appfile.h"
#include "build_info.h"
#include "textlog.h"

/* Selectores de Gestalt que las Multiversal no traen (con prefijo MP_). Valores de Gestalt.h de Apple: por verificar con la salida. */
enum {
    MP_gestaltQuickTime         = 'qtim',   /* versión en la palabra alta (0x0210 = 2.1) */
    MP_gestaltQuickTimeFeatures = 'qtrs'
};

#define PLAY_TICKS (60L * 4)                /* 4 s por archivo */
#define NEW_MOVIE_ACTIVE 1                  /* newMovieActive */

/* Archivos que se intentan (en la carpeta de la aplicación). Ninguno es MP3, FLAC, Ogg ni WMA: eso lo hace el reproductor. */
static const char *const kFiles[] = {
    "t.wav", "t.aif", "t.aifc", "t.au", "t.caf", "t.m4a", "t.ulaw.wav", "t.snd",
    "test.wav", "test.aif", "mix.wav",
};

static OSErr spec_next_to_app(const char *name, FSSpec *out)
{
    FSSpec app;
    Str255 pname;
    size_t len = strlen(name);
    OSErr err = appfile_spec(&app);

    if (err != noErr)
        return err;
    if (len > 31)
        len = 31;
    pname[0] = (unsigned char)len;
    memcpy(pname + 1, name, len);
    return FSMakeFSSpec(app.vRefNum, app.parID, pname, out);
}

static void try_file(const char *name)
{
    FSSpec spec;
    short ref = 0;
    short resid = 0;
    Boolean changed = false;
    Movie movie = NULL;
    OSErr err, err2;
    long t0, t1, n;
    Fixed rate;
    Rect box;
    Boolean done;

    err = spec_next_to_app(name, &spec);
    if (err != noErr) {
        textlog_printf("%-12s no está junto a la app (FSMakeFSSpec err=%d)\n", name, err);
        return;
    }
    err = OpenMovieFile(&spec, &ref, 1 /* fsRdPerm */);
    if (err != noErr) {
        textlog_printf("%-12s OpenMovieFile err=%d\n", name, err);
        return;
    }
    err = NewMovieFromFile(&movie, ref, &resid, NULL, NEW_MOVIE_ACTIVE, &changed);
    err2 = CloseMovieFile(ref);
    if (err != noErr || movie == NULL) {
        textlog_printf("%-12s NewMovieFromFile err=%d (movie=%s)  CloseMovieFile err=%d  -> NO ABRE por este camino\n",
                       name, err, movie ? "sí" : "NULL", err2);
        if (movie)
            DisposeMovie(movie);
        return;
    }
    rate = GetMoviePreferredRate(movie);
    box.top = box.left = box.bottom = box.right = -1;
    GetMovieBox(movie, &box);
    textlog_printf("%-12s ABRE  err=%d resid=%d tasa preferida=0x%08lX  caja=(%d,%d,%d,%d)  CloseMovieFile err=%d\n",
                   name, err, resid, (unsigned long)rate, box.top, box.left, box.bottom, box.right, err2);

    err = PrerollMovie(movie, 0, rate);
    textlog_printf("%-12s   PrerollMovie err=%d\n", "", err);
    StartMovie(movie);
    t0 = TickCount();
    n = 0;
    do {
        MoviesTask(movie, 0);
        n++;
        done = IsMovieDone(movie);
        t1 = TickCount();
    } while (!done && t1 - t0 < PLAY_TICKS);
    StopMovie(movie);
    textlog_printf("%-12s   reproducido %ld ticks (%ld vueltas de MoviesTask), IsMovieDone=%s  -> anote si SONÓ\n",
                   "", t1 - t0, n, done ? "sí" : "no");
    DisposeMovie(movie);
}

int main(void)
{
    long v = 0;
    OSErr err;
    size_t i;

    textlog_init();
    textlog_printf("# MP368K qttest\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__, MP368K_BUILD_FLAGS);

    err = Gestalt(gestaltSystemVersion, &v);
    textlog_printf("sistema: Gestalt('sysv') err=%d valor=0x%04lX\n", err, v);
    v = 0;
    err = Gestalt(MP_gestaltQuickTime, &v);
    textlog_printf("QuickTime: Gestalt('qtim') err=%d valor=0x%08lX\n", err, v);
    if (err != noErr) {
        textlog_printf("QuickTime NO está instalado (o Gestalt no lo informa). No se llama a nada de QuickTime.\n");
        textlog_printf("RESULTADO: SIN QUICKTIME\n");
    } else {
        long f = 0;

        err = Gestalt(MP_gestaltQuickTimeFeatures, &f);
        textlog_printf("QuickTime features: Gestalt('qtrs') err=%d valor=0x%08lX\n", err, f);
        err = EnterMovies();
        textlog_printf("EnterMovies err=%d\n", err);
        if (err == noErr) {
            for (i = 0; i < sizeof kFiles / sizeof kFiles[0]; i++)
                try_file(kFiles[i]);
            ExitMovies();
            textlog_printf("ExitMovies hecho\n");
        }
        textlog_printf("RESULTADO: terminado (revise cuáles abren y cuáles sonaron)\n");
    }

    err = textlog_save_next_to_app("qttest.txt");
    textlog_printf("\nqttest.txt guardado: err=%d\n", err);
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
