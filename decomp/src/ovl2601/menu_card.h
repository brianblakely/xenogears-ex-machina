#ifndef OVL2601_MENU_CARD_H
#define OVL2601_MENU_CARD_H

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
    void *resources[3];       /* 4630 */
    u8 unk463C[0x4654 - 0x463C];
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

/* The gear screen block (ovl2602, menu state + 454, 1f00h bytes). */
typedef struct {
    u8 unk0[0x80];
    POLY_FT4 packets[2]; /* 80 */
    u8 unkD0[0x1ED9 - 0xD0];
    u8 unk1ED9[7];       /* 1ed9 */
    u8 unk1EE0;
    u8 buffer;           /* 1ee1 */
    u8 unk1EE2[0x1F00 - 0x1EE2];
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
    u8 unk66[0x6C - 0x66];
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
    u8 weapons[5];     /* 6a: equipment slots for weapons (ids below 32h) */
    u8 armour[5];      /* 6f: equipment slots for armour (ids from 32h) */
    u8 accessories[3]; /* 74 */
    u8 unk77[0xA0 - 0x77];
    u8 unkA0;      /* a0: ff for a member who cannot be chosen */
    u8 unkA1[3];
} Character;

/* The persistent game data (8006d634). */
typedef struct {
    u8 unk0[0x26C];
    Character characters[11]; /* 26c */
    u8 unk978[0x1924 - 0x978];
    u32 gold;                 /* 1924 */
} GameData;

extern GameData D_8006D634;

/* A party member's detail view; its nine stat words at +b8. */
typedef struct {
    u8 unk0[0xB8];
    u16 stats[9]; /* b8 */
} MemberView;

/* Entries of the equipment (weapons below 32h, armour from 32h), accessory and item tables (10h bytes each). */
typedef struct {
    u16 users; /* 00: party bits of the members who can equip it */
    u16 unk2;
    u16 price; /* 04 */
    u8 type;   /* 06 */
    u8 unk7[5];
    u8 power;  /* 0c: attack or defence */
    u8 unkD[3];
} EquipInfo;

typedef struct {
    u16 users; /* 00 */
    u16 price; /* 02 */
    u8 unk4[4];
    u8 power;  /* 08 */
    u8 unk9[5];
    u16 group; /* 0e: accessories of one group do not add up */
} AccessoryInfo;

typedef struct {
    u16 unk0;
    u16 price; /* 02 */
    u8 unk4[2];
    u8 flags;  /* 06: 10h cannot be sold */
    u8 unk7[9];
} ItemInfo;

/* The unpacked resources (menu state + 330, cch bytes). */
typedef struct {
    EquipInfo *equipment;       /* 00 */
    AccessoryInfo *accessories; /* 04 */
    void *unk8[5];
    ItemInfo *items;     /* 1c */
    u8 unk20[0xCC - 0x20];
} ResourceSet;

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
    ResourceSet *resources; /* 330 */
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
extern u16 D_801D21F0[]; /* party bit of each member id */
extern u8 D_801D2018[];  /* label text ids */
extern s32 D_801D1F54[]; /* command picture pairs */
extern u8 D_801D1FCC[];  /* command label text ids */
extern s32 D_801D1FD8[]; /* command label x offsets */
extern u8 D_801D1FD0[];  /* sell list label text ids */
extern s32 D_801D1FE8[]; /* sell list label x offsets */
extern s32 D_801D1F6C[]; /* list picture pairs (sprite, second layer), eight words per command */
extern s32 D_801D1FF8[]; /* marker x */
extern s32 D_801D2008[]; /* marker y */
extern s32 D_801D201C[]; /* file slot -> list position */
extern s32 D_801D2094[]; /* marker x per list position */
extern s32 D_801D2114[]; /* marker y per list position */
extern s32 D_801D2194[]; /* cursor x per position */
extern s32 D_801D21CC[]; /* member portrait x */
extern u8 D_801D2210[];  /* heading sprite ids */
extern u8 D_801D2214[];  /* alternative heading sprite ids */
extern s32 D_801D2218[]; /* heading x */
extern s32 D_801D2228[]; /* alternative heading x */
extern s32 D_801D2230[]; /* heading y */
extern s32 D_801D2240[]; /* alternative heading y */
extern s32 D_801D1F50;   /* items the shop sells */
extern u16 D_801D2260;   /* count of the item last looked up */
extern s32 D_801D2248;   /* first number x */
extern s32 D_801D224C;   /* first number y */
extern s32 D_801D2250;   /* second number x */
extern s32 D_801D2254;   /* second number y */
extern s32 D_801D2258;   /* third number x */
extern s32 D_801D225C;   /* third number y */
extern s32 D_801D21B0[]; /* cursor y per position */

/* Game state. */
extern u8 D_8006F36C[];  /* inventory 0 counts (100) */
extern u8 D_8006F434[];  /* inventory 1 counts (200) */
extern u8 D_8006F5C4[];  /* inventory 2 counts (150) */
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
extern const CardPrefix D_801C5000;      /* "BISLPS-00800" */
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
u8 *func_80033848(s32 id);               /* equipment name */
u8 *func_800337E8(s32 id);               /* accessory name */
u8 *func_80033818(s32 id);               /* item name */
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
void func_801C54B4(void);
void func_801C5A7C(Label *label, s32 index, s32 row, s32 mode);
void func_801C5E6C(void);
void func_801C6430(void);
void func_801C6460(POLY_G4 *poly, u8 r, u8 g, u8 b);
void func_801C6E90(SVECTOR *quad, u16 x, u16 y, u16 w, u16 h);
void func_801C6EE8(POLY_FT4 *poly);
void func_801C7370(u8 index);
void func_801C768C(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C77F0(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C7A38(u8 index, u16 x, u16 y, u16 w);
void func_801C7D7C(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C80C8(u8 index, u16 x, u16 y, u16 h);
void func_801C8410(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801C875C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar);
void func_801C8EB8(s32 index);
void func_801C908C(s32 index);
void func_801C9260(s32 index);
void func_801C9434(s32 index);
void func_801C9608(s32 index);
void func_801C9744(s32 index);
void func_801C9890(s32 index);
void func_801C9F7C(void);
void func_801CA00C(void);
void func_801CA09C(void);
void func_801CA214(void);
void func_801CA22C(void);
void func_801C8AF0(void);
void func_801C5194(u8 allocate);
void func_801C51F8(u8 allocate);
void func_801C525C(u8 allocate);
void func_801C52C0(u8 allocate);
void func_801C5324(u8 allocate);
void func_801C5388(u8 allocate);
void func_801C53EC(u8 allocate);
void func_801C5450(u8 allocate);
void func_801C6828(u8 mode);
void func_801C88E0(u8 index);
void func_801C5CBC(Label *labels, u8 *text_ids, s32 row, s32 count);
void func_801CB384(u8 first);
void func_801CB7F4(void);
u8 func_801CB894(u8 wait);
void func_801CBC88(u8 render, u8 count, Label *labels, u8 *text_ids, u8 *shown);
void func_801CB014(void);
u8 func_801D1658(void);
void func_801D18A8(void);
void func_801D18E8(void);
void func_801D1928(void);
void func_801D1B18(void);
void func_801CB340(void);
void func_801CC278(u8 menu);
void func_801CC720(u8 menu);
void func_801C70B8(void);
void func_801C7314(u8 index);
void func_80033B34(u8 *codes, u8 *text, s32 count); /* codes to text */
void func_801D0E68(s32 n, u8 *ids, u8 *counts, u8 kind, u8 same_kind, u8 *kinds, u8 member);
void func_801D0C18(u32 gold, u8 *ids, u8 *amounts, s32 n, u8 *inv_ids, u8 *inv_counts, u8 *kinds,
                   u8 inventory, u8 member);
u32 func_801CFF58(u8 id, u8 kind);
void func_801D05BC(s32 top, u8 *ids, u8 *kinds, u8 *chosen, u8 *held);
void func_801D1968(u8 unk0, u8 unk1);
void func_801C50E8(u32 value);
void func_801CB370(void);
u8 func_801D1CA4(void);
u8 func_801CF780(void);
void func_801D1F10(void);
void func_801C6A6C(void);
void func_801C58F4(void);
void func_801C5A6C(void);
void func_801C5EE8(void);
void func_801C64DC(void);
void func_801C5F44(void);
void func_801CCAD8(void);
void func_801CBB08(void);
void func_801CC024(s32 count, s32 *ids);
void func_801CC54C(u8 count, u8 selected, s32 *ids);
void func_801CBCF0(u8 count, Label *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row, u8 mode);
void func_801C604C(s32 position, u8 frame);
void func_801CACC8(void);
void func_801CAED4(void);
void func_801CABF4(void);
void func_801C896C(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void func_801C9AB4(void);
void func_801C9C2C(void);
void func_801CA388(void);
void func_801CA404(void);
void func_801C8E28(void);
void func_801C8DDC(void);
void func_801CA444(void);
void func_801CAB0C(void);
void func_801CAB80(void);
void func_801CCFF4(void);
void func_801CAC7C(u8 sound);
void func_801C8D58(s32 count, POLY_FT4 *packets, s32 first);
void func_801C8C3C(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);

#endif
