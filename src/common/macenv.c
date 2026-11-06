/*
 * macenv — ver macenv.h.
 */
#include "macenv.h"

#include <Memory.h>

/*
 * GetApplLimit() está declarada en las Multiversal Interfaces pero sin trap ni
 * glue (no enlaza), así que se lee la variable de bajo nivel ApplLimit (0x130).
 */
#define kApplLimitAddr 0x130

long macenv_grow_stack(long extra)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"     /* dirección constante baja: intencional */
    Ptr limit = *(Ptr volatile *)kApplLimitAddr;
#pragma GCC diagnostic pop

    SetApplLimit(limit - extra);
    MaxApplZone();
    return StackSpace();
}
