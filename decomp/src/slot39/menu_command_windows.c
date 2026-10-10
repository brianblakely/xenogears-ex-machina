/* Menu overlay unit 801E8070, the last code unit: label placement, the
 * command and choice windows, the name images, the quads' blending and
 * set-up helpers, the drive reset, the development host file read and the
 * disc check. Rodata 801C5278-801C531C, text 801E8070-801E96A4 and its
 * variable at 801EA8F4. Its rodata starts where the jump tables return to
 * 0 mod 8 after 801E433C's odd-length table: the text boundary lies between
 * 801E433C and 801E8070. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libsn.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/stream.h"
#include "menu/panel.h"
#include "menu/screen.h"
#include "menu.h"
#include "file.h"

/* The unit's uninitialized variable, zero in the file after
 * menu_member_screens's: the CdControlB result bytes. */
static u8 menu_cd_control_result[8]; /* 801EA8F4 */

/* 801E8070: Place label `selected` for layout `mode` (0: window row `row`, clearing the
 * other flags first; 1-3, 5, 6: 3D label vertices from the mode's tables;
 * 4: fixed at (4c, 12)) and mark it shown in `flags`. */
void menu_label_place(u8 count, MenuLabel *labels, u8 *table, s32 *offsets, u8 *flags, u8 selected, u8 row,
                   u8 mode) {
    s32 first;

    first = 0;
    switch (mode) {
    case 0:
        menu_label_clear_shown(count, flags);
        (labels[selected].polys + menu_state_current->buffer_index)->x0 = menu_highlight_x_table[row + selected] + 0x16 + offsets[selected];
        (labels[selected].polys + menu_state_current->buffer_index)->y0 = menu_highlight_y_table[row + selected] - 0x22;
        (labels[selected].polys + menu_state_current->buffer_index)->x1 =
            menu_highlight_x_table[row + selected] + 0x16 + offsets[selected] + labels[selected].width;
        (labels[selected].polys + menu_state_current->buffer_index)->y1 = menu_highlight_y_table[row + selected] - 0x22;
        (labels[selected].polys + menu_state_current->buffer_index)->x2 = menu_highlight_x_table[row + selected] + 0x16 + offsets[selected];
        (labels[selected].polys + menu_state_current->buffer_index)->y2 = menu_highlight_y_table[row + selected] - 0x15;
        (labels[selected].polys + menu_state_current->buffer_index)->x3 =
            menu_highlight_x_table[row + selected] + 0x16 + offsets[selected] + labels[selected].width;
        (labels[selected].polys + menu_state_current->buffer_index)->y3 = menu_highlight_y_table[row + selected] - 0x15;
        break;
    case 1:
        menu_set_rect_verts(labels[selected].verts, menu_label_mode1_x_table[selected], menu_label_mode1_y, labels[selected].width, 0xd);
        break;
    case 5:
        first = 8;
    case 2:
        menu_set_rect_verts(labels[selected].verts, menu_label_mode2_x_table[first + selected], menu_label_mode2_y_table[row], labels[selected].width,
                      0xd);
        break;
    case 3:
        menu_set_rect_verts(labels[selected].verts, 0x18, menu_label_mode3_y_table[row], labels[selected].width, 0xd);
        break;
    case 4:
        (labels[selected].polys + menu_state_current->buffer_index)->x0 = 0x4c;
        (labels[selected].polys + menu_state_current->buffer_index)->y0 = 0x12;
        (labels[selected].polys + menu_state_current->buffer_index)->x1 = labels->width + 0x4c;
        (labels[selected].polys + menu_state_current->buffer_index)->y1 = 0x12;
        (labels[selected].polys + menu_state_current->buffer_index)->x2 = 0x4c;
        (labels[selected].polys + menu_state_current->buffer_index)->y2 = 0x1f;
        (labels[selected].polys + menu_state_current->buffer_index)->x3 = labels->width + 0x4c;
        (labels[selected].polys + menu_state_current->buffer_index)->y3 = 0x1f;
        break;
    case 6:
        menu_set_rect_verts(labels[selected].verts, menu_label_mode6_x_table[selected], menu_label_mode6_y_table[selected], labels[selected].width, 0xd);
        break;
    }
    labels[selected].buffer = menu_state_current->buffer_index;
    flags[selected] = 1;
}

/* 801E8474: Open the command window: grow the cursor column one command per two
 * frames (the label column one behind), up to `count` commands. */
void menu_command_window_open(s32 count, MenuCommandImages *images) {
    s32 n;
    s32 i;

    menu_state_current->images->dim = 0;
    menu_state_current->images->dimmed = 0;
    menu_state_current->flags->images_shown = 1;
    for (n = 1; n <= count; n++) {
        if (n != count) {
            menu_state_current->images->count = 0;
            for (i = 0; i < n; i++) {
                menu_state_current->images->count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, images[i].cursor,
                                  &menu_state_current->images->packets[menu_state_current->images->count * 2],
                                  menu_state_current->buffer_index, 0xa0, 0x96, 0x1000);
            }
            menu_state_current->images->buffer = menu_state_current->buffer_index;
        }
        menu_state_current->images->count2 = 0;
        if (n != 1) {
            for (i = 0; i < n - 1; i++) {
                menu_state_current->images->count2 +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, images[i].label,
                                  &menu_state_current->images->packets2[menu_state_current->images->count2 * 2],
                                  menu_state_current->buffer_index, 0xa0, 0x96, 0x1000);
            }
            menu_state_current->images->buffer2 = menu_state_current->buffer_index;
        }
        for (i = 0; i < 2; i++) {
            menu_run_frame();
        }
    }
}

/* Append the sprites of text `text` to a sprite list and its count. A
 * statement macro: its do/while (0) loop notes keep the table addresses
 * hoisted to the function entry as in the original. */
#define ADD_SPRITES(count, list, text)                                                                  \
    do {                                                                                                \
        (count) += sprite_sheet_draw_scaled(menu_state_current->sheet, (text), &(list)[(count) * 2], menu_state_current->buffer_index, \
                                 0xa0, 0x96, 0x1000);                                                   \
    } while (0)

/* 801E86C8: Open the choice window of the command at `offset` past the top cursor:
 * grow its cursor and label columns one choice per two frames (until an
 * empty choice, ffff), counting the choices. */
void menu_choice_window_open(u8 offset) {
    s32 n;
    s32 i;
    s32 growing;

    menu_state_current->lists->first_count = 0;
    menu_state_current->lists->second_count = 0;
    menu_state_current->flags->lists_shown = 1;
    growing = 1;
    for (n = 1; n < 5; n++) {
        menu_state_current->lists->first_count = 0;
        menu_state_current->choice_count = 0;
        for (i = 0; i < n; i++) {
            if (menu_choice_window_images[(offset + menu_state_current->cursor) * 8 + i * 2] != 0xffff) {
                ADD_SPRITES(menu_state_current->lists->first_count, menu_state_current->lists->first,
                            menu_choice_window_images[(offset + menu_state_current->cursor) * 8 + i * 2]);
                menu_state_current->choice_count++;
            } else {
                growing = 0;
            }
        }
        menu_state_current->lists->first_buffer = menu_state_current->buffer_index;
        if (growing) {
            for (i = 0; i < 2; i++) {
                menu_run_frame();
            }
        }
        menu_state_current->lists->second_count = 0;
        for (i = 0; i < n; i++) {
            if (menu_choice_window_images[(offset + menu_state_current->cursor) * 8 + i * 2] != 0xffff) {
                ADD_SPRITES(menu_state_current->lists->second_count, menu_state_current->lists->second,
                            menu_choice_window_images[(offset + menu_state_current->cursor) * 8 + i * 2 + 1]);
            }
        }
        menu_state_current->lists->second_buffer = menu_state_current->buffer_index;
        if (growing) {
            for (i = 0; i < 2; i++) {
                menu_run_frame();
            }
        }
    }
}

/* 801E8978: Lay out the command cursor sprites for `count` commands (images of the
 * `images`, the chosen one `cursor` taking its lit image,
 * +d), then mark the command window for redraw. */
void menu_command_window_set_cursor(u8 count, u8 cursor, MenuCommandImages *images) {
    s32 i;
    s32 image;

    menu_state_current->images->count = 0;
    menu_state_current->images->count2 = 0;
    for (i = 0; i < count; i++) {
        if (i == cursor) {
            image = images[i].cursor + 0xd;
        } else {
            image = images[i].cursor;
        }
        menu_state_current->images->count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, image,
                          &menu_state_current->images->packets[menu_state_current->images->count * 2],
                          menu_state_current->buffer_index, 0xa0, 0x96, 0x1000);
        menu_state_current->images->count2 +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, images[i].label,
                          &menu_state_current->images->packets2[menu_state_current->images->count2 * 2],
                          menu_state_current->buffer_index, 0xa0, 0x96, 0x1000);
    }
    menu_state_current->images->buffer = menu_state_current->buffer_index;
    menu_state_current->images->buffer2 = menu_state_current->buffer_index;
    menu_highlight_place(cursor, 1);
    menu_state_current->flags->sprite_shown = 1;
}

/* 801E8B4C: Lay out the choice cursor sprites of the command at `offset` past the top
 * cursor (the chosen one lit), then mark the window for redraw. */
void menu_choice_window_set_cursor(u8 offset) {
    s32 i;
    s32 image;

    menu_state_current->lists->first_count = 0;
    menu_state_current->lists->second_count = 0;
    for (i = 0; i < menu_state_current->choice_count; i++) {
        if (i == menu_state_current->choice) {
            image = menu_choice_window_images[(offset + menu_state_current->cursor) * 8 + i * 2] + 0xd;
        } else {
            image = menu_choice_window_images[(offset + menu_state_current->cursor) * 8 + i * 2];
        }
        menu_state_current->lists->first_count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, image,
                          &menu_state_current->lists->first[menu_state_current->lists->first_count * 2],
                          menu_state_current->buffer_index, 0xa0, 0x96, 0x1000);
        menu_state_current->lists->second_count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, menu_choice_window_images[(offset + menu_state_current->cursor) * 8 + i * 2 + 1],
                          &menu_state_current->lists->second[menu_state_current->lists->second_count * 2],
                          menu_state_current->buffer_index, 0xa0, 0x96, 0x1000);
    }
    menu_state_current->lists->first_buffer = menu_state_current->buffer_index;
    menu_state_current->lists->second_buffer = menu_state_current->buffer_index;
    menu_highlight_place(menu_state_current->choice + 7, 1);
    menu_state_current->flags->sprite_shown = 1;
}

/* 801E8DA8: Render the two name lines of name pair `image` (ff: blank) and upload them
 * to the label area of row `row` (rows pair up on one 40x13 image). */
void menu_name_image_render(u8 image, u8 row) {
    RECT rect;
    u8 *pixels;

    pixels = heap_alloc(0x3f6, 0);
    bzero(pixels, 0x3f6);
    if (image != 0xff) {
        window_render_text_line(game_data.names[(image >> 1) * 2], pixels, 0x24, 0);
        window_render_text_line(game_data.names[(image >> 1) * 2 + 1], pixels, 0x24, 1);
    }
    rect.x = menu_name_image_vram_x_table[row >> 1] + 0x180;
    rect.y = menu_name_image_vram_y_table[row >> 1];
    rect.w = 0x28;
    rect.h = 0xd;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    heap_free(pixels);
}

/* 801E8EAC: Set `poly`'s blending for `mode`: 0 opaque, 1 additive-dim, 2 plain,
 * 3 dim. */
void menu_quad_set_blending(POLY_FT4 *poly, u8 mode) {
    u8 shade;

    SetShadeTex(poly, 0);
    switch (mode) {
    case 1:
        poly->tpage |= 0x20;
        SetSemiTrans(poly, 1);
    case 3:
        shade = 0x21;
        break;
    case 0:
        SetSemiTrans(poly, 0);
    case 2:
        shade = 0x80;
        break;
    default:
        return;
    }
    poly->r0 = shade;
    poly->g0 = shade;
    poly->b0 = shade;
}

/* 801E8F60: Set the blending of panel `index`'s quads of the current buffer: plain
 * (2), or dim (3) when the dim flag `mode` is set. Every edge list is walked
 * four pairs deep, so each pass also covers the list after it, as the
 * original does. */
void menu_panel_set_dimmed(u8 index, u8 mode) {
    s32 i;

    /* The dim flag becomes the blending mode. */
    if (mode) {
        mode = 3;
    } else {
        mode = 2;
    }
    for (i = 0; i < 4; i++) {
        menu_quad_set_blending(&menu_state_current->panels[index]->corner[i * 2 + menu_state_current->panels[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        menu_quad_set_blending(&menu_state_current->panels[index]->edge[0][i * 2 + menu_state_current->panels[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        menu_quad_set_blending(&menu_state_current->panels[index]->edge[1][i * 2 + menu_state_current->panels[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        menu_quad_set_blending(&menu_state_current->panels[index]->edge[2][i * 2 + menu_state_current->panels[index]->buffer], mode);
    }
    for (i = 0; i < 4; i++) {
        menu_quad_set_blending(&menu_state_current->panels[index]->edge[3][i * 2 + menu_state_current->panels[index]->buffer], mode);
    }
    for (i = 0; i < 2; i++) {
        menu_quad_set_blending(&menu_state_current->panels[index]->bar_ends[i * 2 + menu_state_current->panels[index]->buffer], mode);
    }
    menu_quad_set_blending(&menu_state_current->panels[index]->bar_side[menu_state_current->panels[index]->buffer], mode);
}

/* 801E91C4: Make `poly` semi-transparent, textured without shading, at neutral colour. */
void menu_quad_set_semi_transparent(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* 801E920C: Place `poly` at (x, y) with size (w, h) and texture origin (u, v). */
void menu_quad_place(POLY_FT4 *poly, u16 x, u16 y, u8 u, u8 v, u16 w, u16 h) {
    poly->x0 = x;
    poly->y0 = y;
    poly->x1 = x + w;
    poly->y1 = y;
    poly->x2 = x;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->u0 = u;
    poly->v0 = v;
    poly->u1 = u + w;
    poly->v1 = v;
    poly->u2 = u;
    poly->v2 = v + h;
    poly->u3 = u + w;
    poly->v3 = v + h;
}

/* 801E927C: Initialise `poly` as an opaque textured quad at neutral colour. */
void menu_quad_init(POLY_FT4 *poly) {
    SetPolyFT4(poly);
    SetSemiTrans(poly, 0);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* 801E92CC: On the disc (8002c3d8 is 0 without the PC file server): stop the read, set
 * normal speed (CdlSetmode 0, then a pause) and repeat CdlStop every
 * VSync(3) until it succeeds. */
void menu_cd_stop_drive(void) {
    if (cd_has_pc_file_server() == 0) {
        cd_stop_read(0);
        cd_sync_reads(0);
        cd_set_mode(0);
        cd_sync_reads(0);
        VSync(3);
        do {
            VSync(3);
        } while (CdControlB(8, 0, menu_cd_control_result) == 0);
    }
}

/* 801E9340: Read `size` bytes of host file `name` into `buffer` (development PC link). */
void menu_read_pc_file(char *name, void *buffer, s32 size) {
    s32 handle;

    handle = PCopen(name, 0, 0);
    PCread(handle, buffer, size);
    PCclose(handle);
}

/* 801E93A0: Check that disc `disc` is in the drive and load its index: from the host
 * files on the development link, else once the lid has opened and closed and
 * the motor runs, from the label at sector 0x17 and the index and directory
 * table at sectors 0x18 and 0x28. Returns 0 when loaded, 2 when the seek
 * fails as for a disc that is not a PlayStation disc or the label is not
 * Xenogears', 3 for the other disc. */
s32 menu_cd_check_disc(s32 disc) {
    DiscLabel label = { { 0 } };
    CdlLOC pos;
    s32 result;
    s32 ok;

    cd_sync_reads(0);
    if (cd_has_pc_file_server() != 0) {
        result = 0;
        if (disc == 1) {
            menu_read_pc_file("c:\\work\\cdrom.mdg", cd_file_index, 0x8000);
            menu_read_pc_file("c:\\work\\cdrom.fid", cd_directory_table, 0x7a);
            menu_read_pc_file("c:\\work\\cdrom.fnd", cd_pc_file_names, 0x40000);
        } else {
            menu_read_pc_file("c:\\work\\cdrom2.mdg", cd_file_index, 0x8000);
            menu_read_pc_file("c:\\work\\cdrom2.fid", cd_directory_table, 0x7a);
            menu_read_pc_file("c:\\work\\cdrom2.fnd", cd_pc_file_names, 0x40000);
        }
        return result;
    }
    CdIntToPos(0, &pos);
    do {
        VSync(3);
        CdControlB(1, 0, menu_cd_control_result);
    } while (!(menu_cd_control_result[0] & 0x10));
    do {
        VSync(3);
        CdControlB(1, 0, menu_cd_control_result);
    } while (menu_cd_control_result[0] & 0x10);
    do {
        VSync(3);
        ok = CdControlB(1, 0, menu_cd_control_result);
    } while (!(menu_cd_control_result[0] & 2) || ok == 0);
retry:
    do {
        VSync(3);
    } while (CdControlB(0x13, 0, menu_cd_control_result) == 0);
    do {
        VSync(3);
    } while (CdControlB(2, (u_char *)&pos, menu_cd_control_result) == 0);
    ok = CdControlB(0x15, 0, menu_cd_control_result);
    if ((menu_cd_control_result[0] & 1) && (menu_cd_control_result[1] & 0x40)) {
        if (ok == 0) {
            return 2;
        }
    } else if (ok == 0) {
        goto retry;
    }
    cd_set_mode(0xa0);
    cd_sync_reads(0);
    VSync(3);
    VSync(3);
    cd_read_raw_sectors(0x17, &label, 0x10, 0, 0);
    cd_sync_reads(0);
    if (label.tag == 0x4e45585f) {
        if (label.disc == disc + '0') {
            cd_read_raw_sectors(0x18, cd_file_index, 0x8000, 0, 0);
            result = 0;
            cd_sync_reads(0);
            cd_read_raw_sectors(0x28, cd_directory_table, 0x7a, 0, 0);
            cd_sync_reads(0);
        } else {
            result = 3;
        }
    } else {
        result = 2;
    }
    return result;
}
