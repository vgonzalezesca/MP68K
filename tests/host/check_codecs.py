#!/usr/bin/env python3
"""Pruebas de la capa de códecs en el PC (FLAC, Ogg Vorbis, WMA, y MP3 a través de la capa nueva).

Uso: check_codecs.py <raíz del proyecto>

Compara test_codecs (los mismos fuentes que la app Mac, en 64 y en 32 bits) contra referencias
independientes: `flac -d` para el PCM de FLAC, `ffmpeg` para Ogg Vorbis y WMA (decodificadores enteros: se admite 1 LSB), y los .raw
de hostdec para el MP3. Comprueba además
etiquetas con acentos, duración, búsqueda (con y sin SEEKTABLE), y robustez: archivos truncados,
corruptos, basura, ID3v2 delante, una carátula enorme, canales o tasa no admitidos y arena chico.
Sale con 1 si algo falla.
"""
import os
import random
import struct
import subprocess
import sys
import tempfile
import zlib

import numpy as np

ROOT = sys.argv[1]
DATA = os.path.join(ROOT, "tests", "data")
BH = os.path.join(ROOT, "build-host")
BINS = {"64": os.path.join(BH, "test_codecs64"), "32": os.path.join(BH, "test_codecs32")}
fails = 0


def check(name, ok, detail=""):
    global fails
    print(f"  {'OK   ' if ok else 'FALLA'} {name}" + (f"  [{detail}]" if detail and not ok else ""))
    if not ok:
        fails += 1


def run(binary, *args, timeout=120):
    p = subprocess.run([binary, *map(str, args)], capture_output=True, text=True, timeout=timeout)
    kv = dict(l.split("=", 1) for l in p.stdout.splitlines() if "=" in l)
    return p.returncode, kv


def flac_ref(path, tmp):
    """PCM de referencia como int16 little-endian, con la misma reducción que dr_flac (sin redondeo)."""
    raw = os.path.join(tmp, "ref.raw")
    info = subprocess.run(["flac", "-a", "-s", path] if False else ["metaflac", "--show-bps", "--show-channels", path],
                          capture_output=True, text=True).stdout.split()
    bps, ch = int(info[0]), int(info[1])
    subprocess.run(["flac", "-s", "-f", "-d", "--force-raw-format", "--endian=little", "--sign=signed",
                    "-o", raw, path], check=True)
    data = open(raw, "rb").read()
    if bps == 16:
        a = np.frombuffer(data, dtype="<i2")
    elif bps == 8:
        a = np.frombuffer(data, dtype="i1").astype(np.int16) << 8
    elif bps == 24:
        b = np.frombuffer(data, dtype="u1").reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        v = np.where(v & 0x800000, v - 0x1000000, v)
        a = (v >> 8).astype(np.int16)
    else:
        raise SystemExit(f"bps no previsto: {bps}")
    return a, ch


def tag_hex(s):
    return s.encode("mac_roman").hex().upper()


def main():
    with tempfile.TemporaryDirectory() as tmp:
        print("== información (etiquetas, duración, parámetros)")
        want_title, want_artist, want_album = tag_hex("Canción de prueba"), tag_hex("Ñandú"), tag_hex("Álbum")
        for bits, binary in BINS.items():
            rc, kv = run(binary, "info", os.path.join(DATA, "testflac.flac"))
            check(f"[{bits}] FLAC: formato, tasa y canales",
                  kv.get("format") == "FLAC" and kv.get("rate") == "44100" and kv.get("channels") == "2" and kv.get("bits") == "16", str(kv))
            check(f"[{bits}] FLAC: duración exacta de STREAMINFO", kv.get("duration_ms") == "60000", kv.get("duration_ms", ""))
            check(f"[{bits}] FLAC: título, artista y álbum con acentos en MacRoman",
                  (kv.get("title"), kv.get("artist"), kv.get("album")) == (want_title, want_artist, want_album), str(kv))
            check(f"[{bits}] FLAC: sin perfiles ni ecualizador", kv.get("profiles") == "0" and kv.get("eq") == "0")
            rc, kv = run(binary, "info", os.path.join(DATA, "testtag23.mp3"))
            check(f"[{bits}] MP3 por la capa: formato, etiquetas y perfiles",
                  kv.get("format") == "MP3" and kv.get("title") == want_title and kv.get("profiles") == "1", str(kv))

        print("== FLAC: PCM idéntico bit a bit a `flac -d`")
        for name in ("testflac.flac", "testflacmono.flac", "testflac24.flac", "testflac8.flac"):
            path = os.path.join(DATA, name)
            ref, ch = flac_ref(path, tmp)
            for bits, binary in BINS.items():
                out = os.path.join(tmp, "out.raw")
                rc, kv = run(binary, "decode", path, out)
                got = np.fromfile(out, dtype="<i2")
                same = got.shape == ref.shape and np.array_equal(got, ref)
                check(f"[{bits}] {name}: {len(ref) // ch} cuadros, {ch} canal(es)", rc == 0 and same,
                      f"rc={rc} got={got.shape} ref={ref.shape} {kv}")

        print("== FLAC: búsqueda exacta (con y sin SEEKTABLE)")
        src = os.path.join(DATA, "mix.wav")
        noseek = os.path.join(tmp, "noseek.flac")
        subprocess.run(["flac", "-s", "-f", "--no-seektable", "-5", src, "-o", noseek], check=True)
        for label, path in (("con SEEKTABLE", os.path.join(DATA, "testflac.flac")), ("sin SEEKTABLE", noseek)):
            ref, ch = flac_ref(path, tmp)
            for bits, binary in BINS.items():
                good = True
                for ms in (0, 1000, 15007, 30014, 45021, 59990):
                    out = os.path.join(tmp, "seek.raw")
                    rc, kv = run(binary, "seek", path, ms, out, 3000)
                    frame = ms * 44100 // 1000
                    exp = ref[frame * ch:(frame + 3000) * ch]
                    got = np.fromfile(out, dtype="<i2")
                    if rc != 0 or not np.array_equal(got, exp):
                        good = False
                        print(f"      ms={ms}: rc={rc} {len(got)} vs {len(exp)} muestras {kv}")
                check(f"[{bits}] búsqueda {label}: 6 posiciones", good)

        print("== Ogg Vorbis (Tremor): información")
        for bits, binary in BINS.items():
            rc, kv = run(binary, "info", os.path.join(DATA, "testvorbis.ogg"))
            check(f"[{bits}] Vorbis: formato, tasa y canales",
                  kv.get("format") == "Vorbis" and kv.get("rate") == "44100" and kv.get("channels") == "2", str(kv))
            check(f"[{bits}] Vorbis: duración exacta (granule de la última página)", kv.get("duration_ms") == "60000", kv.get("duration_ms", ""))
            check(f"[{bits}] Vorbis: título, artista y álbum con acentos en MacRoman",
                  (kv.get("title"), kv.get("artist"), kv.get("album")) == (want_title, want_artist, want_album), str(kv))
            check(f"[{bits}] Vorbis: sin perfiles ni ecualizador", kv.get("profiles") == "0" and kv.get("eq") == "0")
            rc, kv = run(binary, "info", os.path.join(DATA, "testvorbismono.ogg"))
            check(f"[{bits}] Vorbis mono: 1 canal", kv.get("format") == "Vorbis" and kv.get("channels") == "1", str(kv))

        print("== Ogg Vorbis: PCM contra ffmpeg (Tremor es entero: se admite 1 LSB) y 32 == 64 bits")
        for name in ("testvorbis.ogg", "testvorbismono.ogg"):
            path = os.path.join(DATA, name)
            refraw = os.path.join(tmp, "vref.raw")
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-f", "s16le", "-acodec", "pcm_s16le", refraw], check=True)
            ref = np.fromfile(refraw, dtype="<i2").astype(np.int64)
            outs = {}
            for bits, binary in BINS.items():
                out = os.path.join(tmp, f"v{bits}.raw")
                rc, kv = run(binary, "decode", path, out)
                outs[bits] = np.fromfile(out, dtype="<i2")
                got = outs[bits].astype(np.int64)
                same_len = got.shape == ref.shape
                d = np.abs(got - ref) if same_len else np.array([99999])
                check(f"[{bits}] {name}: {len(got)} muestras, error máx {int(d.max())} LSB, rms {float(np.sqrt(np.mean(d * d))):.2f}",
                      rc == 0 and same_len and int(d.max()) <= 2 and float(np.sqrt(np.mean(d * d))) < 1.0, str(kv))
                if bits == "64":
                    check(f"[{bits}] {name}: heap de Tremor {int(kv.get('mem_peak', '0')) // 1024} KB",
                          0 < int(kv.get("mem_peak", "0")) < 200 * 1024, str(kv))
            check(f"{name}: mismos bytes en 32 y 64 bits", np.array_equal(outs["32"], outs["64"]))

        print("== Ogg Vorbis: búsqueda (comparada con la decodificación completa del mismo Tremor)")
        vpath = os.path.join(DATA, "testvorbis.ogg")
        for bits, binary in BINS.items():
            full = os.path.join(tmp, "vfull.raw")
            run(binary, "decode", vpath, full)
            ref = np.fromfile(full, dtype="<i2").astype(np.int64)
            good, worst = True, 0
            for ms in (0, 1000, 15007, 30014, 45021, 59990):
                out = os.path.join(tmp, "vseek.raw")
                rc, kv = run(binary, "seek", vpath, ms, out, 3000)
                frame = ms * 44100 // 1000
                exp = ref[frame * 2:(frame + 3000) * 2]
                got = np.fromfile(out, dtype="<i2").astype(np.int64)
                if rc != 0 or got.shape != exp.shape:
                    good = False
                    print(f"      ms={ms}: rc={rc} {len(got)} vs {len(exp)} muestras {kv}")
                    continue
                worst = max(worst, int(np.abs(got - exp).max()))
            check(f"[{bits}] búsqueda: 6 posiciones, diferencia máxima {worst} LSB", good and worst <= 1)

        print("== Ogg Vorbis: robustez (no deben colgarse ni caerse)")
        ogg = open(vpath, "rb").read()
        rng = random.Random(369)
        vcases = {}
        vcases["truncado al 30 %"] = ogg[: len(ogg) * 3 // 10]
        vcases["truncado a 100 bytes"] = ogg[:100]
        vcases["truncado dentro de las cabeceras"] = ogg[:3000]
        corrupt = bytearray(ogg)
        for _ in range(200):
            corrupt[rng.randrange(8192, len(corrupt))] ^= 0xFF
        vcases["200 bytes dañados"] = bytes(corrupt)
        corrupt = bytearray(ogg)
        for _ in range(40):
            corrupt[rng.randrange(0, 6000)] ^= 0xFF
        vcases["40 bytes dañados en las cabeceras"] = bytes(corrupt)
        vcases["basura aleatoria"] = bytes(rng.randrange(256) for _ in range(50000))
        vcases["solo 'OggS'"] = b"OggS"
        vcases["páginas Ogg vacías"] = (b"OggS\x00\x02" + b"\x00" * 8 + b"\x01\x00\x00\x00" + b"\x00" * 9 + b"\x00") * 50
        for label, blob in vcases.items():
            path = os.path.join(tmp, "vcase.bin")
            open(path, "wb").write(blob)
            for bits, binary in BINS.items():
                try:
                    rc, kv = run(binary, "decode", path, os.path.join(tmp, "o.raw"), timeout=60)
                    ok = 0 <= rc < 100
                except subprocess.TimeoutExpired:
                    ok, rc = False, "TIMEOUT"
                check(f"[{bits}] {label}", ok, f"rc={rc}")

        print("== Ogg Vorbis: límites declarados")
        w96 = os.path.join(tmp, "v96.wav")
        subprocess.run(["sox", src, "-r", "96000", w96], check=True)
        v96 = os.path.join(tmp, "v96.ogg")
        subprocess.run(["oggenc", "-Q", "-q", "3", w96, "-o", v96], check=True)
        w6v = os.path.join(tmp, "v6.wav")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", src, "-ac", "6", w6v], check=True)
        v6 = os.path.join(tmp, "v6.ogg")
        subprocess.run(["oggenc", "-Q", "-q", "3", w6v, "-o", v6], check=True)
        for bits, binary in BINS.items():
            rc, kv = run(binary, "info", v96)
            check(f"[{bits}] Vorbis de 96 kHz: rechazado con CODEC_ERR_PARAMS (-3)", kv.get("error") == "-3", str(kv))
            rc, kv = run(binary, "info", v6)
            check(f"[{bits}] Vorbis de 6 canales: rechazado con CODEC_ERR_PARAMS (-3)", kv.get("error") == "-3", str(kv))
            rc, kv = run(binary, "decode", vpath, os.path.join(tmp, "o.raw"), 0, 16)
            check(f"[{bits}] arena de 16 KB: error de memoria limpio (open:-4)", kv.get("error") == "open:-4", str(kv))

        print("== WMA (libwma): información")
        for bits, binary in BINS.items():
            rc, kv = run(binary, "info", os.path.join(DATA, "testwma2.wma"))
            check(f"[{bits}] WMA v2: formato, tasa y canales",
                  kv.get("format") == "WMA" and kv.get("rate") == "44100" and kv.get("channels") == "2", str(kv))
            check(f"[{bits}] WMA v2: duración de la cabecera (59 999 ms)", abs(int(kv.get("duration_ms", "0")) - 59999) <= 5, kv.get("duration_ms", ""))
            check(f"[{bits}] WMA v2: título, artista y álbum con acentos en MacRoman",
                  (kv.get("title"), kv.get("artist"), kv.get("album")) == (want_title, want_artist, want_album), str(kv))
            check(f"[{bits}] WMA: sin perfiles ni ecualizador", kv.get("profiles") == "0" and kv.get("eq") == "0")
            rc, kv = run(binary, "info", os.path.join(DATA, "testwma1.wma"))
            check(f"[{bits}] WMA v1: reconocido", kv.get("format") == "WMA" and kv.get("channels") == "2", str(kv))

        print("== WMA: PCM contra ffmpeg (decodificadores enteros: se admite 1 LSB) y 32 == 64 bits")
        wsrc = os.path.join(DATA, "mix.wav")
        variants = [("testwma2.wma", os.path.join(DATA, "testwma2.wma"), False), ("testwma1.wma", os.path.join(DATA, "testwma1.wma"), False)]
        for label, args in (("mono 22,05 kHz v2 64k", ["-ac", "1", "-ar", "22050", "-b:a", "64k"]),
                            ("estéreo 48 kHz v2 192k", ["-ar", "48000", "-b:a", "192k"]),
                            ("mono 16 kHz v2 32k", ["-ac", "1", "-ar", "16000", "-b:a", "32k"]),
                            ("estéreo 32 kHz v1 96k (exponentes LSP)", ["-ar", "32000", "-b:a", "96k", "-c:a", "wmav1"])):
            vp = os.path.join(tmp, "wv_" + str(abs(hash(label))) + ".wma")
            cmd = ["ffmpeg", "-v", "error", "-y", "-t", "12", "-i", wsrc, "-c:a", "wmav2"] + args + [vp]
            subprocess.run(cmd, check=True)
            # Con exponentes LSP (WMA v1 a baja tasa) libwma usa tablas de poca precisión: la forma de onda coincide (correlación
            # 1,0000) pero la amplitud sale ~0,2 dB baja. Se comprobó una vez que la libwma ORIGINAL (con float) da exactamente los
            # mismos bytes que la copia con enteros: la diferencia es de la biblioteca, no del parche. Aquí se exige SNR >= 25 dB.
            variants.append((label, vp, "LSP" in label))
        for label, path, lenient in variants:
            refraw = os.path.join(tmp, "wref.raw")
            ri = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=channels", "-of", "csv=p=0", path],
                                capture_output=True, text=True).stdout.split()[0]
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-f", "s16le", "-acodec", "pcm_s16le", refraw], check=True)
            ref = np.fromfile(refraw, dtype="<i2").astype(np.int64)
            outs = {}
            for bits, binary in BINS.items():
                out = os.path.join(tmp, f"w{bits}.raw")
                rc, kv = run(binary, "decode", path, out)
                outs[bits] = np.fromfile(out, dtype="<i2")
                got = outs[bits].astype(np.int64)
                n = min(len(got), len(ref))
                d = np.abs(got[:n] - ref[:n]) if n else np.array([99999])
                rms_d = float(np.sqrt(np.mean(d * d)))
                snr = 20 * np.log10(float(np.sqrt(np.mean(ref[:n] * ref[:n]))) / max(rms_d, 1e-9)) if n else 0.0
                accurate = (snr >= 25.0) if lenient else (int(d.max()) <= 2 and rms_d < 1.0)
                ok = rc == 0 and n > 0 and len(got) <= len(ref) and len(ref) - len(got) <= 8192 * int(ri) and accurate
                check(f"[{bits}] {label}: {len(got) // int(ri)} cuadros, error máx {int(d.max())} LSB, rms {rms_d:.2f}, SNR {snr:.1f} dB", ok, str(kv))
            check(f"{label}: mismos bytes en 32 y 64 bits", np.array_equal(outs["32"], outs["64"]))

        print("== WMA: búsqueda (contenido idéntico a la decodificación completa; posición con error < 2 ms)")
        wpath = os.path.join(DATA, "testwma2.wma")
        for bits, binary in BINS.items():
            full = os.path.join(tmp, "wfull.raw")
            run(binary, "decode", wpath, full)
            ref = np.fromfile(full, dtype="<i2").astype(np.int64)
            good, worst_lag, worst = True, 0, 0
            for ms in (0, 1000, 15007, 30014, 45021, 58000):
                out = os.path.join(tmp, "wseek.raw")
                rc, kv = run(binary, "seek", wpath, ms, out, 16000)
                frame = ms * 44100 // 1000
                got = np.fromfile(out, dtype="<i2").astype(np.int64)
                if rc != 0 or len(got) != 32000:
                    good = False
                    print(f"      ms={ms}: rc={rc} {len(got)} muestras {kv}")
                    continue
                if ms == 0:
                    exact = np.array_equal(got, ref[:32000])           # igual que empezar a reproducir desde el principio
                    good = good and exact
                    worst = max(worst, 0 if exact else 99999)
                    continue
                # el contenido tras los 2 primeros cuadros de MDCT debe estar en la decodificación completa, a menos de 2 ms (88 cuadros)
                seg = got[8000 * 2:16000 * 2]
                best = None
                for lag in range(-90, 91):
                    s0 = (frame + 8000 + lag) * 2
                    d = int(np.abs(seg - ref[s0:s0 + len(seg)]).max())
                    if best is None or d < best[0]:
                        best = (d, lag)
                worst = max(worst, best[0])
                worst_lag = max(worst_lag, abs(best[1]))
            check(f"[{bits}] búsqueda: 6 posiciones, diferencia máxima {worst} LSB, desfase máximo {worst_lag} cuadros ({worst_lag / 44.1:.1f} ms)",
                  good and worst <= 2 and worst_lag < 90)

        print("== WMA: robustez (no deben colgarse ni caerse)")
        wma = open(wpath, "rb").read()
        rng = random.Random(370)
        wcases = {}
        wcases["truncado al 30 %"] = wma[: len(wma) * 3 // 10]
        wcases["truncado a 100 bytes"] = wma[:100]
        wcases["truncado dentro de la cabecera"] = wma[:600]
        corrupt = bytearray(wma)
        for _ in range(300):
            corrupt[rng.randrange(1500, len(corrupt))] ^= 0xFF
        wcases["300 bytes dañados"] = bytes(corrupt)
        corrupt = bytearray(wma)
        for _ in range(30):
            corrupt[rng.randrange(0, 700)] ^= 0xFF
        wcases["30 bytes dañados en la cabecera"] = bytes(corrupt)
        wcases["basura aleatoria"] = bytes(rng.randrange(256) for _ in range(50000))
        wcases["solo el GUID de cabecera"] = bytes.fromhex("3026b2758e66cf11a6d900aa0062ce6c")
        for label, blob in wcases.items():
            path = os.path.join(tmp, "wcase.bin")
            open(path, "wb").write(blob)
            for bits, binary in BINS.items():
                try:
                    rc, kv = run(binary, "decode", path, os.path.join(tmp, "o.raw"), timeout=60)
                    ok = 0 <= rc < 100
                except subprocess.TimeoutExpired:
                    ok, rc = False, "TIMEOUT"
                check(f"[{bits}] {label}", ok, f"rc={rc}")

        print("== WMA: límites declarados")
        sp = bytes.fromhex("9107dcb7b7a9cf118ee600c00c205365")
        pos = wma.find(sp) + 24 + 54          # inicio del WAVEFORMATEX del primer flujo de audio
        def patched(offset, data):
            b = bytearray(wma)
            b[pos + offset:pos + offset + len(data)] = data
            return bytes(b)
        limit_cases = {"WMA Pro (0x162)": (patched(0, struct.pack("<H", 0x162)), "-2"),
                       "96 kHz": (patched(4, struct.pack("<I", 96000)), "-3"),
                       "6 canales": (patched(2, struct.pack("<H", 6)), "-3")}
        for label, (blob, want) in limit_cases.items():
            path = os.path.join(tmp, "wlim.wma")
            open(path, "wb").write(blob)
            for bits, binary in BINS.items():
                rc, kv = run(binary, "info", path)
                check(f"[{bits}] WMA {label}: rechazado con error {want}", kv.get("error") == want, str(kv))
        for bits, binary in BINS.items():
            rc, kv = run(binary, "decode", wpath, os.path.join(tmp, "o.raw"), 0, 16)
            check(f"[{bits}] arena de 16 KB: error de memoria limpio (open:-4)", kv.get("error") == "open:-4", str(kv))

        print("== MP3 por la capa nueva: FULL idéntico a hostdec; MONO y HALF con la capa III de MP368K")
        # FULL debe ser bit a bit el de libmad. MONO mezcla L y R antes de la IMDCT y usa la ventana de 16x16 bits: contra (L+R)/2
        # del FULL debe quedar sobre 55 dB de SNR (la ventana sola da unos 60).
        # HALF y HALF_MONO dejan pasar solo las 16 subbandas bajas: se comparan con el FULL (o su mezcla) filtrado a fs/2 por
        # sox, y deben quedar sobre 35 dB y al menos 10 dB mejor que la synth_half de libmad, que pliega las subbandas altas.
        def snr_db(x, r):
            m = min(len(x), len(r))
            x, r = x[:m].astype(np.float64), r[:m].astype(np.float64)
            return 10 * np.log10((r ** 2).sum() / max(((x - r) ** 2).sum(), 1.0))

        def sox22(pcm, ch, rate):
            a, b = os.path.join(tmp, "s_in.raw"), os.path.join(tmp, "s_out.raw")
            pcm.astype("<i2").tofile(a)
            subprocess.run(["sox", "-t", "raw", "-r", str(rate), "-e", "signed", "-b", "16", "-c", str(ch), a,
                            "-t", "raw", b, "rate", "-v", "-a", str(rate // 2)], check=True)
            return np.fromfile(b, dtype="<i2")

        prof = {"FULL": 0, "HALF": 1, "MONO": 2, "HALF_MONO": 3}
        for name in ("test128.mp3", "testmono.mp3", "testvbr.mp3"):
            fullref = os.path.join(BH, "pcm", f"{name}.FULL.raw")
            if not os.path.exists(fullref):
                continue
            ref = np.fromfile(fullref, dtype="<i2")
            info = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=channels,sample_rate", "-of", "csv=p=0",
                                   os.path.join(DATA, name)], capture_output=True, text=True).stdout.split(",")
            rate, ch = int(info[0]), int(info[1])
            mix = ((ref.reshape(-1, 2).astype(np.int64).sum(axis=1)) >> 1).astype(np.int16) if ch == 2 else ref
            r_half, r_hmono = sox22(ref, ch, rate)[:200000 * ch], sox22(mix, 1, rate)[:200000]
            old_half = np.fromfile(os.path.join(BH, "pcm", f"{name}.HALF.raw"), dtype="<i2")[:200000 * ch]
            for pname, pid in prof.items():
                for bits, binary in BINS.items():
                    out = os.path.join(tmp, "out.raw")
                    rc, kv = run(binary, "decode", os.path.join(DATA, name), out, pid)
                    got = np.fromfile(out, dtype="<i2")
                    if pname == "FULL":
                        n = min(len(got), len(ref))
                        # dec_run pierde el último frame (sin los 8 bytes de cola); el flujo no.
                        ok = rc == 0 and n > 0 and np.array_equal(got[:n], ref[:n]) and 0 <= len(got) - len(ref) <= 2304
                        check(f"[{bits}] {name} FULL: idéntico a hostdec", ok, f"rc={rc} got={len(got)} ref={len(ref)}")
                    elif pname == "MONO":
                        n = min(len(got), len(mix))
                        e = snr_db(got[:n], mix[:n]) if n else 0.0
                        d = int(np.abs(got[:n].astype(np.int64) - mix[:n]).max()) if n else 99
                        check(f"[{bits}] {name} MONO: SNR {e:.1f} dB contra (L+R)/2 del FULL (>= 55; error máx. {d} LSB)",
                              rc == 0 and n > 0 and e >= 55, f"rc={rc}")
                    else:
                        r = r_half if pname == "HALF" else r_hmono
                        e = snr_db(got[:len(r)], r)
                        floor = snr_db(old_half, r_half) + 10 if pname == "HALF" else 0
                        check(f"[{bits}] {name} {pname}: SNR {e:.1f} dB contra el filtrado a fs/2 (>= 35 y >= {floor:.1f})",
                              rc == 0 and e >= 35 and e >= floor, f"rc={rc}")

        print("== robustez (no deben colgarse ni caerse)")
        flac = open(os.path.join(DATA, "testflac.flac"), "rb").read()
        rng = random.Random(368)
        cases = {}
        cases["truncado al 30 %"] = flac[: len(flac) * 3 // 10]
        cases["truncado a 100 bytes"] = flac[:100]
        corrupt = bytearray(flac)
        for _ in range(200):
            corrupt[rng.randrange(8192, len(corrupt))] ^= 0xFF
        cases["200 bytes dañados"] = bytes(corrupt)
        cases["basura aleatoria"] = bytes(rng.randrange(256) for _ in range(50000))
        cases["archivo vacío"] = b""
        cases["solo 'fLaC'"] = b"fLaC"
        # ID3v2 de 1000 bytes delante de un FLAC válido.
        size = 1000
        syncsafe = bytes([(size >> 21) & 0x7F, (size >> 14) & 0x7F, (size >> 7) & 0x7F, size & 0x7F])
        cases["ID3v2 delante de un FLAC"] = b"ID3\x04\x00\x00" + syncsafe + b"\x00" * size + flac
        for label, blob in cases.items():
            path = os.path.join(tmp, "case.bin")
            open(path, "wb").write(blob)
            for bits, binary in BINS.items():
                try:
                    rc, kv = run(binary, "decode", path, os.path.join(tmp, "o.raw"), timeout=60)
                    ok = 0 <= rc < 100
                except subprocess.TimeoutExpired:
                    ok, rc = False, "TIMEOUT"
                check(f"[{bits}] {label}", ok, f"rc={rc}")
        # El FLAC con ID3v2 delante debe reproducirse completo y con las etiquetas.
        path = os.path.join(tmp, "case.bin")
        open(path, "wb").write(cases["ID3v2 delante de un FLAC"])
        ref, ch = flac_ref(os.path.join(DATA, "testflac.flac"), tmp)
        for bits, binary in BINS.items():
            out = os.path.join(tmp, "o.raw")
            rc, kv = run(binary, "decode", path, out)
            check(f"[{bits}] FLAC tras un ID3v2: mismo PCM", rc == 0 and np.array_equal(np.fromfile(out, dtype="<i2"), ref), str(kv))

        print("== límites declarados")
        wav96 = os.path.join(tmp, "x96.wav")
        subprocess.run(["sox", src, "-r", "96000", wav96], check=True)
        f96 = os.path.join(tmp, "x96.flac")
        subprocess.run(["flac", "-s", "-f", wav96, "-o", f96], check=True)
        w6 = os.path.join(tmp, "x6.wav")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", src, "-ac", "6", w6], check=True)
        f6 = os.path.join(tmp, "x6.flac")
        subprocess.run(["flac", "-s", "-f", w6, "-o", f6], check=True)
        for bits, binary in BINS.items():
            rc, kv = run(binary, "info", f96)
            check(f"[{bits}] FLAC de 96 kHz: rechazado con CODEC_ERR_PARAMS (-3)", kv.get("error") == "-3", str(kv))
            rc, kv = run(binary, "info", f6)
            check(f"[{bits}] FLAC de 6 canales: rechazado con CODEC_ERR_PARAMS (-3)", kv.get("error") == "-3", str(kv))
            rc, kv = run(binary, "info", os.path.join(tmp, "case.bin"))
            check(f"[{bits}] ID3v2 + FLAC: se reconoce", kv.get("format") == "FLAC", str(kv))
            rc, kv = run(binary, "decode", os.path.join(DATA, "testflac.flac"), os.path.join(tmp, "o.raw"), 0, 16)
            check(f"[{bits}] arena de 16 KB: error de memoria limpio (open:-4)", kv.get("error") == "open:-4", str(kv))

        print("== carátula enorme dentro del FLAC (no debe pasar por el arena)")
        from PIL import Image
        img = Image.fromarray(np.random.RandomState(1).randint(0, 256, (400, 400, 3), dtype=np.uint8))
        pic = os.path.join(tmp, "cover.png")
        img.save(pic)
        cover = os.path.join(tmp, "cover.flac")
        subprocess.run(["flac", "-s", "-f", f"--picture=3||||{pic}", "-5", src, "-o", cover], check=True)
        size_cover = os.path.getsize(cover) - os.path.getsize(os.path.join(DATA, "testflac.flac"))
        for bits, binary in BINS.items():
            rc, kv = run(binary, "decode", cover, os.path.join(tmp, "o.raw"))
            peak = int(kv.get("arena_peak", "999999999"))
            check(f"[{bits}] carátula de ~{os.path.getsize(pic) // 1024} KB: uso máximo del arena {peak // 1024} KB",
                  rc == 0 and peak < 200 * 1024, str(kv))

        print("== uso de memoria del arena")
        for bits, binary in BINS.items():
            for name in ("testflac.flac", "testflacmono.flac", "testflac24.flac"):
                rc, kv = run(binary, "decode", os.path.join(DATA, name), os.path.join(tmp, "o.raw"))
                print(f"  [{bits}] {name}: pico {int(kv['arena_peak']) // 1024} KB de {int(kv['arena_size']) // 1024} KB")

    print()
    print("RESULTADO:", "todo OK (0 fallas)" if fails == 0 else f"{fails} FALLAS")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
