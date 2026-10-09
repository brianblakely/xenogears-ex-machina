#ifndef RESIDENT_STREAM_H
#define RESIDENT_STREAM_H

#include "common.h"

/* Disc stream ring (EVID: analysis/formats/disc-stream-source.md). The ring
 * header holds the slot count, then eight bytes per slot. */
typedef struct {
    u16 state;
    u16 sequence;
    u16 length; /* run of free slots starting here */
    u16 w6;
} StreamSlot;

typedef struct {
    s32 count;
    StreamSlot slots[1];
} StreamRing;

/* Header of a movie frame's first sector: the frame spans `sectors` ring
 * slots; the headers of all its sectors come first (0x20 bytes each), then
 * their 0x7e0-byte payloads. */
typedef struct {
    u8 unknown0[6];
    u16 sectors; /* +0x6 */
    u16 word8;   /* +0x8 */
} StreamFrame;

extern StreamRing *D_8004FE30; /* the ring */
extern StreamSlot *D_8004FE2C; /* its slots */
extern s32 D_8004FE40;         /* its slot count */
extern u16 D_8004FE24;
extern u16 D_8004FE26;
extern u16 D_8004FE28;
extern u16 D_8005A4B8;

StreamRing *func_80028A94(StreamRing *ring);
s32 func_80028AAC(void);
void func_8002B084(u8 intr, u8 *result);
void func_8002B2F0(u8 intr, u8 *result);
void func_8002BA58(void);
void func_8002B5D0(u8 intr, u8 *result);
void func_8002BB50(void);
/* Defined without parameters; 80029EB0 calls it with the (0, 0) of a CD
 * callback. */
void func_8002B8B0();
/* Defined without parameters; 80029EB0 calls it like a CD callback. */
void func_8002BF38();
void func_8002A68C(u8 intr, u8 *result);
void func_8002AC24(u8 intr, u8 *result);
void func_8002BA40(void);

/* More of the stream services. */
s32 func_8002C3D8(void);
s32 func_80028F30(u8 **data, StreamFrame **frame);
StreamRing *func_8002A260(s32 count, s32 mode);

#endif
