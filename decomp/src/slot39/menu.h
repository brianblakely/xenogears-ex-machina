#ifndef SLOT39_MENU_H
#define SLOT39_MENU_H

#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
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
#include "resident/stream.h"
#include "resident/text.h"
#include "menu/card.h"
#include "menu/panel.h"
#include "menu/screen.h"

/*
 * Menu overlay (Disc 1 slot 39, loaded at 801c5000): the menu mode's state.
 * The resident keeps a pointer to it at D_800625A0; the overlay allocates the
 * state's sub-blocks on entry and frees them on exit. Field names follow what
 * the overlay does with them; unknown bytes stay padding.
 */

/* structs: begin */
/* The second half of a listed file's header block (+100). */
typedef struct MenuSaveName {
    u8 text[0x14]; /* two-byte text */
} MenuSaveName;

typedef struct MenuSaveInfo {
    u8 pad0[0x24];
    MenuSaveName names[4]; /* 24: names of the sheet entries */
} MenuSaveInfo;

/* A gear frame record of the data tables. */
typedef struct GearFrame {
    u8 pad0[0x8];
    u16 unk8; /* 8 */
    u16 unkA; /* A */
    u8 padC[0x8];
} GearFrame;

/* A gear engine record of the data tables (+8). */
typedef struct GearEngine {
    u8 pad0[0x4];
    u32 unk4; /* 4 */
    u8 pad8[0xC];
    u8 unk14; /* 14 */
    u8 unk15; /* 15 */
    u8 unk16; /* 16 */
    u8 unk17; /* 17 */
} GearEngine;

/* A gear record of the data tables (+c). */
typedef struct GearPart {
    u8 pad0[0x6];
    u16 unk6; /* 6 */
    u8 pad8[0x4];
    u8 unkC; /* C */
    u8 unkD; /* D */
    u8 unkE; /* E */
    u8 padF[0x1];
} GearPart;

/* A per-character effect record of the data tables (+20). */
typedef struct MenuEffect {
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
} MenuEffect;

/* A gear weapon (or special part) record of the data tables (+18). */
typedef struct GearWeapon {
    u8 attrs[4]; /* 0 */
    u32 users; /* 4: gears that can equip it */
    u8 pad8[0x6];
    u8 unkE; /* E */
    u8 kind; /* F */
    u8 unk10; /* 10 */
    u8 unk11; /* 11 */
    u16 unk12; /* 12 */
} GearWeapon;

/* An item record of the data tables (+1c). */
typedef struct MenuItem {
    u8 pad0[0x4];
    u16 target; /* 4: 4000 all, 1000 none, else one; low bits: target kind */
    u8 use; /* 6: 80 usable in the menu, 40 in battle, 20 field only */
    u8 pad7[0x1];
    u8 amount; /* 8 */
    u8 pad9[0x1];
    s16 flags; /* A: 8000 HP, 4000 EP, 4 stats, 2 +78, 1 debug */
    s16 stats; /* C: stats raised (flag 4) or +78 change (flag 2) */
    u8 padE[0x2];
} MenuItem;

/* A weapon (or special part) record of the data tables (+0). */
typedef struct MenuWeapon {
    u16 users; /* 0: characters that can equip it (bit per character) */
    u8 pad2[0x4];
    u8 kind; /* 6: weapon class; special parts share their weapon's */
    u8 pad7[0x1];
    u16 value; /* 8 */
    u8 a; /* A */
    u8 b; /* B */
    u8 level; /* C */
    u8 padD[0x3];
} MenuWeapon;

/* An accessory record of the data tables (+4). */
typedef struct MenuAccessory {
    u16 users; /* 0 */
    u8 pad2[0x6];
    u8 amount; /* 8: added to +2d */
    u8 kind; /* 9 */
    s16 value; /* A */
    s16 stats; /* C: bonuses raised by the amount */
    u16 groups; /* E: exclusive groups */
} MenuAccessory;

/* A gear accessory record of the data tables (+14). */
typedef struct MenuGearAccessory {
    u32 users; /* 0 */
    u8 pad4[0x2];
    u16 unk6; /* 6: added to the gear's +44 */
    u16 groups; /* 8 */
    u8 padA[0x3];
    u8 unkD; /* D: added to the gear's +40 */
    u8 unkE; /* E: added to the gear's +42 */
    u8 padF[0x1];
    u8 unk10[4]; /* 10: added to the gear's +50 */
    u8 unk14; /* 14: added to the gear's +4d */
    u8 kind; /* 15: effect kind (801e433c) */
    u16 value; /* 16: effect bits or amount */
    u8 unk18; /* 18: added to the gear's +4c */
    u8 pad19[0x1];
    u8 unk1A; /* 1A: added to the gear's +88 entries of kind 4 bits */
    u8 unk1B; /* 1B: added to the gear's +54 */
} MenuGearAccessory;

/* The data table directory (*(state + 330)). */
typedef struct MenuTables {
    MenuWeapon *weapons; /* 0 */
    MenuAccessory *accessories; /* 4 */
    GearEngine *engines; /* 8 */
    GearPart *parts; /* C */
    GearFrame *frames; /* 10 */
    MenuGearAccessory *gearAccessories; /* 14 */
    GearWeapon *gearWeapons; /* 18 */
    MenuItem *items; /* 1C */
    MenuEffect *effects[31]; /* 20: per character, then per gear from 11 */
    u32 unk9C; /* 9C: gear stats (801e3c2c) */
    u32 unkA0; /* A0 */
    u16 unkA4; /* A4 */
    u16 unkA6; /* A6 */
    u16 unkA8; /* A8 */
    u16 unkAA; /* AA */
    u16 unkAC; /* AC */
    u16 unkAE; /* AE */
    u16 unkB0; /* B0 */
    u8 unkB2; /* B2 */
    u8 unkB3; /* B3 */
    u8 unkB4; /* B4 */
    u8 unkB5; /* B5 */
    u8 unkB6; /* B6 */
    u8 padB7[0x1];
    u16 shown[9]; /* B8: stats shown on the equipment screen */
    u8 padCA[0x2];
} MenuTables;

/* The gear record's bytes 0x55-0x57 (pad55, equipAttackScale, chargeRate)
 * as the stat rebuild clears them: one array indexed from the record, which
 * the separate members of GearRecord do not compile alike. */
typedef struct {
    u8 pad[0x55];
    u8 bytes55[3];
} GearRecordBytes55;

/* The record 801e76ec passes to 801e6ae8. */
typedef struct MenuViewSet {
    s32 time; /* 0: play time in frames */
    u16 valueA[3]; /* 4: per view */
    u16 valueB[3]; /* A: per view */
    u8 valueC[3]; /* 10: per view */
    u8 valueD[3]; /* 13: per view */
    u8 levels[3]; /* 16: per view: number shown in the first digit row */
    u8 unk19[3]; /* 19: per view: number of the second digit row */
    u8 images[4]; /* 1C: sheet image per view (+14e), ff none */
    u8 pad20[0x3];
    u8 unk23; /* 23: shown plus one as two digits */
} MenuViewSet;

/* A sub-record of a gear view (+5c8). */
typedef struct MenuGearValue {
    u8 pad0[0x24];
    u16 unk24; /* 24 */
    u8 pad26[0x2];
} MenuGearValue;

/* A gear view (801e4998). */
typedef struct MenuGearView {
    u8 pad0[0x5C8];
    MenuGearValue value; /* 5C8 */
} MenuGearView;

/* The record passed to 801e4998. */
typedef struct MenuGearViews {
    u8 pad0[0x4C];
    MenuGearView *views[4]; /* 4C */
} MenuGearViews;

/* A save information view (801e76ec). */
typedef struct MenuView {
    POLY_FT4 image[2]; /* 0: the entry's sheet image (801e6ae8) */
    POLY_FT4 frame[9][2]; /* 50: frame sprite list */
    POLY_FT4 levelDigits[6][2]; /* 320: digit sprite list of the set's level (+16), per buffer */
    POLY_FT4 aDigits[3][2]; /* 500: of value A (+4) */
    POLY_FT4 bDigits[3][2]; /* 5F0: of value B (+a) */
    POLY_FT4 cDigits[2][2]; /* 6E0: of value C (+10) */
    POLY_FT4 dDigits[2][2]; /* 780: of value D (+13) */
    POLY_FT4 name[2]; /* 820: the entry's name image */
    u8 levelCount; /* 870 */
    u8 unk871; /* 871 */
    u8 aCount; /* 872 */
    u8 bCount; /* 873 */
    u8 cCount; /* 874 */
    u8 dCount; /* 875 */
    u8 frameBuffer; /* 876 */
    u8 buffer; /* 877 */
    u8 shown; /* 878 */
    u8 nameBuffer; /* 879 */
    u8 frameCount; /* 87A */
    u8 pad87B[0x1];
} MenuView;

/* The 2dc0-byte block (*(state + 34c)). */
typedef struct MenuFileInfo {
    POLY_FT4 chars[64]; /* 0: 32 text character quads, pairs per buffer */
    POLY_FT4 cursor[2]; /* A00: 32x32 sprite, per buffer */
    POLY_G4 band[2]; /* A50: shaded band, per buffer */
    MenuView views[3]; /* A98 */
    POLY_FT4 colon0[4]; /* 240C: play time separators */
    POLY_FT4 colon1[4]; /* 24AC */
    POLY_FT4 timeDigits[7][2]; /* 254C: play time digits, per buffer */
    POLY_FT4 title[32]; /* 277C: save title characters, pairs per buffer */
    POLY_FT4 discLabel[2]; /* 2C7C */
    POLY_FT4 discMark[2]; /* 2CCC */
    POLY_FT4 discDigits[2][2]; /* 2D1C: two digits */
    u8 rebuilt; /* 2DBC */
    u8 pad2DBD[0x3];
} MenuBlock34C;

/* Five labels (*(state + 360)). */
typedef struct MenuEquipLabels {
    MenuLabel labels[5]; /* 0 */
    u16 stats[9]; /* 280: stats kept from the equipment screen */
    u8 pad292[0x2];
    u8 visible[5]; /* 294 */
    u8 count; /* 299 */
    u8 pad29A[0x2];
    u8 parts[3][5]; /* 29C: equipment kept from the character or gear */
} MenuLabels360;

/* The save/load screen block (*(state + 42c)). */
typedef struct MenuItemList {
    MenuLabel names[16]; /* 0 */
    MenuLabel values[16]; /* 800 */
    MenuLabel extra[3]; /* 1000 */
    u8 *unk1180; /* 1180 */
    u8 shown[16]; /* 1184 */
    u8 extraShown; /* 1194 */
    u8 pad1195[0x3];
} MenuBlock42C;

/* The file list block (*(state + 430)). */
typedef struct MenuFileList {
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
} MenuBlock430;

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
} MenuBlock438;

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

/* The detail panel block (*(state + 358)): a frame, a portrait, the layout
 * parts and the digit lists of the numbers shown, two quads per part (one per
 * draw buffer) with a screen quad each. */
typedef struct MenuDetail {
    POLY_FT4 frame[2];           /* 0 */
    POLY_FT4 portrait[2];        /* 50 */
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
    SVECTOR frameAt[4];          /* 1EA0 */
    SVECTOR portraitAt[4];       /* 1EC0 */
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
} MenuBlock358;

/* The equipment panel block (*(state + 35c)). */
typedef struct MenuEquipment {
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
} MenuBlock35C;

/* A position record passed to the panel builders (+28 base). */
typedef struct MenuAnchor {
    s32 parts[9]; /* 0: layout sprite positions */
    s32 frame; /* 24: frame sprite */
    s32 base; /* 28: level digits */
    s32 unk2C; /* 2C */
    s32 hp; /* 30 */
    s32 hpMax; /* 34 */
    s32 ep; /* 38 */
    s32 epMax; /* 3C */
    s32 label; /* 40: name label */
} MenuAnchor;

/* A field-menu block (*(state + 39c)): frame quads and part lists. */
typedef struct MenuFieldBlock {
    POLY_FT4 frameA[2]; /* 0 */
    POLY_FT4 frameB[2]; /* 50 */
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

/* A list screen block (*(state + 434)). */
typedef struct MenuEquipList {
    MenuLabel names[8]; /* 0 */
    MenuLabel values[8]; /* 400 */
    MenuLabel title; /* 800 */
    MenuLabel extra[3]; /* 880 */
    u8 *texts[4]; /* A00: description texts: weapons, accessories, gear parts, gear accessories */
    u8 shown[8]; /* A10 */
    u8 extraShown; /* A18 */
    u8 padA19[0x3];
} MenuBlock434;

/* The card access indicator (*(state + 44c), 7bc bytes). */
typedef struct MenuIndicator {
    POLY_F4 fills[2]; /* 0: per buffer */
    POLY_FT4 spriteA[24]; /* 30: sprite 160 */
    POLY_FT4 spriteB[24]; /* 3F0: sprite 161 */
    s32 unk7B0; /* 7B0 */
    s32 unk7B4; /* 7B4 */
    u8 buffer; /* 7B8: buffer it was built for */
    u8 pad7B9[0x3];
} MenuIndicator;

/* A file screen slot (*(state + 3a8 + 4 * slot)): its icon, the two
 * connector lines to the next slot and the cursor box, one per buffer. */
typedef struct MenuSlotImage {
    POLY_FT4 icon[2];  /* 0 */
    LINE_F3 lineA[2];  /* 50 */
    LINE_F3 lineB[2];  /* 80 */
    POLY_F4 box[2];    /* B0 */
    SVECTOR iconAt[4]; /* E0: also the cursor box */
    SVECTOR lineAAt[4]; /* 100 */
    SVECTOR lineBAt[4]; /* 120 */
    DR_MODE boxMode[2]; /* 140 */
} MenuSlotImage;

/* An image block (*(state + 3a8)[i], 158 bytes): a sprite and a green
 * frame drawn as two three-point lines. */
typedef struct MenuImage {
    POLY_FT4 polys[2]; /* 0 */
    LINE_F3 top[2]; /* 50: per buffer: top and right edges */
    LINE_F3 bottom[2]; /* 80: per buffer: left and bottom edges */
    POLY_F4 shade[2]; /* B0: per buffer: semi-transparent cover */
    SVECTOR shadeVerts[4]; /* E0 */
    SVECTOR topVerts[4]; /* 100 */
    SVECTOR bottomVerts[4]; /* 120 */
    DR_MODE modes[2]; /* 140 */
} MenuImage;

/* The sheet images of one command of a command window. */
typedef struct MenuCommandImages {
    s32 cursor; /* 0: cursor image (+d lit) */
    s32 label; /* 4 */
} MenuCommandImages;

/* The disc label read from sector 0 of the data track (file 17). */
typedef struct DiscLabel {
    u8 unk0[3];
    u8 disc; /* 3: '1' or '2' */
    s32 tag; /* 4: "_XEN" */
    u8 unk8[8];
} DiscLabel;

/* Word views of the game data blocks a save file copies whole. */
typedef struct SaveWords10 { s32 w[0x10 / 4]; } SaveWords10;
typedef struct SaveWords18 { s32 w[0x18 / 4]; } SaveWords18;
typedef struct SaveWords78 { s32 w[0x78 / 4]; } SaveWords78;
typedef struct SaveWordsA4 { s32 w[0xA4 / 4]; } SaveWordsA4;
typedef struct SaveWordsDC { s32 w[0xDC / 4]; } SaveWordsDC;
typedef struct SaveWords100 { s32 w[0x100 / 4]; } SaveWords100;
typedef struct SaveWords160 { s32 w[0x160 / 4]; } SaveWords160;
typedef struct SaveWords190 { s32 w[0x190 / 4]; } SaveWords190;

/* A gear as a save file keeps it (3c bytes). */
typedef struct SaveGear {
    SaveWords10 head;     /* 0: the gear record's first 0x10 bytes */
    SaveWords18 entries;  /* 10: its part entries (0x10) */
    s32 variants;         /* 28: fileVariant and spriteVariants (0x5c) */
    u32 hp;               /* 2C */
    u8 pad30[0x4];
    u16 fuel;             /* 34 */
    u8 pad36[0x2];
    u8 defense;           /* 38 */
    u8 field74;           /* 39 */
    u8 field75;           /* 3A */
    u8 pad3B[0x1];
} SaveGear;

/* The game data of a save file (after its header). */
typedef struct SaveData {
    s32 time; /* 0: play time in frames */
    u8 pad4[0x20];
    SaveWordsDC names; /* 24: game data 0 */
    SaveWords190 unk100; /* 100: game data dc */
    SaveWordsA4 chars[11]; /* 290: character records */
    SaveGear gears[20]; /* 99C */
    SaveWords78 unkE4C; /* E4C: game data 1648 */
    SaveWords160 records; /* EC4: game data 16c0 */
    SaveWords100 unk1024; /* 1024: game data 1820 */
    u8 unk1124[0xA38]; /* 1124: game data 1920 */
} SaveData;

/* structs: end */

/* The battle script variables (a resident common): the save keeps them
 * in its flag words. No shared header declares them; battle reads them
 * signed. */
extern u16 D_8005A3A0[16];
/* Gear special part durability per part id: two views into the game data,
 * from +0x2286 and from its flags word at +0x22b6, whose extents the shared
 * GameData leaves unsettled (gearAccessoryIds, flags). */
extern u8 D_8006F8BA[];
#define GEAR_PART_DURABILITY ((u8 *)&D_8006D634.flags)

extern s32 D_80059488;         /* play time in frames */

/* Overlay statics. */
extern u8 D_801E96A4;    /* the file screen saves (nonzero) or loads */
extern u8 D_801E96A5;
extern u8 D_801E977A;    /* inside a command */
extern u8 D_801E9784;    /* nonzero checks the reset combination */
extern u16 D_801E96A8[16]; /* single-bit masks */
extern u16 D_801E96C8[16]; /* single-bit masks */
extern u32 D_801E96E8[];   /* single-bit masks */
extern s32 D_801E9768[];
extern s32 D_801E9E64[];      /* label x offsets: field menu commands */
extern s32 D_801E9E84[];      /* title file screen commands */
extern s32 D_801E9EA0[];
extern MenuCommandImages D_801EA19C[]; /* field menu command cursor images */
extern MenuCommandImages D_801EA1D4[]; /* title file screen cursor images */
extern s32 D_801EA1EC[]; /* per command: four choices of cursor and label images */
extern u8 D_801EA528[];  /* field menu command labels */
extern u8 D_801EA524[];  /* label image layout */
extern u8 D_801EA530[];
extern u8 D_801EA534[];  /* party label layout */
extern s32 D_801E9E4C[3]; /* party label positions: x */
extern s32 D_801E9E58[3]; /* y */
extern u8 D_801EA53C[];  /* file screen command labels: save, then (6) load */
extern u8 D_801EA550[];  /* item target labels */
extern u8 D_801EA558[];
extern u8 D_801EA564[];  /* 801e1014 screen labels */
extern u8 D_801EA568[];  /* status screen labels: a character's page, then (6) a gear's */
extern u8 D_801EA548[];  /* save/load screen labels */
extern u8 D_801EA574[];  /* sound mode labels */
extern s32 D_801E9F88[];  /* sound mode label x offsets */
void func_801E9340(char *name, void *buffer, s32 size);
extern u8 D_801EA8FC;    /* the last choice was cancelled */
extern u8 D_801E9778;    /* a card changed during a choice */
extern u16 D_801EA610[96]; /* two-byte codes of the ASCII characters 0x20-0x7F */
/* Sheet positions of label images, x / 4 and y: per row pair, from entry 3
 * the portrait per slot (characters, then gears), from 6 the view names. */
extern s32 D_801EA578[19];
extern s32 D_801EA5C4[19];
extern s16 D_801E9894[32][2];    /* image block x */
extern s16 D_801E9914[32][2];    /* image block y */
extern s32 D_801EA34C[20]; /* field block part images, ffff none */
extern s32 D_801E9A78[20];
extern s32 D_801E9A00[]; /* highlight positions: x */
extern s32 D_801E9A2C[]; /* y */
extern s32 D_801E9A1C[]; /* choice highlight positions: x */
extern s32 D_801E9A48[]; /* y */
extern s32 D_801E9E94[]; /* choice label x offsets */
extern s32 D_801EA164[2]; /* party window sprite x */
extern s32 D_801EA16C[]; /* party window sprite y per row */
extern s32 D_801EA17C[]; /* portrait panel x per mode */
extern s32 D_801EA18C[]; /* portrait panel y per mode */
extern s32 D_801E9D78;    /* stat bar x offset */
extern s32 D_801E9D7C;    /* stat bar y offset */
extern s32 D_801E9D80;    /* stat digit x offset */
extern s32 D_801E9D84;    /* stat digit y offset */
extern s32 D_801E9DBC[8]; /* equipment screen: part cursor y by special * 4 + part */
extern s32 D_801E9D88[];
extern s32 D_801E9DDC[];  /* arts list cost x positions */
extern s32 D_801E9E14[];  /* arts list cost y positions */
extern u16 D_801E97F0[];
extern s32 D_801E9B18;   /* field block number offsets (x, y): +62 */
extern s32 D_801E9B1C;
extern s32 D_801E9B20;   /* +63 */
extern s32 D_801E9B24;
extern s32 D_801E9B28;   /* field block number offsets (x, y): hp */
extern s32 D_801E9B2C;
extern s32 D_801E9B30;   /* hp max */
extern s32 D_801E9B34;
extern s32 D_801E9B38;   /* ep */
extern s32 D_801E9B3C;
extern s32 D_801E9B40;   /* ep max */
extern s32 D_801E9B44;
extern s32 D_801E9B48;   /* exp */
extern s32 D_801E9B4C;
extern s32 D_801E9B50;   /* exp to next level */
extern s32 D_801E9B54;
extern s32 D_801E9B60[48]; /* detail panel part positions, 24 per layout: x */
extern s32 D_801E9C20[48]; /* y */
extern s32 D_801EA39C[48]; /* detail panel part sprites, 24 per layout, ffff none */
extern s32 D_801E9CE0;   /* detail panel number positions (x, y): level */
extern s32 D_801E9CE4;
extern s32 D_801E9CE8;   /* +63 */
extern s32 D_801E9CEC;
extern s32 D_801E9D10;   /* detail panel +3c */
extern s32 D_801E9D14;
extern s32 D_801E9D18;   /* detail panel +40 */
extern s32 D_801E9D1C;
extern s32 D_801E9D20;   /* detail panel exp */
extern s32 D_801E9D24;
extern s32 D_801E9D28;   /* detail panel exp to next level */
extern s32 D_801E9D2C;
extern s32 D_801E9D30;   /* detail panel +77..79 value */
extern s32 D_801E9D34;
extern s32 D_801E977C[2]; /* detail panel tab sprites */
extern s32 D_801E9D40[7]; /* stat name positions per row: x */
extern s32 D_801E9D5C[7]; /* y */
extern s32 D_801EA45C[];  /* stat name sprites, seven per start row */
extern s32 D_801EA4DC[]; /* status panel layout sprites, nine per layout, ffff none */
extern s32 D_801E9CF0;   /* hp */
extern s32 D_801E9CF4;
extern s32 D_801E9CF8;   /* hp max */
extern s32 D_801E9CFC;
extern s32 D_801E9D00;   /* ep */
extern s32 D_801E9D04;
extern s32 D_801E9D08;   /* ep max */
extern s32 D_801E9D0C;
extern s32 D_801E9D38;   /* detail panel portrait position: x */
extern s32 D_801E9D3C;   /* y */
extern s32 D_801E9B58;   /* field block portrait offset: x */
extern s32 D_801E9B5C;   /* y */
extern s32 D_801E9AC8[20];
extern s32 D_801E9A58[4]; /* marker positions */
extern s32 D_801E9A68[4];
extern u8 D_801E9778;    /* a card message is pending */
extern s32 D_801E9788[3]; /* arts screen window sizes per kind */
extern s32 D_801E9794[3];
extern s32 D_801E97A0[3];
extern u8 D_801E9785;    /* the target panels are allocated */
extern MenuAnchor D_801EA054[]; /* target panel layouts: x and y anchors */
extern MenuAnchor D_801EA098[];
extern MenuAnchor D_801EA0DC[];
extern MenuAnchor D_801EA120[];
extern s32 D_801EA004[];
extern s32 D_801EA010[];
extern s32 D_801E9994[21];    /* text character x per column */
extern s32 D_801E99E8[];      /* text character y per row */
extern s32 D_801E99F0;        /* cursor sprite x */
extern s32 D_801E99F8;        /* cursor sprite y */
extern s32 D_801E9A00[];      /* label x per row (mode 0) */
extern s32 D_801E9A2C[];      /* label y per row (mode 0) */
extern s32 D_801E9EC4[8];     /* label x (mode 1) */
extern u16 D_801E9EE4;        /* label y (mode 1) */
extern s32 D_801E9EE8[];      /* label x (modes 2, 5 from 8) */
extern s32 D_801E9F28[2];     /* label y per row (mode 2) */
extern s32 D_801E9F30[];      /* label y per row (mode 3) */
extern s32 D_801E9F68[2];     /* label x (mode 6) */
extern s32 D_801E9F70[];      /* label y (mode 6) */
extern u8 D_801E97AC[];       /* 801e1544 screen: five sheet images per row, ff none */
extern s32 D_801E9F48[];      /* status command label x offsets (page 0 and 6) */
extern u8 D_801E9808[20];      /* pilot character of each gear */
extern s32 D_801EA494[18];    /* view frame images, ffff none */
extern s32 D_801E9F98[9];     /* view frame x (first view) */
extern s32 D_801E9FBC[9];     /* view frame y */
extern s32 D_801E9FE0[9];     /* play time: x of the two separators and seven digits */
extern s32 D_801EA01C;         /* view digit row x */
extern s32 D_801EA020;         /* view digit row y */
extern s32 D_801EA04C;         /* save title x, y */
extern s32 D_801EA050;
extern s32 D_801EA02C;         /* value A digits x, y */
extern s32 D_801EA030;
extern s32 D_801EA034;         /* value B digits x, y */
extern s32 D_801EA038;
extern s32 D_801EA03C;         /* value C digits x, y */
extern s32 D_801EA040;
extern s32 D_801EA044;         /* value D digits x, y */
extern s32 D_801EA048;
extern s32 D_801EA900[2];     /* per port */
extern u8 D_801E9779;    /* frames between card checks */

/* Resident services. */

void func_8002A428(s32 arg0);
void func_8001BD40(s32 arg0, s32 arg1);
void func_800263E4(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale, s32 flipX, s32 flipY);
s32 func_8002675C(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale);
void func_80033698(s32 x, s32 y);
void func_80039DB8(s32 id, s32 sound); /* play a sound effect */
u32 func_801E1418(u8 slot, u8 row);
void func_801E3A80(MenuTables *tables, u8 id);
void func_801E433C(MenuTables *tables, u8 gear);
u8 func_801E4928(u8 gear);
void func_801E8B4C(u8 offset);
void func_801E86C8(u8 offset);
void func_801E5058(void);
void func_801E5178(void);
void func_801E4754(MenuTables *tables, u8 gear);
void func_801E8EAC(POLY_FT4 *poly, u8 mode);
void func_801E920C(POLY_FT4 *poly, u16 x, u16 y, u8 u, u8 v, u16 w, u16 h);
void func_801E927C(POLY_FT4 *poly);

u8 *func_800337E8(u8 id);
u8 *func_80033848(u8 id);  /* weapon name */
u8 *func_80033A2C(u8 id);  /* gear accessory name */
u8 *func_80033A5C(u8 id);  /* gear part name */
u8 *func_80033728(u8 *table, s32 index); /* message of a table */
u8 *func_80033818(u8 item);  /* item name text */
u8 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line; its width */

/* Overlay functions. */
u8 func_801C531C(u8 offset);
void func_801C55A0(void);
void func_801C57A4(void);
void func_801C58EC(void);
void func_801C6400(void);
void func_801C65F4(void);
void func_801C6AA0(MenuState *state);
u16 func_801C865C(u16 flags, u8 bit);
u32 func_801C8678(u32 flags, u8 bit);
void func_801C6D4C(void);
void func_801C6D5C(void);
void func_801C6D90(void);
void func_801C6E0C(void);
void func_801C6E68(void);
void func_801C6F70(void);
void func_801C72BC(u8 arg0);
void func_801C7B0C(void);
void func_801CB184(void);
u8 func_801CB304(void);
u8 func_801CBD90(u8 arg);
u8 func_801CC6D8(void);
u8 func_801CD2AC(void);
u8 func_801C93A8(void);
void func_801C7BF4(void);
void func_801C7D78(void);
void func_801C7F34(u32 frames);
void func_801C80B8(u32 value);
void func_801C851C(SVECTOR *v, u16 x, u16 y, u16 w, u16 h);
u8 func_801C881C(void);
s32 func_801C891C(s32 channel);
u8 func_801C8A10(u8 port);
u8 func_801C8D78(u8 port);

/* libapi directory entry (firstfile/nextfile). */
struct DIRENTRY {
    char name[20];
    s32 attr;
    s32 size;
    struct DIRENTRY *next;
    s32 head;
    char system[4];
};
void func_801D9B08(void);
void func_801C9EF4(s32 mode, s32 slot);
void func_801CADB0(void);
void func_801E4998(MenuGearViews *views, u8 gear);

/* The menu data archive (file 2 of directory 10h): packed files by index. */
typedef struct MenuDataArchive {
    s32 count; /* 0 */
    void *items; /* 4 */
    void *weapons; /* 8 */
    void *accessories; /* C */
    void *effects[11]; /* 10: per character */
    void *unk3C; /* 3C */
    void *unk40; /* 40 */
    void *engines; /* 44 */
    void *frames; /* 48 */
    void *parts; /* 4C */
    void *unk50; /* 50 */
    void *unk54; /* 54 */
    void *unk58; /* 58 */
    void *gears[20]; /* 5C: per gear */
    void *unkAC; /* AC */
    void *unkB0; /* B0 */
    void *padB4[8];
    void *unkD4[4]; /* D4 */
} MenuDataArchive;
void func_801CB28C(s32 *save);
void func_801C8CA4(u8 port);
u8 func_801C93A8(void);
s32 func_801C9BCC(s32 mode);
s32 func_801C9D34(s32 mode);
s32 func_801CA750(s32 mode);
void func_801CAE08(u8 mode);
void func_801CA1D4(s32 mode, s32 slot);
void func_801CA480(s32 mode, s32 slot);
void func_801CA5F0(s32 mode, s32 slot);
void func_801E781C(s32 index, u8 rebuild);
/* The 31 names at the start of the game data (encoded in the save). */
#define GAME_NAMES ((u8 *)&D_8006D634)
s32 func_80033B34(u8 *codes, u8 *text, s32 count); /* decode a name */

/* The summary at the start of a save payload (801cba4c). */
typedef struct MenuSavePayload {
    s32 time; /* 0: play time */
    u16 hp[3]; /* 4: per party slot */
    u16 hpMax[3]; /* A */
    u8 ep[3]; /* 10 */
    u8 epMax[3]; /* 13 */
    u8 unk16[3]; /* 16 */
    u8 unk19[3]; /* 19 */
    u8 ids[3]; /* 1C: character, ff empty */
    u8 unk1F; /* 1F */
    u8 pad20[0x3];
    u8 digit; /* 23: file digit */
} MenuSavePayload;
/* Block 2 of state + 444 holds the card access indicator. */
void func_801E78C8(s32 file);
void func_801C9270(s32 port);
extern s32 D_801E981C[];          /* card slot (port * 16 + n) of each cursor position */
void func_801C8BEC(void);
void func_801C8EE8(void);
void func_801C8574(s32 sound);
void func_801C8694(u8 arg0);
u8 func_801CAA38(u8 arg);
s32 func_801CACF8(u8 message, u8 confirm, u8 arg);
void func_801CE2B4(s32 count, POLY_FT4 *polys, s32 first);
void func_801CD81C(MenuStatusPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e);
void func_801CDB1C(MenuStatusPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y);
void func_801CDC6C(MenuStatusPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e);
void func_801CE540(void);
void func_801CE660(void);
void func_801CEB5C(void);
void func_801CEBB4(void);
void func_801CE464(void);
void func_801CE0CC(MenuStatusPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e);
void func_801CE198(s32 count, SVECTOR *verts, POLY_FT4 *polys, s32 first);
void func_801CE338(void);
void func_801CE3C8(void);
void func_801CE860(void);
void func_801CEC40(void);
void func_801CF308(void);
void func_801CF37C(void);
void func_801CF5E4(s32 first, s32 firstSlot, s32 end);
void func_801CF8D8(void);
void func_801CFB48(void);
void func_801CFF64(void);
void func_801D01D0(void);
void func_801D02D8(void);
void func_801D0C78(void);
void func_801D09F0(s32 index, u8 full);
void func_801D11F0(void);
void func_801D0D90(void);
void func_801D0E20(void);
void func_801D0E38(void);
void func_801D0EBC(void);
void func_801D0ED4(void);
void func_801D0F54(void);
void func_801D0FD4(void);
void func_801D1030(void);
void func_801D10DC(void);
void func_801D1160(void);
void func_801D12D4(MenuStatusPanel *panel, u8 extra);
void func_801D13F8(void);
void func_801D1464(void);
void func_801D14B0(void);
void func_801D14FC(void);
void func_801D1640(void);
void func_801D17C4(void);
void func_801D1914(void);
void func_801D1AAC(void);
void func_801D1B20(void);
void func_801D1BE8(void);
void func_801D1C48(void);
void func_801D1258(void);
void func_801D1CA0(void);
void func_801D1D40(void);
void func_801D1E80(void);
void func_801D22C4(void);
void func_801C8164(POLY_G4 *poly, u8 r, u8 g, u8 b);
void func_801D22F4(u8 arg0);
void func_801D2484(void);
void func_801D2968(void);
void func_801D249C(u8 show);
void func_801D25E4(void);
void func_801D2D38(void);
void func_801D2F4C(u8 message);
void func_801D3B00(void);
void func_801D397C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 grow, u8 arg6, s32 arg7, u8 arg8);
void func_801D3C4C(u8 slot, u16 x, u16 y, s32 unused, u16 h);
void func_801D3DB0(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801D3FF8(u8 index, u16 x, u16 y, u16 w);
void func_801D433C(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801D4688(u8 index, u16 x, u16 y, u16 h);
void func_801D49D0(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801D4D1C(u8 image, u16 x, u16 y, u16 w, u16 h, u8 arg5, s32 arg6, u8 arg7);
void func_801D4EA0(u8 slot);
void func_801D4F2C(u8 index, u8 mode, s32 x, s32 y);
void func_801D50EC(u8 index, s32 x, s32 y);
void func_801D51EC(u8 index, u8 mode, s32 x, s32 y);
void func_801D53D0(u8 index, u8 mode, s32 x, s32 y);
void func_801D55B4(u8 index, u8 mode, s32 x, s32 y);
void func_801D5794(u8 index, u8 mode, s32 x, s32 y);
void func_801D5A50(u8 index, u8 mode);
void func_801D5ED4(u8 slot, u8 mode);
void func_801D6194(u8 mode);
void func_801D6338(u8 slot, u8 mode);
void func_801D680C(u8 slot, u8 mode);
void func_801D6CF4(u8 slot, u8 mode);
void func_801D7154(u8 slot, u8 mode);
void func_801D74EC(u8 slot, u8 mode);
void func_801D7884(u8 slot, u8 mode);
void func_801D7C3C(u8 slot, u8 mode);
void func_801D7F50(s32 x, s32 y, u8 mode);
void func_801D8644(s32 scale, s32 x, s32 y, u8 compare, u8 first);
void func_801D8DE4(u8 slot, u8 lower, u8 arg2, u8 mode);
void func_801D8EA4(u8 slot, u8 mode, u8 kept, u8 gear);
void func_801D5BA4(s32 x, s32 y);
void func_801D5CF8(s32 x, s32 y);
void func_801D32B4(void);
u8 func_801D9808(void);
void func_801E86C8(u8 row);
void func_801E8B4C(u8 row);
u8 func_801C93A8(void);
u16 func_801C865C(u16 flags, u8 bit);
void func_801DA5BC(s32 row);
void func_801DC3D8(u8 slot, u8 kind);
void func_801DCE60(u8 slot, u8 row, u8 kind);
void func_801DD790(u8 slot, s32 row, u8 kind);
void func_801E35BC();
void func_801DA9A8(s32 entry, s32 row);
void func_801DB39C(u8 mode);
u8 func_801DB920(s32 row, s32 entry);
void func_801DB5E4(u8 mode);
void func_801D9E3C(void);
void func_801E5ACC(void);
void func_801E6450(void);
u8 func_801D9F98(u8 mode, u8 save);
void func_801D1EB0(void);
void func_801D29A8(u8 arg0, u8 arg1);
void func_801D3444(void);
void func_801D3488(u8 row, u8 fighters);
void func_801D3674(void);
void func_801DA518(void);
u8 func_801DBE54(void);
void func_801DC2CC(u8 kind);
void func_801DE36C(void);
void func_801DE400(void);
void func_801DDF24(u8 slot, u8 zoom, u8 kind);
u8 func_801DE29C(u8 slot, u8 arg1);
s32 func_801DE5CC(u8 slot, s32 top, s32 part, u8 special, u8 gear);
s32 func_801DF0D4(u8 slot, u8 part, u8 special, u8 gear);
void func_801DFF5C(s32 part, s32 row, s32 top, u8 special, u8 gear, u8 current, u8 slot);
void func_801E05D0(u8 slot, u8 fade, u8 gear);
u8 func_801E0F78(u8 slot, u8 arg1);
void func_801E2368(void);
u8 func_801E23CC(void);
void func_801E2B80(void);
u8 func_801E2BE4(void);
void func_801E3088(u8 command);
void func_801E3C2C(MenuTables *tables, u8 gear);
void func_801E3ECC(MenuTables *tables, u8 gear);
void func_801E41C0(MenuTables *tables, u8 gear);
void func_801E42AC(MenuTables *tables, u8 gear);
void func_801E4258(MenuTables *tables, u8 gear);
void func_801E4D10(SaveData *save, MenuTables *tables);
void func_801E53CC(u8 index);
void func_801E56E8(s32 index);
void func_801E8DA8(u8 image, u8 row);
void func_801E927C(POLY_FT4 *poly);
void func_801E91C4(POLY_FT4 *poly);
void func_801E5B3C(void);
void func_801E61B0(void);
void func_801E6AE8(u8 index, MenuViewSet *set);
void func_801E6B70(u8 index, MenuViewSet *set);
void func_801E6CFC(u8 index, MenuViewSet *set);
void func_801E6F5C(u8 index, MenuViewSet *set);
void func_801E71B4(u8 index, MenuViewSet *set, s32 file);
void func_801E68AC(MenuViewSet *set);
void func_801E733C(void);
void func_801E649C(void);
void func_801E64E0(void);
void func_801E5924(s32 index);
void func_801E5B88(void);
void func_801E5E4C(void);
void func_801E7E68(MenuLabel *labels, u8 *layout, s32 first, s32 count);
void func_801E8018(u8 count, MenuLabel *labels, u8 *table, u8 *flags);
void func_801E8044(u8 count, u8 *flags);
void func_801E8070(u8 count, MenuLabel *labels, u8 *table, s32 *offsets, u8 *flags, u8 selected, u8 row,
                   u8 mode);
void func_801E8474(s32 count, MenuCommandImages *images);
void func_801E7C50(MenuLabel *label, s32 index, s32 first, u8 mode);
void func_801E8EAC(POLY_FT4 *poly, u8 mode);
void func_801E8F60(u8 index, u8 dim);
void func_801E92CC(void);
s32 func_801E93A0(s32 disc);
void func_801E6668(s32 index);
void func_801E76EC(s32 index);
void func_801E8978(u8 count, u8 cursor, MenuCommandImages *images);
u16 func_801C8640(u16 flags, u8 bit);
void func_801D1EE0(s32 index, u8 outline);
void func_801D261C(void);
void func_801D2EC0(u8 slot, u8 mode);
void func_801D3344(s32 x, s32 y, s32 h);
void func_801D36E0(MenuLabel *label, u8 slot, u8 gear, u8 mode);
s32 func_801D9704(s32 slot, u8 dir, u8 readyOnly);
void func_801DA4A8(void);
void func_801DB02C(u8 index);
void func_801DB0A8(s32 entry, s32 row, u8 kind, u8 index);
void func_801DB340(u8 index);
void func_801DBD4C(s32 a, s32 b);
void func_801DBDB4(void);
void func_801E4A28(SaveData *save);
void func_801CBA4C(MenuSavePayload *payload, u8 port, u8 digit);

#endif
