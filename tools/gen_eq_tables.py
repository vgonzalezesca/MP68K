#!/usr/bin/env python3
"""Genera src/eq_tables.h: ganancias lineales en Q24 para cada cuarto de dB de -24 a +24 dB.

Uso: tools/gen_eq_tables.py [salida]    (por omisión src/eq_tables.h)

Se versiona el resultado para que el build (y el 68k) no necesite Python ni punto flotante.
ganancia(q) = round(10^(q / 80) * 2^24), con q en cuartos de dB. Q24 y no Q28: +24 dB es 15,85 veces
y no cabe en un entero de 32 bits con 28 bits fraccionarios.
"""
import os
import sys

DB_MIN, DB_MAX = -96, 96        # cuartos de dB: -24 .. +24 dB


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "src", "eq_tables.h")
    vals = [round((10 ** (q / 80.0)) * (1 << 24)) for q in range(DB_MIN, DB_MAX + 1)]
    assert vals[-DB_MIN] == 1 << 24, "0 dB debe ser exactamente 1.0"
    assert max(vals) < (1 << 31)
    lines = [
        "/* Generado por tools/gen_eq_tables.py: no editar a mano. */",
        "#ifndef MP368K_EQ_TABLES_H",
        "#define MP368K_EQ_TABLES_H",
        "",
        "#include <stdint.h>",
        "",
        "/* Ganancia lineal en Q24 para cada cuarto de dB; el índice es (cuartos de dB) - EQ_DB_MIN. */",
        f"#define EQ_DB_MIN ({DB_MIN})",
        f"#define EQ_DB_MAX ({DB_MAX})",
        "#define EQ_UNITY  (1L << 24)",
        f"static const int32_t kEqGainQ24[{len(vals)}] = {{",
    ]
    for i in range(0, len(vals), 6):
        lines.append("    " + ", ".join(f"{v}L" for v in vals[i:i + 6]) + ",")
    lines += ["};", "", "#endif", ""]
    open(out, "w").write("\n".join(lines))
    print(f"escrito {out} ({len(vals)} entradas)")


if __name__ == "__main__":
    main()
