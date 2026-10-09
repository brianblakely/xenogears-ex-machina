#ifndef BATTLE_FRAME_H
#define BATTLE_FRAME_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/sprite.h"

/* The battle's frame loop and battle menu (800BD3AC's unit, 800BE6EC-
 * 800BFE48): a frame of the battle (800BE790), the module loads, the battle
 * menu that runs the acting slot's walk and command file, command motions,
 * value watches and targets, requested loads, gear restarts and effect
 * sprites; and the distances and command file parts of 800BFE48's unit
 * (800C06E4-800C08CC, 800C0F70-800C11CC). The late units address the battle
 * area from D_800C3EB0 as one aggregate, BattleArea (battle/area.h). */

/* The battle's sprite source (resident D_8006BE10). */
#define SPRITE_SOURCE ((SpriteSource *)D_8006BE10)

extern s32 D_800C37D0;    /* frame loop nesting */
extern u8 D_800C3780;     /* a slot's sprite commands run */
extern u8 D_800CCB94[];
extern u16 D_800D30E4;    /* the frame time */
extern u8 D_800D2FDC;
extern u8 D_800D36B8;     /* the battle's start mode */
extern u8 D_800C4A39;     /* BATTLE_AREA.buffers[0].drawEnv.r0, which 800B8098 addresses apart from the area */
extern s32 D_800C3D58;    /* gear enemies present */

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

/* The acting slot's walk and command file (800BEFF4-800BF4F0). */
extern void *D_800C3618;             /* the loaded command file */
extern s32 D_800C361C;               /* its slot */
extern u8 D_800D3350;                /* the command file is started */
extern u16 D_800D3634;               /* the current event's targets */
extern Sprite *D_800D363C[];     /* their sprites, NULL ended */
extern s16 D_800D3678;               /* their count */

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

/* Requested loads, gear restarts and effect sprites (800BF9EC-800BFDA8). */
extern u8 D_800C3620;            /* the sound bank of file 5 is loaded */
extern u8 D_800C3621;            /* upload the images of file 1 */
extern u8 D_800C3622;            /* wave bank 7 is loaded (a gear frame's turn) */
extern u8 D_800C362C;            /* restart the party's gears (2: all but the acting) */
extern SoundSequence *D_800C3A6C; /* the transferred wave bank of a command file */

/* Distances, blends and command file parts (800C06E4-800C1140). */
typedef struct {
    s16 x;
    s16 y;
} VramPoint;

extern s32 (*D_800C3A68)[4]; /* four weights per cell, 8 cells a row */

/* The frame loop and the battle menu. */
void func_800BE538(s32 slot, s32 a, s32 b, s32 c); /* run commands on a slot's sprite and wait */
void func_800BE790(void);        /* run one battle frame */
void func_800BEB04(void);        /* load the requested battle module */
void func_800BED30(void);        /* clear the battle menu state */
BattleMenu *func_800BED4C(void); /* open the battle menu */
void func_800BEDE8(void);        /* close the battle menu */
void func_800BEE2C(s32 index, s32 mask, s32 mode); /* 800AA320 on a stack of its own */
s16 func_800BEF24(Sprite *from, Sprite *to); /* the direction between two sprites */
s16 func_800BEF8C(Sprite *sprite); /* the direction to a sprite's target point */
void func_800BF0B4(s32 arg0);    /* set the battle menu's state */
void func_800BF0C4(Sprite *sprite); /* walk a sprite along the path */
void func_800BF2B8(Sprite *sprite); /* load the file of a sprite's command */
s32 func_800BF354(void);         /* start the loaded command file once */
void func_800BF3A4(void);        /* stop the started command file */
void func_800BF3E8(Sprite *sprite); /* face a sprite and the event's first target */
void func_800BF4F0(Sprite *sprite, Sprite *target);
void func_800BF600(s32 command, Sprite *sprite); /* run a command and wait for its motion */
void func_800BF6CC(void);        /* update the battle menu when it is open */
s32 func_800BF6F8(void);
s32 func_800BF720(void);         /* whether a number popup shows */
void func_800BF730(s32 value);
void func_800BF85C(s32 index, s32 slot);
void func_800BF8CC(Sprite *sprite); /* move a sprite's target to the next target */
s32 func_800BF954(Sprite *sprite); /* a sprite's index among the targets */
void func_800BF998(void);        /* count an effect hit */
void func_800BF9EC(void);        /* upload the requested images */
void func_800BFA9C(void);        /* restart the party's gears once requested */
void func_800BFBA0(void);        /* load the sound bank of file 5 once */
Sprite *func_800BFC80(Sprite *sprite, s32 mode, s32 action); /* find or destroy a sprite's effect sprites */
void func_800BFD88(Sprite *sprite, s32 mode);
void func_800BFDA8(Sprite *sprite, s32 mode);

/* Distances and command file parts (800BFE48's unit). */
s32 func_800C07CC(GroundPoint from, GroundPoint to); /* the distance between two points */
void func_800C0828(SVECTOR *from, SVECTOR *to, SVECTOR *angles); /* the direction angles between points */
void func_800C0F70(void);        /* release the transferred sound bank */
SoundBank *func_800C0FAC(s32 *file); /* set up a command file's parts */
void func_800C1140(s32 *file);   /* free a command file's sound bank */

#endif
