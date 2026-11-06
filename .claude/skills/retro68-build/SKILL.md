---
name: retro68-build
description: Instalar y usar el toolchain Retro68 en CachyOS para compilar apps de Mac OS clásico en 68k y PowerPC, con flags de cada equipo y las compilaciones 68k, PPC y FAT (.APPL/.bin/.dsk). Usar al configurar el entorno, tocar CMake o tools/, armar una FAT o diagnosticar errores de compilación/enlace.
---

# Retro68 — build para Mac 68k y PowerPC

Retro68 es un cross-compilador GCC para Macs 68k y PPC. Objetivos del proyecto:
`m68k-apple-macos` y `powerpc-apple-macos` (sin Carbon). Host: CachyOS (Arch).

## Verificado el 2026-11-06 (Retro68 `d773197`, GCC 16.1.0)
- `tools/build-toolchain.sh` usa `--no-carbon`. Tardó 11 min con 12 hilos. Compila los dos gcc; `--skip-thirdparty`
  solo si ya existen ambos. **Borra `toolchain/` antes de empezar.**
- **Cómo se compila:** `tools/build.sh` compila la rama actual en `build/<equipo>/` y copia a `dist/<equipo>/<app>/`.
  `master` no compila apps Mac. `tools/build-all.sh` hace las tres compilaciones desde los **commits** de `68k` y `ppc`, con
  worktrees en `build/wt/`.
- **Lo propio de cada equipo** está en `targets/target.cmake` (flags y apps) y `targets/target.sh` (triple, toolchain y
  comprobación). Archivos de toolchain: `toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake` y
  `toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake`.
- **PPC:** `-mcpu=601` compila y enlaza. Produce `<app>.xcoff`, `<app>.pef` y los mismos `.bin`, `.dsk`, `.APPL`. `SndGetInfo`
  no está en InterfaceLib sino en **SoundLib** (`link_libraries(SoundLib)`), que debe existir en el equipo.
- **FAT:** Retro68 no la documenta. Se arma con una sola llamada:
  `Rez RetroPPCAPPL.r -I<RIncludes> -DCFRAG_NAME="\"app\"" --copy app.code.bin --data app.pef -o app.bin --cc app.dsk ...`.
  Verificado en `sysprobe`: `CODE` idéntico al de 68k, PEF y `cfrg` idénticos a los de PPC, un solo `SIZE`. **No probado que
  arranque:** eso lo prueba Víctor. Rez deja una copia `app.ad` junto a `%app.ad`, que el script borra.
- Un `.r` propio se compila a `<nombre>.r.rsrc.bin`; la FAT debe recibirlo también (`tools/build-all.sh`).

## Regla de oro
Los datos de este skill vienen de un README antiguo del proyecto y de memoria.
**Antes de ejecutar nada, lee el README actual del repositorio de Retro68** (rama
principal) y corrige este archivo si algo difiere. Todo lo marcado "verificar" no está
confirmado.

## Verificado el 2026-10-29 (Retro68 `d773197`)
- Script del proyecto: `tools/build-toolchain.sh` (verifica dependencias con
  `pacman -T`, compila en `~/Retro68-build`, reanuda con `--skip-thirdparty`).
- Paquetes Arch según el README: `cmake gmp mpfr libmpc boost bison flex texinfo ruby`.
  **ruby es obligatorio** (genera las Multiversal Interfaces); boost lo exige Rez.
  binutils y gcc no necesitan boost; las herramientas del host sí.
- `add_application(nombre fuentes... CONSOLE CREATOR "XXXX" TYPE "APPL")`: `CONSOLE`
  enlaza RetroConsole (ventana con `printf`/`getchar`). Genera `nombre.bin`,
  `nombre.APPL` (+ `.rsrc/`, `.finf/`), `nombre.dsk` y `%nombre.ad`. Sin un `.r` propio
  usa `SIZE` de 1 MB.
- gcc se construye con `--with-cpu=m68000`: las libs son 68000.
- Multiversal Interfaces: los headers clásicos (`Sound.h`, `Gestalt.h`, ...) son alias
  de `Multiverse.h`. **Faltan**: `Script.h`, constantes `si*`, bits de
  `gestaltSoundAttr`, Component Manager y `Microseconds` (solo Executor). Ver ADR-005.
- `fopen` de Retro68 abre en el directorio por defecto (`HOpenDF(0,0,...)`); para
  escribir junto a la app usar `src/common/textlog.c`.

## Instalación (resumen, verificar contra el README vigente)
- Dependencias del host: ver arriba.
- Compilar desde fuente en un directorio de build **separado** del código:
  ```
  git clone --recursive <repositorio de Retro68>
  mkdir Retro68-build && cd Retro68-build
  ../Retro68/build-toolchain.bash --no-carbon    # 68k y PowerPC clásico
  ```
- Resultado: `Retro68-build/toolchain/bin` con `m68k-apple-macos-*` y `powerpc-apple-macos-*`
  (`gcc`, `as`, `ld`, `objdump`), más `Rez`, `MakePEF` y `MakeImport`. Exportar `PATH` con esa carpeta.
- Tarda bastante; correr en segundo plano y guardar el log.

## Uso con CMake
- Buscar el archivo toolchain de CMake generado, normalmente bajo
  `toolchain/m68k-apple-macos/cmake/` (**verificar con `find`**) y usarlo:
  ```
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<ruta>/retro68.toolchain.cmake
  cmake --build build
  ```
- En CMake se usa `add_application(...)` (macro de Retro68; **verificar nombre y
  argumentos** en los ejemplos del repo) para producir la aplicación.
- Salidas posibles (según README): `Nombre.APPL` + `.rsrc/` + `.finf/` (compatible con
  la carpeta compartida `extfs` de Basilisk II), `Nombre.bin` (MacBinary) y
  `Nombre.dsk` (imagen HFS cruda de disquete).

## Flags para el LC 580 (68LC040, sin FPU)
- Candidato: `-O2 -m68040 -msoft-float` (+ `-fomit-frame-pointer`, `-ffunction-sections`
  `-fdata-sections` y `-Wl,--gc-sections` si el enlazador lo soporta). **Verificar** que
  el GCC de Retro68 acepte cada flag.
- Comprobación obligatoria, que `tools/build.sh` corre siempre en la rama `68k`:
  `targets/check-nofpu.sh`. No debe aparecer ninguna instrucción de FPU (`fmove`, `fadd`, ...). En la rama `ppc`, el
  equivalente es `targets/check-601.sh`, que busca `mftb`.
- Vigilar que las bibliotecas estándar de Retro68 (compiladas para 68000 por defecto,
  **verificar**) no arrastren llamadas incompatibles; la mezcla de código 68000 y
  68040 es válida, pero no al revés para instrucciones 040-only en código que corra
  en otras CPUs (aquí no importa: el objetivo es solo 040).
- `-m68040` habilita también instrucciones propias del 040 como `MOVE16`.

## Segmentación
- El código 68k clásico se organiza en segmentos con límite práctico de 32 KB por el
  jump table. Retro68 permite una o varias segmentos. Revisar cómo se asignan (atributo
  `section`, opciones del enlazador) en la documentación actual antes de dividir.
- Mantener el decodificador (kernels calientes + libmad) en un segmento que quede
  cargado y bloqueado (`LoadSeg` implícito en la primera llamada, no se descarga si no
  se llama a `UnloadSeg`).

## Empaquetado para transferir
- Basilisk II: carpeta compartida con el `.APPL` y sus carpetas `.rsrc`/`.finf`.
- LC 580 real: `.dsk` o `.bin` (MacBinary, se decodifica con StuffIt). Víctor usa Compact Flash con adaptador IDE;
  `targets/package-visit.sh` (rama `68k`) arma `dist/visita1/`.

## Diagnóstico frecuente
- "undefined reference" a funciones libc: Retro68 usa su propia libc/Newlib; revisar
  qué funciones están disponibles antes de portar libmad.
- libmad: definir las macros de `config.h`/`CFLAGS` a mano (no hay `configure` para el
  objetivo); registrar la combinación elegida en `docs/DECISIONS.md`.
- Binario demasiado grande: `m68k-apple-macos-size`, `-Wl,-Map`, revisar `--gc-sections`.
