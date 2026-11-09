/*
 * wma_ref — referencia en C de los núcleos de WMA (ver wma_kernels.h). Misma aritmética que fixmul32b de libwma:
 * producto de 64 bits desplazado 31 con truncado; suma de 32 bits con envoltura (uint32_t para que esté definida).
 */
#include "wma_kernels.h"

static uint32_t fixmul32b_ref(int32_t x, int32_t y)
{
    return (uint32_t)(int32_t)(((int64_t)x * y) >> 31);
}

void mp368k_wma_fmul_add_add_ref(int32_t *dst, const int32_t *src0, const int32_t *src1, int len)
{
    int i;

    for (i = 0; i < len; i++)
        dst[i] = (int32_t)((uint32_t)dst[i] + fixmul32b_ref(src0[i], src1[i]));
}

void mp368k_wma_fmul_reverse_ref(int32_t *dst, const int32_t *src0, const int32_t *src1, int len)
{
    int i;

    for (i = 0; i < len; i++)
        dst[i] = (int32_t)fixmul32b_ref(src0[i], src1[len - 1 - i]);
}

/* ------------------------------------------------------------------------ */
/* Pasada de la FFT split-radix (pass de third_party/libwma/fft-ffmpeg.c)      */
/* ------------------------------------------------------------------------ */

/* MULT31 de libwma: parte alta del producto de 64 bits, desplazada 1 (la envoltura de 32 bits está definida en uint32_t). */
static uint32_t mult31(int32_t x, int32_t y)
{
    return (uint32_t)(int32_t)(((int64_t)x * y) >> 32) << 1;
}

/* Cuatro cuartos z[0], z[n], z[2n], z[3n] (re, im de 32 bits): mariposa con t1, t2 (de z[2n]) y t5, t6 (de z[3n]). */
static void butterflies(uint32_t *z, unsigned int n, uint32_t t1, uint32_t t2, uint32_t t5, uint32_t t6)
{
    uint32_t *a0 = z, *a1 = z + 2 * n, *a2 = z + 4 * n, *a3 = z + 6 * n;
    uint32_t temp1, temp2;

    temp1 = t5 - t1;
    temp2 = t5 + t1;
    a2[0] = a0[0] - temp2;
    a0[0] = a0[0] + temp2;
    a3[1] = a1[1] - temp1;
    a1[1] = a1[1] + temp1;
    temp1 = t2 - t6;
    temp2 = t2 + t6;
    a3[0] = a1[0] - temp1;
    a1[0] = a1[0] + temp1;
    a2[1] = a0[1] - temp2;
    a0[1] = a0[1] + temp2;
}

/* TRANSFORM_ZERO: sin giro (w = 1) */
static uint32_t *xf_zero(uint32_t *z, unsigned int n)
{
    butterflies(z, n, z[4 * n], z[4 * n + 1], z[6 * n], z[6 * n + 1]);
    return z + 2;
}

/* TRANSFORM con el giro (wre, wim): t1,t2 = XPROD31(z[2n], w), t5,t6 = XNPROD31(z[3n], w) */
static uint32_t *xf(uint32_t *z, unsigned int n, int32_t wre, int32_t wim)
{
    int32_t re2 = (int32_t)z[4 * n], im2 = (int32_t)z[4 * n + 1], re3 = (int32_t)z[6 * n], im3 = (int32_t)z[6 * n + 1];
    uint32_t t1 = mult31(re2, wre) + mult31(im2, wim);
    uint32_t t2 = mult31(im2, wre) - mult31(re2, wim);
    uint32_t t5 = mult31(re3, wre) - mult31(im3, wim);
    uint32_t t6 = mult31(im3, wre) + mult31(re3, wim);

    butterflies(z, n, t1, t2, t5, t6);
    return z + 2;
}

void mp368k_wma_fft_pass_ref(int32_t *z_arg, unsigned int STEP, unsigned int n, const int32_t *T)
{
    uint32_t *z = (uint32_t *)z_arg;
    const int32_t *w = T + STEP;
    const int32_t *w_end = T + 1024;

    z = xf_zero(z, n);
    z = xf(z, n, w[1], w[0]);                   /* TRANSFORM_W10: wim = w[0], wre = w[1] */
    w += STEP;
    do {
        z = xf(z, n, w[1], w[0]);
        w += STEP;
        z = xf(z, n, w[1], w[0]);
        w += STEP;
    } while (w < w_end);
    w_end = T;
    while (w > w_end) {
        z = xf(z, n, w[0], w[1]);               /* TRANSFORM_W01: wre = w[0], wim = w[1] */
        w -= STEP;
        z = xf(z, n, w[0], w[1]);
        w -= STEP;
    }
}
