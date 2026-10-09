#ifndef OVL2602_GEAR_SHOP_H
#define OVL2602_GEAR_SHOP_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/menu.h"
#include "menu/tables.h"

/* The Gear parts shop (ovl2602): what its units share, the menu screen code
 * (ovl2602.c), the Gear screen and shop (gear_shop.c) and the commons
 * (ovl2602_common.c), and the Gear model code outside the overlay that it
 * calls. Its other blocks are the menu screens' (decomp/include/menu). */

/*
 * The gear screen block (MenuState gear_screen, 1f00h bytes): its backdrop
 * and part pictures, and three animated sprite groups: a flicker of three
 * sprites, two lamps that open, idle and close, and a third indicator.
 * States: 0 off, 1 opening, 2 idle, 3 closing (the indicator has 4 steps).
 */
typedef struct GearScreen {
    POLY_FT4 backdrop[2];      /* 0000: drawn with the menu's buffer */
    u8 unk50[0x30];
    POLY_FT4 packets[2];       /* 0080 */
    POLY_FT4 flicker[3][4];    /* 00d0 */
    POLY_FT4 lamps[66];        /* 02b0: 22 per lamp; lamps 0 and 1 */
    POLY_FT4 indicator[36];    /* 0d00 */
    POLY_FT4 frame[28];        /* 12a0 */
    POLY_FT4 parts[5][10];     /* 1700 */
    u8 flicker_count;          /* 1ed0 */
    u8 lamp_count[3];          /* 1ed1: [2] the indicator's */
    u8 flicker_buffer;         /* 1ed4 */
    u8 lamp_buffer[3];         /* 1ed5 */
    u8 flicker_shown;          /* 1ed8 */
    u8 lamp_state[3];          /* 1ed9: [2] the indicator's */
    u8 flicker_timer;          /* 1edc */
    u8 lamp_timer[3];          /* 1edd */
    u8 flicker_frame;          /* 1ee0 */
    u8 buffer;                 /* 1ee1 */
    s16 indicator_frame;       /* 1ee2 */
    s16 lamp_frame[2];         /* 1ee4 */
    u8 part_count[5];          /* 1ee8 */
    u8 parts_buffer;           /* 1eed */
    u8 unk1EEE[2];
    u16 flicker_x, flicker_y;  /* 1ef0 */
    u16 lamp_x[2];             /* 1ef4 */
    u16 lamp_y[2];             /* 1ef8 */
    u16 indicator_x, indicator_y; /* 1efc */
} GearScreen;

/* A model part block (MenuState model_parts[]). */
typedef struct ModelParts {
    void *data0; /* 00 */
    void *data1; /* 04 */
    s16 position[3]; /* 08: initial actor position */
    u8 unkE[4];
    u8 unk12;    /* 12 */
} ModelParts;

/* The camera's move between two points (the common D_801D9050). */
typedef struct CameraMove {
    s32 from[3];     /* 00: previous target */
    s32 to[3];       /* 0c: target */
    s32 step[3];     /* 18: 16.16 step per frame */
    s32 offset[3];   /* 24: 16.16 distance travelled */
    u8 negative[3];  /* 30: moving towards smaller coordinates */
    u8 frames;       /* 33: steps per update */
} CameraMove;

/* The Gear model code's state (801e8674, outside this overlay). */
typedef struct ModelView {
    u8 unk0[0x54];
    s16 unk54;    /* 54 */
    s16 distance; /* 56: camera distance */
} ModelView;

typedef struct ModelState {
    u8 unk0[4];
    ModelView *view; /* 04 */
    u8 unk8[0x1C - 8];
    s16 unk1C;       /* 1c */
    u8 unk1E[0x60 - 0x1E];
    s16 unk60;       /* 60 */
} ModelState;

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions
 * (decomp/src/resident/own_declarations.h). */
void func_80039DB8(s32 effect);            /* play a sound effect */
void func_80033698(s32 x, s32 y);          /* text palettes */
u8 *func_80033728(void *table, s32 index); /* entry of a text table */
u8 *func_80033A2C(s32 id);                 /* kind 3 part name */
u8 *func_80033A5C(s32 id);                 /* kind 4 part name */
void func_80033B34(u8 *codes, u8 *text, s32 count); /* codes to text */
s32 func_80034EAC(u8 *text, void *pixels, s32 width, s32 line); /* render a text line */
s32 func_8002675C(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale); /* sprite */
s32 func_800263E4(void *sheet, s32 id, void *packets, s32 buffer, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* mirrored sprite */
u8 func_8001BD40(u8 low, u8 high); /* random number in [low, high] */

/* The Gear model code (801e7xxx-801e8xxx, outside this overlay) and its state. */
extern MATRIX *D_801E8644;        /* the model code's light colour matrix */
extern ModelState *D_801E8670[2]; /* per model slot */
extern ModelState *D_801E8674;
void func_801E738C(s32 unk0); /* model code setup */
void func_801E742C(s32 index, u16 flags, void *script, void *file, s16 x, s16 y, s16 z, s16 w, s16 *pos);
void func_801E7D14(void *a, void *b, u32 *ot, s32 buffer);
void func_801E7FD4(void);
void func_801E8030(s32 unk0);
void func_801E8330(u16 index, u16 mask, s32 variant);

/* The screen code's data and calls the Gear screen uses (ovl2602.c). */
extern u8 D_801D697C;    /* the model values debug display is on */
extern u8 D_801D6A24[];  /* sell list label text ids */
extern u8 D_801D6A2C[];  /* buy list label text ids */
extern s32 D_801D6A40[]; /* gear list label x offsets */
extern s32 D_801D6C44[]; /* member portrait x */
u32 func_801C5260(u8 id);
u32 func_801C527C(u32 mask, u8 id);
void func_801C5298(u32 value);
void func_801C5CA8(MenuLabel *label, s32 index, s32 row, s32 mode);
void func_801C782C(void);
void func_801C7A88(u8 index);
void func_801C9054(u8 index);
void func_801C90E0(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void func_801C93B0(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);
void func_801C94CC(s32 count, POLY_FT4 *packets, s32 first);
void func_801CB498(u8 sound);
void func_801CB690(void);
void func_801CC1C4(void);
void func_801CCE90(u8 count, MenuLabel *labels, u8 *text_ids, u8 *shown);
void func_801CCEBC(u8 count, u8 *shown);
void func_801CCEE8(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row,
                   u8 mode);
void func_801CD564(u8 menu);

/* The Gear screen's data and calls the screen code uses (gear_shop.c), and
 * the gear summary and rebuild steps that its unit calls before defining
 * them. */
extern s32 D_801D6FD8; /* available members 1-10 */
void func_801CE1D0(void);
void func_801CE2E8(void);
void func_801CE7E0(void);
void func_801CE82C(void);
void func_801CF33C(void);
void func_801CFAB8(u8 unk0, u8 id);
void func_801D0398(u8 back);
u8 func_801D5828(void);
void func_801D5D38(void);
void func_801D5F94(MenuTables *table, u8 id);
void func_801D6150(MenuTables *table, u8 id);
void func_801D61B8(MenuTables *table, u8 id);
void func_801D6250(MenuTables *table, u8 id);
void func_801D62A4(MenuTables *table, u8 id);
void func_801D6334(MenuTables *table, u8 id);
void func_801D6738(MenuTables *table, u8 id);
u8 func_801D690C(u8 id);

/* The overlay's commons (ovl2602_common.c, which defines them ahead of this
 * header). */
extern CameraMove D_801D9050;
extern u8 D_801D9084;     /* gear being edited */
extern u8 *D_801D9088;    /* name pixel buffer */
extern s32 D_801D908C[5]; /* entries in each of the five gear part lists */

#endif
