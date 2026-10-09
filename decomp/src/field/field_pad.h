#ifndef FIELD_FIELD_PAD_H
#define FIELD_FIELD_PAD_H

/* Pad input and the mouse pointer: the buttons the field drains from the
 * resident's pad queue each frame (80074700), the player control's tables
 * and the pointer's pads, divisors and bounds (field.c, field_8007A44C.c). */

#include "common.h"

/* This frame's buttons per port. Port 1's are masked by the work block's
 * input mask and 800adb00; all are dropped on a camera cut. */
extern u16 D_800AFE9C;         /* port 1 held */
extern u16 D_800C2694;         /* port 1 newly pressed */
extern u16 D_800C3900;         /* port 1 repeated (they move a window's choice) */
extern u16 D_800AFEA0;         /* port 2 held */
extern u16 D_800C38F8;         /* port 2 newly pressed */
extern u16 D_800C3908;         /* port 2 repeated */
extern u16 D_800ADB00;         /* port 1 buttons the position allows */

void func_80074700(void);      /* drain the pad queue into this frame's buttons */

/* Player control (event a7). */
extern s32 D_800ADB68;         /* pad input polled this pass */
extern s16 D_800ADB02;         /* frames stuck against terrain */
extern u16 D_800ADF68[16];     /* d-pad direction per button state */
extern u16 D_800ADF88[16];     /* alternate d-pad directions */

/* The pointer: two pad buffers, X and Y divisors, a position per port and
 * the bounds, all scaled by the divisors. */
extern s8 *D_800B0054[2];      /* pointer pad buffers */
extern u16 D_800B005C;         /* X divisor */
extern u16 D_800B0060;         /* Y divisor */
extern s32 D_800B0068[2];      /* X per port */
extern s32 D_800B0070[2];      /* Y per port */
extern s32 D_800C3A44;         /* left */
extern s32 D_800C3A4C;         /* top */
extern s32 D_800C3A50;         /* right */
extern s32 D_800C3A54;         /* bottom */

void func_80071EE8(void);      /* set the pointer up: pads, divisors, bounds, both ports */
void func_8007AD8C(void *pad0, void *pad1);
void func_8007ADA4(s32 left, s32 right, s32 top, s32 bottom);
void func_8007AE14(s32 x_divisor, s32 y_divisor);
void func_8007AE2C(s32 port, s32 x, s32 y);
s32 func_8007AE78(s32 port, s32 *out); /* read a port: x, y, buttons, motion */
void func_8007AF74(s32 port);          /* move a mouse port's pointer by its motion */

#endif
