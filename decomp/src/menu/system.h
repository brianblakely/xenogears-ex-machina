#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

/* Included from menu.h after the generic libgpu/libgte types. */

/* Saved system options word (resident, 0x8006f980). */
typedef struct {
    u32 version : 4;   /* 1 once written */
    u32 option4 : 1;   /* mirrors D_80099D9B */
    u32 option5 : 1;   /* mirrors D_80099D9C */
    u32 option6 : 7;   /* mirrors D_80099D9F */
    u32 option13 : 3;  /* mirrors D_80099D98 */
    u32 complete : 1;  /* every tracked flag was set */
    u32 unused : 15;
} SystemOptions;

/* Resident save block at 0x8006f978: 64 progress flags, one bit each,
 * then the options word. */
typedef struct {
    u8 flags[8];
    SystemOptions options;
} SystemSave;

extern SystemSave D_8006F978;
extern u8 D_8005061C;    /* nonzero keeps the options in D_8006F980 */
extern u8 D_800927EC;
/* Current option settings (0x80099d98). The bytes at 0x02-0x06, 0x09,
 * 0x0A and 0x0C are also addressed through their own symbols
 * (D_80099D9A..D_80099DA4) by some functions. */
typedef struct {
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

/* libgpu DRAWENV layout. */
typedef struct {
    Rect clip;
    s16 ofs[2];
    Rect tw;
    u16 tpage;
    u8 dtd;
    u8 dfe;
    u8 isbg;
    u8 r0, g0, b0;
    u32 dr_env[16];
} DrawEnv;

/* libgpu DISPENV layout. */
typedef struct {
    Rect disp;
    Rect screen;
    u8 isinter;
    u8 isrgb24;
    u8 pad0;
    u8 pad1;
} DispEnv;

/* 16x16 sprite primitive (libgpu SPRT_16). */
typedef struct {
    u8 addr[3];
    u8 len;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
} Sprite16;

/* Filled rectangle primitive (libgpu TILE); colour and code as one word. */
typedef struct {
    u8 addr[3];
    u8 len;
    u32 colour;
    s16 x0, y0;
    s16 w, h;
} Tile;

/* One of the two display buffers (table at 0x8009a0d8, 0xF8 bytes each). */
typedef struct {
    DrawEnv draw;      /* 0x00 */
    DispEnv disp;      /* 0x5C */
    u32 ot;            /* 0x70: one-entry ordering table */
    Sprite16 sprite;   /* 0x74 */
    u8 unk84[0x4C];
    u32 modeD0[3];     /* 0xD0 */
    u32 modeDC[3];     /* 0xDC */
    Tile background;   /* 0xE8 */
} Window;

extern Window D_8009A0D8[];

/* Scene node (0x9C bytes): a typed payload with its own transform, linked
 * into a tree of children. */
typedef struct Node {
    s32 type;          /* 1 model, 2 model set, 3, 4, 5, 6 */
    void *data;        /* type-specific payload */
    void (*callback)(struct Node *node); /* 0x08: run before each update */
    Matrix view;       /* 0x0C: local-to-screen */
    SVector rotation;  /* 0x2C: offset from the parent (0/1 nodes) */
    Vector position;   /* 0x34 */
    SVector unk44;     /* 0x44: rotation angles */
    Matrix unk4C;      /* 0x4C: local-to-world rotation */
    Matrix unk6C;      /* 0x6C: light matrix */
    struct Node *parent; /* 0x8C */
    struct Node *next;   /* 0x90: next sibling */
    struct Node *child;  /* 0x94: first child */
    s32 unk98;
} Node;


/* Loaded model file header. */
typedef struct {
    u8 unk0[0x34];
    s32 unk34;
} ModelFile;

/* Primitive record of a model's packet buffers (0x14 bytes). */
typedef struct {
    u32 tag;
    u32 colour;        /* 0x04 */
    u8 unk8[0xC];
} ModelPrim;

/* Mesh of a model: vertices and primitive groups. */
typedef struct {
    s16 unk0;
    u16 count;         /* 0x02: vertices */
    u16 prims;         /* 0x04 */
    u16 groups;        /* 0x06 */
    void *data;        /* 0x08: vertices */
    s32 unkC;
    u8 *groupData;     /* 0x10: per group a flag byte, a count, count x 8 bytes */
} Mesh;

/* Packet buffers built for a model's mesh. */
typedef struct {
    s16 vertices;      /* 0x00 */
    s16 count;         /* 0x02: primitives */
    void *vertexData;  /* 0x04 */
    u8 *work;          /* 0x08: 8 bytes per vertex */
    Mesh *mesh;        /* 0x0C */
    ModelPrim *prims[2]; /* 0x10: per display buffer */
} ModelPrims;

/* Model payload (0x20 bytes). */
typedef struct {
    u32 flags;
    s32 unk4;
    void *packets[2];  /* 0x08: per display buffer (one block) */
    s32 unk10;
    ModelFile *file;   /* 0x14 */
    u8 r, g, b;        /* 0x18 */
    u8 unk1B;
    s32 unk1C;
} Model;

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
    u32 area[2][3];    /* 0x18: DR_AREA per buffer */
    u32 offset[2][3];  /* 0x30: DR_OFFSET per buffer */
    Tile tile[2];      /* 0x48 */
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
} Task;

/* The caller state pair saved/restored around a nested task scheduler. */
typedef struct {
    u32 *caller_stack;
    Task *task;
} TaskCallerContext;

void func_8008BB00(TaskCallerContext *context);
void func_8008BB1C(TaskCallerContext *context);

extern Vector D_8009A2C8;     /* mesh light direction */
extern s32 D_80059424;
extern s32 D_80059568;
extern s32 D_8005953C;
extern u8 *D_80059528;        /* primitive group being drawn */

/* libgpu CVECTOR layout. */
typedef struct {
    u8 r, g, b, cd;
} CVector;

/* Word count of a primitive, from its tag (libgpu P_TAG len). */
#define TAG_LEN(tag) (((u8 *)(tag))[3])

extern OtPair *D_80091C30;    /* table to compact at the end of the frame */
extern s32 D_80092820;        /* root counter at the frame start */
extern u32 *D_800928E4;       /* ordering table primitives are added to */
extern s32 D_80050100;        /* its depth shift */
extern s32 D_80092810;
extern CVector D_80092818[2]; /* current colour per display buffer */
extern s32 D_80092914;        /* colour changed this frame */

extern s32 D_80091C2C;   /* nonzero: model set players do not own their keys */
extern s32 D_80092824;   /* nodes instanced by the last copy */
extern Node *D_80092828; /* root being instanced */
extern s16 D_80092800;   /* model texture page x (-1: none) */
extern s16 D_80092804;   /* model texture page y */
extern s16 D_80092808;   /* model CLUT x (-1: none) */
extern s16 D_8009280C;   /* model CLUT y */

extern Matrix D_80091C0C; /* identity */
extern Vector D_8009A0C8; /* look-at work: forward */
extern Vector D_8009A918; /* look-at work: up */
extern Vector D_80096F98; /* look-at work: side */
extern Vector D_80097000; /* look-at work: third axis */
extern Vector D_80096FA8; /* last eye position */
extern Matrix D_8009A2D8;
extern Matrix D_80096FE0; /* screen scale */
extern s16 D_8009285C;    /* display width */
extern s16 D_8009286C;    /* display height */
extern u16 D_80059570;   /* pad buttons held this frame */
extern s32 D_800927F4;

extern s32 D_80010000;   /* boot word: -1, 0 or other start state */
extern u8 D_80091BB0[];
extern s32 D_800928CC;
extern Window *D_80092868;
extern Window *D_80092870;
extern s16 D_80092898;
extern s32 D_8009289C;
extern s32 D_800928E8;
extern u8 D_800928A0;
extern u8 D_80092920;
extern u16 D_800928D0;   /* debug display switches */
extern u32 *D_80092938; /* ordering table of the buffer being built */
extern void (*D_80092930)(void *block);
extern s32 D_800927F0;
extern s32 D_80050618;
extern volatile s32 D_80059488; /* vertical blanks counted */
extern s32 D_80059578;   /* primitives drawn this frame */
extern s32 D_800595C0;   /* primitive count this frame */

void func_800852C4(s32 arg); /* the menu task */
extern void (*D_80088BFC[])(s32); /* mode tasks, by D_80050618 */
void func_80088C00(void);
void DrawSyncCallback(void *callback);
void InitGeom(void);
void func_80032498(s32 kind, void *data);
void func_80028470(s32 a, s32 b);
void func_800374E8(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g, s32 h, s32 i, s32 j, s32 k);
void func_80088CBC(s32 index);
void func_8008A110(s16 x, s16 y);
void func_8008A128(s16 x, s16 y);
void func_8008E620(void);
s32 func_80028738(s32 file);
void *func_80031BDC(s32 size, s32 mode);
void func_800295D8(s32 file, void *buffer, s32 a, s32 b);
void SetDefDispEnv(DispEnv *env, s32 x, s32 y, s32 w, s32 h);
void SetDefDrawEnv(DrawEnv *env, s32 x, s32 y, s32 w, s32 h);
void SetDrawEnv(u32 *packet, DrawEnv *env);
void SetDrawArea(u32 *packet, Rect *area);
void SetDrawOffset(u32 *packet, s16 *offset);
void VectorNormal(Vector *in, Vector *out);
void func_8004A480(Vector *a, Vector *b, Vector *out);
void ApplyMatrix(Matrix *m, SVector *in, Vector *out);
void MulMatrix2(Matrix *a, Matrix *b);
void func_800898BC(Matrix *m, SVector *eye, SVector *at, SVector *up);
void func_800324B8(s32 kind);
void func_800320E8(void *p);
void func_80032C18(void *p, s32 mode);
void func_8002CBBC(ModelFile *file);
s32 func_800303C8(ModelFile *file, s32 mode);
void func_8002CB54(ModelFile *file, void **first, void **second);
void func_8002CC54(u16 tpage);
void func_8002CC74(s32 x, s32 y);
void func_8002C8CC(ModelFile *file, void *resource, s32 mode);
void func_8002DDE4(void *target, s32 on, s32 a, s32 b, s32 c, s32 d, s32 e);
Node *func_80089B44(Node *node);
void func_80089D5C(Node *node);
void func_80089EB4(ModelSet *set);
void func_80089FF8(Model *model);
void func_8008C120(Instance *instance);
void func_8002C3E8(u8 *models);
ModelSet *func_80089E74(void);
Node *func_80089C54(void);
Model *func_80089FC4(void);
void func_80089E2C(Node *node, Model *model);
void func_80089E54(Node *node, ModelSet *set);
void func_80089C88(Node *parent, Node *child);
void func_8008A184(Model *model, ModelFile *file);
void func_8008B13C(u8 *data, Player *player, Node *root);
void func_8008B0D8(Player *player);
void func_8008BE4C(ModelPrims *prims, Mesh *mesh);
void func_8008BD70(Mesh *mesh, ModelPrim *prims, u32 *ot, u8 *work);
Node *func_8008C188(Node *source, Node *parent);
Node *func_8008C298(Node *source);
u32 func_800405E4(void);
void func_8008BB3C(Task *task);
/* Project toward D_8009A2C8 onto y=0; writes work.vx/vz, preserving vy/pad.
 * count must be positive. Reads the next vertex even on the last iteration. */
void func_8008C3A8(void *vertices, u8 *work, s32 count);
/* Mesh packet builders consume eight-byte u16 index records and preload one
 * beyond count. They advance D_80059424 past culled packet slots too, and
 * prepend accepted packets to D_80059568 without a depth sort. */
void func_8008C620(u8 *prims, s32 count);
void func_8002C700(ModelFile *file, void *packets, u32 *ot, s32 mode);
void func_80030B14(Matrix *m);
Matrix *func_8003F738(SVector *angles, Matrix *m);
void MulMatrix0(Matrix *a, Matrix *b, Matrix *out);
void TransMatrix(Matrix *m, Vector *t);
/* Scale each rotation column through the GTE, preserving m's translation
 * and leaving the original rotation loaded in the GTE. */
void func_800731F8(Matrix *m, s16 *scale);
void func_8008A63C(Model *model);
void func_8008A78C(Node *node);
void func_8008BCC8(Mesh *mesh, u8 *work);

extern s32 D_80050104;

/* libgte inline_c.h register macros. */
#define gte_SetRotMatrix(r0)                                                   \
    __asm__ volatile("lw $12, 0(%0);"                                          \
                     "lw $13, 4(%0);"                                          \
                     "ctc2 $12, $0;"                                           \
                     "ctc2 $13, $1;"                                           \
                     "lw $12, 8(%0);"                                          \
                     "lw $13, 12(%0);"                                         \
                     "lw $14, 16(%0);"                                         \
                     "ctc2 $12, $2;"                                           \
                     "ctc2 $13, $3;"                                           \
                     "ctc2 $14, $4"                                            \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14")
#define gte_SetTransMatrix(r0)                                                 \
    __asm__ volatile("lw $12, 20(%0);"                                         \
                     "lw $13, 24(%0);"                                         \
                     "ctc2 $12, $5;"                                           \
                     "lw $14, 28(%0);"                                         \
                     "ctc2 $13, $6;"                                           \
                     "ctc2 $14, $7"                                            \
                     :                                                         \
                     : "r"(r0)                                                 \
                     : "$12", "$13", "$14")
#define gte_SetBackColor(r0, r1, r2)                                           \
    __asm__ volatile("sll $12, %0, 4;"                                         \
                     "sll $13, %1, 4;"                                         \
                     "sll $14, %2, 4;"                                         \
                     "ctc2 $12, $13;"                                          \
                     "ctc2 $13, $14;"                                          \
                     "ctc2 $14, $15"                                           \
                     :                                                         \
                     : "r"(r0), "r"(r1), "r"(r2)                               \
                     : "$12", "$13", "$14")
void func_8008C4B0(u8 *prims, s32 count); /* same packet interface as C620 */
s32 func_8008B730(Player *player, s32 frames, s32 steps);
void ClearOTagR(u32 *ot, s32 length);
void AddPrims(u32 *ot, u32 *last, u32 *first);
void func_8008AC7C(OtPair *pair);
s32 func_80028A60(s32 a);
SceneFile *func_8008AF6C(SceneFile *scene);
s32 GetRCnt(s32 counter);
Light *func_8008A254(void);
void func_80089E64(Node *node, void *data);
void func_8008A3A8(OtPair *layer);
void func_8008ABAC(Node **lights);
void func_80030A30(s32 index, Light *light);
Model *func_80089F8C(Model *model);
void SetGeomOffset(s32 x, s32 y);
void func_8002DFF0(s32 w, s32 h);
void func_80089210(s32 width, s32 height);
void func_80089330(s32 width, s32 height);
void func_80089534(s32 width, s32 height);

void func_80019CA0(void);
void TermPrim(void *block);
void func_80037324(void *block);
void func_8008EADC(void);
void func_80032CB8(void);
s32 VSync(s32 mode);
void func_80088C28(void);
void func_8003700C(char *format, ...);
void func_80036DC8(s32 r, s32 g, s32 b);
void func_8008ACB8(s32 a);
void func_8008AC8C(void);
void DrawSync(s32 a);
void PutDispEnv(DispEnv *env);
void DrawOTagEnv(void *block, Window *buffer);
s32 func_800888E4(s32 flag);
void func_800888B0(s32 flag);
s32 func_800889C8(void);
void func_8003278C(s32 a, s32 value, s32 c, s32 d);
void func_8008895C(void);
void func_80088A40(void);

#endif
