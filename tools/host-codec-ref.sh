#!/usr/bin/env bash
# Referencia del PC para codecbench: tests/data/REFERENCE_CODECS.txt.
#
# Decodifica con el arnés de la capa de códecs (los mismos fuentes que la app Mac) los primeros
# DECODE_SECONDS segundos de cada archivo, igual que bench/codecbench.c, y escribe el CRC-32 del
# PCM (big-endian por muestra). Línea: codec,<archivo>,<cuadros>,<canales>,<tasa>,<crc>.
# Requiere haber corrido tools/make_testfiles.sh y tools/host-build-codecs.sh.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SECS=12   # debe coincidir con DECODE_SECONDS de bench/codecbench.c
BIN=$ROOT/build-host/test_codecs32     # el 68k es de 32 bits
[[ -x $BIN ]] || "$ROOT/tools/host-build-codecs.sh"

python3 -I - "$ROOT" "$BIN" "$SECS" <<'EOF'
import os, subprocess, sys, tempfile, zlib
import numpy as np

root, binary, secs = sys.argv[1], sys.argv[2], int(sys.argv[3])
EQ_BENCH = "12,-6,4,0,10,-12,8,0,-4,12,0"        # debe coincidir con kBenchEq de bench/codecbench.c
data = os.path.join(root, "tests", "data")
files = ["test128.mp3", "test192.mp3", "testmono.mp3",
         "testflac.flac", "testflacmono.flac", "testflac24.flac", "testflac8.flac",
         "testvorbis.ogg", "testvorbismono.ogg"]
out = [
    "# Referencia del PC para codecbench. Generado por tools/host-codec-ref.sh",
    f"# primeros {secs} s de cada archivo; pcm_crc = CRC-32 del PCM en big-endian por muestra",
    "# codec,file,frames,channels,rate,pcm_crc",
]
with tempfile.TemporaryDirectory() as tmp:
    for name in files:
        path = os.path.join(data, name)
        if not os.path.exists(path):
            print(f"falta {path}", file=sys.stderr)
            continue
        info = dict(l.split("=", 1) for l in subprocess.run([binary, "info", path], capture_output=True, text=True).stdout.splitlines() if "=" in l)
        rate, ch = int(info["rate"]), int(info["channels"])
        limit = rate * secs
        dur = int(info["duration_ms"])
        if dur and dur * rate // 1000 < limit:
            limit = dur * rate // 1000
        raw = os.path.join(tmp, "o.raw")
        for label, env in ((name, {}), (name + "+EQ", {"MP368K_EQ": EQ_BENCH})) if name == "test128.mp3" else ((name, {}),):
            e = dict(os.environ)
            e.pop("MP368K_EQ", None)
            e.update(env)
            subprocess.run([binary, "seek", path, "0", raw, str(limit)], check=True, capture_output=True, env=e)
            pcm = np.fromfile(raw, dtype="<i2")
            frames = len(pcm) // ch
            crc = zlib.crc32(pcm.astype(">i2").tobytes()) & 0xFFFFFFFF
            out.append(f"codec,{label},{frames},{ch},{rate},{crc:08X}")
            print(f"  {label}: {frames} cuadros, {ch} canal(es), {rate} Hz, crc {crc:08X}")
open(os.path.join(data, "REFERENCE_CODECS.txt"), "w").write("\n".join(out) + "\n")
print("escrito tests/data/REFERENCE_CODECS.txt")
EOF
