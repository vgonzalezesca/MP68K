/*
 * player — ver player.h.
 */
#include "player.h"

#include <Events.h>

#include <string.h>

#include "audio.h"
#include "codec.h"
#include "fileread.h"
#include "trackinfo.h"
#include "mactime.h"
#include "memblock.h"

#define READ_BUF_BYTES   (32L * 1024)
#define CODEC_ARENA_BYTES (256L * 1024)    /* memoria de los códecs de terceros; FLAC usa ~36 KB (medido en el PC) */
#define APP_RING_MS      4000       /* colchón para menús y arrastres que bloquean el bucle */
#define PREROLL_MS       250
#define IDLE_BUDGET_US   20000UL    /* tope por llamada: eventos al menos cada ~50 ms (RNF2) */
#define MAX_FRAME_FRAMES 1152
#define PROBE_FRAMES     14
#define PROBE_SKIP       2          /* frames iniciales que no cuentan (cachés frías) */
#define PROBE_AT_X1000   200        /* se mide al 20 % del archivo: el comienzo suele ser fácil */

/* Umbral del modo automático: RTF mínimo (x100) para quedarse con un perfil. PROVISIONAL:
 * es el presupuesto de SPECS §3; se fija con los datos del LC 580. */
#define AUTO_RTF_X100    150
#define DEGRADE_UNDERRUNS 2         /* cortes dentro de la ventana que hacen bajar de perfil */
#define DEGRADE_WINDOW_TICKS 600    /* 10 s */

static const DecProfile kLadder[] = { DEC_PROFILE_FULL, DEC_PROFILE_HALF, DEC_PROFILE_HALF_MONO };
#define LADDER_N (sizeof kLadder / sizeof kLadder[0])

static struct {
    Boolean inited;
    FileReader fr;
    Boolean fileOpen;
    MemBlock rbuf;
    MemBlock arena;                     /* memoria de los códecs */
    FSSpec spec;
    const Codec *codec;                 /* códec del archivo abierto */
    CodecIO io;
    CodecInfo ci;                       /* datos del archivo abierto */
    DecFrame f;
    Boolean audioOpen;
    Boolean draining;
    unsigned bitsPref;
    unsigned volume;
    int balance;
    unsigned long base_ms;
    unsigned long under_base, under_tick;
    unsigned long under_total_base;     /* underruns acumulados al abrir la pista */
    uint64_t last_idle_us;              /* instante de la llamada anterior a player_idle (0 = no medir) */
    unsigned long max_gap_ms;
    PlayerStatus st;
} P;

const char *player_quality_name(PlayerQuality q)
{
    static const char *const names[QUALITY_COUNT] = {
        "Automatic", "Full", "Half Rate", "Mono", "Half Rate Mono"
    };
    return (unsigned)q < QUALITY_COUNT ? names[q] : "?";
}

/* ------------------------------------------------------------------------ */
/* Utilidades                                                                */
/* ------------------------------------------------------------------------ */

static void apply_gain(void)
{
    unsigned g = (unsigned)((unsigned long)P.volume * P.volume * 256UL / 10000UL);
    unsigned l = g, r = g;

    if (P.balance > 0)
        l = (unsigned)((unsigned long)g * (100 - P.balance) / 100);
    else if (P.balance < 0)
        r = (unsigned)((unsigned long)g * (100 + P.balance) / 100);
    audio_set_gain(l, r);
}

static DecProfile profile_for_quality(PlayerQuality q)
{
    switch (q) {
    case QUALITY_HALF:      return DEC_PROFILE_HALF;
    case QUALITY_MONO:      return DEC_PROFILE_MONO;
    case QUALITY_HALF_MONO: return DEC_PROFILE_HALF_MONO;
    default:                return DEC_PROFILE_FULL;
    }
}

/* Siguiente perfil más barato de la escalera, o el mismo si no hay. */
static DecProfile next_cheaper(DecProfile p)
{
    unsigned i;

    for (i = 0; i + 1 < LADDER_N; i++)
        if (kLadder[i] == p) {
            DecProfile n = kLadder[i + 1];
            if (P.st.in_channels == 1 && n == DEC_PROFILE_HALF_MONO)
                return p;               /* en un archivo mono HALF_MONO == HALF */
            return n;
        }
    return p;
}

static int put_frame(const DecFrame *f)
{
    long done = 0, n;
    int spins = 0;

    while (done < (long)f->count) {
        n = audio_write(f->pcm + done, (long)f->count - done);
        if (n <= 0) {
            if (++spins > 50)
                return -1;
            continue;
        }
        done += n;
    }
    return 0;
}

static void close_stream(void)
{
    if (P.codec != NULL)
        P.codec->close();
}

static OSErr codec_error(int rc)
{
    switch (rc) {
    case CODEC_ERR_PARAMS: return PLAYER_ERR_PARAMS;
    case CODEC_ERR_MEMORY: return memFullErr;
    case CODEC_ERR_UNSUPPORTED: return PLAYER_ERR_UNSUPPORTED;
    default: return ioErr;
    }
}

static void stop_internal(void)
{
    if (P.audioOpen) {
        audio_close();
        P.audioOpen = false;
    }
    close_stream();
    P.draining = false;
    P.base_ms = 0;
    P.st.state = PLAYER_STOPPED;
    P.st.out_rate = P.st.out_channels = P.st.out_bits = 0;
}

/* ------------------------------------------------------------------------ */
/* Inicio de la reproducción en un instante dado                             */
/* ------------------------------------------------------------------------ */

static OSErr open_stream_at(DecProfile prof, unsigned long ms, DecFrame *first)
{
    int rc;

    close_stream();
    rc = P.codec->open(&P.io, prof, (uint8_t *)P.rbuf.data, (size_t)P.rbuf.size, &P.ci);
    if (rc != CODEC_OK)
        return codec_error(rc);
    if (ms > 0 && P.codec->seek(ms) != CODEC_OK)
        return ioErr;
    rc = P.codec->next(first);
    return rc == 1 ? noErr : PLAYER_ERR_UNSUPPORTED;
}

/*
 * (Re)inicia la salida en el instante `ms` con el perfil `prof`: abre el flujo, abre o
 * reinicia el audio, llena el anillo (pre-roll) y arranca. Si keep_paused, queda en pausa.
 */
static OSErr restart_at_ms(unsigned long ms, DecProfile prof, Boolean keep_paused)
{
    DecFrame *f = &P.f;
    AudioInfo ai;
    OSErr err;
    unsigned long long need;
    int rc;

    err = open_stream_at(prof, ms, f);
    if (err != noErr)
        return err;

    if (P.audioOpen && (P.st.out_rate != f->rate || P.st.out_channels != f->channels)) {
        audio_close();
        P.audioOpen = false;
    }
    if (!P.audioOpen) {
        err = audio_open(f->rate, f->channels, P.bitsPref, APP_RING_MS, &ai);
        if (err != noErr)
            return err;
        P.audioOpen = true;
        P.st.out_bits = ai.bits;
    } else {
        err = audio_restart();
        if (err != noErr) {
            P.audioOpen = false;
            return err;
        }
    }
    apply_gain();
    P.st.out_rate = f->rate;
    P.st.out_channels = f->channels;
    P.st.profile = prof;
    P.base_ms = ms;
    P.draining = false;
    P.st.ended = false;

    put_frame(f);
    P.st.frames_decoded++;
    need = (unsigned long long)f->rate * PREROLL_MS / 1000;
    while ((unsigned long long)audio_buffered_frames() < need) {
        rc = P.codec->next(f);
        if (rc != 1) {
            P.draining = true;
            audio_finish();
            break;
        }
        put_frame(f);
        P.st.frames_decoded++;
    }

    err = audio_start();
    if (err != noErr)
        return err;
    if (keep_paused) {
        audio_set_paused(true);
        P.st.state = PLAYER_PAUSED;
    } else {
        P.st.state = PLAYER_PLAYING;
    }
    P.under_base = audio_underruns();
    P.under_tick = TickCount();
    P.last_idle_us = 0;
    P.max_gap_ms = 0;
    return noErr;
}

/* Velocidad (x100) de un perfil, midiendo PROBE_FRAMES frames al 20 % del archivo. 0 = no se pudo. */
static unsigned probe_rtf_x100(DecProfile prof)
{
    DecFrame f;
    unsigned long long dec_us = 0, audio_us = 0;
    int k;

    close_stream();
    if (P.codec->open(&P.io, prof, (uint8_t *)P.rbuf.data, (size_t)P.rbuf.size, &P.ci) != CODEC_OK)
        return 0;
    if (P.codec->seek((unsigned long)((unsigned long long)P.st.duration_ms * PROBE_AT_X1000 / 1000)) != CODEC_OK) {
        close_stream();
        return 0;
    }
    for (k = 0; k < PROBE_FRAMES; k++) {
        uint64_t t0 = mactime_usec();
        int rc = P.codec->next(&f);
        uint64_t dt = mactime_usec() - t0;

        if (rc != 1)
            break;
        if (k >= PROBE_SKIP) {
            dec_us += dt;
            audio_us += (unsigned long long)f.frames * 1000000ULL / f.rate;
        }
    }
    close_stream();
    if (audio_us == 0)
        return 0;
    if (dec_us == 0)
        return 9999;                    /* más rápido que la resolución del reloj */
    return (unsigned)(audio_us * 100ULL / dec_us);
}

static DecProfile auto_pick(void)
{
    DecProfile chosen = DEC_PROFILE_FULL;
    unsigned i;

    P.st.preroll_rtf_x100 = 0;
    for (i = 0; i < LADDER_N; i++) {
        unsigned rtf;

        if (P.st.in_channels == 1 && kLadder[i] == DEC_PROFILE_HALF_MONO)
            continue;
        chosen = kLadder[i];
        rtf = probe_rtf_x100(chosen);
        P.st.preroll_rtf_x100 = rtf;
        if (rtf >= AUTO_RTF_X100 || rtf == 0)
            break;                      /* suficiente (o no se pudo medir: no seguir bajando) */
    }
    return chosen;
}

/* ------------------------------------------------------------------------ */
/* API                                                                       */
/* ------------------------------------------------------------------------ */

OSErr player_init(void)
{
    OSErr err;

    memset(&P, 0, sizeof P);
    err = memblock_alloc(&P.rbuf, READ_BUF_BYTES);
    if (err != noErr)
        return err;
    err = memblock_alloc(&P.arena, CODEC_ARENA_BYTES);
    if (err != noErr) {
        memblock_free(&P.rbuf);
        return err;
    }
    codec_init(P.arena.data, (size_t)P.arena.size);
    P.volume = 100;
    P.balance = 0;
    P.st.quality = QUALITY_AUTO;
    P.inited = true;
    apply_gain();
    return noErr;
}

void player_shutdown(void)
{
    if (!P.inited)
        return;
    stop_internal();
    if (P.fileOpen) {
        fileread_close(&P.fr);
        P.fileOpen = false;
    }
    memblock_free(&P.arena);
    memblock_free(&P.rbuf);
    P.inited = false;
}

OSErr player_open(FSSpec *spec)
{
    OSErr err;
    PlayerQuality q = P.st.quality;
    int n;

    if (!P.inited)
        return paramErr;
    stop_internal();
    if (P.fileOpen) {
        fileread_close(&P.fr);
        P.fileOpen = false;
    }
    memset(&P.st, 0, sizeof P.st);
    P.st.quality = q;

    err = fileread_open_spec(&P.fr, spec);
    if (err != noErr) {
        P.st.last_error = err;
        return err;
    }
    P.fileOpen = true;
    P.spec = *spec;

    n = spec->name[0];
    if (n > (int)sizeof P.st.name - 1)
        n = (int)sizeof P.st.name - 1;
    memcpy(P.st.name, spec->name + 1, n);
    P.st.name[n] = '\0';

    err = trackinfo_probe(&P.fr, &P.io, (uint8_t *)P.rbuf.data, P.rbuf.size, &P.ci, &P.codec);
    if (err != noErr) {
        fileread_close(&P.fr);
        P.fileOpen = false;
        P.codec = NULL;
        P.st.last_error = err;
        return err;
    }

    if (P.ci.tags.title[0] != '\0')
        strcpy(P.st.title, P.ci.tags.title);
    else
        trackinfo_name_without_extension(P.st.name, P.st.title, sizeof P.st.title);
    strcpy(P.st.artist, P.ci.tags.artist);

    strcpy(P.st.format, P.ci.format);
    P.st.has_profiles = P.ci.has_profiles != 0;
    P.st.eq_available = P.ci.eq_capable != 0;
    P.st.bits_src = P.ci.bits_src;
    P.st.vbr = P.ci.vbr != 0;
    P.st.in_rate = P.ci.rate;
    P.st.in_channels = P.ci.channels;
    P.st.duration_ms = P.ci.duration_ms;
    P.st.bitrate = P.ci.bitrate;

    P.st.has_file = true;
    P.st.state = PLAYER_STOPPED;
    P.st.profile = profile_for_quality(q == QUALITY_AUTO ? QUALITY_FULL : q);
    return noErr;
}

OSErr player_play(void)
{
    DecProfile prof;
    OSErr err;

    if (!P.st.has_file)
        return PLAYER_ERR_NO_FILE;
    if (P.st.state == PLAYER_PAUSED) {
        audio_set_paused(false);
        P.st.state = PLAYER_PLAYING;
        return noErr;
    }
    if (P.st.state == PLAYER_PLAYING)
        return noErr;

    P.st.auto_degraded = false;
    P.st.may_stutter = false;
    P.under_total_base = audio_underruns();
    if (!P.ci.has_profiles) {
        /* Sin perfiles no hay a qué bajar: se mide la velocidad solo para avisar si no alcanza. */
        unsigned rtf = probe_rtf_x100(DEC_PROFILE_FULL);

        P.st.preroll_rtf_x100 = rtf;
        P.st.may_stutter = rtf != 0 && rtf < 100;
        prof = DEC_PROFILE_FULL;
    } else if (P.st.quality == QUALITY_AUTO) {
        prof = auto_pick();
        P.st.may_stutter = P.st.preroll_rtf_x100 != 0 && P.st.preroll_rtf_x100 < 100;
    } else {
        prof = profile_for_quality(P.st.quality);
    }
    err = restart_at_ms(0, prof, false);
    if (err != noErr) {
        P.st.last_error = err;
        stop_internal();
    }
    return err;
}

void player_pause(void)
{
    if (P.st.state == PLAYER_PLAYING) {
        audio_set_paused(true);
        P.st.state = PLAYER_PAUSED;
    }
}

void player_stop(void)
{
    if (P.st.state != PLAYER_STOPPED)
        stop_internal();
    P.st.ended = false;
}

OSErr player_seek(unsigned fraction_x1000)
{
    unsigned long ms;
    Boolean was_paused;
    OSErr err;

    if (!P.st.has_file)
        return PLAYER_ERR_NO_FILE;
    if (P.st.state == PLAYER_STOPPED)
        return noErr;
    if (fraction_x1000 > 1000)
        fraction_x1000 = 1000;
    ms = (unsigned long)((unsigned long long)P.st.duration_ms * fraction_x1000 / 1000);
    was_paused = P.st.state == PLAYER_PAUSED;
    err = restart_at_ms(ms, P.st.profile, was_paused);
    if (err != noErr) {
        P.st.last_error = err;
        stop_internal();
    }
    return err;
}

void player_set_quality(PlayerQuality q)
{
    if ((unsigned)q >= QUALITY_COUNT)
        return;
    P.st.quality = q;
    if (P.st.state != PLAYER_STOPPED && q != QUALITY_AUTO && P.ci.has_profiles) {
        DecProfile want = profile_for_quality(q);

        if (want != P.st.profile) {
            unsigned long pos = player_status()->position_ms;
            OSErr err = restart_at_ms(pos, want, P.st.state == PLAYER_PAUSED);

            if (err != noErr) {
                P.st.last_error = err;
                stop_internal();
            }
        }
    }
}

void player_set_output_bits(unsigned bits)
{
    unsigned want = (bits == 8 || bits == 16) ? bits : 0;

    if (want == P.bitsPref)
        return;
    P.bitsPref = want;
    if (P.st.state != PLAYER_STOPPED && P.audioOpen) {
        /* Reabrir la salida con el formato nuevo en la posición actual. */
        unsigned long pos = player_status()->position_ms;
        Boolean paused = P.st.state == PLAYER_PAUSED;

        audio_close();
        P.audioOpen = false;
        if (restart_at_ms(pos, P.st.profile, paused) != noErr)
            stop_internal();
    }
}

void player_set_volume(unsigned volume)
{
    P.volume = volume > 100 ? 100 : volume;
    apply_gain();
}

void player_set_balance(int balance)
{
    P.balance = balance < -100 ? -100 : (balance > 100 ? 100 : balance);
    apply_gain();
}

Boolean player_wants_cpu(void)
{
    return P.st.state != PLAYER_STOPPED && !P.draining && audio_space_frames() >= MAX_FRAME_FRAMES;
}

void player_idle(void)
{
    uint64_t start;

    if (!P.inited || P.st.state == PLAYER_STOPPED)
        return;

    /* Diagnóstico: mayor intervalo entre llamadas mientras suena. Si el gancho de menú
     * funciona, mantener un menú abierto no lo hace crecer. */
    if (P.st.state == PLAYER_PLAYING) {
        uint64_t now_us = mactime_usec();

        if (P.last_idle_us != 0) {
            unsigned long gap = (unsigned long)(now_us - P.last_idle_us) / 1000UL;   /* 32 bits: sin libgcc */

            if (gap > P.max_gap_ms)
                P.max_gap_ms = gap;
        }
        P.last_idle_us = now_us;
    } else {
        P.last_idle_us = 0;
    }

    if (P.draining) {
        if (audio_is_done()) {
            unsigned long dur = P.st.duration_ms;

            stop_internal();
            P.st.ended = true;
            P.base_ms = dur;
        }
        return;
    }

    start = mactime_usec();
    while (audio_space_frames() >= MAX_FRAME_FRAMES) {
        int rc = P.codec->next(&P.f);

        if (rc == 0) {
            P.draining = true;
            audio_finish();
            break;
        }
        if (rc < 0) {
            P.st.last_error = ioErr;
            P.draining = true;
            audio_finish();
            break;
        }
        if (put_frame(&P.f) != 0)
            break;
        P.st.frames_decoded++;
        /* Pasado el tope solo se sigue si el anillo está casi vacío (< 250 ms). */
        if (mactime_usec() - start > IDLE_BUDGET_US &&
            audio_buffered_frames() > (long)(P.st.out_rate / 4))
            break;
    }

    /* Modo automático: si hay cortes repetidos, bajar un perfil en la posición actual. */
    if (P.ci.has_profiles && P.st.quality == QUALITY_AUTO && P.st.state == PLAYER_PLAYING && !P.draining) {
        unsigned long now = TickCount();
        unsigned long u = audio_underruns();

        if (now - P.under_tick > DEGRADE_WINDOW_TICKS) {
            P.under_base = u;
            P.under_tick = now;
        } else if (u - P.under_base >= DEGRADE_UNDERRUNS) {
            DecProfile n = next_cheaper(P.st.profile);

            if (n != P.st.profile) {
                unsigned long pos = player_status()->position_ms;

                if (restart_at_ms(pos, n, false) == noErr)
                    P.st.auto_degraded = true;
                else
                    stop_internal();
            } else {
                P.under_base = u;       /* ya está en el más barato */
                P.under_tick = now;
            }
        }
    }
}

const PlayerStatus *player_status(void)
{
    PlayerStatus *s = &P.st;

    if (s->state != PLAYER_STOPPED && P.audioOpen && s->out_rate) {
        unsigned long long played = audio_frames_played();

        s->position_ms = P.base_ms + (unsigned long)(played * 1000ULL / s->out_rate);
        s->buffered_ms = (long)((unsigned long long)audio_buffered_frames() * 1000ULL / s->out_rate);
    } else {
        s->position_ms = s->ended ? s->duration_ms : (s->state == PLAYER_STOPPED ? 0 : P.base_ms);
        s->buffered_ms = 0;
    }
    if (s->duration_ms && s->position_ms > s->duration_ms)
        s->position_ms = s->duration_ms;
    s->underruns = audio_underruns() - P.under_total_base;
    s->max_gap_ms = P.max_gap_ms;
    s->arena_peak = (unsigned long)codec_arena()->peak;
    return s;
}
