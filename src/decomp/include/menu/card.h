#ifndef MENU_CARD_H
#define MENU_CARD_H

#include "common.h"
#include "psyq/libgpu.h"

/* The menu screens' memory card state (mode 5; see menu/screen.h): the
 * directory scan of both ports and the header of this game's save, which
 * every screen fills in (ovl2601 and ovl2602 too, though the shops never
 * save). */

/* One file of a card listing. */
typedef struct MenuCardFile {
    s32 frames[6]; /* icon animation: image y of each step */
    char name[21]; /* 0x18: directory entry name */
    u8 pad2d[0x2B];
    u8 state;      /* 0x58 */
    u8 pad59[3];
} MenuCardFile;

/* A memory card's header on the file screen: its label and name sprites. */
typedef struct MenuCardHeader {
    POLY_FT4 label[2];
    POLY_FT4 name[4]; /* 0x50 */
} MenuCardHeader;

/* The memory card state (MenuState card). */
typedef struct MenuCard {
    MenuCardFile files[32];       /* 0x0000 */
    TIM_IMAGE icon;               /* 0x0b80: the save icon TIM */
    u8 heads[32][0x200];          /* 0x0b94: first block of each listed file */
    u8 save_magic[2];             /* 0x4b94: this game's save header: "SC" */
    u8 save_icon_flag;            /* 0x4b96 */
    u8 save_blocks;               /* 0x4b97 */
    char save_title[0x5C];        /* 0x4b98: Shift JIS */
    u8 save_palette[0x20];        /* 0x4bf4 */
    u8 save_icon[0x80];           /* 0x4c14 */
    u8 pad4c94[0x100];
    MenuCardHeader card_headers[2]; /* 0x4d94: per port */
    s32 result[2];                /* 0x4f74: per port: the last card check's result */
    s32 cursor;                   /* 0x4f7c: file cursor over both ports (port 2 from 15) */
    s32 unknown4f80;
    s32 file_count;               /* 0x4f84 */
    u8 scanned[2];                /* 0x4f88: per port */
    u8 unknown4f8a[2];
    u8 unknown4f8c[2];
    u8 ours[32];                  /* 0x4f8e: per listed file: carries this game's prefix */
    u8 file_slots[32];            /* 0x4fae */
    char prefix[13];              /* 0x4fce: this game's file name prefix */
    u8 pad4fdb[9];
    u8 present[2];                /* 0x4fe4: per port: card present */
    u8 mode;                      /* 0x4fe6 */
    u8 busy;                      /* 0x4fe7 */
    u8 present_shown[2];          /* 0x4fe8 */
    u8 pad4fea[2];
    s32 events[4];                /* 0x4fec: card event descriptors */
    char title[30];               /* 0x4ffc: save title line of the text file */
    u8 unknown501a;
    u8 unknown501b;
    u32 other_prefix[4];          /* 0x501c: the other file name prefix (13 bytes) */
    u8 pad502c[8];
} MenuCard;

/* A save file name prefix, its terminator included ("BISLPS-00800"). */
typedef struct CardPrefix {
    char name[13];
} CardPrefix;

LAYOUT_CHECK(MenuCardSizes, sizeof(MenuCardFile) == 0x5C && sizeof(MenuCard) == 0x5034 &&
                                OFFSET_OF(MenuCard, prefix) == 0x4FCE &&
                                OFFSET_OF(MenuCard, title) == 0x4FFC);

#endif
