#ifndef OVL3384_DEBRIS_H
#define OVL3384_DEBRIS_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "battle/effect_script.h"

/* One flying piece of the broken model (0x54 bytes). */
typedef struct {
    SVECTOR rotation;  /* +00 */
    SVECTOR spin;      /* +08: added to the rotation every frame */
    s32 position[3];   /* +10: 16.16 fixed point */
    u32 unk1C;
    s32 velocity[3];   /* +20 */
    u32 unk2C;
    s32 gravity;       /* +30: added to the vertical velocity */
    SVECTOR vertex[4]; /* +34 */
} Piece;

/* The effect task (0x74 bytes; resident tasks: the update node, then the
 * drawing node, both with the task as their data): a model (an unrelocated
 * effect script entry) broken into flying pieces. */
typedef struct {
    Task task;          /* +00 */
    Task draw;          /* +1c */
    MATRIX matrix;      /* +38: the model's placement */
    s32 count;          /* +58: pieces */
    s32 life;           /* +5c: frames left */
    u32 unk60;
    void *prims[2];     /* +64: the pieces' primitives, per display buffer */
    ScriptEntry *model; /* +6c */
    Piece *pieces;      /* +70 */
} DebrisTask;

extern SVECTOR battle_module_debris_origin; /* origin */

void battle_module_debris_destroy(Task *node);
void battle_module_debris_update(Task *node);
void battle_module_debris_draw(Task *node);

#endif
