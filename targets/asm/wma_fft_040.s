| wma_fft_040.s — pasada de mariposas de la FFT split-radix de libwma (`pass` de fft-ffmpeg.c), 68040.
|
| Referencia en C: src/wma_ref.c (mp368k_wma_fft_pass_ref). Misma firma, mismo resultado bit a bit. Prueba:
| tests/asm/test_wma_fft.c bajo qemu-m68k (targets/asm-test.sh). SIN MEDIR en el LC 580: no se declara más rápido que la
| referencia hasta medirlo (ADR-008).
|
| void mp368k_wma_fft_pass_040(int32_t *z, unsigned STEP, unsigned n, const int32_t *T);
|
| Cada "transformada" procesa 4 números complejos z[k], z[k+n], z[k+2n], z[k+3n]: t1,t2 = XPROD31(z[2n], w) y t5,t6 =
| XNPROD31(z[3n], w) con MULT31(x,y) = hi(x*y) << 1, y luego la mariposa. Como (A<<1)+(B<<1) = (A+B)<<1 módulo 2^32, cada t
| se arma sumando las dos partes altas de muls.l y desplazando UNA vez.
| Recorrido idéntico al de C: una transformada sin giro (TRANSFORM_ZERO), una con W10, el bucle do-while de dos W10, y el
| bucle while de dos W01 hacia atrás por la tabla.
|
| Registros: d0 = parte baja del producto (d1:d0 -> hi:lo); d2..d5 = t1,t2,t5,t6; d6 = wre; d7 = wim.
|   a0..a3 = z[k], z[k+n], z[k+2n], z[k+3n] (cada transformada los avanza 8 bytes); a4 = w; a5 = STEP*4 bytes; a6 = tope de w.
| Preserva d2-d7/a2-a6 (convención de GCC). Argumentos: en la pila.
|
| Sin FPU, sin código auto-modificable, sin dependencias de tiempo.

        .text
        .even

| t1,t2 = XPROD31(a2, wre, wim); t5,t6 = XNPROD31(a3, wre, wim)   (wre en d6, wim en d7)
        .macro  MULS
        move.l  (%a2),%d0
        muls.l  %d6,%d1:%d0                 | d1 = hi(re2*wre)
        move.l  4(%a2),%d0
        muls.l  %d7,%d2:%d0                 | d2 = hi(im2*wim)
        add.l   %d1,%d2
        add.l   %d2,%d2                     | t1
        move.l  4(%a2),%d0
        muls.l  %d6,%d3:%d0                 | d3 = hi(im2*wre)
        move.l  (%a2),%d0
        muls.l  %d7,%d1:%d0                 | d1 = hi(re2*wim)
        sub.l   %d1,%d3
        add.l   %d3,%d3                     | t2
        move.l  (%a3),%d0
        muls.l  %d6,%d4:%d0                 | d4 = hi(re3*wre)
        move.l  4(%a3),%d0
        muls.l  %d7,%d1:%d0                 | d1 = hi(im3*wim)
        sub.l   %d1,%d4
        add.l   %d4,%d4                     | t5
        move.l  4(%a3),%d0
        muls.l  %d6,%d5:%d0                 | d5 = hi(im3*wre)
        move.l  (%a3),%d0
        muls.l  %d7,%d1:%d0                 | d1 = hi(re3*wim)
        add.l   %d1,%d5
        add.l   %d5,%d5                     | t6
        .endm

| mariposa con t1,t2,t5,t6 en d2..d5; avanza los cuatro punteros
        .macro  BFLY
        move.l  %d4,%d0
        sub.l   %d2,%d0                     | temp1 = t5 - t1
        add.l   %d2,%d4                     | temp2 = t5 + t1
        move.l  (%a0),(%a2)
        sub.l   %d4,(%a2)                   | a2.re = a0.re - temp2
        add.l   %d4,(%a0)                   | a0.re += temp2
        move.l  4(%a1),4(%a3)
        sub.l   %d0,4(%a3)                  | a3.im = a1.im - temp1
        add.l   %d0,4(%a1)                  | a1.im += temp1
        move.l  %d3,%d0
        sub.l   %d5,%d0                     | temp1 = t2 - t6
        add.l   %d5,%d3                     | temp2 = t2 + t6
        move.l  (%a1),(%a3)
        sub.l   %d0,(%a3)                   | a3.re = a1.re - temp1
        add.l   %d0,(%a1)                   | a1.re += temp1
        move.l  4(%a0),4(%a2)
        sub.l   %d3,4(%a2)                  | a2.im = a0.im - temp2
        add.l   %d3,4(%a0)                  | a0.im += temp2
        addq.l  #8,%a0
        addq.l  #8,%a1
        addq.l  #8,%a2
        addq.l  #8,%a3
        .endm

        .macro  XF_W10                      | wim = w[0], wre = w[1]
        move.l  (%a4),%d7
        move.l  4(%a4),%d6
        MULS
        BFLY
        .endm

        .macro  XF_W01                      | wre = w[0], wim = w[1]
        move.l  (%a4),%d6
        move.l  4(%a4),%d7
        MULS
        BFLY
        .endm

        .globl  mp368k_wma_fft_pass_040
mp368k_wma_fft_pass_040:
        movem.l %d2-%d7/%a2-%a6,-(%sp)
        | pila: 44 bytes guardados + 4 de retorno. Argumentos desde 48(%sp): z 48, STEP 52, n 56, T 60
        move.l  48(%sp),%a0
        move.l  56(%sp),%d0
        lsl.l   #3,%d0                      | n * 8 bytes (un complejo = 8 bytes)
        lea     (%a0,%d0.l),%a1
        lea     (%a1,%d0.l),%a2
        lea     (%a2,%d0.l),%a3
        move.l  52(%sp),%d0
        lsl.l   #2,%d0
        move.l  %d0,%a5                     | a5 = STEP * 4
        move.l  60(%sp),%a4
        lea     4096(%a4),%a6               | a6 = T + 1024 palabras (w_end)
        adda.l  %d0,%a4                     | a4 = w = T + STEP

        | TRANSFORM_ZERO: t1 = z[2n].re, t2 = z[2n].im, t5 = z[3n].re, t6 = z[3n].im
        move.l  (%a2),%d2
        move.l  4(%a2),%d3
        move.l  (%a3),%d4
        move.l  4(%a3),%d5
        BFLY

        XF_W10
        adda.l  %a5,%a4                     | w += STEP
        .balign 4
1:
        XF_W10
        adda.l  %a5,%a4
        XF_W10
        adda.l  %a5,%a4
        cmpa.l  %a6,%a4
        bcs.w   1b                          | while (w < w_end)

        move.l  60(%sp),%a6                 | w_end = T
2:
        cmpa.l  %a6,%a4
        bls.w   3f                          | while (w > w_end)
        XF_W01
        suba.l  %a5,%a4
        XF_W01
        suba.l  %a5,%a4
        bra.w   2b
3:
        movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts
