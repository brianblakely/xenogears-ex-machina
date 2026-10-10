#ifndef BATTLE_SPRITE_SCRIPT_H
#define BATTLE_SPRITE_SCRIPT_H

#include "common.h"
#include "resident/sprite.h"

/* The battle's sprite script command handler (800B3F04's unit, 800B3F04-
 * 800B4EDC): the resident sprite runner (and the battle's copy of it,
 * 800C11CC) calls it with a sprite, a command number (1-107) and the
 * command's argument bytes. The debug overlay follows a sprite it marks. */

extern u8 battle_unread_sprite_slot_mark;    /* 1, or a slot + 2 */
extern Sprite *battle_sprite_for_debugger; /* the sprite the debugger follows */

void battle_sprite_command_run(Sprite *sprite, s32 command, u8 *args); /* run sprite script command `command` */

#endif
