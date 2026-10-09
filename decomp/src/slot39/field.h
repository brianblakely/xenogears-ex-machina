#ifndef SLOT39_FIELD_H
#define SLOT39_FIELD_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gamedata.h"
#include "resident/menu.h"
#include "menu/tables.h"

/* The field menu's screens (slot39, menu kind 0): the command list and field
 * blocks, the party and detail panels, and the item, equipment, status and
 * arts screens with the gear stat computations. */

/* An art record of the data tables (MenuTables arts[], per character, then
 * per gear from 11): its target and its ether or fuel cost. */
typedef struct ArtInfo {
    u16 target; /* 0: 4000 all, 1000 none, else one; low bits: target kind */
    u8 pad2[0xF];
    u8 unk11; /* 11 */
    u8 pad12[0x1];
    u8 cost; /* 13: character ether cost */
    u8 pad14[0x3];
    u8 unk17; /* 17: level digit (records 7-13, 801e1ac8) */
    u8 pad18[0xC];
    u16 gearCost; /* 24: gear fuel cost */
    u8 pad26[0x2];
} ArtInfo;

/* The equipment screen's labels (*(state + 360)), and the stats and the
 * equipment kept while a part is chosen. */
typedef struct MenuEquipLabels {
    MenuLabel labels[5]; /* 0 */
    u16 stats[9]; /* 280: stats kept from the equipment screen */
    u8 pad292[0x2];
    u8 visible[5]; /* 294 */
    u8 count; /* 299 */
    u8 pad29A[0x2];
    u8 parts[3][5]; /* 29C: equipment kept from the character or gear */
} MenuEquipLabels;

/* The item screen's list (*(state + 42c)): sixteen rows of names and counts,
 * three extra labels and the item descriptions (+1180). */
typedef struct MenuItemList {
    MenuLabel names[16]; /* 0 */
    MenuLabel values[16]; /* 800 */
    MenuLabel extra[3]; /* 1000 */
    u8 *unk1180; /* 1180 */
    u8 shown[16]; /* 1184 */
    u8 extraShown; /* 1194 */
    u8 pad1195[0x3];
} MenuItemList;

/* The arts list (*(state + 430)): fourteen rows of art names and costs, the
 * headings, the ether rows and the descriptions. */
typedef struct MenuArtsList {
    MenuLabel names[14]; /* 0 */
    MenuLabel values[14]; /* 700 */
    MenuLabel headA; /* E00 */
    MenuLabel headB; /* E80 */
    MenuLabel extra[2]; /* F00 */
    MenuLabel footer; /* 1000 */
    u8 *texts; /* 1080: description texts */
    u8 shown[14]; /* 1084 */
    u8 extraShown; /* 1092 */
    u8 pad1093[0x1];
} MenuArtsList;

/* The status list block (*(state + 438)). */
typedef struct MenuStatusList {
    MenuLabel names[13]; /* 0 */
    MenuLabel values[13]; /* 680 */
    MenuLabel title; /* D00 */
    POLY_FT4 lists[13][10]; /* D80 */
    POLY_G4 gauges[13][2]; /* 21D0 */
    u8 *unk2578; /* 2578 */
    u8 counts[13]; /* 257C */
    u8 starts[13]; /* 2589 */
    u8 shown[13]; /* 2596 */
    u8 gaugeShown[13]; /* 25A3 */
    u8 gaugeBuffer[13]; /* 25B0 */
} MenuStatusList;

/* The field menu command block (*(state + 340)). */
typedef struct MenuFieldMenu {
    POLY_FT4 polys[18]; /* 0 */
    POLY_FT4 cursor[2]; /* 2D0 */
    s32 count; /* 320 */
    u8 start; /* 324 */
    u8 pad325[0x3];
} MenuFieldMenu;

/* The second field menu block (*(state + 344)). */
typedef struct MenuFieldMenu2 {
    POLY_FT4 polys[14]; /* 0 */
    POLY_FT4 polys2[8]; /* 230 */
    u8 start; /* 370 */
    u8 pad371[0x3];
} MenuFieldMenu2;

/* The detail panel block (*(state + 358)): the party slot's portrait (sheet
 * 14b + slot) and name image, the layout parts and the digit lists of the
 * numbers shown, two quads per part (one per draw buffer) with a screen quad
 * each. */
typedef struct MenuDetail {
    POLY_FT4 portrait[2];        /* 0: 48x48 */
    POLY_FT4 name[2];            /* 50: 13 high */
    POLY_FT4 parts[66];          /* A0: layout parts (801d6194) */
    POLY_FT4 level[6];           /* AF0: record +62 */
    POLY_FT4 level2[6];          /* BE0: record +63 */
    POLY_FT4 value3C[16];        /* CD0 */
    POLY_FT4 value40[16];        /* F50 */
    POLY_FT4 exp[14];            /* 11D0 */
    POLY_FT4 expNext[14];        /* 1400 */
    POLY_FT4 hp[10];             /* 1630 */
    POLY_FT4 hpMax[10];          /* 17C0 */
    POLY_FT4 ep[10];             /* 1950 */
    POLY_FT4 epMax[10];          /* 1AE0 */
    POLY_FT4 list1C70[10];       /* 1C70 */
    POLY_FT4 tabs[4];            /* 1E00: two tabs */
    SVECTOR portraitAt[4];       /* 1EA0 */
    SVECTOR nameAt[4];           /* 1EC0 */
    SVECTOR partsAt[33][4];      /* 1EE0 */
    SVECTOR levelAt[3][4];       /* 2300 */
    SVECTOR level2At[3][4];      /* 2360 */
    SVECTOR value3CAt[8][4];     /* 23C0 */
    SVECTOR value40At[8][4];     /* 24C0 */
    SVECTOR expAt[7][4];         /* 25C0 */
    SVECTOR expNextAt[7][4];     /* 26A0 */
    SVECTOR hpAt[5][4];          /* 2780 */
    SVECTOR hpMaxAt[5][4];       /* 2820 */
    SVECTOR epAt[5][4];          /* 28C0 */
    SVECTOR epMaxAt[5][4];       /* 2960 */
    SVECTOR list1C70At[5][4];    /* 2A00 */
    SVECTOR tabsAt[2][4];        /* 2AA0 */
    u8 buffer;                   /* 2AE0 */
    u8 levelCount;               /* 2AE1 */
    u8 level2Count;              /* 2AE2 */
    u8 value3CCount;             /* 2AE3 */
    u8 value40Count;             /* 2AE4 */
    u8 expCount;                 /* 2AE5 */
    u8 expNextCount;             /* 2AE6 */
    u8 hpCount;                  /* 2AE7 */
    u8 hpMaxCount;               /* 2AE8 */
    u8 epCount;                  /* 2AE9 */
    u8 epMaxCount;               /* 2AEA */
    u8 list1C70Count;            /* 2AEB */
    u8 count;                    /* 2AEC: layout parts built */
    u8 tabCount;                 /* 2AED */
    u8 tabBuffer;                /* 2AEE */
    u8 pad2AEF[0x1];
} MenuDetail;

/* The equipment panel block (*(state + 35c)). */
typedef struct MenuEquipPanel {
    POLY_FT4 polys[94];            /* 0: stat name parts (801d7f50) */
    POLY_FT4 rowA[7][8];           /* EB0: per row: parts, two per buffer */
    POLY_FT4 rowB[7][8];           /* 1770 */
    POLY_G4 bars[7][2];            /* 2030: per row, per buffer */
    POLY_G4 highlights[7][2];      /* 2228 */
    SVECTOR verts[188];            /* 2420: stat name quads */
    SVECTOR rowAAt[7][16];         /* 2A00 */
    SVECTOR rowBAt[7][16];         /* 2D80 */
    SVECTOR barAt[7][4];           /* 3100 */
    SVECTOR highlightAt[7][4];     /* 31E0 */
    u8 rowACount[7];               /* 32C0 */
    u8 rowABuffer[7];              /* 32C7 */
    u8 rowBCount[7];               /* 32CE */
    u8 rowBBuffer[7];              /* 32D5 */
    u8 barBuffer[7];               /* 32DC */
    u8 highlightBuffer[7];         /* 32E3 */
    u8 rowShown[7];                /* 32EA */
    u8 buffer;                     /* 32F1 */
    u8 highlighted;                /* 32F2 */
    u8 kind;                       /* 32F3: stat name parts built */
} MenuEquipPanel;

/* A position record passed to the panel builders (+28 base). */
typedef struct MenuAnchor {
    s32 parts[9]; /* 0: layout sprite positions */
    s32 face; /* 24: the portrait (sheet 14b + row) */
    s32 base; /* 28: level digits */
    s32 unk2C; /* 2C */
    s32 hp; /* 30 */
    s32 hpMax; /* 34 */
    s32 ep; /* 38 */
    s32 epMax; /* 3C */
    s32 label; /* 40: name label */
} MenuAnchor;

/* A field-menu block (*(state + 39c)): the party member's portrait (sheet
 * 14b + index) and name image, and part lists. */
typedef struct MenuFieldBlock {
    POLY_FT4 portrait[2]; /* 0 */
    POLY_FT4 name[2]; /* 50 */
    POLY_FT4 list0[54]; /* A0 */
    POLY_FT4 list6[6]; /* 910 */
    POLY_FT4 list8[6]; /* A00 */
    POLY_FT4 list1[6]; /* AF0 */
    POLY_FT4 list2[6]; /* BE0 */
    POLY_FT4 list3[4]; /* CD0 */
    POLY_FT4 list4[4]; /* D70 */
    POLY_FT4 list5[14]; /* E10 */
    POLY_FT4 list7[14]; /* 1040 */
    u8 buffer; /* 1270 */
    u8 count6; /* 1271 */
    u8 count8; /* 1272 */
    u8 count1; /* 1273 */
    u8 count2; /* 1274 */
    u8 count3; /* 1275 */
    u8 count4; /* 1276 */
    u8 count5; /* 1277 */
    u8 count7; /* 1278 */
    u8 count0; /* 1279 */
    u8 pad127A[0x2];
} MenuFieldBlock;

/* The equipment candidate list (*(state + 434)): eight rows, a title, three
 * extra labels and the descriptions of the four equipment kinds. */
typedef struct MenuEquipList {
    MenuLabel names[8]; /* 0 */
    MenuLabel values[8]; /* 400 */
    MenuLabel title; /* 800 */
    MenuLabel extra[3]; /* 880 */
    u8 *texts[4]; /* A00: description texts: weapons, accessories, gear parts, gear accessories */
    u8 shown[8]; /* A10 */
    u8 extraShown; /* A18 */
    u8 padA19[0x3];
} MenuEquipList;

#define GEAR_PART_DURABILITY ((u8 *)&D_8006D634.flags)

/* Gear special part durability per part id: two views into the game data,
 * from +0x2286 and from its flags word at +0x22b6, whose extents the shared
 * GameData leaves unsettled (gearAccessoryIds, flags). */
extern u8 D_8006F8BA[];
extern s32 D_801E9EA0[];      /* item target label x offsets */
extern u8 D_801EA550[];  /* item target labels */
extern u8 D_801EA558[];
extern u8 D_801EA564[];  /* 801e1014 screen labels */
extern u8 D_801EA568[];  /* status screen labels: a character's page, then (6) a gear's */
extern u8 D_801EA548[];  /* item and arts screen labels, eight per page */
extern s32 D_801E9DBC[8]; /* equipment screen: part cursor y by special * 4 + part */
extern s32 D_801E9DDC[];  /* arts list cost x positions */
extern s32 D_801E9E14[];  /* arts list cost y positions */
extern u16 D_801E97F0[];
extern s32 D_801E9788[3]; /* arts screen window sizes per kind */
extern s32 D_801E9794[3];
extern s32 D_801E97A0[3];
extern u8 D_801E9785;    /* the target panels are allocated */
extern u8 D_801E97AC[];       /* 801e1544 screen: five sheet images per row, ff none */
extern s32 D_801E9F48[];      /* status command label x offsets (page 0 and 6) */
extern u8 D_801E9808[20];      /* pilot character of each gear */

/* The field menu's functions that another unit calls, or its own before
 * defining them. */
void func_801D22C4(void);
void func_801D22F4(u8 arg0);
void func_801D2484(void);
void func_801D249C(u8 show);
void func_801D25E4(void);
void func_801D261C(void);
void func_801D2968(void);
void func_801D29A8(u8 arg0, u8 arg1);
void func_801D2D38(void);
void func_801D2EC0(u8 slot, u8 mode);
void func_801D2F4C(u8 message);
void func_801D32B4(void);
void func_801D3488(u8 row, u8 fighters);
void func_801D5A50(u8 index, u8 mode);
void func_801D7C3C(u8 slot, u8 mode);
void func_801D8DE4(u8 slot, u8 lower, u8 arg2, u8 mode);
void func_801D8EA4(u8 slot, u8 mode, u8 kept, u8 gear);
s32 func_801D9704(s32 slot, u8 dir, u8 readyOnly);
u8 func_801D9808(void);
void func_801DA4A8(void);
void func_801DA518(void);
void func_801DA5BC(s32 row);
void func_801DA9A8(s32 entry, s32 row);
void func_801DB02C(u8 index);
void func_801DB0A8(s32 entry, s32 row, u8 kind, u8 index);
void func_801DB340(u8 index);
void func_801DB5E4(u8 mode);
u8 func_801DB920(s32 row, s32 entry);
void func_801DBD4C(s32 a, s32 b);
u8 func_801DBE54(void);
u8 func_801DE29C(u8 slot, u8 arg1);
u8 func_801E0F78(u8 slot, u8 arg1);
u8 func_801E23CC(void);
u8 func_801E2BE4(void);
void func_801E3088(u8 command);
void func_801E35BC();
void func_801E3A80(MenuTables *tables, u8 id);
void func_801E3C2C(MenuTables *tables, u8 gear);
void func_801E3ECC(MenuTables *tables, u8 gear);
void func_801E41C0(MenuTables *tables, u8 gear);
void func_801E4258(MenuTables *tables, u8 gear);
void func_801E42AC(MenuTables *tables, u8 gear);
void func_801E433C(MenuTables *tables, u8 gear);
void func_801E4754(MenuTables *tables, u8 gear);
u8 func_801E4928(u8 gear);
void func_801E4998(MenuTables *tables, u8 gear);
void func_801E5058(void);
void func_801E5178(void);

#endif
