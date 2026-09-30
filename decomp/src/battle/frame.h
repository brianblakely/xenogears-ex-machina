#ifndef BATTLE_FRAME_H
#define BATTLE_FRAME_H

/* The battle's frame loop, controller state and slot sprites (800BE538-
 * 800BF0B4): the late unit addresses the area from D_800C3EB0 as one
 * aggregate, BattleFrame. */

#include "common.h"
#include "psyq.h"
#include "scene.h"
#include "battle_core.h"
#include "files.h"
#include "objects.h"
#include "screen.h"
#include "sprite.h"
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


/* The resident sprite resource block of D_8006BE10. */
typedef struct {
    u8 pad0[0x10];
    u16 *motions; /* 0x10: a count, then offsets from here */
} SpriteResource;

#define SPRITE_RESOURCE ((SpriteResource *)D_8006BE10)

/* The battle slot of a slot's sprite. */
#define SPRITE_SLOT(sprite) ({ s32 low_ = (sprite)->frameBits.bits.slotLow; (sprite)->motion.bits.slotHigh << 2 | low_; })

/* A point of the battle menu's walk (6 bytes); x and z 0xFFFF end it. */
typedef struct {
    u16 x;
    u16 z;
    u8 run; /* 0x04 */
    u8 pad5;
} PathPoint;


#define STACK_LEAVE() __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory")

/* A slot's sprite source (0xC bytes). */
typedef struct {
    void *data;
    s16 x;
    s16 y;
    s32 variant;
} SpriteSource;

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
    BattleSprite *sprites[11];  /* 0x8C8C: the slots' sprites */
    struct ActorTask *tasks[11]; /* 0x8CB8: their tasks */
    u8 pad8CE4[0x8D24 - 0x8CE4];
    SpriteSource sources[11];   /* 0x8D24 */
    u8 pad8DA8[0x8DAC - 0x8DA8];
    s32 frameTicks;             /* 0x8DAC: vertical blanks of the last frame */
} BattleArea;

/* battle_core.h declares the area's first member as D_800C3EB0. */
#define BATTLE_AREA (*(BattleArea *)&D_800C3EB0)

/* Layout checks. */
typedef char BattleFrameCheck[(sizeof(BattleSlot) == 0x1C && sizeof(BattleEvent) == 0x48 && sizeof(PathPoint) == 6) ? 1 : -1];

extern s32 D_800C37D0;    /* frame loop nesting */
extern s16 D_80059494;    /* extra vertical blanks of the last frame (0-4) */
extern s32 D_80059198;    /* frame skip */
extern u16 D_800591B4;    /* a sound request */
extern u8 D_800C3780;     /* a slot's sprite commands run */
extern s32 D_80010000;    /* the debugger's word, -1 none */
extern u8 D_800CCB94[];
extern u16 D_800D30E4;    /* the frame time */

/* The battle menu (D_800C3610, 0x50 bytes). */
typedef struct BattleMenu {
    u8 pad0[4];
    struct BattleSprite *sprite;              /* 0x04: the acting slot's */
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
    struct BattleSprite *target;              /* 0x4C */
} BattleMenu;

extern BattleMenu *D_800C3610;
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
void func_80037324(u32 *ot);
void func_80280A9C(void); /* the debugger's frame hook */
s32 func_8003569C(s32 pad);
void func_800B8354(void);
void func_800B9F78(BattleMenu *menu);
void func_800BF0B4(s32 arg0);
void func_800AA320(u16 index, u16 mask, s32 arg2);

void func_80076544(void);
void func_8008A9C0(s32 skipped);
void func_800A9A50(Matrix *m, s32 arg1, u32 *ot, s32 buffer);
void func_800B8068(u16 sound);
void func_800BB9D4(void);
void func_800BBAB8(void);
void func_800BD3AC(BattleSprite *sprite, s32 command, s32 kind);
void func_800BE0DC(void);
void func_800BEB04(void);
void func_800BEBC4(void);
void func_800BEC18(void);

/* The acting slot's walk and command file (800BEFF4-800BF4F0). */
extern void *D_800C3618;             /* the loaded command file */
extern s32 D_800C361C;               /* its slot */
extern u8 D_800D3350;                /* the command file is started */
extern u16 D_800D3634;               /* the current event's targets */
extern BattleSprite *D_800D363C[];     /* their sprites, NULL ended */
extern s16 D_800D3678;               /* their count */

void func_800B9C00(BattleSprite *sprite);
s32 func_800BEEB4(u32 mask, BattleSprite **list, BattleSprite *target);
s16 func_800BEF24(BattleSprite *from, BattleSprite *to);
s16 func_800BEF8C(BattleSprite *sprite);
void func_800BF0C4(BattleSprite *sprite);
void func_800BF1EC(BattleSprite *sprite, s32 mode);
void func_800BF4F0(BattleSprite *sprite, BattleSprite *target);
s32 func_800C07CC(GroundPoint from, GroundPoint to);
SoundSystem *func_800C0FAC(s32 *file);
void func_800C1140(s32 *file);

/* Command motions, value watches and targets (800BF5E8-800BF998). */
typedef struct SlotWatch {
    u8 pad0[0xC];
    void (*destroy)(struct SlotWatch *watch);  /* 0x0C */
    u8 pad10[0x1C - 0x10];
    BattleSprite *sprite;                        /* 0x1C */
    s32 mode;                                  /* 0x20: the sprite's mode at the start */
    s32 value;                                 /* 0x24: its last value */
    s32 threshold;                             /* 0x28 */
    void (*callback)(BattleSprite *sprite);      /* 0x2C */
} SlotWatch;

extern s32 D_800C3CE8;           /* finished sprite motions */
extern s32 D_800C3628;
extern s16 D_800D2D4C;           /* effect hits */

void func_80021BF8(BattleSprite *sprite, void (*callback)(void)); /* at the motion's end */
s32 func_800B57E4(BattleSprite *sprite);
void func_800B7C34(s32 command);
void func_800BD2E4(void);
s32 func_800BF720(void);

/* Requested loads, gear restarts and effect sprites (800BF9EC-800BFDA8). */
extern u8 D_800C3620;            /* the sound bank of file 5 is loaded */
extern u8 D_800C3621;            /* upload the images of file 1 */
extern s8 D_800C3622;
extern u8 D_800C362C;            /* restart the party's gears (2: all but the acting) */
extern s32 D_800C3A6C;
extern struct ActorTask *D_8005958C;   /* the main task list */

BattleSprite *func_80023B84(BattleSprite *owner, void *motion, void *resource); /* create an effect sprite */
s32 func_80037FD8(void *bank, s32 flags); /* transfer a sound bank */
s32 func_800383EC(u16 id);
s16 func_8003BDFC(s32 wait);             /* sound transfer busy */
void func_800B8D04(void);
BattleSprite *func_800BFC80(BattleSprite *sprite, s32 mode, s32 action);

/* Distances, blends and command file parts (800C06E4-800C1140). */
typedef struct {
    s16 x;
    s16 y;
} VramPoint;

extern s32 (*D_800C3A68)[4]; /* four weights per cell, 8 cells a row */

void func_80022224(void *resource, void *image, VramPoint at, VramPoint clut, s32 arg4); /* upload an image */
void func_80031F70(void *block, s32 size); /* shrink a heap block */
void func_80038310(s32 bank); /* release a wave bank */

#endif
