---
name: mac-sound-playback
description: Reproducir audio PCM en Mac OS clásico con el Sound Manager (canal, doble buffer, callbacks en tiempo de interrupción), consulta de capacidades por Gestalt y diseño del buffer circular y del bucle principal cooperativo para MP368K. Usar al escribir src/audio*.c o diagnosticar cortes de sonido.
---

# Sound Manager para el reproductor

Todo lo de este archivo es de memoria de la API clásica: **confirmar nombres, firmas y
constantes en los headers de Retro68 (`Sound.h`, `Gestalt.h`) y en Inside Macintosh:
Sound** antes de usarlo.

## Verificado en la etapa 5a (ver docs/ETAPA5.md)
- Los callbacks del Sound Manager son `pascal` en el 68k: el compilador de Retro68 lo soporta como
  palabra clave y nombra la función en MAYÚSCULAS; termina con `rtd #8` (limpia los argumentos).
- Las Multiversal Interfaces NO traen `initMono`/`initStereo` ni `volumeCmd` (se definen con prefijo `MP_`),
  ni el Component Manager. Sí traen `SndPlayDoubleBuffer`, `SndDoubleBufferHeader`, `dbBufferReady` y `dbLastBuffer`.
- Disposición verificada por compilación: `dbSoundData` en el desplazamiento 16, `dbFlags` en 4,
  `SndChannel.userInfo` en 12, `SndDoubleBufferHeader` de 24 bytes.
- Las variables globales de Retro68 son direcciones absolutas (no relativas a A5): el callback no necesita fijar A5.
- `GetApplLimit` no tiene glue; ApplLimit se lee en la dirección 0x130.
- **Confirmado por oído (Infinite Mac, SM 3.2):** `initStereo` 0x00C0 / `initMono` 0x0080, 16 bits y 8 bits sin
  signo (silencio 0x80) con `SndPlayDoubleBuffer`. Falta confirmarlo en el LC 580.
- Un `SIZE (-1)` propio en el `.r` de la app reemplaza al de `Retro68APPL.r` (queda uno solo).

## Capacidades del equipo
- Consultar con `Gestalt` el atributo de sonido (`gestaltSoundAttr`) y la versión del
  Sound Manager (`gestaltSoundMgrVersion`, si existe). Interesa: salida estéreo, 16 bits,
  tasas soportadas. Ver `SndGetInfo` / `SPBGetDeviceInfo` para detalles del dispositivo.
- Registrar lo que reporte el LC 580 en `docs/PERF_LOG.md` (etapa 0) y en `SPECS.md`.
- Constantes de tasa en `Sound.h` están en punto fijo (por ejemplo `rate44khz`,
  `rate22khz`; este último es 22,254 kHz, no 22,05 kHz — **verificar**, y comprobar si
  existe una constante para 22 050 Hz exactos). Un desfase de tasa cambia el tono
  ≈ 0,9 %.

## Opciones de reproducción
1. **Doble buffer** (`SndPlayDoubleBuffer` con `SndDoubleBufferHeader`): el Sound
   Manager llama a una función de refill cuando un buffer se agota. Requiere
   Sound Manager 3.0 o superior (**verificar** disponibilidad en la System del LC 580).
2. **Canal + `bufferCmd` + `callBackCmd`**: encolar bloques de PCM y recibir un
   callback al terminar. Más manual, compatible con versiones antiguas.
Elegir según la versión de System; implementar detrás de una interfaz común
(`audio_open/start/stop/close`) para poder cambiar.

## Formato de datos
- Cabecera de sonido muestreado (`SoundHeader`/`ExtSoundHeader`) con muestras de 16 bits
  `twosComplement`, estéreo, y el orden de bytes del 68k (big-endian: coincide con la
  salida natural del decodificador, sin intercambio).
- Dejar el formato exacto de cabecera para confirmar con la documentación.

## Reglas de tiempo de interrupción
- Los callbacks se ejecutan en tiempo de interrupción: **no** mover/alocar memoria, no
  usar File Manager síncrono, no llamar a Toolbox que pueda mover el heap, no llamar a
  libc con `malloc`.
- Si el callback usa variables globales de la app, asegurar el A5 correcto
  (`SetCurrentA5`/`SetA5` o el mecanismo equivalente; **verificar** con Retro68). Usar
  el parámetro de usuario del canal para pasar el contexto en lugar de globales
  cuando sea posible.
- El callback solo: avanzar índice de lectura, marcar buffer libre, y (si hace falta)
  pedir tiempo al bucle principal por una bandera.

## Diseño del flujo de datos
```
 decodificador (bucle principal) ──► ring PCM (bloques fijos, p. ej. 8-16 frames)
                                         │
              Sound Manager callback ◄───┘  (consume bloques, nunca decodifica)
```
- Tamaño del ring: suficiente para tolerar una pausa de interfaz de ~100-200 ms. Calcular
  con la tasa real (bytes = tasa × canales × 2 × segundos). Memoria con `NewPtr` en
  `audio_open`.
- Cuenta de subdesbordamientos (`underrun`) visible en modo depuración.
- Pausa: detener el canal con comandos del Sound Manager y mantener posición.

## Bucle principal cooperativo
- Decodificar por porciones de un frame; después de cada porción, `WaitNextEvent` con
  `sleep = 0` si el ring está casi lleno o el RTF lo permite.
- Si la interfaz necesita animación, limitar el redibujado (barra de progreso a 4-10
  Hz).
- Con RTF cercano a 1, no hay holgura para la UI: bajar de perfil o reducir
  redibujado.

## Lectura de disco
- Lectura asíncrona (`PBReadAsync`) hacia un buffer circular de bytes, con compleción
  que solo marca estado. Tamaño de bloque múltiplo del bloque de disco, y buffer
  suficiente para ≥ 1 s a 192 kbps (≈ 24 KB) como mínimo; medir en disco real.

## Pruebas
- Basilisk II: valida el flujo, no los tiempos. El sonido en el emulador puede ser
  irregular, y no refleja el LC 580.
- LC 580: reproducción continua 10 minutos con contador de underruns en 0.

## Pendiente de verificar en la etapa 0
- Versión de Sound Manager, tasas disponibles, hardware de 16 bits, opciones de
  doble buffer, coste de CPU del propio Sound Manager (medir con y sin decodificar).
