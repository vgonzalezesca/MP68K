# Etapa 5 — Audio (5a) y reproductor con ventana (5b)

## Estado

- **5a (camino de audio): confirmado por oído en Infinite Mac** (ROM Quadra 650, Mac OS 8.1,
  Sound Manager 3.2): tono y velocidad correctos, estéreo por ambos lados, la pista forzada a
  8 bits suena limpia, 0 cortes durante la reproducción. Eso valida `initStereo` = 0x00C0,
  `initMono` = 0x0080 y el 8 bits sin signo (silencio 0x80). **No valida nada del LC 580**: sus
  formatos de salida y tasas siguen sin conocerse hasta correr `sysprobe`/`audiotest` ahí.
- Los `underruns=33`/`17` que `audiotest` mostró al final de cada pista eran un error de la
  prueba (contaba el silencio del anillo al vaciarse); corregido.
- **5b-1 (motor + ventana principal + menús): probada por Víctor en Infinite Mac** (capturas: reproduce un
  MP3 real de 320 kbps a 48 kHz en FULL, partición de 1,5 MB). Hallazgos suyos: el audio se congelaba al abrir
  el menú Calidad, el texto estaba en español y el diseño Winamp no gustó.
- **5b-2 (lista, ID3, Xing/VBR, inglés, Platinum, ganchos de menú): compilada y con sus módulos portables
  probados en el PC; sin ejecutar ni ver en pantalla.** Ver `docs/UI.md`.

# 5a — Camino de audio (`audiotest`)

## Qué hay

```
disco --FSRead--> dec_stream (libmad) --PCM 16 bits--> anillo --Sound Manager (doble buffer)--> salida
 fileread.c        decoder_core.c           ring.c           audio.c (callback en interrupción)
```

| Módulo | Qué hace | Verificación |
|---|---|---|
| `src/decoder_core.c` (`dec_stream_*`) | decodifica por bloques; salta ID3v2; seek; recupera el último frame | PC: mismo PCM que `dec_run` con 5 tamaños de buffer (4096 a 65536, incluido uno impar), con etiqueta ID3v2 y en los 4 perfiles |
| `src/ring.c` | anillo sin bloqueos, un productor y un consumidor | PC: 200 000 bytes con tamaños aleatorios; contadores que dan la vuelta a 2^32 |
| `src/pcmconv.c` | 16 bits → 8 bits sin signo (redondeo, saturación) | PC: extremos, silencio y monotonía |
| `src/common/audio.c` | canal, negociación de formato, doble buffer, pausa, contadores | **solo compila**; callback revisado en el desensamblado (`rtd #8`, argumentos en orden Pascal) |
| `src/common/fileread.c`, `memblock.c` | lectura por bloques y memoria (NewPtr o temporal) | solo compila |
| `src/player/audiotest.c` | reproduce 5 pistas de 15 s con estadísticas por segundo | solo compila |

Pruebas del PC: `tools/host-test.sh` (requiere `tools/host-ref.sh` antes).

## Decisiones de diseño

- **Doble buffer** (`SndPlayDoubleBuffer`, Sound Manager ≥ 3.0). Si es menor, `audio_open`
  devuelve `AUDIO_ERR_NO_DOUBLEBUFFER`; el respaldo con `bufferCmd` no está hecho.
- **Formato de salida negociado:** se consulta `siSampleSizeAvailable` ('ssav') en el canal: 16 bits
  si el hardware los ofrece; si solo ofrece 8, se convierte aquí al escribir en el anillo (no en el
  callback ni en el Sound Manager). Si la consulta falla, 16 bits.
- **Pausa** = el canal sigue vivo y suena silencio sin consumir el anillo. Retardo de hasta
  2 buffers (≈ 93 ms a 44,1 kHz).
- **Lectura síncrona** (`FSRead`) desde el bucle principal, en bloques de 32 KB. Es una
  simplificación: `PBReadAsync` queda para cuando las mediciones lo pidan.
- El callback solo hace `memcpy`/`memset` y contadores; el contexto llega por `chan->userInfo`.

## Qué mirar en `audiotest_results.txt`

- `audio: Sound Manager X.Y, ... N bits (hardware ofrece: ...)`: **responde la pregunta abierta nº 4**
  (8 o 16 bits) si se ejecuta en el LC 580.
- `preroll: ... RTF`: velocidad de decodificación al arrancar la pista.
- `RESULTADO: underruns=...`: debe ser 0. `decode_load`: porcentaje del tiempo de un frame que
  se usa en decodificar (el resto es margen); sobre ~70 % hay poco margen.
- La última pista fuerza 8 bits: sirve para escuchar la ruta de conversión aunque el hardware sea de 16.

## Hay que verificar al escuchar (supuestos sin confirmar)

1. Que suene, y a la **velocidad y tono correctos** (el desfase de tasa se nota como tono distinto).
2. Que el estéreo sea estéreo: `initStereo` = 0x00C0 e `initMono` = 0x0080 son valores de memoria
   de `Sound.h` (las Multiversal no los traen). Si el estéreo sale por un solo lado o el mono
   no suena, el primer sospechoso es esa constante.
3. Que el 8 bits suene limpio: se asume binario con desplazamiento (0x80 = silencio) para
   `dbhSampleSize` = 8. Si suena con mucho ruido o distorsión, el signo está al revés.
4. Que no haya chasquidos en el cambio de pista (cada pista abre y cierra el canal).

## Limitaciones conocidas

- Sin volumen, balance, buscar con el ratón ni cambio de perfil durante la reproducción (5b).
- El perfil automático por pre-roll no está hecho; sus umbrales dependen de las mediciones del LC 580.
- La resolución del reloj del emulador (~1 ms) hace poco precisos los tiempos por frame.
- `dec_run` (usado por `decbench`/`decprofile`) pierde el último frame del archivo porque recibe los datos
  sin los 8 bytes de cola que exige libmad; el flujo no. No afecta las mediciones.

# 5b-1 — Reproductor con ventana (`mp368k`)

| Pieza | Archivo | Estado |
|---|---|---|
| Motor (abrir, reproducir, pausa, buscar, perfiles, automático) | `src/player/player.c` | compila; sin ejecutar |
| Ganancia y balance por software | `src/common/audio.c`, `src/pcmconv.c` | ganancia probada en el PC; sin oír |
| Reinicio del canal (buscar, cambiar de perfil) | `audio_restart()` | compila; sin ejecutar |
| Interfaz y menús | `src/player/{app,ui_main,ui_draw}.c`, `player.r` | compila; **sin ver en pantalla** |

Decisiones y supuestos (todos sin medir):
- **Volumen por software** (una multiplicación por muestra, solo si no está al 100 %): su costo en el
  68LC040 está **sin medir**. No se usa `volumeCmd` porque las Multiversal no lo traen.
- **Modo automático:** al empezar una pista mide la velocidad de decodificación al 20 % del archivo y
  elige el primer perfil de FULL → HALF → HALF_MONO con RTF ≥ 1,5 (umbral **provisional**, el presupuesto de
  SPECS §3; se fija con datos del LC 580). Si hay ≥ 2 cortes en 10 s, baja un perfil en esa posición.
- **Anillo de 2 s** en la app; `audiotest` usa 1 s.
- **Buscar** estima el byte linealmente (exacto en CBR, aproximado en VBR hasta 5b-2); los primeros frames
  tras buscar pueden salir silenciados (reserva de bits).
- **Partición** de 1,5 MB (mínimo 1 MB), con `canBackground`: sigue sonando con otra aplicación al frente.
- Cambiar de perfil o de profundidad de salida durante la reproducción deja un hueco corto.

Para probar (Víctor, en el emulador): abrir un MP3 y que suene; pausa/reanudar; detener; mover la barra;
volumen y balance se oyen; cambiar de perfil desde el menú Calidad; arrastrar la ventana ~1 s no corta;
el tiempo avanza 1 s por segundo; salir limpio. Enviar capturas de pantalla para ajustar el aspecto.

# 5b-2 — Lista, etiquetas, Platinum e inglés

| Pieza | Archivo | Verificación |
|---|---|---|
| Etiquetas ID3v1/v2 → MacRoman | `src/id3.c` | PC: 16 casos armados a mano (Latin-1, UTF-16 LE/BE, UTF-8, v2.2/2.3/2.4, desincronización, cabecera extendida, truncada) y archivos reales etiquetados por `ffmpeg` |
| Cabeceras Xing/Info/VBRI, duración, tiempo → byte | `src/mp3info.c` | PC: duración contra `ffprobe` (±60 ms exacta, ±150 ms estimada); error de búsqueda < 0,05 % en CBR y < 0,3 % en VBR con 5 puntos por archivo |
| Orden: shuffle, repeat, siguiente/anterior, borrado | `src/order.c` | PC: una vuelta de shuffle visita todas una vez; repeat all no repite al dar la vuelta; borrar la actual; 512 entradas |
| Lista, carpeta, `.m3u` | `src/player/playlist.c` | solo compila |
| Ventana de lista (Platinum) | `src/player/ui_list.c` | solo compila |
| Ventana del reproductor (Platinum) | `src/player/ui_main.c`, `ui_draw.c` | solo compila |
| Ganchos `MenuHook`/`DragHook` | `src/player/app.c` | desensamblado revisado (guardan d0–d7/a0–a6; restauran el gancho anterior); **sin verificar en el sistema** |

**Cómo comprobar el arreglo del congelamiento:** reproducir un MP3, activar Options > Show Technical Info,
abrir el menú Quality y mantenerlo ~5 s. Si los ganchos funcionan, `gap` no crece mucho más allá de lo normal y
el audio no se corta; si `gap` salta a ~5000 y el sonido se corta tras unos 4 s, el sistema no los llama.

Hallazgos de esta etapa:
- `GetDblTime` tampoco tiene glue en las Multiversal (igual que `GetApplLimit`): se leen `DoubleTime` (0x02F0) y
  `ApplLimit` (0x0130) directamente. Las Multiversal tampoco traen `Controls.h` como archivo aparte.
- Un MP3 con cabecera Info/Xing se decodifica con un primer frame silencioso de ~26 ms (no se salta).
- Los nombres de archivo de más de 63 caracteres no caben en un `FSSpec` y quedan fuera de la lista.

Límites conocidos (sin medir ni probar en Mac): la barra de desplazamiento nativa se supone Platinum; el
Appearance Manager no se usa para nada más; los `.m3u` de Windows rara vez se resuelven por completo; los
segmentos de código siguen sobre 32 KB (regla 6): Main 70 KB y Runtime 44 KB.
