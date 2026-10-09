#ifndef BATTLE_OWN_DECLARATIONS_H
#define BATTLE_OWN_DECLARATIONS_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
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
s32 func_800716D8(void);    /* one battle frame: the debugger's hook, the task runner */
u16 func_80089C08(u8 slot); /* the mask bit of a slot */

/* battle/ui.h's. */
void func_80076D58(POLY_FT4 *prims, u8 alternate, u8 page); /* set up a quad pair */
s32 func_8008AC00(s32 count); /* allocate a text image block */

/* battle/command.h's. */
void func_800800E8(u8 member); /* leave a member's menu */

/* battle/formation.h's. */
s32 func_80085310(u8 slot, u8 target); /* whether target's slot-info +0xa is below slot's */
void func_800883AC(u8 slot);           /* drop a slot from its group */

/* battle/setup.h's. */
s32 func_8008ABB8(s32 size, s32 mode); /* allocate a battle heap block */

/* battle/windows.h's. */
void func_8008F8F4(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait); /* open a window */
void func_8008FA60(u8 window); /* close a window */

/* battle/model.h's. */
u16 func_8009EF3C(ModelPart *part, s32 scale); /* pose a model hierarchy */

/* battle/objects.h's: create a stage object from its script and model
 * files; reset an object. */
void func_800A8BF0(s32 index, u16 flags, ObjectScriptFile *scriptFile, ObjectModelFile *modelFile, s16 x, s16 y,
                   s16 z, s16 w, SVECTOR *position);
void func_800AA898(BattleObject *object, EffectPool *pool, u8 **scripts, u8 **animations);

/* battle/screen.h's. */
void func_800B3658(SVECTOR *amplitude, s32 frames);    /* quake the view towards amplitude */
void func_800B39C0(s32 a, s32 b, s32 c, s32 d, s32 e); /* fade the screen to a colour */

/* battle/action_file.h's. */
u8 func_800B7E94(void); /* start the loaded single action file; 1 when the acting sprite runs it itself */

/* battle/actor.h's. */
void func_800BB350(u32 slot); /* create a slot's sprite following its object */
void func_800BC404(s32 mask); /* start a camera move */

/* battle/highlight.h's. */
void func_800BCD98(u16 mask); /* highlight the slots of mask */

/* battle/frame.h's. */
s32 func_800BEEB4(u32 mask, Sprite **list, Sprite *target); /* list the sprites of the slots in mask */

#endif
