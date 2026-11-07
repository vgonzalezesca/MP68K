| long call_checked(void (*fn)(), a1, a2, a3, a4, a5, a6, a7)
| Llama a fn con 7 argumentos en la pila (convención de GCC para m68k) después de cargar valores
| centinela en d2-d7 y a2-a6, y devuelve una máscara de bits con los registros que fn NO preservó
| (bit 0 = d2 ... bit 5 = d7, bit 6 = a2 ... bit 10 = a6). 0 = todos preservados.
        .text
        .even
        .globl  call_checked
call_checked:
        movem.l %d2-%d7/%a2-%a6,-(%sp)
        move.l  48(%sp),%a0                 | fn (4 + 44 de lo guardado)
        | copiar a1..a7 (en 52..76(%sp)) a la pila, de derecha a izquierda; cada push desplaza 4
        move.l  76(%sp),-(%sp)
        move.l  76(%sp),-(%sp)
        move.l  76(%sp),-(%sp)
        move.l  76(%sp),-(%sp)
        move.l  76(%sp),-(%sp)
        move.l  76(%sp),-(%sp)
        move.l  76(%sp),-(%sp)
        move.l  #0x11111111,%d2
        move.l  #0x22222222,%d3
        move.l  #0x33333333,%d4
        move.l  #0x44444444,%d5
        move.l  #0x55555555,%d6
        move.l  #0x66666666,%d7
        move.l  #0x77777777,%a2
        move.l  #0x88888888,%a3
        move.l  #0x99999999,%a4
        move.l  #0xAAAAAAAA,%a5
        move.l  #0xBBBBBBBB,%a6
        jsr     (%a0)
        lea     28(%sp),%sp
        moveq   #0,%d0
        cmp.l   #0x11111111,%d2
        beq     1f
        or.l    #0x001,%d0
1:      cmp.l   #0x22222222,%d3
        beq     2f
        or.l    #0x002,%d0
2:      cmp.l   #0x33333333,%d4
        beq     3f
        or.l    #0x004,%d0
3:      cmp.l   #0x44444444,%d5
        beq     4f
        or.l    #0x008,%d0
4:      cmp.l   #0x55555555,%d6
        beq     5f
        or.l    #0x010,%d0
5:      cmp.l   #0x66666666,%d7
        beq     6f
        or.l    #0x020,%d0
6:      cmp.l   #0x77777777,%a2
        beq     7f
        or.l    #0x040,%d0
7:      cmp.l   #0x88888888,%a3
        beq     8f
        or.l    #0x080,%d0
8:      cmp.l   #0x99999999,%a4
        beq     9f
        or.l    #0x100,%d0
9:      cmp.l   #0xAAAAAAAA,%a5
        beq     10f
        or.l    #0x200,%d0
10:     cmp.l   #0xBBBBBBBB,%a6
        beq     11f
        or.l    #0x400,%d0
11:     movem.l (%sp)+,%d2-%d7/%a2-%a6
        rts
