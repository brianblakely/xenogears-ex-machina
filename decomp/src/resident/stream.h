#ifndef RESIDENT_STREAM_H
#define RESIDENT_STREAM_H

#include "common.h"

/* Disc stream ring (EVID: analysis/formats/disc-stream-source.md). The ring
 * header holds the slot count, then eight bytes per slot. */
typedef struct {
    u16 state;
    u16 sequence;
    s16 length; /* run of free slots starting here */
    u16 w6;
} StreamSlot;

typedef struct {
    s32 count;
    StreamSlot slots[1];
} StreamRing;

extern StreamRing *D_8004FE30; /* the ring */
extern StreamSlot *D_8004FE2C; /* its slots */
extern s32 D_8004FE40;         /* its slot count */
extern u16 D_8004FE24;
extern u16 D_8004FE26;
extern u16 D_8004FE28;
extern u16 D_80059F60;
extern u8 D_80059F18[4];       /* CD mode parameter */
extern s32 D_80059F04;         /* PC file server handle of the stream */
/* Stream parameters set by 80029eb0. */
extern u16 D_80059F24, D_80059F28, D_80059F2C, D_80059F30, D_80059F34, D_80059F38;
extern s32 D_80059F3C;
extern u16 D_80059F40, D_80059F44, D_80059F48;
extern s32 D_80059F4C, D_80059F50;

StreamRing *func_80028A94(StreamRing *ring);
s32 func_80028AAC(void);
void func_8002A394(s32 mode);
void func_8002B084(void);
void func_8002B2F0(void);
void func_8002BA58(void);
void func_8002B5D0(void);
void func_8002BB50(void);
void func_8002B8B0(s32 a0, s32 a1);
void func_8002BF38(s32 a0, s32 a1);
void func_8002A68C(void);
void func_8002AC24(void);
void func_8002BA40(void);

#endif
