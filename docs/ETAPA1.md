# Etapa 1 — Microbenchmarks de CPU (`microbench`)

Objetivo: medir en el LC 580 cuánto cuestan las operaciones que dominarán el
decodificador, para decidir qué va a ensamblador y con qué estrategia (SPECS §8, etapa 1).
**Nada de lo que hay aquí está medido todavía.** Las filas de `docs/PERF_LOG.md` siguen
en "sin medir" hasta que devuelvas el archivo de resultados.

## Qué mide

| Test | Qué responde |
|---|---|
| `mulsl_32_indep` / `mulsl_32_dep` | costo de `MULS.L` 32×32→32, y cuánto pesa que cada multiplicación espere a la anterior |
| `mulsl_64_indep` | costo de `MULS.L` 32×32→64: la operación de la síntesis polifásica |
| `mulsw_indep` | costo de `MULS.W`; si es mucho menor, vale la pena evaluar productos de 16 bits |
| `read_seq_{1,2,4,8,16,64}k` | lectura secuencial: dónde se acaba el beneficio de la caché de datos de 4 KB |
| `table_{1,2,4,8,16,32}k` | lectura con índice salteado en una tabla: modela la ventana de síntesis |
| `copy_{move16,movem,movel}_{1k,64k}` | si `MOVE16` realmente gana a `MOVEM` y `MOVE.L`, dentro y fuera de la caché |

Cada test corre 6 veces: la primera (caché fría) se informa aparte y el resumen usa las
otras 5 (mínimo y mediana). Los tests de instrucción restan el costo del bucle (`base_loop`);
los de tabla restan `table_base` (el `add`+`and` del índice); los de lectura y copia son
**brutos**.

## Cómo ejecutarlo

1. `tools/build.sh`, en la rama `68k`, deja `dist/68k/microbench/` (`.bin` y `.dsk`).
2. Llevar `microbench` al LC 580 como `sysprobe` (ver `docs/ETAPA0.md`).
3. Condiciones: sin otras aplicaciones abiertas, sin protector de pantalla, sin tocar el
   ratón mientras corre, AppleTalk desactivado si se puede. Anotar las extensiones activas.
4. Abrir `microbench`. Corre solo y avisa cuando termina (no debería pasar de un par de
   minutos; si pasa de 5, anotar cuánto tardó).
5. Ejecutarlo **dos veces** (cerrando y volviendo a abrir) para ver la variación entre
   ejecuciones. Cada ejecución sobrescribe `bench_results_micro.txt`: renombrar el primero
   antes de la segunda vuelta.

## Devolver los resultados

```
bench/results/2026-MM-DD_lc580_micro_1.txt
bench/results/2026-MM-DD_lc580_micro_2.txt
```

Con fin de línea de Mac clásico (CR). En Linux: `tr '\r' '\n' < archivo > archivo_lf.txt`.

## Cómo leer el resumen

`summary,name,min_usec,med_usec,net_usec,ops,ns_per_op,cycles_per_op,MB_per_s`

- `cycles_per_op` **supone 33 MHz** (dato tuyo, sin medir en el equipo). Si el reloj real
  fuera otro, los ciclos cambian en proporción; los nanosegundos no.
- `MB_per_s` en los tests de copia cuenta bytes copiados (no lectura + escritura).
- `timer_resolution_usec` (cabecera) es el menor paso del reloj observado. Un test cuyo
  `min_usec` no sea al menos ~100 veces esa resolución está cuantizado: no confiar en él.
  (En Infinite Mac salió ~1000 us: un emulador, no sirve para esta medición.)
- Una diferencia grande entre `min` y `med` indica interrupciones o actividad de fondo.
- El ajuste de caché del procesador no es consultable desde la app: anotarlo a mano.

## Limitaciones conocidas

- El ensamblador de `targets/microbench.c` (rama `68k`) es instrumental de medición, no un kernel del
  decodificador. Las instrucciones de multiplicación usan operandos fijos con aspecto
  aleatorio; si el 040 tuviera tiempos que dependen de los datos, esto no lo captura.
- No se desactivan las interrupciones durante la medición; se confía en el mínimo de 5 pasadas.
- Si no hay memoria para los dos buffers de 64 KB (primero `NewPtr`, luego memoria
  temporal), los tests de memoria se omiten y el archivo lo dice.
- En un emulador (Basilisk II, Infinite Mac) el programa sirve para comprobar que corre
  y escribe el archivo; sus tiempos no representan al 68LC040 y no van a `PERF_LOG.md`.
