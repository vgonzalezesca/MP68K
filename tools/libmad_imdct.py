"""
libmad_imdct — extrae el IMDCT de bloques largos de third_party/libmad/layer3.c (rama activa "#if 1": el algoritmo
rápido de Szu-Wei Lee: fastsdct, sdctII, dctIV, imdct36) y la tabla window_l, para generar la referencia en C y el asm.
"""
import re


def _consts(text):
    out = []
    for m in re.finditer(r"(-?)MAD_F\((0x[0-9a-fA-F]+)\)", text):
        v = int(m.group(2), 16)
        out.append(-v if m.group(1) else v)
    return out


def extract(path):
    """Devuelve dict: code (texto C verbatim de las 4 funciones), window_l[36], c[7], scale9[9], scale18[18]."""
    text = open(path, encoding="latin-1").read()
    a = text.index("#  if 1\nstatic\nvoid fastsdct(")
    b = text.index("#  else\n", a)
    code = text[a + len("#  if 1\n"):b]
    wa = text.index("mad_fixed_t const window_l[36] = {")
    wl = _consts(text[wa:text.index("};", wa)])
    en = re.search(r"enum \{(.*?)\};", code, re.S).group(1)
    c = []
    for i in range(7):
        m = re.search(r"\bc%d\s*=\s*(-?)MAD_F\((0x[0-9a-fA-F]+)\)" % i, en)
        v = int(m.group(2), 16)
        c.append(-v if m.group(1) else v)
    s9 = _consts(re.search(r"scale\[9\] = \{(.*?)\};", code, re.S).group(1))
    s18 = _consts(re.search(r"scale\[18\] = \{(.*?)\};", code, re.S).group(1))
    if len(wl) != 36 or len(s9) != 9 or len(s18) != 18:
        raise SystemExit("libmad_imdct: tablas con tamaño inesperado (%d, %d, %d)" % (len(wl), len(s9), len(s18)))
    return {"code": code, "window_l": wl, "c": c, "scale9": s9, "scale18": s18}
