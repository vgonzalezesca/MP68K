/*
 * test_imdct_l — el kernel asm de la IMDCT de bloques largos (generado) contra la referencia en C (generada),
 * bit a bit. Corre bajo qemu-m68k (targets/asm-test.sh). Solo corrección.
 *
 * Compara las 18 muestras de salida (sample[32*i]), el overlap resultante y que no se escriba nada más en el
 * arreglo de muestras; con invert = 0 y 1; y que el kernel preserve d2-d7/a2-a6.
 */
#include <stdint.h>
#include "asmrt.h"
#include "synth_kernels.h"

#define ROUNDS 6000
#define SAMPLE_WORDS (18 * 32)

static int32_t X[18], ov0[18];
static int32_t ov_ref[18], ov_asm[18];
static int32_t smp_ref[SAMPLE_WORDS], smp_asm[SAMPLE_WORDS];
static unsigned fails;

static int32_t val(uint32_t *seed, int mode)
{
    switch (mode) {
    case 0: return 0;
    case 1: return 0x7fffffff;
    case 2: return (int32_t)0x80000000;
    case 3: return (rnd(seed) & 1) ? 0x7fffffff : (int32_t)0x80000000;
    case 4: return (int32_t)(rnd(seed) >> 4) - 0x08000000;      /* rango habitual de libmad */
    default: return (int32_t)rnd(seed);                         /* todo el rango (envoltura) */
    }
}

static void run_one(uint32_t *seed, int mode, int invert, unsigned round)
{
    unsigned i;
    long clob;

    for (i = 0; i < 18; i++) {
        X[i] = val(seed, mode);
        ov0[i] = val(seed, mode);
        ov_ref[i] = ov_asm[i] = ov0[i];
    }
    for (i = 0; i < SAMPLE_WORDS; i++)
        smp_ref[i] = smp_asm[i] = (int32_t)0xA5A5A5A5;
    mp368k_imdct_l_ref(X, ov_ref, smp_ref, invert);
    clob = call_checked((void *)mp368k_imdct_l_040, (long)X, (long)ov_asm, (long)smp_asm, invert, 0, 0, 0);
    if (clob) {
        if (fails < 10) { say("  registros no preservados, mascara "); say_hex((uint32_t)clob); say("\n"); }
        fails++;
    }
    for (i = 0; i < SAMPLE_WORDS; i++)
        if (smp_ref[i] != smp_asm[i]) {
            if (fails < 10) {
                say("  DIFIERE muestra modo "); say_dec((uint32_t)mode); say(" ronda "); say_dec(round);
                say(" invert "); say_dec((uint32_t)invert); say(" palabra "); say_dec(i);
                say(" esperado "); say_hex((uint32_t)smp_ref[i]); say(" obtenido "); say_hex((uint32_t)smp_asm[i]);
                say("\n");
            }
            fails++;
            return;
        }
    for (i = 0; i < 18; i++)
        if (ov_ref[i] != ov_asm[i]) {
            if (fails < 10) {
                say("  DIFIERE overlap modo "); say_dec((uint32_t)mode); say(" ronda "); say_dec(round);
                say(" indice "); say_dec(i);
                say(" esperado "); say_hex((uint32_t)ov_ref[i]); say(" obtenido "); say_hex((uint32_t)ov_asm[i]);
                say("\n");
            }
            fails++;
            return;
        }
}

int main(void)
{
    uint32_t seed = 1818;
    unsigned round, mode;
    int invert;

    say("test_imdct_l: IMDCT larga (ventana, solapado, inversion), asm contra referencia en C\n");
    for (mode = 0; mode < 4; mode++)
        for (invert = 0; invert < 2; invert++)
            run_one(&seed, (int)mode, invert, 0);
    for (round = 0; round < ROUNDS; round++)
        for (invert = 0; invert < 2; invert++)
            run_one(&seed, 4 + (int)(round & 1), invert, round);
    say("casos: "); say_dec(4 * 2 + ROUNDS * 2); say("  fallas: "); say_dec(fails); say("\n");
    say(fails ? "RESULTADO: FALLA\n" : "RESULTADO: OK\n");
    return fails != 0;
}
