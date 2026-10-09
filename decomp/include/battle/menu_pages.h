#ifndef BATTLE_MENU_PAGES_H
#define BATTLE_MENU_PAGES_H

#include "common.h"

/* The command menu's list pages (item, technique and gear pages): the
 * command names' text images (80070E2C's unit, 80076EA4) and the item list
 * page (8008CCCC's, 8008FE18). */

/* A text image's pixels. */
typedef struct TextImage {
    u32 *pixels;
} TextImage;

extern TextImage D_800C3E5C[10]; /* text images of battle messages 0-9: the decimal digits */
extern u32 *D_800D2DB0;      /* blank text image */

extern u8 D_800C33B4[16];    /* gear page glyph ids */
extern s32 D_800C33C4[16];   /* their x */
extern s32 D_800C3404[16];   /* their y */

extern u8 D_800C3DE0[8];     /* the entered combo steps' buttons */

void func_80076EA4(void);                       /* render the command names' text images */
void func_8008FE18(u8 column, u8 row, u8 open); /* build the item list page */

#endif
