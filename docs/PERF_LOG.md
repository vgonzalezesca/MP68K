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
| MULS.L 32×32→32 | (Snow: 20) | medición | sin medir en el LC 580 |
| MULS.L 32×32→64 | (Snow: 20) | medición | sin medir en el LC 580 |
| MULS.W 16×16→32 | (Snow: 16) | medición | sin medir en el LC 580 |
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

## Conteo de instrucciones bajo QEMU (NO es tiempo)

`targets/asm-prof.sh` ejecuta cada decodificador como 68040 bajo `qemu-m68k -d in_asm,exec,nochain` y cuenta las instrucciones dinámicas por función; se corre con dos
largos y se resta (costo marginal: sin inicialización ni arnés de prueba). **Es una medición real de instrucciones ejecutadas, no de tiempo**: el 68040 real tiene
instrucciones de costos muy distintos (`muls.l` 32×32→64, divisiones, saltos) y cachés de 4 KB que QEMU no modela. 2026-11-08, commit de esta ronda, 128 kbps salvo indicación.
"Cuadro" = una muestra por canal (en estéreo, un par). Presupuesto a 44,1 kHz y 33 MHz: 748 ciclos por cuadro estéreo.

| Caso | Instrucciones / cuadro | `mul*` / cuadro | Funciones más pesadas (% de instrucciones) |
|---|---|---|---|
| MP3 FULL, libmad original (C) | 604,8 | 48,9 | `synth_full` 51, `III_decode` 13, `III_imdct_l` 13, `dct32` 11 |
| MP3 FULL, núcleos asm | **431,1** (−29 %) | 48,9 | ventana 41, IMDCT 19, `III_decode` 16, DCT32 13, `III_aliasreduce` 6 |
| MP3 FULL 192 kbps, núcleos asm | 442,6 | 47,6 | ventana 40, IMDCT 20, `III_decode` 14, DCT32 12 |
| MP3 FULL + EQ, núcleos asm | 466,7 | 50,8 | ventana 38, IMDCT 17, `III_decode` 14, DCT32 12; el EQ cuesta 35,5 (antes del kernel asm: 66,8) |
| MP3 HALF, libmad original (C) | 456,8 | 32,9 | `synth_half` 35, `III_decode` 17, IMDCT 17, `dct32` 15 |
| MP3 HALF, núcleos asm | **341,3** (−25 %) | 32,9 | ventana de media tasa 26 (88,5; en C eran 161,9), IMDCT 23, `III_decode` 20, DCT32 16 |
| Ogg Vorbis estéreo (Tremor, C) | 473,7 | 29,8 | `mdct_backward` 38, `decode_map` 10, `decode_packed_entry_number` 9, `mdct_unroll_lap` 8 |
| Ogg Vorbis mono (Tremor, C) | 252,6 | 15,0 | `mdct_backward` 35, `decode_packed_entry_number` 13, `decode_map` 11 |
| WMA v2 estéreo (libwma, conversión a PCM, ventana y FFT `pass` en asm; 2026-11-09) | **316,3** (−13 % sobre 364,7; sin asm de la ronda: 379,7) | 24,9 | `wma_decode_block` 28, `ff_imdct_half` 18, FFT `pass` (asm) 17, `wma_pcm` 6, ventana (asm) 4 |
| WMA v1 estéreo (idem) | 316,7 | 24,9 | igual que v2 |
| FLAC estéreo 16 bits (dr_flac, C; perfil nuevo 2026-11-09) | 239,1 | 14,0 | `drflac__decode_samples_with_residual` 87 (Rice + LPC), paso a `int16` 10 |
| FLAC estéreo 24 bits (dr_flac, C) | 260,6 | 12,9 | `drflac__decode_samples_with_residual` 92, paso a `int16` 5 |
| FLAC mono 16 bits (dr_flac, C) | 110,0 | 6,5 | idem |

Los casos de FLAC salen de `targets/asm-test.sh flac_e2e` (la primera vez que dr_flac corre como 68040 big-endian bajo QEMU: los 4 CRC coinciden con el PC). `memcpy`, `memmove`, `memset` y `qsort` de mp368k minilibc son bucles de bytes (más lentos que los de la libc real) y se separan en el informe.

## Modelo de ciclos bajo QEMU, calibrado con Snow (NO es tiempo medido)

Desde 2026-11-09, `targets/asm-prof.sh` cobra a cada instrucción ejecutada su tiempo de la sección 10 del MC68040UM tal como lo cobra el
emulador **Snow** (leído en su `core/src/cpu_m68k/timing040.rs`, commit `178a122`: `muls.l` 20, `muls.w` 16, desplazamientos 2-4, salto tomado 2
y no tomado 3, modos indexados 3-4, casi todo lo demás 1) y simula el caché de instrucciones de 4 KB. **No modela el caché de datos** (QEMU no da las
direcciones de los datos). Calibración contra `audiotest` en Snow (build `2e4aa2c`, libmad en C, ver el registro de abajo): el modelo da 248 % (FULL)
y 184 % (HALF) donde Snow midió 311 % y 231 %, es decir **Snow ≈ 1,25 × modelo** en los dos casos. "Cuadro" = un instante de entrada (en estéreo, un par);
presupuesto a 44,1 kHz y 33,33 MHz: 756 ciclos por cuadro. Todo es MP3 de 128 kbps estéreo (`test128.mp3`), con los perfiles del reproductor.

| Perfil (2026-11-09, núcleos asm) | Antes de la ronda 4 | Después | × 1,25 (estimación para Snow) | Qué cambió |
|---|---|---|---|---|
| FULL (idéntico a libmad) | 217 % | **206 %** | ~258 % | zona no nula en alias y M/S |
| HALF (22 kHz estéreo) | 154 % | **101 %** | ~126 % | DCT de media tasa, 16 subbandas, ventana de 16 bits |
| MONO (44 kHz mono) | ~140 % | **88 %** | ~110 % | mezcla antes de la IMDCT, sin canal S en M/S, ventana de 16 bits |
| HALF_MONO (22 kHz mono) | ~100 % | **52 %** | ~65 % | todo lo anterior |

Las columnas "× 1,25" son una extrapolación con un solo punto de calibración (y con la libmad en C, no con los núcleos asm): **sin medir**. Lo que sí
miden Snow y, sobre todo, el LC 580 se registra abajo.

## Registro cronológico

### 2026-11-09 — `microbench` y `audiotest` en Snow (Infinite Mac, Quadra 800)
Equipo: **emulador Snow** (Infinite Mac); Gestalt mach=35 (Quadra 800), proc=5 (68040), fpu=3, 40 MB. **No es el LC 580.**
System: 8.1 (0x810)   Build: 68k-2e4aa2c   Flags: -m68040 -msoft-float -O2 -fomit-frame-pointer -ffunction-sections -fdata-sections
`audiotest` de ese build decodifica con la **libmad original en C** (no con los núcleos asm). Sonido: Sound Manager 3.2, el hardware emulado solo ofrece 8 bits.

| Prueba (`microbench`, mínimo de 5 pasadas, 33 MHz supuestos) | ns/op | ciclos/op |
|---|---|---|
| `muls.l` 32×32→32 independientes / dependientes | 601,7 / 602,0 | 19,85 / 19,86 |
| `muls.l` 32×32→64 | 601,8 | 19,85 |
| `muls.w` 16×16→32 | 481,5 | 15,88 |
| lectura secuencial 1 / 4 / 8 / 64 KB (por palabra larga, bruto) | 42,2 / 73,5 / 122,3 / 151,8 | 1,39 / 2,42 / 4,03 / 5,00 |
| lectura con tabla 1-2 KB / 4 KB / 8-32 KB | 90 / 127 / 542-547 | 3,0 / 4,2 / 17,9-18,1 |
| copia de 16 bytes, 1 KB: `move.l` / `movem` / `move16` | 253 / 366 / 1105 | 8,3 / 12,1 / 36,5 |
| copia de 16 bytes, 64 KB: `move.l` / `movem` / `move16` | 1570 / 1656 / 1100 | 51,8 / 54,7 / 36,3 |

| `audiotest` (15 s por pista) | decode_load | underruns | Observaciones |
|---|---|---|---|
| test128.mp3 FULL | 311 % (81,3 ms por frame de 26,1) | 321 | no es tiempo real |
| test128.mp3 HALF | 253 % (231 % sin un frame atípico de 1,42 s) | 493 | |
| test192.mp3 FULL | 311 % | 814 | |
| testmono.mp3 FULL | 145 % | 1084 | |
| test128.mp3 HALF, 8 bits forzado | 231 % | 1246 | |

**Lo que sí dice.** Snow cobra `muls.l` 20 ciclos y `muls.w` 16, y muestra un caché de datos de 4 KB y el costo de los fallos: son exactamente los tiempos
del MC68040UM que su código implementa (`timing040.rs`, `cache/m68040.rs`), así que **confirma el modelo de Snow, no el hardware**. Con 20 ciclos por `muls.l`,
las ~49 multiplicaciones por cuadro del MP3 FULL ya cuestan ~980 ciclos de un presupuesto de 756: **FULL estéreo a 44,1 kHz no llega a tiempo real en un
68040 de 33 MHz con la aritmética de libmad**, con o sin asm. El LC 580 tiene el mismo núcleo y reloj; su memoria y su video pueden hacerlo algo más lento
que este Quadra 800, nunca bastante más rápido.
**Lo que no dice.** Nada del LC 580 real. El archivo de `microbench` traía al final un trozo de la salida de `audiotest`; el programa borra y trunca su archivo al escribirlo
(`src/common/textlog.c`), así que probablemente se juntó al copiarlo. Los datos de las dos pruebas se leyeron por separado.
Decisión derivada: ronda 4 del ADR-014 (perfiles reducidos más baratos) y un modelo de ciclos calibrado con estas cifras (sección de arriba).

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

