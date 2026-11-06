---
name: audio-formats
description: Decodificación de FLAC, Ogg Vorbis y WMA, reproducción por QuickTime y ecualizador de 10 bandas en MP368K: bibliotecas candidatas, restricciones de memoria y punto fijo, en qué formato y equipo aplica cada cosa, y cómo probarlo en el PC. Usar al agregar o modificar un códec, la capa de códecs, QuickTime o el ecualizador. Para MP3 ver mp3-fixedpoint-pipeline.
---

# Formatos y ecualizador (etapa 6-G)

**Nada de esto está implementado.** Las decisiones están en ADR-009, ADR-010 y ADR-011, y el pedido en `docs/ETAPA6.md`. Lo que
se dice de cada biblioteca hay que **confirmarlo leyendo su código y su licencia** antes de traerla a `third_party/`.

## Reglas al traer una biblioteca
- Sin `float` ni `double` en el camino de decodificación: compila también para el 68LC040.
- Conservar sus avisos de copyright y comprobar que su licencia sea compatible con GPLv3.
- Suministrar la memoria en la inicialización. Revisar cada `malloc` y `alloca`, y el uso de pila: la pila de una app clásica es
  pequeña y Retro68 no la agranda (`macenv_grow_stack`).
- Mismo PCM en 68k, PPC y PC: comparar con CRC, como `decbench` contra `REFERENCE.txt`.
- El código compartido va en `master`, y solo lo específico de un equipo, en `targets/`.

## Capa de códecs
Generalizar lo que hoy es `dec_stream_*` (`src/decoder_core.h`): abrir, siguiente bloque de PCM, buscar y cerrar, con los mismos
`DecReadFn`/`DecSeekFn`. `player.c` deja de hablar con libmad directamente. El formato se detecta por contenido, no por la
extensión ni por el tipo de archivo: los archivos traídos de un PC no suelen traer tipo.

## Formatos
| Formato | Biblioteca | Notas |
|---|---|---|
| Ogg Vorbis | Tremor (entero) | sin Opus ni FLAC en Ogg. Tablas por archivo: revisar memoria |
| FLAC | a elegir: dr_flac, libFLAC o la de Rockbox | sin pérdida: se puede comprobar bit a bit |
| WMA v1/v2 | `libwma` de Rockbox | sin Pro, Lossless, Voice ni DRM. Requiere leer ASF. Hay reportes de ruido en algunos flujos: probar contra `ffmpeg` |

`HALF` y `MONO` solo existen para MP3. Definir sus equivalentes, o que la app informe si no alcanza tiempo real.

## QuickTime (ADR-010)
- Las Multiversal traen solo lo básico (`EnterMovies`, `ExitMovies`, `MoviesTask`, `StartMovie`, `StopMovie`, `IsMovieDone`,
  `OpenMovieFile`, `CloseMovieFile`, `NewMovieFromFile`, `DisposeMovie`...), en `defs/QuickTime.yaml`.
- **Faltan** `GetMovieDuration`, `GetMovieTime`, `GetMovieTimeScale`, `SetMovieTimeValue` y `SetMovieVolume`. PPC: declararlas,
  porque `libQuickTimeLib.a` las exporta. 68k: declararlas con el trap `0xAAAA` y el selector correcto; **no se pueden probar aquí**,
  así que deben venir de una fuente confiable.
- `MoviesTask` se llama desde el bucle principal. Abre su propio canal: cerrar el nuestro mientras se usa.
- Comprobar con Gestalt que QuickTime esté y avisar si falta.

## Ecualizador (ADR-011)
Bandas: 70, 180, 320, 600, 1k, 3k, 6k, 12k, 14k y 16k Hz, más preamp y On. Sin fuente primaria de Winamp para las tres primeras.

| Formato | 68k | PPC | Cómo |
|---|---|---|---|
| MP3 | sí | sí | ganancia por línea de frecuencia, antes de la IMDCT, con un parche sobre una copia de libmad |
| Vorbis, WMA | sí | sí | ganancia sobre los coeficientes, antes de la IMDCT |
| FLAC | no | sí | filtros muestra a muestra en `targets/eq_iir.c`, solo PPC |
| QuickTime | no | no | el audio no pasa por la app; controles desactivados |

- Por qué no filtros en 68k: costarían ≈ 267 % de la CPU a 44,1 kHz estéreo (`docs/SPECS_68K.md`, aritmética, sin medir).
- La frecuencia de cada línea depende de la tasa y del tamaño de bloque: recalcular la tabla al cambiar cualquiera de ellos,
  **fuera** del camino caliente y sin `float`. Interpolar entre bandas para no crear escalones.
- **Prueba clave:** con el EQ en plano, el PCM debe ser idéntico bit a bit a la referencia. Con una banda activa, comparar la
  respuesta en frecuencia en el PC con barridos de tono.

## Archivos de prueba
`ffmpeg` del host codifica FLAC, Vorbis, WMAv1 y WMAv2; `flac` y `oggenc` también están instalados. Ampliar `tools/make_testfiles.sh`.
FLAC se compara bit a bit; Vorbis y WMA, por SNR contra `ffmpeg`.
