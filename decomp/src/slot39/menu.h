#ifndef SLOT39_MENU_H
#define SLOT39_MENU_H

#include "common.h"
#include "psyq/libapi.h"

/*
 * Menu overlay (Disc 1 slot 39, loaded at 801c5000): the menu mode's state.
 * The resident keeps a pointer to it at D_800625A0; the overlay allocates the
 * state's sub-blocks on entry and frees them on exit. Field names follow what
 * the overlay does with them; unknown bytes stay padding.
 */

/* PlayStation library types (libgpu/libgte layouts). */
typedef struct RECT {
    s16 x, y;
    s16 w, h;
} RECT;

typedef struct SVECTOR {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct VECTOR {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct MATRIX {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct POLY_G4 {
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

typedef struct POLY_FT4 {
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
    u8 unk50[3]; /* 50 */
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
    u8 pad0[0x1180];
    RECT copy; /* 1180: the screen area copied into the other buffer each frame */
    u8 pad1188[0xA];
    u8 captured; /* 1192 */
    u8 refresh; /* 1193 */
} MenuImages;

/* Shared primitive block (*(state + 348)). */
typedef struct MenuPrims {
    POLY_FT4 polys[2]; /* 0 */
    u8 pad50[0x48];
    u8 fills[2][0x18]; /* 98: per buffer */
    u8 padC8[0x60];
    u8 modes0[2][0xc]; /* 128 */
    u8 modes[2][0xc]; /* 140: per buffer */
    u8 frame; /* 158 */
    u8 mode; /* 159 */
    u8 pad15A[0x1];
    u8 shade; /* 15B */
} MenuPrims;

/* One file entry of a card listing. */
typedef struct MenuCardFile {
    u8 pad0[0x18];
    char name[21]; /* 18: directory entry name */
    u8 pad2D[0x2B];
    u8 state; /* 58 */
    u8 pad59[0x3];
} MenuCardFile;

/* Memory-card state (*(state + 32c)). */
typedef struct MenuCard {
    MenuCardFile files[32]; /* 0 */
    u8 padB80[0x14];
    u8 headers[32][0x200]; /* B94: first block of each listed file */
    u8 pad4B94[0x3E0];
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
    u8 title[30]; /* 4FFC: save title line of the text file */
    u8 unk501A; /* 501A */
    u8 unk501B; /* 501B */
    u8 pad501C[0x18];
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
    u8 pad0[0x11];
    u8 unk11; /* 11 */
    u8 pad12[0x16];
} MenuEffect;

/* A character record of the game data (D_8006D8A0; gears follow from 11). */
typedef struct CharRecord {
    u8 pad0[0x4C];
    u16 hp; /* 4C */
    u16 hpMax; /* 4E */
    u8 pad50[0xB];
    u8 unk5B; /* 5B */
    u8 pad5C[0x6];
    u8 unk62; /* 62 */
    u8 unk63; /* 63 */
    u8 pad64[0x6];
    u8 weapons[5]; /* 6A: [0] the weapon */
    u8 specials[5]; /* 6F: special parts */
    u8 accessories[5]; /* 74 */
    u8 pad79[0x27];
    u8 gear; /* A0: gear record (+11), ff none */
    u8 padA1[0x3];
} CharRecord;

/* An item record of the data tables (+1c). */
typedef struct MenuItem {
    u8 pad0[0x4];
    u16 target; /* 4: 4000 all, 1000 none, else one; low bits: target kind */
    u8 use; /* 6: 80 usable in the menu, 40 in battle, 20 field only */
    u8 pad7[0x9];
} MenuItem;

/* The data table directory (*(state + 330)). */
typedef struct MenuTables {
    u8 *weapons; /* 0 */
    u8 *accessories; /* 4 */
    GearEngine *engines; /* 8 */
    GearPart *parts; /* C */
    GearFrame *frames; /* 10 */
    u8 pad14[0x8];
    MenuItem *items; /* 1C */
    MenuEffect *effects[11]; /* 20: per character */
    u8 pad4C[0x58];
    u16 unkA4; /* A4 */
    u16 unkA6; /* A6 */
    u8 padA8[0x8];
    u16 unkB0; /* B0 */
    u8 unkB2; /* B2 */
    u8 unkB3; /* B3 */
    u8 unkB4; /* B4 */
    u8 padB5[0x3];
    u16 shown[9]; /* B8: stats shown on the equipment screen */
    u8 padCA[0x2];
} MenuTables;

/* A gear record of the game data (D_8006DFAC). */
typedef struct GearRecord {
    u8 pad0[0x2];
    u8 engine; /* 2: record of table +8 */
    u8 unk3; /* 3: record of table +c */
    u8 unk4[0x4]; /* 4: part ids */
    u8 frame; /* 8 */
    u8 unk9[3]; /* 9: part ids (801df5d0 keeps four from here) */
    u8 unkC[4]; /* C: part ids */
    u8 pad10[0x28];
    u16 unk38; /* 38 */
    u16 unk3A; /* 3A */
    u8 unk3C; /* 3C */
    u8 unk3D; /* 3D */
    u8 unk3E; /* 3E */
    u8 unk3F; /* 3F */
    u8 pad40[0x4];
    u16 unk44; /* 44 */
    u8 pad46[0x1A];
    u32 unk60; /* 60 */
    u32 unk64; /* 64 */
    u8 pad68[0x8];
    u16 unk70; /* 70 */
    u16 unk72; /* 72 */
    u8 pad74[0x1];
    u8 unk75; /* 75 */
    u8 pad76[0x22];
    u8 unk98; /* 98 */
    u8 pad99[0x4];
    u8 unk9D; /* 9D */
    u8 unk9E; /* 9E */
    u8 unk9F; /* 9F */
    u8 padA0[0x4];
} GearRecord;

/* A laid-out label: its quads and sprite list. */
typedef struct MenuLabelSlot {
    POLY_FT4 polys[2]; /* 0 */
    SVECTOR verts[4]; /* 50 */
    u8 pad70[0x8];
    u8 *pixels; /* 78 */
    u8 pad7C[0x1];
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
    u8 pad0[0x1C];
    u8 images[4]; /* 1C: sheet image per view (+14e), ff none */
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
    u8 pad0[0x877];
    u8 buffer; /* 877 */
    u8 shown; /* 878 */
    u8 pad879[0x3];
} MenuView;

/* The 2dc0-byte block (*(state + 34c)). */
typedef struct MenuBlock34C {
    u8 pad0[0xA98];
    MenuView views[4]; /* A98 */
    u8 pad2C88[0x134];
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
    u8 pad1080[0x4];
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

/* The status panel block (*(state + 358)). */
typedef struct MenuBlock358 {
    u8 pad0[0x2AE0];
    u8 buffer; /* 2AE0 */
    u8 pad2AE1[0xF];
} MenuBlock358;

/* The equipment panel block (*(state + 35c)). */
typedef struct MenuBlock35C {
    POLY_FT4 polys[231]; /* 0 */
    u8 pad2418[0x8];
    SVECTOR verts[474]; /* 2420 */
    u8 pad32F0[0x1];
    u8 buffer; /* 32F1 */
    u8 pad32F2[0x1];
    u8 kind; /* 32F3 */
} MenuBlock35C;

/* A position record passed to the panel builders (+28 base). */
typedef struct MenuAnchor {
    u8 pad0[0x28];
    s32 base; /* 28 */
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
    POLY_FT4 list6[12]; /* 910 */
    POLY_FT4 list1[6]; /* AF0 */
    POLY_FT4 list2[6]; /* BE0 */
    POLY_FT4 list3[4]; /* CD0 */
    POLY_FT4 list4[4]; /* D70 */
    POLY_FT4 list5[28]; /* E10 */
    u8 buffer; /* 1270 */
    u8 count6; /* 1271 */
    u8 pad1272[0x1];
    u8 count1; /* 1273 */
    u8 count2; /* 1274 */
    u8 count3; /* 1275 */
    u8 count4; /* 1276 */
    u8 count5; /* 1277 */
    u8 pad1278[0x1];
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

/* A portrait frame (*(state + 364)). */
typedef struct MenuPortrait {
    u8 pad0[0x3C0];
    POLY_FT4 side[2]; /* 3C0 */
    POLY_FT4 top[2]; /* 410 */
    POLY_FT4 bottom[2]; /* 460 */
    u8 pad4B0[0x1E0];
    SVECTOR frameVerts[4]; /* 690 */
    SVECTOR sideVerts[4]; /* 6B0 */
    SVECTOR topVerts[4]; /* 6D0 */
    SVECTOR bottomVerts[4]; /* 6F0 */
    u8 pad710[0x4];
    s32 unk714; /* 714 */
    s32 unk718; /* 718 */
    u8 buffer; /* 71C */
    u8 unk71D; /* 71D */
} MenuPortrait;

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
    u8 pad30C[0x10];
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
    u8 *images[32]; /* 3A8 */
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
extern u8 D_8006F8BA[];       /* game data: special part durability per id */
extern u8 D_8006F8EA[];       /* game data: gear special part durability per id */
extern u8 D_8006F008;         /* game data: disc of the loaded file */
extern u16 D_8006EF64;
extern s32 D_8006EF58;        /* game data: money */        /* game data: save title line of text file 1 */
extern u8 D_800594D0;         /* load result: 0, 1 title timeout, 2 loaded */

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
extern u8 D_801E9E64[];
extern u8 D_801E9E84[];
extern u8 D_801E9EA0[];
extern u8 D_801EA19C[];  /* field menu command cursor positions */
extern u8 D_801EA1D4[];  /* title file screen cursor positions */
extern u8 D_801EA528[];  /* field menu command labels */
extern u8 D_801EA524[];  /* label image layout */
extern u8 D_801EA530[];
extern u8 D_801EA534[];  /* party label layout */
extern u16 D_801E9E4C[3][2]; /* party label positions */
extern u16 D_801E9E58[3][2];
extern u8 D_801EA53C[];  /* save file screen command labels */
extern u8 D_801EA542[];  /* title file screen load command labels */
extern u8 D_801EA550[];  /* item target labels */
extern u8 D_801EA558[];
extern u8 D_801EA568[];  /* title file screen command labels */
extern u8 D_801EA548[];  /* save/load screen labels */
extern u8 D_801EA574[];  /* sound mode labels */
extern u8 D_801EA578[];
extern u8 D_801E9F88[];
extern u8 D_801EA8F4[];
extern u8 D_801EA8FC;
extern u8 D_801EA8C0;    /* the last printed character was two-byte */
extern u16 D_801EA5D0[0x80]; /* ASCII to two-byte character codes */
extern s32 D_801EA6FC;   /* gauge: from, to, difference and lengths */
extern s32 D_801EA700;
extern s32 D_801EA704;
extern s32 D_801EA708;
extern s32 D_801EA70C;
extern u8 D_801EA710;
extern u8 D_801EA714;
extern s32 D_801EA34C[20]; /* field block part images, ffff none */
extern s32 D_801E9A78[20];
extern s32 D_801E9AC8[20];
extern s32 D_801E9A58[4]; /* marker positions */
extern s32 D_801E9A68[4];
extern s16 D_801EA724;   /* item list scroll bar */
extern s32 D_801EA728;
extern s16 D_801EA72C;
extern u8 D_801E9778;    /* a card message is pending */
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
extern s32 D_801EA900[2];
extern u8 D_801EA6D0[32];  /* per port and save slot: a save of this game exists */
extern u8 *D_801EA6F4;     /* the save information of the last matched file */
extern u8 D_801E9779;    /* frames between card checks */

/* Resident services. */
void *func_80031BDC(s32 size, s32 flags); /* allocate */
void func_800320E8(void *block);          /* free */
void bzero(void *dst, s32 size);  /* bzero */
void func_8001B970(void);
void func_80026338(void *sheet, s32 id, s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 *f);
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
void CloseEvent(s32 event);           /* EnableEvent */
s32 func_80040494(s32 event);            /* TestEvent */
void func_800405C4(s32 code);
s32 CdSyncCallback(s32 callback);  /* previous callback */
s32 CdReadyCallback(s32 callback);
s32 CdReadCallback(s32 callback);
s32 open(char *name, s32 mode);  /* open */
s32 func_80040544(s32 fd, void *buf, s32 size); /* read */
void func_80040564(s32 fd);               /* close */
void EnterCriticalSection(void);                /* EnterCriticalSection */
void ExitCriticalSection(void);                /* ExitCriticalSection */
void AddPrim(u32 *ot, void *prim);  /* AddPrim */
void SetSemiTrans(void *prim, s32 on);  /* SetSemiTrans */
void SetShadeTex(void *prim, s32 on);  /* SetShadeTex */
void SetPolyFT4(POLY_FT4 *poly);      /* SetPolyFT4 */
void SetPolyG4(POLY_G4 *poly);       /* SetPolyG4 */
void func_8003F738(SVECTOR *angles, MATRIX *m); /* RotMatrix */
void TransMatrix(MATRIX *m, VECTOR *t);      /* TransMatrix */
void SetRotMatrix(MATRIX *m);                 /* SetRotMatrix */
void SetTransMatrix(MATRIX *m);                 /* SetTransMatrix */
void ClearOTagR(u32 *ot, s32 count);  /* ClearOTagR */
void MoveImage(RECT *rect, s32 x, s32 y); /* MoveImage */
void DrawOTag(u32 *ot);             /* DrawOTag */
void PutDrawEnv(void *env);           /* PutDrawEnv */
void PutDispEnv(void *env);           /* PutDispEnv */
void VSync(s32 mode);            /* VSync */
s32 CdControlB(s32 command, s32 arg1, u8 *result);
void RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, s32 *sxy0, s32 *sxy1, s32 *sxy2,
                   s32 *sxy3, s32 *p, s32 *flag); /* RotTransPers4 */
s32 PCopen(s32 arg0, s32 arg1, s32 arg2);
void PCclose(s32 handle);
void func_8004C398(s32 handle, s32 arg1, s32 arg2);
s32 func_8004E784(s32 channel); /* start a card check */
void ClearImage(RECT *rect, s32 r, s32 g, s32 b); /* ClearImage */
void DrawSync(s32 mode);                /* DrawSync */
void LoadImage(RECT *rect, void *pixels); /* LoadImage */
void func_8003852C(void *bank);
void func_8003A094(void *bank);

s32 func_80028530(void);
u8 *func_80033728(u8 *table, u8 index); /* message of a table */
void memmove(void *dst, void *src, s32 size);
u8 *func_80033818(u8 item);  /* item name text */
void func_80033B34(u8 *codes, u8 *text, s32 count); /* codes to text */
u8 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line; its width */
s32 OpenEvent(u32 cause, s32 type, s32 mode, void *handler);
void InitCARD(s32 shared);
void StartCARD(void);
void _bu_init(void);

/* Overlay functions. */
u8 func_801C531C(u8 offset);
void func_801C55A0(void);
void func_801C57A4(void);
void func_801C58EC(void);
void func_801C6400(void);
void func_801C6AA0(MenuState *state);
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
void func_801C7BF4(void);
void func_801C7D78(void);
void func_801C7F34(u32 frames);
void func_801C80B8(u32 value);
void func_801C851C(SVECTOR *v, u16 x, u16 y, u16 w, u16 h);
u8 func_801C881C(void);
s32 func_801C891C(s32 channel);
u8 func_801C8A10(u8 port);
u8 func_801C8D78(u8 port);
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
void func_801CF5E4(s32 arg0, s32 arg1, s32 arg2);
void func_801CF8D8(void);
void func_801CFB48(void);
void func_801CFF64(void);
void func_801D01D0(void);
void func_801D02D8(void);
void func_801D0C78(void);
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
void func_801D8644(u8 slot, s32 x, s32 y, u8 arg3, u8 mode);
void func_801D8DE4(u8 slot, u8 lower, u8 arg2, u8 mode);
void func_801D8EA4(u8 slot, u8 lower, u8 arg2, u8 mode);
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
void func_801D3488(s32 arg0, s32 arg1);
void func_801D3674(void);
void func_801DA518(void);
u8 func_801DBE54(void);
void func_801DC2CC(u8 kind);
void func_801DE36C(void);
void func_801DE400(void);
void func_801DDF24(u8 slot, u8 arg1, s32 arg2);
u8 func_801DE29C(u8 slot, u8 arg1);
void func_801E05D0(u8 slot, u8 arg1, s32 arg2);
u8 func_801E0F78(u8 slot, u8 arg1);
void func_801E2368(void);
u8 func_801E23CC(void);
void func_801E2B80(void);
u8 func_801E2BE4(void);
void func_801E3088(u8 command);
s32 func_801E31C0(MenuTables *tables, u8 character, u8 item);
void func_801E3C2C(MenuTables *tables, u8 gear);
void func_801E3ECC(MenuTables *tables, u8 gear);
void func_801E41C0(MenuTables *tables, u8 gear);
void func_801E42AC(MenuTables *tables, u8 gear);
void func_801E4258(MenuTables *tables, u8 gear);
void func_801E4D10(s32 *save, MenuTables *tables);
void func_801E53CC(u8 index);
void func_801E56E8(s32 index);
void func_801E8DA8(u8 image, u8 row);
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
void func_801E7C50(MenuLabelSlot *slot, s32 index, s32 x, s32 flags);
void func_801E7E68(void *records, u8 *layout, s32 arg2, s32 count);
void func_801E8018(u8 count, MenuLabelSlot *labels, u8 *table, u8 *flags);
void func_801E8044(u8 count, u8 *flags);
void func_801E8070(u8 count, MenuLabelSlot *labels, u8 *table, u8 *arg3, u8 *flags, u8 selected, u8 arg6,
                   s32 arg7);
void func_801E8474(u8 count, u8 *positions);
void func_801E8EAC(POLY_FT4 *poly, u8 mode);
void func_801E8F60(s32 window, u8 mode);
void func_801E920C(POLY_FT4 *poly, s32 x, s32 y, s32 u, s32 v, s32 w, s32 h);
void func_801E92CC(void);
s32 func_801E93A0(s32 disc);
void func_801E6668(s32 index);
void func_801E76EC(s32 index);
void func_801E8978(u8 count, u8 cursor, u8 *positions);

#endif
