#ifndef BATTLE_INPUT_H
#define BATTLE_INPUT_H

#include "common.h"

/* The battle's controller input: the key a menu reads (battle.c 80089ccc,
 * also the event script overlay's), the direction input, and the pause the
 * input wait applies (8008a3ec). */

/* The pressed key, dequeued each frame: 0-3 right/down/left/up, 4 Circle,
 * 5 Cross, 7 Triangle, 13, 14 Start. */
extern u8 battle_pressed_key;
extern u8 battle_direction_input[2];  /* direction input: [0] the previous, [1] the current */
extern u8 battle_paused;     /* the battle is paused */

void battle_read_input(s32 mode); /* read the battle input into battle_pressed_key */

#endif
