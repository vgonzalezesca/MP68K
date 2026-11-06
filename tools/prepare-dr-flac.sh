#!/usr/bin/env bash
# Genera una copia de third_party/dr_libs/dr_flac.h sin punto flotante en el camino de decodificación.
#
# dr_flac usa `double` en un solo lugar que se ejecuta de verdad: la estimación de compresión de la
# búsqueda binaria (drflac__seek_to_approximate_flac_frame, cuando no hay tabla de búsqueda). En el
# 68LC040 eso serían llamadas a libgcc de cientos de ciclos, y la regla 1 de CLAUDE.md prohíbe
# `float`/`double` en lo compartido. Se reemplaza por aritmética entera. Es solo una pista para elegir
# el próximo byte: el bucle converge igual. El original no se modifica (como libmad con el perfilado).
#
# El resto de float en dr_flac son las rutas de salida f32, que no se llaman: con -ffunction-sections
# y -gc-sections no llegan al binario (lo comprueba targets/check-nosoftfloat.sh).
#
# Uso: tools/prepare-dr-flac.sh <directorio de salida>
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:?directorio de salida}
mkdir -p "$OUT"

python3 -I - "$ROOT/third_party/dr_libs/dr_flac.h" "$OUT/dr_flac.h" <<'EOF'
import sys

src, dst = sys.argv[1], sys.argv[2]
text = open(src).read()

old1 = ("const double approxCompressionRatio = (drflac_int64)(lastSuccessfulSeekOffset - pFlac->firstFLACFramePosInBytes) / "
        "((drflac_int64)(pcmRangeLo * pFlac->channels * pFlac->bitsPerSample)/8.0);")
new1 = ("const drflac_uint64 mp_pcmBytesLo = (pcmRangeLo * pFlac->channels * pFlac->bitsPerSample) / 8;\n"
        "                        const drflac_uint64 mp_encBytes = lastSuccessfulSeekOffset - pFlac->firstFLACFramePosInBytes;")
old2 = ("targetByte = lastSuccessfulSeekOffset + (drflac_uint64)(((drflac_int64)((pcmFrameIndex-pcmRangeLo) * pFlac->channels * "
        "pFlac->bitsPerSample)/8.0) * approxCompressionRatio);")
new2 = ("{\n"
        "                            const drflac_uint64 mp_pcmBytesDelta = ((pcmFrameIndex-pcmRangeLo) * pFlac->channels * pFlac->bitsPerSample) / 8;\n"
        "                            targetByte = lastSuccessfulSeekOffset + (mp_pcmBytesLo > 0 ? (mp_pcmBytesDelta * mp_encBytes) / mp_pcmBytesLo : mp_pcmBytesDelta);\n"
        "                        }")

for old in (old1, old2):
    if text.count(old) != 1:
        sys.exit("prepare-dr-flac: el texto a reemplazar no está exactamente una vez (¿otra versión de dr_flac?):\n" + old[:100])
text = text.replace(old1, new1).replace(old2, new2)
if "approxCompressionRatio" in text:
    sys.exit("prepare-dr-flac: queda una referencia a approxCompressionRatio")
open(dst, "w").write(text)
EOF
echo "prepare-dr-flac: $OUT/dr_flac.h"
