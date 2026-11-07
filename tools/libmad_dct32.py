"""
libmad_dct32 — extrae el DCT32 de third_party/libmad/synth.c para generar su referencia en C y su asm.

El dct32 de libmad es código recto: sentencias "tN = expr;" y escrituras "hi[i][slot] = SHIFT(expr);" /
"lo[i][slot] = SHIFT(expr);". Aquí se leen de la fuente original (rama sin OPT_DCTO: constantes de 28 bits y
MUL = mad_f_mul, o sea (int32)(((int64)x*y) >> 28)), de modo que la referencia y el kernel salen del mismo texto.
"""
import re


def extract(path):
    """Devuelve (costab, body): costab = {n: valor entero con signo}, body = lista de sentencias C (sin ';')."""
    text = open(path, encoding="latin-1").read()
    a = text.index("# if defined(OPT_DCTO)\n#  define costab1")
    b = text.index("# else\n", a)
    c = text.index("# endif\n", b)
    consts = {}
    for m in re.finditer(r"#\s*define\s+costab(\d+)\s+MAD_F\((0x[0-9a-fA-F]+)\)", text[b:c]):
        v = int(m.group(2), 16)
        consts[int(m.group(1))] = v - (1 << 32) if v >= 1 << 31 else v
    if len(consts) != 31:
        raise SystemExit("libmad_dct32: se esperaban 31 constantes costab y hay %d" % len(consts))
    start = c + len("# endif\n")
    end = text.index("\n}\n", start)
    body = re.sub(r"/\*.*?\*/", "", text[start:end], flags=re.S)
    stmts = [s.strip() for s in body.split(";") if s.strip()]
    for s in stmts:
        if not (re.match(r"^t\d+\s*=", s) or re.match(r"^(hi|lo)\[\s*\d+\]\[slot\]\s*=\s*SHIFT\(", s)):
            raise SystemExit("libmad_dct32: sentencia no reconocida: " + s)
    return consts, stmts
