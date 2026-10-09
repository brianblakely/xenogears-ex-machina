#ifndef OVL2601_MENU_CARD_H
#define OVL2601_MENU_CARD_H

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"

/* A save file name prefix (13 bytes with its terminator). */
typedef struct {
    char name[13];
} CardPrefix;

/* Two packet groups (menu state + 350, 1194h bytes). */
typedef struct MenuImages {
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
typedef struct MenuSpriteLists {
    POLY_FT4 packets[32];  /* 000 */
    POLY_FT4 packets2[96]; /* 500 */
    s32 count;             /* 1400 */
    s32 count2;            /* 1404 */
    u8 buffer;             /* 1408 */
    u8 buffer2;            /* 1409 */
    u8 unk140A[2];
} ListBlock;

/*
 * The shop screen's packets (menu state + 450, 4788h bytes): sprite groups
 * with their part counts and buffers, the bars and frames of nine rows, the
 * name labels and the resources unpacked from file 2.
 */
typedef struct ShopDetails {
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
    MenuLabel names_a[8];         /* 3c30 */
    MenuLabel names_b[8];         /* 4030 */
    MenuLabel label4430;          /* 4430 */
    MenuLabel label44B0;          /* 44b0 */
    MenuLabel label4530;          /* 4530 */
    MenuLabel label45B0;          /* 45b0 */
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
    u16 attack[16];           /* 46e0: each member's attack before buying */
    u16 defence[16];          /* 4700 */
    u8 unk4720[0x4785 - 0x4720];
    u8 label45B0_shown;       /* 4785 */
    u8 unk4786[2];
} DetailBlock;

/* The gear screen block (ovl2602, menu state + 454, 1f00h bytes). */
typedef struct GearScreen {
    u8 unk0[0x80];
    POLY_FT4 packets[2]; /* 80 */
    u8 unkD0[0x1ED9 - 0xD0];
    u8 unk1ED9[7];       /* 1ed9 */
    u8 unk1EE0;
    u8 buffer;           /* 1ee1 */
    u8 unk1EE2[0x1F00 - 0x1EE2];
} GearScreen;

/* A model part block (ovl2602, menu state + 458/45c). */
typedef struct ModelParts {
    void *data0; /* 00 */
    void *data1; /* 04 */
    u8 unk8[0x12 - 8];
    u8 unk12;    /* 12 */
} ModelParts;

/* Projected markers (ovl2602, menu state + 440). */
typedef struct MenuMarkerQuads {
    POLY_FT4 packets[8]; /* 000 */
    SVECTOR quads[16];   /* 140 */
    u8 buffer;           /* 1c0 */
} MarkerQuads;

/* Cursor and yes/no markers (menu state + 428). */
typedef struct MenuMarkers {
    POLY_FT4 packets[8]; /* 000: two per marker */
    u8 shown[4];         /* 140 */
    u8 at_cursor[4];     /* 144: follows the file cursor */
    u8 buffer[4];        /* 148 */
} MarkerBlock;

/* Cursor block (menu state + 348, 15ch bytes), per draw buffer. */
typedef struct MenuPrims {
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
typedef struct MenuScrollBar {
    POLY_FT4 sprite[2]; /* 00 */
    SVECTOR quad[4];    /* 50 */
    u8 buffer;          /* 70 */
    u8 unk71[3];
} ScrollBar;

/* Animated marker (menu state + 444 + n * 4, 78h bytes). */
typedef struct MenuCursor {
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
typedef struct MenuPanel {
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
typedef struct MenuGrowth {
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
typedef struct MenuCard {
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
typedef struct MenuFlags {
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
typedef struct MenuTables {
    EquipInfo *equipment;       /* 00 */
    AccessoryInfo *accessories; /* 04 */
    void *unk8[5];
    ItemInfo *items;     /* 1c */
    u8 unk20[0xB8 - 0x20];
    u16 stats[9];        /* b8: a member's stats, filled by 801cce1c */
    u8 unkCA[2];
} ResourceSet;


/* Overlay data. */
extern u16 D_801D21F0[]; /* party bit of each member id */
extern u8 D_801D2018[];  /* label text ids */
extern s32 D_801D1F54[]; /* command picture pairs */
extern u8 D_801D1FCC[];  /* command label text ids */
extern s32 D_801D1FD8[]; /* command label x offsets */
extern u8 D_801D1FD0[];  /* sell list label text ids */
extern s32 D_801D1FE8[]; /* sell list label x offsets */
extern u8 D_801D1FD4[];  /* buy list label text ids */
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

/* Resident services. */
extern const CardPrefix D_801C5000;      /* "BISLPS-00800" */
void func_80039DB8(s32 effect);          /* play a sound effect */
extern s32 D_80059488;                   /* sound state saved while paused */
void func_80033698(s32 x, s32 y);        /* text palettes */
u8 *func_80033728(void *table, s32 index); /* entry of a text table */
u8 *func_80033848(s32 id);               /* equipment name */
u8 *func_800337E8(s32 id);               /* accessory name */
u8 *func_80033818(s32 id);               /* item name */
s32 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line */
s32 func_8002675C(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale); /* sprite */
s32 func_800263E4(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* mirrored sprite */


/* This overlay. */
void func_801C54B4(void);
void func_801C5A7C(MenuLabel *label, s32 index, s32 row, s32 mode);
void func_801C5E6C(void);
void func_801C6430(void);
void func_801C6460(POLY_G4 *poly, u8 r, u8 g, u8 b);
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
void func_801C5CBC(MenuLabel *labels, u8 *text_ids, s32 row, s32 count);
void func_801CB384(u8 first);
void func_801CB7F4(void);
u8 func_801CB894(u8 wait);
void func_801CBC88(u8 render, u8 count, MenuLabel *labels, u8 *text_ids, u8 *shown);
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
void func_801CBCF0(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row, u8 mode);
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
void func_801CE480(s32 *diffs, u8 *worse, u8 id, u8 kind, u8 member);
void func_801CE91C(u8 kind, u8 id);
void func_801CD404(s32 count, POLY_FT4 *packets, u8 color);
void func_801CAC7C(u8 sound);
void func_801C8D58(s32 count, POLY_FT4 *packets, s32 first);
void func_801C8C3C(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);

#endif
