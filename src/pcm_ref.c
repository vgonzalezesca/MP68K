/*
 * pcm_ref — referencias en C de las conversiones de PCM (ver pcm_kernels.h). Aritmética de 32 bits con envoltura.
 */
#include "pcm_kernels.h"

static int16_t scale16(int32_t sample)
{
    /* sample += 2^12 envuelve igual que el add.l del asm (y que scale() de libmad en la práctica) */
    sample = (int32_t)((uint32_t)sample + (1UL << 12));
    if (sample >= (1L << 28))
        sample = (1L << 28) - 1;
    else if (sample < -(1L << 28))
        sample = -(1L << 28);
    return (int16_t)(sample >> 13);
}

void mp368k_pcm_scale16_ref(const int32_t *s0, const int32_t *s1, int16_t *out, unsigned int len, unsigned int nch)
{
    unsigned int i;

    if (nch == 1) {
        for (i = 0; i < len; i++)
            out[i] = scale16(s0[i]);
        return;
    }
    for (i = 0; i < len; i++) {
        out[2 * i] = scale16(s0[i]);
        out[2 * i + 1] = scale16(s1[i]);
    }
}

static int16_t wma16(int32_t v)
{
    int32_t s = (int32_t)(8192u - (uint32_t)v) >> 14;       /* -v + 2^13, con envoltura como el asm */

    return (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
}

void mp368k_pcm_wma16_ref(const int32_t *s0, const int32_t *s1, int16_t *out, unsigned int len, unsigned int nch)
{
    unsigned int i;

    if (nch == 1) {
        for (i = 0; i < len; i++)
            out[i] = wma16(s0[i]);
        return;
    }
    for (i = 0; i < len; i++) {
        out[2 * i] = wma16(s0[i]);
        out[2 * i + 1] = wma16(s1[i]);
    }
}

void mp368k_pcm_u8_ref(const int16_t *src, uint8_t *dst, unsigned long n)
{
    unsigned long i;

    for (i = 0; i < n; i++) {
        long v = ((long)src[i] + 128) >> 8;     /* redondeo; floor para negativos */

        if (v > 127)
            v = 127;
        dst[i] = (uint8_t)(v + 128);
    }
}

void mp368k_pcm_gain16_ref(const int16_t *src, int16_t *dst, unsigned long n, unsigned int channels,
                           unsigned int gl, unsigned int gr)
{
    unsigned long i;

    if (gl > 256) gl = 256;
    if (gr > 256) gr = 256;
    if (channels == 1) {
        for (i = 0; i < n; i++)
            dst[i] = (int16_t)(((long)src[i] * (long)gl) >> 8);
        return;
    }
    for (i = 0; i + 1 < n; i += 2) {
        dst[i] = (int16_t)(((long)src[i] * (long)gl) >> 8);
        dst[i + 1] = (int16_t)(((long)src[i + 1] * (long)gr) >> 8);
    }
}
