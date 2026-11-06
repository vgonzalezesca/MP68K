#!/usr/bin/env bash
# Copia agentes, skills y settings desde claude-setup/ a .claude/ del proyecto.
# Uso (desde la raíz del proyecto):  bash claude-setup/install.sh
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .claude
if [ -e .claude/settings.json ]; then
  echo "Aviso: .claude/settings.json ya existe; se conserva el actual (revisa claude-setup/settings.json)."
else
  cp claude-setup/settings.json .claude/settings.json
fi
cp -r claude-setup/agents claude-setup/skills .claude/
echo "Listo. Contenido de .claude/:"
find .claude -type f | sort
echo "Puedes borrar claude-setup/ cuando quieras."
