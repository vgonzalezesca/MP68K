#!/usr/bin/env bash
# Compila la rama actual (68k o ppc), corre las comprobaciones de ese equipo y copia los
# artefactos a dist/<equipo>/<app>/. La rama general no compila apps Mac: ver tools/build-all.sh.
#
#   RETRO68_BUILD  directorio de compilación de Retro68 (por defecto ~/Retro68-build)
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RETRO68_BUILD=${RETRO68_BUILD:-$HOME/Retro68-build}

if [[ ! -f $ROOT/targets/target.sh ]]; then
    echo "Esta es la rama general: no compila aplicaciones Mac (solo tools/host-*.sh)." >&2
    echo "Cambie a la rama 68k o ppc, o use tools/build-all.sh para las tres compilaciones." >&2
    exit 1
fi
# Define TARGET, TRIPLE, APPS y target_check_app().
source "$ROOT/targets/target.sh"

TOOLCHAIN_FILE=$RETRO68_BUILD/toolchain/$TRIPLE/cmake/$TOOLCHAIN_NAME
if [[ ! -f $TOOLCHAIN_FILE ]]; then
    echo "No encuentro $TOOLCHAIN_FILE" >&2
    echo "Compile primero el toolchain: tools/build-toolchain.sh" >&2
    exit 1
fi
export PATH=$RETRO68_BUILD/toolchain/bin:$PATH

BUILD=$ROOT/build/$TARGET
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
cmake --build "$BUILD" -j"$(nproc)"

for app in "${APPS[@]}"; do
    target_check_app "$BUILD" "$app"

    out=$ROOT/dist/$TARGET/$app
    rm -rf "$out"
    mkdir -p "$out"
    cd "$BUILD"
    # .bin = MacBinary (equipo real), .dsk = imagen HFS, .APPL + .rsrc/.finf = carpeta
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
    echo "== $app -> dist/$TARGET/$app/"
    ls -la "$out"
done
