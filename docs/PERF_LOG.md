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
(agregar entradas con la plantilla de arriba)
