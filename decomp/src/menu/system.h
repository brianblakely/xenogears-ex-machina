#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

/* Included from menu.h after the generic libgpu/libgte types. */

/* The resident's option bytes of the field and the menu (its u8[6] at
 * 8005061c) and the byte after them, each by its own name: indexed from the
 * array, func_800852C4 keeps the array's address in a register where the
 * original loads each byte absolutely. */
extern u8 D_8005061C; /* nonzero keeps the options in D_8006D634.options */
extern u8 D_8005061D; /* entry kind (0 bout, 1 bout mode 4, 2 scene) */
extern u8 D_8005061E; /* first actor's model id */
extern u8 D_8005061F; /* second actor's model id */
extern u8 D_80050620; /* option 6 */
extern u8 D_80050621; /* level */
extern u8 D_80050622; /* result of the last menu battle */
/* Current option settings (0x80099d98). */
typedef struct Settings {
    u8 level;     /* 0x00: saved as option13 */
    u8 unk1;
    u8 rate;      /* 0x02: frame rate choice */
    u8 option4;   /* 0x03: port 1 vibration */
    u8 option5;   /* 0x04: port 2 vibration */
    u8 com1;      /* 0x05: side 1 played by the computer */
    u8 driven;    /* 0x06: side 2 played by the computer */
    u8 option6;   /* 0x07 */
    u8 unk8;
    u8 speed;     /* 0x09 */
    u8 command;   /* 0x0A: the opponent's current command */
    u8 unkB;
    s16 unkC;
} Settings;

extern Settings D_80099D98;

/* One of the two display buffers (table at 0x8009a0d8, 0xF8 bytes each). */
typedef struct DisplayBuffer {
    DRAWENV draw;      /* 0x00 */
    DISPENV disp;      /* 0x5C */
    u32 ot;            /* 0x70: one-entry ordering table */
    SPRT_16 sprite;    /* 0x74 */
    u8 unk84[0x4C];
    DR_AREA area;      /* 0xD0 */
    DR_OFFSET offset;  /* 0xDC */
    TILE background;   /* 0xE8: also the screen fade */
} DisplayBuffer;

extern DisplayBuffer D_8009A0D8[2];

/* Scene node (0x9C bytes): a typed payload with its own transform, linked
 * into a tree of children. */
typedef struct Node {
    s32 type;          /* 1 model, 2 model set, 3, 4, 5, 6 */
    void *data;        /* type-specific payload */
    void (*callback)(struct Node *node); /* 0x08: run before each update */
    MATRIX view;       /* 0x0C: local-to-screen */
    SVECTOR rotation;  /* 0x2C: offset from the parent (0/1 nodes) */
    VECTOR position;   /* 0x34 */
    SVECTOR unk44;     /* 0x44: rotation angles */
    MATRIX unk4C;      /* 0x4C: local-to-world rotation */
    MATRIX unk6C;      /* 0x6C: light matrix */
    struct Node *parent; /* 0x8C */
    struct Node *next;   /* 0x90: next sibling */
    struct Node *child;  /* 0x94: first child */
    s32 unk98;
} Node;

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

/* Instance payload (type 5, 0x10 bytes): draws another node's model. */
typedef struct {
    s32 type;          /* the source node's type */
    Node *source;      /* 0x04 */
    s32 unk8;
    ModelPrims *prims; /* 0x0C: own packet buffers for model sources */
} Instance;

/* Light payload (0x14 bytes). */
typedef struct {
    s32 direction[3];
    s16 colour[3];     /* 0x0C: 0x1000 = full */
    s16 unk12;
} Light;

#define NODE_LIGHT(node) ((Light *)(node)->data)

/* Drawing layer (0x68 bytes): an ordering table per display buffer with
 * its drawing area, offset and background packets. */
typedef struct {
    s32 unk0;
    u32 *ot[2];        /* 0x04: per display buffer */
    u32 *last[2];      /* 0x0C: last entry of each */
    s16 length;        /* 0x14 */
    u8 flags;          /* 0x16: 4 own area, 8 own offset, 0x10 background */
    u8 shift;          /* 0x17: 14 - log2(length) */
    DR_AREA area[2];   /* 0x18: per buffer */
    DR_OFFSET offset[2]; /* 0x30: per buffer */
    TileWords tile[2]; /* 0x48 */
} OtPair;

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

/* NodeModel set payload (type 2, 0x1C bytes): a node per hierarchy record and
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

/* NodeModel set file: hierarchy (count, records), models, animations
 * (count, then that many animation data pointers or null). */
typedef struct {
    u32 *hierarchy;
    u8 *models;
    u32 *animations;
} ModelSetFile;

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

/* Cooperative task: registers saved by number, then its stack. */
typedef struct {
    u32 regs[32];      /* 2 v0 .. 31 ra; 28 gp, 29 sp, 30 fp */
    u32 *stack;        /* 0x80 */
} TaskContext;

/* The caller state pair saved/restored around a nested task scheduler. */
typedef struct {
    u32 *caller_stack;
    TaskContext *task;
} TaskCallerContext;

void func_8008BB00(TaskCallerContext *context);
void func_8008BB1C(TaskCallerContext *context);

extern VECTOR D_8009A2C8;     /* mesh light direction */

/* Word count of a primitive, from its tag (libgpu P_TAG len). */
#define TAG_LEN(tag) (((u8 *)(tag))[3])

extern OtPair *D_80091C30;    /* table to compact at the end of the frame */
extern u32 *D_800928E4;       /* ordering table primitives are added to */
extern s32 D_80092914;        /* colour changed this frame */

extern s32 D_80091C2C;   /* nonzero: model set players do not own their keys */

extern MATRIX D_80091C0C; /* identity */
extern VECTOR D_8009A0C8; /* look-at work: forward */
extern VECTOR D_8009A918; /* look-at work: up */
extern VECTOR D_80097000; /* look-at work: third axis */
extern VECTOR D_80096FA8; /* last eye position */
extern MATRIX D_8009A2D8;
extern MATRIX D_80096FE0; /* screen scale */
extern s16 D_8009285C;    /* display width */
extern s16 D_8009286C;    /* display height */

extern char *D_80091BB0[]; /* names of the menu's heap block kinds */
extern s32 D_800928CC;
extern DisplayBuffer *D_80092868;
extern DisplayBuffer *D_80092870;
extern s16 D_80092898;
extern s32 D_8009289C;
extern s32 D_800928E8;
extern u8 D_800928A0;
extern u8 D_80092920;
extern u16 D_800928D0;   /* debug display switches */
extern u32 *D_80092938; /* ordering table of the buffer being built */
extern void (*D_80092930)(void *block);
extern volatile s32 D_80059488; /* vertical blanks counted */

void func_800852C4(s32 arg); /* the menu task */
extern void (*D_80088BFC[])(s32); /* mode tasks, by D_80050618 */
void func_80088C00(void);
void func_80088CBC(s32 index);
void func_8008A110(s16 x, s16 y);
void func_8008A128(s16 x, s16 y);
void func_8008E620(void);
void func_800898BC(MATRIX *m, SVECTOR *eye, SVECTOR *at, SVECTOR *up);
void func_800324B8(s32 kind);
void func_8002CB54(SpriteModel *model, void **first, void **second);
void func_8002CC74(s32 x, s32 y);
void func_8002DDE4(void *target, s32 on, s32 a, s32 b, s32 c, s32 d, s32 e);
Node *func_80089B44(Node *node);
void func_80089D5C(Node *node);
void func_80089EB4(ModelSet *set);
void func_80089FF8(NodeModel *model);
void func_8008C120(Instance *instance);
ModelSet *func_80089E74(void);
Node *func_80089C54(void);
NodeModel *func_80089FC4(void);
void func_80089E2C(Node *node, NodeModel *model);
void func_80089E54(Node *node, ModelSet *set);
void func_80089C88(Node *parent, Node *child);
void func_8008A184(NodeModel *model, SpriteModel *file);
void func_8008B13C(AnimRecord *record, Player *player, Node *root);
void func_8008B0D8(Player *player);
void func_8008BE4C(ModelPrims *prims, SpriteModel *mesh);
void func_8008BD70(SpriteModel *mesh, ModelPrim *prims, u32 *ot, u8 *work);
Node *func_8008C188(Node *source, Node *parent);
Node *func_8008C298(Node *source);
TaskContext *func_8008BA2C(void (*entry)(s32), s32 arg, u32 *stack, s32 words);
void func_8008BB3C(TaskContext *task);
/* Project toward D_8009A2C8 onto y=0; writes work.vx/vz, preserving vy/pad.
 * count must be positive. Reads the next vertex even on the last iteration. */
void func_8008C3A8(void *vertices, u8 *work, s32 count);
/* Mesh packet builders consume eight-byte u16 index records and preload one
 * beyond count. They advance D_80059424 past culled packet slots too, and
 * prepend accepted packets to D_80059568 without a depth sort. */
void func_8008C620(u8 *prims, s32 count);
/* Scale each rotation column through the GTE, preserving m's translation
 * and leaving the original rotation loaded in the GTE. */
void func_800731F8(MATRIX *m, s16 *scale);
void func_8008A63C(NodeModel *model);
void func_8008A78C(Node *node);
void func_8008BCC8(SpriteModel *mesh, u8 *work);


void func_8008C4B0(u8 *prims, s32 count); /* same packet interface as C620 */
s32 func_8008B730(Player *player, s32 frames, s32 steps);
void func_8008AC7C(OtPair *pair);
SceneFile *func_8008AF6C(SceneFile *scene);
Light *func_8008A254(void);
void func_80089E64(Node *node, void *data);
void func_8008A3A8(OtPair *layer);
void func_8008ABAC(Node **lights);
void func_80030A30(s32 index, Light *light);
NodeModel *func_80089F8C(NodeModel *model);
void func_80089210(s32 width, s32 height);
void func_80089330(s32 width, s32 height);
void func_80089534(s32 width, s32 height);

void func_8008EADC(void);
void func_80088C28(void);
void func_8008ACB8(s32 a);
void func_8008AC8C(void);
s32 func_800888E4(s32 flag);
void func_800888B0(s32 flag);
s32 func_800889C8(void);
void func_8008895C(void);
void func_80088A40(void);

#endif
