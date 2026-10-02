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
extern s16 D_80059F60;       /* sector of the frame being read from the PC file server */
extern s32 D_80059F04;         /* PC file server handle of the stream */
extern u8 *D_80059F54;         /* sector headers of the frame being read */
extern u8 *D_80059F58;         /* payloads of the frame being read */
extern s16 D_80059F5C;         /* sectors of the frame being read */
extern u16 D_8005A4B8;
extern u8 D_800596F8[8];       /* PC file server sector subheader */
/* Image stream parameters set by 80029eb0: for images of type 0x1200 and
 * 0x1201, a placement mode (1: base + offset, 2: base + origin + offset,
 * otherwise origin + offset) and a base position. */
extern s16 D_80059F24;
extern u16 D_80059F28, D_80059F2C;
extern s16 D_80059F30;
extern u16 D_80059F34, D_80059F38;
extern s32 D_80059F3C;         /* images left in the stream */
extern s16 D_80059F40, D_80059F44, D_80059F48; /* next strip: x, y, width */
extern u16 *D_80059F4C;        /* heights of the remaining strips */
extern s32 D_80059F50;         /* strips left in the current image */

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

#endif
