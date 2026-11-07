#!/usr/bin/env bash
# Prepara una COPIA de libmad con el gancho del ecualizador (ADR-011) y el punto de enganche del núcleo
# de la ventana de síntesis, el DCT32 y la IMDCT larga (núcleos de src/synth_kernels.h, ADR-014).
# El árbol third_party/libmad/ no se modifica (ADR-006: "sin modificaciones").
#
# Uso: tools/prepare-libmad-eq.sh <directorio de salida>
set -euo pipefail

OUT=${1:?directorio de salida}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/third_party/libmad

rm -rf "$OUT"
mkdir -p "$OUT"
cp "$SRC"/*.c "$SRC"/*.h "$SRC"/*.dat "$OUT"/
cp "$ROOT/third_party/patches/mp368k_eq.h" "$OUT"/
patch -s -p1 -d "$OUT" < "$ROOT/third_party/patches/libmad-0.15.1b-eq.patch"
patch -s -p1 -d "$OUT" < "$ROOT/third_party/patches/libmad-0.15.1b-synth.patch"
# va DESPUÉS del parche del ecualizador: se generó sobre el layer3.c ya parcheado
patch -s -p1 -d "$OUT" < "$ROOT/third_party/patches/libmad-0.15.1b-imdct.patch"
