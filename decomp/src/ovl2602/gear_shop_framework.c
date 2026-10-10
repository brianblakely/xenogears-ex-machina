/*
 * ovl2602 (Disc 1 slot 2602, Disc 2 slot 2597; loaded at 801c5000): the Gear
 * parts shop. Its entry (801ce024) sets up the same shop screen as ovl2601
 * (sell lists, buying with prices and gold, yes/no prompts) and adds the
 * Gear side: the game data's gear records and their part tables, a 3D model
 * of the chosen Gear drawn as actor 1 of the actor module ovl2143
 * (ovl2143/actors.h) with a second 400h-entry ordering table, the member
 * switch and the gear
 * screen block (menu state gear_screen). Functions shared with ovl2601 are
 * recovered from the same source; the ones that differ keep their own
 * versions here.
 *
 * This unit, the screen code and the entry, is the image's first: rodata
 * 801c5000-801c5038, text 801c511c-801ce1d0, data 801d697c-801d6d08 and its
 * variables 801d7108-801d904c. gear_shop.c follows; its head gives the
 * boundary evidence.
 */
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
#include "menu/shop.h"
#include "gear_shop.h"

const CardPrefix gear_shop_save_file_prefix = {"BISLPS-00800"}; /* 801C5000 */

/* The shared screen data. */
u8 gear_shop_debug_values_on = 0; /* 801D697C: the model values debug display (gear_shop_draw_debug_model_values) is on */
/* Command pictures: two per command. */
s32 gear_shop_command_images[8] = {0x109, 0x135, 0x10A, 0x136, 0x10B, 0x137, 0x10C, 0x143}; /* 801D6980 */
/* List pictures: four pairs (sprite, second layer) per command, 0xFFFF none. */
s32 gear_shop_choice_window_images[32] = { /* 801D69A0 */
    0x110, 0x144, 0x111, 0x145, 0x112, 0x146, 0x113, 0x147,
    0x110, 0x145, 0x111, 0x144, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x145, 0x111, 0x144, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x149, 0x111, 0x148, 0x112, 0x147, 0x113, 0x146,
};
/* List label text ids: commands, the sell lists (two sets of four), the buy
 * lists. */
u8 gear_shop_top_label_ids[4] = {0x7B, 0x7B, 0x7B, 0x7B}; /* 801D6A20 */
u8 gear_shop_choice_label_ids[8] = {0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B}; /* 801D6A24 */
u8 gear_shop_buy_label_ids[2] = {0x7C, 0x12}; /* 801D6A2C */
/* List label x offsets: commands, then the gear lists (two sets of four). */
s32 gear_shop_top_label_x_offsets[4] = {0x12, 0x12, 0x12, 0}; /* 801D6A30 */
s32 gear_shop_choice_label_x_offsets[8] = {0, 0x18, 0, 0, 0x18, 0xC, 0xC, 6}; /* 801D6A40 */
/* The four cursor markers' home positions. */
s32 gear_shop_marker_x_table[4] = {0, 0, 132, 228}; /* 801D6A60: x */
s32 gear_shop_marker_y_table[4] = {0, 0, 120, 120}; /* 801D6A70: y */
/* The four command labels' text ids. */
u8 gear_shop_command_label_ids[4] = {9, 10, 11, 12}; /* 801D6A80 */
/* File slot -> list position (positions 15 and 31 are skipped). */
s32 gear_shop_file_cursor_card_slots[30] = { /* 801D6A84 */
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
};
/* Marker position per list position. */
s32 gear_shop_file_slot_x_table[32] = { /* 801D6AFC: x */
    32, 40, 48, 56, 64, 72, 80, 88,
    96, 104, 112, 120, 128, 136, 144, 320,
    176, 184, 192, 200, 208, 216, 224, 232,
    240, 248, 256, 264, 272, 280, 288, 320,
};
s32 gear_shop_file_slot_y_table[32] = { /* 801D6B7C: y */
    14, 34, 54, 14, 34, 54, 14, 34,
    54, 14, 34, 54, 14, 34, 54, 256,
    14, 34, 54, 14, 34, 54, 14, 34,
    54, 14, 34, 54, 14, 34, 54, 256,
};
/* Cursor position per position. */
s32 gear_shop_highlight_x_table[9] = {73, 72, 67, 60, 37, 36, 31, 24, 16};          /* 801D6BFC: x */
s32 gear_shop_highlight_y_table[9] = {201, 181, 162, 144, 201, 181, 162, 144, 128}; /* 801D6C20: y */
/* Member portrait x by shown member. */
s32 gear_shop_portrait_x_table[9] = {72, 98, 124, 150, 176, 202, 228, 254, 280}; /* 801D6C44 */
/* Each member's party bit, as halfwords and as words. */
u16 gear_shop_bit_masks[16] = { /* 801D6C68 */
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
};
u32 gear_shop_bit_masks32[32] = { /* 801D6C88 */
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
    0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000,
    0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000,
};

/* The unit's own uninitialized variables, the first in the file after both
 * units' data (zero there), each in a slot of whole words (decomp/Makefile).
 * Only this unit's code reads them (tools/data_users.py). */
static POLY_FT4 gear_shop_debug_value_packets[200]; /* 801D7108: model values debug display packets, two per sprite */
static s32 gear_shop_debug_value_sprite_count;      /* 801D9048: their sprite count */

/* 801C511C: A random value in [min, max] (ffff stays ffff, a zero max gives 0). */
u16 gear_shop_pick_random_in_range(u16 min, u16 max) {
    s32 range;

    if (min == 0xFFFF) {
        return 0xFFFF;
    }
    if (max == 0) {
        return 0;
    }
    if (min == max) {
        return min;
    }
    range = max - min;
    if (range >= 0xFFFF) {
        return rand();
    }
    return min + (u16)rand() % (range + 1);
}

/* 801C51B8: Place a textured quad at (x, y) of size w x h showing texels (u, v)..(u + w, v + h). */
void gear_shop_quad_place(POLY_FT4 *poly, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h) {
    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->u0 = u;
    poly->u2 = u;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    poly->v0 = v;
    poly->u1 = u + w;
    poly->v1 = v;
    poly->v2 = v + h;
    poly->u3 = u + w;
    poly->v3 = v + h;
}

/* 801C5228: Test member `id`'s bit of a party bit mask. */
u16 gear_shop_test_bit(u16 mask, u8 id) {
    return gear_shop_bit_masks[id] & mask;
}

/* 801C5244: The party bit of member `id`. */
u16 gear_shop_get_bit_mask(u8 id) {
    return gear_shop_bit_masks[id];
}

/* 801C5260: The party bit of member `id`. */
u32 gear_shop_get_bit_mask32(u8 id) {
    return gear_shop_bit_masks32[id];
}

/* 801C527C: Test member `id`'s bit of a party bit mask. */
u32 gear_shop_test_bit32(u32 mask, u8 id) {
    return mask & gear_shop_bit_masks32[id];
}

/* 801C5298: Split `value` into nine decimal digits (menu state +31c), leading zeros blanked (ff). */
void gear_shop_split_digits(u32 value) {
    s32 i;
    u32 divisor;

    divisor = 100000000;
    for (i = 0; i < 9; i++) {
        menu_state_current->digits[i] = value / divisor;
        value %= divisor;
        divisor /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (menu_state_current->digits[i] != 0) {
            if (menu_state_current->digits[i - 1] == 0) {
                menu_state_current->digits[i - 1] = 0xFF;
            }
            break;
        }
        menu_state_current->digits[i - 1] = 0xFF;
    }
}

/* 801C5344: Allocate (nonzero) or release the card state block. */
void gear_shop_alloc_or_free_card_state(u8 allocate) {
    if (allocate) {
        menu_state_current->card = heap_alloc(sizeof(MenuCard), 0);
        bzero((u_char *)menu_state_current->card, sizeof(MenuCard));
    } else {
        heap_free(menu_state_current->card);
    }
}

/* 801C53A8: Allocate (nonzero) or release the screen flag block. */
void gear_shop_alloc_or_free_flags(u8 allocate) {
    if (allocate) {
        menu_state_current->flags = heap_alloc(sizeof(MenuFlags), 0);
        bzero((u_char *)menu_state_current->flags, sizeof(MenuFlags));
    } else {
        heap_free(menu_state_current->flags);
    }
}

/* 801C540C: Allocate (nonzero) or release the image packet block. */
void gear_shop_alloc_or_free_screen_images(u8 allocate) {
    if (allocate) {
        menu_state_current->images = heap_alloc(sizeof(MenuImages), 0);
        bzero((u_char *)menu_state_current->images, sizeof(MenuImages));
    } else {
        heap_free(menu_state_current->images);
    }
}

/* 801C5470: Allocate (nonzero) or release the list packet block. */
void gear_shop_alloc_or_free_sprite_lists(u8 allocate) {
    if (allocate) {
        menu_state_current->lists = heap_alloc(sizeof(MenuSpriteLists), 0);
        bzero((u_char *)menu_state_current->lists, sizeof(MenuSpriteLists));
    } else {
        heap_free(menu_state_current->lists);
    }
}

/* 801C54D4: Allocate (nonzero) or release the unpacked resource table. */
void gear_shop_alloc_or_free_table_directory(u8 allocate) {
    if (allocate) {
        menu_state_current->tables = heap_alloc(0xCC, 0);
        bzero((u_char *)menu_state_current->tables, 0xCC);
    } else {
        heap_free(menu_state_current->tables);
    }
}

/* 801C5538: Allocate (nonzero) or release the cursor block. */
void gear_shop_alloc_or_free_prims(u8 allocate) {
    if (allocate) {
        menu_state_current->prims = heap_alloc(sizeof(MenuPrims), 0);
        bzero((u_char *)menu_state_current->prims, sizeof(MenuPrims));
    } else {
        heap_free(menu_state_current->prims);
    }
}

/* 801C559C: Allocate (nonzero) or release the block at menu state +1e20. */
void gear_shop_alloc_or_free_name_entry_block(u8 allocate) {
    if (allocate) {
        menu_state_current->name_entry = heap_alloc(0xDEC, 0);
        bzero((u_char *)menu_state_current->name_entry, 0xDEC);
    } else {
        heap_free(menu_state_current->name_entry);
    }
}

/* 801C5600: Allocate (nonzero) or release the shop screen's packet block. */
void gear_shop_alloc_or_free_shop_details(u8 allocate) {
    if (allocate) {
        menu_state_current->details = heap_alloc(sizeof(ShopDetails), 0);
        bzero((u_char *)menu_state_current->details, sizeof(ShopDetails));
    } else {
        heap_free(menu_state_current->details);
    }
}

/* 801C5664: Allocate (nonzero) or release the 1f00h-byte block at menu state +454. */
void gear_shop_alloc_or_free_gear_screen(u8 allocate) {
    if (allocate) {
        menu_state_current->gear_screen = heap_alloc(0x1F00, 0);
        bzero((u_char *)menu_state_current->gear_screen, 0x1F00);
    } else {
        heap_free(menu_state_current->gear_screen);
    }
}

/* 801C56C8: Load the screen's resources: the card header (prefix, icon), text images, sprite sheet, labels, the party's portraits, the sound bank and the gear shop tables. */
void gear_shop_load_resources(void) {
    enum {
        ENTRY_UNUSED, ENTRY_MODE, ENTRY_CLUT_X, ENTRY_CLUT_Y,
        ENTRY_PAGE_X, ENTRY_PAGE_Y, ENTRY_WORDS
    };
    TIM_IMAGE tim;
    s32 entries[3 * ENTRY_WORDS];
    MenuResources *res;
    u32 *packed;
    s32 i;
    s32 id;

    res = menu_state_resource_file;
    text_relocate_offset_table(res);
    packed = text_unpack_lzss_alloc(res->files[0], 1);
    OpenTIM((u_long *)packed);
    ReadTIM(&menu_state_current->card->icon);
    *(CardPrefix *)menu_state_current->card->prefix = gear_shop_save_file_prefix;
    menu_state_current->card->save_magic[0] = 'S';
    menu_state_current->card->save_magic[1] = 'C';
    menu_state_current->card->save_icon_flag = 0x11;
    menu_state_current->card->save_blocks = 1;
    bzero(menu_state_current->card->save_title, sizeof(menu_state_current->card->save_title));
    memmove(menu_state_current->card->save_palette, menu_state_current->card->icon.caddr, 0x20);
    i = 0;
    memmove(menu_state_current->card->save_icon, menu_state_current->card->icon.paddr, 0x80);
    heap_free(packed);
    packed = text_unpack_lzss_alloc(res->files[1], 1);
    model_load_tim_list(packed);
    heap_free(packed);
    menu_state_current->sheet = text_unpack_lzss_alloc(res->files[2], 0);
    menu_state_current->label_text = text_unpack_lzss_alloc(res->files[3], 0);
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
    entries[ENTRY_WORDS + ENTRY_PAGE_X] += 0xC;
    packed = text_unpack_lzss_alloc(res->files[4], 1);
    for (; i < 3; i++) {
        id = menu_state_current->flags->party[i];
        if (id != 0xFF) {
            OpenTIM((u_long *)((u8 *)packed + id * 0xB20));
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
    heap_free(packed);
    if (menu_state_debug_start != 0) {
        cd_select_directory(0x10, 2);
        sound_effect_bank = heap_alloc(cd_get_aligned_file_size(5), 0);
        cd_read_file(5, sound_effect_bank, 0, 0x80);
        cd_sync_reads(0);
        cd_select_directory(0x10, 0);
        sound_add_effect_bank(sound_effect_bank);
    }
    menu_state_current->effects = sound_effect_bank;
    menu_state_current->gear_tables = text_unpack_lzss_alloc(res->files[7], 1);
    heap_free(res);
}

/* 801C5B08: Reset the screen state, note which party members are available (and selectable) and load the resources. */
void gear_shop_init_party(void) {
    u16 available;
    s32 i;
    s32 id;

    menu_state_current->cursor = 4;
    menu_state_current->cursor_shown = 0xFF;
    menu_state_current->card_poll_timer = 0x3C;
    menu_state_current->cards_present = 0;
    menu_state_current->unknown335 = 0;
    available = game_data.joined & game_data.available & 0x77F;
    for (i = 0; i < 16; i++) {
        if (gear_shop_test_bit(available, i) && game_data.characters[i].gearId != 0xFF) {
            menu_state_current->present[i] = 1;
        } else {
            menu_state_current->present[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        id = game_data.party[i];
        if (id != 0xFF && menu_state_current->present[id] != 0) {
            menu_state_current->flags->party[i] = id;
        } else {
            menu_state_current->flags->party[i] = 0xFF;
        }
    }
    gear_shop_load_resources();
}

/* 801C5C98: Start building draw buffer 0. */
void gear_shop_reset_buffer_index(void) {
    menu_state_current->buffer_index = 0;
}

/* 801C5CA8: Set up both buffers' quads of label `index` (two columns, 13-pixel rows
 * from `row`): mode 0 maps the text rendered for the command column;
 * otherwise the list layout, dimmed unless bit 7 is set, with the highlight
 * from the low bits. Old-style definition: mode arrives as a promoted int
 * and is narrowed where it is tested. */
void gear_shop_label_init_quads(label, index, row, mode)
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

/* 801C5EE8: Render `count` labels (text ids in pairs) into VRAM and set up their quads. */
void gear_shop_label_render_pairs(MenuLabel *labels, u8 *text_ids, s32 row, s32 count) {
    RECT *rect;
    s32 i;

    for (i = 0; i < count; i += 2) {
        labels[i].width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, text_ids[i]),
                                        menu_state_current->labels[0].pixels, 0x18, 0);
        labels[i + 1].width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, text_ids[i + 1]),
                                            menu_state_current->labels[0].pixels, 0x18, 1);
        rect = &labels[i].rect;
        rect->x = (((i / 2) & 1) << 5) + 0x140;
        rect->y = ((i + row) / 4) * 13;
        rect->w = 0x1C;
        rect->h = 13;
        labels[i + 1].rect = *rect;
        gear_shop_label_init_quads(&labels[i], i, row, 0);
        gear_shop_label_init_quads(&labels[i + 1], i + 1, row, 0);
        LoadImage(rect, (u_long *)menu_state_current->labels[0].pixels);
        DrawSync(0);
    }
}

/* 801C6098: Upload a 16-colour palette with only colour 1 set (7fff, white) at (0, 1c0). */
void gear_shop_upload_label_palette(void) {
    RECT rect;
    RECT unused; /* unused in the original; reserves 8 bytes */
    u16 *palette;

    palette = heap_alloc(0x20, 0);
    bzero((u_char *)palette, 0x20);
    palette[1] = 0x7FFF;
    rect.y = 0x1C0;
    rect.w = 0x10;
    rect.x = 0;
    rect.h = 1;
    LoadImage(&rect, (u_long *)palette);
    DrawSync(0);
    heap_free(palette);
}

/* 801C6114: Load the text palettes and render the four command labels. */
void gear_shop_init_labels(void) {
    text_load_palette(0, 0x1D1);
    menu_state_current->labels[0].pixels = heap_alloc(0x38E, 0);
    gear_shop_label_render_pairs(menu_state_current->labels, gear_shop_command_label_ids, 0, 4);
    gear_shop_upload_label_palette();
}

/* 801C6170: Look up the four sprite sheet entries the screen draws. */
void gear_shop_read_sheet_entries(void) {
    s32 unused[10]; /* unused in the original; reserves 40 bytes */
    MenuSheetEntry *e;

    e = &menu_state_current->sheet_entries[0];
    sprite_sheet_get_texture(menu_state_current->sheet, 0xFE, &e->first, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &menu_state_current->sheet_entries[1];
    sprite_sheet_get_texture(menu_state_current->sheet, 0x103, &e->first, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &menu_state_current->sheet_entries[2];
    sprite_sheet_get_texture(menu_state_current->sheet, 0x100, &e->first, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
    e = &menu_state_current->sheet_entries[3];
    sprite_sheet_get_texture(menu_state_current->sheet, 0x101, &e->first, &e->mode, &e->clut_x, &e->clut_y, &e->page_x, &e->page_y);
}

/* 801C6278: Draw the cursor at `position`; with `frame` also place its shade and edge lines. */
void gear_shop_highlight_place(s32 position, u8 frame) {
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, menu_state_current->prims, menu_state_current->buffer_index, gear_shop_highlight_x_table[position], gear_shop_highlight_y_table[position],
                  0x1000);
    menu_state_current->prims->sprite_buffer = menu_state_current->buffer_index;
    if (frame) {
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x0 = gear_shop_highlight_x_table[position] + 0x14;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y0 = gear_shop_highlight_y_table[position] - 0x24;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x1 = gear_shop_highlight_x_table[position] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y1 = gear_shop_highlight_y_table[position] - 0x24;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x2 = gear_shop_highlight_x_table[position] + 0x14;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y2 = gear_shop_highlight_y_table[position] - 0x14;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x3 = gear_shop_highlight_x_table[position] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y3 = gear_shop_highlight_y_table[position] - 0x14;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->x0 = gear_shop_highlight_x_table[position] + 0x14;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->y0 = gear_shop_highlight_y_table[position] - 0x24;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->x1 = gear_shop_highlight_x_table[position] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->y1 = gear_shop_highlight_y_table[position] - 0x24;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->x2 = gear_shop_highlight_x_table[position] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->y2 = gear_shop_highlight_y_table[position] - 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->x0 = gear_shop_highlight_x_table[position] + 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->y0 = gear_shop_highlight_y_table[position] - 0x24;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->x1 = gear_shop_highlight_x_table[position] + 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->y1 = gear_shop_highlight_y_table[position] - 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->x2 = gear_shop_highlight_x_table[position] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->y2 = gear_shop_highlight_y_table[position] - 0x14;
        menu_state_current->prims->shade_buffer = menu_state_current->buffer_index;
        menu_state_current->flags->cursor_shown = 1;
    }
}

/* 801C665C: Hide the cursor. */
void gear_shop_highlight_hide(void) {
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
}

/* 801C668C: Initialise a gouraud quad fading from (r, g, b) on the top edge to black on the bottom. */
void gear_shop_init_gradient_quad(POLY_G4 *poly, u8 r, u8 g, u8 b) {
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

/* 801C6708: Initialise both buffers' cursor shade, edge lines, screen quad and draw modes. */
void gear_shop_init_highlight_and_fade_prims(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    gear_shop_highlight_hide();
    for (i = 0; i < 2; i++) {
        gear_shop_init_gradient_quad(&menu_state_current->prims->shade[i], 0x80, 0x80, 0);
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

/* 801C6A54: Unpack (mode 0) or release (mode 10h) the item tables, pictures and gear part pictures from file 2. */
void gear_shop_load_or_release_data_set(u8 mode) {
    void **list;

    if (mode < 0x10) {
        list = heap_alloc(cd_get_aligned_file_size(2), 1);
        cd_read_file(2, list, 0, 0x80);
        cd_sync_reads(0);
        text_relocate_offset_table(list);
    }
    switch (mode) {
    case 0:
        menu_state_current->tables->engines = text_unpack_lzss_alloc(list[0x11], 0);
        menu_state_current->tables->parts = text_unpack_lzss_alloc(list[0x13], 0);
        menu_state_current->tables->frames = text_unpack_lzss_alloc(list[0x12], 0);
        menu_state_current->tables->gear_accessories = text_unpack_lzss_alloc(list[0x14], 0);
        menu_state_current->tables->gear_weapons = text_unpack_lzss_alloc(list[0x2B], 0);
        menu_state_current->details->resources[1] = text_unpack_lzss_alloc(list[0x33], 0);
        menu_state_current->details->resources[2] = text_unpack_lzss_alloc(list[0x34], 0);
        menu_state_current->details->resources[3] = text_unpack_lzss_alloc(list[0x30], 0);
        menu_state_current->details->resources[4] = text_unpack_lzss_alloc(list[0x31], 0);
        menu_state_current->details->resources[5] = text_unpack_lzss_alloc(list[0x32], 0);
        menu_state_current->details->resources[6] = text_unpack_lzss_alloc(list[0x2D], 0);
        menu_state_current->details->resources[7] = text_unpack_lzss_alloc(list[0x2E], 0);
        menu_state_current->details->resources[8] = text_unpack_lzss_alloc(list[0x2F], 0);
        break;
    case 0x10:
        heap_free(menu_state_current->tables->engines);
        heap_free(menu_state_current->tables->parts);
        heap_free(menu_state_current->tables->frames);
        heap_free(menu_state_current->tables->gear_accessories);
        heap_free(menu_state_current->tables->gear_weapons);
        heap_free(menu_state_current->details->resources[1]);
        heap_free(menu_state_current->details->resources[2]);
        heap_free(menu_state_current->details->resources[3]);
        heap_free(menu_state_current->details->resources[4]);
        heap_free(menu_state_current->details->resources[5]);
        heap_free(menu_state_current->details->resources[6]);
        heap_free(menu_state_current->details->resources[7]);
        heap_free(menu_state_current->details->resources[8]);
        break;
    }
    if (mode < 0x10) {
        heap_free(list);
    }
}

/* 801C6E74
 * Collect the shop's five part lists (20 ids each) and their counts from the
 * shop tables, release the tables, unpack the file-2 pictures, set up the
 * member bars and the gear screen's title sprite and backdrop.
 */
void gear_shop_init_stock(void) {
    u8 *entry;
    s32 i;
    s32 j;

    entry = menu_state_current->gear_tables + menu_state_screen_parameter * 0x64;
    i = 0;
    gear_shop_stock_list_counts[0] = 0;
    for (; i < 20; i++) {
        menu_state_current->details->stock[0][i] = *(entry + i + 0x14);
        if (menu_state_current->details->stock[0][i] != 0) {
            gear_shop_stock_list_counts[0]++;
        }
    }
    i = 0;
    gear_shop_stock_list_counts[1] = 0;
    for (; i < 20; i++) {
        menu_state_current->details->stock[1][i] = entry[i];
        if (menu_state_current->details->stock[1][i] != 0) {
            gear_shop_stock_list_counts[1]++;
        }
    }
    i = 0;
    gear_shop_stock_list_counts[2] = 0;
    for (; i < 20; i++) {
        menu_state_current->details->stock[2][i] = *(entry + i + 0x28);
        if (menu_state_current->details->stock[2][i] != 0) {
            gear_shop_stock_list_counts[2]++;
        }
    }
    i = 0;
    gear_shop_stock_list_counts[3] = 0;
    for (; i < 20; i++) {
        menu_state_current->details->stock[3][i] = *(entry + i + 0x50);
        if (menu_state_current->details->stock[3][i] != 0) {
            gear_shop_stock_list_counts[3]++;
        }
    }
    i = 0;
    gear_shop_stock_list_counts[4] = 0;
    for (; i < 20; i++) {
        menu_state_current->details->stock[4][i] = *(entry + i + 0x3C);
        if (menu_state_current->details->stock[4][i] != 0) {
            gear_shop_stock_list_counts[4]++;
        }
    }
    heap_free(menu_state_current->gear_tables);
    gear_shop_load_or_release_data_set(0);
    for (j = 0; j < 9; j++) {
        for (i = 0; i < 2; i++) {
            SetLineF3(menu_state_current->details->bar_upper + (j * 2 + i));
            (menu_state_current->details->bar_upper + (j * 2 + i))->r0 = 0xFF;
            (menu_state_current->details->bar_upper + (j * 2 + i))->g0 = 0;
            (menu_state_current->details->bar_upper + (j * 2 + i))->b0 = 0;
            SetLineF3(menu_state_current->details->bar_lower + (j * 2 + i));
            (menu_state_current->details->bar_lower + (j * 2 + i))->r0 = 0xFF;
            (menu_state_current->details->bar_lower + (j * 2 + i))->g0 = 0;
            (menu_state_current->details->bar_lower + (j * 2 + i))->b0 = 0;
            (menu_state_current->details->bar_upper + (j * 2 + i))->x0 = gear_shop_portrait_x_table[j];
            (menu_state_current->details->bar_upper + (j * 2 + i))->y0 = 0xA6;
            (menu_state_current->details->bar_upper + (j * 2 + i))->x1 = gear_shop_portrait_x_table[j] + 0x18;
            (menu_state_current->details->bar_upper + (j * 2 + i))->y1 = 0xA6;
            (menu_state_current->details->bar_upper + (j * 2 + i))->x2 = gear_shop_portrait_x_table[j] + 0x18;
            (menu_state_current->details->bar_upper + (j * 2 + i))->y2 = 0xBC;
            (menu_state_current->details->bar_lower + (j * 2 + i))->x0 = gear_shop_portrait_x_table[j];
            (menu_state_current->details->bar_lower + (j * 2 + i))->y0 = 0xA6;
            (menu_state_current->details->bar_lower + (j * 2 + i))->x1 = gear_shop_portrait_x_table[j];
            (menu_state_current->details->bar_lower + (j * 2 + i))->y1 = 0xBC;
            (menu_state_current->details->bar_lower + (j * 2 + i))->x2 = gear_shop_portrait_x_table[j] + 0x18;
            (menu_state_current->details->bar_lower + (j * 2 + i))->y2 = 0xBC;
        }
        menu_state_current->details->bar_shown[j] = 0;
    }
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x166, menu_state_current->gear_screen->packets, menu_state_current->buffer_index, 0x108, 0x18,
                  0x1000);
    for (j = 0; j < 2; j++) {
        SetPolyFT4((menu_state_current->gear_screen->backdrop + j));
        SetSemiTrans((menu_state_current->gear_screen->backdrop + j), 0);
        SetShadeTex((menu_state_current->gear_screen->backdrop + j), 0);
        (menu_state_current->gear_screen->backdrop + j)->r0 = 0x80;
        (menu_state_current->gear_screen->backdrop + j)->g0 = 0x80;
        (menu_state_current->gear_screen->backdrop + j)->b0 = 0x80;
        menu_state_current->gear_screen->backdrop[j].tpage = GetTPage(0, 0, 0x180, 0);
        menu_state_current->gear_screen->backdrop[j].clut = text_plane0_clut;
        (menu_state_current->gear_screen->backdrop + j)->u0 = 0;
        (menu_state_current->gear_screen->backdrop + j)->v0 = 0x48;
        (menu_state_current->gear_screen->backdrop + j)->u1 = 0x60;
        (menu_state_current->gear_screen->backdrop + j)->v1 = 0x48;
        (menu_state_current->gear_screen->backdrop + j)->u2 = 0;
        (menu_state_current->gear_screen->backdrop + j)->v2 = 0x55;
        (menu_state_current->gear_screen->backdrop + j)->u3 = 0x60;
        (menu_state_current->gear_screen->backdrop + j)->v3 = 0x55;
        (menu_state_current->gear_screen->backdrop + j)->x0 = 0x10;
        (menu_state_current->gear_screen->backdrop + j)->y0 = 0x20;
        (menu_state_current->gear_screen->backdrop + j)->x1 = 0x70;
        (menu_state_current->gear_screen->backdrop + j)->y1 = 0x20;
        (menu_state_current->gear_screen->backdrop + j)->x2 = 0x10;
        (menu_state_current->gear_screen->backdrop + j)->y2 = 0x2D;
        (menu_state_current->gear_screen->backdrop + j)->x3 = 0x70;
        (menu_state_current->gear_screen->backdrop + j)->y3 = 0x2D;
    }
}

/* 801C7604: Set a quad's four corners for the rectangle (x, y, w, h), centred on the screen. */
void gear_shop_set_rect_verts(SVECTOR *quad, u16 x, u16 y, u16 w, u16 h) {
    quad[0].vx = x - 0xA0;
    quad[0].vy = y - 0x70;
    quad[0].vz = 0;
    quad[1].vx = x + w - 0xA0;
    quad[1].vy = y - 0x70;
    quad[1].vz = 0;
    quad[2].vx = x - 0xA0;
    quad[2].vy = y + h - 0x70;
    quad[2].vz = 0;
    quad[3].vx = x + w - 0xA0;
    quad[3].vy = y + h - 0x70;
    quad[3].vz = 0;
}

/* 801C765C: Make a textured quad semi-transparent, unshaded and neutral grey. */
void gear_shop_quad_set_semi_transparent(POLY_FT4 *poly) {
    SetSemiTrans(poly, 1);
    SetShadeTex(poly, 0);
    poly->r0 = 0x80;
    poly->g0 = 0x80;
    poly->b0 = 0x80;
}

/* 801C76A4: Draw the scroll bar at (x, y): the thumb position follows `top` of `count` rows. */
void gear_shop_scroll_bar_show(s32 x, s32 y, s32 height, s32 count, s32 top) {
    s32 offset;

    offset = 0;
    if (menu_state_current->flags->scroll_shown == 0) {
        menu_state_current->scroll = heap_alloc(sizeof(MenuScrollBar), 0);
        bzero((u_char *)menu_state_current->scroll, sizeof(MenuScrollBar));
    }
    if (count < 9) {
        height = 100;
    } else {
        offset = (top * 100) / (count - 8);
        offset = offset * 4000 / 10000;
    }
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x107, menu_state_current->scroll, menu_state_current->buffer_index, x, y,
                  0x1000);
    gear_shop_set_rect_verts(menu_state_current->scroll->verts, x, y + offset, 8, height);
    menu_state_current->scroll->buffer = menu_state_current->buffer_index;
    menu_state_current->flags->scroll_shown = 1;
}

/* 801C782C: Remove the scroll bar. */
void gear_shop_scroll_bar_hide(void) {
    menu_state_current->flags->scroll_shown = 0;
    heap_free(menu_state_current->scroll);
}

/* 801C7870: Create marker `index`, starting on its first frame. */
void gear_shop_list_cursor_alloc(u8 index) {
    menu_state_current->cursors[index] = heap_alloc(sizeof(MenuCursor), 0);
    bzero((u_char *)menu_state_current->cursors[index], sizeof(MenuCursor));
    menu_state_current->cursors[index]->frame = 4;
    menu_state_current->cursors[index]->timer = 0;
}

/* 801C78EC: Animate marker `index` and draw it beside row `row` (at its previous place when `fixed`). */
void gear_shop_list_cursor_place(s32 row, s32 unused, u8 fixed, u8 index) {
    MenuCursor *marker;
    POLY_FT4 *poly;
    s32 visible;
    s32 y;

    marker = menu_state_current->cursors[index];
    if (++marker->timer >= 6) {
        if (--marker->frame < 0) {
            marker->frame = 4;
        }
        marker->timer = 0;
    }
    if (fixed) {
        visible = 1;
    } else {
        y = row * 13 + 0x32;
        visible = 1;
    }
    if (visible) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, marker->frame + 0x15B, marker, menu_state_current->buffer_index, 0,
                      0, 0x1000);
        poly = &marker->polys[menu_state_current->buffer_index];
        gear_shop_set_rect_verts(marker->verts, poly->x0 + 0x1C, poly->y0 + y, poly->x1 - poly->x0,
                      poly->y3 - poly->y0);
        marker->buffer = menu_state_current->buffer_index;
        menu_state_current->flags->cursors_shown[index] = 1;
    } else {
        menu_state_current->flags->cursors_shown[index] = 0;
    }
}

/* 801C7A88: Remove marker `index`. */
void gear_shop_list_cursor_free(u8 index) {
    heap_free(menu_state_current->cursors[index]);
    menu_state_current->flags->cursors_shown[index] = 0;
}

/* 801C7AE4: Initialise panel `index`'s background, draw modes and edge sprite parts. */
void gear_shop_panel_init(u8 index) {
    MenuPanel *panel;
    RECT window;
    u8 i;

    panel = menu_state_current->panels[index];
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
                      GetTPage(0, 0, menu_state_current->sheet_entries[0].page_x, menu_state_current->sheet_entries[0].page_y),
                      &window);
    }
    for (i = 0; i < 4; i++) {
        SetPolyFT4(&panel->edge[0][i]);
        SetShadeTex(&panel->edge[0][i], 1);
        (panel->edge[0] + i)->r0 = 0xFF;
        (panel->edge[0] + i)->g0 = 0xFF;
        (panel->edge[0] + i)->b0 = 0xFF;
        (panel->edge[0] + i)->tpage = GetTPage(menu_state_current->sheet_entries[0].mode, 0,
                                                      menu_state_current->sheet_entries[0].page_x,
                                                      menu_state_current->sheet_entries[0].page_y);
        (panel->edge[0] + i)->clut = GetClut(menu_state_current->sheet_entries[0].clut_x,
                                                     menu_state_current->sheet_entries[0].clut_y);
        SetPolyFT4(&panel->edge[1][i]);
        SetShadeTex(&panel->edge[1][i], 1);
        (panel->edge[1] + i)->r0 = 0xFF;
        (panel->edge[1] + i)->g0 = 0xFF;
        (panel->edge[1] + i)->b0 = 0xFF;
        (panel->edge[1] + i)->tpage = GetTPage(menu_state_current->sheet_entries[1].mode, 0,
                                                       menu_state_current->sheet_entries[1].page_x,
                                                       menu_state_current->sheet_entries[1].page_y);
        (panel->edge[1] + i)->clut = GetClut(menu_state_current->sheet_entries[1].clut_x,
                                                      menu_state_current->sheet_entries[1].clut_y);
        SetPolyFT4(&panel->edge[2][i]);
        SetShadeTex(&panel->edge[2][i], 1);
        (panel->edge[2] + i)->r0 = 0xFF;
        (panel->edge[2] + i)->g0 = 0xFF;
        (panel->edge[2] + i)->b0 = 0xFF;
        (panel->edge[2] + i)->tpage = GetTPage(menu_state_current->sheet_entries[2].mode, 0,
                                                       menu_state_current->sheet_entries[2].page_x,
                                                       menu_state_current->sheet_entries[2].page_y);
        (panel->edge[2] + i)->clut = GetClut(menu_state_current->sheet_entries[2].clut_x,
                                                      menu_state_current->sheet_entries[2].clut_y);
        SetPolyFT4(&panel->edge[3][i]);
        SetShadeTex(&panel->edge[3][i], 1);
        (panel->edge[3] + i)->r0 = 0xFF;
        (panel->edge[3] + i)->g0 = 0xFF;
        (panel->edge[3] + i)->b0 = 0xFF;
        (panel->edge[3] + i)->tpage = GetTPage(menu_state_current->sheet_entries[3].mode, 0,
                                                       menu_state_current->sheet_entries[3].page_x,
                                                       menu_state_current->sheet_entries[3].page_y);
        (panel->edge[3] + i)->clut = GetClut(menu_state_current->sheet_entries[3].clut_x,
                                                      menu_state_current->sheet_entries[3].clut_y);
    }
}

/* 801C7E00: Place panel `index`'s scroll bar (top arrow, bottom arrow, track) at its right edge. */
void gear_shop_panel_layout_scroll_bar(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x105, &panel->bar_ends[0], menu_state_current->buffer_index, x, y, 0x1000);
    sprite_sheet_draw_scaled_flip(menu_state_current->sheet, 0x105, &panel->bar_ends[2], menu_state_current->buffer_index, x,
                  y + h - 8, 0x1000, 0, 1);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x106, &panel->bar_side[0], menu_state_current->buffer_index, x, y + 8,
                  0x1000);
    gear_shop_set_rect_verts(&panel->ends_at[0], x, y, 8, 8);
    gear_shop_set_rect_verts(&panel->ends_at[4], x, y + h, 8, -8);
    gear_shop_set_rect_verts(&panel->side_at[0], x, y + 8, 8, h - 8);
}

/* 801C7F64: Build panel `index`'s four corners around (x, y, w, h). */
void gear_shop_panel_layout_corners(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel;
    s32 i;

    panel = menu_state_current->panels[index];
    panel->corner_parts = 0;
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0xFD, &panel->corner[panel->corner_parts * 2],
                                       menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0xFF, &panel->corner[panel->corner_parts * 2],
                                       menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0x102, &panel->corner[panel->corner_parts * 2],
                                       menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0x104, &panel->corner[panel->corner_parts * 2],
                                       menu_state_current->buffer_index, 0, 0, 0x1000);
    gear_shop_set_rect_verts(&panel->corner_at[0], x - 8, y + 8, 0x10, -0x10);
    gear_shop_set_rect_verts(&panel->corner_at[4], x + w + 8, y + 8, -0x10, -0x10);
    gear_shop_set_rect_verts(&panel->corner_at[8], x - 8, y + h - 8, 0x10, 0x10);
    gear_shop_set_rect_verts(&panel->corner_at[12], x + w + 8, y + h - 8, -0x10, 0x10);
    for (i = 0; i < 4; i++) {
        gear_shop_quad_set_semi_transparent(&panel->corner[i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C81AC: Build panel `index`'s top edge, two pieces across the width. */
void gear_shop_panel_layout_top_edge(u8 index, u16 x, u16 y, u16 w) {
    MenuPanel *panel;
    s32 half;
    s32 i;

    panel = menu_state_current->panels[index];
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
    half = (w - 0x10) / 2;
    gear_shop_set_rect_verts(&panel->edge_at[0][0][0], x + 8, y - 8, half, 0x10);
    gear_shop_set_rect_verts(&panel->edge_at[0][1][0], x + (half + 8), y - 8, half, 0x10);
    for (i = 0; i < 2; i++) {
        gear_shop_quad_set_semi_transparent(&panel->edge[0][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C84F0: Build panel `index`'s bottom edge, two pieces across the width. */
void gear_shop_panel_layout_bottom_edge(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel;
    s32 half;
    s32 i;

    panel = menu_state_current->panels[index];
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
    half = (w - 0x10) / 2;
    gear_shop_set_rect_verts(&panel->edge_at[1][0][0], x + 8, y + h - 8, half, 0x10);
    gear_shop_set_rect_verts(&panel->edge_at[1][1][0], x + (half + 8), y + h - 8, half, 0x10);
    for (i = 0; i < 2; i++) {
        gear_shop_quad_set_semi_transparent(&panel->edge[1][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C883C: Build panel `index`'s left edge, two pieces down the height. */
void gear_shop_panel_layout_left_edge(u8 index, u16 x, u16 y, u16 h) {
    MenuPanel *panel;
    s32 half;
    s32 i;

    panel = menu_state_current->panels[index];
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
    half = (h - 0x10) / 2;
    gear_shop_set_rect_verts(&panel->edge_at[2][0][0], x - 8, y + 8, 0x10, half);
    gear_shop_set_rect_verts(&panel->edge_at[2][1][0], x - 8, y + (half + 8), 0x10, half);
    for (i = 0; i < 2; i++) {
        gear_shop_quad_set_semi_transparent(&panel->edge[2][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C8B84: Build panel `index`'s right edge, two pieces down the height. */
void gear_shop_panel_layout_right_edge(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel;
    s32 half;
    s32 i;

    panel = menu_state_current->panels[index];
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
    half = (h - 0x10) / 2;
    gear_shop_set_rect_verts(&panel->edge_at[3][0][0], x + w - 8, y + 8, 0x10, half);
    gear_shop_set_rect_verts(&panel->edge_at[3][1][0], x + w - 8, y + (half + 8), 0x10, half);
    for (i = 0; i < 2; i++) {
        gear_shop_quad_set_semi_transparent(&panel->edge[3][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801C8ED0: Lay out panel `index` at (x, y, w, h) for this buffer and mark it shown. */
void gear_shop_panel_layout(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar) {
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    menu_state_current->flags->panels_shown[index] = 0;
    gear_shop_set_rect_verts(&panel->fill_at[0], x, y, w, h);
    gear_shop_panel_layout_corners(index, x, y, w, h);
    gear_shop_panel_layout_top_edge(index, x, y, w);
    gear_shop_panel_layout_bottom_edge(index, x, y, w, h);
    gear_shop_panel_layout_left_edge(index, x, y, h);
    gear_shop_panel_layout_right_edge(index, x, y, w, h);
    if (has_bar) {
        gear_shop_panel_layout_scroll_bar(index, x, y, w, h);
    }
    panel->has_bar = has_bar;
    panel->flat = flat;
    panel->ot_entry = ot_entry;
    panel->buffer = menu_state_current->buffer_index;
    menu_state_current->flags->panels_shown[index] = 1;
}

/* 801C9054: Remove panel `index`. */
void gear_shop_panel_close(u8 index) {
    menu_state_current->flags->panels_shown[index] = 0;
    menu_state_current->flags->panels_growing[index] = 0;
    heap_free(menu_state_current->panels[index]);
    heap_free(menu_state_current->growth[index]);
}

/* 801C90E0: Open panel `index` at (x, y, w, h): at once, or growing from its centre when `grow`. */
void gear_shop_panel_open(u8 index, s16 x, s16 y, s16 w, u16 h, u8 grow, u8 flat, s32 ot_entry,
                   u8 has_bar) {
    MenuGrowth *growth;

    if (index >= 2) {
        menu_state_current->panels[index] = heap_alloc(sizeof(MenuPanel), 0);
        bzero((u_char *)menu_state_current->panels[index], sizeof(MenuPanel));
        menu_state_current->growth[index] = heap_alloc(sizeof(MenuGrowth), 0);
        bzero((u_char *)menu_state_current->growth[index], sizeof(MenuGrowth));
        gear_shop_panel_init(index);
    }
    growth = menu_state_current->growth[index];
    if (grow) {
        growth->index = index;
        growth->done = 0;
        growth->x = x;
        growth->y = y;
        growth->w = w;
        growth->h = h;
        growth->cur_w = 0;
        growth->cur_h = 0;
        menu_state_current->flags->panels_growing[index] = 1;
        growth->flat = flat;
        growth->ot_entry = ot_entry;
    } else {
        gear_shop_panel_layout(index, x, y, w, h, flat, ot_entry, has_bar);
    }
}

/* 801C9264: Grow each opening panel by 20h in both directions until it reaches its size. */
void gear_shop_panel_grow_opening(void) {
    MenuGrowth *growth;
    s32 i;
    u8 finished;

    for (i = 0; i < 7; i++) {
        growth = menu_state_current->growth[i];
        if (menu_state_current->flags->panels_growing[i] != 0 && growth->done == 0) {
            finished = 0;
            if (growth->cur_w + 0x20 >= growth->w) {
                growth->cur_w = growth->w;
                finished = 1;
            } else {
                growth->cur_w += 0x20;
            }
            if (growth->cur_h + 0x20 >= growth->h) {
                growth->cur_h = growth->h;
                finished++;
            } else {
                growth->cur_h += 0x20;
            }
            if (finished == 2) {
                growth->done = 1;
            }
            gear_shop_panel_layout(growth->index, growth->x + growth->w / 2 - growth->cur_w / 2,
                          growth->y + growth->h / 2 - growth->cur_h / 2, growth->cur_w,
                          growth->cur_h, growth->flat, growth->ot_entry, growth->has_bar);
        }
    }
}

/* 801C93B0: Project `count` quads and link their packets (every other one from `first`) into OT entry 4. */
void gear_shop_draw_projected_quads(s32 count, SVECTOR *quads, POLY_FT4 *packets, s32 first) {
    long depth;
    long flag;
    s32 i;

    for (i = 0; i < count; i++) {
        RotTransPers4(&quads[i * 4], &quads[i * 4 + 1], &quads[i * 4 + 2], &quads[i * 4 + 3],
                      (long *)&packets[first + i * 2].x0, (long *)&packets[first + i * 2].x1,
                      (long *)&packets[first + i * 2].x2, (long *)&packets[first + i * 2].x3,
                      &depth, &flag);
        AddPrim(&menu_state_current->current->ot[4], &packets[first + i * 2]);
    }
}

/* 801C94CC: Link `count` packets (every other one from `first`) into OT entry 4. */
void gear_shop_draw_quads(s32 count, POLY_FT4 *packets, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        AddPrim(&menu_state_current->current->ot[4], &packets[first + i * 2]);
    }
}

/* 801C9550: Draw the scroll bar when shown. */
void gear_shop_draw_scroll_bar(void) {
    MenuScrollBar *scroll;

    if (menu_state_current->flags->scroll_shown != 0) {
        scroll = menu_state_current->scroll;
        gear_shop_draw_projected_quads(1, scroll->verts, scroll->polys, scroll->buffer);
    }
}

/* 801C959C: Draw the cursor's label draw mode, and the cursor when shown. */
void gear_shop_draw_highlight_sprite(void) {
    AddPrim(&menu_state_current->current->ot[4],
                  &menu_state_current->prims->mode_label[menu_state_current->prims->shade_buffer]);
    if (menu_state_current->flags->sprite_shown != 0) {
        AddPrim(&menu_state_current->current->ot[4],
                      &menu_state_current->prims->sprite[menu_state_current->prims->sprite_buffer]);
    }
}

/* 801C962C: Project and link the four second markers when shown and at least two members are available. */
void gear_shop_draw_member_marks(void) {
    if (menu_state_current->flags->marks_shown != 0 && gear_shop_available_member_count >= 2) {
        gear_shop_draw_projected_quads(4, menu_state_current->marks->verts, menu_state_current->marks->polys, menu_state_current->marks->buffer);
    }
}

/* 801C9690: Project and link panel `index`'s top edge. */
void gear_shop_draw_panel_top_edge(s32 index) {
    long depth;
    long flag;
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    RotTransPers4(&panel->edge_at[0][0][0], &panel->edge_at[0][0][1], &panel->edge_at[0][0][2],
                  &panel->edge_at[0][0][3], (long *)&panel->edge[0][panel->buffer].x0,
                  (long *)&panel->edge[0][panel->buffer].x1,
                  (long *)&panel->edge[0][panel->buffer].x2,
                  (long *)&panel->edge[0][panel->buffer].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[0][panel->buffer]);
    RotTransPers4(&panel->edge_at[0][1][0], &panel->edge_at[0][1][1], &panel->edge_at[0][1][2],
                  &panel->edge_at[0][1][3], (long *)&panel->edge[0][panel->buffer + 2].x0,
                  (long *)&panel->edge[0][panel->buffer + 2].x1,
                  (long *)&panel->edge[0][panel->buffer + 2].x2,
                  (long *)&panel->edge[0][panel->buffer + 2].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[0][panel->buffer + 2]);
}

/* 801C9864: Project and link panel `index`'s bottom edge. */
void gear_shop_draw_panel_bottom_edge(s32 index) {
    long depth;
    long flag;
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    RotTransPers4(&panel->edge_at[1][0][0], &panel->edge_at[1][0][1], &panel->edge_at[1][0][2],
                  &panel->edge_at[1][0][3], (long *)&panel->edge[1][panel->buffer].x0,
                  (long *)&panel->edge[1][panel->buffer].x1,
                  (long *)&panel->edge[1][panel->buffer].x2,
                  (long *)&panel->edge[1][panel->buffer].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[1][panel->buffer]);
    RotTransPers4(&panel->edge_at[1][1][0], &panel->edge_at[1][1][1], &panel->edge_at[1][1][2],
                  &panel->edge_at[1][1][3], (long *)&panel->edge[1][panel->buffer + 2].x0,
                  (long *)&panel->edge[1][panel->buffer + 2].x1,
                  (long *)&panel->edge[1][panel->buffer + 2].x2,
                  (long *)&panel->edge[1][panel->buffer + 2].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[1][panel->buffer + 2]);
}

/* 801C9A38: Project and link panel `index`'s left edge. */
void gear_shop_draw_panel_left_edge(s32 index) {
    long depth;
    long flag;
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    RotTransPers4(&panel->edge_at[2][0][0], &panel->edge_at[2][0][1], &panel->edge_at[2][0][2],
                  &panel->edge_at[2][0][3], (long *)&panel->edge[2][panel->buffer].x0,
                  (long *)&panel->edge[2][panel->buffer].x1,
                  (long *)&panel->edge[2][panel->buffer].x2,
                  (long *)&panel->edge[2][panel->buffer].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[2][panel->buffer]);
    RotTransPers4(&panel->edge_at[2][1][0], &panel->edge_at[2][1][1], &panel->edge_at[2][1][2],
                  &panel->edge_at[2][1][3], (long *)&panel->edge[2][panel->buffer + 2].x0,
                  (long *)&panel->edge[2][panel->buffer + 2].x1,
                  (long *)&panel->edge[2][panel->buffer + 2].x2,
                  (long *)&panel->edge[2][panel->buffer + 2].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[2][panel->buffer + 2]);
}

/* 801C9C0C: Project and link panel `index`'s right edge. */
void gear_shop_draw_panel_right_edge(s32 index) {
    long depth;
    long flag;
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    RotTransPers4(&panel->edge_at[3][0][0], &panel->edge_at[3][0][1], &panel->edge_at[3][0][2],
                  &panel->edge_at[3][0][3], (long *)&panel->edge[3][panel->buffer].x0,
                  (long *)&panel->edge[3][panel->buffer].x1,
                  (long *)&panel->edge[3][panel->buffer].x2,
                  (long *)&panel->edge[3][panel->buffer].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[3][panel->buffer]);
    RotTransPers4(&panel->edge_at[3][1][0], &panel->edge_at[3][1][1], &panel->edge_at[3][1][2],
                  &panel->edge_at[3][1][3], (long *)&panel->edge[3][panel->buffer + 2].x0,
                  (long *)&panel->edge[3][panel->buffer + 2].x1,
                  (long *)&panel->edge[3][panel->buffer + 2].x2,
                  (long *)&panel->edge[3][panel->buffer + 2].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->edge[3][panel->buffer + 2]);
}

/* 801C9DE0: Project and link panel `index`'s background and its draw mode. */
void gear_shop_draw_panel_fill(s32 index) {
    long depth;
    long flag;
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    RotTransPers4(&panel->fill_at[0], &panel->fill_at[1], &panel->fill_at[2], &panel->fill_at[3],
                  (long *)&(panel->fill + panel->buffer)->x0,
                  (long *)&(panel->fill + panel->buffer)->x1,
                  (long *)&(panel->fill + panel->buffer)->x2,
                  (long *)&(panel->fill + panel->buffer)->x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->fill[panel->buffer]);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->fill_mode[panel->buffer]);
}

/* 801C9F1C: Project and link panel `index`'s four corners. */
void gear_shop_draw_panel_corners(s32 index) {
    long depth;
    long flag;
    MenuPanel *panel;
    s32 i;

    panel = menu_state_current->panels[index];
    for (i = 0; i < 4; i++) {
        RotTransPers4(&panel->corner_at[i * 4], &panel->corner_at[i * 4 + 1],
                      &panel->corner_at[i * 4 + 2], &panel->corner_at[i * 4 + 3],
                      (long *)&(panel->corner + (i * 2 + panel->buffer))->x0,
                      (long *)&(panel->corner + (i * 2 + panel->buffer))->x1,
                      (long *)&(panel->corner + (i * 2 + panel->buffer))->x2,
                      (long *)&(panel->corner + (i * 2 + panel->buffer))->x3, &depth, &flag);
        AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->corner[i * 2 + panel->buffer]);
    }
}

/* 801CA068: Project and link panel `index`'s scroll bar: both arrows, then the track. */
void gear_shop_draw_panel_scroll_bar(s32 index) {
    long depth;
    long flag;
    MenuPanel *panel;
    s32 i;

    panel = menu_state_current->panels[index];
    for (i = 0; i < 2; i++) {
        RotTransPers4(&panel->ends_at[i * 4], &panel->ends_at[i * 4 + 1],
                      &panel->ends_at[i * 4 + 2], &panel->ends_at[i * 4 + 3],
                      (long *)&panel->bar_ends[i * 2 + panel->buffer].x0,
                      (long *)&panel->bar_ends[i * 2 + panel->buffer].x1,
                      (long *)&panel->bar_ends[i * 2 + panel->buffer].x2,
                      (long *)&panel->bar_ends[i * 2 + panel->buffer].x3, &depth, &flag);
        AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->bar_ends[i * 2 + panel->buffer]);
    }
    RotTransPers4(&panel->side_at[0], &panel->side_at[1], &panel->side_at[2], &panel->side_at[3],
                  (long *)&panel->bar_side[panel->buffer].x0,
                  (long *)&panel->bar_side[panel->buffer].x1,
                  (long *)&panel->bar_side[panel->buffer].x2,
                  (long *)&panel->bar_side[panel->buffer].x3, &depth, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->bar_side[panel->buffer]);
}

/* 801CA28C: Draw every shown panel; panels that are not flat get their own 3D matrices. */
void gear_shop_draw_panels(void) {
    SVECTOR rotation;
    VECTOR translation;
    MATRIX matrix;
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    MenuPanel *panel;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (menu_state_current->flags->panels_shown[i] != 0) {
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
                gear_shop_draw_panel_corners(i);
                if (panel->has_bar) {
                    gear_shop_draw_panel_scroll_bar(i);
                }
                gear_shop_draw_panel_top_edge(i);
                gear_shop_draw_panel_bottom_edge(i);
                gear_shop_draw_panel_left_edge(i);
                gear_shop_draw_panel_right_edge(i);
                gear_shop_draw_panel_fill(i);
                PopMatrix();
            } else {
                gear_shop_draw_panel_corners(i);
                if (panel->has_bar) {
                    gear_shop_draw_panel_scroll_bar(i);
                }
                gear_shop_draw_panel_top_edge(i);
                gear_shop_draw_panel_bottom_edge(i);
                gear_shop_draw_panel_left_edge(i);
                gear_shop_draw_panel_right_edge(i);
                gear_shop_draw_panel_fill(i);
            }
        }
    }
}

/* 801CA404: Link the shown markers; markers that follow the file cursor move to its slot first. */
void gear_shop_draw_markers(void) {
    s32 i;

    if (menu_state_current->flags->markers_shown != 0) {
        for (i = 0; i < 4; i++) {
            if (menu_state_current->markers->shown[i] != 0) {
                if (menu_state_current->markers->at_cursor[i] != 0) {
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x0 =
                        gear_shop_file_slot_x_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] + 8;
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y0 =
                        gear_shop_file_slot_y_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] - 6;
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x1 =
                        gear_shop_file_slot_x_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] + 0x18;
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y1 =
                        gear_shop_file_slot_y_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] - 6;
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x2 =
                        gear_shop_file_slot_x_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] + 8;
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y2 =
                        gear_shop_file_slot_y_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] + 0xA;
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x3 =
                        gear_shop_file_slot_x_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] + 0x18;
                    (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y3 =
                        gear_shop_file_slot_y_table[gear_shop_file_cursor_card_slots[menu_state_current->card->cursor]] + 0xA;
                }
                AddPrim(&menu_state_current->current->ot[4],
                              menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]));
            }
        }
    }
}

/* 801CA754: Link the shown command labels. */
void gear_shop_draw_command_labels(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (menu_state_current->flags->labels_shown[i] != 0) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->labels[i].polys[menu_state_current->labels[i].buffer]);
        }
    }
}

/* 801CA7E4: Link the shown list labels. */
void gear_shop_draw_list_labels(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (menu_state_current->flags->list_labels_shown[i] != 0) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->list_labels[i].polys[menu_state_current->list_labels[i].buffer]);
        }
    }
}

/* 801CA874: Link the shown info labels, projecting the 3D ones first. */
void gear_shop_draw_row_labels(void) {
    long depth;
    long flag;
    s32 i;

    for (i = 0; i < 6; i++) {
        if (menu_state_current->flags->row_labels_shown[i] != 0) {
            if (menu_state_current->row_labels[i].projected) {
                RotTransPers4(&menu_state_current->row_labels[i].verts[0],
                              &menu_state_current->row_labels[i].verts[1],
                              &menu_state_current->row_labels[i].verts[2],
                              &menu_state_current->row_labels[i].verts[3],
                              (long *)&menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer].x0,
                              (long *)&menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer].x1,
                              (long *)&menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer].x2,
                              (long *)&menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer].x3,
                              &depth, &flag);
                AddPrim(&menu_state_current->current->ot[4], &menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer]);
            } else {
                AddPrim(&menu_state_current->current->ot[4], &menu_state_current->row_labels[i].polys[menu_state_current->row_labels[i].buffer]);
            }
        }
    }
}

/* 801CA9EC: Link the shown extra labels. */
void gear_shop_draw_extra_labels(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (menu_state_current->flags->extra_labels_shown[i] != 0) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->extra_labels[i].polys[menu_state_current->extra_labels[i].buffer]);
        }
    }
}

/* 801CAA7C: Link the message labels while the message is shown, projecting the 3D ones first. */
void gear_shop_draw_notice_labels(void) {
    long depth;
    long flag;
    MenuLabel *label;
    s32 i;

    if (menu_state_current->flags->messages_shown != 0) {
        for (i = 0; i < 3; i++) {
            label = menu_state_current->message_labels[i];
            if (label->projected) {
                RotTransPers4(&label->verts[0], &label->verts[1], &label->verts[2],
                              &label->verts[3], (long *)&label->polys[label->buffer].x0,
                              (long *)&label->polys[label->buffer].x1,
                              (long *)&label->polys[label->buffer].x2,
                              (long *)&label->polys[label->buffer].x3, &depth, &flag);
                AddPrim(&menu_state_current->current->ot[4], &label->polys[label->buffer]);
            } else {
                AddPrim(&menu_state_current->current->ot[4], &label->polys[label->buffer]);
            }
        }
    }
}

/* 801CABD8: The full-screen fade link (ovl2601's 801ca388), empty in the Gear shop. */
void gear_shop_draw_fade_empty(void) {
}

/* 801CABE0: Link every label group. */
void gear_shop_draw_label_layers(void) {
    gear_shop_draw_command_labels();
    gear_shop_draw_list_labels();
    gear_shop_draw_row_labels();
    gear_shop_draw_extra_labels();
    gear_shop_draw_notice_labels();
}

/* 801CAC20: Link both image packet groups, first applying a changed dimming (semi-transparent, 20h grey). */
void gear_shop_draw_screen_images(void) {
    s32 i;

    if (menu_state_current->flags->images_shown != 0) {
        if (menu_state_current->images->dim != menu_state_current->images->dimmed) {
            if (menu_state_current->images->dim != 0) {
                for (i = 0; i < menu_state_current->images->count2; i++) {
                    SetSemiTrans(menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2), 1);
                    SetShadeTex(menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2), 0);
                    menu_state_current->images->packets2[i * 2 + menu_state_current->images->buffer2].tpage |= 0x20;
                    (menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2))->r0 = 0x20;
                    (menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2))->g0 = 0x20;
                    (menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2))->b0 = 0x20;
                }
                for (i = 0; i < menu_state_current->images->count; i++) {
                    SetSemiTrans(menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer), 1);
                    SetShadeTex(menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer), 0);
                    menu_state_current->images->packets[i * 2 + menu_state_current->images->buffer].tpage |= 0x20;
                    (menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer))->r0 = 0x20;
                    (menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer))->g0 = 0x20;
                    (menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer))->b0 = 0x20;
                }
            } else {
                for (i = 0; i < menu_state_current->images->count2; i++) {
                    SetSemiTrans(menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2), 0);
                    SetShadeTex(menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2), 0);
                    menu_state_current->images->packets2[i * 2 + menu_state_current->images->buffer2].tpage |= 0x20;
                    (menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2))->r0 = 0x80;
                    (menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2))->g0 = 0x80;
                    (menu_state_current->images->packets2 + (i * 2 + menu_state_current->images->buffer2))->b0 = 0x80;
                }
                for (i = 0; i < menu_state_current->images->count; i++) {
                    SetSemiTrans(menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer), 0);
                    SetShadeTex(menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer), 0);
                    menu_state_current->images->packets[i * 2 + menu_state_current->images->buffer].tpage |= 0x20;
                    (menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer))->r0 = 0x80;
                    (menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer))->g0 = 0x80;
                    (menu_state_current->images->packets + (i * 2 + menu_state_current->images->buffer))->b0 = 0x80;
                }
            }
            menu_state_current->images->dimmed = menu_state_current->images->dim;
        }
        gear_shop_draw_quads(menu_state_current->images->count2, menu_state_current->images->packets2, menu_state_current->images->buffer2);
        gear_shop_draw_quads(menu_state_current->images->count, menu_state_current->images->packets, menu_state_current->images->buffer);
    }
}

/* 801CB2E8: Link both list packet groups when shown. */
void gear_shop_draw_sprite_lists(void) {
    if (menu_state_current->flags->lists_shown != 0) {
        gear_shop_draw_quads(menu_state_current->lists->second_count, menu_state_current->lists->second, menu_state_current->lists->second_buffer);
        gear_shop_draw_quads(menu_state_current->lists->first_count, menu_state_current->lists->first, menu_state_current->lists->first_buffer);
    }
}

/* 801CB35C: Project and link the shown markers. */
void gear_shop_draw_list_cursors(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (menu_state_current->flags->cursors_shown[i] != 0) {
            gear_shop_draw_projected_quads(1, menu_state_current->cursors[i]->verts, menu_state_current->cursors[i]->polys,
                          menu_state_current->cursors[i]->buffer);
        }
    }
}

/* 801CB3D0: Build the frame's packets: every element while the screen is drawn, then
 * the fade link, which is empty here (801cabd8). */
void gear_shop_draw_screen(void) {
    if (menu_state_current->drawing != 0) {
        gear_shop_panel_grow_opening();
        gear_shop_animate_gear_screen();
        gear_shop_draw_markers();
        gear_shop_draw_label_layers();
        gear_shop_draw_highlight_sprite();
        gear_shop_draw_list_cursors();
        gear_shop_draw_details();
        gear_shop_draw_member_marks();
        gear_shop_draw_scroll_bar();
        gear_shop_draw_screen_images();
        gear_shop_draw_sprite_lists();
        gear_shop_draw_panels();
        gear_shop_draw_gear_screen();
        gear_shop_draw_quads(1, menu_state_current->gear_screen->packets, menu_state_current->gear_screen->buffer);
        gear_shop_draw_model();
    }
    gear_shop_draw_fade_empty();
}

/* 801CB498: Play menu sound `sound` of the effect bank when sounds are on. */
void gear_shop_play_sound(u8 sound) {
    if (menu_state_current->sounds != 0) {
        sound_play_effect_on_last_channels((menu_state_current->effects->id << 16) | sound);
    }
}

/* 801CB4E4: Wait for a controller (sound paused meanwhile), then decode the frame's input into +325. */
void gear_shop_read_input(void) {
    u8 code;
    s32 saved;
    u8 waiting;
    u8 paused;

    code = 8;
    waiting = 1;
    paused = 0;
    do {
        if (pad_get_controller_kind(0) == 0) {
            if (!paused) {
                paused++;
                sound_silence_voices();
                saved = pad_vblank_count;
            }
        } else {
            waiting--;
            if (paused) {
                sound_restore_voices();
                pad_vblank_count = saved;
            }
        }
    } while (waiting);
    if (pad_has_queue_overflowed() != 0) {
        pad_clear_queue();
    } else {
        while (pad_dequeue_state() != 0) {
            if (pad_port0_repeated & 0x2000) {
                code = 0;
                gear_shop_play_sound(1);
                break;
            } else if (pad_port0_repeated & 0x4000) {
                code = 1;
                gear_shop_play_sound(1);
                break;
            } else if (pad_port0_repeated & 0x8000) {
                code = 2;
                gear_shop_play_sound(1);
                break;
            } else if (pad_port0_repeated & 0x1000) {
                code = 3;
                gear_shop_play_sound(1);
                break;
            } else if (pad_port0_pressed & 0x20) {
                code = 4;
                break;
            } else if (pad_port0_pressed & 0x40) {
                code = 5;
                gear_shop_play_sound(3);
                break;
            } else if (pad_port0_pressed & 0x80) {
                code = 6;
                break;
            } else if (pad_port0_pressed & 0x10) {
                code = 7;
                break;
            } else if (pad_port0_repeated & 4) {
                code = 0xA;
                break;
            } else if (pad_port0_repeated & 8) {
                code = 9;
                break;
            }
        }
    }
    menu_state_current->input = code;
}

/* 801CB690: Plan the camera's move from `from` to `to`: steps per axis so x and y arrive together, z over the same count. */
void gear_shop_camera_plan_move(void) {
    u8 done;
    u8 count_x;
    u8 count_y;
    s32 i;

    count_y = 0;
    count_x = 0;
    gear_shop_camera_move.offset[0] = gear_shop_camera_move.offset[1] = gear_shop_camera_move.offset[2] = 0;
    if (gear_shop_camera_move.to[0] >= gear_shop_camera_move.from[0]) {
        gear_shop_camera_move.step[0] = gear_shop_camera_move.to[0] - gear_shop_camera_move.from[0];
        gear_shop_camera_move.negative[0] = 0;
    } else {
        gear_shop_camera_move.step[0] = gear_shop_camera_move.to[0] - gear_shop_camera_move.from[0];
        gear_shop_camera_move.negative[0] = 1;
    }
    if (gear_shop_camera_move.to[1] >= gear_shop_camera_move.from[1]) {
        gear_shop_camera_move.step[1] = gear_shop_camera_move.to[1] - gear_shop_camera_move.from[1];
        gear_shop_camera_move.negative[1] = 0;
    } else {
        gear_shop_camera_move.step[1] = gear_shop_camera_move.to[1] - gear_shop_camera_move.from[1];
        gear_shop_camera_move.negative[1] = 1;
    }
    if (gear_shop_camera_move.step[0] < 0) {
        gear_shop_camera_move.step[0] = ~gear_shop_camera_move.step[0] + 1;
    }
    if (gear_shop_camera_move.step[1] < 0) {
        gear_shop_camera_move.step[1] = ~gear_shop_camera_move.step[1] + 1;
    }
    if (gear_shop_camera_move.step[0] >= gear_shop_camera_move.step[1]) {
        gear_shop_camera_move.step[1] = (gear_shop_camera_move.step[1] << 16) / gear_shop_camera_move.step[0];
        gear_shop_camera_move.step[0] = 0x10000;
    } else {
        gear_shop_camera_move.step[0] = (gear_shop_camera_move.step[0] << 16) / gear_shop_camera_move.step[1];
        gear_shop_camera_move.step[1] = 0x10000;
    }
    done = 1;
    do {
        for (i = 0; i < gear_shop_camera_move.frames; i++) {
            gear_shop_camera_move.offset[0] += gear_shop_camera_move.step[0];
        }
        if (gear_shop_camera_move.negative[0] == 0) {
            if (gear_shop_camera_move.offset[0] / 0x10000 + gear_shop_camera_move.from[0] >= gear_shop_camera_move.to[0]) {
                done = 0;
            } else {
                count_x++;
            }
        } else {
            if (gear_shop_camera_move.to[0] >= gear_shop_camera_move.from[0] - gear_shop_camera_move.offset[0] / 0x10000) {
                done = 0;
            } else {
                count_x++;
            }
        }
    } while (done);
    done = 1;
    do {
        for (i = 0; i < gear_shop_camera_move.frames; i++) {
            gear_shop_camera_move.offset[1] += gear_shop_camera_move.step[1];
        }
        if (gear_shop_camera_move.negative[1] == 0) {
            if (gear_shop_camera_move.offset[1] / 0x10000 + gear_shop_camera_move.from[1] >= gear_shop_camera_move.to[1]) {
                done = 0;
            } else {
                count_y++;
            }
        } else {
            if (gear_shop_camera_move.to[1] >= gear_shop_camera_move.from[1] - gear_shop_camera_move.offset[1] / 0x10000) {
                done = 0;
            } else {
                count_y++;
            }
        }
    } while (done);
    if (count_x < count_y) {
        count_x = count_y;
    }
    if (gear_shop_camera_move.to[2] >= gear_shop_camera_move.from[2]) {
        gear_shop_camera_move.step[2] = gear_shop_camera_move.to[2] - gear_shop_camera_move.from[2];
        gear_shop_camera_move.negative[2] = 0;
    } else {
        gear_shop_camera_move.step[2] = gear_shop_camera_move.to[2] - gear_shop_camera_move.from[2];
        gear_shop_camera_move.negative[2] = 1;
    }
    if (gear_shop_camera_move.step[2] < 0) {
        gear_shop_camera_move.step[2] = ~gear_shop_camera_move.step[2] + 1;
    }
    gear_shop_camera_move.offset[2] = 0;
    gear_shop_camera_move.offset[1] = 0;
    gear_shop_camera_move.offset[0] = 0;
    gear_shop_camera_move.step[2] = (gear_shop_camera_move.step[2] << 16) / count_x;
}

/* 801CBA2C
 * Advance the camera move one update: x and y move the model's depth and
 * height, z turns the gear (actor 1's root's y rotation, from which the
 * module's 801DC5C0 builds its transform); each axis stops (clears its
 * motion bit) at its target. The whole part of the 16.16 offset is written
 * out in each comparison and store.
 */
void gear_shop_camera_step_move(void) {
    s32 i;

    if (menu_state_current->view_motion & 1) {
        for (i = 0; i < gear_shop_camera_move.frames; i++) {
            gear_shop_camera_move.offset[0] += gear_shop_camera_move.step[0];
        }
        if (gear_shop_camera_move.negative[0] == 0) {
            if (gear_shop_camera_move.offset[0] / 0x10000 + gear_shop_camera_move.from[0] >= gear_shop_camera_move.to[0]) {
                menu_state_current->offset2.vz = gear_shop_camera_move.to[0];
                menu_state_current->view_motion &= 6;
            } else {
                menu_state_current->offset2.vz = gear_shop_camera_move.offset[0] / 0x10000 + gear_shop_camera_move.from[0];
            }
        } else {
            if (gear_shop_camera_move.to[0] >= gear_shop_camera_move.from[0] - gear_shop_camera_move.offset[0] / 0x10000) {
                menu_state_current->offset2.vz = gear_shop_camera_move.to[0];
                menu_state_current->view_motion &= 6;
            } else {
                menu_state_current->offset2.vz = gear_shop_camera_move.from[0] - gear_shop_camera_move.offset[0] / 0x10000;
            }
        }
    }
    if (menu_state_current->view_motion & 2) {
        for (i = 0; i < gear_shop_camera_move.frames; i++) {
            gear_shop_camera_move.offset[1] += gear_shop_camera_move.step[1];
        }
        if (gear_shop_camera_move.negative[1] == 0) {
            if (gear_shop_camera_move.offset[1] / 0x10000 + gear_shop_camera_move.from[1] >= gear_shop_camera_move.to[1]) {
                menu_state_current->offset2.vy = gear_shop_camera_move.to[1];
                menu_state_current->view_motion &= 5;
            } else {
                menu_state_current->offset2.vy = gear_shop_camera_move.offset[1] / 0x10000 + gear_shop_camera_move.from[1];
            }
        } else {
            if (gear_shop_camera_move.to[1] >= gear_shop_camera_move.from[1] - gear_shop_camera_move.offset[1] / 0x10000) {
                menu_state_current->offset2.vy = gear_shop_camera_move.to[1];
                menu_state_current->view_motion &= 5;
            } else {
                menu_state_current->offset2.vy = gear_shop_camera_move.from[1] - gear_shop_camera_move.offset[1] / 0x10000;
            }
        }
    }
    if (menu_state_current->view_motion & 4) {
        gear_shop_camera_move.offset[2] += gear_shop_camera_move.step[2];
        if (gear_shop_camera_move.negative[2] == 0) {
            if (gear_shop_camera_move.offset[2] / 0x10000 + gear_shop_camera_move.from[2] >= gear_shop_camera_move.to[2]) {
                gear_model_actors[1]->parts->rotation.vy = gear_shop_camera_move.to[2];
                menu_state_current->view_motion &= 3;
            } else {
                gear_model_actors[1]->parts->rotation.vy = gear_shop_camera_move.offset[2] / 0x10000 + gear_shop_camera_move.from[2];
            }
        } else {
            if (gear_shop_camera_move.to[2] >= gear_shop_camera_move.from[2] - gear_shop_camera_move.offset[2] / 0x10000) {
                gear_model_actors[1]->parts->rotation.vy = gear_shop_camera_move.to[2];
                menu_state_current->view_motion &= 3;
            } else {
                gear_model_actors[1]->parts->rotation.vy = gear_shop_camera_move.from[2] - gear_shop_camera_move.offset[2] / 0x10000;
            }
        }
    }
}

/* 801CBDA0: Load the model's matrices, then the view's. */
void gear_shop_view_update(void) {
    gear_shop_camera_step_move();
    gpu_build_rotation_matrix(&menu_state_current->angles2, &menu_state_current->matrix2);
    TransMatrix(&menu_state_current->matrix2, &menu_state_current->offset2);
    SetRotMatrix(&menu_state_current->matrix2);
    SetTransMatrix(&menu_state_current->matrix2);
    gpu_build_rotation_matrix(&menu_state_current->angles, &menu_state_current->matrix);
    TransMatrix(&menu_state_current->matrix, &menu_state_current->offset);
    SetRotMatrix(&menu_state_current->matrix);
    SetTransMatrix(&menu_state_current->matrix);
}

/* 801CBE60: Debug display (when enabled and the model is loaded): the model translation, the gear actor's turn, ground height and scale, in decimal with a minus sign. */
void gear_shop_draw_debug_model_values(void) {
    s32 values[6];
    s32 i;
    s32 j;
    s32 x;
    s32 y;
    s32 negative;
    s32 value;

    if (gear_shop_debug_values_on != 0 && menu_state_current->model_parts[1]->unk12 != 0) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x21, &gear_shop_debug_value_packets[0], menu_state_current->buffer_index, 0x10, 0x10, 0x1000);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x22, &gear_shop_debug_value_packets[2], menu_state_current->buffer_index, 0x10, 0x20, 0x1000);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x23, &gear_shop_debug_value_packets[4], menu_state_current->buffer_index, 0x10, 0x30, 0x1000);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0xA, &gear_shop_debug_value_packets[6], menu_state_current->buffer_index, 0x10, 0x40, 0x1000);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x10, &gear_shop_debug_value_packets[8], menu_state_current->buffer_index, 0x10, 0x50, 0x1000);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x1C, &gear_shop_debug_value_packets[10], menu_state_current->buffer_index, 0x10, 0x60, 0x1000);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE3, &gear_shop_debug_value_packets[12], menu_state_current->buffer_index, 0xA0, 0x64, 0x1000);
        values[0] = menu_state_current->offset2.vx;
        values[1] = menu_state_current->offset2.vy;
        values[2] = menu_state_current->offset2.vz;
        values[3] = gear_model_actors[1]->parts->rotation.vy;
        values[4] = gear_model_actors[1]->groundY;
        values[5] = gear_model_actors[1]->scale;
        gear_shop_debug_value_sprite_count = 7;
        for (i = 0; i < 6; i++) {
            value = values[i];
            negative = 0;
            if (value < 0) {
                negative = 1;
                gear_shop_debug_value_sprite_count += sprite_sheet_draw_scaled(menu_state_current->sheet, 0xE5, &gear_shop_debug_value_packets[gear_shop_debug_value_sprite_count * 2],
                                            menu_state_current->buffer_index, 0x30, i * 0x10 + 0x10, 0x1000);
                value = ~values[i] + 1;
            }
            gear_shop_split_digits(value);
            for (j = 0, y = i * 0x10 + 0x10, x = negative * 8 + 0x30; j < 9; j++) {
                if (menu_state_current->digits[j] != 0xFF) {
                    gear_shop_debug_value_sprite_count += sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[j],
                                                &gear_shop_debug_value_packets[gear_shop_debug_value_sprite_count * 2], menu_state_current->buffer_index, x, y, 0x1000);
                    x += 8;
                }
            }
        }
        gear_shop_draw_quads(gear_shop_debug_value_sprite_count, gear_shop_debug_value_packets, menu_state_current->buffer_index);
    }
}

/* 801CC1C4: Run one frame: input, buffer swap, both ordering tables, view, packets, then present the finished buffer. */
void gear_shop_run_frame(void) {
    MenuBuffer *env;

    if (*mode_disc_mode_pointer != -1) {
        __asm__ volatile("break 1024");
    }
    gear_shop_read_input();
    boot_check_soft_reset();
    menu_state_current->current =
        menu_state_current->current == &menu_state_current->buffers[0] ? &menu_state_current->buffers[1] : &menu_state_current->buffers[0];
    menu_state_current->buffer_index = menu_state_current->buffer_index == 0;
    ClearOTagR(menu_state_current->current->ot, 16);
    ClearOTagR((u_long *)menu_state_current->current->ot_big, 0x400);
    gear_shop_view_update();
    gear_shop_draw_debug_model_values();
    gear_shop_draw_screen();
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&menu_state_current->current->draw);
    PutDispEnv(&menu_state_current->current->disp);
    ClearImage(&menu_state_current->current->draw.clip, 0, 0, 0);
    env = menu_state_current->current;
    AddPrims(env->ot_big, &env->ot[15], env->ot);
    DrawOTag((u_long *)&menu_state_current->current->ot_big[0x3FF]);
}

/* 801CC31C: Create the marker block: both yes/no markers at the cursor (0), the four markers (2) or one (3). */
void gear_shop_markers_open(u8 mode) {
    s32 i;

    menu_state_current->markers = heap_alloc(sizeof(MenuMarkers), 0);
    bzero((u_char *)menu_state_current->markers, sizeof(MenuMarkers));
    switch (mode) {
    case 0:
        menu_state_current->flags->markers_shown = 1;
        menu_state_current->markers->at_cursor[0] = 1;
        menu_state_current->markers->at_cursor[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, &menu_state_current->markers->polys[i * 2],
                          menu_state_current->buffer_index, gear_shop_marker_x_table[i], gear_shop_marker_y_table[i], 0x800);
            menu_state_current->markers->buffer[i] = menu_state_current->buffer_index;
        }
        break;
    case 3:
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, menu_state_current->markers->polys, menu_state_current->buffer_index,
                      0, 0, 0x800);
        menu_state_current->markers->buffer[0] = menu_state_current->buffer_index;
        menu_state_current->flags->markers_shown = 1;
        break;
    case 1:
        break;
    }
}

/* 801CC4DC: Hide the markers, let a frame pass, and release them. The refuel caller
 * passes an ignored zero; unspecified arity preserves that call. */
void gear_shop_markers_close() {
    menu_state_current->flags->markers_shown = 0;
    gear_shop_run_frame();
    heap_free(menu_state_current->markers);
}

/* 801CC520: The view zoom-in of ovl2601's 801cb340 (item_shop_view_start_zoom_in),
 * empty in the Gear shop (which moves its own camera) and called by nothing. */
void gear_shop_view_start_zoom_in_empty(void) {
}

/* 801CC528: The view zoom-out of ovl2601's 801cb370 (item_shop_view_start_zoom_out),
 * empty here; a command that redraws its screen still calls it. */
void gear_shop_view_start_zoom_out_empty(void) {
}

/* 801CC530: Open the message panel and show three lines of label text from entry `first`. */
void gear_shop_notice_open(u8 first) {
    MenuGrowth *growth;
    MenuLabel *label;
    s32 i;
    s32 x;

    x = 0x50;
    gear_shop_panel_open(4, 0x42, 0x46, 0xC0, 0x40, 1, 1, 4, 0);
    growth = menu_state_current->growth[4];
    while (growth->done == 0) {
        gear_shop_run_frame();
    }
    for (i = 0; i < 4; i++) {
        menu_state_current->message_labels[i] = heap_alloc(sizeof(MenuLabel), 0);
        bzero((u_char *)menu_state_current->message_labels[i], sizeof(MenuLabel));
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
    i = 0;
    do {
        label = menu_state_current->message_labels[i];
        label->width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, first + i), label->pixels,
                                     0x36, i % 2);
        gear_shop_label_init_quads(label, i, 0, 0);
        gear_shop_set_rect_verts(label->verts, x, i * 16 + 0x50, label->width, 13);
        (label->polys + menu_state_current->buffer_index)->u0 = 0;
        (label->polys + menu_state_current->buffer_index)->v0 = (i / 2) * 13 + 0x4E;
        (label->polys + menu_state_current->buffer_index)->u1 = label->width;
        (label->polys + menu_state_current->buffer_index)->v1 = (i / 2) * 13 + 0x4E;
        (label->polys + menu_state_current->buffer_index)->u2 = 0;
        (label->polys + menu_state_current->buffer_index)->v2 = (i / 2) * 13 + 0x5B;
        (label->polys + menu_state_current->buffer_index)->u3 = label->width;
        (label->polys + menu_state_current->buffer_index)->v3 = (i / 2) * 13 + 0x5B;
        label->buffer = menu_state_current->buffer_index;
        label->projected = 1;
        i++;
    } while (i < 3);
    LoadImage(&menu_state_current->message_labels[0]->rect,
              (u_long *)menu_state_current->message_labels[0]->pixels);
    LoadImage(&menu_state_current->message_labels[2]->rect,
              (u_long *)menu_state_current->message_labels[2]->pixels);
    DrawSync(0);
    menu_state_current->flags->messages_shown = 1;
    heap_free(menu_state_current->message_labels[0]->pixels);
    heap_free(menu_state_current->message_labels[2]->pixels);
    if (menu_state_current->flags->unknown5a[1] == 2) {
        menu_state_current->flags->unknown5a[1] = 1;
    }
    gear_shop_run_frame();
    gear_shop_run_frame();
}

/* 801CC9A0: Close the message panel and release its labels, then let a frame pass. */
void gear_shop_notice_close(void) {
    s32 i;

    if (menu_state_current->flags->panels_shown[4] != 0) {
        gear_shop_panel_close(4);
        menu_state_current->flags->messages_shown = 0;
        for (i = 0; i < 4; i++) {
            heap_free(menu_state_current->message_labels[i]);
        }
    }
    menu_state_current->flags->unknown5a[1] = 0;
    gear_shop_run_frame();
}

/* 801CCA40: Let the player choose yes or no (1 = yes; moving only when `movable`); without `wait` the choice ends after 60 idle frames. */
u8 gear_shop_ask_yes_no(u8 wait, u8 movable) {
    u8 choosing;
    u8 yes;
    u8 timer;

    choosing = 1;
    yes = 0;
    timer = 60;
    while (choosing) {
        if (!wait) {
            menu_state_current->markers->shown[2] = 0;
            menu_state_current->markers->shown[3] = 0;
            if (menu_state_current->input != 8) {
                break;
            }
            if (--timer == 0) {
                break;
            }
        }
        gear_shop_run_frame();
        switch (menu_state_current->input) {
        case 4:
            gear_shop_play_sound(2);
            choosing = 0;
            break;
        case 5:
            yes = 0;
            choosing = 0;
            break;
        case 2:
            if (movable) {
                menu_state_current->markers->shown[2] = 1;
                yes = 1;
                menu_state_current->markers->shown[3] = 0;
            }
            break;
        case 0:
            if (movable) {
                menu_state_current->markers->shown[2] = 0;
                yes = 0;
                menu_state_current->markers->shown[3] = 1;
            }
            break;
        }
    }
    menu_state_current->markers->shown[2] = 0;
    menu_state_current->markers->shown[3] = 0;
    return yes;
}

/* 801CCC18: Ask message `message` (movable only without a follow-up); a yes is confirmed by `confirm` unless it is ff. */
s32 gear_shop_notice_ask_yes_no(u8 message, u8 confirm, u8 wait) {
    u8 answer;
    u8 movable;

    movable = 1;
    gear_shop_notice_open(message);
    if (confirm == 0xFF) {
        menu_state_current->markers->shown[3] = 1;
    } else {
        menu_state_current->markers->shown[2] = 0;
        movable = 0;
        menu_state_current->markers->shown[3] = 0;
    }
    answer = gear_shop_ask_yes_no(wait, movable);
    gear_shop_notice_close();
    if (confirm != 0xFF) {
        menu_state_current->markers->shown[3] = 1;
        gear_shop_notice_open(confirm);
        menu_state_current->markers->shown[3] = 1;
        answer = gear_shop_ask_yes_no(wait, 1);
        gear_shop_notice_close();
    }
    return answer;
}

/* 801CCD20: Close the screen: stop drawing, release every block and resource, then the menu state itself. */
void gear_shop_shut_down(void) {
    gear_shop_run_frame();
    gear_shop_run_frame();
    menu_state_current->drawing = 0;
    gear_shop_run_frame();
    do {
        gear_shop_run_frame();
    } while (menu_state_current->buffer_index != 0);
    gear_shop_alloc_or_free_card_state(0);
    gear_shop_alloc_or_free_flags(0);
    gear_shop_alloc_or_free_screen_images(0);
    gear_shop_alloc_or_free_sprite_lists(0);
    gear_shop_alloc_or_free_table_directory(0);
    gear_shop_alloc_or_free_prims(0);
    gear_shop_load_or_release_data_set(0x10);
    gear_shop_alloc_or_free_shop_details(0);
    gear_shop_alloc_or_free_gear_screen(0);
    heap_free(menu_state_current->sheet);
    heap_free(menu_state_current->label_text);
    heap_free(menu_state_current->labels[0].pixels);
    if (menu_state_debug_start != 0) {
        sound_stop_bank_effects(menu_state_current->effects);
        gear_shop_run_frame();
        sound_remove_effect_bank(menu_state_current->effects);
        gear_shop_run_frame();
        heap_free(menu_state_current->effects);
    }
    gear_shop_alloc_or_free_name_entry_block(0);
    heap_free(menu_state_current);
}

/* 801CCE90: Render `count` labels into VRAM (their shown flags are left alone). */
void gear_shop_label_render_table(u8 count, MenuLabel *labels, u8 *text_ids, u8 *shown) {
    gear_shop_label_render_pairs(labels, text_ids, 2, count);
}

/* 801CCEBC: Clear `count` shown flags. */
void gear_shop_label_clear_shown(u8 count, u8 *shown) {
    s32 i;

    for (i = 0; i < count; i++) {
        shown[i] = 0;
    }
}

/* 801CCEE8: Show label `index`: in list row `row` (mode 0, offset by its column) or at the info position (mode 1; labels past the first further left). */
void gear_shop_label_place(u8 count, MenuLabel *labels, u8 *text_ids, s32 *offsets, u8 *shown, u8 index, u8 row, u8 mode) {
    switch (mode) {
    case 0:
        gear_shop_label_clear_shown(count, shown);
        (labels[index].polys + menu_state_current->buffer_index)->x0 = gear_shop_highlight_x_table[row + index] + 0x16 + offsets[index];
        (labels[index].polys + menu_state_current->buffer_index)->y0 = gear_shop_highlight_y_table[row + index] - 0x22;
        (labels[index].polys + menu_state_current->buffer_index)->x1 =
            labels[index].width + (gear_shop_highlight_x_table[row + index] + 0x16 + offsets[index]);
        (labels[index].polys + menu_state_current->buffer_index)->y1 = gear_shop_highlight_y_table[row + index] - 0x22;
        (labels[index].polys + menu_state_current->buffer_index)->x2 = gear_shop_highlight_x_table[row + index] + 0x16 + offsets[index];
        (labels[index].polys + menu_state_current->buffer_index)->y2 = gear_shop_highlight_y_table[row + index] - 0x15;
        (labels[index].polys + menu_state_current->buffer_index)->x3 =
            labels[index].width + (gear_shop_highlight_x_table[row + index] + 0x16 + offsets[index]);
        (labels[index].polys + menu_state_current->buffer_index)->y3 = gear_shop_highlight_y_table[row + index] - 0x15;
        break;
    case 1:
        if (index != 0) {
            (labels[index].polys + menu_state_current->buffer_index)->x0 = 0xCE;
            (labels[index].polys + menu_state_current->buffer_index)->y0 = 0x7E;
            (labels[index].polys + menu_state_current->buffer_index)->x1 = labels->width + 0xCE;
            (labels[index].polys + menu_state_current->buffer_index)->y1 = 0x7E;
            (labels[index].polys + menu_state_current->buffer_index)->x2 = 0xCE;
            (labels[index].polys + menu_state_current->buffer_index)->y2 = 0x8B;
            (labels[index].polys + menu_state_current->buffer_index)->x3 = labels->width + 0xCE;
            (labels[index].polys + menu_state_current->buffer_index)->y3 = 0x8B;
        } else {
            (labels->polys + menu_state_current->buffer_index)->x0 = 0xEC;
            (labels->polys + menu_state_current->buffer_index)->y0 = 0x7E;
            (labels->polys + menu_state_current->buffer_index)->x1 = labels->width + 0xEC;
            (labels->polys + menu_state_current->buffer_index)->y1 = 0x7E;
            (labels->polys + menu_state_current->buffer_index)->x2 = 0xEC;
            (labels->polys + menu_state_current->buffer_index)->y2 = 0x8B;
            (labels->polys + menu_state_current->buffer_index)->x3 = labels->width + 0xEC;
            (labels->polys + menu_state_current->buffer_index)->y3 = 0x8B;
        }
        break;
    }
    labels[index].buffer = menu_state_current->buffer_index;
    shown[index] = 1;
}

/* 801CD310: Reveal `count` image pairs one step at a time (two frames each), the second of each pair one step behind. */
void gear_shop_command_window_open(s32 count, s32 *ids) {
    s32 step;
    s32 i;

    menu_state_current->images->dim = 0;
    menu_state_current->images->dimmed = 0;
    menu_state_current->flags->images_shown = 1;
    for (step = 1; step <= count; step++) {
        if (step != count) {
            menu_state_current->images->count = 0;
            for (i = 0; i < step; i++) {
                menu_state_current->images->count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, ids[i * 2],
                                  menu_state_current->images->packets + menu_state_current->images->count * 2,
                                  menu_state_current->buffer_index, 0xA0, 0x96, 0x1000);
            }
            menu_state_current->images->buffer = menu_state_current->buffer_index;
        }
        menu_state_current->images->count2 = 0;
        if (step != 1) {
            for (i = 0; i < step - 1; i++) {
                menu_state_current->images->count2 +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, ids[i * 2 + 1],
                                  menu_state_current->images->packets2 + menu_state_current->images->count2 * 2,
                                  menu_state_current->buffer_index, 0xA0, 0x96, 0x1000);
            }
            menu_state_current->images->buffer2 = menu_state_current->buffer_index;
        }
        for (i = 0; i < 2; i++) {
            gear_shop_run_frame();
        }
    }
}

/* 801CD564: Reveal the list pictures of the current command (up to four pairs), two frames per step. */
void gear_shop_choice_window_open(u8 menu) {
    s32 animate;
    s32 step;
    s32 i;
    s32 command;
    s32 *ids;
    s32 *paired;

    menu_state_current->images->dim = 0;
    animate = 1;
    step = 1;
    menu_state_current->images->dimmed = 0;
    command = menu;
    menu_state_current->lists->first_count = 0;
    ids = gear_shop_choice_window_images;
    menu_state_current->lists->second_count = 0;
    paired = ids + 1;
    menu_state_current->flags->lists_shown = 1;
    for (; step < 5; step++) {
        menu_state_current->lists->first_count = 0;
        menu_state_current->choice_count = 0;
        for (i = 0; i < step; i++) {
            s32 offset;

            offset = i * 2;
            offset += (command + menu_state_current->cursor) * 8;
            if (ids[offset] != 0xFFFF) {
                menu_state_current->lists->first_count +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, ids[offset],
                                  menu_state_current->lists->first + menu_state_current->lists->first_count * 2,
                                  menu_state_current->buffer_index, 0xA0, 0x96, 0x1000);
                menu_state_current->choice_count++;
            } else {
                animate = 0;
            }
        }
        menu_state_current->lists->first_buffer = menu_state_current->buffer_index;
        if (animate) {
            for (i = 0; i < 2; i++) {
                gear_shop_run_frame();
            }
        }
        menu_state_current->lists->second_count = 0;
        for (i = 0; i < step; i++) {
            s32 offset;

            offset = i * 2;
            offset += (command + menu_state_current->cursor) * 8;
            if (ids[offset] != 0xFFFF) {
                menu_state_current->lists->second_count += sprite_sheet_draw_scaled(
                    menu_state_current->sheet, paired[offset],
                    menu_state_current->lists->second + menu_state_current->lists->second_count * 2, menu_state_current->buffer_index, 0xA0,
                    0x96, 0x1000);
            }
        }
        menu_state_current->lists->second_buffer = menu_state_current->buffer_index;
        if (animate) {
            for (i = 0; i < 2; i++) {
                gear_shop_run_frame();
            }
        }
    }
}

/* 801CD838: Draw `count` image pairs with pair `selected` highlighted (+0dh), and put the cursor on it. */
void gear_shop_command_window_set_cursor(u8 count, u8 selected, s32 *ids) {
    s32 id;
    s32 i;

    menu_state_current->images->count = 0;
    menu_state_current->images->count2 = 0;
    for (i = 0; i < count; i++) {
        if (i == selected) {
            id = ids[i * 2] + 0xD;
        } else {
            id = ids[i * 2];
        }
        menu_state_current->images->count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, id,
                          menu_state_current->images->packets + menu_state_current->images->count * 2, menu_state_current->buffer_index,
                          0xA0, 0x96, 0x1000);
        menu_state_current->images->count2 +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, ids[i * 2 + 1],
                          menu_state_current->images->packets2 + menu_state_current->images->count2 * 2,
                          menu_state_current->buffer_index, 0xA0, 0x96, 0x1000);
    }
    menu_state_current->images->buffer = menu_state_current->buffer_index;
    menu_state_current->images->buffer2 = menu_state_current->buffer_index;
    gear_shop_highlight_place(selected, 1);
    menu_state_current->flags->sprite_shown = 1;
}

/* 801CDA0C: Draw the current command's list pictures with the chosen one highlighted (+0dh), and put the cursor on it. */
void gear_shop_choice_window_set_cursor(u8 menu) {
    s32 id;
    s32 i;

    menu_state_current->lists->first_count = 0;
    menu_state_current->lists->second_count = 0;
    for (i = 0; i < menu_state_current->choice_count; i++) {
        if (i == menu_state_current->choice) {
            id = gear_shop_choice_window_images[(menu + menu_state_current->cursor) * 8 + i * 2] + 0xD;
        } else {
            id = gear_shop_choice_window_images[(menu + menu_state_current->cursor) * 8 + i * 2];
        }
        menu_state_current->lists->first_count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, id,
                          menu_state_current->lists->first + menu_state_current->lists->first_count * 2, menu_state_current->buffer_index,
                          0xA0, 0x96, 0x1000);
        menu_state_current->lists->second_count += sprite_sheet_draw_scaled(
            menu_state_current->sheet, gear_shop_choice_window_images[(menu + menu_state_current->cursor) * 8 + i * 2 + 1],
            menu_state_current->lists->second + menu_state_current->lists->second_count * 2, menu_state_current->buffer_index, 0xA0, 0x96,
            0x1000);
    }
    menu_state_current->lists->first_buffer = menu_state_current->buffer_index;
    menu_state_current->lists->second_buffer = menu_state_current->buffer_index;
    gear_shop_highlight_place(menu_state_current->choice + 4, 1);
    menu_state_current->flags->sprite_shown = 1;
}

/* 801CDC68: Run the chosen top command (0 leaves); afterwards restore the command screen. Returns 0 to leave. */
u8 gear_shop_top_command_run(void) {
    u8 running;
    u8 redraw;

    running = 1;
    if (menu_state_current->cursor != 0) {
        redraw = gear_shop_choice_list_run();
    } else {
        running = 0;
    }
    if (redraw) {
        gear_shop_view_start_zoom_out_empty();
        gear_shop_label_render_table(4, menu_state_current->list_labels, gear_shop_top_label_ids, menu_state_current->flags->list_labels_shown);
    }
    menu_state_current->images->dim = 0;
    menu_state_current->images->dimmed = 1;
    menu_state_current->flags->sprite_shown = 1;
    menu_state_current->flags->cursor_shown = 1;
    menu_state_current->cursor_shown = 0xFF;
    menu_state_current->flags->lists_shown = 0;
    return running;
}

/* 801CDD74: The command screen: leave, sell, buy or the two gear commands, switching members with L1/R1, until leaving. */
void gear_shop_run(void) {
    u8 running;

    running = 1;
    gear_shop_model_load_gear(1, gear_shop_edited_gear);
    menu_state_current->flags->model_shown = running;
    menu_state_current->cursor = 2;
    gear_shop_command_window_open(5, gear_shop_command_images);
    gear_shop_label_render_table(4, menu_state_current->list_labels, gear_shop_top_label_ids, menu_state_current->flags->list_labels_shown);
    if (gear_shop_available_member_count >= 2) {
        menu_state_current->flags->marks_shown = running;
    }
    do {
        gear_shop_run_frame();
        switch (menu_state_current->input) {
        case 4:
            gear_shop_play_sound(2);
            menu_state_current->images->dim = 1;
            gear_shop_highlight_hide();
            gear_shop_label_clear_shown(4, menu_state_current->flags->list_labels_shown);
            menu_state_current->prims->width = 0x4C;
            running = gear_shop_top_command_run();
            menu_state_current->prims->width = 0x40;
            break;
        case 5:
            running = 0;
            break;
        case 1:
            if (menu_state_current->cursor != 0) {
                menu_state_current->cursor--;
            } else {
                menu_state_current->cursor = 3;
            }
            break;
        case 3:
            if (++menu_state_current->cursor >= 4) {
                menu_state_current->cursor = 0;
            }
            break;
        case 9:
            gear_shop_switch_member(0);
            break;
        case 10:
            gear_shop_switch_member(1);
            break;
        }
        if (menu_state_current->cursor != menu_state_current->cursor_shown) {
            gear_shop_command_window_set_cursor(4, menu_state_current->cursor, gear_shop_command_images);
            gear_shop_label_place(4, menu_state_current->list_labels, gear_shop_top_label_ids, gear_shop_top_label_x_offsets,
                          menu_state_current->flags->list_labels_shown, menu_state_current->cursor, 0, 0);
            menu_state_current->cursor_shown = menu_state_current->cursor;
        }
    } while (running);
    gear_shop_debug_values_on = 0;
    gear_shop_model_close();
    gear_shop_member_marks_close();
}

/* 801CE024: Overlay entry: build the gear screen, run it, and tear it down. */
void gear_shop_main(void) {
    gear_shop_alloc_or_free_card_state(1);
    gear_shop_alloc_or_free_flags(1);
    gear_shop_alloc_or_free_screen_images(1);
    gear_shop_alloc_or_free_sprite_lists(1);
    gear_shop_alloc_or_free_table_directory(1);
    gear_shop_alloc_or_free_prims(1);
    gear_shop_alloc_or_free_name_entry_block(1);
    gear_shop_alloc_or_free_shop_details(1);
    gear_shop_alloc_or_free_gear_screen(1);
    menu_state_current->images->screen.x = 0x2C0;
    menu_state_current->images->screen.y = 0x100;
    menu_state_current->images->screen.w = 0x140;
    menu_state_current->images->screen.h = 0xE0;
    menu_state_current->prims->width = 0x40;
    menu_state_current->offset.vz = 0x200;
    menu_state_current->angles.vz = 0;
    menu_state_current->angles.vx = 0;
    menu_state_current->angles.vy = 0;
    menu_state_current->offset2.vz = 0x400;
    menu_state_current->angles2.vz = 0;
    menu_state_current->angles2.vx = 0;
    menu_state_current->angles2.vy = 0x400;
    menu_state_current->view_motion = 0;
    gear_shop_camera_move.from[2] = -0x400;
    gear_shop_camera_move.to[2] = -0x400;
    gear_shop_camera_move.from[0] = 0x400;
    gear_shop_camera_move.from[1] = 0;
    gear_shop_camera_move.to[0] = 0x400;
    gear_shop_camera_move.to[1] = 0;
    gear_shop_camera_move.frames = 0x10;
    gear_shop_init_party();
    gear_shop_reset_buffer_index();
    gear_shop_init_labels();
    gear_shop_init_highlight_and_fade_prims();
    gear_shop_read_sheet_entries();
    gear_shop_init_stock();
    gear_shop_model_init();
    menu_state_current->marks = heap_alloc(sizeof(MenuMarkerQuads), 0);
    bzero((u_char *)menu_state_current->marks, sizeof(MenuMarkerQuads));
    gear_shop_member_marks_layout();
    menu_state_current->drawing = 1;
    menu_state_current->sounds = 1;
    gear_shop_run();
    gear_shop_shut_down();
}
