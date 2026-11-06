/*
 * microbench — Etapa 1 de MP368K: microbenchmarks de CPU para el 68LC040.
 *
 * Mide, con Microseconds(), el costo de las operaciones que dominarán el
 * decodificador (SPECS §8, etapa 1):
 *   - MULS.L 32x32->32 (independiente y dependiente), MULS.L 32x32->64, MULS.W
 *   - lectura secuencial de buffers de 1..64 KB (efecto de la caché de datos)
 *   - lectura con índice pseudoaleatorio en tablas de 1..32 KB
 *   - copia con MOVE16, MOVEM y MOVE.L, con 1 KB (en caché) y 64 KB (fuera)
 *
 * Escribe bench_results_micro.txt junto a la aplicación (formato en
 * .claude/skills/mac-test-workflow/SKILL.md). NO interpreta nada: los ciclos
 * del resumen suponen 33 MHz (dato del usuario, sin medir en el equipo).
 *
 * El ensamblador de este archivo es instrumental de medición, no un kernel del
 * decodificador: no sustituye nada ni se usa fuera de bench/. Sin código
 * auto-modificable; todos los bucles caben de sobra en la caché de
 * instrucciones de 4 KB.
 */
#include <Events.h>
#include <Gestalt.h>
#include <Memory.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "build_info.h"
#include "mactime.h"
#include "textlog.h"

#define ASSUMED_MHZ 33          /* LC 580: dato del usuario, por verificar */
#define RUNS        6           /* 1 pasada fría + 5 para estadística */
#define INS_ITERS   8192        /* iteraciones de los tests de instrucción (x16 ops) */
#define TOTAL_LONGS (1L << 20)  /* lecturas por test de memoria */
#define TABLE_LOADS (1L << 19)  /* lecturas por test de tabla */
#define COPY_BYTES  (4L << 20)  /* bytes copiados por test de copia */
#define BUF_MAX     (64L * 1024)

/* Repetición de cadenas de ensamblador (argumento: literales adyacentes). */
#define REP2(s)  s s
#define REP4(s)  REP2(REP2(s))
#define REP8(s)  REP4(REP2(s))
#define REP16(s) REP4(REP4(s))

/* ------------------------------------------------------------------------ */
/* Kernels de medición. Cada uno: noinline, con asm volatile y clobbers.     */
/* ------------------------------------------------------------------------ */

/* Bucle vacío: costo de "subq + bne" por iteración (se resta en el resumen). */
static __attribute__((noinline)) void t_base(long n)
{
    __asm__ volatile (
        "1:\n\t"
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(n) : : "cc");
}

/* MULS.L Dm,Dn (32x32->32). Cuatro destinos rotando: casi sin dependencias. */
static __attribute__((noinline)) void t_mull32_indep(long n)
{
    __asm__ volatile (
        "move.l #0x12345679,%%d1\n\t"   /* multiplicador impar: nunca llega a 0 */
        "move.l #0x9E3779B1,%%d2\n\t"
        "move.l #0x7F4A7C15,%%d3\n\t"
        "move.l #0x85EBCA6B,%%d4\n\t"
        "move.l #0xC2B2AE35,%%d5\n\t"
        "1:\n\t"
        REP4("muls.l %%d1,%%d2\n\t" "muls.l %%d1,%%d3\n\t"
             "muls.l %%d1,%%d4\n\t" "muls.l %%d1,%%d5\n\t")
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(n) : : "d1", "d2", "d3", "d4", "d5", "cc");
}

/* Igual, pero cada MULS.L depende del anterior (cadena de latencia). */
static __attribute__((noinline)) void t_mull32_dep(long n)
{
    __asm__ volatile (
        "move.l #0x12345679,%%d1\n\t"
        "move.l #0x9E3779B1,%%d2\n\t"
        "1:\n\t"
        REP16("muls.l %%d1,%%d2\n\t")
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(n) : : "d1", "d2", "cc");
}

/* MULS.L Dm,Dh:Dl (32x32->64): la operación de la síntesis polifásica. */
static __attribute__((noinline)) void t_mull64_indep(long n)
{
    __asm__ volatile (
        "move.l #0x12345679,%%d1\n\t"
        "move.l #0x9E3779B1,%%d2\n\t"
        "move.l #0x7F4A7C15,%%d3\n\t"
        "move.l #0x85EBCA6B,%%d4\n\t"
        "move.l #0xC2B2AE35,%%d5\n\t"
        "1:\n\t"
        REP4("muls.l %%d1,%%d6:%%d2\n\t" "muls.l %%d1,%%d6:%%d3\n\t"
             "muls.l %%d1,%%d6:%%d4\n\t" "muls.l %%d1,%%d6:%%d5\n\t")
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(n) : : "d1", "d2", "d3", "d4", "d5", "d6", "cc");
}

/* MULS.W Dm,Dn (16x16->32). */
static __attribute__((noinline)) void t_mulw_indep(long n)
{
    __asm__ volatile (
        "move.l #0x12345679,%%d1\n\t"
        "move.l #0x9E3779B1,%%d2\n\t"
        "move.l #0x7F4A7C15,%%d3\n\t"
        "move.l #0x85EBCA6B,%%d4\n\t"
        "move.l #0xC2B2AE35,%%d5\n\t"
        "1:\n\t"
        REP4("muls.w %%d1,%%d2\n\t" "muls.w %%d1,%%d3\n\t"
             "muls.w %%d1,%%d4\n\t" "muls.w %%d1,%%d5\n\t")
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(n) : : "d1", "d2", "d3", "d4", "d5", "cc");
}

/*
 * Lectura secuencial: `passes` pasadas sobre `buf`, 8 MOVE.L (An)+ por
 * bloque de 32 bytes. Tiempo bruto: incluye subq+bne cada 8 lecturas.
 */
static __attribute__((noinline)) void t_read_seq(long passes, const void *buf, long blocks32)
{
    __asm__ volatile (
        "1:\n\t"
        "move.l %2,%%a0\n\t"
        "move.l %1,%%d7\n\t"
        "2:\n\t"
        REP8("move.l (%%a0)+,%%d0\n\t")
        "subq.l #1,%%d7\n\t"
        "bne 2b\n\t"
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(passes) : "d"(blocks32), "a"(buf)
        : "d0", "d7", "a0", "cc", "memory");
}

/*
 * Lectura con índice que recorre la tabla con paso de 52 bytes (13 longs,
 * impar: visita todas las posiciones). Modela el acceso a la ventana de
 * síntesis. `mask` = tamaño-4. La versión _base omite la lectura para poder
 * restar el costo de "add + and" (y del bucle).
 */
static __attribute__((noinline)) void t_table(long count, const void *buf, long mask)
{
    __asm__ volatile (
        "move.l %2,%%a0\n\t"
        "moveq #52,%%d5\n\t"
        "moveq #0,%%d4\n\t"
        "1:\n\t"
        REP8("add.l %%d5,%%d4\n\t" "and.l %1,%%d4\n\t" "move.l (%%a0,%%d4.l),%%d0\n\t")
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(count) : "d"(mask), "a"(buf)
        : "d0", "d4", "d5", "a0", "cc", "memory");
}

static __attribute__((noinline)) void t_table_base(long count, const void *buf, long mask)
{
    __asm__ volatile (
        "move.l %2,%%a0\n\t"
        "moveq #52,%%d5\n\t"
        "moveq #0,%%d4\n\t"
        "1:\n\t"
        REP8("add.l %%d5,%%d4\n\t" "and.l %1,%%d4\n\t")
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(count) : "d"(mask), "a"(buf)
        : "d0", "d4", "d5", "a0", "cc", "memory");
}

/* Copia de bloques de 128 bytes con MOVE16 (origen y destino alineados a 16). */
static __attribute__((noinline)) void t_copy_move16(long passes, void *dst, const void *src, long blocks128)
{
    __asm__ volatile (
        "1:\n\t"
        "move.l %3,%%a0\n\t"
        "move.l %2,%%a1\n\t"
        "move.l %1,%%d7\n\t"
        "2:\n\t"
        REP8("move16 (%%a0)+,(%%a1)+\n\t")
        "subq.l #1,%%d7\n\t"
        "bne 2b\n\t"
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(passes) : "d"(blocks128), "a"(dst), "a"(src)
        : "d7", "a0", "a1", "cc", "memory");
}

/*
 * Copia con MOVEM: MOVEM registros->memoria no admite post-incremento, así
 * que el destino se recorre hacia abajo con -(An). `dstEnd` = destino+tamaño.
 */
static __attribute__((noinline)) void t_copy_movem(long passes, void *dstEnd, const void *src, long blocks128)
{
    __asm__ volatile (
        "1:\n\t"
        "move.l %3,%%a0\n\t"
        "move.l %2,%%a1\n\t"
        "move.l %1,%%d7\n\t"
        "2:\n\t"
        REP8("movem.l (%%a0)+,%%d0-%%d3\n\t" "movem.l %%d0-%%d3,-(%%a1)\n\t")
        "subq.l #1,%%d7\n\t"
        "bne 2b\n\t"
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(passes) : "d"(blocks128), "a"(dstEnd), "a"(src)
        : "d0", "d1", "d2", "d3", "d7", "a0", "a1", "cc", "memory");
}

/* Copia con MOVE.L (An)+,(Am)+. */
static __attribute__((noinline)) void t_copy_movel(long passes, void *dst, const void *src, long blocks128)
{
    __asm__ volatile (
        "1:\n\t"
        "move.l %3,%%a0\n\t"
        "move.l %2,%%a1\n\t"
        "move.l %1,%%d7\n\t"
        "2:\n\t"
        REP16("move.l (%%a0)+,(%%a1)+\n\t" "move.l (%%a0)+,(%%a1)+\n\t")
        "subq.l #1,%%d7\n\t"
        "bne 2b\n\t"
        "subq.l #1,%0\n\t"
        "bne 1b\n\t"
        : "+d"(passes) : "d"(blocks128), "a"(dst), "a"(src)
        : "d7", "a0", "a1", "cc", "memory");
}

/* ------------------------------------------------------------------------ */
/* Definición de las pruebas                                                 */
/* ------------------------------------------------------------------------ */

enum {
    K_BASE, K_MULL32_INDEP, K_MULL32_DEP, K_MULL64_INDEP, K_MULW_INDEP,
    K_READ, K_TABLE_BASE, K_TABLE, K_COPY16, K_COPYMOVEM, K_COPYMOVEL
};

typedef struct {
    char name[32];       /* identificador en el CSV */
    const char *unit;    /* qué cuenta una "op" */
    int kind;
    int baseline;        /* índice del test cuyo mínimo se resta, o -1 */
    long a, b;           /* parámetros (iteraciones / pasadas, bloques / máscara) */
    long size;           /* tamaño del buffer en bytes (0 si no aplica) */
    long ops;            /* operaciones por llamada */
    long bytes;          /* bytes procesados por llamada (0 si no aplica) */
    Boolean needsMem;
} Test;

#define MAX_TESTS 48
static Test gTests[MAX_TESTS];
static int gCount;

static uint8_t *gSrc, *gDst;

static int add_test(const char *name, const char *unit, int kind, int baseline,
                    long a, long b, long size, long ops, long bytes, Boolean needsMem)
{
    Test *t = &gTests[gCount];
    snprintf(t->name, sizeof t->name, "%s", name);
    t->unit = unit;
    t->kind = kind;
    t->baseline = baseline;
    t->a = a;
    t->b = b;
    t->size = size;
    t->ops = ops;
    t->bytes = bytes;
    t->needsMem = needsMem;
    return gCount++;
}

static void build_tests(void)
{
    static const long kKB[] = { 1, 2, 4, 8, 16, 64 };
    static const long kTblKB[] = { 1, 2, 4, 8, 16, 32 };
    char name[32];
    int base, i;

    base = add_test("base_loop", "iteracion (subq+bne)", K_BASE, -1,
                    INS_ITERS, 0, 0, 0, 0, false);
    add_test("mulsl_32_indep", "instruccion", K_MULL32_INDEP, base,
             INS_ITERS, 0, 0, INS_ITERS * 16L, 0, false);
    add_test("mulsl_32_dep", "instruccion", K_MULL32_DEP, base,
             INS_ITERS, 0, 0, INS_ITERS * 16L, 0, false);
    add_test("mulsl_64_indep", "instruccion", K_MULL64_INDEP, base,
             INS_ITERS, 0, 0, INS_ITERS * 16L, 0, false);
    add_test("mulsw_indep", "instruccion", K_MULW_INDEP, base,
             INS_ITERS, 0, 0, INS_ITERS * 16L, 0, false);

    for (i = 0; i < 6; i++) {
        long bytes = kKB[i] * 1024L;
        snprintf(name, sizeof name, "read_seq_%ldk", kKB[i]);
        add_test(name, "longword leida (bruto)", K_READ, -1,
                 TOTAL_LONGS / (bytes / 4), bytes / 32, bytes, TOTAL_LONGS,
                 TOTAL_LONGS * 4, true);
    }

    base = add_test("table_base", "lectura omitida (add+and)", K_TABLE_BASE, -1,
                    TABLE_LOADS / 8, 0, 0, 0, 0, true);
    for (i = 0; i < 6; i++) {
        long bytes = kTblKB[i] * 1024L;
        snprintf(name, sizeof name, "table_%ldk", kTblKB[i]);
        add_test(name, "lectura indexada", K_TABLE, base,
                 TABLE_LOADS / 8, bytes - 4, bytes, TABLE_LOADS, 0, true);
    }
    /* El test de tabla de 1 KB lleva su máscara en b; la _base ignora b. */

    for (i = 0; i < 2; i++) {
        long bytes = (i == 0 ? 1L : 64L) * 1024L;
        long passes = COPY_BYTES / bytes;
        long blocks = bytes / 128;
        long lines = COPY_BYTES / 16;
        snprintf(name, sizeof name, "copy_move16_%ldk", bytes / 1024);
        add_test(name, "16 bytes copiados", K_COPY16, -1, passes, blocks, bytes, lines, COPY_BYTES, true);
        snprintf(name, sizeof name, "copy_movem_%ldk", bytes / 1024);
        add_test(name, "16 bytes copiados", K_COPYMOVEM, -1, passes, blocks, bytes, lines, COPY_BYTES, true);
        snprintf(name, sizeof name, "copy_movel_%ldk", bytes / 1024);
        add_test(name, "16 bytes copiados", K_COPYMOVEL, -1, passes, blocks, bytes, lines, COPY_BYTES, true);
    }
}

static void run_once(const Test *t)
{
    switch (t->kind) {
    case K_BASE:          t_base(t->a); break;
    case K_MULL32_INDEP:  t_mull32_indep(t->a); break;
    case K_MULL32_DEP:    t_mull32_dep(t->a); break;
    case K_MULL64_INDEP:  t_mull64_indep(t->a); break;
    case K_MULW_INDEP:    t_mulw_indep(t->a); break;
    case K_READ:          t_read_seq(t->a, gSrc, t->b); break;
    case K_TABLE_BASE:    t_table_base(t->a, gSrc, 0xFFC); break;
    case K_TABLE:         t_table(t->a, gSrc, t->b); break;
    case K_COPY16:        t_copy_move16(t->a, gDst, gSrc, t->b); break;
    case K_COPYMOVEM:     t_copy_movem(t->a, gDst + t->size, gSrc, t->b); break;
    case K_COPYMOVEL:     t_copy_movel(t->a, gDst, gSrc, t->b); break;
    }
}

/* ------------------------------------------------------------------------ */
/* Memoria                                                                   */
/* ------------------------------------------------------------------------ */

/* Reserva alineada a 16 bytes (línea de caché y requisito de MOVE16). */
static uint8_t *alloc_aligned(long size)
{
    Ptr p = NewPtr(size + 16);

    if (p == NULL) {
        OSErr err;
        Handle h = TempNewHandle(size + 16, &err);
        if (h == NULL)
            return NULL;
        HLock(h);
        p = *h;
    }
    return (uint8_t *)(((unsigned long)p + 15UL) & ~15UL);
}

static void fill_pattern(uint8_t *p, long size)
{
    uint32_t *w = (uint32_t *)p;
    long i;

    for (i = 0; i < size / 4; i++)
        w[i] = 0x9E3779B1UL * (uint32_t)(i + 1);
}

/* ------------------------------------------------------------------------ */
/* Estadística y resumen                                                     */
/* ------------------------------------------------------------------------ */

typedef struct {
    Boolean valid;
    uint64_t min, med;
} Result;

static Result gRes[MAX_TESTS];

static void stats(const uint64_t us[RUNS], Result *r)
{
    uint64_t s[RUNS - 1], tmp;
    int i, j;

    /* La pasada 1 (fría) se informa aparte y no entra en la estadística. */
    for (i = 1; i < RUNS; i++)
        s[i - 1] = us[i];
    for (i = 1; i < RUNS - 1; i++)
        for (j = i; j > 0 && s[j - 1] > s[j]; j--) {
            tmp = s[j]; s[j] = s[j - 1]; s[j - 1] = tmp;
        }
    r->valid = true;
    r->min = s[0];
    r->med = s[(RUNS - 1) / 2];
}

static void print_x100(uint64_t v)
{
    textlog_printf("%lu.%02lu", (unsigned long)(v / 100), (unsigned long)(v % 100));
}

static void print_summary(void)
{
    int i;

    textlog_printf("\n# summary (min y mediana de las pasadas 2..%d; ciclos suponen %d MHz)\n",
                   RUNS, ASSUMED_MHZ);
    textlog_printf("# neto = min - min(baseline); tests sin baseline son brutos\n");
    textlog_printf("summary,name,min_usec,med_usec,net_usec,ops,ns_per_op,cycles_per_op,MB_per_s\n");

    for (i = 0; i < gCount; i++) {
        const Test *t = &gTests[i];
        const Result *r = &gRes[i];
        uint64_t net;

        if (!r->valid) {
            textlog_printf("summary,%s,omitido\n", t->name);
            continue;
        }
        net = r->min;
        if (t->baseline >= 0 && gRes[t->baseline].valid)
            net = net > gRes[t->baseline].min ? net - gRes[t->baseline].min : 0;

        textlog_printf("summary,%s,%lu,%lu,%lu,%ld,", t->name,
                       (unsigned long)r->min, (unsigned long)r->med,
                       (unsigned long)net, t->ops);
        if (t->ops > 0) {
            print_x100(net * 100000ULL / (uint64_t)t->ops);          /* ns x100 */
            textlog_printf(",");
            print_x100(net * 100ULL * ASSUMED_MHZ / (uint64_t)t->ops); /* ciclos x100 */
        } else
            textlog_printf("-,-");
        textlog_printf(",");
        if (t->bytes > 0 && net > 0) {
            uint64_t mbs10 = (uint64_t)t->bytes * 10ULL / net;       /* bytes/us = MB/s */
            textlog_printf("%lu.%lu", (unsigned long)(mbs10 / 10), (unsigned long)(mbs10 % 10));
        } else
            textlog_printf("-");
        textlog_printf("\n");
    }
}

/* ------------------------------------------------------------------------ */

static long gestalt_val(unsigned long sel)
{
    long v = -1;
    if (Gestalt(sel, (LONGINT *)&v) != noErr)
        v = -1;
    return v;
}

int main(void)
{
    uint64_t us[RUNS];
    Boolean memOk;
    OSErr err;
    int i, r;

    textlog_init();
    textlog_printf("# MP368K bench\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__, MP368K_BUILD_FLAGS);
    textlog_printf("machine: mach=%ld proc=%ld fpu=%ld sysv=0x%lX ram_kb=%ld\n",
                   gestalt_val(gestaltMachineType), gestalt_val(gestaltProcessorType),
                   gestalt_val(gestaltFPUType), (unsigned long)gestalt_val(gestaltSystemVersion),
                   gestalt_val(gestaltPhysicalRAMSize) / 1024);
    textlog_printf("cache: no consultable desde la app; anotar a mano el ajuste del panel de memoria\n");
    textlog_printf("assumed_clock_mhz: %d (supuesto, sin medir)\n", ASSUMED_MHZ);
    textlog_printf("timer_resolution_usec: %lu\n", mactime_resolution_usec());
    textlog_printf("test: etapa1_microbench\n");
    textlog_printf("file: -\nprofile: -\n");

    gSrc = alloc_aligned(BUF_MAX);
    gDst = alloc_aligned(BUF_MAX);
    memOk = gSrc != NULL && gDst != NULL;
    if (memOk) {
        fill_pattern(gSrc, BUF_MAX);
        fill_pattern(gDst, BUF_MAX);
    } else
        textlog_printf("AVISO: sin memoria para buffers de 64 KB; se omiten los tests de memoria\n");
    textlog_printf("buffers: src=%08lX dst=%08lX (alineados a 16)\n",
                   (unsigned long)gSrc, (unsigned long)gDst);

    build_tests();

    for (i = 0; i < gCount; i++) {
        const Test *t = &gTests[i];

        if (t->needsMem && !memOk)
            continue;

        textlog_printf("\ntest: %s\nunit: %s\nrun,usec_total,ops\n", t->name, t->unit);
        for (r = 0; r < RUNS; r++) {
            uint64_t t0 = mactime_usec();
            run_once(t);
            us[r] = mactime_usec() - t0;
            textlog_printf("%d,%lu,%ld\n", r + 1, (unsigned long)us[r], t->ops);
        }
        stats(us, &gRes[i]);
    }

    print_summary();

    err = textlog_save_next_to_app("bench_results_micro.txt");
    textlog_printf("\nbench_results_micro.txt guardado: err=%d%s\n", err,
                   textlog_truncated() ? " (TRUNCADO)" : "");
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
