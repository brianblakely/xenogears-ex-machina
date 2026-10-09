#ifndef BATTLE_MENU_PAGES_H
#define BATTLE_MENU_PAGES_H

/* Command menu list pages (item, technique and gear pages) and their windows. */

#include "common.h"

/* A text image's pixels. */
typedef struct TextImage {
    u32 *pixels;
} TextImage;

extern TextImage D_800C3E5C[10]; /* text images of battle messages 0-9: the decimal digits */
extern u32 *D_800D2DB0;      /* blank text image */
extern u8 D_800D2CC0[0x20];  /* item counts from the 17th entry (800d2cb0 + 16) */

extern u8 D_800C33B4[16];    /* gear page glyph ids */
extern s32 D_800C33C4[16];   /* their x */
extern s32 D_800C3404[16];   /* their y */

extern u8 D_800C3DE0[8];     /* the entered combo steps' buttons */

u8 *func_80033818(s32 id);   /* item name */
u8 *func_80033908(s32 id);   /* art name */
u8 *func_800339FC(s32 id);   /* gear art name */
u8 *func_800338D8(s32 id);   /* battle system text */
u8 *func_800339C8(s32 gear, s32 id); /* a gear's combo step text */
u8 *func_80033A8C(s32 id);   /* gear part text */
u8 *func_80033848(s32 id);   /* equipment name */
void func_80076EA4(void);
void func_8008FE18(u8 column, u8 row, u8 open);

#endif
