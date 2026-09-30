#ifndef BATTLE_MENU_PAGES_H
#define BATTLE_MENU_PAGES_H

/* Command menu list pages (item, technique and gear pages) and their windows. */

#include "common.h"

extern u32 *D_800C3E5C[10]; /* decimal digit text images */
extern u32 *D_800D2DB0;      /* blank text image */
extern u8 D_800D2CC0[0x20];  /* item counts from the 17th entry (800d2cb0 + 16) */

u8 *func_80033818(s32 id);   /* item name */
void func_80076EA4(void);
void func_8008FE18(u8 column, u8 row, u8 open);

#endif
