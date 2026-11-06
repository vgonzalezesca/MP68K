/*
 * ring — ver ring.h. Sin malloc, sin llamadas al sistema: usable en interrupción.
 */
#include "ring.h"

#include <string.h>

void ring_init(Ring *r, uint8_t *buf, unsigned long size)
{
    r->buf = buf;
    r->size = size;
    r->wpos = 0;
    r->rpos = 0;
}

void ring_reset(Ring *r)
{
    r->wpos = 0;
    r->rpos = 0;
}

unsigned long ring_used(const Ring *r)
{
    return r->wpos - r->rpos;
}

unsigned long ring_space(const Ring *r)
{
    return r->size - (r->wpos - r->rpos);
}

unsigned long ring_write(Ring *r, const void *src, unsigned long n)
{
    unsigned long space = ring_space(r), idx, first;

    if (n > space)
        n = space;
    if (n == 0)
        return 0;
    idx = r->wpos % r->size;
    first = r->size - idx;
    if (first > n)
        first = n;
    memcpy(r->buf + idx, src, first);
    if (n > first)
        memcpy(r->buf, (const uint8_t *)src + first, n - first);
    r->wpos += n;               /* publica los datos después de copiarlos */
    return n;
}

unsigned long ring_read(Ring *r, void *dst, unsigned long n)
{
    unsigned long used = ring_used(r), idx, first;

    if (n > used)
        n = used;
    if (n == 0)
        return 0;
    idx = r->rpos % r->size;
    first = r->size - idx;
    if (first > n)
        first = n;
    memcpy(dst, r->buf + idx, first);
    if (n > first)
        memcpy((uint8_t *)dst + first, r->buf, n - first);
    r->rpos += n;               /* libera el espacio después de copiar */
    return n;
}
