#ifndef OVL2601_ITEM_SHOP_H
#define OVL2601_ITEM_SHOP_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/menu.h"

/* The item shop (ovl2601): what its two units share, the menu screen code
 * (ovl2601.c) and the shop's own screens (shop.c). Its blocks are the menu
 * screens' (decomp/include/menu). */

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions
 * (decomp/src/resident/own_declarations.h). */
void func_80039DB8(s32 effect);            /* play a sound effect */
void func_80033698(s32 x, s32 y);          /* text palettes */
u8 *func_80033728(void *table, s32 index); /* entry of a text table */
u8 *func_80033848(s32 id);                 /* equipment name */
u8 *func_800337E8(s32 id);                 /* accessory name */
u8 *func_80033818(s32 id);                 /* item name */
void func_80033B34(u8 *codes, u8 *text, s32 count); /* codes to text */
s32 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line */
s32 func_8002675C(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale); /* sprite */
s32 func_800263E4(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* mirrored sprite */

/* The screen code's data and calls the shop's screens use (ovl2601.c). */
extern s32 D_801D1F50;   /* items the shop sells */
extern u8 D_801D1FD0[];  /* sell list label text ids */
extern u8 D_801D1FD4[];  /* buy list label text ids */
extern s32 D_801D1FE8[]; /* sell list label x offsets */
extern s32 D_801D21CC[]; /* member portrait x */
void func_801C50E8(u32 value);
void func_801C5A7C(MenuLabel *label, s32 index, s32 row, s32 mode);
void func_801C70B8(void);
void func_801C7314(u8 index);
void func_801C88E0(u8 index);
void func_801C896C(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void func_801C8C3C(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);
void func_801C8D58(s32 count, POLY_FT4 *packets, s32 first);
void func_801CAC7C(u8 sound);
void func_801CB014(void);
void func_801CB340(void);
void func_801CB384(u8 first);
void func_801CB7F4(void);
void func_801CBC88(u8 render, u8 count, MenuLabel *labels, u8 *text_ids, u8 *shown);
void func_801CBCF0(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row,
                   u8 mode);
void func_801CC278(u8 menu);
void func_801CC720(u8 menu);

/* The shop's data and calls the screen code uses (shop.c). */
extern s32 D_801D2250; /* second number x */
extern s32 D_801D2254; /* second number y */
void func_801CCFF4(void);
u8 func_801CF780(void);
u8 func_801D1CA4(void);
void func_801D1F10(void);

#endif
