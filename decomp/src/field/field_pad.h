#ifndef FIELD_FIELD_PAD_H
#define FIELD_FIELD_PAD_H

/* Pad input and the mouse pointer: the buttons the field drains from the
 * resident's pad queue each frame (80074700), the player control's tables
 * and the pointer's pads, divisors and bounds (field.c, field_motion.c). */

#include "common.h"
#include "field/monitor.h"

/* This frame's buttons per port (80074700 drains them from the resident's
 * pad queue). Port 1's are masked by the work block's input mask and
 * 800adb00; all are dropped on a camera cut. The held and repeated ones are
 * in field/monitor.h (the debug monitor reads them too). */
extern u16 field_pad_port0_pressed;         /* port 1 newly pressed */
extern u16 field_pad_unread_port1_pressed;  /* port 2 newly pressed */
extern u16 field_pad_port0_allowed_mask;    /* port 1 buttons the position allows */

void field_pad_drain_queue(void);      /* drain the pad queue into this frame's buttons */

/* Player control (event a7); its count of frames stuck against terrain,
 * field_player_stuck_frames, is in field/monitor.h. */
extern s32 field_player_control_polled;           /* pad input polled this pass */
extern s32 field_unread_jump_start_history_index; /* latched jump setting */
extern u16 field_dpad_headings[16];               /* d-pad direction per button state */
extern u16 field_dpad_alt_headings[16];           /* alternate d-pad directions */

/* The pointer: two pad buffers, X and Y divisors, a position per port and
 * the bounds, all scaled by the divisors. */
extern s8 *field_pointer_pads[2];      /* pointer pad buffers */
extern u16 field_pointer_x_divisor;    /* X divisor */
extern u16 field_pointer_y_divisor;    /* Y divisor */
extern s32 field_pointer_x[2];         /* X per port */
extern s32 field_pointer_y[2];         /* Y per port */
extern s32 field_pointer_left;         /* left */
extern s32 field_pointer_top;          /* top */
extern s32 field_pointer_right;        /* right */
extern s32 field_pointer_bottom;       /* bottom */

void field_pointer_init(void);              /* set the pointer up: pads, divisors, bounds, both ports */
void field_pointer_set_pads(void *pad0, void *pad1);
void field_pointer_set_bounds(s32 left, s32 right, s32 top, s32 bottom);
void field_pointer_set_divisors(s32 x_divisor, s32 y_divisor);
void field_pointer_set_position(s32 port, s32 x, s32 y);
s32 field_pointer_read(s32 port, s32 *out); /* read a port: x, y, buttons, motion */
void field_pointer_move_by_mouse(s32 port); /* move a mouse port's pointer by its motion */

#endif
