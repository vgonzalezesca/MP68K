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
MAD=$ROOT/third_party/libmad
mkdir -p "$B/cod"

"$ROOT/tools/prepare-dr-flac.sh" "$B/drflac" >/dev/null

LIB=(bit fixed frame huffman layer12 layer3 stream synth timer version)
OURS=(src/decoder_core.c src/arena.c src/codec.c src/codec_mp3.c src/codec_flac.c src/vorbiscomment.c
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
        gcc "${common[@]}" -DFPM_64BIT -w -c "$MAD/$f.c" -o "$d/mad_$f.o"; objs+=("$d/mad_$f.o")
    done
    gcc "${common[@]}" -w -c "$ROOT/src/dr_flac_impl.c" -o "$d/dr_flac_impl.o"; objs+=("$d/dr_flac_impl.o")
    for f in "${OURS[@]}"; do
        local o=$d/$(basename "$f" .c).o
        gcc "${common[@]}" -Wall -Wextra -Wno-unused-parameter -Wno-multichar -c "$ROOT/$f" -o "$o"; objs+=("$o")
    done
    gcc $flag "${objs[@]}" -o "$B/test_codecs$bits"
}

build 64 8 -m64
build 32 4 -m32
echo "host-build-codecs: build-host/test_codecs64 y test_codecs32"
