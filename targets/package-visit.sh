#!/usr/bin/env bash
# Arma dist/visita1/: todo lo que hay que llevar al LC 580 en una sola visita.
#
#   sysprobe/  microbench/  decbench/  decprofile/  audiotest/  mp368k/  segtest/   aplicaciones (.bin y .dsk)
#   mp3/                                test128.mp3 test192.mp3 testmono.mp3 + REFERENCE.txt
#   audio/                              MP3, FLAC, Ogg Vorbis y WMA para codecbench + REFERENCE_CODECS.txt
#
# decbench, decprofile, audiotest y codecbench buscan sus archivos y su REFERENCE*.txt en la MISMA carpeta que la aplicación: por eso
# se dejan copias de esas apps (solo el .bin) dentro de mp3/ y de audio/. Correr la app desde allí, no desde su propia carpeta.
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

# Copias de las apps junto a los archivos que leen (sin esto salía "no se pudo abrir (err=-43)" y "sin REFERENCE").
for app in decbench decprofile audiotest; do cp "$ROOT/dist/68k/$app/$app.bin" "$OUT/mp3/"; done
for app in codecbench codecbench_ref; do cp "$ROOT/dist/68k/$app/$app.bin" "$OUT/audio/"; done

(cd "$OUT" && find . -type f | sort | xargs ls -l | awk '{print $5, $9}')
echo
echo "CORRER desde mp3/ (decbench, decprofile, audiotest) y desde audio/ (codecbench, codecbench_ref): ahí están sus archivos y su REFERENCE*.txt."
echo "CRC-32 esperados de los MP3 (decbench los imprime para comprobar la transferencia):"
grep -h '^bench,[^,]*,[^,]*,FULL,' "$ROOT/tests/data/REFERENCE.txt" | cut -d, -f2,3
