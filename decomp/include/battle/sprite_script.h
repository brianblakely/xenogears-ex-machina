#ifndef BATTLE_SPRITE_SCRIPT_H
#define BATTLE_SPRITE_SCRIPT_H

/* The battle's sprite script command handler (800B3F04): the resident
 * sprite runner calls it with a sprite, a command number (1-107) and the
 * command's argument bytes. */

#include "common.h"
#include "resident/sprite.h"

extern u8 D_800C3564;    /* 1, or a slot + 2 */
extern Sprite *D_800C3568; /* the sprite the debugger follows */

/* Resident services. */

void func_800B3F04(Sprite *sprite, s32 command, u8 *args); /* run sprite script command `command` */


#endif
