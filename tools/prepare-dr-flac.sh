#!/usr/bin/env bash
# Genera una copia de third_party/dr_libs/dr_flac.h sin punto flotante en el camino de decodificación.
#
# dr_flac usa `float`/`double` en un solo lugar que se ejecuta de verdad: las estimaciones de bytes de la
# búsqueda binaria (drflac__seek_to_approximate_flac_frame y vecinas, cuando no hay tabla de búsqueda). En el
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

old3 = ("targetByte = (drflac_uint64)(((drflac_int64)((pcmFrameIndex - pFlac->currentPCMFrame) * pFlac->channels * pFlac->bitsPerSample)/8.0f) * "
        "DRFLAC_BINARY_SEARCH_APPROX_COMPRESSION_RATIO);")
new3 = "targetByte = (((pcmFrameIndex - pFlac->currentPCMFrame) * pFlac->channels * pFlac->bitsPerSample) / 8) * 3 / 5;   /* x 0.6 */"
old4 = "(drflac_uint64)((drflac_int64)(pFlac->totalPCMFrameCount * pFlac->channels * pFlac->bitsPerSample)/8.0f);"
new4 = "(pFlac->totalPCMFrameCount * pFlac->channels * pFlac->bitsPerSample) / 8;"
old5 = "#define DRFLAC_BINARY_SEARCH_APPROX_COMPRESSION_RATIO 0.6f"
new5 = "/* mp368k: el factor 0.6 de la búsqueda binaria pasó a aritmética entera (ver tools/prepare-dr-flac.sh) */"

# (texto viejo, texto nuevo, veces que debe aparecer)
for old, new, times in ((old1, new1, 1), (old2, new2, 1), (old3, new3, 1), (old4, new4, 2), (old5, new5, 1)):
    if text.count(old) != times:
        sys.exit("prepare-dr-flac: el texto a reemplazar aparece %d veces y se esperaban %d (¿otra versión de dr_flac?):\n%s"
                 % (text.count(old), times, old[:100]))
    text = text.replace(old, new)
for left in ("approxCompressionRatio", "/8.0f", "/8.0)", "BINARY_SEARCH_APPROX_COMPRESSION_RATIO"):
    if left in text.replace("/* mp368k: el factor 0.6 de la búsqueda binaria", ""):
        sys.exit("prepare-dr-flac: queda una referencia sin reemplazar: " + left)
open(dst, "w").write(text)
EOF
echo "prepare-dr-flac: $OUT/dr_flac.h"
