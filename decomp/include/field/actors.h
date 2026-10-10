#ifndef FIELD_ACTORS_H
#define FIELD_ACTORS_H

/* The field's event actors, their descriptors and model instances, the
 * collision triangles and the loaded components that list them: the records
 * the field overlay (src/field) and its debug monitor (debug595) both read. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "resident/sprite.h"

/* The whole part of a 16.16 value (common.h's Fixed) held in an s32. */
#define WHOLE(value) (((Fixed *)&(value))->part.whole)

/* One of an actor's eight script slots (analysis/formats/field-lifecycle.md). */
typedef struct ScriptSlot {
    u16 resume_pc;      /* 0 */
    u8 countdown;       /* 2 */
    u8 tag;             /* 3 */
    u32 value : 16;     /* 4: per-slot argument (e.g. move speed) */
    u32 unk16 : 2;
    u32 priority : 4;   /* bits 18-21 */
    u32 unk22 : 1;
    u32 move_mode : 2;  /* bits 23-24 */
    u32 unk25 : 7;
} ScriptSlot;

/* An actor's packed state word (+12c). */
typedef struct {
    u32 mode : 2;   /* 0-1 */
    u32 unk2 : 3;   /* 2-4 */
    u32 unk5 : 1;   /* 5 */
    u32 depth : 3;  /* 6-8: script call depth */
    u32 octant : 3; /* 9-11 */
    u32 unk12 : 1;  /* 12 */
    u32 layer : 3;  /* 13-15: 801e layer */
    u32 unk16 : 2;  /* 16-17 */
    u32 unk18 : 10; /* 18-27 */
    u32 unk28 : 4;
} ActorState;

/* One 0x138-byte event actor record. */
typedef struct FieldActor {
    u32 flags;           /* 000 */
    u32 layer_flags;     /* 004: bits 3+ switch collision layers off */
    s16 triangle[4];     /* 008: current collision triangle per layer */
    s16 layer;           /* 010 */
    u8 unk012[2];
    u32 floor_attribute; /* 014: the collision attribute under it (80080968) */
    s16 unk18;           /* 018 */
    s16 height;          /* 01A */
    Fixed gravity;       /* 01C: 16.16 */
    s32 position[3];     /* 020: 16.16 x, y, z */
    u8 unk02C[4];
    s32 unk030[3];       /* 030 */
    u8 unk03C[4];
    s32 unk40[3];        /* 040 */
    u8 unk04C[4];
    s32 unk50[3];        /* 050 */
    u8 unk05C[4];
    s16 unk60;           /* 060 */
    s16 unk62;           /* 062 */
    s16 unk64;           /* 064 */
    u8 unk066[2];
    s16 last_position[3]; /* 068: whole x, y, z saved before the update */
    u16 stuck;           /* 06E: gather steps without moving */
    s16 unk70;           /* 070 */
    s16 unk72;           /* 072 */
    u8 ridden_actor;     /* 074: the descriptor index of the actor it rides (80084158), 0xff none */
    u8 unk075;           /* 075 */
    s16 unk76;           /* 076 */
    u16 call_stack[4];   /* 078: return PCs */
    u8 character;        /* 080 */
    u8 unk081;
    u8 unk82;            /* 082 */
    u8 unk83;            /* 083 */
    s32 unk84;           /* 084 */
    s16 unk88;           /* 088 */
    s16 unk8A;           /* 08A */
    ScriptSlot slots[8]; /* 08C */
    u16 pc;              /* 0CC: event working PC, relative to the bytecode */
    u8 slot;             /* 0CE: selected script slot */
    u8 unk0CF;
    s32 target[3];       /* 0D0: move target x, y, z */
    u8 unk0DC[0xE2 - 0xDC];
    u8 unkE2;            /* 0E2 */
    u8 unk0E3;
    s16 unkE4;           /* 0E4 */
    s16 unkE6;           /* 0E6 */
    s16 unkE8;           /* 0E8 */
    s16 unk0EA;          /* 0EA */
    s16 unkEC;           /* 0EC */
    s16 unkEE;           /* 0EE */
    s32 unkF0;           /* 0F0: 16.16 vertical (fall) or push speed */
    s16 scale[3];        /* 0F4 */
    u8 unk0FA[2];
    u8 color0[3];        /* 0FC */
    u8 color1[3];        /* 0FF */
    u16 unk102;          /* 102 */
    s16 heading;         /* 104 */
    s16 heading_goal;    /* 106: bit 15 once turned */
    s16 unk108;          /* 108 */
    u16 sound;           /* 10A */
    u8 sound_volume;     /* 10C */
    u8 sound_mode;       /* 10D: 0xff off */
    u8 unk10E[0x110 - 0x10E];
    struct PlatformLink *link; /* 110: linked platform state (allocated) */
    void *unk114;        /* 114 */
    s32 *list;           /* 118 */
    u16 unk11C;          /* 11C */
    s16 unk11E;          /* 11E */
    void *unk120;        /* 120 */
    s16 unk124;          /* 124: -1 when +120 is free */
    u8 unk126;           /* 126 */
    u8 unk127;           /* 127: sprite slot */
    u16 unk128;          /* 128 */
    u8 unk12A[2];
    union {
        u32 word;
        ActorState bits;
    } state;             /* 12C */
    u32 unk130 : 9;      /* 130: bits 0-8 */
    u32 unk130_9 : 10;   /* bits 9-18 */
    u32 unk130_19 : 9;   /* bits 19-27 */
    u32 sprite_kind : 2; /* bits 28-29 */
    u32 unk130_30 : 2;
    u32 unk134;          /* 134 */
} FieldActor;

/* A 14-byte collision triangle; +0c indexes the attribute table. */
typedef struct {
    s16 unk00[6];
    u8 attribute;
    u8 unk0D;
} CollisionTriangle;

/* A model instance; +12 is its drawing mode. */
typedef struct {
    u8 unk00[4];
    SpriteModel *mesh; /* 04 */
    void *packets[2]; /* 08: per draw buffer */
    u8 unk10[2];
    s16 mode;        /* 12 */
    MorphState *anims; /* 14: its morph channels (80080a18 feeds them) */
    s16 center[3];   /* 18 */
    s16 unk1E;
    s16 radius;      /* 20 */
} FieldInstance;

/* One 0x5C-byte descriptor; one per event actor. */
typedef struct FieldDescriptor {
    FieldInstance *instance; /* 00 */
    Sprite *model;           /* 04 */
    struct FieldMarker *shadow; /* 08: drop shadow quad */
    MATRIX matrix;           /* 0C: its translation is the position */
    MATRIX transform;        /* 2C */
    FieldActor *actor;       /* 4C */
    SVECTOR rotation;        /* 50 */
    u16 flags;               /* 58 */
    u16 unk5A;               /* 5A: bit 0 while it owns a sprite */
} FieldDescriptor;

/* A collision attribute word, also read by byte. */
typedef union {
    u32 word;
    u8 bytes[4];
} Attribute;

/* The loaded field components (80070cc8), one object: stores to structure
 * members do not pass loads of these pointers. */
typedef struct {
    s32 descriptor_count;                      /* 800afb0c */
    FieldDescriptor *descriptors;              /* 800afb10 */
    s32 *geometry;                             /* 800afb14: count, then offsets */
    s32 *collision;                            /* 800afb18 */
    s32 *sprites;                              /* 800afb1c: sprite resource (offset table) */
    Attribute *collision_attributes;           /* 800afb20 */
    CollisionTriangle *collision_triangles[4]; /* 800afb24 */
    void *collision_vertices[4];               /* 800afb34 */
    s32 triangle_counts[4];                    /* 800afb44 */
    s16 layer_count;                           /* 800afb54 */
} FieldComponents;

#endif
