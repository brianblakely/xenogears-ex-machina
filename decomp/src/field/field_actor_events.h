#ifndef FIELD_FIELD_ACTOR_EVENTS_H
#define FIELD_FIELD_ACTOR_EVENTS_H

#include "field.h"

/* Event-instruction helpers of the actor script interpreter (8009e1a0-800a5924). */
s32 func_8009EB48(FieldActor *actor, s32 tag); /* -1 when a slot has `tag` */
s32 func_800A3090(s32 actor, s32 event);       /* entry PC of an actor's event */

#endif
