#!/usr/bin/env bash
# Las tres compilaciones: dist/68k, dist/ppc y dist/fat (ADR-012).
#
# Compila los COMMITS de las ramas 68k y ppc, no el árbol de trabajo: lo que no esté
# confirmado no entra. Usa worktrees en build/wt/ y funciona desde cualquier rama.
# Una app va a la FAT solo si existe en ambas ramas (hoy solo sysprobe; mp368k tras la 6-PPC).
#
#   RETRO68_BUILD  directorio de compilación de Retro68 (por defecto ~/Retro68-build)
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
RETRO68_BUILD=${RETRO68_BUILD:-$HOME/Retro68-build}
WT=$ROOT/build/wt
CREATOR=M368

if [[ -n $(git -C "$ROOT" status --porcelain --untracked-files=no) ]]; then
    echo "Aviso: hay cambios sin confirmar en $(git -C "$ROOT" branch --show-current); no entran en esta compilación."
fi

git -C "$ROOT" worktree prune
declare -A HASH
for t in 68k ppc; do
    if [[ -e $WT/$t/.git ]]; then
        git -C "$WT/$t" checkout -q --detach "$t"
    else
        git -C "$ROOT" worktree add -q --detach "$WT/$t" "$t"
    fi
    HASH[$t]=$(git -C "$WT/$t" rev-parse --short HEAD)
    echo "=== $t @ ${HASH[$t]}"
    RETRO68_BUILD=$RETRO68_BUILD "$WT/$t/tools/build.sh"
    rm -rf "$ROOT/dist/$t"
    mkdir -p "$ROOT/dist"
    cp -a "$WT/$t/dist/$t" "$ROOT/dist/$t"
done

# Lista de apps de cada rama, sin mezclar sus funciones con este shell.
apps_of() { ( source "$WT/$1/targets/target.sh"; echo "${APPS[@]}" ); }

# FAT: CODE de 68k y cfrg en la bifurcación de recursos, PEF en la de datos. Un 68k sin CFM
# ignora cfrg y arranca desde CODE; un PowerPC ve cfrg y carga el PEF.
RIN=$RETRO68_BUILD/toolchain/powerpc-apple-macos/RIncludes
REZ=$RETRO68_BUILD/toolchain/bin/Rez
B68=$WT/68k/build/68k
BPPC=$WT/ppc/build/ppc

rm -rf "$ROOT/dist/fat"
for app in $(apps_of 68k); do
    [[ " $(apps_of ppc) " == *" $app "* ]] || continue
    out=$ROOT/dist/fat/$app
    mkdir -p "$out"
    rsrc=()
    [[ $app == mp368k ]] && rsrc=("$B68/player.r.rsrc.bin")
    (
        cd "$out"
        "$REZ" "$RIN/RetroPPCAPPL.r" -I"$RIN" -DCFRAG_NAME="\"$app\"" \
            --copy "$B68/$app.code.bin" --data "$BPPC/$app.pef" \
            -o "$app.bin" --cc "$app.dsk" --cc "$app.APPL" --cc "%$app.ad" \
            -t APPL -c "$CREATOR" "${rsrc[@]}"
        rm -f "$app.ad"     # copia que Rez deja junto a %app.ad; no es un artefacto
    )
    echo "== $app -> dist/fat/$app/"
    ls -la "$out"
done

{
    echo "68k ${HASH[68k]}"
    echo "ppc ${HASH[ppc]}"
    echo "date $(date -Iseconds)"
} > "$ROOT/dist/BUILD_INFO.txt"
cat "$ROOT/dist/BUILD_INFO.txt"
