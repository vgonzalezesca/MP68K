/*
 * test_eq — prueba unitaria del ecualizador (src/eq.c), sin libmad.
 *
 * Compara la ganancia que aplica eq_apply_granule línea por línea contra un modelo independiente
 * escrito aquí con double (el producto no usa float; la prueba sí), para los tres tipos de bloque,
 * varias tasas y ajustes aleatorios. Comprueba también el recorte de ajustes, que sin EQ o con EQ
 * plano no se instala el gancho, la saturación y el cambio de tasa.
 */
#include "eq.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* En la app lo define la libmad parcheada; aquí un gancho ficticio. */
void (*mp368k_eq_hook)(signed int xr[576], unsigned int samplerate, int block) = 0;

static int fails;

static void check(const char *name, int ok)
{
    printf("  %s %s\n", ok ? "OK   " : "FALLA", name);
    if (!ok)
        fails++;
}

/* Ganancia en dB del modelo para una posición en líneas largas (centro de la línea = i + 0,5). */
static double model_db(const EqSettings *s, unsigned rate, double line_pos)
{
    double pos[EQ_BANDS], db[EQ_BANDS];
    int k;

    for (k = 0; k < EQ_BANDS; k++) {
        pos[k] = kEqBandHz[k] * 1152.0 / rate;
        db[k] = 0.5 * (s->band[k] + s->preamp);
        if (db[k] > 24)
            db[k] = 24;
        if (db[k] < -24)
            db[k] = -24;
    }
    if (line_pos <= pos[0])
        return db[0];
    if (line_pos >= pos[EQ_BANDS - 1])
        return db[EQ_BANDS - 1];
    for (k = 0; k + 1 < EQ_BANDS; k++)
        if (line_pos < pos[k + 1])
            return db[k] + (db[k + 1] - db[k]) * (line_pos - pos[k]) / (pos[k + 1] - pos[k]);
    return db[EQ_BANDS - 1];
}

/* Línea larga equivalente de un coeficiente, según el tipo de bloque. */
static double equiv_line(int block, int idx)
{
    if (block == 0 || (block == 2 && idx < 36))
        return idx + 0.5;
    return 3.0 * (6 * (idx / 18) + idx % 6) + 1.5;      /* línea corta f -> línea larga 3f+1 */
}

static void random_settings(EqSettings *s, unsigned *seed)
{
    int i;

    s->enabled = 1;
    for (i = 0; i < EQ_BANDS; i++) {
        *seed = *seed * 1664525u + 1013904223u;
        s->band[i] = (int8_t)((int)((*seed >> 16) % 49) - 24);
    }
    *seed = *seed * 1664525u + 1013904223u;
    s->preamp = (int8_t)((int)((*seed >> 16) % 49) - 24);
}

int main(void)
{
    static const unsigned rates[] = { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 };
    unsigned seed = 368, r;
    int trial, block, i;
    double worst = 0;
    EqSettings s;
    static signed int in[576], out[576];

    printf("== modelo de ganancia (3 tipos de bloque, 9 tasas, 12 ajustes aleatorios)\n");
    for (r = 0; r < sizeof rates / sizeof rates[0]; r++) {
        for (trial = 0; trial < 12; trial++) {
            random_settings(&s, &seed);
            eq_set(&s);
            for (block = 0; block < 3; block++) {
                for (i = 0; i < 576; i++)
                    in[i] = 4000000 + i * 1009;                     /* positivos y distintos: ganancia = out/in */
                memcpy(out, in, sizeof in);
                eq_apply_granule(out, rates[r], block);
                for (i = 0; i < 576; i++) {
                    double want = pow(10.0, model_db(&s, rates[r], equiv_line(block, i)) / 20.0);
                    double got = (double)out[i] / (double)in[i];
                    double err = fabs(got / want - 1.0);

                    if (err > worst)
                        worst = err;
                }
            }
        }
    }
    printf("  error relativo máximo contra el modelo: %.4f\n", worst);
    check("ganancia dentro de 2,5 % del modelo (0,25 dB de cuantización + 1/64 de línea de posición)", worst < 0.025);

    printf("== gancho y ajustes\n");
    eq_flat(&s);
    s.enabled = 1;
    eq_set(&s);
    check("EQ activado pero plano: sin gancho", mp368k_eq_hook == 0 && !eq_is_active());
    s.band[4] = 6;
    s.enabled = 0;
    eq_set(&s);
    check("EQ apagado con ajustes: sin gancho", mp368k_eq_hook == 0 && !eq_is_active());
    s.enabled = 1;
    eq_set(&s);
    check("EQ activo y no plano: gancho instalado", mp368k_eq_hook != 0 && eq_is_active());
    s.band[0] = 100;
    s.band[1] = -100;
    s.preamp = 127;
    eq_set(&s);
    check("los ajustes se recortan a ±24", eq_get()->band[0] == 24 && eq_get()->band[1] == -24 && eq_get()->preamp == 24);
    eq_flat(&s);
    eq_set(&s);
    check("volver a plano quita el gancho", mp368k_eq_hook == 0);

    printf("== saturación y cambio de tasa\n");
    eq_flat(&s);
    s.enabled = 1;
    s.preamp = 24;
    for (i = 0; i < EQ_BANDS; i++)
        s.band[i] = 24;                                             /* +24 dB (15,85x) en todo */
    eq_set(&s);
    for (i = 0; i < 576; i++)
        in[i] = (i % 3 == 0) ? 2000000000 : ((i % 3 == 1) ? -2000000000 : 0);
    memcpy(out, in, sizeof in);
    eq_apply_granule(out, 44100, 0);
    {
        int sat = 1;

        for (i = 0; i < 576; i++) {
            if (i % 3 == 0 && out[i] != 2147483647)
                sat = 0;
            if (i % 3 == 1 && out[i] != (-2147483647 - 1))
                sat = 0;
            if (i % 3 == 2 && out[i] != 0)
                sat = 0;
        }
        check("satura a ±2^31 en vez de desbordar; el cero sigue en cero", sat);
    }
    random_settings(&s, &seed);
    eq_set(&s);
    {
        int ok = 1;

        for (r = 0; r < 4; r++) {
            unsigned rate = r % 2 ? 48000 : 44100;           /* alterna: la tabla debe reconstruirse */

            for (i = 0; i < 576; i++)
                in[i] = 3000000 + i * 777;
            memcpy(out, in, sizeof in);
            eq_apply_granule(out, rate, 0);
            for (i = 0; i < 576; i++) {
                double want = pow(10.0, model_db(&s, rate, i + 0.5) / 20.0);

                if (fabs((double)out[i] / in[i] / want - 1.0) > 0.025)
                    ok = 0;
            }
        }
        check("alternar 44,1 y 48 kHz reconstruye la tabla", ok);
    }
    {
        unsigned long b[3];

        eq_stats(b);
        printf("  granulos contados: largo=%lu corto=%lu mixto=%lu\n", b[0], b[1], b[2]);
    }

    printf("\nRESULTADO: %s\n", fails == 0 ? "todo OK (0 fallas)" : "HAY FALLAS");
    return fails ? 1 : 0;
}
