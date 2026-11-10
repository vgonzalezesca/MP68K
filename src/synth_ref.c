/*
 * synth_ref — referencia en C de la ventana de síntesis (ver synth_kernels.h).
 *
 * Reproduce el orden y la aritmética de synth_full de libmad (FPM_64BIT sin OPT_*): producto de 64 bits
 * truncado a >> 16 y suma de 32 bits con envoltura. Ese 16 no es el 28 del resto de libmad: en synth.c, sin
 * OPT_SSO, MAD_F_SCALEBITS pasa a FRACBITS - 12 y la tabla D ya viene desplazada 12 bits (PRESHIFT), así que
 * D que recibe el núcleo es la tabla ya desplazada. Se acumula en uint32_t para que la envoltura esté definida.
 * La suma es conmutativa, así que el orden de los términos no importa.
 */
#include "synth_kernels.h"

static uint32_t mulf(int32_t x, int32_t y)
{
    return (uint32_t)(int32_t)(((int64_t)x * y) >> 16);
}

/* Índice dentro de la fila de D de las "mitades +": 0, 14, 12, ..., 2 para j = 0..7. */
#define IDX(j) ((j) == 0 ? 0 : 16 - 2 * (j))

void mp368k_synth_window_full_ref(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                  const int32_t *D, unsigned int pe, unsigned int po, int32_t *pcm)
{
    unsigned int sb;
    int j;
    uint32_t a;
    const int32_t *p, *f;

    /* subbanda 0 */
    a = 0;
    p = D + po;
    for (j = 0; j < 8; j++)
        a += mulf(fx[j], p[IDX(j)]);
    a = 0u - a;
    p = D + pe;
    for (j = 0; j < 8; j++)
        a += mulf(fe[j], p[IDX(j)]);
    pcm[0] = (int32_t)a;

    for (sb = 1; sb < 16; sb++) {
        const int32_t *fer = fe + 8 * sb;           /* fila sb de fe */
        const int32_t *for_ = fo + 8 * (sb - 1);    /* fila sb-1 de fo */
        const int32_t *row = D + 32 * sb;

        a = 0;
        p = row + po;
        for (j = 0; j < 8; j++)
            a += mulf(for_[j], p[IDX(j)]);
        a = 0u - a;
        p = row + pe;
        for (j = 0; j < 8; j++)
            a += mulf(fer[j], p[IDX(j)]);
        pcm[sb] = (int32_t)a;

        a = 0;
        p = row - pe;
        for (j = 0; j < 8; j++)
            a += mulf(fer[j], p[15 + 2 * j]);
        p = row - po;
        for (j = 0; j < 8; j++)
            a += mulf(for_[j], p[15 + 2 * j]);
        pcm[32 - sb] = (int32_t)a;
    }

    /* subbanda 16: fila 15 de fo y fila D[16] */
    f = fo + 8 * 15;
    a = 0;
    p = D + 32 * 16 + po;
    for (j = 0; j < 8; j++)
        a += mulf(f[j], p[IDX(j)]);
    pcm[16] = (int32_t)(0u - a);
}

/* Media tasa (synth_half): solo las subbandas pares; las salidas van a pcm[sb/2] y pcm[16 - sb/2]. */
void mp368k_synth_window_half_ref(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                  const int32_t *D, unsigned int pe, unsigned int po, int32_t *pcm)
{
    unsigned int sb;
    int j;
    uint32_t a;
    const int32_t *p, *f;

    a = 0;
    p = D + po;
    for (j = 0; j < 8; j++)
        a += mulf(fx[j], p[IDX(j)]);
    a = 0u - a;
    p = D + pe;
    for (j = 0; j < 8; j++)
        a += mulf(fe[j], p[IDX(j)]);
    pcm[0] = (int32_t)a;

    for (sb = 2; sb < 16; sb += 2) {
        const int32_t *fer = fe + 8 * sb;
        const int32_t *for_ = fo + 8 * (sb - 1);
        const int32_t *row = D + 32 * sb;

        a = 0;
        p = row + po;
        for (j = 0; j < 8; j++)
            a += mulf(for_[j], p[IDX(j)]);
        a = 0u - a;
        p = row + pe;
        for (j = 0; j < 8; j++)
            a += mulf(fer[j], p[IDX(j)]);
        pcm[sb / 2] = (int32_t)a;

        a = 0;
        p = row - pe;
        for (j = 0; j < 8; j++)
            a += mulf(fer[j], p[15 + 2 * j]);
        p = row - po;
        for (j = 0; j < 8; j++)
            a += mulf(for_[j], p[15 + 2 * j]);
        pcm[16 - sb / 2] = (int32_t)a;
    }

    f = fo + 8 * 15;
    a = 0;
    p = D + 32 * 16 + po;
    for (j = 0; j < 8; j++)
        a += mulf(f[j], p[IDX(j)]);
    pcm[8] = (int32_t)(0u - a);
}

/*
 * Ventana de media tasa de PRECISIÓN REDUCIDA (perfiles HALF y HALF_MONO): x se toma como su palabra alta (x >> 16, con signo)
 * y la ventana como D16 = tabla de 16 bits ya reducida (ver synth_kernels.h); cada producto es un
 * 16x16->32 entero (muls.w en el 68040, sin desplazamientos), se suman en 32 bits con envoltura y la salida es la suma << 2,
 * en la misma escala que la ventana exacta. Mismo recorrido que mp368k_synth_window_half_ref.
 */
static uint32_t mul16(int32_t x, int16_t d)
{
    return (uint32_t)((int32_t)(int16_t)((uint32_t)x >> 16) * (int32_t)d);
}

void mp368k_synth_window_half16_ref(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                    const int16_t *D, unsigned int pe, unsigned int po, int32_t *pcm)
{
    unsigned int sb;
    int j;
    uint32_t a;
    const int16_t *p;
    const int32_t *f;

    a = 0;
    p = D + po;
    for (j = 0; j < 8; j++)
        a += mul16(fx[j], p[IDX(j)]);
    a = 0u - a;
    p = D + pe;
    for (j = 0; j < 8; j++)
        a += mul16(fe[j], p[IDX(j)]);
    pcm[0] = (int32_t)(a << 2);

    for (sb = 2; sb < 16; sb += 2) {
        const int32_t *fer = fe + 8 * sb;
        const int32_t *for_ = fo + 8 * (sb - 1);
        const int16_t *row = D + 32 * sb;

        a = 0;
        p = row + po;
        for (j = 0; j < 8; j++)
            a += mul16(for_[j], p[IDX(j)]);
        a = 0u - a;
        p = row + pe;
        for (j = 0; j < 8; j++)
            a += mul16(fer[j], p[IDX(j)]);
        pcm[sb / 2] = (int32_t)(a << 2);

        a = 0;
        p = row - pe;
        for (j = 0; j < 8; j++)
            a += mul16(fer[j], p[15 + 2 * j]);
        p = row - po;
        for (j = 0; j < 8; j++)
            a += mul16(for_[j], p[15 + 2 * j]);
        pcm[16 - sb / 2] = (int32_t)(a << 2);
    }

    f = fo + 8 * 15;
    a = 0;
    p = D + 32 * 16 + po;
    for (j = 0; j < 8; j++)
        a += mul16(f[j], p[IDX(j)]);
    pcm[8] = (int32_t)((0u - a) << 2);
}

/* La ventana COMPLETA de precisión reducida (perfil MONO): las mismas cuentas que mp368k_synth_window_half16_ref, con el recorrido
 * de mp368k_synth_window_full_ref. */
void mp368k_synth_window_full16_ref(const int32_t *fx, const int32_t *fe, const int32_t *fo,
                                    const int16_t *D, unsigned int pe, unsigned int po, int32_t *pcm)
{
    unsigned int sb;
    int j;
    uint32_t a;
    const int16_t *p;
    const int32_t *f;

    a = 0;
    p = D + po;
    for (j = 0; j < 8; j++)
        a += mul16(fx[j], p[IDX(j)]);
    a = 0u - a;
    p = D + pe;
    for (j = 0; j < 8; j++)
        a += mul16(fe[j], p[IDX(j)]);
    pcm[0] = (int32_t)(a << 2);

    for (sb = 1; sb < 16; sb++) {
        const int32_t *fer = fe + 8 * sb;
        const int32_t *for_ = fo + 8 * (sb - 1);
        const int16_t *row = D + 32 * sb;

        a = 0;
        p = row + po;
        for (j = 0; j < 8; j++)
            a += mul16(for_[j], p[IDX(j)]);
        a = 0u - a;
        p = row + pe;
        for (j = 0; j < 8; j++)
            a += mul16(fer[j], p[IDX(j)]);
        pcm[sb] = (int32_t)(a << 2);

        a = 0;
        p = row - pe;
        for (j = 0; j < 8; j++)
            a += mul16(fer[j], p[15 + 2 * j]);
        p = row - po;
        for (j = 0; j < 8; j++)
            a += mul16(for_[j], p[15 + 2 * j]);
        pcm[32 - sb] = (int32_t)(a << 2);
    }

    f = fo + 8 * 15;
    a = 0;
    p = D + 32 * 16 + po;
    for (j = 0; j < 8; j++)
        a += mul16(f[j], p[IDX(j)]);
    pcm[16] = (int32_t)((0u - a) << 2);
}
