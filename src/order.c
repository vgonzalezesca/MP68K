/*
 * order — ver order.h.
 */
#include "order.h"

#include <string.h>

static unsigned rnd(Order *o, unsigned n)
{
    o->seed = o->seed * 1664525UL + 1013904223UL;
    return (unsigned)((o->seed >> 8) % n);
}

/* Fisher-Yates sobre perm[from..count-1]. */
static void shuffle_from(Order *o, unsigned from)
{
    unsigned i;

    for (i = o->count; i > from + 1; i--) {
        unsigned j = from + rnd(o, i - from);
        unsigned short t = o->perm[i - 1];

        o->perm[i - 1] = o->perm[j];
        o->perm[j] = t;
    }
}

void order_init(Order *o, unsigned long seed)
{
    memset(o, 0, sizeof *o);
    o->pos = -1;
    o->seed = seed ? seed : 1;
}

void order_clear(Order *o)
{
    o->count = 0;
    o->pos = -1;
    o->detached = 0;
}

int order_add(Order *o)
{
    unsigned idx = o->count;

    if (idx >= ORDER_MAX)
        return -1;
    o->perm[idx] = (unsigned short)idx;
    o->count++;
    if (o->shuffle) {
        /* Va a una posición al azar entre las que aún no se tocaron. */
        unsigned from = o->pos >= 0 ? (unsigned)o->pos + 1 : 0;

        if (from < o->count - 1) {
            unsigned j = from + rnd(o, o->count - from);
            unsigned short t = o->perm[o->count - 1];

            o->perm[o->count - 1] = o->perm[j];
            o->perm[j] = t;
        }
    }
    return (int)idx;
}

void order_remove(Order *o, unsigned index)
{
    unsigned p, i;

    if (index >= o->count)
        return;
    for (p = 0; p < o->count && o->perm[p] != index; p++)
        ;
    memmove(&o->perm[p], &o->perm[p + 1], (o->count - p - 1) * sizeof o->perm[0]);
    o->count--;
    for (i = 0; i < o->count; i++)
        if (o->perm[i] > index)
            o->perm[i]--;
    if (o->pos >= 0) {
        if ((int)p < o->pos) {
            o->pos--;
        } else if ((int)p == o->pos) {
            o->pos--;               /* la siguiente pasa a ocupar la posición siguiente a pos */
            o->detached = 1;
        }
    }
    if (o->count == 0) {
        o->pos = -1;
        o->detached = 0;
    }
}

void order_set_shuffle(Order *o, int on)
{
    int cur = order_current(o);
    unsigned i;

    on = on != 0;
    if (on == o->shuffle)
        return;
    o->shuffle = on;
    if (on) {
        for (i = 0; i < o->count; i++)
            o->perm[i] = (unsigned short)i;
        if (cur >= 0) {
            o->perm[0] = (unsigned short)cur;               /* la actual va primero */
            o->perm[cur] = 0;
            o->pos = 0;
            shuffle_from(o, 1);
        } else {
            shuffle_from(o, 0);
        }
    } else {
        for (i = 0; i < o->count; i++)
            o->perm[i] = (unsigned short)i;
        o->pos = cur >= 0 ? cur : (o->detached ? o->pos : -1);
    }
}

void order_set_repeat(Order *o, RepeatMode r)
{
    o->repeat = r;
}

int order_current(const Order *o)
{
    if (o->pos < 0 || o->detached || (unsigned)o->pos >= o->count)
        return -1;
    return o->perm[o->pos];
}

void order_set_current(Order *o, unsigned index)
{
    unsigned p;

    for (p = 0; p < o->count; p++)
        if (o->perm[p] == index) {
            o->pos = (int)p;
            o->detached = 0;
            return;
        }
}

int order_next(Order *o, int automatic)
{
    if (o->count == 0)
        return -1;
    if (automatic && o->repeat == REPEAT_ONE && order_current(o) >= 0)
        return order_current(o);
    if (o->pos + 1 < (int)o->count) {
        o->pos++;
        o->detached = 0;
        return o->perm[o->pos];
    }
    if (o->repeat == REPEAT_ALL) {
        unsigned short last = o->pos >= 0 ? o->perm[o->pos] : 0;

        if (o->shuffle) {
            shuffle_from(o, 0);
            if (o->count > 1 && o->perm[0] == last) {       /* no repetir la misma al dar la vuelta */
                o->perm[0] = o->perm[1];
                o->perm[1] = last;
            }
        }
        o->pos = 0;
        o->detached = 0;
        return o->perm[0];
    }
    return -1;
}

int order_prev(Order *o)
{
    if (o->count == 0 || o->pos < 0)
        return -1;
    if (o->pos > 0) {
        o->pos--;
        o->detached = 0;
        return o->perm[o->pos];
    }
    if (o->repeat == REPEAT_ALL) {
        o->pos = (int)o->count - 1;
        o->detached = 0;
        return o->perm[o->pos];
    }
    o->detached = 0;
    return o->perm[o->pos];
}
