# Etapa 6 — Tres solicitudes

Víctor la pide en tres solicitudes separadas. **Orden recomendado: 6-PPC → 6-G → 6-68K.** Así lo general se prueba en
los dos equipos desde el inicio y el asm optimiza el código ya definitivo.

**Estado (2026-11-07), rama `68k`:** hechos y probados en el PC y bajo QEMU, **sin ver en un Mac ni medir en el LC 580**: capa de códecs, FLAC,
ecualizador del MP3, ícono, **Ogg Vorbis (Tremor), WMA v1/v2 (libwma) y los núcleos en asm 68040 del MP3** (ADR-009, ADR-014). Pendientes:
QuickTime, el ecualizador de Vorbis/WMA, el asm de Vorbis/WMA, y todo lo de PPC.

## 6-PPC — Port al 601 (rama `ppc`)

- **Pedido:** versión PPC en C, sin ensamblador, pensada para el equipo con 601.
- **Entregables:** las apps de `docs/SPECS_PPC.md` compiladas para PowerPC; `mp368k` en `dist/ppc` y en `dist/fat`.
- **Ya sabido del primer `sysprobe` PPC** (`docs/HARDWARE_PPC601.md`): el entorno solo ofrecía 22 050 Hz, 16 bits, mono, así
  que `audio_open` debe negociar **tasa y canales**, no solo profundidad, y mezclar o remuestrear si hace falta. Revisar
  también los retornos de `struct` por valor (ADR-005).
- **Salida:** los criterios de `docs/SPECS_PPC.md`.

## 6-G — Proyecto conjunto (rama `master`, luego se fusiona a ambas)

- **Pedido:** FLAC, OGG y WMA; QuickTime para otros audios (AIFF, WAV, SND...); ecualizador de 10 bandas de 70 Hz a
  16 kHz; compilaciones 68k, PPC y FAT; logo en la paleta de 256 colores de Mac OS 8.

| Pieza | Decisión | Nota |
|---|---|---|
| Capa de códecs | interfaz común sobre lo que hoy es `dec_stream_*` | `player.c` deja de llamar a libmad directamente |
| Ogg Vorbis | Tremor (entero), rama lowmem | ADR-009. **Hecho** (PC y QEMU): heap propio, ~37 KB; PCM a 1 LSB de ffmpeg |
| FLAC | dr_flac | ADR-009. **Hecho** |
| WMA | `libwma` de Rockbox, v1/v2 | ADR-009. **Hecho** (PC y QEMU): lector ASF propio; PCM a 1 LSB de ffmpeg. Sin Pro, Lossless, Voice ni DRM |
| QuickTime | Movie Toolbox, como respaldo para lo no reconocido | ADR-010. Hecho con lo ya declarado (sin selectores nuevos); duración por cabecera |
| Ecualizador | por coeficientes en lo compartido | ADR-011 |
| Logo | `tools/make_logo.py` genera `icl8`, `ics8`, `ICN#`, `ics#`, `BNDL` y `FREF`, y una versión mayor para About | Paleta del sistema de 256 colores. A 32×32 los anillos probablemente haya que redibujarlos, no reducirlos |
| Compilaciones | `tools/build-all.sh` ya hace las tres | la FAT de `mp368k` necesita la 6-PPC |

- **Ecualizador en detalle:** bandas 70, 180, 320, 600, 1k, 3k, 6k, 12k, 14k y 16k; preamp y On; sin presets. En MP3 se
  aplica a las 576 líneas de frecuencia (~38 Hz cada una en bloques largos), antes de la IMDCT, lo que exige parchear
  una copia de libmad como ya se hace con el perfilado: las subbandas miden ~689 Hz a 44,1 kHz y no separan las bandas
  graves. Tablas de ganancia precalculadas, sin `float`.
- **Disponibilidad del EQ:**

| Formato | 68k | PPC |
|---|---|---|
| MP3, Vorbis, WMA | por coeficientes | por coeficientes |
| FLAC | no | filtros muestra a muestra |
| QuickTime (AIFF, WAV, SND...) | no | no |

- **Entregables:** pruebas de PC para cada formato y para el EQ; `docs/UI.md` actualizado.
- **Salida:** FLAC bit a bit contra el original; Vorbis y WMA con el SNR acordado contra `ffmpeg`; las tres
  compilaciones generadas; en el emulador, cada formato suena y el EQ se oye o queda desactivado según la tabla.

## 6-68K — Asm y Sound Manager (rama `68k`)

- **Pedido:** pasar a ensamblador 68k lo más sensible de la decodificación de cada formato, para el 68LC040, buscando
  el máximo provecho del Sound Manager.
- **Entregables y salida:** los de `docs/SPECS_68K.md`. **La velocidad queda `sin medir`** hasta correr en el LC 580.
- **Depende de** la 6-G: el asm se escribe sobre los códecs ya elegidos.

## Avance de la versión 68k (plan "Terminar la versión 68k")

| Fase | Estado |
|---|---|
| 0 — Cimientos: archivos de prueba, `segtest`, chequeo de float por software, tamaños de segmento | hecha; `segtest` corrió en Infinite Mac: OK |
| 1 — Logo e ícono, ventana About | rediseñada (Platinum con contorno, dibujada a mano, con `icl4`/`ics4`); compilada; **falta que Víctor apruebe la hoja de contacto y la vea en el Finder** |
| 2 — Capa de códecs y FLAC | hecha; **`codecbench` en Infinite Mac: los 7 CRC coinciden con el PC**; falta oír un FLAC en `mp368k` |
| 3 — Ecualizador del MP3 | DSP y ventana hechos; pruebas de PC OK (32 y 64 bits); **falta verlo y oírlo en Infinite Mac** |
| 4 — Ogg Vorbis | pendiente |
| 5 — WMA | pendiente |
| 6 — QuickTime (con app de prueba previa) | `qttest` corrió en Infinite Mac (QuickTime 2.5: abre WAV, AIFF, AIFC y AU; no CAF, M4A ni WAV mu-law). Integrado en el reproductor (ADR-010), sin búsqueda ni volumen; **falta que Víctor oiga uno** |
| 7 — Asm 68040 | pendiente |
| 8 — Cierre | pendiente |

## Lo que ya quedó listo (scaffolding)

Ramas, `targets/`, `tools/build.sh`, `tools/build-all.sh`, toolchain con PowerPC y una FAT de prueba de `sysprobe`.
