---
name: retro68-build
description: Instalar y usar el toolchain Retro68 (m68k-apple-macos) en CachyOS para compilar apps de Mac OS clásico 68k, con flags para 68LC040 sin FPU y empaquetado .APPL/.bin/.dsk. Usar al configurar el entorno, tocar CMake o diagnosticar errores de compilación/enlace.
---

# Retro68 — build para Mac 68k

Retro68 es un cross-compilador GCC para Macs 68k y PPC. Objetivo del proyecto:
`m68k-apple-macos` (binutils basado en `m68k-unknown-elf`). Host: CachyOS (Arch).

## Regla de oro
Los datos de este skill vienen de un README antiguo del proyecto y de memoria.
**Antes de ejecutar nada, lee el README actual del repositorio de Retro68** (rama
principal) y corrige este archivo si algo difiere. Todo lo marcado "verificar" no está
confirmado.

## Instalación (resumen, verificar contra el README vigente)
- Dependencias del host (nombres de paquete Arch, verificar): boost, cmake, gmp, mpfr,
  libmpc, bison, flex, texinfo, ninja/make, git, ruby (algunas partes).
- Compilar desde fuente en un directorio de build **separado** del código:
  ```
  git clone --recursive <repositorio de Retro68>
  mkdir Retro68-build && cd Retro68-build
  ../Retro68/build-toolchain.bash --no-ppc       # solo 68k; --no-ppc implica --no-carbon
  ```
- Resultado: `Retro68-build/toolchain/bin` con `m68k-apple-macos-gcc`, `-as`, `-ld`,
  `-objdump`, más las herramientas de empaquetado (Rez, Elf2Mac, MakeAPPL en versiones
  antiguas). Exportar `PATH` con esa carpeta.
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
- Comprobación obligatoria tras cada cambio de flags:
  ```
  m68k-apple-macos-objdump -d build/<app>.elf | grep -E '\sf[a-z]+' | head
  ```
  No debe aparecer ninguna instrucción de FPU (`fmove`, `fadd`, ...). Ajustar la
  expresión según el formato real del desensamblado.
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
- LC 580 real: `.dsk` (disquete de 1,44 MB si cabe; el binario debería caber) o `.bin`
  (MacBinary) para transferir por red/SCSI y decodificar con StuffIt/BinHex según el
  medio. El método final lo decide la pregunta abierta nº 2 de `SPECS.md`.
- Automatizar en `tools/package.sh`: build → `dist/` → `.dsk` + `.bin`.

## Diagnóstico frecuente
- "undefined reference" a funciones libc: Retro68 usa su propia libc/Newlib; revisar
  qué funciones están disponibles antes de portar libmad.
- libmad: definir las macros de `config.h`/`CFLAGS` a mano (no hay `configure` para el
  objetivo); registrar la combinación elegida en `docs/DECISIONS.md`.
- Binario demasiado grande: `m68k-apple-macos-size`, `-Wl,-Map`, revisar `--gc-sections`.
