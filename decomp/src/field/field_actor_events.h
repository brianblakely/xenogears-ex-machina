#ifndef FIELD_FIELD_ACTOR_EVENTS_H
#define FIELD_FIELD_ACTOR_EVENTS_H

#include "field.h"

/* Event-instruction helpers of the actor script interpreter (8009e1a0-800a5924). */
/* An actor's boundary quadrilateral (+114 while state bit 12 is set). */
typedef struct {
    s16 x;
    s16 z;
} BoundaryCorner;

typedef struct {
    BoundaryCorner corners[4];
} ActorBoundary;

/* An actor's integer position cached at +68 (x, y, z). */
#define ACTOR_CACHED_POSITION(actor) ((s16 *)((u8 *)(actor) + 0x68))

/* Player control (event a7). */
extern u16 D_800AFE9C;         /* held pad buttons */
extern u16 D_800C2694;         /* newly pressed pad buttons */
extern s16 D_800ADB02;         /* frames stuck against terrain */
extern s32 D_800ADB28;         /* latched jump setting */
extern s32 D_800ADB64;         /* jump contact, 0xff none */
extern s32 D_800ADB68;         /* pad input polled this pass */
extern u16 D_800ADF68[16];     /* d-pad direction per button state */
extern u16 D_800ADF88[16];     /* alternate d-pad directions */
extern void func_80079288(void);

extern s32 D_8006F990[3];    /* descriptor of each party slot's actor */
void func_8009E574(s32 x, s32 z);
void func_800A0158(s32 slot, s32 *a, s32 *b, s32 *c);
void func_800A0D3C(void);

s32 func_8009EB48(FieldActor *actor, s32 tag); /* -1 when a slot has `tag` */
s32 func_800A3090(s32 actor, s32 event);       /* entry PC of an actor's event */

#endif
