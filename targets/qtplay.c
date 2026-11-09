/*
 * qtplay en 68k — reproducción por QuickTime con las Multiversal Interfaces (ver src/qtplay.h y ADR-010).
 *
 * Usa SOLO funciones que las Multiversal ya declaran para 68k (probadas con targets/qttest.c en QuickTime 2.5 bajo Infinite
 * Mac: EnterMovies, OpenMovieFile, NewMovieFromFile, CloseMovieFile, PrerollMovie, StartMovie, StopMovie,
 * GoToBeginningOfMovie, MoviesTask, IsMovieDone, DisposeMovie, ExitMovies). No hay duración, posición, búsqueda ni volumen
 * de la película porque sus selectores no están declarados y no se inventan.
 *
 * Sin probar en el LC 580. Sin confirmar: que StopMovie + StartMovie reanuden donde quedó (no lo ejercitó qttest).
 * Nada de esto corre en interrupción.
 */
#include "qtplay.h"

#include <Gestalt.h>
#include <MacTypes.h>

/* Selector de Gestalt que las Multiversal no traen. Valor de Gestalt.h de Apple: por verificar con la salida de qttest. */
enum { MP_gestaltQuickTime = 'qtim' };

#define NEW_MOVIE_ACTIVE 1          /* newMovieActive */
#define FS_RD_PERM 1                /* fsRdPerm */

static Boolean sTried;              /* ya se consultó Gestalt y se intentó EnterMovies */
static Boolean sEntered;            /* EnterMovies tuvo éxito */
static unsigned sVersion;
static Movie sMovie;

Boolean qtplay_available(void)
{
    if (!sTried) {
        long v = 0;

        sTried = true;
        if (Gestalt(MP_gestaltQuickTime, &v) == noErr && EnterMovies() == noErr) {
            sEntered = true;
            sVersion = (unsigned)((unsigned long)v >> 16);
        }
    }
    return sEntered;
}

unsigned qtplay_version(void)
{
    return qtplay_available() ? sVersion : 0;
}

OSErr qtplay_open(const FSSpec *spec)
{
    short ref = 0, resid = 0;
    Boolean changed = false;
    Movie m = NULL;
    OSErr err;

    if (!qtplay_available())
        return paramErr;
    qtplay_close();
    err = OpenMovieFile(spec, &ref, FS_RD_PERM);
    if (err != noErr)
        return err;
    err = NewMovieFromFile(&m, ref, &resid, NULL, NEW_MOVIE_ACTIVE, &changed);
    CloseMovieFile(ref);
    if (err != noErr || m == NULL) {
        if (m != NULL)
            DisposeMovie(m);
        return err != noErr ? err : paramErr;
    }
    PrerollMovie(m, 0, GetMoviePreferredRate(m));
    sMovie = m;
    return noErr;
}

void qtplay_start(void)
{
    if (sMovie != NULL)
        StartMovie(sMovie);
}

void qtplay_stop(void)
{
    if (sMovie != NULL)
        StopMovie(sMovie);
}

OSErr qtplay_rewind(void)
{
    if (sMovie == NULL)
        return paramErr;
    GoToBeginningOfMovie(sMovie);
    return noErr;
}

void qtplay_task(void)
{
    if (sMovie != NULL)
        MoviesTask(sMovie, 0);
}

Boolean qtplay_is_done(void)
{
    return sMovie == NULL || IsMovieDone(sMovie);
}

void qtplay_close(void)
{
    if (sMovie != NULL) {
        StopMovie(sMovie);
        DisposeMovie(sMovie);
        sMovie = NULL;
    }
}

void qtplay_shutdown(void)
{
    qtplay_close();
    if (sEntered) {
        ExitMovies();
        sEntered = false;
    }
    sTried = false;
}
