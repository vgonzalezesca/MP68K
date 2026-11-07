#!/usr/bin/env bash
# Prepara una COPIA de Tremor (rama lowmem) con los cambios de MP368K: tipos de stdint, multiplicaciones de 64 bits
# portables y asignaciones al heap del códec (ADR-009). El árbol third_party/tremor/ no se modifica.
#
# Uso: tools/prepare-tremor.sh <directorio de salida>
set -euo pipefail

OUT=${1:?directorio de salida}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/third_party/tremor

rm -rf "$OUT"
mkdir -p "$OUT"
cp "$SRC"/*.c "$SRC"/*.h "$OUT"/
cp "$ROOT/third_party/patches/mp368k_tremor.h" "$OUT"/
patch -s -p1 -d "$OUT" < "$ROOT/third_party/patches/tremor-lowmem-mp368k.patch"
