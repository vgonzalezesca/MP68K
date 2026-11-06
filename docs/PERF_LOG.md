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
Equipo: LC 580 real / Basilisk II (JIT sí|no)
System: 7.x.x
Commit: <hash>   Flags: <-O2 -m68040 -msoft-float ...>
Archivo: <nombre, bitrate, canales>   Perfil: FULL|HALF|MONO|HALF_MONO

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

## Etapa 4 — Kernels

| Kernel | Antes (RTF) | Después (RTF) | Prueba bit a bit | Commit |
|---|---|---|---|---|
|  |  |  |  |  |

## Registro cronológico
(agregar entradas con la plantilla de arriba)
