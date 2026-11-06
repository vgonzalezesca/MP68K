# Interfaz de MP368K (etapa 5b-2): tema Platinum, dos ventanas

Aspecto del tema **Platinum de Mac OS 8** (decisión de Víctor, que descartó el estilo Winamp
oscuro de la 5b-1): fondo gris claro, biseles, pozos hundidos y la fuente del sistema. Dibujo propio
con QuickDraw en color: las Multiversal Interfaces no traen las APIs de temas del Appearance Manager,
así que se imita el aspecto (funciona en cualquier System 7 o posterior). Sin visualizador de espectro
ni ecualizador. **Todo el texto visible está en inglés de EE. UU.**

Todo esto está **compilado pero sin ver en pantalla**: el aspecto se ajusta con capturas de Víctor.

## Paleta

| Elemento | Color |
|---|---|
| Fondo de ventana | `#DDDDDD` |
| Bisel claro / oscuro | `#FFFFFF` / `#7F7F7F` (más un segundo anillo `#F2F2F2` / `#BBBBBB`) |
| Contorno de botón | negro, 1 px, esquinas recortadas |
| Cara del botón | tres bandas `#EEEEEE`, `#DDDDDD`, `#CCCCCC`; pulsado `#AAAAAA` |
| Glifo activo | azul `#3C5AA6` |
| Pantalla LCD | fondo `#C8D2BC`, segmentos `#1C241C`, apagados `#B8C3AC` |
| Lista | blanco; fila seleccionada con el color de resaltado del sistema (`HiliteMode` + `InvertRect`) |

Fuentes: la del sistema (Charcoal bajo Platinum) a 12 pt para el texto principal y Geneva 9 para etiquetas.

## Ventana del reproductor (330 × 144, contenido)

`Rect {arriba, izquierda, abajo, derecha}`, definidos en `src/player/ui_main.c`.

| Elemento | Rect | Contenido |
|---|---|---|
| Pantalla LCD | 10,10 – 54,120 | tiempo `MM:SS` en 7 segmentos; `--:--` sin archivo; tope 99:59 |
| Información | 10,126 – 54,320 | título (12 pt; se desplaza si no cabe), artista, formato (`320 kbps  48 kHz  Stereo  FULL`) |
| Posición | 62,10 – 78,320 | control deslizante; clic o arrastre; al soltar busca |
| Transporte | y 86–108 | Previous, Play, Pause, Stop, Next (30 px c/u) y Eject (abre un archivo) |
| Volumen / Balance | 88,250 – 104,320 / 116,250 – 132,320 | 0–100 (curva cuadrática) / −100…100 con tope en el centro (±8) |
| Alternables | y 114–134 | Shuffle, Repeat (rótulo: `Repeat`, `Repeat All`, `Repeat 1`), Playlist |

Formato de VBR: `VBR 190 kbps` (promedio). Un `*` tras el perfil indica que el modo Automático bajó de perfil.
Con "Show Technical Info" la línea de formato pasa a `buf 950  und 0  gap 12  rtf 2.10`: colchón de audio
(ms), cortes, **mayor intervalo entre llamadas a `player_idle` (ms)** y velocidad medida al elegir el perfil.

## Ventana de la lista (330 × 222)

Se muestra con el botón **Playlist**, el menú Window o al agregar archivos; el cuadro de cierre solo la oculta.

| Elemento | Rect | Contenido |
|---|---|---|
| Lista | 10,10 – 180,304 | filas de 16 px: marcador ▶ de pista actual, `n  Artist - Title`, duración a la derecha |
| Barra de desplazamiento | 10,304 – 180,320 | control nativo (`scrollBarProc`): Platinum con el Appearance Manager de Mac OS 8 (**a verificar**) |
| Botones | y 190–210 | Add…, Add Folder…, Remove, Clear |

Doble clic o Return reproduce; flechas arriba/abajo mueven la selección; Delete quita la fila.

## Qué se redibuja y cuándo

Solo lo que cambia (RNF2: la interfaz no puede quitarle CPU al decodificador): tiempo 1 vez por segundo;
posición cuando se mueve ≥ 0,4 %; botones al cambiar de estado; título/artista/formato al cambiar su texto;
ticker cada ~100 ms solo si el título no cabe. El texto se pinta en modo `srcCopy` con fondo opaco y recorte,
así que no parpadea y no hace falta un GWorld.

## Ratón, teclado y audio

- Botones y deslizadores se siguen con `StillDown`/`GetMouse`; dentro de esos bucles y de la barra de
  desplazamiento se llama a `player_idle()` para no cortar el audio.
- **Menús y arrastre de ventanas:** durante `MenuSelect` y `DragWindow` el sistema corre su propio bucle. Se
  instalan `MenuHook` (0x0A30) y `DragHook` (0x09F6) con un puente en ensamblador que conserva todos los
  registros y llama a `player_idle()`. **No verificado en Mac OS 8.1 con el menú Platinum**: se comprueba
  mirando que `gap` no crezca con un menú abierto. Si el sistema no llama a los ganchos, el audio dura lo
  que dure el anillo (4 s).
- Los diálogos modales (abrir archivo, "About") pausan la reproducción mientras están abiertos.
- Teclado: Space = reproducir/pausa; ←/→ = −5 s / +5 s; ⌘O, ⌘D (Add Files), ⌘F (Add Folder), ⌘W, ⌘Q, ⌘P
  (Play/Pause), ⌘. (Stop), ⌘[ y ⌘] (Previous/Next), ⌘L (Playlist).

## Menús (en-US)

Apple (About MP368K…) · File (Open…, Add Files…, Add Folder…, Open Playlist…, Close Window, Quit) ·
Controls (Play/Pause, Stop, Previous Track, Next Track, Shuffle, Repeat Off/All/One) ·
Quality (Automatic, Full, Half Rate, Mono, Half Rate Mono) ·
Options (Audio Output: Automatic / 8-bit / 16-bit, Show Technical Info) · Window (Playlist).

## Lista, etiquetas y `.m3u`

- **Open…** reemplaza la lista (primero agrega lo nuevo y solo si salió bien quita lo anterior).
  **Add Files…** agrega un MP3 (o un `.m3u`); **Add Folder…** agrega todos los MP3 de la carpeta del archivo
  elegido (por extensión `.mp3` o tipo `MPG3`/`MP3 `), ordenados por nombre.
- **`.m3u`:** líneas con LF, CR o CRLF; se ignoran vacías y `#`. Las rutas relativas (`sub/a.mp3`, `../a.mp3`)
  se resuelven desde la carpeta del `.m3u`; las absolutas o de Windows (`C:\…`) se buscan solo por nombre
  en esa carpeta. Los nombres con acentos se prueban tal cual, como UTF-8 y como Latin-1. Lo que no se
  encuentra se omite y se informa cuántas entradas quedaron fuera.
- Etiquetas: ID3v2.2/2.3/2.4 e ID3v1, convertidas a MacRoman (`src/id3.c`). Límite: la desincronización de
  toda la etiqueta en ID3v2.3 no está soportada.
- Duración: de la cabecera Xing/Info/VBRI si hay (exacta); si no, estimada para CBR. La búsqueda en VBR usa
  la tabla de Xing (error medido < 0,3 % del archivo en un archivo de prueba); VBRI y VBR sin tabla usan
  búsqueda lineal.
- Hasta 512 entradas. Siguiente/Anterior, Shuffle y Repeat siguen `src/order.c` (probado en el PC).
