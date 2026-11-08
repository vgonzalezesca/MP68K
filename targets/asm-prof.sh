#!/usr/bin/env bash
# Perfil de instrucciones dinámicas por función de los decodificadores, ejecutados como 68040 bajo qemu-m68k.
#
# ES UN CONTEO DE INSTRUCCIONES, NO TIEMPO: sirve para decidir dónde escribir asm y para comparar antes/después, no para decir
# "más rápido" (eso solo lo dice una medición en el LC 580). Ver targets/asm/prof/qemu_hot.py.
#
# Uso: targets/asm-prof.sh [caso ...]        casos: mp3 mp3half mp3halforig mp3orig mp3ref mp3eq mp3b192 vorbis vorbismono wma2 wma1   (por omisión, todos)
# Requiere los binarios de targets/asm-test.sh (se generan si faltan).
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build/asmtest
QEMU=${QEMU:-qemu-m68k}
TOP=${TOP:-12}
# Cada caso se corre con dos largos y se informa la diferencia (régimen estable, sin inicialización).
MP3_A=${MP3_A:-12};    MP3_B=${MP3_B:-36}       # cuadros de MP3 (1152 muestras por canal cada uno)
PCM_A=${PCM_A:-8192};  PCM_B=${PCM_B:-30000}    # cuadros (muestras por canal) de Vorbis y WMA
HOT=$ROOT/targets/asm/prof/qemu_hot.py

[[ -x $OUT/e2e_asm && -x $OUT/vorbis_e2e && -x $OUT/wma_e2e_asm ]] || "$ROOT/targets/asm-test.sh" mp3_e2e vorbis_e2e wma_e2e >/dev/null
CASES=("$@")
[[ ${#CASES[@]} -gt 0 ]] || CASES=(mp3 mp3half mp3halforig mp3orig mp3ref mp3eq mp3b192 vorbis vorbismono wma2 wma1)

run() { # etiqueta unidad f1 f2 binario args...   (los args llevan {FRAMES})
    local label=$1 unit=$2 f1=$3 f2=$4 bin=$5; shift 5
    (cd "$ROOT" && python3 -I "$HOT" --top "$TOP" --label "$label" --unit "$unit" --f1 "$f1" --f2 "$f2" --elf "$OUT/$bin" \
        ${CSV_DIR:+--csv "$CSV_DIR/$bin-$label.csv"} -- "$QEMU" -cpu m68040 "$OUT/$bin" "$@")
    echo
}

for c in "${CASES[@]}"; do
    case $c in
        mp3)       run "MP3 128 kbps, nucleos asm"          1152 $MP3_A $MP3_B e2e_asm --nohalf --frames={FRAMES} tests/data/test128.mp3 ;;
        mp3half)   run "MP3 128 kbps, media tasa (HALF), asm"       1152 $MP3_A $MP3_B e2e_asm --onlyhalf --frames={FRAMES} tests/data/test128.mp3 ;;
        mp3halforig) run "MP3 128 kbps, media tasa, libmad ORIGINAL" 1152 $MP3_A $MP3_B e2e_orig --onlyhalf --frames={FRAMES} tests/data/test128.mp3 ;;
        mp3orig)   run "MP3 128 kbps, libmad ORIGINAL (C puro)" 1152 $MP3_A $MP3_B e2e_orig --nohalf --frames={FRAMES} tests/data/test128.mp3 ;;
        mp3ref)    run "MP3 128 kbps, mis referencias en C"   1152 $MP3_A $MP3_B e2e_ref --nohalf --frames={FRAMES} tests/data/test128.mp3 ;;
        mp3eq)     run "MP3 128 kbps, asm + ecualizador"     1152 $MP3_A $MP3_B e2e_asm --nohalf --eq --frames={FRAMES} tests/data/test128.mp3 ;;
        mp3b192)   run "MP3 192 kbps, nucleos asm"           1152 $MP3_A $MP3_B e2e_asm --nohalf --frames={FRAMES} tests/data/test192.mp3 ;;
        vorbis)    run "Ogg Vorbis estereo (Tremor)"         1 $PCM_A $PCM_B vorbis_e2e --frames={FRAMES} tests/data/testvorbis.ogg ;;
        vorbismono) run "Ogg Vorbis mono (Tremor)"           1 $PCM_A $PCM_B vorbis_e2e --frames={FRAMES} tests/data/testvorbismono.ogg ;;
        wma2)      run "WMA v2 estereo 128 kbps (libwma)"    1 $PCM_A $PCM_B wma_e2e_asm --frames={FRAMES} tests/data/testwma2.wma ;;
        wma1)      run "WMA v1 estereo 128 kbps (libwma)"    1 $PCM_A $PCM_B wma_e2e_asm --frames={FRAMES} tests/data/testwma1.wma ;;
        *) echo "caso desconocido: $c" >&2; exit 2 ;;
    esac
done
