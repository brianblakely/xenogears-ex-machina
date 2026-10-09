#ifndef BATTLE_SPRITE_SCRIPT_H
#define BATTLE_SPRITE_SCRIPT_H

#include "common.h"
#include "resident/sprite.h"

/* The battle's sprite script command handler (800B3F04's unit, 800B3F04-
 * 800B4EDC): the resident sprite runner (and the battle's copy of it,
 * 800C11CC) calls it with a sprite, a command number (1-107) and the
 * command's argument bytes. The debug overlay follows a sprite it marks. */


extern u8 D_800C3564;    /* 1, or a slot + 2 */
extern Sprite *D_800C3568; /* the sprite the debugger follows */

void func_800B3F04(Sprite *sprite, s32 command, u8 *args); /* run sprite script command `command` */

#endif
