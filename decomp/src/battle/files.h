#ifndef BATTLE_FILES_H
#define BATTLE_FILES_H

#include "common.h"

/* The battle's gear and sound bank files (8009E53C's unit, 800A9540-
 * 800A979C). */

/* The gears' files in directory 0x28, two bytes per gear id: the base file
 * and the variant count. A gear loads files base + 1 (images), base + 2 (its
 * model) and, with a variant (1..count), base + 2 + variant. */
extern u8 battle_gear_file_table[];

/* A gear's variant file (relocated by 8003342C): a table of extra parts
 * (count, then per part its parent part and offset) and a model block. */
typedef struct {
    u8 pad0[4];
    s16 *table; /* 0x04 */
    u8 *model;  /* 0x08 */
    u8 *end;    /* 0x0C: end of the model block, its images */
} GearPartFile;

void battle_read_gear_files(s32 slot); /* read a slot's gear files */
void battle_read_object_set_files(s32 set);  /* load the battle's sound banks for a set */

#endif
