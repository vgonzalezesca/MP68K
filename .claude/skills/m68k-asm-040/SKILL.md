---
name: m68k-asm-040
description: Guía para escribir y optimizar ensamblador Motorola 68040 (68LC040 sin FPU) en la rama 68k de MP368K: convención de llamada GCC, particularidades del 040, plantilla de kernel, pruebas bit a bit contra C, selector C/asm y metodología de medición. Usar al crear o modificar cualquier archivo en targets/asm/.
---

# Ensamblador 68040 para kernels de audio

## Principios
1. **Se puede escribir antes de medir (ADR-008), pero nada se declara más rápido sin
   medir.** El orden de los kernels es una sospecha: `decprofile` en el emulador es una guía
   débil y los conteos de instrucciones son estimaciones marcadas como tales. Si la medición del
   LC 580 dice que un kernel no ayuda, vuelve a C.
2. Cada kernel: `src/<k>_ref.c` (referencia, en `master`) + `targets/asm/<k>_040.s` (rama `68k`) +
   `tests/test_<k>.c` (bit a bit) + **selector C/asm al compilar** + medición cuando sea posible.
3. Sin código auto-modificable. Sin saltos a datos. Sin depender de temporización.
4. Sin FPU: toda aritmética en enteros.

## Hechos del 68040 que se usan (marcar "verificar" hasta citar M68040UM)
- `MULS.L <ea>,Dh:Dl` (32×32→64) está implementado en el 68040 (el 68060 lo perdió).
  Es la instrucción central de la síntesis. Costo en ciclos: **verificar** en el manual
  (capítulo de tiempos de ejecución) y **medir** en el LC 580 (etapa 1).
- Cachés: 4 KB instrucciones + 4 KB datos (verificar), líneas de 16 bytes. Alinear a 16
  los datos calientes. Los bucles deben caber en la caché de instrucciones.
- `MOVE16 (Ax)+,(Ay)+` copia 16 bytes alineados a 16 sin pasar por registros
  (verificar restricciones de alineación y su comportamiento con la caché de datos).
- Accesos desalineados a palabras/largas son más lentos: evitarlos en el camino caliente.
- Pipelines: el 040 puede solapar instrucciones simples; evitar dependencias de
  dirección (`An` usado inmediatamente después de modificarse). **Verificar con
  medición**, no por intuición.
- El 68LC040 **no tiene FPU** (la MMU sí está presente, a confirmar en M68040UM; el
  68EC040 es el que carece de ambas). No usar instrucciones `F*`. No tocar instrucciones
  de MMU: es territorio del sistema operativo.
- `MOVEM` es eficiente para guardar/restaurar, pero costoso para pocos registros;
  medir.

## Convención de llamada
No asumir. Compilar una función C equivalente con `-S -O2 -m68040 -msoft-float` y copiar:
- Registros que se pueden destruir (scratch) vs. que hay que preservar.
- Cómo llegan los argumentos (en la pila en el ABI estándar de GCC m68k, **verificar**)
  y dónde sale el valor de retorno (`d0`, y quizá `a0` para punteros).
- Anotar la convención confirmada al final de este archivo ("Convención confirmada").

## Sintaxis del ensamblador
El `as` de Retro68 es GNU as para m68k. Hay dos estilos (MIT y Motorola/MRI según
opciones). **Confirmar con un archivo de prueba** cuál acepta por defecto y con qué
opciones (`--register-prefix-optional`, `-mmri`...). Documentar el resultado abajo y
mantener un único estilo en todo el repositorio.

## Plantilla de kernel (adaptar a la sintaxis confirmada)
```
/* targets/asm/dct32_040.s — DCT de 32 puntos, punto fijo mad_fixed_t (28 bits)
 * Referencia: src/dct32_ref.c   Prueba: tests/test_dct32.c
 * Entradas : (pila) puntero in[32], puntero out[32]
 * Preserva : d2-d7/a2-a6
 * Notas    : sin FPU, sin SMC, datos alineados a 16 bytes
 */
        .text
        .even
        .globl  _dct32_040          /* verificar prefijo de símbolos del toolchain */
_dct32_040:
        /* movem.l d2-d7/a2-a3,-(sp) ... */
        /* cuerpo desenrollado */
        /* movem.l (sp)+,d2-d7/a2-a3 */
        rts
```

## Estrategias de optimización a evaluar (en orden de probable retorno)
1. **Síntesis polifásica:** ventana de 512 coeficientes por 32 muestras. Reducir
   multiplicaciones (simetrías), usar acumulación en 64 bits solo donde haga falta,
   evaluar productos de menor precisión (16×16 o 32×16) si la calidad lo tolera y es
   más rápido en el 040 (**medir**).
2. **DCT32:** algoritmo de baja cantidad de multiplicaciones, desenrollado completo.
3. **IMDCT 36/12:** desenrollado, tablas contiguas.
4. **Requantización:** tablas de potencias en 4/3 en lugar de calcular.
5. **Huffman:** tablas planas por nivel, lectura de bits con acumulador de 32 bits.
6. **Conversión a PCM:** redondeo y saturación con `MOVE16` solo si ayuda (medir).

Otros formatos (6-68K; lista completa en `docs/SPECS_68K.md`): residuos Rice y LPC de FLAC; IMDCT, residuos y
piso de Vorbis; IMDCT de WMA; ganancia por coeficiente del EQ. Elegir la estrategia tras leer el código real de
cada biblioteca (skill `audio-formats`).

## Antes de escribir un kernel: medir y aplicar la compuerta (ADR-014)
1. `targets/asm-prof.sh [caso]` da las instrucciones dinámicas por función bajo QEMU, en costo marginal (**conteo, no tiempo**). Casos: `mp3 mp3half mp3orig mp3eq vorbis wma2 ...`.
   Si una función aparece como `?`, es asm sin tipo de función: el script la resuelve con `nm`.
2. **Compuerta:** que pese ≥ ~8 % de su formato y que un diseño concreto quite ≥ ~25 % de las instrucciones del bucle frente a GCC (compilar con `-S -O2 -m68040` y contar).
   Lo que no pase se anota como descartado, con su conteo, en ADR-014 (FLAC, `III_aliasreduce`, `mdct_unroll_lap`...).
3. **Las multiplicaciones mandan:** `muls.l` 32×32→64 es lo caro del 040 y un kernel que no reduce multiplicaciones mejora poco. El costo real está **sin medir**: correr `microbench`
   en el LC 580 antes de seguir invirtiendo.
4. Tras escribirlo: prueba unitaria bit a bit **y `targets/asm-mutate.sh <prueba> '<sed>' ...`** (cada mutante debe fallar; falla si el sed no cambió el archivo), y el e2e que corresponda.

## Pruebas bit a bit
- `tests/test_<k>.c` genera vectores con semilla fija, ejecuta referencia y asm, y
  compara **todos** los resultados. Falla con índice, entrada, esperado y obtenido.
- Incluir: ceros, máximos, mínimos, valores que provoquen saturación, y 10 000
  vectores aleatorios.
- Ejecutar el mismo test en Basilisk II con JIT activado **y** desactivado.
- Se ejecutan bajo `qemu-m68k -cpu m68040` con `targets/asm-test.sh` (ELF autónomo con el GCC y el `ld` de Retro68, sin cross-compilador). Solo sirve para
  corrección; no mide rendimiento real.

## Metodología de medición
- Bucle con N iteraciones, `Microseconds()` antes y después, descartar la primera
  pasada, reportar mediana y mínimo.
- Conviene un modo "datos en caché" y otro "datos fríos" porque la caché de datos del
  040 es de solo 4 KB.
- Registrar en `docs/PERF_LOG.md` (etapa 4).

## Convención confirmada / sintaxis confirmada (2026-11-07, Retro68 GCC 16.1.0, `as` de binutils 2.46)
- **Sintaxis:** GNU as con mnemónicos Motorola y registros con `%` (`move.l 4(%sp),%d0`, `muls.l %d2,%d1:%d0`, `movem.l %d2-%d7/%a2-%a6,-(%sp)`). Es la
  que emite el propio GCC con `-S`. Macros con `.macro`. **No** llamar a una macro como una instrucción real (`mac`...): se renombraron a `PROD`.
- **Símbolos sin guion bajo** (`mp368k_synth_window_full_040`, no `_mp368k...`). `int` y punteros de 4 bytes; `int32_t` es `long`, `mad_fixed_t` es `int`:
  al llamar a un núcleo desde libmad hay que convertir los punteros.
- **Llamada (GCC m68k):** argumentos en la pila, de derecha a izquierda; primer argumento en `4(%sp)` al entrar; resultado en `d0`. `d0`, `d1`, `a0` y `a1` son de
  uso libre; **`d2-d7` y `a2-a6` se preservan** (guardarlos con `movem.l`). Tras guardar N bytes, el primer argumento está en `N+4(%sp)`.
- **`muls.l` 32×32→64:** `muls.l <ea>,Dh:Dl` deja `Dl` = parte baja y `Dh` = alta. `(int32)(((int64)x*y) >> 28)` = `(hi << 4) | (lo >>> 28)`; con `>> 16`,
  `(hi << 16) | (lo >>> 16)`. Como la suma módulo 2^32 es lineal, en una suma de productos se acumulan `suma(hi)` y `suma(lo >>> n)` por separado y se juntan al final.
  `muls.l #imm,Dh:Dl` admite constantes de 32 bits. No usar el mismo registro como `<ea>` y `Dh`.
- **`X / 2` de C redondea hacia cero:** `tst.l d; bpl.s 1f; addq.l #1,d; 1: asr.l #1,d`. Un `asr.l #1` solo no basta.
- **Desplazamientos inmediatos:** el conteo va de 1 a 8; para 16 o 28 se usa un registro (`moveq #28,%d7; lsr.l %d7,%d0`) o `swap`.
- Etiquetas numéricas locales (`1:`, `1b`, `1f`) se pueden repetir; `dbra` cuenta en 16 bits.
- **Alineación del bucle:** `.balign 16` rellena con `nop` en secciones de código (verificado con `objdump`).
- **Probar:** `targets/asm-test.sh` (QEMU, solo corrección). **Mutantes:** cambiar a propósito una constante, un desplazamiento o un signo en una copia del `.s` y
  comprobar que la prueba falla; una prueba que no detecta un mutante no prueba nada. Si el sed del mutante no cambió el archivo, el "OK" es falso.
- **Generador de código recto** (`targets/asm/gen/dflow.py`): DCT32 e IMDCT salen del texto de libmad; sirve para otra rutina recta, no para FFT con bucles de tamaño variable.
- **Cuándo NO escribir asm:** compilar antes la función con `-S -O2 -m68040`. Si GCC ya emite lo que se escribiría (p. ej. la predicción LPC de FLAC), no hay ganancia.
