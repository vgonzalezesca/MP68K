---
name: mac-test-workflow
description: Flujo de pruebas de MP368K en Basilisk II (lógica y regresión) y en el LC 580 real (rendimiento), con protocolo de medición, formato del archivo de resultados y entrega a bench-analyst. Usar al preparar un binario de prueba, pedir mediciones a Víctor o interpretar resultados.
---

# Pruebas: Basilisk II y LC 580

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
Escrito por `bench/microbench.c`. Misma cabecera (`build`, `machine`, `cache`, ...) más
`assumed_clock_mhz`. Cada test es un bloque `test:` / `unit:` / `run,usec_total,ops`
(la columna `ops` reemplaza a `frames,samples_out`, que no aplican a una instrucción).
La pasada 1 es fría. Al final, una sección `summary,...` con
`name,min_usec,med_usec,net_usec,ops,ns_per_op,cycles_per_op,MB_per_s`.
Detalle en `docs/ETAPA1.md`. El agente `bench-analyst` ya conoce esta variante.

## Transferencia al LC 580
Método por definir (pregunta abierta nº 2 de `SPECS.md`). Opciones: disquete de 1,44 MB
con `.dsk`, SCSI, AppleShare/Ethernet, tarjeta CF/IDE. Automatizar en `tools/`.
Para devolver resultados: copiar `bench_results.txt` por el mismo medio.

## Entrega de resultados
Víctor copia el archivo a `bench/results/<fecha>_<equipo>.txt`; `bench-analyst` lo
registra en `docs/PERF_LOG.md` con la plantilla y propone el siguiente paso.

## Comprobaciones antes de pedir una prueba
- [ ] Compila sin warnings relevantes.
- [ ] objdump sin instrucciones de FPU.
- [ ] Pruebas bit a bit en verde en Basilisk II (JIT sí/no).
- [ ] El binario cabe en el medio de transferencia.
- [ ] Hay instrucciones claras para Víctor (qué abrir, qué archivo devolver).
