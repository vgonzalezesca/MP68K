| synth_window_full16_040.s — ventana de la síntesis polifásica COMPLETA del MP3 (un slot de 32 muestras) con PRECISIÓN REDUCIDA (16x16 bits), 68040.
|
| Referencia en C: src/synth_ref.c (mp368k_synth_window_full16_ref). Misma firma, mismo resultado bit a bit.
| Prueba: tests/asm/test_synth_window_full16.c, bajo qemu-m68k (targets/asm-test.sh). SIN MEDIR en el LC 580:
| no se declara más rápido que la referencia hasta medirlo (ADR-008).
| DERIVADO de synth_window_040.s (mismo recorrido, mismos registros de direcciones); solo cambian los productos.
|
| void mp368k_synth_window_full16_040(const int32_t *fx, const int32_t *fe, const int32_t *fo,
|     const int16_t *D16, unsigned pe, unsigned po, int32_t *pcm);
|
| Cada producto es (x >> 16) * D16[k] con muls.w: la palabra alta de x (big-endian) está en la misma dirección que x, así que el
| operando de memoria de muls.w ya es x >> 16; D16 va en d0. Suma de 32 bits con envoltura en d2 y la salida es d2 << 2.
| Registros: d0 = producto; d2 = acumulador; d4 = po*2; d5 = pe*2; d6 = contador; a3 = fila de D16 (64 bytes por fila).
| Preserva d2-d7/a2-a6 (convención de GCC). Argumentos: en la pila.
|
| Sin FPU, sin código auto-modificable, sin dependencias de tiempo.

        .text
        .even

| acc = (x >> 16) * d     (primer producto de una salida): d de 16 bits; la palabra alta de x está en su misma dirección
        .macro  PROD0 f, d
        move.w  \d,%d0
        muls.w  \f,%d0
        move.l  %d0,%d2
        .endm

| acc += (x >> 16) * d
        .macro  PROD f, d
        move.w  \d,%d0
        muls.w  \f,%d0
        add.l   %d0,%d2
        .endm

| dst = acc << 2
        .macro  FIN dst
        lsl.l   #2,%d2
        move.l  %d2,\dst
        .endm

| Ocho productos "+": f[j] * D16[idx(j)], idx = 0, 14, 12, 10, 8, 6, 4, 2 (desplazamientos en bytes: x de 4, D16 de 2)
        .macro  PLUS8 fp, dp
        PROD     0(\fp),0(\dp)
        PROD     4(\fp),28(\dp)
        PROD     8(\fp),24(\dp)
        PROD     12(\fp),20(\dp)
        PROD     16(\fp),16(\dp)
        PROD     20(\fp),12(\dp)
        PROD     24(\fp),8(\dp)
        PROD     28(\fp),4(\dp)
        .endm

        .globl  mp368k_synth_window_full16_040
mp368k_synth_window_full16_040:
        movem.l %d2-%d7/%a2-%a6,-(%sp)
        | pila: 44 bytes guardados + 4 de retorno. Argumentos desde 48(%sp):
        | fx 48, fe 52, fo 56, D 60, pe 64, po 68, pcm 72
        move.l  68(%sp),%d4
        add.l   %d4,%d4                     | po en bytes (D16 de 2 bytes)
        move.l  64(%sp),%d5
        add.l   %d5,%d5                     | pe en bytes
        move.l  48(%sp),%a0                 | fx
        move.l  52(%sp),%a1                 | fe (fila 0)
        move.l  56(%sp),%a2                 | fo (fila 0)
        move.l  60(%sp),%a3                 | D16 (fila 0)
        move.l  72(%sp),%a5                 | pcm

        | ---- subbanda 0: pcm[0] = -(fx * D[0]+po) + (fe fila 0 * D[0]+pe) ----
        lea     (%a3,%d4.l),%a4
        PROD0    0(%a0),0(%a4)
        PROD     4(%a0),28(%a4)
        PROD     8(%a0),24(%a4)
        PROD     12(%a0),20(%a4)
        PROD     16(%a0),16(%a4)
        PROD     20(%a0),12(%a4)
        PROD     24(%a0),8(%a4)
        PROD     28(%a0),4(%a4)
        neg.l   %d2
        lea     (%a3,%d5.l),%a4
        PLUS8   %a1,%a4
        FIN     (%a5)+                      | a5 = &pcm[1]
        lea     124(%a5),%a6                | a6 = &pcm[32] (se usa con pre-decremento)

        lea     32(%a1),%a1                 | fe fila 1
        lea     64(%a3),%a3                | D16 fila 1
        moveq   #14,%d6                     | 15 vueltas con dbra

        .balign 16
1:
        | ---- salida pcm[sb] = -(fo fila sb-1 * D[sb]+po) + (fe fila sb * D[sb]+pe, en orden inverso) ----
        lea     (%a3,%d4.l),%a4
        PROD0    0(%a2),0(%a4)
        PROD     4(%a2),28(%a4)
        PROD     8(%a2),24(%a4)
        PROD     12(%a2),20(%a4)
        PROD     16(%a2),16(%a4)
        PROD     20(%a2),12(%a4)
        PROD     24(%a2),8(%a4)
        PROD     28(%a2),4(%a4)
        neg.l   %d2
        lea     (%a3,%d5.l),%a4
        PLUS8   %a1,%a4
        FIN     (%a5)+

        | ---- salida pcm[32-sb] = fe fila sb * D[sb]-pe + fo fila sb-1 * D[sb]-po ----
        move.l  %a3,%a4
        suba.l  %d5,%a4
        PROD0    0(%a1),30(%a4)
        PROD     4(%a1),34(%a4)
        PROD     8(%a1),38(%a4)
        PROD     12(%a1),42(%a4)
        PROD     16(%a1),46(%a4)
        PROD     20(%a1),50(%a4)
        PROD     24(%a1),54(%a4)
        PROD     28(%a1),58(%a4)
        move.l  %a3,%a4
        suba.l  %d4,%a4
        PROD     28(%a2),58(%a4)
        PROD     24(%a2),54(%a4)
        PROD     20(%a2),50(%a4)
        PROD     16(%a2),46(%a4)
        PROD     12(%a2),42(%a4)
        PROD     8(%a2),38(%a4)
        PROD     4(%a2),34(%a4)
        PROD     0(%a2),30(%a4)
        FIN     -(%a6)

        lea     32(%a1),%a1
        lea     32(%a2),%a2
        lea     64(%a3),%a3
        dbra    %d6,1b

        | ---- subbanda 16: pcm[16] = -(fo fila 15 * D[16]+po); a5 ya apunta a pcm[16] ----
        lea     (%a3,%d4.l),%a4
        PROD0    0(%a2),0(%a4)
        PROD     4(%a2),28(%a4)
        PROD     8(%a2),24(%a4)
        PROD     12(%a2),20(%a4)
        PROD     16(%a2),16(%a4)
        PROD     20(%a2),12(%a4)
        PROD     24(%a2),8(%a4)
        PROD     28(%a2),4(%a4)
        neg.l   %d2
        FIN     (%a5)

        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts
