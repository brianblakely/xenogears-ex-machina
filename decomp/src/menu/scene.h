#ifndef MENU_SCENE_H
#define MENU_SCENE_H

#include "menu.h"

/* Scratchpad work area of the scene drawing. */
typedef struct {
    VECTOR camera; /* 0x00: camera position of this frame */
    SVECTOR point; /* 0x10: particle position relative to the camera */
    SVECTOR from;  /* 0x18: line end points relative to the camera */
    SVECTOR to;    /* 0x20 */
    SVECTOR extra; /* 0x28: fourth corner of a projected quad */
    VECTOR corner[6]; /* 0x30: view-rotated sprite corner offsets */
    u8 unk90[0x20];
    s32 depth;     /* 0xB0: projected depth */
} SceneScratch;

#define SCENE_SCRATCH ((SceneScratch *)0x1F800000)

/* A 3D line segment of the scene, projected into its LINE_F2 each frame. */
typedef struct {
    LINE_F2 line;
    SVECTOR from; /* 0x10 */
    SVECTOR to;   /* 0x18 */
} SceneLine;

/* A sprite with its texture page, one per draw buffer. */
typedef struct {
    DR_TPAGE tpage;
    SPRT sprite;
} SceneSprite;

typedef struct {
    s16 unk0, unk2, unk4; /* position */
    s16 unk6;             /* remaining life */
    s8 unk8;              /* x speed */
    s8 unk9;              /* z speed */
    s16 unkA;             /* vertical speed */
} SceneCell12;

void func_800732AC(void *dst, void *src, s32 size); /* copy memory */

/* A ground particle of the scene (10 bytes; table at D_800926BC). */
typedef struct {
    s16 unk0; /* x */
    s16 unk2; /* height */
    s16 unk4; /* z */
    s8 unk6;  /* remaining life */
    s8 unk7;  /* vertical speed */
    s16 unk8; /* ground height */
} SceneCell10;

/* Ground height map: 128 columns of 256-unit squares, 4 bytes each. */
typedef struct GroundSquare {
    u16 height;
    u16 unk2; /* map renderer: UV high nibbles 0xF0F0, orientation bits 0..1,
               * texture-page/CLUT selector bits 2..3 */
} GroundSquare;

extern GroundSquare *D_800928DC;

void func_80074BA4(Actor *actor);

extern s32 D_800912DC;

/* Glyph of the menu font. */
typedef struct {
    u8 u, v;   /* texture position */
    u8 width;
    u8 height;
} Glyph;

extern volatile s32 D_80059488;

extern char *D_800912F4[];

extern s32 D_800912F0;
/* A page of the settings/system menu (0x3C bytes; table at D_800915AC). */
/* An entry block of a page; +0x14 bit 2 hides it. */
typedef struct {
    u8 unk0[0x14];
    u8 flags;
} PageItem;

typedef struct {
    u8 unk0[4];
    PageItem *item; /* 0x04 */
    u8 unk8[2];
    s16 count;  /* 0x0A */
    u8 unkC[6];
    s16 cursor; /* 0x12 */
    s16 y;      /* 0x14: first text line */
    u8 unk16[6];
    TILE frame[2]; /* 0x1C: the page's box, per draw buffer */
} MenuPage;

void func_8007EC54(u8 *text);
void func_8007F258(void *ot, s32 flag);
extern char *D_8009132C[]; /* names of the entries of setting 10 */
void func_80081100(s32 sound, s32 arg);
extern s32 D_8009130C[]; /* value of each speed setting */
extern u8 D_80091300[];  /* frame rate of each rate setting */
char *func_8007F97C(void);
void func_8007ECF0(u8 *text);
void func_8007F948(MenuPage *page, s32 entry);

/* D_800915AC, D_80092734 and D_80092738 are declared as Menu (window.h);
 * the settings pages view them as MenuPage. */
extern s32 D_80092924;
extern s32 D_800928C8;
extern s32 D_80092940;
extern u8 *D_800928D8; /* the 49 portraits, 0x1000 bytes each */

void func_80080964(s32 page);
void func_80080B58(void);
void func_8007F8B4(void);
void func_80080AA0(s32 forget);
void func_8008509C(s32 a, s32 b);
void *func_800891C0(s32 arg);
/* In-place RGB555 sliding box filter; second row is 0x280 bytes ahead.
 * end is the terminating read cursor, and a pair there is prefetched. */
void func_8007313C(void *pixels, void *end);

typedef struct ListEntry {
    s32 id;
    char *model; /* 0x04: model file name */
    u8 *name;    /* 0x08 */
} ListEntry;

/* VRAM areas of one of the 49 portrait slots (20 bytes; D_8009270C):
 * its palette row and its 30x64 image. */
typedef struct {
    RECT clut;
    RECT image;
    u8 unk10[4];
} GridCell;

extern ListEntry D_80091964[49];
extern struct MoveList *D_80092874; /* per model id */
extern ListEntry **D_800928EC;
extern s32 D_80092888;

extern s32 D_80091364;
void func_80085134(s32 side);

void func_8007E3CC(u32 *ot);
extern Glyph D_80091230[]; /* menu font glyphs: digits, capitals, punctuation */
Glyph *func_8007E8AC(s32 ch);
s32 func_8007E964(s32 ch);
s32 func_8007EB6C(u8 *text);
void func_8007EE08(s32 highlight);
s32 func_8007FF70(s32 value, s32 max, s32 flags);
void func_80080C48(s32 arg);
void func_8007D334(VECTOR *from, VECTOR *to, s32 kind);
void func_8007BBA0(MATRIX *view, MATRIX *local, u32 *ot);
void func_8007C280(MATRIX *view, MATRIX *local, u32 *ot);
void func_8007CAA4(MATRIX *view, MATRIX *local, u32 *ot);
void func_8007D918(u32 *ot);
void func_8007E020(u32 *ot);

/* Menu overlay drawing. */
void func_800811AC(void *ot);


/* Two-player selection wheels: each side's portraits per buffer, and the
 * neighbour offsets and slide of the portraits beside the pick (row 1
 * while sliding right or still). */
extern PolyFT4Words D_80099DA8[2][10];
extern s16 D_800912E0[2][4];
void func_8007F05C(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade);
void func_8007EE68(s32 highlight);

void func_8007D274(VECTOR *from, VECTOR *to);

/* Image data of the menu (+0x3C: the font TIM, +0x64: the banner TIM). */
typedef struct {
    u8 unk0[0x3C];
    u_long *font;
    u8 unk40[0x24];
    u_long *banner;
} MenuFiles;

#endif
