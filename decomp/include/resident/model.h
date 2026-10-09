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

/* One model of a group (0x38 bytes from group + 0x14). The tables are
 * stored as offsets from the group and relocated to addresses once. The
 * relocation loops address every field from the model pointer at nonzero
 * offsets, so the record starts with a word ahead of the tables. */
typedef struct {
    u8 unk0[4];
    u8 *table0;
    u8 *table4;
    u8 *table8;
    u8 *primitives;
    u8 unk14[4];
    ModelList *list; /* optional */
    u8 unk1C[0x1C];
} Model;

/* A loaded model group: its heap block ends at the first model's primitives
 * once trimmed. */
typedef struct {
    s32 count;
    s32 flags;       /* bit 0: relocated; bit 1: trimmed */
    u8 unk8[0xC];
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
    u16 primitive_count;
    u16 group_count;    /* primitive groups */
    SVECTOR *vertices;
    SVECTOR *normals;
    u8 *unk10;          /* primitive groups */
    u8 *unk14;
    u8 *unk18;
    MorphTable *morphs; /* optional */
    SVECTOR box_min;    /* bounding box */
    SVECTOR box_max;
    s32 aux_size;       /* bytes of the auxiliary block (unk18) */
    s32 packet_size;    /* bytes of one packet buffer */
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

/* A quad of a model: its four vertex indices, the first two read as one
 * word. */
typedef struct {
    u32 v01;
    u16 v2;
    u16 v3;
} QuadFace;

/* A model light: its direction vector and color. */
typedef struct {
    s32 vx, vy, vz;
    u16 r, g, b;
} ModelLight;

/* Renderer output packet header. */
typedef struct RenderPacket {
    u8 unk0[3];
    u8 code;
    s32 value;
} RenderPacket;

extern RenderPacket *D_80059424; /* the primitive being built */
extern s32 *D_80059498;          /* lit-color cache: color word, then the face normal */
extern SVECTOR *D_8005952C;      /* vertex normals of the model being drawn */
extern SVECTOR *D_8005953C;      /* vertices of the model being drawn */
extern u32 *D_80059568;          /* the ordering table models are drawn into */
extern s32 D_80059578;           /* primitives drawn */
extern s32 D_80050100;           /* depth shift into the ordering table */

void func_8002DB84(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *normal); /* face normal */

s32 func_8002DDE4(s32 *images, s16 mode, s32 x, s32 y, s16 mode2, u16 x2, u16 y2); /* upload an image list */
u8 *func_8002DFE0(void); /* the shared unpack buffer */

/* A primitive group: its type (an index into D_8004FE50) and count; the
 * primitive records follow. */
typedef struct PrimitiveGroup {
    u8 type;
    u8 unk1;
    s16 count;
} PrimitiveGroup;

/* The renderer of a primitive type: a routine per draw mode, the record
 * stride and the packet sizes. The handwritten draw routines (see
 * model_draw.s) sort by average depth (mode 0), light the faces (1), sort
 * by the farthest (2) or nearest (3) vertex, or depth-cue textured faces
 * by average (4) or farthest depth (5); a type without a variant lists
 * its mode 0 routine. */
typedef struct {
    void (*draw[6])(); /* (u8 *records, s32 count): the handwritten renderers */
    s32 (*prepare)();  /* (aux, record, kind): one record's packets */
    s32 stride;         /* record */
    s32 aux_stride;     /* auxiliary data per record */
    s32 packet_size;
} PrimitiveType;

extern PrimitiveType D_8004FE50[];
extern PrimitiveGroup *D_80059528; /* the primitive group being drawn */
extern s32 D_800595C0;             /* primitives submitted */
extern s32 D_80050104;             /* bounding box test mode (8003101C), 0 off */
extern u8 *D_80059538;             /* auxiliary data of the record being prepared */

s32 func_8002C700(SpriteModel *model, RenderPacket *packets, u32 *ot, s32 mode); /* draw */
void func_8002C8CC(SpriteModel *model, RenderPacket *packets, s32 mode); /* build packets */
void func_8002CCAC(void);
s32 func_8002C3E8(ModelGroup *group);
void func_8002C59C(SpriteModel *model);
void func_8002CB54(ModelBuffer *buffer, u8 **first, u8 **second);
/* Old-style definition: callers pass the mode as an int. */
s32 func_8003101C(); /* (SpriteModel *model, u16 mode): bounding box off screen */

#endif
