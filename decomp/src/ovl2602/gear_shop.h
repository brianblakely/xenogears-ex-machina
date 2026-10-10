#ifndef OVL2602_GEAR_SHOP_H
#define OVL2602_GEAR_SHOP_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/menu.h"
#include "menu/tables.h"
#include "ovl2143/actors.h"

/* The Gear parts shop (ovl2602): what its units share, the menu screen code
 * (gear_shop_framework.c), the Gear screen and shop (gear_shop.c) and the commons
 * (gear_shop_common.c), and its view of the actor module that draws the gear
 * (ovl2143/actors.h). Its other blocks are the menu screens'
 * (decomp/include/menu). */

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

/* The camera's move between two points (the common gear_shop_camera_move). */
typedef struct CameraMove {
    s32 from[3];     /* 00: previous target */
    s32 to[3];       /* 0c: target */
    s32 step[3];     /* 18: 16.16 step per frame */
    s32 offset[3];   /* 24: 16.16 distance travelled */
    u8 negative[3];  /* 30: moving towards smaller coordinates */
    u8 frames;       /* 33: steps per update */
} CameraMove;

/* Resident calls declared here: these callers convert arguments or results
 * differently from the resident definitions
 * (decomp/src/resident/own_declarations.h). */
void sound_play_effect_on_last_channels(s32 effect);                                                   /* play a sound effect */
void text_load_palette(s32 x, s32 y);                                                                  /* text palettes */
u8 *text_get_resource_entry(void *resource, s32 index);                                                /* entry of a text table */
u8 *text_get_gear_accessory_name(s32 index);                                                           /* kind 3 part name */
u8 *text_get_gear_part_name(s32 index);                                                                /* kind 4 part name */
void text_decode_codes(u8 *codes, u8 *out, s32 count);                                                 /* codes to text */
s32 window_render_text_line(u8 *text, void *image, s32 width, s32 flags);                              /* render a text line */
s32 sprite_sheet_draw_scaled(void *sheet, s32 id, void *prims, s32 index, s32 x, s32 y, s32 scale);    /* sprite */
s32 sprite_sheet_draw_scaled_flip(void *sheet, s32 id, void *prims, s32 index, s32 x, s32 y, s32 scale, s32 flip_x,
                  s32 flip_y); /* mirrored sprite */
u8 mode_get_random_byte_in_range(u8 low, u8 high); /* random number in [low, high] */

/* The actor module (ovl2143/actors.h), whose actor 1 is the gear shown. Its
 * draw, which each target declares itself: this screen passes four
 * arguments where the module takes five (the fifth, the frames elapsed, is
 * read from a stack slot this call does not write), with its light record,
 * whose first 0x20 bytes the module reads as the light matrix. */
void gear_model_step_and_draw(MATRIX *m, MenuLight *light, u32 *ot, s32 buffer);

/* The screen code's data and calls the Gear screen uses (gear_shop_framework.c). */
extern u8 gear_shop_debug_values_on;           /* the model values debug display is on */
extern u8 gear_shop_choice_label_ids[];        /* sell list label text ids */
extern u8 gear_shop_buy_label_ids[];           /* buy list label text ids */
extern s32 gear_shop_choice_label_x_offsets[]; /* gear list label x offsets */
extern s32 gear_shop_portrait_x_table[];       /* member portrait x */
u32 gear_shop_get_bit_mask32(u8 id);
u32 gear_shop_test_bit32(u32 mask, u8 id);
void gear_shop_split_digits(u32 value);
void gear_shop_label_init_quads(MenuLabel *label, s32 index, s32 row, s32 mode);
void gear_shop_scroll_bar_hide(void);
void gear_shop_list_cursor_free(u8 index);
void gear_shop_panel_close(u8 index);
void gear_shop_panel_open(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar);
void gear_shop_draw_projected_quads(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first);
void gear_shop_draw_quads(s32 count, POLY_FT4 *packets, s32 first);
void gear_shop_play_sound(u8 sound);
void gear_shop_camera_plan_move(void);
void gear_shop_run_frame(void);
void gear_shop_label_render_table(u8 count, MenuLabel *labels, u8 *text_ids, u8 *shown);
void gear_shop_label_clear_shown(u8 count, u8 *shown);
void gear_shop_label_place(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row,
                   u8 mode);
void gear_shop_choice_window_open(u8 menu);

/* The Gear screen's data and calls the screen code uses (gear_shop.c), and
 * the gear summary and rebuild steps that its unit calls before defining
 * them. */
extern s32 gear_shop_available_member_count; /* available members 1-10 */
void gear_shop_member_marks_layout(void);
void gear_shop_member_marks_close(void);
void gear_shop_draw_model(void);
void gear_shop_draw_gear_screen(void);
void gear_shop_animate_gear_screen(void);
void gear_shop_model_load_gear(u8 slot, u8 gear);
void gear_shop_switch_member(u8 back);
u8 gear_shop_choice_list_run(void);
void gear_shop_model_init(void);
void gear_shop_compute_gear_summary(MenuTables *table, u8 id);
void gear_shop_rebuild_gear_values(MenuTables *table, u8 id);
void gear_shop_set_gear_engine_values(MenuTables *table, u8 id);
void gear_shop_set_gear_frame_values(MenuTables *table, u8 id);
void gear_shop_set_gear_part_values(MenuTables *table, u8 id);
void gear_shop_sum_gear_accessories(MenuTables *table, u8 id);
void gear_shop_set_gear_weapon_values(MenuTables *table, u8 id);
u8 gear_shop_compute_gear_speed_penalty(u8 id);

/* The overlay's commons (gear_shop_common.c, which defines them ahead of this
 * header). */
extern CameraMove gear_shop_camera_move;
extern u8 gear_shop_edited_gear;           /* gear being edited */
extern u8 *gear_shop_name_pixels;          /* name pixel buffer */
extern s32 gear_shop_stock_list_counts[5]; /* entries in each of the five gear part lists */

#endif
