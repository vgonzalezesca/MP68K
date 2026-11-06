/*
 * decprofile — Etapa 3 de MP368K: costo por etapa del decodificador, sin audio.
 *
 * Para cada combinación (archivo, perfil) corre el decodificador con libmad
 * parcheado (marcas de tiempo entre etapas de layer3.c) y también sin reloj,
 * para medir cuánto distorsionan las marcas. Reporta el reparto en % por etapa.
 *
 * Qué NO separa: Huffman y requantización van juntas (libmad requantiza dentro
 * de III_huffdecode) y la síntesis polifásica entera (dct32 + ventana) es una
 * sola cubeta. Escribe bench_results_profile.txt.
 */
#include <Gestalt.h>
#include <Memory.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "appfile.h"
#include "build_info.h"
#include "decoder_core.h"
#include "macenv.h"
#include "mactime.h"
#include "refcrc.h"
#include "textlog.h"

#define ASSUMED_MHZ     33
#define FRAMES_PER_RUN  700UL       /* 7 corridas x 700 = 4900 frames por combinación */
#define PLAIN_RUNS      3           /* sin instrumentos: 1 fría + 2 */
#define PROF_RUNS       4           /* con instrumentos: 1 fría + 3 */
#define SLOW_RUN_US     120000000ULL
#define EXTRA_STACK     (48L * 1024)
#define MIN_STACK       (24L * 1024)
#define OUT_NAME        "bench_results_profile.txt"

typedef struct { const char *file; DecProfile profile; } Combo;

static const Combo kCombos[] = {
    { "test192.mp3", DEC_PROFILE_FULL },
    { "test192.mp3", DEC_PROFILE_HALF },
    { "test128.mp3", DEC_PROFILE_FULL },
    { "testmono.mp3", DEC_PROFILE_FULL },
};
#define NCOMBOS (sizeof kCombos / sizeof kCombos[0])

static const char *const kBucketNames[PROF_COUNT] = {
    "side", "scalef", "huff_requant", "stereo", "reorder", "alias",
    "imdct_overlap", "other_decode", "monomix", "synth", "pcm"
};

typedef struct {
    int valid;
    uint64_t plain_min, prof_total_min;
    uint64_t bucket[PROF_COUNT];        /* de la corrida instrumentada más rápida */
    unsigned long frames, samples_per_ch, marks;
    unsigned channels, out_rate;
    char file[16];
    DecProfile profile;
} Row;

static Row gRows[NCOMBOS];
static int gRowCount;
static unsigned long gClockCostNs;      /* costo de una lectura del reloj, en ns */

static long gestalt_val(unsigned long sel)
{
    long v = -1;
    if (Gestalt(sel, (LONGINT *)&v) != noErr)
        v = -1;
    return v;
}

static unsigned long long clock_fn(void)
{
    return mactime_usec();
}

/* Costo medio de una lectura del reloj (ns). Es poco fiable si la resolución es gruesa. */
static unsigned long measure_clock_cost_ns(void)
{
    enum { N = 5000 };
    uint64_t t0, t1;
    int i;

    t0 = mactime_usec();
    for (i = 0; i < N; i++)
        (void) mactime_usec();
    t1 = mactime_usec();
    return (unsigned long)((t1 - t0) * 1000ULL / N);
}

static void run_combo(const Combo *c, const FileBlob *f, unsigned long file_crc,
                      const DecResult *probe)
{
    DecConfig cfg;
    DecResult res, best;
    uint64_t plain_min = 0, prof_min = 0;
    unsigned long ref_pcm, ref_file;
    int r, nplain = PLAIN_RUNS, nprof = PROF_RUNS, b;
    Row *row = &gRows[gRowCount];

    textlog_printf("\ntest: profile_stages\nfile: %s %u kbps %u canales\nprofile: %s\n",
                   c->file, probe->bitrate / 1000, probe->in_channels, dec_profile_name(c->profile));

    /* 1) sin reloj: tiempo real del decodificador */
    memset(&cfg, 0, sizeof cfg);
    cfg.profile = c->profile;
    cfg.max_frames = FRAMES_PER_RUN;
    textlog_printf("plain,run,usec_total,frames,samples_out\n");
    for (r = 0; r < nplain; r++) {
        uint64_t t0 = mactime_usec(), us;
        dec_run((const uint8_t *)f->data, (size_t)f->size, &cfg, &res);
        us = mactime_usec() - t0;
        textlog_printf("plain,%d,%lu,%lu,%lu\n", r + 1, (unsigned long)us, res.frames,
                       res.samples_per_ch * res.channels);
        if (r > 0 && (plain_min == 0 || us < plain_min))
            plain_min = us;
        if (r == 0 && us > SLOW_RUN_US) {
            nplain = 2;
            nprof = 2;
            textlog_printf("# pasada fria muy lenta: se reducen las corridas\n");
        }
    }

    /* 2) con reloj: reparto por etapa */
    cfg.clock = clock_fn;
    textlog_printf("prof,run,usec_total,frames,samples_out,marks,%s", kBucketNames[0]);
    for (b = 1; b < PROF_COUNT; b++)
        textlog_printf(",%s", kBucketNames[b]);
    textlog_printf("\n");
    memset(&best, 0, sizeof best);
    for (r = 0; r < nprof; r++) {
        uint64_t t0 = mactime_usec(), us;
        dec_run((const uint8_t *)f->data, (size_t)f->size, &cfg, &res);
        us = mactime_usec() - t0;
        textlog_printf("prof,%d,%lu,%lu,%lu,%lu", r + 1, (unsigned long)us, res.frames,
                       res.samples_per_ch * res.channels, res.prof_marks);
        for (b = 0; b < PROF_COUNT; b++)
            textlog_printf(",%lu", (unsigned long)res.prof_us[b]);
        textlog_printf("\n");
        if (r > 0 && (prof_min == 0 || us < prof_min)) {
            prof_min = us;
            best = res;
        }
    }

    /* 3) verificación bit a bit, fuera de la medición y sin reloj */
    cfg.clock = NULL;
    cfg.want_crc = 1;
    dec_run((const uint8_t *)f->data, (size_t)f->size, &cfg, &res);
    if (refcrc_find("prof", c->file, dec_profile_name(c->profile), &ref_pcm, &ref_file))
        textlog_printf("pcm_crc32: %08lX referencia_pc=%08lX -> %s\n", (unsigned long)res.crc, ref_pcm,
                       res.crc == ref_pcm && file_crc == ref_file ? "COINCIDE" : "DIFIERE");
    else
        textlog_printf("pcm_crc32: %08lX (sin referencia)\n", (unsigned long)res.crc);

    snprintf(row->file, sizeof row->file, "%s", c->file);
    row->profile = c->profile;
    row->valid = 1;
    row->plain_min = plain_min;
    row->prof_total_min = prof_min;
    for (b = 0; b < PROF_COUNT; b++)
        row->bucket[b] = best.prof_us[b];
    row->frames = best.frames;
    row->samples_per_ch = best.samples_per_ch;
    row->marks = best.prof_marks;
    row->channels = best.channels;
    row->out_rate = best.out_rate;
    gRowCount++;
}

static void print_pct(uint64_t part, uint64_t total)
{
    uint64_t x10 = total ? part * 1000ULL / total : 0;
    textlog_printf("%lu.%lu", (unsigned long)(x10 / 10), (unsigned long)(x10 % 10));
}

static void print_summary(void)
{
    int i, b;

    textlog_printf("\n# summary: reparto en %% del tiempo de la corrida instrumentada mas rapida\n");
    textlog_printf("# huff_requant y synth son cubetas unicas (libmad no permite separarlas sin mas cambios)\n");
    textlog_printf("# overhead = (tiempo instrumentado - tiempo sin instrumentos) / sin instrumentos\n");
    textlog_printf("# clock_cost_ns = %lu (poco fiable si timer_resolution_usec es grueso)\n", gClockCostNs);
    textlog_printf("summary,file,profile,frames,plain_min_usec,prof_min_usec,overhead_pct,marks_per_frame,rtf_plain");
    for (b = 0; b < PROF_COUNT; b++)
        textlog_printf(",%s_pct", kBucketNames[b]);
    textlog_printf("\n");

    for (i = 0; i < gRowCount; i++) {
        const Row *r = &gRows[i];
        uint64_t sum = 0;

        for (b = 0; b < PROF_COUNT; b++)
            sum += r->bucket[b];
        textlog_printf("summary,%s,%s,%lu,%lu,%lu,", r->file, dec_profile_name(r->profile),
                       r->frames, (unsigned long)r->plain_min, (unsigned long)r->prof_total_min);
        if (r->plain_min > 0 && r->prof_total_min > 0) {
            /* Con signo: negativo = la corrida instrumentada salio mas rapida (ruido). */
            if (r->prof_total_min >= r->plain_min)
                print_pct(r->prof_total_min - r->plain_min, r->plain_min);
            else {
                textlog_printf("-");
                print_pct(r->plain_min - r->prof_total_min, r->plain_min);
            }
            textlog_printf(",%lu,", r->frames ? r->marks / r->frames : 0);
        } else
            textlog_printf("-,%lu,", r->frames ? r->marks / r->frames : 0);
        if (r->plain_min > 0 && r->out_rate > 0)
            textlog_printf("%lu.%03lu",
                (unsigned long)((uint64_t)r->samples_per_ch * 1000000ULL / ((uint64_t)r->out_rate * r->plain_min)),
                (unsigned long)((uint64_t)r->samples_per_ch * 1000000000ULL / ((uint64_t)r->out_rate * r->plain_min) % 1000));
        else
            textlog_printf("-");
        for (b = 0; b < PROF_COUNT; b++) {
            textlog_printf(",");
            print_pct(r->bucket[b], sum);
        }
        textlog_printf("\n");
    }
}

int main(void)
{
    long stack;
    unsigned i;
    OSErr err;

    stack = macenv_grow_stack(EXTRA_STACK);

    textlog_init();
    textlog_printf("# MP368K bench\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__, MP368K_BUILD_FLAGS);
    textlog_printf("library: %s + marcas de perfilado (parche third_party/patches)\n", dec_library_info());
    textlog_printf("machine: mach=%ld proc=%ld fpu=%ld sysv=0x%lX ram_kb=%ld\n",
                   gestalt_val(gestaltMachineType), gestalt_val(gestaltProcessorType),
                   gestalt_val(gestaltFPUType), (unsigned long)gestalt_val(gestaltSystemVersion),
                   gestalt_val(gestaltPhysicalRAMSize) / 1024);
    textlog_printf("cache: no consultable desde la app; anotar a mano el ajuste del sistema\n");
    textlog_printf("assumed_clock_mhz: %d (supuesto, sin medir)\n", ASSUMED_MHZ);
    textlog_printf("timer_resolution_usec: %lu\n", mactime_resolution_usec());
    gClockCostNs = measure_clock_cost_ns();
    textlog_printf("clock_cost_ns: %lu\n", gClockCostNs);
    textlog_printf("stack_space: %ld bytes (tras reservar %ld extra)\n", stack, EXTRA_STACK);
    textlog_printf("frames_per_run: %lu  runs: plain %d, prof %d (la primera de cada tipo es fria)\n",
                   FRAMES_PER_RUN, PLAIN_RUNS, PROF_RUNS);

    if (stack < MIN_STACK) {
        textlog_printf("ABORTO: pila insuficiente (%ld < %ld)\n", stack, MIN_STACK);
        textlog_save_next_to_app(OUT_NAME);
        printf("\n(Presione Return para salir)\n");
        getchar();
        return 1;
    }

    err = refcrc_load();
    textlog_printf("reference: %s\n", err == noErr ? "REFERENCE.txt cargado" : "sin REFERENCE.txt");

    for (i = 0; i < NCOMBOS; i++) {
        FileBlob f;
        DecConfig pc;
        DecResult probe;
        unsigned long file_crc;

        err = appfile_load(kCombos[i].file, &f);
        if (err != noErr) {
            textlog_printf("\n# %s: no se pudo cargar (err=%d), se omite\n", kCombos[i].file, err);
            continue;
        }
        memset(&pc, 0, sizeof pc);
        pc.profile = DEC_PROFILE_FULL;
        pc.max_frames = 1;
        if (dec_run((const uint8_t *)f.data, (size_t)f.size, &pc, &probe) != 0) {
            textlog_printf("\n# %s: no parece un MP3 valido, se omite\n", kCombos[i].file);
            appfile_free(&f);
            continue;
        }
        file_crc = dec_crc32((const uint8_t *)f.data, (size_t)f.size);
        textlog_printf("\n# archivo %s: %ld bytes, crc32 %08lX\n", kCombos[i].file, f.size, file_crc);
        run_combo(&kCombos[i], &f, file_crc, &probe);
        appfile_free(&f);
        textlog_save_next_to_app(OUT_NAME);
    }

    print_summary();
    err = textlog_save_next_to_app(OUT_NAME);
    textlog_printf("\n%s guardado: err=%d%s\n", OUT_NAME, err, textlog_truncated() ? " (TRUNCADO)" : "");
    refcrc_free();
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
