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

# Pesos supuestos en ciclos por instrucción (por verificar): 1 por omisión.
WEIGHTS = [
    (re.compile(r"^mul[su]l$"), 20),        # 32x32 -> 64 (docs/SPECS_68K.md usa ~20)
    (re.compile(r"^mul[su](w)?$"), 5),
    (re.compile(r"^div"), 40),
    (re.compile(r"^bf"), 15),
]
MULS = re.compile(r"^mul[su](l|w)?$")
HARNESS = {"decode_one", "main", "crc_init", "report", "read_file", "io_read", "io_seek", "cb_read", "cb_seek", "cb_tell",
           "cb_close", "say", "say_dec", "say_hex", "eq_on", "_start", "?"}
LIBC = {"memcpy", "memmove", "memset", "memcmp", "strlen", "strcpy", "strcat", "memchr", "qsort"}

IN_RE = re.compile(r"^IN:\s*(\S*)")
INSN_RE = re.compile(r"^0x([0-9a-f]+):\s+(\S+)")
EXEC_RE = re.compile(r"^Trace \d+: 0x[0-9a-f]+ \[[0-9a-f]+/([0-9a-f]+)/")


def weight(mn):
    for rx, w in WEIGHTS:
        if rx.match(mn):
            return w
    return 1


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


def profile(cmd, syms=None):
    """Devuelve ({función: [instr, ciclos, mul]}, salida del programa)."""
    tmp = tempfile.mkdtemp(prefix="qemu_hot_")
    fifo = os.path.join(tmp, "trace")
    os.mkfifo(fifo)
    proc = subprocess.Popen([cmd[0], "-d", "in_asm,exec,nochain", "-D", fifo] + cmd[1:],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    tb, execs, cur = {}, {}, None
    with open(fifo, "r", errors="replace") as f:
        for line in f:
            if cur is not None:
                m = INSN_RE.match(line)
                if m:
                    if cur[1] is None:
                        cur[1] = int(m.group(1), 16)
                    mn = m.group(2)
                    cur[2] += 1
                    cur[3] += weight(mn)
                    cur[4] += 1 if MULS.match(mn) else 0
                    continue
                tb[cur[1]] = (cur[2], cur[3], cur[4], cur[0] or "?")
                cur = None
            m = IN_RE.match(line)
            if m:
                cur = [m.group(1), None, 0, 0, 0]
                continue
            m = EXEC_RE.match(line)
            if m:
                pc = int(m.group(1), 16)
                execs[pc] = execs.get(pc, 0) + 1
    out, err = proc.communicate()
    os.unlink(fifo)
    os.rmdir(tmp)
    if proc.returncode != 0:
        sys.stderr.write(err)
        sys.exit("qemu_hot: el programa terminó con código %d" % proc.returncode)
    per = {}
    for pc, n in execs.items():
        info = tb.get(pc)
        if info is None:
            continue
        insns, cyc, muls, fn = info
        if fn == "?" and syms is not None:                  # asm sin tipo de función: se busca por dirección
            k = bisect.bisect_right(syms[0], pc) - 1
            if k >= 0:
                fn = syms[1][k]
        r = per.setdefault(fn, [0, 0, 0])
        r[0] += n * insns
        r[1] += n * cyc
        r[2] += n * muls
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
        x = pa.get(fn, [0, 0, 0])
        y = pb.get(fn, [0, 0, 0])
        d = [y[0] - x[0], y[1] - x[1], y[2] - x[2]]
        if fn in HARNESS or d[0] <= 0:
            continue
        per[fn] = d
    tot_i = sum(r[0] for r in per.values()) or 1
    tot_c = sum(r[1] for r in per.values()) or 1
    tot_m = sum(r[2] for r in per.values())
    samples = (fb - fa) * a.unit
    rows = sorted(per.items(), key=lambda kv: -kv[1][0])
    print("# %s" % a.label)
    print("# costo marginal de %d cuadros (%d muestras por canal): %d instrucciones = %.1f instr/muestra-canal, %.2f mul*/muestra-canal"
          % (fb - fa, samples, tot_i, tot_i / samples, tot_m / samples))
    print("# (conteo de instrucciones, NO tiempo; sin inicialización ni arnés de prueba)")
    print("# %-34s %8s %8s %10s %10s" % ("función", "% instr", "% est.", "instr/m-c", "mul*/m-c"))
    for fn, (i, c, m) in rows[:a.top]:
        tag = " (libc simulada)" if fn in LIBC else ""
        print("  %-34s %7.1f%% %7.1f%% %10.2f %10.3f" % ((fn + tag)[:34], 100.0 * i / tot_i, 100.0 * c / tot_c, i / samples, m / samples))
    rest = sum(r[0] for _, r in rows[a.top:])
    if rest:
        print("  %-34s %7.1f%%" % ("(otras %d funciones)" % (len(rows) - a.top), 100.0 * rest / tot_i))
    print("# '%% est.' usa pesos SUPUESTOS por verificar: mulsl/mulul=20, mulsw=5, div*=40, bf*=15, el resto 1")
    if a.csv:
        with open(a.csv, "w") as fh:
            fh.write("funcion,instr_por_muestra_canal,mul_por_muestra_canal,ciclos_supuestos_por_muestra_canal\n")
            for fn, (i, c, m) in rows:
                fh.write("%s,%.3f,%.4f,%.3f\n" % (fn, i / samples, m / samples, c / samples))


if __name__ == "__main__":
    main()
