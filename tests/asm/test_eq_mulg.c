/*
 * test_eq_mulg — el kernel asm del ecualizador (ganancia Q24 saturada) contra la referencia en C, bit a bit. Corre bajo
 * qemu-m68k (targets/asm-test.sh). Solo corrección.
 *
 * Casos: ganancia 1,0 (no se toca), ganancias de 0 a ~16x en Q24 (incluida la máxima de la tabla, +24 dB), coeficientes extremos
 * (saturación positiva y negativa, justo en el borde del rango de 32 bits), largos de 0 a 600 y rondas aleatorias; con palabras de
 * guarda a los lados para comprobar que no escribe fuera, y que el kernel preserve d2-d7/a2-a6.
 */
#include <stdint.h>
#include "asmrt.h"
#include "eq_kernels.h"

#define MAXN 600
#define GUARD 4

static int32_t x_ref[GUARD + MAXN + GUARD], x_asm[GUARD + MAXN + GUARD], g[MAXN], x0[MAXN];
static unsigned fails;

static void fail(const char *what, unsigned a, unsigned b, uint32_t exp, uint32_t got)
{
    if (fails < 10) {
        say("  DIFIERE "); say(what); say(" "); say_dec(a); say(" "); say_dec(b);
        say(" esperado "); say_hex(exp); say(" obtenido "); say_hex(got); say("\n");
    }
    fails++;
}

static void run(unsigned n, unsigned tag)
{
    unsigned i;
    long clob;

    for (i = 0; i < GUARD + MAXN + GUARD; i++)
        x_ref[i] = x_asm[i] = (int32_t)0xA5A5A5A5;
    for (i = 0; i < n; i++)
        x_ref[GUARD + i] = x_asm[GUARD + i] = x0[i];
    mp368k_eq_mulg_range_ref(x_ref + GUARD, g, n);
    clob = call_checked((void *)mp368k_eq_mulg_range_040, (long)(x_asm + GUARD), (long)g, (long)n, 0, 0, 0, 0);
    if (clob) fail("registros", tag, (unsigned)clob, 0, 0);
    for (i = 0; i < GUARD + MAXN + GUARD; i++)
        if (x_ref[i] != x_asm[i]) {
            fail("valor", tag, i, (uint32_t)x_ref[i], (uint32_t)x_asm[i]);
            return;
        }
}

int main(void)
{
    static const int32_t xs[] = {
        0, 1, -1, 2, -2, 0x7fffffff, (int32_t)0x80000000, (int32_t)0x80000001, 0x40000000, -0x40000000, 0x10000000, -0x10000000,
        0x00ffffff, 0x01000000, 0x01000001, -0x01000000, 0x7ffffff0, 0x0fffffff, 0x20000000, -0x20000000
    };
    static const int32_t gs[] = {
        1L << 24, 0, 1, -1, (1L << 24) - 1, (1L << 24) + 1, 1L << 23, 1L << 25, 1L << 26, 1L << 27, 255L << 20,   /* ~15,9x: +24 dB */
        0x7fffffff, (int32_t)0x80000000, 0x0FDF0000, 3L << 22, 5L << 21, -(1L << 24), -(1L << 25)
    };
    uint32_t seed = 24;
    unsigned i, n, round;

    say("test_eq_mulg: ganancia Q24 saturada, asm contra referencia en C\n");
    for (i = 0; i < MAXN; i++) {
        x0[i] = xs[i % (sizeof xs / sizeof xs[0])];
        g[i] = gs[(i * 7 + 1) % (sizeof gs / sizeof gs[0])];
    }
    for (n = 0; n <= 120; n++)
        run(n, 1);
    run(MAXN, 1);
    for (i = 0; i < MAXN; i++) {                        /* todas las combinaciones de extremos, de a pares */
        x0[i] = xs[i % (sizeof xs / sizeof xs[0])];
        g[i] = gs[(i / (sizeof xs / sizeof xs[0])) % (sizeof gs / sizeof gs[0])];
    }
    run(MAXN, 2);
    for (round = 0; round < 1500; round++) {
        for (i = 0; i < MAXN; i++) {
            uint32_t r = rnd(&seed);

            x0[i] = (round & 1) ? (int32_t)rnd(&seed) : (int32_t)(rnd(&seed) >> 3) - 0x10000000;       /* rango real de libmad */
            g[i] = (r & 7) == 0 ? (1L << 24) : ((r >> 3) & 1) ? (int32_t)(rnd(&seed) & 0x0FFFFFFF) : (int32_t)(rnd(&seed) & 0x00FFFFFF);
        }
        run(rnd(&seed) % (MAXN + 1), 3 + round);
    }
    say("fallas: "); say_dec(fails); say("\n");
    say(fails ? "RESULTADO: FALLA\n" : "RESULTADO: OK\n");
    return fails != 0;
}
