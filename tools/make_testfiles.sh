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

# Archivos de la etapa 6 (otros formatos). Mismas pistas que los MP3, con las mismas etiquetas.
#   testflac.flac      16 bits estereo           testflac24.flac  24 bits (se reduce a 16 al reproducir)
#   testflacmono.flac  16 bits mono              testflac8.flac   8 bits
#   testvorbis.ogg     oggenc -q4 estereo        testvorbismono.ogg
#   testwma2.wma       WMA v2 128 kbps           testwma1.wma     WMA v1 128 kbps
#   test.wav, test.aif PCM 16 bits (QuickTime o lectores propios)
# Los .snd de Mac son recursos, no archivos de audio estandar: ffmpeg no los escribe.
sox -R "$OUT/mix.wav" -b 24 "$tmp/mix24.wav"
sox -R "$OUT/mix.wav" -b 8 -r 22050 "$tmp/mix8.wav" dither -s
sox -R "$OUT/mix.wav" -c 1 "$tmp/mixmono.wav"
flac -s -f -5 --tag="TITLE=$TITLE" --tag="ARTIST=$ARTIST" --tag="ALBUM=$ALBUM" "$OUT/mix.wav" -o "$OUT/testflac.flac"
flac -s -f -5 "$tmp/mix24.wav" -o "$OUT/testflac24.flac"
flac -s -f -5 "$tmp/mixmono.wav" -o "$OUT/testflacmono.flac"
flac -s -f -5 "$tmp/mix8.wav" -o "$OUT/testflac8.flac"
oggenc -Q -s 368 -q 4 -t "$TITLE" -a "$ARTIST" -l "$ALBUM" "$OUT/mix.wav" -o "$OUT/testvorbis.ogg"
oggenc -Q -s 368 -q 4 "$tmp/mixmono.wav" -o "$OUT/testvorbismono.ogg"
ffmpeg -v error -y -i "$OUT/mix.wav" -c:a wmav2 -b:a 128k -metadata title="$TITLE" -metadata artist="$ARTIST" \
    -metadata album="$ALBUM" "$OUT/testwma2.wma"
ffmpeg -v error -y -i "$OUT/mix.wav" -c:a wmav1 -b:a 128k "$OUT/testwma1.wma"
cp "$OUT/mix.wav" "$OUT/test.wav"
ffmpeg -v error -y -i "$OUT/mix.wav" -c:a pcm_s16be "$OUT/test.aif"

# Archivos del ecualizador (fase 3): ruido blanco para medir la respuesta por banda, y transitorios
# para que el codificador use bloques cortos.
#   testnoise44/48/32.mp3  ruido blanco estereo a 44,1 / 48 / 32 kHz, 256 kbps
#   testnoisemono.mp3      ruido blanco mono a 44,1 kHz
#   testtransient.mp3      rafagas de ruido de 10 ms cada 200 ms (ataques que fuerzan bloques cortos)
for pair in 44100:testnoise44 48000:testnoise48 32000:testnoise32; do
    rate=${pair%%:*}; name=${pair##*:}
    sox -R -n -r "$rate" -c 2 -b 16 "$tmp/$name.wav" synth 20 whitenoise vol 0.25
    lame --quiet --cbr -b 256 -m j "$tmp/$name.wav" "$OUT/$name.mp3"
done
sox -R -n -r 44100 -c 1 -b 16 "$tmp/nm.wav" synth 20 whitenoise vol 0.25
lame --quiet --cbr -b 128 -m m "$tmp/nm.wav" "$OUT/testnoisemono.mp3"
sox -R -n -r 44100 -c 2 -b 16 "$tmp/burst.wav" synth 0.010 whitenoise vol 0.8 pad 0 0.190
sox -R "$tmp/burst.wav" "$tmp/clicks.wav" repeat 99
lame --quiet --cbr -b 192 -m j "$tmp/clicks.wav" "$OUT/testtransient.mp3"

ls -l "$OUT"/*.mp3 "$OUT"/*.flac "$OUT"/*.ogg "$OUT"/*.wma "$OUT"/test.wav "$OUT"/test.aif
sha256sum "$OUT"/*.mp3 "$OUT"/*.flac "$OUT"/*.ogg "$OUT"/*.wma "$OUT"/test.wav "$OUT"/test.aif
