# MP368K — Reproductor MP3 para Macintosh 68k

Reproductor MP3 personal (uso propio, GPL) para un **Macintosh LC 580 real**
(68LC040 a 33 MHz, **sin FPU**, 16 MB de RAM), probado también en **Basilisk II**.
Objetivo: MP3 hasta 192 kbps estéreo, la mayor calidad que el hardware permita
en tiempo real. Mac OS clásico (System 7.x), no A/UX ni Mac OS X.

El dueño del proyecto es Víctor (administrador TI, español de Chile). Responde y
documenta en español de Chile. Comentarios de código en español; identificadores
en inglés.

Documentos que mandan, en este orden: este archivo → `SPECS.md` → `docs/DECISIONS.md`.
Las mediciones reales viven en `docs/PERF_LOG.md`. Los datos del equipo (con
contradicciones entre fuentes señaladas) están en `docs/HARDWARE_LC580.md`: **el sonido
del LC 580 podría ser de 8 bits; no asumir 16 bits/44,1 kHz hasta ejecutar `sysprobe`.**

## Decisión de lenguaje (resumen de ADR-001)

- **C (GCC de Retro68) para todo** + **ensamblador 68k solo en los kernels calientes**
  que el perfilado demuestre que lo merecen (síntesis polifásica, DCT32, IMDCT,
  requantización, Huffman; en ese orden de sospecha, a confirmar midiendo).
- **Pascal descartado**: libmad es C, Retro68 es una cadena C/C++, y Pascal no aporta
  rendimiento sobre GCC `-O2` ni control sobre el ensamblador. Si la interfaz de
  Toolbox exige convención Pascal, los headers de Retro68 ya la resuelven.
- Orden obligatorio: **medir → optimizar**. Prohibido escribir asm sin una medición
  en `docs/PERF_LOG.md` que lo justifique.

## Reglas duras de la plataforma

1. **Sin FPU.** Nada de `float`/`double` en el camino de decodificación. Compilar con
   `-m68040 -msoft-float` (o la combinación que el GCC de Retro68 acepte; verificarlo)
   y comprobar con `m68k-apple-macos-objdump -d` que no aparezcan instrucciones `f*`.
2. **Punto fijo** (`mad_fixed_t`, 28 bits fraccionarios en libmad).
3. **Cachés del 040: 4 KB de instrucciones + 4 KB de datos.** Las tablas de síntesis
   no caben: diseñar accesos secuenciales, alinear datos calientes a 16 bytes.
4. **Prohibido el código auto-modificable y el código generado en runtime.** Rompe el
   JIT de Basilisk II y exige vaciar cachés. Es el motivo por el que MPEGdec falla ahí
   (hipótesis, no confirmada).
5. **El callback del Sound Manager corre en tiempo de interrupción**: no mover ni
   asignar memoria, no llamar al File Manager, no tocar objetos del Toolbox que
   puedan mover el heap. Solo avanzar índices y marcar buffers.
6. **Segmentos de código ≤ 32 KB** (límite clásico del jump table 68k). Verificar
   cómo Retro68 los define antes de dividir módulos.
7. **Memoria total 16 MB** (sistema + app). Pedir en la inicialización con `NewPtr`
   y **no** asignar en el bucle de decodificación.
8. **Sin `malloc`/`printf` en el camino caliente.**

## Reglas de trabajo

- **Nunca afirmes que algo corre, suena o es rápido en el LC 580 sin una medición
  real** pegada en `docs/PERF_LOG.md`. Claude Code no tiene acceso al equipo real.
  Lo que no se pudo medir se declara "sin medir".
- Cada kernel en asm tiene: implementación de referencia en C (`src/*_ref.c`),
  versión asm (`asm/*_040.s`) y una prueba **bit a bit** contra la referencia con
  vectores aleatorios y casos límite (saturación, ceros, máximos).
- Los números del manual del 68040 (ciclos de `MULS.L`, `MOVE16`, etc.) se citan con
  su fuente (M68040UM) o se marcan "por verificar" hasta microbenchmarks en el LC 580.
- Un cambio de rendimiento se acepta solo con medición antes/después en el LC 580.
  Basilisk II sirve para lógica y regresiones, **no** para velocidad.
- **Idioma de la aplicación `mp368k`:** todo texto visible para el usuario (menús, alertas,
  ventanas, mensajes de estado y de error) va en **inglés de EE. UU.** (decisión de Víctor). La
  documentación, los comentarios y las respuestas siguen en español de Chile. Las herramientas de
  desarrollo (`sysprobe`, `bench/`, `audiotest`) pueden seguir en español.
- **Aspecto de la interfaz:** tema **Platinum de Mac OS 8** (decisión de Víctor, que descartó el
  estilo Winamp oscuro). Sin visualizador de espectro ni ecualizador.
- No cambiar el formato de salida de la bitácora de benchmarks sin actualizar
  `docs/PERF_LOG.md` y el agente `bench-analyst`.
- Licencia: el binario resultante es GPL (libmad). Mantener los avisos de copyright
  en `third_party/`.
- No incluir ROMs de Apple ni imágenes de sistema en el repositorio.

## Estructura

```
CLAUDE.md          este archivo
SPECS.md           requisitos, presupuesto de ciclos, etapas y criterios de salida
docs/DECISIONS.md  ADRs (lenguaje, decodificador, perfiles de calidad)
docs/PERF_LOG.md   mediciones reales (plantilla incluida)
src/               C: aplicación, UI, E/S de archivo, audio, orquestación
asm/               kernels 68040 (.s) + sus referencias en src/
bench/             banco de pruebas sin audio (etapas 1-3)
tests/             pruebas bit a bit y de regresión
third_party/       libmad (GPL) y similares, con su licencia
tools/             scripts de build, empaquetado (.dsk/.bin) y análisis
dist/              artefactos generados (ignorado por git)
.claude/skills/    conocimiento reutilizable (ver abajo)
.claude/agents/    subagentes especializados (ver abajo)
```

## Skills disponibles (`.claude/skills/`)

| Skill | Cuándo usarla |
|---|---|
| `retro68-build` | Instalar/usar el toolchain, flags, CMake, empaquetado |
| `m68k-asm-040` | Escribir u optimizar ensamblador para el 68040 |
| `mp3-fixedpoint-pipeline` | Entender/modificar las etapas de decodificación y libmad |
| `mac-sound-playback` | Salida de audio con Sound Manager, doble buffer, interrupciones |
| `mac-test-workflow` | Probar en Basilisk II y en el LC 580, protocolo de medición |

## Agentes (`.claude/agents/`)

| Agente | Función |
|---|---|
| `build-engineer` | Toolchain, CMake, flags, empaquetado, reproducibilidad del build |
| `asm-optimizer` | Kernels 68040 en asm, siempre con referencia C y prueba bit a bit |
| `toolbox-reviewer` | Revisión (solo lectura) de código Toolbox: interrupciones, heap, A5 |
| `bench-analyst` | Interpreta mediciones, actualiza `PERF_LOG.md`, propone perfil/optimización |

## Flujo recomendado

1. Etapa 0 y 1 de `SPECS.md` (entorno + microbenchmarks). Claude Code prepara el
   programa; **Víctor lo ejecuta en el LC 580** y devuelve el archivo de resultados.
2. `bench-analyst` registra los números; recién entonces se decide qué va a asm.
3. Una optimización a la vez, cada una con su prueba bit a bit y su medición.

## Cosas que no sé todavía (no asumir)

Ver "Preguntas abiertas" en `SPECS.md` (versión exacta de System, hardware de sonido
del LC 580, método de transferencia de archivos, ROM para Basilisk II).
