#ifndef BATTLE_OWN_DECLARATIONS_H
#define BATTLE_OWN_DECLARATIONS_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/model.h"
#include "battle/objects.h"
#include "battle/scene.h"

/* The battle's declarations of its functions whose callers in other targets
 * (the event script overlay ovl3087, the result screens ovl2596, the battle
 * loader ovl2615, the scene select ovl2606) were built against different
 * ones: narrow parameters where the battle has words (or the reverse), a
 * narrow or no result, a pointer result where the battle has a word. The
 * shared headers in decomp/include/battle leave these out, and each target
 * declares them for its own calls. */

/* battle/turn.h's subsystem. */
s32 battle_wait_frame(void);      /* one battle frame: the debugger's hook, the task runner */
u16 battle_get_slot_bit(u8 slot); /* the mask bit of a slot */

/* battle/ui.h's. */
void battle_init_text_quad_pair(POLY_FT4 *prims, u8 alternate, u8 page); /* set up a quad pair */
s32 battle_heap_alloc_text_image(s32 count); /* allocate a text image block */

/* battle/command.h's. */
void battle_leave_member_menu(u8 member); /* leave a member's menu */

/* battle/formation.h's. */
s32 battle_is_target_at_lower_x(u8 slot, u8 target); /* whether target's slot-info +0xa is below slot's */
void battle_leave_formation_group(u8 slot);          /* drop a slot from its group */

/* battle/setup.h's. */
s32 battle_heap_alloc(s32 size, s32 mode); /* allocate a battle heap block */

/* battle/windows.h's. */
void battle_window_open(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait); /* open a window */
void battle_window_close(u8 window); /* close a window */

/* battle/model.h's. */
u16 battle_pose_model_hierarchy(ModelPart *part, s32 scale); /* pose a model hierarchy */

/* battle/objects.h's: create a stage object from its script and model
 * files; reset an object. */
void battle_create_object(s32 index, u16 flags, ObjectScriptFile *script_file, ObjectModelFile *model_file, s16 x, s16 y,
                   s16 z, s16 w, SVECTOR *position);
void battle_reset_object(BattleObject *object, EffectPool *pool, u8 **scripts, u8 **animations);

/* battle/screen.h's. */
void battle_quake_start(SVECTOR *amplitude, s32 frames);          /* quake the view towards amplitude */
void battle_screen_fade_start(s32 a, s32 b, s32 c, s32 d, s32 e); /* fade the screen to a colour */

/* battle/action_file.h's. */
u8 battle_single_action_start(void); /* start the loaded single action file; 1 when the acting sprite runs it itself */

/* battle/actor.h's. */
void battle_object_follower_create(u32 slot); /* create a slot's sprite following its object */

/* battle/highlight.h's. */
void battle_highlight_slots(u16 mask); /* highlight the slots of mask */

#endif
