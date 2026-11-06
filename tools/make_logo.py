#!/usr/bin/env python3
"""Genera los íconos y el logo de MP368K con la paleta de 256 colores del sistema de Mac OS.

Uso: tools/make_logo.py [--out src/player/icons.r] [--preview hoja.png] [--source ref.png]

Dibuja el motivo (anillos concéntricos verde azulado sobre una forma redondeada, con una ola y un
trazo oscuro) a la resolución que pida cada ícono, con supersampling, y lo cuantiza a la paleta
estándar de 8 bits de Mac OS (clut 8). A 16 y 32 px no se reduce la imagen original: se dibuja otra
vez con menos anillos y trazos más gruesos, porque los anillos finos no sobreviven a una reducción.

Escribe un .r de Rez con: icl8 (32x32), ics8 (16x16), ICN# y ics# (1 bit con máscara), y
LOGO (96x96, bytes de 8 bits que la app dibuja con la tabla de colores del sistema).

SUPUESTO sin verificar en un Mac: el orden de índices de la paleta (ver mac_palette()). Si los
colores se ven mal en el Finder, ese orden es el primer sospechoso.
"""
import argparse
import math
import sys

import numpy as np
from PIL import Image

# ---------------------------------------------------------------- paleta del sistema (clut 8)


def mac_palette():
    """256 colores de la paleta estándar de 8 bits de Mac OS.

    0-214: cubo de 6 niveles (FF CC 99 66 33 00) por canal, R el más lento, sin el negro;
    215-224: 10 rojos; 225-234: 10 verdes; 235-244: 10 azules; 245-254: 10 grises
    (EE DD BB AA 88 77 55 44 22 11); 255: negro. Blanco en el 0.
    """
    levels = [0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00]
    pal = [(r, g, b) for r in levels for g in levels for b in levels][:215]
    ramp = [0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11]
    pal += [(v, 0, 0) for v in ramp] + [(0, v, 0) for v in ramp] + [(0, 0, v) for v in ramp]
    pal += [(v, v, v) for v in ramp] + [(0, 0, 0)]
    assert len(pal) == 256 and len(set(pal)) == 256, "paleta inválida"
    assert pal[0] == (255, 255, 255) and pal[5] == (255, 255, 0) and pal[255] == (0, 0, 0)
    return np.array(pal, dtype=np.int32)


# ---------------------------------------------------------------- dibujo


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))


def lerp(a, b, t):
    return a + (b - a) * t[..., None]


def over(dst, color, alpha):
    """Compone `color` (3,) o (H,W,3) sobre dst con alfa (H,W)."""
    return dst * (1 - alpha[..., None]) + np.asarray(color, dtype=np.float64) * alpha[..., None]


DARK = np.array([0x1E, 0x52, 0x40], dtype=np.float64)
MID = np.array([0x4F, 0x94, 0x7C], dtype=np.float64)
LIGHT = np.array([0x9A, 0xD0, 0xDE], dtype=np.float64)
HILL1 = np.array([0x99, 0xCC, 0x99], dtype=np.float64)
HILL2 = np.array([0x66, 0xCC, 0x99], dtype=np.float64)
LEAF = np.array([0x00, 0x33, 0x33], dtype=np.float64)
RING = np.array([0xE3, 0xF2, 0xEE], dtype=np.float64)


def render(size, ss=8, rings=5, thick=0.030, strong=0.30):
    """Devuelve (rgb float (size,size,3), alfa float (size,size)) a size px, supersampleado."""
    n = size * ss
    ys, xs = np.mgrid[0:n, 0:n]
    x = (xs + 0.5) / n
    y = (ys + 0.5) / n

    # Forma: superelipse girada, algo más abultada arriba a la derecha.
    ang = math.radians(-12)
    u = (x - 0.5) * math.cos(ang) - (y - 0.5) * math.sin(ang)
    v = (x - 0.5) * math.sin(ang) + (y - 0.5) * math.cos(ang)
    e = 2.35
    d = (np.abs(u / 0.455) ** e + np.abs(v / 0.440) ** e) ** (1 / e)
    edge = 1.5 / n
    shape = 1.0 - smoothstep(1.0 - edge * 2.2, 1.0, d)

    # Degradado: verde oscuro abajo a la izquierda, azul claro arriba a la derecha.
    t = np.clip(0.42 + (x - 0.5) * 0.60 + (0.5 - y) * 0.85, 0, 1)
    base = np.where(t[..., None] < 0.5, lerp(DARK, MID, t * 2), lerp(MID, LIGHT, (t - 0.5) * 2))
    img = base.astype(np.float64)

    # Anillos concéntricos (zona alta-izquierda), interrumpidos por la ola.
    cx, cy = 0.47, 0.43
    r = np.hypot(x - cx, y - cy)
    step = 0.405 / (rings + 0.6) if rings > 1 else 0.2
    ring_mask = np.zeros_like(r)
    for k in range(rings):
        rk = 0.075 + step * k
        band = 1.0 - smoothstep(thick * 0.5 - edge, thick * 0.5 + edge, np.abs(r - rk))
        ring_mask = np.maximum(ring_mask, band)
    # Los anillos se apagan hacia la derecha y abajo, donde pasa la ola.
    wave_y = 0.80 - 0.40 * sigmoid((x - 0.52) * 7.5)
    fade = 1.0 - smoothstep(-0.03, 0.05, y - wave_y)
    # El interior del anillo central queda como un punto, como en la referencia.
    img = over(img, RING, ring_mask * fade * strong)
    dot = 1.0 - smoothstep(0.022 - edge, 0.022 + edge, r)
    img = over(img, RING, dot * strong * 1.1)

    # Ola: dos colinas de verde claro.
    hill1 = smoothstep(-edge, edge, y - wave_y)
    img = over(img, HILL1, hill1)
    wave2 = 0.88 - 0.24 * sigmoid((x - 0.30) * 6.0)
    hill2 = smoothstep(-edge, edge, y - wave2)
    img = over(img, HILL2, hill2)

    # Trazo oscuro en forma de hoja que corta la ola (más fino hacia la punta).
    s = np.clip((x - 0.30) / 0.50, 0, 1)
    center = 0.90 - 0.40 * sigmoid((x - 0.50) * 6.5) - 0.02
    half = 0.030 * np.sin(np.pi * np.clip(s, 0, 1)) ** 0.8
    leaf = (1.0 - smoothstep(half - edge, half + edge, np.abs(y - center))) * ((x > 0.30) & (x < 0.80))
    img = over(img, LEAF, leaf)

    # Reduce el supersampling.
    rgb = img.reshape(size, ss, size, ss, 3).mean(axis=(1, 3))
    alpha = shape.reshape(size, ss, size, ss).mean(axis=(1, 3))
    return rgb, alpha


# ---------------------------------------------------------------- cuantización


def quantize(rgb, pal, dither):
    """Mapea cada pixel al color más cercano de la paleta; dither=True usa Floyd-Steinberg."""
    h, w, _ = rgb.shape
    out = np.zeros((h, w), dtype=np.uint8)
    work = rgb.astype(np.float64).copy()
    weights = np.array([0.30, 0.59, 0.11])
    for y in range(h):
        for x in range(w):
            px = np.clip(work[y, x], 0, 255)
            dist = (((pal - px) ** 2) * weights).sum(axis=1)
            idx = int(np.argmin(dist))
            out[y, x] = idx
            if dither:
                err = px - pal[idx]
                if x + 1 < w:
                    work[y, x + 1] += err * 7 / 16
                if y + 1 < h:
                    if x > 0:
                        work[y + 1, x - 1] += err * 3 / 16
                    work[y + 1, x] += err * 5 / 16
                    if x + 1 < w:
                        work[y + 1, x + 1] += err * 1 / 16
    return out


def quantize_ordered(rgb, pal, spread=34.0):
    """Cuantiza con difuminado ordenado (Bayer 4x4): patrón regular, más limpio que Floyd-Steinberg."""
    h, w, _ = rgb.shape
    bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 - 0.5
    tile = np.tile(bayer, (h // 4 + 1, w // 4 + 1))[:h, :w]
    work = np.clip(rgb + tile[..., None] * spread, 0, 255)
    out = np.zeros((h, w), dtype=np.uint8)
    weights = np.array([0.30, 0.59, 0.11])
    for y in range(h):
        for x in range(w):
            dist = (((pal - work[y, x]) ** 2) * weights).sum(axis=1)
            out[y, x] = int(np.argmin(dist))
    return out


def bayer_bw(rgb, alpha):
    """1 bit con puntos ordenados: 1 = negro. Contorno negro en el borde de la forma."""
    h, w, _ = rgb.shape
    lum = (rgb * np.array([0.30, 0.59, 0.11])).sum(axis=2) / 255.0
    bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0
    tile = np.tile(bayer, (h // 4 + 1, w // 4 + 1))[:h, :w]
    black = (lum < tile * 0.9 + 0.05)
    # Borde: pixel de la forma con algún vecino fuera de ella.
    inside = alpha >= 0.5
    pad = np.pad(inside, 1, constant_values=False)
    neigh = pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:]
    border = inside & ~neigh
    return (black | border) & inside, inside


# ---------------------------------------------------------------- salida a Rez


def hexlines(data, name, rid, per=32):
    b = bytes(data)
    lines = [f"data '{name}' ({rid}) {{"]
    for i in range(0, len(b), per):
        lines.append('\t$"' + b[i:i + per].hex().upper() + '"')
    lines.append("};\n")
    return "\n".join(lines)


def pack_bits(mat):
    h, w = mat.shape
    out = bytearray()
    for y in range(h):
        for x in range(0, w, 8):
            v = 0
            for k in range(8):
                v = (v << 1) | int(mat[y, x + k])
            out.append(v)
    return out


def build(pal, dither_large=True, dither_small=False):
    pieces = {}
    for size, name, ss, rings, thick, strong in ((32, "icl8", 16, 3, 0.062, 0.55),
                                                 (16, "ics8", 16, 2, 0.090, 0.60),
                                                 (96, "LOGO", 8, 5, 0.030, 0.30)):
        rgb, alpha = render(size, ss=ss, rings=rings, thick=thick, strong=strong)
        idx = quantize_ordered(rgb, pal) if (size == 96 and dither_large) else quantize(rgb, pal, dither_small)
        inside = alpha >= 0.5
        idx = np.where(inside, idx, 0).astype(np.uint8)     # fuera de la forma: blanco (0)
        pieces[name] = (rgb, alpha, idx)
    return pieces


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="src/player/icons.r")
    ap.add_argument("--preview", default=None)
    ap.add_argument("--source", default=None, help="imagen de referencia para la hoja de comparación")
    ap.add_argument("--dither", choices=("none", "fs"), default="none", help="difuminado de 16/32 px")
    args = ap.parse_args()

    pal = mac_palette()
    pieces = build(pal, dither_large=True, dither_small=args.dither == "fs")

    rgb32, a32, idx32 = pieces["icl8"]
    rgb16, a16, idx16 = pieces["ics8"]
    _, _, idx96 = pieces["LOGO"]

    bw32, mask32 = bayer_bw(rgb32, a32)
    bw16, mask16 = bayer_bw(rgb16, a16)

    out = []
    out.append("/* Generado por tools/make_logo.py: no editar a mano. */\n")
    out.append(hexlines(idx32.tobytes(), "icl8", 128, 32))
    out.append(hexlines(idx16.tobytes(), "ics8", 128, 16))
    out.append(hexlines(bytes(pack_bits(bw32)) + bytes(pack_bits(mask32)), "ICN#", 128, 16))
    out.append(hexlines(bytes(pack_bits(bw16)) + bytes(pack_bits(mask16)), "ics#", 128, 16))
    out.append(hexlines(idx96.tobytes(), "LOGO", 128, 48))
    with open(args.out, "w") as f:
        f.write("\n".join(out))
    print(f"escrito {args.out}")

    if args.preview:
        scale = 6
        sheet = Image.new("RGB", (96 * 2 * scale + 100, 96 * scale + 20), (221, 221, 221))
        def paint(idx, ox, oy, k, bg=(255, 255, 255)):
            img = Image.new("RGB", (idx.shape[1], idx.shape[0]), bg)
            px = img.load()
            for y in range(idx.shape[0]):
                for x in range(idx.shape[1]):
                    px[x, y] = tuple(int(c) for c in pal[idx[y, x]])
            sheet.paste(img.resize((img.width * k, img.height * k), Image.NEAREST), (ox, oy))
        paint(idx96, 10, 10, scale)
        paint(idx32, 96 * scale + 30, 10, 8)
        paint(idx16, 96 * scale + 30, 10 + 32 * 8 + 10, 8)
        if args.source:
            ref = Image.open(args.source).convert("RGB")
            sheet.paste(ref.resize((96, 96), Image.LANCZOS).resize((96 * 2, 96 * 2), Image.NEAREST),
                        (96 * scale + 30 + 32 * 8 + 10, 10))
        paint(idx32, 96 * scale + 30 + 32 * 8 + 10, 10 + 96 * 2 + 20, 1, (119, 119, 119))
        paint(idx32, 96 * scale + 30 + 32 * 8 + 50, 10 + 96 * 2 + 20, 2, (119, 119, 119))
        paint(idx16, 96 * scale + 30 + 32 * 8 + 10, 10 + 96 * 2 + 20 + 80, 1, (119, 119, 119))
        paint(idx16, 96 * scale + 30 + 32 * 8 + 50, 10 + 96 * 2 + 20 + 80, 2, (119, 119, 119))
        sheet.save(args.preview)
        print(f"vista previa {args.preview}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
