#ifndef OVL2598_PARTY_MENU_H
#define OVL2598_PARTY_MENU_H

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

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

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
    SVECTOR corners[4]; /* 0x50: 3D corners when projected */
    RECT rect;          /* 0x70: VRAM area of the rendered text */
    u8 *image;          /* 0x78: text render buffer */
    u8 highlight;       /* 0x7C: selects the highlighted CLUT */
    u8 buffer;          /* 0x7D: the quad of the buffer it was built for */
    u8 width;           /* 0x7E: rendered text width */
    u8 projected;       /* 0x7F: drawn through the GTE */
} MenuLabel;

/* The cursor/confirmation markers: two quads per marker. */
typedef struct {
    POLY_FT4 poly[8];
    u8 shown[4];  /* 0x140 */
    u8 follow[4]; /* 0x144: placed at the file cursor */
    u8 buffer[4]; /* 0x148 */
} Markers;

/* A draw buffer's environment block (0xB4 bytes): its DRAWENV and DISPENV
 * and a 16-entry ordering table (entry 4: windows and panels). */
typedef struct {
    u8 draw[0x5C]; /* DRAWENV */
    u8 disp[0x14]; /* 0x5C: DISPENV */
    u32 ot[16];    /* 0x70 */
    u8 pad_B0[4];
} DrawEnv;

/* The loaded effect bank object. */
typedef struct {
    u8 pad_0[0x14];
    u16 id; /* 0x14 */
} EffectBank;

/* The 0x1194-byte block at state + 0x350. */
typedef struct {
    u8 pad_0[0x1180];
    RECT screen;     /* 0x1180: VRAM area copied to the display buffer */
    u8 pad_1188[0xC];
} ListBlock;

/* The 0x5034-byte menu work block. */
typedef struct {
    u8 pad_0[0xB80];
    u32 *tim[5];      /* 0xB80: TIM_IMAGE of the card icon (mode, crect, caddr, prect, paddr) */
    u8 pad_B94[0x4B94 - 0xB94];
    u8 magic[2];      /* 0x4B94: save header "SC" */
    u8 icon_type;     /* 0x4B96 */
    u8 blocks;        /* 0x4B97 */
    u8 title[0x5C];   /* 0x4B98 */
    u8 clut[0x20];    /* 0x4BF4 */
    u8 icon[0x80];    /* 0x4C14 */
    u8 pad_4C94[0x4F7C - 0x4C94];
    s32 cursor;       /* 0x4F7C: file screen cursor */
    u8 pad_4F80[0x5034 - 0x4F80];
} MenuWork;

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
    POLY_FT4 frame_side[2];   /* 0x3C0: sprite 0x106 */
    POLY_FT4 frame_ends[4];   /* 0x410: sprite 0x105 at the top, flipped at the bottom */
    POLY_G4 fill[2];          /* 0x4B0 */
    DR_MODE fill_mode[2];     /* 0x4F8 */
    SVECTOR corner_at[16];    /* 0x510: four corner quads */
    SVECTOR edge_at[4][2][4]; /* 0x590: two quads per edge */
    SVECTOR fill_at[4];       /* 0x690 */
    SVECTOR side_at[4];       /* 0x6B0 */
    SVECTOR ends_at[8];       /* 0x6D0: top and bottom quads */
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
    u8 pad_5[0xC - 0x5];
    u8 list_label_shown[8]; /* 0xC */
    u8 row_label_shown[6];  /* 0x14 */
    u8 entry_label_shown[6]; /* 0x1A */
    u8 panel_20[7]; /* 0x20: per panel */
    u8 panel_27[7]; /* 0x27: per panel */
    u8 b_2E;        /* 0x2E: message lines shown */
    u8 markers_on;  /* 0x2F */
    u8 party[3]; /* 0x30: party members, 0xFF empty */
    u8 pad_33;
    u8 label_shown[4]; /* 0x34 */
    u8 pad_38[0x46 - 0x38];
    u8 status_on;      /* 0x46: status panels drawn */
    u8 pad_47[0x6C - 0x47];
} MenuFlags;

/* A character status panel (0xBEC bytes): sprite quads, two per sprite
 * (one per draw buffer). */
typedef struct {
    POLY_FT4 layout[18];  /* 0x0 */
    POLY_FT4 extra[10];   /* 0x2D0 */
    POLY_FT4 face[2];     /* 0x460 */
    POLY_FT4 label[2];    /* 0x4B0 */
    POLY_FT4 level[6];    /* 0x500 */
    POLY_FT4 next[6];     /* 0x5F0 */
    POLY_FT4 hp[10];      /* 0x6E0 */
    POLY_FT4 hp_max[10];  /* 0x870 */
    POLY_FT4 ep[6];       /* 0xA00 */
    POLY_FT4 ep_max[6];   /* 0xAF0 */
    u8 level_count;       /* 0xBE0 */
    u8 next_count;        /* 0xBE1 */
    u8 hp_count;          /* 0xBE2 */
    u8 hp_max_count;      /* 0xBE3 */
    u8 ep_count;          /* 0xBE4 */
    u8 ep_max_count;      /* 0xBE5 */
    u8 buffer;            /* 0xBE6: buffer it was built for */
    u8 shown;             /* 0xBE7 */
    u8 layout_count;      /* 0xBE8 */
    u8 extra_count;       /* 0xBE9 */
    u8 pad_BEA[2];
} StatusPanel;

/* The shared menu state (*D_800625A0), as far as this overlay uses it. */
typedef struct {
    u8 pad_0[0x6C];
    DrawEnv envs[2];    /* 0x6C */
    DrawEnv *draw_env;  /* 0x1D4: the buffer being built */
    SVECTOR view_rotation;    /* 0x1D8 */
    VECTOR view_translation;  /* 0x1E0 */
    MATRIX view_matrix;       /* 0x1F0 */
    u8 pad_210[0x2DC - 0x210];
    void *sprite_sheet; /* 0x2DC: sprite table for func_8002675C */
    void *label_text;   /* 0x2E0: label text offset table */
    EffectBank *effect_bank; /* 0x2E4 */
    u8 pad_2E8[0x308 - 0x2E8];
    s32 buffer_index;    /* 0x308: draw buffer being built (0/1) */
    u8 available[16];    /* 0x30C: character may join the party */
    u8 digits[9];        /* 0x31C: decimal digits (0xFF blank) */
    u8 input_code;       /* 0x325: decoded input of this frame */
    u8 card_poll_timer;  /* 0x326 */
    u8 active;           /* 0x327 */
    u8 pad_328;
    u8 view_motion;      /* 0x329: nonzero while the view moves */
    u8 sounds;           /* 0x32A: nonzero plays menu sounds */
    u8 pad_32B;
    MenuWork *work;      /* 0x32C */
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
    ListBlock *block_350; /* 0x350 */
    u8 *block_354;       /* 0x354: 0x140C bytes */
    u8 pad_358[0x364 - 0x358];
    Panel *panels[7];      /* 0x364 */
    PanelGrowth *growth[7]; /* 0x380 */
    u8 pad_39C[0x428 - 0x39C];
    Markers *markers;      /* 0x428 */
    u8 pad_42C[0x46C - 0x42C];
    SpriteInfo sprites[4]; /* 0x46C: sprites 0xFE, 0x103, 0x100, 0x101 */
    u8 pad_4CC[0x4E0 - 0x4CC];
    MenuLabel labels[4];   /* 0x4E0: the screen's command labels */
    MenuLabel list_labels[8]; /* 0x6E0 */
    MenuLabel row_labels[6];  /* 0xAE0 */
    u8 pad_DE0[0x1DF0 - 0xDE0];
    StatusPanel *member_panels[6]; /* 0x1DF0 */
    StatusPanel *party_panels[3];  /* 0x1E08 */
    u8 members[11];                /* 0x1E14: characters that may join (0xFF end) */
    u8 pad_1E1F[0x1E94 - 0x1E1F];
    u8 b_1E94;                     /* 0x1E94: toggled by button 0x100 */
    u8 b_1E95;                     /* 0x1E95: counts button 1 */
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
/* A character record (0xA4 bytes) of the game data. */
typedef struct {
    u8 pad_0[0x4C];
    u16 hp;     /* 0x4C */
    u16 hp_max; /* 0x4E */
    u16 ep;     /* 0x50 */
    u16 ep_max; /* 0x52 */
    u8 pad_54[0x62 - 0x54];
    u8 level;   /* 0x62 */
    u8 b63;   /* 0x63 */
    u8 pad_64[0xA4 - 0x64];
} CharacterRecord;

/* The game data at 8006d634, as far as the menus use it. */
typedef struct {
    u8 names[31][0x14];            /* text codes */
    CharacterRecord characters[31]; /* 0x26C */
} GameData;

extern GameData D_8006D634;
extern u16 D_8006F94C;   /* characters locked in place */

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
extern void func_8004B54C(s32 mode);           /* VSync */
extern void func_80044AD8(u32 *ot, s32 count); /* ClearOTag */
extern void func_80044C44(void *env);          /* PutDrawEnv */
extern void func_80044E9C(void *env);          /* PutDispEnv */
extern void func_8004495C(RECT *rect, s32 x, s32 y); /* MoveImage */
extern void func_80044BD0(u32 *ot);            /* DrawOTag */
extern void func_80019CA0(void);               /* reset combination check */
extern void func_80039DB8(s32 sound);          /* play a sound */
extern void func_8003A094(void *bank);
extern void func_8003852C(void *bank);
extern s32 func_80035734(s32 port);            /* pad present */
extern void func_80037EE4(void);               /* pause sound */
extern void func_80037E8C(void);               /* resume sound */
extern s32 func_80036410(void);                /* input queue overflowed */
extern void func_80035DB0(void);               /* reset the input queue */
extern s32 func_80035CDC(void);                /* dequeue an input entry */
extern s32 D_80059488;                         /* vsync count */
extern s32 *D_8005917C;                        /* stack guard word (-1 intact) */
extern u16 D_800594A4;                         /* dequeued buttons */
extern u16 D_8005948C;                         /* dequeued buttons (pressed) */
extern void func_80043B48(u32 *ot, void *prim); /* AddPrim */
extern s32 func_8004A73C(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s16 *sxy0,
                         s16 *sxy1, s16 *sxy2, s16 *sxy3, s32 *p, s32 *flag); /* RotTransPers4 */
extern void func_80043CB0(POLY_FT4 *p);        /* SetPolyFT4 */
extern void func_80043CC4(POLY_G4 *p);         /* SetPolyG4 */
extern void func_80043C9C(POLY_F4 *p);         /* SetPolyF4 */
extern void func_80043DA0(LINE_F3 *p);         /* SetLineF3 */
extern void func_800454DC(DR_MODE *p, s32 dfe, s32 dtd, s32 tpage, RECT *tw); /* SetDrawMode */
extern void func_80043BFC(void *p, s32 abe);   /* SetSemiTrans */
extern void func_80043C24(void *p, s32 tge);   /* SetShadeTex */
extern void func_8004960C(void);                /* PushMatrix */
extern void func_800496AC(void);                /* PopMatrix */
extern MATRIX *func_8003F738(SVECTOR *r, MATRIX *m); /* RotMatrix */
extern MATRIX *func_80049D9C(MATRIX *m, VECTOR *v);  /* TransMatrix */
extern void func_80049EFC(MATRIX *m);           /* SetRotMatrix */
extern void func_80049F8C(MATRIX *m);           /* SetTransMatrix */
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
