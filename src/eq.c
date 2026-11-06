/*
 * eq — ver eq.h. Solo enteros: nada de float ni double (regla 1 de CLAUDE.md).
 */
#include "eq.h"

#include <string.h>

#include "eq_tables.h"

typedef signed int coef_t;      /* mad_fixed_t */

/* Lo define la libmad parcheada (third_party/patches/libmad-0.15.1b-eq.patch). */
extern void (*mp368k_eq_hook)(coef_t xr[576], unsigned int samplerate, int block);

const unsigned short kEqBandHz[EQ_BANDS] = { 70, 180, 320, 600, 1000, 3000, 6000, 12000, 14000, 16000 };

static EqSettings sSet;
static int sActive;
static unsigned int sRate;          /* tasa para la que está construida sGain; 0 = sin construir */
static int32_t sGain[576];          /* ganancia Q24 por línea larga */
static int sFirst, sLast;           /* primera y última línea con ganancia distinta de 1,0 */
#ifdef MP368K_EQ_STATS
static unsigned long sBlocks[3];    /* granulos procesados por tipo de bloque (solo en las pruebas) */

void eq_stats(unsigned long out[3])
{
    out[0] = sBlocks[0];
    out[1] = sBlocks[1];
    out[2] = sBlocks[2];
}
#endif

void eq_flat(EqSettings *s)
{
    int i;

    for (i = 0; i < EQ_BANDS; i++)
        s->band[i] = 0;
    s->preamp = 0;
}

static int8_t clamp_step(int v)
{
    return (int8_t)(v > EQ_STEP_MAX ? EQ_STEP_MAX : (v < -EQ_STEP_MAX ? -EQ_STEP_MAX : v));
}

static int is_flat(const EqSettings *s)
{
    int i;

    if (s->preamp != 0)
        return 0;
    for (i = 0; i < EQ_BANDS; i++)
        if (s->band[i] != 0)
            return 0;
    return 1;
}

void eq_set(const EqSettings *s)
{
    int i;

    sSet.enabled = s->enabled != 0;
    for (i = 0; i < EQ_BANDS; i++)
        sSet.band[i] = clamp_step(s->band[i]);
    sSet.preamp = clamp_step(s->preamp);
    sRate = 0;                                      /* la tabla se reconstruye con el próximo granulo */
    sActive = sSet.enabled && !is_flat(&sSet);
    mp368k_eq_hook = sActive ? eq_apply_granule : 0;
}

const EqSettings *eq_get(void)
{
    return &sSet;
}

int eq_is_active(void)
{
    return sActive;
}

/*
 * Construye la ganancia de cada línea larga para la tasa `rate`. El centro de cada banda cae en la
 * línea f·1152/fs (la línea i está en (i+0,5)·(fs/2)/576); se mide en 1/64 de línea (con 1/16 la pendiente de
 * ±12 dB entre bandas de graves, ~8 dB por línea, dejaba hasta 0,5 dB de error de posición). La ganancia en
 * cuartos de dB se interpola linealmente entre centros y queda plana por debajo de la primera banda y
 * por encima de la última.
 */
static void rebuild(unsigned int rate)
{
    unsigned long pos[EQ_BANDS];
    int db[EQ_BANDS], i, k;

    for (k = 0; k < EQ_BANDS; k++) {
        int d = 2 * ((int)sSet.band[k] + (int)sSet.preamp);

        pos[k] = (unsigned long)kEqBandHz[k] * 1152UL * 64UL / rate;     /* 16000·1152·64 = 1,18e9: cabe en 32 bits */
        db[k] = d > EQ_DB_MAX ? EQ_DB_MAX : (d < EQ_DB_MIN ? EQ_DB_MIN : d);
    }
    sFirst = 576;
    sLast = -1;
    k = 0;
    for (i = 0; i < 576; i++) {
        unsigned long p = (unsigned long)i * 64UL + 32UL;
        int d;

        if (p <= pos[0]) {
            d = db[0];
        } else if (p >= pos[EQ_BANDS - 1]) {
            d = db[EQ_BANDS - 1];
        } else {
            while (k + 1 < EQ_BANDS - 1 && p >= pos[k + 1])
                k++;
            long num = (long)(db[k + 1] - db[k]) * (long)(p - pos[k]);
            long den = (long)(pos[k + 1] - pos[k]);

            d = db[k] + (int)((num + (num >= 0 ? den / 2 : -(den / 2))) / den);       /* redondeado, no truncado */
        }
        sGain[i] = kEqGainQ24[d - EQ_DB_MIN];
        if (sGain[i] != EQ_UNITY) {
            if (i < sFirst)
                sFirst = i;
            sLast = i;
        }
    }
    sRate = rate;
}

/* x·g con g en Q24, redondeado y saturado a 32 bits (un desborde sería un chasquido fuerte). */
static coef_t mulg(coef_t x, int32_t g)
{
    long long p = (long long)x * (long long)g;

    p = (p + (1LL << 23)) >> 24;
    if (p > 2147483647LL)
        return 2147483647;
    if (p < -2147483647LL - 1)
        return (-2147483647 - 1);
    return (coef_t)p;
}

void eq_apply_granule(coef_t xr[576], unsigned int rate, int block)
{
    int sb, w, k, i;

    if (rate == 0)
        return;
#ifdef MP368K_EQ_STATS
    if (block >= 0 && block < 3)
        sBlocks[block]++;
#endif
    if (rate != sRate)
        rebuild(rate);
    if (sFirst > sLast)
        return;

    if (block == 0) {                               /* largo: la línea i es la frecuencia i */
        for (i = sFirst; i <= sLast; i++)
            if (sGain[i] != EQ_UNITY)
                xr[i] = mulg(xr[i], sGain[i]);
        return;
    }

    /*
     * Corto o mixto. Tras III_reorder cada subbanda trae 3 ventanas de 6 líneas (18·sb + 6·w + k) y la
     * frecuencia corta es f = 6·sb + k, equivalente a la línea larga 3f+1. En un bloque mixto, los
     * primeros 36 coeficientes (2 subbandas) son largos.
     */
    sb = 0;
    if (block == 2) {
        for (i = 0; i < 36; i++)
            if (sGain[i] != EQ_UNITY)
                xr[i] = mulg(xr[i], sGain[i]);
        sb = 2;
    }
    for (; sb < 32; sb++) {
        for (w = 0; w < 3; w++) {
            for (k = 0; k < 6; k++) {
                int32_t g = sGain[3 * (sb * 6 + k) + 1];

                if (g != EQ_UNITY) {
                    i = sb * 18 + w * 6 + k;
                    xr[i] = mulg(xr[i], g);
                }
            }
        }
    }
}
