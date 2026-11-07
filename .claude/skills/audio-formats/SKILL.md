---
name: audio-formats
description: Decodificación de FLAC, Ogg Vorbis y WMA, reproducción por QuickTime y ecualizador de 10 bandas en MP368K: bibliotecas candidatas, restricciones de memoria y punto fijo, en qué formato y equipo aplica cada cosa, y cómo probarlo en el PC. Usar al agregar o modificar un códec, la capa de códecs, QuickTime o el ecualizador. Para MP3 ver mp3-fixedpoint-pipeline.
---

# Formatos y ecualizador (etapa 6-G)

**Hecho: la capa de códecs y FLAC (validados en el emulador con `codecbench`), el ecualizador del MP3, Ogg Vorbis (Tremor) y WMA (libwma) (los tres últimos probados en el PC y bajo QEMU, no en un Mac). Pendiente: QuickTime y el EQ de Vorbis/WMA.** Las decisiones están en ADR-009, ADR-010 y ADR-011, y el pedido en `docs/ETAPA6.md`. Lo que
se dice de cada biblioteca hay que **confirmarlo leyendo su código y su licencia** antes de traerla a `third_party/`.

## Reglas al traer una biblioteca
- Sin `float` ni `double` en el camino de decodificación: compila también para el 68LC040.
- Conservar sus avisos de copyright y comprobar que su licencia sea compatible con GPLv3.
- Suministrar la memoria en la inicialización. Revisar cada `malloc` y `alloca`, y el uso de pila: la pila de una app clásica es
  pequeña y Retro68 no la agranda (`macenv_grow_stack`).
- Mismo PCM en 68k, PPC y PC: comparar con CRC, como `decbench` contra `REFERENCE.txt`.
- El código compartido va en `master`, y solo lo específico de un equipo, en `targets/`.

## Capa de códecs (hecha)
`src/codec.h`: `CodecIO` (callbacks de lectura/posición + tamaño), `CodecInfo` (formato, tasa, canales, duración, etiquetas,
`has_profiles`, `eq_capable`, `priv` para pasar estado de probe a open) y `Codec` (`sniff`, `probe`, `open`, `next`, `seek`,
`close`). Los códecs entregan rebanadas de ≤ 1152 muestras por canal (`CODEC_MAX_FRAMES`) aunque su bloque nativo sea mayor.
`codec_probe_any` reconoce por contenido (el MP3 va último: su reconocimiento es el más débil). La memoria de los códecs de
terceros sale de un arena (`src/arena.c`) que reserva la app. **Para agregar un formato:** escribir `codec_<x>.c`, registrarlo
en `src/codec.c` (`kCodecs` y las extensiones de `codec_name_matches`), agregarlo a `tools/host-build-codecs.sh` y a
`tests/host/check_codecs.py`, y a `bench/codecbench.c` con su referencia en `tools/host-codec-ref.sh`.
**Pruebas:** siempre en 32 y 64 bits (el 68k es de 32; dr_flac cambia de camino según el tamaño del puntero).
**Cuidado con el float:** un `grep` de `float|double` no alcanza (se cortó en 30 líneas y se escapó la búsqueda de dr_flac);
lo que vale es `targets/check-nosoftfloat.sh` sobre el binario enlazado. El formato se detecta por contenido, no por la
extensión ni por el tipo de archivo: los archivos traídos de un PC no suelen traer tipo.

## Formatos
| Formato | Biblioteca | Notas |
|---|---|---|
| Ogg Vorbis | Tremor, rama **lowmem** (`third_party/tremor/`, BSD 3 cláusulas) | su propio ogg.h/bitwise/framing, sin libogg; heap propio (`src/heap.c`, ~37 KB); sin Opus ni FLAC en Ogg. Parche en `third_party/patches/tremor-lowmem-mp368k.patch`, copia con `tools/prepare-tremor.sh` |
| FLAC | dr_flac (`third_party/dr_libs/`, `tools/prepare-dr-flac.sh`) | sin pérdida: se compara bit a bit con `flac -d` |
| WMA v1/v2 | `libwma` de Rockbox (`third_party/libwma/`, LGPL 2.1+ y GPL 2+) + `src/asfparse.c` propio | sin Pro, Lossless, Voice ni DRM. El único float se reemplazó (`libwma-mp368k.patch`); salida con polaridad invertida y 2 cuadros de arranque (se corrigen). Copia con `tools/prepare-libwma.sh` |

**Cómo se prueba** (`tests/host/check_codecs.py`, 32 y 64 bits): PCM contra `ffmpeg` (admite 1 LSB: Tremor y libwma son enteros, ffmpeg usa float), mismos
bytes en 32 y 64 bits, búsqueda, archivos dañados/truncados/basura, límites declarados, arena chico. Bajo `qemu-m68k` (big-endian), `targets/asm-test.sh vorbis_e2e wma_e2e`
compara el CRC del PCM con el del PC. **WMA v1 con exponentes LSP** tiene ~0,2 dB menos de amplitud que ffmpeg (propio de la biblioteca; ADR-009).

`HALF` y `MONO` solo existen para MP3. Para los demás, la app informa si no alcanza tiempo real ("May not play in real time").

## QuickTime (ADR-010)
- Las Multiversal traen solo lo básico (`EnterMovies`, `ExitMovies`, `MoviesTask`, `StartMovie`, `StopMovie`, `IsMovieDone`,
  `OpenMovieFile`, `CloseMovieFile`, `NewMovieFromFile`, `DisposeMovie`...), en `defs/QuickTime.yaml`.
- **Faltan** `GetMovieDuration`, `GetMovieTime`, `GetMovieTimeScale`, `SetMovieTimeValue` y `SetMovieVolume`. PPC: declararlas,
  porque `libQuickTimeLib.a` las exporta. 68k: declararlas con el trap `0xAAAA` y el selector correcto; **no se pueden probar aquí**,
  así que deben venir de una fuente confiable.
- `MoviesTask` se llama desde el bucle principal. Abre su propio canal: cerrar el nuestro mientras se usa.
- Comprobar con Gestalt que QuickTime esté y avisar si falta.

## Ecualizador (ADR-011) — hecho para MP3
Bandas: 70, 180, 320, 600, 1k, 3k, 6k, 12k, 14k y 16k Hz, más preamp y On (por confirmar contra un Winamp real).

| Formato | 68k | PPC | Cómo |
|---|---|---|---|
| MP3 | **hecho** | sí | gancho `mp368k_eq_hook` en una copia parcheada de libmad, antes de la alias-reduction/IMDCT |
| Vorbis, WMA | **pendiente** | pendiente | ganancia sobre los coeficientes, antes de la IMDCT, con la misma tabla (`src/eq.c`) |
| FLAC | no | sí | filtros muestra a muestra en `targets/eq_iir.c`, solo PPC |
| QuickTime | no | no | el audio no pasa por la app; controles desactivados |

- **Código:** `src/eq.{h,c}` (solo enteros, Q24, multiplicación saturada), `src/eq_tables.h` (generada por `tools/gen_eq_tables.py`),
  `third_party/patches/libmad-0.15.1b-eq.patch` + `mp368k_eq.h`, `tools/prepare-libmad-eq.sh`. La copia parcheada se enlaza como `mad_eq` y
  `decoder_core_eq` y la usa `codecs`; `decbench`, `decprofile` y `audiotest` siguen con la libmad original.
- **Mapeo de frecuencias:** línea larga `i` ↔ `(i+0,5)·(fs/2)/576`. Tras `III_reorder`, un bloque corto queda `18·sb + 6·w + k` y su frecuencia
  `f = 6·sb + k` equivale a la línea larga `3f+1`. En un bloque mixto, los primeros 36 coeficientes son largos. Posición de los centros en
  **1/64 de línea**: con 1/16 la pendiente entre bandas de graves (~8 dB por línea) daba 0,5 dB de error.
- **Latencia:** el anillo de 4 s retrasaría los cambios; con la ventana abierta el motor deja ~600 ms por delante (`player_set_fill_target_ms`).
- **Pruebas:** `test_eq` (modelo independiente, saturación, cambio de tasa) y `check_eq.py` (respuesta sobre ruido blanco a 44,1/48/32 kHz y mono,
  bloques cortos con `testtransient.mp3`, 32 == 64 bits, y EQ plano == referencia). Con el gancho en NULL el PCM es idéntico bit a bit.
- Por qué no filtros en 68k: costarían ≈ 267 % de la CPU a 44,1 kHz estéreo (`docs/SPECS_68K.md`, aritmética, sin medir).

## Archivos de prueba
`tools/make_testfiles.sh` genera FLAC, Ogg Vorbis (`oggenc -s 368`), WMA v1/v2 (`ffmpeg`), WAV y AIFF. FLAC se compara bit a bit; Vorbis y WMA contra `ffmpeg`.
