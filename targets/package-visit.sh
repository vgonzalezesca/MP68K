#!/usr/bin/env bash
# Arma dist/visita1/: todo lo que hay que llevar al LC 580 en una sola visita.
#
#   sysprobe/  microbench/  decbench/  decprofile/  audiotest/  mp368k/  segtest/   aplicaciones (.bin y .dsk)
#   mp3/                                test128.mp3 test192.mp3 testmono.mp3 + REFERENCE.txt
#   audio/                              MP3, FLAC, Ogg Vorbis y WMA para codecbench + REFERENCE_CODECS.txt
#
# decbench, decprofile y audiotest buscan los MP3 y REFERENCE.txt en la MISMA carpeta que la aplicación.
# Requiere haber ejecutado tools/build.sh, tools/make_testfiles.sh y tools/host-ref.sh.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/dist/visita1
rm -rf "$OUT"
mkdir -p "$OUT/mp3"

for app in sysprobe microbench decbench decprofile audiotest codecbench codecbench_ref mp368k segtest; do
    for f in "$ROOT/dist/68k/$app/$app.bin" "$ROOT/dist/68k/$app/$app.dsk"; do
        [[ -f $f ]] || { echo "falta $f: ejecute tools/build.sh" >&2; exit 1; }
    done
    mkdir -p "$OUT/$app"
    cp "$ROOT/dist/68k/$app/$app.bin" "$ROOT/dist/68k/$app/$app.dsk" "$OUT/$app/"
done

for f in test128.mp3 test192.mp3 testmono.mp3 REFERENCE.txt; do
    [[ -f $ROOT/tests/data/$f ]] || { echo "falta tests/data/$f: ejecute tools/make_testfiles.sh y tools/host-ref.sh" >&2; exit 1; }
    cp "$ROOT/tests/data/$f" "$OUT/mp3/"
done

# codecbench busca estos archivos y REFERENCE_CODECS.txt en la MISMA carpeta que la aplicación.
mkdir -p "$OUT/audio"
for f in test128.mp3 test192.mp3 testmono.mp3 testflac.flac testflacmono.flac testflac24.flac testflac8.flac testvorbis.ogg testvorbismono.ogg testwma2.wma testwma1.wma REFERENCE_CODECS.txt; do
    [[ -f $ROOT/tests/data/$f ]] || { echo "falta tests/data/$f: ejecute tools/make_testfiles.sh y tools/host-codec-ref.sh" >&2; exit 1; }
    cp "$ROOT/tests/data/$f" "$OUT/audio/"
done

(cd "$OUT" && find . -type f | sort | xargs ls -l | awk '{print $5, $9}')
echo "CRC-32 esperados de los MP3 (decbench los imprime para comprobar la transferencia):"
grep -h '^bench,[^,]*,[^,]*,FULL,' "$ROOT/tests/data/REFERENCE.txt" | cut -d, -f2,3
