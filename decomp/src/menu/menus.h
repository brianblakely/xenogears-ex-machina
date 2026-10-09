#ifndef MENU_MENUS_H
#define MENU_MENUS_H

#include "common.h"
#include "psyq/libgpu.h"

/* The menus (menu4 8007F834-800802A4, 800808F4-80081ECC): eight choice
 * menus on translucent panels, among them the settings, vibration and
 * options pages, whose draw callbacks show the values; the two captions,
 * the copy of the shown screen and the screen fades. */

/* A centred one-line caption: its text image and one sprite per buffer. */
typedef struct {
    u8 *image;         /* 0x00 */
    SPRT sprite[2];  /* 0x04 */
    s16 width;         /* 0x2C */
    s16 x;             /* 0x2E */
} Caption;

/* One line of a choice menu. */
typedef struct {
    u8 flags;          /* 0x00: 1 = runs every frame, 2 = pad to the widest
                        * line, 4 = skipped by the cursor */
    u8 caption;        /* 0x01: caption text while selected */
    u8 unk2[2];
    s32 text;          /* 0x04 */
    void (*handler)(); /* 0x08: run on confirm with arg (some take none) */
    s32 arg;           /* 0x0C */
    u8 unk10[2];
    s16 half_width;    /* 0x12 */
} MenuItem;

typedef struct Menu Menu;

/* A choice menu drawn on a translucent panel (eight in D_800915AC, 0x3C
 * bytes each); a page's draw callback adds its values column. */
struct Menu {
    u8 title_width;    /* 0x00: nonzero shifts the lines right */
    u8 unk1[3];
    MenuItem *items;   /* 0x04 */
    s16 count;         /* 0x08 */
    s16 parent;        /* 0x0A: menu index returned to on cancel */
    void (*draw)(Menu *menu); /* 0x0C */
    u8 unk10[2];
    s16 cursor;        /* 0x12 */
    s16 y;             /* 0x14 */
    s16 x;             /* 0x16 */
    u8 unk18[4];
    TILE panel[2];  /* 0x1C: one per buffer */
};

extern s32 D_800912F0;
extern char *D_800912F4[];
extern u8 D_80091300[];       /* frame rate of each rate setting */
extern s32 D_8009130C[];      /* value of each speed setting */
extern char *D_8009132C[];    /* names of the entries of setting 10 */
extern s32 D_80091364;        /* pad port of the menu input */
extern MenuItem D_80091368[2]; /* vibration choices, port 1 */
extern MenuItem D_80091390[2]; /* port 2 */
extern Menu D_800915AC[8];
extern u8 D_800928FC;         /* pad port driving the menus */
extern s32 D_80092924;

void func_8007F834(void);
void func_8007F8B4(void);
void func_8007F948(Menu *page, s32 entry);
char *func_8007F97C(void);
s32 func_8007FF70(s32 value, s32 max, s32 flags);
void func_80080964(s32 page);
s32 func_800809BC(void);
void func_800809D8(void);
void func_80080A58(void);
void func_80080AA0(s32 forget);
void func_80080AE8(void);
void func_80080B58(void);
void func_80080C48(s32 mode);
void func_80080D10(void);
void func_80080D20(void *ot);
void func_80080F04(void);
void func_80081100(s32 text, s32 lower);
void func_800811AC(void *ot);
void func_800814AC(void);
void func_80081A44(void);
void func_80081D2C(void);
void func_80081E00(void);
void func_80081E6C(void);

#endif
