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
CFLAGS=(-m68040 -msoft-float -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fno-stack-protector -fwrapv
        -Wall -Wextra -Wno-unused-parameter -I"$ROOT/src" -I"$ROOT/tests/asm")

command -v "$QEMU" >/dev/null || { echo "asm-test: falta $QEMU (sudo pacman -S qemu-user)" >&2; exit 2; }
[[ -x $BIN/m68k-apple-macos-gcc ]] || { echo "asm-test: no encuentro Retro68 en $BIN" >&2; exit 2; }
mkdir -p "$OUT"

GCC=$BIN/m68k-apple-macos-gcc
AS=$BIN/m68k-apple-macos-as
LD=$BIN/m68k-apple-macos-ld.real
LIBGCC=$("$GCC" -m68040 -msoft-float -print-libgcc-file-name)

# prueba : fuentes C de la referencia : fuentes asm del kernel
declare -A REF=( [synth_window]="src/synth_ref.c" [dct32]="src/dct32_ref.c" [imdct_l]="src/imdct_ref.c" [pcm]="src/pcm_ref.c" )
declare -A ASM=( [synth_window]="targets/asm/synth_window_040.s" [dct32]="targets/asm/dct32_040.s" [imdct_l]="targets/asm/imdct_l_040.s" [pcm]="targets/asm/pcm_040.s" )
TESTS=("$@")
[[ ${#TESTS[@]} -gt 0 ]] || TESTS=(synth_window dct32 imdct_l pcm mp3_e2e vorbis_e2e wma_e2e)

"$AS" -m68040 "$ROOT/tests/asm/start.s" -o "$OUT/start.o"
"$AS" -m68040 "$ROOT/tests/asm/shim.s" -o "$OUT/shim.o"

# Decodificación de MP3 completos con libmad (copia parcheada) bajo qemu: el PCM de la síntesis con el kernel asm
# (y con la referencia en C) debe dar el MISMO CRC que la libmad original compilada en el PC.
mp3_e2e() {
    local mad=$OUT/libmad lib=(bit fixed frame huffman layer12 layer3 stream synth timer version) f v name o
    local files=() srcs=() objs
    for f in test128 test192 testmono testtransient; do
        [[ -f $ROOT/tests/data/$f.mp3 ]] && files+=("tests/data/$f.mp3")
    done
    [[ ${#files[@]} -gt 0 ]] || { echo "mp3_e2e: faltan los MP3 de tests/data (tools/make_testfiles.sh)" >&2; return 1; }
    "$ROOT/tools/prepare-libmad-eq.sh" "$mad"
    mkdir -p "$OUT/madh32" "$OUT/madh64"
    SIZEOF_LONG=4 "$ROOT/tools/gen-mad-h.sh" "$OUT/madh32/mad.h" FPM_64BIT
    SIZEOF_LONG=8 "$ROOT/tools/gen-mad-h.sh" "$OUT/madh64/mad.h" FPM_64BIT
    for f in "${lib[@]}"; do srcs+=("$mad/$f.c"); done
    # referencia: PC, síntesis original de libmad
    gcc -O2 -w -DMP368K_HOSTREF -DNDEBUG -DHAVE_LIMITS_H -DFPM_64BIT -DSIZEOF_INT=4 -DSIZEOF_LONG=8 -DSIZEOF_LONG_LONG=8 \
        -I"$OUT/madh64" -I"$mad" "${srcs[@]}" "$ROOT/tests/asm/test_mp3_e2e.c" -o "$OUT/e2e_host"
    (cd "$ROOT" && "$OUT/e2e_host" "${files[@]}") > "$OUT/e2e_host.txt"
    local rc_e2e=0 defs kern
    for v in "ref|-DMP368K_SYNTH_WINDOW=mp368k_synth_window_full_ref -DMP368K_DCT32=mp368k_dct32_ref -DMP368K_IMDCT_L=mp368k_imdct_l_ref|src/synth_ref.c src/dct32_ref.c src/imdct_ref.c" \
             "asm|-DMP368K_SYNTH_WINDOW=mp368k_synth_window_full_040 -DMP368K_DCT32=mp368k_dct32_040 -DMP368K_IMDCT_L=mp368k_imdct_l_040|targets/asm/synth_window_040.s targets/asm/dct32_040.s targets/asm/imdct_l_040.s"; do
        IFS='|' read -r name defs kern <<<"$v"
        objs=("$OUT/start.o" "$OUT/shim.o" "$OUT/minilibc_$name.o")
        "$GCC" "${CFLAGS[@]}" -c "$ROOT/tests/asm/minilibc.c" -o "$OUT/minilibc_$name.o"
        for s in "${lib[@]}"; do
            o=$OUT/madlib_${name}_$s.o
            "$GCC" -m68040 -msoft-float -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fwrapv -w -DNDEBUG -DHAVE_LIMITS_H \
                -DFPM_64BIT -DSIZEOF_INT=4 -DSIZEOF_LONG=4 -DSIZEOF_LONG_LONG=8 $defs \
                -I"$OUT/madh32" -I"$mad" -I"$ROOT/src" -c "$mad/$s.c" -o "$o"
            objs+=("$o")
        done
        "$GCC" "${CFLAGS[@]}" -I"$OUT/madh32" -c "$ROOT/tests/asm/test_mp3_e2e.c" -o "$OUT/e2e_$name.o"; objs+=("$OUT/e2e_$name.o")
        for k in $kern; do
            o=$OUT/e2e_k_${name}_$(basename "$k" | tr . _).o
            if [[ $k == *.c ]]; then "$GCC" "${CFLAGS[@]}" -c "$ROOT/$k" -o "$o"; else "$AS" -m68040 "$ROOT/$k" -o "$o"; fi
            objs+=("$o")
        done
        "$LD" -m m68kelf -static -nostdlib -Ttext=0x80000000 -e _start "${objs[@]}" "$LIBGCC" -o "$OUT/e2e_$name"
        (cd "$ROOT" && "$QEMU" -cpu m68040 "$OUT/e2e_$name" "${files[@]}") > "$OUT/e2e_$name.txt"
        if diff -u "$OUT/e2e_host.txt" "$OUT/e2e_$name.txt" >"$OUT/e2e_$name.diff"; then
            echo "  OK    nucleos $name: CRC de la sintesis igual al de libmad original en ${#files[@]} archivos"
        else
            echo "  FALLA nucleos $name:"; cat "$OUT/e2e_$name.diff"; rc_e2e=1
        fi
    done
    cat "$OUT/e2e_host.txt"
    return $rc_e2e
}

# Ogg Vorbis completos con Tremor (copia parcheada) bajo qemu-m68k, que es big-endian como el 68k real, contra el mismo
# programa compilado en el PC (little-endian). El decodificador es entero: el CRC del PCM debe coincidir exactamente.
vorbis_e2e() {
    local trem=$OUT/tremor lib=(bitwise codebook dsp floor0 floor1 floor_lookup framing info mapping0 mdct misc res012 vorbisfile)
    local files=() f objs=() o
    for f in testvorbis testvorbismono; do
        [[ -f $ROOT/tests/data/$f.ogg ]] && files+=("tests/data/$f.ogg")
    done
    [[ ${#files[@]} -gt 0 ]] || { echo "vorbis_e2e: faltan los .ogg de tests/data (tools/make_testfiles.sh)" >&2; return 1; }
    "$ROOT/tools/prepare-tremor.sh" "$trem"
    local srcs=(); for f in "${lib[@]}"; do srcs+=("$trem/$f.c"); done
    gcc -O2 -w -fwrapv -DMP368K_HOSTREF -DMP368K_TREMOR -I"$trem" -I"$ROOT/src" "${srcs[@]}" "$ROOT/src/heap.c" \
        "$ROOT/tests/asm/test_vorbis_e2e.c" -o "$OUT/vorbis_host"
    (cd "$ROOT" && "$OUT/vorbis_host" "${files[@]}") | sed 's/ heap=.*//' > "$OUT/vorbis_host.txt"
    objs=("$OUT/start.o" "$OUT/shim.o" "$OUT/minilibc_vorbis.o")
    "$GCC" "${CFLAGS[@]}" -c "$ROOT/tests/asm/minilibc.c" -o "$OUT/minilibc_vorbis.o"
    for f in "${lib[@]}"; do
        o=$OUT/tremor_$f.o
        "$GCC" -m68040 -msoft-float -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fwrapv -w -DMP368K_TREMOR \
            -I"$trem" -I"$ROOT/src" -c "$trem/$f.c" -o "$o"
        objs+=("$o")
    done
    "$GCC" "${CFLAGS[@]}" -c "$ROOT/src/heap.c" -o "$OUT/heap.o"; objs+=("$OUT/heap.o")
    "$GCC" "${CFLAGS[@]}" -DMP368K_TREMOR -I"$trem" -c "$ROOT/tests/asm/test_vorbis_e2e.c" -o "$OUT/vorbis_e2e.o"; objs+=("$OUT/vorbis_e2e.o")
    "$LD" -m m68kelf -static -nostdlib -Ttext=0x80000000 -e _start "${objs[@]}" "$LIBGCC" -o "$OUT/vorbis_e2e"
    (cd "$ROOT" && "$QEMU" -cpu m68040 "$OUT/vorbis_e2e" "${files[@]}") | sed 's/ heap=.*//' > "$OUT/vorbis_m68k.txt"
    if diff -u "$OUT/vorbis_host.txt" "$OUT/vorbis_m68k.txt" > "$OUT/vorbis.diff"; then
        echo "  OK    Tremor en 68k (big-endian, qemu) == PC (little-endian): CRC del PCM igual en ${#files[@]} archivos"
    else
        echo "  FALLA Tremor en 68k distinto del PC:"; cat "$OUT/vorbis.diff"; return 1
    fi
    cat "$OUT/vorbis_host.txt"
}

# WMA completos con el códec de MP368K (lector ASF + libwma) bajo qemu-m68k (big-endian) contra el PC (little-endian).
wma_e2e() {
    local wma=$OUT/libwma lib=(wmadeci wmafixed ffmpeg_bitstream mdct mdct_lookup fft-ffmpeg) ours=(src/asfparse.c src/id3.c src/arena.c)
    local files=() f objs=() o
    for f in testwma2 testwma1; do
        [[ -f $ROOT/tests/data/$f.wma ]] && files+=("tests/data/$f.wma")
    done
    [[ ${#files[@]} -gt 0 ]] || { echo "wma_e2e: faltan los .wma de tests/data (tools/make_testfiles.sh)" >&2; return 1; }
    "$ROOT/tools/prepare-libwma.sh" "$wma"
    local srcs=(); for f in "${lib[@]}"; do srcs+=("$wma/$f.c"); done
    gcc -O2 -w -fwrapv -DMP368K_HOSTREF -DROCKBOX -I"$wma" -I"$wma/codecs/lib" -I"$ROOT/src" "${srcs[@]}" \
        "${ours[@]/#/$ROOT/}" "$ROOT/src/codec_wma.c" "$ROOT/tests/asm/test_wma_e2e.c" -o "$OUT/wma_host"
    (cd "$ROOT" && "$OUT/wma_host" "${files[@]}") > "$OUT/wma_host.txt"
    objs=("$OUT/start.o" "$OUT/shim.o" "$OUT/minilibc_wma.o")
    "$GCC" "${CFLAGS[@]}" -c "$ROOT/tests/asm/minilibc.c" -o "$OUT/minilibc_wma.o"
    for f in "${lib[@]}"; do
        o=$OUT/wma_$f.o
        "$GCC" -m68040 -msoft-float -O2 -fomit-frame-pointer -ffreestanding -fno-builtin -fwrapv -w -DROCKBOX \
            -I"$wma" -I"$wma/codecs/lib" -I"$ROOT/src" -c "$wma/$f.c" -o "$o"
        objs+=("$o")
    done
    for f in "${ours[@]}"; do
        o=$OUT/wma_ours_$(basename "$f" .c).o
        "$GCC" "${CFLAGS[@]}" -c "$ROOT/$f" -o "$o"; objs+=("$o")
    done
    "$GCC" "${CFLAGS[@]}" -DROCKBOX -I"$wma" -I"$wma/codecs/lib" -c "$ROOT/src/codec_wma.c" -o "$OUT/wma_codec.o"; objs+=("$OUT/wma_codec.o")
    "$GCC" "${CFLAGS[@]}" -c "$ROOT/tests/asm/test_wma_e2e.c" -o "$OUT/wma_e2e.o"; objs+=("$OUT/wma_e2e.o")
    "$LD" -m m68kelf -static -nostdlib -Ttext=0x80000000 -e _start "${objs[@]}" "$LIBGCC" -o "$OUT/wma_e2e"
    (cd "$ROOT" && "$QEMU" -cpu m68040 "$OUT/wma_e2e" "${files[@]}") > "$OUT/wma_m68k.txt"
    if diff -u "$OUT/wma_host.txt" "$OUT/wma_m68k.txt" > "$OUT/wma.diff"; then
        echo "  OK    WMA en 68k (big-endian, qemu) == PC (little-endian): CRC del PCM igual en ${#files[@]} archivos"
    else
        echo "  FALLA WMA en 68k distinto del PC:"; cat "$OUT/wma.diff"; return 1
    fi
    cat "$OUT/wma_host.txt"
}

rc=0
for t in "${TESTS[@]}"; do
    if [[ $t == wma_e2e ]]; then
        echo "== wma_e2e"
        wma_e2e || rc=1
        continue
    fi
    if [[ $t == vorbis_e2e ]]; then
        echo "== vorbis_e2e"
        vorbis_e2e || rc=1
        continue
    fi
    if [[ $t == mp3_e2e ]]; then
        echo "== mp3_e2e"
        mp3_e2e || rc=1
        continue
    fi
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
