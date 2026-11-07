#!/usr/bin/env bash
# Pruebas bit a bit de los kernels en ensamblador 68040 bajo qemu-m68k (Linux, sin libc).
#
# Cada prueba (tests/asm/test_*.c) contiene la referencia en C y el kernel asm, los compara y sale con
# código distinto de cero si algo difiere. Se compilan con el GCC de Retro68 y se enlazan con su ld
# (emulación m68kelf) a un ELF estático, con un _start propio y llamadas al sistema por `trap #0`.
# SOLO PRUEBA CORRECCIÓN: QEMU no modela tiempos ni cachés del 68040 real.
#
# Uso: targets/asm-test.sh [nombre ...]      (por omisión, todas las pruebas)
# Requiere: qemu-m68k (paquete qemu-user de Arch) y Retro68 (RETRO68_BUILD, por defecto ~/Retro68-build).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RETRO68_BUILD=${RETRO68_BUILD:-$HOME/Retro68-build}
BIN=$RETRO68_BUILD/toolchain/bin
OUT=$ROOT/build/asmtest
QEMU=${QEMU:-qemu-m68k}
CFLAGS=(-m68040 -msoft-float -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fno-stack-protector
        -Wall -Wextra -Wno-unused-parameter -I"$ROOT/src" -I"$ROOT/tests/asm")

command -v "$QEMU" >/dev/null || { echo "asm-test: falta $QEMU (sudo pacman -S qemu-user)" >&2; exit 2; }
[[ -x $BIN/m68k-apple-macos-gcc ]] || { echo "asm-test: no encuentro Retro68 en $BIN" >&2; exit 2; }
mkdir -p "$OUT"

GCC=$BIN/m68k-apple-macos-gcc
AS=$BIN/m68k-apple-macos-as
LD=$BIN/m68k-apple-macos-ld.real
LIBGCC=$("$GCC" -m68040 -msoft-float -print-libgcc-file-name)

# prueba : fuentes C de la referencia : fuentes asm del kernel
declare -A REF=( [synth_window]="src/synth_ref.c" )
declare -A ASM=( [synth_window]="targets/asm/synth_window_040.s" )
TESTS=("$@")
[[ ${#TESTS[@]} -gt 0 ]] || TESTS=(synth_window)

"$AS" -m68040 "$ROOT/tests/asm/start.s" -o "$OUT/start.o"
"$AS" -m68040 "$ROOT/tests/asm/shim.s" -o "$OUT/shim.o"

rc=0
for t in "${TESTS[@]}"; do
    objs=("$OUT/start.o" "$OUT/shim.o")
    "$GCC" "${CFLAGS[@]}" -c "$ROOT/tests/asm/test_$t.c" -o "$OUT/test_$t.o"; objs+=("$OUT/test_$t.o")
    for c in ${REF[$t]}; do
        o=$OUT/ref_$(basename "$c" .c).o
        "$GCC" "${CFLAGS[@]}" -c "$ROOT/$c" -o "$o"; objs+=("$o")
    done
    for s in ${ASM[$t]}; do
        o=$OUT/asm_$(basename "$s" .s).o
        "$AS" -m68040 "$ROOT/$s" -o "$o"; objs+=("$o")
    done
    "$LD" -m m68kelf -static -nostdlib -Ttext=0x80000000 -e _start "${objs[@]}" "$LIBGCC" -o "$OUT/test_$t"
    echo "== $t"
    "$QEMU" -cpu m68040 "$OUT/test_$t" || rc=1
done
exit $rc
