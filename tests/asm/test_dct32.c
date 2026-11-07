/*
 * test_dct32 — el kernel asm del DCT de 32 puntos (generado) contra la referencia en C (generada), bit a bit.
 * Corre bajo qemu-m68k (targets/asm-test.sh). Solo corrección.
 *
 * Casos: ceros, máximos, mínimos, extremos mezclados y 4000 rondas aleatorias, cada una en los 8 valores de
 * "slot". Compara los dos arreglos lo/hi COMPLETOS (también que no se escriba fuera de las posiciones esperadas)
 * y que el kernel preserve d2-d7/a2-a6.
 */
#include <stdint.h>
#include "asmrt.h"
#include "synth_kernels.h"

#define ROUNDS 4000

static int32_t in[32];
static int32_t lo_ref[16][8], hi_ref[16][8], lo_asm[16][8], hi_asm[16][8];
static unsigned fails;

static void fill(uint32_t *seed, int mode)
{
    unsigned i;

    for (i = 0; i < 32; i++)
        in[i] = mode == 0 ? 0 : mode == 1 ? 0x7fffffff : mode == 2 ? (int32_t)0x80000000
              : mode == 3 ? ((rnd(seed) & 1) ? 0x7fffffff : (int32_t)0x80000000)
              : mode == 4 ? (int32_t)(rnd(seed) >> 3) - 0x10000000 : (int32_t)rnd(seed);
}

static void run_one(unsigned slot, int mode, unsigned round)
{
    unsigned i;
    long clob;

    for (i = 0; i < 128; i++)
        (&lo_ref[0][0])[i] = (&hi_ref[0][0])[i] = (&lo_asm[0][0])[i] = (&hi_asm[0][0])[i] = (int32_t)0xA5A5A5A5;
    mp368k_dct32_ref(in, slot, lo_ref, hi_ref);
    clob = call_checked((void *)mp368k_dct32_040, (long)in, (long)slot, (long)lo_asm, (long)hi_asm, 0, 0, 0);
    if (clob) {
        if (fails < 10) { say("  registros no preservados, mascara "); say_hex((uint32_t)clob); say("\n"); }
        fails++;
    }
    for (i = 0; i < 128; i++) {
        if ((&lo_ref[0][0])[i] != (&lo_asm[0][0])[i] || (&hi_ref[0][0])[i] != (&hi_asm[0][0])[i]) {
            if (fails < 10) {
                say("  DIFIERE modo "); say_dec((uint32_t)mode); say(" ronda "); say_dec(round);
                say(" slot "); say_dec(slot); say(" posicion "); say_dec(i);
                say(" lo esperado "); say_hex((uint32_t)(&lo_ref[0][0])[i]); say(" obtenido "); say_hex((uint32_t)(&lo_asm[0][0])[i]);
                say(" hi esperado "); say_hex((uint32_t)(&hi_ref[0][0])[i]); say(" obtenido "); say_hex((uint32_t)(&hi_asm[0][0])[i]);
                say("\n");
            }
            fails++;
            break;
        }
    }
}

int main(void)
{
    uint32_t seed = 3232;
    unsigned slot, round, mode;

    say("test_dct32: DCT de 32 puntos, asm contra referencia en C\n");
    for (mode = 0; mode < 4; mode++) {
        fill(&seed, (int)mode);
        for (slot = 0; slot < 8; slot++)
            run_one(slot, (int)mode, 0);
    }
    for (round = 0; round < ROUNDS; round++) {
        fill(&seed, 4 + (int)(round & 1));      /* mitad en el rango real de libmad, mitad en todo el rango */
        for (slot = 0; slot < 8; slot++)
            run_one(slot, 4 + (int)(round & 1), round);
    }
    say("casos: "); say_dec(4 * 8 + ROUNDS * 8); say("  fallas: "); say_dec(fails); say("\n");
    say(fails ? "RESULTADO: FALLA\n" : "RESULTADO: OK\n");
    return fails != 0;
}
