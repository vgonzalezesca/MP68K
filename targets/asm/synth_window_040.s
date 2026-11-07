| synth_window_040.s — ventana de la síntesis polifásica del MP3 (un slot de 32 muestras), 68040.
|
| Referencia en C: src/synth_ref.c (mp368k_synth_window_full_ref). Misma firma, mismo resultado bit a bit.
| Prueba: tests/asm/test_synth_window.c, bajo qemu-m68k (targets/asm-test.sh). SIN MEDIR en el LC 580:
| no se declara más rápido que la referencia hasta medirlo (ADR-008).
|
| void mp368k_synth_window_full_040(const int32_t *fx, const int32_t *fe, const int32_t *fo,
|                                   const int32_t *D, unsigned pe, unsigned po, int32_t *pcm);
|
| Aritmética exacta de libmad (FPM_64BIT, sin OPT_*, ventana D pre-desplazada 12 bits): cada producto es
| (int32)(((int64)x*y) >> 16), truncado, y las sumas son de 32 bits con envoltura. Un producto 32x32->64 sale de
| muls.l como hi:lo; (hi:lo) >> 16 = (hi << 16) | (lo >>> 16). Como la suma módulo 2^32 es lineal, en vez de
| armar cada producto se acumulan por separado accH = suma(hi) y accL = suma(lo >>> 16), y al final
| resultado = (accH << 16) + accL. La negación de una suma parcial (MLN) es negar accH y accL.
|
| Registros: d0:d1 = lo:hi del producto; d2 = accH; d3 = accL; d4 = po*4; d5 = pe*4; d6 = contador; d7 = 16.
|   a1 = fila sb de fe; a2 = fila sb-1 de fo; a3 = fila sb de D; a4 = D +/- pe/po; a5 = &pcm[sb]; a6 = &pcm[32-sb].
| Preserva d2-d7/a2-a6 (convención de GCC); d0, d1, a0 y a1 son de uso libre. Argumentos: en la pila.
|
| Sin FPU, sin código auto-modificable, sin dependencias de tiempo.

        .text
        .even

| acc = x*y >> 16     (primer producto de una salida)
        .macro  PROD0 f, d
        move.l  \f,%d0
        muls.l  \d,%d1:%d0
        lsr.l   %d7,%d0
        move.l  %d1,%d2
        move.l  %d0,%d3
        .endm

| acc += x*y >> 16
        .macro  PROD f, d
        move.l  \f,%d0
        muls.l  \d,%d1:%d0
        lsr.l   %d7,%d0
        add.l   %d1,%d2
        add.l   %d0,%d3
        .endm

| dst = (accH << 16) + accL
        .macro  FIN dst
        swap    %d2
        clr.w   %d2
        add.l   %d3,%d2
        move.l  %d2,\dst
        .endm

| Ocho productos "+": f[j] * D[idx(j)], idx = 0, 14, 12, 10, 8, 6, 4, 2 (desplazamientos en bytes)
        .macro  PLUS8 fp, dp
        PROD     0(\fp),0(\dp)
        PROD     4(\fp),56(\dp)
        PROD     8(\fp),48(\dp)
        PROD     12(\fp),40(\dp)
        PROD     16(\fp),32(\dp)
        PROD     20(\fp),24(\dp)
        PROD     24(\fp),16(\dp)
        PROD     28(\fp),8(\dp)
        .endm

        .globl  mp368k_synth_window_full_040
mp368k_synth_window_full_040:
        movem.l %d2-%d7/%a2-%a6,-(%sp)
        | pila: 44 bytes guardados + 4 de retorno. Argumentos desde 48(%sp):
        | fx 48, fe 52, fo 56, D 60, pe 64, po 68, pcm 72
        move.l  68(%sp),%d4
        lsl.l   #2,%d4                      | po en bytes
        move.l  64(%sp),%d5
        lsl.l   #2,%d5                      | pe en bytes
        moveq   #16,%d7
        move.l  48(%sp),%a0                 | fx
        move.l  52(%sp),%a1                 | fe (fila 0)
        move.l  56(%sp),%a2                 | fo (fila 0)
        move.l  60(%sp),%a3                 | D (fila 0)
        move.l  72(%sp),%a5                 | pcm

        | ---- subbanda 0: pcm[0] = -(fx * D[0]+po) + (fe fila 0 * D[0]+pe) ----
        lea     (%a3,%d4.l),%a4
        PROD0    0(%a0),0(%a4)
        PROD     4(%a0),56(%a4)
        PROD     8(%a0),48(%a4)
        PROD     12(%a0),40(%a4)
        PROD     16(%a0),32(%a4)
        PROD     20(%a0),24(%a4)
        PROD     24(%a0),16(%a4)
        PROD     28(%a0),8(%a4)
        neg.l   %d2
        neg.l   %d3
        lea     (%a3,%d5.l),%a4
        PLUS8   %a1,%a4
        FIN     (%a5)+                      | a5 = &pcm[1]
        lea     124(%a5),%a6                | a6 = &pcm[32] (se usa con pre-decremento)

        lea     32(%a1),%a1                 | fe fila 1
        lea     128(%a3),%a3                | D fila 1
        moveq   #14,%d6                     | 15 vueltas con dbra

        .balign 16
1:
        | ---- salida pcm[sb] = -(fo fila sb-1 * D[sb]+po) + (fe fila sb * D[sb]+pe, en orden inverso) ----
        lea     (%a3,%d4.l),%a4
        PROD0    0(%a2),0(%a4)
        PROD     4(%a2),56(%a4)
        PROD     8(%a2),48(%a4)
        PROD     12(%a2),40(%a4)
        PROD     16(%a2),32(%a4)
        PROD     20(%a2),24(%a4)
        PROD     24(%a2),16(%a4)
        PROD     28(%a2),8(%a4)
        neg.l   %d2
        neg.l   %d3
        lea     (%a3,%d5.l),%a4
        PLUS8   %a1,%a4
        FIN     (%a5)+

        | ---- salida pcm[32-sb] = fe fila sb * D[sb]-pe + fo fila sb-1 * D[sb]-po ----
        move.l  %a3,%a4
        suba.l  %d5,%a4
        PROD0    0(%a1),60(%a4)
        PROD     4(%a1),68(%a4)
        PROD     8(%a1),76(%a4)
        PROD     12(%a1),84(%a4)
        PROD     16(%a1),92(%a4)
        PROD     20(%a1),100(%a4)
        PROD     24(%a1),108(%a4)
        PROD     28(%a1),116(%a4)
        move.l  %a3,%a4
        suba.l  %d4,%a4
        PROD     28(%a2),116(%a4)
        PROD     24(%a2),108(%a4)
        PROD     20(%a2),100(%a4)
        PROD     16(%a2),92(%a4)
        PROD     12(%a2),84(%a4)
        PROD     8(%a2),76(%a4)
        PROD     4(%a2),68(%a4)
        PROD     0(%a2),60(%a4)
        FIN     -(%a6)

        lea     32(%a1),%a1
        lea     32(%a2),%a2
        lea     128(%a3),%a3
        dbra    %d6,1b

        | ---- subbanda 16: pcm[16] = -(fo fila 15 * D[16]+po); a5 ya apunta a pcm[16] ----
        lea     (%a3,%d4.l),%a4
        PROD0    0(%a2),0(%a4)
        PROD     4(%a2),56(%a4)
        PROD     8(%a2),48(%a4)
        PROD     12(%a2),40(%a4)
        PROD     16(%a2),32(%a4)
        PROD     20(%a2),24(%a4)
        PROD     24(%a2),16(%a4)
        PROD     28(%a2),8(%a4)
        neg.l   %d2
        neg.l   %d3
        FIN     (%a5)

        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts
