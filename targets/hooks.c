/*
 * Ganchos de menú y arrastre en 68k. MenuHook (0x0A30) y DragHook (0x09F6) son rutinas que el
 * sistema llama una y otra vez durante MenuSelect y DragWindow. Esperan que se conserven TODOS
 * los registros, de ahí el puente en ensamblador. Ver platform.h.
 */
#include <LowMem.h>
#include <Menus.h>

#include "platform.h"

__asm__(
    ".text\n"
    ".even\n"
    ".globl mp_hook_stub\n"
    "mp_hook_stub:\n"
    "    movem.l %d0-%d7/%a0-%a6,-(%sp)\n"
    "    jsr app_hook_idle\n"
    "    movem.l (%sp)+,%d0-%d7/%a0-%a6\n"
    "    rts\n");

extern void mp_hook_stub(void);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"     /* variables globales del sistema en direcciones bajas */

long platform_menu_select(Point where)
{
    ProcPtr saved = LMGetMenuHook();
    long result;

    LMSetMenuHook((ProcPtr)(void *)mp_hook_stub);
    result = MenuSelect(where);
    LMSetMenuHook(saved);
    return result;
}

void platform_drag_window(WindowPtr win, Point where)
{
    ProcPtr saved = LMGetDragHook();

    LMSetDragHook((ProcPtr)(void *)mp_hook_stub);
    DragWindow(win, where, &qd.screenBits.bounds);
    LMSetDragHook(saved);
}

#pragma GCC diagnostic pop
