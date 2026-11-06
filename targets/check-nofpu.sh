#!/usr/bin/env bash
# Falla si el binario contiene alguna instrucción de FPU (TARGET.md, regla 1;
# SPECS RNF5). El LC 580 tiene 68LC040: una instrucción f* provoca una
# excepción de línea F.
#
# Uso: targets/check-nofpu.sh <dir de build> <app>
set -euo pipefail

BUILD=${1:?dir de build}
APP=${2:?nombre de la app}
OBJDUMP=${OBJDUMP:-m68k-apple-macos-objdump}

# Retro68 deja el ELF enlazado junto al CODE convertido (.code.bin.gdb).
elf=""
for cand in "$BUILD/$APP.code.bin.gdb" "$BUILD/$APP.gdb"; do
    [[ -f $cand ]] && { elf=$cand; break; }
done
if [[ -z $elf ]]; then
    echo "check-nofpu: no encuentro el ELF de $APP en $BUILD" >&2
    exit 2
fi

# Formato de objdump: "dirección:<TAB>palabras<TAB>mnemónico operandos".
# Retro68 deja la tabla DWARF .eh_frame dentro de la sección de código, bajo la
# etiqueta __EH_FRAME_BEGIN__*; objdump la desensambla como si fueran
# instrucciones y produce falsos positivos (p. ej. "fmovew"). Se ignora solo ese
# bloque (hasta la siguiente etiqueta) y se informa cuántas líneas se saltaron.
out=$("$OBJDUMP" -d "$elf" | awk -F'\t' '
    /^[0-9a-f]+ <.*>:$/ { skip = ($0 ~ /<__EH_FRAME_BEGIN__/); next }
    skip { skipped++; next }
    NF >= 3 && $3 ~ /^f[a-z]/ { print }
    END { print "#skipped " skipped + 0 }
')
skipped=$(sed -n 's/^#skipped //p' <<<"$out")
fpu=$(grep -v '^#skipped ' <<<"$out" || true)
if [[ -n $fpu ]]; then
    echo "check-nofpu: $APP contiene instrucciones de FPU:" >&2
    echo "$fpu" | head -20 >&2
    echo "(total: $(echo "$fpu" | wc -l))" >&2
    exit 1
fi
echo "check-nofpu: $APP sin instrucciones de FPU ($(basename "$elf"); $skipped líneas de .eh_frame ignoradas)"
