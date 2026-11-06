# SPECS — MP368K

Estado: borrador inicial. Todo número de rendimiento aquí es **estimación sin medir**
salvo que diga lo contrario. Se reemplaza por datos reales en `docs/PERF_LOG.md`.

## 1. Objetivo

Reproductor de audio para Mac OS clásico, en dos bifurcaciones y tres compilaciones:

| Compilación | Rama | Equipo de referencia | Lenguaje |
|---|---|---|---|
| 68k | `68k` | Macintosh LC 580 (68LC040, sin FPU) | C + ensamblador 68040 en los kernels |
| PPC | `ppc` | Power Macintosh con PowerPC 601 | solo C |
| FAT | ambas | los dos | un solo archivo con 68k y PPC |

**Estado real hoy:** el reproductor de **MP3 en 68k** (probado en emulador, nada medido en un equipo real) y, compilados y probados
solo en el PC, la capa de códecs, **FLAC** y el **ecualizador del MP3** (FLAC se validó en el emulador con `codecbench`; falta oír ambos).
PPC compila `sysprobe` y nada más. **Ogg Vorbis, WMA y QuickTime no existen todavía**: son la etapa 6
(`docs/ETAPA6.md`). El logo está compilado, sin verlo en pantalla. Lo que sigue es el **objetivo**, no lo que hay.

Formatos propios (objetivo): MP3 (hasta 192 kbps estéreo como meta en el LC 580), FLAC, Ogg Vorbis y WMA.
Lo demás (AIFF, WAV, SND...) a través de QuickTime. Con ecualizador de 10 bandas. El modelo de ramas está en
`CLAUDE.md` y ADR-007; lo propio de cada equipo, en `docs/SPECS_68K.md` y `docs/SPECS_PPC.md`.

## 2. Plataformas objetivo

| Elemento | LC 580 (`68k`) | Mac con 601 (`ppc`) |
|---|---|---|
| CPU | 68LC040 a 33 MHz, sin FPU (dato del usuario) | PowerPC 601, modelo por confirmar; se diseña para el piso, 60 MHz (sin medir) |
| Detalle | `docs/HARDWARE_LC580.md` | `docs/HARDWARE_PPC601.md` |
| Cachés | 4 KB instrucciones + 4 KB datos (por verificar en M68040UM) | 32 KB (por verificar) |
| Sistema | Mac OS 8.0 y 8.1 (dato del usuario). Sound Manager 3.x por confirmar con Gestalt. No se garantiza 7.x | por confirmar |
| Sonido | **Contradicción sin resolver:** Víctor indica 22 kHz, 16 bits, estéreo; EveryMac y apple-history indican 8 bits. Se resuelve con `sysprobe` | por verificar con `sysprobe` |
| Emulador | Infinite Mac, Basilisk II: solo lógica, no velocidad | por verificar cuál sirve |
| Toolchain | Retro68 `m68k-apple-macos`, host CachyOS | Retro68 `powerpc-apple-macos` |

## 3. Presupuesto de ciclos del LC 580 (aritmética, no medición)

El del 601 está en `docs/SPECS_PPC.md`.

- Un frame MPEG-1 Layer III = 1152 muestras por canal.
- A 44,1 kHz: 1152 / 44100 = **26,12 ms** por frame.
- A 33 MHz: ≈ **862 000 ciclos por frame** (100 % de CPU).
- Por muestra de salida (estéreo, 2304 muestras/frame): ≈ **374 ciclos** al 100 %.
- Meta inicial: **factor de tiempo real (RTF) ≥ 1,5**, es decir decodificar un frame
  en ≤ 67 % del tiempo del frame. Eso deja ≈ **250 ciclos por muestra de salida**
  para todo el decodificador, y el resto para Sound Manager, disco e interfaz.
- Hipótesis a verificar: la síntesis polifásica (≈ 16 multiplicaciones por muestra
  de salida más la DCT32) domina. Si `MULS.L` 32×32→64 cuesta ~20 ciclos en el 040
  (**dato no verificado**), las multiplicaciones solas consumirían ~el presupuesto
  completo a calidad plena. De ahí la necesidad de perfiles de calidad.

RTF = tiempo de audio decodificado / tiempo de reloj usado. RTF 1,0 es el límite;
bajo 1,0 hay cortes.

## 4. Perfiles de calidad (seleccionables)

| Perfil | Descripción | Cuándo |
|---|---|---|
| `FULL` | 44,1 kHz estéreo, síntesis completa | si el RTF medido lo permite |
| `HALF` | salida a mitad de tasa (≈ 22 kHz). En libmad es decimación sin filtro (aliasing) con `dct32` completo; ver ADR-006 | candidato principal para 192 kbps (calidad sin escuchar) |
| `MONO` | mezcla L+R antes de síntesis | margen holgado |
| `HALF_MONO` | combinación | último recurso |

libmad 0.15.1b ya trae `HALF` (`MAD_OPTION_HALFSAMPLERATE` / `synth_half`), **verificado**:
es decimación sin filtro pasa-bajos (idéntica a una de cada dos muestras de FULL), con el
`dct32` completo, así que ahorra solo parte de la ventana de síntesis. Un `HALF` con filtrado
real o con 16 subbandas sería implementación propia (ADR-006). `MONO` se implementa mezclando
L+R en las subbandas antes de la síntesis (verificado a ≤ 1 LSB de mezclar el PCM).

El perfil se elige en configuración y, opcionalmente, de forma automática según el
RTF medido en los primeros N frames.

Estos perfiles hoy solo existen para MP3. Los equivalentes para los demás formatos se definen
en la 6-G; si un archivo no alcanza tiempo real y no hay perfil que lo salve, la app lo informa.

## 5. Requisitos funcionales

- RF1. Abrir un archivo de audio desde un diálogo estándar y reproducirlo.
- RF2. Controles: reproducir/pausar, detener, barra de progreso con búsqueda
  (aproximada por posición de bytes; exacta si hay cabecera Xing/VBR).
- RF3. Lista de reproducción (archivos, carpeta y `.m3u`). Hecha en la 5b-2.
- RF4. Saltar etiquetas ID3v2 al inicio y ID3v1 al final; mostrar título/artista si
  existen (ISO-8859-1 → MacRoman).
- RF5. MPEG-1 Layer III a 32/44,1/48 kHz; MPEG-2 LSF (16/22,05/24 kHz) como opcional.
  Layer I/II fuera de alcance. Formato libre (free format) fuera de alcance.
- RF6. VBR: leer cabecera Xing/Info para duración y tabla de búsqueda.
- RF7. Mostrar el RTF y el perfil activo (modo depuración).
- RF8. Resistencia a errores: frame corrupto → silenciar ese frame y resincronizar,
  sin colgar la aplicación.
- RF9. FLAC, Ogg Vorbis y WMA v1/v2 decodificados por la app (ADR-009). Sin WMA Pro, Lossless,
  Voice ni DRM; sin Opus ni FLAC dentro de Ogg.
- RF10. QuickTime para los demás formatos que el sistema pueda abrir: AIFF, WAV, SND... (ADR-010).
- RF11. Ecualizador de 10 bandas: 70, 180, 320, 600, 1k, 3k, 6k, 12k, 14k y 16k Hz, más preamp y
  On, con aspecto Platinum. Su disponibilidad depende del formato y del equipo (ADR-011).
- RF12. Logo e íconos propios en la paleta de 256 colores de Mac OS 8.

## 6. Requisitos no funcionales

- RNF1. Sin cortes de audio en el perfil elegido con RTF ≥ 1,3 sostenido en el equipo real.
- RNF2. La interfaz debe seguir respondiendo (eventos atendidos al menos cada
  ~50 ms) mientras se decodifica.
- RNF3. Memoria: partición de la aplicación ≤ 3 MB (a revisar tras medir).
- RNF4. Las tres compilaciones desde `tools/build-all.sh`, con un solo comando.
- RNF5. Rama `68k`: cero instrucciones de FPU en el binario (verificado con objdump). Rama `ppc`:
  sin `mftb`, que el 601 no tiene.
- RNF6. Compatibilidad con JIT de Basilisk II (sin código auto-modificable).
- RNF7. Licencia GPL (`LICENSE`), con los avisos de cada biblioteca de terceros conservados.

## 7. Arquitectura

```
 [Archivo] -> lector (File Manager) -> capa de códecs (6-G)
                                         |-- MP3: libmad   [kernels asm: rama 68k]
                                         |-- FLAC          [kernels asm: rama 68k]
                                         |-- Vorbis        [kernels asm: rama 68k]
                                         |-- WMA           [kernels asm: rama 68k]
                                         `-- (el EQ actúa aquí, sobre coeficientes)
           -> PCM 16 bits -> buffer circular -> Sound Manager (doble buffer / callback)

 [Archivo no reconocido] -> QuickTime -> su propio canal del Sound Manager (sin EQ)
```

- Decodificación en el bucle principal en **porciones de un frame**, cediendo a
  `WaitNextEvent` entre porciones. El callback de audio solo mueve índices.
- Dos ventanas en estilo Platinum, más la del ecualizador (`docs/UI.md`).

## 8. Etapas, entregables y criterios de salida

### Etapa 0 — Entorno
- Programa `sysprobe`: vuelca a `sysprobe.txt` las consultas Gestalt y del Sound
  Manager listadas al final de `docs/HARDWARE_LC580.md`. Se ejecuta primero en el LC 580.
- Instalar Retro68 en CachyOS; compilar una app "hola mundo" 68k.
- Confirmar flags `-m68040 -msoft-float`; objdump sin instrucciones `f*`.
- La transferencia de archivos al LC 580 la resuelve Víctor por su cuenta; Claude Code
  solo entrega los artefactos en `dist/` (`.bin`, `.dsk` y/o `.APPL` con `.rsrc`/`.finf`).
- **Salida:** la app abre en Basilisk II y en el LC 580.

### Etapa 1 — Microbenchmarks de CPU en el LC 580
- Medir con `Microseconds()`: `MULS.L` (32×32→32 y →64), `MULS.W`, `MOVE16`, `MOVEM`,
  lectura secuencial vs. salto con tabla de 8 KB/16 KB (efecto de la caché de datos).
- **Salida:** tabla en `docs/PERF_LOG.md` con ciclos estimados por operación.

### Etapa 2 — Banco de pruebas del decodificador (sin audio)
- Compilar libmad en C; decodificar N frames de 3 archivos de prueba
  (128 kbps, 192 kbps, mono) y reportar RTF por perfil.
- **Salida:** RTF baseline de libmad en C puro en el LC 580.

### Etapa 3 — Perfilado por etapas
- Instrumentar: Huffman, requantización/reorden, stereo, antialias, IMDCT, síntesis,
  conversión a PCM. Reportar % por etapa.
- **Salida:** ranking de costo y lista priorizada de kernels a optimizar.

### Etapa 4 — Kernels en ensamblador (iterativa)
- Un kernel por iteración: referencia C → asm → prueba bit a bit → medición.
- **Salida por iteración:** RTF mejora medido; prueba en verde en Basilisk II
  (JIT activado y desactivado) y en el LC 580.
- **Salida de la etapa:** RTF ≥ 1,5 en el perfil objetivo, o decisión documentada
  de qué perfil se ofrece como máximo.

### Etapa 5 — Reproductor
- Sound Manager con doble buffer, lectura asíncrona, UI mínima, perfiles.
- **Salida:** reproducción continua de 10 minutos sin cortes en el LC 580.

### Etapa 6 — Bifurcación y ampliación
Se pide en tres solicitudes separadas; detalle y criterios de salida en `docs/ETAPA6.md`.
- **6-PPC** (rama `ppc`): port del reproductor al 601, en C.
- **6-G** (rama `master`): capa de códecs, FLAC, Ogg Vorbis, WMA, QuickTime, ecualizador, logo y
  las tres compilaciones del reproductor.
- **6-68K** (rama `68k`): kernels en asm por formato y del EQ; uso óptimo del Sound Manager. Incluye
  la etapa 4, que ya no espera mediciones (ADR-008).

## 9. Plan de pruebas

- **Bit a bit:** cada kernel asm contra su referencia C (vectores aleatorios con
  semilla fija + casos límite).
- **Regresión de audio:** decodificar archivos de prueba y comparar el PCM contra el
  de libmad compilado en el host (CachyOS). Para los perfiles que cambian la salida
  (`HALF`, `MONO`) comparar contra una referencia propia en C documentada.
- **Otros formatos:** FLAC, bit a bit contra el original. Vorbis y WMA, por SNR contra `ffmpeg`, con el
  umbral que se fije en la 6-G.
- **Rendimiento:** solo cuenta lo medido en el equipo real (ver `mac-test-workflow`).
- **Archivos de prueba sugeridos:** 128 kbps CBR estéreo, 192 kbps CBR estéreo,
  VBR, mono, con ID3v2 grande, con frames corruptos.

## 10. Riesgos

| Riesgo | Mitigación |
|---|---|
| 192 kbps estéreo a 44,1 kHz no llega a tiempo real | Perfiles `HALF`/`MONO`; medir temprano |
| Mejora en Basilisk II no se traduce al LC 580 | Medir siempre en hardware real |
| Caché de datos de 4 KB penaliza tablas | Reorganizar tablas, acceso secuencial, MOVE16 |
| Interrupciones/Sound Manager provocan cortes | Buffer PCM grande, callback mínimo |
| Toolchain o headers de Retro68 con diferencias | Documentar versión exacta en `docs/DECISIONS.md` |
| Cadena de transferencia lenta al LC 580 | Definir en etapa 0, automatizar `.dsk`/`.bin` |
| Vorbis y WMA pueden no llegar a tiempo real en el 68LC040, ni con asm | Medir; la app informa si no alcanza |
| Asm escrito sin medir no acelera lo que importa | Selector C/asm y medición cuando Víctor pueda (ADR-008) |
| La FAT no arranca (receta sin documentar en Retro68) | Probada primero con `sysprobe` (ADR-012) |
| Cada cambio general se fusiona a dos ramas | Flujo único desde `master`; documentos solo en `master` (ADR-007) |

## 11. Preguntas abiertas

Resueltas (respuestas de Víctor):
1. System del LC 580: **Mac OS 8.0 y 8.1**.
2. Transferencia de archivos: **Compact Flash con adaptador IDE a CF**.
3. Basilisk II: ROM de un **Performa 630**. Pendiente de verificar que esa ROM arranque bien en
   Basilisk II (no todas las ROM 68040 son compatibles; si falla, probar con una ROM de Quadra
   650/800/900/950). En Infinite Mac usa ROM de Quadra 650. Las ROM no van al repositorio.
4. Disco del LC 580: la misma CF por IDE. **No hay unidad de CD.** Rendimiento del disco sin medir.

Abiertas:
5. Sonido del LC 580: Víctor indica **22 kHz, 16 bits, estéreo**; EveryMac y apple-history indican
   **8 bits estéreo**. Se resuelve con `sysprobe` en el equipo (ver `docs/HARDWARE_LC580.md`). Si la tasa
   máxima es ≈ 22 kHz, `HALF` pasa a ser el objetivo y se revisa el RTF necesario.
6. Mac OS 8.1 usa HFS+ en volúmenes grandes: confirmar que el volumen donde se copie
   la app y los MP3 sea legible y que el nombre de archivo sea compatible con el
   formato de transferencia que use Víctor.
7. **Modelo exacto del Mac con 601** (6100, 7100, 8100...): sin ese dato se diseña para el piso de la
   familia, 60 MHz. Falta también su sistema y su sonido.
8. **Bandas del ecualizador:** 70/180/320 Hz son las indicadas por Víctor, sin fuente primaria de
   Winamp; la documentación de VLC lista 60/170/310. Conviene compararlas con un Winamp real.
