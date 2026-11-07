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
