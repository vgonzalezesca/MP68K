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

## Pruebas bit a bit
- `tests/test_<k>.c` genera vectores con semilla fija, ejecuta referencia y asm, y
  compara **todos** los resultados. Falla con índice, entrada, esperado y obtenido.
- Incluir: ceros, máximos, mínimos, valores que provoquen saturación, y 10 000
  vectores aleatorios.
- Ejecutar el mismo test en Basilisk II con JIT activado **y** desactivado.
- Opcional: ejecutar los kernels bajo `qemu-m68k -cpu m68040` si hay un
  cross-compilador `m68k-linux-gnu` disponible (verificar). Solo sirve para
  corrección; no mide rendimiento real.

## Metodología de medición
- Bucle con N iteraciones, `Microseconds()` antes y después, descartar la primera
  pasada, reportar mediana y mínimo.
- Conviene un modo "datos en caché" y otro "datos fríos" porque la caché de datos del
  040 es de solo 4 KB.
- Registrar en `docs/PERF_LOG.md` (etapa 4).

## Convención confirmada / sintaxis confirmada
(rellenar tras la etapa 0)
