# SPECS — MP368K

Estado: borrador inicial. Todo número de rendimiento aquí es **estimación sin medir**
salvo que diga lo contrario. Se reemplaza por datos reales en `docs/PERF_LOG.md`.

## 1. Objetivo

Reproductor de MP3 para Macintosh LC 580 que decodifique en tiempo real archivos
MPEG-1/2 Layer III de hasta 192 kbps estéreo, con la mayor calidad posible, usando
un decodificador de punto fijo y kernels en ensamblador 68040.

## 2. Plataforma objetivo

| Elemento | Valor | Estado |
|---|---|---|
| CPU | 68LC040 a 33 MHz, sin FPU (la MMU sí existe en el LC040; por confirmar). Detalle completo en `docs/HARDWARE_LC580.md` | dato del usuario |
| Cachés | 4 KB instrucciones + 4 KB datos | por verificar en M68040UM |
| RAM | 16 MB | dato del usuario |
| Sistema | Mac OS 8.1 (dato del usuario). Incluye Sound Manager 3.x (por confirmar con Gestalt), así que el doble buffer debería estar disponible. Objetivo mínimo de compatibilidad: 8.1; no se garantiza 7.x | dato del usuario |
| Sonido | **Las fuentes indican salida estéreo de 8 bits**; tasas y chip desconocidos. Puede que 44,1 kHz/16 bits no sea nativo | por verificar con `sysprobe` (etapa 0) |
| Emulador | Basilisk II (JIT activable) | solo lógica, no velocidad |
| Toolchain | Retro68 (`m68k-apple-macos`), host CachyOS | por instalar |

## 3. Presupuesto de ciclos (aritmética, no medición)

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

## 5. Requisitos funcionales

- RF1. Abrir un MP3 desde un diálogo estándar de archivos y reproducirlo.
- RF2. Controles: reproducir/pausar, detener, barra de progreso con búsqueda
  (aproximada por posición de bytes; exacta si hay cabecera Xing/VBR).
- RF3. Lista simple de reproducción (archivo `.m3u` o carpeta) — etapa 6.
- RF4. Saltar etiquetas ID3v2 al inicio y ID3v1 al final; mostrar título/artista si
  existen (ISO-8859-1 → MacRoman).
- RF5. MPEG-1 Layer III a 32/44,1/48 kHz; MPEG-2 LSF (16/22,05/24 kHz) como opcional.
  Layer I/II fuera de alcance. Formato libre (free format) fuera de alcance.
- RF6. VBR: leer cabecera Xing/Info para duración y tabla de búsqueda.
- RF7. Mostrar el RTF y el perfil activo (modo depuración).
- RF8. Resistencia a errores: frame corrupto → silenciar ese frame y resincronizar,
  sin colgar la aplicación.

## 6. Requisitos no funcionales

- RNF1. Sin cortes de audio en el perfil elegido con RTF ≥ 1,3 sostenido en el LC 580.
- RNF2. La interfaz debe seguir respondiendo (eventos atendidos al menos cada
  ~50 ms) mientras se decodifica.
- RNF3. Memoria: partición de la aplicación ≤ 3 MB (a revisar tras medir).
- RNF4. Binario reproducible desde `tools/` con un solo comando.
- RNF5. Cero instrucciones de FPU en el binario final (verificado con objdump).
- RNF6. Compatibilidad con JIT de Basilisk II (sin código auto-modificable).
- RNF7. Licencia GPL, con avisos de libmad conservados.

## 7. Arquitectura

```
 [Archivo MP3] -> lector asíncrono (File Manager) -> buffer circular de bytes
        -> sincronización de frames + cabecera -> libmad (Huffman, requantización,
        stereo, antialias, IMDCT, overlap)  [kernels asm candidatos]
        -> síntesis polifásica (dct32 + ventana)  [kernel asm candidato]
        -> conversión a PCM 16 bits (redondeo)  [kernel asm candidato]
        -> buffer circular de PCM -> Sound Manager (doble buffer / callback)
```

- Decodificación en el bucle principal en **porciones de un frame**, cediendo a
  `WaitNextEvent` entre porciones. El callback de audio solo mueve índices.
- Interfaz mínima: ventana con lista/título, botones, barra de progreso.

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

### Etapa 6 — Pulido
- ID3, Xing/VBR, búsqueda, lista de reproducción, manejo de errores, ajustes
  persistentes en archivo de preferencias, ícono y recursos.

## 9. Plan de pruebas

- **Bit a bit:** cada kernel asm contra su referencia C (vectores aleatorios con
  semilla fija + casos límite).
- **Regresión de audio:** decodificar archivos de prueba y comparar el PCM contra el
  de libmad compilado en el host (CachyOS). Para los perfiles que cambian la salida
  (`HALF`, `MONO`) comparar contra una referencia propia en C documentada.
- **Rendimiento:** solo cuenta lo medido en el LC 580 (ver `mac-test-workflow`).
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

## 11. Preguntas abiertas (Víctor debe responder)

Resueltas:
1. System del LC 580: **Mac OS 8.1**.
2. Transferencia de archivos: la gestiona Víctor.
3. Basilisk II: ROM de un **Performa 630** con Mac OS 8.1. Pendiente de verificar que
   esa ROM arranque bien en Basilisk II (no todas las ROM 68040 son compatibles; si
   falla, probar con una ROM de Quadra 650/800/900/950). La ROM no va al repositorio.

Abiertas:
4. Hardware de sonido del LC 580: EveryMac y apple-history indican **8 bits estéreo**.
   Confirmar formatos y tasas reales con `sysprobe` (etapa 0, ver
   `docs/HARDWARE_LC580.md`). Si se confirma 8 bits y tasa máxima ≈ 22 kHz, el perfil
   `HALF` pasa a ser el objetivo y se revisa el RTF necesario.
5. ¿Qué disco usa el LC 580 (IDE interno, SCSI, CF)? Afecta el tamaño de buffer y la
   lectura asíncrona.
6. Mac OS 8.1 usa HFS+ en volúmenes grandes: confirmar que el volumen donde se copie
   la app y los MP3 sea legible y que el nombre de archivo sea compatible con el
   formato de transferencia que use Víctor.
