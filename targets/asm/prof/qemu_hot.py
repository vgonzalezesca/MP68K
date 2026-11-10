#!/usr/bin/env python3
"""
qemu_hot — instrucciones dinámicas por función de un programa de 68k corrido bajo qemu-m68k, en régimen estable.

Uso: qemu_hot.py [opciones] -- qemu-m68k -cpu m68040 programa [argumentos con {FRAMES}]

Corre QEMU con `-d in_asm,exec,nochain` y lee la traza por una tubería (no se guarda: son decenas de MB cada 50 ms de audio).
`in_asm` da las instrucciones de cada bloque de traducción (TB) con su función, y `exec` cada ejecución de un TB; instrucciones
dinámicas de una función = suma de (ejecuciones del TB x instrucciones del TB).

El programa se corre DOS veces, con {FRAMES} = --f1 y --f2, y se informa la DIFERENCIA: es el costo de decodificar (f2 - f1) cuadros más,
sin la inicialización (tablas, qsort...) y sin lo que no escala con el audio. Se excluyen las funciones del arnés de prueba (CRC, E/S).

ES UN CONTEO DE INSTRUCCIONES, NO TIEMPO. El 68040 real tiene instrucciones de costos muy distintos (muls.l, divisiones, saltos) y cachés
de 4 KB que QEMU no modela. Se agrega una estimación con PESOS SUPUESTOS (por verificar con el M68040UM y con microbench en el LC 580); las
decisiones se toman con el conteo y con las multiplicaciones ejecutadas, que sí son hechos. memcpy/memmove/memset son los de mp368k minilibc
(bucles de bytes), más lentos que los de la libc real: se marcan.
"""
import argparse
import bisect
import os
import re
import subprocess
import sys
import tempfile

# Modelo de ciclos: tiempos de la sección 10 del MC68040UM tal como los cobra el emulador Snow (su timing040.rs, leído el
# 2026-11-09), que es donde Víctor midió microbench y audiotest: muls.l 20, muls.w 16, desplazamientos 2-4, salto tomado 2 y
# no tomado 3, rts 5, modos indexados 3-4, y 1 para casi todo lo demás. Se aproxima desde el texto del desensamblado de QEMU.
# NO incluye el caché de datos (QEMU no da las direcciones de los datos); el de instrucciones se simula aparte.
INDEXED = re.compile(r"@\([^)]*,%")
MOVEM_RANGE = re.compile(r"%(d|a)(\d)-%(?:d|a)(\d)")
MOVEM_ONE = re.compile(r"%(?:d\d|a\d|fp|sp)")
MULS = re.compile(r"^mul[su][lw]?$")
COND = re.compile(r"^b(ne|eq|pl|mi|cc|cs|hi|ls|ge|lt|gt|le|vc|vs)[swl]?$")
SHIFT = re.compile(r"^(as|ls|ro|rox)[lr][bwl]?$")
ALU_A = re.compile(r"^(adda|suba)[wl]$")


def movem_regs(ops):
    n = 0
    lst = [o for o in ops.split(",") if "@" not in o]
    for part in "/".join(lst).split("/"):
        m = MOVEM_RANGE.search(part)
        if m:
            n += int(m.group(3)) - int(m.group(2)) + 1
        elif MOVEM_ONE.search(part):
            n += 1
    return max(n, 1)


def insn_cost(mn, ops):
    """(espera, base) en ciclos de una instrucción, por su mnemónico y sus operandos en la sintaxis de QEMU."""
    idx = 2 if INDEXED.search(ops) else 0
    pcrel = 2 if "%pc@" in ops else 0
    mem = "@" in ops
    if mn in ("mulsl", "mulul"):
        return pcrel, 20 + idx
    if mn in ("mulsw", "muluw", "muls", "mulu"):
        return pcrel, 16 + idx
    if mn.startswith("div"):
        return pcrel, (44 if mn.endswith("l") else 27) + idx
    if SHIFT.match(mn):
        base = 3 if mn.startswith("asl") else 2
        return 0, base + (0 if ops.startswith("#") or mem else 1) + idx
    if mn == "swap" or mn == "extw":
        return 0, 2
    if mn.startswith("bf"):
        reg = 0 if re.search(r",\d+,\d+", ops) or re.match(r"[^,]*,\d+,\d+", ops) else 1
        base = {"bfextu": 4, "bfexts": 4, "bfffo": 6, "bfins": 4, "bftst": 4}.get(mn, 6)
        return (2, base + 3 + idx) if mem else (0, base + reg)
    if COND.match(mn):
        return 0, 0                                 # se cobra al ejecutar: 2 si salta, 3 si no
    if mn.startswith("bra"):
        return 0, 2
    if mn.startswith("bsr"):
        return 1, 1
    if mn in ("jsr", "jmp"):
        if pcrel:
            return 5, 1
        if idx:
            return 0, 6
        return (3, 1) if "@(" in ops else (2, 1)
    if mn == "rts":
        return 0, 5
    if mn.startswith("db"):
        return 0, 3
    if mn.startswith("movem"):
        to_regs = ops.split(",")[0].find("@") >= 0
        return 1, (2 if to_regs else 1) + movem_regs(ops) + (6 if idx else 0)
    if mn == "lea":
        return (0, 4) if idx else ((3, 1) if pcrel else ((1, 1) if "@(" in ops else (0, 1)))
    if mn == "pea":
        return (0, 5) if idx else (1, 2)
    if mn.startswith("link"):
        return 2, 1
    if mn == "unlk":
        return 1, 1
    if ALU_A.match(mn):
        src = ops.split(",")[0]
        if idx:
            return 0, 5
        if src.startswith("#") or src.startswith("%a") and "@" not in src:
            return 0, 1
        return (1 if "@(" in src or "@+" in src or "@-" in src else 0) + pcrel, 2
    if mn.startswith("move"):
        parts = ops.rsplit(",", 1)
        src, dst = (parts + [""])[:2] if len(parts) == 2 else (ops, "")
        si, di = bool(INDEXED.search(src)), bool(INDEXED.search(dst))
        if si and di:
            return 0, 8
        if si:
            return 0, 5 if "@" in dst else 3
        if di:
            return 0, 4 if "@" in src else 3
        if "@" in src and "@" in dst:
            return 1 + pcrel, 1
        return pcrel, 1
    return pcrel, 1 + idx


HARNESS = {"decode_one", "main", "crc_init", "report", "read_file", "io_read", "io_seek", "cb_read", "cb_seek", "cb_tell",
           "cb_close", "say", "say_dec", "say_hex", "eq_on", "_start", "?"}
LIBC = {"memcpy", "memmove", "memset", "memcmp", "strlen", "strcpy", "strcat", "memchr", "qsort"}

IN_RE = re.compile(r"^IN:\s*(\S*)")
INSN_RE = re.compile(r"^0x([0-9a-f]+):\s+(\S+)\s*(.*)")
EXEC_RE = re.compile(r"^Trace \d+: 0x[0-9a-f]+ \[[0-9a-f]+/([0-9a-f]+)/")
TARGET_RE = re.compile(r"0x([0-9a-f]+)\s*$")
CLOCK_HZ = 33333333.0           # reloj del 68040 del Quadra 800 de Snow y del LC 580 (dato de EveryMac, por verificar)
ICACHE_MISS = 15                # ciclos de traer una línea de 16 bytes de la RAM en el Quadra 800 de Snow (6-3-3-3)


def load_symbols(elf, nm):
    """Símbolos definidos del ELF ordenados por dirección (incluye los de asm sin tipo de función, que QEMU no nombra)."""
    out = subprocess.run([nm, "-n", "--defined-only", elf], capture_output=True, text=True).stdout
    addrs, names = [], []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "TtWw":
            addrs.append(int(parts[0], 16))
            names.append(parts[2])
    return addrs, names


class Tb:
    __slots__ = ("fn", "pc", "insns", "cyc", "muls", "target", "lines", "prev", "execs", "taken", "imiss")

    def __init__(self, fn):
        self.fn, self.pc, self.insns, self.cyc, self.muls = fn, None, 0, 0, 0
        self.target, self.lines, self.prev = None, (), 2
        self.execs = self.taken = self.imiss = 0


def profile(cmd, syms=None):
    """Devuelve ({función: [instr, ciclos, mul, fallos del caché de instrucciones]}, salida del programa)."""
    tmp = tempfile.mkdtemp(prefix="qemu_hot_")
    fifo = os.path.join(tmp, "trace")
    os.mkfifo(fifo)
    proc = subprocess.Popen([cmd[0], "-d", "in_asm,exec,nochain", "-D", fifo] + cmd[1:],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    tbs, cur, last = {}, None, None
    # caché de instrucciones del 68040: 64 conjuntos de 4 líneas de 16 bytes; el reemplazo real usa un contador que aquí
    # se aproxima con uno que avanza en cada fallo
    sets = [[-1, -1, -1, -1] for _ in range(64)]
    victim = 0
    with open(fifo, "r", errors="replace") as f:
        for line in f:
            if cur is not None:
                m = INSN_RE.match(line)
                if m:
                    addr, mn, ops = int(m.group(1), 16), m.group(2), m.group(3).strip()
                    if cur.pc is None:
                        cur.pc = addr
                        cur.lines = [addr >> 4]
                    elif cur.lines[-1] != addr >> 4:
                        cur.lines.append(addr >> 4)
                    if (addr + 2) >> 4 != cur.lines[-1]:
                        cur.lines.append((addr + 2) >> 4)
                    cur.insns += 1
                    lead, base = insn_cost(mn, ops)
                    cur.cyc += base + max(0, lead - (cur.prev - 1))
                    cur.prev = base or 2
                    cur.muls += 1 if MULS.match(mn) else 0
                    if COND.match(mn):
                        t = TARGET_RE.search(ops)
                        cur.target = int(t.group(1), 16) if t else None
                    continue
                tbs[cur.pc] = cur
                cur = None
            m = EXEC_RE.match(line)
            if m:
                pc = int(m.group(1), 16)
                if last is not None and last.target == pc:
                    last.taken += 1
                tb = tbs.get(pc)
                last = tb
                if tb is not None:
                    tb.execs += 1
                    for ln in tb.lines:
                        ways = sets[ln & 63]
                        if ln not in ways:
                            ways[victim & 3] = ln
                            victim += 1
                            tb.imiss += 1
                continue
            m = IN_RE.match(line)
            if m:
                cur = Tb(m.group(1) or "?")
    out, err = proc.communicate()
    os.unlink(fifo)
    os.rmdir(tmp)
    if proc.returncode != 0:
        sys.stderr.write(err)
        sys.exit("qemu_hot: el programa terminó con código %d" % proc.returncode)
    per = {}
    for pc, tb in tbs.items():
        if not tb.execs:
            continue
        fn = tb.fn
        if fn == "?" and syms is not None:                  # asm sin tipo de función: se busca por dirección
            k = bisect.bisect_right(syms[0], pc) - 1
            if k >= 0:
                fn = syms[1][k]
        r = per.setdefault(fn, [0, 0, 0, 0])
        r[0] += tb.execs * tb.insns
        r[1] += tb.execs * tb.cyc
        if tb.target is not None:                           # salto condicional final: 2 ciclos si salta, 3 si no
            r[1] += 2 * tb.taken + 3 * (tb.execs - tb.taken)
        r[2] += tb.execs * tb.muls
        r[3] += tb.imiss
    return per, out


def frames_of(out):
    return max([int(x) for x in re.findall(r"frames=(\d+)", out)] or [0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=12)
    ap.add_argument("--csv")
    ap.add_argument("--label", default="")
    ap.add_argument("--f1", type=int, required=True, help="cuadros de la corrida corta")
    ap.add_argument("--f2", type=int, required=True, help="cuadros de la corrida larga")
    ap.add_argument("--elf", help="ELF del programa, para ponerle nombre a las funciones de asm que QEMU no nombra")
    ap.add_argument("--nm", default=os.path.expanduser("~/Retro68-build/toolchain/bin/m68k-apple-macos-nm"))
    ap.add_argument("--unit", type=int, default=1, help="muestras por canal que vale cada 'cuadro' que informa el programa (MP3: 1152)")
    ap.add_argument("--rate", type=int, default=44100, help="Hz del audio de entrada, para el porcentaje de tiempo real")
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd and a.cmd[0] == "--" else a.cmd
    if not cmd or not any("{FRAMES}" in c for c in cmd):
        sys.exit("qemu_hot: el comando debe llevar {FRAMES}")

    syms = load_symbols(a.elf, a.nm) if a.elf else None
    pa, outa = profile([c.replace("{FRAMES}", str(a.f1)) for c in cmd], syms)
    pb, outb = profile([c.replace("{FRAMES}", str(a.f2)) for c in cmd], syms)
    fa, fb = frames_of(outa), frames_of(outb)
    if fb <= fa:
        sys.exit("qemu_hot: la corrida larga no produjo más cuadros (%d y %d)" % (fa, fb))
    per = {}
    for fn in set(pa) | set(pb):
        x = pa.get(fn, [0, 0, 0, 0])
        y = pb.get(fn, [0, 0, 0, 0])
        d = [y[k] - x[k] for k in range(4)]
        if fn in HARNESS or d[0] <= 0:
            continue
        per[fn] = d
    tot_i = sum(r[0] for r in per.values()) or 1
    tot_c = sum(r[1] for r in per.values()) or 1
    tot_m = sum(r[2] for r in per.values())
    tot_x = sum(r[3] for r in per.values())
    samples = (fb - fa) * a.unit
    budget = CLOCK_HZ / a.rate
    rows = sorted(per.items(), key=lambda kv: -kv[1][1])
    cyc = tot_c / samples
    icache = tot_x * ICACHE_MISS / samples
    print("# %s" % a.label)
    print("# costo marginal de %d cuadros (%d muestras por canal): %.1f instr, %.2f mul* y %.0f ciclos del modelo por muestra-canal"
          % (fb - fa, samples, tot_i / samples, tot_m / samples, cyc))
    print("# modelo (MC68040UM sec. 10 como lo cobra Snow, SIN caché de datos): %.0f ciclos + %.0f de fallos del caché de instrucciones"
          " = %.0f%% del tiempo real a 33,33 MHz y %d Hz (presupuesto %.0f)"
          % (cyc, icache, 100.0 * (cyc + icache) / budget, a.rate, budget))
    print("# NO es una medición: ni de Snow (falta el caché de datos) ni del LC 580. Sin inicialización ni arnés de prueba.")
    print("# %-34s %8s %10s %10s %10s %8s" % ("función", "% ciclos", "ciclos/m-c", "instr/m-c", "mul*/m-c", "i-miss"))
    for fn, (i, c, m, x) in rows[:a.top]:
        tag = " (libc simulada)" if fn in LIBC else ""
        print("  %-34s %7.1f%% %10.1f %10.2f %10.3f %8.3f" % ((fn + tag)[:34], 100.0 * c / tot_c, c / samples, i / samples, m / samples, x / samples))
    rest = sum(r[1] for _, r in rows[a.top:])
    if rest:
        print("  %-34s %7.1f%% %10.1f" % ("(otras %d funciones)" % (len(rows) - a.top), 100.0 * rest / tot_c, rest / samples))
    if a.csv:
        with open(a.csv, "w") as fh:
            fh.write("funcion,instr_por_muestra_canal,mul_por_muestra_canal,ciclos_modelo_por_muestra_canal,fallos_icache_por_muestra_canal\n")
            for fn, (i, c, m, x) in rows:
                fh.write("%s,%.3f,%.4f,%.3f,%.4f\n" % (fn, i / samples, m / samples, c / samples, x / samples))


if __name__ == "__main__":
    main()
