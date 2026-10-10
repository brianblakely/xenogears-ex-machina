/* Initialized small globals between the first sprite unit's .sdata and the
 * second's: the sprite engine's settings and the battle overlay's module and
 * single-action requests. Every user loads and stores them absolutely: the
 * first three sprite units, which reach small data of their own through $gp,
 * and the overlays (tools/data_users.py --range 80059198:800591b8). GCC
 * writes a -G8 unit's data, commons and .externs ahead of its code, so a
 * one-pass ASPSX would have seen a user's own definition first: they are no
 * user's own. The owner is inferred from the link position alone, an object
 * linked between sprite.o and sprite_construction.o that never uses them. A
 * data-only unit (GP 8 in the target fragment) is the simplest such owner; a
 * text seam at 80021EBC instead of 80022090 (the functions from there touch
 * no small data) would leave a code unit that fits as well. Whether
 * sprite_single_action_request ends it or opens sprite_construction.o's .sdata is undetermined. */
#include "common.h"
#include "resident/sprite.h"
#include "resident/task.h"

s32 sprite_frame_skip = 0; /* 80059198: extra frames per update */
SpriteVoice *sprite_script_sound_bank = NULL; /* 8005919C */
s32 sprite_unused_settings_word1 = 0; /* 800591A0: unreferenced; size from value and alignment */
s32 sprite_unused_settings_word2 = 0; /* 800591A4: unreferenced; size from value and alignment */
s32 sprite_default_scale = 0x2000; /* 800591A8 */
u8 task_new_tasks_active = 0; /* 800591AC: new main-list tasks count as active */
u8 sprite_in_battle = 0; /* 800591AD */
u8 sprite_in_worldmap = 0; /* 800591AE */
u8 task_alloc_mode = 0; /* 800591AF: allocation mode for sprite tasks */
u8 sprite_battle_module_loaded = 1; /* 800591B0: the battle module is loaded */
u8 sprite_single_action_done = 1; /* 800591B1: the battle's requested single action is done (800b8068) */
u8 sprite_loaded_battle_module = 0; /* 800591B2: the loaded battle module */
u8 sprite_requested_battle_module = 0; /* 800591B3: the requested battle module */
u16 sprite_single_action_request = 0; /* 800591B4: the battle's single-action request (800b8054), run by its frame loop */
