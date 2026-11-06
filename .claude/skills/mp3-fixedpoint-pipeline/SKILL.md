---
name: mp3-fixedpoint-pipeline
description: Mapa del decodificador MP3 de punto fijo (libmad) para MP368K: etapas, qué funciones las implementan, opciones de configuración, perfiles de calidad (FULL/HALF/MONO) y qué medir. Usar al integrar, modificar o perfilar libmad o al diseñar perfiles de calidad.
---

# Pipeline MP3 (MPEG-1/2 Layer III) con libmad

## Verificado en la etapa 2 (ver ADR-006 y docs/ETAPA2.md)
- Versión 0.15.1b en `third_party/libmad/`; `mad.h` se regenera (`tools/gen-mad-h.sh`).
- Opciones `FPM_*`: no hay `FPM_68K`; se usa `FPM_64BIT` (compila a `muls.l` 64 bits, sin libgcc).
- Solo `main_data` y `overlap` usan malloc, y se pueden suministrar estáticos.
- `synth_half` = decimación sin filtro. MONO por mezcla en subbandas funciona.

## Fuente
libmad (MAD, punto fijo, GPL). Ubicar el código en `third_party/libmad/` con su licencia.
Descargar una versión estable (la última conocida es 0.15.1b; **verificar** y registrar
la versión y el checksum en `docs/DECISIONS.md`). Los nombres de función de abajo son
de memoria: **confirmarlos leyendo el código** antes de basarse en ellos.

## Etapas y dónde viven (verificar)
| Etapa | Archivo / función (probable) | Sospecha de costo |
|---|---|---|
| Sincronización y cabecera | `stream.c`, `frame.c`, `header` | bajo |
| Side info, scalefactors | `layer3.c` | bajo |
| Huffman | `III_huffdecode`, `huffman.c` | medio, crece con el bitrate |
| Requantización | `III_requantize` | medio |
| Reorden (bloques cortos) | `III_reorder` | bajo |
| Estéreo (MS/intensidad) | `III_stereo` | bajo |
| Antialias | `III_aliasreduce` | bajo |
| IMDCT (36/12) + overlap | `III_imdct_l`, `III_imdct_s`, `III_overlap` | medio |
| Inversión de frecuencias | `III_freqinver` | bajo |
| Síntesis polifásica | `synth.c`: `dct32`, `synth_full`, `synth_half` | **alto (hipótesis)** |
| Conversión a PCM | código del usuario (redondeo/saturación de `mad_fixed_t`) | medio |

Confirmar el orden real de costo con la etapa 3 de `SPECS.md`. No optimizar por intuición.

## Formato numérico
- `mad_fixed_t`: entero con signo de 32 bits, 28 bits fraccionarios (`MAD_F_FRACBITS`),
  rango nominal [-8, 8). Verificar en `fixed.h`.
- La multiplicación `mad_f_mul` se define según `FPM_*` en `fixed.h`/`config`.
  Revisar las opciones disponibles (hay variantes de exactitud/velocidad, como una
  aproximada que usa productos de 32 bits). **No asumir** que exista una `FPM_*`
  específica para 68k; si no existe, definir una propia y documentarla.
- Salida: el reproductor convierte `mad_fixed_t` a 16 bits (redondeo, saturación; el
  ejemplo `minimad` de libmad muestra un patrón, verificar). Opcional: ruido
  (dither) — solo si el RTF lo permite.

## Opciones de configuración (verificar en `config.h`/`mad.h`)
- `OPT_SPEED` vs `OPT_ACCURACY`: velocidad vs. exactitud.
- `OPT_SSO`: variante de síntesis más rápida con pérdida de exactitud.
- `MAD_OPTION_HALFSAMPLERATE` / `synth_half`: salida a mitad de tasa (la API existe
  según recuerdo; confirmar que funcione en la versión usada).
- Desactivar `ASO_*` y `HAVE_*` que no apliquen al objetivo.
- Activar solo lo que se compruebe necesario; documentar la combinación final.

## Perfiles de calidad
| Perfil | Cambio |
|---|---|
| FULL | todo el pipeline, 44,1 kHz estéreo |
| HALF | **verificado:** `MAD_OPTION_HALFSAMPLERATE` es decimación sin filtro con `dct32` completo (no descarta subbandas); ver ADR-006 |
| MONO | mezcla L+R antes de la síntesis (o decodificar un solo canal si el modo lo permite) |
| HALF_MONO | ambas |
- Atención: en estéreo conjunto (MS) mezclar L+R correctamente tras el paso de
  estéreo; en intensity stereo se necesita cuidado.
- Los perfiles que cambian el PCM deben tener su referencia en C propia para las
  pruebas (`tests/`).

## Plan de perfilado (etapa 3)
1. Marcar cada etapa con contadores `Microseconds()` acumulados en variables globales
   (sin `printf` en el bucle).
2. Decodificar ≥ 2000 frames por archivo de prueba.
3. Reportar % por etapa y RTF por perfil, en el LC 580.
4. Entregar a `bench-analyst` para priorizar.

## Referencias externas (no incluir su código sin revisar licencia)
- Estándar ISO/IEC 11172-3 (MPEG-1 Audio) y 13818-3 (MPEG-2 Audio).
- mpg123: tiene rutas de síntesis a media/cuarto de tasa (`synth_2to1`, `synth_4to1`)
  como inspiración de diseño; licencia LGPL.
