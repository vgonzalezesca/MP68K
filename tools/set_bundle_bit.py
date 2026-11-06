#!/usr/bin/env python3
"""Activa el bit "has bundle" (0x2000) de los flags del Finder de una aplicación ya empaquetada.

Uso: tools/set_bundle_bit.py <carpeta dist/<equipo>/<app>> <app>

El Finder solo usa el BNDL/FREF/ícono de una aplicación si ese bit está activo, y Retro68 deja los
flags en cero. Parchea los cuatro formatos que genera build.sh:
  <app>.bin        MacBinary: byte 73 (y el CRC de la cabecera, si estaba bien)
  <app>.dsk        imagen HFS: el registro del archivo en el catálogo (se busca 'APPL' + creador)
  .finf/<app>.APPL 32 bytes de FInfo: flags en el desplazamiento 8
  %<app>.ad        AppleDouble: entrada 9 (Finder Info), flags en el desplazamiento 8

No hace nada si el .bin no tiene un recurso BNDL. Cada parche se verifica leyendo de vuelta.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from macbin_info import parse_macbinary, parse_rsrc  # noqa: E402

HAS_BUNDLE = 0x2000


def crc_ccitt(data, crc=0):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def patch_macbinary(path):
    raw = bytearray(open(path, "rb").read())
    crc_ok = struct.unpack(">H", raw[124:126])[0] == crc_ccitt(raw[:124])
    flags = (raw[73] << 8) | raw[101]
    flags |= HAS_BUNDLE
    raw[73], raw[101] = flags >> 8, flags & 0xFF
    if crc_ok:
        raw[124:126] = struct.pack(">H", crc_ccitt(raw[:124]))
    open(path, "wb").write(raw)
    chk = open(path, "rb").read()
    assert (chk[73] << 8 | chk[101]) & HAS_BUNDLE
    return "CRC recalculado" if crc_ok else "sin CRC válido previo (se deja como estaba)"


def patch_dsk(path, creator):
    raw = bytearray(open(path, "rb").read())
    pat = b"APPL" + creator
    positions = []
    i = raw.find(pat)
    while i >= 0:
        positions.append(i)
        i = raw.find(pat, i + 1)
    if len(positions) != 1:
        raise SystemExit(f"{path}: se esperaba 1 registro 'APPL{creator.decode()}' y hay {len(positions)}")
    p = positions[0] + 8
    flags = struct.unpack(">H", raw[p:p + 2])[0] | HAS_BUNDLE
    raw[p:p + 2] = struct.pack(">H", flags)
    open(path, "wb").write(raw)
    chk = open(path, "rb").read()
    assert struct.unpack(">H", chk[p:p + 2])[0] & HAS_BUNDLE
    return f"registro en el desplazamiento {positions[0]}"


def patch_finf(path):
    raw = bytearray(open(path, "rb").read())
    flags = struct.unpack(">H", raw[8:10])[0] | HAS_BUNDLE
    raw[8:10] = struct.pack(">H", flags)
    open(path, "wb").write(raw)


def patch_appledouble(path):
    raw = bytearray(open(path, "rb").read())
    if struct.unpack(">I", raw[:4])[0] != 0x00051607:
        raise SystemExit(f"{path}: no es AppleDouble")
    n = struct.unpack(">H", raw[24:26])[0]
    for k in range(n):
        eid, off, length = struct.unpack(">III", raw[26 + 12 * k:38 + 12 * k])
        if eid == 9:
            flags = struct.unpack(">H", raw[off + 8:off + 10])[0] | HAS_BUNDLE
            raw[off + 8:off + 10] = struct.pack(">H", flags)
            open(path, "wb").write(raw)
            return
    raise SystemExit(f"{path}: sin entrada Finder Info")


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    folder, app = argv[1], argv[2]
    binpath = os.path.join(folder, app + ".bin")
    _, rsrc = parse_macbinary(binpath)
    if ("BNDL", 128) not in parse_rsrc(rsrc):
        print(f"  bundle: {app} no tiene BNDL, no se toca")
        return 0
    creator = open(binpath, "rb").read()[69:73]
    print(f"  bundle .bin: {patch_macbinary(binpath)}")
    dsk = os.path.join(folder, app + ".dsk")
    if os.path.exists(dsk):
        print(f"  bundle .dsk: {patch_dsk(dsk, creator)}")
    finf = os.path.join(folder, ".finf", app + ".APPL")
    if os.path.exists(finf):
        patch_finf(finf)
        print("  bundle .finf: ok")
    ad = os.path.join(folder, "%" + app + ".ad")
    if os.path.exists(ad):
        patch_appledouble(ad)
        print("  bundle %.ad: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
