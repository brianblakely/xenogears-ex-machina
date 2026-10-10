#ifndef RESIDENT_BATTLE_OVERLAY_H
#define RESIDENT_BATTLE_OVERLAY_H

#include "common.h"
#include "resident/sprite.h"

/* The battle overlay's objects and calls the resident's sprite code uses, by
 * its own views (the battle overlay declares them with its types): the
 * acting sprite and a group's sprites. 800b2aec has a K&R definition there. */
extern Sprite *battle_acting_sprite;                                                                            /* the acting sprite */
extern Sprite *battle_area_event_target_sprites[];                                                              /* the sprites of a group, NULL-terminated */
extern u8 battle_sprites_paused;
struct ScriptEntry;
void battle_tmd_draw_object(struct ScriptEntry *entry, u8 *packets, u32 *ot, s32 unused, s32 bias, s32 blend); /* draw an effect-script entry with its packet buffer */
void battle_tmd_tint_packets(void *entry, u8 *packets0, u8 *packets1, s16 red, s16 green, s16 blue);            /* tint a model */
void battle_sprite_update_ground(Sprite *sprite);                                                               /* rest a sprite on the stage floor */
void battle_camera_register_sprite(SpriteTask *task);                                                           /* register a camera marker */
void battle_sprite_vm_run(Sprite *sprite);                                                                      /* run a sprite's script */

#endif
