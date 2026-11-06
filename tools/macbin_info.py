#!/usr/bin/env python3
"""Informa los recursos de un archivo MacBinary: tipos, tamaños y segmentos CODE.

Uso: tools/macbin_info.py <app.bin> [--warn-code-kb N]

Los segmentos CODE de 68k son el límite que importa para la regla 6 (32 KB, CLAUDE.md).
Sale con 0 siempre: es informativo. Con --warn-code-kb marca los CODE mayores que N KB.
"""
import struct
import sys


def parse_macbinary(path):
    raw = open(path, "rb").read()
    dlen, rlen = struct.unpack(">II", raw[83:91])
    pad = lambda n: (n + 127) // 128 * 128
    return raw[128:128 + dlen], raw[128 + pad(dlen):128 + pad(dlen) + rlen]


def parse_rsrc(rsrc):
    out = {}
    if not rsrc:
        return out
    data_off, map_off = struct.unpack(">II", rsrc[:8])
    m = rsrc[map_off:]
    type_off = struct.unpack(">H", m[24:26])[0]
    ntypes = struct.unpack(">h", m[type_off:type_off + 2])[0] + 1
    for i in range(ntypes):
        e = m[type_off + 2 + 8 * i:type_off + 2 + 8 * (i + 1)]
        t = e[:4].decode("mac_roman")
        count = struct.unpack(">H", e[4:6])[0] + 1
        ref = struct.unpack(">H", e[6:8])[0]
        for j in range(count):
            r = m[type_off + ref + 12 * j:type_off + ref + 12 * (j + 1)]
            rid = struct.unpack(">h", r[:2])[0]
            off = int.from_bytes(r[5:8], "big")
            size = struct.unpack(">I", rsrc[data_off + off:data_off + off + 4])[0]
            out[(t, rid)] = rsrc[data_off + off + 4:data_off + off + 4 + size]
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    warn_kb = None
    if "--warn-code-kb" in argv:
        warn_kb = int(argv[argv.index("--warn-code-kb") + 1])
    data, rsrc = parse_macbinary(argv[1])
    res = parse_rsrc(rsrc)
    code = sorted((rid, len(b)) for (t, rid), b in res.items() if t == "CODE" and rid > 0)
    total = sum(n for _, n in code)
    line = ", ".join(f"{rid}:{n / 1024:.1f}K" for rid, n in code)
    print(f"  CODE {len(code)} segmentos, {total / 1024:.1f} KB: {line}")
    if warn_kb is not None:
        big = [(rid, n) for rid, n in code if n > warn_kb * 1024]
        if big:
            print(f"  AVISO: segmentos sobre {warn_kb} KB (regla 6): " +
                  ", ".join(f"{rid}={n / 1024:.1f}K" for rid, n in big))
    if data:
        print(f"  datos: {len(data) / 1024:.1f} KB (PEF)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
