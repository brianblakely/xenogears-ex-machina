#ifndef BATTLE_SPRITE_VM_H
#define BATTLE_SPRITE_VM_H

/* The battle's copy of the resident sprite animation VM (800C11CC): the
 * fields of a sprite (BattleSprite, the resident Sprite) that the VM uses
 * beyond BattleSprite's, as views of their words. */

#include "common.h"
#include "psyq.h"
#include "sprite.h"

/* The frame word at 0xA8 (resident Sprite frame_bits). */
typedef union {
    u32 word;
    struct {
        unsigned sequencerOwned : 1;
        unsigned bounce : 10;
        unsigned frame : 6;   /* frame table index */
        unsigned step : 3;
        unsigned phase : 2;
        unsigned commands : 6; /* commands run in the current step, 63 at most */
        unsigned state : 2;   /* 0 idle, 1 ended by 81, 2 waited on by 98 */
        unsigned slotLow : 2;
    } bits;
} SpriteFrameBits;

/* The motion word at 0xAC (resident Sprite motion). */
typedef union {
    u32 word;
    struct {
        unsigned slotHigh : 2;
        unsigned mirror : 1;    /* mirror every frame */
        unsigned frameFlip : 1; /* the current frame is mirrored */
        unsigned pad4 : 3;
        unsigned divisor : 12;  /* frame time scale, 256 = 1 */
        unsigned pad19 : 13;
    } bits;
} SpriteMotionBits;

/* The render word at 0x3C (resident Sprite render). */
typedef union {
    u32 word;
    struct {
        unsigned sides : 2;  /* 1: one-sided */
        unsigned pad2 : 1;
        unsigned flip : 1;   /* mirrored frame */
        unsigned flipY : 1;
        unsigned blend : 3;
        unsigned pad8 : 24;
    } bits;
} SpriteRenderBits;

#define SPRITE_RENDER_BITS(sprite) (((SpriteRenderBits *)&(sprite)->render)->bits)
#define SPRITE_FRAME_BITS(sprite) (((SpriteFrameBits *)&(sprite)->frameBits)->bits)
#define SPRITE_MOTION_BITS(sprite) (((SpriteMotionBits *)&(sprite)->motion)->bits)
/* The completion callback (0x68). */
#define SPRITE_CALLBACK(sprite) (*(void (**)(BattleSprite *))(sprite)->pad68)
/* The frame remapping table (0x60) of one-sided sprites. */
#define SPRITE_FRAME_MAP(sprite) (*(u8 **)&(sprite)->pad54[0x60 - 0x54])
/* The motion a sprite's parent is waited on in (0x8D, command 98). */
#define SPRITE_WAIT_MOTION(sprite) (*(s8 *)&(sprite)->pad86[0x8D - 0x86])

/* A little-endian s16 at index i of a command's arguments, as the VM
 * forms it (the high byte shifted, then narrowed). */
#define VM_S16(p, i) ((s16)((p)[(i) + 1] << 8) | (p)[i])

extern u8 D_8004FC40[];   /* resident: the byte widths of commands 80-FF */
extern s32 D_80059428;
extern SVECTOR D_800D30B0; /* the camera's angles */
extern s32 D_800D30B8;     /* the camera's distance */
extern u8 D_800C3624;      /* set by command 8F */

/* Resident sprite services. */
void func_8001D2B0(BattleSprite *sprite, s32 frame);
u8 *func_8001FBA4(BattleSprite *sprite, u8 *code);
void func_8001FBE4();      /* the resident VM's commands */
s32 func_80021C20(); /* a u8, taken as int */
s32 func_80021C6C(BattleSprite *sprite);
void func_80021CA0(BattleSprite *sprite, u8 value);
void func_80021CF8(BattleSprite *sprite, u8 *value);
void func_80021FC0(BattleSprite *sprite, s32 speed);
void func_80022D44(BattleSprite *sprite);
void func_80023538(BattleSprite *sprite, u8 *animation);

/* Battle services (called unprototyped). */
u8 *func_800B168C();
s32 func_800B16A4();
void func_800B1720();
void func_800B1EA0();
void func_800B3658();
s32 func_800B3B6C(); /* a u8, taken as int */
void func_800B3F04();
void func_800BA614(BattleSprite *sprite);
void func_800BA768(BattleSprite *sprite);
void func_800BF8CC(BattleSprite *sprite);
s32 func_800BF954(BattleSprite *sprite);

void func_800C11CC(BattleSprite *sprite);

#endif
