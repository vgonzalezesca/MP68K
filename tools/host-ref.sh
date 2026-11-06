#!/usr/bin/env bash
# Oráculo del PC para la etapa 2.
#
# Compila src/decoder_core.c (el mismo archivo que usa la app Mac) de dos formas:
#   hostdec      con el libmad de third_party/ (misma configuración que el 68k)
#   hostdec_sys  con el libmad instalado en el sistema (solo control de cordura)
# Ejecuta ambos sobre tests/data/*.mp3, compara y escribe tests/data/REFERENCE.txt
# (CRC del PCM por archivo y perfil), que es lo que se compara con decbench.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
B=$ROOT/build-host
MAD=$ROOT/third_party/libmad
mkdir -p "$B/libmad"

SIZEOF_LONG=8 "$ROOT/tools/gen-mad-h.sh" "$B/libmad/mad.h" FPM_64BIT

LIB_SRC=(bit fixed frame huffman layer12 layer3 stream synth timer version)
srcs=()
for f in "${LIB_SRC[@]}"; do srcs+=("$MAD/$f.c"); done

# -O2 sin -ffast-math: el resultado debe ser idéntico bit a bit al del 68k.
common=(-O2 -w -DNDEBUG -DHAVE_LIMITS_H -DFPM_64BIT -DSIZEOF_INT=4 -DSIZEOF_LONG=8 -DSIZEOF_LONG_LONG=8)

gcc "${common[@]}" -I"$B/libmad" -I"$MAD" -I"$ROOT/src" \
    "${srcs[@]}" "$ROOT/src/decoder_core.c" "$ROOT/tests/host/hostdec.c" -o "$B/hostdec"

gcc -O2 -w -I"$ROOT/src" \
    "$ROOT/src/decoder_core.c" "$ROOT/tests/host/hostdec.c" -lmad -o "$B/hostdec_sys"

# Biblioteca parcheada (etapa 3): copia con marcas de perfilado, sin tocar el original.
tools_prof="$B/libmad_prof"
"$ROOT/tools/prepare-libmad-prof.sh" "$tools_prof"
psrcs=()
for f in "${LIB_SRC[@]}"; do psrcs+=("$tools_prof/$f.c"); done
gcc "${common[@]}" -DMP368K_PROFILE -I"$B/libmad" -I"$tools_prof" -I"$ROOT/src" \
    "${psrcs[@]}" "$tools_prof/mp368k_prof.c" "$ROOT/src/decoder_core.c" \
    "$ROOT/tests/host/hostprof.c" -o "$B/hostprof"

shopt -s nullglob
files=("$ROOT"/tests/data/*.mp3)
if [[ ${#files[@]} -eq 0 ]]; then
    echo "No hay MP3 en tests/data/. Ejecute tools/make_testfiles.sh" >&2
    exit 1
fi

mkdir -p "$B/pcm"
"$B/hostdec" -d "$B/pcm" "${files[@]}" > "$B/own.csv"
"$B/hostdec_sys" "${files[@]}" > "$B/sys.csv"

"$B/hostdec" -f 700 "${files[@]}" | grep '^bench,' | sed 's/^bench,/prof,/' > "$B/own_prof.csv"
"$B/hostprof" -f 700 "${files[@]}" > "$B/prof_out.txt"
echo "== biblioteca con marcas vs sin marcas (700 frames): el PCM debe ser identico"
if diff <(grep '^prof,' "$B/prof_out.txt") "$B/own_prof.csv"; then echo "IDENTICOS"; else echo "LAS MARCAS ALTERAN EL AUDIO"; exit 1; fi

echo "== libmad propio vs libmad del sistema (sin la línea de versión)"
if diff <(grep -v '^#' "$B/own.csv") <(grep -v '^#' "$B/sys.csv"); then
    echo "IDENTICOS"
else
    echo "DIFIEREN (esperable solo si el libmad del sistema usa otras opciones de compilación)"
fi

{
    echo "# Referencia del PC para decbench. Generado por tools/host-ref.sh"
    echo "# gcc: $(gcc --version | head -1)"
    echo "# sox: $(sox --version 2>&1 | head -1)   lame: $(lame --version | head -1)"
    echo "# sha256 de los MP3:"
    (cd "$ROOT/tests/data" && sha256sum ./*.mp3 | sed 's/^/#   /')
    echo "# kind,file,file_crc,profile,frames,errors,samples_per_ch,channels,out_rate,pcm_crc"
    cat "$B/own.csv"
    echo "# kind=prof: primeros 700 frames (decprofile)"
    cat "$B/own_prof.csv"
} > "$ROOT/tests/data/REFERENCE.txt"

echo "== escrito tests/data/REFERENCE.txt"

echo "== comprobaciones de audio (MONO, HALF, FULL vs ffmpeg)"
python3 -I "$ROOT/tests/host/check_pcm.py" "$B/pcm" "$ROOT/tests/data"
