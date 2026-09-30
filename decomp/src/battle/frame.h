#ifndef BATTLE_FRAME_H
#define BATTLE_FRAME_H

/* The battle's frame loop, controller state and slot sprites (800BE538-
 * 800BF0B4): the late unit addresses the area from D_800C3EB0 as one
 * aggregate, BattleFrame. */

#include "common.h"
#include "psyq.h"
#include "scene.h"
#include "battle_core.h"
#include "psyq/libetc.h"
#include "psyq/libapi.h"

/* One of the two display buffers (0x4070 bytes). */
typedef struct {
    u8 drawEnv[0x5C];  /* DRAWENV */
    u8 dispEnv[0x14];  /* 0x5C: DISPENV */
    u32 ot[0x1000];    /* 0x70: cleared in reverse */
} FrameBuffer;

/* Controller state history entry (8 bytes). */
typedef struct {
    u16 held;
    u16 pressed;
    u16 released;
    u16 time;
} PadRecord;

/* A slot's sprite (a resident sprite; fields as far as used). */
typedef struct SlotSprite {
    s32 x, y, z;                /* 16.16 */
    u8 padC[0x48 - 0xC];
    s32 field48;                /* 0x48 */
    u8 pad4C[0x74 - 0x4C];
    struct SlotSprite *target;  /* 0x74 */
    u8 pad78[0x7C - 0x78];
    s32 *resource;              /* 0x7C: its file first */
    u8 pad80[0xA0 - 0x80];
    s16 targetX;                /* 0xA0 */
    s16 targetY;                /* 0xA2 */
    s16 targetZ;                /* 0xA4 */
    u8 padA6[0xA8 - 0xA6];
    u32 frameBits;              /* 0xA8: bits 30-31 the slot's low bits */
    union {
        u32 word;               /* bits 0-1 the slot's high bits */
        struct {
            u8 pad[3];
            s8 mode;            /* 10 while running commands, 0x15 ... */
        } b;
    } motion;                   /* 0xAC */
    s8 idleMode;                /* 0xB0 */
} SlotSprite;

/* The battle slot of a slot's sprite. */
#define SPRITE_SLOT(sprite) (((sprite)->motion.word & 3) << 2 | (sprite)->frameBits >> 30)

/* A point of the battle menu's walk (6 bytes); x and z 0xFFFF end it. */
typedef struct {
    u16 x;
    u16 z;
    u8 run; /* 0x04 */
    u8 pad5;
} PathPoint;

/* A point on the ground passed by value. */
typedef struct {
    s16 x;
    s16 z;
} GroundPoint;

/* Switch the stack to top for the calls up to STACK_LEAVE. */
#define STACK_ENTER(top)                                                                           \
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"            \
                     :                                                                             \
                     : "r"(top)                                                                    \
                     : "$8", "memory")
#define STACK_LEAVE() __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory")

/* The battle's state from D_800C3EB0 (as the late unit sees it). */
typedef struct {
    Formation *formation;       /* 0x0000 */
    BattleSlot slots[11];       /* 0x0004 */
    BattleEvent events[32];     /* 0x0138 */
    u8 padA38[4];
    PathPoint path[51];         /* 0x0A3C */
    u8 padB6E[2];
    FrameBuffer buffers[2];     /* 0x0B70 */
    FrameBuffer *current;       /* 0x8C50 */
    u32 *ot;                    /* 0x8C54 */
    u16 held;                   /* 0x8C58 */
    u16 held2;                  /* 0x8C5A: the second controller */
    u16 pressed;                /* 0x8C5C */
    u16 pressed2;               /* 0x8C5E */
    u16 released;               /* 0x8C60 */
    u16 heldOnly;               /* 0x8C62: held on the first, not the second */
    PadRecord history[4];       /* 0x8C64: the last changes, newest first */
    s32 buffer;                 /* 0x8C84: the buffer being drawn */
    u8 pad8C88[4];
    SlotSprite *slotSprites[11]; /* 0x8C8C */
    u8 pad8CB8[0x8DAC - 0x8CB8];
    s32 frameTicks;             /* 0x8DAC: vertical blanks of the last frame */
} BattleFrame;

#define BATTLE_FRAME (*(BattleFrame *)&D_800C3EB0)

/* Layout checks. */
typedef char BattleFrameCheck[(sizeof(BattleSlot) == 0x1C && sizeof(BattleEvent) == 0x48 && sizeof(PathPoint) == 6) ? 1 : -1];

/* Frame timing and the view (D_800D309C). */
typedef struct {
    u8 pad0[0x20];
    Matrix view;     /* 0x20 */
    s32 drawn;       /* 0x40: vertical blank after drawing */
    s32 synced;      /* 0x44: after the GPU finished */
    s32 start;       /* 0x48: at the frame's start */
} FrameClock;

extern FrameClock D_800D309C;
extern s32 D_800C37D0;    /* frame loop nesting */
extern s16 D_80059494;    /* extra vertical blanks of the last frame (0-4) */
extern s32 D_80059198;    /* frame skip */
extern u16 D_800591B4;    /* a sound request */
extern u8 D_800C37CC;     /* free the objects' extra files once idle */
extern u8 D_800C3780;     /* a slot's sprite commands run */
extern s32 D_80010000;    /* the debugger's word, -1 none */
extern u32 *D_8005956C;
extern u8 D_800CCB94[];
extern u16 D_800D30E4;    /* the frame time */

/* The battle menu (D_800C3610, 0x50 bytes). */
typedef struct BattleMenu {
    u8 pad0[4];
    struct SlotSprite *sprite;              /* 0x04: the acting slot's */
    void (*update)(struct BattleMenu *menu); /* 0x08 */
    u8 padC[0x1C - 0xC];
    s32 state;                              /* 0x1C */
    u8 pad20[0x24 - 0x20];
    s32 slot;                               /* 0x24: the acting slot */
    s32 targetSlot;                         /* 0x28 */
    s32 field2C;                            /* 0x2C: the next path point */
    s32 field30;                            /* 0x30 */
    s32 field34;                            /* 0x34 */
    u8 pad38[0x44 - 0x38];
    s32 field44;                            /* 0x44 */
    u8 field48;                             /* 0x48 */
    u8 field49;                             /* 0x49 */
    u8 field4A;                             /* 0x4A */
    u8 pad4B;
    struct SlotSprite *target;              /* 0x4C */
} BattleMenu;

extern BattleMenu *D_800C3610;
extern s32 D_800C360C;
extern s32 D_800C3E20;
extern s16 D_800D2E54;
extern u8 D_800591B0;    /* the battle module is loaded */
extern u8 D_800591B2;    /* the loaded battle module */
extern u8 D_800591B3;    /* the requested battle module */

/* SDK calls of the frame loop. */
void ClearOTagR(u32 *ot, s32 n);
void DrawOTag(u32 *ot);
void PutDrawEnv(void *env);
void PutDispEnv(void *env);

/* Resident services. */
void func_80019CA0(void);
void func_8001C964(void);
void func_8001C9F8(void);
void func_8001D468(void);
void func_80024FE4(u32 *ot);
void func_80024FF4(Matrix *view);
void func_80025044(void);
void func_800250E0(s32 buffer);
s32 func_800286CC(void); /* the disc is busy */
void func_80037324(u32 *ot);
void func_80280A9C(void); /* the debugger's frame hook */
void func_800245D8(SlotSprite *sprite, s32 mode);
s32 func_8003569C(s32 pad);
s16 func_80023124(GroundPoint to, GroundPoint from); /* the direction between points */
void *func_80031BDC(u32 size, s32 mode);
void func_800320E8(void *block);
void func_800295D8(s32, s32, s32, s32);
void func_800284B4(s32 *a, s32 *b);
void func_80028470(s32 a, s32 b);
void func_800B8354(void);
void func_800B9F78(BattleMenu *menu);
void func_800BF0B4(s32 arg0);
void func_800AA320(u16 index, u16 mask, s32 arg2);

void func_80076544(void);
void func_8008A9C0(s32 skipped);
void func_800A9A50(Matrix *m, s32 arg1, u32 *ot, s32 buffer);
void func_800B136C(void);
void func_800B8068(u16 sound);
void func_800BB9D4(void);
void func_800BBAB8(void);
void func_800BD3AC(SlotSprite *sprite, s32 command, s32 kind);
void func_800BE0DC(void);
void func_800BEB04(void);
void func_800BEBC4(void);
void func_800BEC18(void);

#endif
