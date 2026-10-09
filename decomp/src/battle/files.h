#ifndef BATTLE_FILES_H
#define BATTLE_FILES_H

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* The gears' files in directory 0x28, two bytes per gear id: the base file
 * and the variant count. A gear loads files base + 1 (images), base + 2 (its
 * model) and, with a variant (1..count), base + 2 + variant. */
extern u8 D_800C3508[];

/* A gear's variant file (relocated by 8003342C): a table of extra parts
 * (count, then per part its parent part and offset) and a model block. */
typedef struct {
    u8 pad0[4];
    s16 *table; /* 0x04 */
    u8 *model;  /* 0x08 */
    u8 *end;    /* 0x0C: end of the model block, its images */
} GearPartFile;

/* Resident services. */
void func_8002DDE4(void *images, s16 on, s32 a, s32 b, s16 c, s32 d, s32 e); /* upload images */


#endif
