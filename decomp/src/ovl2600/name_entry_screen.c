/* Overlay 2600 (Disc 1 slot 2600, Disc 2 slot 2595; loaded at 0x801c5000):
 * the character name entry screen. A character grid (name_entry_grid_codes) is walked
 * with the cursor, the name is built as text codes and decoded for display,
 * and the result is stored in the game data's name table (names[id]). The
 * three party portraits are loaded for the screen. Much of the drawing and
 * list code is the same as overlay 2598's (the party screen), compiled into
 * this image. The image is this one unit: rodata 801c5000-801c5040, text to
 * 801cbea0 and data to the file's end at 801cc134; no jump table phase
 * change or second data block marks another. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "menu/card.h"
#include "menu/panel.h"
#include "menu/screen.h"
#include "name_entry.h"

/* The four cursor markers' home positions. */
s32 name_entry_marker_x_table[4] = {0, 0, 180, 276}; /* 801CBEA0: x */
s32 name_entry_marker_y_table[4] = {0, 0, 200, 200}; /* 801CBEB0: y */

/* The name entry grid: 36 entries of six text codes (five shown, then 0x0F
 * or 0xFF). */
u8 name_entry_grid_codes[216] = { /* 801CBEC0 */
    0x20, 0x21, 0x22, 0x23, 0x24, 0x0F, 0x34, 0x35, 0x36, 0x37, 0x38, 0x0F,
    0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x0F, 0x51, 0x52, 0x53, 0x54, 0x55, 0x0F,
    0x3A, 0x3B, 0x3C, 0x61, 0x62, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0x0F, 0x25, 0x26, 0x27, 0x28, 0x29, 0x0F,
    0x39, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x42, 0x43, 0x44, 0x45, 0x46, 0x0F,
    0x56, 0x0F, 0xCF, 0xCF, 0xCF, 0xCF, 0x63, 0x64, 0x65, 0xCF, 0xCF, 0xCF,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0xCF,
    0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x0F,
    0x47, 0x48, 0x49, 0x4A, 0x4B, 0x0F, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x0F,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
    0xCF, 0xCF, 0xCF, 0xCF, 0xCF, 0x0F, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF,
    0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0xFF,
    0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0xFF, 0xCF, 0xCF, 0xCF, 0xCF, 0x1F, 0xFF,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF,
    0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xC0, 0xCF, 0xCF, 0xCF, 0x1F, 0xFF,
};

/* The four command labels' text ids. */
u8 name_entry_command_label_ids[4] = {9, 10, 11, 12}; /* 801CBF98 */

/* File cursor -> slot (slots 15 and 31 are skipped). */
s32 name_entry_file_cursor_card_slots[30] = { /* 801CBF9C */
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};
/* Slot positions. */
s32 name_entry_file_slot_x_table[32] = { /* 801CC014: x */
    32, 40, 48, 56, 64, 72, 80, 88,
    96, 104, 112, 120, 128, 136, 144, 320,
    176, 184, 192, 200, 208, 216, 224, 232,
    240, 248, 256, 264, 272, 280, 288, 320,
};
s32 name_entry_file_slot_y_table[32] = { /* 801CC094: y */
    14, 34, 54, 14, 34, 54, 14, 34,
    54, 14, 34, 54, 14, 34, 54, 256,
    14, 34, 54, 14, 34, 54, 14, 34,
    54, 14, 34, 54, 14, 34, 54, 256,
};

/* Each character's bit in the party flags. */
u16 name_entry_bit_masks[16] = { /* 801CC114 */
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
};

/* 801C5040: Test character `index`'s bit (table name_entry_bit_masks) in `flags`. */
s32 name_entry_test_bit(s32 flags, u8 index) {
    return name_entry_bit_masks[index] & flags;
}

/* 801C505C: Allocate (nonzero) or release the 0x5034-byte work block. */
void name_entry_alloc_or_free_card_state(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x5034, 0);
        menu_state_current->card = block;
        bzero(block, 0x5034);
    } else {
        heap_free(menu_state_current->card);
    }
}

/* 801C50C0: Allocate (nonzero) or release the menu flag block. */
void name_entry_alloc_or_free_flags(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x6C, 0);
        menu_state_current->flags = block;
        bzero(block, 0x6C);
    } else {
        heap_free(menu_state_current->flags);
    }
}

/* 801C5124: Allocate (nonzero) or release the 0x1194-byte block at state + 0x350. */
void name_entry_alloc_or_free_screen_images(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x1194, 0);
        menu_state_current->images = block;
        bzero(block, 0x1194);
    } else {
        heap_free(menu_state_current->images);
    }
}

/* 801C5188: Allocate (nonzero) or release the 0x140C-byte block at state + 0x354. */
void name_entry_alloc_or_free_sprite_lists(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x140C, 0);
        menu_state_current->lists = block;
        bzero(block, 0x140C);
    } else {
        heap_free(menu_state_current->lists);
    }
}

/* 801C51EC: Allocate (nonzero) or release the 0xCC-byte block at state + 0x330. */
void name_entry_alloc_or_free_table_directory(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0xCC, 0);
        menu_state_current->tables = block;
        bzero(block, 0xCC);
    } else {
        heap_free(menu_state_current->tables);
    }
}

/* 801C5250: Allocate (nonzero) or release the 0x15C-byte block at state + 0x348. */
void name_entry_alloc_or_free_prims(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x15C, 0);
        menu_state_current->prims = block;
        bzero(block, 0x15C);
    } else {
        heap_free(menu_state_current->prims);
    }
}

/* 801C52B4: Allocate (nonzero) or release the name entry block. */
void name_entry_alloc_or_free_name_entry_block(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0xDEC, 0);
        menu_state_current->name_entry = block;
        bzero(block, 0xDEC);
    } else {
        heap_free(menu_state_current->name_entry);
    }
}

/* 801C5318: Load the screen's resources: the card icon TIM and file name into the work
 * block's save header, the palette data, sprite sheet and label texts, the
 * named character's entry length and three portraits (uploaded to the
 * portrait sprites' VRAM), and the menu sound bank when sound is on. */
void name_entry_load_resources(void) {
    enum {
        ENTRY_UNUSED, ENTRY_MODE, ENTRY_CLUT_X, ENTRY_CLUT_Y,
        ENTRY_PAGE_X, ENTRY_PAGE_Y, ENTRY_WORDS
    };
    TIM_IMAGE tim;
    s32 entries[3 * ENTRY_WORDS];
    MenuResources *archive = menu_state_resource_file;
    u8 *data;
    u8 use_table = 1;
    s32 i;
    u8 id;

    text_relocate_offset_table(archive);
    data = text_unpack_lzss_alloc(archive->files[0], 1);
    OpenTIM((u_long *)data);
    ReadTIM(&menu_state_current->card->icon);
    strcpy(menu_state_current->card->prefix, "BISLPS-00800");
    menu_state_current->card->save_magic[0] = 'S';
    menu_state_current->card->save_magic[1] = 'C';
    menu_state_current->card->save_icon_flag = 0x11;
    menu_state_current->card->save_blocks = 1;
    bzero(menu_state_current->card->save_title, 0x5C);
    memmove(menu_state_current->card->save_palette, menu_state_current->card->icon.caddr, 0x20);
    memmove(menu_state_current->card->save_icon, menu_state_current->card->icon.paddr, 0x80);
    heap_free(data);
    data = text_unpack_lzss_alloc(archive->files[1], 1);
    model_load_tim_list((u32 *)data);
    heap_free(data);
    menu_state_current->sheet = text_unpack_lzss_alloc(archive->files[2], 0);
    menu_state_current->label_text = text_unpack_lzss_alloc(archive->files[3], 0);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x14B,
                  &entries[ENTRY_UNUSED], &entries[ENTRY_MODE],
                  &entries[ENTRY_CLUT_X], &entries[ENTRY_CLUT_Y],
                  &entries[ENTRY_PAGE_X], &entries[ENTRY_PAGE_Y]);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x14C,
                  &entries[ENTRY_WORDS + ENTRY_UNUSED], &entries[ENTRY_WORDS + ENTRY_MODE],
                  &entries[ENTRY_WORDS + ENTRY_CLUT_X], &entries[ENTRY_WORDS + ENTRY_CLUT_Y],
                  &entries[ENTRY_WORDS + ENTRY_PAGE_X], &entries[ENTRY_WORDS + ENTRY_PAGE_Y]);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x14D,
                  &entries[2 * ENTRY_WORDS + ENTRY_UNUSED], &entries[2 * ENTRY_WORDS + ENTRY_MODE],
                  &entries[2 * ENTRY_WORDS + ENTRY_CLUT_X], &entries[2 * ENTRY_WORDS + ENTRY_CLUT_Y],
                  &entries[2 * ENTRY_WORDS + ENTRY_PAGE_X], &entries[2 * ENTRY_WORDS + ENTRY_PAGE_Y]);
    menu_state_current->portrait_table = text_unpack_lzss_alloc(archive->files[5], 1);
    if (menu_state_screen_parameter < 11) {
        menu_state_current->name_entry->max_length = 9;
        menu_state_current->flags->party[0] = menu_state_screen_parameter;
    } else {
        menu_state_current->name_entry->max_length = 10;
        if (menu_state_screen_parameter - 11 == 9 && (game_data.joined & 0x400)) {
            menu_state_current->flags->party[0] = 10;
            use_table = 0;
        }
        if (use_table) {
            menu_state_current->flags->party[0] = menu_state_current->portrait_table[menu_state_screen_parameter - 11];
        }
    }
    menu_state_current->portraits[0] = menu_state_current->portrait_table[menu_state_screen_parameter * 3 + 0x20];
    menu_state_current->portraits[1] = menu_state_current->portrait_table[menu_state_screen_parameter * 3 + 0x21];
    menu_state_current->portraits[2] = menu_state_current->portrait_table[menu_state_screen_parameter * 3 + 0x22];
    i = 0;
    heap_free(menu_state_current->portrait_table);
    entries[ENTRY_WORDS + ENTRY_PAGE_X] += 12;
    data = text_unpack_lzss_alloc(archive->files[4], 1);
    for (; i < 3; i++) {
        id = menu_state_current->flags->party[i];
        if (id != 0xFF) {
            OpenTIM((u_long *)(data + id * 0xB20));
            ReadTIM(&tim);
            tim.crect->x = entries[i * ENTRY_WORDS + ENTRY_CLUT_X];
            tim.crect->y = entries[i * ENTRY_WORDS + ENTRY_CLUT_Y];
            tim.prect->x = entries[i * ENTRY_WORDS + ENTRY_PAGE_X];
            tim.prect->y = entries[i * ENTRY_WORDS + ENTRY_PAGE_Y];
            LoadImage(tim.crect, tim.caddr);
            LoadImage(tim.prect, tim.paddr);
        }
    }
    DrawSync(0);
    heap_free(data);
    if (menu_state_debug_start) {
        cd_select_directory(0x10, 2);
        sound_effect_bank = heap_alloc(cd_get_aligned_file_size(5), 0);
        cd_read_file(5, sound_effect_bank, 0, 0x80);
        cd_sync_reads(0);
        cd_select_directory(0x10, 0);
        sound_add_effect_bank(sound_effect_bank);
    }
    menu_state_current->effects = sound_effect_bank;
    heap_free(archive);
}

/* 801C58B8: Reset the screen state, mark which characters may join, take the current
 * party (members that may not join become empty) and load the resources. */
void name_entry_init_party(void) {
    s32 i;
    u16 flags;
    s32 id;

    menu_state_current->cursor = 4;
    menu_state_current->cursor_shown = 0xFF;
    menu_state_current->card_poll_timer = 60;
    menu_state_current->cards_present = 0;
    menu_state_current->unknown335 = 0;
    flags = game_data.joined & game_data.available & 0x7FF;
    for (i = 0; i < 16; i++) {
        if (name_entry_test_bit(flags, i) & 0xFFFF) {
            menu_state_current->present[i] = 1;
        } else {
            menu_state_current->present[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = game_data.party[i];
        if (id != 0xFF && menu_state_current->present[id]) {
            menu_state_current->flags->party[i] = id;
        } else {
            menu_state_current->flags->party[i] = 0xFF;
        }
    }
    name_entry_load_resources();
}

/* 801C5A30: Start building draw buffer 0. */
void name_entry_reset_buffer_index(void) {
    menu_state_current->buffer_index = 0;
}

/* 801C5A40: Upload the text CLUT: 16 black entries except white entry 1 at (0, 0x1C0). */
void name_entry_upload_label_palette(void) {
    RECT rect;
    RECT unused; /* unused in the original; reserves 8 bytes */
    u16 *clut = heap_alloc(0x20, 0);

    bzero((u_char *)clut, 0x20);
    clut[1] = 0x7FFF;
    rect.x = 0;
    rect.y = 0x1C0;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, (u_long *)clut);
    DrawSync(0);
    heap_free(clut);
}

/* 801C5ABC: Set up label `index`'s two quads: mode 0 maps the text rendered for the
 * command column at row + index; otherwise the list layout, dimmed unless
 * bit 7 is set, with the highlight from the low bits. Old-style definition:
 * mode arrives as a promoted int and is narrowed where it is tested. */
void name_entry_label_init_quads(label, index, row, mode)
    MenuLabel *label;
    s32 index;
    s32 row;
    u8 mode;
{
    POLY_FT4 *poly;
    s32 i;
    u8 dim;

    for (i = 0; i < 2; i++) {
        poly = &label->polys[i];
        dim = 0;
        SetPolyFT4(poly);
        SetSemiTrans(poly, 0);
        SetShadeTex(poly, 0);
        poly->r0 = 0x80;
        poly->g0 = 0x80;
        poly->b0 = 0x80;
        if (mode == 0) {
            label->highlight = index & 1;
            poly->tpage = GetTPage(0, 0, 0x140, 0);
            poly->u0 = ((index / 2) & 1) << 7;
            poly->v0 = ((index + row) / 4) * 13;
            poly->u1 = (((index / 2) & 1) << 7) + label->width;
            poly->v1 = ((index + row) / 4) * 13;
            poly->u2 = ((index / 2) & 1) << 7;
            poly->v2 = ((index + row) / 4) * 13 + 13;
            poly->u3 = (((index / 2) & 1) << 7) + label->width;
            poly->v3 = ((index + row) / 4) * 13 + 13;
        } else {
            if (!(mode & 0x80)) {
                dim = 0x20;
                SetSemiTrans(poly, 1);
                poly->r0 = dim;
                poly->g0 = dim;
                poly->b0 = dim;
            }
            label->highlight = (mode & 0x7F) - 1;
            poly->tpage = GetTPage(0, 0, 0x180, 0x80) | dim;
            poly->u0 = (index & 1) * 0x60;
            poly->v0 = (index / 2) * 13 + row;
            poly->u1 = (index & 1) * 0x60 + label->width;
            poly->v1 = (index / 2) * 13 + row;
            poly->u2 = (index & 1) * 0x60;
            poly->v2 = (index / 2) * 13 + row + 13;
            poly->u3 = (index & 1) * 0x60 + label->width;
            poly->v3 = (index / 2) * 13 + row + 13;
        }
        label->polys[i].clut = label->highlight ? text_plane1_clut : text_plane0_clut;
    }
    label->projected = 0;
}

/* 801C5CFC: Render `count` label texts (pairs of text ids) into VRAM, two per line,
 * and set up their quads. */
void name_entry_label_render_pairs(MenuLabel *labels, u8 *text_ids, s32 row, s32 count) {
    s32 i;
    RECT *rect;

    for (i = 0; i < count; i += 2) {
        labels[i].width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, text_ids[i]),
                                        menu_state_current->labels[0].pixels, 0x18, 0);
        rect = &labels[i].rect;
        labels[i + 1].width =
            window_render_text_line(text_get_resource_entry(menu_state_current->label_text, text_ids[i + 1]),
                          menu_state_current->labels[0].pixels, 0x18, 1);
        labels[i].rect.x = (((i / 2) & 1) << 5) + 0x140;
        labels[i].rect.y = ((i + row) / 4) * 13;
        labels[i].rect.w = 0x1C;
        labels[i].rect.h = 13;
        labels[i + 1].rect = labels[i].rect;
        name_entry_label_init_quads(&labels[i], i, row, 0);
        name_entry_label_init_quads(&labels[i + 1], i + 1, row, 0);
        LoadImage(rect, (u_long *)menu_state_current->labels[0].pixels);
        DrawSync(0);
    }
}

/* 801C5EAC: Set up the four command labels and the text CLUT. */
void name_entry_init_labels(void) {
    text_load_palette(0, 0x1D1);
    menu_state_current->labels[0].pixels = heap_alloc(0x38E, 0);
    name_entry_label_render_pairs(menu_state_current->labels, name_entry_command_label_ids, 0, 4);
    name_entry_upload_label_palette();
}

/* 801C5F08: Look up the four cursor/frame sprites of the sheet. */
void name_entry_read_sheet_entries(void) {
    s32 unused[10]; /* unused in the original; reserves 40 bytes */
    sprite_sheet_get_texture(menu_state_current->sheet, 0xFE, &menu_state_current->sheet_entries[0].first,
                  &menu_state_current->sheet_entries[0].mode, &menu_state_current->sheet_entries[0].clut_x,
                  &menu_state_current->sheet_entries[0].clut_y, &menu_state_current->sheet_entries[0].page_x,
                  &menu_state_current->sheet_entries[0].page_y);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x103, &menu_state_current->sheet_entries[1].first,
                  &menu_state_current->sheet_entries[1].mode, &menu_state_current->sheet_entries[1].clut_x,
                  &menu_state_current->sheet_entries[1].clut_y, &menu_state_current->sheet_entries[1].page_x,
                  &menu_state_current->sheet_entries[1].page_y);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x100, &menu_state_current->sheet_entries[2].first,
                  &menu_state_current->sheet_entries[2].mode, &menu_state_current->sheet_entries[2].clut_x,
                  &menu_state_current->sheet_entries[2].clut_y, &menu_state_current->sheet_entries[2].page_x,
                  &menu_state_current->sheet_entries[2].page_y);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x101, &menu_state_current->sheet_entries[3].first,
                  &menu_state_current->sheet_entries[3].mode, &menu_state_current->sheet_entries[3].clut_x,
                  &menu_state_current->sheet_entries[3].clut_y, &menu_state_current->sheet_entries[3].page_x,
                  &menu_state_current->sheet_entries[3].page_y);
}

/* 801C6010: Clear the party list's flags 3 and 4. */
void name_entry_highlight_hide(void) {
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
}

/* 801C6040: Make `poly` a gouraud quad fading from (r, g, b) at the top to black. */
void name_entry_init_gradient_quad(POLY_G4 *poly, u8 r, u8 g, u8 b) {
    SetPolyG4(poly);
    poly->r0 = r;
    poly->g0 = g;
    poly->b0 = b;
    poly->r1 = r;
    poly->g1 = g;
    poly->b1 = b;
    poly->r2 = 0;
    poly->g2 = 0;
    poly->b2 = 0;
    poly->r3 = 0;
    poly->g3 = 0;
    poly->b3 = 0;
}

/* 801C60BC: Set up the backdrop primitives of both draw buffers: the gradient, the
 * full-screen fade quad, the two green frame lines and the draw modes. */
void name_entry_init_highlight_and_fade_prims(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    name_entry_highlight_hide();
    for (i = 0; i < 2; i++) {
        name_entry_init_gradient_quad(&menu_state_current->prims->shade[i], 0x80, 0x80, 0);
        SetSemiTrans(&menu_state_current->prims->shade[i], 1);
        SetLineF3(&menu_state_current->prims->upper[i]);
        (menu_state_current->prims->upper + i)->r0 = 0;
        (menu_state_current->prims->upper + i)->g0 = 0x40;
        (menu_state_current->prims->upper + i)->b0 = 0;
        SetLineF3(&menu_state_current->prims->lower[i]);
        (menu_state_current->prims->lower + i)->r0 = 0;
        (menu_state_current->prims->lower + i)->g0 = 0x40;
        (menu_state_current->prims->lower + i)->b0 = 0;
        SetPolyF4(&menu_state_current->prims->fade[i]);
        (menu_state_current->prims->fade + i)->x0 = 0;
        (menu_state_current->prims->fade + i)->y0 = 0;
        (menu_state_current->prims->fade + i)->x1 = 0x140;
        (menu_state_current->prims->fade + i)->y1 = 0;
        (menu_state_current->prims->fade + i)->x2 = 0;
        (menu_state_current->prims->fade + i)->y2 = 0xE0;
        (menu_state_current->prims->fade + i)->x3 = 0x140;
        (menu_state_current->prims->fade + i)->y3 = 0xE0;
        (menu_state_current->prims->fade + i)->r0 = 0x80;
        (menu_state_current->prims->fade + i)->g0 = 0x80;
        (menu_state_current->prims->fade + i)->b0 = 0x80;
        SetSemiTrans(&menu_state_current->prims->fade[i], 1);
        SetDrawMode(&menu_state_current->prims->mode_label[i], 0, 0, GetTPage(0, 0, 0x140, 0x80),
                      &window);
        SetDrawMode(&menu_state_current->prims->mode_sprite[i], 0, 0, GetTPage(0, 2, 0x180, 0),
                      &window);
    }
}

/* 801C6408: Place a quad's four vertices around the screen centre (160, 112). */
void name_entry_set_rect_verts(SVECTOR *v, u16 x, u16 y, u16 w, u16 h) {
    v[0].vx = x - 160;
    v[0].vy = y - 112;
    v[0].vz = 0;
    v[1].vx = x + w - 160;
    v[1].vy = y - 112;
    v[1].vz = 0;
    v[2].vx = x - 160;
    v[2].vz = 0;
    v[3].vx = x + w - 160;
    v[3].vz = 0;
    v[2].vy = y + h - 112;
    v[3].vy = y + h - 112;
}

/* 801C6460: Make `poly` semi-transparent and untinted. */
void name_entry_quad_set_semi_transparent(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* 801C64A8: Set up panel `index`'s primitives: its translucent grey fill and draw
 * modes, and the textured edge strips from the four frame sprites. */
void name_entry_panel_init(u8 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    RECT window;
    u8 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    menu_state_current->flags->panels_shown[index] = 0;
    menu_state_current->flags->panels_growing[index] = 0;
    for (i = 0; i < 2; i++) {
        SetPolyG4(&panel->fill[i]);
        (panel->fill + i)->r0 = 0x68;
        (panel->fill + i)->g0 = 0x68;
        (panel->fill + i)->b0 = 0x68;
        (panel->fill + i)->r1 = 0x68;
        (panel->fill + i)->g1 = 0x68;
        (panel->fill + i)->b1 = 0x68;
        (panel->fill + i)->r2 = 0x68;
        (panel->fill + i)->g2 = 0x68;
        (panel->fill + i)->b2 = 0x68;
        (panel->fill + i)->r3 = 0x68;
        (panel->fill + i)->g3 = 0x68;
        (panel->fill + i)->b3 = 0x68;
        SetSemiTrans(&panel->fill[i], 1);
        SetDrawMode(&panel->fill_mode[i], 0, 0,
                      GetTPage(0, 0, menu_state_current->sheet_entries[0].page_x,
                                    menu_state_current->sheet_entries[0].page_y),
                      &window);
    }
    for (i = 0; i < 4; i++) {
        SetPolyFT4(&panel->edge[0][i]);
        SetShadeTex(&panel->edge[0][i], 1);
        (panel->edge[0] + i)->r0 = 0xFF;
        (panel->edge[0] + i)->g0 = 0xFF;
        (panel->edge[0] + i)->b0 = 0xFF;
        (panel->edge[0] + i)->tpage =
            GetTPage(menu_state_current->sheet_entries[0].mode, 0, menu_state_current->sheet_entries[0].page_x,
                          menu_state_current->sheet_entries[0].page_y);
        (panel->edge[0] + i)->clut =
            GetClut(menu_state_current->sheet_entries[0].clut_x, menu_state_current->sheet_entries[0].clut_y);
        SetPolyFT4(&panel->edge[1][i]);
        SetShadeTex(&panel->edge[1][i], 1);
        (panel->edge[1] + i)->r0 = 0xFF;
        (panel->edge[1] + i)->g0 = 0xFF;
        (panel->edge[1] + i)->b0 = 0xFF;
        (panel->edge[1] + i)->tpage =
            GetTPage(menu_state_current->sheet_entries[1].mode, 0, menu_state_current->sheet_entries[1].page_x,
                          menu_state_current->sheet_entries[1].page_y);
        (panel->edge[1] + i)->clut =
            GetClut(menu_state_current->sheet_entries[1].clut_x, menu_state_current->sheet_entries[1].clut_y);
        SetPolyFT4(&panel->edge[2][i]);
        SetShadeTex(&panel->edge[2][i], 1);
        (panel->edge[2] + i)->r0 = 0xFF;
        (panel->edge[2] + i)->g0 = 0xFF;
        (panel->edge[2] + i)->b0 = 0xFF;
        (panel->edge[2] + i)->tpage =
            GetTPage(menu_state_current->sheet_entries[2].mode, 0, menu_state_current->sheet_entries[2].page_x,
                          menu_state_current->sheet_entries[2].page_y);
        (panel->edge[2] + i)->clut =
            GetClut(menu_state_current->sheet_entries[2].clut_x, menu_state_current->sheet_entries[2].clut_y);
        SetPolyFT4(&panel->edge[3][i]);
        SetShadeTex(&panel->edge[3][i], 1);
        (panel->edge[3] + i)->r0 = 0xFF;
        (panel->edge[3] + i)->g0 = 0xFF;
        (panel->edge[3] + i)->b0 = 0xFF;
        (panel->edge[3] + i)->tpage =
            GetTPage(menu_state_current->sheet_entries[3].mode, 0, menu_state_current->sheet_entries[3].page_x,
                          menu_state_current->sheet_entries[3].page_y);
        (panel->edge[3] + i)->clut =
            GetClut(menu_state_current->sheet_entries[3].clut_x, menu_state_current->sheet_entries[3].clut_y);
    }
}

/* 801C67C4: Build panel `index`'s frame sprites for this buffer at (x, y) with height
 * `h` and place its top, bottom and side vectors. */
void name_entry_panel_layout_scroll_bar(u8 index, u16 x, u16 y, s32 unused, u16 h) {
    MenuPanel *panel = menu_state_current->panels[index];

    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x105, panel->bar_ends, menu_state_current->buffer_index,
                  x, y, 0x1000);
    sprite_sheet_draw_scaled_flip(menu_state_current->sheet, 0x105, &panel->bar_ends[2],
                  menu_state_current->buffer_index, x, y + h - 8, 0x1000, 0, 1);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x106, panel->bar_side, menu_state_current->buffer_index,
                  x, y + 8, 0x1000);
    name_entry_set_rect_verts(&panel->ends_at[0], x, y, 8, 8);
    name_entry_set_rect_verts(&panel->ends_at[4], x, y + h, 8, -8);
    name_entry_set_rect_verts(panel->side_at, x, y + 8, 8, h - 8);
}

/* 801C6928: Build panel `index`'s four corner sprites for this buffer and place them
 * around the rectangle (x, y, w, h). */
void name_entry_panel_layout_corners(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel = menu_state_current->panels[index];
    s32 i;

    panel->corner_parts = 0;
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0xFD, panel->corner,
                                         menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0xFF, &panel->corner[panel->corner_parts * 2],
                      menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x102, &panel->corner[panel->corner_parts * 2],
                      menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts +=
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x104, &panel->corner[panel->corner_parts * 2],
                      menu_state_current->buffer_index, 0, 0, 0x1000);
    name_entry_set_rect_verts(&panel->corner_at[0], x - 8, y + 8, 16, -16);
    name_entry_set_rect_verts(&panel->corner_at[4], x + w + 8, y + 8, -16, -16);
    name_entry_set_rect_verts(&panel->corner_at[8], x - 8, y + h - 8, 16, 16);
    name_entry_set_rect_verts(&panel->corner_at[12], x + w + 8, y + h - 8, -16, 16);
    for (i = 0; i < 4; i++) {
        name_entry_quad_set_semi_transparent(&panel->corner[i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C6B70: Map panel `index`'s top edge pieces for this buffer and place them in two
 * halves along the top of (x, y, w). */
void name_entry_panel_layout_top_edge(u8 index, u16 x, u16 y, u16 w) {
    MenuPanel *panel = menu_state_current->panels[index];
    s32 half;
    s32 i;

    (panel->edge[0] + menu_state_current->buffer_index)->u0 = 0;
    (panel->edge[0] + menu_state_current->buffer_index)->v0 = 0x84;
    (panel->edge[0] + menu_state_current->buffer_index)->u1 = 7;
    (panel->edge[0] + menu_state_current->buffer_index)->v1 = 0x84;
    (panel->edge[0] + menu_state_current->buffer_index)->u2 = 0;
    (panel->edge[0] + menu_state_current->buffer_index)->v2 = 0x94;
    (panel->edge[0] + menu_state_current->buffer_index)->u3 = 7;
    (panel->edge[0] + menu_state_current->buffer_index)->v3 = 0x94;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->u0 = 0;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->v0 = 0x84;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->u1 = 7;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->v1 = 0x84;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->u2 = 0;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->v2 = 0x94;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->u3 = 7;
    (panel->edge[0] + menu_state_current->buffer_index + 2)->v3 = 0x94;
    half = (w - 16) / 2;
    name_entry_set_rect_verts(panel->edge_at[0][0], x + 8, y - 8, half, 16);
    name_entry_set_rect_verts(panel->edge_at[0][1], x + (half + 8), y - 8, half, 16);
    for (i = 0; i < 2; i++) {
        name_entry_quad_set_semi_transparent(&panel->edge[0][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C6EB4: Map panel `index`'s bottom edge pieces for this buffer and place them in
 * two halves along the bottom of (x, y, w, h). */
void name_entry_panel_layout_bottom_edge(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel = menu_state_current->panels[index];
    s32 half;
    s32 i;

    (panel->edge[1] + menu_state_current->buffer_index)->u0 = 8;
    (panel->edge[1] + menu_state_current->buffer_index)->v0 = 0x84;
    (panel->edge[1] + menu_state_current->buffer_index)->u1 = 0xF;
    (panel->edge[1] + menu_state_current->buffer_index)->v1 = 0x84;
    (panel->edge[1] + menu_state_current->buffer_index)->u2 = 8;
    (panel->edge[1] + menu_state_current->buffer_index)->v2 = 0x94;
    (panel->edge[1] + menu_state_current->buffer_index)->u3 = 0xF;
    (panel->edge[1] + menu_state_current->buffer_index)->v3 = 0x94;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->u0 = 8;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->v0 = 0x84;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->u1 = 0xF;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->v1 = 0x84;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->u2 = 8;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->v2 = 0x94;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->u3 = 0xF;
    (panel->edge[1] + menu_state_current->buffer_index + 2)->v3 = 0x94;
    half = (w - 16) / 2;
    name_entry_set_rect_verts(panel->edge_at[1][0], x + 8, y + h - 8, half, 16);
    name_entry_set_rect_verts(panel->edge_at[1][1], x + (half + 8), y + h - 8, half, 16);
    for (i = 0; i < 2; i++) {
        name_entry_quad_set_semi_transparent(&panel->edge[1][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C7200: Map panel `index`'s left edge pieces for this buffer and place them in two
 * halves down the left of (x, y, h). */
void name_entry_panel_layout_left_edge(u8 index, u16 x, u16 y, u16 h) {
    MenuPanel *panel = menu_state_current->panels[index];
    s32 half;
    s32 i;

    (panel->edge[2] + menu_state_current->buffer_index)->u0 = 0x10;
    (panel->edge[2] + menu_state_current->buffer_index)->v0 = 0x84;
    (panel->edge[2] + menu_state_current->buffer_index)->u1 = 0x20;
    (panel->edge[2] + menu_state_current->buffer_index)->v1 = 0x84;
    (panel->edge[2] + menu_state_current->buffer_index)->u2 = 0x10;
    (panel->edge[2] + menu_state_current->buffer_index)->v2 = 0x8B;
    (panel->edge[2] + menu_state_current->buffer_index)->u3 = 0x20;
    (panel->edge[2] + menu_state_current->buffer_index)->v3 = 0x8B;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->u0 = 0x10;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->v0 = 0x84;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->u1 = 0x20;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->v1 = 0x84;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->u2 = 0x10;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->v2 = 0x8B;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->u3 = 0x20;
    (panel->edge[2] + menu_state_current->buffer_index + 2)->v3 = 0x8B;
    half = (h - 16) / 2;
    name_entry_set_rect_verts(panel->edge_at[2][0], x - 8, y + 8, 16, half);
    name_entry_set_rect_verts(panel->edge_at[2][1], x - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        name_entry_quad_set_semi_transparent(&panel->edge[2][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C7548: Map panel `index`'s right edge pieces for this buffer and place them in two
 * halves down the right of (x, y, w, h). */
void name_entry_panel_layout_right_edge(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel = menu_state_current->panels[index];
    s32 half;
    s32 i;

    (panel->edge[3] + menu_state_current->buffer_index)->u0 = 0x10;
    (panel->edge[3] + menu_state_current->buffer_index)->v0 = 0x8C;
    (panel->edge[3] + menu_state_current->buffer_index)->u1 = 0x20;
    (panel->edge[3] + menu_state_current->buffer_index)->v1 = 0x8C;
    (panel->edge[3] + menu_state_current->buffer_index)->u2 = 0x10;
    (panel->edge[3] + menu_state_current->buffer_index)->v2 = 0x93;
    (panel->edge[3] + menu_state_current->buffer_index)->u3 = 0x20;
    (panel->edge[3] + menu_state_current->buffer_index)->v3 = 0x93;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->u0 = 0x10;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->v0 = 0x8C;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->u1 = 0x20;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->v1 = 0x8C;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->u2 = 0x10;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->v2 = 0x93;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->u3 = 0x20;
    (panel->edge[3] + menu_state_current->buffer_index + 2)->v3 = 0x93;
    half = (h - 16) / 2;
    name_entry_set_rect_verts(panel->edge_at[3][0], x + w - 8, y + 8, 16, half);
    name_entry_set_rect_verts(panel->edge_at[3][1], x + w - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        name_entry_quad_set_semi_transparent(&panel->edge[3][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C7894: Lay out panel `index` at (x, y, w, h) for this buffer: fill, corners and
 * edges, the frame sprites when `framed`, and mark it shown. */
void name_entry_panel_layout(u8 index, u16 x, u16 y, u16 w, u16 h, u8 style, s32 param, u8 framed) {
    MenuPanel *panel = menu_state_current->panels[index];

    menu_state_current->flags->panels_shown[index] = 0;
    name_entry_set_rect_verts(panel->fill_at, x, y, w, h);
    name_entry_panel_layout_corners(index, x, y, w, h);
    name_entry_panel_layout_top_edge(index, x, y, w);
    name_entry_panel_layout_bottom_edge(index, x, y, w, h);
    name_entry_panel_layout_left_edge(index, x, y, h);
    name_entry_panel_layout_right_edge(index, x, y, w, h);
    if (framed) {
        name_entry_panel_layout_scroll_bar(index, x, y, w, h);
    }
    panel->has_bar = framed;
    panel->flat = style;
    panel->ot_entry = param;
    panel->buffer = menu_state_current->buffer_index;
    menu_state_current->flags->panels_shown[index] = 1;
}

/* 801C7A18: Hide panel `index` and release its block and growth record. */
void name_entry_panel_close(u8 index) {
    menu_state_current->flags->panels_shown[index] = 0;
    menu_state_current->flags->panels_growing[index] = 0;
    heap_free(menu_state_current->panels[index]);
    heap_free(menu_state_current->growth[index]);
}

/* 801C7AA4: Open panel `index` (allocating it first unless it is 0 or 1): either
 * start its growth animation toward (x, y, w, h) or lay it out at once. */
void name_entry_panel_open(u8 index, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 style, s32 param,
                   u8 framed) {
    MenuGrowth *growth;

    if (index >= 2) {
        menu_state_current->panels[index] = heap_alloc(0x720, 0);
        bzero((u_char *)menu_state_current->panels[index], 0x720);
        menu_state_current->growth[index] = heap_alloc(0x18, 0);
        bzero((u_char *)menu_state_current->growth[index], 0x18);
        name_entry_panel_init(index);
    }
    growth = menu_state_current->growth[index];
    if (animate) {
        growth->index = index;
        growth->done = 0;
        growth->x = x;
        growth->y = y;
        growth->w = w;
        growth->h = h;
        growth->cur_w = 0;
        growth->cur_h = 0;
        menu_state_current->flags->panels_growing[index] = 1;
        growth->flat = style;
        growth->ot_entry = param;
    } else {
        name_entry_panel_layout(index, x, y, w, h, style, param, framed);
    }
}

/* 801C7C28: Grow every opening panel by 32 in width and height per frame until it
 * reaches its size, laying it out centred on its final rectangle. */
void name_entry_panel_grow_opening(void) {
    s32 i;
    MenuGrowth *growth;
    u8 done;

    for (i = 0; i < 7; i++) {
        growth = menu_state_current->growth[i];
        if (menu_state_current->flags->panels_growing[i] && !growth->done) {
            done = 0;
            if (growth->cur_w + 32 >= growth->w) {
                growth->cur_w = growth->w;
                done++;
            } else {
                growth->cur_w = growth->cur_w + 32;
            }
            if (growth->cur_h + 32 >= growth->h) {
                growth->cur_h = growth->h;
                done++;
            } else {
                growth->cur_h = growth->cur_h + 32;
            }
            if (done == 2) {
                growth->done = 1;
            }
            name_entry_panel_layout(growth->index, growth->x + growth->w / 2 - growth->cur_w / 2,
                          growth->y + growth->h / 2 - growth->cur_h / 2, growth->cur_w,
                          growth->cur_h, growth->flat, growth->ot_entry, growth->has_bar);
        }
    }
}

/* 801C7D74: Project panel `index`'s two top edge pieces through the GTE and draw them. */
void name_entry_draw_panel_top_edge(s32 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    long depth;
    long flag;

    RotTransPers4(&panel->edge_at[0][0][0], &panel->edge_at[0][0][1], &panel->edge_at[0][0][2],
                  &panel->edge_at[0][0][3], (long *)&(panel->edge[0] + panel->buffer)->x0,
                  (long *)&(panel->edge[0] + panel->buffer)->x1,
                  (long *)&(panel->edge[0] + panel->buffer)->x2,
                  (long *)&(panel->edge[0] + panel->buffer)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[0][panel->buffer]);
    RotTransPers4(&panel->edge_at[0][1][0], &panel->edge_at[0][1][1], &panel->edge_at[0][1][2],
                  &panel->edge_at[0][1][3], (long *)&(panel->edge[0] + panel->buffer + 2)->x0,
                  (long *)&(panel->edge[0] + panel->buffer + 2)->x1,
                  (long *)&(panel->edge[0] + panel->buffer + 2)->x2,
                  (long *)&(panel->edge[0] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[0][panel->buffer + 2]);
}

/* 801C7F48: Project panel `index`'s two bottom edge pieces through the GTE and draw them. */
void name_entry_draw_panel_bottom_edge(s32 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    long depth;
    long flag;

    RotTransPers4(&panel->edge_at[1][0][0], &panel->edge_at[1][0][1], &panel->edge_at[1][0][2],
                  &panel->edge_at[1][0][3], (long *)&(panel->edge[1] + panel->buffer)->x0,
                  (long *)&(panel->edge[1] + panel->buffer)->x1,
                  (long *)&(panel->edge[1] + panel->buffer)->x2,
                  (long *)&(panel->edge[1] + panel->buffer)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[1][panel->buffer]);
    RotTransPers4(&panel->edge_at[1][1][0], &panel->edge_at[1][1][1], &panel->edge_at[1][1][2],
                  &panel->edge_at[1][1][3], (long *)&(panel->edge[1] + panel->buffer + 2)->x0,
                  (long *)&(panel->edge[1] + panel->buffer + 2)->x1,
                  (long *)&(panel->edge[1] + panel->buffer + 2)->x2,
                  (long *)&(panel->edge[1] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[1][panel->buffer + 2]);
}

/* 801C811C: Project panel `index`'s two left edge pieces through the GTE and draw them. */
void name_entry_draw_panel_left_edge(s32 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    long depth;
    long flag;

    RotTransPers4(&panel->edge_at[2][0][0], &panel->edge_at[2][0][1], &panel->edge_at[2][0][2],
                  &panel->edge_at[2][0][3], (long *)&(panel->edge[2] + panel->buffer)->x0,
                  (long *)&(panel->edge[2] + panel->buffer)->x1,
                  (long *)&(panel->edge[2] + panel->buffer)->x2,
                  (long *)&(panel->edge[2] + panel->buffer)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[2][panel->buffer]);
    RotTransPers4(&panel->edge_at[2][1][0], &panel->edge_at[2][1][1], &panel->edge_at[2][1][2],
                  &panel->edge_at[2][1][3], (long *)&(panel->edge[2] + panel->buffer + 2)->x0,
                  (long *)&(panel->edge[2] + panel->buffer + 2)->x1,
                  (long *)&(panel->edge[2] + panel->buffer + 2)->x2,
                  (long *)&(panel->edge[2] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[2][panel->buffer + 2]);
}

/* 801C82F0: Project panel `index`'s two right edge pieces through the GTE and draw them. */
void name_entry_draw_panel_right_edge(s32 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    long depth;
    long flag;

    RotTransPers4(&panel->edge_at[3][0][0], &panel->edge_at[3][0][1], &panel->edge_at[3][0][2],
                  &panel->edge_at[3][0][3], (long *)&(panel->edge[3] + panel->buffer)->x0,
                  (long *)&(panel->edge[3] + panel->buffer)->x1,
                  (long *)&(panel->edge[3] + panel->buffer)->x2,
                  (long *)&(panel->edge[3] + panel->buffer)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[3][panel->buffer]);
    RotTransPers4(&panel->edge_at[3][1][0], &panel->edge_at[3][1][1], &panel->edge_at[3][1][2],
                  &panel->edge_at[3][1][3], (long *)&(panel->edge[3] + panel->buffer + 2)->x0,
                  (long *)&(panel->edge[3] + panel->buffer + 2)->x1,
                  (long *)&(panel->edge[3] + panel->buffer + 2)->x2,
                  (long *)&(panel->edge[3] + panel->buffer + 2)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->edge[3][panel->buffer + 2]);
}

/* 801C84C4: Project panel `index`'s fill through the GTE and draw it with its mode. */
void name_entry_draw_panel_fill(s32 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    long depth;
    long flag;

    RotTransPers4(&panel->fill_at[0], &panel->fill_at[1], &panel->fill_at[2], &panel->fill_at[3],
                  (long *)&(panel->fill + panel->buffer)->x0,
                  (long *)&(panel->fill + panel->buffer)->x1,
                  (long *)&(panel->fill + panel->buffer)->x2,
                  (long *)&(panel->fill + panel->buffer)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->fill[panel->buffer]);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->fill_mode[panel->buffer]);
}

/* 801C8600: Project panel `index`'s four corner sprites through the GTE and draw them. */
void name_entry_draw_panel_corners(s32 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    long depth;
    long flag;
    s32 i;

    for (i = 0; i < 4; i++) {
        RotTransPers4(&panel->corner_at[i * 4], &panel->corner_at[i * 4 + 1],
                      &panel->corner_at[i * 4 + 2], &panel->corner_at[i * 4 + 3],
                      (long *)&panel->corner[i * 2 + panel->buffer].x0,
                      (long *)&panel->corner[i * 2 + panel->buffer].x1,
                      (long *)&panel->corner[i * 2 + panel->buffer].x2,
                      (long *)&panel->corner[i * 2 + panel->buffer].x3, &depth, &flag);
        AddPrim(menu_state_current->current->ot + panel->ot_entry,
                      &panel->corner[i * 2 + panel->buffer]);
    }
}

/* 801C874C: Project panel `index`'s frame sprites (top, bottom, side) and draw them. */
void name_entry_draw_panel_scroll_bar(s32 index) {
    MenuPanel *panel = menu_state_current->panels[index];
    long depth;
    long flag;
    s32 i;

    for (i = 0; i < 2; i++) {
        RotTransPers4(&panel->ends_at[i * 4], &panel->ends_at[i * 4 + 1],
                      &panel->ends_at[i * 4 + 2], &panel->ends_at[i * 4 + 3],
                      (long *)&(panel->bar_ends + (i * 2 + panel->buffer))->x0,
                      (long *)&(panel->bar_ends + (i * 2 + panel->buffer))->x1,
                      (long *)&(panel->bar_ends + (i * 2 + panel->buffer))->x2,
                      (long *)&(panel->bar_ends + (i * 2 + panel->buffer))->x3, &depth, &flag);
        AddPrim(menu_state_current->current->ot + panel->ot_entry,
                      &panel->bar_ends[i * 2 + panel->buffer]);
    }
    RotTransPers4(&panel->side_at[0], &panel->side_at[1], &panel->side_at[2], &panel->side_at[3],
                  (long *)&(panel->bar_side + panel->buffer)->x0,
                  (long *)&(panel->bar_side + panel->buffer)->x1,
                  (long *)&(panel->bar_side + panel->buffer)->x2,
                  (long *)&(panel->bar_side + panel->buffer)->x3, &depth, &flag);
    AddPrim(menu_state_current->current->ot + panel->ot_entry, &panel->bar_side[panel->buffer]);
}

/* 801C8970: Draw every shown panel; style-0 panels are projected with an identity
 * rotation at depth 0x200. */
void name_entry_draw_panels(void) {
    SVECTOR rotation;
    VECTOR translation;
    MATRIX matrix;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    MenuPanel *panel;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (menu_state_current->flags->panels_shown[i]) {
            panel = menu_state_current->panels[i];
            if (panel->flat == 0) {
                PushMatrix();
                rotation.vz = 0;
                rotation.vy = 0;
                rotation.vx = 0;
                translation.vy = 0;
                translation.vx = 0;
                translation.vz = 0x200;
                gpu_build_rotation_matrix(&rotation, &matrix);
                TransMatrix(&matrix, &translation);
                SetRotMatrix(&matrix);
                SetTransMatrix(&matrix);
                name_entry_draw_panel_corners(i);
                if (panel->has_bar) {
                    name_entry_draw_panel_scroll_bar(i);
                }
                name_entry_draw_panel_top_edge(i);
                name_entry_draw_panel_bottom_edge(i);
                name_entry_draw_panel_left_edge(i);
                name_entry_draw_panel_right_edge(i);
                name_entry_draw_panel_fill(i);
                PopMatrix();
            } else {
                name_entry_draw_panel_corners(i);
                if (panel->has_bar) {
                    name_entry_draw_panel_scroll_bar(i);
                }
                name_entry_draw_panel_top_edge(i);
                name_entry_draw_panel_bottom_edge(i);
                name_entry_draw_panel_left_edge(i);
                name_entry_draw_panel_right_edge(i);
                name_entry_draw_panel_fill(i);
            }
        }
    }
}

/* 801C8AE8: Draw the four cursor markers when markers are on; a marker that follows
 * the file cursor is first moved to the selected slot's position. */
void name_entry_draw_markers(void) {
    s32 i;
    MenuState *state;

    if (menu_state_current->flags->markers_shown) {
        for (i = 0; i < 4; i++) {
            state = menu_state_current;
            if (state->markers->shown[i]) {
                if (state->markers->at_cursor[i]) {
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x0 =
                        name_entry_file_slot_x_table[name_entry_file_cursor_card_slots[state->card->cursor]] + 8;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y0 =
                        name_entry_file_slot_y_table[name_entry_file_cursor_card_slots[state->card->cursor]] - 6;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x1 =
                        name_entry_file_slot_x_table[name_entry_file_cursor_card_slots[state->card->cursor]] + 24;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y1 =
                        name_entry_file_slot_y_table[name_entry_file_cursor_card_slots[state->card->cursor]] - 6;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x2 =
                        name_entry_file_slot_x_table[name_entry_file_cursor_card_slots[state->card->cursor]] + 8;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y2 =
                        name_entry_file_slot_y_table[name_entry_file_cursor_card_slots[state->card->cursor]] + 10;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->x3 =
                        name_entry_file_slot_x_table[name_entry_file_cursor_card_slots[state->card->cursor]] + 24;
                    (state->markers->polys + (i * 2 + state->markers->buffer[i]))->y3 =
                        name_entry_file_slot_y_table[name_entry_file_cursor_card_slots[state->card->cursor]] + 10;
                }
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->markers->polys[i * 2 + menu_state_current->markers->buffer[i]]);
            }
        }
    }
}

/* 801C8E38: Draw the shown command labels. */
void name_entry_draw_command_labels(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (menu_state_current->flags->labels_shown[i]) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->labels[i].polys[menu_state_current->labels[i].buffer]);
        }
    }
}

/* 801C8EC8: Draw the shown list labels. */
void name_entry_draw_list_labels(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (menu_state_current->flags->list_labels_shown[i]) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->list_labels[i].polys[menu_state_current->list_labels[i].buffer]);
        }
    }
}

/* 801C8F58: Draw the shown row labels, projecting the 3D ones through the GTE first. */
void name_entry_draw_row_labels(void) {
    long depth;
    long flag;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (menu_state_current->flags->row_labels_shown[i]) {
            if (menu_state_current->row_labels[i].projected) {
                MenuLabel *label = &menu_state_current->row_labels[i];

                RotTransPers4(&label->verts[0], &label->verts[1], &label->verts[2],
                              &label->verts[3],
                              (long *)&label->polys[menu_state_current->row_labels[i].buffer].x0,
                              (long *)&label->polys[menu_state_current->row_labels[i].buffer].x1,
                              (long *)&label->polys[menu_state_current->row_labels[i].buffer].x2,
                              (long *)&label->polys[menu_state_current->row_labels[i].buffer].x3, &depth,
                              &flag);
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer]);
            } else {
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer]);
            }
        }
    }
}

/* 801C90D0: Draw the shown name entry labels. */
void name_entry_draw_extra_labels(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (menu_state_current->flags->extra_labels_shown[i]) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->extra_labels[i].polys[menu_state_current->extra_labels[i].buffer]);
        }
    }
}

/* 801C9160: Draw the shown message lines, projecting the 3D ones through the GTE. */
void name_entry_draw_notice_labels(void) {
    long depth;
    long flag;
    s32 i;
    MenuLabel *line;

    if (menu_state_current->flags->messages_shown) {
        for (i = 0; i < 3; i++) {
            line = menu_state_current->message_labels[i];
            if (line->projected) {
                RotTransPers4(&line->verts[0], &line->verts[1], &line->verts[2], &line->verts[3],
                              (long *)&line->polys[line->buffer].x0,
                              (long *)&line->polys[line->buffer].x1,
                              (long *)&line->polys[line->buffer].x2,
                              (long *)&line->polys[line->buffer].x3, &depth, &flag);
                AddPrim(&menu_state_current->current->ot[4], &line->polys[line->buffer]);
            } else {
                AddPrim(&menu_state_current->current->ot[4], &line->polys[line->buffer]);
            }
        }
    }
}

/* 801C92BC: Draw this buffer's fade quad and the second draw mode. */
void name_entry_draw_fade(void) {
    AddPrim(&menu_state_current->current->ot[8],
                  &menu_state_current->prims->fade[menu_state_current->buffer_index]);
    AddPrim(&menu_state_current->current->ot[8],
                  &menu_state_current->prims->mode_sprite[menu_state_current->buffer_index]);
}

/* 801C9338: Draw the name entry: the projected cursor and name quads, the confirm,
 * back and frame sprites, the grid lines, the blinking caret after the name
 * and the 36 grid characters. */
void name_entry_draw_grid_and_name(void) {
    long depth;
    long flag;
    s32 i;

    if (menu_state_current->flags->name_entry_shown) {
        if (menu_state_current->name_entry->shown) {
            RotTransPers4(&menu_state_current->name_entry->cursor_at[0],
                          &menu_state_current->name_entry->cursor_at[1],
                          &menu_state_current->name_entry->cursor_at[2],
                          &menu_state_current->name_entry->cursor_at[3],
                          (long *)&(menu_state_current->name_entry->cursor + menu_state_current->name_entry->cursor_buffer)->x0,
                          (long *)&(menu_state_current->name_entry->cursor + menu_state_current->name_entry->cursor_buffer)->x1,
                          (long *)&(menu_state_current->name_entry->cursor + menu_state_current->name_entry->cursor_buffer)->x2,
                          (long *)&(menu_state_current->name_entry->cursor + menu_state_current->name_entry->cursor_buffer)->x3,
                          &depth, &flag);
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->name_entry->cursor[menu_state_current->name_entry->cursor_buffer]);
            RotTransPers4(&menu_state_current->name_entry->name_at[0], &menu_state_current->name_entry->name_at[1],
                          &menu_state_current->name_entry->name_at[2], &menu_state_current->name_entry->name_at[3],
                          (long *)&(menu_state_current->name_entry->name + menu_state_current->name_entry->name_buffer)->x0,
                          (long *)&(menu_state_current->name_entry->name + menu_state_current->name_entry->name_buffer)->x1,
                          (long *)&(menu_state_current->name_entry->name + menu_state_current->name_entry->name_buffer)->x2,
                          (long *)&(menu_state_current->name_entry->name + menu_state_current->name_entry->name_buffer)->x3,
                          &depth, &flag);
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->name_entry->name[menu_state_current->name_entry->name_buffer]);
        }
        if (menu_state_current->name_entry->grid_shown) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->name_entry->confirm[menu_state_current->name_entry->parts_buffer]);
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->name_entry->back[menu_state_current->name_entry->parts_buffer]);
            for (i = 0; i < menu_state_current->name_entry->frame_count; i++) {
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->name_entry->frame[i * 2 + menu_state_current->name_entry->parts_buffer]);
            }
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->name_entry->line_a[menu_state_current->name_entry->lines_buffer]);
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->name_entry->line_b[menu_state_current->name_entry->lines_buffer]);
            if (++menu_state_current->name_entry->blink >= 61) {
                menu_state_current->name_entry->blink = 0;
            }
            if (menu_state_current->name_entry->blink < 30) {
                (menu_state_current->name_entry->caret + menu_state_current->buffer_index)->x0 =
                    menu_state_current->name_entry->length * 8 + 0x50;
                (menu_state_current->name_entry->caret + menu_state_current->buffer_index)->y0 = 0xC6;
                (menu_state_current->name_entry->caret + menu_state_current->buffer_index)->x1 =
                    menu_state_current->name_entry->length * 8 + 0x58;
                (menu_state_current->name_entry->caret + menu_state_current->buffer_index)->y1 = 0xC6;
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->name_entry->caret[menu_state_current->buffer_index]);
            }
            for (i = 0; i < 36; i++) {
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->name_entry->chars[i * 2 + menu_state_current->name_entry->chars_buffer]);
            }
        }
    }
}

/* 801C97FC: Draw all labels, the name entry labels and the message lines. */
void name_entry_draw_label_layers(void) {
    name_entry_draw_command_labels();
    name_entry_draw_list_labels();
    name_entry_draw_row_labels();
    name_entry_draw_extra_labels();
    name_entry_draw_notice_labels();
}

/* 801C983C: Per-frame screen drawing: panels, markers, labels and the name entry while
 * active, then the fade. */
void name_entry_draw_screen(void) {
    if (menu_state_current->drawing) {
        name_entry_panel_grow_opening();
        name_entry_draw_markers();
        name_entry_draw_label_layers();
        name_entry_draw_grid_and_name();
        name_entry_draw_panels();
    }
    name_entry_draw_fade();
}

/* 801C989C: Play menu sound `sound` from the loaded effect bank when sounds are on. */
void name_entry_play_sound(u8 sound) {
    if (menu_state_current->sounds) {
        sound_play_effect_on_last_channels((menu_state_current->effects->id << 16) | sound);
    }
}

/* 801C98E8: Read this frame's input into the input code (8: none). Without a pad the
 * sound is paused (keeping the vsync count) until one is connected; an
 * overflowed queue is reset; otherwise entries are dequeued until one holds
 * a button the screen uses (directions and cancel play their sounds). */
void name_entry_read_input(void) {
    u8 code = 8;
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;

    do {
        if (pad_get_controller_kind(0) == 0) {
            if (paused == 0) {
                paused++;
                sound_silence_voices();
                vsyncs = pad_vblank_count;
            }
        } else {
            waiting--;
            if (paused) {
                sound_restore_voices();
                pad_vblank_count = vsyncs;
            }
        }
    } while (waiting);
    if (pad_has_queue_overflowed()) {
        pad_clear_queue();
    } else {
        while (pad_dequeue_state()) {
            if (pad_port0_repeated & 0x2000) {
                code = 0;
                name_entry_play_sound(1);
                break;
            }
            if (pad_port0_repeated & 0x4000) {
                code = 1;
                name_entry_play_sound(1);
                break;
            }
            if (pad_port0_repeated & 0x8000) {
                code = 2;
                name_entry_play_sound(1);
                break;
            }
            if (pad_port0_repeated & 0x1000) {
                code = 3;
                name_entry_play_sound(1);
                break;
            }
            if (pad_port0_pressed & 0x20) {
                code = 4;
                break;
            }
            if (pad_port0_pressed & 0x40) {
                code = 5;
                name_entry_play_sound(3);
                break;
            }
            if (pad_port0_pressed & 0x80) {
                code = 6;
                break;
            }
            if (pad_port0_pressed & 0x10) {
                code = 7;
                break;
            }
            if (pad_port0_pressed & 4) {
                code = 10;
                break;
            }
            if (pad_port0_pressed & 8) {
                code = 9;
                break;
            }
            if (pad_port0_pressed & 0x800) {
                code = 11;
                break;
            }
            if (pad_port0_pressed & 0x100) {
                code = 12;
                menu_state_current->debug_show = menu_state_current->debug_show == 0;
                break;
            }
            if (pad_port0_pressed & 1) {
                menu_state_current->debug_value++;
                break;
            }
        }
    }
    menu_state_current->input = code;
}

/* 801C9AF4: Advance the view motion (4/3 start zooming in/out, 2/1 run them) and load
 * the view rotation and translation into the GTE. */
void name_entry_view_update(void) {
    switch (menu_state_current->view_motion) {
    case 4:
        menu_state_current->offset.vz = 0x200;
        menu_state_current->angles.vz = 0;
        menu_state_current->angles.vy = 0;
        menu_state_current->angles.vx = 0;
        menu_state_current->offset.vy = 0;
        menu_state_current->offset.vx = 0;
        menu_state_current->view_motion = 2;
        break;
    case 3:
        menu_state_current->offset.vz = 0x800;
        menu_state_current->angles.vz = 0;
        menu_state_current->angles.vy = 0;
        menu_state_current->angles.vx = 0;
        menu_state_current->offset.vy = 0;
        menu_state_current->offset.vx = 0;
        menu_state_current->view_motion = 1;
        break;
    case 2:
        menu_state_current->angles.vy -= 0x60;
        menu_state_current->offset.vz += 0x40;
        if (menu_state_current->offset.vz >= 0xE00) {
            menu_state_current->view_motion = 0;
        }
        break;
    case 1:
        menu_state_current->angles.vx += 0x7C;
        menu_state_current->offset.vz -= 0x30;
        if (menu_state_current->offset.vz < 0x200) {
            menu_state_current->offset.vz = 0x200;
            menu_state_current->angles.vz = 0;
            menu_state_current->angles.vx = 0;
            menu_state_current->angles.vy = 0;
            menu_state_current->view_motion = 0;
        }
        break;
    }
    gpu_build_rotation_matrix(&menu_state_current->angles, &menu_state_current->matrix);
    TransMatrix(&menu_state_current->matrix, &menu_state_current->offset);
    SetRotMatrix(&menu_state_current->matrix);
    SetTransMatrix(&menu_state_current->matrix);
}

/* 801C9C34: Run one menu frame: check the stack guard, read input, check the reset
 * combination, swap to the other draw buffer, draw the screen and present
 * it. */
void name_entry_run_frame(void) {
    MenuState *state;
    MenuBuffer *env;
    s32 shown;

    if (*mode_disc_mode_pointer != -1) {
        __asm__ volatile("break 1024");
    }
    name_entry_read_input();
    boot_check_soft_reset();
    state = menu_state_current;
    env = &state->buffers[0];
    if (state->current == env) {
        env = &state->buffers[1];
    }
    state->current = env;
    state->buffer_index = state->buffer_index == 0;
    ClearOTagR(state->current->ot, 16);
    name_entry_view_update();
    name_entry_draw_screen();
    shown = menu_state_current->buffer_index == 0;
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&menu_state_current->current->draw);
    PutDispEnv(&menu_state_current->current->disp);
    MoveImage(&menu_state_current->images->screen, 0, shown * 0xE0);
    DrawOTag(&menu_state_current->current->ot[15]);
}

/* 801C9D5C: Allocate the markers and set them up for `mode`: 0 and 2 build all four
 * at their home positions (0 also turns them on following the cursor), 3
 * builds the first at the origin and turns them on, 1 leaves them empty. */
void name_entry_markers_open(u8 mode) {
    s32 i;
    MenuMarkers *markers = heap_alloc(0x14C, 0);

    menu_state_current->markers = markers;
    bzero((u_char *)markers, 0x14C);
    switch (mode) {
    case 0:
        menu_state_current->flags->markers_shown = 1;
        menu_state_current->markers->at_cursor[0] = 1;
        menu_state_current->markers->at_cursor[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, menu_state_current->markers->polys + i * 2,
                          menu_state_current->buffer_index, name_entry_marker_x_table[i], name_entry_marker_y_table[i], 0x800);
            menu_state_current->markers->buffer[i] = menu_state_current->buffer_index;
        }
        break;
    case 3:
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, menu_state_current->markers->polys,
                      menu_state_current->buffer_index, 0, 0, 0x800);
        menu_state_current->markers->buffer[0] = menu_state_current->buffer_index;
        menu_state_current->flags->markers_shown = 1;
        break;
    case 1:
        break;
    }
}

/* 801C9F1C: Hide the markers, let one frame pass and release them. */
void name_entry_markers_close(void) {
    menu_state_current->flags->markers_shown = 0;
    name_entry_run_frame();
    heap_free(menu_state_current->markers);
}

/* 801C9F60: Start view motion 3 with its sound. */
void name_entry_view_start_zoom_in(void) {
    menu_state_current->view_motion = 3;
    name_entry_play_sound(0x5B);
}

/* 801C9F90: Start view motion 4 with its sound. */
void name_entry_view_start_zoom_out(void) {
    menu_state_current->view_motion = 4;
    name_entry_play_sound(0x5C);
}

/* 801C9FC0: Open the message: allocate four line labels (pairs share one render
 * buffer), render texts `first`..`first + 2` into VRAM, set up their 3D
 * quads and show them. */
void name_entry_notice_open(u8 first) {
    u16 x = 0x48;
    s32 i;
    MenuLabel *line;

    for (i = 0; i < 4; i++) {
        void *block = heap_alloc(0x80, 0);

        menu_state_current->message_labels[i] = block;
        bzero(block, 0x80);
        if (!(i & 1)) {
            menu_state_current->message_labels[i]->pixels = heap_alloc(0x5CA, 0);
            menu_state_current->message_labels[i]->rect.x = 0x140;
            menu_state_current->message_labels[i]->rect.y = (i / 2) * 13 + 0x4E;
            menu_state_current->message_labels[i]->rect.w = 0x3A;
            menu_state_current->message_labels[i]->rect.h = 13;
        } else {
            menu_state_current->message_labels[i]->pixels = menu_state_current->message_labels[i - 1]->pixels;
        }
    }
    for (i = 0; i < 3; i++) {
        line = menu_state_current->message_labels[i];
        line->width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, first + i), line->pixels,
                                    0x36, i % 2);
        name_entry_label_init_quads(line, i, 0, 0);
        name_entry_set_rect_verts(line->verts, x, i * 16 + 0xA0, line->width, 13);
        (line->polys + menu_state_current->buffer_index)->u0 = 0;
        (line->polys + menu_state_current->buffer_index)->v0 = (i / 2) * 13 + 0x4E;
        (line->polys + menu_state_current->buffer_index)->u1 = line->width;
        (line->polys + menu_state_current->buffer_index)->v1 = (i / 2) * 13 + 0x4E;
        (line->polys + menu_state_current->buffer_index)->u2 = 0;
        (line->polys + menu_state_current->buffer_index)->v2 = (i / 2) * 13 + 0x5B;
        (line->polys + menu_state_current->buffer_index)->u3 = line->width;
        (line->polys + menu_state_current->buffer_index)->v3 = (i / 2) * 13 + 0x5B;
        line->buffer = menu_state_current->buffer_index;
        line->projected = 1;
    }
    LoadImage(&menu_state_current->message_labels[0]->rect,
              (u_long *)menu_state_current->message_labels[0]->pixels);
    LoadImage(&menu_state_current->message_labels[2]->rect,
              (u_long *)menu_state_current->message_labels[2]->pixels);
    DrawSync(0);
    menu_state_current->flags->messages_shown = 1;
    heap_free(menu_state_current->message_labels[0]->pixels);
    heap_free(menu_state_current->message_labels[2]->pixels);
    name_entry_run_frame();
    name_entry_run_frame();
}

/* 801CA39C: Turn the message lines off, release their four blocks and run a frame. */
void name_entry_notice_close(void) {
    s32 i;

    menu_state_current->flags->messages_shown = 0;
    for (i = 0; i < 4; i++) {
        heap_free(menu_state_current->message_labels[i]);
    }
    name_entry_run_frame();
}

/* 801CA400: Leave the screen: run the closing frames until buffer 0 is shown and
 * release everything (a frame passes around the sound bank release). */
void name_entry_shut_down(void) {
    name_entry_run_frame();
    name_entry_run_frame();
    menu_state_current->drawing = 0;
    name_entry_run_frame();
    do {
        name_entry_run_frame();
    } while (menu_state_current->buffer_index != 0);
    name_entry_alloc_or_free_card_state(0);
    name_entry_alloc_or_free_flags(0);
    name_entry_alloc_or_free_screen_images(0);
    name_entry_alloc_or_free_sprite_lists(0);
    name_entry_alloc_or_free_table_directory(0);
    name_entry_alloc_or_free_prims(0);
    heap_free(menu_state_current->sheet);
    heap_free(menu_state_current->label_text);
    heap_free(menu_state_current->labels[0].pixels);
    if (menu_state_debug_start) {
        sound_stop_bank_effects(menu_state_current->effects);
        name_entry_run_frame();
        sound_remove_effect_bank(menu_state_current->effects);
        name_entry_run_frame();
        heap_free(menu_state_current->effects);
    }
    name_entry_alloc_or_free_name_entry_block(0);
    heap_free(menu_state_current);
}

/* 801CA558: Build the name entry grid: set up the 72 character quads and the name
 * quad, render the 36 grid entries (five codes each from name_entry_grid_codes) into
 * VRAM and place them in 4 columns of 9, map and place the name quad, and
 * set up the green grid lines and the caret. */
void name_entry_build_grid(void) {
    u16 codes[8];
    u8 text[16];
    RECT rect;
    u8 *image = heap_alloc(0x2BE, 0);
    s32 i;
    s32 k;
    s32 column;
    s32 row;

    for (i = 0; i < 74; i++) {
        SetPolyFT4(&menu_state_current->name_entry->chars[i]);
        (menu_state_current->name_entry->chars + i)->r0 = 0x80;
        (menu_state_current->name_entry->chars + i)->g0 = 0x80;
        (menu_state_current->name_entry->chars + i)->b0 = 0x80;
        SetSemiTrans(&menu_state_current->name_entry->chars[i], 0);
        SetShadeTex(&menu_state_current->name_entry->chars[i], 1);
        menu_state_current->name_entry->chars[i].clut = text_plane0_clut;
        menu_state_current->name_entry->chars[i].tpage = GetTPage(0, 0, 0x180, 0);
    }
    for (i = 0; i < 36; i++) {
        for (k = 0; k < 5; k++) {
            codes[k] = name_entry_grid_codes[i * 6 + k];
        }
        text_decode_codes(codes, text, 5);
        bzero(image, 0x2BE);
        window_render_text_line(text, image, 0x18, 0);
        rect.x = (i % 2) * 32 + 0x180;
        rect.y = (i / 2) * 13;
        rect.w = 0x1C;
        rect.h = 13;
        LoadImage(&rect, (u_long *)image);
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->u0 = (i % 2) << 7;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->v0 = (i / 2) * 13;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->u1 = ((i % 2) << 7) + 0x3C;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->v1 = (i / 2) * 13;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->u2 = (i % 2) << 7;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->v2 = (i / 2) * 13 + 13;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->u3 = ((i % 2) << 7) + 0x3C;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->v3 = (i / 2) * 13 + 13;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->x0 = (i / 9) * 0x30 + 0x44;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->y0 = (i % 9) * 16 + 0x2E;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->x1 = (i / 9) * 0x30 + 0x80;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->y1 = (i % 9) * 16 + 0x2E;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->x2 = (i / 9) * 0x30 + 0x44;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->y2 = (i % 9) * 16 + 0x3B;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->x3 = (i / 9) * 0x30 + 0x80;
        (menu_state_current->name_entry->chars + (i * 2 + menu_state_current->buffer_index))->y3 = (i % 9) * 16 + 0x3B;
        DrawSync(0);
    }
    heap_free(image);
    menu_state_current->name_entry->chars_buffer = menu_state_current->buffer_index;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->u0 = 0;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->v0 = 0xEA;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->u1 = menu_state_current->name_entry->max_length * 8;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->v1 = 0xEA;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->u2 = 0;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->v2 = 0xF7;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->u3 = menu_state_current->name_entry->max_length * 8;
    (menu_state_current->name_entry->name + menu_state_current->buffer_index)->v3 = 0xF7;
    name_entry_set_rect_verts(menu_state_current->name_entry->name_at,
                  menu_state_current->name_entry->name[menu_state_current->buffer_index].x0 + 0x50,
                  menu_state_current->name_entry->name[menu_state_current->buffer_index].y0 + 0xB6,
                  menu_state_current->name_entry->max_length * 8, 13);
    menu_state_current->name_entry->name_buffer = menu_state_current->buffer_index;
    for (i = 0; i < 2; i++) {
        SetLineF3(&menu_state_current->name_entry->line_a[i]);
        (menu_state_current->name_entry->line_a + i)->r0 = 0;
        (menu_state_current->name_entry->line_a + i)->g0 = 0xFF;
        (menu_state_current->name_entry->line_a + i)->b0 = 0;
        SetLineF3(&menu_state_current->name_entry->line_b[i]);
        (menu_state_current->name_entry->line_b + i)->r0 = 0;
        (menu_state_current->name_entry->line_b + i)->g0 = 0xFF;
        (menu_state_current->name_entry->line_b + i)->b0 = 0;
        SetLineF2(&menu_state_current->name_entry->caret[i]);
        (menu_state_current->name_entry->caret + i)->r0 = 0;
        (menu_state_current->name_entry->caret + i)->g0 = 0x80;
        (menu_state_current->name_entry->caret + i)->b0 = 0;
    }
}

/* 801CADC8: Open the name entry: its panel and message, the cursor sprite and its
 * quad, the confirm/back/grid sprites, the two command labels, the
 * character grid, and show the entry. */
void name_entry_open_entry_screen(void) {
    name_entry_panel_open(3, 0x10, 0x9A, 0xC0, 0x3C, 1, 1, 4, 0);
    name_entry_notice_open(0x1D);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x14B, menu_state_current->name_entry->cursor,
                  menu_state_current->buffer_index, 0, 0, 0x1000);
    name_entry_set_rect_verts(menu_state_current->name_entry->cursor_at,
                  menu_state_current->name_entry->cursor[menu_state_current->buffer_index].x0 + 0x18,
                  menu_state_current->name_entry->cursor[menu_state_current->buffer_index].y0 + 0x9E,
                  menu_state_current->name_entry->cursor[menu_state_current->buffer_index].x1 -
                      menu_state_current->name_entry->cursor[menu_state_current->buffer_index].x0,
                  menu_state_current->name_entry->cursor[menu_state_current->buffer_index].y3 -
                      menu_state_current->name_entry->cursor[menu_state_current->buffer_index].y0);
    menu_state_current->name_entry->cursor_buffer = menu_state_current->buffer_index;
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xF9, menu_state_current->name_entry->confirm,
                  menu_state_current->buffer_index, 0xE8, 0xB6, 0x1000);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xFC, menu_state_current->name_entry->back,
                  menu_state_current->buffer_index, 0xE0, 0xC6, 0x1000);
    menu_state_current->name_entry->frame_count =
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0xF0, menu_state_current->name_entry->frame,
                      menu_state_current->buffer_index, 0xF4, 0x6E, 0x1000);
    menu_state_current->name_entry->parts_buffer = menu_state_current->buffer_index;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->x0 = 0xF8;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->y0 = 0xB6;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->x1 = menu_state_current->labels[0].width + 0xF8;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->y1 = 0xB6;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->x2 = 0xF8;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->y2 = 0xC3;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->x3 = menu_state_current->labels[0].width + 0xF8;
    (menu_state_current->labels[0].polys + menu_state_current->buffer_index)->y3 = 0xC3;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->x0 = 0xF0;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->y0 = 0xC6;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->x1 = menu_state_current->labels[3].width + 0xF0;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->y1 = 0xC6;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->x2 = 0xF0;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->y2 = 0xD3;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->x3 = menu_state_current->labels[3].width + 0xF0;
    (menu_state_current->labels[3].polys + menu_state_current->buffer_index)->y3 = 0xD3;
    menu_state_current->labels[3].buffer = menu_state_current->buffer_index;
    name_entry_build_grid();
    menu_state_current->name_entry->shown = 1;
    menu_state_current->flags->name_entry_shown = 1;
}

/* 801CB1C4: Render the name being entered (text codes `codes`) into VRAM at (0x180, 0xEA). */
void name_entry_render_name(u8 *codes) {
    RECT rect;
    u8 *image = heap_alloc(0x3F6, 0);

    bzero(image, 0x3F6);
    window_render_text_line(codes, image, 0x24, 0);
    rect.x = 0x180;
    rect.y = 0xEA;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, (u_long *)image);
    DrawSync(0);
    heap_free(image);
}

/* 801CB25C: Start a name entry: fill the entry buffer `codes` with blanks (code 0x1C)
 * ending in the terminator, and copy the character's current name. */
void name_entry_init_buffers(u8 *codes, u8 *name) {
    s32 i;

    for (i = 0; i < 20; i += 2) {
        codes[i] = 0x1C;
        codes[i + 1] = 0;
        name[i] = game_data.names[menu_state_screen_parameter][i];
        name[i + 1] = (game_data.names[menu_state_screen_parameter] + 1)[i];
    }
    codes[18] = 0x1F;
    codes[19] = 0;
}

/* 801CB2F0: Number of two-byte text codes before the terminator (0x1F 0x00). */
u8 name_entry_count_codes(u8 *codes) {
    s32 i;

    for (i = 0; i < 20; i += 2) {
        if (codes[i] == 0x1F && codes[i + 1] == 0) {
            break;
        }
    }
    return i / 2;
}

/* 801CB33C: The name entry loop: zoom the view in, open the grid, move the cursor over
 * the character grid (skipping blank cells) and edit the name until it is
 * confirmed with a non-empty name; then store it for the three characters
 * of the portrait list (trailing blanks cut), and close the screen.
 * Confirming (11) is written first in the switch and again where a blank
 * cell is picked (4); cross-jumping later merges the two copies into the
 * one after case 4, but the copies give 15 and `running` the references
 * that put them in s5 and fp (0xCF stays without a register and is
 * reloaded at each compare), and make 15 the first constant loop.c hoists. */
void name_entry_run(void) {
    u8 codes[24];
    u8 name[24];
    s32 prev_col = 0xFF;
    s32 prev_row = 0xFF;
    s32 col = 22;
    s32 row = 4;
    u8 dirty;
    u8 running;
    u8 c;
    s32 index;
    s32 i;
    s32 k;
    s32 last;

    name_entry_init_buffers(codes, name);
    dirty = 0;
    name_entry_open_entry_screen();
    name_entry_render_name(name);
    running = 1;
    name_entry_view_start_zoom_in();
    while (menu_state_current->view_motion) {
        name_entry_run_frame();
    }
    name_entry_panel_open(2, 0x38, 0x26, 0xD0, 0x60, 1, 0, 4, 0);
    while (!menu_state_current->growth[2]->done) {
        name_entry_run_frame();
    }
    menu_state_current->name_entry->grid_shown = 1;
    menu_state_current->flags->labels_shown[0] = 1;
    menu_state_current->flags->labels_shown[3] = 1;
    name_entry_markers_open(1);
    menu_state_current->flags->markers_shown = 1;
    while (running) {
        if (col != prev_col || row != prev_row) {
            sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, menu_state_current->markers,
                          menu_state_current->buffer_index, col * 8 + 0x48, row * 16 + 0x36, 0x800);
            (menu_state_current->name_entry->line_a + menu_state_current->buffer_index)->x0 = col * 8 + 0x44;
            (menu_state_current->name_entry->line_a + menu_state_current->buffer_index)->y0 = row * 16 + 0x2E;
            (menu_state_current->name_entry->line_a + menu_state_current->buffer_index)->x1 = col * 8 + 0x4C;
            (menu_state_current->name_entry->line_a + menu_state_current->buffer_index)->y1 = row * 16 + 0x2E;
            (menu_state_current->name_entry->line_a + menu_state_current->buffer_index)->x2 = col * 8 + 0x4C;
            (menu_state_current->name_entry->line_a + menu_state_current->buffer_index)->y2 = row * 16 + 0x3A;
            (menu_state_current->name_entry->line_b + menu_state_current->buffer_index)->x0 = col * 8 + 0x44;
            (menu_state_current->name_entry->line_b + menu_state_current->buffer_index)->y0 = row * 16 + 0x2E;
            (menu_state_current->name_entry->line_b + menu_state_current->buffer_index)->x1 = col * 8 + 0x44;
            (menu_state_current->name_entry->line_b + menu_state_current->buffer_index)->y1 = row * 16 + 0x3A;
            (menu_state_current->name_entry->line_b + menu_state_current->buffer_index)->x2 = col * 8 + 0x4C;
            (menu_state_current->name_entry->line_b + menu_state_current->buffer_index)->y2 = row * 16 + 0x3A;
            menu_state_current->name_entry->lines_buffer = menu_state_current->buffer_index;
            prev_col = col;
            menu_state_current->markers->buffer[0] = menu_state_current->buffer_index;
            prev_row = row;
            menu_state_current->markers->shown[0] = 1;
        }
        if (dirty) {
            name_entry_render_name(name);
            dirty = 0;
        }
        name_entry_run_frame();
        switch (menu_state_current->input) {
        case 11:
            if (codes[0] != 0xF) {
                running = 0;
            }
            break;
        case 5:
            if (menu_state_current->name_entry->length) {
                menu_state_current->name_entry->length--;
            }
            codes[menu_state_current->name_entry->length * 2] = 0xF;
            codes[menu_state_current->name_entry->length * 2 + 1] = 0;
            text_decode_codes(codes, name, name_entry_count_codes(codes));
            dirty = 1;
            break;
        case 4:
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = name_entry_grid_codes[index];
            if (c != 0x1F) {
                if (menu_state_current->name_entry->length < menu_state_current->name_entry->max_length) {
                    if (c == 0xCF || c == 0xF) {
                        name_entry_grid_codes[index] = 0xC3;
                    }
                    codes[menu_state_current->name_entry->length * 2] = name_entry_grid_codes[index];
                    codes[menu_state_current->name_entry->length * 2 + 1] = 0;
                    text_decode_codes(codes, name, name_entry_count_codes(codes));
                    dirty = 1;
                    menu_state_current->name_entry->length++;
                    name_entry_play_sound(2);
                } else {
                    name_entry_play_sound(4);
                }
                break;
            }
            if (codes[0] != 0xF) {
                running = 0;
            }
            break;
        case 0:
            col++;
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = name_entry_grid_codes[index];
            if (c == 0xFF) {
                col = 0;
            } else if (c == 0xF || c == 0xCF) {
                col++;
            }
            break;
        case 2:
            if (--col < 0) {
                col = 22;
            } else {
                index = (row + (col / 6) * 9) * 6 + col % 6;
                c = name_entry_grid_codes[index];
                if (c == 0xF || c == 0xCF) {
                    col--;
                }
            }
            break;
        case 1:
            if (++row >= 5) {
                row = 0;
            }
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = name_entry_grid_codes[index];
            if (c == 0xCF) {
                row = 0;
            } else if (c == 0xF) {
                row++;
            }
            break;
        case 3:
            if (--row < 0) {
                row = 4;
            }
            index = (row + (col / 6) * 9) * 6 + col % 6;
            c = name_entry_grid_codes[index];
            if (c == 0xF || c == 0xCF) {
                row--;
            }
            break;
        case 9:
            if (++menu_state_current->name_entry->length > menu_state_current->name_entry->max_length) {
                menu_state_current->name_entry->length--;
            }
            break;
        case 10:
            if (menu_state_current->name_entry->length) {
                menu_state_current->name_entry->length--;
            }
            break;
        }
    }
    for (i = 0, last = 0; i < 3; i++) {
        for (k = 0; k < 20; k++) {
            game_data.names[menu_state_current->portraits[i]][k] = 0;
        }
        for (k = 0; k < 18; k++) {
            game_data.names[menu_state_current->portraits[i]][k] = name[k];
            if (name[k] == 0) {
                break;
            }
            if (name[k] != 0x4F) {
                last = k;
            }
        }
        for (k = last + 1; k < 20; k++) {
            game_data.names[menu_state_current->portraits[i]][k] = 0;
        }
    }
    menu_state_current->flags->labels_shown[0] = 0;
    menu_state_current->flags->labels_shown[3] = 0;
    name_entry_markers_close();
    menu_state_current->name_entry->grid_shown = 0;
    name_entry_panel_close(2);
    name_entry_view_start_zoom_out();
    while (menu_state_current->offset.vz < 0x600) {
        name_entry_run_frame();
    }
    menu_state_current->flags->name_entry_shown = 0;
    name_entry_notice_close();
    name_entry_panel_close(3);
}

/* 801CBDBC: Overlay entry: allocate and set up the name entry screen, run it and leave. */
void name_entry_main(void) {
    MenuState *state;

    name_entry_alloc_or_free_card_state(1);
    name_entry_alloc_or_free_flags(1);
    name_entry_alloc_or_free_screen_images(1);
    name_entry_alloc_or_free_sprite_lists(1);
    name_entry_alloc_or_free_table_directory(1);
    name_entry_alloc_or_free_prims(1);
    name_entry_alloc_or_free_name_entry_block(1);
    state = menu_state_current;
    state->images->screen.x = 0x2C0;
    state->images->screen.y = 0x100;
    state->images->screen.w = 0x140;
    state->images->screen.h = 0xE0;
    state->prims->width = 0x40;
    name_entry_init_party();
    name_entry_reset_buffer_index();
    name_entry_init_labels();
    name_entry_init_highlight_and_fade_prims();
    name_entry_read_sheet_entries();
    menu_state_current->drawing = 1;
    menu_state_current->sounds = 1;
    name_entry_run();
    name_entry_shut_down();
}
