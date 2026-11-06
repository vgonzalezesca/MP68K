#!/usr/bin/env bash
# Falla si algún objeto compilado por nosotros referencia rutinas de punto flotante por software de
# libgcc (__adddf3, __muldf3, __floatsidf, __extendsfdf2...). Con -msoft-float el código pasa
# targets/check-nofpu.sh, pero cada una de esas llamadas cuesta cientos de ciclos en un 68LC040:
# en el camino de decodificación serían un desastre silencioso.
#
# Solo mira nuestros objetos (CMakeFiles/**.obj): la libc de Retro68 trae float por dentro
# (_dtoa_r para printf) y eso no es código nuestro. Los enteros de 64 bits (__muldi3...) no cuentan.
#
# Uso: targets/check-nosoftfloat.sh <dir de build>
set -euo pipefail

BUILD=${1:?dir de build}
NM=${NM:-m68k-apple-macos-nm}

# Nombres de libgcc para float y double: llevan "df", "sf" o "tf" (no "si" ni "di" solos).
pat='^__[a-z]*(df|sf|tf)[0-9a-z]*$'

hits=""
count=0
while IFS= read -r obj; do
    count=$((count + 1))
    syms=$("$NM" -u "$obj" 2>/dev/null | awk '{print $2}' | grep -E "$pat" || true)
    [[ -n $syms ]] && hits+="${obj#$BUILD/}: $(echo $syms)"$'\n'
done < <(find "$BUILD" -name '*.obj' -path '*CMakeFiles*')

if [[ $count -eq 0 ]]; then
    echo "check-nosoftfloat: no encontré objetos en $BUILD" >&2
    exit 2
fi
if [[ -n $hits ]]; then
    echo "check-nosoftfloat: objetos con float por software ($count revisados):" >&2
    printf '%s' "$hits" >&2
    exit 1
fi
echo "check-nosoftfloat: $count objetos sin float por software"
