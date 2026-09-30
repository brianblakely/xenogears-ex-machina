#ifndef RESIDENT_CD_H
#define RESIDENT_CD_H

#include "common.h"

/* Resident disc access: file table, CD command state and the stream ring. */

/* One loaded file: its file number and the heap block holding its data. A
 * zero id terminates a table. */
typedef struct {
    u16 id;
    void *data;
} FileEntry;

/* PsyQ libcd (SDK region, kept by address): CdReadyCallback, CdControl,
 * CdIntToPos. */
extern void *func_80040FB4(void (*callback)());
/* CdControl(command, param[, result]): the game passes two arguments. */
extern s32 func_8004111C();
extern u8 *func_80041430(s32 sector, u8 *location);

extern s32 func_800286CC(void);
extern s32 func_800288EC(s32 file);
extern s16 func_80028928(void);
extern s32 func_800289D0(s32 file);
extern s32 func_8004C338(s32 handle);

extern void func_8002A68C();

extern s32 D_8004FDF8;
extern s32 D_8004FDFC;
extern s32 D_8004FE14;
extern s32 D_8004FE18;
extern s32 D_8004FE1C;
extern s32 D_8004FE34;
extern s32 D_8004FE38;
extern s32 D_8004FE48;
extern s32 D_8004FE4C;
extern u8 D_80059F10[4];
extern u8 D_80059F18[4];

#endif
