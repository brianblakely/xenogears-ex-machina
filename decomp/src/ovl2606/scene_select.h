#ifndef OVL2606_SCENE_SELECT_H
#define OVL2606_SCENE_SELECT_H

/* The debug battle-scene selector: the battle overlay's frame buffers, pad
 * decoder (battle/input.h) and allocator it uses, and its own tables. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "battle/area.h"
#include "battle/input.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"

extern u8 D_800658DC[]; /* the resident's encounter sets (16 x 0x20), copied from the file */
extern void *D_800D39D8;

extern void func_8008AB70(void);
extern void *func_8008ABB8(s32 size, s32 mode); /* battle allocation */
extern void func_8009B1E4(void);

#endif
