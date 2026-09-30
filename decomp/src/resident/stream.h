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

void func_8002A394(s32 mode);
void func_8002A68C(void);
void func_8002AC24(void);
void func_8002BA40(void);

#endif
