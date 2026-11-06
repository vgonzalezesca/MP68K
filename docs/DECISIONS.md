# Decisiones de arquitectura (ADR)

Formato: contexto → decisión → consecuencias → cuándo revisarla.

## ADR-001 — Lenguaje: C + ensamblador 68k (sin Pascal)

**Contexto.** El objetivo es MP3 hasta 192 kbps estéreo en un 68LC040 a 33 MHz.
Se consideraron ensamblador, Pascal y C.

**Decisión.**
- Base en **C** compilada con el GCC de Retro68.
- **Ensamblador 68040** solo en kernels calientes, y solo después de medir.
- **Pascal descartado.**

**Razones.**
1. libmad (decodificador elegido) está en C.
2. Retro68 es una cadena C/C++; no se conoce un compilador Pascal integrado
   (por verificar). Los compiladores Pascal clásicos son nativos de Mac, no
   corren en el host Linux.
3. Pascal no ofrece mejor código que GCC `-O2` ni control a nivel de instrucción;
   cuando el rendimiento importa, la herramienta correcta es el ensamblador.
4. La parte no crítica (interfaz, archivos) se escribe más rápido y con menos
   errores en C.

**Consecuencias.** Convención de llamada GCC m68k (ver skill `m68k-asm-040`).
Los headers de Retro68 resuelven las llamadas Toolbox con convención Pascal.

**Revisar si** el perfilado muestra que casi todo el tiempo está fuera de los kernels
identificados (entonces se amplía el ensamblador, no se cambia a Pascal).

## ADR-002 — Decodificador: libmad como base

**Contexto.** Alternativas: libmad (punto fijo, GPL), mpg123 con ruta sin FPU
(LGPL), minimp3 (usa float en su ruta común; descartado para equipo sin FPU).

**Decisión.** libmad como base; perfilar y reescribir en asm lo que pese.

**Consecuencias.** Binario GPL (aceptado: uso personal). Se debe revisar en el código
fuente real qué opciones `FPM_*`, `OPT_SPEED`, `OPT_SSO` y soporte de media tasa
existen antes de planear los kernels. **No asumir** que haya ruta optimizada para 68k.

**Revisar si** la etapa 2 muestra que la estructura de libmad impide llegar al
presupuesto; entonces evaluar un decodificador propio por etapas.

## ADR-003 — Perfiles de calidad desde el inicio

**Contexto.** La estimación indica que 192 kbps estéreo a calidad plena es incierto
en este hardware.

**Decisión.** Diseñar `FULL`, `HALF`, `MONO`, `HALF_MONO` desde la etapa 2, y elegir
con datos medidos. La selección automática por RTF es opcional.

**Consecuencias.** La interfaz de la capa de síntesis debe admitir variantes sin
duplicar el resto del decodificador.

## ADR-004 — Medición solo en hardware real

**Decisión.** Basilisk II valida lógica y regresiones; el rendimiento se acepta solo
con mediciones del LC 580 registradas en `PERF_LOG.md`.

## ADR-005 — Toolchain: Retro68 con Multiversal Interfaces

**Contexto.** Hay que fijar la versión del toolchain, las interfaces de Toolbox y los
flags para el 68LC040 sin FPU.

**Decisión.**
- Retro68 commit `d77319746eb3` (2026-10-28), solo 68k (`build-toolchain.bash --no-ppc`),
  en `~/Retro68` / `~/Retro68-build`. Script: `tools/build-toolchain.sh`.
- **Multiversal Interfaces** (las libres, incluidas en Retro68) por ahora.
- Flags: `-O2 -m68040 -msoft-float -fomit-frame-pointer -ffunction-sections
  -fdata-sections`; enlace con `-Wl,-gc-sections`. Se conservan los nombres para MacsBug.
  **Por verificar** al compilar que GCC acepte la combinación y que `check-nofpu.sh`
  quede en cero.
- Las bibliotecas de Retro68 vienen compiladas para 68000 (`--with-cpu=m68000`); es
  código válido en el 040.

**Razones.** Las Universal Interfaces de Apple no se pueden redistribuir y exigen sacar
el MPW 3.5 GM con un Mac/emulador. Las Multiversal alcanzan para la etapa 0.

**Consecuencias (verificado en el código de multiversal).**
- Faltan las constantes `si*` del Sound Manager y los bits de `gestaltSoundAttr`: se
  definen en el código con prefijo `MP_`.
- `Microseconds()` solo existe para Executor: glue propia del trap `_Microseconds`
  (A193) en `src/common/mactime.c`.
- Falta el Component Manager (`FindNextComponent`, `GetSoundOutputInfo`); por eso
  `sysprobe` consulta la salida con `SndGetInfo` sobre un canal.
- No existe `Script.h`; `NumVersion` es `LONGINT` (en Universal es un struct).

**Revisar si** la etapa 5 necesita el Component Manager o APIs posteriores a System 7.0
(Sound Manager 3.x avanzado): entonces Víctor aporta las Universal Interfaces 3.4 desde
el MPW 3.5 GM y se recompila Retro68 con `--universal`.

## ADR-006 — libmad 0.15.1b: configuración y hallazgos verificados

**Contexto.** Etapa 2: compilar libmad sin `configure` para Retro68 y comparar contra el PC.

**Decisión.**
- Fuente: `libmad-0.15.1b.tar.gz` de SourceForge (`downloads.sourceforge.net/project/mad/libmad/0.15.1b/`),
  MD5 `1be543bc30c56fb6bea1d7bf6a64e66c` (coincide con el publicado por BLFS y Debian),
  SHA256 `bbfac3ed6bfbc2823d3775ebb931087371e142bb0e9bb1bee51a76a6e0078690` (calculado
  localmente). Sin modificaciones. Copiado a `third_party/libmad/` con `COPYING`,
  `COPYRIGHT` y `CREDITS`.
- Configuración: `FPM_64BIT`, sin `OPT_SPEED`/`OPT_ACCURACY`, sin `ASO_*`, `NDEBUG`,
  `SIZEOF_INT=4`, `SIZEOF_LONG=4`, `SIZEOF_LONG_LONG=8`. Es la misma que usa el libmad de Arch
  (su `mad.h` solo difiere en dos líneas en blanco del que genera `tools/gen-mad-h.sh`).
- Se compilan los `.c` sin `decoder.c` (usa `fork`/`pipe`). El `mad.h` del tarball viene
  generado en otra máquina con `FPM_INTEL` fijo: se regenera con `tools/gen-mad-h.sh`
  (regla de `Makefile.am`).
- Memoria: libmad solo asigna dinámicamente `stream.main_data` (malloc) y `frame.overlap`
  (calloc), y solo si el puntero es NULL. `src/decoder_core.c` les da buffers estáticos y
  no llama a `mad_stream_finish`/`mad_frame_finish`: cumple la regla 8 sin tocar libmad.

**Hallazgos verificados (código y pruebas).**
- No existe `FPM_68K`. Las opciones son 64BIT, INTEL, ARM, MIPS, SPARC, PPC y DEFAULT
  (esta última pierde precisión). `FPM_64BIT` compila a `muls.l` 32×32→64 del 040 más
  4 instrucciones de desplazamiento por producto, **sin llamadas a libgcc**; hay 265
  de esas multiplicaciones en `decbench`. Su costo real: sin medir.
- **`MAD_OPTION_HALFSAMPLERATE` = decimación sin filtro.** `synth_half` ejecuta el `dct32`
  completo (32 subbandas) y evalúa solo 16 de las 32 salidas de la ventana. La salida es
  idéntica bit a bit a tomar una muestra de cada dos de FULL. Consecuencias:
  (1) hay aliasing de lo que está sobre ≈ 11 kHz; (2) el ahorro es solo parte de la ventana,
  no el `dct32`. Esto **contradice** lo que decían SPECS §4 y la skill ("síntesis con 16
  subbandas"); ambos quedaron corregidos. Un HALF con filtrado real o con 16 subbandas
  exigiría implementación propia; decidir tras medir (etapas 3-4).
- MONO (mezcla en subbandas + modo de un canal) queda a ≤ 1 LSB de mezclar el PCM de FULL.
- Pila: `layer3.c` declara `xr[2][576]` (4,6 KB) en la pila y Retro68 no la agranda.
  `src/common/macenv.c` reserva 48 KB extra (`SetApplLimit` + `MaxApplZone`; `ApplLimit` se
  lee en 0x130 porque `GetApplLimit` no tiene glue en las Multiversal). Sin probar en un Mac.
- **Segmentos de código (regla 6, CLAUDE.md):** Retro68 asigna segmentos con un mapa fijo
  (`Elf2Mac/SegmentMap.cc`): 1 = Runtime (libretrocrt, libgcc, libc), 4 = RetroConsole,
  2 = Main (todo lo demás). En `decbench`: Runtime 67 240 B, Main 36 048 B, RetroConsole
  37 552 B, libstdc++ 28 204 B: **la regla de ≤ 32 KB no se cumple y nada la hace cumplir**.
  `sysprobe` (Runtime de 65 KB) corrió en el emulador, lo que no demuestra nada sobre el
  LC 580. Pendiente: confirmarlo en el equipo; si falla, `SegmentMap(filename)` permite
  repartir Main en varios segmentos.

- **Perfilado (etapa 3):** se usa una copia parcheada de libmad (`third_party/patches/`,
  `tools/prepare-libmad-prof.sh`); el árbol `third_party/libmad/` sigue sin modificaciones.
  Las marcas son no-ops sin reloj y no alteran el audio (CRC idéntico, verificado en el PC).

**Revisar si** la etapa 4 compara `OPT_SPEED`/`OPT_SSO` o `ASO_ZEROCHECK` (cambian el PCM:
tendrían su propio CRC de referencia) o si el RTF baseline obliga a un `HALF` propio.

## Plantilla para nuevas decisiones

```
## ADR-NNN — Título
Contexto:
Decisión:
Razones:
Consecuencias:
Revisar si:
```
