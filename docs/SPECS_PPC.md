# SPECS PPC — Power Macintosh con PowerPC 601 (rama `ppc`)

Todo número es **aritmética o dato sin medir**, salvo que diga lo contrario. Datos del equipo:
`docs/HARDWARE_PPC601.md`.

## Equipo

Un Power Macintosh con PowerPC 601, **modelo por confirmar**. Sin ese dato se diseña para el piso de la familia: el
6100/60 (60 MHz, bus de 30 MHz, 32 KB de L1). Si el equipo real es más rápido, solo hay más margen.

**Nota:** Víctor mencionó primero el Power Macintosh 6200, pero ese modelo trae un PowerPC **603**, no un 601. Decidió
apuntar al 601. Un binario compilado para el 601 debería correr también en un 603 o 604, sin garantía hasta probarlo.

## Lenguaje

**Solo C**, por decisión de Víctor. Sin ensamblador propio. Tampoco `FPM_PPC` de libmad: se mantiene `FPM_64BIT`, en C y
con el mismo PCM bit a bit que 68k y PC. Si la medición mostrara que hace falta, se reabre la decisión con Víctor.

## Presupuesto de ciclos

A 100 % de CPU, 60 MHz, 44,1 kHz estéreo: 1 152 / 44 100 = 26,12 ms por frame ≈ 1 567 000 ciclos por frame ≈
**680 ciclos por muestra de salida**. El bus de 30 MHz puede limitar antes que la CPU (por medir).

## Port (se ejecuta en la 6-PPC)

| Pieza | Qué cambia |
|---|---|
| Reloj | `targets/mactime.c` llama a `Microseconds` de InterfaceLib, sin `mftb`: el 601 no tiene Time Base |
| Ganchos de menú y arrastre | `targets/hooks.c`: el sistema los llama como código 68k, a través de Mixed Mode. Sin verificar |
| Callbacks | `New...UPP` y un solo descriptor por callback. Falta revisar `audio.c` |
| Alineación | las Multiversal usan `pragma pack(2)` para las estructuras del Toolbox; revisar las propias |
| `macenv.c`, `ui_list.c`, `LMGet...` | revisar variables globales de bajo nivel en Mixed Mode |
| Apps | `decbench`, `decprofile`, `audiotest` y `mp368k`. `microbench` no aplica: mide instrucciones 68k |
| `SIZE`, memoria | la partición y la pila se comportan distinto bajo CFM; revisar `macenv_grow_stack` |

## Ecualizador (ADR-011)

Por coeficientes para MP3, Vorbis y WMA, con el mismo código compartido que 68k. **FLAC usa filtros muestra a muestra,
solo en esta rama** (`targets/eq_iir.c`); ahí se permite `float`, porque el 601 tiene FPU (por verificar). Fijo o `float`
se decide midiendo. QuickTime, sin EQ.

## Criterios de salida

- `decbench` con los mismos CRC que la referencia del PC: el PCM es idéntico al de 68k.
- `objdump` sin `mftb`.
- `mp368k` funciona en un emulador PowerPC, con el mismo protocolo de `docs/UI.md`.
- Reproducción continua de 10 minutos sin cortes en el equipo real: **sin medir** hasta tenerlo.

## Pendiente conocido

Modelo exacto, sistema, sonido y medio de transferencia del equipo. Qué emulador sirve para probar (Infinite Mac: por
verificar).
