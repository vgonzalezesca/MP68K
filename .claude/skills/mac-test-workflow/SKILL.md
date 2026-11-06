---
name: mac-test-workflow
description: Flujo de pruebas de MP368K en emuladores (lógica y regresión) y en los equipos reales, LC 580 y Mac con 601 (rendimiento), con protocolo de medición, formato del archivo de resultados y entrega a bench-analyst. Usar al preparar un binario de prueba, pedir mediciones a Víctor o interpretar resultados.
---

# Pruebas: emuladores, LC 580 y Mac con 601

## Qué compilación probar dónde
| Compilación | Dónde | Comando |
|---|---|---|
| `dist/68k` | Infinite Mac o Basilisk II (68k), LC 580 | `tools/build.sh` en la rama `68k` |
| `dist/ppc` | emulador PowerPC (por verificar cuál), Mac con 601 | `tools/build.sh` en la rama `ppc` |
| `dist/fat` | **los dos**: es la única forma de probar que arranca en cada lado | `tools/build-all.sh` |

Infinite Mac (`infinitemac.org`) es el emulador que usa Víctor en 68k (ROM Quadra 650, Mac OS 8.1). Falta verificar qué
opciones PowerPC ofrece. Todo lo medido en un emulador es solo lógica: nunca va a `PERF_LOG.md` como dato de un equipo real.
La primera línea de cada archivo de resultados indica la compilación (`build: 68k-<hash>...` o `build: ppc-<hash>...`).

## Reparto de responsabilidades
- **Claude Code** compila, empaqueta, escribe el programa de prueba y analiza los
  resultados.
- **Víctor** ejecuta en el emulador y en el LC 580 y devuelve el archivo de resultados.
- Claude Code **no** puede ejecutar el LC 580 ni una Mac real. Todo resultado de
  rendimiento viene de Víctor; sin él, se declara "sin medir".

## Basilisk II (solo lógica y regresión)
- Necesita una **ROM de un Mac 68040** y un sistema instalado; Víctor los aporta (no
  se incluyen en el repositorio). Si es necesario, extraer la ROM del propio equipo.
- Configuración habitual (verificar nombres en la versión usada): CPU 68040 (`cpu 4`),
  modelo adecuado (`modelid`), RAM ≥ 16 MB, `jit true/false`, carpeta compartida
  (`extfs`) con la salida de Retro68 (`.APPL` + `.rsrc` + `.finf`).
- Ejecutar **siempre dos veces**: JIT activado y desactivado. Una diferencia de
  comportamiento indica código dependiente del emulador (SMC, cachés).
- La velocidad en el emulador no representa el LC 580: no usar para decidir perfiles.

## LC 580 real (rendimiento)
- Cerrar aplicaciones, desactivar protectores de pantalla y extensiones pesadas;
  anotar la lista de extensiones activas.
- Caché del procesador: dejar la configuración por defecto del sistema y anotarla;
  si se prueba otra, registrarlo.
- Ejecutar cada prueba ≥ 5 veces; descartar la primera para el análisis (se reporta
  aparte).

## Mac con 601 real (rendimiento)
- Mismo protocolo que el LC 580. Primero `sysprobe`, para confirmar modelo, CPU nativa, sistema y sonido
  (`docs/HARDWARE_PPC601.md`). `microbench` no aplica: mide instrucciones 68k.
- Un binario 68k corriendo en un PowerPC va emulado: una FAT debe arrancar por el lado PEF. `native_cpu` no lo demuestra,
  porque informa la CPU, no el lado que corre; compararlo con `decbench` nativo si hay dudas.

## Programa de benchmark (bench/)
Aplicación con una única ventana (o consola mínima) que:
1. Ejecuta las pruebas del banco (etapas 1-3) según un menú o una lista fija.
2. Mide con `Microseconds()` (Time Manager, verificar disponibilidad en la System).
3. **Escribe los resultados en un archivo de texto** `bench_results.txt` en la carpeta de
   la aplicación, con el formato de abajo.
4. No usa audio salvo en la etapa 5.

### Formato de `bench_results.txt`
```
# MP368K bench
build: <hash> <fecha> <flags>
machine: <Gestalt: modelo, CPU, System, RAM, sonido>
cache: <si se pudo consultar>
test: <nombre>
file: <nombre> <bitrate> <canales>
profile: FULL|HALF|MONO|HALF_MONO
run,usec_total,frames,samples_out
1,...
2,...
...
```
Mantener CSV simple para que `bench-analyst` lo procese con scripts de `tools/`.

### Variante de la etapa 1: `bench_results_micro.txt`
Escrito por `targets/microbench.c` (solo rama `68k`). Misma cabecera (`build`, `machine`, `cache`, ...) más
`assumed_clock_mhz`. Cada test es un bloque `test:` / `unit:` / `run,usec_total,ops`
(la columna `ops` reemplaza a `frames,samples_out`, que no aplican a una instrucción).
La pasada 1 es fría. Al final, una sección `summary,...` con
`name,min_usec,med_usec,net_usec,ops,ns_per_op,cycles_per_op,MB_per_s`.
Detalle en `docs/ETAPA1.md`. El agente `bench-analyst` ya conoce esta variante.

## Transferencia a los equipos
LC 580: Compact Flash con adaptador IDE (`targets/package-visit.sh`, rama `68k`, arma `dist/visita1/`). Mac con 601: por
definir. Para devolver resultados: copiar `bench_results.txt` por el mismo medio.

## Entrega de resultados
Víctor copia el archivo a `bench/results/<fecha>_<equipo>.txt`; `bench-analyst` lo
registra en `docs/PERF_LOG.md` con la plantilla y propone el siguiente paso.

## Comprobaciones antes de pedir una prueba
- [ ] Compila sin warnings relevantes.
- [ ] 68k: objdump sin instrucciones de FPU. PPC: objdump sin `mftb`.
- [ ] Pruebas bit a bit en verde en Basilisk II (JIT sí/no).
- [ ] El binario cabe en el medio de transferencia.
- [ ] Hay instrucciones claras para Víctor (qué abrir, qué archivo devolver).
