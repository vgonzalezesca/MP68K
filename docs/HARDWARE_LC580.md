# Hardware: Macintosh LC 580 (Performa 580 / 580CD / 588CD)

Compilado el 2026-10-29 desde fuentes públicas. **Ninguno de estos datos fue verificado
en el equipo real de Víctor.** Donde las fuentes se contradicen se indica. Lo que
importa para el proyecto está marcado con ▶.

Fuentes: EveryMac (LC 580), Low End Mac (LC 580 / Performa 580), apple-history.com,
macdat.net, Apple Support (documento 112226).

## Identificación
| Dato | Valor |
|---|---|
| Familia | Macintosh LC (serie 500), all-in-one, form factor "LC 520" |
| Nombre en código | Dragonkid (apple-history) |
| Número de modelo Apple | M3872 (EveryMac) |
| Gestalt machine ID | 99 |
| Variantes | Performa 580 / 580CD / 588CD (Performa 58x lanzado el 13-abr-1995) |
| Lanzamiento | 3-abr-1995, US$ 1 300 |
| Descontinuación | **contradicción**: 1-ago-1995 (EveryMac) vs. 2-mar-1996 (Low End Mac, Apple) |

## Procesador y caché ▶
| Dato | Valor |
|---|---|
| CPU | Motorola 68LC040, 33 MHz |
| FPU | **ninguna** (se obtiene reemplazando por un 68040 completo) |
| MMU | el 68LC040 incluye MMU pero no FPU (el 68EC040 no tiene ninguna). **Por confirmar** en M68040UM |
| Bus del sistema | 33 MHz (EveryMac: el reloj de entrada es 66 MHz, sin duplicar el procesador) |
| Caché L1 | 4 KB instrucciones + 4 KB datos (apple-history); EveryMac lo resume como "8 KB" |
| Caché L2 | no incluida; opcional por la ranura PDS (tarjetas de terceros, descontinuadas) |
| Rendimiento citado | "29 MIPS", 10,0 relativo al SE (Low End Mac) |
| Direccionamiento | 32 bits |
| Upgrade | tarjetas PowerPC para PDS (Apple, DayStar, Sonnet) |

## Memoria
| Dato | Valor |
|---|---|
| RAM en placa | 4 MB |
| Ranuras | 2 × SIMM de 72 pines, ≥ 80 ns |
| Ranura 1 | 1, 2, 4, 8, 16 o 32 MB |
| Ranura 2 | solo 1, 4 o 16 MB (de una cara) |
| Máximo | 52 MB (4 + 32 + 16) |
| De fábrica | 8 MB |
| ROM | 1 MB (equipo original) |

Nota: la página de Apple Support (112226) muestra ROM de 4 MB, PowerPC 601 a 66 MHz y
FPU. Esos datos son de la **configuración con tarjeta Processor Upgrade**, no del LC 580
original. No aplican a un 68LC040.

## Video
| Dato | Valor |
|---|---|
| VRAM | 1 MB (dos SIMM de VRAM de 512 KB, 60 ns); es el máximo |
| Pantalla | CRT color de 14", 640×480; hasta 16 bits de color a esa resolución |
| Segunda pantalla | requiere tarjeta de video en la ranura PDS |
| Ranura Video I/O | acepta la tarjeta sintonizadora de TV de Apple para Performa |

## Almacenamiento ▶
| Dato | Valor |
|---|---|
| Disco interno | IDE de 3,5", 250 o 500 MB de fábrica (**sin SCSI interno**) |
| Controlador IDE | un solo dispositivo; PIO modo 3 máx. (11,1 MB/s) |
| CD-ROM | 2× (modelos CD) |
| Disquete | 1,44 MB, expulsión manual |
| Advertencia | HD SC Setup de System 7.5.1 o anterior no reconoce discos IDE modernos; usar 7.5.2+ con Drive Setup 1.0.3 o superior |

Para el reproductor: 192 kbps son 24 KB/s. El disco no es un cuello de botella.

## Expansión y puertos
| Dato | Valor |
|---|---|
| Ranuras | 1 × LC PDS, 1 × Comm, 1 × Video I/O |
| Red | sin Ethernet integrado; Ethernet o módem opcionales en la ranura Comm |
| Serie | 2 × mini DIN-8 (RS-422), normalmente limitados a 57,6 kbps |
| SCSI | 1 × DB-25 **externo** |
| ADB | EveryMac: 1; Low End Mac: 2 (teclado y ratón). **Contradicción** |
| Video externo | DB-15, solo con tarjeta opcional |
| Audio | micrófono PlainTalk, auriculares frontales, salida de línea (macdat), altavoz |

## Sonido ▶▶ (el dato más importante para el proyecto)
| Fuente | Qué dice |
|---|---|
| EveryMac | salida **estéreo de 8 bits**; entrada solo micrófono PlainTalk; tasas de muestreo no listadas |
| apple-history.com | salida estéreo de 8 bits (mini jack); entrada mono de 8 bits; altavoz y micrófono mono |
| macdat.net | altavoces estéreo, Line Out, Mic In, Headphone Out |
| Apple Support | no indica profundidad ni tasas |

**Conclusión provisional:** al menos dos fuentes independientes indican **8 bits**. No
encontré las tasas de muestreo ni el chip de audio. Mi proyecto había asumido 16 bits
y 44,1 kHz nativos; **esa suposición está en duda y debe medirse en el equipo** (etapa 0).

Consecuencias posibles (hipótesis, sin verificar):
- Si la salida es de 8 bits y las tasas máximas son bajas (en Macs con ese tipo de
  hardware es habitual 22,254 kHz), decodificar a 44,1 kHz completos no mejoraría lo
  que se escucha, y el perfil `HALF` (≈ 22 kHz) pasaría a ser el objetivo natural.
- Si se entrega PCM de 16 bits y el Sound Manager lo convierte a 8 bits, la conversión
  puede costar CPU. Conviene medir ese costo y, si es relevante, producir directamente
  el formato nativo.
- Con 8 bits el ruido de cuantización (~48 dB de rango) limita la calidad percibida:
  el ruido por redondeo y el dither importan más que la precisión de 28 bits del
  decodificador.

## Sistema operativo
| Dato | Valor |
|---|---|
| De fábrica | System 7.5 |
| Soportado | 7.5 hasta Mac OS 8.1 (EveryMac); Low End Mac menciona 7.1 con Enabler 065 |
| Máximo sin PowerPC | Mac OS 8.1 (el de Víctor) |

## Físico y energía
17,9" alto × 13,5" ancho × 16,5" fondo (45,5 × 34,3 × 41,9 cm); 40,5 lb (18,4 kg);
fuente 90–264 V CA, 47–63 Hz, 40 W máx. (EveryMac; Apple indica 60 W con la tarjeta
PowerPC); batería PRAM alcalina de 4,5 V.

## Diferencias con el LC 575
Placa lógica nueva con IDE en vez de SCSI interno, monitor más barato, ranura Video I/O
y VRAM duplicada (de 512 KB a 1 MB).

## Qué debe confirmar la etapa 0 en el LC 580 real
Programa `sysprobe` que escriba `sysprobe.txt` con:
- `Gestalt`: `gestaltMachineType` (esperado 99), `gestaltProcessorType`, `gestaltFPUType`
  (esperado ninguna), `gestaltSystemVersion`, `gestaltPhysicalRAMSize`,
  `gestaltSoundAttr`, versión del Sound Manager, `gestaltMMUType`.
- Salida de sonido: formatos y tasas soportados (consulta al Sound Manager:
  `SndGetInfo` con `siSampleRate`, `siSampleSize`, `siNumberChannels`,
  `siSampleRateAvailable`, `siSampleSizeAvailable`; **verificar nombres** en `Sound.h`).
- Estado de la caché del procesador (si es consultable) y tamaño del heap disponible.
