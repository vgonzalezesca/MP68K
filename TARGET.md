# Rama `master`: el proyecto general

Esta rama contiene lo que comparten los dos equipos. Las ramas `68k` y `ppc` nacen de aquí y reciben sus
cambios por fusión.

- **No agregues código de un solo equipo.** Va en la rama de ese equipo, bajo `targets/`.
- **Aquí no se compilan apps Mac.** Solo las pruebas de PC: `tools/host-ref.sh` y `tools/host-test.sh`.
- Después de un cambio, fusiónalo a `68k` y a `ppc` y compila ambas (`tools/build-all.sh`). Hasta entonces
  el cambio solo está probado en el PC.
- **No edites este archivo para hablar de un equipo:** cada rama tiene su propio `TARGET.md`, y un cambio
  aquí generaría conflictos al fusionar.
