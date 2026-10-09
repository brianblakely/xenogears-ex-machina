#ifndef MENU_NODE_H
#define MENU_NODE_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "display.h"

/* The 3D scene graph (menu7 800898BC-8008A2B8, 8008A3E0-8008AC0C,
 * 8008AF6C-8008B5FC, 8008B730-8008BA2C, 8008BCC8-8008C7C0): nodes with
 * typed payloads (models, model sets, lights, instances), their animation
 * players, the light rigs that view them, and the mesh packets of the
 * instanced models. */

/* Scene node (0x9C bytes): a typed payload with its own transform, linked
 * into a tree of children. */
typedef struct Node {
    s32 type;          /* 1 model, 2 model set, 3, 4, 5, 6 */
    void *data;        /* type-specific payload */
    void (*callback)(struct Node *node); /* 0x08: run before each update */
    MATRIX view;       /* 0x0C: local-to-screen */
    SVECTOR offset;    /* 0x2C: offset from the parent (0/1 nodes) */
    VECTOR position;   /* 0x34 */
    SVECTOR angles;    /* 0x44: rotation angles */
    MATRIX unk4C;      /* 0x4C: local-to-world rotation */
    MATRIX unk6C;      /* 0x6C: light matrix */
    struct Node *parent; /* 0x8C */
    struct Node *next;   /* 0x90: next sibling */
    struct Node *child;  /* 0x94: first child */
    s32 unk98;
} Node;

/* Model payload (type 1, 0x20 bytes): a loaded sprite model, its morph
 * state and its packets. */
typedef struct {
    u32 flags;
    s32 unk4;
    void *packets[2];  /* 0x08: per display buffer (one block) */
    MorphState *morph; /* 0x10 */
    SpriteModel *file; /* 0x14 */
    u8 r, g, b;        /* 0x18 */
    u8 unk1B;
    s32 unk1C;
} NodeModel;

/* Primitive record of a model's packet buffers (0x14 bytes). */
typedef struct {
    u32 tag;
    u32 colour;        /* 0x04 */
    u8 unk8[0xC];
} ModelPrim;

/* Packet buffers built for a model's mesh (a resident sprite model's
 * vertices and primitive groups). */
typedef struct {
    s16 vertices;      /* 0x00 */
    s16 count;         /* 0x02: primitives */
    void *vertexData;  /* 0x04 */
    u8 *work;          /* 0x08: 8 bytes per vertex */
    SpriteModel *mesh; /* 0x0C */
    ModelPrim *prims[2]; /* 0x10: per display buffer */
} ModelPrims;

/* Instance payload (type 5, 0x10 bytes): draws another node's model. */
typedef struct {
    s32 type;          /* the source node's type */
    Node *source;      /* 0x04 */
    s32 unk8;
    ModelPrims *prims; /* 0x0C: own packet buffers for model sources */
} Instance;

/* Light payload (0x14 bytes); the resident loads it into the GTE light
 * matrix (80030a30). */
typedef struct Light {
    s32 direction[3];
    s16 colour[3];     /* 0x0C: 0x1000 = full */
    s16 unk12;
} Light;

#define NODE_LIGHT(node) ((Light *)(node)->data)

/* Animation record: what it drives and its value or stream offset. */
typedef struct {
    u8 kind;           /* & 0x7F: 3-5 node angle x/y/z, 6-8 node 0x2C x/y/z */
    u8 node;           /* index into the model set's nodes */
    u16 value;         /* key value, or channel stream offset */
} AnimRecord;

typedef struct {
    s16 frames;
    s16 unk2;
    s16 keys;          /* 0x04 */
    s16 channels;      /* 0x06 */
    AnimRecord records[1]; /* 0x08: keys, then channels */
} AnimHeader;

/* Constant key of a player (8 bytes). */
typedef struct {
    s16 *target;
    s16 value;         /* 0x04 */
    s16 angular;       /* 0x06: eased the short way round */
} Key;

/* Animation channel of a player (0x14 bytes). */
typedef struct {
    u8 *start;
    u8 *current;       /* 0x04: position in the stream */
    s16 *target;       /* 0x08 */
    u16 hold;          /* 0x0C: frames left at the current delta */
    s16 value;         /* 0x0E */
    s16 delta;         /* 0x10 */
    s16 angular;       /* 0x12 */
} Channel;

/* Animation player. */
typedef struct {
    AnimHeader *header;
    Key *keys;         /* 0x04 */
    Channel *channels; /* 0x08 */
    s32 unkC;
    s16 unk10;
    s16 frame;         /* 0x12 */
} Player;

/* Hierarchy record of a model set file (0x10 bytes). */
typedef struct {
    s16 parent;        /* -1: the set's root */
    s16 model;         /* -1: none; else index of a 0x38-byte model */
    struct { s16 vx, vy, vz; } angle; /* 0x04: no vector padding */
    s16 offset[3];     /* 0x0A: initial 0x2C components */
} HierarchyRecord;

/* Model set payload (type 2, 0x1C bytes): a node per hierarchy record and
 * an animation player per animation. */
typedef struct {
    s16 nodeCount;
    s16 count;         /* 0x02: players */
    struct Node **nodes; /* 0x04 */
    Player *players;   /* 0x08 */
    s32 unkC;
    s16 scale[3];      /* 0x10: 4096 = 1.0 */
    s16 unk16;
    HierarchyRecord *records; /* 0x18 */
} ModelSet;

/* Model set file: hierarchy (count, records), models, animations
 * (count, then that many animation data pointers or null). */
typedef struct {
    u32 *hierarchy;
    u8 *models;
    u32 *animations;
} ModelSetFile;

/* Scene file loaded as one block; its pointers are relative to the
 * address it was built at (0x1C). */
typedef struct {
    u8 *unk0;
    u8 *unk4;
    u32 *table;        /* 0x08: count, then that many pointers */
    u8 *target;        /* 0x0C: texture/CLUT target */
    u8 *unk10;
    u8 *unk14;
    u8 *unk18;
    u8 *base;          /* 0x1C */
    u8 *unk20;
    u8 *unk24;
} SceneFile;

/* Three-light rig with an ambient colour (0x28C bytes): the 3D view's
 * camera node, its lights and its drawing layer. */
typedef struct {
    s32 unk0;
    Node *camera;      /* 0x04 */
    Node *lights[3];   /* 0x08 */
    Node storage[4];   /* 0x14 */
    OtPair *layer;     /* 0x284 */
    u8 r, g, b;        /* 0x288 */
    u8 unk28B;
} LightRig;

extern MATRIX D_80091C0C;  /* identity */
extern s32 D_80091C2C;     /* nonzero: model set players do not own their keys */
extern s32 D_8009289C;     /* nonzero: model sets compose with their parent's view */
extern VECTOR D_80096FA8;  /* the scene origin: the last eye position */
extern VECTOR D_80097000;  /* look-at work: third axis */
extern VECTOR D_8009A0C8;  /* look-at work: forward */
extern VECTOR D_8009A2C8;  /* mesh light direction */
extern MATRIX D_8009A2D8;
extern VECTOR D_8009A918;  /* look-at work: up */

void func_800898BC(MATRIX *m, SVECTOR *eye, SVECTOR *at, SVECTOR *up);
void func_80089A98(LightRig *view, VECTOR *target, VECTOR *eye);
Node *func_80089B44(Node *node);
Node *func_80089C54(void);
void func_80089C88(Node *parent, Node *child);
void func_80089D5C(Node *node);
void func_80089E2C(Node *node, NodeModel *model);
void func_80089E54(Node *node, ModelSet *set);
void func_80089E64(Node *node, void *data);
ModelSet *func_80089E74(void);
void func_80089EB4(ModelSet *set);
NodeModel *func_80089F8C(NodeModel *model);
NodeModel *func_80089FC4(void);
void func_80089FF8(NodeModel *model);
void func_8008A110(s16 x, s16 y);
void func_8008A128(s16 x, s16 y);
void func_8008A140(s16 tx, s16 ty, s16 cx, s16 cy);
void func_8008A168(void);
void func_8008A184(NodeModel *model, SpriteModel *file);
Light *func_8008A254(void);
LightRig *func_8008A3E0(OtPair *layer);
void func_8008A5BC(LightRig *rig);
void func_8008A62C(void);
void func_8008A63C(NodeModel *model);
void func_8008A78C(Node *node);
void func_8008A7E0(Node *node);
void func_8008ABAC(Node **lights);
SceneFile *func_8008AF6C(SceneFile *scene);
void func_8008B0D8(Player *player);
void func_8008B13C(AnimRecord *record, Player *player, Node *root);
Node *func_8008B38C(ModelSetFile *file);
s32 func_8008B730(Player *player, s32 frames, s32 steps);
void func_8008BCC8(SpriteModel *mesh, u8 *work);
void func_8008BD70(SpriteModel *mesh, ModelPrim *prims, u32 *ot, u8 *work);
void func_8008BE4C(ModelPrims *prims, SpriteModel *mesh);
void func_8008C120(Instance *instance);
Node *func_8008C188(Node *source, Node *parent);
Node *func_8008C298(Node *source);
Node *func_8008C2C0(Node *source);
void func_8008C2E8(Node *node);
/* Project toward D_8009A2C8 onto y=0; writes work.vx/vz, preserving vy/pad.
 * count must be positive. Reads the next vertex even on the last iteration. */
void func_8008C3A8(void *vertices, u8 *work, s32 count);
/* Mesh packet builders consume eight-byte u16 index records and preload one
 * beyond count. They advance D_80059424 past culled packet slots too, and
 * prepend accepted packets to D_80059568 without a depth sort. */
void func_8008C4B0(u8 *prims, s32 count); /* triangles */
void func_8008C620(u8 *prims, s32 count); /* quads */

#endif
