#ifndef RESIDENT_BATTLE_OVERLAY_H
#define RESIDENT_BATTLE_OVERLAY_H

#include "common.h"
#include "resident/sprite.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/frame.h"

/* The battle overlay's objects and calls the resident's sprite code uses. The
 * acting sprite (battle/action_file.h), the event's target sprites
 * (battle/frame.h), the paused flag and the ground step (battle/actor.h) come
 * from the battle's headers. The resident declares the calls below itself:
 * the battle defines the entry draw (800b1f6c) and the tint (800b2aec) K&R,
 * and no battle header declares the tint, the camera marker or the sprite
 * script runner. */
struct ScriptEntry;
void battle_tmd_draw_object(struct ScriptEntry *entry, u8 *packets, u32 *ot, s32 unused, s32 bias, s32 blend);  /* draw an effect-script entry with its packet buffer */
void battle_tmd_tint_packets(void *entry, u8 *packets0, u8 *packets1, s16 red, s16 green, s16 blue);            /* tint a model */
void battle_camera_register_sprite(SpriteTask *task);                                                           /* register a camera marker */
void battle_sprite_vm_run(Sprite *sprite);                                                                      /* run a sprite's script */

#endif
