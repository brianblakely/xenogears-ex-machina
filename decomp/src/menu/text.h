#ifndef MENU_TEXT_H
#define MENU_TEXT_H

#include "common.h"
#include "psyq/libgpu.h"

/* The menu's text (menu4 8007E528-8007EEE8): the banner sprite, the font
 * and its text quads, the text cursor, scale and colour, and the message
 * table the captions and scenes draw from. */

/* Glyph of the menu font. */
typedef struct {
    u8 u, v;   /* texture position */
    u8 width;
    u8 height;
} Glyph;

/* A sprite with its texture page, one per draw buffer. */
typedef struct {
    DR_TPAGE tpage;
    SPRT sprite;
} SceneSprite;

/* Image data of the menu (+0x3C: the font TIM, +0x64: the banner TIM). */
typedef struct {
    u8 unk0[0x3C];
    u_long *font;
    u8 unk40[0x24];
    u_long *banner;
} MenuFiles;

extern Glyph D_80091230[]; /* menu font glyphs: digits, capitals, punctuation */
extern s32 D_800912DC;     /* text width scale (0x100 = 1) */
extern s32 D_80092880;     /* the menu's message table (func_80033728) */

void func_8007E528(s32 state);
void func_8007E574(void *ot);
void func_8007E634(MenuFiles *files);
void func_8007E894(s32 x, s32 y);
Glyph *func_8007E8AC(s32 ch);
void func_8007E954(s32 value);
s32 func_8007E964(s32 ch);
s32 func_8007EB6C(u8 *text);
void func_8007EBE0(u8 *text);
void func_8007EC54(u8 *text);
void func_8007ECF0(u8 *text);
void func_8007EE08(s32 highlight);
void func_8007EE68(s32 highlight);

#endif
