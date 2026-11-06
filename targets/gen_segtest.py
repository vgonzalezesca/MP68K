#!/usr/bin/env python3
"""Genera segtest_gen.c: cientos de funciones con aritmética de 32 bits y su resultado esperado.

Uso: gen_segtest.py <salida.c> [cantidad de funciones]

Sirve para comprobar que una aplicación 68k con un segmento de código mucho mayor que 32 KB corre
bien (regla 6 de CLAUDE.md). Las funciones se llaman a través de una tabla y en un orden
barajado, así que las llamadas saltan por todo el segmento. El resultado esperado se calcula
aquí, en el PC, emulando enteros de 32 bits sin signo.
"""
import random
import sys

M = 0xFFFFFFFF
ROUNDS = 3


def make_function(rng):
    """Devuelve (código C del cuerpo, función Python equivalente)."""
    ops = []
    for _ in range(36):
        kind = rng.choice(("muladd", "xorshr", "addshl", "rotl", "sub"))
        if kind == "muladd":
            k, a = rng.randrange(1, M) | 1, rng.randrange(M)
            ops.append((f"x = x * 0x{k:08X}UL + 0x{a:08X}UL;", lambda x, k=k, a=a: (x * k + a) & M))
        elif kind == "xorshr":
            s, c = rng.randrange(1, 31), rng.randrange(M)
            ops.append((f"x ^= (x >> {s}) ^ 0x{c:08X}UL;", lambda x, s=s, c=c: (x ^ (x >> s) ^ c) & M))
        elif kind == "addshl":
            s, c = rng.randrange(1, 31), rng.randrange(M)
            ops.append((f"x += (x << {s}) ^ 0x{c:08X}UL;", lambda x, s=s, c=c: (x + ((x << s) & M ^ c)) & M))
        elif kind == "rotl":
            s = rng.randrange(1, 31)
            ops.append((f"x = (x << {s}) | (x >> {32 - s});", lambda x, s=s: ((x << s) | (x >> (32 - s))) & M))
        else:
            c = rng.randrange(M)
            ops.append((f"x -= 0x{c:08X}UL;", lambda x, c=c: (x - c) & M))
    return [o[0] for o in ops], [o[1] for o in ops]


def main(out, count):
    rng = random.Random(368)
    funcs = [make_function(rng) for _ in range(count)]
    order = list(range(count))
    rng.shuffle(order)

    x = 0x12345678
    for _ in range(ROUNDS):
        for idx in order:
            for fn in funcs[idx][1]:
                x = fn(x)

    with open(out, "w") as f:
        f.write("/* Generado por targets/gen_segtest.py: no editar. */\n")
        f.write('#include "segtest.h"\n\n')
        for i, (body, _) in enumerate(funcs):
            f.write(f"unsigned long seg_f{i}(unsigned long x)\n{{\n")
            for line in body:
                f.write(f"    {line}\n")
            f.write("    return x;\n}\n\n")
        f.write("const SegFn seg_table[SEG_COUNT] = {\n")
        for i in range(count):
            f.write(f"    seg_f{i},\n")
        f.write("};\n\n")
        f.write("const unsigned short seg_order[SEG_COUNT] = {\n")
        for i in range(0, count, 16):
            f.write("    " + ", ".join(str(v) for v in order[i:i + 16]) + ",\n")
        f.write("};\n\n")
        f.write(f"const unsigned long seg_expected = 0x{x:08X}UL;\n")
        f.write(f"const unsigned seg_rounds = {ROUNDS};\n")

    with open(out.replace("_gen.c", ".h"), "w") as f:
        f.write("/* Generado por targets/gen_segtest.py: no editar. */\n")
        f.write("#ifndef MP368K_SEGTEST_H\n#define MP368K_SEGTEST_H\n\n")
        f.write(f"#define SEG_COUNT {count}\n\n")
        f.write("typedef unsigned long (*SegFn)(unsigned long);\n")
        f.write("extern const SegFn seg_table[SEG_COUNT];\n")
        f.write("extern const unsigned short seg_order[SEG_COUNT];\n")
        f.write("extern const unsigned long seg_expected;\n")
        f.write("extern const unsigned seg_rounds;\n\n#endif\n")


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 700)
