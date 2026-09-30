#ifndef RESIDENT_MENU_H
#define RESIDENT_MENU_H

#include "gpu.h"

/* Resident support for the menu mode (mode 5): its work block is reached
 * through *800625a0. Only the fields the resident uses are named. */

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

/* One display buffer: environments and a 16-entry reverse ordering table. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u32 ot[16];
    u32 unknown;
} MenuBuffer;

typedef struct {
    u8 unknown0[0x6C];
    MenuBuffer buffers[2];      /* +0x6c */
    MenuBuffer *current;        /* +0x1d4 */
    SVECTOR angles;             /* +0x1d8 */
    VECTOR offset;              /* +0x1e0 */
    u8 unknown1f0[0x28];
    SVECTOR angles2;            /* +0x218 */
    VECTOR offset2;             /* +0x220 */
    u8 unknown230[0xB8];
    s32 word2e8;                /* +0x2e8 */
    u8 unknown2ec[0x1C];
    s32 buffer_index;           /* +0x308 */
    u8 unknown30c[0x19];
    u8 input;                   /* +0x325 */
    u8 unknown326[3];
    u8 view_motion;             /* +0x329 */
    u8 unknown32a[0x1B6A];
    u8 debug_show;              /* +0x1e94 */
    u8 debug_value;             /* +0x1e95 */
} MenuWork;

extern MenuWork *D_800625A0;

void func_8001BDDC(MenuBuffer *buffer);
void func_8001BF38(void);
void func_8001C074(void);

void func_80044AD8(u32 *ot, s32 count); /* ClearOTagR */
void func_8004A12C(s32 x, s32 y);      /* SetGeomOffset */
void func_8004A14C(s32 h);             /* SetGeomScreen */

#endif
