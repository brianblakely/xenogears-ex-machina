/* arena_menu_screens: text 8007E528-80081ECC, rodata 8006FE1C-800701B0, data
 * 80091230-8009178C, variables 800926D4-80092768 and 80095498-80095580.
 * The menu's text (the banner, font, cursor and colours), the selection
 * screen (the entry list, portraits and the two sides' wheels), the
 * settings, vibration and options pages, the choice menus and captions,
 * and the screen fades. Its jump tables lie at 4 mod 8 (8006FE1C-
 * 8007019C); its first function reads its variables and 80081D2C is the
 * last that does, so its end lies at 80081E00, 80081E6C or 80081ECC (the
 * two fades between touch only commons); the latest is kept. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/text.h"
#include "actor.h"
#include "bout.h"
#include "camera.h"
#include "display.h"
#include "effects.h"
#include "helpers.h"
#include "menus.h"
#include "mode.h"
#include "packets.h"
#include "resident_views.h"
#include "script.h"
#include "select.h"
#include "sound.h"
#include "text.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static POLY_FT4 *arena_text_quads[2]; /* 800926D4: text quads, per draw buffer */
static s32 arena_text_quad_count; /* 800926DC */
static u16 arena_text_tpage; /* 800926E0: text texture page */
static u16 arena_text_clut; /* 800926E4: text CLUT */
static s16 arena_text_cursor_x; /* 800926E8 */
static s16 arena_text_cursor_y; /* 800926EC */
static u8 arena_text_red; /* 800926F0: text colour r, g, b */
static u8 arena_text_green; /* 800926F4 */
static u8 arena_text_blue; /* 800926F8 */
static u8 arena_menu_blur_count; /* 800926FC */
static s32 arena_select_first_pick; /* 80092700: first side's pick */
static s32 arena_select_second_pick; /* 80092704: second side's pick */
static u8 arena_text_banner_timer; /* 80092708 */
static GridCell *arena_select_portrait_slots; /* 8009270C */
static u32 arena_select_confirmed_sides; /* 80092710: bit 0/1: controller port 1/2 unavailable */
static s32 arena_select_first_previous_pick; /* 80092714 */
static s32 arena_select_first_neighbor_slide; /* 80092718 */
static s32 arena_select_first_pick_slide; /* 8009271C */
static s32 arena_select_second_previous_pick; /* 80092720 */
static s32 arena_select_second_neighbor_slide; /* 80092724 */
static s32 arena_select_second_pick_slide; /* 80092728 */
static s32 arena_select_first_name_highlight; /* 8009272C: port 1 vibration entry selected */
static s32 arena_select_second_name_highlight; /* 80092730: port 2 vibration entry selected */
static Menu *arena_current_menu; /* 80092734: menu being shown */
static Menu *arena_menu_previous; /* 80092738: menu to return to */
static u8 arena_menu_upper_caption; /* 8009273C */
static u8 arena_menu_lower_caption; /* 80092740 */
static s32 arena_menu_selected_caption; /* 80092744: caption of the selected line */
static s32 arena_menu_buttons_held; /* 80092748: bit 0: both sides may pick the same entry; pad buttons held */
static u32 arena_menu_buttons_repeated; /* 8009274C: pad buttons repeating this frame */
static u32 arena_menu_buttons_pressed; /* 80092750: pad buttons pressed this frame */
static s32 arena_select_second_side_picking; /* 80092754 */
static u8 arena_menu_paused; /* 80092758 */
static u8 arena_menu_blur_pending; /* 8009275C */
static void *arena_menu_screen_copy; /* 80092760: loaded image data */
static u8 arena_menu_stick_deflected; /* 80092764: stick is deflected */

/* Its larger ones, past the program's end (not in the file), each unit's
 * after every unit's small ones (menu.mk). */
static DR_MOVE arena_menu_screen_copy_moves[2]; /* 80095498 */
static DR_TPAGE arena_menu_panel_tpages[2]; /* 800954C8 */
static SceneSprite arena_text_banner_sprites[2]; /* 800954D8 */
/* The upper and lower captions, which share one pixel buffer
 * (arena_menu_init_captions). */
static Caption arena_menu_captions[2]; /* 80095510 */
static DR_TPAGE arena_menu_caption_tpages[2]; /* 80095570 */

/* Menu font glyphs (arena_text_find_glyph maps characters to these). */
Glyph arena_text_glyphs[] = { /* 80091230 */
    { 0x3C, 0x13, 0x15, 0x12 }, { 0x18, 0, 5, 0x12 }, { 0x20, 0, 0xC, 0x12 },
    { 0x30, 0, 0xC, 0x12 }, { 0x40, 0, 0xB, 0x12 }, { 0x4C, 0, 0xB, 0x12 },
    { 0x58, 0, 0xF, 0x12 }, { 0x68, 0, 0xA, 0x12 }, { 0x74, 0, 0xF, 0x12 },
    { 0x84, 0, 0xF, 0x12 }, { 0x94, 0, 9, 0x12 }, { 0xA0, 0, 0xA, 0x12 },
    { 0xAC, 0, 0xB, 0x12 }, { 0xB8, 0, 0xE, 0x12 }, { 0xC8, 0, 9, 0x12 },
    { 0xD4, 0, 9, 0x12 }, { 0xE0, 0, 0xB, 0x12 }, { 0xEC, 0, 9, 0x12 },
    { 0, 0x13, 3, 0x12 }, { 4, 0x13, 6, 0x12 }, { 0xC, 0x13, 9, 0x12 },
    { 0x18, 0x13, 6, 0x12 }, { 0x20, 0x13, 0xE, 0x12 }, { 0x30, 0x13, 9, 0x12 },
    { 0x3C, 0x13, 0x15, 0x12 }, { 0x54, 0x13, 0xB, 0x12 }, { 0x60, 0x13, 0x15, 0x13 },
    { 0x78, 0x13, 0xA, 0x12 }, { 0x84, 0x13, 8, 0x12 }, { 0x90, 0x13, 9, 0x12 },
    { 0x9C, 0x13, 8, 0x12 }, { 0xA8, 0x13, 0xB, 0x12 }, { 0xB4, 0x13, 0x10, 0x12 },
    { 0xC8, 0x13, 0xA, 0x12 }, { 0xD4, 0x13, 0xB, 0x12 }, { 0xE0, 0x13, 9, 0x12 },
    { 0xF8, 0, 4, 0x12 }, { 0xEC, 0x13, 3, 0x12 }, { 0xF0, 0x13, 7, 0x12 },
    { 0xF8, 0x13, 7, 0x12 }, { 0, 0, 0x16, 0x12 }, { 0, 0, 8, 0 },
    { 0x60, 0x13, 3, 7 },
};

s32 arena_text_width_scale = 0x100; /* 800912DC: text width scale (0x100 = 1) */

/* Neighbour offsets and slide of the selection wheel portraits, per row. */
s16 arena_select_wheel_neighbors[2][4] = { { 1, -1, 2, 0x24 }, { -1, -2, 1, -0x24 } }; /* 800912E0 */

s32 arena_menu_copy_one_more_frame = 0; /* 800912F0 */

/* Handlers of the menu lines (arena_menu_pages, defined below): they take no
 * argument or the menu index. */
void arena_select_step_first_pick();
void arena_menu_toggle_port1_vibration();
void arena_select_step_second_pick();
void arena_menu_toggle_port2_vibration();
void arena_select_enter();
void arena_menu_choose_tutorial();
void arena_menu_step_match_count();
void arena_menu_step_level();
void arena_menu_toggle_rubber_band();
void arena_menu_resume_bout();
void arena_menu_step_motion_speed();
void arena_menu_step_frame_rate();
void arena_menu_toggle_first_actor_com();
void arena_menu_toggle_second_actor_com();
void arena_menu_give_up();
void arena_menu_step_ai_command();
void arena_menu_draw_settings_values();
void arena_menu_draw_bonus_battling_values();
void arena_menu_draw_practice_values();

/* 8007E528: Set the scene state, playing sound 0x24 when state 10 starts from 0. */
void arena_text_set_banner_timer(s32 state) {
    if (arena_text_banner_timer == 0 && state == 10) {
        arena_sound_play_effect(0x24);
    }
    arena_text_banner_timer = state;
}

/* 8007E574: While the scene state counts down, draw its sprite (when flag 2 is set). */
void arena_text_draw_banner(void *ot) {
    if (arena_text_banner_timer != 0) {
        if (arena_frame_count & 2) {
            AddPrim(ot, &arena_text_banner_sprites[arena_draw_buffer_index].sprite);
            AddPrim(ot, &arena_text_banner_sprites[arena_draw_buffer_index].tpage);
        }
        arena_text_banner_timer--;
    }
}

/* 8007E624: The text quads used this frame. */
s32 arena_text_get_quad_count(void) {
    return arena_text_quad_count;
}

/* 8007E634: Allocate the text quads, load the font (with its palette's colours 0, 2
 * and 3 replaced) and the banner image, and build the banner sprite. */
void arena_text_load_font_and_banner(MenuImageFile *files) {
    TIM_IMAGE image;
    SceneSprite *banner;
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s16 *palette;
    s32 i;

    arena_text_quads[0] = heap_alloc(0xFA0, 0);
    arena_text_quads[1] = heap_alloc(0xFA0, 0);
    for (i = 0; i < 100; i++) {
        ((u8 *)&arena_text_quads[0][i].tag)[3] = 0;
        ((u8 *)&arena_text_quads[1][i].tag)[3] = 0;
    }
    OpenTIM(files->font);
    ReadTIM(&image);
    palette = (s16 *)image.caddr;
    palette[2] = -0x6F9D;
    palette[0] = 0;
    palette[3] = -1;
    LoadImage(image.crect, image.caddr);
    LoadImage(image.prect, image.paddr);
    arena_text_clut = GetClut(image.crect->x, image.crect->y);
    arena_text_tpage = GetTPage(0, 1, image.prect->x, image.prect->y);
    arena_text_quad_count = 0;
    OpenTIM(files->banner);
    ReadTIM(&image);
    palette = (s16 *)image.caddr;
    palette[0] = 0;
    LoadImage(image.crect, image.caddr);
    LoadImage(image.prect, image.paddr);
    banner = &arena_text_banner_sprites[0];
    setlen(&arena_text_banner_sprites[0].sprite, 4);
    setcode(&arena_text_banner_sprites[0].sprite, 0x65);
    SetDrawTPage(&banner->tpage, 0, 0, GetTPage(0, 1, image.prect->x, image.prect->y));
    arena_text_banner_sprites[0].sprite.clut = GetClut(image.crect->x, image.crect->y);
    arena_text_banner_sprites[0].sprite.x0 = 0x40;
    arena_text_banner_sprites[0].sprite.y0 = 0xBE;
    arena_text_banner_sprites[0].sprite.w = 0xC4;
    arena_text_banner_sprites[0].sprite.h = 0xD;
    arena_text_banner_sprites[0].sprite.u0 = image.prect->x * 4;
    arena_text_banner_sprites[0].sprite.v0 = image.prect->y;
    arena_text_banner_sprites[1] = *banner;
}

/* 8007E894: Move the text cursor. */
void arena_text_move_cursor(s32 x, s32 y) {
    arena_text_cursor_x = x;
    arena_text_cursor_y = y;
}

/* 8007E8AC: The font glyph of a character: digits, capitals and a few punctuation
 * marks; NULL for anything else. */
Glyph *arena_text_find_glyph(s32 ch) {
    if (ch >= '0' && ch <= '9') {
        ch -= '0';
    } else if (ch >= 'A' && ch <= 'Z') {
        ch -= 'A' - 10;
    } else {
        switch (ch) {
        case '!':
            ch = 0x24;
            break;
        case ':':
            ch = 0x25;
            break;
        case '-':
            ch = 0x26;
            break;
        case '/':
            ch = 0x27;
            break;
        case '#':
            ch = 0x28;
            break;
        case ' ':
            ch = 0x29;
            break;
        case '\'':
            ch = 0x2A;
            break;
        default:
            return NULL;
        }
    }
    return &arena_text_glyphs[ch];
}

/* 8007E954: Set the text width scale (0x100 = 1). */
void arena_text_set_width_scale(s32 value) {
    arena_text_width_scale = value;
}

/* 8007E964: Draw one character at the text cursor (at most 101 quads a frame; '('
 * only advances) and move the cursor right by its scaled width. Declared
 * int without a return value, as the original's unfilled delay slot shows.
 * The vertices, UVs and colour word are written through casts of the packet
 * fields (not struct member stores), so no global load moves above them;
 * the texture page/CLUT and the length are member stores. */
s32 arena_text_draw_char(s32 ch) {
    POLY_FT4 *quad;
    Glyph *glyph;
    s32 right;

    if (arena_text_quad_count < 101) {
        quad = arena_text_quads[arena_draw_buffer_index];
        quad += arena_text_quad_count;
        glyph = arena_text_find_glyph(ch);
        if (glyph != NULL) {
            if (ch != '(') {
                ch = glyph->width | 3; /* the quad width, in the same variable */
                *(u32 *)&quad->x0 = arena_text_cursor_x | (arena_text_cursor_y << 16);
                right = arena_text_cursor_x + ((ch * arena_text_width_scale) >> 8);
                *(u32 *)&quad->x1 = right | (arena_text_cursor_y << 16);
                *(u32 *)&quad->x2 = arena_text_cursor_x | ((arena_text_cursor_y + glyph->height) << 16);
                *(u32 *)&quad->x3 = right | ((arena_text_cursor_y + glyph->height) << 16);
                *(u16 *)&quad->u0 = glyph->u | (glyph->v << 8);
                *(u16 *)&quad->u1 = (glyph->u + ch) | (glyph->v << 8);
                *(u16 *)&quad->u2 = glyph->u | ((glyph->v + (glyph->height + 1)) << 8);
                *(u16 *)&quad->u3 = (glyph->u + ch) | ((glyph->v + (glyph->height + 1)) << 8);
                quad->tpage = arena_text_tpage;
                quad->clut = arena_text_clut;
                setlen(quad, 9);
                *(u32 *)&quad->r0 = arena_text_red | (arena_text_green << 8) | (arena_text_blue << 16) | 0x2C000000;
                arena_text_quad_count++;
            }
            arena_text_cursor_x += ((glyph->width * arena_text_width_scale) >> 8) + 2;
        }
    }
}

/* 8007EB6C: Width of a text string in pixels at the current text scale. */
s32 arena_text_measure_width(u8 *text) {
    s32 width = 0;

    while (*text != 0) {
        width += ((arena_text_find_glyph(*text++)->width * arena_text_width_scale) >> 8) + 2;
    }
    return width;
}

/* 8007EBE0: Draw a line of text at the cursor and move the cursor to the next line. */
void arena_text_draw_line(u8 *text) {
    s32 x = arena_text_cursor_x;

    while (*text != 0) {
        arena_text_draw_char(*text++);
    }
    arena_text_cursor_x = x;
    arena_text_cursor_y += 0x14;
}

/* 8007EC54: Draw a line of text centred on the cursor, then move to the next line. */
void arena_text_draw_line_centered(u8 *text) {
    s32 x = arena_text_cursor_x;

    arena_text_cursor_x -= arena_text_measure_width(text) / 2;
    while (*text != 0) {
        arena_text_draw_char(*text++);
    }
    arena_text_cursor_x = x;
    arena_text_cursor_y += 0x14;
}

/* 8007ECF0: Draw a line of text ending at the cursor, then move to the next line. */
void arena_text_draw_line_right_aligned(u8 *text) {
    s32 x = arena_text_cursor_x;

    arena_text_cursor_x -= arena_text_measure_width(text);
    while (*text != 0) {
        arena_text_draw_char(*text++);
    }
    arena_text_cursor_x = x;
    arena_text_cursor_y += 0x14;
}

/* 8007ED84: Draw a line of text shifted left by an offset, then move to the next line. */
void arena_text_draw_line_shifted_left(u8 *text, s32 offset) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    s32 x = arena_text_cursor_x;

    arena_text_cursor_x = x - offset;
    while (*text != 0) {
        arena_text_draw_char(*text++);
    }
    arena_text_cursor_x = x;
    arena_text_cursor_y += 0x14;
}

/* 8007EE08: Set the text colour: highlighted (fading red) or plain white. */
void arena_text_set_highlight(s32 highlight) {
    if (highlight) {
        arena_text_red = pad_vblank_count * 20;
        arena_text_green = 0xFF;
        arena_text_blue = 0;
    } else {
        arena_text_red = 0xFF;
        arena_text_green = 0xFF;
        arena_text_blue = 0xFF;
    }
}

/* 8007EE68: Set the text colour: highlighted (fading toward blue) or plain white.
 * Sample the VBlank counter separately for the red and green channels. */
void arena_text_set_blue_highlight(s32 highlight) {
    if (highlight) {
        s32 red = 0xFF - pad_vblank_count * 20;
        s32 green = 0xFF - pad_vblank_count * 20;

        arena_text_blue = 0xFF;
        arena_text_red = red;
        arena_text_green = green;
        return;
    }
    arena_text_red = 0xFF;
    arena_text_green = 0xFF;
    arena_text_blue = 0xFF;
}

/* 8007EEE8: Build the list of the 49 entries (or, when filtering, of those whose
 * required level the current level reaches) and order it when filtering. */
void arena_select_build_list(s32 filter) {
    s32 level = game_data.vars[0];
    ListEntry **list = heap_alloc(0xC4, 1);
    MoveList *source;
    s32 i;

    arena_select_entries = list;
    source = arena_actor_move_lists;
    arena_select_entry_count = 0;
    for (i = 0; i < 49; source++, i++) {
        if (!filter || source->level <= level) {
            arena_select_entries[arena_select_entry_count++] = &arena_select_gears[i];
        }
    }
    if (filter) {
        arena_progress_add_unlocked_entry();
    }
}

/* 8007EFB4: Allocate and lay out the 49 portrait slots: palette rows 511 down and
 * a 7x7 grid of image areas. */
void arena_select_alloc_portrait_slots(void) {
    u8 unused[0x30]; /* never used; the original frame keeps its slot */
    GridCell *cell;
    s32 id;
    s32 row;
    s32 col;
    s16 top;

    cell = arena_select_portrait_slots = heap_alloc(0x3D4, 0);
    id = 0x1FF;
    for (row = 0; row < 7; row++) {
        top = row * 0x20 + 0x1A0;
        for (col = 0; col < 7; col++) {
            s16 left = col << 6;

            cell->clut.x = 0x200;
            cell->clut.y = id--;
            cell->clut.w = 0x80;
            cell->clut.h = 1;
            cell->image.x = top;
            cell->image.y = left;
            cell->image.w = 0x1E;
            cell->image.h = 0x40;
            cell++;
        }
    }
}

/* 8007F05C: Draw the portrait of a list entry (the index wraps around the list) on
 * the left or right side. At fade 64 it is shown full size and unshaded;
 * below, it is shaded and shrunk by fade / 16, inset from x and grown from
 * a 0x34 by 0x38 base. */
void arena_select_draw_portrait(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade) {
    GridCell *cell;
    s32 inset;
    s32 top;
    s32 bottom;
    s32 left;
    s32 width;
    s32 height;
    s32 u;

    if (right_side) {
        x += 0xD3;
    } else {
        x += 0x33;
    }
    if (index > arena_select_entry_count - 1) {
        index -= arena_select_entry_count;
    }
    if (index < 0) {
        index += arena_select_entry_count;
    }
    index = arena_select_entries[index]->id;
    cell = &arena_select_portrait_slots[index];
    if (fade == 0x40) {
        quad->len = 9;
        ((u8 *)&quad->rgbc)[3] = 0x2D;
        quad->xy0 = x | 0x300000;
        quad->xy1 = (x + 0x3C) | 0x300000;
        quad->xy2 = x | 0x700000;
        quad->xy3 = (x + 0x3C) | 0x700000;
    } else {
        fade += 0x40;
        quad->len = 9;
        quad->rgbc = fade | (fade << 8) | (fade << 16) | 0x2C000000;
        fade -= 0x40;
        fade >>= 4; /* from here on, how far the portrait shrinks */
        inset = fade - 4;
        left = x - inset;
        top = 0x34 - fade;
        quad->xy0 = left | (top << 16);
        width = 0x34;
        quad->xy1 = (left + width + fade * 2) | (top << 16);
        height = 0x38;
        bottom = top + height + fade * 2;
        quad->xy2 = left | (bottom << 16);
        quad->xy3 = (left + width + fade * 2) | (bottom << 16);
    }
    u = cell->image.x * 2;
    quad->uv0 = u | (cell->image.y << 8);
    quad->uv1 = (u + 0x3B) | (cell->image.y << 8);
    quad->uv2 = u | ((cell->image.y + 0x3F) << 8);
    quad->uv3 = (u + 0x3B) | ((cell->image.y + 0x3F) << 8);
    quad->clut = GetClut(cell->clut.x, cell->clut.y);
    quad->tpage = GetTPage(1, 0, cell->image.x & 0xFF80, cell->image.y);
    AddPrim(arena_current_ot, quad);
}

/* 8007F258: Draw the two-player selection: each side's pick, sliding in from its
 * previous one (the long way round wraps), with its neighbours when the
 * side is available, then "VS" and both names. The arguments are unused. */
void arena_select_draw_wheels(void *packets, s32 arg) {
    VECTOR unused[2]; /* the original frame has 32 unused bytes */
    PolyFT4Words *quad = arena_select_wheel_quads[arena_draw_buffer_index];
    s32 step;
    s32 row;

    step = arena_select_first_previous_pick - arena_select_first_pick;
    if (step != 0) {
        if (abs(step) >= 4) {
            step = -step;
        }
        arena_select_first_neighbor_slide = step > 0 ? -0x24 : 0x24;
        arena_select_first_pick_slide = arena_select_first_neighbor_slide = arena_select_first_neighbor_slide; /* the original rereads it */
    }
    step = arena_select_second_previous_pick - arena_select_second_pick;
    if (step != 0) {
        if (abs(step) >= 4) {
            step = -step;
        }
        arena_select_second_neighbor_slide = step > 0 ? -0x24 : 0x24;
        arena_select_second_pick_slide = arena_select_second_neighbor_slide = arena_select_second_neighbor_slide;
    }
    if (arena_select_first_neighbor_slide != 0) {
        arena_select_first_neighbor_slide = arena_select_first_neighbor_slide > 0 ? arena_select_first_neighbor_slide - 2 : arena_select_first_neighbor_slide + 2;
    }
    if (arena_select_first_pick_slide != 0) {
        arena_select_first_pick_slide = arena_select_first_pick_slide > 0 ? arena_select_first_pick_slide - 4 : arena_select_first_pick_slide + 4;
    }
    if (arena_select_second_neighbor_slide != 0) {
        arena_select_second_neighbor_slide = arena_select_second_neighbor_slide > 0 ? arena_select_second_neighbor_slide - 2 : arena_select_second_neighbor_slide + 2;
    }
    if (arena_select_second_pick_slide != 0) {
        arena_select_second_pick_slide = arena_select_second_pick_slide > 0 ? arena_select_second_pick_slide - 4 : arena_select_second_pick_slide + 4;
    }
    arena_select_first_previous_pick = arena_select_first_pick;
    arena_select_second_previous_pick = arena_select_second_pick;
    row = arena_select_first_neighbor_slide >= 0;
    arena_select_draw_portrait(arena_select_first_pick, quad++, 0, arena_select_first_pick_slide, ((0x24 - abs(arena_select_first_pick_slide)) << 6) / 36);
    if (!(arena_select_confirmed_sides & 1)) {
        arena_select_draw_portrait(arena_select_first_pick + arena_select_wheel_neighbors[row][0], quad++, 0, arena_select_wheel_neighbors[row][3] + arena_select_first_neighbor_slide,
                      (abs(arena_select_first_neighbor_slide) << 6) / 36);
        arena_select_draw_portrait(arena_select_first_pick + arena_select_wheel_neighbors[row][1], quad++, 0, -0x24, 0);
        arena_select_draw_portrait(arena_select_first_pick + arena_select_wheel_neighbors[row][2], quad++, 0, 0x24, 0);
    }
    row = arena_select_second_neighbor_slide >= 0;
    arena_select_draw_portrait(arena_select_second_pick, quad++, 1, arena_select_second_pick_slide, ((0x24 - abs(arena_select_second_pick_slide)) << 6) / 36);
    if (!(arena_select_confirmed_sides & 2)) {
        arena_select_draw_portrait(arena_select_second_pick + arena_select_wheel_neighbors[row][0], quad++, 1, arena_select_wheel_neighbors[row][3] + arena_select_second_neighbor_slide,
                      (abs(arena_select_second_neighbor_slide) << 6) / 36);
        arena_select_draw_portrait(arena_select_second_pick + arena_select_wheel_neighbors[row][1], quad++, 1, -0x24, 0);
        arena_select_draw_portrait(arena_select_second_pick + arena_select_wheel_neighbors[row][2], quad, 1, 0x24, 0);
    }
    arena_text_move_cursor(0xA0, 0x78);
    arena_text_draw_line_centered("VS");
    arena_text_move_cursor(0x50, 0x78);
    if (arena_progress_is_flag_set(arena_select_entries[arena_select_first_pick]->id)) {
        arena_text_set_blue_highlight(arena_select_first_name_highlight);
    } else {
        arena_text_set_highlight(arena_select_first_name_highlight);
    }
    arena_text_draw_line_centered(arena_select_entries[arena_select_first_pick]->name);
    arena_text_move_cursor(0xF0, 0x78);
    if (arena_progress_is_flag_set(arena_select_entries[arena_select_second_pick]->id)) {
        arena_text_set_blue_highlight(arena_select_second_name_highlight);
    } else {
        arena_text_set_highlight(arena_select_second_name_highlight);
    }
    arena_text_draw_line_centered(arena_select_entries[arena_select_second_pick]->name);
    arena_text_set_highlight(0);
}

/* Names of the game levels, then (after the frame rates and speeds) of the
 * entries of setting 10. GCC emits an initializer's string literals last to
 * first, after those of the code before it. */
char *arena_menu_level_names[] = { "EASY", "NORMAL", "HARD" }; /* 800912F4 */

u8 arena_menu_frame_rates[] = { 0x1E, 0x14, 0xF, 0xC, 0xA, 6, 5, 4, 3, 2, 1 }; /* 80091300 */

s32 arena_menu_motion_speeds[] = { 0x60, 0x80, 0xBB, 0x100, 0x180, 0x200, 0x300, 0x400 }; /* 8009130C */

char *arena_menu_ai_command_names[] = { /* 8009132C */
    "BYSTANDER", "ON GUARD", "CONTROLLER2", "UP AND AT'EM",
    "SLOWPOKE", "MAGIC FIRER ", "MAGIC JUMPER", "KANGAROO",
    "GIVE CHASE", "RUN AWAY", "BACK DASH", "EASY BATTLE",
    "NORMAL BATTLE", "HARD BATTLE",
};

s32 arena_menu_applied_pad_port = 0; /* 80091364 */

/* 8007F834: Hide both captions and forget the selected line's caption. */
void arena_menu_hide_captions(void) {
    arena_menu_lower_caption = 0;
    arena_menu_upper_caption = 0;
    arena_menu_selected_caption = 0;
}

/* 8007F854: Leave the settings screen: camera mode 1, and both actors' previous
 * stance effect state (unkD4 bits 2-3) set to 3. */
void arena_menu_resume_bout(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    arena_menu_copy_one_more_frame = 1;
    arena_mode_set_state(1);
    arena_current_menu = NULL;
    arena_menu_hide_captions();
    ACTOR_STANCE_BITS(&arena_first_actor)->prev_stance = 3;
    ACTOR_STANCE_BITS(&arena_second_actor)->prev_stance = 3;
}

/* 8007F8B4: Leave the menus: camera mode 1, no menu shown, the captions hidden. */
void arena_menu_close(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    arena_mode_set_state(1);
    arena_current_menu = NULL;
    arena_menu_hide_captions();
}

/* 8007F8E4: Close the system menu and go on: scene mode 3 when option 6 is set or
 * the first round was played, else mode 6 with the round count stepped
 * back (the round is played again). */
void arena_menu_give_up(void) {
    arena_menu_open_pause(0);
    if (arena_settings.option6 != 0 || arena_bout_round_number == 1) {
        arena_mode_set_state(3);
    } else {
        arena_bout_round_number--;
        arena_mode_set_state(6);
    }
}

/* 8007F948: Highlight the text of a page's entry when it is under the cursor. */
void arena_menu_highlight_entry(Menu *page, s32 entry) {
    if (page->cursor == entry) {
        arena_text_set_highlight(1);
    } else {
        arena_text_set_highlight(0);
    }
}

/* 8007F97C: Name of the chosen first setting. */
char *arena_menu_get_level_name(void) {
    return arena_menu_level_names[arena_settings.level];
}

/* 8007F9A0: Draw the values column of the settings page, right-aligned, applying the
 * chosen speed as it is shown. */
void arena_menu_draw_settings_values(Menu *page) {
    char text[8];

    arena_text_move_cursor(page->panel[0].x0 + page->panel[0].w - 10, page->y);
    arena_text_set_highlight(0);
    arena_menu_highlight_entry(page, 0);
    arena_text_draw_line_right_aligned(arena_menu_get_level_name());
    arena_menu_highlight_entry(page, 1);
    sprintf(text, "%d", arena_settings.speed + 1);
    arena_settings.unkC = arena_bout_motion_speed = arena_menu_motion_speeds[arena_settings.speed];
    arena_text_draw_line_right_aligned(text);
    arena_menu_highlight_entry(page, 2);
    sprintf(text, "%dFPS", arena_menu_frame_rates[arena_settings.rate]);
    arena_text_draw_line_right_aligned(text);
    arena_menu_highlight_entry(page, 3);
    arena_text_draw_line_right_aligned(arena_settings.com1 ? "COM" : "USER1");
    arena_menu_highlight_entry(page, 4);
    arena_text_draw_line_right_aligned(arena_settings.driven ? "COM" : "USER2");
    arena_text_set_highlight(0);
}

/* 8007FB0C: Draw the values column of the second settings page; the chosen entry of
 * setting 10 is also passed to 80081100 as 0x15 + entry. */
void arena_menu_draw_practice_values(Menu *page) {
    char text[8];

    arena_text_move_cursor(page->panel[0].x0 + page->panel[0].w - 10, page->y);
    arena_text_set_highlight(0);
    arena_text_draw_line_right_aligned("");
    arena_menu_highlight_entry(page, 1);
    arena_text_draw_line_right_aligned(arena_menu_ai_command_names[arena_settings.command]);
    arena_menu_show_caption(arena_settings.command + 0x15, 1);
    arena_menu_highlight_entry(page, 2);
    sprintf(text, "%dFPS", arena_menu_frame_rates[arena_settings.rate]);
    arena_text_draw_line_right_aligned(text);
    arena_text_set_highlight(0);
}

/* 8007FBEC: Draw the vibration page: per controller port, the vibration setting when
 * a type-4 controller without the "COM" setting is connected (the entry is
 * hidden otherwise). */
void arena_menu_draw_port_pages(void) {
    Menu *page;
    s32 active;
    s32 unused[2]; /* unused in the original; reserves 8 bytes */

    arena_text_move_cursor(0xA0, 0x8C);
    active = arena_select_confirmed_sides ^ 1;
    active &= 1;
    page = &arena_menu_pages[5];
    if (active && arena_menu_pages[5].cursor == 0) {
        arena_select_first_name_highlight = 1;
    } else {
        arena_select_first_name_highlight = 0;
    }
    arena_text_move_cursor(0x50, 0x8C);
    if (pad_get_controller_kind(0) == 4 && arena_settings.com1 == 0) {
        if (active) {
            arena_menu_highlight_entry(page, 1);
        }
        arena_text_draw_line_centered((arena_settings.option4 & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        arena_menu_pages[5].items[1].flags &= ~4;
    } else {
        arena_menu_pages[5].items[1].flags |= 4;
    }
    arena_text_set_highlight(0);

    active = arena_select_confirmed_sides >> 1;
    active ^= 1;
    active &= 1;
    if (active && arena_select_second_side_picking == 0) {
        active = 0;
    }
    page = &arena_menu_pages[6];
    if (active && arena_menu_pages[6].cursor == 0) {
        arena_select_second_name_highlight = 1;
    } else {
        arena_select_second_name_highlight = 0;
    }
    arena_text_move_cursor(0xF0, 0x8C);
    if (pad_get_controller_kind(1) == 4 && arena_settings.driven == 0) {
        if (active) {
            arena_menu_highlight_entry(page, 1);
        }
        arena_text_draw_line_centered((arena_settings.option5 & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        arena_menu_pages[6].items[1].flags &= ~4;
    } else {
        arena_menu_pages[6].items[1].flags |= 4;
    }
    arena_text_set_highlight(0);
    arena_select_draw_wheels(arena_current_ot, 1);
}

/* 8007FE48: Draw the values column of the options page. */
void arena_menu_draw_bonus_battling_values(Menu *page) {
    char text[16];
    char *value;

    arena_text_set_highlight(0);
    arena_text_move_cursor(page->panel[0].x0 + page->panel[0].w - 10, page->y);
    arena_text_draw_line_right_aligned("");
    arena_text_draw_line_right_aligned("");
    arena_text_draw_line_right_aligned("");
    arena_text_draw_line_right_aligned("");
    arena_menu_highlight_entry(page, 4);
    if (arena_settings.option6 != 0) {
        sprintf(text, "%d", arena_settings.option6);
        value = text;
    } else {
        value = "#";
    }
    arena_text_draw_line_right_aligned(value);
    arena_menu_highlight_entry(page, 5);
    arena_text_draw_line_right_aligned(arena_menu_level_names[arena_settings.level]);
    arena_menu_highlight_entry(page, 6);
    arena_text_draw_line_right_aligned(arena_rubber_band_enabled ? "ON" : "OFF");
    arena_text_set_highlight(0);
}

/* 8007FF70: Step a settings value with left/right: flag 4 reverses the direction,
 * flag 2 uses the repeating buttons, flag 1 wraps around (else clamps
 * silently). Plays the cursor sound when moved. */
s32 arena_menu_step_setting(s32 value, s32 max, s32 flags) {
    s32 step = 1;
    u32 buttons;
    s32 moved;

    if (flags & 4) {
        step = -1;
    }
    moved = 0;
    if (flags & 2) {
        buttons = arena_menu_buttons_repeated;
    } else {
        buttons = arena_menu_buttons_pressed;
    }
    if (buttons & 0x2000) {
        value += step;
    }
    if (buttons & 0x8000) {
        value -= step;
    }
    if (buttons & 0xA000) {
        moved = 1;
    }
    if (flags & 1) {
        if (value == -1) {
            value = max;
        }
        if (value > max) {
            value = 0;
        }
    } else {
        if (value == -1) {
            moved = 0;
            value = 0;
        }
        if (value > max) {
            moved = 0;
            value = max;
        }
    }
    if (moved) {
        arena_sound_play_effect(0x20);
    }
    return value;
}

/* 80080054: Menu line handlers: step one setting with left/right (arena_menu_step_setting):
 * the level, the speed, the frame rate, each port's vibration, each side's
 * computer control, option 6, rubber band battle and the opponent's
 * command. */
void arena_menu_step_level(void) {
    arena_settings.level = arena_menu_step_setting(arena_settings.level, 2, 0);
}

/* 80080090 */
void arena_menu_step_motion_speed(void) {
    arena_settings.speed = arena_menu_step_setting(arena_settings.speed, 7, 2);
}

/* 800800CC */
void arena_menu_step_frame_rate(void) {
    arena_settings.rate = arena_menu_step_setting(arena_settings.rate, 4, 2);
}

/* 80080108 */
void arena_menu_toggle_port1_vibration(void) {
    arena_settings.option4 = arena_menu_step_setting(arena_settings.option4, 1, 1);
}

/* 80080144 */
void arena_menu_toggle_port2_vibration(void) {
    arena_settings.option5 = arena_menu_step_setting(arena_settings.option5, 1, 1);
}

/* 80080180 */
void arena_menu_toggle_first_actor_com(void) {
    arena_settings.com1 = arena_menu_step_setting(arena_settings.com1, 1, 1);
}

/* 800801BC */
void arena_menu_toggle_second_actor_com(void) {
    arena_settings.driven = arena_menu_step_setting(arena_settings.driven, 1, 1);
}

/* 800801F8 */
void arena_menu_step_match_count(void) {
    arena_settings.option6 = arena_menu_step_setting(arena_settings.option6, 3, 2);
}

/* 80080234 */
void arena_menu_toggle_rubber_band(void) {
    arena_rubber_band_enabled = arena_menu_step_setting(arena_rubber_band_enabled, 1, 1);
}

/* 80080268 */
void arena_menu_step_ai_command(void) {
    arena_settings.command = arena_menu_step_setting(arena_settings.command, 13, 3);
}

/* 800802A4: First side's selection: cancel, move (skipping the other side's pick
 * unless shared picks are allowed or both already coincide) and confirm. */
void arena_select_step_first_pick(void) {
    s32 same;

    if (arena_menu_applied_pad_port == 0 && (pad_port0_pressed & 0x40)) {
        arena_select_confirmed_sides &= ~1;
        arena_actor_release_model(0);
        arena_sound_play_effect(0x22);
    }
    if (!(arena_select_confirmed_sides & 1)) {
        same = arena_select_first_pick == arena_select_second_pick;
        do {
            arena_select_first_pick = arena_menu_step_setting(arena_select_first_pick, arena_select_entry_count - 1, 3);
        } while (arena_select_first_pick == arena_select_second_pick && !(arena_menu_buttons_held & 1) && !same);
        if (arena_menu_applied_pad_port == 0 && (pad_port0_pressed & 0x20)) {
            arena_select_confirmed_sides |= 1;
            arena_sound_play_effect(0x21);
            arena_actor_load_model(0, arena_select_entries[arena_select_first_pick]->id);
        }
    }
}

/* 8008040C: Second side's selection; cancelling outside mode 2 leaves the screen. */
void arena_select_step_second_pick(void) {
    s32 same;

    if (arena_menu_buttons_pressed & 0x40) {
        if (arena_play_mode != 2) {
            arena_select_confirmed_sides = 0;
            arena_actor_release_model(1);
            arena_sound_play_effect(0x22);
            return;
        }
        arena_select_confirmed_sides &= ~2;
    }
    if (!(arena_select_confirmed_sides & 2)) {
        same = arena_select_first_pick == arena_select_second_pick;
        do {
            arena_select_second_pick = arena_menu_step_setting(arena_select_second_pick, arena_select_entry_count - 1, 3);
        } while (arena_select_first_pick == arena_select_second_pick && !(arena_menu_buttons_held & 1) && !same);
        if (arena_menu_buttons_pressed & 0x20) {
            arena_select_confirmed_sides |= 2;
            arena_sound_play_effect(0x21);
            arena_actor_load_model(1, arena_select_entries[arena_select_second_pick]->id);
        }
    }
}

/* 80080570: Show both sides' picks; entries 4, 7, 11, 30 and 31 show as a plain
 * flag instead, depending on the other side's pick. */
void arena_select_load_picked_models(void) {
    s32 first = arena_select_entries[arena_select_first_pick]->id;
    s32 second = arena_select_entries[arena_select_second_pick]->id;

    switch (first) {
    case 4:
    case 7:
    case 11:
    case 30:
    case 31:
        first = second == 0;
        break;
    }
    arena_actor_load_model(0, first);
    switch (second) {
    case 4:
    case 7:
    case 11:
    case 30:
    case 31:
        second = first != 1;
        break;
    }
    arena_actor_load_model(1, second);
}

/* 80080644: Load both picks' portraits (palette and image) into their VRAM slots and
 * mark both sides confirmed. */
void arena_select_load_pick_portraits(s32 first, s32 second) {
    u8 *data = heap_alloc(0x2000, 0);
    u8 *other;
    GridCell *cell;

    cd_read_raw_sectors(cd_get_file_sector(6) + first * 2, data, 0x1000, 0, 0);
    other = data + 0x1000;
    cd_read_raw_sectors(cd_get_file_sector(6) + second * 2, other, 0x1000, 0, 0);
    arena_select_first_pick = first;
    arena_select_second_pick = second;
    arena_select_first_previous_pick = first;
    arena_select_second_previous_pick = second;
    arena_select_confirmed_sides = 3;
    cd_sync_reads(0);
    cell = &arena_select_portrait_slots[first];
    LoadImage(&cell->clut, (u_long *)data);
    LoadImage(&cell->image, (u_long *)(data + 0x100));
    cell = &arena_select_portrait_slots[second];
    LoadImage(&cell->clut, (u_long *)other);
    LoadImage(&cell->image, (u_long *)(data + 0x1100));
    heap_delay_free(data, 2);
}

/* 80080780: Enter the selection screen in a mode: upload every portrait once, set
 * the pages' entry counts and labels, and reset both sides. */
void arena_select_enter(s32 mode) {
    GridCell *cell;
    s32 i;

    if (arena_select_portraits_in_vram == 0) {
        cd_sync_reads(0);
        cell = arena_select_portrait_slots;
        for (i = 0; i < 49; i++, cell++) {
            LoadImage(&cell->clut, (u_long *)(arena_select_portraits + (i << 12)));
            LoadImage(&cell->image, (u_long *)(arena_select_portraits + (i << 12) + 0x100));
        }
        heap_free(arena_select_portraits);
        arena_select_portraits_in_vram = 1;
    }
    arena_play_mode = mode;
    if (mode == 4) {
        arena_menu_pages[5].parent = 3;
    } else {
        arena_menu_pages[5].parent = 4;
    }
    arena_menu_pages[6].parent = 5;
    if (mode == 3) {
        arena_menu_port1_items[0].caption = 0x27;
        arena_menu_port2_items[0].caption = 0x28;
    } else {
        arena_menu_port1_items[0].caption = 0x25;
        arena_menu_port2_items[0].caption = 0x26;
    }
    arena_mode_set_state(1);
    arena_select_confirmed_sides = 0;
    arena_select_second_pick_slide = 0;
    arena_select_second_neighbor_slide = 0;
    arena_select_first_pick_slide = 0;
    arena_select_first_neighbor_slide = 0;
    arena_select_first_previous_pick = arena_select_first_pick;
    arena_select_second_previous_pick = arena_select_second_pick;
    arena_menu_show_page(5);
}

/* 800808F4: Menu line handler: hide the captions and end the menu screen (arena_menu_screen_done). */
void arena_menu_end_screen(void) {
    arena_menu_screen_done = 1;
    arena_menu_hide_captions();
}

/* 80080920: Menu line handler: end the menu screen, restart the opening and give
 * both actor slots their first models again. */
void arena_menu_choose_tutorial(void) {
    arena_menu_screen_done = 1;
    arena_scene_start_tutorial();
    arena_actor_load_model(0, 0);
    arena_actor_load_model(1, 1);
}

/* 80080964: Show a page, remembering the current one; 0xff returns to it. */
void arena_menu_show_page(s32 page) {
    Menu *previous;

    if (page == 0xFF) {
        arena_current_menu = arena_menu_previous;
        return;
    }
    previous = arena_current_menu;
    arena_current_menu = &arena_menu_pages[page];
    arena_menu_previous = previous;
}

/* 800809BC: Whether page 3 is shown. */
s32 arena_menu_is_title_shown(void) {
    return arena_current_menu == &arena_menu_pages[3];
}

/* 800809D8: Enter the settings/system menu at page 3 with every state reset. */
void arena_menu_enter_title(void) {
    sound_stop_all_effects();
    arena_current_menu = NULL;
    arena_menu_show_page(3);
    arena_menu_pages[3].cursor = 0;
    arena_menu_pages[4].cursor = 0;
    arena_play_mode = 0;
    arena_menu_paused = 0;
    arena_menu_hide_captions();
    arena_menu_screen_done = 0;
    arena_menu_free_screen_copy(0);
    arena_select_portraits_in_vram = 0;
    arena_select_portraits = arena_load_whole_file(6);
}

/* 80080A58: Release the loaded portraits unless they were uploaded (once). */
void arena_select_drop_portraits(void) {
    if (arena_select_portraits_in_vram == 0) {
        cd_sync_reads(0);
        heap_free(arena_select_portraits);
        arena_select_portraits_in_vram = 1;
    }
}

/* 80080AA0: Free the loaded image data (or just forget it). */
void arena_menu_free_screen_copy(s32 forget) {
    if (forget) {
        arena_menu_screen_copy = NULL;
    }
    if (arena_menu_screen_copy != NULL) {
        heap_free(arena_menu_screen_copy);
        arena_menu_screen_copy = NULL;
    }
}

/* 80080AE8: Unpack the loaded image data and upload it to VRAM (320,256)-(640,474). */
void arena_menu_blur_screen_copy(void) {
    s16 rect[4];

    if (arena_menu_screen_copy != NULL) {
        DrawSync(0);
        rect[0] = 0x140;
        rect[1] = 0x100;
        rect[2] = 0x140;
        rect[3] = 0xDA;
        arena_box_filter_rgb555(arena_menu_screen_copy, (u8 *)arena_menu_screen_copy + 0x21E80);
        LoadImage((RECT *)rect, arena_menu_screen_copy);
    }
}

/* 80080B58: Keep a copy of the shown screen: allocate the image buffer once, copy
 * the displayed buffer's area to (320,256) and read it back. */
void arena_menu_store_screen_copy(void) {
    RECT area;

    if (arena_menu_screen_copy == NULL) {
        heap_set_quiet_failures(1);
        arena_menu_screen_copy = heap_alloc(0x22100, 0);
        heap_set_quiet_failures(0);
    }
    DrawSync(0);
    area = arena_display_buffers[(arena_draw_buffer_index + 1) & 1].draw.clip;
    MoveImage(&area, 0x140, 0x100);
    if (arena_menu_screen_copy != NULL) {
        StoreImage(&area, arena_menu_screen_copy);
    }
    DrawSync(0);
}

/* 80080C48: Open the system menu: mode 1 at page 0, mode 2 at page 7, else close. */
void arena_menu_open_pause(s32 mode) {
    sound_stop_all_effects();
    arena_sound_play_effect(0x1F);
    if (mode == 1) {
        arena_current_menu = NULL;
        arena_menu_show_page(0);
        arena_menu_pages[0].cursor = 0;
        arena_menu_pages[2].cursor = 1;
    } else if (mode == 2) {
        arena_current_menu = NULL;
        arena_menu_show_page(7);
        arena_menu_pages[7].cursor = 1;
    } else {
        goto close;
    }
    arena_mode_set_state(0);
    arena_menu_paused = 1;
    arena_menu_blur_count = 0;
    arena_menu_blur_pending = 1;
    arena_menu_store_screen_copy();
    return;
close:
    arena_menu_close();
}

/* 80080D10: Drop this frame's text quads. */
void arena_text_drop_quads(void) {
    arena_text_quad_count = 0;
}

/* 80080D20: Link this frame's text quads and the menu overlay: the shown page's box
 * with its texture page and, while a page or the copy request is active, a
 * move of the kept screen copy into the draw buffer. */
void arena_menu_draw_overlay(void *ot) {
    POLY_FT4 *quad = arena_text_quads[arena_draw_buffer_index];
    RECT area;
    s32 i;

    for (i = 0; i < arena_text_quad_count; i++, quad++) {
        AddPrim(ot, quad);
    }
    arena_text_quad_count = 0;
    arena_menu_draw_captions(ot);
    if ((arena_current_menu != NULL && arena_menu_paused != 0) || arena_menu_copy_one_more_frame != 0) {
        if (arena_current_menu != NULL) {
            AddPrim(ot, &arena_current_menu->panel[arena_draw_buffer_index]);
            SetDrawTPage(&arena_menu_panel_tpages[arena_draw_buffer_index], 0, 0, GetTPage(0, 2, 0, 0));
            AddPrim(ot, &arena_menu_panel_tpages[arena_draw_buffer_index]);
        }
        area.x = 0x140;
        area.y = 0x100;
        area.w = 0x140;
        area.h = 0xDA;
        SetDrawMove(&arena_menu_screen_copy_moves[arena_draw_buffer_index], &area, arena_display_buffers[arena_draw_buffer_index].draw.clip.x,
                      arena_display_buffers[arena_draw_buffer_index].draw.clip.y);
        AddPrim(ot, &arena_menu_screen_copy_moves[arena_draw_buffer_index]);
    }
    arena_menu_copy_one_more_frame = 0;
}

/* 80080F04: Set up the two semi-transparent sprite strips (at y 180 and 195) sharing
 * one pixel buffer, and their texture page. */
void arena_menu_init_captions(void) {
    u8 *pixels = heap_alloc(0x6B4, 0);

    arena_menu_captions[0].image = arena_menu_captions[1].image = pixels;
    *(u32 *)&arena_menu_captions[0].sprite[0].x0 = 0xB40000;
    *(u16 *)&arena_menu_captions[0].sprite[0].u0 = 0x3000;
    SetSprt(&arena_menu_captions[0].sprite[0]);
    SetShadeTex(&arena_menu_captions[0].sprite[0], 1);
    arena_menu_captions[0].sprite[0].h = 0xD;
    arena_menu_captions[0].sprite[0].clut = text_plane0_clut;
    arena_menu_captions[0].sprite[1] = arena_menu_captions[0].sprite[0];
    *(u32 *)&arena_menu_captions[1].sprite[0].x0 = 0xC30000;
    *(u16 *)&arena_menu_captions[1].sprite[0].u0 = 0x3000;
    SetSprt(&arena_menu_captions[1].sprite[0]);
    SetShadeTex(&arena_menu_captions[1].sprite[0], 1);
    arena_menu_captions[1].sprite[0].h = 0xD;
    arena_menu_captions[1].sprite[0].clut = text_plane1_clut;
    arena_menu_captions[1].sprite[1] = arena_menu_captions[1].sprite[0];
    SetDrawTPage(&arena_menu_caption_tpages[0], 0, 0, GetTPage(0, 0, 0x140, 0x30));
    arena_menu_caption_tpages[1] = arena_menu_caption_tpages[0];
}

/* 80081094: Render a caption's text into its image and centre it on the screen. */
void arena_menu_render_caption(Caption *caption, s32 text, s32 arg) {
    s32 width;

    width = window_render_text_line(text_get_resource_entry(arena_text_message_table, text), caption->image, 0x3F, arg);
    caption->width = width;
    caption->x = (0x140 - width) / 2;
}

/* 80081100: Show a text in the upper (0) or lower (1) caption; re-render only when
 * the text changes. */
void arena_menu_show_caption(s32 text, s32 lower) {
    RECT rect;

    if (lower == 0) {
        if (text == arena_menu_upper_caption) {
            return;
        }
        arena_menu_upper_caption = text;
        arena_menu_render_caption(&arena_menu_captions[0], text, 0);
    } else {
        if (text == arena_menu_lower_caption) {
            return;
        }
        arena_menu_lower_caption = text;
        arena_menu_render_caption(&arena_menu_captions[1], text, 1);
    }
    rect.x = 0x140;
    rect.y = 0x30;
    rect.w = 0x42;
    rect.h = 0xD;
    LoadImage(&rect, (void *)arena_menu_captions[0].image);
}

/* 800811AC: Link the shown captions into the ordering table. */
void arena_menu_draw_captions(void *ot) {
    Caption *caption;

    if (arena_menu_upper_caption != 0) {
        caption = &arena_menu_captions[0];
        caption->sprite[arena_draw_buffer_index].w = caption->width;
        caption->sprite[arena_draw_buffer_index].x0 = caption->x;
        AddPrim(ot, &caption->sprite[arena_draw_buffer_index]);
    }
    if (arena_menu_lower_caption != 0) {
        caption = &arena_menu_captions[1];
        caption->sprite[arena_draw_buffer_index].w = caption->width;
        caption->sprite[arena_draw_buffer_index].x0 = caption->x;
        AddPrim(ot, &caption->sprite[arena_draw_buffer_index]);
    }
    if (arena_menu_upper_caption | arena_menu_lower_caption) {
        AddPrim(ot, ((u8 (*)[8])arena_menu_caption_tpages)[arena_draw_buffer_index]);
    }
}

/* 800812BC: Measure a menu's lines and size its panel around the widest one. */
void arena_menu_layout_page(Menu *menu) {
    MenuItem *item;
    TILE *panel;
    s32 i;
    s32 widest;

    widest = 0;
    for (i = 0; i < menu->count; i++) {
        item = &menu->items[i];
        item->half_width = arena_text_measure_width(item->text) / 2;
        widest = (widest < item->half_width) ? item->half_width : widest;
    }
    panel = &menu->panel[0];
    setlen(panel, 3);
    panel->w = widest * 2 + 0x14;
    panel->x0 = 0x96 - widest;
    menu->cursor = 0;
    *(u32 *)&panel->r0 = 0x60102020;
    menu->y = 0x6D - menu->count * 10;
    panel->code |= 2;
    panel->h = menu->count * 20 + 0x14;
    panel->y0 = menu->y - 10;
    for (i = 0; i < menu->count; i++) {
        item = &menu->items[i];
        if (item->flags & 2) {
            item->half_width = widest;
        }
    }
    if (menu->title_width != 0) {
        panel->w += menu->title_width;
        panel->x0 -= menu->title_width >> 1;
        menu->x = 0xA0 - (menu->title_width >> 1) - widest;
    }
    menu->panel[1] = menu->panel[0];
    arena_text_set_highlight(0);
}

/* 800814AC: Lay out all eight menus and reset the menu display. */
void arena_menu_init_pages(void) {
    u32 i;

    for (i = 0; i < 8; i++) {
        arena_menu_layout_page(&arena_menu_pages[i]);
    }
    arena_current_menu = NULL;
    arena_select_first_pick = 0;
    arena_select_second_pick = 1;
    arena_menu_init_captions();
}

/* 8008151C: Open a menu: place the cursor and draw every line. */
void arena_menu_draw_page(Menu *menu) {
    s32 i;

    if (menu == NULL) {
        return;
    }
    arena_current_menu = menu;
    if (menu == &arena_menu_pages[5]) {
        return;
    }
    if (menu->title_width != 0) {
        arena_text_move_cursor(menu->x, menu->y);
    } else {
        arena_text_move_cursor(0xA0, menu->y);
    }
    for (i = 0; i < menu->count; i++) {
        arena_menu_highlight_entry(menu, i);
        if (menu->title_width != 0) {
            arena_text_draw_line(menu->items[i].text);
        } else {
            arena_text_draw_line_shifted_left(menu->items[i].text, menu->items[i].half_width);
        }
    }
    if (menu->draw != NULL) {
        menu->draw(menu);
    }
}

/* 8008162C: One frame of menu input from a pad port: caption, stick sound, confirm,
 * cancel and cursor movement (skipping disabled lines, wrapping). Declared
 * with a value it never returns, as the unfilled final delay slot shows. */
s32 arena_menu_apply_pad_input(Menu *menu, s32 port) {
    MenuItem *item;
    void (*handler)();
    s32 type;
    s32 x;
    s32 y;

    item = &menu->items[menu->cursor];
    if (port == 0 || menu == &arena_menu_pages[7]) {
        arena_menu_show_caption(arena_menu_selected_caption, 0);
        arena_menu_selected_caption = menu->items[menu->cursor].caption;
    }
    arena_menu_applied_pad_port = port;
    type = 0;
    if (port == 1) {
        arena_menu_buttons_held = pad_port1_held;
        arena_menu_buttons_repeated = pad_port1_repeated;
        arena_menu_buttons_pressed = pad_port1_pressed;
        type = pad_get_controller_kind(1);
        x = pad_port1_left_stick_y - 0x80;
        y = pad_port1_left_stick_x - 0x80;
    } else if (port == 0) {
        arena_menu_buttons_held = pad_port0_held;
        arena_menu_buttons_repeated = pad_port0_repeated;
        arena_menu_buttons_pressed = pad_port0_pressed;
        type = pad_get_controller_kind(0);
        x = pad_port0_left_stick_y - 0x80;
        y = pad_port0_left_stick_x - 0x80;
    }
    if (type == 3 || type == 4) {
        if (SquareRoot0(x * x + y * y) > 0x40) {
            if (arena_menu_stick_deflected == 0) {
                arena_sound_play_effect(0x24);
                arena_menu_stick_deflected = 1;
            }
            goto stick_done;
        }
    }
    arena_menu_stick_deflected = 0;
stick_done:
    handler = item->handler;
    if (handler != NULL) {
        if (item->flags & 1) {
            handler(item->arg);
        } else if (arena_menu_buttons_pressed & 0x20) {
            arena_sound_play_effect(0x21);
            handler(item->arg);
        }
    }
    if (arena_current_menu != NULL) {
        if ((arena_menu_buttons_pressed & 0x40) && !(port == 1 && menu == &arena_menu_pages[6])) {
            if (arena_current_menu == &arena_menu_pages[menu->parent] && menu->parent != 5 && menu->parent != 6) {
                arena_sound_play_effect(0x24);
            } else {
                arena_menu_show_page(menu->parent);
                arena_sound_play_effect(0x22);
            }
        }
        if (arena_menu_buttons_repeated & 0x1000) {
            arena_sound_play_effect(0x1E);
            if (--menu->cursor < 0) {
                menu->cursor = menu->count - 1;
            }
            if (menu->items[menu->cursor].flags & 4) {
                menu->cursor--;
            }
        }
        if (arena_menu_buttons_repeated & 0x4000) {
            arena_sound_play_effect(0x1E);
            menu->cursor++;
            if (menu->items[menu->cursor].flags & 4) {
                menu->cursor++;
            }
        }
        if (menu->cursor < 0) {
            menu->cursor = menu->count - 1;
        }
        if (menu->cursor >= menu->count) {
            menu->cursor = 0;
        }
        if (menu->items[menu->cursor].flags & 4) {
            menu->cursor--;
        }
    }
}

/* The lines of the menus; their texts are the literals of each table, emitted
 * last to first after the code before it ("" is arena_menu_draw_practice_values's). */

/* Vibration choices per port; the selected one's caption is set at run
 * time. */
MenuItem arena_menu_port1_items[2] = { /* 80091368 */
    { 1, 0x25, { 0 }, (s32)"", arena_select_step_first_pick },
    { 1, 0x29, { 0 }, (s32)"", arena_menu_toggle_port1_vibration },
};
MenuItem arena_menu_port2_items[2] = { /* 80091390 */
    { 1, 0x26, { 0 }, (s32)"", arena_select_step_second_pick },
    { 1, 0x29, { 0 }, (s32)"", arena_menu_toggle_port2_vibration },
};
MenuItem arena_menu_title_items[] = { /* 800913B8 */
    { 0, 1, { 0 }, (s32)"BONUS BATTLING", arena_menu_show_page, 4 },
    { 4, 0, { 0 }, (s32)"" },
    { 0, 2, { 0 }, (s32)"PRACTICE", arena_select_enter, 4 },
    { 0, 3, { 0 }, (s32)"TUTORIAL", arena_menu_choose_tutorial },
    { 0, 5, { 0 }, (s32)"EXIT", (void (*)(s32))arena_mode_exit },
};
MenuItem arena_menu_bonus_battling_items[] = { /* 8009141C */
    { 0, 7, { 0 }, (s32)"PLAYER1 VS COM", arena_select_enter, 1 },
    { 0, 8, { 0 }, (s32)"PLAYER1 VS PLAYER2", arena_select_enter, 2 },
    { 0, 9, { 0 }, (s32)"COM VS COM", arena_select_enter, 3 },
    { 4, 0, { 0 }, (s32)"" },
    { 3, 0xA, { 0 }, (s32)"NUM OF MATCHES", arena_menu_step_match_count },
    { 3, 0xB, { 0 }, (s32)"COM LEVEL", arena_menu_step_level },
    { 3, 0xC, { 0 }, (s32)"RUBBER BAND", arena_menu_toggle_rubber_band },
};
MenuItem arena_menu_pause_items[] = { /* 800914A8 */
    { 0, 0xD, { 0 }, (s32)"CONTINUE BOUT", arena_menu_resume_bout },
    { 0, 0xE, { 0 }, (s32)"GIVE UP", arena_menu_show_page, 2 },
};
MenuItem arena_menu_settings_items[] = { /* 800914D0 */
    { 1, 0xB, { 0 }, (s32)"GAME LEVEL", arena_menu_step_level },
    { 1, 0x13, { 0 }, (s32)"MOTION SPEED", arena_menu_step_motion_speed },
    { 1, 0x14, { 0 }, (s32)"FRAME RATE", arena_menu_step_frame_rate },
    { 1, 0x23, { 0 }, (s32)"GEAR 1", arena_menu_toggle_first_actor_com },
    { 1, 0x24, { 0 }, (s32)"GEAR 2", arena_menu_toggle_second_actor_com },
};
MenuItem arena_menu_give_up_items[] = { /* 80091534 */
    { 0, 0x2A, { 0 }, (s32)"YES", arena_menu_give_up },
    { 0, 0x2A, { 0 }, (s32)"NO", arena_menu_show_page },
};
MenuItem arena_menu_practice_items[] = { /* 8009155C */
    { 0, 0x10, { 0 }, (s32)"RETURN TO PRACTICE", arena_menu_resume_bout },
    { 1, 0x12, { 0 }, (s32)"AI", arena_menu_step_ai_command },
    { 1, 0x14, { 0 }, (s32)"FRAME RATE", arena_menu_step_frame_rate },
    { 0, 0x11, { 0 }, (s32)"EXIT PRACTICE MODE", arena_menu_give_up },
};

/* The menus: lines, line count, menu returned to on cancel, extra drawing. */
Menu arena_menu_pages[8] = { /* 800915AC */
    { 0, { 0 }, arena_menu_pause_items, 2, 0 },
    { 0x20, { 0 }, arena_menu_settings_items, 5, 0, arena_menu_draw_settings_values },
    { 0, { 0 }, arena_menu_give_up_items, 2, 0 },
    { 0, { 0 }, arena_menu_title_items, 5, 3 },
    { 0, { 0 }, arena_menu_bonus_battling_items, 7, 3, arena_menu_draw_bonus_battling_values },
    { 0, { 0 }, arena_menu_port1_items, 2, 3 },
    { 0, { 0 }, arena_menu_port2_items, 2, 3 },
    { 1, { 0 }, arena_menu_practice_items, 4, 7, arena_menu_draw_practice_values },
};

/* Unreferenced. */
const char arena_unused_empty_string[] = ""; /* 80070198 */

/* 80081A44: The controller menu pair (menus 5 and 6) for the current mode: run the
 * menu of the available port (both in mode 2, where an unavailable port's
 * menu returns to menu 5 instead of 4), note when neither port is
 * available, set which sides the pads drive, then draw. Mode 5 runs menu
 * 5 for port 1 without resetting arena_select_second_side_picking and steps the second side's
 * pick for port 2. */
void arena_menu_update_port_pages(void) {
    switch (arena_play_mode) {
    case 1:
        switch ((s32)arena_select_confirmed_sides) {
        case 0:
            arena_select_second_side_picking = 0;
            arena_menu_apply_pad_input(&arena_menu_pages[5], 0);
            break;
        case 1:
            arena_select_second_side_picking = 1;
            arena_menu_apply_pad_input(&arena_menu_pages[6], 0);
            break;
        case 3:
            arena_menu_screen_done = 1;
            break;
        }
        arena_settings.com1 = 0;
        arena_settings.driven = 1;
        break;
    case 2:
        arena_select_second_side_picking = 1;
        if (arena_select_confirmed_sides & 1) {
            arena_menu_pages[5].parent = 5;
        } else {
            arena_menu_pages[5].parent = 4;
        }
        if (arena_select_confirmed_sides & 2) {
            arena_menu_pages[6].parent = 5;
        } else {
            arena_menu_pages[6].parent = 4;
        }
        arena_menu_apply_pad_input(&arena_menu_pages[5], 0);
        arena_menu_apply_pad_input(&arena_menu_pages[6], 1);
        if (arena_select_confirmed_sides == 3) {
            arena_menu_screen_done = 1;
        }
        arena_settings.com1 = 0;
        arena_settings.driven = 0;
        break;
    case 3:
        switch ((s32)arena_select_confirmed_sides) {
        case 0:
            arena_select_second_side_picking = 0;
            arena_menu_apply_pad_input(&arena_menu_pages[5], 0);
            break;
        case 1:
            arena_select_second_side_picking = 1;
            arena_menu_apply_pad_input(&arena_menu_pages[6], 0);
            break;
        case 3:
            arena_menu_screen_done = 1;
            break;
        }
        arena_settings.com1 = 1;
        arena_settings.driven = 1;
        break;
    case 4:
        switch ((s32)arena_select_confirmed_sides) {
        case 0:
            arena_select_second_side_picking = 0;
            arena_menu_apply_pad_input(&arena_menu_pages[5], 0);
            break;
        case 1:
            arena_select_second_side_picking = 1;
            arena_menu_apply_pad_input(&arena_menu_pages[6], 0);
            break;
        case 3:
            arena_menu_screen_done = 1;
            break;
        }
        arena_settings.com1 = 0;
        arena_settings.driven = 1;
        break;
    case 5:
        switch ((s32)arena_select_confirmed_sides) {
        case 0:
            arena_menu_apply_pad_input(&arena_menu_pages[5], 0);
            break;
        case 1:
            if (arena_select_second_pick == 3) {
                arena_select_confirmed_sides = 3;
            } else {
                arena_select_second_pick++;
                if (arena_select_second_pick > arena_select_entry_count) {
                    arena_select_second_pick -= arena_select_entry_count;
                }
                if (arena_select_second_pick < 0) {
                    arena_select_second_pick += arena_select_entry_count;
                }
            }
            break;
        case 3:
            arena_menu_screen_done = 1;
            break;
        }
        arena_settings.com1 = 0;
        arena_settings.driven = 1;
        break;
    }
    arena_menu_draw_port_pages();
}

/* 80081D2C: One frame of the menu layer: pending refresh, the shown menu's input
 * (with the extra-speed button) and its drawing. */
void arena_menu_update(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    Menu *menu;

    if (arena_menu_blur_pending != 0) {
        arena_menu_blur_screen_copy();
        arena_menu_blur_pending = 0;
    }
    pad_merge_queued_states();
    menu = arena_current_menu;
    if (menu == &arena_menu_pages[5]) {
        arena_menu_update_port_pages();
        return;
    }
    if (menu != NULL) {
        if ((pad_port0_pressed & 1) && arena_menu_blur_count < 5) {
            arena_menu_blur_count++;
            arena_menu_blur_screen_copy();
        }
        arena_menu_apply_pad_input(menu, arena_menu_driving_pad_port);
    }
    arena_menu_draw_page(arena_current_menu);
}

/* 80081E00: Dim the screen below the top band with the half-grey fade tiles. */
void arena_menu_dim_background(void) {
    arena_display_buffers[0].background.y0 = 0x60;
    arena_display_buffers[1].background.y0 = 0x60;
    arena_display_buffers[0].background.r0 = 0x7F;
    arena_display_buffers[0].background.g0 = 0x7F;
    arena_display_buffers[0].background.b0 = 0x7F;
    arena_display_buffers[1].background.r0 = 0x7F;
    arena_display_buffers[1].background.g0 = 0x7F;
    arena_display_buffers[1].background.b0 = 0x7F;
    arena_display_buffers[0].background.h = arena_display_height - 0x60;
    arena_display_buffers[1].background.h = arena_display_height - 0x60;
}

/* 80081E6C: Clear the fade tiles back to the full, black screen. */
void arena_menu_reset_background(void) {
    arena_display_buffers[0].background.y0 = 0;
    arena_display_buffers[1].background.y0 = 0;
    arena_display_buffers[0].background.r0 = 0;
    arena_display_buffers[0].background.g0 = 0;
    arena_display_buffers[0].background.b0 = 0;
    arena_display_buffers[1].background.r0 = 0;
    arena_display_buffers[1].background.g0 = 0;
    arena_display_buffers[1].background.b0 = 0;
    arena_display_buffers[0].background.h = arena_display_height;
    arena_display_buffers[1].background.h = arena_display_height;
}
