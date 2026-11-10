"""
dflow — generador de ensamblador 68040 para rutinas de flujo de datos recto (DCT32, IMDCT...).

Se escribe el algoritmo en Python con operadores sobre valores simbólicos (Sym); cada operación se anota en
un programa, y emit_kernel() lo convierte a ensamblador con asignación de registros y derrame a la pila.
Todo es aritmética de 32 bits con envoltura, como la de libmad con FPM_64BIT:

  a + b, a - b, -a        add/sub/neg de 32 bits
  a * 2                   duplicar
  half(a)                 a / 2 de C: con signo, redondeado hacia cero
  mul28(a, c)             (int32)(((int64)a * c) >> 28), truncado (mad_f_mul de libmad); c es una constante
                          entera o una referencia a memoria (mem(base, byte_offset))

Convención de los registros del kernel generado:
  d1 = parte alta del producto (borrador)     d7 = 28 (cuenta del desplazamiento)
  d0, d2-d6 = valores                          a0 = entrada, a1/a2 = bases de salida (las fija el prólogo)
Las rutas de derrame usan 4*n(%sp). Sin FPU, sin código auto-modificable.

SIN MEDIR en el LC 580: no se declara más rápido que la versión en C hasta medirlo (ADR-008).
"""

DATA_REGS = ["%d0", "%d2", "%d3", "%d4", "%d5", "%d6"]


class Mem:
    """Operando en memoria: byte_offset(base). Solo lectura (entrada o tabla)."""

    def __init__(self, base, off):
        self.base, self.off = base, off

    def text(self):
        return f"{self.off}({self.base})"


class Sym:
    prog = None

    def __init__(self, kind, args=(), mem=None, imm=None):
        self.kind, self.args, self.mem, self.imm = kind, args, mem, imm
        self.id = len(Sym.prog.syms)
        Sym.prog.syms.append(self)
        if kind != "in":
            Sym.prog.ops.append(self)

    def __add__(self, o):
        if isinstance(o, int):
            raise TypeError("sumas con constantes no soportadas")
        return Sym("add", (self, o))

    def __sub__(self, o):
        return Sym("sub", (self, o))

    def __neg__(self):
        return Sym("neg", (self,))

    def __mul__(self, o):
        if o != 2:
            raise ValueError("solo se admite * 2")
        return Sym("dbl", (self,))

    __rmul__ = __mul__


class Prog:
    def __init__(self):
        self.syms = []
        self.ops = []
        self.stores = []

    def __enter__(self):
        Sym.prog = self
        return self

    def __exit__(self, *a):
        Sym.prog = None


def inp(mem):
    return Sym("in", mem=mem)


def half(a):
    return Sym("half", (a,))


def mul28(a, c):
    """c: int (constante de 32 bits con signo) o Mem."""
    if isinstance(c, int):
        c = ((c + (1 << 31)) & 0xFFFFFFFF) - (1 << 31)
        return Sym("mulc", (a,), imm=c)
    return Sym("mulm", (a,), mem=c)


def store(mem, a):
    """Escribe a en memoria (4 bytes)."""
    s = Sym("store", (a,), mem=mem)
    return s


def prune(prog):
    """Quita las operaciones cuyo resultado no llega a ninguna escritura en memoria (código muerto)."""
    live = set()
    for op in reversed(prog.ops):
        if op.kind == "store" or op.id in live:
            live.add(op.id)
            for a in op.args:
                live.add(a.id)
    prog.ops = [op for op in prog.ops if op.id in live]


def emit_kernel(prog, name, prologue, epilogue, frame_slots_extra=0):
    """Devuelve el texto del cuerpo (entre prólogo y epílogo) y el número de huecos de pila usados."""
    prune(prog)
    ops = prog.ops
    uses = {s.id: [] for s in prog.syms}
    for i, op in enumerate(ops):
        for a in op.args:
            uses[a.id].append(i)

    out = []
    reg_of = {}          # id -> registro
    holder = {}          # registro -> id
    slot_of = {}         # id -> hueco de pila
    free_slots = []
    nslots = [0]
    labels = [0]

    def last_use(s):
        u = uses[s.id]
        return u[-1] if u else -1

    def next_use(s, pos):
        for u in uses[s.id]:
            if u > pos:
                return u
        return 10 ** 9

    def operand(s):
        if s.kind == "in":
            return s.mem.text()
        if s.id in reg_of:
            return reg_of[s.id]
        return f"{4 * slot_of[s.id]}(%sp)"

    def new_slot():
        if free_slots:
            return free_slots.pop()
        nslots[0] += 1
        return nslots[0] - 1

    def release(s):
        if s.id in reg_of:
            r = reg_of.pop(s.id)
            del holder[r]
        if s.id in slot_of:
            free_slots.append(slot_of.pop(s.id))

    def get_reg(pos, protect):
        for r in DATA_REGS:
            if r not in holder:
                return r
        # derramar el valor con el uso más lejano
        best, best_nu = None, -1
        for r, sid in holder.items():
            if sid in protect:
                continue
            nu = next_use(prog.syms[sid], pos)
            if nu > best_nu:
                best, best_nu = r, nu
        sid = holder[best]
        sym = prog.syms[sid]
        if sid not in slot_of:
            slot_of[sid] = new_slot()
            out.append(f"        move.l  {best},{4 * slot_of[sid]}(%sp)")
        del holder[best]
        del reg_of[sid]
        return best

    def bind(sym, r):
        reg_of[sym.id] = r
        holder[r] = sym.id

    def dies(sym, pos):
        return sym.kind != "in" and last_use(sym) == pos

    def in_reg(sym):
        return sym.kind != "in" and sym.id in reg_of

    for pos, op in enumerate(ops):
        k = op.kind
        if k == "store":
            a = op.args[0]
            src = operand(a)
            if a.kind == "in" or (a.id not in reg_of):
                out.append(f"        move.l  {src},%d1")
                src = "%d1"
            out.append(f"        move.l  {src},{op.mem.text()}")
            if dies(a, pos):
                release(a)
            continue

        protect = {a.id for a in op.args if a.kind != "in"}
        dst = None
        if k in ("neg", "dbl", "half", "mulc", "mulm"):
            a = op.args[0]
            if in_reg(a) and dies(a, pos):
                dst = reg_of[a.id]
                release_a = True
                del holder[dst]
                del reg_of[a.id]
                if a.id in slot_of:
                    free_slots.append(slot_of.pop(a.id))
            else:
                dst = get_reg(pos, protect)
                out.append(f"        move.l  {operand(a)},{dst}")
                if dies(a, pos):
                    release(a)
            bind(op, dst)
            if k == "neg":
                out.append(f"        neg.l   {dst}")
            elif k == "dbl":
                out.append(f"        add.l   {dst},{dst}")
            elif k == "half":
                labels[0] += 1
                n = labels[0] % 9 + 1  # etiquetas locales numéricas: se pueden repetir
                out.append(f"        tst.l   {dst}")
                out.append(f"        bpl.s   {n}f")
                out.append(f"        addq.l  #1,{dst}")
                out.append(f"{n}:      asr.l   #1,{dst}")
            else:
                src = f"#{op.imm}" if k == "mulc" else op.mem.text()
                out.append(f"        muls.l  {src},%d1:{dst}")
                out.append(f"        lsl.l   #4,%d1")
                out.append(f"        lsr.l   %d7,{dst}")
                out.append(f"        add.l   %d1,{dst}")
            continue

        # add / sub
        a, b = op.args
        if k == "add" and in_reg(b) and dies(b, pos) and not (in_reg(a) and dies(a, pos)):
            a, b = b, a            # la suma conmuta: reutiliza el registro del que muere
        if in_reg(a) and dies(a, pos) and not (a is b):
            dst = reg_of[a.id]
            del holder[dst]
            del reg_of[a.id]
            if a.id in slot_of:
                free_slots.append(slot_of.pop(a.id))
            ob = operand(b)
            out.append(f"        {'add' if k == 'add' else 'sub'}.l   {ob},{dst}")
            if dies(b, pos) and b is not a:
                release(b)
        else:
            dst = get_reg(pos, protect)
            out.append(f"        move.l  {operand(a)},{dst}")
            out.append(f"        {'add' if k == 'add' else 'sub'}.l   {operand(b)},{dst}")
            if dies(a, pos):
                release(a)
            if dies(b, pos) and b is not a:
                release(b)
        bind(op, dst)

    return "\n".join(out), nslots[0]
