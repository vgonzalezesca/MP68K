#!/usr/bin/env bash
# Prueba de mutantes de un kernel asm: aplica cambios pequeños a una copia del .s y comprueba que la prueba unitaria FALLE.
# Una prueba que no detecta un mutante no prueba nada. Falla (exit 1) si algún mutante sobrevive o si un cambio no modificó el archivo.
#
# Uso: targets/asm-mutate.sh <prueba> '<expresion sed>' ['<expresion sed>' ...]
#   pruebas: synth_window dct32 imdct_l pcm eq_mulg ...   (las de targets/asm-test.sh; antes hay que haberla corrido una vez)
# Cada expresión sed se aplica sola (un mutante por expresión).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build/asmtest
BIN=${RETRO68_BUILD:-$HOME/Retro68-build}/toolchain/bin
QEMU=${QEMU:-qemu-m68k}
TEST=${1:?prueba}; shift
declare -A ASM=( [synth_window]="targets/asm/synth_window_040.s" [synth_window_half]="targets/asm/synth_window_half_040.s" [dct32]="targets/asm/dct32_040.s" [imdct_l]="targets/asm/imdct_l_040.s"
                 [pcm]="targets/asm/pcm_040.s" [eq_mulg]="targets/asm/eq_040.s" )
declare -A REFOBJ=( [synth_window]="ref_synth_ref.o" [synth_window_half]="ref_synth_ref.o" [dct32]="ref_dct32_ref.o" [imdct_l]="ref_imdct_ref.o" [pcm]="ref_pcm_ref.o" [eq_mulg]="ref_eq_ref.o" )
[[ -n ${ASM[$TEST]:-} ]] || { echo "asm-mutate: prueba desconocida: $TEST" >&2; exit 2; }
for f in start.o shim.o "test_$TEST.o" "${REFOBJ[$TEST]}"; do
    [[ -f $OUT/$f ]] || { echo "asm-mutate: falta $OUT/$f: corra targets/asm-test.sh $TEST" >&2; exit 2; }
done
LIBGCC=$("$BIN/m68k-apple-macos-gcc" -m68040 -msoft-float -print-libgcc-file-name)
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

n=0; survived=0
for expr in "$@"; do
    n=$((n + 1))
    sed -E "$expr" "$ROOT/${ASM[$TEST]}" > "$tmp/m$n.s"
    if cmp -s "$ROOT/${ASM[$TEST]}" "$tmp/m$n.s"; then
        echo "  ERROR mutante $n ('$expr'): el sed no cambió el archivo"; survived=$((survived + 1)); continue
    fi
    if ! "$BIN/m68k-apple-macos-as" -m68040 "$tmp/m$n.s" -o "$tmp/m$n.o" 2>"$tmp/m$n.err"; then
        echo "  ERROR mutante $n ('$expr'): no ensambla: $(head -1 "$tmp/m$n.err")"; survived=$((survived + 1)); continue
    fi
    "$BIN/m68k-apple-macos-ld.real" -m m68kelf -static -nostdlib -Ttext=0x80000000 -e _start "$OUT/start.o" "$OUT/shim.o" \
        "$OUT/test_$TEST.o" "$OUT/${REFOBJ[$TEST]}" "$tmp/m$n.o" "$LIBGCC" -o "$tmp/m$n"
    if (cd "$tmp" && timeout 120 "$QEMU" -cpu m68040 "./m$n" >"$tmp/m$n.out" 2>&1); then
        echo "  SOBREVIVIÓ mutante $n ('$expr'): la prueba NO lo detecta"; survived=$((survived + 1))
    else
        echo "  detectado  mutante $n ('$expr')"
    fi
done
echo "asm-mutate $TEST: $n mutantes, $survived sin detectar"
[[ $survived -eq 0 ]]
