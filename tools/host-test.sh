#!/usr/bin/env bash
# Pruebas del PC para los módulos portables: anillo, conversión a 8 bits y
# decodificación en flujo (contra el CRC de archivo entero de dec_run).
# Requiere haber corrido tools/make_testfiles.sh y tools/host-ref.sh.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build-host
MAD=$ROOT/third_party/libmad
[[ -f $B/own.csv ]] || { echo "ejecute primero tools/host-ref.sh" >&2; exit 1; }

LIB=(bit fixed frame huffman layer12 layer3 stream synth timer version)
srcs=(); for f in "${LIB[@]}"; do srcs+=("$MAD/$f.c"); done

gcc -O2 -Wall -w -DNDEBUG -DHAVE_LIMITS_H -DFPM_64BIT -DSIZEOF_INT=4 -DSIZEOF_LONG=8 -DSIZEOF_LONG_LONG=8 \
    -I"$B/libmad" -I"$MAD" -I"$ROOT/src" "${srcs[@]}" \
    "$ROOT/src/decoder_core.c" "$ROOT/src/ring.c" "$ROOT/src/pcmconv.c" "$ROOT/src/pcm_ref.c" \
    "$ROOT/tests/host/test_units.c" -o "$B/test_units"

rc=0
for f in test128 test192 testmono; do
    echo "== $f.mp3"
    "$B/test_units" "$ROOT/tests/data/$f.mp3" "$B/own.csv" || rc=1
done

# --- etapa 6: asignador de uso general (heap) para Tremor y libwma, en 64 y 32 bits ---
for bits in 64 32; do
    gcc -m$bits -O2 -Wall -Wextra -I"$ROOT/src" "$ROOT/src/heap.c" "$ROOT/tests/host/test_heap.c" -o "$B/test_heap$bits"
    echo "== heap ($bits bits)"
    "$B/test_heap$bits" || rc=1
done

# --- etapa 5b-2: id3, order y mp3info ---
gcc -O2 -Wall -Wextra -Wno-unused -I"$ROOT/src" "$ROOT/src/id3.c" "$ROOT/src/order.c" \
    "$ROOT/tests/host/test_modules.c" -o "$B/test_modules"
echo "== id3 + order"
"$B/test_modules" || rc=1

gcc -O2 -Wall -w -DNDEBUG -DHAVE_LIMITS_H -DFPM_64BIT -DSIZEOF_INT=4 -DSIZEOF_LONG=8 -DSIZEOF_LONG_LONG=8 \
    -I"$B/libmad" -I"$MAD" -I"$ROOT/src" "${srcs[@]}" "$ROOT/src/decoder_core.c" "$ROOT/src/id3.c" \
    "$ROOT/src/mp3info.c" "$ROOT/tests/host/test_files.c" -o "$B/test_files"

dur_ms() { ffprobe -v error -show_entries format=duration -of csv=p=0 "$1" | awk '{printf "%d", $1 * 1000 + 0.5}'; }
# archivo : cabecera esperada : etiquetas
for spec in "test128:none:" "testmono:none:" "testinfo:info:" "testvbr:toc:" "testtag23:info:tags" "testtag24:info:tags"; do
    IFS=: read -r f hdr tags <<<"$spec"
    [[ -f $ROOT/tests/data/$f.mp3 ]] || { echo "falta tests/data/$f.mp3 (tools/make_testfiles.sh)"; rc=1; continue; }
    echo "== $f.mp3"
    "$B/test_files" "$ROOT/tests/data/$f.mp3" "$(dur_ms "$ROOT/tests/data/$f.mp3")" "$hdr" $tags || rc=1
done

# --- etapa 6: capa de codecs (FLAC y MP3), en 64 y 32 bits ---
echo "== capa de codecs (FLAC, MP3) en 64 y 32 bits"
"$ROOT/tools/host-build-codecs.sh" && python3 -I "$ROOT/tests/host/check_codecs.py" "$ROOT" || rc=1
echo "== ecualizador (unitaria y sobre audio real) en 64 y 32 bits"
for b in 64 32; do "$B/test_eq$b" | grep -E 'FALLA|RESULTADO' || rc=1; done
python3 -I "$ROOT/tests/host/check_eq.py" "$ROOT" || rc=1

# --- etapa 6: archivos que reproduce QuickTime (WAV, AIFF, AIFC, AU): reconocimiento y duración por cabecera ---
echo "== qtfmt (WAV, AIFF, AIFC, AU) contra ffprobe, en 64 y 32 bits"
QT=$B/qt; rm -rf "$QT"; mkdir -p "$QT"
for bits in 64 32; do
    gcc -m$bits -O2 -Wall -Wextra -I"$ROOT/src" "$ROOT/src/qtfmt.c" "$ROOT/tests/host/test_qtfmt.c" -o "$B/test_qtfmt$bits" || rc=1
done
sox -R -n -r 44100 -c 2 -b 16 "$QT/s16.wav" synth 3.5 sine 440
sox -R -n -r 22050 -c 1 -b 8  "$QT/m8.wav"  synth 2.25 sine 330
sox -R -n -r 48000 -c 2 -b 24 "$QT/s24.wav" synth 1.7 sine 550
sox -R -n -r 44100 -c 2 -b 16 "$QT/s16.aif" synth 3.5 sine 440
sox -R -n -r 11025 -c 1 -b 8  "$QT/m8.aif"  synth 2.5 sine 330
sox -R -n -r 48000 -c 2 -b 24 "$QT/s24.aif" synth 1.7 sine 550
sox -R -n -r 44100 -c 2 -b 16 "$QT/s16.au"  synth 3.5 sine 440
sox -R -n -r 8000  -c 1 -e mu-law "$QT/ulaw.au" synth 2.5 sine 330
ffmpeg -v error -y -i "$QT/s16.wav" -c:a pcm_s16le -f aiff "$QT/s16le.aifc"
ffmpeg -v error -y -i "$QT/s16.wav" -c:a pcm_mulaw -ar 8000 -ac 1 "$QT/ulaw.wav"
head -c 100000 "$QT/s16.wav" > "$QT/cut.wav"
head -c 100000 "$QT/s16.aif" > "$QT/cut.aif"
head -c 5000 /dev/urandom > "$QT/basura.bin"
printf 'RIFF\x24\x00\x00\x00WAVE' > "$QT/vacio.wav"
for f in s16.wav m8.wav s24.wav s16.aif m8.aif s24.aif s16.au ulaw.au s16le.aifc ulaw.wav cut.wav cut.aif; do
    ms=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$QT/$f" | awk '{printf "%d", $1 * 1000 + 0.5}')
    hz=$(ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate -of csv=p=0 "$QT/$f")
    ch=$(ffprobe -v error -select_streams a:0 -show_entries stream=channels -of csv=p=0 "$QT/$f")
    case $f in
        ulaw.*) pcm=0 ;;
        *.aifc) pcm=1 ;;
        *) pcm=1 ;;
    esac
    case $f in
        *.wav) fmt=WAV ;;
        *.aif) fmt=AIFF ;;
        *.aifc) fmt=AIFC ;;
        *.au) fmt=AU ;;
    esac
    for bits in 64 32; do "$B/test_qtfmt$bits" ok "$QT/$f" $fmt "$hz" "$ch" "$ms" $pcm || rc=1; done
done
for bits in 64 32; do
    "$B/test_qtfmt$bits" err "$QT/basura.bin" UNSUPPORTED || rc=1
    "$B/test_qtfmt$bits" err "$QT/vacio.wav" UNSUPPORTED || rc=1
    "$B/test_qtfmt$bits" names || rc=1
done
exit $rc
