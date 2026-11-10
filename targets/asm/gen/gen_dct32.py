#!/usr/bin/env python3
"""Genera targets/asm/dct32_040.s: el dct32 de libmad en ensamblador 68040.

Lee las sentencias de third_party/libmad/synth.c (tools/libmad_dct32.py), las ejecuta sobre valores simbólicos
(dflow.py) y emite el asm con asignación de registros. Referencia en C: src/dct32_ref.c (generada del mismo texto).
Con --half genera mp368k_dct32_half_040 (targets/asm/dct32_half_040.s): las mismas cuentas, pero solo escribe las salidas que lee
synth_half de libmad (filas pares de lo y filas impares de hi); el resto se elimina como código muerto.
Uso: targets/asm/gen/gen_dct32.py [--half] [salida]
"""
import os
import re
import sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", "..", ".."))
sys.path.insert(0, here)
sys.path.insert(0, os.path.join(root, "tools"))
import dflow
import libmad_dct32

consts, stmts = libmad_dct32.extract(os.path.join(root, "third_party", "libmad", "synth.c"))
args = sys.argv[1:]
half = "--half" in args
args = [a for a in args if a != "--half"]
name = "mp368k_dct32_half_040" if half else "mp368k_dct32_040"
fname = "dct32_half_040.s" if half else "dct32_040.s"
out_path = args[0] if args else os.path.join(root, "targets", "asm", fname)

with dflow.Prog() as prog:
    IN = [dflow.inp(dflow.Mem("%a0", 4 * k)) for k in range(32)]
    T = {}
    env = {"IN": IN, "T": T, "MUL": lambda a, c: dflow.mul28(a, c),
           "C": consts}
    for s in stmts:
        s = re.sub(r"\s+", " ", s)
        m = re.match(r"^t(\d+) = (.+)$", s)
        if m:
            e = re.sub(r"\bin\[(\d+)\]", r"IN[\1]", m.group(2))
            e = re.sub(r"\bcostab(\d+)\b", r"C[\1]", e)
            e = re.sub(r"\bt(\d+)\b", r"T[\1]", e)
            T[int(m.group(1))] = eval(e, {}, env)
            continue
        m = re.match(r"^(hi|lo)\[ ?(\d+)\]\[slot\] = SHIFT\((.+)\)$", s)
        e = re.sub(r"\bcostab(\d+)\b", r"C[\1]", m.group(3))
        e = re.sub(r"\bt(\d+)\b", r"T[\1]", e)
        base = "%a1" if m.group(1) == "hi" else "%a2"
        if half and (int(m.group(2)) & 1) != (1 if m.group(1) == "hi" else 0):
            continue                    # synth_half no lee esta fila
        dflow.store(dflow.Mem(base, 32 * int(m.group(2))), eval(e, {}, env))

body, nslots = dflow.emit_kernel(prog, name, None, None)
frame = 4 * nslots
what = ("solo las filas que lee synth_half (lo pares, hi impares); las demás quedan sin escribir"
        if half else "las 32 salidas")
txt = f"""| {fname} — DCT de 32 puntos de libmad en ensamblador 68040: {what}.
| GENERADO por targets/asm/gen/gen_dct32.py{' --half' if half else ''} (dflow.py) desde third_party/libmad/synth.c: no editar a mano.
| Referencia en C: src/dct32_ref.c (generada del mismo texto). Prueba: tests/asm/test_dct32{'_half' if half else ''}.c bajo qemu-m68k.
| SIN MEDIR en el LC 580: no se declara más rápido que la referencia hasta medirlo (ADR-008).
|
| void {name}(const int32_t in[32], unsigned slot, int32_t lo[16][8], int32_t hi[16][8]);
| Aritmética de libmad con FPM_64BIT: sumas de 32 bits con envoltura; cada producto es
| (int32)(((int64)x*y) >> 28) = (hi << 4) | (lo >>> 28) de muls.l.
| d1 = parte alta del producto, d7 = 28, d0/d2-d6 = valores, huecos de pila para los que no caben.
| Preserva d2-d7/a2-a6 (convención de GCC); a0 = in, a1 = hi + slot*4, a2 = lo + slot*4.
| Hay {len(prog.ops)} operaciones y {nslots} huecos de pila ({frame} bytes).

        .text
        .even
        .globl  {name}
{name}:
        movem.l %d2-%d7/%a2,-(%sp)
        move.l  32(%sp),%a0                 | in
        move.l  36(%sp),%d0
        lsl.l   #2,%d0                      | slot en bytes
        move.l  44(%sp),%a1                 | hi
        adda.l  %d0,%a1
        move.l  40(%sp),%a2                 | lo
        adda.l  %d0,%a2
        moveq   #28,%d7
"""
if frame:
    txt += f"        lea     -{frame}(%sp),%sp\n"
txt += body + "\n"
if frame:
    txt += f"        lea     {frame}(%sp),%sp\n"
txt += """        movem.l (%sp)+,%d2-%d7/%a2
        rts
"""
open(out_path, "w").write(txt)
print(f"gen_dct32: {out_path}  ({len(prog.ops)} operaciones, {nslots} huecos de pila)")
