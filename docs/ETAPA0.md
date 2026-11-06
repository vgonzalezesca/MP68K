# Etapa 0 — Toolchain y `sysprobe`

Objetivo: tener Retro68 funcionando y averiguar qué hardware de sonido tiene de verdad
el LC 580 (¿8 o 16 bits?, ¿qué tasas?) antes de decidir el perfil de calidad.

## 1. Preparar el host (CachyOS, una sola vez)

```sh
sudo pacman -S --needed boost ruby      # lo único que faltaba al 2026-10-29
tools/build-toolchain.sh                 # 11 min con 12 hilos; log en ~/Retro68-build/build-toolchain.log
```

## 2. Compilar

```sh
git switch 68k     # o ppc; master no compila apps Mac
tools/build.sh
```

Compila, verifica que no haya instrucciones de FPU (`targets/check-nofpu.sh`, en la rama `68k`) y deja los
artefactos en `dist/68k/sysprobe/`:

| Archivo | Para qué |
|---|---|
| `sysprobe.bin` | MacBinary: para llevar al LC 580 (decodificar con StuffIt Expander) |
| `sysprobe.dsk` | imagen HFS: montable en Basilisk II o grabable en disquete |
| `sysprobe.APPL` + `.rsrc/` + `.finf/` | carpeta compartida (`extfs`) de Basilisk II |

## 3. Ejecutar

Hacerlo **tres veces**: Basilisk II con JIT activado, Basilisk II con JIT desactivado y
el LC 580 real.

1. Copiar `sysprobe` a una carpeta con escritura (no en un disquete protegido ni un CD).
2. Abrirlo con doble clic. Se abre una ventana con los resultados.
3. Al final dice `sysprobe.txt guardado: err=0`. Presionar Return para salir.
4. El archivo `sysprobe.txt` queda **en la misma carpeta que la aplicación**.

En el LC 580, anotar además a mano:
- la lista de extensiones activas (o "estándar de Mac OS 8.1"),
- el ajuste de caché del procesador si hay un panel de control para eso,
- si los altavoces y los auriculares suenan normales con un sonido de alerta.

## 4. Devolver los resultados

Copiar cada archivo al repositorio con este nombre:

```
bench/results/2026-MM-DD_lc580_sysprobe.txt
bench/results/2026-MM-DD_basilisk-jit_sysprobe.txt
bench/results/2026-MM-DD_basilisk-nojit_sysprobe.txt
```

El archivo usa fin de línea de Mac OS clásico (CR). Si se ve en una sola línea en Linux:

```sh
tr '\r' '\n' < sysprobe.txt > sysprobe_lf.txt
```

Si la aplicación se cuelga o no abre, anotar en qué paso y, si aparece, el número de
error del cuadro de diálogo (por ejemplo "error de tipo 3"). Eso también es un
resultado útil.

## Qué se espera ver en el LC 580 (sin verificar)

| Clave | Esperado | Por qué importa |
|---|---|---|
| `machine` | 99 | confirma el modelo |
| `processor` | 5 (68040) | |
| `fpu` | 0 (ninguna) | confirma la ausencia de FPU |
| `system` | 8.1.0 | |
| `sound_mgr_version` | 3.x | doble buffer y `SndGetInfo` |
| `SndGetInfo 'ssav'` | ¿8? ¿16? | **dato clave**: profundidad de salida |
| `SndGetInfo 'srav'` | ¿22254? ¿44100? | **dato clave**: tasas de salida |
| `usec_por_60_ticks` | ≈ 997 500 | valida el reloj del banco de pruebas |

Si la salida resulta ser de 8 bits y ≤ 22 kHz, el perfil `HALF` pasa a ser el objetivo
(ver `SPECS.md` §11, pregunta 4).
