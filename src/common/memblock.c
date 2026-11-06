/*
 * memblock — ver memblock.h.
 */
#include "memblock.h"

#include <Memory.h>

#include <string.h>

OSErr memblock_alloc(MemBlock *b, long size)
{
    long want = (size > 0 ? size : 1) + 16;     /* margen para alinear a 16 */

    memset(b, 0, sizeof *b);
    b->ptr = NewPtr(want);
    if (b->ptr != NULL) {
        b->data = b->ptr;
    } else {
        OSErr err;

        b->handle = TempNewHandle(want, &err);
        if (b->handle == NULL)
            return err != noErr ? err : memFullErr;
        HLock(b->handle);
        b->data = *b->handle;
    }
    b->data = (void *)(((unsigned long)b->data + 15UL) & ~15UL);
    b->size = size;
    return noErr;
}

void memblock_free(MemBlock *b)
{
    if (b->ptr != NULL)
        DisposePtr(b->ptr);
    if (b->handle != NULL)
        DisposeHandle(b->handle);
    memset(b, 0, sizeof *b);
}
