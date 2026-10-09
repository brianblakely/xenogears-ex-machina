#ifndef BATTLE_FRAME_H
#define BATTLE_FRAME_H

/* The battle's frame loop, controller state and slot sprites (800BE538-
 * 800BF0B4): the late unit addresses the area from D_800C3EB0 as one
 * aggregate, BattleArea (area.h). */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "scene.h"
#include "battle_core.h"
#include "battle/area.h"
#include "files.h"
#include "objects.h"
#include "screen.h"
#include "sprite_effect.h"
#include "psyq/libetc.h"
#include "psyq/libapi.h"

/* The battle's sprite source (resident D_8006BE10). */
#define SPRITE_SOURCE ((SpriteSource *)D_8006BE10)

extern s32 D_800C37D0;    /* frame loop nesting */
extern u8 D_800C3780;     /* a slot's sprite commands run */
extern u8 D_800CCB94[];
extern u16 D_800D30E4;    /* the frame time */

/* The battle menu (D_800C3610, 0x50 bytes). */
typedef struct BattleMenu {
    u8 pad0[4];
    struct Sprite *sprite;              /* 0x04: the acting slot's */
    void (*update)(struct BattleMenu *menu); /* 0x08 */
    u8 padC[0x1C - 0xC];
    s32 state;                              /* 0x1C */
    s32 turnSlot;                           /* 0x20: the slot whose turn it is */
    s32 slot;                               /* 0x24: the acting slot */
    s32 targetSlot;                         /* 0x28 */
    s32 field2C;                            /* 0x2C: the next path point */
    s32 field30;                            /* 0x30 */
    s32 field34;                            /* 0x34 */
    u8 pad38[0x40 - 0x38];
    s32 field40;                            /* 0x40 */
    s32 field44;                            /* 0x44 */
    u8 field48;                             /* 0x48 */
    u8 field49;                             /* 0x49 */
    u8 field4A;                             /* 0x4A */
    u8 pad4B;
    struct Sprite *target;              /* 0x4C */
} BattleMenu;

extern BattleMenu *D_800C3610;
extern s32 D_800C3E20;
extern s16 D_800D2E54;

/* SDK calls of the frame loop. */

/* Resident services. */
void func_80280A9C(void); /* the debugger's frame hook */
void func_800B8354(void);
u8 func_800B7E94(void); /* start the loaded single action file; 1 when the acting sprite runs it itself */
void func_800B89F4(void);
void func_800BED30(void);
void func_800BE108(void);
void func_800BF3A4(void);
void func_800BF9EC(void);
void func_800BB7F8(void);
void func_800BCD8C(void);
void func_800B7C28(void);
extern u8 D_800D2FDC;
extern u8 D_800D36B8;  /* the battle's start mode */
extern u8 D_800C4A39;  /* BATTLE_AREA.buffers[0].drawEnv.r0, which 800B8098 addresses apart from the area */
extern s32 D_800C3D58; /* gear enemies present */
void func_800A8B0C(void);
void func_800B7870(void);
void func_800B8284(void);
void func_800B88C4(void);
void func_800B8840(void);
void func_800A5E9C(u8 *first, u8 *second); /* the two buffers' background colours */
u8 func_801E7210(Formation **formation, s32 a, s32 b, u8 *c, u8 *d, u8 *colour);
void func_801E62E0(s32 arg0);
void func_801E8588(void);
void func_801E893C(void);
void func_801E91E8(void);
void func_801E9594(void);
void func_800A9F94(void);
void func_800A4820(void);
void func_800BADD4(s32 slot);
void func_800B9B54(Sprite *sprite, Sprite *other);
void func_800B9F78(BattleMenu *menu);
void func_800BF0B4(s32 arg0);
void func_800AA320(u16 index, u16 mask, s32 arg2);

void func_80076544(void);
void func_8008A9C0(s32 skipped);
void func_800A9A50(MATRIX *m, s32 arg1, u32 *ot, s32 buffer);
void func_800B8068(s32 action);
void func_800BB9D4(void);
void func_800BBAB8(void);
void func_800BD3AC(Sprite *sprite, s32 command, s32 kind);
void func_800BE0DC(void);
void func_800BEB04(void);
void func_800BEBC4(void);
void func_800BEC18(void);

/* The acting slot's walk and command file (800BEFF4-800BF4F0). */
extern void *D_800C3618;             /* the loaded command file */
extern s32 D_800C361C;               /* its slot */
extern u8 D_800D3350;                /* the command file is started */
extern u16 D_800D3634;               /* the current event's targets */
extern Sprite *D_800D363C[];     /* their sprites, NULL ended */
extern s16 D_800D3678;               /* their count */

void func_800B9C00(); /* unprototyped (sprite, other) */
s32 func_800BEEB4(u32 mask, Sprite **list, Sprite *target);
s16 func_800BEF24(Sprite *from, Sprite *to);
s16 func_800BEF8C(Sprite *sprite);
void func_800BF0C4(Sprite *sprite);
void func_800BF1EC(Sprite *sprite, s32 mode);
void func_800BF4F0(Sprite *sprite, Sprite *target);
s32 func_800C07CC(GroundPoint from, GroundPoint to);
SoundBank *func_800C0FAC(s32 *file);
void func_800C1140(s32 *file);

/* Command motions, value watches and targets (800BF5E8-800BF998). */
typedef struct SlotWatch {
    u8 pad0[0xC];
    void (*destroy)(struct SlotWatch *watch);  /* 0x0C */
    u8 pad10[0x1C - 0x10];
    Sprite *sprite;                        /* 0x1C */
    s32 mode;                                  /* 0x20: the sprite's mode at the start */
    s32 value;                                 /* 0x24: its last value */
    s32 threshold;                             /* 0x28 */
    void (*callback)(Sprite *sprite);      /* 0x2C */
} SlotWatch;

extern s32 D_800C3628;
extern s16 D_800D2D4C;           /* effect hits */

s32 func_800B57E4(Sprite *sprite);
void func_800B7C34(s32 command);
void func_800BD2E4(void);
s32 func_800BF720(void);

/* Requested loads, gear restarts and effect sprites (800BF9EC-800BFDA8). */
extern u8 D_800C3620;            /* the sound bank of file 5 is loaded */
extern u8 D_800C3621;            /* upload the images of file 1 */
extern u8 D_800C3622;            /* wave bank 7 is loaded (a gear frame's turn) */
extern u8 D_800C362C;            /* restart the party's gears (2: all but the acting) */
extern s32 D_800C3A6C;

s32 func_800383EC(u16 id);
s16 func_8003BDFC(s32 wait);             /* sound transfer busy */
void func_800B8D04(void);
Sprite *func_800BFC80(Sprite *sprite, s32 mode, s32 action);

/* Distances, blends and command file parts (800C06E4-800C1140). */
typedef struct {
    s16 x;
    s16 y;
} VramPoint;

extern s32 (*D_800C3A68)[4]; /* four weights per cell, 8 cells a row */

void func_80022224(void *resource, void *image, VramPoint at, VramPoint clut, s32 arg4); /* upload an image */

#endif
