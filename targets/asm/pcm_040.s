| pcm_040.s — conversiones de PCM en ensamblador 68040: scale16 (salida de la síntesis -> int16), u8 y gain16.
|
| Referencias en C: src/pcm_ref.c (mp368k_pcm_*_ref). Misma firma y mismo resultado bit a bit (src/pcm_kernels.h).
| Prueba: tests/asm/test_pcm.c bajo qemu-m68k (targets/asm-test.sh). SIN MEDIR en el LC 580: no se declara más
| rápido que la referencia hasta medirlo (ADR-008).
|
| Sin FPU, sin código auto-modificable. Preservan d2-d7/a2-a6 (convención de GCC); argumentos en la pila.

        .text
        .even

| d0 = clamp((d0 + 2^12) >> 13, -32768, 32767). Equivale al orden de libmad (acotar y luego >> 13): ver pcm_kernels.h.
        .macro  SC
        add.l   %d4,%d0
        asr.l   %d3,%d0
        cmp.l   %d5,%d0
        ble.s   2f
        move.l  %d5,%d0
2:      cmp.l   %d6,%d0
        bge.s   3f
        move.l  %d6,%d0
3:
        .endm

| void mp368k_pcm_scale16_040(const int32_t *s0, const int32_t *s1, int16_t *out, unsigned len, unsigned nch);
        .globl  mp368k_pcm_scale16_040
mp368k_pcm_scale16_040:
        movem.l %d2-%d6/%a2,-(%sp)          | 24 bytes: argumentos desde 28(%sp)
        move.l  28(%sp),%a0                 | s0
        move.l  32(%sp),%a1                 | s1
        move.l  36(%sp),%a2                 | out
        move.l  40(%sp),%d2                 | len
        beq.s   9f
        move.l  #4096,%d4
        moveq   #13,%d3
        move.l  #32767,%d5
        move.l  #-32768,%d6
        cmp.l   #1,44(%sp)                  | nch == 1 ?
        beq.s   5f
1:      move.l  (%a0)+,%d0                  | estéreo intercalado
        SC
        move.w  %d0,(%a2)+
        move.l  (%a1)+,%d0
        SC
        move.w  %d0,(%a2)+
        subq.l  #1,%d2
        bne.s   1b
        bra.s   9f
5:      move.l  (%a0)+,%d0                  | mono
        SC
        move.w  %d0,(%a2)+
        subq.l  #1,%d2
        bne.s   5b
9:      movem.l (%sp)+,%d2-%d6/%a2
        rts

| void mp368k_pcm_u8_040(const int16_t *src, uint8_t *dst, unsigned long n);
        .globl  mp368k_pcm_u8_040
mp368k_pcm_u8_040:
        move.l  %d2,-(%sp)                  | argumentos desde 8(%sp)
        move.l  8(%sp),%a0
        move.l  12(%sp),%a1
        move.l  16(%sp),%d2
        beq.s   9f
1:      move.w  (%a0)+,%d0
        ext.l   %d0
        add.l   #128,%d0
        asr.l   #8,%d0                      | -128 .. 128
        cmp.l   #127,%d0
        ble.s   2f
        moveq   #127,%d0
2:      eori.b  #128,%d0                    | v + 128 módulo 256
        move.b  %d0,(%a1)+
        subq.l  #1,%d2
        bne.s   1b
9:      move.l  (%sp)+,%d2
        rts

| void mp368k_pcm_gain16_040(const int16_t *src, int16_t *dst, unsigned long n, unsigned channels,
|                            unsigned gl, unsigned gr);
        .globl  mp368k_pcm_gain16_040
mp368k_pcm_gain16_040:
        movem.l %d2-%d5,-(%sp)              | 16 bytes: argumentos desde 20(%sp)
        move.l  20(%sp),%a0                 | src
        move.l  24(%sp),%a1                 | dst
        move.l  28(%sp),%d2                 | n
        move.l  36(%sp),%d4                 | gl
        move.l  40(%sp),%d5                 | gr
        cmp.l   #256,%d4
        bls.s   1f
        move.l  #256,%d4
1:      cmp.l   #256,%d5
        bls.s   2f
        move.l  #256,%d5
2:      cmp.l   #1,32(%sp)                  | channels == 1 ?
        bne.s   6f
        tst.l   %d2
        beq.s   9f
3:      move.w  (%a0)+,%d0                  | mono: solo gl
        muls.w  %d4,%d0
        asr.l   #8,%d0
        move.w  %d0,(%a1)+
        subq.l  #1,%d2
        bne.s   3b
        bra.s   9f
6:      lsr.l   #1,%d2                      | estéreo: n/2 pares; un elemento suelto al final no se toca
        beq.s   9f
7:      move.w  (%a0)+,%d0
        muls.w  %d4,%d0
        asr.l   #8,%d0
        move.w  %d0,(%a1)+
        move.w  (%a0)+,%d0
        muls.w  %d5,%d0
        asr.l   #8,%d0
        move.w  %d0,(%a1)+
        subq.l  #1,%d2
        bne.s   7b
9:      movem.l (%sp)+,%d2-%d5
        rts
