/*
 * test_synth_window_half16 — la ventana de media tasa de 16 bits en asm contra su referencia en C, bit a bit.
 * Corre bajo qemu-m68k (targets/asm-test.sh). Solo corrección: no mide tiempos.
 *
 * Casos: ceros, máximos, mínimos, mezcla de extremos y 3000 rondas aleatorias, cada una en las 16 fases
 * (pe/po como en libmad). Además comprueba que el kernel preserva d2-d7/a2-a6 y que no escribe fuera de
 * las 16 palabras de salida.
 */
#include <stdint.h>
#include "asmrt.h"
#include "synth_kernels.h"

#define ROUNDS  3000
#define GUARD   8

static int16_t D[17 * 32];
static int32_t filt[2][2][16][8];      /* como mad_synth.filter[ch]: [eo][peo][s][v] */
static int32_t outbuf_ref[GUARD + 16 + GUARD];
static int32_t outbuf_asm[GUARD + 16 + GUARD];

static unsigned fails;

static void fill(uint32_t *seed, int mode)
{
    unsigned i;
    int32_t *f = &filt[0][0][0][0];

    for (i = 0; i < sizeof D / 2; i++)
        D[i] = mode == 0 ? 0 : mode == 1 ? 0x7fff : mode == 2 ? (int16_t)0x8000
             : mode == 3 ? ((rnd(seed) & 1) ? 0x7fff : (int16_t)0x8000) : (int16_t)rnd(seed);
    for (i = 0; i < sizeof filt / 4; i++)
        f[i] = mode == 0 ? 0 : mode == 1 ? 0x7fffffff : mode == 2 ? (int32_t)0x80000000
             : mode == 3 ? ((rnd(seed) & 1) ? 0x7fffffff : (int32_t)0x80000000) : (int32_t)rnd(seed);
}

static void run_one(unsigned ph, int mode, unsigned round)
{
    unsigned pe = ph & ~1u, po = ((ph - 1) & 0xf) | 1, i, bad = 0;
    long clob;
    const int32_t *fx = &filt[0][~ph & 1][0][0], *fe = &filt[0][ph & 1][0][0], *fo = &filt[1][~ph & 1][0][0];

    for (i = 0; i < GUARD + 16 + GUARD; i++)
        outbuf_ref[i] = outbuf_asm[i] = (int32_t)0xA5A5A5A5;
    mp368k_synth_window_half16_ref(fx, fe, fo, D, pe, po, outbuf_ref + GUARD);
    clob = call_checked((void *)mp368k_synth_window_half16_040, (long)fx, (long)fe, (long)fo, (long)D,
                        (long)pe, (long)po, (long)(outbuf_asm + GUARD));
    if (clob) {
        if (fails < 10) { say("  registros no preservados, mascara "); say_hex((uint32_t)clob); say("\n"); }
        fails++;
    }
    for (i = 0; i < GUARD + 16 + GUARD; i++)
        if (outbuf_ref[i] != outbuf_asm[i]) {
            if (fails < 10) {
                say("  DIFIERE modo "); say_dec((uint32_t)mode); say(" ronda "); say_dec(round);
                say(" fase "); say_dec(ph); say(" indice "); say_dec(i - GUARD < 40 ? (uint32_t)(i - GUARD) : i);
                say(" esperado "); say_hex((uint32_t)outbuf_ref[i]); say(" obtenido "); say_hex((uint32_t)outbuf_asm[i]);
                say("\n");
            }
            bad = 1;
            break;
        }
    if (bad)
        fails++;
}

int main(void)
{
    uint32_t seed = 368;
    unsigned ph, round, mode;

    say("test_synth_window_half16: ventana de media tasa de 16 bits, asm contra referencia en C\n");
    for (mode = 0; mode < 4; mode++) {          /* ceros, maximos, minimos, extremos mezclados */
        fill(&seed, (int)mode);
        for (ph = 0; ph < 16; ph++)
            run_one(ph, (int)mode, 0);
    }
    for (round = 0; round < ROUNDS; round++) {
        fill(&seed, 4);
        for (ph = 0; ph < 16; ph++)
            run_one(ph, 4, round);
    }
    say("casos: "); say_dec(4 * 16 + ROUNDS * 16); say("  fallas: "); say_dec(fails); say("\n");
    say(fails ? "RESULTADO: FALLA\n" : "RESULTADO: OK\n");
    return fails != 0;
}
