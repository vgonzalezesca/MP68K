/*
 * test_pcm — los kernels asm de conversión de PCM (scale16, u8, gain16) contra las referencias en C, bit a bit.
 * Corre bajo qemu-m68k (targets/asm-test.sh). Solo corrección.
 *
 * scale16: ceros, extremos (incluida la envoltura de +4096 cerca de INT_MAX), todos los umbrales de saturación y
 * rondas aleatorias, en mono y estéreo, con largos de 0 a 1152 y salida con palabras de guarda.
 * wma16: umbrales alrededor de 8192 y los extremos (incluida la envoltura de 8192 - INT_MIN), mono y estéreo, y rondas aleatorias.
 * u8: las 65 536 entradas posibles. gain16: ganancias 0..300 (más de 256 se limita), mono y estéreo, y n impar.
 */
#include <stdint.h>
#include "asmrt.h"
#include "pcm_kernels.h"

#define MAXN 1152
#define GUARD 8

static int32_t s0[MAXN], s1[MAXN];
static int16_t o_ref[2 * MAXN + 2 * GUARD], o_asm[2 * MAXN + 2 * GUARD];
static unsigned fails;

static void fail(const char *what, unsigned a, unsigned b, uint32_t exp, uint32_t got)
{
    if (fails < 10) {
        say("  DIFIERE "); say(what); say(" "); say_dec(a); say(" "); say_dec(b);
        say(" esperado "); say_hex(exp); say(" obtenido "); say_hex(got); say("\n");
    }
    fails++;
}

static void run_scale(unsigned len, unsigned nch, unsigned tag)
{
    unsigned i, total = nch * len + 2 * GUARD;
    long clob;

    for (i = 0; i < 2 * MAXN + 2 * GUARD; i++)
        o_ref[i] = o_asm[i] = (int16_t)0x5A5A;
    mp368k_pcm_scale16_ref(s0, s1, o_ref + GUARD, len, nch);
    clob = call_checked((void *)mp368k_pcm_scale16_040, (long)s0, (long)s1, (long)(o_asm + GUARD), (long)len, (long)nch, 0, 0);
    if (clob) fail("scale16 registros", tag, (unsigned)clob, 0, 0);
    for (i = 0; i < total; i++)
        if (o_ref[i] != o_asm[i]) {
            fail("scale16", tag, i, (uint32_t)(uint16_t)o_ref[i], (uint32_t)(uint16_t)o_asm[i]);
            return;
        }
}

static void run_wma(unsigned len, unsigned nch, unsigned tag)
{
    unsigned i, total = nch * len + 2 * GUARD;
    long clob;

    for (i = 0; i < 2 * MAXN + 2 * GUARD; i++)
        o_ref[i] = o_asm[i] = (int16_t)0x5A5A;
    mp368k_pcm_wma16_ref(s0, s1, o_ref + GUARD, len, nch);
    clob = call_checked((void *)mp368k_pcm_wma16_040, (long)s0, (long)s1, (long)(o_asm + GUARD), (long)len, (long)nch, 0, 0);
    if (clob) fail("wma16 registros", tag, (unsigned)clob, 0, 0);
    for (i = 0; i < total; i++)
        if (o_ref[i] != o_asm[i]) {
            fail("wma16", tag, i, (uint32_t)(uint16_t)o_ref[i], (uint32_t)(uint16_t)o_asm[i]);
            return;
        }
}

static void run_gain(const int16_t *src, unsigned n, unsigned ch, unsigned gl, unsigned gr, unsigned tag)
{
    static int16_t a[MAXN + 4], b[MAXN + 4];
    unsigned i;
    long clob;

    for (i = 0; i < MAXN + 4; i++) {
        a[i] = b[i] = (int16_t)(i < n ? src[i] : 0x1234);
    }
    mp368k_pcm_gain16_ref(a, a, n, ch, gl, gr);
    clob = call_checked((void *)mp368k_pcm_gain16_040, (long)b, (long)b, (long)n, (long)ch, (long)gl, (long)gr, 0);
    if (clob) fail("gain16 registros", tag, (unsigned)clob, 0, 0);
    for (i = 0; i < MAXN + 4; i++)
        if (a[i] != b[i]) {
            fail("gain16", tag, i, (uint32_t)(uint16_t)a[i], (uint32_t)(uint16_t)b[i]);
            return;
        }
}

int main(void)
{
    uint32_t seed = 16;
    unsigned i, n, nch, round;
    static int16_t src16[MAXN + 4];
    static uint8_t u_ref[65536], u_asm[65536];
    static int16_t all[65536];
    long clob;

    say("test_pcm: scale16, u8 y gain16, asm contra referencia en C\n");

    /* scale16: umbrales y extremos */
    {
        static const int32_t special[] = {
            0, 1, -1, 4095, 4096, -4096, -4097, 8191, 8192, 0x0fffefff, 0x0ffff000, 0x0fffffff, 0x10000000,
            0x10000fff, 0x10001000, -0x10000000, -0x0fffefff, -0x10001000, -0x10001001, -0x10002000,
            0x7fffefff, 0x7ffff000, 0x7fffffff, (int32_t)0x80000000, (int32_t)0x80000001, 0x00001fff, 0x00002000,
            0x0fffe000, 0x0fffd000
        };
        unsigned ns = sizeof special / sizeof special[0];

        for (i = 0; i < MAXN; i++) {
            s0[i] = special[i % ns];
            s1[i] = special[(i * 7 + 3) % ns];
        }
        for (nch = 1; nch <= 2; nch++)
            for (n = 0; n <= 70; n++)
                run_scale(n, nch, 1);
        run_scale(MAXN, 2, 1);
    }
    for (round = 0; round < 800; round++) {
        for (i = 0; i < MAXN; i++) {
            s0[i] = (round & 1) ? (int32_t)rnd(&seed) : (int32_t)(rnd(&seed) >> 2) - 0x20000000;
            s1[i] = (round & 1) ? (int32_t)rnd(&seed) : (int32_t)(rnd(&seed) >> 2) - 0x20000000;
        }
        run_scale(rnd(&seed) % (MAXN + 1), 1 + (round & 1), 2 + round);
    }

    /* wma16: umbrales de saturacion de (8192 - v) >> 14 y extremos */
    {
        static const int32_t special[] = {
            0, 1, -1, 8191, 8192, 8193, -8191, -8192, -8193, 0x1fffe000, 0x1fffffff, 0x20000000, -0x1fffe000, -0x20000000,
            -0x20000001, -0x20002000, 0x0fffffff, 0x10000000, -0x10000000, 0x7fffffff, 0x7fffe000, (int32_t)0x80000000,
            (int32_t)0x80000001, (int32_t)0x80002000, 16383, 16384, -16383, -16384, 0x00003fff, 0x00004000
        };
        unsigned ns = sizeof special / sizeof special[0];

        for (i = 0; i < MAXN; i++) {
            s0[i] = special[i % ns];
            s1[i] = special[(i * 5 + 2) % ns];
        }
        for (nch = 1; nch <= 2; nch++)
            for (n = 0; n <= 70; n++)
                run_wma(n, nch, 1);
        run_wma(MAXN, 2, 1);
    }
    for (round = 0; round < 800; round++) {
        for (i = 0; i < MAXN; i++) {
            s0[i] = (round & 1) ? (int32_t)rnd(&seed) : (int32_t)(rnd(&seed) >> 3) - 0x10000000;
            s1[i] = (round & 1) ? (int32_t)rnd(&seed) : (int32_t)(rnd(&seed) >> 3) - 0x10000000;
        }
        run_wma(rnd(&seed) % (MAXN + 1), 1 + (round & 1), 2 + round);
    }

    /* u8: todas las entradas */
    for (i = 0; i < 65536; i++)
        all[i] = (int16_t)i;
    mp368k_pcm_u8_ref(all, u_ref, 65536);
    clob = call_checked((void *)mp368k_pcm_u8_040, (long)all, (long)u_asm, 65536, 0, 0, 0, 0);
    if (clob) fail("u8 registros", 0, (unsigned)clob, 0, 0);
    for (i = 0; i < 65536; i++)
        if (u_ref[i] != u_asm[i]) {
            fail("u8", i, 0, u_ref[i], u_asm[i]);
            break;
        }
    mp368k_pcm_u8_ref(all, u_ref, 0);                   /* n = 0: no escribe */
    call_checked((void *)mp368k_pcm_u8_040, (long)all, (long)u_asm, 0, 0, 0, 0, 0);

    /* gain16 */
    for (round = 0; round < 400; round++) {
        for (i = 0; i < MAXN; i++)
            src16[i] = (int16_t)(round < 4 ? (round == 0 ? 0 : round == 1 ? 32767 : round == 2 ? -32768 : (i & 1 ? 32767 : -32768))
                                           : (int16_t)rnd(&seed));
        n = round < 8 ? round + MAXN - 8 : rnd(&seed) % (MAXN + 1);
        run_gain(src16, n, 1, rnd(&seed) % 300, 0, round);
        run_gain(src16, n, 2, rnd(&seed) % 300, rnd(&seed) % 300, 1000 + round);
    }
    run_gain(src16, 64, 2, 256, 256, 5000);
    run_gain(src16, 64, 2, 0, 0, 5001);
    run_gain(src16, 63, 2, 77, 191, 5002);                /* n impar: el último elemento no se toca */

    say("fallas: "); say_dec(fails); say("\n");
    say(fails ? "RESULTADO: FALLA\n" : "RESULTADO: OK\n");
    return fails != 0;
}
