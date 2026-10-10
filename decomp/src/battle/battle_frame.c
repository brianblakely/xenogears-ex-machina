/* Battle unit from 800BD3AC to 800BFE48: the number popups and the running
 * total, a battle frame (800BE790), the battle modules' load, the battle menu
 * with the acting slot's walk and command file, command motions, value
 * watches and targets, requested loads, gear restarts and effect sprites
 * (Cygnus CDK GCC 2.7.2). 800BD3AC's table at 0x80070ADC sits at 4 mod 8
 * directly after 800B9F78's odd-length table at 0 mod 8; the functions from
 * 800BA4E0 to 800BD2E4 have no rodata, and the boundary is placed at the
 * first function that has. Its own 11 entries are followed directly by
 * 800BFE48's at 0x80070B08 (0 mod 8), so the unit ends before 800BFE48. */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/command.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/highlight.h"
#include "battle/objects.h"
#include "battle/scene.h"
#include "battle/setup.h"
#include "battle/sprite.h"
#include "battle/stage.h"
#include "battle/ui.h"
#include "overlays.h"
#include "own_declarations.h"
#include "popup.h"
#include "resident_views.h"
#include "sprite_effect.h"

/* Functions of other units declared as this unit calls them, which differs
 * from their definitions. */
void battle_tick_frame(s32 skipped);                              /* the result-screen step (a u8 member there) */
void battle_start_object_script(u16 index, u16 mask, s32 script); /* start a stage object's effect (an s16 mask there) */

/* This unit's functions, declared before their first use. */
void battle_damage_popup_destroy(Task *task);
void battle_damage_popup_draw(Task *draw);
void battle_damage_popup_fade(DamagePopup *popup);
void battle_damage_popup_drift(DamagePopup *popup);
void battle_total_popup_show(void);
void battle_total_popup_refresh(void);
void battle_number_popup_show(s32 value);
void battle_format_decimal(s32 value, u8 *text, s32 digits, u8 leading, s32 base);
void battle_read_pads_with_slowdown(void);
void battle_read_pads(void);
void battle_walk_to_target(Sprite *sprite, s32 mode);

/* The unit's own uninitialized variable (its .bss, after
 * battle_flow.c's); the commons follow (battle_common.c). */
static s32 battle_finished_motion_count; /* 800C3CE8: finished sprite motions */

/* This unit's data (800c374c-800c37d4). The flags battle_showing_status_drains and battle_effects_disabled
 * are followed by stray bytes, so they stay original data. */
s32 battle_unread_popup_word = 0; /* 800C374C */
DamagePopup *battle_damage_popups = NULL; /* 800C3750 */
s16 battle_popup_first_glyph_x_table[5] = {-4, -8, -12, -16, -20}; /* 800C3754: the first glyph's x by digit count */
MATRIX battle_popup_view_matrix = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0x200}}; /* 800C3760 */
INCLUDE_ORIGINAL(".data", battle_showing_status_drains, 0x800C3780, 4);
u8 battle_hex_digits[32] = "0123456789ABCDEF0123456789abcdef"; /* 800C3784 */
u32 battle_powers_of_ten[] = {10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000}; /* 800C37A4 */
INCLUDE_ORIGINAL(".data", battle_effects_disabled, 0x800C37C8, 4);
u8 battle_gear_objects_loaded = 0; /* 800C37CC */
s32 battle_frame_nesting = 0; /* 800C37D0 */

/* 800BD3AC: Show value over sprite as a damage popup of kind (replacing the sprite's
 * earlier ones): 1 a prefix glyph, 2 green, 3 magenta with a prefix, 4 a
 * single glyph that only fades, 5 red, 10 and 11 (blue) with a suffix
 * glyph; the digits follow. Only while the battle menu is open. */
void battle_damage_popup_show(Sprite *sprite, s32 value, s32 kind) {
    DamagePopup *popup;
    u8 text[0x38];
    s32 x;
    s32 i;

    if (battle_current_menu == NULL) {
        return;
    }
    battle_total_popup_show();
    battle_unread_gear_attack_step_flag = 0;
    for (popup = battle_damage_popups; popup != NULL; popup = popup->next) {
        if (popup->sprite == sprite) {
            popup->task.destroy(&popup->task);
        }
    }
    popup = (DamagePopup *)task_alloc_two_node_task(sizeof(DamagePopup), NULL, (void (*)(Task *))battle_damage_popup_drift, battle_damage_popup_draw,
                                         battle_damage_popup_destroy);
    popup->sprite = sprite;
    popup->timer = 8;
    popup->next = battle_damage_popups;
    popup->x = sprite->x;
    battle_damage_popups = popup;
    popup->y = sprite->y;
    popup->z = sprite->z;
    popup->scale.vx = 0x2000;
    popup->scale.vy = 0x2000;
    popup->scale.vz = 0x2000;
    popup->colour.rgbc[3] = 0x2D;
    popup->angles.vx = 0;
    popup->angles.vy = 0;
    popup->angles.vz = 0;
    popup->colour.rgbc[0] = 0x80;
    popup->colour.rgbc[1] = 0x80;
    popup->colour.rgbc[2] = 0x80;
    popup->right = sprite->motion.bits.mirror;
    popup->glyphCount = 0;
    if (kind != 4) {
        battle_format_decimal(value, text, 5, 0, 0);
        x = battle_popup_first_glyph_x_table[text[0] - 1];
    }
    switch (kind) {
    case 4:
        popup->timer = 0x40;
        popup->glyphCount = sprite_sheet_fill_parts(battle_glyph_table, 0x7C, popup->glyphs, -0x14, -0x10);
        popup->colour.rgbc[3] = (popup->colour.rgbc[3] & ~1) | 2;
        task_set_update_callback(&popup->task, (void (*)(Task *))battle_damage_popup_fade);
        break;
    case 5:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0;
        popup->colour.rgbc[3] &= ~1;
        break;
    case 10:
        popup->glyphCount += sprite_sheet_fill_parts(battle_glyph_table, 0x7F, &popup->glyphs[popup->glyphCount], x, 0);
        break;
    case 11:
        popup->glyphCount += sprite_sheet_fill_parts(battle_glyph_table, 0x91, &popup->glyphs[popup->glyphCount], x, 0);
        popup->colour.rgbc[2] = 0x80;
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[3] &= ~1;
        break;
    case 1:
        popup->glyphCount += sprite_sheet_fill_parts(battle_glyph_table, 0x80, &popup->glyphs[popup->glyphCount], x, -0x10);
        x += 0x20;
        break;
    case 3:
        popup->glyphCount += sprite_sheet_fill_parts(battle_glyph_table, 0x80, &popup->glyphs[popup->glyphCount], x, -0x10);
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[2] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[3] &= ~1;
        x += 0x20;
        break;
    case 2:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0;
        popup->colour.rgbc[3] &= ~1;
        break;
    }
    if (kind != 4) {
        for (i = 0; i != text[0]; i++, x += 10) {
            popup->glyphCount += sprite_sheet_fill_parts(battle_glyph_table, text[i + 1] + 0x72, &popup->glyphs[popup->glyphCount], x, -0x10);
        }
    }
}

/* 800BD7A0: Damage popup destroy: unlink it from battle_damage_popups and end it. */
void battle_damage_popup_destroy(Task *task) {
    DamagePopup *popup = (DamagePopup *)task;
    DamagePopup *entry = battle_damage_popups;
    DamagePopup *previous = NULL;

    for (; entry != NULL; entry = entry->next) {
        if (entry == popup) {
            if (previous != NULL) {
                previous->next = entry->next;
            } else {
                battle_damage_popups = entry->next;
            }
            break;
        }
        previous = entry;
    }
    task_destroy_two_node_task(task);
}

/* 800BD810: Draw a popup glyph as a textured quad in colour (its word also sets the
 * primitive code) through the current matrices, added to the ordering
 * table's first entry, while the primitive buffer has room. */
void battle_popup_draw_glyph(SpritePart *glyph, s32 colour) {
    POLY_FT4 *poly = (POLY_FT4 *)sprite_queue_next_free;
    long p;
    long flag;
    u16 x, y;
    s32 w, h;
    u8 u, v;
    u8 uw, vh;

    if ((u8 *)sprite_queue_next_free + sizeof(POLY_FT4) < sprite_queue_block_end) {
        sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(POLY_FT4));
        setlen(poly, 9);
        *(s32 *)&poly->r0 = colour;
        poly->tpage = glyph->tpage;
        poly->clut = glyph->clut;
        w = glyph->w;
        h = glyph->h;
        x = glyph->x;
        y = glyph->y;
        sprite_quad_corners[0].vx = x;
        sprite_quad_corners[0].vy = y;
        sprite_quad_corners[1].vx = x + w;
        sprite_quad_corners[1].vy = y;
        sprite_quad_corners[2].vx = x + w;
        sprite_quad_corners[2].vy = y + h;
        sprite_quad_corners[3].vx = x;
        sprite_quad_corners[3].vy = y + h;
        RotTransPers4(&sprite_quad_corners[0], &sprite_quad_corners[1], &sprite_quad_corners[2], &sprite_quad_corners[3], (long *)&poly->x0,
                      (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &p, &flag);
        u = glyph->u;
        v = glyph->v;
        uw = glyph->w;
        vh = glyph->h;
        poly->u0 = u;
        poly->v0 = v;
        poly->u1 = u + uw;
        poly->v1 = v;
        poly->u2 = u;
        poly->v2 = v + vh;
        poly->u3 = u + uw;
        poly->v3 = v + vh;
        addPrim((u32 *)sprite_ot, poly);
    }
}

/* 800BD974: The camera-space offset of point's projection from the geometry offset
 * (doubled), at the screen distance. */
void battle_popup_project_point(SVECTOR *point, VECTOR *out) {
    SVECTOR unused; /* allocated in the original frame */
    s16 screen[2];
    long p;
    long flag;
    long offsetX;
    long offsetY;

    SetRotMatrix(&battle_camera_view_matrix);
    SetTransMatrix(&battle_camera_view_matrix);
    RotTransPers(point, (long *)screen, &p, &flag);
    ReadGeomOffset(&offsetX, &offsetY);
    out->vx = (screen[0] - offsetX) * 2;
    out->vy = (screen[1] - offsetY) * 2;
    out->vz = ReadGeomScreen();
}

/* 800BDA1C: Damage popup draw: its glyphs turned, scaled and placed over its
 * point. */
void battle_damage_popup_draw(Task *draw) {
    MATRIX m;
    SVECTOR point;
    VECTOR offset;
    DamagePopup *popup = draw->data;
    s32 i;
    SpritePart *glyph;

    battle_popup_view_matrix.t[2] = ReadGeomScreen();
    point.vx = popup->x >> 16;
    point.vy = popup->y >> 16;
    point.vz = popup->z >> 16;
    battle_popup_project_point(&point, &offset);
    gpu_build_rotation_matrix(&popup->angles, &m);
    TransMatrix(&m, &offset);
    CompMatrix(&battle_popup_view_matrix, &m, &m);
    ScaleMatrix(&m, &popup->scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0, glyph = popup->glyphs; i != popup->glyphCount; i++, glyph++) {
        battle_popup_draw_glyph(glyph, popup->colour.word);
    }
}

/* 800BDB08: Damage popup fade: darken by 2 a frame until its time is up. */
void battle_damage_popup_fade(DamagePopup *popup) {
    battle_total_popup_refresh();
    popup->colour.rgbc[0] -= 2;
    popup->colour.rgbc[1] -= 2;
    popup->colour.rgbc[2] -= 2;
    if (--popup->timer < 0) {
        popup->task.destroy(&popup->task);
    }
}

/* 800BDB74: Damage popup fade out: darken by 8 a frame (green and blue follow red)
 * until black or its time is up. */
void battle_damage_popup_fade_out(Task *task) {
    DamagePopup *popup = (DamagePopup *)task;

    battle_total_popup_refresh();
    popup->colour.rgbc[0] = sprite_add_clamp_byte(popup->colour.rgbc[0], -8);
    popup->colour.rgbc[1] = sprite_add_clamp_byte(popup->colour.rgbc[0], -8);
    popup->colour.rgbc[2] = sprite_add_clamp_byte(popup->colour.rgbc[0], -8);
    if (--popup->timer < 0 || (popup->colour.rgbc[0] | popup->colour.rgbc[1] | popup->colour.rgbc[2]) == 0) {
        popup->task.destroy(&popup->task);
    }
}

/* 800BDC14: Damage popup hold: after 16 frames switch to the fade out (800BDB74). */
void battle_damage_popup_hold(DamagePopup *popup) {
    battle_total_popup_refresh();
    if (--popup->timer < 0) {
        popup->timer = 16;
        popup->colour.rgbc[3] = (popup->colour.rgbc[3] | 2) & ~1;
        task_set_update_callback(&popup->task, battle_damage_popup_fade_out);
    }
}

/* 800BDC78: Damage popup drift: move 12 a frame to its side, then hold (800BDC14)
 * for 16 frames. */
void battle_damage_popup_drift(DamagePopup *popup) {
    battle_total_popup_refresh();
    if (popup->right) {
        popup->x += 0xC0000;
    } else {
        popup->x -= 0xC0000;
    }
    if (--popup->timer < 0) {
        popup->timer = 16;
        task_set_update_callback(&popup->task, (void (*)(Task *))battle_damage_popup_hold);
    }
}

/* 800BDCF8: Running total destroy: end its draw task and itself. */
void battle_total_popup_destroy(TotalPopup *total) {
    battle_current_total_popup = NULL;
    task_unlink_draw_node(&total->draw);
    task_unlink_main_node(&total->task);
}

/* 800BDD34: The running total's empty update (800BDE58 starts its task with it). */
void battle_total_popup_empty_update(void) {
}

/* 800BDD3C: Running total draw: its label glyph, then its digits turned, scaled and
 * centred on the screen. */
void battle_total_popup_draw(Task *draw) {
    TotalPopup *total = draw->data;
    MATRIX m;
    SVECTOR unused; /* allocated in the original frame */
    VECTOR offset;
    long x;
    long y;
    s32 i;
    SpritePart *glyph;

    ReadGeomOffset(&x, &y);
    sprite_sheet_queue_quads(battle_glyph_table, 0x81, total->x, total->y, (u_long *)sprite_ot);
    offset.vx = (0xA0 - x) * 2;
    offset.vy = (0x46 - y) * 2;
    offset.vz = ReadGeomScreen();
    battle_popup_view_matrix.t[2] = ReadGeomScreen();
    gpu_build_rotation_matrix(&total->angles, &m);
    TransMatrix(&m, &offset);
    CompMatrix(&battle_popup_view_matrix, &m, &m);
    ScaleMatrix(&m, &total->scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0, glyph = total->glyphs; i != total->glyphCount; i++, glyph++) {
        battle_popup_draw_glyph(glyph, total->colour.word);
    }
}

/* 800BDE58: Show the running total (battle_total_popup at (0x90, 0x2A)), unless shown or
 * sprite commands run. */
void battle_total_popup_show(void) {
    if (battle_current_total_popup == NULL && battle_showing_status_drains == 0) {
        battle_current_total_popup = &battle_total_popup;
        task_link_main_node(NULL, &battle_total_popup.task);
        task_link_draw_node(&battle_total_popup.task, &battle_total_popup.draw);
        task_set_update_callback(&battle_total_popup.task, (void (*)(Task *))battle_total_popup_empty_update);
        task_set_draw_callback(&battle_total_popup.draw, battle_total_popup_draw);
        task_set_destroy_callback(&battle_total_popup.task, (void (*)(Task *))battle_total_popup_destroy);
        battle_total_popup.x = 0x90;
        battle_total_popup.y = 0x2A;
        battle_total_popup.task.data = &battle_total_popup;
        battle_total_popup.draw.data = &battle_total_popup;
        battle_total_popup.glyphCount = 0;
        battle_total_popup_shown_value = -1;
    }
}

/* 800BDF1C: Update the running total when battle_running_total changed: pop the value
 * (800BE330) and show it coloured by the popup kind (2 green, 3 magenta,
 * 11 blue, else white). */
void battle_total_popup_refresh(void) {
    s32 value = battle_running_total;
    TotalPopup *total;
    u8 text[8];
    s32 i;
    s32 x;

    if (battle_current_total_popup != NULL && battle_showing_status_drains == 0) {
        total = battle_current_total_popup;
        if (value != battle_total_popup_shown_value) {
            battle_total_popup_shown_value = value;
            battle_number_popup_show(value);
            total->colour.rgbc[3] = 0x2D;
            total->scale.vx = 0x2000;
            total->scale.vy = 0x2000;
            total->scale.vz = 0x2000;
            total->colour.rgbc[3] &= ~1;
            total->angles.vx = 0;
            total->angles.vy = 0;
            total->angles.vz = 0;
            switch (battle_popup_color_kind) {
            case 11:
                total->colour.rgbc[0] = 0;
                total->colour.rgbc[1] = 0;
                total->colour.rgbc[2] = 0x80;
                break;
            case 3:
                total->colour.rgbc[0] = 0x80;
                total->colour.rgbc[1] = 0;
                total->colour.rgbc[2] = 0x80;
                break;
            case 2:
                total->colour.rgbc[0] = 0;
                total->colour.rgbc[1] = 0x80;
                total->colour.rgbc[2] = 0;
                break;
            default:
                total->colour.rgbc[0] = 0x80;
                total->colour.rgbc[1] = 0x80;
                total->colour.rgbc[2] = 0x80;
                break;
            }
            battle_format_decimal(value, text, 5, 0, 0);
            x = battle_popup_first_glyph_x_table[text[0] - 1];
            total->glyphCount = 0;
            for (i = 0; i != text[0]; x += 10) {
                total->glyphCount += sprite_sheet_fill_parts(battle_glyph_table, text[i + 1] + 0x72, &total->glyphs[total->glyphCount], x, -8);
                i++;
            }
        }
    }
}

/* 800BE0DC: Hide the running total (800BDCF8), if shown. */
void battle_total_popup_hide(void) {
    if (battle_current_total_popup != NULL) {
        battle_total_popup_destroy(battle_current_total_popup);
    }
}

/* 800BE108: Clear battle_current_total_popup and battle_unread_popup_word. */
void battle_total_popup_reset(void) {
    battle_current_total_popup = 0;
    battle_unread_popup_word = 0;
}

/* 800BE11C: Popup update: spin and shrink it, fade its colour, end it with its life. */
void battle_number_popup_update(NumberPopup *popup) {
    popup->angle.vz += popup->spin;
    popup->scale.vx += 0x330;
    popup->scale.vy += 0x330;
    popup->scale.vz += 0x330;
    popup->colour.rgbc[0] = sprite_add_clamp_byte(popup->colour.rgbc[0], -4);
    popup->colour.rgbc[1] = sprite_add_clamp_byte(popup->colour.rgbc[1], -4);
    popup->colour.rgbc[2] = sprite_add_clamp_byte(popup->colour.rgbc[2], -4);
    if (--popup->life == 0) {
        popup->destroy(popup);
    }
}

/* 800BE1C4: Popup drawing: its glyphs six times, each copy turned back 5 more and
 * shrunk by 0x330, centred on the screen from the geometry offset. */
void battle_number_popup_draw(PopupTask *task) {
    MATRIX m;
    SVECTOR unused; /* allocated in the original frame */
    SVECTOR angle;
    VECTOR offset;
    VECTOR scale;
    long x;
    long y;
    NumberPopup *popup = task->popup;
    s32 i;
    s32 j;
    SpritePart *glyph;

    ReadGeomOffset(&x, &y);
    offset.vx = (0xA0 - x) * 2;
    offset.vy = (0x46 - y) * 2;
    offset.vz = ReadGeomScreen();
    battle_popup_view_matrix.t[2] = ReadGeomScreen();
    sprite_copy_svector(&angle, &popup->angle);
    scale.vx = popup->scale.vx;
    scale.vy = popup->scale.vy;
    scale.vz = popup->scale.vz;
    for (i = 0; i != 6; i++) {
        gpu_build_rotation_matrix(&angle, &m);
        TransMatrix(&m, &offset);
        CompMatrix(&battle_popup_view_matrix, &m, &m);
        ScaleMatrix(&m, &scale);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        for (j = 0, glyph = popup->glyphs; j != popup->glyphCount; j++, glyph++) {
            battle_popup_draw_glyph(glyph, popup->colour.word);
        }
        angle.vz -= 5;
        scale.vx -= 0x330;
        scale.vy -= 0x330;
        scale.vz -= 0x330;
    }
}

/* 800BE330: Show value as a number popup, coloured by the popup kind battle_popup_color_kind (2
 * green, 3 magenta, 11 blue, else white), spinning one way at random. */
void battle_number_popup_show(s32 value) {
    NumberPopup *popup;
    u8 text[8];
    s32 i;
    s32 x;

    popup = (NumberPopup *)task_alloc_two_node_task(sizeof(NumberPopup), NULL, (void (*)(Task *))battle_number_popup_update,
                                         (void (*)(Task *))battle_number_popup_draw, NULL);
    popup->spin = -(((rand() & 3) - 2) * 8);
    if (popup->spin == 0) {
        popup->spin = 6;
    }
    popup->life = 32;
    popup->colour.rgbc[3] = 0x2E;
    popup->scale.vx = 0x2000;
    popup->scale.vy = 0x2000;
    popup->scale.vz = 0x2000;
    popup->glyphCount = 0;
    popup->angle.vx = 0;
    popup->angle.vy = 0;
    popup->angle.vz = 0;
    switch (battle_popup_color_kind) {
    case 11:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 3:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 2:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0;
        break;
    default:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0x80;
        break;
    }
    battle_format_decimal(value, text, 5, 0, 0);
    x = battle_popup_first_glyph_x_table[text[0] - 1];
    popup->glyphCount = 0;
    for (i = 0; i != text[0]; x += 10) {
        popup->glyphCount += sprite_sheet_fill_parts(battle_glyph_table, text[i + 1] + 0x72, &popup->glyphs[popup->glyphCount], x, -8);
        i++;
    }
    for (i = 0; i != popup->glyphCount; i++) {
        popup->glyphs[i].w--;
        popup->glyphs[i].h--;
    }
}

/* 800BE538: Run up to three commands (kinds 0, 1 and 10) on slot's sprite outside the
 * battle menu and wait frames until they are done. */
void battle_show_status_drain_amounts(s32 slot, s32 first, s32 second, s32 third) {
    Sprite *sprite;
    s32 mode;
    BattleMenu *menu;

    task_active_main_count = 0;
    task_new_tasks_active = 1;
    sprite = BATTLE_AREA.sprites[slot];
    battle_showing_status_drains = 1;
    if (sprite != NULL) {
        mode = (s8)sprite->motion.bytes[3];
        sprite_start_animation(sprite, 10);
        menu = battle_current_menu;
        battle_current_menu = (BattleMenu *)1;
        if (first) {
            battle_damage_popup_show(sprite, first, 0);
        }
        if (second) {
            battle_damage_popup_show(sprite, second, 1);
        }
        if (third) {
            battle_damage_popup_show(sprite, third, 10);
        }
        battle_current_menu = menu;
        while (battle_count_active_tasks()) {
            battle_run_frame();
        }
        while ((s8)sprite->motion.bytes[3] == 10) {
            battle_run_frame();
        }
        sprite_start_animation(sprite, mode);
    }
    battle_showing_status_drains = 0;
    battle_total_popup_hide();
    task_active_main_count = 0;
    task_new_tasks_active = 0;
}

/* 800BE6A0: Write value as digits hexadecimal glyphs (battle_hex_digits, plus base) after
 * the count in text. */
void battle_format_hex(s32 value, u8 *text, s32 digits, s32 base) {
    s32 i;
    s32 last;

    i = 0;
    if (digits != 0) {
        last = digits - 1;
        do {
            text[i + 1] = battle_hex_digits[(value >> ((last - i) * 4)) & 0xF] + base;
        } while (++i != digits);
    }
    text[0] = digits;
}

/* 800BE6E8: Write value in decimal after the count in text: a '-' for a negative
 * value, then its last digits + 1 digits (plus base), leading zeros only
 * when leading is set. */
void battle_format_decimal(s32 value, u8 *text, s32 digits, u8 leading, s32 base) {
    s32 count = 0;
    u8 *out = text + 1;
    u8 digit;

    if (value < 0) {
        count = 1;
        text[1] = '-';
        out = text + 2;
        value = -value;
        digits--;
    }
    while (value %= battle_powers_of_ten[digits], digits != 0) {
        digits--;
        digit = value / battle_powers_of_ten[digits];
        if (digit) {
            leading = 1;
        }
        if (leading) {
            *out++ = digit + base;
            count++;
        }
    }
    *out = value + base;
    text[0] = count + 1;
}

/* 800BE790: Run one battle frame: swap the display buffers, read the controllers,
 * update the sprites, the stage and the effects (the skipped frames once
 * more each) with the stack in the scratchpad, draw, time the frame and
 * present it; the outermost frame also runs the battle menu, a requested
 * single action and the deferred free of the objects' extra files. */
void battle_run_frame(void) {
    BattleArea *frame;
    FrameBuffer *buffer;
    s32 skipped;

    battle_frame_nesting++;
    boot_check_soft_reset();
    battle_camera.start = VSync(-1);
    frame = &BATTLE_AREA;
    buffer = &frame->buffers[0];
    if (frame->current == buffer) {
        buffer = &frame->buffers[1];
    }
    frame->current = buffer;
    frame->ot = buffer->ot;
    ClearOTagR((u_long *)buffer->ot, 0x1000);
    frame->buffer = 1 - frame->buffer;
    if (mode_disc_mode != -1) {
        battle_read_pads_with_slowdown();
        __asm__ volatile(".word 0x0001000D"); /* break 1: the debugger breakpoint */
        battle_debug_run_tools_frame();
    }
    sprite_queue_start_fill(frame->buffer);
    battle_camera_step();
    battle_set_view_and_draw_stage();
    sprite_set_view_matrix(&battle_camera.matrix);
    sprite_set_ot((s32)frame->ot);
    if (mode_disc_mode != -1) {
        console_flush((u_long *)frame->ot);
    }
    battle_update_stage(&battle_camera.matrix, (s32)battle_light_matrix, (u32 *)sprite_ot, frame->buffer);
    SPAD_STACK_ENTER();
    sprite_build_pending_frames();
    task_run_draw_list();
    task_run_main_list();
    skipped = task_catch_up_frame_count;
    while (--skipped != -1) {
        battle_camera_step();
        task_run_main_list();
    }
    SPAD_STACK_LEAVE();
    battle_draw_hud();
    battle_tick_frame(0);
    while (--task_catch_up_frame_count != -1) {
        battle_tick_frame(1);
    }
    battle_camera.cpu = VSync(1);
    DrawSync(0);
    battle_camera.gpu = VSync(1);
    task_catch_up_frame_count = VSync(-1) - battle_camera.start - sprite_frame_skip;
    if (task_catch_up_frame_count < 0) {
        task_catch_up_frame_count = 0;
    }
    if (task_catch_up_frame_count >= 5) {
        task_catch_up_frame_count = 4;
    }
    BATTLE_AREA.frameTicks = task_catch_up_frame_count + sprite_frame_skip;
    if (sprite_frame_skip != 0) {
        VSync(sprite_frame_skip + 1);
    } else {
        VSync(0);
    }
    PutDispEnv(&BATTLE_AREA.current->dispEnv);
    PutDrawEnv(&BATTLE_AREA.current->drawEnv);
    sprite_queue_run_uploads();
    DrawOTag((u_long *)&BATTLE_AREA.current->ot[0xFFF]);
    battle_load_module();
    if (battle_frame_nesting == 1) {
        if (battle_current_menu != NULL) {
            battle_current_menu->update(battle_current_menu);
        }
        if (sprite_single_action_request != 0) {
            s32 action = sprite_single_action_request;

            sprite_single_action_request = 0;
            battle_single_action_run(action);
        }
        if (battle_gear_objects_loaded != 0 && cd_get_pending_read_count() == 0) {
            battle_gear_objects_loaded = 0;
            battle_wait_objects_idle();
        }
    }
    battle_frame_nesting--;
}

/* 800BEB04: Load the requested battle module (sprite_requested_battle_module) into 0x801FC000 when it
 * changed, around the module switch 800B8354, and mark it loaded. */
void battle_load_module(void) {
    s32 saved0;
    s32 saved1;
    u8 module;

    if (sprite_loaded_battle_module != (module = sprite_requested_battle_module)) {
        sprite_loaded_battle_module = sprite_requested_battle_module;
        battle_wait_for_disc();
        cd_get_selected_directory(&saved0, &saved1);
        cd_select_directory(0xC, 2);
        cd_read_file(module + 2, (void *)0x801FC000, 0, 0x80);
        battle_wait_for_disc();
        cd_select_directory(saved0, saved1);
        DrawSync(0);
        VSync(0);
        EnterCriticalSection();
        FlushCache();
        ExitCriticalSection();
    }
    sprite_battle_module_loaded = 1;
}

/* 800BEBC4: Read the controllers; holding select (0x100) slows the frame down. */
void battle_read_pads_with_slowdown(void) {
    battle_read_pads();
    if (BATTLE_AREA.held & 0x100) {
        VSync(8);
        task_catch_up_frame_count = 0;
    }
}

/* 800BEC18: Read both controllers: held, newly pressed and released buttons, and a
 * history of the first controller's last changes. */
void battle_read_pads(void) {
    s32 held;
    u16 old;

    held = pad_read_buttons(0) & 0xFFFF;
    old = BATTLE_AREA.held;
    BATTLE_AREA.held = held;
    BATTLE_AREA.pressed = ~old & held;
    BATTLE_AREA.released = old & ~held;
    held = pad_read_buttons(1);
    BATTLE_AREA.pressed2 = ~BATTLE_AREA.held2 & held;
    BATTLE_AREA.held2 = held;
    BATTLE_AREA.heldOnly = BATTLE_AREA.held & ~held;
    if (BATTLE_AREA.held != BATTLE_AREA.history[0].held) {
        BATTLE_AREA.history[3] = BATTLE_AREA.history[2];
        BATTLE_AREA.history[2] = BATTLE_AREA.history[1];
        BATTLE_AREA.history[1] = BATTLE_AREA.history[0];
        BATTLE_AREA.history[0].held = BATTLE_AREA.held;
        BATTLE_AREA.history[0].pressed = BATTLE_AREA.pressed;
        BATTLE_AREA.history[0].released = BATTLE_AREA.released;
        BATTLE_AREA.history[0].time = battle_frame_start_time;
    }
}

/* 800BED30: Clear the battle menu state. */
void battle_menu_clear(void) {
    battle_unread_menu_word = 0;
    battle_current_menu = NULL;
    battle_knocked_down_mask = 0;
}

/* 800BED4C: Open the battle menu (800B9F78 its update). */
BattleMenu *battle_menu_open(void) {
    BattleMenu *menu = heap_alloc(sizeof(BattleMenu), 0);

    battle_current_menu = menu;
    menu->update = battle_menu_update;
    battle_area_event_index = 0;
    menu->field4A = 0;
    battle_current_menu->field30 = 0;
    battle_current_menu->field48 = 1;
    battle_current_menu->field2C = 1;
    battle_current_menu->sprite = NULL;
    battle_current_menu->field49 = 0;
    battle_current_menu->field34 = 0;
    battle_menu_set_state(0);
    task_active_main_count = 0;
    task_new_tasks_active = 1;
    return battle_current_menu;
}

/* 800BEDE8: Close the battle menu. */
void battle_menu_close(void) {
    heap_free(battle_current_menu);
    task_active_main_count = 0;
    battle_current_menu = NULL;
    task_new_tasks_active = 0;
}

/* 800BEE2C: Run 800AA320 on a 4 KB stack of its own. */
void battle_start_object_script_on_own_stack(s32 index, s32 mask, s32 script) {
    u8 *stack = heap_alloc(0x1000, 1);

    STACK_ENTER(stack + 0xF9C);
    battle_start_object_script(index, mask, script);
    STACK_LEAVE();
    heap_free(stack);
}

/* 800BEEB4: List the slot sprites of the slots in mask (up to 11, NULL-terminated),
 * setting their target; their count. */
s32 battle_list_slot_sprites(u32 mask, Sprite **list, Sprite *target) {
    s32 i;
    s32 count;
    Sprite *sprite;

    i = 0;
    count = i;

    for (; i != 11; i++, mask = (mask & 0xFFFF) >> 1) {
        if (mask & 1) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL) {
                sprite->partner = target;
                list[count] = sprite;
                count++;
            }
        }
    }
    list[count] = NULL;
    return count;
}

/* 800BEF24: The direction from sprite from to sprite to on the ground. */
s16 battle_get_sprite_direction(Sprite *from, Sprite *to) {
    GroundPoint a;
    GroundPoint b;

    a.x = from->x >> 16;
    a.z = from->z >> 16;
    b.x = to->x >> 16;
    b.z = to->z >> 16;
    return sprite_get_ground_direction(b, a);
}

/* 800BEF8C: The direction from sprite to its target point on the ground. */
s16 battle_get_target_direction(Sprite *sprite) {
    GroundPoint a;
    GroundPoint b;

    a.x = sprite->x >> 16;
    a.z = sprite->z >> 16;
    b.x = sprite->target_x;
    b.z = sprite->target_z;
    return sprite_get_ground_direction(b, a);
}

/* 800BEFF4: Make slot the acting slot, returning the previous acting sprite to idle. */
void battle_menu_set_acting_slot(s32 slot) {
    Sprite *sprite = battle_current_menu->sprite;

    if (sprite != NULL && battle_current_menu->slot != slot && !BATTLE_AREA.slots[SPRITE_SLOT(sprite)].hidden) {
        sprite_start_animation(sprite, (s8)sprite->b0.byteb0);
    }
    battle_current_menu->slot = slot;
    battle_current_menu->sprite = BATTLE_AREA.sprites[slot];
}

/* 800BF0B4: Set the battle menu's state. */
void battle_menu_set_state(s32 state) {
    battle_current_menu->state = state;
}

/* 800BF0C4: Walk sprite to the next point of the path, or at its end, to its target. */
void battle_walk_next_path_point(Sprite *sprite) {
    if (BATTLE_AREA.path[battle_current_menu->field2C].x == 0xFFFF && BATTLE_AREA.path[battle_current_menu->field2C].z == 0xFFFF) {
        sprite->target_y = 0;
        sprite->target_x = sprite->x >> 16;
        sprite->target_z = sprite->z >> 16;
        battle_walk_beside_target(sprite, sprite->partner);
        return;
    }
    sprite->target_x = BATTLE_AREA.path[battle_current_menu->field2C].x;
    sprite->target_z = BATTLE_AREA.path[battle_current_menu->field2C].z;
    sprite->target_y = 0;
    battle_walk_to_target(sprite, BATTLE_AREA.path[battle_current_menu->field2C].run ? 3 : 2);
    battle_current_menu->field2C++;
}

/* 800BF1EC: Start sprite moving to its target point with motion mode. */
void battle_walk_to_target(Sprite *sprite, s32 mode) {
    GroundPoint from;
    GroundPoint to;

    from.x = sprite->x >> 16;
    from.z = sprite->z >> 16;
    to.x = sprite->target_x;
    to.z = sprite->target_z;
    battle_current_menu->field44 = battle_get_ground_distance(from, to);
    sprite_set_direction(sprite, battle_get_target_direction(sprite));
    sprite_set_facing(sprite, battle_get_target_direction(sprite));
    sprite_start_animation(sprite, mode);
    battle_menu_set_state(6);
}

/* 800BF2B8: Load the file of sprite's resource for its slot's command. */
void battle_load_slot_command_file(Sprite *sprite) {
    s32 file;
    void *block;

    battle_stop_reads_finish_loads();
    cd_select_directory(0x2C, 1);
    file = ((SpriteSequencer *)sprite->sequencer)->word0;
    block = heap_alloc(cd_get_aligned_file_size(file), 1);
    cd_read_file(file, block, 0, 0x80);
    battle_command_file = block;
    battle_command_file_slot = SPRITE_SLOT(sprite);
}

/* 800BF354: Start the loaded command file once. */
s32 battle_start_command_file_once(void) {
    s32 result;

    if (battle_command_file_started == 0) {
        result = (s32)battle_install_command_file_parts(battle_command_file);
        battle_command_file_started = 1;
    }
    return result;
}

/* 800BF3A4: Stop the started command file. */
void battle_stop_command_file(void) {
    if (battle_command_file_started != 0) {
        battle_free_command_file_sound_bank(battle_command_file);
        battle_command_file_started = 0;
    }
}

/* 800BF3E8: Face sprite and the first target of the current event at each other. */
void battle_face_first_target(Sprite *sprite) {
    Sprite *first;
    s32 slot;
    Sprite *target;

    battle_area_event_target_mask = BATTLE_AREA.events[battle_area_event_index].targetMask;
    if ((battle_area_event_target_count = battle_list_slot_sprites(BATTLE_AREA.events[battle_area_event_index].targetMask, battle_area_event_target_sprites, sprite)) == 0) {
        battle_area_event_target_sprites[0] = sprite;
    }
    target = battle_area_event_target_sprites[0];
    sprite->partner = target;
    target->partner = sprite;
    first = battle_area_event_target_sprites[0];
    slot = SPRITE_SLOT(target);
    battle_current_menu->target = first;
    battle_current_menu->targetSlot = slot;
    sprite_set_facing(sprite, battle_get_sprite_direction(sprite, sprite->partner));
    if ((s8)target->motion.bytes[3] != 0x15) {
        sprite_set_facing(target, battle_get_sprite_direction(sprite->partner, sprite));
    }
}

/* 800BF4F0: At the path's end, step sprite beside target; else walk the path on. */
void battle_walk_beside_target(Sprite *sprite, Sprite *target) {
    s16 x;

    if (BATTLE_AREA.path[battle_current_menu->field2C].x == 0xFFFF && BATTLE_AREA.path[battle_current_menu->field2C].z == 0xFFFF) {
        sprite->x = sprite->target_x << 16;
        sprite->z = sprite->target_z << 16;
        x = target->x >> 16;
        sprite->target_x = (s16)(sprite->x >> 16) >= x ? x + 0x50 : x - 0x50;
        sprite->target_z = target->z >> 16;
        sprite->target_y = 0;
        if (sprite->target_x == (s16)(sprite->x >> 16) && sprite->target_z == (s16)(sprite->z >> 16)) {
            battle_sprite_arrive_at_target(sprite);
            return;
        }
        battle_walk_to_target(sprite, 3);
        battle_menu_set_state(2);
        return;
    }
    battle_walk_next_path_point(sprite);
}

/* 800BF5E8: Count a finished sprite motion. */
void battle_count_finished_motion(void) {
    battle_finished_motion_count++;
}

/* 800BF600: Run command with sprite playing its motion, then wait for the motion's end. */
void battle_single_action_load_during_motion(s32 command, Sprite *sprite) {
    if (sprite->animations == 0) {
        battle_single_action_load(command);
        return;
    }
    battle_finished_motion_count = 0;
    if ((s8)sprite->motion.bytes[3] != 0) {
        sprite_set_completion_callback(sprite, battle_count_finished_motion);
        sprite_start_animation(sprite, (s8)sprite->motion.bytes[3]);
    }
    battle_single_action_load(command);
    if ((s8)sprite->motion.bytes[3] != 0) {
        while (battle_finished_motion_count == 0) {
            battle_run_frame();
        }
        sprite_set_completion_callback(sprite, NULL);
    }
}

/* 800BF6CC: Update the battle menu when it is open. */
void battle_show_results_if_menu_open(void) {
    if (battle_current_menu != NULL) {
        battle_show_results();
    }
}

/* 800BF6F8: task_active_main_count less one while a number popup shows. */
s32 battle_count_active_tasks(void) {
    return task_active_main_count - battle_is_total_popup_shown();
}

/* 800BF720: Whether a number popup shows. */
s32 battle_is_total_popup_shown(void) {
    return battle_current_total_popup != 0;
}

/* 800BF730: Set battle_single_action_start_request. */
void battle_request_single_action_start(s32 value) {
    battle_single_action_start_request = value;
}

/* 800BF73C: Watch a sprite's value; on a rise or a fall under the threshold call back
 * and end. */
void battle_distance_watch_update(Task *task) {
    SlotWatch *watch = (SlotWatch *)task;
    s32 last = watch->value;

    watch->value = battle_sprite_get_target_distance(watch->sprite);
    if (last < watch->value || watch->value < watch->threshold) {
        watch->callback(watch->sprite);
        watch->destroy(watch);
    }
}

/* 800BF7C8: Start watching sprite's value against threshold with callback. */
void battle_distance_watch_start(Sprite *sprite, s32 threshold, void (*callback)(Sprite *sprite)) {
    SlotWatch *watch = (SlotWatch *)task_alloc_main_task(sprite->block, sizeof(SlotWatch) - 0x1C);

    task_set_update_callback((Task *)watch, battle_distance_watch_update);
    watch->callback = callback;
    watch->sprite = sprite;
    watch->mode = (s8)sprite->motion.bytes[3];
    watch->value = battle_sprite_get_target_distance(sprite);
    watch->threshold = threshold;
    sprite->motion.word |= 0x20;
}

/* 800BF85C: Make slot's sprite act on the sprite of slot target alone. */
void battle_set_single_target(s32 slot, s32 target) {
    Sprite *sprite = BATTLE_AREA.sprites[slot];

    if (sprite != NULL) {
        battle_acting_sprite = sprite;
        sprite->partner = BATTLE_AREA.sprites[target];
        battle_area_event_target_mask = 1 << target;
        battle_area_event_target_sprites[0] = BATTLE_AREA.sprites[target];
        battle_area_event_target_sprites[1] = NULL;
    }
}

/* 800BF8CC: Move sprite's target to the next of the event's targets. */
void battle_sprite_next_target(Sprite *sprite) {
    s32 i;

    for (i = 0; i != battle_area_event_target_count; i++) {
        if (battle_area_event_target_sprites[i] == sprite->partner) {
            break;
        }
    }
    if (i >= battle_area_event_target_count) {
        sprite->partner = battle_area_event_target_sprites[0];
    } else {
        sprite->partner = battle_area_event_target_sprites[i + 1];
    }
}

/* 800BF954: The index of sprite among the event's targets. */
s32 battle_get_target_index(Sprite *sprite) {
    s32 i;

    for (i = 0; i != battle_area_event_target_count; i++) {
        if (battle_area_event_target_sprites[i] == sprite) {
            break;
        }
    }
    return i;
}

/* 800BF998: Count an effect hit; at the second, signal event 11. */
void battle_count_effect_hit(void) {
    battle_effect_hit_count++;
    battle_pending_hit_count++;
    if (battle_effect_hit_count == 2) {
        battle_free_object(0xB);
    }
}

/* 800BF9EC: Upload the images of file 1 of directory 0x2C once requested. */
void battle_upload_requested_images(void) {
    void *file;

    if (battle_image_upload_requested != 0) {
        battle_wait_for_disc();
        cd_select_directory(0x2C, 0);
        file = heap_alloc(cd_get_aligned_file_size(1), 0);
        cd_read_file(1, file, 0, 0x80);
        battle_wait_for_disc();
        model_load_image_list(file, 0, 0, 0, 0, 0, 0);
        battle_run_frame();
        heap_free(file);
        battle_image_upload_requested = 0;
    }
}

/* 800BFA9C: Once requested, restart the party slots in gears (in mode 2 all but the
 * acting one) and wait for them to stop moving. */
void battle_restart_party_gears(void) {
    s32 i;
    s32 acting;

    if (battle_gear_restart_mode != 0) {
        battle_finish_loads();
        battle_run_frame();
        battle_run_frame();
        acting = SPRITE_SLOT(battle_acting_sprite);
        for (i = 0; i != 3; i++) {
            if (BATTLE_AREA.slots[i].gear != 0 && (battle_gear_restart_mode != 2 || i != acting) && BATTLE_AREA.slots[i].field2 < 0x11) {
                battle_gear_load_start(i);
            }
        }
        while (battle_gear_object_load_count != 0) {
            battle_run_frame();
        }
        battle_gear_restart_mode = 0;
    }
}

/* 800BFBA0: Load and transfer the sound bank of file 5 of directory 0x2C once. */
void battle_load_wave_bank_5(void) {
    u8 *file;

    if (battle_wave_bank_5_loaded == 0) {
        battle_wait_for_disc();
        cd_select_directory(0x2C, 0);
        file = heap_alloc(cd_get_aligned_file_size(5), 0);
        cd_read_file(5, file, 0, 0x80);
        battle_wait_for_disc();
        if (sound_find_wave_bank(*(u16 *)(file + 0x20)) == 0) {
            battle_release_wave_bank();
            battle_transferred_wave_bank = sound_load_wave_bank((SoundSequence *)file, 0);
            while (sound_sync_transfer(0) != 0) {
                battle_run_frame();
            }
            battle_wave_bank_5_loaded = 1;
            battle_wave_bank_7_loaded = 0;
        }
        heap_free(file);
    }
}

/* 800BFC80: Find the resident effect sprites of sprite with motion mode (any with
 * action 2): action 0 returns the first, the others destroy them. */
Sprite *battle_find_or_destroy_effect_sprites(Sprite *sprite, s32 mode, s32 action) {
    Task *owner = sprite->block;
    Task *task;
    Sprite *child;

    for (task = task_main_list; task != NULL; task = task->next) {
        if (task->owner == owner && (task->link.word & 0x1FFFFFFF) == (owner->id.word & 0x1FFFFFFF)
            && (task->link.word >> 29 & 1)) {
            child = task->data;
            if (child->image == sprite_shared_source) {
                if (action != 2) {
                    if ((s8)child->motion.bytes[3] != mode) {
                        continue;
                    }
                    if (action == 0) {
                        return child;
                    }
                }
                ((Task *)child->block)->destroy(child->block);
            }
        }
    }
    return NULL;
}

/* 800BFD88: Destroy the resident effect sprites of sprite with motion mode. */
void battle_destroy_effect_sprites(Sprite *sprite, s32 mode) {
    battle_find_or_destroy_effect_sprites(sprite, mode, 1);
}

/* 800BFDA8: Give sprite a resident effect sprite playing motion mode, unless it has. */
void battle_add_effect_sprite(Sprite *sprite, s32 mode) {
    u8 saved;
    Sprite *child;

    if (sprite->animations != 0 && battle_find_or_destroy_effect_sprites(sprite, mode, 0) == NULL) {
        void *motion = (void *)(SPRITE_SOURCE->animations[mode + 1] + (s32)SPRITE_SOURCE->animations);
        saved = task_new_tasks_active;
        task_new_tasks_active = 0;
        child = sprite_create_child(sprite, motion, (SpriteSource *)sprite_shared_source);
        child->motion.bytes[3] = mode;
        child->partner = sprite;
        task_new_tasks_active = saved;
        child->b0.wordb0 |= 0x100;
    }
}
