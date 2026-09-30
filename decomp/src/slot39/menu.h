#ifndef SLOT39_MENU_H
#define SLOT39_MENU_H

#include "common.h"

/*
 * Menu overlay (Disc 1 slot 39, loaded at 801c5000): the menu mode's state.
 * The resident keeps a pointer to it at D_800625A0; the overlay allocates the
 * state's sub-blocks on entry and frees them on exit. Field names follow what
 * the overlay does with them; unknown bytes stay padding.
 */

/* structs: begin */
/* Party block (*(state + 33c)): per-part redraw flags, the label set and the party ids. */
typedef struct MenuParty {
    u8 pad0[0x3];
    u8 redraw3; /* 3 */
    u8 redraw4; /* 4 */
    u8 pad5[0x4];
    u8 redraw9; /* 9 */
    u8 redrawA; /* A */
    u8 padB[0x1];
    u8 labels[0x24]; /* C: label placement of the command window */
    u8 ids[3]; /* 30: character ids of the party slots, ff empty */
    u8 pad33[0x2D];
    u8 ready; /* 60 */
    u8 pad61[0xB];
} MenuParty;

/* Screen images (*(state + 350)). */
typedef struct MenuImages {
    u8 pad0[0x1192];
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

/* Memory-card state (*(state + 32c)). */
typedef struct MenuCard {
    u8 pad0[0x4FE6];
    u8 mode; /* 4FE6 */
    u8 pad4FE7[0x4D];
} MenuCard;

/* The menu mode's state (*D_800625A0). */
typedef struct MenuState {
    u8 pad0[0x2D8];
    s32 frameCounter; /* 2D8: frames since last cleared */
    void *sheet; /* 2DC: sprite sheet */
    void *labels; /* 2E0: label text */
    void *effectBank; /* 2E4: menu sound effect bank */
    u8 pad2E8[0x3D];
    u8 input; /* 325: decoded input of this frame */
    u8 cardPollTimer; /* 326 */
    u8 drawing; /* 327: nonzero draws the screen each frame */
    u8 unk328; /* 328 */
    u8 viewMotion; /* 329 */
    u8 sounds; /* 32A: nonzero plays menu effects */
    u8 partyCount; /* 32B */
    MenuCard *card; /* 32C: memory-card state (5034 bytes) */
    u8 *tables; /* 330: data table directory (cc bytes) */
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
    u8 pad34C[0x4];
    MenuImages *screenImages; /* 350: screen images (1194 bytes) */
    u8 *block354; /* 354: 140c bytes */
    u8 pad358[0x44];
    u8 *fieldBlocks[3]; /* 39C: three 127c-byte field blocks */
    u8 pad3A8[0x80];
    u8 *markers; /* 428: marker block (14c bytes) */
    u8 pad42C[0xB0];
    u8 firstMember; /* 4DC: first occupied party slot */
    u8 pad4DD[0x203];
    u8 commandLabels[0x700]; /* 6E0 */
} MenuState;
/* structs: end */

extern MenuState *D_800625A0; /* the menu state */
extern u8 D_80059460;         /* menu kind: 0 field menu, 2 title file screen, 6 other */
extern u8 D_80059171;         /* the triangle menu opened the menu */
extern u8 D_800594D0;         /* load result: 0, 1 title timeout, 2 loaded */

/* Overlay statics. */
extern u8 D_801E96A4;    /* the file screen saves (nonzero) or loads */
extern u8 D_801E96A5;
extern u8 D_801E977A;    /* inside a command */
extern u8 D_801E9784;    /* nonzero checks the reset combination */
extern u8 D_801E9E64[];
extern u8 D_801E9E84[];
extern u8 D_801EA19C[];  /* field menu command cursor positions */
extern u8 D_801EA1D4[];  /* title file screen cursor positions */
extern u8 D_801EA528[];  /* field menu command labels */
extern u8 D_801EA530[];  /* title file screen command labels */
extern u8 D_801EA8FC;

/* Resident services. */
void *func_80031BDC(s32 size, s32 flags); /* allocate */
void func_800320E8(void *block);          /* free */
void func_8003F8E8(void *dst, s32 size);  /* bzero */
void func_8001B970(void);

s32 func_80028530(void);

/* Overlay functions. */
u8 func_801C531C(u8 offset);
void func_801C7BF4(void);
void func_801C8574(u8 sound);
void func_801C8694(u8 arg0);
s32 func_801CACF8(u8 arg0, u8 arg1, u8 arg2);
void func_801D1E80(void);
void func_801D22C4(void);
void func_801D22F4(u8 arg0);
void func_801D2484(void);
u8 func_801D9808(void);
u8 func_801D9F98(u8 mode, u8 save);
void func_801D1EB0(void);
void func_801D29A8(u8 arg0, u8 arg1);
void func_801D3674(void);
u8 func_801DBE54(void);
u8 func_801DE29C(u8 slot, u8 arg1);
u8 func_801E0F78(u8 slot, u8 arg1);
u8 func_801E23CC(void);
u8 func_801E2BE4(void);
void func_801E3088(u8 command);
void func_801E8018(u8 count, u8 *labels, u8 *table, u8 *placement);
void func_801E8044(u8 count, u8 *placement);
void func_801E8070(u8 count, u8 *labels, u8 *table, u8 *arg3, u8 *placement, u8 selected, s32 arg6,
                   s32 arg7);
void func_801E8474(u8 count, u8 *positions);
void func_801E8978(u8 count, u8 cursor, u8 *positions);

#endif
