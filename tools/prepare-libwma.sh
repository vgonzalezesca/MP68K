#!/usr/bin/env bash
# Prepara una COPIA de libwma (Rockbox) lista para compilar sin el entorno de códecs de Rockbox (ADR-009):
#  - cabeceras de reemplazo mínimas (third_party/patches/libwma_shim/): codecs.h, platform.h, codecs/lib/codeclib.h
#  - el único float del camino de decodificación (pow_m1_4) reemplazado por enteros (libwma-mp368k.patch)
# El árbol third_party/libwma/ no se modifica.
#
# Uso: tools/prepare-libwma.sh <directorio de salida>
set -euo pipefail

OUT=${1:?directorio de salida}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/third_party/libwma
SHIM=$ROOT/third_party/patches/libwma_shim

rm -rf "$OUT"
mkdir -p "$OUT/codecs/lib" "$OUT/codecs/libasf"
cp "$SRC"/*.c "$SRC"/*.h "$OUT"/
cp "$SRC/asf.h" "$OUT/codecs/libasf/asf.h"
cp "$SHIM/codecs.h" "$SHIM/platform.h" "$OUT"/
cp "$SHIM/codecs/lib/codeclib.h" "$OUT/codecs/lib/codeclib.h"
patch -s -p1 -d "$OUT" < "$ROOT/third_party/patches/libwma-mp368k.patch"
