#!/usr/bin/env python3
"""Genera targets/asm/imdct_l_040.s: la IMDCT de bloques largos (block_type 0) de libmad en ensamblador 68040.

Transcribe fastsdct/sdctII/dctIV/imdct36 de third_party/libmad/layer3.c (constantes leídas de la fuente por
tools/libmad_imdct.py) sobre valores simbólicos (dflow.py), añade la ventana window_l, el solapado y la inversión de
frecuencias, y emite el asm con asignación de registros. Referencia en C: src/imdct_ref.c (el texto de libmad).
Uso: targets/asm/gen/gen_imdct.py [salida]
"""
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", "..", ".."))
sys.path.insert(0, here)
sys.path.insert(0, os.path.join(root, "tools"))
import dflow
import libmad_imdct
from dflow import half, inp, mul28, store, Mem

d = libmad_imdct.extract(os.path.join(root, "third_party", "libmad", "layer3.c"))
out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "targets", "asm", "imdct_l_040.s")
C = d["c"]


def fastsdct(x):
    """x: 9 valores. Devuelve y[0..16] (solo los índices pares), como fastsdct de libmad."""
    a0 = x[3] + x[5]
    a1 = x[3] - x[5]
    a2 = x[6] + x[2]
    a3 = x[6] - x[2]
    a4 = x[1] + x[7]
    a5 = x[1] - x[7]
    a6 = x[8] + x[0]
    a7 = x[8] - x[0]
    a8 = a0 + a2
    a9 = a0 - a2
    a10 = a0 - a6
    a11 = a2 - a6
    a12 = a8 + a6
    a13 = a1 - a3
    a14 = a13 + a7
    a15 = a3 + a7
    a16 = a1 - a7
    a17 = a1 + a3
    m0 = mul28(a17, -C[3])
    m1 = mul28(a16, -C[0])
    m2 = mul28(a15, -C[4])
    m3 = mul28(a14, -C[1])
    m4 = mul28(a5, -C[1])
    m5 = mul28(a11, -C[6])
    m6 = mul28(a10, -C[5])
    m7 = mul28(a9, -C[2])
    a18 = x[4] + a4
    a19 = 2 * x[4] - a4
    a20 = a19 + m5
    a21 = a19 - m5
    a22 = a19 + m6
    a23 = m4 + m2
    a24 = m4 - m2
    a25 = m4 + m1
    y = {}
    y[0] = a18 + a12
    y[2] = m0 - a25
    y[4] = m7 - a20
    y[6] = m3
    y[8] = a21 - m6
    y[10] = a24 - m1
    y[12] = a12 - 2 * a18
    y[14] = a23 + m0
    y[16] = a22 + m7
    return y


def sdctII(x):
    t = [x[i] + x[17 - i] for i in range(9)]
    ye = fastsdct(t)
    t = [mul28(x[i] - x[17 - i], d["scale9"][i]) for i in range(9)]
    yo = fastsdct(t)
    X = [None] * 18
    for k in range(0, 17, 2):
        X[k] = ye[k]
        X[k + 1] = yo[k]
    for i in (3, 11):
        for j in (0, 2, 4, 6):
            X[i + j] = X[i + j] - X[i + j - 2]
    return X


def dctIV(y):
    t = [mul28(y[i], d["scale18"][i]) for i in range(18)]
    X = sdctII(t)
    X[0] = half(X[0])
    for i in range(1, 17):
        X[i] = half(X[i]) - X[i - 1]
    X[17] = half(X[17]) - X[16]
    return X


def imdct36(x):
    t = dctIV(x)
    y = [None] * 36
    for i in range(9):
        y[i] = t[9 + i]
    for i in range(9, 27):
        y[i] = -t[36 - (9 + i) - 1]
    for i in range(27, 36):
        y[i] = -t[i - 27]
    return y


with dflow.Prog() as prog:
    X = [inp(Mem("%a0", 4 * i)) for i in range(18)]
    y = imdct36(X)
    z = [mul28(y[i], d["window_l"][i]) for i in range(36)]
    for i in range(18):
        s = z[i] + inp(Mem("%a1", 4 * i))       # lee overlap[i] antes de reemplazarlo
        store(Mem("%a2", 128 * i), s)           # sample[i][sb]: 32 palabras por fila
        store(Mem("%a1", 4 * i), z[i + 18])

body, nslots = dflow.emit_kernel(prog, "mp368k_imdct_l_040", None, None)
frame = 4 * nslots
odd = "\n".join(f"        neg.l   {128 * i}(%a2)" for i in range(1, 18, 2))
txt = f"""| imdct_l_040.s — IMDCT de bloques largos (block_type 0) de libmad, con ventana, solapado e inversión, en ensamblador 68040.
| GENERADO por targets/asm/gen/gen_imdct.py (dflow.py) desde third_party/libmad/layer3.c: no editar a mano.
| Referencia en C: src/imdct_ref.c (el texto de libmad). Prueba: tests/asm/test_imdct_l.c bajo qemu-m68k.
| SIN MEDIR en el LC 580: no se declara más rápido que la referencia hasta medirlo (ADR-008).
|
| void mp368k_imdct_l_040(const int32_t X[18], int32_t overlap[18], int32_t *sample, int invert);
| Aritmética de libmad con FPM_64BIT: sumas de 32 bits con envoltura; cada producto es
| (int32)(((int64)x*y) >> 28) = (hi << 4) | (lo >>> 28) de muls.l; X/2 de C redondea hacia cero.
| d1 = parte alta del producto, d7 = 28, d0/d2-d6 = valores, huecos de pila para los que no caben.
| Preserva d2-d7/a2-a6 (convención de GCC); a0 = X, a1 = overlap, a2 = sample.
| Hay {len(prog.ops)} operaciones y {nslots} huecos de pila ({frame} bytes).

        .text
        .even
        .globl  mp368k_imdct_l_040
mp368k_imdct_l_040:
        movem.l %d2-%d7/%a2,-(%sp)
        move.l  32(%sp),%a0                 | X
        move.l  36(%sp),%a1                 | overlap
        move.l  40(%sp),%a2                 | sample
        moveq   #28,%d7
"""
if frame:
    txt += f"        lea     -{frame}(%sp),%sp\n"
txt += body + "\n"
if frame:
    txt += f"        lea     {frame}(%sp),%sp\n"
txt += f"""        tst.l   44(%sp)                     | invert
        beq.s   1f
{odd}
1:      movem.l (%sp)+,%d2-%d7/%a2
        rts
"""
open(out_path, "w").write(txt)
print(f"gen_imdct: {out_path}  ({len(prog.ops)} operaciones, {nslots} huecos de pila)")
