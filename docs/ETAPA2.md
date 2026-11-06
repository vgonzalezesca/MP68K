# Etapa 2 — Decodificador de pruebas (`decbench`) y la visita al LC 580

Objetivo: el RTF base de libmad en C puro, por perfil, sin audio (SPECS §8, etapa 2).
**Nada de lo que hay aquí está medido en el LC 580.** `docs/PERF_LOG.md` sigue en "sin medir".

## Qué se verificó en el PC (con evidencia)

Con `tools/host-ref.sh` (mismo `src/decoder_core.c` que la app Mac, `FPM_64BIT`):

| Comprobación | Resultado |
|---|---|
| libmad propio vs libmad del sistema (24 casos: 3 archivos × 4 perfiles × 2 modos) | CRC idénticos |
| FULL vs `ffmpeg` | SNR ≈ 99,5 dB, diferencia máxima 1 LSB |
| MONO vs (L+R)/2 de FULL | ≤ 1 LSB (criterio ≤ 2) |
| HALF vs una de cada dos muestras de FULL | idéntico bit a bit |
| HALF_MONO vs MONO decimado | idéntico bit a bit |
| Frames / errores | 2297 frames por archivo, 0 errores |

**Atención con HALF:** `synth_half` de libmad no filtra ni descarta subbandas: calcula el
`dct32` completo y evalúa solo la mitad de las salidas de la ventana. Es decimación sin
filtro (las frecuencias sobre ≈ 11 kHz se pliegan a la banda audible) y ahorra solo parte
de la síntesis. Ver ADR-006. La calidad de HALF **no se ha escuchado**.

## Preparar la visita (en el PC)

```sh
tools/make_testfiles.sh     # genera tests/data/*.mp3 (60 s, 128/192/mono)
tools/host-ref.sh           # oráculo del PC -> tests/data/REFERENCE.txt
tools/build.sh              # compila las 3 apps (verifica FPU)
tools/package-visit.sh      # arma dist/visita1/
```

Puedes reemplazar los MP3 por música tuya con los mismos nombres (`test128.mp3`,
`test192.mp3`, `testmono.mp3`) y correr `tools/host-ref.sh` otra vez; el programa registra
tamaño y CRC-32 de cada archivo.

## En el LC 580 (una sola visita)

Condiciones: sin otras aplicaciones abiertas, sin protector de pantalla, sin tocar el
ratón mientras corre; anotar extensiones activas y el ajuste de caché del sistema.

1. **`sysprobe`** → `sysprobe.txt`. (Responde: ¿sonido de 8 o 16 bits?, ¿qué tasas?, reloj.)
2. **`microbench`** → `bench_results_micro.txt`. Dos veces (renombrar el primero).
3. **`decbench`**: copiarlo **en la misma carpeta que `test128.mp3`, `test192.mp3`,
   `testmono.mp3` y `REFERENCE.txt`**. Abrirlo. Escribe `bench_results_decoder.txt` al
   terminar cada archivo (si hay que abortar, los parciales quedan guardados).
   - Duración: **sin medir**. Si el RTF resulta bajo puede tardar decenas de minutos
     (300 frames × 6 corridas × 10 combinaciones). Si la primera corrida de una
     combinación pasa de 40 s, el programa baja a 3 corridas y lo avisa en el archivo.

Devolver todo a `bench/results/<fecha>_lc580_<nombre>.txt` (fin de línea CR; en Linux
`tr '\r' '\n'`).

## Cómo leer el resultado de `decbench`

- `pcm_crc32: ... -> COINCIDE` en todas las combinaciones = el 68k decodifica **idéntico** al
  PC. Si algo dice `DIFIERE`, lo primero a revisar es el CRC del archivo MP3 (cabecera
  `# archivo ...`): si no coincide con el de `REFERENCE.txt`, el archivo se corrompió al
  transferirlo, no es un problema del decodificador.
- `summary,...,rtf_min,rtf_med,cycles_per_out_sample_min`: RTF ≥ 1,000 es tiempo real; la
  meta de SPECS §3 es ≥ 1,5 (≈ 250 ciclos por muestra de salida). Los ciclos suponen 33 MHz.
- `timer_resolution_usec`: si es ~1000, los tiempos cortos están cuantizados.
- `stack_space`: libmad necesita varios KB de pila; el programa reserva 48 KB extra y se
  niega a decodificar si quedan menos de 24 KB.

## Limitaciones conocidas

- Los MP3 sintéticos (ruido rosa + barrido + acorde) pueden costar distinto en Huffman que
  música real; el RTF final debe confirmarse con archivos reales.
- `decbench` mide 300 frames desde el inicio de cada archivo (≈ 7,8 s), no 2000+ (eso
  corresponde al perfilado por etapas, etapa 3).
- Los tiempos incluyen el reinicio del decodificador (≈ 10 KB de `memset`) en cada corrida.
- Segmentos de código sobre 32 KB: ver ADR-006.
