| Punto de entrada del banco bajo qemu-m68k (Linux, sin libc). Llama a main(argc, argv) y sale con su valor.
| Al entrar, (%sp) = argc y 4(%sp) = argv[0].
        .text
        .globl  _start
_start:
        move.l  (%sp),%d0
        lea     4(%sp),%a0
        move.l  %a0,-(%sp)                  | argv
        move.l  %d0,-(%sp)                  | argc
        jsr     main
        move.l  %d0,%d1
        moveq   #1,%d0                      | exit
        trap    #0
