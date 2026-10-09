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
/* Sheet positions of label images, x / 4 and y: per row pair, from entry 3
 * the portrait per slot (characters, then gears), from 6 the view names. */
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
void func_8002A428(s32 arg0);
void func_8001BD40(s32 arg0, s32 arg1);
void func_800263E4(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale, s32 flipX, s32 flipY);
s32 func_8002675C(void *sheet, s32 image, void *dst, s32 buffer, s32 x, s32 y, s32 scale);
void func_80033698(s32 x, s32 y);
void func_80039DB8(s32 id, s32 sound); /* play a sound effect */
u8 *func_800337E8(u8 id);
u8 *func_80033848(u8 id);  /* weapon name */
u8 *func_80033A2C(u8 id);  /* gear accessory name */
u8 *func_80033A5C(u8 id);  /* gear part name */
u8 *func_80033728(u8 *table, s32 index); /* message of a table */
u8 *func_80033818(u8 item);  /* item name text */
u8 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line; its width */
s32 func_80033B34(u8 *codes, u8 *text, s32 count); /* decode a name */

void func_801C65F4(void);
void func_801C6AA0(MenuState *state);
void func_801C6D4C(void);
void func_801C6D5C(void);
void func_801C6D90(void);
void func_801C6E0C(void);
void func_801C6E68(void);
void func_801C6F70(void);
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
void func_801CE198(s32 count, SVECTOR *verts, POLY_FT4 *polys, s32 first);
void func_801CE2B4(s32 count, POLY_FT4 *polys, s32 first);
void func_801CE338(void);
void func_801CE3C8(void);
void func_801CEC40(void);
void func_801CF308(void);
void func_801D09F0(s32 index, u8 full);
void func_801D0C78(void);
void func_801D0D90(void);
void func_801D0E20(void);
void func_801D0E38(void);
void func_801D0EBC(void);
void func_801D0ED4(void);
void func_801D0F54(void);
void func_801D0FD4(void);
void func_801D1030(void);
void func_801D10DC(void);
void func_801D1160(void);
void func_801D11F0(void);
void func_801D1258(void);
void func_801D1464(void);
void func_801D14B0(void);
void func_801D1B20(void);
void func_801D1BE8(void);
void func_801D1C48(void);
void func_801D1CA0(void);
void func_801D1D40(void);
void func_801D1E80(void);
void func_801D1EB0(void);
void func_801D1EE0(s32 index, u8 outline);
void func_801D3344(s32 x, s32 y, s32 h);
void func_801D3444(void);
void func_801D3674(void);
void func_801D36E0(MenuLabel *label, u8 slot, u8 gear, u8 mode);
void func_801D397C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 grow, u8 arg6, s32 arg7, u8 arg8);
void func_801D3B00(void);
void func_801D3C4C(u8 slot, u16 x, u16 y, s32 unused, u16 h);
void func_801D3DB0(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801D3FF8(u8 index, u16 x, u16 y, u16 w);
void func_801D433C(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801D4688(u8 index, u16 x, u16 y, u16 h);
void func_801D49D0(u8 index, u16 x, u16 y, u16 w, u16 h);
void func_801D4D1C(u8 image, u16 x, u16 y, u16 w, u16 h, u8 arg5, s32 arg6, u8 arg7);
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
