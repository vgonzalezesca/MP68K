# Etapa 3 — Costo por etapa (`decprofile`)

Objetivo: saber **dónde** se va el tiempo del decodificador en el LC 580, para decidir qué
kernel pasa a ensamblador y en qué orden (SPECS §8, etapa 3). **Nada está medido todavía**:
la tabla de `docs/PERF_LOG.md` sigue en "sin medir". Las hipótesis (la síntesis domina) son
solo hipótesis.

## Cómo mide

- Se compila una **copia** de libmad con un parche (`third_party/patches/libmad-0.15.1b-profile.patch`,
  9 líneas en `layer3.c`). El original de `third_party/libmad/` no se toca. `tools/prepare-libmad-prof.sh`
  hace la copia y aplica el parche.
- Cada marca suma a su cubeta el tiempo desde la marca anterior. La síntesis, la mezcla MONO y
  la conversión a PCM las cronometra `src/decoder_core.c` alrededor de cada frame.
- `decprofile` corre cada combinación **sin reloj** (tiempo real del decodificador) y **con
  reloj** (reparto por etapa) y calcula cuánto distorsionan las marcas.
- Combinaciones: `test192` FULL y HALF, `test128` FULL, `testmono` FULL. Por combinación:
  3 corridas sin reloj + 4 con reloj, de 700 frames (4900 frames en total, sobre los 2000 que
  pide SPECS). La primera de cada tipo es fría y no entra en la estadística.
- Duración estimada: **sin medir**. Si una corrida fría pasa de 120 s, baja a 2 corridas y lo
  avisa.

## Cubetas

| Cubeta | Contenido | Limitación |
|---|---|---|
| `side` | CRC, side info, copia de `main_data` | |
| `scalef` | scalefactors | |
| `huff_requant` | Huffman **y** requantización | libmad requantiza dentro de `III_huffdecode`: no se pueden separar sin cambiar más código |
| `stereo`, `reorder`, `alias` | estéreo MS/intensidad, reorden de bloques cortos, reducción de alias | `stereo` es 0 en `test192` (se codificó en estéreo normal, no conjunto) |
| `imdct_overlap` | IMDCT + overlap + inversión de frecuencias | van intercaladas por subbanda |
| `other_decode` | sincronía, cabecera y el resto de `mad_frame_decode` | se calcula por diferencia |
| `monomix` | mezcla L+R del perfil MONO | |
| `synth` | síntesis polifásica entera (dct32 + ventana) | una sola cubeta |
| `pcm` | conversión a 16 bits (redondeo y saturación) | |

## Qué mirar en el resultado

- `overhead_pct`: cuánto más lento va con marcas. Si es alto (por ejemplo > 5 %), el reparto
  exagera las etapas cortas; hay `marks_per_frame` marcas por frame y `clock_cost_ns` por lectura.
- `timer_resolution_usec`: si es grueso (~1000), los porcentajes de las etapas cortas no son
  confiables (cada marca mide intervalos de microsegundos).
- `pcm_crc32 ... COINCIDE`: el audio es idéntico al del PC con 700 frames.
- Los porcentajes son de la corrida instrumentada más rápida; el RTF real es `rtf_plain`.

## En el PC (solo validación de la instrumentación)

`tools/host-ref.sh` comprueba que, con y sin marcas, el PCM es idéntico y que las cubetas suman
el tiempo. Los porcentajes del PC (x86) **no** predicen los del 68k.

## Orden de la visita

`sysprobe` → `microbench` (dos veces) → `decbench` → `decprofile`, todos con los MP3 y
`REFERENCE.txt` en la misma carpeta que la aplicación (ver `docs/ETAPA2.md`). Devolver
`bench_results_profile.txt` a `bench/results/<fecha>_lc580_profile.txt`.
