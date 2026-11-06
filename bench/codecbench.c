/*
 * codecbench — velocidad y exactitud de cada códec (MP3, FLAC, ...), sin audio.
 *
 * Para cada archivo de prueba junto a la aplicación decodifica los primeros DECODE_SECONDS segundos
 * leyendo desde disco (como el reproductor), RUNS veces (la primera, en frío, se informa aparte), y
 * calcula el RTF y los ciclos por muestra de salida (suponiendo 33 MHz, sin medir en el equipo). El
 * tiempo incluye la lectura con FSRead, igual que al reproducir.
 *
 * Una pasada extra, FUERA del cronometraje, calcula el CRC-32 del PCM (big-endian por muestra). Si
 * junto a la aplicación está REFERENCE_CODECS.txt (tools/host-codec-ref.sh) se compara con el CRC
 * del PC: coincidir significa que el 68k decodifica bit a bit igual que el PC.
 *
 * Escribe bench_results_codecs.txt (formato de mac-test-workflow).
 */
#include <Gestalt.h>
#include <Memory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "appfile.h"
#include "build_info.h"
#include "codec.h"
#include "fileread.h"
#include "macenv.h"
#include "mactime.h"
#include "memblock.h"
#include "textlog.h"

#define ASSUMED_MHZ     33          /* LC 580: dato del usuario, por verificar */
#define DECODE_SECONDS  12UL        /* segundos de audio por corrida */
#define RUNS            4           /* 1 pasada fría + 3 para estadística */
#define SLOW_RUN_US     60000000ULL /* si la pasada fría pasa de esto, se reducen las corridas */
#define EXTRA_STACK     (48L * 1024)
#define MIN_STACK       (24L * 1024)
#define ARENA_BYTES     (256L * 1024)
#define BUF_BYTES       (32L * 1024)
#define OUT_NAME        "bench_results_codecs.txt"
#define REF_NAME        "REFERENCE_CODECS.txt"

static const char *const kFiles[] = {
    "test128.mp3", "test192.mp3", "testmono.mp3",
    "testflac.flac", "testflacmono.flac", "testflac24.flac", "testflac8.flac",
};
#define NFILES (sizeof kFiles / sizeof kFiles[0])

typedef struct {
    char file[24];
    char codec[8];
    unsigned long frames;
    unsigned channels, rate;
    uint64_t min, med, cold;
    unsigned long crc, ref_crc;
    int has_ref, match;
} Row;

static Row gRows[NFILES];
static int gRowCount;
static FileBlob gRef;
static int gRefLoaded;
static unsigned long gCrcTable[256];

static long gestalt_val(unsigned long sel)
{
    long v = -1;

    if (Gestalt(sel, (LONGINT *)&v) != noErr)
        v = -1;
    return v;
}

static void crc_init(void)
{
    unsigned long i, k, c;

    for (i = 0; i < 256; i++) {
        c = i;
        for (k = 0; k < 8; k++)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
        gCrcTable[i] = c;
    }
}

static unsigned long crc_pcm(unsigned long crc, const int16_t *pcm, unsigned count)
{
    unsigned i;

    for (i = 0; i < count; i++) {
        uint16_t s = (uint16_t)pcm[i];

        crc = gCrcTable[(crc ^ (s >> 8)) & 0xFF] ^ (crc >> 8);          /* big-endian: byte alto primero */
        crc = gCrcTable[(crc ^ (s & 0xFF)) & 0xFF] ^ (crc >> 8);
    }
    return crc;
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

/* Busca "codec,<archivo>,frames,canales,tasa,crc" en REFERENCE_CODECS.txt. 1 si la encuentra. */
static int ref_find(const char *file, unsigned long frames, unsigned long *crc)
{
    const char *p = gRef.data, *end;
    size_t flen = strlen(file);

    if (!gRefLoaded)
        return 0;
    end = p + gRef.size;
    while (p < end) {
        const char *line = p, *eol = p;
        unsigned long fr = 0, c = 0;
        const char *q;
        int field = 0;

        while (eol < end && *eol != '\n' && *eol != '\r')
            eol++;
        if (line < eol && *line != '#' && (size_t)(eol - line) > 6 && strncmp(line, "codec,", 6) == 0 &&
            strncmp(line + 6, file, flen) == 0 && line[6 + flen] == ',') {
            q = line + 6 + flen + 1;
            while (q < eol) {
                const char *comma = q;
                char tmp[16];
                size_t n;

                while (comma < eol && *comma != ',')
                    comma++;
                n = (size_t)(comma - q);
                if (n >= sizeof tmp)
                    n = sizeof tmp - 1;
                memcpy(tmp, q, n);
                tmp[n] = '\0';
                if (field == 0)
                    fr = strtoul(tmp, NULL, 10);
                else if (field == 3)
                    c = strtoul(tmp, NULL, 16);
                field++;
                q = comma + 1;
            }
            if (fr == frames) {
                *crc = c;
                return 1;
            }
        }
        p = eol;
        while (p < end && (*p == '\n' || *p == '\r'))
            p++;
    }
    return 0;
}

/* Decodifica hasta `limit` cuadros por canal (0 = todo). Devuelve los cuadros hechos; si crc != NULL, lo calcula. */
static unsigned long decode_run(const Codec *c, CodecIO *io, uint8_t *buf, const CodecInfo *ci,
                                unsigned long limit, unsigned long *crc, unsigned *channels, unsigned *rate)
{
    unsigned long frames = 0;
    unsigned long acc = 0xFFFFFFFFUL;

    if (c->open(io, DEC_PROFILE_FULL, buf, (size_t)BUF_BYTES, ci) != CODEC_OK)
        return 0;
    for (;;) {
        DecFrame f;
        int rc = c->next(&f);

        if (rc != 1)
            break;
        *channels = f.channels;
        *rate = f.rate;
        if (limit && frames + f.frames > limit) {
            f.frames = (unsigned)(limit - frames);
            f.count = f.frames * f.channels;
        }
        if (crc != NULL)
            acc = crc_pcm(acc, f.pcm, f.count);
        frames += f.frames;
        if (limit && frames >= limit)
            break;
    }
    c->close();
    if (crc != NULL)
        *crc = acc ^ 0xFFFFFFFFUL;
    return frames;
}

static void bench_file(const char *name, uint8_t *buf)
{
    FileReader fr;
    CodecIO io;
    CodecInfo ci;
    const Codec *c;
    int err = 0, r, runs = RUNS, n;
    uint64_t us[RUNS], sorted[RUNS - 1];
    unsigned long frames = 0, limit, crc = 0, ref = 0;
    unsigned channels = 0, rate = 0;
    Row *row = &gRows[gRowCount];
    OSErr oe;

    oe = fileread_open_app_file(&fr, name);
    if (oe != noErr) {
        textlog_printf("\n# %s: no se pudo abrir (err=%d), se omite\n", name, oe);
        return;
    }
    io.read = fileread_read;
    io.seek = fileread_seek;
    io.ctx = &fr;
    io.size = fr.size;
    c = codec_probe_any(&io, buf, BUF_BYTES, &ci, &err);
    if (c == NULL) {
        textlog_printf("\n# %s: formato no reconocido o no admitido (codigo %d), se omite\n", name, err);
        fileread_close(&fr);
        return;
    }
    limit = (unsigned long)ci.rate * DECODE_SECONDS;
    if (ci.duration_ms && (unsigned long long)ci.duration_ms * ci.rate / 1000ULL < limit)
        limit = (unsigned long)((unsigned long long)ci.duration_ms * ci.rate / 1000ULL);

    textlog_printf("\ntest: decode\nfile: %s %s %u kbps %u canales %u Hz %u bits\nprofile: FULL\n"
                   "run,usec_total,frames\n",
                   name, ci.format, ci.bitrate / 1000, ci.channels, ci.rate, ci.bits_src);

    for (r = 0; r < runs; r++) {
        uint64_t t0 = mactime_usec();

        frames = decode_run(c, &io, buf, &ci, limit, NULL, &channels, &rate);
        us[r] = mactime_usec() - t0;
        textlog_printf("%d,%lu,%lu\n", r + 1, (unsigned long)us[r], frames);
        if (r == 0 && us[0] > SLOW_RUN_US) {
            runs = 2;
            textlog_printf("# pasada fria muy lenta: se reducen las corridas a %d\n", runs);
        }
    }

    /* Pasada de verificación, fuera de la medición. */
    frames = decode_run(c, &io, buf, &ci, limit, &crc, &channels, &rate);
    row->has_ref = ref_find(name, frames, &ref);
    row->match = row->has_ref && ref == crc;
    if (row->has_ref)
        textlog_printf("pcm_crc32: %08lX referencia_pc=%08lX -> %s\n", crc, ref, row->match ? "COINCIDE" : "DIFIERE");
    else
        textlog_printf("pcm_crc32: %08lX (sin referencia para %lu cuadros)\n", crc, frames);

    n = runs - 1;                           /* la pasada 1 (fría) no entra */
    memcpy(sorted, us + 1, (size_t)n * sizeof sorted[0]);
    sort_u64(sorted, n);

    snprintf(row->file, sizeof row->file, "%s", name);
    snprintf(row->codec, sizeof row->codec, "%s", ci.format);
    row->frames = frames;
    row->channels = channels;
    row->rate = rate;
    row->cold = us[0];
    row->min = sorted[0];
    row->med = sorted[(n - 1) / 2];
    row->crc = crc;
    row->ref_crc = ref;
    gRowCount++;
    fileread_close(&fr);
    textlog_save_next_to_app(OUT_NAME);
}

static void print_x1000(uint64_t v)
{
    textlog_printf("%lu.%03lu", (unsigned long)(v / 1000), (unsigned long)(v % 1000));
}

static void print_summary(void)
{
    int i;

    textlog_printf("\n# summary (min y mediana de las pasadas 2..; %lu s de audio por corrida, con lectura de disco; ciclos suponen %d MHz)\n",
                   DECODE_SECONDS, ASSUMED_MHZ);
    textlog_printf("# rtf = (cuadros por canal / tasa de salida) / tiempo; 1.000 es el limite, bajo 1 hay cortes\n");
    textlog_printf("# presupuesto de SPECS 3: ~250 ciclos por muestra de salida para RTF 1,5\n");
    textlog_printf("summary,file,codec,cold_usec,min_usec,med_usec,frames,channels,out_rate,rtf_min,rtf_med,cycles_per_out_sample_min,pcm_crc,ref,match\n");

    for (i = 0; i < gRowCount; i++) {
        const Row *r = &gRows[i];
        uint64_t out_samples = (uint64_t)r->frames * r->channels;

        textlog_printf("summary,%s,%s,%lu,%lu,%lu,%lu,%u,%u,", r->file, r->codec, (unsigned long)r->cold,
                       (unsigned long)r->min, (unsigned long)r->med, r->frames, r->channels, r->rate);
        if (r->min > 0 && r->med > 0 && r->rate > 0 && out_samples > 0) {
            uint64_t base = (uint64_t)r->frames * 1000000000ULL;

            print_x1000(base / ((uint64_t)r->rate * r->min));
            textlog_printf(",");
            print_x1000(base / ((uint64_t)r->rate * r->med));
            textlog_printf(",%lu.%lu", (unsigned long)(r->min * ASSUMED_MHZ * 10 / out_samples / 10),
                           (unsigned long)(r->min * ASSUMED_MHZ * 10 / out_samples % 10));
        } else {
            textlog_printf("-,-,-");
        }
        textlog_printf(",%08lX,%s,%s\n", r->crc, r->has_ref ? "si" : "no", r->has_ref ? (r->match ? "COINCIDE" : "DIFIERE") : "-");
    }
}

int main(void)
{
    long stack;
    unsigned i;
    OSErr err;
    MemBlock arena, buf;

    stack = macenv_grow_stack(EXTRA_STACK);

    textlog_init();
    textlog_printf("# MP368K bench\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__, MP368K_BUILD_FLAGS);
    textlog_printf("machine: mach=%ld proc=%ld fpu=%ld sysv=0x%lX ram_kb=%ld\n",
                   gestalt_val(gestaltMachineType), gestalt_val(gestaltProcessorType),
                   gestalt_val(gestaltFPUType), (unsigned long)gestalt_val(gestaltSystemVersion),
                   gestalt_val(gestaltPhysicalRAMSize) / 1024);
    textlog_printf("cache: no consultable desde la app; anotar a mano el ajuste del sistema\n");
    textlog_printf("assumed_clock_mhz: %d (supuesto, sin medir)\n", ASSUMED_MHZ);
    textlog_printf("timer_resolution_usec: %lu\n", mactime_resolution_usec());
    textlog_printf("stack_space: %ld bytes (tras reservar %ld extra)\n", stack, EXTRA_STACK);
    textlog_printf("decode_seconds: %lu  runs: %d (1 fria + %d)\n", DECODE_SECONDS, RUNS, RUNS - 1);

    if (stack < MIN_STACK) {
        textlog_printf("ABORTO: pila insuficiente (%ld < %ld).\n", stack, MIN_STACK);
        textlog_save_next_to_app(OUT_NAME);
        printf("\n(Presione Return para salir)\n");
        getchar();
        return 1;
    }
    if (memblock_alloc(&arena, ARENA_BYTES) != noErr || memblock_alloc(&buf, BUF_BYTES) != noErr) {
        textlog_printf("ABORTO: sin memoria para el arena y el buffer.\n");
        textlog_save_next_to_app(OUT_NAME);
        printf("\n(Presione Return para salir)\n");
        getchar();
        return 1;
    }
    codec_init(arena.data, (size_t)arena.size);
    crc_init();

    err = appfile_load(REF_NAME, &gRef);
    gRefLoaded = err == noErr;
    textlog_printf("reference: %s\n", gRefLoaded ? REF_NAME " cargado" : "sin " REF_NAME " (no se compara con el PC)");

    for (i = 0; i < NFILES; i++)
        bench_file(kFiles[i], (uint8_t *)buf.data);

    print_summary();
    textlog_printf("arena_peak_bytes: %lu\n", (unsigned long)codec_arena()->peak);
    err = textlog_save_next_to_app(OUT_NAME);
    textlog_printf("\n%s guardado: err=%d%s\n", OUT_NAME, err, textlog_truncated() ? " (TRUNCADO)" : "");
    if (gRefLoaded)
        appfile_free(&gRef);
    memblock_free(&buf);
    memblock_free(&arena);
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
