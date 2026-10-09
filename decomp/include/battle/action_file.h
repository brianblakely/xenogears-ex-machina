#ifndef BATTLE_ACTION_FILE_H
#define BATTLE_ACTION_FILE_H

#include "common.h"
#include "resident/sprite.h"

/* Single actions and their command files (800B7134's unit, 800B7870-
 * 800B8068): the battle's intro swirl, a requested single action's command
 * file (0x22 + 2 * index, read into a heap block; its images upload and its
 * sound bank starts) and stream (0x23 + 2 * index); also the party's sprites'
 * end (800BB080). The event script overlay starts command files too. */

extern u8 D_800C3CEC;  /* a command file is loaded */
extern u8 D_800C35D4;  /* the command file's sound bank is started */
extern s16 D_800C3DF0; /* the acting sprite's command motion */

void func_800B7870(void);           /* the battle's intro swirl */
void func_800B7C28(void);
void func_800B7C34(s32 command);    /* load a single action's command file and stream */
void func_800B8048(Sprite *sprite); /* set the acting sprite of a single action */
void func_800B8054(s32 action); /* request single action `action` (800b8068 runs it) */
void func_800B8068(s32 action);     /* run a requested single action */
void func_800BB080(s32 keep);       /* end the party's sprites other than keep's */

#endif
