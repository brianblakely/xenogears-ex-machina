#ifndef BATTLE_SPRITE_VM_H
#define BATTLE_SPRITE_VM_H

/* The battle's copy of the resident sprite animation VM (800C11CC). */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "sprite_effect.h"

/* A little-endian s16 at index i of a command's arguments, as the VM
 * forms it (the high byte shifted, then narrowed). */
#define VM_S16(p, i) ((s16)((p)[(i) + 1] << 8) | (p)[i])

extern u8 D_8004FC40[];   /* resident: the byte widths of commands 80-FF */
extern SVECTOR D_800D30B0; /* the camera's angles */
extern s32 D_800D30B8;     /* the camera's distance */
extern u8 D_800C3624;      /* set by command 8F */

/* Callers convert arguments/result differently from the resident definition:
 * the resident VM's commands (called unprototyped), and 80021c20's u8 result
 * taken as an int. */
void func_8001FBE4();
s32 func_80021C20();

/* Battle services (called unprototyped). */
u8 *func_800B168C();
s32 func_800B16A4();
void func_800B1720();
void func_800B1EA0();
void func_800B3658();
s32 func_800B3B6C(); /* a u8, taken as int */
void func_800B3F04();
void func_800BA614(Sprite *sprite);
void func_800BA768(Sprite *sprite);
void func_800BF8CC(Sprite *sprite);
s32 func_800BF954(Sprite *sprite);

void func_800C11CC(Sprite *sprite);

#endif
