#!/usr/bin/env bash
# Falla si el código compartido llama a SndSoundManagerVersion o usa NumVersion directamente.
# En PowerPC esa llamada corrompe memoria (ver src/common/sndver.h). Solo sndver.h y
# targets/sndver.c pueden nombrarla.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

hits=$(grep -rnE '\b(SndSoundManagerVersion|NumVersion)\b' src bench tests \
           --include='*.c' --include='*.h' --include='*.s' |
       grep -v '^src/common/sndver\.h:' || true)
if [[ -n $hits ]]; then
    echo "check-numversion: uso directo de SndSoundManagerVersion/NumVersion (usar sndver_get):" >&2
    echo "$hits" >&2
    exit 1
fi
echo "check-numversion: sin usos directos"
