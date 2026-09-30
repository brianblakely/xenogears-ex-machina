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

/*
 * Menu overlay (Disc 1 slot 39, loaded at 801c5000): the menu mode's state.
 * The resident keeps a pointer to it at D_800625A0; the overlay allocates the
 * state's sub-blocks on entry and frees them on exit. Field names follow what
 * the overlay does with them; unknown bytes stay padding.
 */

/* structs: begin */
/* Party block (*(state + 33c)): per-part redraw flags, the label set and the party ids. */
typedef struct MenuParty {
    u8 fieldShown[3]; /* 0 */
    u8 redraw3; /* 3 */
    u8 redraw4; /* 4 */
    u8 redraw5; /* 5 */
    u8 redraw6; /* 6 */
    u8 redraw7; /* 7 */
    u8 unk8; /* 8 */
    u8 redraw9; /* 9 */
    u8 redrawA; /* A */
    u8 unkB; /* B */
    u8 labels[8]; /* C: shown flags of the command labels */
    u8 unk14[6]; /* 14 */
    u8 unk1A[6]; /* 1A */
    u8 unk20[7]; /* 20: per portrait: shown */
    u8 unk27[7]; /* 27: per portrait mark: shown */
    u8 unk2E; /* 2E */
    u8 unk2F; /* 2F */
    u8 ids[3]; /* 30 */
    u8 messageShown; /* 33: a card message is shown */
    u8 unk34[4]; /* 34 */
    u8 unk38[8]; /* 38: character ids of the party slots, ff empty */
    u8 unk40[6]; /* 40 */
    u8 unk46; /* 46 */
    u8 pad47[0x1];
    u8 unk48; /* 48 */
    u8 unk49; /* 49 */
    u8 unk4A; /* 4A */
    u8 unk4B; /* 4B */
    u8 unk4C; /* 4C */
    u8 unk4D; /* 4D */
    u8 unk4E; /* 4E */
    u8 pad4F[0x1];
    u8 unk50[3]; /* 50: per image block (+444): shown; block 2 (the card access indicator): 1 shown, 2 closing */
    u8 unk53; /* 53 */
    u8 unk54[6]; /* 54 */
    u8 pad5A[0x2];
    u8 unk5C[4]; /* 5C */
    u8 ready[3]; /* 60 */
    u8 pad63[0x4];
    u8 unk67; /* 67 */
    u8 cardMode; /* 68: the file screen is in card mode */
    u8 pad69[0x3];
} MenuParty;

/* Screen images (*(state + 350)). */
typedef struct MenuImages {
    POLY_FT4 packets[56]; /* 0 */
    POLY_FT4 packets2[56]; /* 8C0 */
    RECT copy; /* 1180: the screen area copied into the other buffer each frame */
    s32 count; /* 1188 */
    s32 count2; /* 118C */
    u8 buffer; /* 1190 */
    u8 buffer2; /* 1191 */
    u8 captured; /* 1192: images dimmed (inside a command) */
    u8 refresh; /* 1193: dimming last applied */
} MenuImages;

/* Shared primitive block (*(state + 348)). */
typedef struct MenuPrims {
    POLY_FT4 polys[2]; /* 0 */
    POLY_G4 box[2]; /* 50: highlight background */
    POLY_F4 fills[2]; /* 98: per buffer: full-screen fade */
    LINE_F3 edgeA[2]; /* C8: highlight outline, top and right */
    LINE_F3 edgeB[2]; /* F8: highlight outline, left and bottom */
    DR_MODE modes0[2]; /* 128 */
    DR_MODE modes[2]; /* 140: per buffer */
    u8 frame; /* 158 */
    u8 mode; /* 159 */
    u8 pad15A[0x1];
    u8 shade; /* 15B */
} MenuPrims;

/* One file entry of a card listing. */
typedef struct MenuCardFile {
    s32 frames[6]; /* 0: icon animation: image y of each step */
    char name[21]; /* 18: directory entry name */
    u8 pad2D[0x2B];
    u8 state; /* 58 */
    u8 pad59[0x3];
} MenuCardFile;

/* A memory card's header on the file screen: its label and name sprites. */
typedef struct MenuCardHeader {
    POLY_FT4 label[2]; /* 0 */
    POLY_FT4 name[4];  /* 50 */
} MenuCardHeader;

/* The second half of a listed file's header block (+100). */
typedef struct MenuSaveInfo {
    u8 pad0[0x24];
    u8 names[4][0x14]; /* 24: names of the sheet entries (two-byte text) */
} MenuSaveInfo;

/* Memory-card state (*(state + 32c)). */
typedef struct MenuCard {
    MenuCardFile files[32]; /* 0 */
    TIM_IMAGE icon; /* B80: the file icon TIM */
    u8 headers[32][0x200]; /* B94: first block of each listed file */
    u8 saveMagic[2]; /* 4B94: header of a written file: "SC" */
    u8 saveIconFlag; /* 4B96 */
    u8 saveBlocks; /* 4B97 */
    char saveTitle[0x5C]; /* 4B98: Shift-JIS */
    u8 savePalette[0x20]; /* 4BF4 */
    u8 saveIcon[0x80]; /* 4C14 */
    u8 pad4C94[0x100];
    MenuCardHeader cardHeaders[2]; /* 4D94: per port */
    s32 result[2]; /* 4F74: per port: last card check result */
    s32 cursor; /* 4F7C: file cursor over both ports (port 2 from 15) */
    s32 unk4F80; /* 4F80 */
    s32 fileCount; /* 4F84 */
    u8 scanned[2]; /* 4F88: per port */
    u8 unk4F8A[2]; /* 4F8A */
    u8 unk4F8C[2]; /* 4F8C */
    u8 ours[32]; /* 4F8E: per listed file: carries this game's prefix */
    u8 fileSlots[32]; /* 4FAE */
    char prefix[12]; /* 4FCE: this game's file name prefix */
    u8 pad4FDA[0xA];
    u8 present[2]; /* 4FE4: per port: card present */
    u8 mode; /* 4FE6 */
    u8 busy; /* 4FE7 */
    u8 presentShown[2]; /* 4FE8 */
    u8 pad4FEA[0x2];
    s32 events[4]; /* 4FEC: card event descriptors */
    char title[30]; /* 4FFC: save title line of the text file */
    u8 unk501A; /* 501A */
    u8 unk501B; /* 501B */
    u32 otherPrefix[4]; /* 501C: the other file name prefix (13 bytes) */
    u8 pad502C[0x8];
} MenuCard;

/* One of the two display buffers. */
typedef struct MenuBuffer {
    u8 draw[0x5c]; /* 0: DRAWENV */
    u8 disp[0x14]; /* 5C: DISPENV */
    u32 ot[16]; /* 70: reverse ordering table */
    u8 padB0[0x4];
} MenuBuffer;

/* The menu's sound effect bank (resident). */
typedef struct MenuSoundBank {
    u8 pad0[0x14];
    u16 id; /* 14 */
} MenuSoundBank;

/* A point moving along a line (801c81e0, 801c8324). */
typedef struct MenuMover {
    s32 x0; /* 0 */
    s32 x1; /* 4 */
    s32 y0; /* 8 */
    s32 y1; /* C */
    s32 stepX; /* 10: 8.8 per step */
    s32 stepY; /* 14 */
    s32 accX; /* 18: 8.8 distance moved */
    s32 accY; /* 1C */
    u8 negX; /* 20: moving towards smaller x */
    u8 negY; /* 21 */
    u8 speed; /* 22: steps per frame */
    u8 done; /* 23 */
} MenuMover;

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

/* A character record of the game data (D_8006D8A0; gears follow from 11). */
typedef struct CharRecord {
    u16 weaponValue; /* 0: from the weapon record (+8) */
    u8 weaponA; /* 2 */
    u8 weaponB; /* 3: 100 adds the value to +8e */
    u8 level; /* 4: from the weapon record (+c) */
    u8 pad5[0x13];
    u16 weapon2Value; /* 18: kind 4: the second weapon's values */
    u8 weapon2A; /* 1A */
    u8 weapon2B; /* 1B */
    u8 unk1C; /* 1C */
    u8 pad1D[0xB];
    u8 bonus[8]; /* 28: equipment bonuses of the base values (58) */
    u8 unk30; /* 30: accessory kinds 8, 9 */
    u8 unk31; /* 31 */
    u16 unk32; /* 32: accessory kind 5 bits */
    u8 pad34[0x8];
    u32 unk3C; /* 3C */
    u32 unk40; /* 40 */
    u32 exp; /* 44 */
    u32 expNext; /* 48 */
    u16 hp; /* 4C */
    u16 hpMax; /* 4E */
    u16 ep; /* 50 */
    u16 epMax; /* 52 */
    u8 pad54[0x2];
    u8 unk56; /* 56 */
    u8 pad57[0x1];
    u8 unk58; /* 58 */
    u8 unk59; /* 59 */
    u8 unk5A; /* 5A */
    u8 unk5B; /* 5B */
    u8 unk5C; /* 5C */
    u8 pad5D[0x1];
    u8 unk5E; /* 5E */
    u8 unk5F; /* 5F */
    u8 pad60[0x2];
    u8 unk62; /* 62 */
    u8 unk63; /* 63 */
    u8 pad64[0x6];
    u8 weapons[5]; /* 6A: [0] the weapon */
    u8 specials[5]; /* 6F: special parts; [0] inventory list 1 entry (801e0434), kind 4: first weapon; [3] kind 4: second weapon */
    u8 accessories[5]; /* 74: accessory records */
    u8 unk79; /* 79 */
    u8 pad7A[0x4];
    u16 unk7E; /* 7E: accessory kind 1 bits */
    u8 pad80[0x2];
    u16 unk82; /* 82: kind 2 */
    u8 pad84[0x2];
    u16 unk86; /* 86: kind 3 */
    u8 pad88[0x2];
    u16 unk8A; /* 8A: kind 4 */
    u8 pad8C[0x2];
    u16 unk8E; /* 8E: kind 7 */
    u16 unk90[7]; /* 90: progress values (801e1418) */
    u8 pad9E[0x2];
    u8 gear; /* A0: gear record (+11), ff none */
    u8 unkA1; /* A1: accessory kind 10 */
    u8 padA2[0x2];
} CharRecord;

/* A weapon slot of a gear record. */
typedef struct GearSlot {
    u16 value; /* 0 */
    u8 unk2; /* 2 */
    u8 unk3; /* 3 */
    u8 unk4; /* 4 */
    u8 pad5[0x3];
} GearSlot;

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
    u8 pad4[0x4];
    u16 groups; /* 8 */
    u8 padA[0x12];
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

/* A gear record of the game data (D_8006DFAC). */
typedef struct GearRecord {
    u8 pad0[0x2];
    u8 engine; /* 2: record of table +8 */
    u8 unk3; /* 3: record of table +c */
    u8 unk4[0x4]; /* 4: part ids; [0] inventory list 4 entry (801e0434), first slot weapon of gears 5, d */
    u8 frame; /* 8 */
    u8 unk9[3]; /* 9: part ids (801df5d0 keeps four from here); copied to the gear's other form (801e3ecc) */
    u8 unkC[4]; /* C: [0] weapon record of table +18 */
    GearSlot slots[3]; /* 10 */
    u8 pad28[0x10];
    u16 unk38; /* 38 */
    u16 unk3A; /* 3A */
    u8 unk3C; /* 3C */
    u8 unk3D; /* 3D */
    u8 unk3E; /* 3E */
    u8 unk3F; /* 3F */
    u16 unk40; /* 40 */
    u16 unk42; /* 42 */
    u16 unk44; /* 44 */
    u8 pad46[0x4];
    u8 unk4A; /* 4A */
    u8 pad4B[0x2];
    u8 unk4D; /* 4D */
    u8 pad4E[0x6];
    u8 unk54; /* 54 */
    u8 pad55[0x1];
    u8 unk56; /* 56 */
    u8 pad57[0x5];
    u8 attrs[4]; /* 5C */
    u32 unk60; /* 60 */
    u32 unk64; /* 64 */
    u16 unk68; /* 68 */
    u16 unk6A; /* 6A */
    u8 pad6C[0x4];
    u16 unk70; /* 70 */
    u16 unk72; /* 72 */
    u8 unk74; /* 74 */
    u8 unk75; /* 75 */
    u8 pad76[0x10];
    u16 unk86; /* 86 */
    u8 pad88[0x10];
    u8 unk98; /* 98 */
    u8 unk99; /* 99 */
    u8 pad9A[0x2];
    u8 unk9C; /* 9C */
    u8 unk9D; /* 9D */
    u8 unk9E; /* 9E */
    u8 unk9F; /* 9F */
    u8 padA0[0x4];
} GearRecord;

/* A laid-out label: its quads and sprite list. */
typedef struct MenuLabelSlot {
    POLY_FT4 polys[2]; /* 0 */
    SVECTOR verts[4]; /* 50 */
    RECT rect; /* 70: VRAM area of the rendered text */
    u8 *pixels; /* 78 */
    u8 palette; /* 7C: 0 the plain palette (D_800595D4), else D_80059414 */
    u8 count; /* 7D: the quad shown */
    u8 width; /* 7E */
    u8 visible; /* 7F */
} MenuLabelSlot;

/* Two sprite lists (*(state + 354)). */
typedef struct MenuSpriteLists {
    POLY_FT4 first[32]; /* 0 */
    POLY_FT4 second[96]; /* 500 */
    s32 firstCount; /* 1400 */
    s32 secondCount; /* 1404 */
    u8 firstStart; /* 1408 */
    u8 secondStart; /* 1409 */
    u8 pad140A[0x2];
} MenuSpriteLists;

/* An image block (*(state + 444)): quads and sprite list. */
typedef struct MenuImageBlock {
    POLY_FT4 polys[2]; /* 0 */
    SVECTOR verts[4]; /* 50 */
    s32 frame; /* 70: animation frame, counts down from 4 */
    u8 timer; /* 74: frames shown of the current animation frame */
    u8 count; /* 75 */
    u8 pad76[0x2];
} MenuImageBlock;

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
typedef struct MenuBlock34C {
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
typedef struct MenuLabels360 {
    MenuLabelSlot labels[5]; /* 0 */
    u16 stats[9]; /* 280: stats kept from the equipment screen */
    u8 pad292[0x2];
    u8 visible[5]; /* 294 */
    u8 count; /* 299 */
    u8 pad29A[0x2];
    u8 parts[3][5]; /* 29C: equipment kept from the character or gear */
} MenuLabels360;

/* The marker block (*(state + 428)): two quads per marker. */
typedef struct MenuMarkers {
    POLY_FT4 polys[8]; /* 0 */
    u8 visible[4]; /* 140 */
    u8 unk144[2]; /* 144 */
    u8 pad146[0x2];
    u8 current[4]; /* 148 */
} MenuMarkers;

/* The save/load screen block (*(state + 42c)). */
typedef struct MenuBlock42C {
    MenuLabelSlot names[16]; /* 0 */
    MenuLabelSlot values[16]; /* 800 */
    MenuLabelSlot extra[3]; /* 1000 */
    u8 *unk1180; /* 1180 */
    u8 shown[16]; /* 1184 */
    u8 extraShown; /* 1194 */
    u8 pad1195[0x3];
} MenuBlock42C;

/* The file list block (*(state + 430)). */
typedef struct MenuBlock430 {
    MenuLabelSlot names[14]; /* 0 */
    MenuLabelSlot values[14]; /* 700 */
    MenuLabelSlot headA; /* E00 */
    MenuLabelSlot headB; /* E80 */
    MenuLabelSlot extra[2]; /* F00 */
    MenuLabelSlot footer; /* 1000 */
    u8 *texts; /* 1080: description texts */
    u8 shown[14]; /* 1084 */
    u8 extraShown; /* 1092 */
    u8 pad1093[0x1];
} MenuBlock430;

/* The status list block (*(state + 438)). */
typedef struct MenuBlock438 {
    MenuLabelSlot names[13]; /* 0 */
    MenuLabelSlot values[13]; /* 680 */
    MenuLabelSlot title; /* D00 */
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
typedef struct MenuBlock358 {
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
typedef struct MenuBlock35C {
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

/* A panel built by 801ce0cc: frame quads and part lists (two quads per entry). */
typedef struct MenuPanel {
    POLY_FT4 list0[18]; /* 0 */
    POLY_FT4 extra[10]; /* 2D0 */
    POLY_FT4 frameA[2]; /* 460 */
    POLY_FT4 frameB[2]; /* 4B0 */
    POLY_FT4 list1[6]; /* 500 */
    POLY_FT4 list2[6]; /* 5F0 */
    POLY_FT4 list3[10]; /* 6E0 */
    POLY_FT4 list4[10]; /* 870 */
    POLY_FT4 list5[6]; /* A00 */
    POLY_FT4 list6[6]; /* AF0 */
    u8 counts[6]; /* BE0 */
    u8 buffer; /* BE6 */
    u8 shown; /* BE7 */
    u8 count0; /* BE8 */
    u8 padBE9[0x3];
} MenuPanel;

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

/* A one-quad sprite (*(state + 43c)). */
typedef struct MenuBlock43C {
    POLY_FT4 polys[2]; /* 0 */
    SVECTOR verts[4]; /* 50 */
    u8 buffer; /* 70 */
    u8 pad71[0x3];
} MenuBlock43C;

/* A four-quad sprite (*(state + 440)). */
typedef struct MenuBlock440 {
    POLY_FT4 polys[8]; /* 0 */
    SVECTOR verts[16]; /* 140 */
    u8 buffer; /* 1C0 */
    u8 pad1C1[0x3];
} MenuBlock440;

/* A portrait mark growing to its size (*(state + 380)). */
typedef struct MenuMark {
    u16 x; /* 0 */
    u16 y; /* 2 */
    u16 w; /* 4 */
    u16 h; /* 6 */
    u16 curW; /* 8 */
    u16 curH; /* A */
    s32 unkC; /* C */
    u8 image; /* 10 */
    u8 done; /* 11 */
    u8 unk12; /* 12 */
    u8 unk13; /* 13 */
    u8 pad14[0x4];
} MenuMark;

/* A list screen block (*(state + 434)). */
typedef struct MenuBlock434 {
    MenuLabelSlot names[8]; /* 0 */
    MenuLabelSlot values[8]; /* 400 */
    MenuLabelSlot title; /* 800 */
    MenuLabelSlot extra[3]; /* 880 */
    u8 *texts[4]; /* A00: description texts: weapons, accessories, gear parts, gear accessories */
    u8 shown[8]; /* A10 */
    u8 extraShown; /* A18 */
    u8 padA19[0x3];
} MenuBlock434;

/* A portrait window (*(state + 364), 720 bytes): a 3D panel of corner, edge
 * and frame sprites around a translucent fill, two quads per piece (one per
 * draw buffer); the same layout as ovl2600's panels. */
typedef struct MenuPortrait {
    POLY_FT4 corner[8];      /* 0: corner sprite parts */
    POLY_FT4 edge[4][4];     /* 140: top, bottom, left and right edges, two pieces each */
    POLY_FT4 frameSide[2];   /* 3C0: sprite 106 */
    POLY_FT4 frameEnds[4];   /* 410: sprite 105 at the top, flipped at the bottom */
    POLY_G4 fill[2];         /* 4B0 */
    DR_MODE fillMode[2];     /* 4F8 */
    SVECTOR cornerAt[16];    /* 510: four corner quads */
    SVECTOR edgeAt[4][2][4]; /* 590: two quads per edge */
    SVECTOR fillAt[4];       /* 690 */
    SVECTOR sideAt[4];       /* 6B0 */
    SVECTOR endsAt[8];       /* 6D0: top and bottom quads */
    s32 cornerParts;         /* 710: corner parts built */
    s32 style;               /* 714: 0 draws under an identity rotation */
    s32 depth;               /* 718: ordering table depth */
    u8 buffer;               /* 71C: buffer it was laid out for */
    u8 framed;               /* 71D: frame sprites built */
} MenuPortrait;

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

/* A 20-byte game data record at D_8006ECF4 (801e5178 resets eleven). */
typedef struct GameRecordECF4 {
    u16 values[4]; /* 0 */
    u8 pad8[0xF];
    u8 flag; /* 17 */
    u8 pad18[0x2];
    u16 unk1A; /* 1A */
    u8 pad1C[0x4];
} GameRecordECF4;

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
    SaveWords10 head; /* 0: the gear record's first 10 bytes */
    SaveWords18 slots; /* 10: its slots */
    s32 attrs; /* 28: attrs (5c) */
    u32 unk60; /* 2C */
    u8 pad30[0x4];
    u16 unk38; /* 34 */
    u8 pad36[0x2];
    u8 unk99; /* 38 */
    u8 unk74; /* 39 */
    u8 unk75; /* 3A */
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

/* The game data from 8006d634 to the flags at 8006f8ea: the code addresses
 * its tables relative to one another, so they are one object. */
typedef struct GameData {
    u8 names[5][2][0x14]; /* 0 (8006d634): name line pairs */
    u8 names10[0x14]; /* C8 */
    u8 unkDC[0x190]; /* DC (8006d710) */
    CharRecord chars[11]; /* 26C (8006d8a0, D_8006D8A0) */
    GearRecord gears[20]; /* 978 (8006dfac, D_8006DFAC) */
    u8 unk1648[0x78]; /* 1648 (8006ec7c) */
    GameRecordECF4 records[11]; /* 16C0 (8006ecf4, D_8006ECF4) */
    u8 unk1820[0x100]; /* 1820 (8006ee54) */
    u8 unk1920[0x996]; /* 1920 (8006ef54) */
    u16 flags; /* 22B6 (8006f8ea, D_8006F8EA) */
} GameData;

/* The menu mode's state (*D_800625A0). */
typedef struct MenuState {
    MenuMover movers[3]; /* 0 */
    MenuBuffer buffers[2]; /* 6C */
    MenuBuffer *current; /* 1D4: the buffer being built */
    SVECTOR viewAngles; /* 1D8: 3D view rotation */
    VECTOR viewOffset; /* 1E0: 3D view translation */
    MATRIX viewMatrix; /* 1F0 */
    u8 pad210[0xC8];
    s32 frameCounter; /* 2D8: frames since last cleared */
    void *sheet; /* 2DC: sprite sheet */
    void *labels; /* 2E0: label text */
    MenuSoundBank *effectBank; /* 2E4: menu sound effect bank */
    u8 pad2E8[0x4];
    s32 time[7]; /* 2EC: play time digits: hours (three), minutes and seconds (two each) */
    s32 bufferIndex; /* 308: 0/1: the buffer being built; the drawing callback clears it */
    u8 present[16]; /* 30C: per character: in the party bits */
    u8 digits[9]; /* 31C: decimal digits of a number, leading zeros ff */
    u8 input; /* 325: decoded input of this frame */
    u8 cardPollTimer; /* 326 */
    u8 drawing; /* 327: nonzero draws the screen each frame */
    u8 unk328; /* 328 */
    u8 viewMotion; /* 329 */
    u8 sounds; /* 32A: nonzero plays menu effects */
    u8 partyCount; /* 32B */
    MenuCard *card; /* 32C: memory-card state (5034 bytes) */
    MenuTables *tables; /* 330: data table directory (cc bytes) */
    u8 cardsPresent; /* 334 */
    u8 unk335; /* 335 */
    u8 cursor; /* 336: top command cursor */
    u8 cursorShown; /* 337: cursor the labels were last drawn for */
    u8 choice; /* 338 */
    u8 choiceShown; /* 339 */
    u8 choiceCount; /* 33A */
    u8 fighters; /* 33B: party members with a gear */
    MenuParty *party; /* 33C: party block (6c bytes) */
    MenuFieldMenu *fieldMenu; /* 340: field-menu block (328 bytes) */
    MenuFieldMenu2 *fieldMenu2; /* 344: field-menu block (374 bytes) */
    MenuPrims *primitives; /* 348: shared primitive block (15c bytes) */
    MenuBlock34C *block34C; /* 34C: 2dc0 bytes */
    MenuImages *screenImages; /* 350: screen images (1194 bytes) */
    MenuSpriteLists *spriteLists; /* 354: 140c bytes */
    MenuBlock358 *block358; /* 358 */
    MenuBlock35C *block35C; /* 35C */
    MenuLabels360 *labels360; /* 360 */
    MenuPortrait *portraits[7]; /* 364 */
    MenuMark *portraitMarks[7]; /* 380 */
    MenuFieldBlock *fieldBlocks[3]; /* 39C: three 127c-byte field blocks */
    MenuSlotImage *images[32]; /* 3A8: file screen slots */
    MenuMarkers *markers; /* 428: marker block (14c bytes) */
    MenuBlock42C *block42C; /* 42C: 1198 bytes */
    MenuBlock430 *block430; /* 430: 1094 bytes */
    MenuBlock434 *block434; /* 434 */
    MenuBlock438 *block438; /* 438 */
    MenuBlock43C *block43C; /* 43C */
    MenuBlock440 *block440; /* 440 */
    MenuImageBlock *blocks444[10]; /* 444 */
    s32 sheetEntries[4][6]; /* 46C: sprite sheet records (80026338) */
    s32 unk4CC; /* 4CC */
    s32 unk4D0; /* 4D0 */
    s32 unk4D4; /* 4D4 */
    u8 loadState; /* 4D8 */
    u8 unk4D9; /* 4D9 */
    u8 pad4DA[0x2];
    u8 firstMember; /* 4DC: first occupied party slot */
    u8 pad4DD[0x3];
    MenuLabelSlot topLabels[4]; /* 4E0: label images (801e7e68); the first owns the 38e-byte pixel block */
    MenuLabelSlot commandLabels[8]; /* 6E0: laid-out labels (801e7e68) */
    MenuLabelSlot partyLabels[6]; /* AE0 */
    MenuLabelSlot fileLabels[6]; /* DE0 */
    MenuLabelSlot labels10E0[8]; /* 10E0 */
    MenuLabelSlot labels14E0[6]; /* 14E0 */
    MenuLabelSlot labels17E0[2]; /* 17E0 */
    MenuLabelSlot labels18E0[6]; /* 18E0 */
    MenuLabelSlot soundLabels[4]; /* 1BE0 */
    MenuLabelSlot *blocks1DE0[4]; /* 1DE0 */
    u8 pad1DF0[0x18];
    MenuPanel *panels[3]; /* 1E08 */
} MenuState;
/* structs: end */

extern MenuState *D_800625A0; /* the menu state */
extern u8 D_80059460;         /* menu kind: 0 field menu, 2 title file screen, 6 other */
extern u8 D_80059171;         /* the triangle menu opened the menu */
extern u8 D_80059178;         /* menu sound effects loaded */
extern u8 D_800594CC;         /* field menu cursor kept between openings */
extern CharRecord D_8006D8A0[]; /* game data: character records, then gears from 11 */
extern GearRecord D_8006DFAC[]; /* game data: gear records (D_8006D8A0 + 11) */
extern u8 D_8006F5C4[150];    /* game data: inventory item counts */
extern u8 D_8006F65A[150];    /* game data: inventory item ids */
/* The inventory at D_8006F5C4 as one record: item counts, then item ids. */
typedef struct Inventory {
    u8 counts[150];
    u8 ids[150];
} Inventory;
#define INVENTORY ((Inventory *)D_8006F5C4)
extern u16 D_8006F958[16];    /* game data */
extern u16 D_8005A3A0[16];
extern u8 D_8006F8E5[];       /* game data */
extern GameRecordECF4 D_8006ECF4[11];
extern u16 D_8006F364;
extern GameData D_8006D634;    /* game data (also named at its tables below) */
extern u16 D_8006F8EA;        /* game data: flags */
extern u8 D_8006F36C[];       /* game data inventory lists (counts, ids) */
extern u8 D_8006F3D0[];
extern u8 D_8006F434[];
extern u8 D_8006F4FC[];
extern u8 D_8006F5C6[];
extern u8 D_8006F65C[];
extern u8 D_8006F6F0[];
extern u8 D_8006F754[];
extern u8 D_8006F7B8[];
extern u8 D_8006F84E[];
extern u8 D_8006F368[3];      /* game data: character of each party slot, ff empty */
extern u16 D_8006F364;        /* game data: party member bits */
extern u16 D_8006F366;
extern u16 D_8006F008;        /* game data: disc of the saved file (0 based) */
extern u8 D_8006F8BA[];       /* game data: special part durability per id */
/* Gear special part durability per id, from the game data flags word on. */
#define GEAR_PART_DURABILITY ((u8 *)&D_8006F8EA)
extern u16 D_8006EF64;
extern s32 D_8006EF58;        /* game data: money */        /* game data: save title line of text file 1 */
extern u8 D_800594D0;         /* load result: 0, 1 title timeout, 2 loaded */

extern u16 D_80059414;         /* label palette (clut) */
extern u16 D_800595D4;         /* plain label palette (clut) */
extern u16 D_8005948C;         /* pad buttons pressed this frame */
extern u16 D_800594A4;         /* pad buttons of the dequeued input (repeating) */
extern s32 D_80059488;         /* play time in frames */
extern s32 *D_8005917C;       /* stack guard word, -1 while intact */

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
extern u8 D_801EA53C[];  /* save file screen command labels */
extern u8 D_801EA542[];  /* title file screen load command labels */
extern u8 D_801EA550[];  /* item target labels */
extern u8 D_801EA558[];
extern u8 D_801EA564[];  /* 801e1014 screen labels */
extern u8 D_801EA568[];  /* title file screen command labels */
extern u8 D_801EA548[];  /* save/load screen labels */
extern u8 D_801EA574[];  /* sound mode labels */
extern u8 D_801E9F88[];
extern u8 D_801EA8F4[];
extern u8 *D_8004FDF0;         /* disc directory records */
extern u8 *D_8004FDF4;
extern u8 *D_8004FE48;
void func_8002954C(s32 file, void *buffer, s32 size, s32 arg3, s32 arg4); /* read a disc file */
void func_801E9340(char *name, void *buffer, s32 size);
extern u8 D_801EA8FC;    /* the last choice was cancelled */
extern u8 D_801E9778;    /* a card changed during a choice */
extern u8 D_801EA8C0;    /* the last printed character was two-byte */
extern u16 D_801EA5D0[0x80]; /* ASCII to two-byte character codes (used from 0x20); the first
                              * 12 bytes also serve as the gear portrait v per slot (s32) */
extern s32 D_801EA6FC;   /* gauge: from, to, difference and lengths */
extern s32 D_801EA700;
extern s32 D_801EA704;
extern s32 D_801EA708;
extern s32 D_801EA70C;
extern u8 D_801EA710;
extern u8 D_801EA714;
extern s32 D_801EA578[];         /* label image x per row pair */
extern s32 D_801EA5C4[];         /* label image y per row pair */
extern s32 D_801EA590[];         /* view name image x (D_801EA578 from row 6) */
extern s32 D_801EA5DC[];         /* view name image y */
extern u16 D_801E9894[32][2];    /* image block x */
extern u16 D_801E9914[32][2];    /* image block y */
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
extern s32 D_801EA578[]; /* character portrait u / 4 per slot */
extern s32 D_801E9D78;    /* stat bar x offset */
extern s32 D_801E9D7C;    /* stat bar y offset */
extern s32 D_801E9D80;    /* stat digit x offset */
extern s32 D_801E9D84;    /* stat digit y offset */
extern s32 D_801E9D88[];
extern s32 D_801E9DDC[];  /* arts list cost x positions */
extern s32 D_801E9E14[];  /* arts list cost y positions */
extern u16 D_801E97F0[];
extern u8 D_801EA7F8[];  /* equipment list entry counts */  /* per character: arts usable from the menu */
extern u16 D_8006ECF6[];  /* game data: per character (32 bytes): arts known */
extern u16 D_8006ECFA[];
extern u16 D_8006ED0E[];  /* part panel row y positions */
extern s32 D_801EA584[]; /* gear portrait u / 4 per slot */
extern s32 D_801EA5C4[]; /* character portrait v per slot */
extern u16 D_80059414;   /* portrait palette of odd images */
extern u16 D_800595D4;   /* portrait palette of even images */
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
extern s16 D_801EA724;   /* item list scroll bar */
extern s32 D_801EA728;
extern s16 D_801EA72C;
extern u8 D_801E9778;    /* a card message is pending */
extern s32 D_801E9788[3]; /* arts screen window sizes per kind */
extern s32 D_801E9794[3];
extern s32 D_801E97A0[3];
extern u8 D_801E9785;    /* the target panels are allocated */
extern MenuAnchor D_801EA054[]; /* target panel layouts: x and y anchors */
extern MenuAnchor D_801EA098[];
extern MenuAnchor D_801EA0DC[];
extern MenuAnchor D_801EA120[];
extern u8 D_801EA730[];  /* equipment list entry ids */
extern s32 D_801EA718;   /* card event and handler ids */
extern s32 D_801EA71C;
extern s32 D_801EA720;
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
extern u8 D_801EA56E[];       /* status command extra labels */
extern u8 D_80059179;         /* forbids the status command toggle */
extern u8 D_801E9808[20];      /* pilot character of each gear */
extern s32 D_801EA494[9];     /* view frame images, ffff none */
extern s32 D_801E9F98[9];     /* view frame x (first view) */
extern s32 D_801E9FBC[9];     /* view frame y */
extern s32 D_801E9FE0[9];     /* play time: x of the two separators and seven digits */
extern s32 D_801EA01C;         /* view digit row x */
extern s32 D_801EA020;         /* view digit row y */
extern u16 D_801EA04C;         /* save title x, y */
extern u16 D_801EA050;
extern s32 D_801EA02C;         /* value A digits x, y */
extern s32 D_801EA030;
extern s32 D_801EA034;         /* value B digits x, y */
extern s32 D_801EA038;
extern s32 D_801EA03C;         /* value C digits x, y */
extern s32 D_801EA040;
extern s32 D_801EA044;         /* value D digits x, y */
extern s32 D_801EA048;
extern u8 D_801EA8C4[0x20];    /* icon palette buffer */
extern RECT D_801EA8E4;        /* icon image area */
extern RECT D_801EA8EC;        /* icon palette area */
extern s32 D_801EA900[2];     /* per port */
extern u8 D_801EA6D0[2][16]; /* per port and save slot: a save of this game exists */
extern u8 *D_801EA6F4;     /* the save information of the last matched file */
extern u8 D_801E9779;    /* frames between card checks */

/* Resident services. */
void *func_80031BDC(s32 size, s32 flags); /* allocate */
void func_800320E8(void *block);          /* free */
void func_8001B970(void);
void func_80026338(void *sheet, s32 id, s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 *f);

/* Menu resource loading (801c65f4). */
typedef struct MenuResources {
    s32 count;
    void *files[8]; /* packed files, relocated by 8003342c */
} MenuResources;

/* Texture of a sprite sheet entry (80026338's six outputs). */
typedef struct SheetEntry {
    s32 unk0;
    s32 mode;
    s32 clutX, clutY;
    s32 pageX, pageY;
} SheetEntry;

extern MenuResources *D_8005945C; /* the menu resources */
extern void *D_8006259C;          /* the menu effect bank */
void func_8003342C(void *archive);                /* relocate an offset table */
void *func_80032E88(void *packed, s32 mode);     /* unpack into a new block */
void func_8002DD20(void *list);                   /* load a TIM list */
void func_80038428(void *bank);
void func_80028470(s32 arg0, s32 arg1);
s32 func_800288EC(s32 file);                                 /* file size */
void func_800295D8(s32 file, void *dst, s32 arg2, s32 arg3); /* read file */
void func_80028A60(s32 arg0);
void func_8002A428(s32 arg0);
void func_8002A498(s32 arg0);
s32 func_8002C3D8(void);                                /* wait for the read */
void func_80019CA0(void);
void func_8001BD40(s32 arg0, s32 arg1);
void func_800263E4(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale, s32 flipX, s32 flipY);
s32 func_8002675C(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale);
void func_80033698(s32 x, s32 y);
s32 func_80035734(s32 port);  /* pad connected */
s32 func_80035CDC(void);      /* dequeue pad input */
void func_80035DB0(void);
s32 func_80036410(void);
void func_80037E8C(void);     /* resume sound */
void func_80037EE4(void);     /* pause sound */
void func_80039DB8(s32 id, s32 sound); /* play a sound effect */
void func_800404C4(u32 event, s32 spec); /* UnDeliverEvent */
s32 func_80040494(s32 event);            /* TestEvent */
u16 *func_800405C4(s32 code);   /* 16x16 font glyph of a two-byte code, -1 none */
s32 func_80040544(s32 fd, void *buf, s32 size); /* read */
void func_80040564(s32 fd);               /* close */
u32 func_801E1418(u8 slot, u8 row);
void func_801E3A80(MenuTables *tables, u8 id);
void func_801E433C(MenuTables *tables, u8 gear);
void func_801E8B4C(u8 offset);
void func_801E86C8(u8 offset);
void func_801E5058(void);
void func_801E5178(void);
void func_801E4754(MenuTables *tables, u8 gear);
void func_801E8EAC(POLY_FT4 *poly, u8 mode);
void func_801E920C(POLY_FT4 *poly, u16 x, u16 y, u8 u, u8 v, u16 w, u16 h);
void func_801E927C(POLY_FT4 *poly);
void func_8003F738(SVECTOR *angles, MATRIX *m); /* RotMatrix */
s32 func_8004E784(s32 channel); /* start a card check */
void func_8003852C(void *bank);
void func_8003A094(void *bank);

s32 func_80028530(void);
u8 *func_800337E8(u8 id);
u8 *func_80033908(s32 index); /* art name */
u8 *func_800339FC(s32 index); /* gear art name */
u8 *func_80033A8C(s32 index); /* gear special art name */  /* accessory name */
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
struct DIRENTRY *func_80040584(char *name, struct DIRENTRY *dir); /* firstfile */
struct DIRENTRY *func_80040594(struct DIRENTRY *dir);             /* nextfile */
/* The card device names ("bu00:", "bu10:"), shared with assembly still. */
extern char D_801C50A8[] __attribute__((aligned(4)));
extern char D_801C50B0[] __attribute__((aligned(4)));
extern char D_801C50B8[];         /* "__tmp_file" */
s32 func_800405B4(char *name);    /* erase */
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
void func_80039E60(s32 sound);
/* The 31 names at the start of the game data (encoded in the save). */
#define GAME_NAMES ((u8 *)&D_8006D634)
s32 func_80033B34(u8 *codes, u8 *text, s32 count); /* decode a name */
void func_80033C20(u8 *text, u8 *codes);            /* encode a name */

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
#define MENU_INDICATOR ((MenuIndicator *)D_800625A0->blocks444[2])
void func_801E78C8(s32 file);
void func_801C9270(s32 port);
extern u8 D_801EA6F8;
extern s32 D_801E981C[];          /* card slot (port * 16 + n) of each cursor position */
void func_801C8BEC(void);
void func_801C8EE8(void);
void func_801C8574(s32 sound);
void func_801C8694(u8 arg0);
u8 func_801CAA38(u8 arg);
s32 func_801CACF8(u8 message, u8 confirm, u8 arg);
void func_801CE2B4(s32 count, POLY_FT4 *polys, s32 first);
void func_801CD81C(MenuPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e);
void func_801CDB1C(MenuPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y);
void func_801CDC6C(MenuPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e);
void func_801CE540(void);
void func_801CE660(void);
void func_801CEB5C(void);
void func_801CEBB4(void);
void func_801CE464(void);
void func_801CE0CC(MenuPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e);
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
void func_801D12D4(MenuPanel *panel, u8 extra);
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
void func_801D7CFC(u8 slot, u8 mode, u8 arg2);
void func_801D7F50(s32 x, s32 y, u8 mode);
void func_801D8644(s32 scale, s32 x, s32 y, u8 compare, u8 first);
void func_801D8DE4(u8 slot, u8 lower, u8 arg2, u8 mode);
void func_801D8EA4(u8 slot, u8 mode, u8 kept, u8 gear);
void func_801D5BA4(s32 x, s32 y);
void func_801D5CF8(s32 x, s32 y);
void func_801D32B4(void);
u8 func_801D9808(void);
s32 func_80038824(void);
void func_800386C4(s32 mode);
void func_801E86C8(u8 row);
void func_801E8B4C(u8 row);
u8 func_801C93A8(void);
u16 func_801C865C(u16 flags, u8 bit);
void func_801DA5BC(s32 row);
void func_801DC3D8(u8 slot, u8 kind);
void func_801DCE60(u8 slot, u8 row, u8 kind);
void func_801DD790(u8 slot, s32 row, u8 kind);
void func_801E35BC(MenuTables *tables, u8 user, u8 target, u8 effect, u8 gear);
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
void func_801E05D0(u8 slot, u8 arg1, s32 arg2);
u8 func_801E0F78(u8 slot, u8 arg1);
void func_801E2368(void);
u8 func_801E23CC(void);
void func_801E2B80(void);
u8 func_801E2BE4(void);
void func_801E3088(u8 command);
u8 func_801E31C0(MenuTables *tables, u8 id, u8 item);
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
void func_801E7E68(MenuLabelSlot *labels, u8 *layout, s32 first, s32 count);
void func_801E8018(u8 count, MenuLabelSlot *labels, u8 *table, u8 *flags);
void func_801E8044(u8 count, u8 *flags);
void func_801E8070(u8 count, MenuLabelSlot *labels, u8 *table, s32 *offsets, u8 *flags, u8 selected, u8 row,
                   u8 mode);
void func_801E8474(s32 count, MenuCommandImages *images);
void func_801E7C50(MenuLabelSlot *label, s32 index, s32 first, u8 mode);
void func_801E8EAC(POLY_FT4 *poly, u8 mode);
void func_801E8F60(u8 index, u8 dim);
void func_801E92CC(void);
s32 func_801E93A0(s32 disc);
void func_801E6668(s32 index);
void func_801E76EC(s32 index);
void func_801E8978(u8 count, u8 cursor, MenuCommandImages *images);
s32 func_801D9704(s32 slot, u8 dir, u8 readyOnly);
u16 func_801C8640(u16 flags, u8 bit);
void func_801D1EE0(s32 index, u8 outline);
void func_801D261C(void);
void func_801D2EC0(u8 slot, u8 mode);
void func_801D3344(s32 x, s32 y, s32 h);
void func_801D36E0(MenuLabelSlot *label, u8 slot, u8 gear, u8 mode);
void func_801DA4A8(void);
void func_801DB02C(u8 index);
void func_801DB0A8(s32 entry, s32 row, u8 kind, u8 index);
void func_801DB340(u8 index);
void func_801DBD4C(s32 a, s32 b);
void func_801DBDB4(void);
void func_801E4A28(SaveData *save);

#endif
