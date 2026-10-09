#ifndef RESIDENT_MENU_H
#define RESIDENT_MENU_H

#include "gpu.h"

/* Resident support for the menu mode (mode 5): its work block is reached
 * through *800625a0. Only the fields the resident uses are named. */

/* One display buffer: environments and a 16-entry reverse ordering table. */
typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u_long ot[16];
    u32 unknown;
} MenuBuffer;

typedef struct MenuWork {
    u8 unknown0[0x6C];
    MenuBuffer buffers[2];      /* +0x6c */
    MenuBuffer *current;        /* +0x1d4 */
    SVECTOR angles;             /* +0x1d8 */
    VECTOR offset;              /* +0x1e0 */
    u8 unknown1f0[0x28];
    SVECTOR angles2;            /* +0x218 */
    VECTOR offset2;             /* +0x220 */
    u8 unknown230[0xA8];
    s32 frame_counter;          /* +0x2d8 */
    u8 unknown2dc[0xC];
    s32 word2e8;                /* +0x2e8 */
    u8 unknown2ec[0x1C];
    s32 buffer_index;           /* +0x308 */
    u8 unknown30c[0x19];
    u8 input;                   /* +0x325 */
    u8 unknown326;
    u8 drawing;                 /* +0x327 */
    u8 unknown328;
    u8 view_motion;             /* +0x329 */
    u8 unknown32a[0x1B6A];
    u8 debug_show;              /* +0x1e94 */
    u8 debug_value;             /* +0x1e95 */
} MenuWork;

extern MenuWork *D_800625A0;

extern u8 D_80059178;       /* debug start: choose the menu screen */
extern u8 D_80059460;       /* menu screen */
extern u8 D_80059171;       /* menu screen parameter */
extern char *D_8004FA9C[7]; /* menu screen names */
extern void *D_8005945C;
extern void *D_800658CC;
extern void *D_8006BE24;
extern u32 *D_8005A4AC[2]; /* the menu's large ordering tables, one per draw buffer */

/* Menu overlay (801c5000) entries. */
void func_801C62A8(void);
void func_801CB0A8(void);
void func_801CBDBC(void);
void func_801CCD28(void);
void func_801CE024(void);

void func_8001BDDC(MenuBuffer *buffer);
void func_8001BE14(void);
void func_8001BEEC(void);
void func_8001C1A8(void);
void func_8001BF38(void);
void func_8001C074(void);

/* More of the menu screens' resident state. */

#endif
