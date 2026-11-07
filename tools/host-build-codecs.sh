#!/usr/bin/env bash
# Compila en el PC el arnés de la capa de códecs (tests/host/test_codecs.c) con los MISMOS fuentes
# que la app Mac, en 64 bits y en 32 bits (-m32). Los 32 bits importan: el 68k es de 32 bits y
# dr_flac elige otra caché de bits y otras rutas aritméticas según el tamaño del puntero.
#
# Nuestros fuentes se compilan con -Wall -Wextra; el código de terceros (libmad, dr_flac) con -w.
# Salida: build-host/test_codecs64 y build-host/test_codecs32.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build-host
MAD=$B/libmad_eq               # copia parcheada con el gancho del ecualizador (ADR-011)
mkdir -p "$B/cod"

"$ROOT/tools/prepare-dr-flac.sh" "$B/drflac" >/dev/null
"$ROOT/tools/prepare-libmad-eq.sh" "$MAD"

LIB=(bit fixed frame huffman layer12 layer3 stream synth timer version)
# MP368K_SYNTH_KERNEL=off compila la síntesis original de libmad (para comparar contra el núcleo de referencia).
SYNTH_DEF=(-DMP368K_SYNTH_WINDOW=mp368k_synth_window_full_ref -DMP368K_DCT32=mp368k_dct32_ref -DMP368K_IMDCT_L=mp368k_imdct_l_ref)
SYNTH_SRC=(src/synth_ref.c src/dct32_ref.c src/imdct_ref.c)
if [[ ${MP368K_SYNTH_KERNEL:-on} == off ]]; then SYNTH_DEF=(); SYNTH_SRC=(); fi
OURS=(src/decoder_core.c src/eq.c "${SYNTH_SRC[@]}" src/arena.c src/codec.c src/codec_mp3.c src/codec_flac.c src/vorbiscomment.c
      src/id3.c src/mp3info.c tests/host/test_codecs.c)

build() {
    local bits=$1 sizeof_long=$2 flag=$3
    local d=$B/cod/$bits
    mkdir -p "$d/libmad"
    SIZEOF_LONG=$sizeof_long "$ROOT/tools/gen-mad-h.sh" "$d/libmad/mad.h" FPM_64BIT
    local common=(-O2 $flag -I"$d/libmad" -I"$MAD" -I"$B/drflac" -I"$ROOT/src"
                  -DNDEBUG -DHAVE_LIMITS_H -DSIZEOF_INT=4 -DSIZEOF_LONG="$sizeof_long" -DSIZEOF_LONG_LONG=8)
    local objs=()
    for f in "${LIB[@]}"; do
        gcc "${common[@]}" -DFPM_64BIT "${SYNTH_DEF[@]}" -w -c "$MAD/$f.c" -o "$d/mad_$f.o"; objs+=("$d/mad_$f.o")
    done
    gcc "${common[@]}" -w -c "$ROOT/src/dr_flac_impl.c" -o "$d/dr_flac_impl.o"; objs+=("$d/dr_flac_impl.o")
    for f in "${OURS[@]}"; do
        local o=$d/$(basename "$f" .c).o
        gcc "${common[@]}" -DMP368K_EQ_STATS -fwrapv -Wall -Wextra -Wno-unused-parameter -Wno-multichar -c "$ROOT/$f" -o "$o"; objs+=("$o")
    done
    gcc $flag "${objs[@]}" -o "$B/test_codecs$bits"
    # Prueba unitaria del ecualizador (no necesita libmad: define un gancho ficticio).
    gcc "${common[@]}" -DMP368K_EQ_STATS -Wall -Wextra -Wno-unused-parameter "$ROOT/src/eq.c" "$ROOT/tests/host/test_eq.c" \
        -lm -o "$B/test_eq$bits"
}

build 64 8 -m64
build 32 4 -m32
echo "host-build-codecs: build-host/test_codecs{64,32} y test_eq{64,32}"
