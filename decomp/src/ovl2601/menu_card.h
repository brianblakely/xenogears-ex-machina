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
#include "menu/card.h"
#include "menu/panel.h"
#include "menu/screen.h"

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
