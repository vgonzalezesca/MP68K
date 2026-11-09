/*
 * test_wma_window — los kernels asm de la ventana de WMA (fmul_add_add y fmul_reverse) contra las referencias en C, bit a bit.
 * Corre bajo qemu-m68k (targets/asm-test.sh). Solo corrección.
 *
 * Entradas: ceros, extremos (INT_MIN * INT_MIN, INT_MAX * INT_MAX, INT_MIN * -1...), potencias de dos, y rondas aleatorias
 * con largos de 1 a 2048 (y los tamaños de bloque reales de WMA: 128..4096), con palabras de guarda alrededor de la salida y
 * dst inicial aleatorio (la suma debe envolver igual). Largo 0 y negativo: no deben tocar nada.
 */
#include <stdint.h>
#include "asmrt.h"
#include "wma_kernels.h"

#define MAXN 4096
#define GUARD 4

static int32_t a[MAXN], b[MAXN];
static int32_t d_ref[MAXN + 2 * GUARD], d_asm[MAXN + 2 * GUARD];
static unsigned fails;

static void fail(const char *what, unsigned a_, unsigned b_, uint32_t exp, uint32_t got)
{
    if (fails < 10) {
        say("  DIFIERE "); say(what); say(" "); say_dec(a_); say(" "); say_dec(b_);
        say(" esperado "); say_hex(exp); say(" obtenido "); say_hex(got); say("\n");
    }
    fails++;
}

static void run(int which, int len, const int32_t *dst0, unsigned tag)
{
    int i, n = len > 0 ? len : 0;
    long clob;
    void (*ref)(int32_t *, const int32_t *, const int32_t *, int) = which ? mp368k_wma_fmul_reverse_ref : mp368k_wma_fmul_add_add_ref;
    void *asmf = which ? (void *)mp368k_wma_fmul_reverse_040 : (void *)mp368k_wma_fmul_add_add_040;

    for (i = 0; i < MAXN + 2 * GUARD; i++)
        d_ref[i] = d_asm[i] = (i >= GUARD && i < GUARD + n) ? dst0[i - GUARD] : (int32_t)0x5A5A5A5A;
    ref(d_ref + GUARD, a, b, len);
    clob = call_checked(asmf, (long)(d_asm + GUARD), (long)a, (long)b, (long)len, 0, 0, 0);
    if (clob)
        fail(which ? "reverse registros" : "add_add registros", tag, (unsigned)clob, 0, 0);
    for (i = 0; i < n + 2 * GUARD && i < MAXN + 2 * GUARD; i++)
        if (d_ref[i] != d_asm[i]) {
            fail(which ? "reverse" : "add_add", tag, (unsigned)i, (uint32_t)d_ref[i], (uint32_t)d_asm[i]);
            return;
        }
}

static void both(int len, const int32_t *dst0, unsigned tag)
{
    run(0, len, dst0, tag);
    run(1, len, dst0, tag);
}

int main(void)
{
    static int32_t dst0[MAXN];
    static const int32_t edge[] = { 0, 1, -1, 2, -2, 0x7FFFFFFF, (int32_t)0x80000000, 0x40000000, (int32_t)0xC0000000,
                                    0x7FFFFFFE, (int32_t)0x80000001, 0x00010000, (int32_t)0xFFFF0000, 0x12345678, (int32_t)0xEDCBA987 };
    const unsigned ne = sizeof edge / sizeof edge[0];
    uint32_t seed = 0x57A11CEu;
    unsigned i, j, r;
    static const int sizes[] = { 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 33, 100, 127, 128, 255, 256, 511, 512, 1023, 1024, 2048, 4096 };

    say("test_wma_window: kernels asm contra la referencia en C\n");

    for (i = 0; i < MAXN; i++)
        dst0[i] = 0;
    /* todos los pares de valores extremos (cada par repetido 3 veces para cubrir el desenrollado por largo) */
    for (i = 0; i < ne; i++)
        for (j = 0; j < ne; j++) {
            unsigned k;

            for (k = 0; k < 16; k++) {
                a[k] = edge[i];
                b[k] = edge[(j + k) % ne];
            }
            both(16, dst0, i * ne + j);
            for (k = 0; k < 16; k++)
                dst0[k] = edge[(i + k) % ne];                /* suma sobre un dst que ya trae envoltura */
            both(16, dst0, 1000 + i * ne + j);
            for (k = 0; k < 16; k++)
                dst0[k] = 0;
        }
    /* aleatorios con todos los largos de la lista y los de 1 a 40 */
    for (r = 0; r < 3; r++)
        for (i = 0; i < sizeof sizes / sizeof sizes[0] + 40; i++) {
            int len = i < 40 ? (int)(i + 1) : sizes[i - 40];
            int k;

            for (k = 0; k < len; k++) {
                a[k] = (int32_t)rnd(&seed);
                b[k] = (r == 0) ? (int32_t)rnd(&seed) : (int32_t)(rnd(&seed) >> (r * 8));   /* r>0: ventanas de menor magnitud */
                dst0[k] = (int32_t)rnd(&seed);
            }
            both(len, dst0, 2000 + (unsigned)len);
        }
    /* largo 0 y negativo: no tocan la salida */
    both(0, dst0, 3000);
    both(-5, dst0, 3001);

    if (fails) {
        say("RESULTADO: FALLA ("); say_dec(fails); say(" diferencias)\n");
        return 1;
    }
    say("RESULTADO: todo OK (0 fallas)\n");
    return 0;
}
