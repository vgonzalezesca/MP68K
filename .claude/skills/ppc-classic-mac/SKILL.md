---
name: ppc-classic-mac
description: Escribir y portar código C para PowerPC en Mac OS clásico dentro de la rama ppc de MP368K: CFM/PEF, UPP y Mixed Mode, alineación de estructuras, variables globales de bajo nivel y particularidades del PowerPC 601. Usar al trabajar en la rama ppc, en targets/ de esa rama o al revisar si código compartido corre en PowerPC.
---

# PowerPC clásico (rama `ppc`)

Lo marcado "verificado" se comprobó en los headers y bibliotecas de Retro68. Lo demás viene de memoria de la API clásica:
**confirmarlo** en el código de Retro68 y en Inside Macintosh antes de basarse en ello. Nada de esto se ha ejecutado en un
Mac real.

## Reglas de esta rama
- **Solo C**, sin ensamblador propio. Tampoco `FPM_PPC` en libmad: se mantiene `FPM_64BIT` para tener el mismo PCM.
- `float` se permite solo en `targets/` de esta rama (ADR-011), nunca en código compartido.
- Lo que sirva a los dos equipos va en `master`.

## Formato del ejecutable
- **CFM/PEF:** el código va en la bifurcación de datos, y el recurso `cfrg (0)` le dice al sistema cómo cargarlo.
  `add_application` lo hace con `MakePEF` y `RetroPPCAPPL.r` (verificado).
- **No hay segmentos ni jump table:** la regla de 32 KB no aplica aquí.
- **No hay A5:** las globales se resuelven por TOC. Código entre fragmentos pasa por vectores de transición.
- Las bibliotecas del sistema se enlazan contra stubs en `multiversal/libppc` (verificado: `libInterfaceLib.a`,
  `libQuickTimeLib.a`, `libSoundLib.a`, `libDriverServicesLib.a`, `libMathLib.a` y otras).

## UPP y Mixed Mode
- Un puntero a función C en PPC **no** es lo que espera el sistema: hay que pasarlo por `New...UPP`, que crea un descriptor
  de rutina con `NewRoutineDescriptor`. En 68k la misma macro devuelve el puntero tal cual (verificado en `Multiverse.h`).
- **Nunca hacer un cast:** compila y se cae en ejecución.
- Crear **un solo** descriptor por callback: guardarlo en una estática.
- En PPC `pascal` queda definido como vacío (verificado); en 68k es una palabra clave real.
- El Toolbox que aún es 68k se llama a través de Mixed Mode: **un cambio de modo cuesta** (sin medir). No hacerlo en un
  bucle caliente.

## Ganchos de menú y arrastre
`MenuHook` y `DragHook` son variables globales de bajo nivel que el sistema llama como código 68k, sin argumentos, y espera
los registros intactos. `targets/hooks.c` instala un descriptor de Mixed Mode. **Sin verificar:** si Mixed Mode conserva todos
los registros 68k que el sistema espera. Comprobar con `gap` en la línea técnica (`docs/UI.md`).

## Alineación
Las Multiversal encierran las estructuras del Toolbox en `#pragma pack(push, 2)` (verificado), como el 68k. Las estructuras
propias siguen la alineación de PowerPC; no asumir que tienen el mismo diseño que en 68k si se leen de archivo o se comparten
con el sistema.

## PowerPC 601 (`docs/HARDWARE_PPC601.md`)
- **Sin Time Base:** `mftb` no existe en el 601. Usar `Microseconds` de InterfaceLib (verificado que está), que Multiversal no
  declara: la declaración está en `targets/mactime.c`.
- `objdump` no debe mostrar `mftb` (`targets/check-601.sh`).
- FPU integrada y 32 KB de caché: por verificar. Un binario para el 601 debería correr en un 603 o 604, sin garantía hasta
  probarlo.

## Flags (`targets/target.cmake`)
`-mcpu=601 -O2 -fomit-frame-pointer`. El enlazador es XCOFF: `-gc-sections` no se usa aquí.
