#ifndef BATTLE_ACTION_FILE_H
#define BATTLE_ACTION_FILE_H

/* A single action's command file and its intro swirl (800B7870-800B8068):
 * the file (0x22 + 2 * index) loads into a heap block, its images upload
 * and its sound bank starts; its stream (0x23 + 2 * index) plays. */

#include "common.h"
#include "frame.h"

extern s32 *D_800594F0;  /* resident: the loaded command file */
extern void *D_800594BC; /* resident: its stream ring */
extern u8 D_800C3CEC;    /* a command file is loaded */
extern u8 D_800C3624;
extern u8 D_800C35D4;    /* the command file's sound bank is started */
extern s16 D_800C3DF0;   /* the acting sprite's command motion */

void func_800BB080(s32 keep);
s32 func_8001EE68(u8 *frame); /* the frame takes its image from the sequencer (a byte) */
void func_80021BF0(BattleSprite *sprite, void *resource);
void *func_8002A260(s32 blocks, s32 mode);        /* allocate a stream ring */
void func_80029EB0(s32 file, void *ring, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8, s32 a9);

#endif
