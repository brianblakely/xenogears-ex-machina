#ifndef BATTLE_ACTION_FILE_H
#define BATTLE_ACTION_FILE_H

/* A single action's command file and its intro swirl (800B7870-800B8068):
 * the file (0x22 + 2 * index) loads into a heap block, its images upload
 * and its sound bank starts; its stream (0x23 + 2 * index) plays. */

#include "common.h"
#include "frame.h"

extern u8 D_800C3CEC;    /* a command file is loaded */
extern u8 D_800C3624;
extern u8 D_800C35D4;    /* the command file's sound bank is started */
extern s16 D_800C3DF0;   /* the acting sprite's command motion */

void func_800BB080(s32 keep);
void func_801E5840(u8 phase);                    /* the battle module's set-up phase */
ScreenShatter *func_800B73EC(void);
void func_800B7330(void *block);
void func_80029EB0(s32 file, void *ring, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8, s32 a9);

#endif
