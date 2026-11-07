| Punto de entrada del banco bajo qemu-m68k (Linux, sin libc). Llama a main y sale con su valor.
        .text
        .globl  _start
_start:
        jsr     main
        move.l  %d0,%d1
        moveq   #1,%d0              | exit
        trap    #0
