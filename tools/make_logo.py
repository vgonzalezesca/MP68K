#!/usr/bin/env python3
"""Genera los íconos y el logo de MP368K con la paleta de 256 colores del sistema de Mac OS.

Uso: tools/make_logo.py [--out src/player/icons.r] [--preview hoja.png]

Los íconos de 32×32 y 16×16 se dibujan A MANO, píxel por píxel, en assets/logo/icon32.txt e
icon16.txt: una letra por color, todos EXACTOS de la paleta del sistema (múltiplos de 0x33 por
canal), sin difuminado. Este script solo los convierte. El logo grande de la ventana About
(96×96) se dibuja por procedimiento, con un degradado difuminado en orden (Bayer) y el mismo
contorno y reflejo de estilo Platinum.

Escribe un .r de Rez con: icl8 e ics8 (256 colores), icl4 e ics4 (16 colores), ICN# e ics#
(1 bit con máscara) y LOGO (96×96, bytes de 8 bits que la app dibuja con la tabla de colores del
sistema).

SUPUESTOS sin verificar en un Mac: el orden de índices de la paleta de 256 colores (mac_palette)
y el de la de 16 colores (mac16_palette). Si los colores se ven mal en el Finder, esos órdenes
son el primer sospechoso.
"""
import argparse
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets", "logo")

# ---------------------------------------------------------------- paletas del sistema


def mac_palette():
    """256 colores de la paleta estándar de 8 bits de Mac OS (clut 8).

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


def mac16_palette():
    """16 colores estándar de Mac OS (clut 4), en el orden de los píxeles de icl4/ics4."""
    hexes = ["FFFFFF", "FCF305", "FF6402", "DD0806", "F20884", "4600A5", "0000D4", "02ABEA",
             "1FB714", "006412", "562C05", "90713A", "C0C0C0", "808080", "404040", "000000"]
    return np.array([[int(h[i:i + 2], 16) for i in (0, 2, 4)] for h in hexes], dtype=np.int32)


# Qué color de los 16 usa cada letra en icl4/ics4 (no hay turquesas: se elige lo más cercano
# conservando el contraste y el aspecto verde azulado).
FOUR_BIT = {"K": 15, "D": 9, "M": 7, "L": 7, "H": 0, "G": 12, "g": 8, "S": 13}

# ---------------------------------------------------------------- rejillas dibujadas a mano


def load_grid(path, size):
    """Devuelve (leyenda {letra: (r,g,b)}, filas) tras validar el archivo."""
    legend, rows = {}, []
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        if len(line) >= 8 and line[1] == "=" and all(c in "0123456789ABCDEFabcdef" for c in line[2:8]) \
                and (len(line) == 8 or line[8] == " "):
            legend[line[0]] = tuple(int(line[2:8][i:i + 2], 16) for i in (0, 2, 4))
        else:
            rows.append(line)
    if len(rows) != size or any(len(r) != size for r in rows):
        sys.exit(f"{path}: se esperaban {size} filas de {size} columnas")
    bad = {c for r in rows for c in r} - set(legend) - {"."}
    if bad:
        sys.exit(f"{path}: letras sin color en la leyenda: {sorted(bad)}")
    return legend, rows


def grid_to_indices(legend, rows, pal):
    """Índices de la paleta de 256 colores (0 = blanco donde no hay nada) y la máscara."""
    lookup = {tuple(int(v) for v in c): i for i, c in enumerate(pal)}
    n = len(rows)
    idx = np.zeros((n, n), dtype=np.uint8)
    mask = np.zeros((n, n), dtype=bool)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch == ".":
                continue
            rgb = legend[ch]
            if rgb not in lookup:
                sys.exit(f"el color #{rgb[0]:02X}{rgb[1]:02X}{rgb[2]:02X} de '{ch}' no es de la paleta del sistema")
            idx[y, x] = lookup[rgb]
            mask[y, x] = True
    return idx, mask


def grid_to_4bit(rows):
    n = len(rows)
    out = np.zeros((n, n), dtype=np.uint8)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            if ch != ".":
                out[y, x] = FOUR_BIT[ch]
    return out


def grid_to_1bit(rows):
    """1 bit (1 = negro): contorno y sombra propia negros; tono medio y sombra proyectada punteados."""
    n = len(rows)
    bw = np.zeros((n, n), dtype=bool)
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            checker = (x + y) % 2 == 0
            bw[y, x] = ch in "KD" or (ch in "MS" and checker)
    return bw


# ---------------------------------------------------------------- logo grande (procedural)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))


def lerp(a, b, t):
    return a + (b - a) * t[..., None]


def over(dst, color, alpha):
    return dst * (1 - alpha[..., None]) + np.asarray(color, dtype=np.float64) * alpha[..., None]


DARK = np.array([0x1E, 0x52, 0x40], dtype=np.float64)
MID = np.array([0x4F, 0x94, 0x7C], dtype=np.float64)
LIGHT = np.array([0x9A, 0xD0, 0xDE], dtype=np.float64)
HILL1 = np.array([0x99, 0xCC, 0x99], dtype=np.float64)
HILL2 = np.array([0x66, 0xCC, 0x99], dtype=np.float64)
LEAF = np.array([0x00, 0x33, 0x33], dtype=np.float64)
RING = np.array([0xE3, 0xF2, 0xEE], dtype=np.float64)
SPEC = np.array([0xCC, 0xFF, 0xFF], dtype=np.float64)


def render_logo(size, ss=8, rings=5, thick=0.030, strong=0.30):
    """Logo grande (rgb float, alfa float) con contorno oscuro y reflejo del borde, estilo Platinum."""
    n = size * ss
    ys, xs = np.mgrid[0:n, 0:n]
    x = (xs + 0.5) / n
    y = (ys + 0.5) / n

    ang = math.radians(-12)
    u = (x - 0.5) * math.cos(ang) - (y - 0.5) * math.sin(ang)
    v = (x - 0.5) * math.sin(ang) + (y - 0.5) * math.cos(ang)
    e = 2.35
    d = (np.abs(u / 0.455) ** e + np.abs(v / 0.440) ** e) ** (1 / e)
    edge = 1.5 / n
    shape = 1.0 - smoothstep(1.0 - edge * 2.2, 1.0, d)

    t = np.clip(0.42 + (x - 0.5) * 0.60 + (0.5 - y) * 0.85, 0, 1)
    base = np.where(t[..., None] < 0.5, lerp(DARK, MID, t * 2), lerp(MID, LIGHT, (t - 0.5) * 2))
    img = base.astype(np.float64)

    cx, cy = 0.47, 0.43
    r = np.hypot(x - cx, y - cy)
    step = 0.405 / (rings + 0.6) if rings > 1 else 0.2
    ring_mask = np.zeros_like(r)
    for k in range(rings):
        rk = 0.075 + step * k
        band = 1.0 - smoothstep(thick * 0.5 - edge, thick * 0.5 + edge, np.abs(r - rk))
        ring_mask = np.maximum(ring_mask, band)
    wave_y = 0.80 - 0.40 * sigmoid((x - 0.52) * 7.5)
    fade = 1.0 - smoothstep(-0.03, 0.05, y - wave_y)
    img = over(img, RING, ring_mask * fade * strong)
    dot = 1.0 - smoothstep(0.022 - edge, 0.022 + edge, r)
    img = over(img, RING, dot * strong * 1.1)

    hill1 = smoothstep(-edge, edge, y - wave_y)
    img = over(img, HILL1, hill1)
    wave2 = 0.88 - 0.24 * sigmoid((x - 0.30) * 6.0)
    hill2 = smoothstep(-edge, edge, y - wave2)
    img = over(img, HILL2, hill2)

    s = np.clip((x - 0.30) / 0.50, 0, 1)
    center = 0.90 - 0.40 * sigmoid((x - 0.50) * 6.5) - 0.02
    half = 0.030 * np.sin(np.pi * np.clip(s, 0, 1)) ** 0.8
    leaf = (1.0 - smoothstep(half - edge, half + edge, np.abs(y - center))) * ((x > 0.30) & (x < 0.80))
    img = over(img, LEAF, leaf)

    # Contorno oscuro de ~2 px y, justo dentro, un reflejo claro en el arco de arriba a la izquierda.
    px = 1.0 / (size * 0.45)                       # tamaño de un píxel final en unidades de d
    outline = smoothstep(1.0 - 2.0 * px - edge, 1.0 - 2.0 * px + edge, d)
    img = over(img, LEAF, outline)
    rim = smoothstep(1.0 - 4.0 * px - edge, 1.0 - 3.0 * px, d) * (1 - smoothstep(1.0 - 2.0 * px, 1.0 - 2.0 * px + edge, d))
    facing = smoothstep(0.15, -0.35, (u + v) / 0.9)                 # solo arriba a la izquierda
    img = over(img, SPEC, rim * facing * 0.85)

    rgb = img.reshape(size, ss, size, ss, 3).mean(axis=(1, 3))
    alpha = shape.reshape(size, ss, size, ss).mean(axis=(1, 3))
    return rgb, alpha


def quantize_ordered(rgb, pal, spread=34.0):
    """Cuantiza con difuminado ordenado (Bayer 4x4): patrón regular, más limpio que Floyd-Steinberg."""
    h, w, _ = rgb.shape
    bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) / 16.0 - 0.5
    tile = np.tile(bayer, (h // 4 + 1, w // 4 + 1))[:h, :w]
    work = np.clip(rgb + tile[..., None] * spread, 0, 255)
    out = np.zeros((h, w), dtype=np.uint8)
    weights = np.array([0.30, 0.59, 0.11])
    for yy in range(h):
        for xx in range(w):
            dist = (((pal - work[yy, xx]) ** 2) * weights).sum(axis=1)
            out[yy, xx] = int(np.argmin(dist))
    return out


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
    for yy in range(h):
        for xx in range(0, w, 8):
            v = 0
            for k in range(8):
                v = (v << 1) | int(mat[yy, xx + k])
            out.append(v)
    return out


def pack_nibbles(mat):
    h, w = mat.shape
    out = bytearray()
    for yy in range(h):
        for xx in range(0, w, 2):
            out.append((int(mat[yy, xx]) << 4) | int(mat[yy, xx + 1]))
    return out


# ---------------------------------------------------------------- hoja de contacto


def rgb_image(idx, mask, pal, bg=(255, 255, 255)):
    h, w = idx.shape
    img = Image.new("RGB", (w, h), bg)
    px = img.load()
    for yy in range(h):
        for xx in range(w):
            if mask[yy, xx]:
                px[xx, yy] = tuple(int(c) for c in pal[idx[yy, xx]])
    return img


def mask_image(mask):
    return Image.fromarray((mask * 255).astype(np.uint8))


def bw_image(bw, mask, bg=(221, 221, 221)):
    h, w = bw.shape
    img = Image.new("RGB", (w, h), bg)
    px = img.load()
    for yy in range(h):
        for xx in range(w):
            if mask[yy, xx]:
                px[xx, yy] = (0, 0, 0) if bw[yy, xx] else (255, 255, 255)
    return img


def contact_sheet(path, pal, pal16, i32, m32, i16, m16, n32, n16, bw32, bw16, logo_idx, logo_mask):
    sheet = Image.new("RGB", (1180, 720), (221, 221, 221))
    d = ImageDraw.Draw(sheet)

    def paste_scaled(img, x, y, k):
        sheet.paste(img.resize((img.width * k, img.height * k), Image.NEAREST), (x, y))

    paste_scaled(rgb_image(i32, m32, pal, (221, 221, 221)), 10, 10, 10)
    paste_scaled(rgb_image(i16, m16, pal, (221, 221, 221)), 350, 10, 10)
    paste_scaled(rgb_image(logo_idx, logo_mask, pal, (221, 221, 221)), 540, 10, 3)
    d.text((10, 336), "32x32 a 10x      16x16 a 10x      logo 96x96 a 3x", fill=(0, 0, 0))

    # El Finder dibuja con la máscara; sobre fondos distintos y en estado seleccionado.
    c32 = rgb_image(i32, m32, pal, (0, 0, 0))
    c16 = rgb_image(i16, m16, pal, (0, 0, 0))
    k32, k16 = mask_image(m32), mask_image(m16)
    y0 = 360
    for k, (label, bg) in enumerate([("gris Platinum", "gray"), ("blanco", "white"), ("patron de escritorio", "pat"),
                                     ("seleccionado", "sel")]):
        x0 = 10 + k * 285
        tile = Image.new("RGB", (270, 130), (255, 255, 255) if bg != "gray" else (221, 221, 221))
        td = ImageDraw.Draw(tile)
        if bg == "pat":
            for yy in range(130):
                for xx in range(270):
                    if (xx + yy) % 2 == 0:
                        td.point((xx, yy), fill=(153, 153, 153))
        icon32, icon16 = c32, c16
        if bg == "sel":
            td.rectangle([14, 8, 14 + 33, 8 + 33], fill=(60, 90, 166))
            tint = np.array([0x3C, 0x5A, 0xA6], dtype=np.float64)
            icon32 = Image.fromarray((np.array(c32).astype(np.float64) * 0.55 + tint * 0.45).astype(np.uint8))
            icon16 = Image.fromarray((np.array(c16).astype(np.float64) * 0.55 + tint * 0.45).astype(np.uint8))
        tile.paste(icon32, (15, 9), k32)
        tile.paste(c32.resize((64, 64), Image.NEAREST), (80, 6), k32.resize((64, 64), Image.NEAREST))
        tile.paste(icon16, (15, 60), k16)
        td.rectangle([34, 60, 90, 75], fill=(60, 90, 166) if bg == "sel" else tile.getpixel((100, 100)))
        td.text((36, 63), "MP368K", fill=(255, 255, 255) if bg == "sel" else (0, 0, 0))
        td.text((15, 112), label, fill=(0, 0, 0))
        sheet.paste(tile, (x0, y0))

    y1 = 510
    d.text((10, y1), "icl4 (16 colores)         ics4                ICN# (1 bit)          ics#          (x6)", fill=(0, 0, 0))
    paste_scaled(rgb_image(n32, m32, pal16, (221, 221, 221)), 10, y1 + 16, 6)
    paste_scaled(rgb_image(n16, m16, pal16, (221, 221, 221)), 220, y1 + 16, 6)
    paste_scaled(bw_image(bw32, m32), 340, y1 + 16, 6)
    paste_scaled(bw_image(bw16, m16), 560, y1 + 16, 6)
    sheet.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "src", "player", "icons.r"))
    ap.add_argument("--preview", default=None)
    args = ap.parse_args()

    pal = mac_palette()
    pal16 = mac16_palette()

    leg32, rows32 = load_grid(os.path.join(ASSETS, "icon32.txt"), 32)
    leg16, rows16 = load_grid(os.path.join(ASSETS, "icon16.txt"), 16)
    i32, m32 = grid_to_indices(leg32, rows32, pal)
    i16, m16 = grid_to_indices(leg16, rows16, pal)
    n32, n16 = grid_to_4bit(rows32), grid_to_4bit(rows16)
    bw32, bw16 = grid_to_1bit(rows32), grid_to_1bit(rows16)

    rgb, alpha = render_logo(96)
    logo_mask = alpha >= 0.5
    logo_idx = np.where(logo_mask, quantize_ordered(rgb, pal), 0).astype(np.uint8)

    out = ["/* Generado por tools/make_logo.py desde assets/logo/icon*.txt: no editar a mano. */\n"]
    out.append(hexlines(i32.tobytes(), "icl8", 128, 32))
    out.append(hexlines(i16.tobytes(), "ics8", 128, 16))
    out.append(hexlines(pack_nibbles(n32), "icl4", 128, 16))
    out.append(hexlines(pack_nibbles(n16), "ics4", 128, 8))
    out.append(hexlines(bytes(pack_bits(bw32 & m32)) + bytes(pack_bits(m32)), "ICN#", 128, 16))
    out.append(hexlines(bytes(pack_bits(bw16 & m16)) + bytes(pack_bits(m16)), "ics#", 128, 16))
    out.append(hexlines(logo_idx.tobytes(), "LOGO", 128, 48))
    with open(args.out, "w") as f:
        f.write("\n".join(out))
    print(f"escrito {args.out}  (colores usados a 32 px: {len(set(i32[m32].tolist()))}, a 16 px: {len(set(i16[m16].tolist()))})")

    if args.preview:
        contact_sheet(args.preview, pal, pal16, i32, m32, i16, m16, n32, n16, bw32, bw16, logo_idx, logo_mask)
        print(f"hoja de contacto {args.preview}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
