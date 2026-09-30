#ifndef SLOT39_MENU_H
#define SLOT39_MENU_H

#include "common.h"

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
    u8 pad0[0x3];
    u8 redraw3; /* 3 */
    u8 redraw4; /* 4 */
    u8 redraw5; /* 5 */
    u8 redraw6; /* 6 */
    u8 pad7[0x2];
    u8 redraw9; /* 9 */
    u8 redrawA; /* A */
    u8 unkB; /* B */
    u8 labels[8]; /* C: shown flags of the command labels */
    u8 unk14[6]; /* 14 */
    u8 pad1A[0x15];
    u8 unk2F; /* 2F */
    u8 ids[3]; /* 30: character ids of the party slots, ff empty */
    u8 pad33[0x16];
    u8 unk49; /* 49 */
    u8 pad4A[0x9];
    u8 unk53; /* 53 */
    u8 unk54; /* 54 */
    u8 pad55[0xB];
    u8 ready; /* 60 */
    u8 pad61[0xB];
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
    u8 pad0[0x158];
    u8 frame; /* 158 */
    u8 pad159[0x2];
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
    u8 pad4F7C[0x8];
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
    u8 pad4FE7[0x1];
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

/* The data table directory (*(state + 330)). */
typedef struct MenuTables {
    u8 *weapons; /* 0 */
    u8 *accessories; /* 4 */
    u8 pad8[0x8];
    GearFrame *frames; /* 10 */
    u8 pad14[0x8];
    u8 *items; /* 1C */
    u8 pad20[0xAC];
} MenuTables;

/* A gear record of the game data (D_8006DFAC). */
typedef struct GearRecord {
    u8 pad0[0x8];
    u8 frame; /* 8 */
    u8 pad9[0x67];
    u16 unk70; /* 70 */
    u16 unk72; /* 72 */
    u8 pad74[0x30];
} GearRecord;

/* The menu mode's state (*D_800625A0). */
typedef struct MenuState {
    MenuMover movers[3]; /* 0 */
    MenuBuffer buffers[2]; /* 6C */
    MenuBuffer *current; /* 1D4: the buffer being built */
    u8 pad1D8[0x100];
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
    u8 *fieldMenu; /* 340: field-menu block (328 bytes) */
    u8 *fieldMenu2; /* 344: field-menu block (374 bytes) */
    MenuPrims *primitives; /* 348: shared primitive block (15c bytes) */
    u8 *block34C; /* 34C: 2dc0 bytes */
    MenuImages *screenImages; /* 350: screen images (1194 bytes) */
    u8 *block354; /* 354: 140c bytes */
    u8 pad358[0xC];
    u8 *portraits[2]; /* 364: two 720-byte blocks */
    u8 pad36C[0x14];
    u8 *portraitMarks[2]; /* 380 */
    u8 pad388[0x14];
    u8 *fieldBlocks[3]; /* 39C: three 127c-byte field blocks */
    u8 *images[32]; /* 3A8 */
    u8 *markers; /* 428: marker block (14c bytes) */
    u8 pad42C[0x10];
    u8 *block43C; /* 43C */
    u8 *block440; /* 440 */
    u8 pad444[0x28];
    s32 sheetEntries[4][6]; /* 46C: sprite sheet records (80026338) */
    s32 unk4CC; /* 4CC */
    s32 unk4D0; /* 4D0 */
    s32 unk4D4; /* 4D4 */
    u8 loadState; /* 4D8 */
    u8 unk4D9; /* 4D9 */
    u8 pad4DA[0x2];
    u8 firstMember; /* 4DC: first occupied party slot */
    u8 pad4DD[0x3];
    u8 labelImages[0x78]; /* 4E0: label image records (801e7e68) */
    u8 *labelPixels; /* 558: 38e-byte label pixel block */
    u8 pad55C[0x184];
    u8 labelSlots[24][0x100]; /* 6E0: laid-out label rows (801e7e68) */
} MenuState;
/* structs: end */

extern MenuState *D_800625A0; /* the menu state */
extern u8 D_80059460;         /* menu kind: 0 field menu, 2 title file screen, 6 other */
extern u8 D_80059171;         /* the triangle menu opened the menu */
extern u8 D_80059178;         /* menu sound effects loaded */
extern u8 D_800594CC;         /* field menu cursor kept between openings */
extern GearRecord D_8006DFAC[]; /* game data: gear records */
extern u8 D_8006F008;         /* game data: disc of the loaded file */
extern u16 D_8006EF64;        /* game data: save title line of text file 1 */
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
extern u8 D_801EA19C[];  /* field menu command cursor positions */
extern u8 D_801EA1D4[];  /* title file screen cursor positions */
extern u8 D_801EA528[];  /* field menu command labels */
extern u8 D_801EA524[];  /* label image layout */
extern u8 D_801EA530[];
extern u8 D_801EA568[];  /* title file screen command labels */
extern u8 D_801EA8FC;
extern s32 D_801EA900[2];
extern u8 D_801EA6D0[32];  /* per port and save slot: a save of this game exists */
extern u8 *D_801EA6F4;     /* the save information of the last matched file */
extern u8 D_801E9779;    /* frames between card checks */

/* Resident services. */
void *func_80031BDC(s32 size, s32 flags); /* allocate */
void func_800320E8(void *block);          /* free */
void func_8003F8E8(void *dst, s32 size);  /* bzero */
void func_8001B970(void);
void func_80026338(void *sheet, s32 id, s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 *f);
void func_80028470(s32 arg0, s32 arg1);
s32 func_800288EC(s32 file);                                 /* file size */
void func_800295D8(s32 file, void *dst, s32 arg2, s32 arg3); /* read file */
void func_80028A60(s32 arg0);                                /* wait for the read */
void func_80019CA0(void);
void func_8001BD40(s32 arg0, s32 arg1);
void func_80033698(s32 x, s32 y);
s32 func_80035734(s32 port);  /* pad connected */
s32 func_80035CDC(void);      /* dequeue pad input */
void func_80035DB0(void);
s32 func_80036410(void);
void func_80037E8C(void);     /* resume sound */
void func_80037EE4(void);     /* pause sound */
void func_80039DB8(s32 id, s32 sound); /* play a sound effect */
void func_800404C4(u32 event, s32 spec); /* UnDeliverEvent */
void func_80040484(s32 event);           /* EnableEvent */
s32 func_80040494(s32 event);            /* TestEvent */
s32 func_80040534(char *name, s32 mode);  /* open */
s32 func_80040544(s32 fd, void *buf, s32 size); /* read */
void func_80040564(s32 fd);               /* close */
void func_800404D4(void);                /* EnterCriticalSection */
void func_800404E4(void);                /* ExitCriticalSection */
void func_80043BFC(void *prim, s32 on);  /* SetSemiTrans */
void func_80043C24(void *prim, s32 on);  /* SetShadeTex */
void func_80043CB0(POLY_FT4 *poly);      /* SetPolyFT4 */
void func_80043CC4(POLY_G4 *poly);       /* SetPolyG4 */
void func_80044AD8(u32 *ot, s32 count);  /* ClearOTagR */
void func_8004495C(RECT *rect, s32 x, s32 y); /* MoveImage */
void func_80044BD0(u32 *ot);             /* DrawOTag */
void func_80044C44(void *env);           /* PutDrawEnv */
void func_80044E9C(void *env);           /* PutDispEnv */
void func_8004B54C(s32 mode);            /* VSync */
s32 func_8004E784(s32 channel); /* start a card check */
void func_800445D0(s32 mode);                /* DrawSync */
void func_80044894(RECT *rect, void *pixels); /* LoadImage */
void func_8003852C(void *bank);
void func_8003A094(void *bank);

s32 func_80028530(void);

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
void func_801C7B0C(void);
void func_801C7BF4(void);
void func_801C7D78(void);
void func_801C7F34(u32 frames);
u8 func_801C881C(void);
s32 func_801C891C(s32 channel);
u8 func_801C8A10(u8 port);
u8 func_801C8D78(u8 port);
void func_801C8BEC(void);
void func_801C8EE8(void);
void func_801C8574(s32 sound);
void func_801C8694(u8 arg0);
s32 func_801CACF8(u8 arg0, u8 arg1, u8 arg2);
void func_801CE198(u8 kind, u8 *sprites, u8 *block, u8 count);
void func_801D1CA0(void);
void func_801D1D40(void);
void func_801D1E80(void);
void func_801D22C4(void);
void func_801D22F4(u8 arg0);
void func_801D2484(void);
void func_801D2968(void);
void func_801D2D38(void);
void func_801D2F4C(u8 message);
void func_801D397C(s32 arg0, s32 x, s32 y, s32 w, s32 h, s32 arg5, s32 arg6, s32 arg7, s32 arg8);
void func_801D5BA4(s32 x, s32 y);
void func_801D5CF8(s32 x, s32 y);
void func_801D32B4(void);
u8 func_801D9808(void);
u8 func_801D9F98(u8 mode, u8 save);
void func_801D1EB0(void);
void func_801D29A8(u8 arg0, u8 arg1);
void func_801D3674(void);
u8 func_801DBE54(void);
void func_801DDF24(u8 slot, u8 arg1, s32 arg2);
u8 func_801DE29C(u8 slot, u8 arg1);
u8 func_801E0F78(u8 slot, u8 arg1);
u8 func_801E23CC(void);
u8 func_801E2BE4(void);
void func_801E3088(u8 command);
void func_801E41C0(MenuTables *tables, u8 gear);
void func_801E42AC(MenuTables *tables, u8 gear);
void func_801E4258(MenuTables *tables, u8 gear);
void func_801E5B88(void);
void func_801E5E4C(void);
void func_801E7E68(u8 *records, u8 *layout, s32 arg2, s32 count);
void func_801E8018(u8 count, u8 *labels, u8 *table, u8 *placement);
void func_801E8044(u8 count, u8 *flags);
void func_801E8070(u8 count, u8 *labels, u8 *table, u8 *arg3, u8 *placement, u8 selected, s32 arg6,
                   s32 arg7);
void func_801E8474(u8 count, u8 *positions);
void func_801E92CC(void);
s32 func_801E93A0(s32 disc);
void func_801E8978(u8 count, u8 cursor, u8 *positions);

#endif
