/*
 * pcmconv — ver pcmconv.h. El trabajo lo hace el núcleo elegido por el build (src/pcm_kernels.h):
 * la referencia en C (src/pcm_ref.c) o, en la rama 68k, el asm.
 */
#include "pcmconv.h"
#include "pcm_kernels.h"

void pcm_s16_to_u8(const int16_t *src, uint8_t *dst, unsigned long n)
{
    MP368K_PCM_U8(src, dst, n);
}

void pcm_apply_gain16(const int16_t *src, int16_t *dst, unsigned long n,
                      unsigned channels, unsigned gl, unsigned gr)
{
    MP368K_PCM_GAIN16(src, dst, n, channels, gl, gr);
}
