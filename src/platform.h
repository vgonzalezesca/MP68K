/*
 * platform — lo que cada equipo implementa por su cuenta en targets/ (ramas 68k y ppc).
 * El código compartido solo conoce estas firmas.
 */
#ifndef MP368K_PLATFORM_H
#define MP368K_PLATFORM_H

#include <Quickdraw.h>
#include <Windows.h>

/*
 * MenuSelect y DragWindow corren su propio bucle y el nuestro deja de ejecutarse, así que el
 * audio dejaría de alimentarse. Estas versiones instalan MenuHook/DragHook mientras duran
 * (targets/hooks.c). Sin verificar en Mac OS 8.1 con el menú Platinum: si el sistema no llama a
 * los ganchos, el audio dura lo que dure el anillo. Ver "gap" en la línea técnica.
 */
long platform_menu_select(Point where);
void platform_drag_window(WindowPtr win, Point where);

/* Lo llama el gancho en cada vuelta del bucle del sistema; lo define app.c. */
void app_hook_idle(void);

#endif
