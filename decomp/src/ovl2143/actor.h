#ifndef OVL2143_ACTOR_H
#define OVL2143_ACTOR_H

/* The scene module's actors: a model hierarchy with its channels, image
 * animations and surfaces, its effect script state and animation events,
 * the files it is created from, its sound bank, the sprites linked to its
 * nodes and the points anchored to them. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "hierarchy.h"
#include "image_anim.h"
#include "particles.h"
#include "surface.h"

/* Callers convert arguments/result differently from the resident
 * definition (s16 angles and ids there, its result a bank pointer; s16
 * modes): turn a sprite and set its angle, the loaded bank with `bank`'s id
 * (nonzero when there is one) and an image list upload. */
void func_80021FE0(Sprite *sprite, s32 direction);
void func_800223B0(Sprite *sprite, s32 angle);
s32 func_8003864C(SoundBank *bank, s32 id);
void func_8002DDE4(void *images, s32 on, s32 a, s32 b, s32 c, s32 d, s32 e);

/* An actor's sound block (the script file's second part, an offset table
 * relocated in place): its sound effect bank, then the end of the bank's
 * data (the same address when there is none). */
typedef struct {
    u8 pad0[8];
    SoundBank *bank;        /* +8 */
    void *end;              /* +c */
} SoundOwner;

/* A table of script entry points. */
typedef struct {
    s32 count;
    s32 *entries;
} EntryTable;

/* A scene actor: a model hierarchy with its script state. */
typedef struct Actor {
    ModelTable *models;    /* +0 */
    ModelPart *parts;       /* +4 */
    s32 *entries;           /* +8: script entry points */
    EntryTable *shared;     /* +c: entries 0x50- */
    s32 pc;                 /* +10: script position */
    s32 *locals;            /* +14: [0] count, then entries 0-0x3f */
    s32 *globals;           /* +18: [0] count, then entries 0x40- */
    s16 scale;              /* +1c */
    s16 h1E;                /* +1e */
    u8 index;               /* +20: slot in D_801E8670 */
    u8 b21;                 /* +21 */
    u8 b22;                 /* +22 */
    u8 b23;                 /* +23 */
    s16 size[3];            /* +24: height, x and z extents */
    u8 reference;           /* +2a: default entry reference (bit 7 flag) */
    u8 depth;               /* +2b: queued calls + 1 */
    u8 queue_source[4];     /* +2c */
    u8 queue_entry[4];      /* +30 */
    u8 active;              /* +34 */
    u8 b35;                 /* +35 */
    u8 b36;                 /* +36 */
    u8 scaled;              /* +37: build with the scaled hierarchy update */
    u8 b38;                 /* +38 */
    u8 b39;                 /* +39 */
    s16 h3A;                /* +3a */
    u16 h3C;                /* +3c */
    s16 h3E;                /* +3e */
    u16 h40;                /* +40 */
    u16 h42;                /* +42 */
    u16 h44;                /* +44 */
    u16 h46;                /* +46 */
    s16 h48;                /* +48 */
    u16 flags;              /* +4a */
    s32 w4C;                /* +4c */
    s32 w50;                /* +50 */
    s32 w54;                /* +54 */
    s16 aim_actor;          /* +58: reference of the actor aimed at */
    s16 aim_node;           /* +5a */
    u8 parent;              /* +5c: actor carrying this one, 0xff none */
    u8 inherit;             /* +5d: take the carrier node's rotation */
    s16 parent_node;        /* +5e */
    s16 h60;                /* +60: root height */
    u8 b62;                 /* +62 */
    u8 b63;                 /* +63 */
    s16 aim_offset[3];      /* +64: point aimed at in the node's space */
    s16 offset[3];          /* +6a: position in the carrier node's space */
    s16 spin[3];            /* +70: angular step, applied in eighths */
    s16 spin_accel[3];      /* +76: added to spin each tick */
    s16 drift[3];           /* +7c: local movement before model/actor scaling */
    s16 drift_accel[3];     /* +82: added to drift each tick */
    s16 target[3];          /* +88 */
    s16 h8E;                /* +8e */
    s16 h90;                /* +90: -1 none */
    s16 h92;                /* +92 */
    u16 shift_x;            /* +94: added to image animation positions */
    u16 shift_y;            /* +96 */
    s16 anim_state;         /* +98: the frame, -1 none */
    s16 anim_loop;          /* +9a: -1 no loop */
    s16 anim_frame;         /* +9c: the events run */
    s16 anim_frames;        /* +9e: the event count */
    u8 *anim_pos;           /* +a0: next event */
    u8 *anim_start;         /* +a4 */
    void *group;            /* +a8: the model group block */
    void *blockAC;          /* +ac */
    SoundOwner *ownerB0;    /* +b0: the actor's sound bank */
    SoundOwner *ownerB4;    /* +b4 */
    POLY_FT4 prims[2];      /* +b8: a shadow quad per buffer */
    u8 pad108[2];
    u16 mask;               /* +10a */
    u8 channel_count;       /* +10c */
    u8 count10D;            /* +10d: 0x24-byte records at +114 */
    u8 count10E;            /* +10e: 0x30-byte records at +118 */
    u8 pad10F;
    ColorFade *channels;    /* +110 */
    Surface *records24;     /* +114 */
    ImageAnim *records30;   /* +118 */
    s32 previous[3];        /* +11c: root position before the step */
    s32 moved[3];           /* +128: root movement of the step */
} Actor;

/* This overlay's link of a sprite to an actor node. */
typedef struct {
    u8 pad0[4];
    void (*update)(Task *task); /* +4: the sprite's own update */
    Actor *actor;           /* +8 */
    s16 node;               /* +c */
    s16 follow;             /* +e: take the height from the actor */
    SVECTOR offset;         /* +10 */
} SpriteLink;

/* A sprite attachment of an actor script. */
typedef struct {
    u8 pad0[5];
    u8 node;                /* +5 */
    u16 offset[3];          /* +6 */
    u8 follow;              /* +c */
    u8 padD[6];
    u8 linked;              /* +13 */
} SpriteSpec;

/* A point attached to an actor node (0x14 bytes): its world position is
 * the node's matrix applied to `offset` (func_801E1880). */
typedef struct {
    s16 pos[3];
    s16 active;             /* +6 */
    SVECTOR offset;         /* +8 */
    s16 actor;              /* +10: -1 none */
    s16 node;               /* +12 */
} Anchor;

/* An actor's scene description (the file's +10 record's +4). */
typedef struct {
    u8 pad0[2];
    s16 size[3];            /* +2 */
    s16 scale;              /* +8 */
    u8 reference;           /* +a */
    u8 padB;
    u16 flags;              /* +c */
    u8 channel_count;       /* +e */
    u8 padF;
    u8 count30;             /* +10 */
    u8 pad11;
    u8 count24;             /* +12 */
    u8 pad13;
    u16 records[1];         /* +14: the records24 descriptions */
} ActorDesc;

typedef struct {
    u8 pad0[4];
    ActorDesc *desc;        /* +4 */
    s32 *tables[1];         /* +8: per records24 entry */
} ActorInfo;

/* An actor's files: images, the model group (up to the hierarchy links) and
 * its description; and its script block with its sound bank. */
typedef struct {
    u8 pad0[4];
    void *images;           /* +4 */
    u8 *group;              /* +8 */
    HierarchyLink *links;   /* +c: follows the model group */
    ActorInfo *info;        /* +10 */
} ActorFile;

typedef struct {
    u8 pad0[4];
    s32 *locals;            /* +4 */
    s32 entries[1];         /* +8 */
} ScriptBlock;

typedef struct {
    u8 pad0[4];
    ScriptBlock *script;    /* +4 */
    SoundOwner *owner;      /* +8 */
} ActorScript;

void func_800796F4(void);

void func_801DCEC8(Actor *actor, MATRIX *m, MATRIX *light, s32 mode, s32 ticks, u32 *ot, s32 buffer);
void func_801E1880(Actor **actors);
void func_801E3534(Actor *actor, EffectPool *pool, s32 *entries, s32 *locals);
void func_801E35D0(Actor *actor, Actor *source, EffectPool *pool, s32 entry);
s32 func_801E36BC(Actor *actor, EffectPool *pool, s32 ticks, s32 arg3, s32 arg4);
void func_801E37D0(Actor *actor);
void func_801E39F0(Actor *actor, EffectPool *pool, s32 arg2, s32 arg3, s32 arg4);
void func_801E5C74(Actor *actor, Animation *anim, s32 loop);
s32 func_801E5CD8(Actor *actor, s32 which);
void func_801E5D44(Actor *actor, EffectPool *pool, s32 arg2);
void func_801E632C(Actor *actor);
void func_801E63A8(Actor *actor);
s16 func_801E66BC(VECTOR *dir, void *a, void *b, s32 divisor);
s32 func_801E67F8(void);
s32 func_801E6830(Actor *actor, u8 ref, u16 *mask);
s32 func_801E6910(Actor *actor, u8 ref, s32 *flag);
void func_801E6974(Actor *actor, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 tag,
                   u8 smooth, s16 x0, s16 y0, s16 z0, s16 x1, s16 y1, s16 z1, s16 duration);
void func_801E6D94(Actor *actor, ModelPart *part, s32 flags);
void func_801E6F64(SpriteTask *sprite);
void func_801E7094(Actor *actor, ModelPart *part, u8 flags, s16 x, s16 y, s16 z);
void func_801E7298(Actor *actor);
void func_801E8030(s32 index);
void func_801E8330(u16 index, u16 mask, s32 arg2);
void func_801E8394(Actor *source, u16 index, u16 mask, s32 arg3);
s32 func_801E8480(s32 index);
void func_801E8510(Actor *actor);

#endif
