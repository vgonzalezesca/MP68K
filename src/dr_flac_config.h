/*
 * Opciones de dr_flac que deben ser iguales en todos los archivos que lo incluyen
 * (cambian el diseño de sus estructuras). Se incluye antes que dr_flac.h.
 *
 *   NO_STDIO / NO_WCHAR  no usamos sus funciones de archivo (la E/S va por callbacks)
 *   NO_OGG               solo FLAC nativo; el FLAC dentro de Ogg queda fuera de alcance
 *   NO_SIMD              sin SSE/NEON: el objetivo es un 68LC040 (y el PC se prueba igual)
 */
#ifndef MP368K_DR_FLAC_CONFIG_H
#define MP368K_DR_FLAC_CONFIG_H

#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_WCHAR
#define DR_FLAC_NO_OGG
#define DR_FLAC_NO_SIMD

#endif
