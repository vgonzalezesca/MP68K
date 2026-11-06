#!/usr/bin/env bash
# Compila el toolchain Retro68 (68k y PowerPC clásico, sin Carbon) en CachyOS/Arch.
#
# Rutas (configurables):
#   RETRO68_SRC    fuente de Retro68         (por defecto ~/Retro68)
#   RETRO68_BUILD  directorio de compilación (por defecto ~/Retro68-build)
#
# Tarda bastante (binutils + gcc dos veces). El log queda en
# $RETRO68_BUILD/build-toolchain.log y el commit usado en RETRO68_COMMIT.
set -euo pipefail

RETRO68_SRC=${RETRO68_SRC:-$HOME/Retro68}
RETRO68_BUILD=${RETRO68_BUILD:-$HOME/Retro68-build}
RETRO68_REPO=https://github.com/autc04/Retro68.git

# Dependencias según el README de Retro68 (sección Arch Linux).
deps=(cmake gmp mpfr libmpc boost bison flex texinfo ruby)
if missing=$(pacman -T "${deps[@]}"); then
    :
else
    echo "Faltan paquetes del sistema. Instálelos con:" >&2
    echo "  sudo pacman -S --needed $(echo $missing)" >&2
    exit 1
fi

if [[ ! -d $RETRO68_SRC/.git ]]; then
    git clone --recursive "$RETRO68_REPO" "$RETRO68_SRC"
else
    git -C "$RETRO68_SRC" submodule update --init
fi

mkdir -p "$RETRO68_BUILD"
git -C "$RETRO68_SRC" log -1 --format='%H %cd' > "$RETRO68_BUILD/RETRO68_COMMIT"
echo "Retro68: $(cat "$RETRO68_BUILD/RETRO68_COMMIT")"

extra=()
# Si gcc y binutils de ambos equipos ya se compilaron (por ejemplo, falló después por
# boost), no repetirlos. Con solo uno de los dos hay que compilar todo.
if [[ -x $RETRO68_BUILD/toolchain/bin/m68k-apple-macos-gcc &&
      -x $RETRO68_BUILD/toolchain/bin/powerpc-apple-macos-gcc ]]; then
    extra+=(--skip-thirdparty)
fi

cd "$RETRO68_BUILD"
"$RETRO68_SRC/build-toolchain.bash" --no-carbon "${extra[@]}" 2>&1 | tee build-toolchain.log

echo
echo "Listo. Herramientas en: $RETRO68_BUILD/toolchain/bin"
echo "Siguiente paso: tools/build.sh"
