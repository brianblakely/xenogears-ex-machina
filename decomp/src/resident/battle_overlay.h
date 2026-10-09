#ifndef RESIDENT_BATTLE_OVERLAY_H
#define RESIDENT_BATTLE_OVERLAY_H

#include "common.h"
#include "resident/sprite.h"

/* The battle overlay's objects and calls the resident's sprite code uses, by
 * its own views (the battle overlay declares them with its types): the
 * acting sprite and a group's sprites. 800b2aec has a K&R definition there. */
extern Sprite *D_800C3E1C;        /* the acting sprite */
extern Sprite *D_800D363C[];      /* the sprites of a group, NULL-terminated */
extern u8 D_800C3664;
struct ScriptEntry;
void func_800B1F6C(struct ScriptEntry *entry, u8 *packets, u32 *ot, s32 unused, s32 depth, s32 blend); /* draw an effect-script entry with its packet buffer */
void func_800B2AEC(void *model, u8 *packets0, u8 *packets1, s16 red, s16 green, s16 blue); /* tint a model */
void func_800BA8F4(Sprite *sprite);   /* rest a sprite on the stage floor */
void func_800BC158(SpriteTask *task); /* register a camera marker */
void func_800C11CC(Sprite *sprite);   /* run a sprite's script */

#endif
