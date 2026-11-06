/*
 * audiotest — Etapa 5a de MP368K: prueba del camino de audio completo.
 *
 *   disco --FSRead--> decodificador en flujo --> anillo --> Sound Manager (doble buffer)
 *
 * Reproduce cada pista de la lista durante TEST_SECONDS (o hasta que termine) con un
 * perfil dado, imprime estadísticas una vez por segundo y escribe audiotest_results.txt.
 * Es una prueba de funcionamiento y de cortes (underruns), NO una medición de rendimiento
 * válida salvo que se ejecute en el LC 580.
 *
 * Qué hay que mirar: (1) que suene; (2) underruns en 0; (3) cuánto del tiempo de un frame
 * ocupa decodificarlo (decode_load); (4) el formato negociado (bits, versión del Sound Manager).
 */
#include <Gestalt.h>
#include <Events.h>
#include <Memory.h>
#include <Timer.h>

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "build_info.h"
#include "decoder_core.h"
#include "fileread.h"
#include "macenv.h"
#include "mactime.h"
#include "memblock.h"
#include "textlog.h"

#define TEST_SECONDS   15
#define PREROLL_MS     250
#define READ_BUF_BYTES (32L * 1024)
#define EXTRA_STACK    (48L * 1024)
#define MIN_STACK      (24L * 1024)
#define OUT_NAME       "audiotest_results.txt"

typedef struct {
    const char *file;
    DecProfile profile;
    unsigned want_bits;     /* 0 = automático, 8 o 16 */
} Track;

static const Track kTracks[] = {
    { "test128.mp3", DEC_PROFILE_FULL, 0 },
    { "test128.mp3", DEC_PROFILE_HALF, 0 },
    { "test192.mp3", DEC_PROFILE_FULL, 0 },
    { "testmono.mp3", DEC_PROFILE_FULL, 0 },
    { "test128.mp3", DEC_PROFILE_HALF, 8 },     /* fuerza la ruta de 8 bits */
};
#define NTRACKS (sizeof kTracks / sizeof kTracks[0])

static long gestalt_val(unsigned long sel)
{
    long v = -1;
    if (Gestalt(sel, (LONGINT *)&v) != noErr)
        v = -1;
    return v;
}

/* Escribe todo el PCM de un frame en el anillo (hay lugar: se comprobó antes). */
static int write_frame(const DecFrame *f)
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

static void play_track(const Track *t)
{
    FileReader fr;
    MemBlock rbuf;
    DecFrame f;
    AudioInfo ai;
    OSErr err;
    int rc;
    unsigned long frames_decoded = 0, dec_sum_us = 0, dec_max_us = 0;
    unsigned long frame_us, last_tick, now_tick, start_tick;
    unsigned long preroll_us, preroll_audio_ms;
    unsigned long final_underruns = 0, final_played_ms = 0;
    uint64_t t0;
    int opened_audio = 0;

    textlog_printf("\n=== %s perfil %s bits %s\n", t->file, dec_profile_name(t->profile),
                   t->want_bits ? (t->want_bits == 8 ? "8 (forzado)" : "16 (forzado)") : "auto");

    err = fileread_open_app_file(&fr, t->file);
    if (err != noErr) {
        textlog_printf("# no se pudo abrir (err=%d), se omite\n", err);
        return;
    }
    err = memblock_alloc(&rbuf, READ_BUF_BYTES);
    if (err != noErr) {
        textlog_printf("# sin memoria para el buffer de lectura (err=%d)\n", err);
        fileread_close(&fr);
        return;
    }
    if (dec_stream_open(t->profile, fileread_read, fileread_seek, &fr, (uint8_t *)rbuf.data,
                        (size_t)rbuf.size) != 0) {
        textlog_printf("# dec_stream_open fallo\n");
        goto out;
    }
    if (dec_stream_next(&f) != 1) {
        textlog_printf("# no se pudo decodificar el primer frame\n");
        goto out;
    }
    textlog_printf("archivo: %ld bytes, %u kbps, %u Hz, %u canales; salida %u Hz, %u canales\n", fr.size,
                   f.bitrate / 1000, f.in_rate, f.in_channels, f.rate, f.channels);

    err = audio_open(f.rate, f.channels, t->want_bits, 0, &ai);
    if (err != noErr) {
        textlog_printf("# audio_open fallo: err=%d%s\n", err,
                       err == AUDIO_ERR_NO_DOUBLEBUFFER ? " (Sound Manager < 3.0)" : "");
        goto out;
    }
    opened_audio = 1;
    textlog_printf("audio: Sound Manager %u.%u, %u Hz, %u canales, %u bits (hardware ofrece: %s%s%s), "
                   "anillo %lu bytes, buffer %u frames\n",
                   ai.sm_major, ai.sm_minor, ai.rate, ai.channels, ai.bits,
                   ai.hw_sizes == 0 ? "desconocido" : "", (ai.hw_sizes & 1) ? "8 " : "",
                   (ai.hw_sizes & 2) ? "16" : "", ai.ring_bytes, ai.buf_frames);

    frame_us = (unsigned long)((unsigned long long)f.frames * 1000000ULL / f.rate);

    /* Pre-roll: llenar PREROLL_MS de audio y medir cuánto cuesta producirlo. */
    t0 = mactime_usec();
    write_frame(&f);
    frames_decoded = 1;
    preroll_audio_ms = 0;
    while (audio_buffered_frames() < (long)((unsigned long)ai.rate * PREROLL_MS / 1000)) {
        rc = dec_stream_next(&f);
        if (rc != 1)
            break;
        write_frame(&f);
        frames_decoded++;
    }
    preroll_us = (unsigned long)(mactime_usec() - t0);
    preroll_audio_ms = (unsigned long)((unsigned long long)audio_buffered_frames() * 1000ULL / ai.rate);
    textlog_printf("preroll: %lu ms de audio en %lu us (RTF %lu.%02lu)\n", preroll_audio_ms, preroll_us,
                   preroll_us ? (unsigned long)(preroll_audio_ms * 1000ULL / preroll_us) : 0,
                   preroll_us ? (unsigned long)(preroll_audio_ms * 100000ULL / preroll_us % 100) : 0);

    err = audio_start();
    if (err != noErr) {
        textlog_printf("# audio_start fallo: err=%d\n", err);
        goto out;
    }

    start_tick = last_tick = TickCount();
    for (;;) {
        now_tick = TickCount();

        if (audio_space_frames() >= 1152) {
            uint64_t d0 = mactime_usec();
            unsigned long dt;

            rc = dec_stream_next(&f);
            dt = (unsigned long)(mactime_usec() - d0);
            if (rc == 0) {
                textlog_printf("fin del archivo\n");
                audio_finish();
                break;
            }
            if (rc < 0) {
                textlog_printf("# error fatal de decodificacion\n");
                audio_finish();
                break;
            }
            dec_sum_us += dt;
            if (dt > dec_max_us)
                dec_max_us = dt;
            frames_decoded++;
            if (write_frame(&f) != 0) {
                textlog_printf("# el anillo no acepta datos\n");
                break;
            }
        } else {
            long dummy;
            Delay(1, &dummy);               /* cede ~16 ms mientras el anillo está lleno */
        }

        if (now_tick - last_tick >= 60) {
            last_tick = now_tick;
            textlog_printf("t=%3lus sonado=%5lums buffer=%4ldms underruns=%lu frames=%lu\n",
                           (now_tick - start_tick) / 60,
                           (unsigned long)((unsigned long long)audio_frames_played() * 1000ULL / ai.rate),
                           (long)((unsigned long long)audio_buffered_frames() * 1000ULL / ai.rate),
                           audio_underruns(), frames_decoded);
        }
        if ((now_tick - start_tick) / 60 >= TEST_SECONDS)
            break;
    }

    /*
     * Cifras de la reproducción, tomadas ANTES de drenar: al vaciarse el anillo el
     * callback entrega silencio y eso no es un corte (un error de la primera versión de
     * esta prueba contaba ese silencio como underruns).
     */
    final_underruns = audio_underruns();
    final_played_ms = (unsigned long)((unsigned long long)audio_frames_played() * 1000ULL / ai.rate);
    audio_finish();

    if (audio_is_done() == 0) {
        /* Esperar a que se vacíe el anillo (máx. ~2,5 s). */
        unsigned long wait_start = TickCount();
        while (!audio_is_done() && TickCount() - wait_start < 150) {
            long dummy;
            Delay(2, &dummy);
        }
    }

    textlog_printf("RESULTADO: underruns=%lu sonado=%lums frames=%lu errores_mp3=%lu\n", final_underruns,
                   final_played_ms, frames_decoded, dec_stream_errors());
    if (frames_decoded > 1)
        textlog_printf("decode_load: promedio %lu us/frame, maximo %lu us, frame dura %lu us (carga %lu%%)\n",
                       dec_sum_us / (frames_decoded > 1 ? frames_decoded - 1 : 1), dec_max_us, frame_us,
                       frame_us ? (dec_sum_us / (frames_decoded - 1)) * 100 / frame_us : 0);

out:
    if (opened_audio)
        audio_close();
    dec_stream_close();
    memblock_free(&rbuf);
    fileread_close(&fr);
}

int main(void)
{
    long stack;
    unsigned i;
    OSErr err;

    stack = macenv_grow_stack(EXTRA_STACK);

    textlog_init();
    textlog_printf("# MP368K audiotest (etapa 5a)\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__, MP368K_BUILD_FLAGS);
    textlog_printf("machine: mach=%ld proc=%ld fpu=%ld sysv=0x%lX ram_kb=%ld\n",
                   gestalt_val(gestaltMachineType), gestalt_val(gestaltProcessorType),
                   gestalt_val(gestaltFPUType), (unsigned long)gestalt_val(gestaltSystemVersion),
                   gestalt_val(gestaltPhysicalRAMSize) / 1024);
    textlog_printf("timer_resolution_usec: %lu\n", mactime_resolution_usec());
    textlog_printf("stack_space: %ld bytes\n", stack);
    textlog_printf("Cada pista suena %d s. Escuche y anote si hay cortes, ruido o tono incorrecto.\n",
                   TEST_SECONDS);

    if (stack < MIN_STACK) {
        textlog_printf("ABORTO: pila insuficiente (%ld < %ld)\n", stack, MIN_STACK);
    } else {
        for (i = 0; i < NTRACKS; i++) {
            play_track(&kTracks[i]);
            textlog_save_next_to_app(OUT_NAME);         /* parcial, por si hay un cuelgue */
        }
    }

    err = textlog_save_next_to_app(OUT_NAME);
    textlog_printf("\n%s guardado: err=%d%s\n", OUT_NAME, err, textlog_truncated() ? " (TRUNCADO)" : "");
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
