#!/usr/bin/env bash
# Compila MP368K con Retro68, verifica que no haya instrucciones de FPU y
# copia los artefactos a dist/<app>/.
#
#   RETRO68_BUILD  directorio de compilación de Retro68 (por defecto ~/Retro68-build)
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RETRO68_BUILD=${RETRO68_BUILD:-$HOME/Retro68-build}
TOOLCHAIN_FILE=$RETRO68_BUILD/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
APPS=(sysprobe microbench decbench decprofile audiotest mp368k)

if [[ ! -f $TOOLCHAIN_FILE ]]; then
    echo "No encuentro $TOOLCHAIN_FILE" >&2
    echo "Compile primero el toolchain: tools/build-toolchain.sh" >&2
    exit 1
fi
export PATH=$RETRO68_BUILD/toolchain/bin:$PATH

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
cmake --build "$ROOT/build"

for app in "${APPS[@]}"; do
    "$ROOT/tools/check-nofpu.sh" "$ROOT/build" "$app"

    out=$ROOT/dist/$app
    rm -rf "$out"
    mkdir -p "$out"
    cd "$ROOT/build"
    # .bin = MacBinary (LC 580), .dsk = imagen HFS, .APPL + .rsrc/.finf = carpeta
    # compartida de Basilisk II, %app.ad = AppleDouble.
    for f in "$app.bin" "$app.dsk" "$app.APPL" "%$app.ad"; do
        [[ -e $f ]] && cp -a "$f" "$out/"
    done
    # .rsrc/ y .finf/ de la compilación contienen los archivos de TODAS las apps: copiar
    # solo los de esta.
    for d in .rsrc .finf; do
        if [[ -e $d/$app.APPL ]]; then
            mkdir -p "$out/$d"
            cp -a "$d/$app.APPL" "$out/$d/"
        fi
    done
    cd "$ROOT"
    echo "== $app -> dist/$app/"
    ls -la "$out"
done
