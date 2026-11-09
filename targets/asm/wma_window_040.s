| wma_window_040.s — ventana de la IMDCT de WMA (libwma), 68040.
|
| Referencia en C: src/wma_ref.c (mp368k_wma_fmul_add_add_ref y mp368k_wma_fmul_reverse_ref). Misma firma, mismo resultado
| bit a bit. Prueba: tests/asm/test_wma_window.c bajo qemu-m68k (targets/asm-test.sh). SIN MEDIR en el LC 580: no se declara
| más rápido que la referencia hasta medirlo (ADR-008).
|
| void mp368k_wma_fmul_add_add_040(int32_t *dst, const int32_t *src0, const int32_t *src1, int len);
|     dst[i] += (int32)(((int64)src0[i] * src1[i]) >> 31)              (suma de 32 bits con envoltura)
| void mp368k_wma_fmul_reverse_040(int32_t *dst, const int32_t *src0, const int32_t *src1, int len);
|     dst[i]  = (int32)(((int64)src0[i] * src1[len-1-i]) >> 31)
|
| Un producto 32x32->64 sale de muls.l como d1:d0 = hi:lo; (hi:lo) >> 31 = (hi << 1) | (lo >>> 31), y eso es
| add.l d0,d0 (X = bit 31 de lo) seguido de addx.l d1,d1. len > 0 y <= 65536 (se cuenta con dbra); len <= 0: no hace nada.
| Preserva d2/a2 (convención de GCC); d0, d1, a0 y a1 son de uso libre. Argumentos: en la pila.
|
| Sin FPU, sin código auto-modificable, sin dependencias de tiempo.

        .text
        .even

        .globl  mp368k_wma_fmul_add_add_040
mp368k_wma_fmul_add_add_040:
        movem.l %d2/%a2,-(%sp)
        | pila: 8 bytes guardados + 4 de retorno. Argumentos desde 12(%sp): dst 12, src0 16, src1 20, len 24
        move.l  24(%sp),%d2
        ble.s   2f
        move.l  12(%sp),%a0
        move.l  16(%sp),%a1
        move.l  20(%sp),%a2
        subq.l  #1,%d2
        .balign 4
1:
        move.l  (%a1)+,%d0
        muls.l  (%a2)+,%d1:%d0
        add.l   %d0,%d0
        addx.l  %d1,%d1
        add.l   %d1,(%a0)+
        dbra    %d2,1b
2:
        movem.l (%sp)+,%d2/%a2
        rts

        .globl  mp368k_wma_fmul_reverse_040
mp368k_wma_fmul_reverse_040:
        movem.l %d2/%a2,-(%sp)
        move.l  24(%sp),%d2
        ble.s   2f
        move.l  12(%sp),%a0
        move.l  16(%sp),%a1
        move.l  20(%sp),%a2
        move.l  %d2,%d0
        lsl.l   #2,%d0
        adda.l  %d0,%a2                     | a2 = src1 + len (se lee con pre-decremento)
        subq.l  #1,%d2
        .balign 4
1:
        move.l  (%a1)+,%d0
        muls.l  -(%a2),%d1:%d0
        add.l   %d0,%d0
        addx.l  %d1,%d1
        move.l  %d1,(%a0)+
        dbra    %d2,1b
2:
        movem.l (%sp)+,%d2/%a2
        rts
