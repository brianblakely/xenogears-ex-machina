#ifndef MENU_SCENE_H
#define MENU_SCENE_H

#include "menu.h"

/* Scratchpad work area of the scene drawing. */
typedef struct {
    Vector camera; /* 0x00: camera position of this frame */
    SVector point; /* 0x10: particle position relative to the camera */
    SVector from;  /* 0x18: line end points relative to the camera */
    SVector to;    /* 0x20 */
    SVector extra; /* 0x28: fourth corner of a projected quad */
    Vector corner[6]; /* 0x30: view-rotated sprite corner offsets */
    u8 unk90[0x20];
    s32 depth;     /* 0xB0: projected depth */
} SceneScratch;

#define SCENE_SCRATCH ((SceneScratch *)0x1F800000)

/* libgpu LINE_F2 layout. */
typedef struct {
    u8 addr[3];
    u8 len;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LineF2;

/* A 3D line segment of the scene, projected into its LINE_F2 each frame. */
typedef struct {
    LineF2 line;
    SVector from; /* 0x10 */
    SVector to;   /* 0x18 */
} SceneLine;

/* libgpu SPRT layout. */
typedef struct {
    u8 addr[3];
    u8 len;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 w, h;
} Sprt;

/* A sprite with its texture page, one per draw buffer. */
typedef struct {
    DrTpage tpage;
    Sprt sprite;
} SceneSprite;

extern SceneSprite D_800954D8[2];

typedef struct {
    s16 unk0, unk2, unk4; /* position */
    s16 unk6;             /* remaining life */
    s8 unk8;              /* x speed */
    s8 unk9;              /* z speed */
    s16 unkA;             /* vertical speed */
} SceneCell12;

/* TILE with its colour and code as one word (16 bytes). */
typedef struct {
    u8 addr[3];
    u8 len;
    u32 rgbc;
    s16 x0, y0;
    s16 w, h;
} TileWords;

extern TileWords *D_800926CC[2]; /* scene cell tiles per draw buffer */
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
typedef struct {
    u16 height;
    u16 unk2; /* map renderer: UV high nibbles 0xF0F0, orientation bits 0..1,
               * texture-page/CLUT selector bits 2..3 */
} GroundSquare;

extern GroundSquare *D_800928DC;

void func_80074BA4(Actor *actor);

extern SceneLine D_80094818[100];
/* libgpu TILE_1 layout, colour and code written as one word. */
typedef struct {
    u8 addr[3];
    u8 len;
    u32 rgbc;
    s16 x0, y0;
} Tile1;

extern Tile1 *D_800926C0[2]; /* ground particle tiles per draw buffer */
extern SceneCell12 *D_800926C8;
extern SceneCell10 *D_800926BC;
extern u8 D_80092708;
extern s32 D_800926DC;
extern s16 D_800926E8;
extern s16 D_800926EC;
extern s32 D_800912DC;

/* Glyph of the menu font. */
typedef struct {
    u8 u, v;   /* texture position */
    u8 width;
    u8 height;
} Glyph;

/* Text cursor and colour of the menu's text drawing. */
extern u8 D_800926F0, D_800926F4, D_800926F8; /* text colour r, g, b */
extern volatile s32 D_80059488;

extern char *D_800912F4[];
extern u32 D_8009274C; /* pad buttons repeating this frame */
extern u32 D_80092750; /* pad buttons pressed this frame */

extern u8 D_8009273C;
extern u8 D_80092740;
extern s32 D_80092744;
extern s32 D_800912F0;
/* A page of the settings/system menu (0x3C bytes; table at D_800915AC). */
/* libgpu DR_MOVE layout. */
typedef struct {
    u32 tag;
    u32 code[5];
} DrMove;

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
    TileRgb frame[2]; /* 0x1C: the page's box, per draw buffer */
} MenuPage;

/* Formats shared by the settings pages ("%d", "%dFPS"). */
extern char D_8006FF5C[];
extern char D_8006FF60[];
extern char D_8006FF7C[]; /* "" */
extern char D_8006FE8C[]; /* level and computer command names */
extern char D_8007008C[]; /* menu line texts */
extern u32 D_80092710;  /* bit 0/1: controller port 1/2 unavailable */
extern s32 D_80092754;
extern s32 D_8009272C;  /* port 1 vibration entry selected */
extern s32 D_80092730;  /* port 2 vibration entry selected */
s32 func_80035734(s32 port); /* controller type */
void func_8007EC54(u8 *text);
void func_8007F258(void *ot, s32 flag);
extern char *D_8009132C[]; /* names of the entries of setting 10 */
void func_80081100(s32 sound, s32 arg);
extern s32 D_8009130C[]; /* value of each speed setting */
extern u8 D_80091300[];  /* frame rate of each rate setting */
char *func_8007F97C(void);
void func_8007ECF0(u8 *text);
void func_8007F948(MenuPage *page, s32 entry);
s32 sprintf(char *out, char *format, ...); /* sprintf */

/* D_800915AC, D_80092734 and D_80092738 are declared as Menu (window.h);
 * the settings pages view them as MenuPage. */
extern s32 D_80092924;
extern s32 D_800928C8;
extern u8 D_80092758;
extern s32 D_80092940;
extern u8 *D_800928D8; /* the 49 portraits, 0x1000 bytes each */
extern void *D_80092760; /* loaded image data */

void func_80080964(s32 page);
void func_80080B58(void);
void func_8007F8B4(void);
void func_80031BB4(s32 high); /* choose the heap end to allocate from */
void MoveImage(Rect *rect, s32 x, s32 y); /* copy a VRAM area */
void StoreImage(Rect *rect, void *pixels);  /* read a VRAM area */
extern u8 D_800926FC;
extern u8 D_8009275C;
void func_80080AA0(s32 forget);
void func_8008509C(s32 a, s32 b);
void *func_800891C0(s32 arg);
/* In-place RGB555 sliding box filter; second row is 0x280 bytes ahead.
 * end is the terminating read cursor, and a pair there is prefetched. */
void func_8007313C(void *pixels, void *end);

typedef struct {
    s32 id;
    char *model; /* 0x04: model file name */
    u8 *name;    /* 0x08 */
} ListEntry;

extern char D_80070284[]; /* the gears' model files and names, heap tag names */

/* VRAM areas of one of the 49 portrait slots (20 bytes; D_8009270C):
 * its palette row and its 30x64 image. */
typedef struct {
    s16 clut_x;
    s16 clut_y;
    s16 clut_w;
    s16 clut_h;
    s16 image_x;
    s16 image_y;
    s16 image_w;
    s16 image_h;
    u8 unk10[4];
} GridCell;

extern u16 D_8006EF64;
extern ListEntry D_80091964[49];
extern struct MoveList *D_80092874; /* per model id */
extern ListEntry **D_800928EC;
extern s32 D_80092888;
extern GridCell *D_8009270C;

/* Two-player selection: each side's pick and confirmation. */
extern s32 D_80092700;   /* first side's pick */
extern s32 D_80092704;   /* second side's pick */
extern s32 D_80092714;
extern s32 D_80092718;
extern s32 D_8009271C;
extern s32 D_80092720;
extern s32 D_80092724;
extern s32 D_80092728;
extern s32 D_80092748;   /* bit 0: both sides may pick the same entry */
extern s32 D_80091364;
void func_80085134(s32 side);
void *func_800289D0(s32 index);
void func_8002954C(void *entry, void *dst, s32 size, s32 a3, s32 a4);

void func_8007E3CC(u32 *ot);
extern Glyph D_80091230[]; /* menu font glyphs: digits, capitals, punctuation */
Glyph *func_8007E8AC(s32 ch);
s32 func_8007E964(s32 ch);
s32 func_8007EB6C(u8 *text);
void func_8007EE08(s32 highlight);
s32 func_8007FF70(s32 value, s32 max, s32 flags);
void func_80080C48(s32 arg);
void func_8007D334(Vector *from, Vector *to, s32 kind);
void func_8004A8EC(Matrix *m, Matrix *out);
void func_8007BBA0(Matrix *view, Matrix *local, u32 *ot);
void func_8007C280(Matrix *view, Matrix *local, u32 *ot);
void func_8007CAA4(Matrix *view, Matrix *local, u32 *ot);
void func_8007D918(u32 *ot);
void func_8007E020(u32 *ot);

/* Menu overlay drawing. */
extern DrTpage D_800954C8[2];
extern DrMove D_80095498[2];
extern PolyFT4 *D_800926D4[2]; /* text quads, per draw buffer */
void SetDrawMove(DrMove *p, Rect *rect, s32 x, s32 y);     /* set a DR_MOVE */
void func_800811AC(void *ot);

/* SPRT with its position and texture coordinates as whole words. */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    u32 xy0;
    u16 uv0;
    u16 clut;
    s16 w, h;
} SprtWords;

/* A sprite strip drawn from a shared pixel buffer, per draw buffer. */
typedef struct {
    u8 *pixels;
    SprtWords sprite[2];
    u8 unk2C[4];
} SpriteStrip;

extern SpriteStrip D_80095510[2];
extern DrTpage D_80095570[2];
extern u16 D_800595D4;
extern u16 D_80059414;
void SetSprt(void *prim);           /* initialise a SPRT */
void SetShadeTex(void *prim, s32 semi); /* set semi-transparency */

/* POLY_FT4 with its positions written as whole words and its texture
 * coordinates as halfwords. */
typedef struct {
    u8 addr[3];
    u8 len;
    u32 rgbc;
    u32 xy0;
    u16 uv0;
    u16 clut;
    u32 xy1;
    u16 uv1;
    u16 tpage;
    u32 xy2;
    u16 uv2;
    u16 pad1;
    u32 xy3;
    u16 uv3;
    u16 pad2;
} PolyFT4Words;

/* Two-player selection wheels: each side's portraits per buffer, and the
 * neighbour offsets and slide of the portraits beside the pick (row 1
 * while sliding right or still). */
extern PolyFT4Words D_80099DA8[2][10];
extern s16 D_800912E0[2][4];
void func_8007F05C(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade);
void func_8007EE68(s32 highlight);

extern u16 D_800926E0; /* text texture page */
extern u16 D_800926E4; /* text CLUT */


void func_8007D274(Vector *from, Vector *to);

/* Image data of the menu (+0x3C: the font TIM, +0x64: the banner TIM). */
typedef struct {
    u8 unk0[0x3C];
    u32 *font;
    u8 unk40[0x24];
    u32 *banner;
} MenuFiles;


#endif
