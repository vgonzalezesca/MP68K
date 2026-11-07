# SPECS 68k — Macintosh LC 580 (rama `68k`)

Todo número es **aritmética o dato sin medir**, salvo que diga lo contrario. Datos del equipo:
`docs/HARDWARE_LC580.md`.

## Equipo

68LC040 a 33 MHz, sin FPU, cachés de 4 KB + 4 KB (por verificar), 16 MB, Mac OS 8.0/8.1, CF por IDE, sin CD.
Sonido sin resolver: Víctor indica 22 kHz, 16 bits, estéreo; EveryMac y apple-history indican 8 bits.

## Presupuesto de ciclos

A 100 % de CPU, 33 MHz, 44,1 kHz estéreo: **374 ciclos por muestra de salida** (SPECS §3). Meta: RTF ≥ 1,5, o sea
≈ 250 ciclos por muestra para decodificar, y el resto para Sound Manager, disco e interfaz. A 22 kHz estéreo, el
doble.

## Kernels en asm (se ejecuta en la 6-68K)

Por decisión de Víctor, lo más sensible de la decodificación de cada formato pasa a ensamblador 68040 sin esperar
mediciones del LC 580 (ADR-008). El orden de cada lista es una sospecha, no una medición: `decprofile` en el
emulador sirve de guía débil, y conteos de instrucciones como estimación marcada.

| Formato | Estado (2026-11-07) | Pendiente |
|---|---|---|
| MP3 | **hechos** (PC y QEMU, sin medir): ventana de síntesis (a mano), DCT32 e IMDCT larga (generados), conversión a PCM, 8 bits y ganancia (a mano) | requantización, Huffman, IMDCT corta y bloques de inicio/fin/mixtos |
| FLAC | **descartado por ahora** (ADR-014): GCC ya genera la predicción LPC óptima; el conteo de ceros con `bfffo` podría ser más lento | decidir con mediciones del LC 580 |
| Vorbis | en C (Tremor) | IMDCT, residuos, piso, ventana; EQ por coeficientes |
| WMA | en C (libwma) | IMDCT, ruido y exponentes, ventana; EQ por coeficientes |

Cada kernel: referencia en C (`src/*_ref.c`, algunas generadas), `targets/asm/*_040.s`, prueba bit a bit bajo `qemu-m68k`
(`targets/asm-test.sh`) y selector C/asm al compilar (`-DMP368K_ASM`). Ningún kernel se declara más rápido hasta medirlo en
el LC 580 (ADR-014: el banco de QEMU solo prueba corrección).

**Advertencia honesta:** Vorbis y WMA pueden no llegar a tiempo real en este equipo, ni con asm. Es lo primero que
hay que medir de esta rama.

## Sound Manager (se ejecuta en la 6-68K)

Sacarle el máximo provecho significa, sobre todo, **no pedirle conversiones**:

- Producir directamente el formato y la tasa nativos que informe `sysprobe`, para que el Sound Manager no convierta
  ni remuestree. Hoy se negocia la profundidad, pero no la tasa.
- Tamaño de los doble buffer según la salida real.
- Evaluar `GetSoundOutputInfo` (necesita glue propia: las Multiversal no traen el Component Manager en 68k).
- Medir el costo del propio Sound Manager, con y sin decodificar.

## Ecualizador (ADR-011)

Aritmética con `MULS.L` ≈ 20 ciclos (dato por verificar), sin medir:

| Método | Multiplicaciones/s (44,1 kHz estéreo) | CPU a 33 MHz |
|---|---|---|
| Filtros muestra a muestra, 10 bandas × 5 multiplicaciones | 4,41 M | ≈ 267 % (133 % a 22 kHz): **inviable** |
| Ganancia por coeficiente | 88 200 | ≈ 5-10 %, contando accesos a memoria |

Por eso en esta rama el EQ solo existe por coeficientes: MP3, Vorbis y WMA. **FLAC no tiene EQ aquí** y QuickTime no
lo tiene en ningún equipo. Los controles se desactivan cuando no aplica.

## Criterios de salida

- Las pruebas bit a bit en verde en PC y emulador. Sin instrucciones `f*` (`targets/check-nofpu.sh`).
- PCM del binario 68k idéntico a la referencia, con los kernels activados y desactivados.
- Reproducción continua de 10 minutos sin cortes en el LC 580, por formato: **sin medir** hasta que Víctor pueda.

## Pendiente conocido

- **Regla de 32 KB por segmento sin resolver:** Main 70 KB, Runtime 44 KB (ADR-006). Corrió en el emulador, lo que no
  demuestra nada sobre el LC 580, y los códecs la agravarán. Si falla, `SegmentMap(filename)` permite repartir Main.
- Mediciones de `sysprobe`, `microbench`, `decbench`, `decprofile` y `audiotest` en el equipo real.
