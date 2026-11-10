# Rama `68k`: Macintosh LC 580

Objetivo y presupuesto: `docs/SPECS_68K.md`. Datos del equipo: `docs/HARDWARE_LC580.md`. Lo que sirva también a PPC va en
`master`; esta rama solo agrega lo que está en `targets/`.

## Reglas duras de este equipo

1. **Sin FPU.** Nada de `float`/`double` en todo el binario. Compilar con `-m68040 -msoft-float` y comprobar con
   `targets/check-nofpu.sh`, que `tools/build.sh` corre siempre.
2. **Cachés del 040: 4 KB de instrucciones + 4 KB de datos** (por verificar). Las tablas de síntesis no caben: diseñar
   accesos secuenciales y alinear datos calientes a 16 bytes.
3. **Segmentos de código ≤ 32 KB** (límite clásico del jump table 68k). **Hoy no se cumple** (Main 70 KB, ADR-006) y sigue
   sin resolverse. Verificar cómo Retro68 los define antes de dividir módulos.
4. **Memoria total 16 MB** (sistema + app).
5. **Ensamblador solo en `targets/asm/`**, con las condiciones de `CLAUDE.md` (ADR-008). Skill `m68k-asm-040`, agente
   `asm-optimizer`.
6. **Ecualizador solo por coeficientes.** Sin filtros muestra a muestra: costarían ≈ 267 % de la CPU (`docs/SPECS_68K.md`,
   aritmética, sin medir).

## Qué hay en `targets/`

| Archivo | Para qué |
|---|---|
| `target.cmake`, `target.sh` | flags, lista de apps, comprobaciones |
| `mactime.c` | trap `_Microseconds` (A193) |
| `hooks.c` | `MenuHook`/`DragHook` con un puente en asm |
| `microbench.c` | microbenchmarks de instrucciones 68k |
| `qttest.c` | Fase 6: ¿hay QuickTime y qué audio abre? Solo funciones ya declaradas por las Multiversal; sin selectores inventados (sin probar en un Mac) |
| `check-nofpu.sh`, `package-visit.sh` | comprobación de FPU; paquete para llevar al LC 580 |
| `asm/` | kernels 68040: MP3 (ventanas exactas y de 16 bits, DCT32 completo y de media tasa, IMDCT larga, PCM), EQ y WMA; los `dct32*` e `imdct_l` los genera `asm/gen/`, y las ventanas de 16 bits se derivan de las exactas; ADR-014 |
| `asm-test.sh` | prueba bit a bit de los kernels y decodificaciones completas bajo `qemu-m68k` (solo corrección, no tiempos) |
| `asm-prof.sh`, `asm/prof/qemu_hot.py` | perfil por función bajo QEMU con el modelo de ciclos del MC68040UM que usa Snow (sin caché de datos): estimación, no medición |

## Compilar

`tools/build.sh` deja las apps en `dist/68k/`. **No edites aquí documentos compartidos:** hazlo en `master` y fusiona.
