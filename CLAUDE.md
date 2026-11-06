# MP368K — Reproductor de audio para Mac OS clásico (68k y PowerPC)

Reproductor personal (uso propio, GPL) con dos equipos de referencia, en dos bifurcaciones:

| Rama | Equipo | Lenguaje |
|---|---|---|
| `68k` | **Macintosh LC 580 real** (68LC040 a 33 MHz, **sin FPU**, 16 MB, Mac OS 8.1) | C + asm 68040 en los kernels |
| `ppc` | **Power Macintosh con PowerPC 601** (modelo por confirmar; se diseña para 60 MHz) | solo C |
| `master` | general: lo que comparten ambos | C portable |

**Estado real:** el reproductor de MP3 en 68k existe y se probó en emulador. FLAC y la capa de códecs están compilados y
probados solo en el PC (falta verlos sonar en un Mac). PPC compila `sysprobe` y nada más; Vorbis, WMA, QuickTime y el
ecualizador son la etapa 6 y **no están hechos** (`docs/ETAPA6.md`).

Objetivo: MP3 hasta 192 kbps estéreo, FLAC, Ogg Vorbis, WMA y otros formatos vía QuickTime, con la mayor
calidad que cada equipo permita en tiempo real. Mac OS clásico, no A/UX ni Mac OS X. Tres compilaciones:
68k, PPC y FAT. También se prueba en emuladores (Infinite Mac, Basilisk II).

El dueño del proyecto es Víctor (administrador TI, español de Chile). Responde y
documenta en español de Chile. Comentarios de código en español; identificadores
en inglés. El README es la excepción: va en inglés para GitHub.

Documentos que mandan, en este orden: este archivo → `TARGET.md` (reglas de la rama actual) →
`SPECS.md` → `docs/SPECS_68K.md` o `docs/SPECS_PPC.md` → `docs/DECISIONS.md`. Las mediciones reales
viven en `docs/PERF_LOG.md`. Los datos de los equipos, con las contradicciones entre fuentes señaladas,
están en `docs/HARDWARE_LC580.md` y `docs/HARDWARE_PPC601.md`: **el sonido del LC 580 podría ser de 8 bits;
no asumir 16 bits/44,1 kHz en ningún equipo hasta ejecutar `sysprobe`.**

**Reglas de la rama actual:** lee `TARGET.md` (cambia según la rama).

@TARGET.md

## Modelo de ramas (ADR-007)

- **Flujo único:** `master` → `68k` y `master` → `ppc`. Nunca entre `68k` y `ppc`, ni de vuelta a `master`.
- **Dónde va cada cosa:** si sirve a los dos equipos, en `master`; si solo a uno, en su rama, bajo `targets/`.
- **Documentos compartidos, skills, agentes y `docs/PERF_LOG.md` se editan solo en `master`** y se fusionan a
  las ramas. Es lo que evita conflictos. `TARGET.md` y `targets/` son lo único que difiere por rama.
- `master` no compila apps Mac: solo las pruebas de PC (`tools/host-ref.sh`, `tools/host-test.sh`).
- `tools/build.sh` compila la rama actual; `tools/build-all.sh` hace las tres compilaciones desde los
  **commits** de `68k` y `ppc`.
- Tras un cambio en `master`, fusionarlo a ambas ramas y compilar las dos antes de darlo por bueno.

## Decisión de lenguaje (ADR-001 y ADR-008)

- **Rama `68k`:** C (GCC de Retro68) + **ensamblador 68k en los kernels de decodificación**, por decisión de
  Víctor. Puede escribirse antes de medir en el LC 580 (ver "Reglas de trabajo").
- **Rama `ppc`:** solo C. Sin ensamblador propio.
- **Pascal descartado**: libmad es C, Retro68 es una cadena C/C++, y Pascal no aporta
  rendimiento sobre GCC `-O2` ni control sobre el ensamblador. Si la interfaz de
  Toolbox exige convención Pascal, los headers de Retro68 ya la resuelven.

## Reglas duras del código compartido (`src/`, `bench/`, `third_party/`)

Todo esto compila para los dos equipos, así que manda el más limitado:

1. **Sin `float`/`double` en lo compartido.** El 68LC040 no tiene FPU.
2. **Punto fijo** (`mad_fixed_t`, 28 bits fraccionarios en libmad). El MP3 usa `FPM_64BIT` en ambas ramas,
   para que el PCM sea idéntico bit a bit entre 68k, PPC y PC.
3. **Prohibido el código auto-modificable y el código generado en runtime.** Rompe el
   JIT de Basilisk II y exige vaciar cachés. Es el motivo por el que MPEGdec falla ahí
   (hipótesis, no confirmada).
4. **El callback del Sound Manager corre en tiempo de interrupción**: no mover ni
   asignar memoria, no llamar al File Manager, no tocar objetos del Toolbox que
   puedan mover el heap. Solo avanzar índices y marcar buffers.
5. **Todo callback del Toolbox pasa por `New...UPP`, nunca por un cast.** En 68k es la misma función; en
   PowerPC un cast se cae. Un solo descriptor por callback: crearlo una vez, no en cada llamada.
6. **Pedir memoria en la inicialización** con `NewPtr` y **no** asignar en el bucle de decodificación.
7. **Sin `malloc`/`printf` en el camino caliente.**
8. **Lo que sea de un solo equipo no va aquí:** va en `targets/`, detrás de `src/platform.h`.

## Reglas de trabajo

- **Nunca afirmes que algo corre, suena o es rápido en un equipo real sin una medición
  real** pegada en `docs/PERF_LOG.md`. Claude Code no tiene acceso a esos equipos.
  Lo que no se pudo medir se declara "sin medir".
- **Asm antes de medir (ADR-008, decisión de Víctor):** se permite escribir un kernel sin medición previa
  del LC 580, con estas condiciones, todas obligatorias:
  - referencia en C (`src/*_ref.c`) y versión asm (`targets/asm/*_040.s`);
  - prueba **bit a bit** contra la referencia, con vectores aleatorios y casos límite
    (saturación, ceros, máximos);
  - selector C/asm al compilar, para poder comparar las dos en el mismo equipo;
  - queda marcado `sin medir` y **no se declara más rápido** hasta medirlo en el LC 580. Si la medición
    dice que no ayuda, se vuelve a C.
- Los números de los manuales (M68040UM, MPC601UM) se citan con su fuente o se marcan "por verificar"
  hasta microbenchmarks en el equipo.
- Una mejora de rendimiento se **confirma** solo con medición antes/después en el equipo real.
  Los emuladores sirven para lógica y regresiones, **no** para velocidad.
- **Idioma de la aplicación `mp368k`:** todo texto visible para el usuario (menús, alertas,
  ventanas, mensajes de estado y de error) va en **inglés de EE. UU.** (decisión de Víctor). La
  documentación, los comentarios y las respuestas siguen en español de Chile. Las herramientas de
  desarrollo (`sysprobe`, `bench/`, `audiotest`) pueden seguir en español.
- **Aspecto de la interfaz:** tema **Platinum de Mac OS 8** (decisión de Víctor, que descartó el
  estilo Winamp oscuro). Sin visualizador de espectro. **Con ecualizador de 10 bandas**, también en
  Platinum, según el formato y el equipo (ADR-011).
- No cambiar el formato de salida de la bitácora de benchmarks sin actualizar
  `docs/PERF_LOG.md` y el agente `bench-analyst`.
- Licencia: el binario resultante es GPL (`LICENSE`, GPLv3; libmad es "v2 o posterior"). Mantener los
  avisos de copyright en `third_party/` y revisar la licencia de cada biblioteca antes de traerla.
- No incluir ROMs de Apple ni imágenes de sistema en el repositorio.

## Estructura

```
CLAUDE.md          este archivo (igual en todas las ramas)
TARGET.md          reglas de la rama actual (distinto en cada rama)
SPECS.md           requisitos generales, etapas y criterios de salida
docs/SPECS_68K.md, docs/SPECS_PPC.md   presupuesto de ciclos y objetivos por equipo
docs/ETAPA6.md     las tres solicitudes de la etapa 6
docs/DECISIONS.md  ADRs
docs/PERF_LOG.md   mediciones reales (plantilla incluida)
src/               C compartido: aplicación, UI, E/S de archivo, audio, orquestación
bench/             bancos sin audio compartidos (decbench, decprofile)
tests/             pruebas bit a bit y de regresión
third_party/       libmad (GPL) y similares, con su licencia
tools/             build, toolchain, pruebas de PC y análisis
targets/           solo en las ramas 68k y ppc: flags, reloj, ganchos y lo propio del equipo
targets/asm/       solo en 68k: kernels 68040 (.s)
dist/              artefactos: dist/68k, dist/ppc, dist/fat (ignorado por git)
.claude/skills/    conocimiento reutilizable (ver abajo)
.claude/agents/    subagentes especializados (ver abajo)
```

## Skills disponibles (`.claude/skills/`)

| Skill | Cuándo usarla |
|---|---|
| `retro68-build` | Instalar/usar el toolchain de los dos equipos, flags, CMake, empaquetado, FAT |
| `m68k-asm-040` | Escribir u optimizar ensamblador para el 68040 |
| `ppc-classic-mac` | Código para PowerPC clásico: CFM/PEF, UPP, Mixed Mode, particularidades del 601 |
| `mp3-fixedpoint-pipeline` | Entender/modificar las etapas de decodificación MP3 y libmad |
| `audio-formats` | FLAC, Vorbis, WMA, QuickTime y el ecualizador |
| `mac-sound-playback` | Salida de audio con Sound Manager, doble buffer, interrupciones |
| `mac-test-workflow` | Probar en emuladores y en los equipos reales, protocolo de medición |

## Agentes (`.claude/agents/`)

| Agente | Función |
|---|---|
| `build-engineer` | Toolchain, CMake, flags, las tres compilaciones, reproducibilidad del build |
| `asm-optimizer` | Kernels 68040 en asm, siempre con referencia C, prueba bit a bit y selector C/asm |
| `toolbox-reviewer` | Revisión (solo lectura) de código Toolbox: interrupciones, heap, A5, UPP |
| `bench-analyst` | Interpreta mediciones de ambos equipos, actualiza `PERF_LOG.md`, propone perfil/optimización |

## Flujo recomendado

1. Etapa 6 en tres solicitudes, en este orden: **6-PPC → 6-G → 6-68K** (`docs/ETAPA6.md`). Así lo general
   se prueba en ambos equipos desde el inicio y el asm optimiza el código ya definitivo.
2. Claude Code prepara los programas; **Víctor los ejecuta en los equipos** y devuelve los archivos de
   resultados; `bench-analyst` registra los números.
3. Una optimización a la vez, cada una con su prueba bit a bit.

## Cosas que no sé todavía (no asumir)

Ver "Preguntas abiertas" en `SPECS.md` (modelo del 601, hardware de sonido de ambos equipos, bandas exactas
del ecualizador).
