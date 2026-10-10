#ifndef OVL2606_SCENE_SELECT_H
#define OVL2606_SCENE_SELECT_H

/* The debug battle-scene selector: the battle overlay's frame buffers, pad
 * decoder (battle/input.h), heap mode (battle/setup.h) and allocator it uses,
 * and its own tables. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "battle/area.h"
#include "battle/input.h"
#include "battle/setup.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/formation.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"

extern void *battle_music_file_block;

extern void *battle_heap_alloc(s32 size, s32 mode); /* battle allocation */
extern void battle_grant_debug_items_and_skills(void);

#endif
