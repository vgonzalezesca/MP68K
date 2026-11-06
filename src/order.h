/*
 * order — orden de reproducción de una lista: normal o aleatorio, con repetición.
 *
 * Trabaja solo con índices de entradas (0..count-1); la lista de archivos vive aparte. Portable
 * (sin Toolbox) para probarlo en el PC.
 */
#ifndef MP368K_ORDER_H
#define MP368K_ORDER_H

#define ORDER_MAX 512

typedef enum { REPEAT_OFF = 0, REPEAT_ALL, REPEAT_ONE } RepeatMode;

typedef struct {
    unsigned short perm[ORDER_MAX];     /* orden de reproducción: perm[posición] = índice de entrada */
    unsigned count;
    int pos;                            /* posición actual en perm; -1 = ninguna */
    int detached;                       /* 1 si se borró la entrada actual (pos queda como "última") */
    int shuffle;
    RepeatMode repeat;
    unsigned long seed;
} Order;

void order_init(Order *o, unsigned long seed);
void order_clear(Order *o);

/* Agrega una entrada al final; devuelve su índice, o -1 si está llena. */
int order_add(Order *o);
/* Quita la entrada `index`; las de índice mayor bajan una posición. */
void order_remove(Order *o, unsigned index);

void order_set_shuffle(Order *o, int on);
void order_set_repeat(Order *o, RepeatMode r);

/* Índice de la entrada actual, o -1 si no hay. */
int order_current(const Order *o);
void order_set_current(Order *o, unsigned index);

/*
 * Siguiente entrada. `automatic` = 1 cuando la pista terminó sola (REPEAT_ONE la repite); 0 si
 * el usuario pidió "siguiente". Devuelve el índice, o -1 si se acabó la lista (sin repetir).
 */
int order_next(Order *o, int automatic);
/* Entrada anterior (con REPEAT_ALL da la vuelta). Devuelve -1 si no hay ninguna actual. */
int order_prev(Order *o);

#endif
