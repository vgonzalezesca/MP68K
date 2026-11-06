#!/usr/bin/env bash
# Falla si el código propio o de terceros que SÍ llega al binario llama a rutinas de punto flotante
# por software de libgcc (__adddf3, __muldf3, __floatsidf, __extendsfdf2...). Con -msoft-float el
# código pasa targets/check-nofpu.sh, pero cada una de esas llamadas cuesta cientos de ciclos en un
# 68LC040: en el camino de decodificación serían un desastre silencioso.
#
# Dos niveles:
#  1. Objetos (CMakeFiles/**.obj): ninguno puede referenciar float por software, salvo los de
#     terceros que se listan en ALLOWED_OBJ (dr_flac trae rutas de salida f32 que nunca se llaman).
#  2. Para esos objetos permitidos, se comprueba en cada binario ENLAZADO que ninguna de sus
#     funciones sobrevivió llamando a float: con -ffunction-sections y -gc-sections las rutas f32
#     sin uso desaparecen. Si alguna sobrevive, es un uso real y el chequeo falla.
# La libc de Retro68 trae float por dentro (_dtoa_r para printf): no es código nuestro y no se mira.
# Los enteros de 64 bits (__muldi3...) no cuentan.
#
# Uso: targets/check-nosoftfloat.sh <dir de build>
set -euo pipefail

BUILD=${1:?dir de build}
NM=${NM:-m68k-apple-macos-nm}
OBJDUMP=${OBJDUMP:-m68k-apple-macos-objdump}
ALLOWED_OBJ=${ALLOWED_OBJ:-dr_flac_impl}            # expresión regular sobre el nombre del objeto
THIRD_PARTY_FN=${THIRD_PARTY_FN:-^drflac}           # funciones de esos objetos, por prefijo

# Nombres de libgcc para float y double: llevan "df", "sf" o "tf" (no "si" ni "di" solos).
pat='^__[a-z]*(df|sf|tf)[0-9a-z]*$'

hits=""
allowed_seen=0
count=0
while IFS= read -r obj; do
    count=$((count + 1))
    syms=$("$NM" -u "$obj" 2>/dev/null | awk '{print $2}' | grep -E "$pat" || true)
    [[ -z $syms ]] && continue
    if [[ $(basename "$obj") =~ $ALLOWED_OBJ ]]; then
        allowed_seen=1
        continue
    fi
    hits+="${obj#$BUILD/}: $(echo $syms)"$'\n'
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

survivors=""
checked=0
if [[ $allowed_seen -eq 1 ]]; then
    for elf in "$BUILD"/*.code.bin.gdb; do
        [[ -f $elf ]] || continue
        checked=$((checked + 1))
        bad=$("$OBJDUMP" -m m68k:68040 -d "$elf" | awk -F'\t' -v fnpat="$THIRD_PARTY_FN" -v sfpat='^__[a-z]*(df|sf|tf)[0-9a-z]*$' '
            /^[0-9a-f]+ <.*>:$/ { fn = $0; sub(/^[0-9a-f]+ </, "", fn); sub(/>:$/, "", fn); next }
            $3 ~ /^(jsr|bsr|jbsr|jmp|bra)/ && fn ~ fnpat {
                if (match($3, /<__[a-z0-9_]+(\+0x[0-9a-f]+)?>/)) {
                    t = substr($3, RSTART + 1, RLENGTH - 2); sub(/\+0x[0-9a-f]+$/, "", t)
                    if (t ~ sfpat) print fn " -> " t
                }
            }' | sort -u)
        [[ -n $bad ]] && survivors+="$(basename "$elf"): $(echo "$bad" | tr '\n' ';')"$'\n'
    done
fi
if [[ -n $survivors ]]; then
    echo "check-nosoftfloat: código de terceros con float por software que llega al binario:" >&2
    printf '%s' "$survivors" >&2
    exit 1
fi

if [[ $allowed_seen -eq 1 ]]; then
    echo "check-nosoftfloat: $count objetos sin float por software; el de terceros permitido (rutas f32 sin uso) no llega a $checked binarios"
else
    echo "check-nosoftfloat: $count objetos sin float por software"
fi
