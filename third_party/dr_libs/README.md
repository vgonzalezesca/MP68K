# dr_libs (solo dr_flac)

- Origen: https://github.com/mackron/dr_libs (`dr_flac.h`, v0.13.4, 2026-10-31).
- Licencia: dominio público (Unlicense) o MIT-0, a elección. El texto está al final de `dr_flac.h`.
- SHA-256 de `dr_flac.h`: `b8078eeb5d523502eb7964508cd7ec8450bc79d383bec90aa17fd9e2fefd9c24`.
- El archivo no se modifica. `tools/prepare-dr-flac.sh` genera una copia con un único cambio:
  la estimación de compresión de la búsqueda binaria pasa de `double` a aritmética entera
  (regla 1 de CLAUDE.md: sin punto flotante en lo compartido).
