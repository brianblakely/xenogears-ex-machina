/*
 * Gear screen and shop code: the model and parts panel, member switching,
 * stock and sale lists, fitting and refuelling. Rodata 801c5038-801c511c,
 * text 801ce1d0-801d697c, data 801d6d08-801d7108 and its variable at
 * 801d904c, before the commons. A separate translation unit from the
 * screen code: these callers pass quad coordinates as words, while the
 * shared screen unit's local calls see the helper's narrow definition.
 */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/model.h"
#include "resident/text.h"
#include "menu/screen.h"
#include "menu/shop.h"
#include "menu/tables.h"
#include "gear_shop.h"

/* Headings: two sets of four sprites and the two alternative ones, with
 * their positions. */
u8 gear_shop_buy_heading_images[8] = {0xF2, 0xDE, 0xF3, 0xE5, 0xE5, 0xE3, 0xE5, 0xE5}; /* 801D6D08 */
u8 gear_shop_sell_heading_images[2] = {0xF2, 0xE3}; /* 801D6D10 */
s32 gear_shop_buy_heading_x_table[8] = {150, 48, 48, 224, 208, 208, 208, 208}; /* 801D6D14: x */
s32 gear_shop_sell_heading_x_table[2] = {150, 224}; /* 801D6D34 */
s32 gear_shop_buy_heading_y_table[8] = {158, 190, 198, 88, 72, 80, 72, 72}; /* 801D6D3C: y */
s32 gear_shop_sell_heading_y_table[2] = {158, 88}; /* 801D6D5C */
/* The three nine-digit numbers' positions. */
s32 gear_shop_gold_x = 232; /* 801D6D64 */
s32 gear_shop_gold_y = 78; /* 801D6D68 */
s32 gear_shop_total_x = 232; /* 801D6D6C */
s32 gear_shop_total_y = 88; /* 801D6D70 */
s32 gear_shop_new_gold_x = 232; /* 801D6D74 */
s32 gear_shop_new_gold_y = 100; /* 801D6D78 */

/* Per gear (17; gear 7 has no model values): the model's first file id,
 * variant and two model values. */
u16 gear_shop_model_file_ids[17] = { /* 801D6D7C */
    0x6BA, 0x70E, 0x6BC, 0x6DC, 0x6D4, 0x6DE, 0x6E8, 0x702, 0x6E4,
    0x700, 0x6BA, 0x710, 0x712, 0x714, 0x716, 0x72C, 0x6DC,
};
u8 gear_shop_model_variants[17] = {13, 4, 12, 4, 7, 8, 10, 0, 13, 4, 1, 4, 4, 4, 4, 4, 4}; /* 801D6DA0 */
u16 gear_shop_model_field60_values[17] = { /* 801D6DB4 */
    200, 200, 192, 192, 204, 192, 192, 0, 200,
    215, 200, 187, 187, 178, 178, 210, 192,
};
u16 gear_shop_model_field1c_values[17] = { /* 801D6DD8 */
    240, 215, 243, 244, 235, 231, 221, 0, 211,
    178, 250, 212, 195, 213, 189, 180, 244,
};
/* The camera's targets: x per gear; y per gear, command (1-3) and list
 * entry; the distance per command and list entry. gear_shop_gear_view_open indexes the
 * last two from the command's row, one row before their first entries. */
s16 gear_shop_camera_x_targets[17] = { /* 801D6DFC */
    352, 352, 312, 352, 352, 400, 596, 0, 564,
    300, 352, 452, 500, 508, 676, 396, 352,
};
s16 gear_shop_camera_y_targets[17 * 12] = { /* 801D6E20 */
    128, 0, 0, 0, 128, 0, 0, 0, 64, 0, 64, -48,
    128, 0, 0, 0, 128, 0, 0, 0, 84, 0, 84, -40,
    128, 12, 0, 0, 128, 12, 0, 0, 96, 0, 96, -40,
    128, 0, 0, 0, 128, 0, 0, 0, 88, 0, 88, -60,
    120, -20, 0, 0, 120, -20, 0, 0, 84, 0, 84, -64,
    100, -30, 0, 0, 100, -30, 0, 0, 72, 0, 72, -70,
    76, -8, 0, 0, 76, -8, 0, 0, 40, 0, 40, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    136, -4, 0, 0, 136, -4, 0, 0, 84, 0, 84, 0,
    68, 116, 0, 0, 68, 116, 0, 0, 40, 0, 40, 20,
    128, 0, 0, 0, 128, 0, 0, 0, 64, 0, 64, -48,
    100, 0, 0, 0, 100, 0, 0, 0, 76, 0, 76, -50,
    96, 0, 0, 0, 96, 0, 0, 0, 68, 0, 68, -50,
    88, 0, 0, 0, 88, 0, 0, 0, 64, 0, 64, -58,
    56, 0, 0, 0, 56, 0, 0, 0, 28, 0, 28, -52,
    68, -20, 0, 0, 68, -20, 0, 0, 36, 0, 36, -44,
    128, 0, 0, 0, 128, 0, 0, 0, 88, 0, 88, -60,
};
s16 gear_shop_camera_distance_targets[3 * 4] = { /* 801D6FB8 */
    1024, 0, 0, 0,
    -1024, -2048, 0, 0,
    1024, -1024, -1024, 1024,
};

/* The two second markers' x. */
s32 gear_shop_member_mark_x_table[2] = {16, 276}; /* 801D6FD0 */
s32 gear_shop_available_member_count = 0; /* 801D6FD8: available members 1-10 */
s32 gear_shop_current_member_index = 0; /* 801D6FDC: index of the gear screen's member among the available ones */

/* The two lamps: their sprite ids, four per frame and five frames per lamp
 * (0xFFFF none). */
u16 gear_shop_lamp_frame_images[40] = { /* 801D6FE0 */
    0x169, 0xFFFF, 0xFFFF, 0xFFFF,
    0x169, 0x16A, 0xFFFF, 0xFFFF,
    0x169, 0x16A, 0x16B, 0xFFFF,
    0x169, 0x16A, 0x16B, 0x16C,
    0x169, 0x16A, 0x16B, 0x16D,
    0x169, 0xFFFF, 0xFFFF, 0xFFFF,
    0x169, 0x16E, 0xFFFF, 0xFFFF,
    0x169, 0x16E, 0x16F, 0xFFFF,
    0x169, 0x16E, 0x16F, 0x170,
    0x169, 0x16E, 0x16F, 0x171,
};
/* Lamp and indicator position per command and list cursor. */
u8 gear_shop_lamp_positions_by_cursor[16] = { /* 801D7030 */
    0xFF, 0xFF, 0xFF, 0xFF,
    0, 1, 0xFF, 0xFF,
    0, 1, 0xFF, 0xFF,
    2, 3, 4, 5,
};
u16 gear_shop_lamp_x_table[2] = {100, 230}; /* 801D7040: lamp x */
/* Lamp y choices, six per lamp. */
u16 gear_shop_lamp_y_choices[12] = { /* 801D7044 */
    50, 66, 82, 98, 114, 130,
    150, 134, 118, 102, 86, 70,
};
u16 gear_shop_indicator_x_choices[6] = {238, 238, 150, 150, 50, 50}; /* 801D705C: indicator x choices */
u16 gear_shop_indicator_y_choices[6] = {42, 150, 42, 150, 42, 150};  /* 801D7068: indicator y choices */
u16 gear_shop_flicker_x_choices[6] = {42, 42, 100, 100, 150, 150}; /* 801D7074: flicker x choices */
u16 gear_shop_flicker_y_choices[6] = {110, 30, 110, 30, 110, 30};  /* 801D7080: flicker y choices */

/* The gear parts frame: sprite ids and positions. */
u16 gear_shop_gear_values_frame_images[14] = {0x11, 0x19, 0x3E, 0xF, 0x1E, 0xE, 0x15, 0x3E, 0x20, 0xE, 0x12, 0x10, 0x11, 0x1D}; /* 801D708C */
u16 gear_shop_gear_values_frame_x_table[14] = {210, 218, 250, 210, 218, 226, 234, 250, 210, 218, 226, 234, 242, 250}; /* 801D70A8 */
u16 gear_shop_gear_values_frame_y_table[14] = {50, 50, 58, 66, 66, 66, 66, 74, 82, 82, 82, 82, 82, 82}; /* 801D70C4 */
/* Gear value positions (x, y). */
u16 gear_shop_gear_hp_x = 210; /* 801D70E0 */
u16 gear_shop_gear_hp_y = 58; /* 801D70E2 */
u16 gear_shop_gear_max_hp_x = 258; /* 801D70E4 */
u16 gear_shop_gear_max_hp_y = 58; /* 801D70E6 */
u16 gear_shop_gear_fuel_x = 218; /* 801D70E8 */
u16 gear_shop_gear_fuel_y = 74; /* 801D70EA */
u16 gear_shop_gear_max_fuel_x = 258; /* 801D70EC */
u16 gear_shop_gear_max_fuel_y = 74; /* 801D70EE */
u16 gear_shop_gear_value68_x = 234; /* 801D70F0 */
u16 gear_shop_gear_value68_y = 90; /* 801D70F2 */
/* The pilot of each gear. */
u8 gear_shop_gear_pilots[20] = {0, 0, 1, 2, 3, 4, 5, 7, 8, 6, 1, 9, 3, 4, 5, 0, 9, 15, 15, 15}; /* 801D70F4 */

/* The unit's own uninitialized variable, after gear_shop_framework.c's in the file
 * (zero there); the overlay's commons follow (gear_shop_common.c). */
static u16 gear_shop_held_count; /* 801D904C: count of the item last looked up */

/* This unit passes quad coordinates as words; gear_shop_framework.c defines the helper
 * with u16 parameters, and that prototype here would mask them (four more
 * 16-bit masks: two in gear_shop_member_marks_layout, one each in gear_shop_layout_sell_rows and
 * gear_shop_layout_stock_rows). */
void gear_shop_set_rect_verts();

/* 801CE1D0: Draw the two second-marker sprites and set their four quads. */
void gear_shop_member_marks_layout(void) {
    POLY_FT4 *poly;
    MenuMarkerQuads *marks;
    s32 i;

    for (i = 0; i < 2; i++) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, i + 0x164, &menu_state_current->marks->polys[i * 4],
                      menu_state_current->buffer_index, gear_shop_member_mark_x_table[i], 0x64, 0x1000);
    }
    for (i = 0; i < 4; i++) {
        marks = menu_state_current->marks;
        poly = &marks->polys[i * 2 + menu_state_current->buffer_index];
        gear_shop_set_rect_verts(&menu_state_current->marks->verts[i * 4], poly->x0, poly->y0, poly->x1 - poly->x0,
                      poly->y3 - poly->y0);
    }
    menu_state_current->marks->buffer = menu_state_current->buffer_index;
}

/* 801CE2E8: Hide the second marker block, let a frame pass, and release it. */
void gear_shop_member_marks_close(void) {
    menu_state_current->flags->marks_shown = 0;
    gear_shop_run_frame();
    heap_free(menu_state_current->marks);
}

/* 801CE32C: Draw the shop's detail packets: member bars, portraits, headings, list rows, labels, numbers and prices. */
void gear_shop_draw_details(void) {
    s32 i;

    if (menu_state_current->flags->unknown5a[0] != 0) {
        for (i = 0; i < 9; i++) {
            if (menu_state_current->details->bar_shown[i] != 0) {
                AddPrim(&menu_state_current->current->ot[4],
                        &menu_state_current->details->bar_upper[i * 2 + menu_state_current->buffer_index]);
                AddPrim(&menu_state_current->current->ot[4],
                        &menu_state_current->details->bar_lower[i * 2 + menu_state_current->buffer_index]);
            }
        }
        gear_shop_draw_quads(menu_state_current->details->heading_count, menu_state_current->details->heading,
                      menu_state_current->details->heading_buffer);
        gear_shop_draw_quads(menu_state_current->details->group2D0_count, menu_state_current->details->group2D0,
                      menu_state_current->details->group2D0_buffer);
        gear_shop_draw_quads(menu_state_current->details->members_count, menu_state_current->details->members,
                      menu_state_current->details->members_buffer);
        for (i = 0; i < 8; i++) {
            if (menu_state_current->details->name_shown[i] != 0) {
                gear_shop_draw_projected_quads(1, menu_state_current->details->names_a[i].verts, menu_state_current->details->names_a[i].polys,
                              menu_state_current->details->names_a[i].buffer);
                gear_shop_draw_projected_quads(1, menu_state_current->details->names_b[i].verts, menu_state_current->details->names_b[i].polys,
                              menu_state_current->details->names_b[i].buffer);
            }
        }
        if (menu_state_current->details->label4530_shown != 0) {
            gear_shop_draw_projected_quads(1, menu_state_current->details->label4530.verts, menu_state_current->details->label4530.polys,
                          menu_state_current->details->label4530.buffer);
        }
        if (menu_state_current->details->label4430_shown != 0) {
            gear_shop_draw_projected_quads(1, menu_state_current->details->label4430.verts, menu_state_current->details->label4430.polys,
                          menu_state_current->details->label4430.buffer);
        }
        if (menu_state_current->details->label44B0_shown != 0) {
            gear_shop_draw_projected_quads(1, menu_state_current->details->label44B0.verts, menu_state_current->details->label44B0.polys,
                          menu_state_current->details->label44B0.buffer);
        }
        if (menu_state_current->details->label45B0_shown != 0) {
            gear_shop_draw_projected_quads(1, menu_state_current->details->label45B0.verts, menu_state_current->details->label45B0.polys,
                          menu_state_current->details->label45B0.buffer);
        }
        if (menu_state_current->details->digits_shown != 0) {
            AddPrim(&menu_state_current->current->ot[4], &menu_state_current->details->frame[menu_state_current->buffer_index]);
            gear_shop_draw_quads(menu_state_current->details->digits1_count, menu_state_current->details->digits1,
                          menu_state_current->details->digits1_buffer);
            gear_shop_draw_quads(menu_state_current->details->digits2_count, menu_state_current->details->digits2,
                          menu_state_current->details->digits2_buffer);
            gear_shop_draw_quads(menu_state_current->details->digits3_count, menu_state_current->details->digits3,
                          menu_state_current->details->digits3_buffer);
            if (menu_state_current->details->digits4_shown != 0) {
                gear_shop_draw_quads(menu_state_current->details->digits4_count, menu_state_current->details->digits4,
                              menu_state_current->details->digits4_buffer);
            }
        }
        for (i = 0; i < 8; i++) {
            gear_shop_draw_quads(menu_state_current->details->row_count[i], menu_state_current->details->rows[i],
                          menu_state_current->details->row_buffer[i]);
        }
        for (i = 0; i < 9; i++) {
            gear_shop_draw_quads(menu_state_current->details->cells_a_count[i], menu_state_current->details->cells_a[i],
                          menu_state_current->details->cells_a_buffer[i]);
            gear_shop_draw_quads(menu_state_current->details->cells_b_count[i], menu_state_current->details->cells_b[i],
                          menu_state_current->details->cells_b_buffer[i]);
        }
    }
    if (menu_state_current->flags->unknown5a[1] == 1) {
        gear_shop_draw_quads(menu_state_current->details->group1220_count, menu_state_current->details->group1220,
                      menu_state_current->details->group1220_buffer);
    }
    if (menu_state_current->flags->price_shown != 0) {
        gear_shop_draw_quads(menu_state_current->details->price_count, menu_state_current->details->price,
                      menu_state_current->details->price_buffer);
    }
}

/* 801CE7E0: Draw the separately loaded model when shown. */
void gear_shop_draw_model(void) {
    if (menu_state_current->flags->model_shown != 0) {
        gear_model_step_and_draw(&menu_state_current->matrix2, &menu_state_current->light, menu_state_current->current->ot_big, menu_state_current->buffer_index);
    }
}

/* 801CE82C: Draw the gear screen's animated sprites, then its backdrop, frame and part pictures when shown. */
void gear_shop_draw_gear_screen(void) {
    s32 i;

    if (menu_state_current->flags->gear_shown != 0) {
        if (menu_state_current->gear_screen->flicker_shown != 0) {
            gear_shop_draw_quads(menu_state_current->gear_screen->flicker_count, menu_state_current->gear_screen->flicker[0],
                          menu_state_current->gear_screen->flicker_buffer);
            gear_shop_draw_quads(menu_state_current->gear_screen->flicker_count, menu_state_current->gear_screen->flicker[1],
                          menu_state_current->gear_screen->flicker_buffer);
            gear_shop_draw_quads(menu_state_current->gear_screen->flicker_count, menu_state_current->gear_screen->flicker[2],
                          menu_state_current->gear_screen->flicker_buffer);
        }
        for (i = 0; i < 2; i++) {
            if (menu_state_current->gear_screen->lamp_state[i] != 0) {
                gear_shop_draw_quads(menu_state_current->gear_screen->lamp_count[i], &menu_state_current->gear_screen->lamps[i * 22],
                              menu_state_current->gear_screen->lamp_buffer[i]);
            }
        }
        if (menu_state_current->gear_screen->lamp_state[2] != 0) {
            gear_shop_draw_quads(menu_state_current->gear_screen->lamp_count[2], menu_state_current->gear_screen->indicator,
                          menu_state_current->gear_screen->lamp_buffer[2]);
        }
    }
    if (menu_state_current->flags->gear_parts_shown != 0) {
        gear_shop_draw_quads(1, menu_state_current->gear_screen->backdrop, menu_state_current->buffer_index);
        gear_shop_draw_quads(0xE, menu_state_current->gear_screen->frame, menu_state_current->gear_screen->parts_buffer);
        gear_shop_draw_quads(menu_state_current->gear_screen->part_count[0], menu_state_current->gear_screen->parts[0],
                      menu_state_current->gear_screen->parts_buffer);
        gear_shop_draw_quads(menu_state_current->gear_screen->part_count[1], menu_state_current->gear_screen->parts[1],
                      menu_state_current->gear_screen->parts_buffer);
        gear_shop_draw_quads(menu_state_current->gear_screen->part_count[2], menu_state_current->gear_screen->parts[2],
                      menu_state_current->gear_screen->parts_buffer);
        gear_shop_draw_quads(menu_state_current->gear_screen->part_count[3], menu_state_current->gear_screen->parts[3],
                      menu_state_current->gear_screen->parts_buffer);
        gear_shop_draw_quads(menu_state_current->gear_screen->part_count[4], menu_state_current->gear_screen->parts[4],
                      menu_state_current->gear_screen->parts_buffer);
    }
}

/* 801CEA68: Animate the two lamps: pick their position (from the cursor while opening, now and then at random while idle), draw the frame's sprites and step the frame. */
void gear_shop_animate_lamps(void) {
    u16 at[2][4];
    u8 moved;
    s32 i;
    s32 j;
    u16 id;

    for (i = 0; i < 2; i++) {
        moved = 0;
        if (menu_state_current->gear_screen->lamp_state[i] == 0) {
            continue;
        }
        switch (menu_state_current->gear_screen->lamp_state[i]) {
        case 1:
            at[0][i] = gear_shop_lamp_x_table[i];
            at[1][i] = gear_shop_lamp_y_choices[i * 6 + gear_shop_lamp_positions_by_cursor[menu_state_current->cursor * 4 + menu_state_current->choice]];
            moved = 1;
            break;
        case 2:
            if (mode_get_random_byte_in_range(0, 0xFF) < 0x10) {
                at[0][i] = gear_shop_lamp_x_table[i];
                at[1][i] = gear_shop_lamp_y_choices[i * 6 + mode_get_random_byte_in_range(0, 5)];
                moved = 1;
            }
            break;
        }
        if (moved) {
            menu_state_current->gear_screen->lamp_x[i] = at[0][i];
            menu_state_current->gear_screen->lamp_y[i] = at[1][i];
        }
        if (menu_state_current->gear_screen->lamp_timer[i] != 0) {
            menu_state_current->gear_screen->lamp_timer[i]--;
            continue;
        }
        menu_state_current->gear_screen->lamp_count[i] = 0;
        for (j = 0; j < 4; j++) {
            id = gear_shop_lamp_frame_images[i * 20 + (menu_state_current->gear_screen->lamp_frame[i] * 4 + j)];
            if (id != 0xFFFF) {
                menu_state_current->gear_screen->lamp_count[i] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, id,
                                  &menu_state_current->gear_screen->lamps[i * 22 + menu_state_current->gear_screen->lamp_count[i] * 2],
                                  menu_state_current->buffer_index, menu_state_current->gear_screen->lamp_x[i],
                                  menu_state_current->gear_screen->lamp_y[i], 0x1000);
            }
        }
        menu_state_current->gear_screen->lamp_buffer[i] = menu_state_current->buffer_index;
        switch (menu_state_current->gear_screen->lamp_state[i]) {
        case 1:
            if (++menu_state_current->gear_screen->lamp_frame[i] == 4) {
                menu_state_current->gear_screen->lamp_state[i] = 2;
            }
            menu_state_current->gear_screen->lamp_timer[i] = 2;
            break;
        case 2:
            if (++menu_state_current->gear_screen->lamp_frame[i] >= 5) {
                menu_state_current->gear_screen->lamp_frame[i] = 3;
            }
            menu_state_current->gear_screen->lamp_timer[i] = 8;
            break;
        case 3:
            if (--menu_state_current->gear_screen->lamp_frame[i] < 0) {
                menu_state_current->gear_screen->lamp_state[i] = 0;
                menu_state_current->gear_screen->flicker_shown = 0;
            }
            menu_state_current->gear_screen->lamp_timer[i] = 2;
            break;
        }
    }
}

/* 801CEEA8: Animate the indicator: its position (from the cursor while opening, now and then at random while idle), its sprite, and its open-idle-close frame steps. */
void gear_shop_animate_indicator(void) {
    u8 moved;
    u16 x;
    u16 y;
    s32 index;

    if (menu_state_current->gear_screen->lamp_state[2] != 0) {
        if (menu_state_current->gear_screen->lamp_timer[2] != 0) {
            menu_state_current->gear_screen->lamp_timer[2]--;
            return;
        }
        moved = 0;
        switch (menu_state_current->gear_screen->lamp_state[2]) {
        case 1:
            index = gear_shop_lamp_positions_by_cursor[menu_state_current->cursor * 4 + menu_state_current->choice];
            x = gear_shop_indicator_x_choices[index];
            moved = 1;
            y = gear_shop_indicator_y_choices[index];
            break;
        case 2:
            if (mode_get_random_byte_in_range(0, 0xFF) < 4) {
                x = gear_shop_indicator_x_choices[mode_get_random_byte_in_range(0, 5)];
                y = gear_shop_indicator_y_choices[mode_get_random_byte_in_range(0, 5)];
                moved = 1;
            }
            break;
        }
        if (moved) {
            menu_state_current->gear_screen->indicator_x = x;
            menu_state_current->gear_screen->indicator_y = y;
        }
        menu_state_current->gear_screen->lamp_count[2] =
            sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->gear_screen->indicator_frame + 0x172,
                          menu_state_current->gear_screen->indicator, menu_state_current->buffer_index, menu_state_current->gear_screen->indicator_x,
                          menu_state_current->gear_screen->indicator_y, 0x1000);
        menu_state_current->gear_screen->lamp_buffer[2] = menu_state_current->buffer_index;
        menu_state_current->gear_screen->lamp_timer[2] = 1;
        switch (menu_state_current->gear_screen->lamp_state[2]) {
        case 1:
            if (++menu_state_current->gear_screen->indicator_frame >= 3) {
                menu_state_current->gear_screen->lamp_state[2] = 2;
            }
            break;
        case 2:
            if (++menu_state_current->gear_screen->indicator_frame >= 11) {
                menu_state_current->gear_screen->indicator_frame = 3;
            }
            break;
        case 3:
            menu_state_current->gear_screen->indicator_frame = 2;
            menu_state_current->gear_screen->lamp_state[2] = 4;
            break;
        case 4:
            if (--menu_state_current->gear_screen->indicator_frame < 0) {
                menu_state_current->gear_screen->lamp_state[2] = 0;
            }
            break;
        }
    }
}

/* 801CF184: Animate the flicker every fifth frame: now and then move it to a random place, draw its three sprites (a diagonal row) and toggle their frame. */
void gear_shop_animate_flicker(void) {
    u16 x;
    u16 y;
    s32 i;

    if (menu_state_current->gear_screen->flicker_shown != 0) {
        if (menu_state_current->gear_screen->flicker_timer != 0) {
            menu_state_current->gear_screen->flicker_timer--;
            return;
        }
        if (mode_get_random_byte_in_range(0, 0xFF) < 8) {
            x = gear_shop_flicker_x_choices[mode_get_random_byte_in_range(0, 5)];
            y = gear_shop_flicker_y_choices[mode_get_random_byte_in_range(0, 5)];
            menu_state_current->gear_screen->flicker_x = x;
            menu_state_current->gear_screen->flicker_y = y;
        }
        for (i = 0; i < 3; i++) {
            menu_state_current->gear_screen->flicker_count =
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->gear_screen->flicker_frame + 0x167,
                              menu_state_current->gear_screen->flicker[i], menu_state_current->buffer_index,
                              menu_state_current->gear_screen->flicker_x + i * 8, menu_state_current->gear_screen->flicker_y + i * 10,
                              0x1000);
        }
        menu_state_current->gear_screen->flicker_frame ^= 1;
        menu_state_current->gear_screen->flicker_buffer = menu_state_current->buffer_index;
        menu_state_current->gear_screen->flicker_timer = 4;
    }
}

/* 801CF33C: Build the gear screen's packets when shown. */
void gear_shop_animate_gear_screen(void) {
    if (menu_state_current->flags->gear_shown != 0) {
        gear_shop_animate_lamps();
        gear_shop_animate_indicator();
        gear_shop_animate_flicker();
    }
}

/* 801CF38C: Render name `index` of the name table into VRAM at (180h, 48h). */
void gear_shop_render_name(u8 index) {
    RECT rect;

    gear_shop_name_pixels = heap_alloc(0x3F6, 0);
    bzero(gear_shop_name_pixels, 0x3F6);
    window_render_text_line(game_data.names[index], gear_shop_name_pixels, 0x24, 0);
    rect.x = 0x180;
    rect.y = 0x48;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, (u_long *)gear_shop_name_pixels);
    DrawSync(0);
    heap_free(gear_shop_name_pixels);
}

/* 801CF448: Build the gear parts panel: its fourteen frame sprites, five of the gear's values in decimal, and the gear's name. */
void gear_shop_layout_gear_values(void) {
    s32 i;
    s32 j;

    for (j = 0; j < 14; j++) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, gear_shop_gear_values_frame_images[j], &menu_state_current->gear_screen->frame[j * 2],
                      menu_state_current->buffer_index, gear_shop_gear_values_frame_x_table[j], gear_shop_gear_values_frame_y_table[j], 0x1000);
    }
    gear_shop_split_digits(game_data.gears[gear_shop_edited_gear].hp);
    menu_state_current->gear_screen->part_count[0] = 0;
    for (i = 0; i < 5; i++) {
        if (menu_state_current->digits[i + 4] != 0xFF) {
            menu_state_current->gear_screen->part_count[0] +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i + 4],
                              &menu_state_current->gear_screen->parts[0][menu_state_current->gear_screen->part_count[0] * 2],
                              menu_state_current->buffer_index, gear_shop_gear_hp_x + i * 8, gear_shop_gear_hp_y, 0x1000);
        }
    }
    gear_shop_split_digits(game_data.gears[gear_shop_edited_gear].maxHp);
    menu_state_current->gear_screen->part_count[1] = 0;
    for (i = 0, j = 0; i < 5; i++) {
        if (menu_state_current->digits[i + 4] != 0xFF) {
            menu_state_current->gear_screen->part_count[1] +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i + 4],
                              &menu_state_current->gear_screen->parts[1][menu_state_current->gear_screen->part_count[1] * 2],
                              menu_state_current->buffer_index, gear_shop_gear_max_hp_x + j * 8, gear_shop_gear_max_hp_y, 0x1000);
            j++;
        }
    }
    gear_shop_split_digits(game_data.gears[gear_shop_edited_gear].fuel);
    menu_state_current->gear_screen->part_count[2] = 0;
    for (i = 0; i < 4; i++) {
        if (menu_state_current->digits[i + 5] != 0xFF) {
            menu_state_current->gear_screen->part_count[2] +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i + 5],
                              &menu_state_current->gear_screen->parts[2][menu_state_current->gear_screen->part_count[2] * 2],
                              menu_state_current->buffer_index, gear_shop_gear_fuel_x + i * 8, gear_shop_gear_fuel_y, 0x1000);
        }
    }
    gear_shop_split_digits(game_data.gears[gear_shop_edited_gear].maxFuel);
    menu_state_current->gear_screen->part_count[3] = 0;
    for (i = 0, j = 0; i < 4; i++) {
        if (menu_state_current->digits[i + 5] != 0xFF) {
            menu_state_current->gear_screen->part_count[3] +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i + 5],
                              &menu_state_current->gear_screen->parts[3][menu_state_current->gear_screen->part_count[3] * 2],
                              menu_state_current->buffer_index, gear_shop_gear_max_fuel_x + j * 8, gear_shop_gear_max_fuel_y, 0x1000);
            j++;
        }
    }
    gear_shop_split_digits(game_data.gears[gear_shop_edited_gear].field68);
    menu_state_current->gear_screen->part_count[4] = 0;
    for (i = 0; i < 5; i++) {
        if (menu_state_current->digits[i + 4] != 0xFF) {
            menu_state_current->gear_screen->part_count[4] +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i + 4],
                              &menu_state_current->gear_screen->parts[4][menu_state_current->gear_screen->part_count[4] * 2],
                              menu_state_current->buffer_index, gear_shop_gear_value68_x + i * 8, gear_shop_gear_value68_y, 0x1000);
        }
    }
    gear_shop_render_name(gear_shop_edited_gear + 0xB);
    menu_state_current->gear_screen->parts_buffer = menu_state_current->buffer_index;
}

/* 801CF9BC: Load model `model`'s two files (ids from the model table) into part block `slot`. */
void gear_shop_model_read_files(u8 model, u8 slot) {
    cd_select_directory(4, 0);
    menu_state_current->model_parts[slot]->data0 = heap_alloc(cd_get_aligned_file_size(gear_shop_model_file_ids[model]), 0);
    cd_read_file(gear_shop_model_file_ids[model], menu_state_current->model_parts[slot]->data0, 0, 0x80);
    cd_sync_reads(0);
    menu_state_current->model_parts[slot]->data1 = heap_alloc(cd_get_aligned_file_size(gear_shop_model_file_ids[model] + 1), 0);
    cd_read_file(gear_shop_model_file_ids[model] + 1, menu_state_current->model_parts[slot]->data1, 0, 0x80);
    cd_sync_reads(0);
    cd_select_directory(0x10, 0);
}

/* 801CFAB8
 * Create gear `gear`'s model as actor `slot` of the actor module (ovl2143)
 * with its ground height and scale, turn actor 1 a quarter turn back and
 * tilt it, run the script entry of the gear's variant, and show the parts
 * panel.
 */
void gear_shop_model_load_gear(u8 slot, u8 gear) {
    u8 variant;

    variant = 0;
    gear_model_create_actor(slot, 0, menu_state_current->model_parts[slot]->data0, menu_state_current->model_parts[slot]->data1,
                  slot * 64 + 0x200, 0, 0, slot + 0x1C0, menu_state_current->model_parts[slot]->position);
    gear_model_actors[slot]->groundY = gear_shop_model_field60_values[gear];
    gear_model_actors[slot]->scale = gear_shop_model_field1c_values[gear];
    gear_model_actors[1]->parts->rotation.vy -= 0x400;
    model_ot_depth_shift = 0;
    gear_model_actors[1]->parts->rotation.vx -= 0x20;
    if (gear != 0xFF) {
        variant = gear_shop_model_variants[gear];
    }
    gear_model_select_and_call_entry(slot, 0, variant);
    heap_free(menu_state_current->model_parts[slot]->data1);
    menu_state_current->model_parts[slot]->unk12 = 1;
    gear_shop_layout_gear_values();
    menu_state_current->flags->gear_parts_shown = 1;
}

/* 801CFC60
 * Swing the camera to the edited gear's view for the current command, open
 * the lamps, indicator and flicker, and wait until the lamps and indicator
 * are open. The y table holds three command rows (commands 1-3) of four list
 * entries per gear, the distance table one such row set.
 */
void gear_shop_gear_view_open(void) {
    s32 row;

    gear_shop_camera_move.from[0] = gear_shop_camera_move.to[0];
    gear_shop_camera_move.from[1] = gear_shop_camera_move.to[1];
    gear_shop_camera_move.from[2] = gear_shop_camera_move.to[2];
    gear_shop_camera_move.to[0] = gear_shop_camera_x_targets[gear_shop_edited_gear];
    row = gear_shop_edited_gear * 3;
    gear_shop_camera_move.to[1] = gear_shop_camera_y_targets[(menu_state_current->cursor + row - 1) * 4 + menu_state_current->choice];
    gear_shop_camera_move.to[2] = gear_shop_camera_distance_targets[(menu_state_current->cursor - 1) * 4 + menu_state_current->choice];
    gear_shop_camera_plan_move();
    menu_state_current->view_motion = 7;
    menu_state_current->gear_screen->flicker_shown = 1;
    menu_state_current->gear_screen->lamp_state[0] = 1;
    menu_state_current->gear_screen->lamp_state[1] = 1;
    menu_state_current->gear_screen->lamp_state[2] = 1;
    menu_state_current->gear_screen->flicker_timer = 0;
    menu_state_current->gear_screen->lamp_timer[0] = 0;
    menu_state_current->gear_screen->lamp_timer[1] = 0;
    menu_state_current->gear_screen->lamp_timer[2] = 0;
    menu_state_current->gear_screen->flicker_frame = 0;
    menu_state_current->gear_screen->lamp_frame[0] = 0;
    menu_state_current->gear_screen->lamp_frame[1] = 0;
    menu_state_current->gear_screen->indicator_frame = 0;
    menu_state_current->gear_screen->flicker_x = 0x18;
    menu_state_current->gear_screen->flicker_y = 0x6E;
    menu_state_current->flags->gear_shown = 1;
    while (menu_state_current->gear_screen->lamp_state[0] != 2) {
        gear_shop_run_frame();
    }
    while (menu_state_current->gear_screen->lamp_state[2] != 2) {
        gear_shop_run_frame();
    }
}

/* 801CFF18: Show the gear screen: close its lamps and indicator, swing the camera and start its motion (7). */
void gear_shop_gear_view_close(void) {
    menu_state_current->gear_screen->lamp_state[0] = 3;
    menu_state_current->gear_screen->lamp_state[1] = 3;
    menu_state_current->gear_screen->lamp_state[2] = 3;
    menu_state_current->gear_screen->flicker_timer = 0;
    menu_state_current->gear_screen->lamp_timer[0] = 0;
    menu_state_current->gear_screen->lamp_timer[1] = 0;
    menu_state_current->gear_screen->lamp_timer[2] = 0;
    menu_state_current->flags->gear_shown = 1;
    gear_shop_camera_move.from[0] = gear_shop_camera_move.to[0];
    gear_shop_camera_move.from[1] = gear_shop_camera_move.to[1];
    gear_shop_camera_move.from[2] = gear_shop_camera_move.to[2];
    gear_shop_camera_move.to[0] = 0x400;
    gear_shop_camera_move.to[1] = 0;
    gear_shop_camera_move.to[2] = -0x400;
    gear_shop_camera_plan_move();
    menu_state_current->view_motion = 7;
}

/* 801D0054: Tint `count` packet pairs of this buffer red (0) or blue (1). */
void gear_shop_tint_quads(s32 count, POLY_FT4 *packets, u8 color) {
    s32 i;

    for (i = 0; i < count; i++) {
        SetShadeTex(&packets[i * 2 + menu_state_current->buffer_index], 0);
        switch (color) {
        case 0:
            (packets + (i * 2 + menu_state_current->buffer_index))->r0 = 0x80;
            (packets + (i * 2 + menu_state_current->buffer_index))->g0 = 0x40;
            (packets + (i * 2 + menu_state_current->buffer_index))->b0 = 0x40;
            break;
        case 1:
            (packets + (i * 2 + menu_state_current->buffer_index))->r0 = 0x40;
            (packets + (i * 2 + menu_state_current->buffer_index))->g0 = 0x40;
            (packets + (i * 2 + menu_state_current->buffer_index))->b0 = 0x80;
            break;
        }
    }
}

/* 801D0220: Reveal the available party members' portraits one member per frame. */
void gear_shop_reveal_portraits(void) {
    s32 step;
    s32 shown;
    s32 i;

    menu_state_current->flags->unknown5a[0] = 1;
    for (step = 1; step < 12; step++) {
        shown = 0;
        menu_state_current->details->members_count = 0;
        for (i = 0; i < step; i++) {
            if (menu_state_current->present[i] != 0) {
                menu_state_current->details->members_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, i + 0x14E, &menu_state_current->details->members[shown * 2],
                                  menu_state_current->buffer_index, gear_shop_portrait_x_table[shown], 0xA6, 0x1000);
                shown++;
            }
        }
        menu_state_current->details->members_buffer = menu_state_current->buffer_index;
        gear_shop_run_frame();
    }
}

/* 801D0348: Count the available members 0-10 into gear_shop_available_member_count. */
void gear_shop_count_available_members(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        if (menu_state_current->present[i] != 0) {
            gear_shop_available_member_count++;
        }
    }
}

/* 801D0398: Switch the gear screen to the previous (`back`) or next available member and load its gear. */
void gear_shop_switch_member(u8 back) {
    s32 next;
    s32 member;

    if (menu_state_current->view_motion == 0 && gear_shop_available_member_count >= 2) {
        next = gear_shop_current_member_index;
        if (!back) {
            if (++next >= gear_shop_available_member_count) {
                next = 0;
            }
        } else {
            if (--next < 0) {
                next = gear_shop_available_member_count - 1;
            }
        }
        gear_shop_current_member_index = next;
        next++;
        member = -1;
        while (next != 0) {
            member++;
            if (menu_state_current->present[member] != 0) {
                next--;
            }
        }
        gear_shop_run_frame();
        menu_state_current->model_parts[1]->unk12 = 0;
        gear_model_free_actor(1);
        gear_shop_run_frame();
        gear_shop_model_read_files(game_data.characters[member].gearId, 1);
        gear_shop_edited_gear = game_data.characters[member].gearId;
        gear_shop_model_load_gear(1, game_data.characters[member].gearId);
        gear_shop_run_frame();
    }
}

/* 801D04E8: Draw heading set `set` (four sprites). */
void gear_shop_layout_buy_headings(u8 set) {
    s32 i;

    menu_state_current->details->heading_count = 0;
    for (i = 0; i < 4; i++) {
        menu_state_current->details->heading_count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, gear_shop_buy_heading_images[set * 4 + i],
                          menu_state_current->details->heading + menu_state_current->details->heading_count * 2,
                          menu_state_current->buffer_index, gear_shop_buy_heading_x_table[set * 4 + i], gear_shop_buy_heading_y_table[set * 4 + i], 0x1000);
    }
    menu_state_current->details->heading_buffer = menu_state_current->buffer_index;
}

/* 801D05EC: Draw the two alternative heading sprites. */
void gear_shop_layout_sell_headings(void) {
    s32 i;

    menu_state_current->details->heading_count = 0;
    for (i = 0; i < 2; i++) {
        menu_state_current->details->heading_count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, gear_shop_sell_heading_images[i],
                          menu_state_current->details->heading + menu_state_current->details->heading_count * 2,
                          menu_state_current->buffer_index, gear_shop_sell_heading_x_table[i], gear_shop_sell_heading_y_table[i], 0x1000);
    }
    menu_state_current->details->heading_buffer = menu_state_current->buffer_index;
}

/* 801D06D8: Show the number panel: its two frame lines and three numbers, plus a fourth when `lower` (shifted left and up by `lower`). */
void gear_shop_layout_gold_numbers(u32 first, u32 second, u32 third, u32 fourth, u8 lower) {
    s32 i;

    for (i = 0; i < 2; i++) {
        SetLineF2(&menu_state_current->details->frame[i]);
        (menu_state_current->details->frame + i)->r0 = 0xFF;
        (menu_state_current->details->frame + i)->g0 = 0xFF;
        (menu_state_current->details->frame + i)->b0 = 0xFF;
        (menu_state_current->details->frame + i)->x0 = gear_shop_total_x - 8 - lower * 16;
        (menu_state_current->details->frame + i)->y0 = gear_shop_total_y + 9 - lower * 8;
        (menu_state_current->details->frame + i)->x1 = gear_shop_total_x + 0x4E - lower * 16;
        (menu_state_current->details->frame + i)->y1 = gear_shop_total_y + 9 - lower * 8;
    }
    gear_shop_split_digits(first);
    menu_state_current->details->digits1_count = 0;
    for (i = 0; i < 9; i++) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->digits1_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              &menu_state_current->details->digits1[menu_state_current->details->digits1_count * 2],
                              menu_state_current->buffer_index, gear_shop_gold_x + i * 8 - lower * 16, gear_shop_gold_y - lower * 16,
                              0x1000);
        }
    }
    menu_state_current->details->digits1_buffer = menu_state_current->buffer_index;
    gear_shop_split_digits(second);
    menu_state_current->details->digits2_count = 0;
    for (i = 0; i < 9; i++) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->digits2_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              &menu_state_current->details->digits2[menu_state_current->details->digits2_count * 2],
                              menu_state_current->buffer_index, gear_shop_total_x + i * 8 - lower * 16, gear_shop_total_y - lower * 16,
                              0x1000);
        }
    }
    menu_state_current->details->digits2_buffer = menu_state_current->buffer_index;
    gear_shop_split_digits(third);
    menu_state_current->details->digits3_count = 0;
    for (i = 0; i < 9; i++) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->digits3_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              &menu_state_current->details->digits3[menu_state_current->details->digits3_count * 2],
                              menu_state_current->buffer_index, gear_shop_new_gold_x + i * 8 - lower * 16, gear_shop_new_gold_y - lower * 8,
                              0x1000);
        }
    }
    menu_state_current->details->digits3_buffer = menu_state_current->buffer_index;
    menu_state_current->details->digits4_count = 0;
    menu_state_current->details->digits4_shown = 0;
    if (lower) {
        gear_shop_split_digits(fourth);
        for (i = 0; i < 9; i++) {
            if (menu_state_current->digits[i] != 0xFF) {
                menu_state_current->details->digits4_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                                  &menu_state_current->details->digits4[menu_state_current->details->digits4_count * 2],
                                  menu_state_current->buffer_index, gear_shop_total_x + i * 8 - lower * 16, gear_shop_total_y - lower * 8,
                                  0x1000);
            }
        }
        menu_state_current->details->digits4_buffer = menu_state_current->buffer_index;
        menu_state_current->details->digits4_shown = 1;
    }
    menu_state_current->details->digits_shown = 1;
}

/* 801D0C20: Draw a nine-digit number (the party's gold) at (6bh, 54h), or lower at (53h, 64h). */
void gear_shop_layout_notice_total(u32 value, u8 lower) {
    s32 i;
    s32 x;
    s32 y;

    x = 0x68;
    y = 0x54;
    if (lower) {
        x = 0x50;
        y = 0x64;
    }
    gear_shop_split_digits(value);
    i = 0;
    menu_state_current->details->group1220_count = 0;
    for (; i < 9; i++, x += 8) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->group1220_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              menu_state_current->details->group1220 + menu_state_current->details->group1220_count * 2,
                              menu_state_current->buffer_index, x + 3, y, 0x1000);
        }
    }
    menu_state_current->details->group1220_buffer = menu_state_current->buffer_index;
    menu_state_current->flags->unknown5a[1] = 2;
}

/* 801D0D4C: Draw a nine-digit price and its unit sprite at (aah, aeh). */
void gear_shop_layout_price_digits(u32 value) {
    s32 i;
    s32 x;

    gear_shop_split_digits(value);
    i = 0;
    x = 0xAA;
    menu_state_current->details->price_count = 0;
    for (; i < 9; i++, x += 8) {
        if (menu_state_current->digits[i] != 0xFF) {
            menu_state_current->details->price_count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[i],
                              menu_state_current->details->price + menu_state_current->details->price_count * 2,
                              menu_state_current->buffer_index, x, 0xAE, 0x1000);
        }
    }
    menu_state_current->details->price_count +=
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x10,
                      menu_state_current->details->price + menu_state_current->details->price_count * 2, menu_state_current->buffer_index,
                      0xF2, 0xAE, 0x1000);
    menu_state_current->details->price_buffer = menu_state_current->buffer_index;
    menu_state_current->flags->price_shown = 1;
}

/* 801D0EC8: Hide the shop list's packets; with `close` also close its panels, scroll bar and marker. */
void gear_shop_hide_details(u8 close) {
    s32 i;

    menu_state_current->flags->unknown5a[0] = 0;
    menu_state_current->details->label4430_shown = 0;
    menu_state_current->details->label44B0_shown = 0;
    menu_state_current->details->digits_shown = 0;
    menu_state_current->details->heading_count = 0;
    menu_state_current->details->group2D0_count = 0;
    menu_state_current->details->members_count = 0;
    for (i = 0; i < 9; i++) {
        menu_state_current->details->bar_shown[i] = 0;
        menu_state_current->details->cells_a_count[i] = 0;
        menu_state_current->details->cells_b_count[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        menu_state_current->details->name_shown[i] = 0;
        menu_state_current->details->row_count[i] = 0;
    }
    menu_state_current->details->label4530_shown = 0;
    if (close) {
        gear_shop_panel_close(2);
        gear_shop_panel_close(3);
        gear_shop_scroll_bar_hide();
        gear_shop_list_cursor_free(0);
    }
}

/* 801D1078: Party bits of the available members whose gear has part `item` of kind `kind` fitted. */
u32 gear_shop_find_part_holders(u8 item, u8 kind) {
    u16 members;
    u8 found;
    s32 i;
    s32 k;

    members = 0;
    if (item != 0) {
        for (i = 0; i < 16; i++) {
            found = 0;
            if (menu_state_current->present[i] != 0) {
                switch (kind) {
                case 1:
                    if (game_data.gears[game_data.characters[i].gearId].engine == item) {
                        found = 1;
                    }
                    break;
                case 0:
                    if (game_data.gears[game_data.characters[i].gearId].frame == item) {
                        found = 1;
                    }
                    break;
                case 2:
                    if (game_data.gears[game_data.characters[i].gearId].field3 == item) {
                        found = 1;
                    }
                    break;
                case 4:
                    for (k = 0; k < 4; k++) {
                        if (item < 0x32) {
                            if (game_data.gears[game_data.characters[i].gearId].weapons[k] == item) {
                                found = 1;
                                break;
                            }
                        } else if (game_data.gears[game_data.characters[i].gearId].partItems[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                case 3:
                    for (k = 0; k < 3; k++) {
                        if (game_data.gears[game_data.characters[i].gearId].parts[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                }
            }
            if (found) {
                members |= gear_shop_get_bit_mask32(game_data.characters[i].gearId);
            }
        }
    }
    return members;
}

/* 801D1304
 * Show part `id` of kind `kind` (3 or 4): its name and sell price (half the
 * table price) labels, the bars of the members who can use it and the marks
 * of those holding it. Returns the sell price.
 */
u32 gear_shop_show_sell_entry(u8 id, u8 kind) {
    RECT rect;
    u8 unused[16]; /* unused in the original; reserves 16 bytes */
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u8 *pixels;
    u32 users;
    u32 price;
    s32 value;
    u32 holders;
    s32 digit;
    u8 started;
    u8 gear;
    s32 j;
    s32 i;

    users = 0;
    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = heap_alloc(0x618, 0);
    bzero(pixels, 0x618);
    bzero(codes, 14);
    switch (kind) {
    case 4:
        menu_state_current->details->label4430.width = window_render_text_line(text_get_gear_part_name(id), pixels, 0x39, 0);
        users = menu_state_current->tables->gear_weapons[id].users;
        price = menu_state_current->tables->gear_weapons[id].price >> 1;
        value = price;
        break;
    case 3:
        menu_state_current->details->label4430.width = window_render_text_line(text_get_gear_accessory_name(id), pixels, 0x39, 0);
        users = menu_state_current->tables->gear_accessories[id].users;
        price = menu_state_current->tables->gear_accessories[id].price >> 1;
        value = price;
        break;
    }
    holders = gear_shop_find_part_holders(id, kind);
    started = 0;
    for (i = 0, j = 4; j > 0; i++, j--) {
        digit = value / divisors[j];
        if (digit != 0 || started) {
            codes[i * 2] = digit + 0x10;
            started = 1;
            value -= digit * divisors[j];
        } else {
            codes[i * 2] = 0xC3;
        }
    }
    codes[8] = value % 10 + 0x10;
    text_decode_codes(codes, text, 5);
    menu_state_current->details->label44B0.width = window_render_text_line(text, pixels, 0x39, 1);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    gear_shop_label_init_quads(&menu_state_current->details->label4430, 0, 0, 0);
    gear_shop_label_init_quads(&menu_state_current->details->label44B0, 0, 0, 0);
    menu_state_current->details->label44B0.polys[menu_state_current->buffer_index].clut = text_plane1_clut;
    gear_shop_quad_place(&menu_state_current->details->label4430.polys[menu_state_current->buffer_index], 0x2C, 0x12, 0, 0x4E,
                  menu_state_current->details->label4430.width, 13);
    gear_shop_quad_place(&menu_state_current->details->label44B0.polys[menu_state_current->buffer_index], 0x98, 0x12, 0, 0x4E,
                  menu_state_current->details->label44B0.width, 13);
    gear_shop_set_rect_verts(menu_state_current->details->label4430.verts, 0x2C, 0x12, menu_state_current->details->label4430.width, 13);
    gear_shop_set_rect_verts(menu_state_current->details->label44B0.verts, 0x98, 0x12, menu_state_current->details->label44B0.width, 13);
    menu_state_current->details->label4430.buffer = menu_state_current->buffer_index;
    menu_state_current->details->label44B0.buffer = menu_state_current->buffer_index;
    heap_free(pixels);
    if (id) {
        menu_state_current->details->label4430_shown = 1;
        menu_state_current->details->label44B0_shown = 1;
    } else {
        menu_state_current->details->label4430_shown = 0;
        menu_state_current->details->label44B0_shown = 0;
    }
    i = 0;
    j = 0;
    menu_state_current->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (menu_state_current->present[i] != 0) {
            gear = game_data.characters[i].gearId;
            if (gear_shop_test_bit32(users, gear)) {
                menu_state_current->details->bar_shown[j] = 1;
            } else {
                menu_state_current->details->bar_shown[j] = 0;
            }
            if (gear_shop_test_bit32(holders, gear)) {
                menu_state_current->details->group2D0_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE,
                                  &menu_state_current->details->group2D0[menu_state_current->details->group2D0_count * 2],
                                  menu_state_current->buffer_index, gear_shop_portrait_x_table[j] + 0xE, 0xB4, 0x1000);
            }
            j++;
        }
    }
    menu_state_current->details->group2D0_buffer = menu_state_current->buffer_index;
    return price;
}

/* 801D18F8
 * Draw the eight visible rows of a list from entry `top`: each part's name,
 * the count held and, when some are chosen, "x" and the chosen count.
 */
void gear_shop_layout_sell_rows(s32 top, u8 *ids, u8 *kinds, u8 *chosen, u8 *held) {
    RECT rect;
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u8 *pixels;
    s32 value;
    s32 digit;
    u8 started;
    u8 tens;
    s32 row;
    s32 i;
    s32 j;

    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = heap_alloc(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        bzero(codes, 14);
        menu_state_current->details->row_count[row] = 0;
        if (ids[top + row] != 0) {
            value = held[top + row];
            switch (kinds[top + row]) {
            case 4:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_gear_part_name(ids[top + row]), pixels, 0x24, 0);
                break;
            case 3:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_gear_accessory_name(ids[top + row]), pixels, 0x24, 0);
                break;
            }
            started = 0;
            for (i = 0, j = 4; j > 0; i++, j--) {
                digit = value / divisors[j];
                if (digit != 0 || started) {
                    codes[i * 2] = digit + 0x10;
                    started = 1;
                    value -= digit * divisors[j];
                } else {
                    codes[i * 2] = 0xC3;
                }
            }
            codes[i * 2] = value % 10 + 0x10;
            text_decode_codes(codes, text, 5);
            menu_state_current->details->names_b[row].width = window_render_text_line(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            gear_shop_label_init_quads(&menu_state_current->details->names_a[row], row, 0x80, 0x81);
            gear_shop_set_rect_verts(menu_state_current->details->names_a[row].verts, 0x24, row * 13 + 0x32,
                          menu_state_current->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            DrawSync(0);
            gear_shop_label_init_quads(&menu_state_current->details->names_b[row], row, 0x80, 0x82);
            gear_shop_set_rect_verts(menu_state_current->details->names_b[row].verts, 0x8C, row * 13 + 0x32,
                          menu_state_current->details->names_b[row].width, 13);
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->name_shown[row] = 1;
            if (chosen[top + row] != 0) {
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE5, menu_state_current->details->rows[row],
                                  menu_state_current->buffer_index, 0xB4, row * 13 + 0x36, 0x1000);
                tens = chosen[top + row] / 10;
                if (tens != 0) {
                    menu_state_current->details->row_count[row] +=
                        sprite_sheet_draw_scaled(menu_state_current->sheet, tens,
                                      &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                      menu_state_current->buffer_index, 0xBC, row * 13 + 0x36, 0x1000);
                }
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, (u8)(chosen[top + row] % 10),
                                  &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                  menu_state_current->buffer_index, 0xC4, row * 13 + 0x36, 0x1000);
                menu_state_current->details->row_buffer[row] = menu_state_current->buffer_index;
            }
        } else {
            menu_state_current->details->name_shown[row] = 0;
        }
    }
    heap_free(pixels);
}

/* 801D1F20: Set the party's gold (capped at 9999999) and, with `remove`, take the chosen amounts out of the inventory. */
void gear_shop_settle_sale(u32 gold, u8 *ids, u8 *amounts, s32 n, u8 *inv_ids, u8 *inv_counts, u8 *unused,
                   u8 remove, u8 unused_member) {
    u32 *party_gold;
    s32 i;
    s32 j;

    gear_shop_play_sound(0xD1);
    party_gold = &game_data.gold;
    *party_gold = gold;
    if (gold > 9999999) {
        *party_gold = 9999999;
    }
    if (remove) {
        for (i = 0; i < n; i++) {
            if (ids[i] != 0) {
                for (j = 0; j < n; j++) {
                    if (ids[i] == inv_ids[j]) {
                        inv_counts[j] -= amounts[i];
                        if (inv_counts[j] == 0) {
                            inv_ids[j] = 0;
                        }
                    }
                }
            }
        }
    }
}

/* 801D2054
 * The sell list (twin of the item shop's): choose how many of each of `n`
 * held parts to sell, eight rows at a time, with the running total and the
 * gold after the sale; confirming settles the sale. Every row uses `kind`;
 * `remove` controls inventory removal and the settlement ignores `member`.
 */
void gear_shop_sell_list_run(s32 n, u8 *ids, u8 *counts, u8 kind, u8 remove, u8 *unused_kinds, u8 member) {
    u8 running = 1;
    u8 first = 1;
    u8 redraw = 1;
    s32 row = 0;
    s32 last_row = 0xFF;
    s32 top = 0;
    s32 last_top = 0xFF;
    u8 sell_ids[n];
    u8 sell_kinds[n];
    u8 chosen[n];
    u8 selectable[n];
    u8 *held;
    u32 gold = game_data.gold;
    u8 held_storage[n];
    u32 total;
    u32 new_gold;
    u32 price;
    s32 count;
    s32 collected;
    s32 i;
    s32 index;

    held = held_storage;
    new_gold = gold;
    total = 0;

    for (i = 0; i < n; i++) {
        held[i] = 0;
        selectable[i] = 0;
        chosen[i] = 0;
        sell_ids[i] = 0;
        sell_kinds[i] = kind;
    }
    collected = 0;
    for (i = 0; i < n; i++) {
        if (ids[i] != 0 && counts[i] != 0) {
            sell_ids[collected] = ids[i];
            held[collected] = counts[i];
            selectable[collected] = 1;
            collected++;
        }
    }
    count = collected;
    gear_shop_list_cursor_alloc(0);
    while (running) {
        gear_shop_run_frame();
        if (top != last_top || redraw) {
            gear_shop_layout_sell_rows(top, sell_ids, sell_kinds, chosen, held);
            gear_shop_scroll_bar_show(0xC, 0x32, 0x3C, count, top);
        }
        if (row != last_row || top != last_top) {
            last_row = row;
            price = gear_shop_show_sell_entry(sell_ids[top + row], sell_kinds[top + row]);
            last_top = top;
            menu_state_current->flags->unknown5a[0] = 1;
        }
        gear_shop_list_cursor_place(row, top, 0, 0);
        if (first) {
            gear_shop_panel_open(2, 0xC, 0x2A, 0xC4, 0x74, 0, 1, 4, 1);
            gear_shop_panel_open(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            gear_shop_reveal_portraits();
            gear_shop_layout_sell_headings();
            first = 0;
        }
        if (redraw) {
            gear_shop_layout_gold_numbers(gold, total, new_gold, 0, 0);
            redraw = 0;
        }
        switch (menu_state_current->input) {
        case 4:
            if (total != 0) {
                gear_shop_play_sound(2);
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->scroll_shown = 0;
                running = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                gear_shop_markers_open(0);
                gear_shop_layout_notice_total(total, 0);
                if (gear_shop_notice_ask_yes_no(0x95, 0xFF, 1)) {
                    gear_shop_settle_sale(new_gold, sell_ids, chosen, n, ids, counts, sell_kinds, remove, member);
                } else {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->cursors_shown[0] = running;
                }
                gear_shop_markers_close();
            } else {
                gear_shop_play_sound(4);
            }
            break;
        case 5:
            running = 0;
            if (total != 0) {
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->scroll_shown = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                gear_shop_markers_open(0);
                if (!gear_shop_notice_ask_yes_no(0x92, 0xFF, 1)) {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->cursors_shown[0] = running;
                }
                gear_shop_markers_close();
            }
            break;
        case 1:
            row++;
            if (row >= 8) {
                row = 7;
                if (count - 8 < ++top) {
                    top--;
                }
            }
            break;
        case 3:
            row--;
            if (row < 0) {
                top--;
                row = 0;
                if (top < 0) {
                    top = 0;
                }
            }
            break;
        case 0:
            index = top + row;
            if (selectable[index] && held[index] - 1 >= 0) {
                redraw = 1;
                total += price;
                chosen[index]++;
                new_gold += price;
                held[index]--;
            }
            break;
        case 2:
            index = top + row;
            if (selectable[index] && chosen[index] - 1 >= 0) {
                total -= price;
                redraw = 1;
                held[index]++;
                new_gold -= price;
                chosen[index]--;
            }
            break;
        }
    }
}

/* 801D2784: Run the sell list for inventory 3 (150 entries). */
void gear_shop_gear_accessory_sell_list_run(void) {
    gear_shop_sell_list_run(150, game_data.gearAccessoryIds, game_data.gearAccessoryIds - 150, 3, 1, game_data.gearAccessoryIds - 150, 0);
}

/* 801D27C4: Run the sell list for inventory 4 (100 entries). */
void gear_shop_gear_part_sell_list_run(void) {
    gear_shop_sell_list_run(100, game_data.gearPartIds, game_data.gearPartIds - 100, 4, 1, game_data.gearPartIds - 100, 0);
}

/* 801D2804: Run sell list `page` * 3 + cursor (3 and 4 are the two inventories), then restore the list labels. */
void gear_shop_chosen_sell_list_run(u8 page) {
    u8 close;

    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    menu_state_current->flags->lists_shown = 0;
    gear_shop_label_clear_shown(4, menu_state_current->flags->list_labels_shown);
    close = 1;
    switch (menu_state_current->choice + page * 3) {
    case 3:
        gear_shop_gear_accessory_sell_list_run();
        break;
    case 4:
        gear_shop_gear_part_sell_list_run();
        break;
    }
    gear_shop_hide_details(close);
    menu_state_current->flags->lists_shown = 1;
    menu_state_current->flags->sprite_shown = 1;
    menu_state_current->flags->cursor_shown = 1;
    gear_shop_label_render_table(4, menu_state_current->list_labels, gear_shop_choice_label_ids, menu_state_current->flags->list_labels_shown);
}

/* 801D2950: Render the edited gear's part name of list `kind` (0 frame, 1 engine, 2 armour) into its label and show it. */
void gear_shop_show_fitted_part_name(u8 kind) {
    RECT rect;
    u8 *pixels;

    pixels = heap_alloc(0x3F6, 0);
    switch (kind) {
    case 0:
        menu_state_current->details->label4530.width = window_render_text_line(
            text_get_resource_entry(menu_state_current->details->resources[6], game_data.gears[gear_shop_edited_gear].frame), pixels, 0x24, 0);
        break;
    case 1:
        menu_state_current->details->label4530.width = window_render_text_line(
            text_get_resource_entry(menu_state_current->details->resources[7], game_data.gears[gear_shop_edited_gear].engine), pixels, 0x24, 0);
        break;
    case 2:
        menu_state_current->details->label4530.width = window_render_text_line(
            text_get_resource_entry(menu_state_current->details->resources[8], game_data.gears[gear_shop_edited_gear].field3), pixels, 0x24, 0);
        break;
    }
    rect.x = 0x198;
    rect.y = 0xB4;
    rect.w = 0x28;
    rect.h = 0xD;
    LoadImage(&rect, (u_long *)pixels);
    gear_shop_label_init_quads(&menu_state_current->details->label4530, 9, 0x80, 0x81);
    gear_shop_set_rect_verts(menu_state_current->details->label4530.verts, 0xD4, 0x8E, menu_state_current->details->label4530.width, 0xD);
    DrawSync(0);
    menu_state_current->details->label4530.buffer = menu_state_current->buffer_index;
    menu_state_current->details->label4530_shown = 1;
    heap_free(pixels);
}

/* 801D2B74
 * Draw the eight visible rows of the parts shop's stock from entry `top`:
 * each part's name and price (`dims[row]` 80h when `gold` covers it and, for
 * kinds 0-2, the edited gear can fit it) and, when some are chosen, "x" and
 * the amount. Returns half the price of the edited gear's fitted part of the
 * last row's kind (0-2): its trade-in value.
 */
u32 gear_shop_layout_stock_rows(s32 top, s32 gold, u8 *dims) {
    RECT rect;
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u32 users;
    u32 trade;
    s32 price;
    GearFrameInfo *frame;
    GearEngineInfo *engine;
    GearPartInfo *armour;
    s32 value;
    u8 *pixels;
    s32 digit;
    u8 started;
    u8 tens;
    s32 row;
    s32 i;
    s32 j;

    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = heap_alloc(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        dims[row] = 0;
        bzero(codes, 14);
        menu_state_current->details->row_count[row] = 0;
        if (menu_state_current->shop_items[top + row] != 0) {
            switch (menu_state_current->shop_kinds[top + row]) {
            case 0:
                frame = &menu_state_current->tables->frames[game_data.gears[gear_shop_edited_gear].frame];
                trade = frame->price >> 1;
                menu_state_current->details->names_a[row].width = window_render_text_line(
                    text_get_resource_entry(menu_state_current->details->resources[6], menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                frame = &menu_state_current->tables->frames[menu_state_current->shop_items[top + row]];
                value = frame->price;
                price = value;
                users = frame->users;
                break;
            case 1:
                engine = &menu_state_current->tables->engines[game_data.gears[gear_shop_edited_gear].engine];
                trade = engine->price >> 1;
                menu_state_current->details->names_a[row].width = window_render_text_line(
                    text_get_resource_entry(menu_state_current->details->resources[7], menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                engine = &menu_state_current->tables->engines[menu_state_current->shop_items[top + row]];
                value = engine->price;
                price = value;
                users = engine->users;
                break;
            case 2:
                armour = &menu_state_current->tables->parts[game_data.gears[gear_shop_edited_gear].field3];
                trade = armour->price >> 1;
                menu_state_current->details->names_a[row].width = window_render_text_line(
                    text_get_resource_entry(menu_state_current->details->resources[8], menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                armour = &menu_state_current->tables->parts[menu_state_current->shop_items[top + row]];
                value = armour->price;
                price = value;
                users = armour->users;
                break;
            case 4:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_gear_part_name(menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                value = menu_state_current->tables->gear_weapons[menu_state_current->shop_items[top + row]].price;
                price = value;
                users = -1;
                break;
            case 3:
                menu_state_current->details->names_a[row].width =
                    window_render_text_line(text_get_gear_accessory_name(menu_state_current->shop_items[top + row]), pixels, 0x24, 0);
                value = menu_state_current->tables->gear_accessories[menu_state_current->shop_items[top + row]].price;
                price = value;
                users = -1;
                break;
            }
            if (gold >= price) {
                if (gear_shop_test_bit32(users, gear_shop_edited_gear)) {
                    dims[row] = 0x80;
                }
            }
            started = 0;
            for (i = 0, j = 4; j > 0; i++, j--) {
                digit = value / divisors[j];
                if (digit != 0 || started) {
                    codes[i * 2] = digit + 0x10;
                    started = 1;
                    value -= digit * divisors[j];
                } else {
                    codes[i * 2] = 0xC3;
                }
            }
            codes[i * 2] = value % 10 + 0x10;
            text_decode_codes(codes, text, 5);
            menu_state_current->details->names_b[row].width = window_render_text_line(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            gear_shop_label_init_quads(&menu_state_current->details->names_a[row], row, 0x80, dims[row] + 1);
            gear_shop_set_rect_verts(menu_state_current->details->names_a[row].verts, 0x24, row * 13 + 0x32,
                          menu_state_current->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, (u_long *)pixels);
            DrawSync(0);
            gear_shop_label_init_quads(&menu_state_current->details->names_b[row], row, 0x80, dims[row] + 2);
            gear_shop_set_rect_verts(menu_state_current->details->names_b[row].verts, 0x8C, row * 13 + 0x32,
                          menu_state_current->details->names_b[row].width, 13);
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->names_a[row].buffer = menu_state_current->buffer_index;
            menu_state_current->details->name_shown[row] = 1;
            if (menu_state_current->details->amounts[top + row] != 0) {
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xF1, menu_state_current->details->rows[row],
                                  menu_state_current->buffer_index, 0xB4, row * 13 + 0x36, 0x1000);
                tens = menu_state_current->details->amounts[top + row] / 10;
                if (tens != 0) {
                    menu_state_current->details->row_count[row] +=
                        sprite_sheet_draw_scaled(menu_state_current->sheet, tens,
                                      &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                      menu_state_current->buffer_index, 0xBC, row * 13 + 0x36, 0x1000);
                }
                menu_state_current->details->row_count[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, (u8)(menu_state_current->details->amounts[top + row] % 10),
                                  &menu_state_current->details->rows[row][menu_state_current->details->row_count[row] * 2],
                                  menu_state_current->buffer_index, 0xC4, row * 13 + 0x36, 0x1000);
                menu_state_current->details->row_buffer[row] = menu_state_current->buffer_index;
            }
        } else {
            menu_state_current->details->name_shown[row] = 0;
        }
    }
    heap_free(pixels);
    return trade;
}

/* 801D3558
 * Preview fitting part `part` of list `kind` (3 parts, 4 weapons) to member
 * `member`'s gear: fit it (a part of the same type, else in place of the
 * lowest-ranked part; a weapon of the same class), recompute the gear, and
 * return the change of its two shown values against the stored ones as
 * magnitudes and signs (1 a decrease); then restore the gear.
 */
void gear_shop_compare_fitted_stats(s32 *change, u8 *decrease, u8 part, u8 kind, u8 member) {
    s16 values[2][4]; /* preview and saved stats, with an eight-byte row stride */
    u8 saved[8];
    GearWeaponInfo *weapon;
    GearAccessoryInfo *fitted;
    GearAccessoryInfo *current;
    u8 gear;
    u8 replace;
    u8 lowest;
    s32 slot;
    s32 k;

    gear = game_data.characters[member].gearId;
    saved[0] = game_data.gears[gear].weapons[0];
    saved[1] = game_data.gears[gear].partItems[0];
    saved[2] = game_data.gears[gear].partItems[1];
    saved[3] = game_data.gears[gear].partItems[2];
    saved[4] = game_data.gears[gear].partItems[3];
    saved[5] = game_data.gears[gear].parts[0];
    saved[6] = game_data.gears[gear].parts[1];
    saved[7] = game_data.gears[gear].parts[2];
    switch (kind) {
    case 4:
        if (part < 0x32) {
            game_data.gears[gear].weapons[0] = part;
        } else {
            weapon = &menu_state_current->tables->gear_weapons[part];
            for (k = 0; k < 4; k++) {
                if (menu_state_current->tables->gear_weapons[game_data.gears[gear].partItems[k]].kind == weapon->kind) {
                    game_data.gears[gear].partItems[k] = part;
                }
            }
        }
        break;
    case 3:
        replace = 1;
        fitted = &menu_state_current->tables->gear_accessories[part];
        for (k = 0; k < 3; k++) {
            current = &menu_state_current->tables->gear_accessories[saved[5 + k]];
            if (current->groups != 0 && current->groups == fitted->groups) {
                replace = 0;
                game_data.gears[gear].parts[k] = part;
            }
        }
        if (replace) {
            lowest = 0xFF;
            for (k = 0; k < 3; k++) {
                current = &menu_state_current->tables->gear_accessories[saved[5 + k]];
                if (lowest >= current->unkD) {
                    lowest = current->unkD;
                    slot = k;
                }
            }
            game_data.gears[gear].parts[slot] = part;
        }
        break;
    }
    gear_shop_rebuild_gear_values(menu_state_current->tables, game_data.characters[member].gearId);
    gear_shop_compute_gear_summary(menu_state_current->tables, game_data.characters[member].gearId);
    values[0][0] = menu_state_current->tables->gear.attack;
    values[0][1] = menu_state_current->tables->gear.defense;
    values[1][0] = menu_state_current->details->attack[member];
    values[1][1] = menu_state_current->details->defense[member];
    for (k = 0; k < 2; k++) {
        if (values[0][k] >= values[1][k]) {
            change[k] = values[0][k] - values[1][k];
            decrease[k] = 0;
        } else {
            change[k] = values[1][k] - values[0][k];
            decrease[k] = 1;
        }
    }
    game_data.gears[gear].weapons[0] = saved[0];
    game_data.gears[gear].partItems[0] = saved[1];
    game_data.gears[gear].partItems[1] = saved[2];
    game_data.gears[gear].partItems[2] = saved[3];
    game_data.gears[gear].partItems[3] = saved[4];
    game_data.gears[gear].parts[0] = saved[5];
    game_data.gears[gear].parts[1] = saved[6];
    game_data.gears[gear].parts[2] = saved[7];
    gear_shop_rebuild_gear_values(menu_state_current->tables, game_data.characters[member].gearId);
}

/* 801D3A3C: The count held of item `id` in an inventory of `n` ids and counts (0 if absent). */
u16 gear_shop_find_inventory_count(u8 *ids, u8 *counts, s32 n, u8 id) {
    u8 count;
    s32 i;

    count = 0;
    for (i = 0; i < n; i++) {
        if (ids[i] == id) {
            count = counts[i];
            break;
        }
    }
    return count;
}

/* 801D3A80: Show how many of item `id` the party holds in inventory `kind` (3 or 4) beside the list. */
void gear_shop_show_held_count(u8 kind, u8 id) {
    RECT rect;
    u8 codes[4];
    u8 text[8];
    u8 *ids;
    u8 *counts;
    s32 n;
    u8 *pixels;
    u16 count;
    u8 known;

    known = 0;
    switch (kind) {
    case 4:
        ids = game_data.gearPartIds;
        counts = ids - 100;
        n = 100;
        known = 1;
        break;
    case 3:
        ids = game_data.gearAccessoryIds;
        counts = ids - 150;
        n = 150;
        known = 1;
        break;
    }
    if (!known) {
        return;
    }
    count = gear_shop_find_inventory_count(ids, counts, n, id);
    gear_shop_held_count = count;
    pixels = heap_alloc(0x3F6, 0);
    codes[1] = 0;
    codes[3] = 0;
    if (count / 10) {
        codes[0] = count / 10 + 0x10;
    } else {
        codes[0] = 0xC3;
    }
    codes[2] = count % 10 + 0x10;
    text_decode_codes(codes, text, 2);
    menu_state_current->details->label45B0.width = window_render_text_line(text, pixels, 0x24, 1);
    rect.x = 0x198;
    rect.y = 0xB4;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    gear_shop_label_init_quads(&menu_state_current->details->label45B0, 9, 0x80, 0x82);
    gear_shop_set_rect_verts(menu_state_current->details->label45B0.verts, 0xF8, 0x8E, menu_state_current->details->label45B0.width,
                  13);
    menu_state_current->details->label45B0.buffer = menu_state_current->buffer_index;
    menu_state_current->details->label45B0_shown = 1;
    heap_free(pixels);
}

/* 801D3C78
 * Show parts-shop entry `top + row` (`dims` unused): its name label, the bars
 * of the members whose gear can fit it, the marks of those whose gear holds it
 * and, for parts (kinds 3 and 4) each fitting member's attack and defence
 * change (tinted by whether it drops), then how many the party holds.
 * Returns its price.
 */
u32 gear_shop_show_stock_entry(s32 row, s32 top, u8 *dims) {
    RECT rect;
    s32 diffs[2];
    u8 worse[2];
    u32 price;
    u32 holders;
    u32 users;
    u8 id;
    u8 kind;
    u8 *pixels;
    u8 gear;
    s32 i;
    s32 shown;
    s32 k;
    s32 xa;
    s32 xb;

    id = menu_state_current->shop_items[top + row];
    kind = menu_state_current->shop_kinds[top + row];
    pixels = heap_alloc(0x618, 0);
    bzero(pixels, 0x618);
    users = 0;
    switch (kind) {
    case 0:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[3], id), pixels, 0x39, 0);
        price = menu_state_current->tables->frames[id].price;
        users = menu_state_current->tables->frames[id].users;
        break;
    case 1:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[4], id), pixels, 0x39, 0);
        price = menu_state_current->tables->engines[id].price;
        users = menu_state_current->tables->engines[id].users;
        break;
    case 2:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[5], id), pixels, 0x39, 0);
        price = menu_state_current->tables->parts[id].price;
        users = menu_state_current->tables->parts[id].users;
        break;
    case 4:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[1], id), pixels, 0x39, 0);
        price = menu_state_current->tables->gear_weapons[id].price;
        users = menu_state_current->tables->gear_weapons[id].users;
        break;
    case 3:
        menu_state_current->details->label4430.width =
            window_render_text_line(text_get_resource_entry(menu_state_current->details->resources[2], id), pixels, 0x39, 0);
        price = menu_state_current->tables->gear_accessories[id].price;
        users = menu_state_current->tables->gear_accessories[id].users;
        break;
    }
    holders = gear_shop_find_part_holders(id, kind);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    gear_shop_label_init_quads(&menu_state_current->details->label4430, 0, 0, 0);
    gear_shop_quad_place(&menu_state_current->details->label4430.polys[menu_state_current->buffer_index], 0x2C, 0x12, 0, 0x4E,
                  menu_state_current->details->label4430.width, 13);
    gear_shop_set_rect_verts(menu_state_current->details->label4430.verts, 0x2C, 0x12, menu_state_current->details->label4430.width, 13);
    menu_state_current->details->label4430.buffer = menu_state_current->buffer_index;
    heap_free(pixels);
    if (id) {
        menu_state_current->details->label4430_shown = 1;
    } else {
        menu_state_current->details->label4430_shown = 0;
    }
    i = 0;
    shown = 0;
    menu_state_current->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (menu_state_current->present[i] != 0) {
            gear = game_data.characters[i].gearId;
            if (gear_shop_test_bit32(users, gear)) {
                menu_state_current->details->bar_shown[shown] = 1;
            } else {
                menu_state_current->details->bar_shown[shown] = 0;
            }
            if (gear_shop_test_bit32(holders, gear)) {
                menu_state_current->details->group2D0_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE,
                                  &menu_state_current->details->group2D0[menu_state_current->details->group2D0_count * 2],
                                  menu_state_current->buffer_index, gear_shop_portrait_x_table[shown] + 0xE, 0xB4, 0x1000);
            }
            menu_state_current->details->cells_a_count[shown] = 0;
            menu_state_current->details->cells_b_count[shown] = 0;
            if ((kind == 3 || kind == 4) && menu_state_current->details->bar_shown[shown] != 0) {
                diffs[1] = 0;
                diffs[0] = 0;
                gear_shop_compare_fitted_stats(diffs, worse, id, kind, i);
                if (diffs[0] != 0) {
                    gear_shop_split_digits(diffs[0]);
                    for (k = 0, xa = shown * 26 + 0x49; k < 3; k++) {
                        if (menu_state_current->digits[k + 6] != 0xFF) {
                            menu_state_current->details->cells_a_count[shown] += sprite_sheet_draw_scaled(
                                menu_state_current->sheet, menu_state_current->digits[k + 6],
                                &menu_state_current->details->cells_a[shown][menu_state_current->details->cells_a_count[shown] * 2],
                                menu_state_current->buffer_index, xa + k * 8, 0xBE, 0x1000);
                        }
                    }
                    gear_shop_tint_quads(menu_state_current->details->cells_a_count[shown], menu_state_current->details->cells_a[shown],
                                  worse[0]);
                    menu_state_current->details->cells_a_buffer[shown] = menu_state_current->buffer_index;
                }
                if (diffs[1] != 0) {
                    gear_shop_split_digits(diffs[1]);
                    for (k = 0, xb = shown * 26 + 0x49; k < 3; k++) {
                        if (menu_state_current->digits[k + 6] != 0xFF) {
                            menu_state_current->details->cells_b_count[shown] += sprite_sheet_draw_scaled(
                                menu_state_current->sheet, menu_state_current->digits[k + 6],
                                &menu_state_current->details->cells_b[shown][menu_state_current->details->cells_b_count[shown] * 2],
                                menu_state_current->buffer_index, xb + k * 8, 0xC6, 0x1000);
                        }
                    }
                    gear_shop_tint_quads(menu_state_current->details->cells_b_count[shown], menu_state_current->details->cells_b[shown],
                                  worse[1]);
                    menu_state_current->details->cells_b_buffer[shown] = menu_state_current->buffer_index;
                }
            }
            shown++;
        }
    }
    menu_state_current->details->group2D0_buffer = menu_state_current->buffer_index;
    gear_shop_show_held_count(kind, id);
    return price;
}

/* 801D44FC
 * Set the party's gold (capped at 9999999) and apply the purchases: kinds
 * 0-2 are fitted to the gear being edited, kinds 3 and 4 go into their
 * inventories (added to a part already held, at most 99, or into the first
 * free slot).
 */
void gear_shop_settle_purchase(u32 gold) {
    u32 *party_gold;
    s32 i;
    s32 j;
    u8 new_item;

    gear_shop_play_sound(0xD1);
    party_gold = &game_data.gold;
    *party_gold = gold;
    if (gold > 9999999) {
        *party_gold = 9999999;
    }
    for (i = 0; i < 0x30; i++) {
        if (menu_state_current->shop_items[i] != 0 && menu_state_current->details->amounts[i] != 0) {
            switch (menu_state_current->shop_kinds[i]) {
            case 0:
                game_data.gears[gear_shop_edited_gear].frame = menu_state_current->shop_items[i];
                break;
            case 1:
                game_data.gears[gear_shop_edited_gear].engine = menu_state_current->shop_items[i];
                break;
            case 2:
                game_data.gears[gear_shop_edited_gear].field3 = menu_state_current->shop_items[i];
                break;
            case 4:
                new_item = 1;
                for (j = 0; j < 100; j++) {
                    if (game_data.gearPartIds[j] == menu_state_current->shop_items[i]) {
                        new_item = 0;
                        if ((game_data.gearPartCounts[j] += menu_state_current->details->amounts[i]) >= 100) {
                            game_data.gearPartCounts[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 100; j++) {
                        if (game_data.gearPartIds[j] == 0) {
                            game_data.gearPartIds[j] = menu_state_current->shop_items[i];
                            game_data.gearPartCounts[j] = menu_state_current->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            case 3:
                new_item = 1;
                for (j = 0; j < 150; j++) {
                    if (game_data.gearAccessoryIds[j] == menu_state_current->shop_items[i]) {
                        new_item = 0;
                        if ((game_data.gearAccessoryCounts[j] += menu_state_current->details->amounts[i]) >= 100) {
                            game_data.gearAccessoryCounts[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 150; j++) {
                        if (game_data.gearAccessoryIds[j] == 0) {
                            game_data.gearAccessoryIds[j] = menu_state_current->shop_items[i];
                            game_data.gearAccessoryCounts[j] = menu_state_current->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
}

/* 801D4888: Whether shop part `index` differs from the edited gear's part of that kind (0 when the gear already has it or better). */
u8 gear_shop_is_part_upgrade(s32 index) {
    u8 wanted;

    wanted = 1;
    switch (menu_state_current->shop_kinds[index]) {
    case 0:
        if (game_data.gears[gear_shop_edited_gear].frame >= menu_state_current->shop_items[index]) {
            wanted = 0;
        }
        break;
    case 1:
        if (game_data.gears[gear_shop_edited_gear].engine >= menu_state_current->shop_items[index]) {
            wanted = 0;
        }
        break;
    case 2:
        if (game_data.gears[gear_shop_edited_gear].field3 >= menu_state_current->shop_items[index]) {
            wanted = 0;
        }
        break;
    }
    return wanted;
}

/* 801D498C
 * The parts shop's buy list `page` * 3 + cursor. With `fit` (lists 0-2) one
 * part is fitted to the edited gear, trading in its current part; otherwise
 * (lists 3 and 4) parts are bought by amount into the inventories.
 */
u8 gear_shop_buy_list_run(u8 page, u8 fit) {
    u8 dims[8];
    u8 running;
    u8 redraw;
    s32 row;
    u8 first;
    s32 top;
    s32 last_row;
    u32 total;
    s32 last_top;
    u32 gold;
    u32 new_gold;
    u32 funds;
    u32 trade_in;
    u32 credit;
    u8 message;
    u8 confirm;
    s32 panel_x;
    s32 panel_w;
    s32 count;
    u32 price;
    s32 i;
    s32 held_next;
    u8 *stock;

    gold = game_data.gold;
    running = 1;
    first = 1;
    redraw = 1;
    row = 0;
    last_row = 0xFF;
    top = 0;
    last_top = 0xFF;
    total = 0;
    message = 0x8F;
    confirm = 0xFF;
    panel_x = 0xE0;
    credit = 0;
    panel_w = 0x40;
    new_gold = gold;
    funds = new_gold;
    for (i = 0; i < 11; i++) {
        if (game_data.characters[i].gearId != 0xFF) {
            gear_shop_rebuild_gear_values(menu_state_current->tables, game_data.characters[i].gearId);
            gear_shop_compute_gear_summary(menu_state_current->tables, game_data.characters[i].gearId);
            menu_state_current->details->attack[i] = menu_state_current->tables->gear.attack;
            menu_state_current->details->defense[i] = menu_state_current->tables->gear.defense;
        }
    }
    bzero(menu_state_current->shop_items, 0x30);
    bzero(menu_state_current->shop_kinds, 0x30);
    bzero(menu_state_current->details->amounts, 0x30);
    for (i = 0; i < 20; i++) {
        stock = (u8 *)menu_state_current->details->stock;
        stock += (page * 3 + menu_state_current->choice) * 20 + i;
        menu_state_current->shop_items[i] = *stock;
        menu_state_current->shop_kinds[i] = page * 3 + menu_state_current->choice;
    }
    count = gear_shop_stock_list_counts[page * 3 + menu_state_current->choice];
    menu_state_current->images->dim = 1;
    gear_shop_list_cursor_alloc(0);
    while (running) {
        gear_shop_run_frame();
        if (top != last_top || redraw) {
            if (fit) {
                trade_in = gear_shop_layout_stock_rows(top, funds, dims);
            } else {
                trade_in = gear_shop_layout_stock_rows(top, new_gold, dims);
            }
            gear_shop_scroll_bar_show(0xC, 0x32, 0x3C, count, top);
        }
        if (row != last_row || top != last_top) {
            price = gear_shop_show_stock_entry(row, top, dims);
            last_row = row;
            last_top = top;
            menu_state_current->flags->unknown5a[0] = 1;
            if (fit) {
                panel_x = 0xC8;
                panel_w = 0x70;
                redraw = 1;
                new_gold = funds;
                total = 0;
                credit = 0;
                if (dims[row] != 0) {
                    total = price;
                    new_gold -= price;
                    credit = trade_in;
                    new_gold += credit;
                }
            }
        }
        gear_shop_list_cursor_place(row, top, 0, 0);
        if (first) {
            gear_shop_panel_open(2, 0xC, 0x2A, 0xC4 - fit * 0x14, 0x74, 0, 1, 4, 1);
            gear_shop_panel_open(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            gear_shop_panel_open(5, panel_x, 0x7A, panel_w, 0x24, 0, 1, 4, 0);
            gear_shop_label_render_table(2, menu_state_current->list_labels, gear_shop_buy_label_ids, menu_state_current->flags->list_labels_shown);
            gear_shop_label_place(2, menu_state_current->list_labels, gear_shop_buy_label_ids, gear_shop_choice_label_x_offsets, menu_state_current->flags->list_labels_shown,
                          fit, 0, 1);
            if (fit) {
                gear_shop_show_fitted_part_name(menu_state_current->choice + page * 3);
            }
            gear_shop_reveal_portraits();
            gear_shop_layout_buy_headings(fit);
            first = 0;
        }
        if (redraw) {
            gear_shop_layout_gold_numbers(gold, total, new_gold, credit, fit);
            redraw = 0;
        }
        switch (menu_state_current->input) {
        case 4:
            if (total != 0) {
                gear_shop_play_sound(2);
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->scroll_shown = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                menu_state_current->flags->panels_shown[5] = 0;
                running = 0;
                menu_state_current->flags->list_labels_shown[fit] = 0;
                if (fit) {
                    if (gear_shop_is_part_upgrade(top + row)) {
                        message = 0xA3;
                    } else {
                        message = 0xAF;
                        confirm = 0xB2;
                    }
                } else {
                    gear_shop_layout_notice_total(total, 0);
                }
                gear_shop_markers_open(0);
                if (gear_shop_notice_ask_yes_no(message, confirm, 1)) {
                    if (fit) {
                        menu_state_current->details->amounts[top + row] = 1;
                    }
                    gear_shop_settle_purchase(new_gold);
                } else {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->panels_shown[5] = 1;
                    menu_state_current->flags->list_labels_shown[fit] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->cursors_shown[0] = 1;
                }
                gear_shop_markers_close();
                confirm = 0xFF;
            } else {
                gear_shop_play_sound(4);
            }
            break;
        case 5:
            menu_state_current->flags->panels_shown[5] = 0;
            running = 0;
            menu_state_current->flags->list_labels_shown[fit] = 0;
            if (total != 0 && !fit) {
                menu_state_current->flags->unknown5a[0] = 0;
                menu_state_current->flags->panels_shown[2] = 0;
                menu_state_current->flags->panels_shown[3] = 0;
                menu_state_current->flags->scroll_shown = 0;
                menu_state_current->flags->cursors_shown[0] = 0;
                gear_shop_markers_open(0);
                if (!gear_shop_notice_ask_yes_no(0x8C, 0xFF, 1)) {
                    running = 1;
                    menu_state_current->flags->panels_shown[2] = 1;
                    menu_state_current->flags->panels_shown[3] = 1;
                    menu_state_current->flags->panels_shown[5] = 1;
                    menu_state_current->flags->list_labels_shown[0] = 1;
                    menu_state_current->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    menu_state_current->flags->cursors_shown[0] = 1;
                }
                gear_shop_markers_close();
            }
            break;
        case 1:
            row++;
            if (row >= 8) {
                row = 7;
                top++;
                if (count - 8 < top) {
                    top--;
                }
            }
            break;
        case 3:
            row--;
            if (row < 0) {
                top--;
                row = 0;
                if (top < 0) {
                    top = 0;
                }
            }
            break;
        case 0:
            if (dims[row] != 0 && !fit &&
                menu_state_current->details->amounts[top + row] + (held_next = gear_shop_held_count + 1) < 100) {
                total += price;
                new_gold -= price;
                redraw = 1;
                menu_state_current->details->amounts[top + row]++;
            }
            break;
        case 2:
            if (!fit && menu_state_current->details->amounts[top + row] != 0) {
                total -= price;
                new_gold += price;
                redraw = 1;
                menu_state_current->details->amounts[top + row] -= 1;
            }
            break;
        }
    }
    menu_state_current->details->label45B0_shown = 0;
    gear_shop_panel_close(5);
    gear_shop_label_clear_shown(2, menu_state_current->flags->list_labels_shown);
    gear_shop_hide_details(1);
    return 1;
}

/* 801D5398
 * Refuel and repair the edited gear. Fuel costs 10 gold per 100 missing (at
 * least 10); with too little gold, buy what the gold covers. Repairs are free
 * and come with any purchase. A full tank can be repaired without buying fuel.
 */
void gear_shop_refuel_and_repair(void) {
    u8 message;
    u8 mode;
    u8 confirm;
    u8 done;
    u16 units;
    u32 price;

    done = 0;
    message = 0xA6;
    mode = 1;
    confirm = 1;
    if (game_data.gears[gear_shop_edited_gear].maxFuel == game_data.gears[gear_shop_edited_gear].fuel) {
        message = 0xB5;
        if (game_data.gears[gear_shop_edited_gear].hp == game_data.gears[gear_shop_edited_gear].maxHp) {
            message = 0xB8;
            mode = 0;
            confirm = 0;
        } else {
            mode = 2;
        }
    }
    units = (game_data.gears[gear_shop_edited_gear].maxFuel - game_data.gears[gear_shop_edited_gear].fuel) / 100;
    if (units == 0) {
        units = 1;
    }
    price = units * 10;
    gear_shop_panel_open(5, 0xA2, 0xA6, 0x60, 0x14, 0, 1, 4, 0);
    gear_shop_layout_price_digits(game_data.gold);
    if (mode != 0) {
        gear_shop_layout_notice_total(price, 1);
    }
    gear_shop_run_frame();
    gear_shop_markers_open(0);
    if (gear_shop_notice_ask_yes_no(message, 0xFF, confirm) != 0) {
        menu_state_current->flags->unknown5a[1] = 0;
        switch (mode) {
        case 1:
            if (game_data.gold < price) {
                if (gear_shop_notice_ask_yes_no(0xA9, 0xFF, 1) != 0) {
                    if (game_data.gold != 0) {
                        done = 1;
                    }
                    price = game_data.gold / 10;
                    game_data.gears[gear_shop_edited_gear].fuel += price * 100;
                    game_data.gold %= 10;
                    if (game_data.gears[gear_shop_edited_gear].maxFuel < game_data.gears[gear_shop_edited_gear].fuel) {
                        game_data.gears[gear_shop_edited_gear].fuel = game_data.gears[gear_shop_edited_gear].maxFuel;
                    }
                }
            } else {
                game_data.gold -= price;
                game_data.gears[gear_shop_edited_gear].fuel = game_data.gears[gear_shop_edited_gear].maxFuel;
                done = 1;
            }
            break;
        case 2:
            done = 1;
            break;
        }
    }
    if (done) {
        game_data.gears[gear_shop_edited_gear].hp = game_data.gears[gear_shop_edited_gear].maxHp;
        gear_shop_play_sound(0xD1);
    }
    gear_shop_markers_close(0);
    menu_state_current->flags->price_shown = 0;
    gear_shop_panel_close(5);
}

/* 801D573C: Leave the gear list: hide the cursor and labels and the shop list packets. */
u8 gear_shop_leave_gear_list(void) {
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    gear_shop_label_clear_shown(4, menu_state_current->flags->list_labels_shown);
    gear_shop_hide_details(0);
    return 2;
}

/* 801D57A8: Redraw the current gear list: parts (0-2) or the fourth list. */
void gear_shop_fit_or_refuel(void) {
    switch (menu_state_current->choice) {
    case 0:
    case 1:
    case 2:
        gear_shop_buy_list_run(0, 1);
        gear_shop_rebuild_gear_values(menu_state_current->tables, gear_shop_edited_gear);
        break;
    case 3:
        gear_shop_refuel_and_repair();
        break;
    }
}

/* 801D5828
 * The gear parts list of the current command (the fourth command starts at
 * its fourth entry, when gear_shop_leave_gear_list allows it): move the cursor, open the
 * gear view for the chosen list (4), page the members (9, 10), leave (5).
 */
u8 gear_shop_choice_list_run(void) {
    u8 running;
    u8 first;
    u8 base;

    running = 1;
    first = 1;
    base = 0;
    if (menu_state_current->cursor == 3 && !gear_shop_leave_gear_list()) {
        return 1;
    }
    menu_state_current->choice = 0;
    menu_state_current->choice_shown = 0xFF;
    if (menu_state_current->cursor == 3) {
        menu_state_current->choice = 3;
        base = 4;
    }
    while (running) {
        gear_shop_run_frame();
        if (menu_state_current->view_motion == 0 && gear_shop_available_member_count >= 2) {
            menu_state_current->flags->marks_shown = 1;
        }
        if (first) {
            gear_shop_label_render_table(4, menu_state_current->list_labels, &gear_shop_choice_label_ids[base], menu_state_current->flags->list_labels_shown);
            gear_shop_choice_window_open(0);
            first = 0;
        }
        if (menu_state_current->choice != menu_state_current->choice_shown) {
            gear_shop_label_place(4, menu_state_current->list_labels, &gear_shop_choice_label_ids[base], &gear_shop_choice_label_x_offsets[base],
                          menu_state_current->flags->list_labels_shown, menu_state_current->choice, 4, 0);
            gear_shop_choice_window_set_cursor(0);
            menu_state_current->choice_shown = menu_state_current->choice;
        }
        if (menu_state_current->gear_screen->lamp_state[0] == 0) {
            menu_state_current->flags->gear_parts_shown = 1;
            menu_state_current->flags->gear_shown = 0;
        }
        switch (menu_state_current->input) {
        case 4:
            menu_state_current->flags->sprite_shown = 0;
            menu_state_current->flags->cursor_shown = 0;
            menu_state_current->flags->lists_shown = 0;
            menu_state_current->flags->gear_parts_shown = 0;
            gear_shop_label_clear_shown(4, menu_state_current->flags->list_labels_shown);
            gear_shop_play_sound(2);
            gear_shop_gear_view_open();
            menu_state_current->flags->marks_shown = 0;
            switch (menu_state_current->cursor) {
            case 1:
                gear_shop_chosen_sell_list_run(1);
                break;
            case 2:
                gear_shop_buy_list_run(1, 0);
                break;
            case 3:
                gear_shop_fit_or_refuel();
                break;
            }
            gear_shop_gear_view_close();
            gear_shop_run_frame();
            gear_shop_layout_gear_values();
            gear_shop_run_frame();
            menu_state_current->flags->sprite_shown = 1;
            menu_state_current->flags->cursor_shown = 1;
            menu_state_current->flags->lists_shown = 1;
            gear_shop_label_render_table(4, menu_state_current->list_labels, &gear_shop_choice_label_ids[base], menu_state_current->flags->list_labels_shown);
            menu_state_current->choice_shown = 0xFF;
            break;
        case 5:
            running = 0;
            break;
        case 1:
            if (menu_state_current->choice != 0) {
                menu_state_current->choice--;
            } else {
                menu_state_current->choice = menu_state_current->choice_count - 1;
            }
            break;
        case 3:
            if (++menu_state_current->choice >= menu_state_current->choice_count) {
                menu_state_current->choice = 0;
            }
            break;
        case 9:
            gear_shop_switch_member(0);
            break;
        case 10:
            gear_shop_switch_member(1);
            break;
        }
    }
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    gear_shop_label_clear_shown(4, menu_state_current->flags->list_labels_shown);
    return 1;
}

/* 801D5D38: Set up the Gear model: the actor module, its light, both large ordering tables and the two model blocks, then show the first present member's gear. */
void gear_shop_model_init(void) {
    s32 i;

    i = 0;
    gear_model_init(0x40);
    menu_state_current->light.direction.vx = 0x546;
    menu_state_current->light.direction.vy = -0xE39;
    menu_state_current->light.direction.vz = 0x546;
    menu_state_current->light.direction.pad = 0;
    menu_state_current->light.unknown8[0] = 0;
    menu_state_current->light.unknown8[1] = 0;
    menu_state_current->light.unknown8[2] = 0;
    menu_state_current->light.unknown8[3] = 0;
    menu_state_current->light.unknown8[4] = 0;
    menu_state_current->light.colour.m[0][0] = 0x600;
    menu_state_current->light.colour.m[0][1] = 0;
    menu_state_current->light.colour.m[0][2] = 0;
    menu_state_current->light.colour.m[1][0] = 0x600;
    menu_state_current->light.colour.m[1][1] = 0;
    menu_state_current->light.colour.m[1][2] = 0;
    menu_state_current->light.colour.m[2][0] = 0x600;
    menu_state_current->light.colour.m[2][1] = 0;
    menu_state_current->light.colour.m[2][2] = 0;
    gear_model_color_matrix = &menu_state_current->light.colour;
    SetBackColor(0x3C, 0x3C, 0x3C);
    menu_state_current->buffers[0].ot_big = menu_state_big_ots[0];
    menu_state_current->buffers[1].ot_big = menu_state_big_ots[1];
    menu_state_current->model_parts[0] = heap_alloc(sizeof(ModelParts), 0);
    bzero((u_char *)menu_state_current->model_parts[0], sizeof(ModelParts));
    menu_state_current->model_parts[1] = heap_alloc(sizeof(ModelParts), 0);
    bzero((u_char *)menu_state_current->model_parts[1], sizeof(ModelParts));
    while (menu_state_current->present[i] == 0) {
        i++;
    }
    gear_shop_edited_gear = game_data.characters[i].gearId;
    gear_shop_model_read_files(gear_shop_edited_gear, 1);
    gear_shop_count_available_members();
}

/* 801D5EB8: Close the gear model once the view has stopped moving, and release its two blocks. */
void gear_shop_model_close(void) {
    while (menu_state_current->view_motion != 0) {
        gear_shop_run_frame();
    }
    gear_shop_debug_values_on = 0;
    menu_state_current->flags->model_shown = 0;
    gear_shop_run_frame();
    gear_model_shut_down();
    menu_state_current->model_parts[0]->unk12 = 0;
    menu_state_current->model_parts[1]->unk12 = 0;
    heap_free(menu_state_current->model_parts[0]);
    heap_free(menu_state_current->model_parts[1]);
}

/* 801D5F94
 * Summarise gear `id` for the parts screen: its values plus its parts' and
 * its pilot's bonuses.
 */
void gear_shop_compute_gear_summary(MenuTables *table, u8 id) {
    GearRecord *gear;
    CharacterRecord *pilot;
    s32 bonus;

    if (game_data.flags & 0x1000) {
        gear_shop_gear_pilots[9] = 10; /* gear 9's pilot */
    }
    gear = &game_data.gears[id];
    pilot = &game_data.characters[gear_shop_gear_pilots[id]];
    table->gear.hp = gear->hp;
    table->gear.max_hp = gear->maxHp;
    table->gear.defense = gear->bodyDefense + gear->equipBodyDefense;
    table->gear.ether_defense = pilot->etherDefense + pilot->equipEtherDefense + gear->equipArmor + gear->armor;
    table->gear.value68 = gear->field68 + gear->equip68a;
    table->gear.value6a = gear->field6A;
    table->gear.fuel = gear->fuel;
    table->gear.max_fuel = gear->maxFuel;
    bonus = gear->attack * (gear->field74 + gear->equipAttackScale);
    if (id == 5 || id == 13) {
        table->gear.attack = (gear->entries[0].valueE + gear->entries[2].valueE) * 6 / 10 + bonus;
    } else {
        table->gear.attack = gear->entries[0].valueE + bonus;
    }
    table->gear.hit = gear->hitBonus + gear->equipHitBonus;
    table->gear.speed = gear->speed - gear->speedPenalty;
    table->gear.frame_factor = gear->frameFactor;
    table->gear.value9d = gear->field9D;
    table->gear.guard = gear->guard;
}

/* 801D6150: Rebuild gear `id`'s derived values. */
void gear_shop_rebuild_gear_values(MenuTables *table, u8 id) {
    gear_shop_set_gear_engine_values(table, id);
    gear_shop_set_gear_part_values(table, id);
    gear_shop_set_gear_frame_values(table, id);
    gear_shop_sum_gear_accessories(table, id);
    gear_shop_set_gear_weapon_values(table, id);
}

/* 801D61B8: Copy gear `id`'s values from its +2 entry of the table's 18h-byte records, capping +60. */
void gear_shop_set_gear_engine_values(MenuTables *table, u8 id) {
    GearRecord *gear;
    GearEngineInfo *record;

    gear = &game_data.gears[id];
    record = table->engines;
    record += gear->engine;
    gear->maxHp = record->unk4;
    gear->field68 = record->unk8;
    gear->speed = record->unk14;
    gear->frameFactor = record->unk15;
    gear->field9D = record->unk16;
    gear->hitBonus = record->unk17;
    if (gear->maxHp < gear->hp) {
        gear->hp = gear->maxHp;
    }
}

/* 801D6250: Copy gear `id`'s two words from its entry (+8) of the table's 14h-byte records. */
void gear_shop_set_gear_frame_values(MenuTables *table, u8 id) {
    GearRecord *gear;
    GearFrameInfo *entry;

    gear = &game_data.gears[id];
    entry = table->frames;
    entry += gear->frame;
    gear->bodyDefense = entry->unk8;
    gear->armor = entry->unkA;
}

/* 801D62A4: Copy gear `id`'s values from its +3 entry of the table's 10h-byte records, capping +38. */
void gear_shop_set_gear_part_values(MenuTables *table, u8 id) {
    GearRecord *gear;
    GearPartInfo *record;
    u16 limit;

    gear = &game_data.gears[id];
    record = table->parts;
    limit = gear->fuel;
    record += gear->field3;
    gear->maxFuel = record->unk6;
    gear->attack = record->unkC;
    gear->pad3D = record->unkD;
    gear->field3E = record->unkE;
    gear->attackScale = record->unkE;
    if (gear->maxFuel < limit) {
        gear->fuel = gear->maxFuel;
    }
}

/* 801D6334: Sum gear `id`'s three parts into its derived values and effect bits, and update its pilot's ability bits. */
void gear_shop_sum_gear_accessories(MenuTables *table, u8 id) {
    GearRecord *gear;
    GearAccessoryInfo *part;
    u16 *abilities;
    u16 *status;
    u8 i;
    u8 k;

    gear = &game_data.gears[id];
    abilities = &game_data.skills[gear_shop_gear_pilots[id]].unlocksA;
    status = &game_data.skills[gear_shop_gear_pilots[id]].flags1A;
    gear->equipBodyDefense = 0;
    gear->equipArmor = 0;
    gear->equip68a = 0;
    gear->field48 = 0;
    gear->equipGuard = 0;
    gear->equipHitBonus = 0;
    gear->equipSpeed = 0;
    gear->field4F = 0;
    gear->field6E = 0;
    for (i = 0; i < 16; i++) {
        gear->resistances[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        gear->speedBonus[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        ((GearRecordBytes55 *)gear)->bytes55[i] = 0;
    }
    gear->equipFrameFactor = 0;
    gear->field7E = 0;
    gear->status82 = 0;
    gear->status84.half.permanent &= 0xF000;
    *abilities &= 0xDB7F;
    for (k = 0; k < 3; k++) {
        part = table->gear_accessories;
        part += gear->parts[k];
        gear->equipBodyDefense += part->unkD;
        gear->equipArmor += part->unkE;
        gear->equip68a += part->unk6;
        gear->equipGuard += part->unk18;
        gear->equipHitBonus += part->unk14;
        for (i = 0; i < 4; i++) {
            gear->speedBonus[i] += part->unk10[i];
        }
        switch (part->kind) {
        case 1:
            gear->field7E |= part->value;
            break;
        case 2:
            gear->status82 |= part->value;
            break;
        case 3:
            gear->status84.half.permanent |= part->value;
            break;
        case 4:
            for (i = 0; i < 16; i++) {
                gear->field6E |= part->value;
                if (part->value & (0x8000 >> i)) {
                    gear->resistances[i] += part->unk1A;
                }
            }
            break;
        case 5:
            gear->field4F += part->value;
            break;
        case 6:
            if ((*abilities & 0x1000) && (*abilities & 0x800)) {
                *abilities |= 0x400;
            }
            break;
        case 7:
            if ((*abilities & 0x200) && (*abilities & 0x100)) {
                *abilities |= 0x80;
            }
            break;
        case 8:
            if ((*abilities & 0x40) && (*abilities & 0x20)) {
                *abilities |= 0x10;
            }
            break;
        case 9:
            gear->field48 |= part->value;
            /* fallthrough */
        case 10:
            gear->equipAttackScale += part->value;
            break;
        case 11:
            gear->chargeRate += part->value;
            break;
        }
    }
    gear->speedPenalty = gear_shop_compute_gear_speed_penalty(id);
    if (gear->field4F != 0) {
        *status |= 0x8000;
    } else if (id == game_data.characters[gear_shop_gear_pilots[id]].gearId) {
        *status &= 0x7FFF;
    }
}

/* 801D6738: Copy gear `id`'s weapon values from the weapon table; gear 5 and 13 carry three weapons. */
void gear_shop_set_gear_weapon_values(MenuTables *table, u8 id) {
    GearRecord *gear;
    GearWeaponInfo *weapon;

    gear = &game_data.gears[id];
    weapon = table->gear_weapons;
    weapon += gear->weapons[0];
    gear->entries[0].valueE = weapon->unkE;
    gear->entries[0].field0 = weapon->unk12;
    gear->entries[0].value10 = weapon->unk10;
    gear->entries[0].value11 = weapon->unk11;
    gear->fileVariant = weapon->attrs[0];
    gear->spriteVariants[0] = weapon->attrs[1];
    gear->spriteVariants[1] = weapon->attrs[2];
    gear->spriteVariants[2] = weapon->attrs[3];
    if (gear->entries[0].value11 == 100) {
        gear->status84.half.permanent &= 0xFFF;
        gear->status84.half.permanent |= gear->entries[0].field0;
    }
    if (id == 5 || id == 13) {
        weapon = table->gear_weapons;
        weapon += gear->partItems[0];
        gear->entries[0].valueE = weapon->unkE;
        gear->entries[0].field0 = weapon->unk12;
        gear->entries[0].value10 = weapon->unk10;
        gear->entries[0].value11 = weapon->unk11;
        gear->spriteVariants[0] = weapon->attrs[1];
        weapon = table->gear_weapons;
        weapon += gear->partItems[1];
        gear->entries[1].valueE = weapon->unkE;
        gear->entries[1].field0 = weapon->unk12;
        gear->entries[1].value10 = weapon->unk10;
        gear->entries[1].value11 = weapon->unk11;
        gear->spriteVariants[1] = weapon->attrs[2];
        weapon = table->gear_weapons;
        weapon += gear->partItems[3];
        gear->entries[2].valueE = weapon->unkE;
        gear->entries[2].field0 = weapon->unk12;
        gear->entries[2].value10 = weapon->unk10;
        gear->entries[2].value11 = weapon->unk11;
        gear->spriteVariants[2] = weapon->attrs[3];
    }
}

/* 801D690C: Half of (gear `id`'s +44 / 120 less its +75), not below zero. */
u8 gear_shop_compute_gear_speed_penalty(u8 id) {
    GearRecord *gear;
    s16 value;

    gear = &game_data.gears[id];
    value = ((u16)(gear->equip68a / 120) - gear->field75) / 2;
    if (value < 0) {
        value = 0;
    }
    return value;
}
