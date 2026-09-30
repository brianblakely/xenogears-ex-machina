#ifndef WORLDMAP_H
#define WORLDMAP_H

#include "common.h"

/* Resident services used by the world map. */
s32 func_800288EC(s32 file);                              /* file size, rounded to words */
void *func_80031BDC(s32 size, s32 mode);                  /* allocate a block */
s32 func_800295D8(s32 file, void *dest, s32 offset, s32 mode); /* read one file */

/* One entry of a disc-read list; a zero file ends the list. */
typedef struct {
    s16 file;
    void *dest;
} FileLoad;

s32 func_80029AFC(FileLoad *list, s32 offset, s32 mode);  /* read a file list */

/* Per-area file set: the base disc file number and three area parameters. */
typedef struct {
    s16 file;
    s16 param2;
    s16 param4;
    s16 param6;
} WorldmapArea;

extern u16 D_8009B564[];          /* area thresholds, indexed from 1 */
extern WorldmapArea D_8009B57C[]; /* area file sets */

/* Loaded area files and their buffers. */
extern s32 D_8009D3C4, D_8009C174, D_8009C17C, D_8009D3D0, D_8009CC98;
extern s32 D_8009D800, D_8009D3C8, D_8009BCD8, D_8009BCC8, D_8009BD08;
extern s32 D_8009D2B4, D_8009D160, D_8009D7CC, D_8009C610;
extern void *D_8009C59C, *D_8009BD20, *D_8009C180, *D_8009D528;
extern void *D_8005945C;
extern void *D_8006259C;
extern void *D_8009BC38, *D_8009BCB0;
extern FileLoad D_8009D3F8[]; /* shared read list */

/* Party: three character ids (0xFF empty) and per-character records. */
typedef struct {
    u8 gear; /* piloted gear, 0xFF none */
    u8 pad1[0xA3];
} CharacterRecord;

extern u8 D_8006F368[3];
extern CharacterRecord D_8006D940[];
extern void *D_8009CD34[3]; /* character model buffers */
extern void *D_8009BDF8[3]; /* gear model buffers */
extern s32 D_8009C170;      /* loaded party members */
extern s32 D_8004F304;
extern void *D_8009C88C, *D_8009C884, *D_8009C888, *D_8009C614;

/* Gouraud quad packet (PsyQ POLY_G4 layout); colour words carry the code
 * in their top byte. */
typedef struct {
    u32 tag;
    u32 rgb0;
    s32 xy0;
    u32 rgb1;
    s32 xy1;
    u32 rgb2;
    s32 xy2;
    u32 rgb3;
    s32 xy3;
} PolyG4;

#define setPolyG4(p) (((u8 *)(p))[3] = 8, ((u8 *)(p))[7] = 0x38)

extern PolyG4 D_8009D194[4][2]; /* sky gradient bands, per buffer */

/* Resident world-map return state. */
typedef struct {
    u16 x;       /* 8006ee54 */
    u16 z;
    u16 heading;
    u16 unk5A;
    u16 unk5C;
    u16 unk5E;
    u16 unk60;
    u16 unk62;
    u16 unk64;
    u16 vehicle_heading; /* 8006ee66 */
    u16 flags;   /* 8006ee68: 0x4000 vehicle, 0x2000 restore, low bits kind */
    s16 unk6A;
    u16 unk6C;
    u16 unk6E;
    u16 unk70;
    u16 unk72;
    u16 unk74;
    u16 unk76;   /* 8006ee76 */
} WorldmapReturn;

extern WorldmapReturn D_8006EE54;
extern u8 D_8006F8E5, D_8006F8E6, D_8006F8E7;

typedef struct {
    s32 vx, vy, vz;
} Vec3;

/* Named arrival point: position in world units and its id; -1 ends a list. */
typedef struct {
    s16 x;
    s16 id;
    s16 z;
    s16 pad;
} WorldmapSpot;

extern s32 D_8009BE10; /* movement mode */
extern Vec3 D_8009C5AC; /* player position (20.12) */
extern s32 D_8009C584;  /* player heading */
extern WorldmapSpot *D_8009D3F4;

void func_8008DFF4(Vec3 *position);

/* PsyQ libgpu environments. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    u32 dr_env[16];
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct {
    DRAWENV draw;
    DISPENV disp;
    u32 unk70;
    u32 unk74;
} DisplayBuffer;

extern DisplayBuffer D_8009BBC8[2];
extern s32 D_8009BCDC;
extern u8 D_8009BB48[3]; /* background colour */

void func_80044110(s32 mode);
void func_8004A14C(s32 value);
DRAWENV *func_80043928(DRAWENV *env, s32 x, s32 y, s32 w, s32 h);
DISPENV *func_800439E0(DISPENV *env, s32 x, s32 y, s32 w, s32 h);
void func_8002C6E0(s32 r, s32 g, s32 b);
void func_8004A0EC(s32 r, s32 g, s32 b);
void func_8004A10C(s32 r, s32 g, s32 b);
void func_80048AB0(s32 a, s32 b, s32 c);

/* Actor slots (0x80 bytes each). */
typedef struct {
    u8 pad0[0x4C];
    s32 handle;
    u8 pad50[0x30];
} WorldmapActor;

extern WorldmapActor *D_8009BE24;
extern void *D_80062528;
extern u16 D_8006F954[]; /* resident flag words */
extern void *D_8009BC3C, *D_8009BCB4;

void func_800320E8(void *block); /* free a block */
void *func_80032E88(void *block, s32 mode);
void func_800230A8(s32 handle);
void func_8003A89C(void *a, s32 b, s32 c);
void func_80039FF8(void);
void func_8003852C(void *data);
void func_80024FB8(void);
void func_8007474C(void);
void func_80074F04(void);
void func_800750DC(void);
void func_80075460(void);
void func_80084818(void);
void func_80086124(void);
void func_80086568(void);
void func_800866C8(void);
void func_80088FF4(void);
void func_80089128(void);
void func_80092DD0(void);
void func_800931B0(void);
void func_800960BC(void);
void func_800976A0(void);
void func_80097D64(void);

/* Area file: section offsets from its start. */
typedef struct {
    s32 unk0;
    s32 spots;   /* spot block */
    s32 off8, offC, off10, off14, off18, off1C, off20, off24;
    s32 unk28;
    s32 models[16];
} AreaHeader;

/* Spot block: arrival points and a table of four sections. */
typedef struct {
    s32 spots;
    s32 table;
} SpotHeader;

extern void *D_8009D308, *D_8009CD48, *D_8009C7EC, *D_8009BD30, *D_8009D784;
extern void *D_8009BCC0;
extern s32 *D_8009D7C8, *D_8009D77C; /* texture animation sections: count, offsets */
extern void *D_8009D73C[16];
extern s32 *D_8009BD00;

/* Texture animations: each slot uploads one image of a frame sequence. */
typedef struct {
    s16 image;    /* image index in the animation's data */
    s16 duration; /* frames; negative ends the sequence */
} TexAnimFrame;

typedef struct {
    RECT rect;
    s32 unk8;
    TexAnimFrame *frames;
} TexAnimSlot;

typedef struct {
    u8 *images;
    TexAnimSlot *slot;
    s16 frame;
    s16 timer;
} TexAnim;

extern TexAnimSlot D_8009A1E8[], D_8009A250[];
extern TexAnim *D_8009D780, *D_8009D7D0;
extern s32 D_8009CC9C, D_8009CD64;

void func_80044894(RECT *rect, void *data); /* upload to VRAM */

extern void *D_8009BE14, *D_8009BE18;
extern WorldmapSpot *D_8009D30C; /* ring of 16 recent positions */
extern s32 D_8009BE38;

/* PsyQ libgte types. */
typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* Scratchpad work area of the sky renderer. */
typedef struct {
    SVECTOR angle;
    MATRIX view;
    MATRIX rotation;
    s32 p;
    s32 flag;
} SkyScratch;

#define SKY_SCRATCH ((SkyScratch *)0x1F800000)

#define addPrim(ot, p) \
    (*(u32 *)(p) = (*(u32 *)(p) & 0xFF000000) | (*(u32 *)(ot) & 0xFFFFFF), \
     *(u32 *)(ot) = (*(u32 *)(ot) & 0xFF000000) | ((u32)(p) & 0xFFFFFF))

extern SVECTOR D_8009A280[4][4]; /* sky band corners */
extern u16 D_8009BD3A;          /* camera yaw */
extern MATRIX D_8009C808;       /* camera matrix */
extern s32 D_8009D7F0;          /* current buffer */
extern s32 D_80050100;          /* ordering-table depth shift */

void func_8004A92C(SVECTOR *angle, MATRIX *m); /* RotMatrix */
MATRIX *func_8004931C(MATRIX *a, MATRIX *b, MATRIX *out); /* MulMatrix0 */
void func_80049EFC(MATRIX *m); /* SetRotMatrix */
void func_80049F8C(MATRIX *m); /* SetTransMatrix */
s32 func_8004A73C(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s32 *sxy0, s32 *sxy1,
                  s32 *sxy2, s32 *sxy3, s32 *p, s32 *flag); /* RotAverage4 */

/* Textured quad packet (PsyQ POLY_FT4 layout). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} PolyFT4;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_TWIN;

extern PolyFT4 D_8009C744[4];
extern DR_TWIN D_8009D3D8[2];

u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
u16 func_80043A58(s32 x, s32 y);                  /* GetClut */
void func_80043BFC(void *p, s32 abe);             /* SetSemiTrans */
void func_800453AC(DR_TWIN *p, RECT *tw);         /* SetTexWindow */

extern s16 D_8009C854[16];
extern s32 D_8009D64C, D_8009BE40, D_8009BCC4, D_8009D80C;

/* Frame state. */
typedef struct {
    u8 pad0[0x70];
    u32 *ot; /* ordering table */
    s32 unk74;
} WorldmapView;

extern s32 D_8009D144;
extern s16 D_8009D558;
extern s32 D_8009C5BC;
extern WorldmapView *D_8009BE3C;
extern u8 D_8009BBB4[], D_8009BD40[], D_8009BE28[];

void func_80073B04(void);
void func_800737EC(void);
void func_800740B8(void);
void func_800747DC(void);
void func_800848F4(void);
void func_80085CDC(void);
void func_8008615C(void);
void func_80086798(void);
void func_80089748(void);
void func_80089C78(void);
void func_80096130(void);
void func_80097244(void *);
void func_80097440(void *);
void func_800980D4(void *);
void func_800981C8(void *);
void func_800983A0(void *);
void func_80098CC0(void);
void func_8009932C(u32 *ot, s32, void *);

#endif
