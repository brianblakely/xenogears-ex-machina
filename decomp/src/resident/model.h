#ifndef RESIDENT_MODEL_H
#define RESIDENT_MODEL_H

#include "common.h"
#include "heap.h"
#include "psyq/libgte.h"

/* Resident model renderer. Field names follow their observed use; unknown
 * bytes keep their offsets. */

/* A model's list of paired data offsets: entries 0..last (none when last
 * is -1). */
typedef struct {
    s32 unk0;
    u8 *first;
    u8 *second;
} ModelListEntry;

typedef struct {
    s32 last;
    ModelListEntry entries[1];
} ModelList;

/* One model of a group. The tables are stored as offsets from the group
 * and relocated to addresses once. */
typedef struct {
    u8 *table0;
    u8 *table4;
    u8 *table8;
    u8 *primitives;
    u8 unk10[4];
    ModelList *list; /* optional */
    u8 unk18[0x20];
} Model;

/* A loaded model group: its heap block ends at the first model's primitives
 * once trimmed. */
typedef struct {
    s32 count;
    s32 flags;       /* bit 0: relocated; bit 1: trimmed */
    u8 unk8[0x10];
    Model models[1];
} ModelGroup;

/* A model's primitive buffer. */
typedef struct {
    u16 flags;       /* bit 0: owns `buffer`; bit 6: trimmed */
    u8 unk2[0x12];
    u8 *end;
    u8 *buffer;
    u8 unk1C[0x18];
    s32 size;
} ModelBuffer;

/* Vertex morphing. A morph delta moves vertex `index` by the delta times
 * the channel weight (4.12 fixed point). */
typedef struct {
    s16 dx, dy, dz;
    s16 index;
} MorphDelta;

/* A morph target: `count` vertex and normal deltas. The entry after the
 * last target lists the `count` vertices the morphs touch (s16 indices). */
typedef struct {
    s32 count;
    void *deltas;
    MorphDelta *normals;
} MorphTarget;

typedef struct {
    s32 count;
    MorphTarget targets[1];
} MorphTable;

/* A sprite model (relocated by 8002C59C). */
typedef struct {
    u16 flags;          /* bit 4: has normals; bit 5: relocated */
    u16 vertex_count;
    u8 unk4[4];
    SVECTOR *vertices;
    SVECTOR *normals;
    u8 unk10[0xC];
    MorphTable *morphs; /* optional */
} SpriteModel;

/* A morph channel: its update function steps `weight` toward `target`. */
typedef struct MorphChannel {
    s32 (*update)(struct MorphChannel *channel);
    s32 target;
    s32 weight;
    s32 step;
    u8 unk10[0x10];
} MorphChannel;

/* The morph state of a sprite model: its own vertex and normal arrays
 * (the model draws from morphed copies) and one channel per target. */
typedef struct {
    SpriteModel *model;
    SVECTOR *vertices;
    SVECTOR *normals;
    s32 count;
    MorphChannel *channels;
} MorphState;

/* A model light: its direction vector and color. */
typedef struct {
    s32 vx, vy, vz;
    u16 r, g, b;
} ModelLight;

extern MATRIX D_80059F64; /* light directions, one per row */
extern MATRIX D_80059F84; /* light colors, one per column */

/* Renderer output packet header. */
typedef struct {
    u8 unk0[3];
    u8 code;
    s32 value;
} RenderPacket;

extern RenderPacket *D_80059424; /* the primitive being built */
extern s32 *D_80059498;          /* lit-color cache: color word, then the face normal */
extern SVECTOR *D_8005952C;      /* vertex normals of the model being drawn */
extern SVECTOR *D_8005953C;      /* vertices of the model being drawn */

void func_8002DB84(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *normal); /* face normal */

s32 func_8002DDE4(s32 *images, s16 mode, s32 x, s32 y, s16 mode2, u16 x2, u16 y2); /* upload an image list */
u8 *func_8002DFE0(void); /* the shared unpack buffer */

#endif
