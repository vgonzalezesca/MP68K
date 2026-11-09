#!/usr/bin/env bash
# Arma dist/visita1/: todo lo que hay que llevar al LC 580 en una sola visita.
#
#   sysprobe/  microbench/  decbench/  decprofile/  audiotest/  mp368k/  segtest/   aplicaciones (.bin y .dsk)
#   mp3/                                test128.mp3 test192.mp3 testmono.mp3 + REFERENCE.txt
#   audio/                              MP3, FLAC, Ogg Vorbis y WMA para codecbench + REFERENCE_CODECS.txt
#   qt/                                 qttest.bin + clips de 6 s (WAV, AIFF, AIFC, AU, CAF, M4A, WAV mu-law) para probar QuickTime
#
# decbench, decprofile, audiotest y codecbench buscan sus archivos y su REFERENCE*.txt en la MISMA carpeta que la aplicación: por eso
# se dejan copias de esas apps (solo el .bin) dentro de mp3/ y de audio/. Correr la app desde allí, no desde su propia carpeta.
# Requiere haber ejecutado tools/build.sh, tools/make_testfiles.sh y tools/host-ref.sh.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/dist/visita1
rm -rf "$OUT"
mkdir -p "$OUT/mp3"

for app in sysprobe microbench decbench decprofile audiotest codecbench codecbench_ref mp368k segtest qttest; do
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

# qttest busca los clips junto a la aplicación. Clips de 6 s hechos de mix.wav con sox/ffmpeg (ninguno es MP3, FLAC, Ogg ni WMA).
# Si falta una herramienta, ese clip se omite y qttest lo informa como "no está junto a la app".
mkdir -p "$OUT/qt"
cp "$ROOT/dist/68k/qttest/qttest.bin" "$OUT/qt/"
if [[ -f $ROOT/tests/data/mix.wav ]]; then
    clip() { "$@" || echo "aviso: no se pudo generar un clip de qt/ ($*)" >&2; }
    clip sox -R "$ROOT/tests/data/mix.wav" "$OUT/qt/t.wav" trim 0 6
    clip ffmpeg -v error -y -i "$OUT/qt/t.wav" -c:a pcm_s16be -f aiff "$OUT/qt/t.aif"
    clip ffmpeg -v error -y -i "$OUT/qt/t.wav" -c:a pcm_s16le -f aiff "$OUT/qt/t.aifc"
    clip sox -R "$OUT/qt/t.wav" "$OUT/qt/t.au"
    clip ffmpeg -v error -y -i "$OUT/qt/t.wav" -c:a pcm_s16le -f caf "$OUT/qt/t.caf"
    clip ffmpeg -v error -y -i "$OUT/qt/t.wav" -c:a aac -b:a 128k "$OUT/qt/t.m4a"
    clip ffmpeg -v error -y -i "$OUT/qt/t.wav" -c:a pcm_mulaw -ar 8000 -ac 1 "$OUT/qt/t.ulaw.wav"
else
    echo "aviso: falta tests/data/mix.wav; qt/ queda sin clips (ejecute tools/make_testfiles.sh)" >&2
fi

(cd "$OUT" && find . -type f | sort | xargs ls -l | awk '{print $5, $9}')
echo
echo "CORRER desde mp3/ (decbench, decprofile, audiotest) y desde audio/ (codecbench, codecbench_ref): ahí están sus archivos y su REFERENCE*.txt."
echo "qttest: correr desde qt/ (ahí están sus clips); devolver qttest.txt. Anotar a mano cuáles SONARON: el programa no puede oírlo."
echo "CRC-32 esperados de los MP3 (decbench los imprime para comprobar la transferencia):"
grep -h '^bench,[^,]*,[^,]*,FULL,' "$ROOT/tests/data/REFERENCE.txt" | cut -d, -f2,3
