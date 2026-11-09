/*
 * test_wma_fft — el kernel asm de la pasada de la FFT de libwma contra la referencia en C, bit a bit.
 * Corre bajo qemu-m68k (targets/asm-test.sh). Solo corrección.
 *
 * Se prueba con los tamaños reales (FFT de 32 a 4096 puntos: n = N/4 y STEP = 8192/N, como en DECL_FFT; fft16 y menores no usan pass), con una tabla de senos y
 * cosenos de 1026 palabras aleatoria y también con extremos (todo en INT_MAX / INT_MIN), datos aleatorios, y palabras de guarda
 * alrededor del arreglo. El recorrido (ZERO, W10, do-while, while hacia atrás) debe ser el mismo que el de C.
 */
#include <stdint.h>
#include "asmrt.h"
#include "wma_kernels.h"

#define MAXZ (2 * 4096)                     /* 4096 complejos = 8192 palabras */
#define GUARD 8

static int32_t T[1026];
static int32_t z_ref[MAXZ + 2 * GUARD], z_asm[MAXZ + 2 * GUARD];
static unsigned fails;

static void fail(const char *what, unsigned a, unsigned b, uint32_t exp, uint32_t got)
{
    if (fails < 10) {
        say("  DIFIERE "); say(what); say(" "); say_dec(a); say(" "); say_dec(b);
        say(" esperado "); say_hex(exp); say(" obtenido "); say_hex(got); say("\n");
    }
    fails++;
}

static void run(unsigned N, unsigned STEP, unsigned tag)
{
    unsigned n = N / 4, words = 2 * N, i;
    long clob;

    for (i = 0; i < words + 2 * GUARD; i++)
        z_asm[i] = z_ref[i] = (i >= GUARD && i < GUARD + words) ? z_ref[i] : (int32_t)0x5A5A5A5A;
    mp368k_wma_fft_pass_ref(z_ref + GUARD, STEP, n, T);
    clob = call_checked((void *)mp368k_wma_fft_pass_040, (long)(z_asm + GUARD), (long)STEP, (long)n, (long)T, 0, 0, 0);
    if (clob)
        fail("pass registros", tag, (unsigned)clob, 0, 0);
    for (i = 0; i < words + 2 * GUARD; i++)
        if (z_ref[i] != z_asm[i]) {
            fail("pass", tag, i, (uint32_t)z_ref[i], (uint32_t)z_asm[i]);
            return;
        }
}

static void fill_random(uint32_t *seed, unsigned words, unsigned shift)
{
    unsigned i;

    for (i = 0; i < words; i++)
        z_ref[GUARD + i] = (int32_t)(rnd(seed) >> shift) ;
}

int main(void)
{
    uint32_t seed = 0xFF7DA55u;
    unsigned N, i, r, tag = 0;

    say("test_wma_fft: pasada de la FFT, kernel asm contra la referencia en C\n");

    for (r = 0; r < 4; r++) {
        for (i = 0; i < 1026; i++)
            T[i] = r == 0 ? (int32_t)rnd(&seed) : (r == 1 ? ((i & 1) ? 0x7FFFFFFF : (int32_t)0x80000000) : (int32_t)(rnd(&seed) >> 1));
        for (N = 32; N <= 4096; N *= 2) {
            unsigned rep;

            for (rep = 0; rep < 3; rep++) {
                if (r == 1 && rep == 0) {                       /* extremos: todo en INT_MAX */
                    for (i = 0; i < 2 * N; i++)
                        z_ref[GUARD + i] = 0x7FFFFFFF;
                } else if (r == 1 && rep == 1) {                /* extremos: todo en INT_MIN */
                    for (i = 0; i < 2 * N; i++)
                        z_ref[GUARD + i] = (int32_t)0x80000000;
                } else {
                    fill_random(&seed, 2 * N, rep == 2 ? 3 : 0);
                }
                run(N, 8192 / N, ++tag);
            }
        }
    }
    if (fails) {
        say("RESULTADO: FALLA ("); say_dec(fails); say(" diferencias)\n");
        return 1;
    }
    say("RESULTADO: todo OK (0 fallas)\n");
    return 0;
}
