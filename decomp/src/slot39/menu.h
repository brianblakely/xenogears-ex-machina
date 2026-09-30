#ifndef SLOT39_MENU_H
#define SLOT39_MENU_H

#include "common.h"

/*
 * Menu overlay (Disc 1 slot 39, loaded at 801c5000): the menu mode's state.
 * The resident keeps a pointer to it at D_800625A0; the overlay allocates the
 * state's sub-blocks on entry and frees them on exit.
 */

typedef struct MenuState {
    u8 pad0[0x2d8];
    s32 frameCounter;   /* 2d8: frames since last cleared */
    void *sheet;        /* 2dc: sprite sheet */
    void *labels;       /* 2e0: label text */
    void *effectBank;   /* 2e4: menu sound effect bank */
    u8 pad2e8[0x32c - 0x2e8];
    u8 *card;           /* 32c: memory-card state (5034 bytes) */
    u8 *tables;         /* 330: data table directory (cc bytes) */
    u8 cardsPresent;    /* 334 */
    u8 unk335;
    u8 cursor;          /* 336: top command cursor */
    u8 cursorShown;     /* 337: cursor the labels were last drawn for */
    u8 choice;          /* 338 */
    u8 choiceShown;     /* 339 */
    u8 choiceCount;     /* 33a */
    u8 fighters;        /* 33b: party members with a gear */
    u8 *party;          /* 33c: party block (6c bytes) */
    u8 *fieldMenu;      /* 340: field-menu block (328 bytes) */
    u8 *fieldMenu2;     /* 344: field-menu block (374 bytes) */
    u8 *primitives;     /* 348: shared primitive block (15c bytes) */
    u8 pad34c[4];
    u8 *screenImages;   /* 350: screen images (1194 bytes) */
    u8 *block354;       /* 354: 140c bytes */
    u8 pad358[0x39c - 0x358];
    u8 *fieldBlocks[3]; /* 39c: three 127c-byte field blocks */
    u8 pad3a8[0x428 - 0x3a8];
    u8 *markers;        /* 428: marker block (14c bytes) */
    u8 pad42c[0x4dc - 0x42c];
    u8 firstMember;     /* 4dc: first occupied party slot */
} MenuState;

extern MenuState *D_800625A0; /* the menu state */
extern u8 D_80059460;         /* menu kind: 0 field menu, 2 title file screen, 6 other */

/* Resident heap and memory services. */
void *func_80031BDC(s32 size, s32 flags); /* allocate */
void func_800320E8(void *block);          /* free */
void func_8003F8E8(void *dst, s32 size);  /* bzero */

#endif
