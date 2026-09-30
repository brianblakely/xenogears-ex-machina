#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include "common.h"

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
/* Current option settings (0x80099d98). */
typedef struct {
    u8 option13;  /* 0x00 */
    u8 unk1;
    u8 unk2;
    u8 option4;   /* 0x03 */
    u8 option5;   /* 0x04 */
    u8 unk5;
    u8 unk6;
    u8 option6;   /* 0x07 */
} Settings;

extern Settings D_80099D98;

typedef struct {
    s16 x, y, w, h;
} Rect;

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

/* libgte MATRIX layout. */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} Matrix;

/* libgte SVECTOR layout. */
typedef struct {
    s16 vx, vy, vz, pad;
} SVector;

/* Drawing layer: area/offset packets for both buffers and a background
 * tile per buffer. */
typedef struct {
    u8 unk0[0x16];
    u8 flags;          /* 0x16 */
    u8 unk17;
    u32 area[2][3];    /* 0x18: DR_AREA per buffer */
    u32 offset[2][3];  /* 0x30: DR_OFFSET per buffer */
    Tile tile[2];      /* 0x48 */
} Layer;

/* Scene node (0x9C bytes): a typed payload with its own transform, linked
 * into a tree of children. */
typedef struct Node {
    s32 type;          /* 1 model, 2 model set, 3, 4, 5, 6 */
    void *data;        /* type-specific payload */
    s32 unk8;
    Matrix view;       /* 0x0C */
    SVector rotation;  /* 0x2C */
    Vector position;   /* 0x34 */
    SVector unk44;
    Matrix unk4C;
    Matrix unk6C;
    struct Node *parent; /* 0x8C */
    struct Node *next;   /* 0x90: next sibling */
    struct Node *child;  /* 0x94: first child */
    s32 unk98;
} Node;

typedef struct {
    s32 unk0;
    Node *node;
} NodeOwner;

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

/* Packet buffers built for a model. */
typedef struct {
    s16 unk0;
    s16 count;         /* 0x02 */
    u8 unk4[0xC];
    ModelPrim *prims[2]; /* 0x10: per display buffer */
} ModelPrims;

/* Model payload (0x20 bytes). */
typedef struct {
    u32 flags;
    s32 unk4;
    void *resource;    /* 0x08 */
    ModelPrims *prims; /* 0x0C */
    s32 unk10;
    ModelFile *file;   /* 0x14 */
    u8 r, g, b;        /* 0x18 */
    u8 unk1B;
    s32 unk1C;
} Model;

/* Record of a model set (0x14 bytes). */
typedef struct {
    s32 loaded;
    void *data;
    u8 unk8[0xC];
} ModelEntry;

/* Model set payload. */
typedef struct {
    s16 unk0;
    s16 count;         /* 0x02 */
    void *unk4;
    ModelEntry *entries; /* 0x08 */
} ModelSet;

/* Scale payload (0x1C bytes). */
typedef struct {
    s32 unk0;
    s32 unk4;
    s32 unk8;
    s32 unkC;
    s16 scale[3];      /* 0x10: 4096 = 1.0 */
    s16 unk16;
    s32 unk18;
} Scale;

/* Light payload (0x14 bytes). */
typedef struct {
    s32 colour[3];
    s16 direction[3];  /* 0x0C */
    s16 unk12;
} Light;

/* Ordering table pair (0x68 bytes). */
typedef struct {
    s32 unk0;
    u32 *ot[2];        /* 0x04: per display buffer */
    u32 *last[2];      /* 0x0C: last entry of each */
    u16 length;        /* 0x14 */
    u8 unk16;
    u8 shift;          /* 0x17: 14 - log2(length) */
    u8 unk18[0x50];
} OtPair;

/* Model resource holder freed with its resource. */
typedef struct {
    s32 unk0;
    void *resource;
} Holder;

/* Three-light rig with an ambient colour (0x28C bytes). */
typedef struct {
    s32 unk0;
    Node *nodes[4];    /* 0x04: root, then the three lights */
    Node storage[4];   /* 0x14 */
    Holder *holder;    /* 0x284 */
    u8 r, g, b;        /* 0x288 */
    u8 unk28B;
} LightRig;

/* libgpu CVECTOR layout. */
typedef struct {
    u8 r, g, b, cd;
} CVector;

extern s32 D_80092810;
extern CVector D_80092818[2]; /* current colour per display buffer */
extern s32 D_80092914;        /* colour changed this frame */

extern s32 D_80091C2C;   /* nonzero: model set entries are not owned */
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

void func_80088BFC(void);
void func_800444D8(void *callback);
void func_80048BC4(void);
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
void func_800439E0(DispEnv *env, s32 x, s32 y, s32 w, s32 h);
void func_80043928(DrawEnv *env, s32 x, s32 y, s32 w, s32 h);
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y);
void func_80045534(u32 *packet, DrawEnv *env);
void func_800453E8(u32 *packet, Rect *area);
void func_8004546C(u32 *packet, s16 *offset);
void func_80048D7C(Vector *in, Vector *out);
void func_8004A480(Vector *a, Vector *b, Vector *out);
void func_80049CEC(Matrix *m, SVector *in, Vector *out);
void func_80049BDC(Matrix *a, Matrix *b);
void func_800898BC(Matrix *m, SVector *eye, SVector *at, SVector *up);
void func_800324B8(s32 kind);
void func_800320E8(void *p);
void func_80032C18(void *p, s32 mode);
void func_8002CBBC(ModelFile *file);
s32 func_800303C8(ModelFile *file, s32 mode);
void func_8002CB54(ModelFile *file, void **resource, ModelPrims **prims);
void func_8002CC54(u16 tpage);
void func_8002CC74(s32 x, s32 y);
void func_8002C8CC(ModelFile *file, void *resource, s32 mode);
void func_800732AC(ModelPrims *prims, void *resource, s32 b);
void func_8002DDE4(void *target, s32 on, s32 a, s32 b, s32 c, s32 d, s32 e);
Node *func_80089B44(Node *node);
void func_80089D5C(Node *node);
void func_80089EB4(ModelSet *set);
void func_80089FF8(Model *model);
void func_8008C120(void *data);
Light *func_8008A254(void);
void func_80089E64(Node *node, void *data);
void func_8008A3A8(Holder *holder);
void func_8008ABAC(Node **lights);
void func_80030A30(s32 index, Light *light);
Model *func_80089F8C(Model *model);
void func_8004A12C(s32 x, s32 y);
void func_8004A14C(s32 h);
void func_8002DFF0(s32 w, s32 h);
void func_80089210(s32 width, s32 height);
void func_80089330(s32 width, s32 height);
void func_80089534(s32 width, s32 height);
void *func_8008BA2C(void *file, s32 a, void *buffer, s32 size);
void func_80019CA0(void);
void func_80043BE4(void *block);
void func_80037324(void *block);
void func_8008BB3C(void *state);
void func_8008EADC(void);
void func_80032CB8(void);
void func_80043B48(void *block, void *data);
s32 func_8004B54C(s32 mode);
void func_80088C28(void);
void func_8003700C(char *format, ...);
void func_80036DC8(s32 r, s32 g, s32 b);
void func_8008ACB8(s32 a);
void func_8008AC8C(void);
void func_800445D0(s32 a);
void func_80044E9C(DispEnv *env);
void func_80044D48(void *block, Window *buffer);
s32 func_800888E4(s32 flag);
void func_800888B0(void);
s32 func_800889C8(void);
void func_8003278C(s32 a, s32 value, s32 c, s32 d);
s16 func_80043A58(s32 x, s32 y);
void func_8008895C(void);
void func_80088A40(void);

#endif
