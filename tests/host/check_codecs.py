#!/usr/bin/env python3
"""Pruebas de la capa de códecs en el PC (FLAC, y MP3 a través de la capa nueva).

Uso: check_codecs.py <raíz del proyecto>

Compara test_codecs (los mismos fuentes que la app Mac, en 64 y en 32 bits) contra referencias
independientes: `flac -d` para el PCM de FLAC, y los .raw de hostdec para el MP3. Comprueba además
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

        print("== MP3 por la capa nueva: mismo PCM que hostdec")
        prof = {"FULL": 0, "HALF": 1, "MONO": 2, "HALF_MONO": 3}
        for name in ("test128.mp3", "testmono.mp3", "testvbr.mp3"):
            for pname, pid in prof.items():
                refpath = os.path.join(BH, "pcm", f"{name}.{pname}.raw")
                if not os.path.exists(refpath):
                    continue
                ref = np.fromfile(refpath, dtype="<i2")
                for bits, binary in BINS.items():
                    out = os.path.join(tmp, "out.raw")
                    rc, kv = run(binary, "decode", os.path.join(DATA, name), out, pid)
                    got = np.fromfile(out, dtype="<i2")
                    n = min(len(got), len(ref))
                    # dec_run pierde el último frame (sin los 8 bytes de cola); el flujo no.
                    ok = rc == 0 and n > 0 and np.array_equal(got[:n], ref[:n]) and 0 <= len(got) - len(ref) <= 2304
                    check(f"[{bits}] {name} {pname}", ok, f"rc={rc} got={len(got)} ref={len(ref)}")

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
