#!/usr/bin/env python3
"""Pruebas del ecualizador sobre audio real (MP3 de ruido blanco y de transitorios).

Uso: check_eq.py <raíz del proyecto>

Decodifica con test_codecs (los mismos fuentes que la app, en 64 y 32 bits) con el EQ apagado y con
distintos ajustes (variable MP368K_EQ), y comprueba:
  * EQ plano o apagado: PCM idéntico al de siempre (y al REFERENCE_CODECS.txt).
  * 32 y 64 bits dan exactamente los mismos bytes con el EQ activo.
  * Respuesta en frecuencia: el cambio de potencia en un tercio de octava alrededor de cada centro
    coincide con el modelo (interpolación lineal en frecuencia entre centros) a 44,1, 48 y 32 kHz, en
    mono y con ajustes mezclados. Tolerancia: ±3 dB en 70/180/320 Hz (38 Hz por línea: poca
    resolución, es un límite conocido) y ±1,5 dB en el resto.
  * Bloques cortos: con el archivo de transitorios se ejercitan de verdad (contador del arnés) y el
    realce de 3 kHz se nota.
Sale con 1 si algo falla.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np

ROOT = sys.argv[1]
DATA = os.path.join(ROOT, "tests", "data")
BH = os.path.join(ROOT, "build-host")
BINS = {"64": os.path.join(BH, "test_codecs64"), "32": os.path.join(BH, "test_codecs32")}
BANDS_HZ = [70, 180, 320, 600, 1000, 3000, 6000, 12000, 14000, 16000]
fails = 0


def check(name, ok, detail=""):
    global fails
    print(f"  {'OK   ' if ok else 'FALLA'} {name}" + (f"  [{detail}]" if detail and not ok else ""))
    if not ok:
        fails += 1


def decode(binary, path, out, eq=None):
    env = dict(os.environ)
    env.pop("MP368K_EQ", None)
    if eq is not None:
        env["MP368K_EQ"] = ",".join(str(v) for v in eq)
    p = subprocess.run([binary, "decode", path, out], capture_output=True, text=True, env=env, timeout=120)
    kv = dict(l.split("=", 1) for l in p.stdout.splitlines() if "=" in l)
    return p.returncode, kv


def spectrum(pcm, channels, rate):
    """Potencia media por frecuencia del canal izquierdo (Welch, Hann, 4096)."""
    x = pcm.reshape(-1, channels)[:, 0].astype(np.float64)
    n = 4096
    win = np.hanning(n)
    acc = np.zeros(n // 2 + 1)
    count = 0
    for i in range(0, len(x) - n, n // 2):
        acc += np.abs(np.fft.rfft(x[i:i + n] * win)) ** 2
        count += 1
    return np.fft.rfftfreq(n, 1.0 / rate), acc / max(count, 1)


def model_gain_db(f, steps, rate):
    """Ganancia en dB del modelo: interpolación lineal en frecuencia entre centros, plana fuera."""
    centers = np.array(BANDS_HZ, dtype=np.float64)
    db = np.array([0.5 * (steps[k] + steps[10]) for k in range(10)])
    return np.interp(f, centers, db)


def measure(base, eqd, channels, rate, steps):
    """(medido, modelo) en dB del cambio de potencia en un tercio de octava alrededor de cada centro."""
    f, p0 = spectrum(base, channels, rate)
    _, p1 = spectrum(eqd, channels, rate)
    res = []
    for fc in BANDS_HZ:
        lo, hi = fc / 2 ** (1 / 6), fc * 2 ** (1 / 6)
        sel = (f >= lo) & (f <= hi) & (f < rate / 2 * 0.95)
        if not sel.any() or fc >= rate / 2 * 0.9:
            res.append(None)
            continue
        got = 10 * np.log10(p1[sel].sum() / p0[sel].sum())
        want = 10 * np.log10((p0[sel] * 10 ** (model_gain_db(f[sel], steps, rate) / 10)).sum() / p0[sel].sum())
        res.append((got, want))
    return res


def main():
    with tempfile.TemporaryDirectory() as tmp:
        out_a, out_b = os.path.join(tmp, "a.raw"), os.path.join(tmp, "b.raw")

        print("== EQ plano o apagado: sin cambios")
        ref = {}
        for l in open(os.path.join(DATA, "REFERENCE_CODECS.txt")):
            if l.startswith("codec,"):
                p = l.strip().split(",")
                ref[p[1]] = p[5]
        import zlib
        for name in ("test128.mp3", "test192.mp3", "testmono.mp3"):
            for bits, binary in BINS.items():
                rc, _ = decode(binary, os.path.join(DATA, name), out_a, eq=[0] * 11)
                # el CRC de REFERENCE_CODECS cubre los primeros 12 s; se compara el prefijo equivalente
                pcm = np.fromfile(out_a, dtype="<i2")
                ch = 1 if name == "testmono.mp3" else 2
                frames = 529200
                crc = zlib.crc32(pcm[: frames * ch].astype(">i2").tobytes()) & 0xFFFFFFFF
                check(f"[{bits}] {name} con EQ plano: CRC de referencia", rc == 0 and f"{crc:08X}" == ref[name])

        print("== 32 y 64 bits: mismos bytes con el EQ activo")
        eq_mix = [12, -6, 4, 0, 10, -12, 8, 0, -4, 12, 0]
        for name in ("testnoise44.mp3", "testtransient.mp3", "testnoisemono.mp3", "testnoise32.mp3"):
            outs = []
            for bits, binary in BINS.items():
                path = os.path.join(tmp, f"o{bits}.raw")
                decode(binary, os.path.join(DATA, name), path, eq=eq_mix)
                outs.append(open(path, "rb").read())
            check(f"{name}: 64 bits == 32 bits", outs[0] == outs[1] and len(outs[0]) > 0)

        print("== respuesta en frecuencia contra el modelo (ruido blanco, 32 bits)")
        binary = BINS["32"]
        cases = []
        for k in range(10):
            steps = [0] * 11
            steps[k] = 24
            cases.append((f"banda {BANDS_HZ[k]} Hz a +12 dB", steps))
        for k in (1, 5, 9):
            steps = [0] * 11
            steps[k] = -24
            cases.append((f"banda {BANDS_HZ[k]} Hz a -12 dB", steps))
        cases.append(("ajuste mezclado", eq_mix))
        cases.append(("preamp -12 dB", [0] * 10 + [-24]))
        cases.append(("preamp +6 dB y banda 1 kHz a +6", [0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 12]))
        for fname, rate, ch in (("testnoise44.mp3", 44100, 2), ("testnoise48.mp3", 48000, 2),
                                ("testnoise32.mp3", 32000, 2), ("testnoisemono.mp3", 44100, 1)):
            path = os.path.join(DATA, fname)
            decode(binary, path, out_a)
            base = np.fromfile(out_a, dtype="<i2")
            worst = {"low": 0.0, "rest": 0.0}
            bad = []
            for label, steps in cases:
                decode(binary, path, out_b, eq=steps)
                eqd = np.fromfile(out_b, dtype="<i2")
                n = min(len(base), len(eqd))
                for k, r in enumerate(measure(base[:n], eqd[:n], ch, rate, steps)):
                    if r is None:
                        continue
                    err = abs(r[0] - r[1])
                    tol = 3.0 if BANDS_HZ[k] <= 320 else 1.5
                    key = "low" if BANDS_HZ[k] <= 320 else "rest"
                    worst[key] = max(worst[key], err)
                    if err > tol:
                        bad.append(f"{label} @ {BANDS_HZ[k]} Hz: medido {r[0]:+.1f} dB, modelo {r[1]:+.1f} dB")
            check(f"{fname}: {len(cases)} ajustes; error máximo {worst['low']:.2f} dB en graves (tol 3) y "
                  f"{worst['rest']:.2f} dB en el resto (tol 1,5)", not bad, "; ".join(bad[:3]))

        print("== bloques cortos (archivo de transitorios)")
        rc, kv = decode(binary, os.path.join(DATA, "testtransient.mp3"), out_b, eq=[0, 0, 0, 0, 0, 24, 0, 0, 0, 0, 0])
        blocks = [int(v) for v in kv.get("eq_blocks", "0,0,0").split(",")]
        check(f"se procesan granulos de bloque corto (largo={blocks[0]} corto={blocks[1]} mixto={blocks[2]})",
              blocks[1] > 50 and blocks[0] > 50)
        decode(binary, os.path.join(DATA, "testtransient.mp3"), out_a)
        base = np.fromfile(out_a, dtype="<i2")
        eqd = np.fromfile(out_b, dtype="<i2")
        n = min(len(base), len(eqd))
        f, p0 = spectrum(base[:n], 2, 44100)
        _, p1 = spectrum(eqd[:n], 2, 44100)
        sel = (f > 2700) & (f < 3300)
        gain = 10 * np.log10(p1[sel].sum() / p0[sel].sum())
        check(f"banda de 3 kHz a +12 dB sobre transitorios: {gain:+.1f} dB medidos (se esperan +8 a +13)", 8 <= gain <= 13)
        sel = (f > 400) & (f < 700)
        far = 10 * np.log10(p1[sel].sum() / p0[sel].sum())
        check(f"lejos de la banda (400-700 Hz) casi no cambia: {far:+.1f} dB", abs(far) < 1.5)

    print()
    print("RESULTADO:", "todo OK (0 fallas)" if fails == 0 else f"{fails} FALLAS")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
