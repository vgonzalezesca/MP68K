# Interfaz de MP368K (etapa 5b-2): tema Platinum, dos ventanas

Aspecto del tema **Platinum de Mac OS 8** (decisión de Víctor, que descartó el estilo Winamp
oscuro de la 5b-1): fondo gris claro, biseles, pozos hundidos y la fuente del sistema. Dibujo propio
con QuickDraw en color: las Multiversal Interfaces no traen las APIs de temas del Appearance Manager,
así que se imita el aspecto (funciona en cualquier System 7 o posterior). Sin visualizador de espectro.
**Todo el texto visible está en inglés de EE. UU.**

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
  instalan `MenuHook` (0x0A30) y `DragHook` (0x09F6) en `targets/hooks.c`: en 68k, un puente en ensamblador
  que conserva todos los registros y llama a `player_idle()`; en PPC, un descriptor de Mixed Mode.
  **No verificado en Mac OS 8.1 con el menú Platinum**: se comprueba mirando que `gap` no crezca con un menú
  abierto. Si el sistema no llama a los ganchos, el audio dura lo que dure el anillo (4 s).
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

## Formatos de audio (6-G, fase 2: compilado, sin ver en pantalla)

- El motor reconoce el formato **por el contenido** (`codec_probe_any`), no por la extensión: MP3 y FLAC. Add Folder… usa la
  extensión o el tipo de archivo de Mac solo para decidir qué ofrecer.
- La línea de formato muestra, según el códec: MP3 → `128 kbps  44.1 kHz  Stereo  FULL`; FLAC → `FLAC  16-bit  44.1 kHz  Stereo`.
- **El menú Quality solo actúa sobre MP3** (los perfiles FULL/HALF/MONO son de libmad): con un FLAC abierto sus ítems quedan
  desactivados.
- Si la velocidad medida al empezar una pista no alcanza para tiempo real (RTF < 1,0), la línea del artista dice
  **"May not play in real time"**. Es una medición del equipo donde se corre: en el emulador no dice nada sobre el LC 580.
- Con Show Technical Info la línea técnica agrega `mem NNK` (pico del arena de los códecs).
- Límites con mensaje claro: más de 2 canales o una tasa fuera de 8–48 kHz ("…only play audio with 1 or 2 channels…").

## Logo e ícono (6-G, fase 1/1b: compilado, sin ver en el Finder)

- **Dirección (elegida por Víctor): Platinum con contorno.** Esfera con anillos concéntricos verde azulado, colina clara y un trazo
  oscuro, a partir de su imagen (`assets/logo/reference.png`). Contorno de 1 px, luz desde arriba a la izquierda, sombra proyectada de
  1 px abajo a la derecha, sin difuminado a 16 y 32 px.
- **16 y 32 px se dibujan a mano, píxel por píxel**, en `assets/logo/icon32.txt` e `icon16.txt`: una letra por color, todos **exactos de
  la paleta del sistema** (7 colores a 32 px y 7 a 16 px); se editan como texto. 32 px: 3 anillos de punto medio (arcos continuos) y
  el trazo oscuro; 16 px: un anillo con punto central y la colina, sin el trazo (no cabe).
- `tools/make_logo.py` los convierte y genera **`icl8`/`ics8`** (256 colores), **`icl4`/`ics4`** (16 colores: letra → color del sistema, ver
  `FOUR_BIT`), **`ICN#`/`ics#`** (1 bit con máscara; la sombra proyectada entra en la máscara) y **`LOGO`** (96×96, dibujo por procedimiento
  con degradado difuminado en orden y el mismo contorno y reflejo). `assets/logo/contact_sheet.png` es la hoja de contacto: 10× y 1×,
  sobre gris Platinum, blanco, patrón de escritorio y seleccionado (aproximado), y los de 4 y 1 bit.
- **Mac OS 8.0/8.1 no usa íconos de 48×48** (eso llegó con 8.5), así que no se generan.
- **Recursos** (`src/player/icons.r`, generado; `player.r` trae `BNDL`, `FREF`, la firma `M368` y `vers`).
- **About:** ventana propia (`ui_about.c`), no un diálogo modal: el audio sigue sonando. Dibuja `LOGO` con la tabla de colores
  estándar del sistema (`GetCTable(8)`); el blanco puro es transparente. Se cierra con clic, Return, Enter, Esc o el cuadro de cierre.
- **Supuestos sin verificar en un Mac:** el orden de índices de la paleta de 256 colores (cubo de 6 niveles sin el negro, 10 rojos, 10
  verdes, 10 azules, 10 grises, negro al final) y el de la de 16 colores (blanco, amarillo, naranja, rojo, magenta, púrpura, azul, cian,
  verde, verde oscuro, café, canela, gris claro, gris medio, gris oscuro, negro). Si los colores se ven mal en el Finder, ese orden es el
  primer sospechoso.
- **Bit "bundle":** el Finder solo muestra el ícono si el archivo lo tiene; Retro68 lo deja en cero, así que
  `tools/set_bundle_bit.py` lo activa en el `.bin` (con su CRC), el `.dsk`, `.finf` y el AppleDouble, y lee de vuelta cada parche.
  Si aun así no se ve, probar reconstruir el escritorio (mantener ⌘ y Opción al arrancar) o copiar la app a un volumen nuevo.

## Ecualizador (6-G, fase 3: compilado y probado en el PC, sin ver en pantalla)

- **Ventana** Window ▸ Equalizer (⌘E), 330×206, textos en inglés de EE. UU.: casilla **On** (activada por omisión), cuatro presets
  **Flat, Bass, Treble, Vocal**, deslizador **Preamp** y 10 deslizadores verticales **70, 180, 320, 600, 1K, 3K, 6K, 12K, 14K, 16K Hz**
  con escala +12 / 0 / −12 dB. Pasos de 0,5 dB (±12 dB) y detente en 0 dB. Mientras se arrastra, el valor se lee arriba ("1 kHz  +3.5 dB").
  El cuadro de cierre solo la oculta. **Los presets son propios, no los de Winamp**, y los valores están sujetos a oído (en pasos de 0,5 dB:
  Bass 12,10,8,4,0…; Treble …0,2,4,8,10,12; Vocal −4,−2,0,4,8,8,4,0,−2,−4).
- **Solo actúa sobre MP3.** Con un FLAC abierto los controles quedan atenuados y dice "The equalizer is not available for FLAC files."
  (decisión de Víctor: en 68k, FLAC no tiene EQ; QuickTime tampoco).
- **Cómo suena:** el EQ multiplica los coeficientes de frecuencia antes de la IMDCT (ADR-011), así que **no filtra el PCM**. Con el EQ apagado o
  plano el PCM es idéntico bit a bit. Costo: hasta ~2 304 multiplicaciones por frame, **sin medir en el LC 580** (la fila "test128.mp3+EQ" de
  `codecbench` lo mide).
- **Latencia:** el anillo guarda 4 s para menús y arrastres, así que con la ventana abierta el motor solo deja ~600 ms de audio por delante
  (`player_set_fill_target_ms`) y al cerrarla vuelve a 4 s. Lo que ya estaba en el anillo se oye antes: **hasta ~3 s después de abrir la ventana
  un cambio no se oye todavía**. Menos colchón significa más riesgo de cortes mientras se ajusta.
- **Límites conocidos:** a 44,1 kHz una línea de frecuencia mide ~38 Hz, así que 70, 180 y 320 Hz tienen poca resolución (error medido hasta
  1,3 dB contra el modelo); los MP3 de 128 kbps suelen cortar sobre 16 kHz y esa banda casi no se nota; un preamp alto recorta. Los ajustes
  **no se guardan** entre ejecuciones (no se pidió).
- **Pruebas de PC (32 y 64 bits):** modelo de ganancia con error ≤ 2,2 % (0,19 dB); respuesta sobre ruido blanco codificado a 44,1/48/32 kHz y mono,
  a 0,1–0,2 dB del modelo en medios y agudos; bloques cortos ejercitados con un archivo de transitorios; mismo resultado en 32 y 64 bits.
  **Falta verla y oírla en Infinite Mac.**

