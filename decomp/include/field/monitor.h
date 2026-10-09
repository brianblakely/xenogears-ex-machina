#ifndef FIELD_MONITOR_H
#define FIELD_MONITOR_H

/* The field and its debug monitor (debug595, at 80280000): the monitor's
 * entries and debug-lines flag, which the field calls and sets while
 * D_800C268C is clear, and the field's state and calls that the monitor
 * shows, edits and makes. The field and the monitor both include this; the
 * monitor's views of the field's own objects (its view, work block, actors
 * and emitters) stay in debug595.h. */

#include "common.h"

/* The monitor: its debug-lines flag and entries (debug595.c), and the
 * field's flag for its absence (field_common.c, set by 80077e88) and its
 * page toggle (field.c). */
extern s32 D_800C268C;         /* set when the debug monitor is absent */
extern s32 D_80285988;         /* debug lines shown: the field sets it on a talk or a
                                * touch, the monitor clears it each frame (80281400) */
extern s32 D_800ADAFC;         /* the monitor's page toggle (field.c) */

void func_80281B00(char *name); /* start a named timer ("EVENT CODE", "PARTICLE  ") */
void func_802811EC(void);
void func_80281204(s32 kind);
void func_8028125C(void);
void func_802812A4(void);
void func_80281400(void);
void func_80281450(void);
void func_802815B0(void);
void func_80284EA4(void);      /* move the camera by the pad (L2 and the debug button) */

/* This frame's held and repeated buttons per port, which the monitor reads
 * (80074700 drains them; the field's field_pad.h has the newly pressed ones
 * and port 1's mask). */
extern u16 D_800AFE9C;         /* port 1 held */
extern u16 D_800C3900;         /* port 1 repeated (they move a window's choice) */
extern u16 D_800AFEA0;         /* port 2 held */
extern u16 D_800C3908;         /* port 2 repeated */
extern s16 D_800ADB02;         /* frames stuck against terrain (player control) */

/* The frame: its times and the draw buffer. */
extern s32 D_800ADB9C;         /* frame start time; the monitor's marks restart it */
extern s32 D_800ADBA0;         /* frame draw (CPU) time */
extern s32 D_800ADBA4;         /* GPU time (the VSync counter, 8007781c) */
extern s32 D_800ADB08;         /* the current draw buffer (0 or 1) */
void func_80073E38(void);      /* refresh the instances' bounds and modes */
void func_80071D08(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr); /* start a fade */

/* The camera. */
extern s32 D_800ADB94;         /* camera distance */
extern s32 D_800ADB98;         /* set by the monitor's dolly (80284ea4); the field's
                                * motion tests it (8007b814, 80084158) */
s32 func_8009A514(void);       /* camera octant (0..7; the monitor's CamDIR) */
s32 func_8009744C(void);       /* the controlled actor's facing octant (ChrDIR) */

/* The events: the actor count, the variable bank and its read (a reference
 * is a byte offset into the bank). Variable 0 is the scenario flag. */
extern s32 D_800ADBFC;         /* event actor count */
extern s16 D_800C3A68[0x400];  /* event variable bank */
s32 func_800A3018(s32 reference); /* read a variable */
void func_8008E718(void);      /* draw distinct random numbers into the work block */
void func_800A3F4C(void);      /* write the snapshot */

/* The particle effects: the template the events (and the monitor's editor)
 * set up, and the effect slots. */
extern s32 D_800B0044;         /* the template the monitor edits (BANK); cleared with
                                * the templates */
extern s32 D_800ADB40;         /* the selected template's +52 (80089004), 0xff none */
s32 func_800A99A8(s32 owner);  /* start an effect; -1 when no slot */
void func_800A98E8(s32 owner, s32 release); /* stop an owner's effects */

#endif
