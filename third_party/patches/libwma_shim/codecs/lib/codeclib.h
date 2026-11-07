/* mp368k: sustituto mínimo de codeclib.h de Rockbox (solo lo que usan libwma, mdct y fft). */
#ifndef MP368K_SHIM_CODECLIB_H
#define MP368K_SHIM_CODECLIB_H
#include <codecs.h>
#include "mdct.h"
#include "fft.h"
extern void ff_imdct_calc(unsigned int nbits, int32_t *output, const int32_t *input);
extern void ff_imdct_half(unsigned int nbits, int32_t *output, const int32_t *input);
static inline int av_log2(unsigned int v)
{
    int n = 0;

    while (v >>= 1)
        n++;
    return n;
}
#endif
