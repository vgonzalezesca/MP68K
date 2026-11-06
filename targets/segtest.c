/*
 * segtest — comprueba que un segmento de código muy mayor que 32 KB corre bien en 68k.
 *
 * La regla 6 de CLAUDE.md (segmentos ≤ 32 KB) nunca se cumplió con este toolchain. Esta app
 * ejecuta cientos de funciones generadas (targets/gen_segtest.py) a través de una tabla y en
 * orden barajado, y compara el resultado con el calculado en el PC. Si coincide, las llamadas
 * dentro de un segmento grande funcionan. No demuestra nada sobre llamadas entre segmentos
 * distintos ni sobre el LC 580: solo sobre el sistema donde se corra.
 */
#include <stdio.h>

#include "build_info.h"
#include "segtest.h"
#include "textlog.h"

int main(void)
{
    unsigned long x = 0x12345678UL;
    unsigned r;
    unsigned i;
    unsigned long first, last, lo, hi;
    OSErr err;

    textlog_init();
    textlog_printf("# MP368K segtest\n");
    textlog_printf("build: %s %s %s %s\n", MP368K_BUILD_ID, __DATE__, __TIME__, MP368K_BUILD_FLAGS);

    for (r = 0; r < seg_rounds; r++)
        for (i = 0; i < SEG_COUNT; i++)
            x = seg_table[seg_order[i]](x);

    first = (unsigned long)seg_table[0];
    last = (unsigned long)seg_table[SEG_COUNT - 1];
    lo = first < last ? first : last;
    hi = first < last ? last : first;
    for (i = 0; i < SEG_COUNT; i++) {
        unsigned long a = (unsigned long)seg_table[i];
        if (a < lo) lo = a;
        if (a > hi) hi = a;
    }

    textlog_printf("funciones: %u, rondas: %u\n", SEG_COUNT, seg_rounds);
    textlog_printf("rango de direcciones de las funciones: 0x%08lX - 0x%08lX (%lu bytes)\n",
                   lo, hi, hi - lo);
    textlog_printf("resultado: 0x%08lX  esperado: 0x%08lX\n", x, seg_expected);
    textlog_printf("RESULTADO: %s\n", x == seg_expected ? "OK" : "FALLA");

    err = textlog_save_next_to_app("segtest.txt");
    textlog_printf("\nsegtest.txt guardado: err=%d\n", err);
    printf("\n(Presione Return para salir)\n");
    getchar();
    return 0;
}
