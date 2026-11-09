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
