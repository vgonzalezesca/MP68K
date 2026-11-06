# Decisiones de arquitectura (ADR)

Formato: contexto → decisión → consecuencias → cuándo revisarla.

## ADR-001 — Lenguaje: C + ensamblador 68k (sin Pascal)

*Ajustada por ADR-008: el asm ya no espera una medición previa. Rige solo en la rama `68k`; la rama `ppc` es solo C.*

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

*Ajustada por ADR-008: sigue vigente para **confirmar** una mejora, ya no como requisito para escribir asm.*

**Decisión.** Basilisk II valida lógica y regresiones; el rendimiento se acepta solo
con mediciones del equipo real registradas en `PERF_LOG.md`.

## ADR-005 — Toolchain: Retro68 con Multiversal Interfaces

**Contexto.** Hay que fijar la versión del toolchain, las interfaces de Toolbox y los
flags para el 68LC040 sin FPU.

**Decisión.**
- Retro68 commit `d77319746eb3` (2026-10-28), 68k y PowerPC clásico, sin Carbon
  (`build-toolchain.bash --no-carbon`; hasta la bifurcación fue `--no-ppc`),
  en `~/Retro68` / `~/Retro68-build`. Script: `tools/build-toolchain.sh`.
- **Multiversal Interfaces** (las libres, incluidas en Retro68) por ahora.
- Flags: `-O2 -m68040 -msoft-float -fomit-frame-pointer -ffunction-sections
  -fdata-sections`; enlace con `-Wl,-gc-sections`. Se conservan los nombres para MacsBug.
  **Por verificar** al compilar que GCC acepte la combinación y que `targets/check-nofpu.sh`
  quede en cero. Los flags de cada equipo están en `targets/target.cmake`.
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
- **Corrupción de memoria en PowerPC por esa diferencia (2026-11-06; hipótesis apoyada en el desensamblado y en
  los bytes del log).** En el primer `sysprobe` PPC, 4 bytes `03 21 80 00` (la versión 3.2.1 del Sound Manager)
  aparecieron sobre el texto del log. Las Multiversal declaran `SndSoundManagerVersion` como si devolviera un número;
  en PowerPC un `struct` se devuelve por un puntero oculto en `r3`, y sin ese puntero la función escribió sobre lo que
  `r3` tuviera (el destino del último `memcpy`). En 68k no ocurre. **Regla:** toda función cuyo retorno sea un `struct` en
  Apple pero un número en las Multiversal se llama por una función propia de `targets/` (aquí `sndver_get`), y
  `tools/check-numversion.sh` lo hace cumplir. Falta revisar otros retornos por valor, como `Point`.

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

- **Dato nuevo (2026-11-06):** `sysprobe` y `mp368k` ya corren en Infinite Mac con segmentos de 63 y 70 KB, así que el límite
  de 32 KB no se aplica de forma visible en ese sistema. Sigue sin probarse en el LC 580. `tools/build.sh` ahora imprime los
  tamaños de cada segmento y `segtest` (rama `68k`) ejecuta código repartido en un segmento de 226 KB. **Falta correrlo.**

**Revisar si** la etapa 4 compara `OPT_SPEED`/`OPT_SSO` o `ASO_ZEROCHECK` (cambian el PCM:
tendrían su propio CRC de referencia) o si el RTF baseline obliga a un `HALF` propio.

## ADR-007 — Bifurcación en ramas de git: `master`, `68k` y `ppc`

**Contexto.** Víctor quiere una versión PowerPC como bifurcación, con la etapa 6 pedida en tres solicitudes: 68k, PPC y
el proyecto conjunto. Se comparó una sola rama con carpetas por equipo contra ramas de git separadas.

**Decisión (de Víctor).** Ramas de git. `master` es la general; `68k` y `ppc` nacen de ella.
- Se usa `master` y no `main`: `MP68K/main` solo tiene `LICENSE` y un README, sin historia común con el código.
- Flujo único: `master` → `68k` y `master` → `ppc`. Nunca entre ramas de equipo ni de vuelta a `master`.
- Lo propio de un equipo vive en `targets/`, con las mismas rutas en ambas ramas, detrás de `src/platform.h`. `master`
  no tiene `targets/` y no compila apps Mac, solo pruebas de PC.
- Documentos, skills, agentes y `PERF_LOG.md` se editan solo en `master`. `TARGET.md` es distinto en cada rama y
  `CLAUDE.md` lo importa, para no tener un `CLAUDE.md` por rama.

**Consecuencias.** Cada cambio general se fusiona dos veces. La FAT compila desde dos ramas con worktrees (ADR-012). Un
cambio general solo queda probado en un Mac después de fusionarlo.

**Revisar si** las fusiones se vuelven costosas; entonces, volver a una sola rama con carpetas es directo, porque
`targets/` ya separa el código.

## ADR-008 — Asm antes de medir en el LC 580

**Contexto.** ADR-001 y ADR-004 prohibían escribir asm sin una medición del LC 580. Víctor no quiere esperar esas
lecturas y pidió pasar a asm lo más sensible de la decodificación de cada formato.

**Decisión (de Víctor).** Se permite escribir kernels antes de medir, con estas condiciones obligatorias: referencia en
C, prueba bit a bit, selector C/asm al compilar y marca `sin medir`.

**Consecuencias.** Se puede invertir esfuerzo en kernels que al final no sean los que importan: el orden es una sospecha,
con `decprofile` del emulador como guía débil. El selector permite compararlos en el mismo equipo apenas haya acceso a
él. No cambia la regla de honestidad: nada se declara más rápido sin medición real.

**Revisar cuando** haya mediciones del LC 580: un kernel que no ayuda vuelve a C.

## ADR-009 — Formatos propios: FLAC, Ogg Vorbis y WMA

**Contexto.** Hay que decodificar sin FPU en el 68LC040 y con licencia compatible con GPL.

**Decisión (a confirmar al traer cada biblioteca en la 6-G, leyendo su código y su licencia).**
- **Ogg Vorbis:** Tremor, decodificador entero de Xiph. Sin Opus ni FLAC dentro de Ogg.
- **WMA:** `libwma` de Rockbox (punto fijo, derivada de FFmpeg), v1/v2. Sin Pro, Lossless, Voice ni DRM. Un hilo de
  MPlayer-dev de 2008 reporta ruido audible en algunos flujos: probarla contra `ffmpeg`.
- **FLAC:** elegir tras leer el código; candidatas: dr_flac, libFLAC y la de Rockbox.

**Resolución de FLAC (2026-11-06).** Se eligió **dr_flac 0.13.4** (dominio público o MIT-0) tras leer su código: toda la
decodificación es entera; acepta asignadores propios (se le da un arena, ~36 KB de pico medido en el PC); `onTell` es
opcional; usa `long long` para la caché de bits (en el 68k cuesta llamadas a libgcc, **sin medir**). Trae `float`/`double`
en las rutas de salida `f32` (que no se llaman y desaparecen con `-gc-sections`) y **también en la búsqueda binaria**
(`/8.0f`, un factor `0.6f` y un `double`): eso último sí se ejecuta, y lo descubrió `targets/check-nosoftfloat.sh` sobre el
binario enlazado, no una lectura del código. `tools/prepare-dr-flac.sh` genera una copia con esos cuatro puntos en aritmética
entera; el original no se modifica. La búsqueda sigue exacta (probada con y sin SEEKTABLE, 32 y 64 bits). Límites: hasta 2
canales y de 8 a 48 kHz; 24 bits se reducen a 16 sin redondeo. Las etiquetas y la duración las lee un lector propio, para que
una carátula grande no pase por el arena. Ogg Vorbis y WMA siguen pendientes de leer su código y su licencia.

**Consecuencias.** Más memoria y más código, lo que agrava la regla de 32 KB por segmento. Tremor y WMA pueden asignar
memoria durante la decodificación: hay que revisarlo contra las reglas 6 y 7. Vorbis y WMA pueden no alcanzar tiempo real
en el 68LC040 (sin medir). El código WMA deriva de ingeniería inversa: revisar antes de distribuir binarios.

## ADR-010 — QuickTime como respaldo para los demás formatos

**Contexto.** Víctor quiere soportar AIFF, WAV, SND y otros sin esfuerzo, a través del framework QuickTime.

**Decisión.** Lo que la capa de códecs no reconozca se abre con el Movie Toolbox (`EnterMovies`, `NewMovieFromFile`,
`MoviesTask`...). Los lectores propios de WAV/AIFF se descartan por ahora.

**Hallazgos (verificado en los headers y bibliotecas).** Las Multiversal traen solo 21 funciones de QuickTime, sin
`GetMovieDuration`, `GetMovieTime`, `GetMovieTimeScale`, `SetMovieTimeValue` ni `SetMovieVolume`. `libQuickTimeLib.a` de
PPC las exporta, pero no están declaradas. Hay que escribir las declaraciones y, para 68k, sus selectores del trap
`0xAAAA`, que **no se pueden probar aquí**.

**Consecuencias.**
- QuickTime manda el audio directo al Sound Manager, por su propio canal: **sin ecualizador en ningún equipo**, y hay que
  cerrar nuestro canal mientras se usa.
- Depende de la versión instalada en cada equipo; comprobar con Gestalt y avisar si falta.
- Si abrir archivos que no son películas con `NewMovieFromFile` exige QuickTime 3 o posterior: por verificar.

**Revisar si** se quiere EQ en WAV/AIFF: se agregan lectores propios, que además funcionarían sin QuickTime.

## ADR-011 — Ecualizador de 10 bandas según el formato

**Contexto.** Víctor pidió un ecualizador de 10 bandas igual al de Winamp; antes lo había descartado. Filtrar muestra a
muestra costaría ≈ 267 % de la CPU del 68LC040 (aritmética, `docs/SPECS_68K.md`).

**Decisión (de Víctor).** Por coeficientes para MP3, Vorbis y WMA en ambos equipos; filtros muestra a muestra para FLAC,
solo en `ppc`; sin EQ en QuickTime, con los controles desactivados. Aspecto Platinum, no el de Winamp.

**Bandas.** 70, 180, 320, 600, 1k, 3k, 6k, 12k, 14k y 16k Hz, según indicó Víctor. **Sin fuente primaria de Winamp:** la
documentación de VLC lista 60/170/310 para las tres primeras. Es una tabla: se corrige sin costo si un Winamp real
muestra otra cosa. El rango de ganancia también queda por confirmar (±12 dB como valor inicial; VLC documenta ±20 dB).

**Resolución (2026-11-06).** Implementado en `src/eq.c` (solo enteros) con un gancho en una copia parcheada de libmad
(`third_party/patches/libmad-0.15.1b-eq.patch`; el original no se toca): una llamada por granulo y canal, antes de la alias-reduction en
bloques largos y tras el reordenamiento en cortos y mixtos. Tras `III_reorder` un bloque corto queda como `18·sb + 6·w + k` y su frecuencia
es `f = 6·sb + k`, equivalente a la línea larga `3f+1`: no hacen falta tablas de sfb. Ganancias en Q24 (no Q28: +24 dB es 15,85× y no
cabe), tabla de 193 entradas en cuartos de dB generada por `tools/gen_eq_tables.py` y versionada, interpolación lineal en frecuencia entre
centros con posición en **1/64 de línea** (con 1/16 la pendiente de ±12 dB entre bandas de graves dejaba 0,5 dB de error de posición, lo
descubrió la prueba unitaria), multiplicación saturada. Con el gancho en NULL el PCM es idéntico bit a bit (verificado con los CRC de
`REFERENCE_CODECS.txt` en 32 y 64 bits). **Medido en el PC:** 0,1–0,2 dB contra el modelo en 600 Hz–16 kHz y hasta 1,3 dB en 70/180/320 Hz
(poca resolución). La latencia del anillo de 4 s se resuelve con un colchón objetivo de ~600 ms mientras la ventana está abierta, en vez de
reiniciar la pista (un reinicio en MP3 VBR saltaría de posición). Costo en el LC 580: **sin medir**.

**Consecuencias.** No suena igual que un filtro en el dominio del tiempo: en bloques cortos de MP3 hay menos resolución
en frecuencia. En MP3 hay que parchear una copia de libmad; el árbol `third_party/libmad/` sigue sin modificar. Se
permite `float` solo en `targets/` de la rama `ppc`.

## ADR-012 — FAT armada con Rez desde dos ramas

**Contexto.** Víctor pidió una compilación FAT. Retro68 no la documenta, pero `add_application` muestra los ingredientes:
en 68k, `--copy <app>.code.bin`; en PPC, `RetroPPCAPPL.r` (`cfrg`) y `--data <app>.pef`.

**Decisión.** `tools/build-all.sh` compila los commits de `68k` y `ppc` en worktrees bajo `build/wt/` y combina ambos en
una sola llamada a `Rez`. Solo entran las apps que existan en las dos ramas.

**Consecuencias.** La FAT no ve cambios sin confirmar. Se comprueba la estructura del archivo; **que arranque en cada lado
solo lo prueba Víctor**, primero con `sysprobe`.

## ADR-013 — Logo e ícono con la paleta de 256 colores del sistema

**Contexto.** Víctor pidió un logo parecido a una imagen suya, optimizado a la paleta de 256 colores de Mac OS 8.

**Decisión.** Dirección "Platinum con contorno" (elegida por Víctor). Los íconos de 16 y 32 px se **dibujan a mano**, píxel por píxel,
en `assets/logo/icon*.txt` (colores exactos de la paleta del sistema, sin difuminado); `tools/make_logo.py` los convierte a `icl8`, `ics8`,
`icl4`, `ics4`, `ICN#` e `ics#`, y dibuja por procedimiento solo el logo grande (96×96). Antes se redibujaba todo por procedimiento y a 16 y
32 px salía una mancha sin contorno. Los recursos van en `src/player/icons.r` (generado y versionado). El About dibuja `LOGO` con la
tabla de colores del sistema, no con un `PICT`. `tools/set_bundle_bit.py` activa el bit de bundle porque Retro68 no lo hace.

**Consecuencias.** El orden de índices de la paleta es un **supuesto sin verificar** (documentado en el script y en `docs/UI.md`).
El resultado depende del juicio de Víctor sobre el diseño a 32×32.

**Revisar si** los colores salen mal en el Finder o el ícono no aparece.

## Plantilla para nuevas decisiones

```
## ADR-NNN — Título
Contexto:
Decisión:
Razones:
Consecuencias:
Revisar si:
```
