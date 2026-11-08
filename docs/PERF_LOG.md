# Bitácora de rendimiento

Solo se registran **mediciones reales**. Una fila sin medir se marca `sin medir`.
Medición = programa del repositorio, compilado con el commit indicado, ejecutado
por Víctor en el equipo indicado.

## Convención

- Reloj: `Microseconds()` (Time Manager). Repetir ≥ 5 veces y reportar mediana y
  mínimo; descartar la primera pasada (cachés frías) y reportarla aparte.
- RTF = (muestras decodificadas / tasa de salida) / tiempo de reloj usado.
- Siempre anotar: equipo, System, CPU/clock, extensiones cargadas relevantes,
  perfil de calidad, archivo de prueba, commit y flags de compilación.
- Cerrar otras aplicaciones y desactivar protectores de pantalla antes de medir.

## Plantilla de registro

```
### AAAA-MM-DD — <qué se midió>
Equipo: LC 580 real / Mac con 601 real (modelo) / emulador (cuál; JIT sí|no)
System: x.x.x
Build: <68k-hash | ppc-hash>   Flags: <los que imprime el programa>   Kernels: C | asm
Archivo: <nombre, formato, bitrate, canales>   Perfil: FULL|HALF|MONO|HALF_MONO   EQ: on|off

| Prueba | Mediana | Mínimo | Observaciones |
|---|---|---|---|
|  |  |  |  |

Conclusión:
Decisión derivada:
```

## Etapa 1 — Microbenchmarks de CPU

| Operación | Ciclos estimados (LC 580) | Fuente | Estado |
|---|---|---|---|
| MULS.L 32×32→32 |  | medición | sin medir |
| MULS.L 32×32→64 |  | medición | sin medir |
| MULS.W 16×16→32 |  | medición | sin medir |
| MOVE16 (por 16 bytes) |  | medición | sin medir |
| Lectura secuencial 8 KB |  | medición | sin medir |
| Lectura con tabla 16 KB |  | medición | sin medir |

## Etapa 2 — RTF baseline libmad en C

| Archivo | Perfil | RTF LC 580 | RTF Basilisk II | Estado |
|---|---|---|---|---|
| 128 kbps estéreo | FULL |  |  | sin medir |
| 192 kbps estéreo | FULL |  |  | sin medir |
| 192 kbps estéreo | HALF |  |  | sin medir |
| 192 kbps estéreo | MONO |  |  | sin medir |

## Etapa 3 — Costo por etapa (% del tiempo de decodificación)

| Etapa | % | Estado |
|---|---|---|
| Huffman |  | sin medir |
| Requantización / reorden |  | sin medir |
| Stereo / antialias |  | sin medir |
| IMDCT + overlap |  | sin medir |
| Síntesis polifásica (DCT32 + ventana) |  | sin medir |
| Conversión a PCM |  | sin medir |

## Etapa 4 / 6-68K — Kernels en asm (LC 580)

Se pueden escribir antes de medir (ADR-008). Un kernel sin medir **no** se considera más rápido.

| Formato | Kernel | RTF con C | RTF con asm | Prueba bit a bit | Build | Estado |
|---|---|---|---|---|---|---|
|  |  |  |  |  |  | sin medir |

## Etapa 6-PPC — RTF baseline en el Mac con 601

Modelo del equipo: por confirmar.

| Archivo | Perfil | RTF | Estado |
|---|---|---|---|
| 128 kbps estéreo | FULL |  | sin medir |
| 192 kbps estéreo | FULL |  | sin medir |

## Etapa 6-G — RTF por formato

| Formato | Archivo | RTF LC 580 | RTF Mac con 601 | EQ | Estado |
|---|---|---|---|---|---|
| FLAC | 16 bits, 44,1 kHz, estéreo |  |  | off | sin medir |
| Ogg Vorbis | calidad por definir |  |  | off | sin medir |
| WMA v2 | 128 kbps |  |  | off | sin medir |
| MP3 | 192 kbps estéreo |  |  | on | sin medir |

## Registro cronológico

### 2026-11-08 — `codecbench` con los kernels asm, en emulador
Equipo: **emulador (Infinite Mac)**; Gestalt dice mach=36, proc=5 (68040), 64 MB. **No es el LC 580: los tiempos no valen para velocidad.**
System: 8.1 (0x810)
Build: 68k-ab73363-mod   Flags: -m68040 -msoft-float -O2 -fomit-frame-pointer -ffunction-sections -fdata-sections   Kernels: asm (`synth_kernel: asm`)
Archivo: test128.mp3 (128 kbps), test192.mp3 (192), testmono.mp3 (64, mono), test128.mp3 con EQ activo (`kBenchEq`)   Perfil: FULL   EQ: on solo en la última fila

| Prueba | Mediana | Mínimo | CRC del PCM | Observaciones |
|---|---|---|---|---|
| test128.mp3 | 3 613 999 µs | 3 609 000 µs | AAECF8F6 | RTF del emulador 3,32 |
| test192.mp3 | 3 811 001 µs | 3 806 001 µs | 2398BAAC | RTF del emulador 3,15 |
| testmono.mp3 | 1 743 000 µs | 1 741 000 µs | 5C79D5A0 | RTF del emulador 6,88 |
| test128.mp3 + EQ | 4 052 000 µs | 4 044 000 µs | A91D9702 | +12 % sobre la misma pista sin EQ, en el emulador |

Resolución del reloj: 999 µs. Pasadas: 1 fría + 3.

**Lo que sí confirma:** los 4 CRC coinciden con `REFERENCE_CODECS.txt` del PC (se compararon a mano: el programa dijo "sin referencia" porque ese archivo no estaba
junto a la app). Es la primera vez que los núcleos asm (síntesis, DCT32, IMDCT larga, PCM) producen el PCM correcto en una build real de Mac OS 8.1 sobre un 68040
emulado, y que lo hace también con el ecualizador activo.
**Lo que no se puede concluir:** nada de velocidad. Las "ciclos por muestra" del programa suponen 33 MHz y aquí no aplican; no hay corrida de `codecbench_ref` en la misma
máquina, así que tampoco hay comparación C contra asm; y el +12 % del EQ es del emulador. FLAC, Ogg y WMA no entraron: salieron "no se pudo abrir (err=-43)" porque los
archivos no estaban junto a la app (el paquete de visita los dejaba en `audio/` y las apps en carpetas aparte; corregido). Víctor confirmó aparte que Ogg y WMA funcionan en `mp368k`.
Decisión derivada: la próxima corrida es `codecbench` y `codecbench_ref` desde `audio/`; la comparación que importa es en el LC 580.

