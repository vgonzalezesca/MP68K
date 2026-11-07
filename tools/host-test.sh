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
exit $rc
