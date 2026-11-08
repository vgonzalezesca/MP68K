/*
 * eq_ref — referencia en C del núcleo del ecualizador (ver eq_kernels.h). Misma aritmética que mulg() de eq.c.
 */
#include "eq_kernels.h"

void mp368k_eq_mulg_range_ref(int32_t *xr, const int32_t *gain, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < n; i++) {
        long long p;

        if (gain[i] == EQ_UNITY_GAIN)
            continue;
        p = (long long)xr[i] * (long long)gain[i];
        p = (p + (1LL << 23)) >> 24;
        if (p > 2147483647LL)
            xr[i] = 2147483647;
        else if (p < -2147483647LL - 1)
            xr[i] = (-2147483647 - 1);
        else
            xr[i] = (int32_t)p;
    }
}
