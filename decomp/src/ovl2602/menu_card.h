#ifndef OVL2602_MENU_CARD_H
#define OVL2602_MENU_CARD_H

#include "common.h"

/* PsyQ libgpu primitives and rectangles used by the card screens. */
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

/* libgs/libgpu TIM description (OpenTIM/ReadTIM). */
typedef struct {
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

/* A save file name prefix (13 bytes with its terminator). */
typedef struct {
    char name[13];
} CardPrefix;

/* The menu resources block (*8005945c): packed files by index. */
typedef struct {
    s32 count;
    void *files[8];
} MenuResources;

/* Texture of a sprite sheet entry (80026338's six outputs). */
typedef struct {
    s32 unk0;
    s32 mode;           /* texture mode for GetTPage */
    s32 clut_x, clut_y; /* GetClut */
    s32 page_x, page_y; /* GetTPage */
} SheetEntry;

/* A text label: its quad per draw buffer and the VRAM area of its pixels. */
typedef struct {
    POLY_FT4 poly[2]; /* 00 */
    SVECTOR quad[4];  /* 50: corners when projected */
    RECT rect;        /* 70: pixel area in VRAM */
    void *pixels;     /* 78: label pixel buffer (only labels[0] of the menu state) */
    u8 highlight;     /* 7c */
    u8 buffer;        /* 7d */
    u8 width;         /* 7e: text width in pixels */
    u8 projected;     /* 7f: drawn through the GTE */
} Label;

/* Two packet groups (menu state + 350, 1194h bytes). */
typedef struct {
    POLY_FT4 packets[56];  /* 000 */
    POLY_FT4 packets2[56]; /* 8c0 */
    RECT screen;           /* 1180: area copied to the shown buffer */
    s32 count;             /* 1188 */
    s32 count2;            /* 118c */
    u8 buffer;             /* 1190 */
    u8 buffer2;            /* 1191 */
    u8 dim;                /* 1192: requested dimming */
    u8 dimmed;             /* 1193: applied dimming */
} ImageBlock;

/* Two list packet groups (menu state + 354, 140ch bytes). */
typedef struct {
    POLY_FT4 packets[32];  /* 000 */
    POLY_FT4 packets2[96]; /* 500 */
    s32 count;             /* 1400 */
    s32 count2;            /* 1404 */
    u8 buffer;             /* 1408 */
    u8 buffer2;            /* 1409 */
    u8 unk140A[2];
} ListBlock;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LINE_F2;

/*
 * The shop screen's packets (menu state + 450, 4788h bytes): sprite groups
 * with their part counts and buffers, the bars and frames of nine rows, the
 * name labels and the resources unpacked from file 2.
 */
typedef struct {
    POLY_FT4 members[18];     /* 0000: count 46a5, buffer 46a6 */
    POLY_FT4 group2D0[18];    /* 02d0: count 46a9, buffer 46a8 */
    POLY_FT4 heading[44];     /* 05a0: count 46ab, buffer 46aa */
    POLY_FT4 digits1[18];     /* 0c80: count 46ad, buffer 46ac */
    POLY_FT4 digits2[18];     /* 0f50: count 46af, buffer 46ae */
    POLY_FT4 group1220[36];   /* 1220: count 46b4, buffer 46b3 */
    POLY_FT4 price[20];       /* 17c0: ovl2602, count 46bb, buffer 46ba */
    POLY_FT4 digits3[18];     /* 1ae0: count 46b1, buffer 46b0 */
    POLY_FT4 rows[8][8];      /* 1db0: counts 468c, buffers 4694 */
    POLY_FT4 cells_a[9][6];   /* 27b0: counts 46bc, buffers 46ce */
    POLY_FT4 cells_b[9][6];   /* 3020: counts 46c5, buffers 46d7 */
    LINE_F3 bar_upper[18];    /* 3890: two per row */
    LINE_F3 bar_lower[18];    /* 3a40 */
    LINE_F2 frame[2];         /* 3bf0 */
    u8 unk3C10[0x20];
    Label names_a[8];         /* 3c30 */
    Label names_b[8];         /* 4030 */
    Label label4430;          /* 4430 */
    Label label44B0;          /* 44b0 */
    Label label4530;          /* 4530 */
    Label label45B0;          /* 45b0 */
    void *resources[9];       /* 4630: unpacked from file 2 */
    u8 amounts[0x30];         /* 4654 */
    u8 name_shown[8];         /* 4684 */
    u8 row_count[8];          /* 468c */
    u8 row_buffer[8];         /* 4694 */
    u8 bar_shown[9];          /* 469c */
    u8 members_count;         /* 46a5 */
    u8 members_buffer;        /* 46a6 */
    u8 label4430_shown;       /* 46a7 */
    u8 group2D0_buffer;       /* 46a8 */
    u8 group2D0_count;        /* 46a9 */
    u8 heading_buffer;        /* 46aa */
    u8 heading_count;         /* 46ab */
    u8 digits1_buffer;        /* 46ac */
    u8 digits1_count;         /* 46ad */
    u8 digits2_buffer;        /* 46ae */
    u8 digits2_count;         /* 46af */
    u8 digits3_buffer;        /* 46b0 */
    u8 digits3_count;         /* 46b1 */
    u8 digits_shown;          /* 46b2 */
    u8 group1220_buffer;      /* 46b3 */
    u8 group1220_count;       /* 46b4 */
    u8 label44B0_shown;       /* 46b5 */
    u8 unk46B6;
    u8 unk46B7[3];
    u8 price_buffer;          /* 46ba */
    u8 price_count;           /* 46bb */
    u8 cells_a_count[9];      /* 46bc */
    u8 cells_b_count[9];      /* 46c5 */
    u8 cells_a_buffer[9];     /* 46ce */
    u8 cells_b_buffer[9];     /* 46d7 */
    u8 unk46E0[0x4785 - 0x46E0];
    u8 label45B0_shown;       /* 4785 */
    u8 unk4786[2];
} DetailBlock;

/*
 * The gear screen block (ovl2602, menu state + 454, 1f00h bytes): its
 * backdrop and part pictures, and three animated sprite groups: a flicker of
 * three sprites, two lamps that open, idle and close, and a third indicator.
 * States: 0 off, 1 opening, 2 idle, 3 closing (the indicator has 4 steps).
 */
typedef struct {
    POLY_FT4 backdrop[2];      /* 0000: drawn with the menu's buffer */
    u8 unk50[0x30];
    POLY_FT4 packets[2];       /* 0080 */
    POLY_FT4 flicker[3][4];    /* 00d0 */
    POLY_FT4 lamps[66];        /* 02b0: 22 per lamp; lamps 0 and 1 */
    POLY_FT4 indicator[36];    /* 0d00 */
    POLY_FT4 frame[28];        /* 12a0 */
    POLY_FT4 parts[5][10];     /* 1700 */
    u8 flicker_count;          /* 1ed0 */
    u8 lamp_count[3];          /* 1ed1: [2] the indicator's */
    u8 flicker_buffer;         /* 1ed4 */
    u8 lamp_buffer[3];         /* 1ed5 */
    u8 flicker_shown;          /* 1ed8 */
    u8 lamp_state[3];          /* 1ed9: [2] the indicator's */
    u8 flicker_timer;          /* 1edc */
    u8 lamp_timer[3];          /* 1edd */
    u8 flicker_frame;          /* 1ee0 */
    u8 buffer;                 /* 1ee1 */
    s16 indicator_frame;       /* 1ee2 */
    s16 lamp_frame[2];         /* 1ee4 */
    u8 part_count[5];          /* 1ee8 */
    u8 parts_buffer;           /* 1eed */
    u8 unk1EEE[2];
    u16 flicker_x, flicker_y;  /* 1ef0 */
    u16 lamp_x[2];             /* 1ef4 */
    u16 lamp_y[2];             /* 1ef8 */
    u16 indicator_x, indicator_y; /* 1efc */
} GearScreen;

/* A model part block (ovl2602, menu state + 458/45c). */
typedef struct {
    void *data0; /* 00 */
    void *data1; /* 04 */
    u8 unk8[0x12 - 8];
    u8 unk12;    /* 12 */
} ModelParts;

/* Projected markers (ovl2602, menu state + 440). */
typedef struct {
    POLY_FT4 packets[8]; /* 000 */
    SVECTOR quads[16];   /* 140 */
    u8 buffer;           /* 1c0 */
} MarkerQuads;

/* Cursor and yes/no markers (menu state + 428). */
typedef struct {
    POLY_FT4 packets[8]; /* 000: two per marker */
    u8 shown[4];         /* 140 */
    u8 at_cursor[4];     /* 144: follows the file cursor */
    u8 buffer[4];        /* 148 */
} MarkerBlock;

/* Cursor block (menu state + 348, 15ch bytes), per draw buffer. */
typedef struct {
    POLY_FT4 sprite[2];     /* 00 */
    POLY_G4 shade[2];       /* 50 */
    POLY_F4 screen[2];      /* 98 */
    LINE_F3 upper[2];       /* c8 */
    LINE_F3 lower[2];       /* f8 */
    DR_MODE mode_label[2];  /* 128 */
    DR_MODE mode_sprite[2]; /* 140 */
    u8 sprite_buffer;       /* 158 */
    u8 shade_buffer;        /* 159 */
    u8 unk15A;
    u8 width;               /* 15b */
} CursorBlock;

/* Scroll bar (menu state + 43c, 74h bytes). */
typedef struct {
    POLY_FT4 sprite[2]; /* 00 */
    SVECTOR quad[4];    /* 50 */
    u8 buffer;          /* 70 */
    u8 unk71[3];
} ScrollBar;

/* Animated marker (menu state + 444 + n * 4, 78h bytes). */
typedef struct {
    POLY_FT4 sprite[2]; /* 00 */
    SVECTOR quad[4];    /* 50 */
    s32 frame;          /* 70: 4..0 */
    u8 timer;           /* 74 */
    u8 buffer;          /* 75 */
    u8 unk76[2];
} Marker;

/*
 * A framed 3D panel (menu state + 364 + n * 4, 720h bytes). Its sprite parts
 * come in buffer pairs: corners 0-7, top edge 8-11, bottom 12-15, left
 * 16-19, right 20-23, then the scroll bar 24-29; each part has a quad of
 * corner vectors (quads[]) that is projected when drawn.
 */
typedef struct {
    POLY_FT4 parts[30];  /* 000 */
    POLY_G4 back[2];     /* 4b0: background */
    DR_MODE mode[2];     /* 4f8 */
    SVECTOR quads[64];   /* 510: four corners each for the corner pieces 0-3,
                            top 4-5, bottom 6-7, left 8-9, right 10-11,
                            background 12 and bar 13-15 */
    s32 part_count;      /* 710: corner parts the sprite sheet produced */
    s32 flat;            /* 714: drawn with the current matrices, not the panel's own */
    s32 ot_entry;        /* 718 */
    u8 buffer;           /* 71c */
    u8 has_bar;          /* 71d */
    u8 unk71E[2];
} Panel;

/* A panel opening from its centre (menu state + 380 + n * 4, 18h bytes). */
typedef struct {
    u16 x, y, w, h;      /* 00: final rectangle */
    u16 cur_w, cur_h;    /* 08: current size */
    s32 ot_entry;        /* 0c */
    u8 index;            /* 10 */
    u8 done;             /* 11 */
    u8 flat;             /* 12 */
    u8 has_bar;          /* 13 */
    u8 unk14[4];
} PanelGrowth;

/*
 * Card state block (menu state + 32c, 5034h bytes): the directory scan, the
 * file heads and this game's save header.
 */
typedef struct {
    u8 unk0[0xB80];
    TIM_IMAGE icon;    /* b80: the save icon TIM */
    u8 heads[32][0x200]; /* b94: first 200h bytes of each file */
    u8 save_magic[2];  /* 4b94: "SC" */
    u8 save_icon_flag; /* 4b96 */
    u8 save_blocks;    /* 4b97 */
    u8 save_title[0x5C];   /* 4b98 */
    u8 save_palette[0x20]; /* 4bf4 */
    u8 save_icon[0x80];    /* 4c14 */
    u8 unk4C94[0x4F7C - 0x4C94];
    s32 cursor_slot;   /* 4f7c */
    u8 unk4F80[0x4FCE - 0x4F80];
    char game_prefix[13]; /* 4fce */
    u8 unk4FDB[0x5034 - 0x4FDB];
} CardState;

/* Screen flag block (menu state + 33c, 6ch bytes): what is shown, and the party. */
typedef struct {
    u8 unk0[3];
    u8 cursor_shown; /* 03 */
    u8 unk4;         /* 04 */
    u8 unk5[9 - 5];
    u8 images_shown; /* 09 */
    u8 lists_shown;  /* 0a */
    u8 unkB;
    u8 list_label_shown[8]; /* 0c */
    u8 info_label_shown[6]; /* 14 */
    u8 extra_label_shown[6]; /* 1a: ovl2602 */
    u8 panel_shown[7];   /* 20 */
    u8 panel_growing[7]; /* 27 */
    u8 message_shown;    /* 2e */
    u8 marks_shown;      /* 2f */
    u8 members[3];   /* 30: party member ids, ff none */
    u8 unk33;
    u8 label_shown[4];   /* 34 */
    u8 unk38[0x49 - 0x38];
    u8 scroll_shown; /* 49 */
    u8 unk4A[0x50 - 0x4A];
    u8 marker_shown[2]; /* 50 */
    u8 unk52;
    u8 marks_b_shown; /* 53: ovl2602 */
    u8 unk54[0x5A - 0x54];
    u8 unk5A;
    u8 unk5B;
    u8 unk5C[0x63 - 0x5C];
    u8 model_shown;  /* 63: ovl2602 */
    u8 price_shown;  /* 64: ovl2602 */
    u8 gear_shown;   /* 65: ovl2602 */
    u8 gear_parts_shown; /* 66: ovl2602 */
    u8 unk67[0x6C - 0x67];
} ScreenFlags;

/* A linked sound effect bank. */
typedef struct {
    u8 unk0[0x14];
    u16 id; /* 14 */
} EffectBank;

/* A draw buffer's environments and ordering table (b4h bytes). */
typedef struct {
    u8 draw[0x5C]; /* 00: DRAWENV */
    u8 disp[0x14]; /* 5c: DISPENV */
    u32 ot[16];    /* 70 */
    u32 *ot_big;   /* b0: ovl2602's 400h-entry ordering table */
} DrawEnv;

typedef struct {
    s16 m[3][3];
    s16 pad;
    s32 t[3];
} MATRIX;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

/* A character record of the game data (a4h bytes, 8006d8a0). */
typedef struct {
    u8 unk0[4];
    u8 unk4;       /* 04 */
    u8 unk5[0x1C - 5];
    u8 unk1C;      /* 1c */
    u8 unk1D[0x28 - 0x1D];
    u8 base[8];    /* 28 */
    u8 unk30[0x56 - 0x30];
    u8 unk56;      /* 56 */
    u8 unk57;
    u8 bonus[8];   /* 58 */
    u8 unk60[0x6A - 0x60];
    u8 gear[13];   /* 6a: equipped items */
    u8 unk77[0xA0 - 0x77];
    u8 unkA0;      /* a0: ff for a member who cannot be chosen */
    u8 unkA1[3];
} Character;

/* A party member's detail view; its nine stat words at +b8. */
typedef struct {
    u8 unk0[0xB8];
    u16 stats[9]; /* b8 */
} MemberView;

/* Menu state (*800625a0); only the fields this overlay touches are named. */
typedef struct {
    u8 unk0[0x6C];
    DrawEnv envs[2];     /* 6c */
    DrawEnv *draw_env;   /* 1d4: current buffer's draw environment */
    SVECTOR view_rotation;    /* 1d8 */
    VECTOR view_translation;  /* 1e0 */
    MATRIX view_matrix;       /* 1f0 */
    u8 unk210[8];
    SVECTOR model_rotation;   /* 218: ovl2602 */
    VECTOR model_translation; /* 220 */
    MATRIX model_matrix;      /* 230 */
    u8 unk250[0x298 - 0x250];
    u8 model_b[0x2DC - 0x298]; /* 298 */
    void *sprite_sheet;  /* 2dc */
    void *label_text;    /* 2e0 */
    EffectBank *effect_bank; /* 2e4 */
    u8 unk2E8[0x308 - 0x2E8];
    s32 buffer;          /* 308: draw buffer being built (0/1) */
    u8 member_present[16]; /* 30c */
    u8 digits[9];        /* 31c: decimal digits, leading zeros ff */
    u8 input;            /* 325: the frame's decoded input */
    u8 poll_timer;       /* 326 */
    u8 drawing;          /* 327: the screen is drawn */
    u8 unk328;
    u8 view_motion;      /* 329: 4/3 start zooming in/out, 2/1 zooming */
    u8 sounds;           /* 32a: menu sound effects enabled */
    u8 unk32B;
    CardState *card;     /* 32c */
    void **resources;    /* 330: cch bytes of unpacked resources */
    u8 unk334;
    u8 unk335;
    u8 top_cursor;       /* 336 */
    u8 unk337;
    u8 list_cursor;      /* 338 */
    u8 unk339;           /* 339: list cursor last drawn */
    u8 list_count;       /* 33a */
    u8 unk33B;
    ScreenFlags *flags;  /* 33c */
    u8 unk340[0x348 - 0x340];
    CursorBlock *cursor; /* 348 */
    u8 unk34C[0x350 - 0x34C];
    ImageBlock *images;  /* 350 */
    ListBlock *lists;    /* 354 */
    u8 unk358[0x364 - 0x358];
    Panel *panels[7];    /* 364 */
    PanelGrowth *growth[7]; /* 380 */
    u8 unk39C[0x428 - 0x39C];
    MarkerBlock *marks;  /* 428 */
    u8 unk42C[0x43C - 0x42C];
    ScrollBar *scroll;   /* 43c */
    MarkerQuads *marks_b; /* 440: ovl2602's projected markers */
    Marker *markers[2];  /* 444 */
    u8 unk44C[4];
    DetailBlock *details; /* 450 */
    GearScreen *unk454;  /* 454: ovl2602's gear screen block */
    ModelParts *model_parts[2]; /* 458: ovl2602 */
    u8 unk460[0x46C - 0x460];
    SheetEntry sheet_entries[4]; /* 46c */
    u8 unk4CC[0x4E0 - 0x4CC];
    Label labels[4];     /* 4e0: command labels */
    Label list_labels[8]; /* 6e0 */
    Label info_labels[6]; /* ae0 */
    Label extra_labels[6]; /* de0: ovl2602 */
    u8 unk10E0[0x1DE0 - 0x10E0];
    Label *message_labels[4]; /* 1de0 */
    u8 unk1DF0[0x1E20 - 0x1DF0];
    void *unk1E20;       /* 1e20: dech bytes */
    u8 unk1E24[0x1E2C - 0x1E24];
    u8 *unk1E2C;         /* 1e2c: shop tables, 5ch bytes (three kinds of 30 ids) per shop */
    u8 shop_items[0x30]; /* 1e30: the shop's item ids */
    u8 shop_kinds[0x30];  /* 1e60: their kind (0 weapon, 1 armour, 2 item) */
    u8 *gear_tables;     /* 1e90: ovl2602 */
    u8 select_toggle;    /* 1e94: flipped by select */
    u8 unk1E95;          /* 1e95: counts button-1 presses */
} MenuState;

extern MenuState *D_800625A0;

/* Overlay data. */
extern u16 D_801D6C68[]; /* party bit of each member id */
extern u8 D_801D6A80[];  /* label text ids */
extern s32 D_801D69A0[]; /* list picture pairs (sprite, second layer), eight words per command */
extern s32 D_801D6A60[]; /* marker x */
extern s32 D_801D6A70[]; /* marker y */
extern s32 D_801D6A84[]; /* file slot -> list position */
extern s32 D_801D6AFC[]; /* marker x per list position */
extern s32 D_801D6B7C[]; /* marker y per list position */
extern s32 D_801D6BFC[]; /* cursor x per position */
extern s32 D_801D6C44[]; /* member portrait x */
extern u8 D_801D6D10[];  /* alternative heading sprite ids */
extern s32 D_801D6D14[]; /* heading x */
extern s32 D_801D6D34[]; /* alternative heading x */
extern s32 D_801D6D3C[]; /* heading y */
extern s32 D_801D6D5C[]; /* alternative heading y */
extern u16 D_801D904C;   /* count of the item last looked up */
extern s32 D_801D6D64;   /* first number x */
extern s32 D_801D6D68;   /* first number y */
extern s32 D_801D6D6C;   /* second number x */
extern s32 D_801D6D70;   /* second number y */
extern s32 D_801D6D74;   /* third number x */
extern s32 D_801D6C20[]; /* cursor y per position */

/* Game state. */
extern u8 D_8006F3D0[];  /* inventory 0 ids (100), counts just before */
extern u8 D_8006F4FC[];  /* inventory 1 ids (200), counts just before */
extern u8 D_8006F65A[];  /* inventory 2 ids (150), counts just before */
extern Character D_8006D8A0[];
extern u16 D_8006F364;   /* party members joined */
extern u16 D_8006F366;   /* party members available */
extern u8 D_8006F368[3]; /* party member ids */
extern u16 D_80059414;   /* highlighted text CLUT */
extern u16 D_800595D4;   /* plain text CLUT */

/* Resident services. */
extern MenuResources *D_8005945C;        /* the menu resources block */
extern EffectBank *D_8006259C;           /* the loaded menu sound bank */
void OpenTIM(void *tim);           /* OpenTIM */
s32 ReadTIM(TIM_IMAGE *image);     /* ReadTIM */
void func_8002DD20(void *list);          /* load a TIM list into VRAM */
void func_80028470(s32 unk0, s32 unk1);  /* disc access mode */
void func_80038428(EffectBank *bank);    /* link an effect bank */
extern u8 D_80059171;                    /* shop number */
void SetLineF2(LINE_F2 *prim);       /* SetLineF2 */
extern s32 *D_8005917C;                  /* debug word; not -1 stops at a break */
void func_80019CA0(void);                /* soft reset combination */
void ClearOTagR(u32 *ot, s32 n);      /* ClearOTagR */
s32 VSync(s32 mode);             /* VSync */
void PutDrawEnv(void *env);           /* PutDrawEnv */
void PutDispEnv(void *env);           /* PutDispEnv */
s32 MoveImage(RECT *rect, s32 x, s32 y); /* MoveImage */
void DrawOTag(u32 *ot);             /* DrawOTag */
s32 func_80035734(s32 port);             /* controller present */
void func_80037EE4(void);                /* pause the sound */
void func_80037E8C(void);                /* resume the sound */
s32 func_80036410(void);                 /* input queue overflowed */
void func_80035DB0(void);                /* reset the input queue */
s32 func_80035CDC(void);                 /* dequeue an input entry */
void func_80039DB8(s32 effect);          /* play a sound effect */
extern u8 D_80059178;                    /* menu sound effects loaded */
void func_8003A094(EffectBank *bank);    /* stop the bank's effects */
void func_8003852C(EffectBank *bank);    /* unlink an effect bank */
extern s32 D_80059488;                   /* sound state saved while paused */
extern u16 D_800594A4;                   /* dequeued buttons */
extern u16 D_8005948C;                   /* dequeued buttons, second set */
void *func_80031BDC(s32 size, s32 mode); /* heap allocate */
void func_800320E8(void *block);         /* heap free */
void bzero(void *dst, s32 size); /* bzero */
void memmove(void *dst, void *src, s32 size); /* memmove */
void *func_80032E88(void *packed, s32 mode); /* unpack into a new block */
void func_8003342C(void *list);          /* relocate an offset list */
void func_80026338(void *sheet, s32 id, s32 *u, s32 *v, s32 *w, s32 *h, s32 *x, s32 *y);
void func_80033698(s32 x, s32 y);        /* text palettes */
u8 *func_80033728(void *table, s32 index); /* entry of a text table */
s32 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line */
s32 func_8002675C(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale); /* sprite */
s32 func_800263E4(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* mirrored sprite */
s32 func_800288EC(s32 file);             /* file size in words */
void func_800295D8(s32 file, void *dst, s32 offset, s32 mode); /* disc read */
s32 func_80028A60(s32 mode);             /* disc wait */

/* libgpu */
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y); /* GetTPage */
void SetSemiTrans(void *prim, s32 abe);           /* SetSemiTrans */
void SetShadeTex(void *prim, s32 tge);           /* SetShadeTex */
void SetPolyFT4(POLY_FT4 *prim);                /* SetPolyFT4 */
void SetPolyG4(POLY_G4 *prim);                 /* SetPolyG4 */
void SetPolyF4(POLY_F4 *prim);                 /* SetPolyF4 */
void SetLineF3(LINE_F3 *prim);                 /* SetLineF3 */
void SetDrawMode(DR_MODE *p, s32 dfe, s32 dtd, s32 tpage, RECT *tw); /* SetDrawMode */
u16 GetClut(s32 x, s32 y);                   /* GetClut */
void AddPrim(void *ot, void *prim);          /* AddPrim */
void PushMatrix(void);                          /* PushMatrix */
void PopMatrix(void);                          /* PopMatrix */
MATRIX *func_8003F738(SVECTOR *r, MATRIX *m);      /* RotMatrix */
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);       /* TransMatrix */
void SetRotMatrix(MATRIX *m);                     /* SetRotMatrix */
void SetTransMatrix(MATRIX *m);                     /* SetTransMatrix */
s32 RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s16 *xy0, s16 *xy1, s16 *xy2,
                  s16 *xy3, s32 *p, s32 *flag); /* RotTransPers4 */
void LoadImage(RECT *rect, void *data);        /* LoadImage */
s32 DrawSync(s32 mode);                       /* DrawSync */

/* This overlay. */
u16 func_801C5228(u16 mask, u8 id);
void func_801C56C8(void);
void func_801C5CA8(Label *label, s32 index, s32 row, s32 mode);
void func_801C6098(void);
void func_801C665C(void);
void func_801C668C(POLY_G4 *poly, u8 r, u8 g, u8 b);
void func_801C7604(SVECTOR *quad, u16 x, u16 y, u16 w, u16 h);
void func_801C765C(POLY_FT4 *poly);
void func_801C7AE4(u8 index);
void func_801C7E00(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C7F64(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C81AC(u8 index, u16 x, u16 y, u16 w);
void func_801C84F0(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C883C(u8 index, u16 x, u16 y, u16 h);
void func_801C8B84(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C8ED0(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar);
void func_801C9690(s32 index);
void func_801C9864(s32 index);
void func_801C9A38(s32 index);
void func_801C9C0C(s32 index);
void func_801C9DE0(s32 index);
void func_801C9F1C(s32 index);
void func_801CA068(s32 index);
void func_801CA754(void);
void func_801CA7E4(void);
void func_801CA874(void);
void func_801CA9EC(void);
void func_801CAA7C(void);
void func_801C9264(void);
void func_801C5344(u8 allocate);
void func_801C53A8(u8 allocate);
void func_801C540C(u8 allocate);
void func_801C5470(u8 allocate);
void func_801C54D4(u8 allocate);
void func_801C5538(u8 allocate);
void func_801C559C(u8 allocate);
void func_801C5600(u8 allocate);
void func_801C6A54(u8 mode);
void func_801C9054(u8 index);
void func_801C5EE8(Label *labels, u8 *text_ids, s32 row, s32 count);
void func_801CC9A0(void);
void func_801CC1C4(void);
void func_801CD564(u8 menu);
void func_801C782C(void);
void func_801C7A88(u8 index);
void func_80033B34(u8 *codes, u8 *text, s32 count); /* codes to text */
void func_801D2054(s32 n, u8 *ids, u8 *counts, u8 kind, u8 unk4, u8 *counts2, u8 unk6);
void func_801C5298(u32 value);
u16 func_801C5244(u8 id);
void func_801C5B08(void);
void func_801C5C98(void);
void func_801C6114(void);
void func_801C6708(void);
void func_801C6170(void);
void func_801CCD20(void);
void func_801CD310(s32 count, s32 *ids);
void func_801CD838(u8 count, u8 selected, s32 *ids);
void func_801CCEE8(u8 count, Label *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row, u8 mode);
void func_801C6278(s32 position, u8 frame);
void func_801C90E0(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void func_801CA28C(void);
void func_801CA404(void);
void func_801CABE0(void);
void func_801C959C(void);
void func_801C9550(void);
void func_801CAC20(void);
void func_801CB2E8(void);
void func_801CB35C(void);
void func_801CB498(u8 sound);
void func_801C94CC(s32 count, POLY_FT4 *packets, s32 first);
void func_801C93B0(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);

/* ovl2602 only. */
extern u8 D_8006F754[];  /* inventory 4 ids (100), counts just before */
extern u8 D_8006F84E[];  /* inventory 3 ids (150), counts just before */
void func_801C5600(u8 allocate);
void func_801C5664(u8 allocate);
u8 func_801CCA40(u8 wait, u8 movable);

/* A gear record of the game data (a4h bytes, 8006dfac). */
typedef struct {
    u8 unk0[2];
    u8 unk2;       /* 02: entry of the 18h-byte table */
    u8 unk3;       /* 03: entry of the 10h-byte table */
    u8 unk4[4];
    u8 unk8;       /* 08: entry of the 14h-byte table */
    u8 unk9[0x38 - 9];
    u16 unk38;     /* 38 */
    u16 unk3A;     /* 3a */
    u8 unk3C, unk3D, unk3E, unk3F; /* 3c */
    u8 unk40[4];
    u16 unk44;     /* 44 */
    u8 unk46[0x60 - 0x46];
    u32 unk60;     /* 60 */
    u32 unk64;     /* 64 */
    u16 unk68;     /* 68 */
    u8 unk6A[0x70 - 0x6A];
    u16 unk70;     /* 70 */
    u16 unk72;     /* 72 */
    u8 unk74;
    u8 unk75;      /* 75 */
    u8 unk76[0x98 - 0x76];
    u8 unk98;      /* 98 */
    u8 unk99[4];
    u8 unk9D, unk9E, unk9F; /* 9d */
    u8 unkA0[4];
} Gear;

typedef struct {
    u8 unk0[4];
    u32 unk4;  /* 04 */
    u16 unk8;  /* 08 */
    u8 unkA[0x14 - 0xA];
    u8 unk14, unk15, unk16, unk17; /* 14 */
} GearRecord18;

typedef struct {
    u8 unk0[6];
    u16 unk6;  /* 06 */
    u8 unk8[4];
    u8 unkC, unkD, unkE, unkF; /* 0c */
} GearRecord10;

typedef struct {
    u8 unk0[8];
    u16 unk8; /* 08 */
    u16 unkA; /* 0a */
    u8 unkC[0x14 - 0xC];
} GearEntry;

typedef struct {
    u8 unk0[8];
    GearRecord18 *records18; /* 08 */
    GearRecord10 *records10; /* 0c */
    GearEntry *entries;      /* 10 */
} GearTable;

extern Gear D_8006DFAC[];
extern u32 D_801D6C88[];  /* party bit of each member id */
extern s32 D_801D6FD8;    /* available members 1-10 */
s32 rand(void);  /* rand */
void func_801E7D14(void *a, void *b, u32 *ot, s32 buffer);
u32 func_801C527C(u32 mask, u8 id);
u32 func_801C5260(u8 id);
void func_801CCEBC(u8 count, u8 *shown);
void func_801D0EC8(u8 close);
void func_801D61B8(GearTable *table, u8 id);
void func_801D62A4(GearTable *table, u8 id);
void func_801D6250(GearTable *table, u8 id);
void func_801D6334(GearTable *table, u8 id);
void func_801D6738(GearTable *table, u8 id);
extern u8 D_801D9084;      /* gear being edited */
extern u8 *D_801D9088;     /* name pixel buffer */
extern u8 D_801D697C;
extern u8 D_8006D634[][0x14]; /* names */
void func_801D498C(u8 unk0, u8 unk1);
void func_801D5398(void);
void func_801D6150(GearTable *table, u8 id);
void func_801CBA2C(void);
void func_801CEA68(void);
void func_801CEEA8(void);
void func_801CF184(void);
void func_801E7FD4(void);
extern u16 D_801D6D7C[];   /* model file ids */
extern u8 D_801D6A20[];    /* command label text ids */
extern s32 D_801D6FD0[];   /* second marker x */
extern u8 D_801D6D08[];    /* heading sprite ids, four per set */
extern s32 D_801D6D14[];   /* heading x */
extern s32 D_801D6D3C[];   /* heading y */
void func_801CC530(u8 first);
void func_801CC9A0(void);
void func_801CC528(void);
u8 func_801D5828(void);
void func_801CCE90(u8 count, Label *labels, u8 *text_ids, u8 *shown);
extern u8 D_801D6A24[];    /* sell list label text ids */
extern s32 D_801D6FDC;     /* index of the gear screen's member among the available ones */
/* The camera's move between two points (801d9050). */
typedef struct {
    s32 from[3];     /* 00: previous target */
    s32 to[3];       /* 0c: target */
    s32 step[3];     /* 18: 16.16 step per frame */
    s32 offset[3];   /* 24: 16.16 distance travelled */
    u8 negative[3];  /* 30: moving towards smaller coordinates */
    u8 frames;       /* 33: steps per update */
} CameraMove;
extern CameraMove D_801D9050;

/* The Gear model code's state (801e8674, outside this overlay). */
typedef struct {
    u8 unk0[0x56];
    s16 distance; /* 56: camera distance */
} ModelView;
typedef struct {
    u8 unk0[4];
    ModelView *view; /* 04 */
    u8 unk8[0x1C - 8];
    s16 unk1C;       /* 1c */
    u8 unk1E[0x60 - 0x1E];
    s16 unk60;       /* 60 */
} ModelState;
extern ModelState *D_801E8674;
extern POLY_FT4 D_801D7108[]; /* camera debug display packets, two per sprite */
extern s32 D_801D9048;        /* their sprite count */
u8 func_8001BD40(u8 low, u8 high); /* random number in [low, high] */
extern u16 D_801D6FE0[];        /* lamp sprite ids, four per frame, five frames per lamp (ffff none) */
extern u8 D_801D7030[];         /* lamp and indicator position per command and list cursor */
extern u16 D_801D7040[2];       /* lamp x */
extern u16 D_801D7044[];        /* lamp y choices, six per lamp */
extern u16 D_801D705C[6];       /* indicator x choices */
extern u16 D_801D7068[6];       /* indicator y choices */
extern u16 D_801D7074[6];       /* flicker x choices */
extern u16 D_801D7080[6];       /* flicker y choices */
extern u16 D_801D708C[14];      /* gear parts frame sprite ids */
extern u16 D_801D70A8[14];      /* their x */
extern u16 D_801D70C4[14];      /* their y */
/* Gear value positions (x, y). */
extern u16 D_801D70E0, D_801D70E2, D_801D70E4, D_801D70E6, D_801D70E8;
extern u16 D_801D70EA, D_801D70EC, D_801D70EE, D_801D70F0, D_801D70F2;
extern u32 D_8006EF58;     /* party gold */
void ClearImage(void *env, s32 unk1, s32 unk2, s32 unk3);
void AddPrims(u32 *ot, u32 *first, u32 *last); /* link an OT range into another OT */
void func_801CB4E4(void);
void func_801CBE60(void);
void func_801CABD8(void);
void func_801E8030(s32 unk0);
void func_801CFAB8(u8 unk0, u8 id);
void func_801CB690(void);
void func_801D2784(void);
void func_801D27C4(void);
void func_801CE82C(void);
void func_801CF33C(void);
void func_801CE7E0(void);
void func_801C962C(void);
void func_801C5C98(void);
void func_801C6114(void);
void func_801C6708(void);
void func_801C6170(void);
void func_801C6E74(void);
void func_801D5D38(void);
void func_801CE1D0(void);
void func_801CDD74(void);
void func_801CCD20(void);
void func_801C5B08(void);
extern s32 D_801D6980[];   /* command picture pairs */
extern s32 D_801D6A30[];   /* command label x offsets */
void func_801CCEE8(u8 count, Label *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row, u8 mode);
void func_801D0398(u8 back);
u8 func_801CDC68(void);
void func_801CE2E8(void);

#endif
