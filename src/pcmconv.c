/*
 * pcmconv — ver pcmconv.h.
 */
#include "pcmconv.h"

void pcm_s16_to_u8(const int16_t *src, uint8_t *dst, unsigned long n)
{
    unsigned long i;

    for (i = 0; i < n; i++) {
        long v = ((long)src[i] + 128) >> 8;     /* redondeo; floor para negativos */

        if (v > 127)
            v = 127;
        dst[i] = (uint8_t)(v + 128);
    }
}

void pcm_apply_gain16(const int16_t *src, int16_t *dst, unsigned long n,
                      unsigned channels, unsigned gl, unsigned gr)
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
