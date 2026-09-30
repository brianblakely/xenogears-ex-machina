#ifndef BATTLE_MODEL_H
#define BATTLE_MODEL_H

#include "common.h"

/* A table of the models of a relocated model group (0x38 bytes each, from
 * group +0x10). */
typedef struct {
    u8 **models;
    u32 count;
} ModelList;

/* Resident services. */
void func_80032498(s32 tag, s32 quiet);       /* select the heap owner tag */
void *func_80031BDC(u32 size, s32 mode);      /* allocate */
u32 func_8002C3E8(u8 *group);                 /* relocate a model group; its count */

#endif
