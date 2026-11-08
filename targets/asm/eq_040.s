| eq_040.s — núcleo del ecualizador por coeficientes en ensamblador 68040 (ADR-014).
|
| Referencia en C: src/eq_ref.c (mp368k_eq_mulg_range_ref). Misma firma y mismo resultado bit a bit (src/eq_kernels.h).
| Prueba: tests/asm/test_eq_mulg.c bajo qemu-m68k (targets/asm-test.sh). SIN MEDIR en el LC 580: no se declara más rápido que la
| referencia hasta medirlo (ADR-008).
|
| void mp368k_eq_mulg_range_040(int32_t *xr, const int32_t *gain, unsigned n);
|   para cada i: si gain[i] != 2^24, xr[i] = saturar32((xr[i]*gain[i] + 2^23) >> 24)
| El producto de 64 bits (hi:lo) + 2^23 se desplaza 24: cabe en 32 bits si hi está en [-2^23, 2^23); entonces el resultado es
| (hi << 8) | (lo >>> 24). Si no cabe, se satura según el signo de hi. Ganancia 1,0: no se toca (ahorra el muls.l).
| Registros: d0:d1 = lo:hi, d2 = 2^24, d3 = 0, d4 = temporal, d5 = 24, d6 = ganancia, d7 = 2^23; a2 = fin de xr.

        .text
        .even
        .globl  mp368k_eq_mulg_range_040
mp368k_eq_mulg_range_040:
        movem.l %d2-%d7/%a2,-(%sp)          | 28 bytes: argumentos desde 32(%sp)
        move.l  32(%sp),%a0                 | xr
        move.l  36(%sp),%a1                 | gain
        move.l  40(%sp),%d0                 | n
        beq.s   9f
        lsl.l   #2,%d0
        lea     (%a0,%d0.l),%a2             | fin de xr
        move.l  #0x1000000,%d2
        moveq   #0,%d3
        moveq   #24,%d5
        move.l  #0x800000,%d7
1:      move.l  (%a1)+,%d6
        cmp.l   %d2,%d6
        beq.s   2f                          | ganancia 1,0: no cambia
        move.l  (%a0),%d0
        muls.l  %d6,%d1:%d0
        add.l   %d7,%d0
        addx.l  %d3,%d1                     | + 2^23 en 64 bits
        move.l  %d1,%d4
        add.l   %d7,%d4                     | hi + 2^23: en [0, 2^24) si el resultado cabe en 32 bits
        cmp.l   #0x1000000,%d4
        bcc.s   3f                          | sin signo: fuera de rango -> saturar
        lsl.l   #8,%d1
        lsr.l   %d5,%d0
        or.l    %d0,%d1
        move.l  %d1,(%a0)
2:      addq.l  #4,%a0
        cmp.l   %a2,%a0
        bne.s   1b
9:      movem.l (%sp)+,%d2-%d7/%a2
        rts
3:      tst.l   %d1
        bmi.s   4f
        move.l  #0x7FFFFFFF,(%a0)
        bra.s   2b
4:      move.l  #0x80000000,(%a0)
        bra.s   2b
