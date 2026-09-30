#ifndef RESIDENT_HEAP_H
#define RESIDENT_HEAP_H

#include "common.h"

/* Resident game heap: allocate `size` bytes with an allocation mode, free a
 * block. */
extern void *func_80031BDC(s32 size, s32 mode);
extern void func_800320E8(void *block);

#endif
