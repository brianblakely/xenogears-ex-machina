#ifndef MENU_SCENE_H
#define MENU_SCENE_H

#include "menu.h"

/* Scratchpad work area of the scene drawing. */
typedef struct {
    Vector camera; /* 0x00: camera position of this frame */
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

/* libgte SVECTOR layout. */
typedef struct {
    s16 vx, vy, vz, pad;
} SVector;

/* A 3D line segment of the scene, projected into its LINE_F2 each frame. */
typedef struct {
    LineF2 line;
    SVector from; /* 0x10 */
    SVector to;   /* 0x18 */
} SceneLine;

/* libgpu DR_TPAGE and SPRT layouts. */
typedef struct {
    u32 tag;
    u32 code[1];
} DrTpage;

typedef struct {
    u32 tag;
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
extern u8 D_800928A0;   /* draw buffer being built */
extern s32 D_800928E8;

typedef struct {
    s16 unk0, unk2, unk4, unk6;
    u8 unk8[4];
} SceneCell12;

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
    u16 unk2;
} GroundSquare;

extern GroundSquare *D_800928DC;

/* A model shown by the menu scene (fields known from 8007b210). */
typedef struct {
    u8 unk0[0x4C];
    s8 unk4C;
    u8 unk4D[2];
    u8 unk4F;
    u8 unk50[2];
    u8 unk52;
    u8 unk53[0x945];
    s16 unk998;
    s16 unk99A;
    u8 unk99C[0x30];
    u8 unk9CC[0xC00];
    u8 *unk15CC;
} SceneModel;

extern s32 D_8009292C;
void func_80074BA4(SceneModel *model);
void func_80074678(SceneModel *model, s32 x, s32 y);

extern Vector D_80096FA8; /* camera position */
extern SceneLine D_80094818[100];
/* libgpu TILE_1 layout, colour and code written as one word. */
typedef struct {
    u8 addr[3];
    u8 len;
    u32 rgbc;
    s16 x0, y0;
} Tile1;

/* Rectangle in VRAM (libgpu RECT). */
typedef struct {
    s16 x, y, w, h;
} Rect;

/* Per draw buffer block whose first member is the drawn area. */
typedef struct {
    Rect area;
    u8 unk8[0xF0];
} FrameArea;

extern FrameArea D_8009A0D8[2];
extern Tile1 *D_800926C0;
extern Tile1 *D_800926C4;
extern SceneCell12 *D_800926C8;
extern SceneCell10 *D_800926BC;
extern u8 D_80092708;
extern s32 D_800926DC;
extern s16 D_800926E8;
extern s16 D_800926EC;
extern s32 D_800912DC;

/* Glyph of the menu font. */
typedef struct {
    u8 unk0[2];
    u8 width; /* 0x02 */
} Glyph;

/* Text cursor and colour of the menu's text drawing. */
extern u8 D_800926F0, D_800926F4, D_800926F8; /* text colour r, g, b */
extern s32 D_80059488;

/* Options of the menu's settings screen, one byte each. */
extern u8 D_80099D98[];
extern u8 D_80092884;
extern char *D_800912F4[];
extern u32 D_8009274C; /* pad buttons repeating this frame */
extern u32 D_80092750; /* pad buttons pressed this frame */

extern s8 D_8009273C;
extern s8 D_80092740;
extern s32 D_80092744;
extern s32 D_800912F0;
/* A page of the settings/system menu (0x3C bytes; table at D_800915AC). */
/* An entry block of a page; +0x14 bit 2 hides it. */
typedef struct {
    u8 unk0[0x14];
    u8 flags;
} MenuItem;

typedef struct {
    u8 unk0[4];
    MenuItem *item; /* 0x04 */
    u8 unk8[2];
    s16 count;  /* 0x0A */
    u8 unkC[6];
    s16 cursor; /* 0x12 */
    s16 y;      /* 0x14 */
    u8 unk16[0xE];
    s16 x;      /* 0x24 */
    u8 unk26[2];
    s16 width;  /* 0x28 */
    u8 unk2A[0x12];
} MenuPage;

/* Formats shared by the settings pages ("%d", "%dFPS"). */
extern char D_8006FF5C[];
extern char D_8006FF60[];
extern char D_8006FF7C[]; /* "" */
extern u32 D_80092710;  /* bit 0/1: controller port 1/2 unavailable */
extern s32 D_80092754;
extern s32 D_8009272C;  /* port 1 vibration entry selected */
extern s32 D_80092730;  /* port 2 vibration entry selected */
extern s32 D_80092938;
s32 func_80035734(s32 port); /* controller type */
void func_8007EC54(u8 *text);
void func_8007F258(s32 arg, s32 flag);
extern char *D_8009132C[]; /* names of the entries of setting 10 */
void func_80081100(s32 sound, s32 arg);
extern s32 D_8009130C[]; /* value of each speed setting */
extern u8 D_80091300[];  /* frame rate of each rate setting */
extern s16 D_80099DA4;
char *func_8007F97C(void);
void func_8007ECF0(u8 *text);
void func_8007E894(s32 x, s32 y);
void func_8007F948(MenuPage *page, s32 entry);
s32 func_8003FBF8(char *out, char *format, ...); /* sprintf */

extern MenuPage D_800915AC[];
extern MenuPage *D_80092734; /* shown page */
extern MenuPage *D_80092738; /* page to return to */
extern s32 D_80092924;
extern s32 D_800928C8;
extern s8 D_80092758;
extern s32 D_80092940;
extern u8 *D_800928D8; /* the 49 portraits, 0x1000 bytes each */
extern void *D_80092760; /* loaded image data */

void func_80080964(s32 page);
void func_80080B58(void);
void func_8007F8B4(void);
void func_80031BB4(s32 high); /* choose the heap end to allocate from */
void func_8004495C(Rect *rect, s32 x, s32 y); /* copy a VRAM area */
void func_800448F8(Rect *rect, void *pixels);  /* read a VRAM area */
s32 func_8003FA38(void);                       /* random number */
extern s8 D_800926FC;
extern s8 D_8009275C;
void func_80080AA0(s32 forget);
void func_800719F0(void);
void func_8008509C(s32 a, s32 b);
void func_80039FF8(void);
void *func_800891C0(s32 arg);
void func_80028A60(s32 arg);
void func_800320E8(void *block); /* free a heap block */
void func_800445D0(s32 mode);    /* wait for drawing */
void func_8007313C(void *src, void *dst);
s32 func_80044894(s16 *rect, void *pixels); /* load an image into VRAM */
extern s32 D_80092950;

/* 32-byte records of the list at D_80092874; +4 is the required level. */
typedef struct {
    u8 unk0[4];
    s16 level;
    u8 unk6[0x1A];
} ListSource;

typedef struct {
    s32 id;
    u8 unk4[8];
} ListEntry;

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
extern ListSource *D_80092874;
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
extern u8 D_80091369;
extern u8 D_80091391;
extern s32 D_80092748;   /* bit 0: both sides may pick the same entry */
extern s32 D_80091364;
extern u16 D_8005948C;   /* first controller's newly pressed buttons */
void func_80085134(s32 side);
void *func_800289D0(s32 index);
void func_8002954C(void *entry, void *dst, s32 size, s32 a3, s32 a4);
void func_80032C18(void *block, s32 arg);

void *func_80031BDC(s32 size, s32 flag); /* allocate from the heap */
void func_8008895C(void);
void func_8007E3CC(void *arg);
void func_80043B48(void *ot, void *prim); /* link a primitive into an OT entry */
Glyph *func_8007E8AC(s32 ch);
void func_8007E964(s32 ch);
s32 func_8007EB6C(u8 *text);
void func_8007EE08(s32 highlight);
void func_8007F834(void);
s32 func_8007FF70(s32 value, s32 max, s32 flags);
void func_80080C48(s32 arg);
void func_8007D334(s32 arg0, s32 arg1, s32 kind);
void func_8008EB4C(s32 sound);

#endif
