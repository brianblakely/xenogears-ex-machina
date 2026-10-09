#ifndef BATTLE_ACTION_FILE_H
#define BATTLE_ACTION_FILE_H

/* A single action's command file and its intro swirl (800B7870-800B8068):
 * the file (0x22 + 2 * index) loads into a heap block, its images upload
 * and its sound bank starts; its stream (0x23 + 2 * index) plays. */

#include "common.h"
#include "resident/sprite.h"

extern u8 D_800C3CEC;    /* a command file is loaded */
extern u8 D_800C35D4;    /* the command file's sound bank is started */
extern s16 D_800C3DF0;   /* the acting sprite's command motion */

void func_800B7870(void);
void func_800B7C28(void);
void func_800B7C34(s32 command);
u8 func_800B7E94(void); /* start the loaded single action file; 1 when the acting sprite runs it itself */
void func_800B8048(Sprite *sprite);
void func_800B8054(s32 action); /* request single action `action` (800b8068 runs it) */
void func_800B8068(s32 action);
void func_800BB080(s32 keep);

#endif
