#!/usr/bin/env python3
"""Comprobaciones de audio sobre el PCM que escribe hostdec.

Uso: python3 -I tests/host/check_pcm.py <dir con *.raw> <dir con *.mp3>

  FULL vs ffmpeg   SNR (dB) y diferencia máxima, alineando el retardo. Informativo:
                   libmad es de punto fijo y ffmpeg de punto flotante.
  MONO vs FULL     CRITERIO: |MONO - (L+R)/2 de FULL| <= 2 LSB (mezclar antes o
                   después de la síntesis es lo mismo salvo redondeo).
  HALF == FULL     CRITERIO: HALF es exactamente una de cada dos muestras de FULL
                   (synth_half de libmad decima sin filtro pasa-bajos: aliasing).
  HALF_MONO        CRITERIO: igual a MONO decimado.
"""
import os
import subprocess
import sys

import numpy as np


def load_raw(path, channels):
    a = np.fromfile(path, dtype="<i2").astype(np.int64)
    return a.reshape(-1, channels) if channels > 1 else a.reshape(-1, 1)


def snr_db(ref, test):
    ref = ref.astype(np.float64)
    err = ref - test.astype(np.float64)
    p_sig = np.mean(ref ** 2)
    p_err = np.mean(err ** 2)
    if p_err == 0:
        return float("inf")
    return 10 * np.log10(p_sig / p_err)


def best_lag(a, b, max_lag=4000, n=1 << 18):
    """Retardo (en muestras) que alinea b con a, por correlación cruzada con FFT."""
    a = a[:n].astype(np.float64)
    b = b[:n].astype(np.float64)
    size = 1 << int(np.ceil(np.log2(len(a) + len(b))))
    c = np.fft.irfft(np.fft.rfft(a, size) * np.conj(np.fft.rfft(b, size)), size)
    lags = np.concatenate([np.arange(0, max_lag), np.arange(-max_lag, 0)])
    idx = np.concatenate([np.arange(0, max_lag), np.arange(size - max_lag, size)])
    return int(lags[np.argmax(c[idx])])


def ffmpeg_pcm(mp3, channels):
    out = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", mp3, "-f", "s16le", "-acodec", "pcm_s16le", "-"],
        capture_output=True, check=True).stdout
    a = np.frombuffer(out, dtype="<i2").astype(np.int64)
    return a.reshape(-1, channels)


def main():
    pcm_dir, mp3_dir = sys.argv[1], sys.argv[2]
    failed = False

    for name, ch in (("test128.mp3", 2), ("test192.mp3", 2), ("testmono.mp3", 1)):
        full = load_raw(f"{pcm_dir}/{name}.FULL.raw", ch)
        print(f"== {name}: {len(full)} muestras por canal")

        # FULL vs ffmpeg
        ref = ffmpeg_pcm(f"{mp3_dir}/{name}", ch)
        lag = best_lag(ref[:, 0], full[:, 0])
        if lag >= 0:
            r, f = ref[lag:], full
        else:
            r, f = ref, full[-lag:]
        n = min(len(r), len(f))
        r, f = r[:n], f[:n]
        print(f"  FULL vs ffmpeg: retardo {lag} muestras, SNR {snr_db(r, f):.1f} dB, "
              f"diff max {np.abs(r - f).max()} LSB (informativo)")

        # MONO vs mezcla de FULL
        mono = load_raw(f"{pcm_dir}/{name}.MONO.raw", 1)
        mix = np.floor((full.sum(axis=1) + (1 if ch == 2 else 0)) / ch)  # (L+R)/2 redondeado
        dmax = np.abs(mono[:, 0] - mix).max()
        ok = dmax <= 2
        failed |= not ok
        print(f"  MONO vs (L+R)/2 de FULL: diff max {dmax} LSB -> {'OK' if ok else 'FALLA (criterio <= 2)'}")

        # HALF == una de cada dos muestras de FULL (decimación sin filtro: synth_half de libmad)
        half = load_raw(f"{pcm_dir}/{name}.HALF.raw", ch)
        dec = full[0::2][:len(half)]
        ok = len(half) == len(full) // 2 and np.array_equal(dec, half)
        failed |= not ok
        print(f"  HALF == FULL[0::2]: largo {len(half)} (esperado {len(full) // 2}) -> "
              f"{'OK, identico bit a bit' if ok else 'FALLA'}")

        # HALF_MONO == MONO decimado
        hm = load_raw(f"{pcm_dir}/{name}.HALF_MONO.raw", 1)
        ok = np.array_equal(mono[0::2][:len(hm)], hm) and len(hm) == len(mono) // 2
        failed |= not ok
        print(f"  HALF_MONO == MONO[0::2]: -> {'OK, identico bit a bit' if ok else 'FALLA'}")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
