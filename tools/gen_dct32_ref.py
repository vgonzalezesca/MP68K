#!/usr/bin/env python3
"""Genera src/dct32_ref.c: el dct32 de libmad (third_party/libmad/synth.c) como referencia en C del kernel asm.

Uso: tools/gen_dct32_ref.py [salida]     (por omisión src/dct32_ref.c)
El texto de las sentencias es el de libmad, sin cambios: la referencia es la misma aritmética.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import libmad_dct32

root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
consts, stmts = libmad_dct32.extract(os.path.join(root, "third_party", "libmad", "synth.c"))
out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "src", "dct32_ref.c")

names = sorted({int(m) for s in stmts for m in re.findall(r"\bt(\d+)\b", s)})
lines = [
    "/*",
    " * dct32_ref — DCT de 32 puntos de libmad como referencia en C del kernel asm (src/synth_kernels.h).",
    " * GENERADO por tools/gen_dct32_ref.py desde third_party/libmad/synth.c: no editar a mano.",
    " * libmad: Copyright (C) 2000-2004 Underbit Technologies, Inc. (GPL v2 o posterior).",
    " * Aritmética de 32 bits con envoltura; MUL es mad_f_mul con FPM_64BIT: (int32)(((int64)x*y) >> 28).",
    " * Compilar con -fwrapv (la envoltura de enteros con signo es intencional).",
    " */",
    '#include "synth_kernels.h"',
    "",
    "typedef int32_t fx;",
    "#define MUL(x, y)  ((fx)(((int64_t)(x) * (y)) >> 28))",
    "#define SHIFT(x)   (x)",
]
for n in sorted(consts):
    lines.append("#define costab%d ((fx)%d)" % (n, consts[n]))
lines += [
    "",
    "void mp368k_dct32_ref(const int32_t in[32], unsigned int slot, int32_t lo[16][8], int32_t hi[16][8])",
    "{",
    "    fx " + ", ".join("t%d" % n for n in names) + ";",
    "",
]
for s in stmts:
    lines.append("    " + re.sub(r"\s+", " ", s) + ";")
lines.append("}")
open(out_path, "w").write("\n".join(lines) + "\n")
print("gen_dct32_ref: %s (%d sentencias)" % (out_path, len(stmts)))
