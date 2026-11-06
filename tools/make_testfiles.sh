#!/usr/bin/env bash
# Genera los MP3 de prueba de la etapa 2 en tests/data/ (fuera de git).
#
# Audio sintético y reproducible (sox -R): ruido rosa + barrido + acorde, 60 s,
# 44,1 kHz. Cuesta más a Huffman que una pista muy tonal; no representa música real.
# Para usar música propia, reemplazar test128.mp3, test192.mp3 y testmono.mp3 por
# archivos con los mismos nombres (el programa registra su tamaño y su CRC-32).
#
# Requiere: sox, lame, ffmpeg.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/tests/data
SECONDS_LEN=${SECONDS_LEN:-60}
mkdir -p "$OUT"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

sox -R -n -r 44100 -c 2 -b 16 "$tmp/noise.wav" synth "$SECONDS_LEN" pinknoise vol 0.30
sox -R -n -r 44100 -c 2 -b 16 "$tmp/sweep.wav" synth "$SECONDS_LEN" sine 50-16000 vol 0.30
sox -R -n -r 44100 -c 2 -b 16 "$tmp/chord.wav" synth "$SECONDS_LEN" sine 220 sine 277.2 sine 329.6 vol 0.15
sox -R -m "$tmp/noise.wav" "$tmp/sweep.wav" "$tmp/chord.wav" "$OUT/mix.wav" norm -3

# -t: sin etiqueta Xing/LAME (todos los frames son de audio). --cbr: bitrate fijo.
lame --quiet --cbr -t -b 128 -m j "$OUT/mix.wav" "$OUT/test128.mp3"
lame --quiet --cbr -t -b 192 -m s "$OUT/mix.wav" "$OUT/test192.mp3"
lame --quiet --cbr -t -b 64  -m m "$OUT/mix.wav" "$OUT/testmono.mp3"

# Archivos extra de la etapa 5b-2 (etiquetas y cabeceras de duracion).
#   testvbr.mp3    VBR (-V4) con cabecera Xing y tabla de busqueda
#   testinfo.mp3   CBR 128 con cabecera Info (sin -t)
#   testtag23.mp3  ID3v2.3 + ID3v1 con acentos (ffmpeg)    testtag24.mp3  ID3v2.4 (UTF-8)
lame --quiet -V4 "$OUT/mix.wav" "$OUT/testvbr.mp3"
lame --quiet --cbr -b 128 -m j "$OUT/mix.wav" "$OUT/testinfo.mp3"
TITLE="Canci\xc3\xb3n de prueba"; ARTIST="\xc3\x91and\xc3\xba"; ALBUM="\xc3\x81lbum"
TITLE=$(printf "$TITLE"); ARTIST=$(printf "$ARTIST"); ALBUM=$(printf "$ALBUM")
ffmpeg -v error -y -i "$OUT/testinfo.mp3" -c copy -map_metadata -1 -id3v2_version 3 -write_id3v1 1 \
    -metadata title="$TITLE" -metadata artist="$ARTIST" -metadata album="$ALBUM" "$OUT/testtag23.mp3"
ffmpeg -v error -y -i "$OUT/testinfo.mp3" -c copy -map_metadata -1 -id3v2_version 4 \
    -metadata title="$TITLE" -metadata artist="$ARTIST" -metadata album="$ALBUM" "$OUT/testtag24.mp3"

ls -l "$OUT"/*.mp3
sha256sum "$OUT"/*.mp3
