#ifndef OVL2600_NAME_ENTRY_H
#define OVL2600_NAME_ENTRY_H

#include "common.h"

/* PsyQ libgpu primitives and rectangles. */
typedef struct {
    s16 x, y, w, h;
} RECT;

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
} POLY_FT4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
} POLY_F4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    u32 pad;
} LINE_F3;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_MODE;

typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

/* The screen backdrop primitives (0x15C bytes), one of each per buffer. */
typedef struct {
    u8 pad_0[0x50];
    POLY_G4 gradient[2]; /* 0x50 */
    POLY_F4 fade[2];     /* 0x98 */
    LINE_F3 line_a[2];   /* 0xC8 */
    LINE_F3 line_b[2];   /* 0xF8 */
    DR_MODE mode_a[2];   /* 0x128 */
    DR_MODE mode_b[2];   /* 0x140 */
    u8 pad_158[3];
    u8 b_15B;            /* 0x15B */
} Backdrop;

/* A menu text label: two textured quads (one per draw buffer) showing a
 * line rendered into VRAM at `rect`. */
typedef struct {
    POLY_FT4 poly[2];
    u8 pad_50[0x20];
    RECT rect;     /* 0x70: VRAM area of the rendered text */
    u8 *image;     /* 0x78: text render buffer */
    u8 highlight;  /* 0x7C: selects the highlighted CLUT */
    u8 pad_7D;
    u8 width;      /* 0x7E: rendered text width */
    u8 shown;      /* 0x7F */
} MenuLabel;

/* The six outputs of func_80026338 for one sprite. */
typedef struct {
    s32 unk0;
    s32 tpage_mode; /* texture depth for GetTPage */
    s32 clut_x;
    s32 clut_y;
    s32 page_x;
    s32 page_y;
} SpriteInfo;

/* Sprite part packets built by func_8002675C: one quad per draw buffer. */
typedef struct {
    POLY_FT4 poly[2];
} SpriteParts;

/* A 3D menu panel: four edge strips (two pieces per draw buffer), the frame
 * sprites, the translucent fill and the corner vectors it is projected from. */
typedef struct {
    POLY_FT4 corner[8];       /* 0x0: corner sprite parts, two per part */
    POLY_FT4 edge[4][4];      /* 0x140 */
    SpriteParts frame_side;   /* 0x3C0: sprite 0x106 */
    SpriteParts frame_top;    /* 0x410: sprite 0x105 */
    SpriteParts frame_bottom; /* 0x460: sprite 0x105, flipped */
    POLY_G4 fill[2];          /* 0x4B0 */
    DR_MODE fill_mode[2];     /* 0x4F8 */
    SVECTOR corner_at[4][4];  /* 0x510: corner quads */
    SVECTOR edge_at[4][2][4]; /* 0x590: two quads per edge */
    SVECTOR fill_at[4];       /* 0x690 */
    SVECTOR side[4];          /* 0x6B0 */
    SVECTOR top[4];           /* 0x6D0 */
    SVECTOR bottom[4];        /* 0x6F0 */
    s32 corner_parts;         /* 0x710: corner parts built */
    s32 style;                /* 0x714 */
    s32 param;                /* 0x718 */
    u8 buffer;                /* 0x71C: buffer it was laid out for */
    u8 framed;                /* 0x71D: frame sprites built */
    u8 pad_71E[2];
} Panel;

/* A panel's opening animation (0x18 bytes). */
typedef struct {
    u16 x, y, w, h; /* final rectangle */
    u16 cur_w;      /* 0x8 */
    u16 cur_h;      /* 0xA */
    s32 param;      /* 0xC */
    u8 index;       /* 0x10 */
    u8 open;        /* 0x11: fully grown */
    u8 style;       /* 0x12 */
    u8 framed;      /* 0x13 */
    u8 pad_14[4];
} PanelGrowth;

/* The menu flag block (0x6C bytes): per-window/panel state bytes and the
 * party being edited. */
typedef struct {
    u8 pad_0[3];
    u8 flag_3; /* 0x3 */
    u8 flag_4; /* 0x4 */
    u8 pad_5[0x20 - 0x5];
    u8 panel_20[7]; /* 0x20: per panel */
    u8 panel_27[7]; /* 0x27: per panel */
    u8 pad_2E[0x30 - 0x2E];
    u8 party[3]; /* 0x30: party members, 0xFF empty */
    u8 pad_33[0x6C - 0x33];
} MenuFlags;

/* The shared menu state (*D_800625A0), as far as this overlay uses it. */
typedef struct {
    u8 pad_0[0x2DC];
    void *sprite_sheet; /* 0x2DC: sprite table for func_8002675C */
    void *label_text;   /* 0x2E0: label text offset table */
    void *effect_bank;  /* 0x2E4 */
    u8 pad_2E8[0x308 - 0x2E8];
    s32 buffer_index;    /* 0x308: draw buffer being built (0/1) */
    u8 available[16];    /* 0x30C: character may join the party */
    u8 pad_31C[0x325 - 0x31C];
    u8 input_code;       /* 0x325: decoded input of this frame */
    u8 card_poll_timer;  /* 0x326 */
    u8 active;           /* 0x327 */
    u8 pad_328[0x32C - 0x328];
    u8 *work;            /* 0x32C: 0x5034-byte work block */
    u8 *block_330;       /* 0x330: 0xCC bytes */
    u8 b_334;            /* 0x334 */
    u8 b_335;            /* 0x335 */
    u8 top_cursor;       /* 0x336 */
    u8 b_337;            /* 0x337 */
    u8 pad_338[0x33C - 0x338];
    MenuFlags *flags;    /* 0x33C */
    u8 pad_340[0x348 - 0x340];
    Backdrop *backdrop;  /* 0x348 */
    u8 pad_34C[0x350 - 0x34C];
    u8 *block_350;       /* 0x350: 0x1194 bytes */
    u8 *block_354;       /* 0x354: 0x140C bytes */
    u8 pad_358[0x364 - 0x358];
    Panel *panels[7];      /* 0x364 */
    PanelGrowth *growth[7]; /* 0x380 */
    u8 pad_39C[0x46C - 0x39C];
    SpriteInfo sprites[4]; /* 0x46C: sprites 0xFE, 0x103, 0x100, 0x101 */
    u8 pad_4CC[0x4E0 - 0x4CC];
    MenuLabel labels[4];   /* 0x4E0: the screen's command labels */
    u8 pad_6E0[0x1E20 - 0x6E0];
    u8 *entry;             /* 0x1E20: name entry block (0xDEC bytes) */
} MenuState;

extern MenuState *D_800625A0;

/* The loaded menu resource archive: entries are packed data. */
typedef struct {
    s32 count;
    void *entry[7];
} MenuArchive;

extern MenuArchive *D_8005945C;
extern void *D_8006259C; /* effect bank */
extern u8 D_80059178;    /* sound effects enabled */
extern u16 D_80059414;   /* highlighted text CLUT */
extern u16 D_800595D4;   /* normal text CLUT */
extern u16 D_8006F364;   /* characters that may join */
extern u16 D_8006F366;
extern u8 D_8006F368[3]; /* current party (0xFF empty) */

extern void *func_80031BDC(s32 size, s32 mode); /* allocate */
extern void func_800320E8(void *block);         /* release */
extern void func_8003F8E8(void *dst, s32 size); /* bzero */
extern void func_8003F99C(void *dst, void *src, s32 size); /* memmove */
extern void *func_80032E88(void *packed, s32 mode);       /* unpack */
extern void func_8003342C(void *archive);
extern void func_8002DD20(void *data);
extern void func_80028470(s32 a, s32 b);
extern s32 func_800288EC(s32 id);
extern void func_800295D8(s32 id, void *buffer, s32 a, s32 b);
extern void func_80028A60(s32 a);
extern void func_80038428(void *bank);
extern void func_800471B4(void *tim);          /* OpenTIM */
extern void func_800471C4(void *image);        /* ReadTIM */
extern void func_80044894(RECT *rect, void *data); /* LoadImage */
extern void func_800445D0(s32 mode);           /* DrawSync */
extern void func_80043CB0(POLY_FT4 *p);        /* SetPolyFT4 */
extern void func_80043CC4(POLY_G4 *p);         /* SetPolyG4 */
extern void func_80043C9C(POLY_F4 *p);         /* SetPolyF4 */
extern void func_80043DA0(LINE_F3 *p);         /* SetLineF3 */
extern void func_800454DC(DR_MODE *p, s32 dfe, s32 dtd, s32 tpage, RECT *tw); /* SetDrawMode */
extern void func_80043BFC(void *p, s32 abe);   /* SetSemiTrans */
extern void func_80043C24(void *p, s32 tge);   /* SetShadeTex */
extern u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
extern u16 func_80043A58(s32 x, s32 y);                   /* GetClut */
extern s32 func_8002675C(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y,
                         s32 scale);
extern s32 func_800263E4(void *sheet, s32 id, void *parts, s32 buffer, s32 x, s32 y,
                          s32 scale, s32 a, s32 b);
extern void *func_80033728(void *table, s32 index);      /* message address */
extern u8 func_80034EAC(void *text, u8 *image, s32 a, s32 b); /* render text */
extern void func_80033698(s32 a, s32 b);
extern void func_80026338(void *sheet, s32 id, s32 *a, s32 *b, s32 *c, s32 *d,
                          s32 *e, s32 *f);

#endif
