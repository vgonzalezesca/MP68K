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

**Decisión (confirmada al traer cada biblioteca, leyendo su código y su licencia: FLAC, Vorbis y WMA abajo).**
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
una carátula grande no pase por el arena.

**Resolución de Ogg Vorbis (2026-11-07).** **Tremor, rama `lowmem`** (commit `89a7534` de `gitlab.xiph.org/xiph/tremor`), no la rama
principal: trae su propio `ogg.h`, `bitwise.c` y `framing.c` (la principal exige libogg) y usa menos memoria. **Licencia BSD de 3
cláusulas** (leída en su `COPYING`: "Copyright (c) 2002, Xiph.org Foundation"), compatible con GPLv3; el reporte de "GPLv2 en un paquete
de Fedora" no aplica al código de Xiph. No tiene `float` ni `double` (verificado con `grep` y con `targets/check-nosoftfloat.sh` sobre
el binario). Todas sus asignaciones pasan por las macros `_ogg_malloc` y compañía: se redirigen al heap del códec
(`third_party/patches/tremor-lowmem-mp368k.patch`, que además sustituye las multiplicaciones de 64 bits, que dependían de
`BYTE_ORDER`, por desplazamientos de `int64`; `third_party/tremor/` no se modifica). Como Tremor libera en orden arbitrario, el arena
(que solo libera lo último) no sirve: `src/heap.c` es un asignador de primer ajuste con fusión, probado con 200 000 operaciones al azar
en 32 y 64 bits. Tremor no revisa todas sus asignaciones, así que si el heap se agota dentro de una llamada se sale con `longjmp` y el
códec informa `CODEC_ERR_MEMORY` (probado con heaps de 17 a 64 KB: nunca se cae). El reconocimiento, las etiquetas y la duración los lee
un lector de páginas Ogg propio, sin Tremor, para no tocar la memoria del decodificador al agregar una pista a la lista. **Medido en el PC:**
PCM a **1 LSB de `ffmpeg`** (error rms 0,71 LSB, SNR ≈ 80 dB, estéreo y mono), **mismos bytes en 32 y 64 bits**, y el mismo CRC corriendo
como 68040 big-endian bajo QEMU y como x86 little-endian; heap de Tremor de ~37 KB en el archivo de prueba (q4, ~90 kbps). Límites: hasta
2 canales y de 8 a 48 kHz; solo el primer flujo si hay varios encadenados. **Tiempo real en el LC 580: sin medir** (puede no alcanzar).

**Resolución de WMA (2026-11-07).** **`libwma` de Rockbox** (commit `a13aaaf`, espejo en GitHub de `git.rockbox.org`) con las piezas de su
`codeclib` que necesita (bits y VLC de FFmpeg, MDCT, FFT). **Licencias leídas por archivo:** los derivados de FFmpeg son LGPL 2/2.1 "o
posterior" y los de Rockbox (`wmafixed`, `mdct_lookup`) GPL 2 "o posterior": todo es compatible con la GPLv3 (detalle en
`third_party/libwma/README.mp368k`). **Tenía un `float` en el camino caliente** (`pow_m1_4`, un truco de exponente y mantisa que corre en
cada bloque con exponentes LSP): `third_party/patches/libwma-mp368k.patch` lo reemplaza por enteros replicando el redondeo de IEEE;
**verificado idéntico byte a byte al original con float**. No usa `malloc` (arreglos estáticos, ~150 KB de datos); se le dieron cabeceras de
reemplazo mínimas del entorno de Rockbox (`third_party/patches/libwma_shim/`). El contenedor lo lee `src/asfparse.c` (propio, a partir de
la especificación y del enfoque de libasf): cabecera, etiquetas, paquetes de longitud variable y objetos partidos entre paquetes; la
búsqueda es por hora de envío con un paquete de margen y salto exacto con la hora de presentación del primer objeto (error < 1 ms).
**Medido en el PC contra `ffmpeg`, en 5 formatos** (v2 estéreo 44,1/48 kHz, mono 22,05/16 kHz, v1 44,1 kHz): **1 LSB de error máximo**,
mismos bytes en 32 y 64 bits y el mismo CRC como 68040 big-endian bajo QEMU. Dos particularidades de la biblioteca, medidas: la **polaridad
de su salida está invertida** (inaudible; se corrige) y los **2 primeros cuadros de MDCT son arranque** del decodificador (ffmpeg los
descarta; el códec también). **WMA v1 con exponentes LSP** (p. ej. v1 a 32 kHz y 96 kbps): la forma de onda es idéntica (correlación
1,0000) pero la amplitud sale ~0,2 dB baja (SNR 31 dB); es de las tablas de baja precisión de la biblioteca, no del parche (la versión
original con float da exactamente lo mismo). El hilo de MPlayer-dev que reportaba ruido **no se reprodujo** con archivos de `ffmpeg`. Límites:
solo v1 y v2 (Pro, Voice, Lossless y DRM se rechazan con "formato no soportado"), hasta 2 canales y de 8 a 48 kHz. **Tiempo real en el LC
580: sin medir** (puede no alcanzar).

**Consecuencias.** Más memoria y más código, lo que agrava la regla de 32 KB por segmento. Tremor y WMA pueden asignar
memoria durante la decodificación: se resolvió con un heap dentro del arena (regla 6). Vorbis y WMA pueden no alcanzar tiempo real
en el 68LC040 (sin medir). El código WMA deriva de ingeniería inversa: revisar antes de distribuir binarios.

## ADR-010 — QuickTime como respaldo para los demás formatos

**Contexto.** Víctor quiere soportar AIFF, WAV, SND y otros sin esfuerzo, a través del framework QuickTime.

**Decisión.** Lo que la capa de códecs no reconozca se abre con el Movie Toolbox (`EnterMovies`, `NewMovieFromFile`,
`MoviesTask`...). Los lectores propios de WAV/AIFF se descartan por ahora (solo se lee la cabecera para la duración; ver el resultado de `qttest` abajo).

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

**Resultado de `qttest` (2026-11-09, Infinite Mac, Mac OS 8.1; `targets/qttest.c`).** `Gestalt('qtim')` = 0x02508000: **QuickTime 2.5**. Con `OpenMovieFile`
+ `NewMovieFromFile` (funciones ya declaradas) **abrieron y avanzaron con `MoviesTask`: WAV PCM, AIFF, AIFC y AU**. **No abrieron:** CAF (error -2048), M4A/AAC
(-32766) y WAV mu-law de 8 kHz (-2048); el significado exacto de los códigos está por verificar. `MoviesTask(movie, 0)` cuesta ~55 µs, cabe en el bucle del
reproductor. **Sin confirmar:** que el audio haya sonado (el programa no lo oye) ni nada en el LC 580. Para abrir no hizo falta ningún selector nuevo.

**Implementación (sin selectores nuevos; nada se inventa).** `src/qtfmt.{h,c}` reconoce WAV, AIFF/AIFC y AU por contenido y saca la **duración** (y tasa, canales y bits) de la
cabecera, probado contra ffprobe en 64 y 32 bits; `src/qtplay.h` es la interfaz y `targets/qtplay.c` (solo 68k) la implementa con las 12 funciones ya declaradas
(`EnterMovies`, `OpenMovieFile`, `NewMovieFromFile`, `CloseMovieFile`, `PrerollMovie`, `StartMovie`, `StopMovie`, `GoToBeginningOfMovie`, `MoviesTask`, `IsMovieDone`,
`DisposeMovie`, `ExitMovies`); sin ese archivo, `qtplay.h` deja reemplazos que dicen "QuickTime no está instalado". En `player.c` el modo QuickTime cierra nuestro canal
de audio, llama a `MoviesTask` desde `player_idle` y da la pista por terminada con `IsMovieDone` o al pasar 1,5 s de la duración. Límites asumidos, hasta tener selectores
con fuente: la **posición es una estimación** con `TickCount` (no `GetMovieTime`), la **búsqueda solo vuelve al comienzo** y el **volumen no actúa**; tampoco hay balance,
calidad, salida 8/16 ni ecualizador. Sin QuickTime (Gestalt falla) la app dice "QuickTime is not installed". Si Víctor aporta `Movies.h` de Apple (fuera de git),
los selectores que falten se agregan anotando su fuente y se activa búsqueda y volumen.

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

## ADR-014 — Núcleos en ensamblador 68040, probados bajo QEMU

**Contexto.** Víctor pidió pasar a asm lo crítico de la decodificación (ADR-008: referencia en C, prueba bit a bit, selector C/asm,
"sin medir"). El 68040 real solo lo ve Víctor, así que hacía falta una forma de probar la **corrección** aquí.

**Decisión.** Un banco bajo `qemu-m68k -cpu m68040` (`targets/asm-test.sh`): ELF estático autónomo, compilado con el GCC de Retro68 y
enlazado con su `ld` (emulación `m68kelf`), con un `_start` propio y llamadas al sistema de Linux por `trap #0`; no se instaló ningún
cross-compilador. Cada núcleo se compara bit a bit con su referencia en C con vectores aleatorios y de borde, y se comprueba que
preserve `d2-d7/a2-a6`. Además, decodificaciones **completas** corren como 68040 big-endian y su CRC se compara con el del PC.
El selector es `-DMP368K_ASM=ON|OFF`; con asm activo se arma también `codecbench_ref` con la libmad original para compararlos en el mismo equipo.

**Núcleos hechos (MP3).** Todos reproducen **exactamente** la aritmética de libmad con `FPM_64BIT`: cada producto es `(int32)((int64)x*y >> n)`,
truncado, y las sumas son de 32 bits con envoltura. **`n` es 16, no 28, en la ventana de síntesis:** en `synth.c`, sin `OPT_SSO`, la tabla D va
pre-desplazada 12 bits y `MAD_F_SCALEBITS` pasa a 28 − 12 (el DCT32 y la IMDCT usan 28); una primera referencia con 28 dio otro PCM y la
prueba lo delató. Acumular en 64 bits y desplazar una sola vez sería más rápido pero cambiaría el PCM en el último bit: **no se hizo** y
queda como decisión futura de Víctor.
- `targets/asm/synth_window_040.s` (a mano): la ventana de la síntesis completa (de `synth_full`). Acumula por separado la parte alta y
  `lo >>> 16` de cada `muls.l` y las junta al final, lo que ahorra el armado de cada producto de 64 bits.
- `targets/asm/dct32_040.s` y `imdct_l_040.s` (**generados** por `targets/asm/gen/dflow.py`): el DCT32 y la IMDCT de bloques largos normales
  (con ventana, solapado e inversión de frecuencias). Se parte del texto de libmad: las sentencias del `dct32` se leen de `synth.c`, y el
  algoritmo de la IMDCT se transcribe al DSL del generador con las constantes leídas de `layer3.c`; la referencia en C (`src/dct32_ref.c`,
  `src/imdct_ref.c`) sale del mismo texto. Asignación de registros con derrame a la pila, sin trabajo a mano.
- `targets/asm/pcm_040.s` (a mano): conversión de la salida de la síntesis a 16 bits, a 8 bits y ganancia.
Los bloques de inicio, fin, cortos y mixtos, y la IMDCT corta, siguen en C (se llaman al núcleo solo en el caso común).

**Verificación (todo en el PC, bajo QEMU).** 48 064 + 32 032 + 12 008 casos aleatorios contra las referencias; **mutantes** (una constante
cambiada en 1, un desplazamiento, un signo) hacen fallar cada prueba; síntesis completa de 4 MP3 (estéreo, mono y transitorios, **en perfil
completo y media tasa**) con el mismo CRC que la libmad original del PC. Tremor y WMA corren como 68040 big-endian con el mismo CRC que en x86.

**Lo que NO se hizo, y por qué.**
- **FLAC en asm: descartado.** Se compiló la predicción LPC con el GCC de Retro68 y sale justo lo que se escribiría a mano (tabla de saltos y
  `move.l`, `muls.l` con memoria, `add.l` por coeficiente): no hay ganancia que justifique el riesgo. Reemplazar el conteo de ceros de dr_flac por
  `bfffo` podría ser **más lento** que su tabla en el caso habitual de códigos Rice cortos: solo se decide midiendo en el LC 580.
- **Vorbis y WMA en asm: pendiente.** Tremor y libwma corren en C. Sus candidatos son la IMDCT (FFT con bucles de tamaño variable, que el
  generador de código recto no cubre) y el ecualizador por coeficientes; se decide con las mediciones de `codecbench`.
- Huffman, requantización y la IMDCT corta del MP3: segunda tanda.

**Ronda 2 (2026-11-08): medir primero.** Víctor entregó el bench de `codecbench` en Infinite Mac: los 4 CRC de MP3 con los núcleos asm coinciden con el PC (primera
confirmación en una build real de Mac OS 8.1 sobre un 68040 emulado); los tiempos son del emulador y no se interpretan (`docs/PERF_LOG.md`). Como no había ningún perfil
del 68k de Vorbis, WMA ni del EQ, se hizo uno: **`targets/asm-prof.sh`** corre los decodificadores bajo QEMU con `-d in_asm,exec,nochain` (sin plugins instalados) y cuenta
**instrucciones dinámicas por función**, en costo marginal (corrida larga menos corta) y sin el arnés de prueba. **Es un conteo de instrucciones, no tiempo.**
- **Compuerta (para no repetir el caso de FLAC):** una función entra a asm solo si pesa ≥ ~8 % de su formato y un diseño concreto quita ≥ ~25 % de las instrucciones de
  su bucle frente a lo que emite GCC. **Pasaron:** el kernel del ecualizador (GCC gastaba 33 instrucciones por línea; el asm, ~17 en promedio con líneas sin ganancia:
  67 → 35 por cuadro), la conversión de salida de WMA (35 → 20), y la **ventana de la síntesis de media tasa** (`synth_half`, 162 → 89, −45 %), que resultó ser lo más pesado
  del perfil HALF, el que probablemente use el LC 580 (`targets/asm/eq_040.s`, `pcm_040.s`, `synth_window_half_040.s`; todos con referencia en C, prueba bit a bit bajo QEMU,
  **mutantes** con `targets/asm-mutate.sh` y e2e como 68040 big-endian). **No pasaron:** `III_aliasreduce` (con 2 productos por salida el truco de acumuladores partidos casi
  no ahorra, ~−20 %); `mdct_unroll_lap` de Tremor (~−21 %); y las etapas de la MDCT de Tremor tomadas por separado (`XPROD31` ahorra una instrucción de ~9; ~−11 %).
  Quedan sin tocar la MDCT de Tremor (38 % de Vorbis), la FFT y la IMDCT de libwma (≈ 40 % de WMA) y la lectura de bits de Huffman: son los candidatos que tendrían
  más peso, pero cada uno exige una reescritura grande de bucles con muchos casos (tamaños de bloque) por un ahorro estimado menor del 30 % de su parte.
- **Resultado (instrucciones por cuadro estéreo, `docs/PERF_LOG.md`):** MP3 FULL 605 → 431 (−29 %); MP3 HALF 457 → 341 (−25 %); WMA 380 → 365; MP3 con EQ 498 → 467.
- **Lo que dicen estos números, con sus límites.** Los decodificadores de Vorbis y WMA estéreo gastan ~365-475 instrucciones por cuadro y el MP3 FULL ~431, contra un
  presupuesto de 748 ciclos por cuadro a 44,1 kHz y 33 MHz, y hacen 25-50 multiplicaciones `muls.l` por cuadro. **Si `muls.l` cuesta ~20 ciclos en el 68LC040, lo
  que dominaría son las multiplicaciones y varios de estos casos no llegarían a tiempo real; si cuesta ~5, quedarían al borde.** Ese dato **no lo sé**: la búsqueda
  en la web no dio la tabla del M68040UM y el `microbench` solo se ha corrido en el emulador. **Lo primero que hay que medir en el LC 580 es `microbench`** (Etapa 1 de
  `docs/PERF_LOG.md`): con eso el conteo se convierte en una estimación de ciclos creíble y se sabe si más asm sirve o si hace falta reducir trabajo (menos tasa, mono).
- **Perfil rápido del MP3 (acumular en 64 bits y desplazar una vez): diferido.** Víctor lo aprobó con la premisa de que sería "bastante más rápido". Con los números, la
  ventana pasaría de 5 a 4 instrucciones por producto (−20 % de la ventana, ~−8 % del MP3) y **no reduce las multiplicaciones**, que serían lo que domina: el beneficio de
  velocidad sería pequeño y a cambio agrega un modo en la interfaz, en la escalera de Automático y en las pruebas. Se retoma si el `microbench` del LC 580 muestra que
  `muls.l` es barato (entonces el conteo de instrucciones sí importa), o si Víctor lo quiere por la exactitud (el PCM sería más preciso, no menos).

**Consecuencias.** El código crece (en `mp368k`, de 197 a 283 KB de CODE con Vorbis y WMA; el segmento Main supera el 226 KB que `segtest`
probó en Infinite Mac). Los núcleos generados se regeneran con `targets/asm/gen/gen_*.py` si cambia libmad. **Sin medir en el LC 580:
ningún núcleo se declara más rápido que la referencia.** QEMU no modela tiempos ni cachés: solo prueba corrección.

**Revisar cuando** haya mediciones: un núcleo que no ayuda vuelve a C (el selector lo permite).

## Plantilla para nuevas decisiones

```
## ADR-NNN — Título
Contexto:
Decisión:
Razones:
Consecuencias:
Revisar si:
```
