/*
 * decbench — Etapa 2 de MP368K: RTF base de libmad en C puro, sin audio.
 *
 * Para cada MP3 presente junto a la aplicación (test128.mp3, test192.mp3,
 * testmono.mp3) y cada perfil aplicable decodifica FRAMES_PER_RUN frames desde
 * memoria, RUNS veces (la primera, en frío, se informa aparte), y calcula el RTF
 * y los ciclos por muestra de salida (suponiendo 33 MHz, sin medir en el equipo).
 *
 * Una pasada extra, FUERA del cronometraje, calcula el CRC-32 del PCM. Si junto
 * a la aplicación está REFERENCE.txt (generado por tools/host-ref.sh) se compara
 * con el CRC del PC: coincidir significa que el 68k decodifica bit a bit igual.
 * Escribe bench_results_decoder.txt (formato de mac-test-workflow).
 */
#include <Gestalt.h>
#include <Memory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "appfile.h"
#include "build_info.h"
#include "decoder_core.h"
#include "macenv.h"
#include "mactime.h"
#include "refcrc.h"
#include "textlog.h"

#define ASSUMED_MHZ     33          /* LC 580: dato del usuario, por verificar */
#define RUNS            6           /* 1 pasada fría + 5 para estadística */
#define SLOW_RUNS       3           /* si la pasada fría pasa de SLOW_RUN_US */
#define SLOW_RUN_US     40000000ULL
#define FRAMES_PER_RUN  300UL
#define EXTRA_STACK     (48L * 1024)
#define MIN_STACK       (24L * 1024)
#define OUT_NAME        "bench_results_decoder.txt"
#define REF_NAME        "REFERENCE.txt"   /* solo para el mensaje */

static const char *const kFiles[] = { "test128.mp3", "test192.mp3", "testmono.mp3" };
#define NFILES (sizeof kFiles / sizeof kFiles[0])

typedef struct {
    char file[16];
    DecProfile profile;
    int valid;
    uint64_t min, med;
    unsigned long frames, samples_per_ch;
    unsigned channels, out_rate;
} Row;

#define MAX_ROWS (NFILES * DEC_PROFILE_COUNT)
static Row gRows[MAX_ROWS];
static int gRowCount;

static long gestalt_val(unsigned long sel)
{
    long v = -1;
    if (Gestalt(sel, (LONGINT *)&v) != noErr)
        v = -1;
    return v;
}

static void sort_u64(uint64_t *a, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        uint64_t t = a[i];
        for (j = i; j > 0 && a[j - 1] > t; j--)
            a[j] = a[j - 1];
        a[j] = t;
    }
}

static void bench_profile(const char *name, const FileBlob *f, unsigned long file_crc,
                          DecProfile profile, const DecResult *probe)
{
    uint64_t us[RUNS], sorted[RUNS - 1];
    DecConfig cfg;
    DecResult res;
    Row *row = &gRows[gRowCount];
    int runs = RUNS, r, n;
    unsigned long ref_pcm, ref_file;

    memset(&cfg, 0, sizeof cfg);
    cfg.profile = profile;
    cfg.max_frames = FRAMES_PER_RUN;

    textlog_printf("\ntest: decode\nfile: %s %u kbps %u canales\nprofile: %s\n"
                   "run,usec_total,frames,samples_out\n",
                   name, probe->bitrate / 1000, probe->in_channels, dec_profile_name(profile));

    memset(&res, 0, sizeof res);
    for (r = 0; r < runs; r++) {
        uint64_t t0 = mactime_usec();
        dec_run((const uint8_t *)f->data, (size_t)f->size, &cfg, &res);
        us[r] = mactime_usec() - t0;
        textlog_printf("%d,%lu,%lu,%lu\n", r + 1, (unsigned long)us[r], res.frames,
                       res.samples_per_ch * res.channels);
        if (r == 0 && us[0] > SLOW_RUN_US) {
            runs = SLOW_RUNS;
            textlog_printf("# pasada fria muy lenta: se reducen las corridas a %d\n", runs);
        }
    }

    /* Pasada de verificación, fuera de la medición. */
    cfg.want_crc = 1;
    dec_run((const uint8_t *)f->data, (size_t)f->size, &cfg, &res);
    if (refcrc_find("bench", name, dec_profile_name(profile), &ref_pcm, &ref_file))
        textlog_printf("pcm_crc32: %08lX referencia_pc=%08lX -> %s\n", (unsigned long)res.crc,
                       ref_pcm, res.crc == ref_pcm && file_crc == ref_file ? "COINCIDE" : "DIFIERE");
    else
        textlog_printf("pcm_crc32: %08lX (sin referencia)\n", (unsigned long)res.crc);
    if (res.errors != 0 || res.fatal)
        textlog_printf("# AVISO: errores de libmad=%lu fatal=%d\n", res.errors, res.fatal);

    n = runs - 1;                           /* la pasada 1 (fría) no entra */
    memcpy(sorted, us + 1, n * sizeof sorted[0]);
    sort_u64(sorted, n);

    snprintf(row->file, sizeof row->file, "%s", name);
    row->profile = profile;
    row->valid = 1;
    row->min = sorted[0];
    row->med = sorted[(n - 1) / 2];
    row->frames = res.frames;
    row->samples_per_ch = res.samples_per_ch;
    row->channels = res.channels;
    row->out_rate = res.out_rate;
    gRowCount++;
}

static void print_x1000(uint64_t v)
{
    textlog_printf("%lu.%03lu", (unsigned long)(v / 1000), (unsigned long)(v % 1000));
}

static void print_summary(void)
{
    int i;

    textlog_printf("\n# summary (min y mediana de las pasadas 2..; %lu frames por corrida; ciclos suponen %d MHz)\n",
                   FRAMES_PER_RUN, ASSUMED_MHZ);
    textlog_printf("# rtf = (muestras por canal / tasa de salida) / tiempo; 1.000 es el limite, bajo 1 hay cortes\n");
    textlog_printf("# presupuesto de SPECS 3: ~250 ciclos por muestra de salida para RTF 1,5\n");
    textlog_printf("summary,file,profile,min_usec,med_usec,frames,samples_per_ch,channels,out_rate,rtf_min,rtf_med,cycles_per_out_sample_min\n");

    for (i = 0; i < gRowCount; i++) {
        const Row *r = &gRows[i];
        uint64_t out_samples = (uint64_t)r->samples_per_ch * r->channels;

        textlog_printf("summary,%s,%s,%lu,%lu,%lu,%lu,%u,%u,", r->file,
                       dec_profile_name(r->profile), (unsigned long)r->min,
                       (unsigned long)r->med, r->frames, r->samples_per_ch,
                       r->channels, r->out_rate);
        if (r->min > 0 && r->med > 0 && r->out_rate > 0 && out_samples > 0) {
            uint64_t base = (uint64_t)r->samples_per_ch * 1000000000ULL;
            print_x1000(base / ((uint64_t)r->out_rate * r->min));
            textlog_printf(",");
            print_x1000(base / ((uint64_t)r->out_rate * r->med));
            textlog_printf(",%lu.%lu\n",
                           (unsigned long)(r->min * ASSUMED_MHZ * 10 / out_samples / 10),
                           (unsigned long)(r->min * ASSUMED_MHZ * 10 / out_samples % 10));
        } else
            textlog_printf("-,-,-\n");
    }
}

/* ------------------------------------------------------------------------ */

int main(void)
{
    long stack;
    unsigned i;
    OSErr err;

    stack = macenv_grow_stack(EXTRA_STACK);

    textlog_init();
    textlog_printf("# MP368K bench\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__, MP368K_BUILD_FLAGS);
    textlog_printf("library: %s\n", dec_library_info());
    textlog_printf("machine: mach=%ld proc=%ld fpu=%ld sysv=0x%lX ram_kb=%ld\n",
                   gestalt_val(gestaltMachineType), gestalt_val(gestaltProcessorType),
                   gestalt_val(gestaltFPUType), (unsigned long)gestalt_val(gestaltSystemVersion),
                   gestalt_val(gestaltPhysicalRAMSize) / 1024);
    textlog_printf("cache: no consultable desde la app; anotar a mano el ajuste del sistema\n");
    textlog_printf("assumed_clock_mhz: %d (supuesto, sin medir)\n", ASSUMED_MHZ);
    textlog_printf("timer_resolution_usec: %lu\n", mactime_resolution_usec());
    textlog_printf("stack_space: %ld bytes (tras reservar %ld extra)\n", stack, EXTRA_STACK);
    textlog_printf("frames_per_run: %lu  runs: %d (1 fria + %d)\n", FRAMES_PER_RUN, RUNS, RUNS - 1);

    if (stack < MIN_STACK) {
        textlog_printf("ABORTO: pila insuficiente (%ld < %ld); libmad necesita varios KB.\n", stack, MIN_STACK);
        textlog_save_next_to_app(OUT_NAME);
        printf("\n(Presione Return para salir)\n");
        getchar();
        return 1;
    }

    err = refcrc_load();
    textlog_printf("reference: %s\n", err == noErr ? REF_NAME " cargado" : "sin " REF_NAME " (no se compara con el PC)");

    for (i = 0; i < NFILES; i++) {
        FileBlob f;
        DecConfig pc;
        DecResult probe;
        unsigned long file_crc;
        int p;

        err = appfile_load(kFiles[i], &f);
        if (err != noErr) {
            textlog_printf("\n# %s: no se pudo cargar (err=%d), se omite\n", kFiles[i], err);
            continue;
        }

        memset(&pc, 0, sizeof pc);
        pc.profile = DEC_PROFILE_FULL;
        pc.max_frames = 1;
        if (dec_run((const uint8_t *)f.data, (size_t)f.size, &pc, &probe) != 0) {
            textlog_printf("\n# %s: no parece un MP3 valido, se omite\n", kFiles[i]);
            appfile_free(&f);
            continue;
        }
        file_crc = dec_crc32((const uint8_t *)f.data, (size_t)f.size);
        textlog_printf("\n# archivo %s: %ld bytes, crc32 %08lX, %u kbps, %u canales, %u Hz, layer %u\n",
                       kFiles[i], f.size, file_crc, probe.bitrate / 1000, probe.in_channels,
                       probe.in_rate, probe.layer);

        for (p = 0; p < DEC_PROFILE_COUNT; p++) {
            /* En un archivo mono, MONO == FULL y HALF_MONO == HALF: no se repiten. */
            if (probe.in_channels == 1 && (p == DEC_PROFILE_MONO || p == DEC_PROFILE_HALF_MONO))
                continue;
            bench_profile(kFiles[i], &f, file_crc, (DecProfile)p, &probe);
        }
        appfile_free(&f);

        /* Guardar resultados parciales por si hay que abortar una corrida larga. */
        textlog_save_next_to_app(OUT_NAME);
    }

    print_summary();
    err = textlog_save_next_to_app(OUT_NAME);
    textlog_printf("\n%s guardado: err=%d%s\n", OUT_NAME, err,
                   textlog_truncated() ? " (TRUNCADO)" : "");
    refcrc_free();
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
