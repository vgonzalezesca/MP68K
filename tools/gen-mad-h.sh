#!/usr/bin/env bash
# Genera mad.h (la cabecera pública de libmad) con la configuración del proyecto.
#
# El tarball de libmad trae un mad.h ya generado en otra máquina, con
# "# define FPM_INTEL" fijo: no sirve. Esta es la regla de Makefile.am
# (target "mad.h") sin autoconf; el contenido de las cabeceras no se modifica.
#
# Uso: tools/gen-mad-h.sh <salida> <FPM_xxx> [OPT_SPEED|OPT_ACCURACY]
#   SIZEOF_LONG=8 para el PC (por defecto 4, que es el 68k)
#   ej.: tools/gen-mad-h.sh build/libmad/mad.h FPM_64BIT
set -euo pipefail

OUT=${1:?archivo de salida}
FPM=${2:?FPM_64BIT | FPM_DEFAULT | ...}
OPT=${3:-}

SRC=$(cd "$(dirname "$0")/../third_party/libmad" && pwd)
exported_headers="version.h fixed.h bit.h timer.h stream.h frame.h synth.h decoder.h"

mkdir -p "$(dirname "$OUT")"
{
    sed -e '1s|.*|/*|' -e '1b' -e '$s|.*| */|' -e '$b' -e 's/^.*/ *&/' "$SRC/COPYRIGHT"
    echo
    echo "# ifdef __cplusplus"
    echo 'extern "C" {'
    echo "# endif"
    echo
    echo "# define $FPM"
    echo
    [[ -n $OPT ]] && { echo "# define $OPT"; echo; }
    # Mismos tamaños que se pasan con -D al compilar libmad (ver CMakeLists.txt).
    echo "# define SIZEOF_INT 4"
    echo "# define SIZEOF_LONG ${SIZEOF_LONG:-4}"
    echo "# define SIZEOF_LONG_LONG 8"
    echo
    for header in $exported_headers; do
        echo
        sed -n -f "$SRC/mad.h.sed" "$SRC/$header"
    done
    echo
    echo "# ifdef __cplusplus"
    echo '}'
    echo "# endif"
} > "$OUT"
