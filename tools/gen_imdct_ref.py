#!/usr/bin/env python3
"""Genera src/imdct_ref.c: la IMDCT de bloques largos de libmad (layer3.c) como referencia en C del kernel asm.

Uso: tools/gen_imdct_ref.py [salida]     (por omisión src/imdct_ref.c)
Las funciones fastsdct/sdctII/dctIV/imdct36 son el texto de libmad, sin cambios. mp368k_imdct_l_ref compone lo mismo
que III_imdct_l (ventana normal, block_type 0) + III_overlap + III_freqinver de una subbanda.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import libmad_imdct

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
d = libmad_imdct.extract(os.path.join(root, "third_party", "libmad", "layer3.c"))
out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "src", "imdct_ref.c")

wl = ",\n    ".join(", ".join("%d" % v for v in d["window_l"][i:i + 6]) for i in range(0, 36, 6))
text = """/*
 * imdct_ref — IMDCT de bloques largos de libmad como referencia en C del kernel asm (src/synth_kernels.h).
 * GENERADO por tools/gen_imdct_ref.py desde third_party/libmad/layer3.c: no editar a mano.
 * libmad: Copyright (C) 2000-2004 Underbit Technologies, Inc. (GPL v2 o posterior).
 * Las cuatro funciones de abajo (fastsdct, sdctII, dctIV, imdct36) son el texto de libmad. Aritmética de 32 bits con
 * envoltura; mad_f_mul es la de FPM_64BIT: (int32)(((int64)x*y) >> 28). Compilar con -fwrapv.
 */
#include "synth_kernels.h"

/* fastsdct escribe en X + 1 (y[0..16] pares): correcto, pero GCC lo ve como acceso fuera del arreglo */
#pragma GCC diagnostic ignored "-Wstringop-overflow"

typedef int32_t mad_fixed_t;
#define MAD_F(x) ((mad_fixed_t)(x))
#define mad_f_mul(x, y) ((mad_fixed_t)(((int64_t)(x) * (y)) >> 28))

/* window_l[i] = sin((PI / 36) * (i + 1/2)) */
static const mad_fixed_t window_l[36] = {
    %s
};

%s
void mp368k_imdct_l_ref(const int32_t X[18], int32_t overlap[18], int32_t *sample, int invert)
{
    mad_fixed_t z[36];
    unsigned int i;

    imdct36(X, z);
    for (i = 0; i < 36; i += 4) {
        z[i + 0] = mad_f_mul(z[i + 0], window_l[i + 0]);
        z[i + 1] = mad_f_mul(z[i + 1], window_l[i + 1]);
        z[i + 2] = mad_f_mul(z[i + 2], window_l[i + 2]);
        z[i + 3] = mad_f_mul(z[i + 3], window_l[i + 3]);
    }
    for (i = 0; i < 18; i++) {
        sample[32 * i] = z[i] + overlap[i];
        overlap[i] = z[i + 18];
    }
    if (invert)
        for (i = 1; i < 18; i += 2)
            sample[32 * i] = -sample[32 * i];
}
""" % (wl, d["code"])
open(out_path, "w").write(text)
print("gen_imdct_ref: %s" % out_path)
