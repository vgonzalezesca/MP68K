# Hardware: Power Macintosh con PowerPC 601

Compilado el 2026-11-06 desde fuentes públicas. **Ninguno de estos datos fue verificado en un equipo real.** Falta
saber qué modelo usará Víctor; los datos de abajo son del **Power Macintosh 6100/60**, el piso de la familia, usado como
referencia de diseño. Lo que importa para el proyecto está marcado con ▶.

## Por qué no el 6200

Víctor mencionó primero el Power Macintosh 6200 con un 601, pero ese modelo trae un **PowerPC 603 a 75 MHz**, con FPU
integrada, 16 KB de L1, 256 KB de L2 y bus de sistema de 37,5 MHz (EveryMac, Wikipedia). Ante la contradicción, Víctor
eligió apuntar a un Mac con 601.

## Procesador y caché ▶

| Dato | Valor | Fuente |
|---|---|---|
| CPU | PowerPC 601, 60 MHz (modelos posteriores, 66 MHz) | EveryMac, Wikipedia |
| Bus del sistema | 30 MHz | Macintosh Repository, Retrotech |
| Caché L1 | 32 KB. Unificada (instrucciones y datos): de memoria, **por verificar** en MPC601UM | Macintosh Repository |
| Caché L2 | 256 KB, opcional | Retrotech |
| FPU | integrada: de memoria, **por verificar** con `sysprobe` y MPC601UM | — |
| Otros modelos con 601 | 7100 y 8100: de memoria, velocidades **por verificar** | — |

## Reloj ▶

**El 601 no tiene Time Base:** no existe `mftb`. Tiene los registros RTCU (segundos) y RTCL (nanosegundos), con
`mfrtcu`/`mfrtcl` (código de Linux para el 601). El proyecto no los usa directamente: `targets/mactime.c` llama a
`Microseconds` de InterfaceLib. `objdump` debe confirmar que no aparezca `mftb`.

## Memoria y disco

RAM de fábrica de 8 o 16 MB; disco de 160 o 250 MB; CD-ROM 2× opcional (EveryMac). Máximos y tipo de SIMM: por verificar
según el modelo.

## Sonido ▶

**Sin datos:** las fuentes consultadas no confirman profundidad ni tasas. No asumir 16 bits ni 44,1 kHz hasta correr
`sysprobe`.

## Sistema operativo

Por confirmar según el modelo y lo que tenga instalado Víctor.

## Qué debe confirmar `sysprobe` en el equipo

`native_cpu` (0x0101 para un 601), `proc_clock_hz`, `fpu`, `machine`, `system`, `phys_ram`, versión del Sound Manager,
formatos y tasas de salida.

## Fuentes

- [EveryMac: Power Macintosh 6100/60](https://everymac.com/systems/apple/powermac/specs/powermac_6100_60.html)
- [Macintosh Repository: 6100/60](https://www.macintoshrepository.org/mac-specs/110-power-macintosh-6100-60-pc-)
- [Retrotech: Power Macintosh 6100](https://wiki.retrotechcollection.com/Power_Macintosh_6100)
- [Wikipedia: Power Macintosh 6100](https://en.wikipedia.org/wiki/Power_Macintosh_6100)
- [EveryMac: Power Macintosh 6200/75](https://everymac.com/systems/apple/powermac/specs/powermac_6200_75.html)
- [Wikipedia: Power Macintosh 6200](https://en.wikipedia.org/wiki/Power_Macintosh_6200)
- [Linux: time code for 601 processors](https://source-hub.etos-pro.ru/microsoft/WSL2-Linux-Kernel/commit/96c44507601d64f29b8ccc867637292e326c7019)
