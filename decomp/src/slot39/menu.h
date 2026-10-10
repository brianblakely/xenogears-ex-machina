#ifndef SLOT39_MENU_H
#define SLOT39_MENU_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/menu.h"

/* The menu overlay's screen framework (slot39; the field menu, kind 0, the
 * title screen's file screen, kind 2, and the CD change, kind 6): the menu
 * loops and blocks, the frame and input, labels, sprites and quads, panels,
 * the command windows and the view, and the overlay data they share. The
 * memory card and file screen are in file.h, the field menu's character
 * screens in field.h. */

/* The sheet images of one command of a command window. */
typedef struct MenuCommandImages {
    s32 cursor; /* 0: cursor image (+d lit) */
    s32 label; /* 4 */
} MenuCommandImages;

/* The menu data archive (file 2 of directory 10h): packed files by index. */
typedef struct MenuDataArchive {
    s32 count; /* 0 */
    void *items; /* 4 */
    void *weapons; /* 8 */
    void *accessories; /* C */
    void *effects[11]; /* 10: per character */
    void *unk3C; /* 3C */
    void *unk40; /* 40 */
    void *engines; /* 44 */
    void *frames; /* 48 */
    void *parts; /* 4C */
    void *unk50; /* 50 */
    void *unk54; /* 54 */
    void *unk58; /* 58 */
    void *gears[20]; /* 5C: per gear */
    void *unkAC; /* AC */
    void *unkB0; /* B0 */
    void *padB4[8];
    void *unkD4[4]; /* D4 */
} MenuDataArchive;

extern s32 D_801EA1EC[]; /* per command: four choices of cursor and label images */
/* Sheet positions (u / 4, v) of the name images 801e8da8 renders: 0-2 the
 * party slots' characters, 3-5 their gears, from 6 the file views. */
extern s32 D_801EA578[19];
extern s32 D_801EA5C4[19];
extern s32 D_801E9A00[]; /* highlight positions: x */
extern s32 D_801E9A2C[]; /* y */
extern s32 D_801E9EC4[8];     /* label x (mode 1) */
extern u16 D_801E9EE4;        /* label y (mode 1) */
extern s32 D_801E9EE8[];      /* label x (modes 2, 5 from 8) */
extern s32 D_801E9F28[2];     /* label y per row (mode 2) */
extern s32 D_801E9F30[];      /* label y per row (mode 3) */
extern s32 D_801E9F68[2];     /* label x (mode 6) */
extern s32 D_801E9F70[];      /* label y (mode 6) */

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions (decomp/src/resident/own_declarations.h). */
void cd_set_mode(s32 mode);
void mode_get_random_byte_in_range(s32 low, s32 high);
void sprite_sheet_draw_scaled_flip(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale, s32 flipX, s32 flipY);
s32 sprite_sheet_draw_scaled(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale);
void text_load_palette(s32 x, s32 y);
void sound_play_effect_on_last_channels(s32 id, s32 sound); /* play a sound effect */
u8 *text_get_accessory_name(u8 id);
u8 *text_get_weapon_name(u8 id);  /* weapon name */
u8 *text_get_gear_accessory_name(u8 id);  /* gear accessory name */
u8 *text_get_gear_part_name(u8 id);  /* gear part name */
u8 *text_get_resource_entry(u8 *table, s32 index); /* message of a table */
u8 *text_get_item_name(u8 item);  /* item name text */
u8 window_render_text_line(u8 *text, void *pixels, s32 width, s32 line); /* render a text line; its width */
s32 text_decode_codes(u8 *codes, u8 *text, s32 count); /* decode a name */

/* The framework's functions that another unit calls, or its own before
 * defining them. */
void func_801C72BC(u8 arg0);
void func_801C7B0C(void);
void func_801C7BF4(void);
void func_801C7D78(void);
void func_801C7F34(u32 frames);
void func_801C80B8(u32 value);
void func_801C8164(POLY_G4 *poly, u8 r, u8 g, u8 b);
void func_801C851C(SVECTOR *v, u16 x, u16 y, u16 w, u16 h);
void func_801C8574(s32 sound);
u16 func_801C8640(u16 flags, u8 bit);
u16 func_801C865C(u16 flags, u8 bit);
u32 func_801C8678(u32 flags, u8 bit);
void func_801D1CA0(void);
void func_801D1D40(void);
void func_801D1E80(void);
void func_801D1EB0(void);
void func_801D1EE0(s32 index, u8 outline);
void func_801D3344(s32 x, s32 y, s32 h);
void func_801D3444(void);
void func_801D3674(void);
void func_801D36E0(MenuLabel *label, u8 slot, u8 gear, u8 mode);
void func_801D397C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void func_801D3B00(void);
void func_801D4D1C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar);
void func_801D4EA0(u8 slot);
void func_801D5BA4(s32 x, s32 y);
void func_801D5CF8(s32 x, s32 y);
void func_801E53CC(u8 index);
void func_801E7C50(MenuLabel *label, s32 index, s32 first, u8 mode);
void func_801E7E68(MenuLabel *labels, u8 *layout, s32 first, s32 count);
void func_801E8018(u8 count, MenuLabel *labels, u8 *table, u8 *flags);
void func_801E8044(u8 count, u8 *flags);
void func_801E8070(u8 count, MenuLabel *labels, u8 *table, s32 *offsets, u8 *flags, u8 selected, u8 row,
                   u8 mode);
void func_801E8474(s32 count, MenuCommandImages *images);
void func_801E86C8(u8 offset);
void func_801E8978(u8 count, u8 cursor, MenuCommandImages *images);
void func_801E8B4C(u8 offset);
void func_801E8DA8(u8 image, u8 row);
void func_801E8EAC(POLY_FT4 *poly, u8 mode);
void func_801E8F60(u8 index, u8 dim);
void func_801E91C4(POLY_FT4 *poly);
void func_801E920C(POLY_FT4 *poly, u16 x, u16 y, u8 u, u8 v, u16 w, u16 h);
void func_801E927C(POLY_FT4 *poly);

#endif
