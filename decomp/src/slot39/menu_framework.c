/*
 * Menu overlay (mode 5; Disc 1 slot 39 / unpacked 2597, Disc 2 slot 34 /
 * unpacked 2592; loaded at 801c5000). The resident mode table enters it
 * through 8001c634 with no overlay preloaded. By the menu kind in
 * menu_state_screen it runs the field main menu (items, equipment and the other
 * commands; kind 0), the title screen's file (memory-card load) screen
 * (kind 2) or kind 6. It keeps its state behind menu_state_current and reads and
 * writes "bu00:"/"bu10:" memory-card files (BASLUS-00664...).
 *
 * This unit is the image's first: the screen framework, the memory card
 * and save code and the field menu's screens up to the item screen. Rodata
 * 801C5000-801C50FC, text 801C531C-801DBDB4, the overlay's initialized data
 * (801E96A4-801EA6D0, all of it: the other units' tables lie among its own)
 * and its variables from 801EA6D0. menu_member_screens.c's head gives the
 * boundary evidence.
 */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
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
#include "menu/tables.h"
#include "menu.h"
#include "file.h"
#include "field.h"

/* Declared here only: menu_member_screens.c calls it without a prototype. */
void menu_detail_layout_tabs(u8 slot, u8 shown, u8 second);

/* menu_member_screens.c's item use, which this unit calls without a prototype:
 * its u8 result is tested unmasked. */
s32 menu_use_item_on_character();

/* The overlay's initialized data: all of it is defined here, ahead of the
 * units' uninitialized variables. */
u8 menu_save_command_stays_open = 0; /* 801E96A4: the file screen saves (nonzero) or loads */
/* The flag menu_saving_at_cd_change (0), then 08 00 before the word-aligned masks,
 * which nothing reads: taken as its padding, holding stray bytes, by analogy with
 * battle's flag battle_applying_item_results (08 00 00) and ovl2596's
 * battle_results_fanfare_started (04 00 00); neither the bytes nor the vendor tools
 * tell it from an unreferenced byte 8 (docs/matching.md). It stays original data
 * (slot39.classification.txt). */
INCLUDE_ORIGINAL_UNALIGNED(".data", menu_saving_at_cd_change, 0x801E96A5, 3);
extern u8 menu_saving_at_cd_change; /* set while menu kind 6 (menu_cd_change_run) saves: menu_save_build_payload
                       * then stores 1, not the disc number - 1, in vars[82] */
/* single-bit masks */
u16 menu_bit_masks[16] = { /* 801E96A8 */
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000,
};
u16 menu_bit_masks_msb_first[16] = { /* 801E96C8 */
    0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100,
    0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001,
};
u32 menu_bit_masks32[32] = { /* 801E96E8 */
    0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000,
};
s32 menu_card_event_results[4] = { 0, -3, -1, -2 }; /* 801E9768 */
u8 menu_card_changed = 0; /* 801E9778: a card changed during a choice */
u8 menu_card_poll_interval = 0x1E; /* 801E9779: frames between card checks */
u8 menu_inside_command = 1; /* 801E977A: inside a command */
s32 menu_detail_tab_images[2] = { 0x159, 0x15A }; /* 801E977C: detail panel tab sprites */
u8 menu_soft_reset_enabled = 1; /* 801E9784: nonzero checks the reset combination */
u8 menu_target_panels_allocated = 0; /* 801E9785: the target panels are allocated */
s32 menu_arts_screen_panel6_widths[3] = { 0x11A, 0x11A, 0xA2 }; /* 801E9788: arts screen window sizes per kind */
s32 menu_arts_screen_panel4_x_table[3] = { 0xDA, 0xDA, 0xC8 }; /* 801E9794 */
s32 menu_arts_screen_panel4_widths[3] = { 0x52, 0x52, 0x64 }; /* 801E97A0 */
/* 801E1544 screen: five sheet images per row (ff none) for its 13 rows
 * (menu_deathblow_screen_build). Its alignment padding holds stray assembler bytes
 * (00 07 2e) that nothing reads, so it stays original data
 * (slot39.classification.txt). */
INCLUDE_ORIGINAL(".data", menu_deathblow_row_images, 0x801E97AC, 68);
u16 menu_arts_screen_usable_art_masks[12] = { 2, 0, 1, 0, 6, 0, 0, 1, 0, 1, 0, 0 }; /* 801E97F0 */
u8 menu_gear_pilots[20] = { 0, 0, 1, 2, 3, 4, 5, 7, 8, 6, 1, 9, 3, 4, 5, 0, 9, 0xF, 0xF, 0xF }; /* 801E9808: pilot character of each gear */
/* card slot (port * 16 + n) of each cursor position */
s32 menu_file_cursor_card_slots[30] = { /* 801E981C */
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
};
/* image block x */
s16 menu_file_slot_x_table[32][2] = { /* 801E9894 */
    { 0x20, 0 }, { 0x28, 0 }, { 0x30, 0 }, { 0x38, 0 }, { 0x40, 0 }, { 0x48, 0 }, { 0x50, 0 }, { 0x58, 0 },
    { 0x60, 0 }, { 0x68, 0 }, { 0x70, 0 }, { 0x78, 0 }, { 0x80, 0 }, { 0x88, 0 }, { 0x90, 0 }, { 0x140, 0 },
    { 0xB0, 0 }, { 0xB8, 0 }, { 0xC0, 0 }, { 0xC8, 0 }, { 0xD0, 0 }, { 0xD8, 0 }, { 0xE0, 0 }, { 0xE8, 0 },
    { 0xF0, 0 }, { 0xF8, 0 }, { 0x100, 0 }, { 0x108, 0 }, { 0x110, 0 }, { 0x118, 0 }, { 0x120, 0 }, { 0x140, 0 },
};
/* image block y */
s16 menu_file_slot_y_table[32][2] = { /* 801E9914 */
    { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 },
    { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0x100, 0 },
    { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 },
    { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0x100, 0 },
};
/* text character x per column */
s32 menu_file_info_char_x_table[21] = { /* 801E9994 */
    0x30, 0x3C, 0x48, 0x54, 0x60, 0x6C, 0x78,
    0x84, 0x90, 0x9C, 0xA8, 0xB4, 0xC0, 0xCC,
    0xD8, 0xE4, 0xF0, 0xFC, 0x108, 0x114, 0x120,
};
s32 menu_file_info_char_y_table[2] = { 0x56, 0x66 }; /* 801E99E8: text character y per row */
s32 menu_file_info_cursor_x = 0x10; /* 801E99F0: cursor sprite x */
s32 menu_unused_file_info_cursor_right = 0x30; /* 801E99F4: unreferenced */
s32 menu_file_info_cursor_y = 0x56; /* 801E99F8: cursor sprite y */
s32 menu_unused_file_info_cursor_bottom = 0x76; /* 801E99FC: unreferenced */
s32 menu_highlight_x_table[7] = { 0x49, 0x48, 0x43, 0x3C, 0x34, 0x26, 0x17 }; /* 801E9A00: highlight positions: x */
s32 menu_choice_highlight_x_table[4] = { 0x25, 0x24, 0x1F, 0x18 }; /* 801E9A1C: choice highlight positions: x */
s32 menu_highlight_y_table[7] = { 0xC9, 0xB5, 0xA2, 0x90, 0x7E, 0x6F, 0x61 }; /* 801E9A2C: highlight positions: y */
s32 menu_choice_highlight_y_table[4] = { 0xC9, 0xB5, 0xA2, 0x90 }; /* 801E9A48: choice highlight positions: y */
s32 menu_marker_x_table[4] = { 0, 0, 0xB4, 0x114 }; /* 801E9A58: marker positions */
s32 menu_marker_y_table[4] = { 0, 0, 0xC8, 0xC8 }; /* 801E9A68 */
s32 menu_field_block_part_x_table[20] = { /* 801E9A78 */
    0x48, 0x30, 0x38, 0x58, 0x30, 0x38, 0x58, 0x30, 0x38, 0x58,
    0x80, 0x88, 0x90, 0x98, 0x98, 0xA8, 0xB0, 0xB0, 0x80, 0x80,
};
s32 menu_field_block_part_y_table[20] = { /* 801E9AC8 */
    0, 8, 8, 8, 0x20, 0x20, 0x20, 0x28, 0x28, 0x28,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x20, 0x28,
};
/* Field block number offsets (x, y). */
s32 menu_field_block_level_x = 0x40; /* 801E9B18: +62 */
s32 menu_field_block_level_y = 8; /* 801E9B1C */
s32 menu_field_block_level2_x = 0x60; /* 801E9B20: +63 */
s32 menu_field_block_level2_y = 8; /* 801E9B24 */
s32 menu_field_block_hp_x = 0x40; /* 801E9B28: hp */
s32 menu_field_block_hp_y = 0x20; /* 801E9B2C */
s32 menu_field_block_max_hp_x = 0x60; /* 801E9B30: hp max */
s32 menu_field_block_max_hp_y = 0x20; /* 801E9B34 */
s32 menu_field_block_ep_x = 0x48; /* 801E9B38: ep */
s32 menu_field_block_ep_y = 0x28; /* 801E9B3C */
s32 menu_field_block_max_ep_x = 0x60; /* 801E9B40: ep max */
s32 menu_field_block_max_ep_y = 0x28; /* 801E9B44 */
s32 menu_field_block_exp_next_a_x = 0x88; /* 801E9B48: exp */
s32 menu_field_block_exp_next_a_y = 0x20; /* 801E9B4C */
s32 menu_field_block_exp_next_b_x = 0x88; /* 801E9B50: exp to next level */
s32 menu_field_block_exp_next_b_y = 0x28; /* 801E9B54 */
s32 menu_field_block_name_x = 0x30; /* 801E9B58: field block name image offset: x */
s32 menu_field_block_name_y = 0x11; /* 801E9B5C: y */
/* detail panel part positions, 24 per layout: x */
s32 menu_detail_part_x_table[48] = { /* 801E9B60 */
    0x50, 0x58, 0x70, 0x78, 0x80, 0xB0, 0xB8, 0xC0, 0xC8, 0xC8, 0xD8, 0xE0,
    0xE0, 0x48, 0x50, 0x70, 0x48, 0x50, 0x70, 0x38, 0x68, 0x80, 0x48, 0x48,
    0x38, 0x40, 0x58, 0x60, 0x68, 0x98, 0xA0, 0xA8, 0xB0, 0xB0, 0xC0, 0xC8,
    0xC8, 0x30, 0x38, 0x38, 0x30, 0x38, 0x38, 0x28, 0x38, 0x38, 0x30, 0x30,
};
/* y */
s32 menu_detail_part_y_table[48] = { /* 801E9C20 */
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 0x2E, 0x2E, 0x2E, 0x36, 0x36, 0x36, 0x46, 0x46, 0x46, 0xE, 0x16,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 0x2E, 0x2E, 0x36, 0x3E, 0x3E, 0x46, 0x4E, 0x36, 0x36, 0xE, 0x16,
};
/* Detail panel number positions (x, y). */
s32 menu_detail_level_x = 0x50; /* 801E9CE0: level */
s32 menu_detail_level_y = 0xE; /* 801E9CE4 */
s32 menu_detail_level2_x = 0x50; /* 801E9CE8: +63 */
s32 menu_detail_level2_y = 0x16; /* 801E9CEC */
s32 menu_detail_hp_x = 0x58; /* 801E9CF0: hp */
s32 menu_detail_hp_y = 0x2E; /* 801E9CF4 */
s32 menu_detail_max_hp_x = 0x78; /* 801E9CF8: hp max */
s32 menu_detail_max_hp_y = 0x2E; /* 801E9CFC */
s32 menu_detail_ep_x = 0x60; /* 801E9D00: ep */
s32 menu_detail_ep_y = 0x36; /* 801E9D04 */
s32 menu_detail_max_ep_x = 0x78; /* 801E9D08: ep max */
s32 menu_detail_max_ep_y = 0x36; /* 801E9D0C */
s32 menu_detail_exp_total_a_x = 0x70; /* 801E9D10: +3c */
s32 menu_detail_exp_total_a_y = 0xE; /* 801E9D14 */
s32 menu_detail_exp_total_b_x = 0x70; /* 801E9D18: +40 */
s32 menu_detail_exp_total_b_y = 0x16; /* 801E9D1C */
s32 menu_detail_exp_next_a_x = 0xB8; /* 801E9D20: exp */
s32 menu_detail_exp_next_a_y = 0xE; /* 801E9D24 */
s32 menu_detail_exp_next_b_x = 0xB8; /* 801E9D28: exp to next level */
s32 menu_detail_exp_next_b_y = 0x16; /* 801E9D2C */
s32 menu_detail_field77_value_x = 0x50; /* 801E9D30: +77..79 value */
s32 menu_detail_field77_value_y = 0x46; /* 801E9D34 */
s32 menu_detail_name_x = 0x48; /* 801E9D38: detail panel name image position: x */
s32 menu_detail_name_y = 0x20; /* 801E9D3C: y */
s32 menu_stat_name_x_table[7] = { 8, 8, 8, 8, 8, 8, 8 }; /* 801E9D40: stat name positions per row: x */
s32 menu_stat_name_y_table[7] = { 0, 8, 0x10, 0x18, 0x20, 0x28, 0x30 }; /* 801E9D5C: y */
s32 menu_stat_bar_x_offset = 0x28; /* 801E9D78: stat bar x offset */
s32 menu_stat_bar_y_offset = 2; /* 801E9D7C: stat bar y offset */
s32 menu_stat_digits_x_offset = 0x68; /* 801E9D80: stat digit x offset */
s32 menu_stat_digits_y_offset = 0; /* 801E9D84: stat digit y offset */
s32 menu_equip_labels_row_y_table[13] = { /* 801E9D88 */
    0x76, 0x85, 0x92, 0x9F, 0xAE, 0x1E, 0x39, 0x46, 0x53, 0x1E, 0x39, 0x53, 0x6C,
};
s32 menu_equip_screen_part_cursor_y_table[8] = { 0x15, 0x31, 0x3D, 0x4B, 0x15, 0x31, 0x4B, 0x64 }; /* 801E9DBC: equipment screen: part cursor y by special * 4 + part */
/* arts list cost x positions */
s32 menu_arts_screen_cost_x_table[14] = { /* 801E9DDC */
    0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0xFC, 0x114,
};
/* arts list cost y positions */
s32 menu_arts_screen_cost_y_table[14] = { /* 801E9E14 */
    0x12, 0x12, 0x22, 0x22, 0x32, 0x32, 0x42, 0x42, 0x52, 0x52, 0x62, 0x62, 0xAC, 0xAC,
};
s32 menu_party_label_x_table[3] = { 0x98, 0x70, 0xA8 }; /* 801E9E4C: party label positions: x */
s32 menu_party_label_y_table[3] = { 0x76, 0x92, 0xAE }; /* 801E9E58: y */
s32 menu_field_command_label_x_offsets[8] = { 0x12, 6, 0x12, 6, 6, 0x12, 0, 0 }; /* 801E9E64: label x offsets: field menu commands */
s32 menu_title_load_label_x_offsets[4] = { 0, 6, 0, 0 }; /* 801E9E84: title file screen commands */
s32 menu_choice_label_x_offsets[3] = { 0xC, 6, 0xC }; /* 801E9E94: choice label x offsets */
s32 menu_file_command_label_x_offsets[9] = { 0x12, 0xC, 0xC, 0xB0, 0x80, 0xB0, 0x76, 0x92, 0xAE }; /* 801E9EA0 */
s32 menu_label_mode1_x_table[8] = { 0x98, 0x98, 0x98, 0xBC, 0xBC, 0xBC, 0, 0 }; /* 801E9EC4: label x (mode 1) */
u16 menu_label_mode1_y = 0x93; /* 801E9EE4: label y (mode 1) */
/* label x (modes 2, 5 from 8) */
s32 menu_label_mode2_x_table[16] = { /* 801E9EE8 */
    0xD2, 0xD2, 0xD2, 0xF6, 0xF6, 0xF6, 0xE4, 0x10C,
    0xD2, 0xD2, 0xD2, 0xF6, 0xF6, 0xF6, 0xD4, 0x10C,
};
s32 menu_label_mode2_y_table[2] = { 0x8C, 0xAC }; /* 801E9F28: label y per row (mode 2) */
s32 menu_label_mode3_y_table[6] = { 0x12, 0x2C, 0x12, 0x2C, 0x46, 0x60 }; /* 801E9F30: label y per row (mode 3) */
s32 menu_gear_command_label_x_offsets[8] = { 0xC, 6, 6, 0x12, 0, 0, 0xC, 6 }; /* 801E9F48: status command label x offsets (page 0 and 6) */
s32 menu_label_mode6_x_table[2] = { 6, 0xC }; /* 801E9F68: label x (mode 6) */
s32 menu_label_mode6_y_table[6] = { 0, 0, 0x98, 0x70, 0x76, 0x92 }; /* 801E9F70: label y (mode 6) */
s32 menu_sound_mode_label_x_offsets[4] = { 6, 0xC, 6, 0 }; /* 801E9F88: sound mode label x offsets */
s32 menu_save_view_frame_x_table[9] = { 0x4C, 0x54, 0x74, 0x4C, 0x54, 0x74, 0x4C, 0x54, 0x74 }; /* 801E9F98: view frame x (first view) */
s32 menu_save_view_frame_y_table[9] = { 0x4E, 0x4E, 0x4E, 0x62, 0x62, 0x62, 0x6A, 0x6A, 0x6A }; /* 801E9FBC: view frame y */
s32 menu_file_info_play_time_x_table[9] = { 0x34, 0x4C, 0x1C, 0x24, 0x2C, 0x3C, 0x44, 0x54, 0x5C }; /* 801E9FE0: play time: x of the two separators and seven digits */
s32 menu_save_view_image_x_table[3] = { 0xC, 0x1C, 0x2C }; /* 801EA004 */
s32 menu_save_view_image_y_table[3] = { 0x4C, 0x54, 0x5C }; /* 801EA010 */
s32 menu_save_view_level_x = 0x5C; /* 801EA01C: view level digits x */
s32 menu_save_view_level_y = 0x4E; /* 801EA020: view level digits y */
s32 menu_unused_save_view_level2_x = 0x7C; /* 801EA024: unreferenced */
s32 menu_unused_save_view_level2_y = 0x4E; /* 801EA028: unreferenced */
s32 menu_save_view_hp_x = 0x5C; /* 801EA02C: view HP digits x, y */
s32 menu_save_view_hp_y = 0x62; /* 801EA030 */
s32 menu_save_view_max_hp_x = 0x7C; /* 801EA034: view maximum HP digits x, y */
s32 menu_save_view_max_hp_y = 0x62; /* 801EA038 */
s32 menu_save_view_ep_x = 0x64; /* 801EA03C: view EP digits x, y */
s32 menu_save_view_ep_y = 0x6A; /* 801EA040 */
s32 menu_save_view_max_ep_x = 0x7C; /* 801EA044: view maximum EP digits x, y */
s32 menu_save_view_max_ep_y = 0x6A; /* 801EA048 */
s32 menu_save_title_x = 0x70; /* 801EA04C: save title x, y */
s32 menu_save_title_y = 0x74; /* 801EA050 */
/* Target panel layouts: x and y anchors. */
MenuAnchor menu_target_panel_x_anchors[1] = { /* 801EA054 */
    { 0x48, 0x50, 0x70, 0x48, 0x50, 0x70, 0x48, 0x50, 0x70, 0x18, 0x58, 0x78, 0x58, 0x78, 0x60, 0x78, 0x48 },
};
MenuAnchor menu_target_panel_gear_x_anchors[1] = { /* 801EA098 */
    { 0x48, 0x50, 0x70, 0x48, 0x50, 0x58, 0x50, 0x50, 0x50, 0x18, 0x58, 0x78, 0x60, 0x60, 0x60, 0x78, 0x48 },
};
MenuAnchor menu_target_panel_y_anchors[1] = { /* 801EA0DC */
    { 0x1E, 0x1E, 0x1E, 0x36, 0x36, 0x36, 0x3E, 0x3E, 0x3E, 0x16, 0x1E, 0x1E, 0x36, 0x36, 0x3E, 0x3E, 0x28 },
};
MenuAnchor menu_target_panel_gear_y_anchors[1] = { /* 801EA120 */
    { 0x1E, 0x1E, 0x1E, 0x36, 0x36, 0x3E, 0x36, 0x36, 0x36, 0x16, 0x1E, 0x1E, 0x36, 0x3E, 0x3E, 0x3E, 0x28 },
};
s32 menu_member_mark_x_table[2] = { 0, 0x114 }; /* 801EA164: party window sprite x */
s32 menu_member_mark_y_table[4] = { 0x66, 0x76, 0x76, 0xC }; /* 801EA16C: party window sprite y per row */
s32 menu_name_label_x_table[4] = { 0x34, 0xA0, 0x24, 0xA0 }; /* 801EA17C: name label position per mode (801d36e0): x */
s32 menu_name_label_y_table[4] = { 0x80, 0xC2, 0x20, 0x5A }; /* 801EA18C: y */
/* field menu command cursor images */
MenuCommandImages menu_field_command_images[7] = { /* 801EA19C */
    { 0x109, 0x131 }, { 0x10A, 0x130 }, { 0x10B, 0x12F }, { 0x10C, 0x12E },
    { 0x10D, 0x12D }, { 0x10E, 0x12C }, { 0x10F, 0x12B },
};
/* title file screen cursor images */
MenuCommandImages menu_title_load_command_images[3] = { /* 801EA1D4 */
    { 0x109, 0x125 }, { 0x10A, 0x124 }, { 0x10B, 0x123 },
};
/* per command: four choices of cursor and label images (ffff none) */
s32 menu_choice_window_images[88] = { /* 801EA1EC */
    0x110, 0x142, 0x111, 0x141, 0x112, 0x140, 0xFFFF, 0xFFFF,
    0x110, 0x12A, 0x111, 0x129, 0x112, 0x127, 0xFFFF, 0xFFFF,
    0x110, 0x13E, 0x111, 0x13D, 0x112, 0x13C, 0x113, 0x13F,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x134, 0x111, 0x133, 0x112, 0x132, 0xFFFF, 0xFFFF,
    0x110, 0x131, 0x111, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x12A, 0x111, 0x129, 0x112, 0x128, 0xFFFF, 0xFFFF,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x12A, 0x111, 0x129, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
};
/* field block part images, ffff none */
s32 menu_field_block_part_images[20] = { /* 801EA34C */
    0xFFFF, 0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E,
    0x17, 0x28, 0x3B, 0x37, 0x37, 0x15, 0x1F, 0x1F, 0xFFFF, 0xFFFF,
};
/* detail panel part sprites, 24 per layout, ffff none */
s32 menu_detail_part_images[48] = { /* 801EA39C */
    0x15, 0x1F, 0xE, 0x3B, 0x33, 0x17, 0x28, 0x3B, 0x37, 0x37, 0x15, 0x1F,
    0x1F, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E, 0xE6, 0x91, 0xE7, 0xFFFF, 0xFFFF,
    0x15, 0x1F, 0xE, 0x3B, 0x33, 0x17, 0x28, 0x3B, 0x37, 0x37, 0x15, 0x1F,
    0x1F, 0x11, 0x19, 0x3E, 0xF, 0x15, 0x3E, 0xE6, 0x3E, 0x3E, 0xFFFF, 0xFFFF,
};
/* stat name sprites, seven per start row */
s32 menu_stat_name_images[14] = { /* 801EA45C */
    0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE4, 0xEB,
    0xDE, 0xF3, 0xF4, 0xF6, 0xEB, 0xF7, 0xF7,
};
/* view frame images (ffff none) */
s32 menu_save_view_frame_images[18] = { /* 801EA494 */
    0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
};
/* status panel layout sprites, nine per layout, ffff none */
s32 menu_status_panel_layout_images[18] = { /* 801EA4DC */
    0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E,
    0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0x19, 0x19, 0x19,
};
/* Label lists. */
u8 menu_command_label_ids[4] = { 0x09, 0x0A, 0x0B, 0x0C }; /* 801EA524: label image layout */
u8 menu_field_command_label_ids[8] = { 0x0A, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x02 }; /* 801EA528: field menu command labels */
u8 menu_title_load_label_ids[2] = { 0x08, 0x01 }; /* 801EA530 */
u8 menu_party_label_ids[6] = { 0x12, 0x11, 0x10, 0x13, 0x14, 0x15 }; /* 801EA534: party label layout */
/* file screen command labels: save, then load */
u8 menu_file_command_label_ids[12] = { /* 801EA53C */
    0x1B, 0x1A, 0x19, 0x18, 0x16, 0x17,
    0x1B, 0x1A, 0x18, 0x19, 0x16, 0x17,
};
u8 menu_item_arts_label_ids[8] = { 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F }; /* 801EA548: item and arts screen labels */
u8 menu_item_target_label_ids[8] = { 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x7A, 0x7B }; /* 801EA550: item target labels */
u8 menu_equip_screen_label_ids[12] = { /* 801EA558 */
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75,
    0x70, 0x71, 0x72, 0x7B, 0x7B, 0x75,
};
u8 menu_deathblow_screen_label_ids[2] = { 0x76, 0x76 }; /* 801EA564: 801E1014 screen labels */
/* status screen labels: a character's page, then a gear's */
u8 menu_gear_command_label_ids[12] = { /* 801EA568 */
    0x12, 0x11, 0x77, 0x78, 0x13, 0x14,
    0x12, 0x11, 0x77, 0x79, 0x13, 0x14,
};
u8 menu_sound_mode_label_ids[4] = { 0x9B, 0x9C, 0x9D, 0x7B }; /* 801EA574: sound mode labels */
/* Label image positions in the sheet, per row pair: x / 4, then y. */
s32 menu_name_image_vram_x_table[19] = { /* 801EA578 */
    0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18,
};
s32 menu_name_image_vram_y_table[19] = { /* 801EA5C4 */
    0x3B, 0x48, 0x48, 0x55, 0x55, 0x62, 0x62, 0x6F, 0x6F, 0x7C, 0x7C, 0x89, 0x89, 0x96, 0x96, 0xA3, 0xA3, 0xB0, 0xB0,
};
/* Two-byte (Shift JIS) codes of the ASCII characters 0x20-0x7F. */
u16 menu_ascii_to_sjis_table[96] = { /* 801EA610 */
    0x8140, 0x8149, 0x814A, 0x8194, 0x8190, 0x8193, 0x8195, 0x8166,
    0x8169, 0x816A, 0x8196, 0x817B, 0x8143, 0x817C, 0x8144, 0x815E,
    0x824F, 0x8250, 0x8251, 0x8252, 0x8253, 0x8254, 0x8255, 0x8256,
    0x8257, 0x8258, 0x8146, 0x8147, 0x8183, 0x8181, 0x8184, 0x8148,
    0x8197, 0x8260, 0x8261, 0x8262, 0x8263, 0x8264, 0x8265, 0x8266,
    0x8267, 0x8268, 0x8269, 0x826A, 0x826B, 0x826C, 0x826D, 0x826E,
    0x826F, 0x8270, 0x8271, 0x8272, 0x8273, 0x8274, 0x8275, 0x8276,
    0x8277, 0x8278, 0x8279, 0x816D, 0x818F, 0x816E, 0x81F4, 0x8151,
    0x8165, 0x8281, 0x8282, 0x8283, 0x8284, 0x8285, 0x8286, 0x8287,
    0x8288, 0x8289, 0x828A, 0x828B, 0x828C, 0x828D, 0x828E, 0x828F,
    0x8290, 0x8291, 0x8292, 0x8293, 0x8294, 0x8295, 0x8296, 0x8297,
    0x8298, 0x8299, 0x829A, 0x816F, 0x8162, 0x8170, 0x814D, 0x8140,
};

/* The unit's uninitialized variables, zero in the file after all units'
 * initialized data, each in a slot of whole words (decomp/Makefile). */
static u8 menu_card_own_save_flags[2][16];          /* 801EA6D0: per port and save slot: a save of this game exists */
static s32 menu_unused_card_scan_word;              /* 801EA6F0: unreferenced */
static u8 *menu_card_unread_last_matched_save_info; /* 801EA6F4: the save information of the last matched file */
static u8 menu_card_unread_access_restarted;        /* 801EA6F8 */
static s32 menu_stat_bar_from;                      /* 801EA6FC: gauge: from, to, difference and lengths */
static s32 menu_stat_bar_unread_to;                 /* 801EA700 */
static s32 menu_stat_bar_change;                    /* 801EA704 */
static s32 menu_stat_bar_length;                    /* 801EA708 */
static s32 menu_stat_bar_change_length;             /* 801EA70C */
static u8 menu_stat_bar_change_color;               /* 801EA710 */
static u8 menu_stat_bar_change_sign_image;          /* 801EA714 */
static CdlCB menu_saved_cd_sync_callback;           /* 801EA718: the CD sync, ready and read callbacks, saved in card mode */
static CdlCB menu_saved_cd_ready_callback;          /* 801EA71C */
static CdlCB menu_saved_cd_read_callback;           /* 801EA720 */

/* 801C531C: Run the command at `offset` past the top cursor (0 back, 1 load/save file,
 * 2..6 the field-menu screens, 7/8 the title file screen's load and new game,
 * 9 reset), then restore the command window. Returns 0 when the menu ends. */
u8 menu_top_command_run(u8 offset) {
    u8 redraw;
    u8 stay;

    menu_inside_command = 1;
    stay = 1;
    redraw = 1;
    switch (menu_state_current->cursor + offset) {
    case 7:
        redraw = menu_sound_mode_screen_run();
        break;
    case 8:
        if (menu_file_screen_run(1, 0)) {
            mode_result_code = 2;
            stay = 0;
        }
        break;
    case 1:
        redraw = menu_file_screen_run(0, menu_save_command_stays_open);
        break;
    case 2:
        redraw = menu_gear_command_run();
        break;
    case 3:
        redraw = menu_arts_command_run(menu_state_current->first_member, 1);
        break;
    case 4:
        redraw = menu_item_screen_run();
        break;
    case 5:
        redraw = menu_equip_command_run(menu_state_current->first_member, 1);
        break;
    case 6:
        redraw = menu_character_command_run();
        break;
    case 9:
        mode_load_initial_game_data();
    case 0:
        redraw = 0;
        stay = 0;
        break;
    }
    menu_state_current->card->mode = 0;
    if (redraw) {
        menu_view_start_zoom_out();
        if (menu_state_screen == 0) {
            menu_field_blocks_slide(1, 0);
        } else if (menu_state_screen == 2) {
            menu_label_render_table(8, menu_state_current->list_labels, menu_title_load_label_ids, menu_state_current->flags->list_labels_shown);
            menu_state_current->prims->width = 0x4c;
        }
    }
    menu_top_command_close(offset);
    menu_member_marks_hide();
    menu_state_current->images->dim = 0;
    menu_state_current->images->dimmed = 1;
    menu_state_current->flags->sprite_shown = 1;
    menu_state_current->flags->cursor_shown = 1;
    menu_state_current->cursor_shown = 0xff;
    menu_state_current->flags->lists_shown = 0;
    menu_soft_reset_enabled = 1;
    return stay;
}

/* 801C55A0: The field menu's command loop: move the cursor over the seven commands and
 * run the chosen one until the menu is left. */
void menu_field_menu_run(void) {
    u8 ok;
    u8 stay;

    stay = 1;
    do {
        menu_run_frame();
        switch (menu_state_current->input) {
        case 4:
            ok = 1;
            if (menu_state_current->cursor == 2 && menu_state_current->fighters == 0) {
                ok = 0;
                menu_play_sound(4);
            }
            if (ok) {
                menu_state_current->images->dim = 1;
                menu_highlight_hide();
                menu_label_clear_shown(8, menu_state_current->flags->list_labels_shown);
                stay = menu_top_command_run(0);
            }
            break;
        case 5:
            stay = 0;
            break;
        case 1:
            if (menu_state_current->cursor != 0) {
                menu_state_current->cursor--;
            } else {
                menu_state_current->cursor = 6;
            }
            break;
        case 3:
            if (++menu_state_current->cursor >= 7) {
                menu_state_current->cursor = 0;
            }
            break;
        }
        if (menu_state_current->cursor != menu_state_current->cursor_shown) {
            menu_command_window_set_cursor(7, menu_state_current->cursor, menu_field_command_images);
            menu_label_place(8, menu_state_current->list_labels, menu_field_command_label_ids, menu_field_command_label_x_offsets, menu_state_current->flags->list_labels_shown,
                          menu_state_current->cursor, 0, 0);
            menu_state_current->cursor_shown = menu_state_current->cursor;
        }
    } while (stay);
}

/* 801C57A4: Menu kind 6: wait for the pad to be released, check the cards and, when a
 * file can be saved, run the save file screen; then the loaded-disc check. */
void menu_cd_change_run(void) {
    u8 save;
    u8 found;

    menu_state_screen_parameter = 1;
    menu_inside_command = 0;
    save = 1;
    menu_view_start_zoom_in();
    while (menu_state_current->view_motion != 0) {
        menu_run_frame();
    }
    menu_markers_layout(0);
    found = menu_notice_ask_yes_no(0x7d, 0xff, 1);
    if ((found || menu_yes_no_cancelled != 0) && menu_notice_ask_yes_no(0x80, 0xff, 1) != 0) {
        save = 0;
    }
    menu_markers_hide();
    if (save) {
        menu_saving_at_cd_change = 1;
        menu_save_command_stays_open = 1;
        menu_top_command_run(0);
        menu_save_command_stays_open = 0;
        menu_saving_at_cd_change = 0;
    }
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    menu_cd_ask_for_disc(menu_state_screen_parameter);
}

/* 801C58EC: The title screen's file screen loop: choose among its three commands; after
 * 600 idle frames on the first screen it returns with mode_result_code = 1. */
void menu_title_load_menu_run(void) {
    u8 stay;

    stay = 1;
    menu_command_window_open(4, menu_title_load_command_images);
    menu_label_render_table(8, menu_state_current->list_labels, menu_title_load_label_ids, menu_state_current->flags->list_labels_shown);
    menu_state_current->frame_counter = 0;
    do {
        menu_run_frame();
        switch (menu_state_current->input) {
        case 4:
            menu_state_current->images->dim = 1;
            menu_highlight_hide();
            menu_label_clear_shown(8, menu_state_current->flags->list_labels_shown);
            menu_state_current->prims->width = 0x40;
            stay = menu_top_command_run(7);
            menu_soft_reset_enabled = 0;
            menu_state_current->frame_counter = 0;
            break;
        case 1:
            if (menu_state_current->cursor != 0) {
                menu_state_current->cursor--;
            } else {
                menu_state_current->cursor = 2;
            }
            menu_state_current->frame_counter = 0;
            break;
        case 3:
            if (++menu_state_current->cursor >= 3) {
                menu_state_current->cursor = 0;
            }
            menu_state_current->frame_counter = 0;
            break;
        }
        if (menu_state_current->cursor != menu_state_current->cursor_shown) {
            menu_command_window_set_cursor(3, menu_state_current->cursor, menu_title_load_command_images);
            menu_label_place(8, menu_state_current->list_labels, menu_title_load_label_ids, menu_title_load_label_x_offsets, menu_state_current->flags->list_labels_shown,
                          menu_state_current->cursor, 0, 0);
            menu_state_current->cursor_shown = menu_state_current->cursor;
        }
        if (cd_get_disc_number() == 1 && (u32)menu_state_current->frame_counter > 600) {
            stay = 0;
            mode_result_code = 1;
        }
    } while (stay);
    menu_label_clear_shown(8, menu_state_current->flags->list_labels_shown);
}

/* 801C5B54: Allocate and clear (nonzero) or free (zero) the memory-card state block. */
void menu_alloc_or_free_card_state(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x5034, 0);
        menu_state_current->card = block;
        bzero(block, 0x5034);
    } else {
        heap_free(menu_state_current->card);
    }
}

/* 801C5BB8: Allocate and clear (nonzero) or free (zero) the party block. */
void menu_alloc_or_free_flags(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x6c, 0);
        menu_state_current->flags = block;
        bzero(block, 0x6c);
    } else {
        heap_free(menu_state_current->flags);
    }
}

/* 801C5C1C: Allocate and clear (nonzero) or free (zero) the screen image block. */
void menu_alloc_or_free_screen_images(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x1194, 0);
        menu_state_current->images = block;
        bzero(block, 0x1194);
    } else {
        heap_free(menu_state_current->images);
    }
}

/* 801C5C80: Allocate and clear (nonzero) or free (zero) the sprite lists (+354). */
void menu_alloc_or_free_sprite_lists(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x140c, 0);
        menu_state_current->lists = block;
        bzero(block, 0x140c);
    } else {
        heap_free(menu_state_current->lists);
    }
}

/* 801C5CE4: Allocate and clear (nonzero) or free (zero) the data table directory. */
void menu_alloc_or_free_table_directory(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0xcc, 0);
        menu_state_current->tables = block;
        bzero(block, 0xcc);
    } else {
        heap_free(menu_state_current->tables);
    }
}

/* 801C5D48: Allocate and clear (nonzero) or free (zero) the first field-menu block. */
void menu_alloc_or_free_field_menu_block(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x328, 0);
        menu_state_current->field_menu = block;
        bzero(block, 0x328);
    } else {
        heap_free(menu_state_current->field_menu);
    }
}

/* 801C5DAC: Allocate and clear (nonzero) or free (zero) the second field-menu block. */
void menu_alloc_or_free_field_menu2_block(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x374, 0);
        menu_state_current->field_menu2 = block;
        bzero(block, 0x374);
    } else {
        heap_free(menu_state_current->field_menu2);
    }
}

/* 801C5E10: Allocate and clear (nonzero) or free (zero) the shared primitive block. */
void menu_alloc_or_free_prims(u8 allocate) {
    if (allocate) {
        void *block = heap_alloc(0x15c, 0);
        menu_state_current->prims = block;
        bzero(block, 0x15c);
    } else {
        heap_free(menu_state_current->prims);
    }
}

/* 801C5E74: Allocate and clear (nonzero) or free (zero) the three field blocks. */
void menu_alloc_or_free_field_blocks(u8 allocate) {
    s32 i;

    if (allocate) {
        for (i = 0; i < 3; i++) {
            void *block = heap_alloc(0x127c, 0);
            menu_state_current->field_blocks[i] = block;
            bzero(block, 0x127c);
        }
    } else {
        for (i = 0; i < 3; i++) {
            heap_free(menu_state_current->field_blocks[i]);
        }
    }
}

/* 801C5F10: Allocate the blocks of the menu kind in menu_state_screen (the card state for the
 * title file screen and kind 6; the card state and field blocks for the field
 * menu). */
void menu_alloc_kind_blocks(void) {
    void *block;

    menu_alloc_or_free_flags(1);
    menu_alloc_or_free_screen_images(1);
    menu_alloc_or_free_sprite_lists(1);
    menu_alloc_or_free_table_directory(1);
    menu_alloc_or_free_prims(1);
    block = heap_alloc(0x14c, 0);
    menu_state_current->markers = block;
    bzero(block, 0x14c);
    switch (menu_state_screen) {
    case 0:
        menu_alloc_or_free_card_state(1);
        menu_alloc_or_free_field_menu_block(1);
        menu_alloc_or_free_field_menu2_block(1);
        menu_alloc_or_free_field_blocks(1);
        break;
    case 2:
    case 6:
        menu_alloc_or_free_card_state(1);
        break;
    }
}

/* 801C5FE4: Leave the menu: keep the field menu's cursor, stop drawing, free the sound
 * bank and every block, then the state itself. */
void menu_shut_down(void) {
    switch (menu_state_screen) {
    case 0:
        menu_highlight_hide();
        menu_field_blocks_slide(0, 0);
        menu_state_current->flags->field_menu2_shown = 0;
        menu_state_current->flags->field_menu_shown = 0;
        if (menu_state_screen_parameter == 0) {
            menu_state_saved_cursor = menu_state_current->cursor;
        }
        break;
    case 2:
    case 6:
        break;
    }
    menu_run_frame();
    menu_run_frame();
    menu_state_current->drawing = 0;
    menu_run_frame();
    do {
        menu_run_frame();
    } while (menu_state_current->buffer_index != 0);
    menu_alloc_or_free_flags(0);
    menu_alloc_or_free_screen_images(0);
    menu_alloc_or_free_sprite_lists(0);
    menu_alloc_or_free_table_directory(0);
    menu_alloc_or_free_prims(0);
    heap_free(menu_state_current->markers);
    heap_free(menu_state_current->sheet);
    heap_free(menu_state_current->label_text);
    heap_free(menu_state_current->labels[0].pixels);
    if (menu_state_debug_start != 0) {
        sound_stop_bank_effects(menu_state_current->effects);
        menu_run_frame();
        sound_remove_effect_bank(menu_state_current->effects);
        menu_run_frame();
        heap_free(menu_state_current->effects);
    }
    switch (menu_state_screen) {
    case 0:
        menu_alloc_or_free_card_state(0);
        menu_alloc_or_free_field_menu_block(0);
        menu_alloc_or_free_field_menu2_block(0);
        menu_alloc_or_free_field_blocks(0);
        heap_free(menu_state_current->panels[0]);
        heap_free(menu_state_current->growth[0]);
        heap_free(menu_state_current->panels[1]);
        heap_free(menu_state_current->growth[1]);
        break;
    case 2:
    case 6:
        menu_alloc_or_free_card_state(0);
        break;
    }
    heap_free(menu_state_current);
}

/* 801C62A8: The menu mode: allocate, set up the screen, run the menu of kind
 * menu_state_screen (then, after the title file screen, the disc check of the loaded
 * file) and tear down. */
void menu_main(void) {
    u8 kind;

    menu_alloc_kind_blocks();
    menu_init_screen();
    menu_state_current->drawing = 1;
    menu_state_current->sounds = 1;
    kind = menu_state_screen;
    switch (kind) {
    case 0:
        menu_field_menu_open();
        menu_field_menu_run();
        break;
    case 2:
        mode_result_code = 0;
        menu_title_load_menu_run();
        menu_state_current->flags->images_shown = 0;
        menu_state_current->flags->sprite_shown = 0;
        menu_state_current->flags->cursor_shown = 0;
        switch (mode_result_code) {
        case 0:
            menu_cd_ask_for_disc(0);
            break;
        case 2:
            menu_cd_ask_for_disc(game_data.vars[82]);
            break;
        }
        break;
    case 6:
        menu_cd_change_run();
        break;
    }
    menu_shut_down();
}

/* 801C6400: Initialize the card scan and read the scenario's title from its text
 * file, skipping complete lines and two-byte characters. */
void menu_card_init_and_read_title(void) {
    u8 *file;
    u8 *text;
    s32 i;
    s32 j;
    u16 line;
    u8 c;
    s32 newline;
    s32 decrement;

    for (i = 0; i < 2; i++) {
        menu_state_current->card->scanned[i] = 0;
        menu_state_current->card->unknown4f8a[i] = 0;
        menu_state_current->card->unknown4f8c[i] = 0xff;
    }
    menu_state_current->card->mode = 0;
    for (i = 0; i < 32; i++) {
        menu_state_current->card->files[i].state = 0;
        menu_state_current->card->file_slots[i] = 0xff;
    }
    line = game_data.vars[0];
    i = 0;
    cd_select_directory(0x10, 1);
    file = heap_alloc(cd_get_aligned_file_size(1), 1);
    cd_read_file(1, file, 0, 0x80);
    cd_sync_reads(0);
    text = file;
    if (line != 0) {
        decrement = 0xFFFF;
        newline = '\n';
        do {
        next:
            c = text[i];
            if (c >= 0x80) {
                i += 2;
                goto next;
            }
            if (c != newline) {
                i += 1;
                goto next;
            }
            line += decrement;
            i += 1;
        } while (line != 0);
    }
    for (j = 0; j < 30; j++) {
        menu_state_current->card->title[j] = text[i++];
    }
    menu_state_current->card->unknown501a = menu_state_current->card->unknown501b = 0;
    cd_select_directory(0x10, 0);
    heap_free(text);
}

/* 801C65F4: Load the menu resources: the card file header template (name prefixes,
 * "SC" header, icon palette and pixels), the TIM list, sprite sheet and
 * label text, the party's portraits and, with sound, the effect bank. */
void menu_load_resources(void) {
    TIM_IMAGE tim;
    s32 tex[3 * 6]; /* per entry 80026338's six outputs: -, mode, clut x/y, page x/y */
    MenuResources *res;
    void *data;
    s32 i;
    u8 id;

    res = menu_state_resource_file;
    text_relocate_offset_table(res);
    data = text_unpack_lzss_alloc(res->files[0], 1);
    OpenTIM(data);
    ReadTIM(&menu_state_current->card->icon);
    strcpy(menu_state_current->card->prefix, "BASLUS-00664");
    strcpy((char *)menu_state_current->card->other_prefix, "BASLUS-01160");
    menu_state_current->card->save_magic[0] = 'S';
    menu_state_current->card->save_magic[1] = 'C';
    menu_state_current->card->save_icon_flag = 0x11;
    menu_state_current->card->save_blocks = 1;
    bzero(menu_state_current->card->save_title, sizeof(menu_state_current->card->save_title));
    memmove(menu_state_current->card->save_palette, menu_state_current->card->icon.caddr, sizeof(menu_state_current->card->save_palette));
    memmove(menu_state_current->card->save_icon, menu_state_current->card->icon.paddr, sizeof(menu_state_current->card->save_icon));
    heap_free(data);
    data = text_unpack_lzss_alloc(res->files[1], 1);
    model_load_tim_list(data);
    heap_free(data);
    menu_state_current->sheet = text_unpack_lzss_alloc(res->files[2], 0);
    menu_state_current->label_text = text_unpack_lzss_alloc(res->files[3], 0);
    sprite_sheet_get_texture(menu_state_current->sheet, 0xe0, &tex[0], &tex[1], &tex[2],
                  &tex[3], &tex[4], &tex[5]);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x14b, &tex[0], &tex[1], &tex[2],
                  &tex[3], &tex[4], &tex[5]);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x14c, &tex[6], &tex[7], &tex[8],
                  &tex[9], &tex[10], &tex[11]);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x14d, &tex[12], &tex[13], &tex[14],
                  &tex[15], &tex[16], &tex[17]);
    tex[10] += 0xc;
    data = text_unpack_lzss_alloc(res->files[4], 1);
    for (i = 0; i < 3; i++) {
        id = menu_state_current->flags->party[i];
        if (id != 0xff) {
            OpenTIM((u_long *)((u8 *)data + id * 0xb20));
            ReadTIM(&tim);
            tim.crect->x = tex[i * 6 + 2];
            tim.crect->y = tex[i * 6 + 3];
            tim.prect->x = tex[i * 6 + 4];
            tim.prect->y = tex[i * 6 + 5];
            LoadImage(tim.crect, tim.caddr);
            LoadImage(tim.prect, tim.paddr);
        }
    }
    DrawSync(0);
    heap_free(data);
    if (menu_state_debug_start != 0) {
        cd_select_directory(0x10, 2);
        sound_effect_bank = heap_alloc(cd_get_aligned_file_size(5), 0);
        cd_read_file(5, sound_effect_bank, 0, 0x80);
        cd_sync_reads(0);
        cd_select_directory(0x10, 0);
        sound_add_effect_bank(sound_effect_bank);
    }
    menu_state_current->effects = sound_effect_bank;
    heap_free(res);
}

/* 801C6AA0: Set up the party: the starting top cursor, which characters are present,
 * the party slots (and which have a gear), the first occupied slot; then
 * load the resources. */
void menu_init_party(MenuState *state) {
    s32 i;
    u16 members;
    s32 id;

    if (menu_state_screen == 0 && menu_state_screen_parameter == 0) {
        menu_state_current->cursor = menu_state_saved_cursor;
    } else {
        menu_state_current->cursor = 1;
    }
    menu_state_current->cursor_shown = 0xff;
    menu_state_current->card_poll_timer = 0x3c;
    menu_state_current->cards_present = 0;
    menu_state_current->unknown335 = 0;
    menu_state_current->party_count = 0;
    members = game_data.joined & game_data.available & 0x7ff;
    for (i = 0; i < 16; i++) {
        if (menu_test_bit(members, i)) {
            menu_state_current->present[i] = 1;
        } else {
            menu_state_current->present[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        menu_state_current->flags->ready[i] = 0;
        id = game_data.party[i];
        if (id != 0xff && menu_state_current->present[id]) {
            menu_state_current->flags->party[i] = id;
            menu_state_current->party_count++;
            if (game_data.characters[menu_state_current->flags->party[i]].gearId != 0xff) {
                menu_state_current->flags->ready[i] = 1;
                menu_state_current->fighters++;
            }
        } else {
            menu_state_current->flags->party[i] = 0xff;
        }
    }
    for (i = 0; i < 3; i++) {
        if (menu_state_current->flags->party[i] != 0xff) {
            menu_state_current->first_member = i;
            break;
        }
    }
    menu_load_resources();
}

/* 801C6D4C: Mark the buffer being built as sent (the draw callback). */
void menu_reset_buffer_index(void) {
    menu_state_current->buffer_index = 0;
}

/* 801C6D5C: Clear the resource load state. */
void menu_file_screen_reset_state(void) {
    menu_state_current->unknown4cc = 0;
    menu_state_current->unknown4d0 = 0;
    menu_state_current->load_state = 0;
    menu_state_current->unknown4d9 = 0;
    menu_state_current->unknown4d4 = 0;
}

/* 801C6D90: Upload a 16-entry palette at (0, 1c0) whose entry 1 is white. */
void menu_upload_label_palette(void) {
    RECT rect;
    u8 reserved[8]; /* the original frame reserves 8 unused bytes */
    u16 *clut;

    clut = heap_alloc(0x20, 0);
    bzero((u_char *)clut, 0x20);
    clut[1] = 0x7fff;
    rect.x = 0;
    rect.y = 0x1c0;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, (u_long *)clut);
    DrawSync(0);
    heap_free(clut);
}

/* 801C6E0C: Set up the label text: the font position, the label pixel block and the
 * four label image records, and the label palette. */
void menu_init_labels(void) {
    text_load_palette(0, 0x1d1);
    menu_state_current->labels[0].pixels = heap_alloc(0x38e, 0);
    menu_label_render_pairs(menu_state_current->labels, menu_command_label_ids, 0, 4);
    menu_upload_label_palette();
}

/* 801C6E68: Read the four sprite sheet records used by the menu. */
void menu_read_sheet_entries(void) {
    u8 reserved[40]; /* the original frame reserves 40 unused bytes */

    sprite_sheet_get_texture(menu_state_current->sheet, 0xfe, &menu_state_current->sheet_entries[0].first, &menu_state_current->sheet_entries[0].mode,
                  &menu_state_current->sheet_entries[0].clut_x, &menu_state_current->sheet_entries[0].clut_y,
                  &menu_state_current->sheet_entries[0].page_x, &menu_state_current->sheet_entries[0].page_y);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x103, &menu_state_current->sheet_entries[1].first, &menu_state_current->sheet_entries[1].mode,
                  &menu_state_current->sheet_entries[1].clut_x, &menu_state_current->sheet_entries[1].clut_y,
                  &menu_state_current->sheet_entries[1].page_x, &menu_state_current->sheet_entries[1].page_y);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x100, &menu_state_current->sheet_entries[2].first, &menu_state_current->sheet_entries[2].mode,
                  &menu_state_current->sheet_entries[2].clut_x, &menu_state_current->sheet_entries[2].clut_y,
                  &menu_state_current->sheet_entries[2].page_x, &menu_state_current->sheet_entries[2].page_y);
    sprite_sheet_get_texture(menu_state_current->sheet, 0x101, &menu_state_current->sheet_entries[3].first, &menu_state_current->sheet_entries[3].mode,
                  &menu_state_current->sheet_entries[3].clut_x, &menu_state_current->sheet_entries[3].clut_y,
                  &menu_state_current->sheet_entries[3].page_x, &menu_state_current->sheet_entries[3].page_y);
}

/* 801C6F70: Set up the frame primitives of both buffers: the shaded backdrop, two dark
 * green separator lines, the grey full-screen fade and two draw modes. */
void menu_init_highlight_and_fade_prims(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    menu_highlight_hide();
    for (i = 0; i < 2; i++) {
        menu_init_gradient_quad(&menu_state_current->prims->shade[i], 0x80, 0x80, 0);
        SetSemiTrans(&menu_state_current->prims->shade[i], 1);
        SetLineF3(&menu_state_current->prims->upper[i]);
        setRGB0(&menu_state_current->prims->upper[i], 0, 0x40, 0);
        SetLineF3(&menu_state_current->prims->lower[i]);
        setRGB0(&menu_state_current->prims->lower[i], 0, 0x40, 0);
        SetPolyF4(&menu_state_current->prims->fade[i]);
        setXY4(&menu_state_current->prims->fade[i], 0, 0, 0x140, 0, 0, 0xe0, 0x140, 0xe0);
        setRGB0(&menu_state_current->prims->fade[i], 0x80, 0x80, 0x80);
        SetSemiTrans(&menu_state_current->prims->fade[i], 1);
        SetDrawMode(&menu_state_current->prims->mode_label[i], 0, 0, GetTPage(0, 0, 0x140, 0x80), &window);
        SetDrawMode(&menu_state_current->prims->mode_sprite[i], 0, 0, GetTPage(0, 2, 0x180, 0), &window);
    }
}

/* 801C72BC: Load (codes below 10h, read from file 2 of directory 10h) or release
 * (10h + the same code) the menu data set `code` into the table directory and
 * the screen blocks. */
void menu_load_or_release_data_set(u8 code) {
    MenuDataArchive *archive;
    s32 i;
    u8 id;
    u8 gear;

    if (code < 0x10) {
        cd_select_directory(0x10, 0);
        archive = heap_alloc(cd_get_aligned_file_size(2), 1);
        cd_read_file(2, archive, 0, 0x80);
        cd_sync_reads(0);
        text_relocate_offset_table(archive);
    }
    switch (code) {
    case 0:
        menu_state_current->tables->items = text_unpack_lzss_alloc(archive->items, 0);
        menu_state_current->item_list->unk1180 = text_unpack_lzss_alloc(archive->unk3C, 0);
        break;
    case 1:
        menu_state_current->tables->engines = text_unpack_lzss_alloc(archive->engines, 0);
        menu_state_current->tables->frames = text_unpack_lzss_alloc(archive->frames, 0);
        menu_state_current->tables->parts = text_unpack_lzss_alloc(archive->parts, 0);
        menu_state_current->tables->gear_accessories = text_unpack_lzss_alloc(archive->unk50, 0);
        break;
    case 2:
        for (i = 0; i < 3; i++) {
            id = menu_state_current->flags->party[i];
            if (id != 0xff) {
                menu_state_current->tables->arts[menu_state_current->flags->party[i]] = text_unpack_lzss_alloc(archive->effects[id], 0);
            }
        }
        menu_state_current->arts_list->texts = text_unpack_lzss_alloc(archive->unk40, 0);
        break;
    case 3:
        menu_state_current->tables->equipment = text_unpack_lzss_alloc(archive->weapons, 0);
        menu_state_current->tables->accessories = text_unpack_lzss_alloc(archive->accessories, 0);
        menu_state_current->tables->gear_weapons = text_unpack_lzss_alloc(archive->unkAC, 0);
        menu_state_current->tables->gear_accessories = text_unpack_lzss_alloc(archive->unk50, 0);
        break;
    case 4:
        for (i = 0; i < 3; i++) {
            id = menu_state_current->flags->party[i];
            if (id != 0xff) {
                menu_state_current->tables->arts[menu_state_current->flags->party[i]] = text_unpack_lzss_alloc(archive->effects[id], 0);
            }
        }
        menu_state_current->status_list->unk2578 = text_unpack_lzss_alloc(archive->unkB0, 0);
        break;
    case 5:
    case 6:
        for (i = 0; i < 3; i++) {
            id = menu_state_current->flags->party[i];
            if (id != 0xff) {
                gear = game_data.characters[id].gearId;
                if (gear != 0xff) {
                    (menu_state_current->tables->arts + 11)[game_data.characters[menu_state_current->flags->party[i]].gearId] =
                        text_unpack_lzss_alloc(archive->gears[gear], 0);
                    menu_set_gear_fuel_art_cost(menu_state_current->tables, game_data.characters[menu_state_current->flags->party[i]].gearId);
                }
            }
        }
        if (code == 5) {
            menu_state_current->arts_list->texts = text_unpack_lzss_alloc(archive->unk54, 0);
        } else {
            menu_state_current->arts_list->texts = text_unpack_lzss_alloc(archive->unk58, 0);
        }
        break;
    case 7:
        menu_state_current->equip_list->texts[0] = text_unpack_lzss_alloc(archive->unkD4[0], 0);
        menu_state_current->equip_list->texts[1] = text_unpack_lzss_alloc(archive->unkD4[1], 0);
        menu_state_current->equip_list->texts[2] = text_unpack_lzss_alloc(archive->unkD4[2], 0);
        menu_state_current->equip_list->texts[3] = text_unpack_lzss_alloc(archive->unkD4[3], 0);
        break;
    case 0x10:
        heap_free(menu_state_current->tables->items);
        heap_free(menu_state_current->item_list->unk1180);
        break;
    case 0x11:
        heap_free(menu_state_current->tables->engines);
        heap_free(menu_state_current->tables->frames);
        heap_free(menu_state_current->tables->parts);
        heap_free(menu_state_current->tables->gear_accessories);
        break;
    case 0x12:
        for (i = 0; i < 3; i++) {
            id = menu_state_current->flags->party[i];
            if (id != 0xff) {
                heap_free(menu_state_current->tables->arts[id]);
            }
        }
        heap_free(menu_state_current->arts_list->texts);
        break;
    case 0x13:
        heap_free(menu_state_current->tables->equipment);
        heap_free(menu_state_current->tables->accessories);
        heap_free(menu_state_current->tables->gear_weapons);
        heap_free(menu_state_current->tables->gear_accessories);
        break;
    case 0x14:
        for (i = 0; i < 3; i++) {
            id = menu_state_current->flags->party[i];
            if (id != 0xff) {
                heap_free(menu_state_current->tables->arts[id]);
            }
        }
        heap_free(menu_state_current->status_list->unk2578);
        break;
    case 0x15:
    case 0x16:
        for (i = 0; i < 3; i++) {
            id = menu_state_current->flags->party[i];
            if (id != 0xff) {
                gear = game_data.characters[id].gearId;
                if (gear != 0xff) {
                    heap_free((menu_state_current->tables->arts + 11)[gear]);
                }
            }
        }
        heap_free(menu_state_current->arts_list->texts);
        break;
    case 0x17:
        heap_free(menu_state_current->equip_list->texts[0]);
        heap_free(menu_state_current->equip_list->texts[1]);
        heap_free(menu_state_current->equip_list->texts[2]);
        heap_free(menu_state_current->equip_list->texts[3]);
        break;
    }
    if (code < 0x10) {
        heap_free(archive);
    }
}

/* 801C7B0C: Set up the screen: the copied screen area, the environments, labels,
 * palettes and sheet records, then the card state and load state. */
void menu_init_screen(void) {
    MenuState *state = menu_state_current;

    state->images->screen.x = 0x2c0;
    state->images->screen.y = 0x100;
    state->images->screen.w = 0x140;
    state->images->screen.h = 0xe0;
    state->prims->width = 0x40;
    menu_init_party(state);
    menu_reset_buffer_index();
    menu_init_labels();
    menu_init_highlight_and_fade_prims();
    menu_read_sheet_entries();
    switch (menu_state_screen) {
    case 2:
        menu_state_current->prims->width = 0x4c;
    case 0:
    case 6:
        menu_card_init_and_read_title();
        menu_file_screen_reset_state();
        break;
    }
}

/* 801C7BF4: One menu frame: poll the pads, swap and clear the buffers, build the frame
 * (updates, input, sounds), wait for the previous frame, show this one and copy
 * the screen area into the other buffer. */
void menu_run_frame(void) {
    MenuBuffer *buffer;
    s32 other;

    if (*mode_disc_mode_pointer != -1) {
        /* ASPSX "break 1": code 1 in the upper code field (0x400 for maspsx) */
        __asm__ volatile("break 0x400");
    }
    menu_read_input();
    if (menu_soft_reset_enabled != 0) {
        boot_check_soft_reset();
    }
    buffer = &menu_state_current->buffers[0];
    if (menu_state_current->current == buffer) {
        buffer = &menu_state_current->buffers[1];
    }
    menu_state_current->current = buffer;
    menu_state_current->buffer_index = menu_state_current->buffer_index == 0;
    ClearOTagR(menu_state_current->current->ot, 16);
    mode_get_random_byte_in_range(0, 0xff);
    menu_view_update();
    menu_state_current->frame_counter++;
    menu_split_play_time(pad_vblank_count);
    menu_play_time_window_update();
    menu_draw_screen();
    other = menu_state_current->buffer_index == 0;
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&menu_state_current->current->draw);
    PutDispEnv(&menu_state_current->current->disp);
    MoveImage(&menu_state_current->images->screen, 0, other * 0xe0);
    DrawOTag(&menu_state_current->current->ot[15]);
    menu_card_poll_ports();
    menu_card_list_unscanned_ports();
}

/* 801C7D78: Decode this frame's input into menu_state_current->input (0-3 directions, 4-7 the
 * face buttons, 9/10 the shoulder buttons, 12 select, 8 none), playing the
 * cursor sounds; while the pad is disconnected, pause the sound and the play
 * time. */
void menu_read_input(void) {
    u8 waiting;
    u8 paused;
    s32 frames;
    u8 input;

    waiting = 1;
    paused = 0;
    do {
        if (pad_get_controller_kind(0) == 0) {
            if (!paused) {
                paused++;
                sound_silence_voices();
                frames = pad_vblank_count;
            }
        } else {
            waiting--;
            if (paused) {
                sound_restore_voices();
                pad_vblank_count = frames;
            }
        }
    } while (waiting);
    input = 8;
    if (pad_has_queue_overflowed() != 0) {
        pad_clear_queue();
    } else {
        while (pad_dequeue_state() != 0) {
            if (pad_port0_repeated & 0x2000) {
                input = 0;
                menu_play_sound(1);
                break;
            }
            if (pad_port0_repeated & 0x4000) {
                input = 1;
                menu_play_sound(1);
                break;
            }
            if (pad_port0_repeated & 0x8000) {
                input = 2;
                menu_play_sound(1);
                break;
            }
            if (pad_port0_repeated & 0x1000) {
                input = 3;
                menu_play_sound(1);
                break;
            }
            if (pad_port0_pressed & 0x20) {
                input = 4;
                menu_play_sound(2);
                break;
            }
            if (pad_port0_pressed & 0x40) {
                input = 5;
                menu_play_sound(3);
                break;
            }
            if (pad_port0_pressed & 0x80) {
                input = 6;
                break;
            }
            if (pad_port0_pressed & 0x10) {
                input = 7;
                break;
            }
            if (pad_port0_repeated & 4) {
                input = 10;
                menu_play_sound(1);
                break;
            }
            if (pad_port0_repeated & 8) {
                input = 9;
                menu_play_sound(1);
                break;
            }
            if (pad_port0_pressed & 0x100) {
                input = 12;
                break;
            }
        }
    }
    menu_state_current->input = input;
}

/* 801C7F34: Split the play time `frames` into the digits of hhh:mm:ss (the hundreds,
 * tens and units of hours, then tens and units of minutes and seconds). */
void menu_split_play_time(u32 frames) {
    menu_state_current->time[0] = frames / 21600000;
    frames %= 21600000;
    menu_state_current->time[1] = frames / 2160000;
    frames %= 2160000;
    menu_state_current->time[2] = frames / 216000;
    frames %= 216000;
    menu_state_current->time[3] = frames / 36000;
    frames %= 36000;
    menu_state_current->time[4] = frames / 3600;
    frames %= 3600;
    menu_state_current->time[5] = frames / 600;
    frames %= 600;
    menu_state_current->time[6] = frames / 60;
}

/* 801C80B8: Split `value` into nine decimal digits, blanking (ff) the leading zeros. */
void menu_split_digits(u32 value) {
    u32 unit;
    s32 i;

    unit = 100000000;
    for (i = 0; i < 9; i++) {
        menu_state_current->digits[i] = value / unit;
        value %= unit;
        unit /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (menu_state_current->digits[i] != 0) {
            if (menu_state_current->digits[i - 1] == 0) {
                menu_state_current->digits[i - 1] = 0xff;
            }
            return;
        }
        menu_state_current->digits[i - 1] = 0xff;
    }
}

/* 801C8164: Initialise a gradient quad: the top edge colour (r, g, b), the bottom black. */
void menu_init_gradient_quad(POLY_G4 *poly, u8 r, u8 g, u8 b) {
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

/* 801C81E0: Start mover `slot` along the line (x0, y0)-(x1, y1) at `speed` steps per
 * frame: the major axis steps one unit (8.8 fixed point), the minor axis its
 * slope. */
void menu_mover_start(s32 x0, s32 y0, s32 x1, s32 y1, u8 speed, u8 slot) {
    s32 dx;
    s32 dy;

    menu_state_current->movers[slot].x0 = x0;
    menu_state_current->movers[slot].y0 = y0;
    menu_state_current->movers[slot].x1 = x1;
    menu_state_current->movers[slot].y1 = y1;
    if (x1 < x0) {
        dx = x0 - x1;
        menu_state_current->movers[slot].neg_x = 1;
    } else {
        dx = x1 - x0;
        menu_state_current->movers[slot].neg_x = 0;
    }
    if (y1 < y0) {
        dy = y0 - y1;
        menu_state_current->movers[slot].neg_y = 1;
    } else {
        dy = y1 - y0;
        menu_state_current->movers[slot].neg_y = 0;
    }
    if (dx >= dy) {
        menu_state_current->movers[slot].step_x = 0x100;
        menu_state_current->movers[slot].step_y = (dy << 8) / dx;
    } else {
        menu_state_current->movers[slot].step_y = 0x100;
        menu_state_current->movers[slot].step_x = (dx << 8) / dy;
    }
    menu_state_current->movers[slot].speed = speed;
    menu_state_current->movers[slot].acc_x = 0;
    menu_state_current->movers[slot].acc_y = 0;
    menu_state_current->movers[slot].done = 0;
}

/* 801C8324: Advance mover `slot` by its speed and mark it done once the major axis
 * passes the end point. */
void menu_mover_step(u8 slot) {
    s32 i;

    for (i = 0; i < menu_state_current->movers[slot].speed; i++) {
        if (menu_state_current->movers[slot].neg_x) {
            menu_state_current->movers[slot].acc_x -= menu_state_current->movers[slot].step_x;
        } else {
            menu_state_current->movers[slot].acc_x += menu_state_current->movers[slot].step_x;
        }
        if (menu_state_current->movers[slot].neg_y) {
            menu_state_current->movers[slot].acc_y -= menu_state_current->movers[slot].step_y;
        } else {
            menu_state_current->movers[slot].acc_y += menu_state_current->movers[slot].step_y;
        }
    }
    if (menu_state_current->movers[slot].step_x == 0x100) {
        if (menu_state_current->movers[slot].neg_x) {
            if (menu_state_current->movers[slot].acc_x / 256 + menu_state_current->movers[slot].x0 < menu_state_current->movers[slot].x1) {
                menu_state_current->movers[slot].done = 1;
            }
        } else if (menu_state_current->movers[slot].x1 < menu_state_current->movers[slot].acc_x / 256 + menu_state_current->movers[slot].x0) {
            menu_state_current->movers[slot].done = 1;
        }
    } else if (menu_state_current->movers[slot].neg_y) {
        if (menu_state_current->movers[slot].acc_y / 256 + menu_state_current->movers[slot].y0 < menu_state_current->movers[slot].y1) {
            menu_state_current->movers[slot].done = 1;
        }
    } else if (menu_state_current->movers[slot].y1 < menu_state_current->movers[slot].acc_y / 256 + menu_state_current->movers[slot].y0) {
        menu_state_current->movers[slot].done = 1;
    }
}

/* 801C851C: Set the four corners of a screen rectangle as vertices centred on (a0, 70). */
void menu_set_rect_verts(SVECTOR *v, u16 x, u16 y, u16 w, u16 h) {
    v[0].vx = x - 0xa0;
    v[0].vy = y - 0x70;
    v[0].vz = 0;
    v[1].vx = x + w - 0xa0;
    v[1].vy = y - 0x70;
    v[1].vz = 0;
    v[2].vx = x - 0xa0;
    v[2].vz = 0;
    v[3].vx = x + w - 0xa0;
    v[3].vz = 0;
    v[2].vy = y + h - 0x70;
    v[3].vy = y + h - 0x70;
}

/* 801C8574: Play menu sound effect `sound` from the menu's bank when sounds are on. */
void menu_play_sound(s32 sound) {
    if (menu_state_current->sounds != 0) {
        sound_play_effect_on_last_channels((menu_state_current->effects->id << 16) | (u8)sound, sound);
    }
}

/* 801C85C0: Bit `bit` of the menu_bit_masks_msb_first masks. */
u16 menu_get_bit_mask_msb_first(u8 bit) {
    return menu_bit_masks_msb_first[bit];
}

/* 801C85DC: Bit `bit` of the menu_bit_masks masks. */
u16 menu_get_bit_mask(u8 bit) {
    return menu_bit_masks[bit];
}

/* 801C85F8: All bits but `bit` (menu_bit_masks_msb_first). */
u16 menu_get_clear_mask_msb_first(u8 bit) {
    return ~menu_bit_masks_msb_first[bit];
}

/* 801C861C: All bits but `bit` (menu_bit_masks). */
u16 menu_get_clear_mask(u8 bit) {
    return ~menu_bit_masks[bit];
}

/* 801C8640: Test bit `bit` (menu_bit_masks_msb_first) of `flags`. */
u16 menu_test_bit_msb_first(u16 flags, u8 bit) {
    return menu_bit_masks_msb_first[bit] & flags;
}

/* 801C865C: Test bit `bit` (menu_bit_masks) of `flags`. */
u16 menu_test_bit(u16 flags, u8 bit) {
    return menu_bit_masks[bit] & flags;
}

/* 801C8678: Test bit `bit` (menu_bit_masks32) of `flags`. */
u32 menu_test_bit32(u32 flags, u8 bit) {
    return flags & menu_bit_masks32[bit];
}

/* 801C8694: Ask for disc `disc` + 1 until it is in the drive, showing the change-disc
 * notice (and, for a wrong disc, the wrong-disc message for 29 frames). */
void menu_cd_ask_for_disc(u8 disc) {
    u8 checking;
    u8 frames;

    menu_view_start_zoom_in();
    checking = 1;
    while (menu_state_current->view_motion != 0) {
        menu_run_frame();
    }
    menu_markers_layout(0);
    while (checking) {
        if (cd_get_disc_number() == disc + 1) {
            checking = 0;
        } else {
            menu_cd_stop_drive();
            menu_notice_open(disc * 3 - 0x7d);
            if (menu_cd_check_disc(disc + 1) != 0) {
                frames = 0x1d;
                menu_notice_close();
                menu_notice_open(0x89);
                do {
                    frames--;
                    menu_run_frame();
                } while (frames);
                menu_notice_close();
                menu_run_frame();
            } else {
                menu_notice_close();
                checking = 0;
            }
        }
    }
    menu_markers_hide();
}

/* 801C87C4: Discard pending memory-card events. */
void menu_card_discard_events(void) {
    UnDeliverEvent(0xf4000001, 4);
    UnDeliverEvent(0xf4000001, 0x8000);
    UnDeliverEvent(0xf4000001, 0x100);
    UnDeliverEvent(0xf4000001, 0x2000);
}

/* 801C881C: Wait for a memory-card event; returns 0 done, 1 error, 2 timeout, 3 new card. */
u8 menu_card_wait_event(void) {
    for (;;) {
        if (TestEvent(menu_state_current->card->events[3]) == 1) {
            menu_card_discard_events();
            return 3;
        }
        if (TestEvent(menu_state_current->card->events[1]) == 1) {
            menu_card_discard_events();
            return 1;
        }
        if (TestEvent(menu_state_current->card->events[0]) == 1) {
            menu_card_discard_events();
            return 0;
        }
        if (TestEvent(menu_state_current->card->events[2]) == 1) {
            menu_card_discard_events();
            return 2;
        }
    }
}

/* 801C891C: Start a card check on `channel` and wait for its event: the menu_card_event_results
 * result for it, or -1 when the check cannot start. */
s32 menu_card_check_channel(s32 channel) {
    if (_card_info(channel) == 0) {
        return -1;
    }
    return menu_card_event_results[menu_card_wait_event()];
}

/* 801C8960: Finish a frame, then enable the four card events in a critical section. */
void menu_card_close_events(void) {
    menu_run_frame();
    EnterCriticalSection();
    CloseEvent(menu_state_current->card->events[0]);
    CloseEvent(menu_state_current->card->events[1]);
    CloseEvent(menu_state_current->card->events[2]);
    CloseEvent(menu_state_current->card->events[3]);
    ExitCriticalSection();
}

/* 801C8A10: Check the card in `port`; a removed card clears its file listing. Returns 0
 * when the check could not run (-2). */
u8 menu_card_check_port(u8 port) {
    u8 ok;
    s32 result;
    s32 i;

    ok = 1;
    menu_state_current->card->present[port] = 1;
    result = menu_card_check_channel(port ? 0x10 : 0);
    if (result == 0 && menu_state_current->card->result[port] == -1) {
        result = 1;
        menu_state_current->card->result[port] = 0;
    } else {
        menu_state_current->card->result[port] = result;
    }
    if (result == -1) {
        menu_state_current->card->unknown4f8a[port] = 0;
        menu_state_current->card->present[port] = 0;
        menu_card_blocks_used[port] = 0;
        for (i = 0; i < 16; i++) {
            menu_state_current->card->file_slots[port * 16 + i] = 0xff;
            menu_state_current->card->ours[port * 16 + i] = 0;
            menu_state_current->card->files[port * 16 + i].state = 0;
        }
    }
    if (result == -2) {
        ok = 0;
    }
    if (menu_state_current->card->present_shown[port] != menu_state_current->card->present[port]) {
        menu_state_current->card->scanned[port] = 0;
        menu_state_current->card->present_shown[port] = menu_state_current->card->present[port];
    }
    return ok;
}

/* 801C8BEC: While the card screen is up, recheck both ports every
 * menu_card_poll_interval frames. */
void menu_card_poll_ports(void) {
    if (menu_state_current->card->mode != 0) {
        if (++menu_state_current->card_poll_timer > menu_card_poll_interval) {
            menu_card_check_port(0);
            menu_card_check_port(1);
            if (menu_state_current->card->result[0] == -1 && menu_state_current->card->result[1] == -1) {
                menu_state_current->cards_present = 0;
            }
            menu_state_current->card_poll_timer = 0;
        }
    }
}

/* 801C8CA4: Unless port `port` could not be checked, show the card notice for 59 frames. */
void menu_card_retry_wait(u8 port) {
    MenuCard *card;
    s32 frames;

    card = menu_state_current->card;
    if (card->result[port] != -2) {
        card->mode = 2;
        frames = 59;
        do {
            frames--;
            menu_run_frame();
        } while (frames != 0);
        menu_state_current->card->mode = 0;
    }
}

/* 801C8D1C: Unless port `port` could not be checked, wait 59 vertical blanks. */
void menu_card_retry_wait_vsync(u8 port) {
    s32 frames;

    if (menu_state_current->card->result[port] != -2) {
        frames = 59;
        do {
            VSync(0);
            frames--;
        } while (frames != 0);
    }
}

/* 801C8D78: List the directory of card port `port`: clear the port's slot entries and
 * save marks, then copy each file name into the port's file entries.
 * Stores and returns the file count. */
u8 menu_card_list_directory(u8 port) {
    struct DIRENTRY dir;
    char device[8];
    s32 i;
    s32 retry;
    u8 count;

    menu_card_blocks_used[port] = 0;
    retry = 5;
    for (i = 0; i < 16; i++) {
        menu_state_current->card->file_slots[port * 16 + i] = 0xff;
        menu_card_own_save_flags[port][i] = 0;
    }
    if (port == 0) {
        __builtin_memcpy(device, "bu00:", 6);
    } else {
        __builtin_memcpy(device, "bu10:", 6);
    }
    while (--retry != 0) {
        count = 0;
        if (firstfile(device, &dir) == &dir) {
            do {
                strcpy(menu_state_current->card->files[port * 16 + count].name, dir.name);
                count++;
            } while (nextfile(&dir) == &dir);
        }
        menu_state_current->card->unknown4f8a[port] = count;
        break;
    }
    return count;
}

/* 801C8EE8: List the files of each port not yet scanned; in mode 1 a port with files
 * marks the cards present. */
void menu_card_list_unscanned_ports(void) {
    switch (menu_state_current->card->mode) {
    case 1:
        if (menu_state_current->card->scanned[0] == 0) {
            if (menu_card_list_directory(0)) {
                menu_state_current->cards_present = 1;
            }
            menu_state_current->card->scanned[0] = 1;
        }
        if (menu_state_current->card->scanned[1] == 0) {
            if (menu_card_list_directory(1)) {
                menu_state_current->cards_present = 1;
            }
            menu_state_current->card->scanned[1] = 1;
        }
        break;
    case 2:
        if (menu_state_current->card->scanned[0] == 0) {
            menu_card_list_directory(0);
            menu_state_current->card->scanned[0] = 1;
        }
        if (menu_state_current->card->scanned[1] == 0) {
            menu_card_list_directory(1);
            menu_state_current->card->scanned[1] = 1;
        }
        break;
    }
}

/* 801C9038: Read the 512-byte first block of card file `name` into `dst`; 0 on success,
 * -1 on failure. */
s32 menu_card_read_first_block(char *name, void *dst) {
    s32 fd;

    fd = open(name, 3);
    if (fd == -1) {
        return -1;
    }
    if (read(fd, dst, 0x200) != 0x200) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/* 801C90B0: Read the first block of file `file` on port `port` into its header buffer
 * and list the file's blocks in the port's slot entries. */
void menu_card_read_file_head(u8 port, u8 file) {
    char device[8];
    char name[64];
    s32 retry;
    s32 i;

    retry = 1;
    if (port == 0) {
        __builtin_memcpy(device, "bu00:", 6);
    } else {
        __builtin_memcpy(device, "bu10:", 6);
    }
    strcpy(name, device);
    strcat(name, menu_state_current->card->files[port * 16 + file].name);
    while (menu_card_read_first_block(name, menu_state_current->card->heads[port * 16 + file]) == -1) {
        if (--retry == 0) {
            break;
        }
        menu_card_retry_wait(port);
    }
    for (i = 0; i < menu_state_current->card->heads[port * 16 + file][3]; i++) {
        menu_state_current->card->file_slots[port * 16 + menu_state_current->card->file_count] = file + port * 16;
        menu_state_current->card->file_count++;
    }
}

/* 801C9270: Mark the files of `port` whose names carry this game's prefix and note the
 * save slot each holds. */
void menu_card_mark_own_saves(s32 port) {
    s32 i;
    s32 j;
    s32 match;
    u8 *header;
    u8 *saves;

    for (i = 0; i < 16; i++) {
        menu_state_current->card->ours[port * 16 + i] = 0;
    }
    for (i = 0; i < 15; i++) {
        j = 0;
        match = 1;
        for (; j < 12; j++) {
            if (menu_state_current->card->files[menu_state_current->card->file_slots[port * 16 + i]].name[j] !=
                menu_state_current->card->prefix[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            menu_state_current->card->ours[port * 16 + i] = 1;
            header = menu_state_current->card->heads[menu_state_current->card->file_slots[port * 16 + i]];
            menu_card_unread_last_matched_save_info = header + 0x100;
            saves = menu_card_own_save_flags[port];
            saves[header[0x123]] = 1;
        }
    }
}

/* 801C93A8: Refresh the cards before a save or load. With no card in either port: show
 * message 23 and, still none, wait a second and return 1; otherwise reset
 * the card events and listings. With cards: relist each port whose file
 * count changed (erase the temporary file, read each file's header and icon)
 * while the cards stay as they were, mark this game's files and place the
 * cursor marker. */
u8 menu_card_refresh(void) {
    char path[64];
    u8 present[2];
    u8 unused[48]; /* unused in the original; reserves 48 bytes */
    MenuState *state;
    s32 port;
    s32 i;
    s32 wait;
    u8 noCard;
    u8 marked;
    u8 stop;

    menu_card_unread_access_restarted = 0;
    menu_state_current->card->mode = 2;
    menu_run_frame();
    /* Always 0 (a byte shifted right by 8), but only combine sees that: both
     * cse passes keep `marked` a register copy (move s6,s4) and the cursor
     * marker code still tests it, so its store is never reached. */
    noCard = menu_state_current->card->present[0] >> 8;
    marked = noCard;
    if (*(u16 *)menu_state_current->card->present == 0) {
        menu_notice_close();
        menu_state_current->flags->card_message_shown = 0;
        menu_state_current->flags->file_info_shown = 0;
        menu_run_frame();
        menu_notice_open(0x23);
        menu_state_current->load_state = 1;
        menu_state_current->flags->markers_shown = 0;
        menu_state_current->card->unknown4f80 = 0xff;
        menu_state_current->card->cursor = 0;
        if (*(u16 *)menu_state_current->card->present == 0) {
            menu_state_current->card->unknown4f8c[0] = menu_state_current->card->unknown4f8c[1] = 0xff;
            menu_run_frame();
            wait = 59;
            do {
                VSync(0);
            } while (--wait != 0);
            noCard = 1;
        }
        if (!noCard) {
            menu_card_restart_access();
            menu_state_current->card->present_shown[0] = menu_state_current->card->present_shown[1] = 0xff;
            menu_state_current->card->scanned[0] = 0;
            menu_state_current->card->scanned[1] = 0;
            menu_state_current->card_poll_timer = 0x3c;
            menu_run_frame();
            menu_card_unread_access_restarted = 1;
        }
        menu_notice_close();
    }
    menu_card_poll_interval = 1;
    present[0] = menu_state_current->card->present[0];
    present[1] = menu_state_current->card->present[1];
    if (!noCard) {
        menu_state_current->sounds = 0;
        for (port = 0; port < 2; port++) {
            stop = 0;
            if (menu_state_current->card->unknown4f8a[port] != menu_state_current->card->unknown4f8c[port] &&
                menu_state_current->card->unknown4f8a[port] != 0) {
                menu_card_restart_access();
                for (i = 0; i < 16; i++) {
                    menu_state_current->card->file_slots[port * 16 + i] = 0xff;
                }
                if (port == 0) {
                    __builtin_memcpy(path, "bu00:", 6);
                } else {
                    __builtin_memcpy(path, "bu10:", 6);
                }
                strcat(path, "__tmp_file");
                delete(path);
                menu_state_current->card->file_count = 0;
                menu_card_blocks_used[port] = 0;
                for (i = 0; i < menu_state_current->card->unknown4f8a[port]; i++) {
                    if (menu_state_current->card->present[port] != 0 && present[0] == menu_state_current->card->present[0] &&
                        present[1] == menu_state_current->card->present[1]) {
                        menu_card_read_file_head(port, i);
                        menu_save_icon_upload(port * 16 + i);
                        menu_state_current->card->files[port * 16 + i].state = 1;
                        menu_run_frame();
                        if (menu_state_current->card->scanned[port] != 0) {
                            continue;
                        }
                        stop = port + 1;
                    } else {
                        stop = 2;
                    }
                    port = 2;
                    noCard = 0;
                    break;
                }
                if (!stop) {
                    for (; i < 16; i++) {
                        menu_state_current->card->files[port * 16 + i].state = 0;
                    }
                    menu_state_current->card->unknown4f8c[port] = menu_state_current->card->unknown4f8a[port];
                }
            }
            if (!stop && menu_state_current->card->unknown4f8a[port] == 0) {
                for (i = 0; i < 16; i++) {
                    menu_state_current->card->files[port * 16 + i].state = 0;
                }
                menu_state_current->card->unknown4f8c[port] = 0xff;
            }
        }
        if (!stop) {
            menu_card_mark_own_saves(0);
            menu_card_mark_own_saves(1);
        }
    }
    if (!stop) {
        if (marked) {
            menu_state_current->flags->file_info_shown = 1;
        }
        state = menu_state_current;
        if (state->flags->markers_shown != 0 && state->markers->at_cursor[0] != 0) {
            setXY4(&state->markers->polys[state->markers->buffer[0]],
                   menu_file_slot_x_table[menu_file_cursor_card_slots[state->card->cursor]][0] + 8, menu_file_slot_y_table[menu_file_cursor_card_slots[state->card->cursor]][0] - 6,
                   menu_file_slot_x_table[menu_file_cursor_card_slots[state->card->cursor]][0] + 0x18, menu_file_slot_y_table[menu_file_cursor_card_slots[state->card->cursor]][0] - 6,
                   menu_file_slot_x_table[menu_file_cursor_card_slots[state->card->cursor]][0] + 8, menu_file_slot_y_table[menu_file_cursor_card_slots[state->card->cursor]][0] + 0xa,
                   menu_file_slot_x_table[menu_file_cursor_card_slots[state->card->cursor]][0] + 0x18, menu_file_slot_y_table[menu_file_cursor_card_slots[state->card->cursor]][0] + 0xa);
        }
    }
    menu_state_current->sounds = 1;
    menu_card_poll_interval = 0x1e;
    return noCard;
}

/* 801C9BCC: Whether the cursor slot suits `mode`: 0 a file is there, 1 this game's file
 * is there (both need files listed), 2 its card is present; other modes always
 * suit. A suitable slot sets the load state to 2. */
s32 menu_file_cursor_is_suitable(s32 mode) {
    MenuCard *card;
    s32 cursor;
    s32 ok;

    card = menu_state_current->card;
    ok = 1;
    cursor = card->cursor;
    switch (mode) {
    case 0:
        if (*(u32 *)card->scanned & 0xffff0000) {
            if (card->present[menu_file_cursor_card_slots[cursor] / 16]) {
                if (card->file_slots[menu_file_cursor_card_slots[cursor]] == 0xff) {
                    ok = 0;
                }
            } else {
                ok = 0;
            }
        } else {
            ok = 0;
        }
        break;
    case 1:
        if (*(u32 *)card->scanned & 0xffff0000) {
            if (card->present[menu_file_cursor_card_slots[cursor] / 16] && card->file_slots[menu_file_cursor_card_slots[cursor]] != 0xff) {
                if (!card->ours[menu_file_cursor_card_slots[cursor]]) {
                    ok = 0;
                }
            } else {
                ok = 0;
            }
        } else {
            ok = 0;
        }
        break;
    case 2:
        if (!card->present[menu_file_cursor_card_slots[cursor] / 16]) {
            ok = 0;
        }
        break;
    }
    if (ok) {
        menu_state_current->load_state = 2;
    }
    return ok;
}

/* 801C9D34: The first cursor position of the present cards whose slot suits `mode`
 * (as 801c9bcc), or ff; modes 0 and 1 stop at once when no files are listed.
 * Sets the load state to 2. */
s32 menu_file_cursor_find_first(s32 mode) {
    s32 found;
    s32 searching;
    s32 end;
    s32 first;
    s32 i;

    found = 0xff;
    searching = 1;
    end = 30;
    first = menu_state_current->card->present[0] == 0 ? 15 : 0;
    if (menu_state_current->card->present[1] == 0) {
        end = 15;
    }
    for (i = first; i < end && searching; i++) {
        switch (mode) {
        case 0:
            if (*(u32 *)menu_state_current->card->scanned & 0xffff0000) {
                if (menu_state_current->card->present[menu_file_cursor_card_slots[i] / 16] &&
                    menu_state_current->card->file_slots[menu_file_cursor_card_slots[i]] != 0xff) {
                    found = i;
                    searching = 0;
                }
            } else {
                searching = 0;
            }
            break;
        case 1:
            if (*(u32 *)menu_state_current->card->scanned & 0xffff0000) {
                if (menu_state_current->card->present[menu_file_cursor_card_slots[i] / 16] &&
                    menu_state_current->card->file_slots[menu_file_cursor_card_slots[i]] != 0xff && menu_state_current->card->ours[menu_file_cursor_card_slots[i]]) {
                    found = i;
                    searching = 0;
                }
            } else {
                searching = 0;
            }
            break;
        case 2:
            if (menu_state_current->card->present[menu_file_cursor_card_slots[i] / 16]) {
                found = i;
                searching = 0;
            }
            break;
        }
    }
    menu_state_current->load_state = 2;
    return found;
}

/* 801C9EF4: Move the cursor from `slot` a row down for `mode`: 0 to the next slot
 * below holding a file, else to the first file after the next row; 1 the
 * same for this game's files; 2 a row down when that card is present.
 * Mode 2 keeps the shape of the other modes' scans as single-pass loops. */
void menu_file_cursor_move_down(s32 mode, s32 slot) {
    s32 i;

    switch (mode) {
    case 0:
        for (; slot + 3 < 30; slot += 3) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot + 3]] != 0xff) {
                menu_state_current->card->cursor = slot + 3;
                break;
            }
        }
        if (slot + 3 >= 30) {
            i = menu_state_current->card->cursor + 3;
            if (i < 30) {
                for (; i + 1 < 30; i++) {
                    if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[i + 1]] != 0xff) {
                        menu_state_current->card->cursor = i + 1;
                        break;
                    }
                }
            }
        }
        break;
    case 1:
        while (slot + 3 < 30) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot + 3]] == 0xff ||
                !menu_state_current->card->ours[menu_file_cursor_card_slots[slot + 3]]) {
                slot += 3;
            } else {
                menu_state_current->card->cursor = slot + 3;
                break;
            }
        }
        if (slot + 3 >= 30) {
            i = menu_state_current->card->cursor + 3;
            if (i < 30) {
                while (i + 1 < 30) {
                    if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[i + 1]] == 0xff ||
                        !menu_state_current->card->ours[menu_file_cursor_card_slots[i + 1]]) {
                        i++;
                    } else {
                        menu_state_current->card->cursor = i + 1;
                        break;
                    }
                }
            }
        }
        break;
    case 2:
        if (menu_state_current->card->present[(slot + 3) / 15]) {
            for (; slot + 3 < 30; slot += 3) {
                menu_state_current->card->cursor = slot + 3;
                break;
            }
            if (slot + 3 >= 30) {
                i = menu_state_current->card->cursor + 3;
                if (i < 30) {
                    for (; i + 1 < 30; i++) {
                        menu_state_current->card->cursor = i + 1;
                        break;
                    }
                }
            }
        }
        break;
    }
}

/* 801CA1D4: Move the cursor from `slot` a row up for `mode`: 0 to the next slot above
 * holding a file, else to the last file before the previous row; 1 the same
 * for this game's files; 2 a row up when that card is present. */
void menu_file_cursor_move_up(s32 mode, s32 slot) {
    s32 cursor;
    s32 i;

    switch (mode) {
    case 0:
        for (; slot - 3 >= 0; slot -= 3) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot - 3]] != 0xff) {
                menu_state_current->card->cursor = slot - 3;
                break;
            }
        }
        if (slot - 3 < 0) {
            i = menu_state_current->card->cursor - 3;
            if (i >= 0) {
                for (; i - 1 >= 0; i--) {
                    if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[i - 1]] != 0xff) {
                        menu_state_current->card->cursor = i - 1;
                        break;
                    }
                }
            }
        }
        break;
    case 1:
        while (slot - 3 >= 0) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot - 3]] == 0xff ||
                !menu_state_current->card->ours[menu_file_cursor_card_slots[slot - 3]]) {
                slot -= 3;
            } else {
                menu_state_current->card->cursor = slot - 3;
                break;
            }
        }
        if (slot - 3 < 0) {
            cursor = menu_state_current->card->cursor;
            i = cursor - 3;
            if (i >= 0) {
                while (i - 1 >= 0) {
                    if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[i - 1]] == 0xff ||
                        !menu_state_current->card->ours[menu_file_cursor_card_slots[i - 1]]) {
                        i--;
                    } else {
                        menu_state_current->card->cursor = i - 1;
                        break;
                    }
                }
            }
        }
        break;
    case 2:
        if (menu_state_current->card->present[(slot - 3) / 15]) {
            for (; slot - 3 >= 0; slot -= 3) {
                menu_state_current->card->cursor = slot - 3;
                break;
            }
            if (slot - 3 < 0) {
                i = menu_state_current->card->cursor - 3;
                if (i >= 0) {
                    for (; i - 1 >= 0; i--) {
                        menu_state_current->card->cursor = i - 1;
                        break;
                    }
                }
            }
        }
        break;
    }
}

/* 801CA480: Move the cursor from `slot` right for `mode`: 0 to the next slot holding
 * a file, 1 to the next slot holding this game's file, 2 one slot on when
 * that card is present. */
void menu_file_cursor_move_right(s32 mode, s32 slot) {
    s32 next;

    switch (mode) {
    case 0:
        for (; slot + 1 < 30; slot++) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot + 1]] != 0xff) {
                menu_state_current->card->cursor = slot + 1;
                break;
            }
        }
        break;
    case 1:
        while (slot + 1 < 30) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot + 1]] == 0xff || !menu_state_current->card->ours[menu_file_cursor_card_slots[slot + 1]]) {
                slot++;
            } else {
                menu_state_current->card->cursor = slot + 1;
                break;
            }
        }
        break;
    case 2:
        next = slot + 1;
        if (menu_state_current->card->present[next / 15] && next < 30) {
            menu_state_current->card->cursor = next;
        }
        break;
    }
}

/* 801CA5F0: Move the cursor from `slot` left for `mode`: 0 to the previous slot
 * holding a file, 1 to the previous slot holding this game's file, 2 one slot
 * back when that card is present. */
void menu_file_cursor_move_left(s32 mode, s32 slot) {
    s32 next;

    switch (mode) {
    case 0:
        for (; slot - 1 >= 0; slot--) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot - 1]] != 0xff) {
                menu_state_current->card->cursor = slot - 1;
                break;
            }
        }
        break;
    case 1:
        while (slot - 1 >= 0) {
            if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[slot - 1]] == 0xff || !menu_state_current->card->ours[menu_file_cursor_card_slots[slot - 1]]) {
                slot--;
            } else {
                menu_state_current->card->cursor = slot - 1;
                break;
            }
        }
        break;
    case 2:
        next = slot - 1;
        if (menu_state_current->card->present[next / 15] && next >= 0) {
            menu_state_current->card->cursor = next;
        }
        break;
    }
}

/* Unreferenced; it follows the card file strings, which are literals. */
const char menu_unused_empty_string[] = ""; /* 801C50C4 */

/* 801CA750: The file screen's cursor input for `mode` (801c9bcc): move the cursor
 * (down, right, up, left), confirm (1) or cancel (2); a new cursor slot shows
 * its file's details. */
s32 menu_file_cursor_update(s32 mode) {
    s32 result;

    result = 0;
    switch (menu_state_current->input) {
    case 4:
        result = 1;
        break;
    case 5:
        result = 2;
        break;
    case 0:
        menu_file_cursor_move_down(mode, menu_state_current->card->cursor);
        break;
    case 2:
        menu_file_cursor_move_up(mode, menu_state_current->card->cursor);
        break;
    case 1:
        menu_file_cursor_move_right(mode, menu_state_current->card->cursor);
        break;
    case 3:
        menu_file_cursor_move_left(mode, menu_state_current->card->cursor);
        break;
    }
    if (menu_state_current->card->cursor != menu_state_current->card->unknown4f80) {
        menu_file_info_show(menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]],
                      menu_state_current->card->ours[menu_file_cursor_card_slots[menu_state_current->card->cursor]]);
        menu_state_current->card->unknown4f80 = menu_state_current->card->cursor;
    }
    return result;
}

/* 801CA8C0: Build the save header's title: "XENOGEARS/No." (full width), file number
 * `file` + 1 as two full-width digits, a full-width space and the save title
 * line. */
void menu_save_build_header_title(u8 file) {
    strcpy(menu_state_current->card->save_title, "\x82\x77\x82\x64\x82\x6d\x82\x6e\x82\x66\x82\x64\x82\x60\x82\x71\x82\x72"
                                         "\x81\x5e\x82\x6d\x82\x8f\x81\x44");
    menu_state_current->card->save_title[26] = 0x82;
    menu_state_current->card->save_title[27] = (file + 1) / 10 + 0x4f;
    menu_state_current->card->save_title[28] = 0x82;
    menu_state_current->card->save_title[29] = (file + 1) % 10 + 0x4f;
    menu_state_current->card->save_title[30] = 0x81;
    menu_state_current->card->save_title[31] = 0x40;
    menu_state_current->card->save_title[32] = 0;
    menu_state_current->card->save_title[33] = 0;
    strcat(menu_state_current->card->save_title, menu_state_current->card->title);
}

/* 801CAA38: The yes/no choice: wait for confirm or cancel while left/right move the
 * highlight (2 yes, 0 no). Without `watch` it waits b4h frames at most while
 * no input comes and returns on any other input; with `watch` a card change
 * clears that port's listing and cancels. Returns 1 for yes. */
u8 menu_ask_yes_no(u8 watch) {
    u8 present[2];
    u8 yes;
    u8 waiting;
    u8 timer;

    present[0] = menu_state_current->card->present[0];
    present[1] = menu_state_current->card->present[1];
    waiting = 1;
    if (menu_inside_command) {
        menu_state_current->card->mode = 2;
    }
    menu_yes_no_cancelled = 0;
    yes = 0;
    timer = 0xb4;
    do {
        if (!watch) {
            menu_state_current->markers->shown[2] = 0;
            menu_state_current->markers->shown[3] = 0;
            if (menu_state_current->input != 8 || --timer == 0) {
                break;
            }
        }
        menu_run_frame();
        if (watch && menu_inside_command) {
            if (present[0] != menu_state_current->card->present[0]) {
                menu_state_current->card->scanned[0] = 0;
                menu_state_current->input = 5;
                menu_card_changed = 1;
            }
            if (present[1] != menu_state_current->card->present[1]) {
                menu_state_current->card->scanned[1] = 0;
                menu_state_current->input = 5;
                menu_card_changed = 1;
            }
        }
        switch (menu_state_current->input) {
        case 4:
            waiting = 0;
            break;
        case 5:
            waiting = yes = 0;
            menu_yes_no_cancelled = 1;
            break;
        case 2:
            menu_state_current->markers->shown[2] = 1;
            yes = 1;
            menu_state_current->markers->shown[3] = 0;
            break;
        case 0:
            menu_state_current->markers->shown[2] = 0;
            yes = 0;
            menu_state_current->markers->shown[3] = 1;
            break;
        }
    } while (waiting);
    menu_state_current->markers->shown[2] = 0;
    menu_state_current->markers->shown[3] = 0;
    menu_state_current->card->mode = 0;
    return yes;
}

/* 801CACF8: Show card message `message` and ask (801caa38 with `arg`); when answered
 * yes and a follow-up `confirm` is given (not ff), ask that too. Returns the
 * last answer. */
s32 menu_notice_ask_yes_no(u8 message, u8 confirm, u8 arg) {
    u8 answer;

    menu_notice_open(message);
    menu_state_current->markers->shown[3] = 1;
    answer = menu_ask_yes_no(arg);
    menu_notice_close();
    if (confirm != 0xff && answer) {
        menu_notice_open(confirm);
        menu_state_current->markers->shown[3] = 1;
        answer = menu_ask_yes_no(arg);
        menu_notice_close();
    }
    return answer;
}

/* 801CADB0: Reset the file cursor: the first port with a card (port 2's slots start at 15). */
void menu_file_cursor_reset(void) {
    menu_state_current->card->unknown4f80 = 0xff;
    menu_state_current->card->cursor = 0;
    if (menu_state_current->card->present[0] == 0 && menu_state_current->card->present[1] != 0) {
        menu_state_current->card->cursor = 15;
    }
}

/* 801CAE08: The card access indicator: 0 removes it (after a frame its block is
 * released), 1 builds it (a flat quad and sprites 160/161 at (a0, 64)) and
 * starts its effects, 2 closes it. */
void menu_card_indicator_set(u8 mode) {
    switch (mode) {
    case 0:
        if (menu_state_current->flags->cursors_shown[2]) {
            menu_state_current->flags->cursors_shown[2] = 0;
            menu_run_frame();
            heap_free(menu_state_current->indicator);
        }
        break;
    case 1:
        menu_state_current->indicator = heap_alloc(sizeof(MenuIndicator), 0);
        bzero((u_char *)menu_state_current->indicator, sizeof(MenuIndicator));
        SetPolyF4(&menu_state_current->indicator->fills[menu_state_current->buffer_index]);
        setRGB0(&menu_state_current->indicator->fills[menu_state_current->buffer_index], 0xa0, 0xa0, 0);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x160, menu_state_current->indicator->spriteA, menu_state_current->buffer_index, 0xa0, 0x64, 0x1000);
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x161, menu_state_current->indicator->spriteB, menu_state_current->buffer_index, 0xa0, 0x64, 0x1000);
        menu_state_current->indicator->buffer = menu_state_current->buffer_index;
        menu_state_current->flags->cursors_shown[2] = 1;
        menu_state_current->indicator->unk7B4 = 8;
        sound_play_effect((menu_state_current->effects->id << 16) | 0xe0);
        sound_play_effect((menu_state_current->effects->id << 16) | 0xe1);
        sound_play_effect((menu_state_current->effects->id << 16) | 0x8f);
        break;
    case 2:
        if (menu_state_current->flags->cursors_shown[2]) {
            menu_state_current->indicator->unk7B0 = 0x100;
            setRGB0(&menu_state_current->indicator->fills[menu_state_current->indicator->buffer], 0, 0xa0, 0);
            menu_state_current->flags->cursors_shown[2] = 2;
        }
        break;
    }
}

/* 801CB184: Decode the 31 names of the game data in place: each name's code pairs up
 * to the first zero pair go through 80033b34 into a 20-byte buffer that is
 * copied back whole. */
void menu_decode_game_names(void) {
    u8 codes[24];
    u8 decoded[20];
    s32 n;
    s32 i;
    u8 *src;
    u8 *dst;

    for (n = 0; n < 31; n++) {
        for (i = 0; i < 20; i += 2) {
            src = &GAME_NAMES[n * 20];
            codes[i] = src[i];
            codes[i + 1] = GAME_NAMES[n * 20 + i + 1];
            if (src[i] == 0 && GAME_NAMES[n * 20 + i + 1] == 0) {
                break;
            }
        }
        text_decode_codes(codes, decoded, i / 2);
        dst = &GAME_NAMES[n * 20];
        for (i = 0; i < 20; i++) {
            dst[i] = decoded[i];
        }
    }
}

/* 801CB28C: Apply a loaded save: its derived tables, play time and the 16 resident
 * words copied from the game data, then finish (801cb184). */
void menu_apply_loaded_save(SaveData *save) {
    s32 i;

    menu_restore_game_data_from_save(save, menu_state_current->tables);
    pad_vblank_count = save->summary.time;
    for (i = 0; i < 16; i++) {
        mode_battle_ai_variables[i] = game_data.flagWords[i + 1];
    }
    menu_decode_game_names();
}

/* Read the save file in 100h chunks into a 2100h block (a failed read
 * closes the file and frees the block) and apply it when its sum
 * matches (a statement macro). */
#define READ_SAVE()                                          \
    do {                                                     \
        menu_state_current->flags->markers_shown = 0;                \
        buffer = heap_alloc(0x2100, 1);                   \
        done = 0;                                            \
        p = buffer;                                          \
        menu_state_current->flags->file_info_shown = 0;              \
        menu_card_indicator_set(1);                                    \
        do {                                                 \
            menu_run_frame();                                 \
            retry = 5;                                       \
            do {                                             \
                if (read(fd, p, 0x100) != 0x100) {           \
                    fd = 0;                                  \
                    menu_card_retry_wait(port);                     \
                }                                            \
            } while (fd == 0 && --retry != 0);               \
            if (fd == 0) {                                   \
                close(file);                                 \
                goto release;                                \
            }                                                \
            done += 0x100;                                   \
            p += 0x100;                                      \
        } while (done < menu_state_current->card->save_blocks << 13); \
        close(fd);                                           \
        p = buffer + 0x100;                                  \
        sum = 0;                                             \
        for (i = 0; i < 0x1eff; i++) {                       \
            sum += *p++;                                     \
        }                                                    \
        if (sum == *p) {                                     \
            p = buffer + 0x100;                              \
            menu_load_or_release_data_set(1);                                \
            menu_apply_loaded_save((SaveData *)p);                    \
            menu_load_or_release_data_set(0x11);                             \
        } else {                                             \
            fd = 0;                                          \
        }                                                    \
    } while (0)

/* 801CB304: The load: refresh the cards (no card returns), find a slot with this
 * game's file (none: message 62 and return 0), then run the cursor until
 * cancel (return 0) or confirm: ask (message 65); on yes read the file in
 * 100h chunks into a 2100h block and, when the payload's eight-bit sum matches
 * its last byte, apply it; message 5c ends the load, a failure shows message
 * 3e and continues. */
u8 menu_load_command_run(void) {
    char path[64];
    s32 first;
    u8 result;
    s32 again;
    u8 port;
    u8 retry;
    s32 fd;
    s32 file;
    s32 done;
    u8 *buffer;
    u8 *p;
    u8 sum;
    s32 i;
    s32 failed; /* open's failure result, held in a variable */

    result = 1;
    first = 1;
    again = 1;
    menu_file_cursor_reset();
    do {
        if (menu_card_refresh()) {
            break;
        }
        if (first) {
            if (menu_state_current->flags->card_message_shown) {
                menu_notice_close();
            }
            first = 0;
            menu_state_current->markers->shown[0] = 1;
        }
        if (!menu_file_cursor_is_suitable(1) && (menu_state_current->card->cursor = menu_file_cursor_find_first(1)) == 0xff) {
            menu_state_current->flags->markers_shown = 0;
            menu_state_current->flags->file_info_shown = 0;
            menu_state_current->load_state = 1;
            menu_state_current->flags->file_info_shown = 0;
            menu_notice_ask_yes_no(0x62, 0xff, 0);
            result = 0;
            break;
        }
        menu_state_current->flags->markers_shown = 1;
        switch (menu_file_cursor_update(1)) {
        case 1:
            menu_state_current->markers->at_cursor[0] = 0;
            menu_state_current->card->mode = 0;
            port = 0;
            if (menu_state_current->card->cursor < 15) {
                __builtin_memcpy(path, "bu00:", 6);
            } else {
                __builtin_memcpy(path, "bu10:", 6);
                port = 1;
            }
            strcat(path,
                   menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]].name);
            if (!(u8)menu_notice_ask_yes_no(0x65, 0xff, 1)) {
                menu_state_current->card->unknown4f80 = 0xff;
            } else {
                menu_notice_open(0x3b);
                retry = 5;
                menu_state_current->sounds = 0;
                failed = -1;
                do {
                    fd = open(path, 1);
                    if (fd == failed) {
                        fd = 0;
                        menu_card_retry_wait(port);
                    }
                } while (fd == 0 && --retry != 0);
                file = fd;
                if (file != 0) {
                    READ_SAVE();
                release:
                    heap_free(buffer);
                }
                menu_card_indicator_set(2);
                menu_notice_close();
                if (fd != 0) {
                    again = 0;
                    menu_state_current->sounds = 1;
                    menu_play_sound(0x34);
                    menu_state_current->sounds = 0;
                    menu_notice_ask_yes_no(0x5c, 0xff, 0);
                } else {
                    menu_notice_ask_yes_no(0x3e, 0xff, 0);
                    menu_state_current->card->mode = 2;
                }
                menu_card_indicator_set(0);
                menu_state_current->card->scanned[0] = 0;
                menu_state_current->card->scanned[1] = 0;
                menu_state_current->card->unknown4f8c[0] = 0xff;
                menu_state_current->card->unknown4f8c[1] = 0xff;
            }
            menu_state_current->markers->at_cursor[0] = 1;
            break;
        case 2:
            again = 0;
            result = 0;
            menu_state_current->sounds = 1;
            break;
        }
    } while (again);
    menu_state_current->card->mode = 1;
    menu_state_current->cards_present = 1;
    return result;
}

/* 801CB8AC: The unformatted-card question for `port`: show message 29h + 3 * port and
 * wait while no input comes and the cards stay as they were; a card change
 * returns 0, otherwise the answer to message 2f. */
u8 menu_card_ask_format(u8 port) {
    u8 present[2];
    u8 changed;
    u8 answer;

    present[0] = menu_state_current->card->present[0];
    present[1] = menu_state_current->card->present[1];
    menu_notice_open(port * 3 + 0x29);
    menu_state_current->input = 8;
    answer = 0;
    menu_state_current->card->mode = 2;
    changed = 0;
    while (menu_state_current->input == 8) {
        menu_run_frame();
        if (present[0] != menu_state_current->card->present[0] || present[1] != menu_state_current->card->present[1]) {
            changed = 1;
            break;
        }
    }
    menu_notice_close();
    if (!changed) {
        answer = menu_notice_ask_yes_no(0x2f, 0xff, 1);
    }
    return answer;
}

/* 801CB9E8: The save slot to use on `port`: `slot`, or for ff the first free one. */
u8 menu_save_choose_digit(u8 port, u8 slot) {
    s32 i;
    u8 found;

    found = 0;
    if (slot == 0xff) {
        for (i = 0; i < 15; i++) {
            if (menu_card_own_save_flags[port][i] == 0) {
                found = i;
                break;
            }
        }
        return found;
    }
    return slot;
}

/* 801CBA4C: Fill the zeroed save payload: the globals 8005a3a0 into the game data, the
 * party summary (per slot: character id or ff, HP, maximum HP, EP, maximum EP
 * and three more bytes), the play time and file digit `digit`, each name
 * encoded in place, the disc, then the game data copy (801e4a28) and the names
 * decoded back. */
void menu_save_build_payload(SaveSummary *payload, u8 port, u8 digit) {
    u8 codes[24];
    u8 encoded[20];
    s32 i;
    s32 j;
    u8 *name;
    u8 *names;

    for (j = 0; j < 16; j++) {
        game_data.flagWords[j + 1] = mode_battle_ai_variables[j];
    }
    for (i = 0; i < 3; i++) {
        if (menu_state_current->flags->party[i] != 0xff) {
            payload->ids[i] = menu_state_current->flags->party[i];
            payload->hp[i] = game_data.characters[menu_state_current->flags->party[i]].hp;
            payload->hpMax[i] = game_data.characters[menu_state_current->flags->party[i]].maxHp;
            payload->ep[i] = game_data.characters[menu_state_current->flags->party[i]].ep;
            payload->epMax[i] = game_data.characters[menu_state_current->flags->party[i]].maxEp;
            payload->level[i] = game_data.characters[menu_state_current->flags->party[i]].level;
            payload->level2[i] = game_data.characters[menu_state_current->flags->party[i]].level2;
        } else {
            payload->ids[i] = 0xff;
        }
    }
    payload->time = pad_vblank_count;
    payload->unk1F = 0;
    payload->digit = digit;
    names = GAME_NAMES;
    for (i = 0; i < 31; i++) {
        name = &names[i * 20];
        for (j = 0; j < 20; j++) {
            codes[j] = name[j];
            encoded[j] = 0;
        }
        text_encode(codes, (u16 *)encoded);
        for (j = 0; j < 20; j++) {
            name[j] = encoded[j];
        }
    }
    if (!menu_saving_at_cd_change) {
        game_data.vars[82] = cd_get_disc_number() - 1;
    } else {
        game_data.vars[82] = 1;
    }
    menu_copy_game_data_to_save((SaveData *)payload);
    menu_decode_game_names();
}

/* 801CBD90: The save: refresh the cards (none: returns 1), then run the file cursor
 * until cancel or confirm. An unformatted card asks to format it; an empty
 * slot asks message 5f, another game's file is refused (c4) and this game's
 * file asks 38 and is erased (keeping its digit). The save writes the
 * header (title 801ca8c0) and the checksummed payload (801cba4c) to a
 * temporary file and renames it to the prefix and digit, then reports 5c or
 * 35 and refreshes the listing. With `kind` 0 one save ends it. */
u8 menu_save_command_run(u8 kind) {
    char finalName[64];
    char tempName[64];
    char device[8];
    char digitText[16];
    s32 again;
    s32 created;
    u8 first;
    u8 result;
    u8 existing;
    u8 proceed;
    u8 port;
    u8 digit;
    u8 retry;
    s32 fd;
    s32 written;
    u8 *buffer;
    u8 *p;
    u8 *q;
    u8 sum;
    s32 i;

    existing = 0xff;
    first = 1;
    again = 1;
    result = 0;
    menu_file_cursor_reset();
    do {
        if (menu_card_refresh()) {
            result = 1;
            break;
        }
        if (first) {
            if (menu_state_current->flags->card_message_shown) {
                menu_notice_close();
            }
            first = 0;
            menu_state_current->markers->shown[0] = 1;
        }
        if (!menu_file_cursor_is_suitable(2) && (menu_state_current->card->cursor = menu_file_cursor_find_first(2)) == 0xff) {
            menu_notice_ask_yes_no(0xac, 0xff, 0);
            break;
        }
        menu_state_current->flags->markers_shown = 1;
        switch (menu_file_cursor_update(2)) {
        case 1:
            menu_state_current->markers->at_cursor[0] = 0;
            menu_state_current->card->mode = 0;
            proceed = 1;
            if (menu_state_current->card->cursor < 15) {
                __builtin_memcpy(device, "bu00:", 6);
                port = 0;
            } else {
                __builtin_memcpy(device, "bu10:", 6);
                port = 1;
            }
            if (menu_state_current->card->result[port] == -2) {
                if (!menu_card_ask_format(port)) {
                    menu_state_current->markers->at_cursor[0] = 1;
                    continue;
                }
                menu_notice_open(0x26);
                if (format(device)) {
                    menu_notice_close();
                    menu_notice_ask_yes_no(0x5c, 0xff, 0);
                } else {
                    menu_notice_close();
                    proceed = 0;
                    again = 0;
                }
            }
            if (proceed) {
                if (menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]] != 0xff) {
                    if (menu_state_current->card->ours[menu_file_cursor_card_slots[menu_state_current->card->cursor]]) {
                        if ((u8)menu_notice_ask_yes_no(0x38, 0xff, 1)) {
                            strcpy(tempName, device);
                            strcat(tempName, menu_state_current->card
                                                 ->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]]
                                                 .name);
                            delete(tempName);
                            existing = (menu_state_current->card->heads +
                                        menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]])[0][0x123];
                        } else {
                            proceed = 0;
                            menu_state_current->card->unknown4f80 = 0xff;
                        }
                    } else {
                        menu_notice_ask_yes_no(0xc4, 0xff, 0);
                        menu_state_current->markers->at_cursor[0] = 1;
                        continue;
                    }
                } else if (!(u8)menu_notice_ask_yes_no(0x5f, 0xff, 1)) {
                    proceed = 0;
                    menu_state_current->card->unknown4f80 = 0xff;
                }
            }
            if (proceed) {
                menu_state_current->flags->markers_shown = 0;
                menu_state_current->flags->file_info_shown = 0;
                menu_state_current->card->unknown4f80 = 0xff;
                menu_state_current->card->unknown4f8c[0] = 0xff;
                menu_state_current->card->unknown4f8c[1] = 0xff;
                digit = menu_save_choose_digit(port, existing);
                digitText[0] = digit + '0';
                digitText[1] = 0;
                strcpy(finalName, device);
                strcpy(tempName, device);
                strcat(finalName, menu_state_current->card->prefix);
                strcat(finalName, digitText);
                strcat(tempName, "__tmp_file");
                menu_notice_open(0x32);
                menu_state_current->sounds = 0;
                delete(tempName);
                retry = 3;
                do {
                    fd = open(tempName, menu_state_current->card->save_blocks << 16 | 0x200);
                    if (fd == -1) {
                        fd = 0;
                        menu_card_retry_wait(port);
                    } else {
                        created = fd;
                    }
                } while (fd == 0 && --retry != 0);
                close(created);
                if (fd != 0) {
                    retry = 3;
                    do {
                        fd = open(tempName, 2);
                        if (fd == -1) {
                            fd = 0;
                            menu_card_retry_wait(port);
                        }
                    } while (fd == 0 && --retry != 0);
                    if (fd == 0) {
                        delete(tempName);
                    }
                    menu_run_frame();
                }
                if (fd != 0) {
                    menu_save_build_header_title(digit);
                    retry = 3;
                    do {
                        if (write(fd, menu_state_current->card->save_magic, 0x100) == -1) {
                            fd = 0;
                            menu_card_retry_wait(port);
                        }
                    } while (fd == 0 && --retry != 0);
                    if (fd == 0) {
                        close(0);
                        delete(tempName);
                    } else {
                        written = 0x100;
                        buffer = heap_alloc(0x1f00, 1);
                        bzero(buffer, 0x1f00);
                        p = buffer;
                        menu_save_build_payload((SaveSummary *)buffer, port, digit);
                        q = buffer;
                        sum = 0;
                        for (i = 0; i < 0x1eff; i++) {
                            sum += *q++;
                        }
                        *q = sum;
                        menu_card_indicator_set(1);
                        do {
                            menu_run_frame();
                            retry = 3;
                            do {
                                if (write(fd, p, 0x100) != 0x100) {
                                    fd = 0;
                                    menu_card_retry_wait(port);
                                }
                            } while (fd == 0 && --retry != 0);
                            if (fd == 0) {
                                close(created);
                                strcpy(tempName, device);
                                strcat(tempName, "__tmp_file");
                                delete(tempName);
                                goto release;
                            }
                            written += 0x100;
                            p += 0x100;
                        } while (written < menu_state_current->card->save_blocks << 13);
                        close(fd);
                        strcpy(tempName, device);
                        strcat(tempName, "__tmp_file");
                        retry = 3;
                        do {
                            if (!rename(tempName, finalName)) {
                                fd = 0;
                                menu_card_retry_wait(port);
                            }
                        } while (fd == 0 && --retry != 0);
                        if (fd == 0) {
                            delete(tempName);
                        }
                    release:
                        heap_free(buffer);
                    }
                }
                menu_notice_close();
                menu_card_indicator_set(2);
                if (fd != 0) {
                    menu_state_current->sounds = 1;
                    menu_play_sound(0x34);
                    menu_state_current->sounds = 0;
                    menu_notice_ask_yes_no(0x5c, 0xff, 0);
                } else {
                    menu_notice_ask_yes_no(0x35, 0xff, 0);
                }
                menu_card_indicator_set(0);
                menu_state_current->card->mode = 2;
                while (menu_state_current->card_poll_timer != 1) {
                    menu_run_frame();
                }
                menu_state_current->card->scanned[0] = 0;
                menu_state_current->card->scanned[1] = 0;
            }
            menu_state_current->markers->at_cursor[0] = 1;
            menu_state_current->sounds = 1;
            if (kind) {
                break;
            }
            /* fall through: a single save ends it */
        case 2:
            again = 0;
            break;
        }
    } while (again);
    menu_state_current->card->mode = 1;
    menu_state_current->cards_present = 1;
    return result;
}

/* 801CC6D8: The file screen's copy command: pick a file and copy it to the other
 * card (asking first; no card, a full card or an existing file there
 * refuse). The copy reads the header and data blocks and writes them to a
 * temporary file that is then renamed; it reports 5c, or ac for a full
 * card and 44 otherwise, and refreshes the listing. Returns 1 when there
 * was no card. */
u8 menu_copy_command_run(void) {
    char destName[64];
    char other[8];
    char srcName[64];
    char tempName[64];
    char device[8];
    u8 buffer[0x200];
    u8 header[0x200];
    s32 src;
    u8 first;
    u8 result;
    u8 full;
    s32 again;
    u8 proceed;
    s32 dest;
    u8 ask;
    u8 noCard;
    u8 exists;
    u8 retry;
    s32 phase;
    s32 fd;
    s32 srcFd;
    s32 destFd;
    s32 written;
    s32 i;
    s32 slot;

    full = 0;
    first = 1;
    again = 1;
    result = 0;
    menu_file_cursor_reset();
    do {
        if (menu_card_refresh()) {
            result = 1;
            break;
        }
        if (first) {
            if (menu_state_current->flags->card_message_shown) {
                menu_notice_close();
            }
            first = 0;
            menu_state_current->markers->shown[0] = 1;
        }
        if (!menu_file_cursor_is_suitable(0) && (menu_state_current->card->cursor = menu_file_cursor_find_first(0)) == 0xff) {
            menu_state_current->markers->shown[0] = 0;
            menu_state_current->flags->file_info_shown = 0;
            menu_notice_ask_yes_no(0x62, 0xff, 0);
            break;
        }
        menu_state_current->flags->markers_shown = 1;
        switch (menu_file_cursor_update(0)) {
        case 1:
            menu_state_current->markers->at_cursor[0] = 0;
            menu_state_current->card->mode = 0;
            dest = menu_state_current->card->cursor / 15 == 0;
            proceed = 1;
            if (!dest) {
                ask = 0x47;
                noCard = 0x4a;
                exists = 0x4d;
            } else {
                ask = 0xbb;
                noCard = 0xbe;
                exists = 0xc1;
            }
            if (!(u8)menu_notice_ask_yes_no(ask, 0xff, 1)) {
                proceed = 0;
                menu_state_current->card->unknown4f80 = 0xff;
            }
            if (proceed && !menu_state_current->card->present[dest]) {
                menu_notice_open(noCard);
                if (!menu_state_current->card->present[dest]) {
                    for (i = 0xb3; i != 0; i--) {
                        VSync(0);
                    }
                    proceed = 0;
                    again = 0;
                }
                menu_notice_close();
            }
            if (proceed && menu_card_blocks_used[dest] >= 15) {
                menu_card_blocks_used[dest] = 0;
                menu_state_current->flags->markers_shown = 0;
                menu_notice_ask_yes_no(0xac, 0xff, 0);
                proceed = 0;
                again = 0;
            }
            if (proceed) {
                dest = menu_state_current->card->cursor / 15 == 0;
                if (dest) {
                    __builtin_memcpy(device, "bu00:", 6);
                    __builtin_memcpy(other, "bu10:", 6);
                    src = 0;
                } else {
                    __builtin_memcpy(device, "bu10:", 6);
                    __builtin_memcpy(other, "bu00:", 6);
                    src = 1;
                }
                if (menu_state_current->card->result[dest] == -2) {
                    if (menu_card_ask_format(dest)) {
                        menu_notice_open(0x26);
                        if (format(other)) {
                            menu_notice_close();
                            menu_notice_ask_yes_no(0x5c, 0xff, 0);
                        } else {
                            menu_notice_close();
                            proceed = 0;
                        }
                    } else {
                        menu_state_current->markers->at_cursor[0] = 1;
                        continue;
                    }
                }
            }
            if (proceed) {
                for (i = 0; i < menu_state_current->card->unknown4f8a[dest]; i++) {
                    if (!strcmp(menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]].name,
                                menu_state_current->card->files[dest * 16 + i].name)) {
                        i = 0xff;
                        break;
                    }
                }
                if (i == 0xff) {
                    menu_notice_ask_yes_no(exists, 0xff, 0);
                    proceed = 0;
                }
            }
            if (proceed) {
                menu_notice_open(0x41);
                menu_state_current->sounds = 0;
                menu_state_current->flags->markers_shown = 0;
                menu_state_current->flags->file_info_shown = 0;
                menu_state_current->card->unknown4f80 = 0xff;
                menu_state_current->card->unknown4f8c[0] = 0xff;
                menu_state_current->card->unknown4f8c[1] = 0xff;
                strcpy(srcName, device);
                strcpy(destName, other);
                strcpy(tempName, other);
                strcat(srcName, menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]].name);
                strcat(destName, menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]].name);
                strcat(tempName, "__tmp_file");
                retry = 3;
                do {
                    fd = open(srcName, 1);
                    if (fd == -1) {
                        fd = 0;
                        menu_card_retry_wait(src);
                    }
                } while (fd == 0 && --retry != 0);
                if (fd != 0) {
                    srcFd = fd;
                    retry = 3;
                    do {
                        if (read(fd, header, 0x200) == -1) {
                            fd = 0;
                            menu_card_retry_wait(src);
                        }
                    } while (fd == 0 && --retry != 0);
                    if (fd == 0) {
                        close(srcFd);
                    } else {
                        delete(tempName);
                        retry = 1;
                        do {
                            destFd = open(tempName, header[3] << 16 | 0x200);
                            if (destFd == -1) {
                                fd = 0;
                                full = 1;
                            }
                        } while (fd == 0 && --retry != 0);
                        if (fd == 0) {
                            close(srcFd);
                        }
                        close(destFd);
                        if (fd != 0) {
                            retry = 3;
                            do {
                                destFd = open(tempName, 2);
                                if (destFd == -1) {
                                    fd = 0;
                                    menu_card_retry_wait(dest);
                                }
                            } while (fd == 0 && --retry != 0);
                            if (fd == 0) {
                                close(srcFd);
                                delete(tempName);
                            } else {
                                retry = 3;
                                do {
                                    if (write(destFd, header, 0x200) == -1) {
                                        fd = 0;
                                        menu_card_retry_wait(dest);
                                    }
                                } while (fd == 0 && --retry != 0);
                                if (fd == 0) {
                                    close(srcFd);
                                    close(destFd);
                                    destFd = 0;
                                    delete(tempName);
                                }
                                written = 0x200;
                                phase = 0;
                                menu_card_indicator_set(1);
                                for (;;) {
                                    menu_run_frame();
                                    if (phase == 0) {
                                        retry = 3;
                                        do {
                                            if (read(fd, buffer, 0x200) != 0x200) {
                                                fd = 0;
                                                menu_card_retry_wait(src);
                                            }
                                        } while (fd == 0 && --retry != 0);
                                        if (fd == 0) {
                                            close(srcFd);
                                            close(destFd);
                                            strcpy(tempName, other);
                                            strcat(tempName, "__tmp_file");
                                            delete(tempName);
                                            break;
                                        }
                                        phase = 1;
                                    } else {
                                        retry = 3;
                                        do {
                                            if (write(destFd, buffer, 0x200) != 0x200) {
                                                fd = 0;
                                                menu_card_retry_wait(dest);
                                            }
                                        } while (fd == 0 && --retry != 0);
                                        if (fd == 0) {
                                            close(srcFd);
                                            close(destFd);
                                            strcpy(tempName, other);
                                            strcat(tempName, "__tmp_file");
                                            delete(tempName);
                                            break;
                                        }
                                        phase = 0;
                                        written += 0x200;
                                        if (written >= header[3] << 13) {
                                            close(destFd);
                                            close(fd);
                                            strcpy(tempName, other);
                                            strcat(tempName, "__tmp_file");
                                            rename(tempName, destName);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (proceed) {
                menu_card_indicator_set(2);
                menu_notice_close();
                if (fd != 0) {
                    menu_state_current->sounds = 1;
                    menu_play_sound(0x34);
                    menu_state_current->sounds = 0;
                    menu_notice_ask_yes_no(0x5c, 0xff, 0);
                } else {
                    menu_notice_ask_yes_no(!full ? 0x44 : 0xac, 0xff, 0);
                }
                menu_card_indicator_set(0);
                menu_state_current->card->mode = 2;
                while (menu_state_current->card_poll_timer != 1) {
                    menu_run_frame();
                }
                for (slot = 0; slot < 32; slot++) {
                    menu_state_current->card->file_slots[slot] = 0xff;
                    menu_state_current->card->ours[slot] = 0;
                    menu_state_current->card->files[slot].state = 0;
                }
                menu_state_current->card->scanned[0] = 0;
                menu_state_current->card->scanned[1] = 0;
                menu_card_changed = 1;
            }
            menu_state_current->markers->at_cursor[0] = 1;
            menu_state_current->sounds = 1;
            /* fallthrough */
        case 2:
            again = 0;
            break;
        }
    } while (again);
    menu_state_current->card->mode = 1;
    menu_state_current->cards_present = 1;
    return result;
}

/* 801CD2AC: The file screen's delete command: pick a file with the cursor, confirm and
 * erase it, then force the cards to be scanned again. Returns 1 when the
 * card check ended the screen. */
u8 menu_delete_command_run(void) {
    char path[64];
    u8 first;
    s32 again;
    u8 ended;
    s32 i;

    first = 1;
    again = 1;
    ended = 0;
    menu_file_cursor_reset();
    do {
        if (menu_card_refresh()) {
            ended = 1;
            break;
        }
        if (first) {
            first = 0;
            if (menu_state_current->flags->card_message_shown != 0) {
                menu_notice_close();
            }
            menu_state_current->markers->shown[0] = 1;
        }
        if (menu_file_cursor_is_suitable(0) == 0 && (menu_state_current->card->cursor = menu_file_cursor_find_first(0)) == 0xff) {
            menu_state_current->markers->shown[0] = 0;
            menu_state_current->flags->file_info_shown = 0;
            menu_notice_ask_yes_no(0x62, 0xff, 0);
            break;
        }
        menu_state_current->flags->markers_shown = 1;
        switch (menu_file_cursor_update(0)) {
        case 1:
            menu_state_current->markers->at_cursor[0] = 0;
            menu_state_current->card->mode = 0;
            if ((u8)menu_notice_ask_yes_no(0x56, 0x59, 1)) {
                menu_notice_open(0x50);
                menu_state_current->flags->markers_shown = 0;
                menu_state_current->flags->file_info_shown = 0;
                menu_state_current->card->unknown4f80 = 0xff;
                if (menu_state_current->card->cursor < 15) {
                    __builtin_memcpy(path, "bu00:", 6);
                } else {
                    __builtin_memcpy(path, "bu10:", 6);
                }
                strcat(path, menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]]
                                 .name);
                delete(path);
                menu_notice_close();
                menu_state_current->sounds = 1;
                menu_play_sound(0x34);
                menu_state_current->sounds = 0;
                menu_notice_ask_yes_no(0x5c, 0xff, 0);
                menu_state_current->card->mode = 2;
                while (menu_state_current->card_poll_timer != 1) {
                    menu_run_frame();
                }
                for (i = 0; i < 32; i++) {
                    menu_state_current->card->file_slots[i] = 0xff;
                    menu_state_current->card->ours[i] = 0;
                    menu_state_current->card->files[i].state = 0;
                }
                menu_state_current->card->scanned[0] = 0;
                menu_state_current->card->scanned[1] = 0;
                menu_state_current->card->unknown4f8c[0] = 0xff;
                menu_state_current->card->unknown4f8c[1] = 0xff;
                menu_card_changed = 1;
            }
            menu_state_current->markers->at_cursor[0] = 1;
            /* fallthrough */
        case 2:
            again = 0;
            break;
        }
    } while (again);
    menu_state_current->card->mode = 1;
    menu_state_current->cards_present = 1;
    menu_state_current->sounds = 1;
    return ended;
}

/* 801CD710: Run the card command chosen on the file screen (0 copy?, 1 delete?, 2 the
 * save or load of the menu kind); returns 0 when the menu should close. */
u8 menu_file_command_run(u8 arg) {
    u8 stay;

    menu_markers_layout(0);
    menu_state_current->flags->markers_shown = 0;
    stay = 1;
    switch (menu_state_current->choice) {
    case 0:
        menu_state_current->cards_present = 7;
        if (menu_delete_command_run()) {
            stay = 0;
        }
        break;
    case 1:
        menu_state_current->cards_present = 6;
        if (menu_copy_command_run()) {
            stay = 0;
        }
        break;
    case 2:
        if (menu_state_screen != 2) {
            menu_state_current->cards_present = 3;
            if (menu_save_command_run(arg)) {
                stay = 0;
            }
        } else {
            menu_state_current->cards_present = 2;
            if (menu_load_command_run()) {
                stay = 0;
            }
        }
        break;
    }
    menu_markers_hide();
    return stay;
}

/* 801CD81C: Build status panel `panel`'s layout sprites (layout `layout`) for character
 * `ch` on row `row` at the positions of `x` and `y`, its frame sprite (14b +
 * row) and its name label (the character's or, in layout 1, its gear's). */
void menu_status_panel_layout_sprites(MenuStatusPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y, u8 layout) {
    s32 i;

    panel->layout_count = 0;
    for (i = 0; i < 9; i++) {
        if (menu_status_panel_layout_images[layout * 9 + i] != 0xffff) {
            panel->layout_count += sprite_sheet_draw_scaled(menu_state_current->sheet, menu_status_panel_layout_images[layout * 9 + i],
                                           &panel->layout[panel->layout_count * 2], menu_state_current->buffer_index,
                                           x->parts[i], row * 56 + y->parts[i], 0x1000);
        }
    }
    sprite_sheet_draw_scaled(menu_state_current->sheet, row + 0x14b, panel->face, menu_state_current->buffer_index, x->face,
                  row * 56 + y->face, 0x1000);
    menu_quad_init(&panel->label[menu_state_current->buffer_index]);
    panel->label[menu_state_current->buffer_index].tpage = GetTPage(0, 0, 0x180, 0);
    if (layout == 0) {
        panel->label[menu_state_current->buffer_index].clut = (ch & 1) ? text_plane1_clut : text_plane0_clut;
    } else {
        panel->label[menu_state_current->buffer_index].clut = (game_data.characters[ch].gearId & 1) ? text_plane0_clut : text_plane1_clut;
    }
    menu_quad_place(&panel->label[menu_state_current->buffer_index], (u16)x->label, (u16)(y->label + row * 56),
                  (u8)(menu_name_image_vram_x_table[layout * 3 + row] * 4), (u8)menu_name_image_vram_y_table[layout * 3 + row], (layout * 3) * 8 + 0x48,
                  13);
}

/* 801CDB1C: Lay out character `ch`'s level digits (the last three of +62) at row `row`
 * of `panel` and prepare the +63 digits. */
void menu_status_panel_layout_level(MenuStatusPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y) {
    s32 i;
    u8 digit;

    menu_split_digits(game_data.characters[ch].level);
    panel->level_count = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[6 + i];
        if (digit != 0xff) {
            panel->level_count += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &panel->level[panel->level_count * 2],
                                              menu_state_current->buffer_index, i * 8 + x->base, row * 56 + y->base,
                                              0x1000);
        }
    }
    menu_split_digits(game_data.characters[ch].level2);
    panel->level2_count = 0;
}

/* 801CDC6C: Lay out status panel `panel`'s numbers for character `ch` on row `row`:
 * hp (by digit position) and hp maximum (packed) of the character (three
 * digits) or, in layout 1, of its gear (five digits); in layout 0 also ep and
 * ep maximum (two digits). */
void menu_status_panel_layout_hp_ep(MenuStatusPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y, u8 layout) {
    s32 digits;
    s32 first;
    s32 i;
    s32 n;
    u8 digit;

    if (layout == 0) {
        menu_split_digits(game_data.characters[ch].hp);
        digits = 3;
        first = 6;
    } else {
        digits = 5;
        menu_split_digits(game_data.gears[game_data.characters[ch].gearId].hp);
        first = 4;
    }
    panel->hp_count = 0;
    for (i = 0; i < digits; i++) {
        digit = menu_state_current->digits[first + i];
        if (digit != 0xff) {
            panel->hp_count += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &panel->hp[panel->hp_count * 2],
                                              menu_state_current->buffer_index, i * 8 + x->hp, row * 56 + y->hp, 0x1000);
        }
    }
    if (layout == 0) {
        menu_split_digits(game_data.characters[ch].maxHp);
    } else {
        menu_split_digits(game_data.gears[game_data.characters[ch].gearId].maxHp);
    }
    panel->hp_max_count = 0;
    for (i = 0, n = 0; i < digits; i++) {
        digit = menu_state_current->digits[first + i];
        if (digit != 0xff) {
            panel->hp_max_count += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &panel->hp_max[panel->hp_max_count * 2],
                                              menu_state_current->buffer_index, n * 8 + x->hpMax, row * 56 + y->hpMax, 0x1000);
            n++;
        }
    }
    if (layout == 0) {
        menu_split_digits(game_data.characters[ch].ep);
        panel->ep_count = 0;
        for (i = 0; i < 2; i++) {
            digit = menu_state_current->digits[7 + i];
            if (digit != 0xff) {
                panel->ep_count += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &panel->ep[panel->ep_count * 2],
                                                  menu_state_current->buffer_index, i * 8 + x->ep, row * 56 + y->ep, 0x1000);
            }
        }
        menu_split_digits(game_data.characters[ch].maxEp);
        panel->ep_max_count = 0;
        for (i = 0, n = 0; i < 2; i++) {
            digit = menu_state_current->digits[7 + i];
            if (digit != 0xff) {
                panel->ep_max_count += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &panel->ep_max[panel->ep_max_count * 2],
                                                  menu_state_current->buffer_index, n * 8 + x->epMax, row * 56 + y->epMax,
                                                  0x1000);
                n++;
            }
        }
    }
}

/* 801CE0CC: Build the parts of `panel` (801cd81c, 801cdb1c, 801cdc6c) and show it. */
void menu_status_panel_build(MenuStatusPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y, u8 layout) {
    menu_status_panel_layout_sprites(panel, ch, row, x, y, layout);
    menu_status_panel_layout_level(panel, ch, row, x, y);
    menu_status_panel_layout_hp_ep(panel, ch, row, x, y, layout);
    panel->shown = 1;
    panel->buffer = menu_state_current->buffer_index;
}

/* 801CE198: Project `count` quads: each takes the next four of `verts` into every
 * other quad of `polys` from `index` and is added to the frame. */
void menu_draw_projected_quads(s32 count, SVECTOR *verts, POLY_FT4 *polys, s32 first) {
    long p;
    long flag;
    s32 i;

    for (i = 0; i < count; i++) {
        RotTransPers4(&verts[i * 4], &verts[i * 4 + 1], &verts[i * 4 + 2], &verts[i * 4 + 3],
                      (long *)&polys[first + i * 2].x0, (long *)&polys[first + i * 2].x1,
                      (long *)&polys[first + i * 2].x2, (long *)&polys[first + i * 2].x3, &p,
                      &flag);
        AddPrim(&menu_state_current->current->ot[4], &polys[first + i * 2]);
    }
}

/* 801CE2B4: Add `count` quads of `polys` to the frame, every other one from `first`. */
void menu_draw_quads(s32 count, POLY_FT4 *polys, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        AddPrim(&menu_state_current->current->ot[4], &polys[first + i * 2]);
    }
}

/* 801CE338: Add the shared mode primitive and, while panel 4 shows, its quad. */
void menu_draw_highlight_sprite(void) {
    AddPrim(&menu_state_current->current->ot[4], &menu_state_current->prims->mode_label[menu_state_current->prims->shade_buffer]);
    if (menu_state_current->flags->sprite_shown != 0) {
        AddPrim(&menu_state_current->current->ot[4], &menu_state_current->prims->sprite[menu_state_current->prims->sprite_buffer]);
    }
}

/* 801CE3C8: While party flag +2f is set, add the current quad of each visible marker. */
void menu_draw_markers(void) {
    s32 i;

    if (menu_state_current->flags->markers_shown != 0) {
        for (i = 0; i < 4; i++) {
            if (menu_state_current->markers->shown[i] != 0) {
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->markers->polys[i * 2 + menu_state_current->markers->buffer[i]]);
            }
        }
    }
}

/* 801CE464: Draw the field menu blocks: the command list and its cursor (+340) and the
 * two lists of the second block (+344). */
void menu_draw_money_and_play_time(void) {
    if (menu_state_current->flags->field_menu_shown != 0) {
        menu_draw_quads(menu_state_current->field_menu->count, menu_state_current->field_menu->polys, menu_state_current->field_menu->start);
        AddPrim(&menu_state_current->current->ot[4], &menu_state_current->field_menu->cursor[menu_state_current->field_menu->start]);
    }
    if (menu_state_current->flags->field_menu2_shown != 0) {
        menu_draw_quads(7, menu_state_current->field_menu2->polys, menu_state_current->field_menu2->start);
        menu_draw_quads(4, menu_state_current->field_menu2->polys2, menu_state_current->field_menu2->start);
    }
}

/* 801CE540: Draw the shown field blocks: their two frame quads and part lists. */
void menu_draw_field_blocks(void) {
    s32 i;
    MenuFieldBlock *block;

    for (i = 0; i < 3; i++) {
        block = menu_state_current->field_blocks[i];
        if (menu_state_current->flags->field_blocks_shown[i] != 0) {
            AddPrim(&menu_state_current->current->ot[4], &block->portrait[block->buffer]);
            AddPrim(&menu_state_current->current->ot[4], &block->name[block->buffer]);
            menu_draw_quads(block->count0, block->list0, block->buffer);
            menu_draw_quads(block->count1, block->list1, block->buffer);
            menu_draw_quads(block->count2, block->list2, block->buffer);
            menu_draw_quads(block->count3, block->list3, block->buffer);
            menu_draw_quads(block->count4, block->list4, block->buffer);
            menu_draw_quads(block->count5, block->list5, block->buffer);
            menu_draw_quads(block->count6, block->list6, block->buffer);
        }
    }
}

/* 801CE660: While party flag +7 is set, draw the detail panel: portrait, name, tabs,
 * layout parts and the number lists (level2, value40 and expNext are not
 * drawn here). */
void menu_draw_detail_panel(void) {
    if (menu_state_current->flags->detail_shown != 0) {
        menu_draw_projected_quads(1, menu_state_current->detail->portraitAt, menu_state_current->detail->portrait, menu_state_current->detail->buffer);
        menu_draw_projected_quads(1, menu_state_current->detail->nameAt, menu_state_current->detail->name, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->tabCount, menu_state_current->detail->tabsAt[0], menu_state_current->detail->tabs, menu_state_current->detail->tabBuffer);
        menu_draw_projected_quads(menu_state_current->detail->count, menu_state_current->detail->partsAt[0], menu_state_current->detail->parts, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->hpCount, menu_state_current->detail->hpAt[0], menu_state_current->detail->hp, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->hpMaxCount, menu_state_current->detail->hpMaxAt[0], menu_state_current->detail->hpMax, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->epCount, menu_state_current->detail->epAt[0], menu_state_current->detail->ep, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->epMaxCount, menu_state_current->detail->epMaxAt[0], menu_state_current->detail->epMax, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->levelCount, menu_state_current->detail->levelAt[0], menu_state_current->detail->level, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->value3CCount, menu_state_current->detail->value3CAt[0], menu_state_current->detail->value3C, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->expCount, menu_state_current->detail->expAt[0], menu_state_current->detail->exp, menu_state_current->detail->buffer);
        menu_draw_projected_quads(menu_state_current->detail->list1C70Count, menu_state_current->detail->list1C70At[0], menu_state_current->detail->list1C70, menu_state_current->detail->buffer);
    }
}

/* 801CE860: Draw the shown rows of the block at +35c: while highlighted, each row's
 * highlight quad and second part list; then its bar and first part list. */
void menu_draw_equip_panel_rows(void) {
    s32 i;
    long p;
    long flag;

    for (i = 0; i < 7; i++) {
        if (menu_state_current->equip_panel->rowShown[i] != 0) {
            if (menu_state_current->equip_panel->highlighted != 0) {
                RotTransPers4(&menu_state_current->equip_panel->highlightAt[i][0],
                              &menu_state_current->equip_panel->highlightAt[i][1],
                              &menu_state_current->equip_panel->highlightAt[i][2],
                              &menu_state_current->equip_panel->highlightAt[i][3],
                              (long *)&menu_state_current->equip_panel->highlights[i][menu_state_current->equip_panel->highlightBuffer[i]].x0,
                              (long *)&menu_state_current->equip_panel->highlights[i][menu_state_current->equip_panel->highlightBuffer[i]].x1,
                              (long *)&menu_state_current->equip_panel->highlights[i][menu_state_current->equip_panel->highlightBuffer[i]].x2,
                              (long *)&menu_state_current->equip_panel->highlights[i][menu_state_current->equip_panel->highlightBuffer[i]].x3,
                              &p, &flag);
                AddPrim(&menu_state_current->current->ot[4], &menu_state_current->equip_panel->highlights[i][menu_state_current->equip_panel->highlightBuffer[i]]);
                menu_draw_projected_quads(menu_state_current->equip_panel->rowBCount[i], menu_state_current->equip_panel->rowBAt[i], menu_state_current->equip_panel->rowB[i], menu_state_current->equip_panel->rowBBuffer[i]);
            }
            RotTransPers4(&menu_state_current->equip_panel->barAt[i][0],
                          &menu_state_current->equip_panel->barAt[i][1],
                          &menu_state_current->equip_panel->barAt[i][2],
                          &menu_state_current->equip_panel->barAt[i][3],
                          (long *)&menu_state_current->equip_panel->bars[i][menu_state_current->equip_panel->barBuffer[i]].x0,
                          (long *)&menu_state_current->equip_panel->bars[i][menu_state_current->equip_panel->barBuffer[i]].x1,
                          (long *)&menu_state_current->equip_panel->bars[i][menu_state_current->equip_panel->barBuffer[i]].x2,
                          (long *)&menu_state_current->equip_panel->bars[i][menu_state_current->equip_panel->barBuffer[i]].x3,
                          &p, &flag);
            AddPrim(&menu_state_current->current->ot[4], &menu_state_current->equip_panel->bars[i][menu_state_current->equip_panel->barBuffer[i]]);
            menu_draw_projected_quads(menu_state_current->equip_panel->rowACount[i], menu_state_current->equip_panel->rowAAt[i], menu_state_current->equip_panel->rowA[i], menu_state_current->equip_panel->rowABuffer[i]);
        }
    }
}

/* 801CEB5C: While party flag +8 is set, draw the sprites of the block at +35c. */
void menu_draw_equip_panel(void) {
    MenuEquipPanel *block;

    if (menu_state_current->flags->equipment_shown != 0) {
        block = menu_state_current->equip_panel;
        menu_draw_projected_quads(block->kind, block->verts, block->polys, block->buffer);
        menu_draw_equip_panel_rows();
    }
}

/* 801CEBB4: While party flag +4b is set, draw the visible labels of the block at +360. */
void menu_draw_equip_labels(void) {
    s32 i;

    if (menu_state_current->flags->equip_labels_shown != 0) {
        for (i = 0; i < 5; i++) {
            if (menu_state_current->equip_labels->visible[i] != 0) {
                menu_draw_projected_quads(1, menu_state_current->equip_labels->labels[i].verts,
                              menu_state_current->equip_labels->labels[i].polys, menu_state_current->equip_labels->count);
            }
        }
    }
}

/* 801CEC40: While party flag +9 is set, draw both screen image lists, first applying
 * a changed dimming (semi-transparent, 20h grey). */
void menu_draw_screen_images(void) {
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
        menu_draw_quads(menu_state_current->images->count2, menu_state_current->images->packets2, menu_state_current->images->buffer2);
        menu_draw_quads(menu_state_current->images->count, menu_state_current->images->packets, menu_state_current->images->buffer);
    }
}

/* 801CF308: While party flag +a is set, draw the two sprite lists of the block at +354. */
void menu_draw_sprite_lists(void) {
    if (menu_state_current->flags->lists_shown != 0) {
        menu_draw_quads(menu_state_current->lists->second_count, menu_state_current->lists->second,
                      menu_state_current->lists->second_buffer);
        menu_draw_quads(menu_state_current->lists->first_count, menu_state_current->lists->first,
                      menu_state_current->lists->first_buffer);
    }
}

/* 801CF37C: In file selection (+4d8 == 2), draw the pulsing cursor box of every slot
 * of the selected slot's file (only the selected slot when it is empty). */
void menu_draw_file_cursor_boxes(void) {
    MenuSlotImage *image;
    s32 i;
    long p;
    long flag;
    u8 file;
    u8 match;

    if (menu_state_current->load_state == 2) {
        file = menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]];
        for (i = 0; i < 32; i++) {
            match = 1;
            if (file == menu_state_current->card->file_slots[i]) {
                if (file == 0xff) {
                    match = i == menu_file_cursor_card_slots[menu_state_current->card->cursor];
                }
                if (match) {
                    image = menu_state_current->slots[i];
                    (image->box + menu_state_current->buffer_index)->r0 = menu_state_current->unknown4d4;
                    (image->box + menu_state_current->buffer_index)->g0 = menu_state_current->unknown4d4;
                    (image->box + menu_state_current->buffer_index)->b0 = menu_state_current->unknown4d4;
                    RotTransPers4(&image->iconAt[0], &image->iconAt[1], &image->iconAt[2],
                                  &image->iconAt[3],
                                  (long *)&image->box[menu_state_current->buffer_index].x0,
                                  (long *)&image->box[menu_state_current->buffer_index].x1,
                                  (long *)&image->box[menu_state_current->buffer_index].x2,
                                  (long *)&image->box[menu_state_current->buffer_index].x3, &p, &flag);
                    AddPrim(&menu_state_current->current->ot[4], &image->box[menu_state_current->buffer_index]);
                    AddPrim(&menu_state_current->current->ot[4], &image->boxMode[menu_state_current->buffer_index]);
                }
            }
        }
    }
}

/* 801CF5E4: Draw the file icons of card rows `first`..`end` - 1 that are in use into
 * the file screen slots from `firstSlot` on: each icon's texture window (u by row,
 * v by animation frame +4cc) and palette, projected and drawn. */
void menu_draw_file_icons(s32 first, s32 firstSlot, s32 end) {
    MenuSlotImage *image;
    s32 row;
    s32 slot;
    s32 i;

    row = first;
    slot = firstSlot;
    for (; row < end; row++) {
        if (menu_state_current->card->files[row].state != 0) {
            for (i = 0; i < menu_state_current->card->heads[row][3]; i++, slot++) {
                image = menu_state_current->slots[slot];
                (image->icon + menu_state_current->buffer_index)->u0 = row * 16;
                (image->icon + menu_state_current->buffer_index)->v0 = menu_state_current->card->files[row].frames[menu_state_current->unknown4cc];
                (image->icon + menu_state_current->buffer_index)->u1 = row * 16 + 16;
                (image->icon + menu_state_current->buffer_index)->v1 = menu_state_current->card->files[row].frames[menu_state_current->unknown4cc];
                (image->icon + menu_state_current->buffer_index)->u2 = row * 16;
                (image->icon + menu_state_current->buffer_index)->v2 = menu_state_current->card->files[row].frames[menu_state_current->unknown4cc] + 16;
                (image->icon + menu_state_current->buffer_index)->u3 = row * 16 + 16;
                (image->icon + menu_state_current->buffer_index)->v3 = menu_state_current->card->files[row].frames[menu_state_current->unknown4cc] + 16;
                image->icon[menu_state_current->buffer_index].clut = GetClut(row * 16, row / 16 + 0x1c1);
                menu_draw_projected_quads(1, image->iconAt, image->icon, menu_state_current->buffer_index);
            }
        }
    }
}

/* 801CF8D8: While the file screen is up, build (while party flag +68 is set) and draw
 * the header of each present card: its label (122 on the cursor's card
 * while +2f is set, else 115) and its name sprite. */
void menu_draw_card_headers(void) {
    u8 built[2];
    s32 side;
    s32 parts;
    s32 i;
    u8 normal;

    built[1] = 0;
    built[0] = 0;
    if (menu_state_current->load_state != 0) {
        for (side = 0; side < 2; side++) {
            if (menu_state_current->card->present[side] != 0 && menu_state_current->flags->card_mode != 0) {
                normal = 1;
                if (menu_state_current->flags->markers_shown != 0 && side == menu_file_cursor_card_slots[menu_state_current->card->cursor] / 16) {
                    normal = 0;
                }
                if (normal) {
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x115, menu_state_current->card->card_headers[side].label,
                                  menu_state_current->buffer_index, side * 0x90 + 0x1e, 0x36, 0x1000);
                } else {
                    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x122, menu_state_current->card->card_headers[side].label,
                                  menu_state_current->buffer_index, side * 0x90 + 0x1e, 0x36, 0x1000);
                }
                parts = sprite_sheet_draw_scaled(menu_state_current->sheet, side + 0x162, menu_state_current->card->card_headers[side].name,
                                      menu_state_current->buffer_index, side * 0x90 + 0x1b, 0x36, 0x1000);
                built[side] = 1;
            }
            if (built[side]) {
                for (i = 0; i < parts; i++) {
                    AddPrim(&menu_state_current->current->ot[4],
                            &menu_state_current->card->card_headers[side].name[i * 2 + menu_state_current->buffer_index]);
                }
                AddPrim(&menu_state_current->current->ot[4],
                        &menu_state_current->card->card_headers[side].label[menu_state_current->buffer_index]);
            }
        }
    }
}

/* 801CFB48: While the file screen is up, draw the connector lines of every slot but
 * the last of each present card: red within the selected file during file
 * selection, green otherwise. */
void menu_draw_file_slot_frames(void) {
    MenuSlotImage *image;
    s32 i;
    long p;
    long flag;
    u8 file;
    u8 match;

    if (menu_state_current->load_state != 0) {
        file = menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]];
        for (i = 0; i < 32; i++) {
            image = menu_state_current->slots[i];
            if (menu_state_current->card->present[i / 16] && (i / 16) * 16 != i - 15) {
                match = 0;
                if (file == menu_state_current->card->file_slots[i] && menu_state_current->load_state == 2) {
                    match = 1;
                    if (file == 0xff) {
                        match = i == menu_file_cursor_card_slots[menu_state_current->card->cursor];
                    }
                }
                if (match) {
                    (image->lineA + menu_state_current->buffer_index)->r0 = 0xff;
                    (image->lineA + menu_state_current->buffer_index)->g0 = 0;
                    (image->lineA + menu_state_current->buffer_index)->b0 = 0;
                    (image->lineB + menu_state_current->buffer_index)->r0 = 0xff;
                    (image->lineB + menu_state_current->buffer_index)->g0 = 0;
                    (image->lineB + menu_state_current->buffer_index)->b0 = 0;
                } else {
                    (image->lineA + menu_state_current->buffer_index)->r0 = 0;
                    (image->lineA + menu_state_current->buffer_index)->g0 = 0xff;
                    (image->lineA + menu_state_current->buffer_index)->b0 = 0;
                    (image->lineB + menu_state_current->buffer_index)->r0 = 0;
                    (image->lineB + menu_state_current->buffer_index)->g0 = 0xff;
                    (image->lineB + menu_state_current->buffer_index)->b0 = 0;
                }
                RotTransPers3(&image->lineAAt[0], &image->lineAAt[1], &image->lineAAt[3],
                              (long *)&image->lineA[menu_state_current->buffer_index].x0,
                              (long *)&image->lineA[menu_state_current->buffer_index].x1,
                              (long *)&image->lineA[menu_state_current->buffer_index].x2, &p, &flag);
                AddPrim(&menu_state_current->current->ot[4], &image->lineA[menu_state_current->buffer_index]);
                RotTransPers3(&image->lineBAt[0], &image->lineBAt[2], &image->lineBAt[3],
                              (long *)&image->lineB[menu_state_current->buffer_index].x0,
                              (long *)&image->lineB[menu_state_current->buffer_index].x1,
                              (long *)&image->lineB[menu_state_current->buffer_index].x2, &p, &flag);
                AddPrim(&menu_state_current->current->ot[4], &image->lineB[menu_state_current->buffer_index]);
            }
        }
    }
}

/* 801CFF64: While the card access indicator is shown, draw its bar (as long as the
 * progress +7b0), its sprite and the blinking arrows (steady while closing);
 * while running, advance the progress and wrap it past 100. */
void menu_draw_card_indicator(void) {
    if (menu_state_current->flags->cursors_shown[2] != 0) {
        setXY4(&menu_state_current->indicator->fills[menu_state_current->indicator->buffer], 0x20, 0x61, menu_state_current->indicator->unk7B0 + 0x20, 0x61,
               0x20, 0x68, menu_state_current->indicator->unk7B0 + 0x20, 0x68);
        if ((u32)menu_state_current->frame_counter % 6 >= 4 || menu_state_current->flags->cursors_shown[2] == 2) {
            menu_draw_quads(2, menu_state_current->indicator->spriteB, menu_state_current->indicator->buffer);
        }
        menu_draw_quads(12, menu_state_current->indicator->spriteA, menu_state_current->indicator->buffer);
        AddPrim(&menu_state_current->current->ot[4], &menu_state_current->indicator->fills[menu_state_current->indicator->buffer]);
        if (menu_state_current->flags->cursors_shown[2] == 1) {
            if ((menu_state_current->indicator->unk7B0 += menu_state_current->indicator->unk7B4) > 0x100) {
                menu_state_current->indicator->unk7B0 = 0;
            }
        }
    }
}

/* 801D01D0: Animate the file screen: its layers, the 15-frame x 6 blink and the
 * 4-step pulse between 4 and 128. */
void menu_file_screen_update(void) {
    menu_draw_file_cursor_boxes();
    menu_draw_file_icons(0, 0, 0x10);
    menu_draw_file_icons(0x10, 0x10, 0x20);
    menu_draw_file_slot_frames();
    menu_draw_card_headers();
    menu_draw_card_indicator();
    if (++menu_state_current->unknown4d0 == 15) {
        menu_state_current->unknown4d0 = 0;
        if (++menu_state_current->unknown4cc == 6) {
            menu_state_current->unknown4cc = 0;
        }
    }
    if (menu_state_current->unknown4d9 == 0) {
        menu_state_current->unknown4d4 += 4;
        if (menu_state_current->unknown4d4 > 0x80) {
            menu_state_current->unknown4d9 = 1;
            menu_state_current->unknown4d4 = 0x7c;
        }
    } else {
        menu_state_current->unknown4d4 -= 4;
        if (menu_state_current->unknown4d4 < 0) {
            menu_state_current->unknown4d9 = 0;
            menu_state_current->unknown4d4 = 4;
        }
    }
}

/* 801D02D8: While party flag +b is set, draw the save/load screen's block (+34c):
 * without the file details, the 32 text characters and the selected file's
 * icon cursor (its slot's column and animation step); with them, each shown
 * view's lists, image and name and the shared play time, disc and title
 * lists. Then the shaded band. */
void menu_draw_file_info(void) {
    s32 i;

    if (!menu_state_current->flags->file_info_shown) {
        return;
    }
    if (!menu_state_current->file_info->rebuilt) {
        menu_draw_quads(32, menu_state_current->file_info->chars, menu_state_current->buffer_index);
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->u0 =
            menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]] * 16;
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->v0 =
            menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]]
                .frames[menu_state_current->unknown4cc];
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->u1 =
            menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]] * 16 + 15;
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->v1 =
            menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]]
                .frames[menu_state_current->unknown4cc];
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->u2 =
            menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]] * 16;
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->v2 =
            menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]]
                .frames[menu_state_current->unknown4cc] + 15;
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->u3 =
            menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]] * 16 + 15;
        (menu_state_current->file_info->cursor + menu_state_current->buffer_index)->v3 =
            menu_state_current->card->files[menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]]]
                .frames[menu_state_current->unknown4cc] + 15;
        menu_state_current->file_info->cursor[menu_state_current->buffer_index].clut =
            GetClut(menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]] * 16,
                    menu_state_current->card->file_slots[menu_file_cursor_card_slots[menu_state_current->card->cursor]] / 16 + 0x1c1);
        AddPrim(&menu_state_current->current->ot[4], &menu_state_current->file_info->cursor[menu_state_current->buffer_index]);
    } else {
        for (i = 0; i < 3; i++) {
            if (menu_state_current->file_info->views[i].shown) {
                menu_draw_quads(menu_state_current->file_info->views[i].frameCount, menu_state_current->file_info->views[i].frame[0],
                              menu_state_current->file_info->views[i].frameBuffer);
                menu_draw_quads(menu_state_current->file_info->views[i].levelCount, menu_state_current->file_info->views[i].levelDigits[0],
                              menu_state_current->file_info->views[i].buffer);
                menu_draw_quads(menu_state_current->file_info->views[i].level2Count, menu_state_current->file_info->views[i].levelDigits[3],
                              menu_state_current->file_info->views[i].buffer);
                menu_draw_quads(menu_state_current->file_info->views[i].hpCount, menu_state_current->file_info->views[i].hpDigits[0],
                              menu_state_current->file_info->views[i].buffer);
                menu_draw_quads(menu_state_current->file_info->views[i].hpMaxCount, menu_state_current->file_info->views[i].hpMaxDigits[0],
                              menu_state_current->file_info->views[i].buffer);
                menu_draw_quads(menu_state_current->file_info->views[i].epCount, menu_state_current->file_info->views[i].epDigits[0],
                              menu_state_current->file_info->views[i].buffer);
                menu_draw_quads(menu_state_current->file_info->views[i].epMaxCount, menu_state_current->file_info->views[i].epMaxDigits[0],
                              menu_state_current->file_info->views[i].buffer);
                AddPrim(&menu_state_current->current->ot[4],
                        &menu_state_current->file_info->views[i].image[menu_state_current->file_info->views[0].buffer]);
                AddPrim(&menu_state_current->current->ot[4],
                        &menu_state_current->file_info->views[i].name[menu_state_current->file_info->views[i].nameBuffer]);
            }
        }
        menu_draw_quads(11, menu_state_current->file_info->colon0, menu_state_current->file_info->views[0].buffer);
        menu_draw_quads(4, menu_state_current->file_info->discLabel, menu_state_current->file_info->views[0].buffer);
        menu_draw_quads(16, menu_state_current->file_info->title, menu_state_current->file_info->views[0].buffer);
    }
    AddPrim(&menu_state_current->current->ot[4], &menu_state_current->file_info->band[menu_state_current->buffer_index]);
}

/* 801D0954: Project the four vertices `v` into quad `index` of `polys` and add it at
 * depth `otz`. */
void menu_draw_projected_quad(SVECTOR *v, POLY_FT4 *polys, s32 index, s32 otz) {
    long p;
    long flag;
    POLY_FT4 *poly;

    poly = &polys[index];
    RotTransPers4(&v[0], &v[1], &v[2], &v[3], (long *)&poly->x0, (long *)&poly->x1,
                  (long *)&poly->x2, (long *)&poly->x3, &p, &flag);
    AddPrim(&menu_state_current->current->ot[otz], poly);
}

/* 801D09F0: Draw panel `index`: its corners, the scroll bar with `has_bar`, the edges
 * and the fill. */
void menu_draw_panel(s32 index, u8 has_bar) {
    MenuPanel *panel;
    s32 i;
    long p;
    long flag;

    panel = menu_state_current->panels[index];
    for (i = 0; i < 4; i++) {
        menu_draw_projected_quad(&panel->corner_at[i * 4], &panel->corner[i * 2], panel->buffer, panel->ot_entry);
    }
    if (has_bar) {
        for (i = 0; i < 2; i++) {
            menu_draw_projected_quad(&panel->ends_at[i * 4], &panel->bar_ends[i * 2], panel->buffer, panel->ot_entry);
        }
        menu_draw_projected_quad(panel->side_at, panel->bar_side, panel->buffer, panel->ot_entry);
    }
    menu_draw_projected_quad(panel->edge_at[0][0], &panel->edge[0][0], panel->buffer, panel->ot_entry);
    menu_draw_projected_quad(panel->edge_at[0][1], &panel->edge[0][2], panel->buffer, panel->ot_entry);
    menu_draw_projected_quad(panel->edge_at[1][0], &panel->edge[1][0], panel->buffer, panel->ot_entry);
    menu_draw_projected_quad(panel->edge_at[1][1], &panel->edge[1][2], panel->buffer, panel->ot_entry);
    menu_draw_projected_quad(panel->edge_at[2][0], &panel->edge[2][0], panel->buffer, panel->ot_entry);
    menu_draw_projected_quad(panel->edge_at[2][1], &panel->edge[2][2], panel->buffer, panel->ot_entry);
    menu_draw_projected_quad(panel->edge_at[3][0], &panel->edge[3][0], panel->buffer, panel->ot_entry);
    menu_draw_projected_quad(panel->edge_at[3][1], &panel->edge[3][2], panel->buffer, panel->ot_entry);
    RotTransPers4(&panel->fill_at[0], &panel->fill_at[1], &panel->fill_at[2], &panel->fill_at[3],
                  (long *)&panel->fill[panel->buffer].x0, (long *)&panel->fill[panel->buffer].x1,
                  (long *)&panel->fill[panel->buffer].x2, (long *)&panel->fill[panel->buffer].x3,
                  &p, &flag);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->fill[panel->buffer]);
    AddPrim(&menu_state_current->current->ot[panel->ot_entry], &panel->fill_mode[panel->buffer]);
}

/* 801D0C78: Draw the shown panels; those not flat are drawn under an identity
 * rotation at depth 0x200. */
void menu_draw_panels(void) {
    s32 i;
    SVECTOR angles;
    VECTOR offset;
    MATRIX m;
    SVECTOR unused;

    for (i = 0; i < 7; i++) {
        if (menu_state_current->flags->panels_shown[i] != 0) {
            if (menu_state_current->panels[i]->flat == 0) {
                PushMatrix();
                angles.vz = 0;
                angles.vy = 0;
                angles.vx = 0;
                offset.vy = 0;
                offset.vx = 0;
                offset.vz = 0x200;
                gpu_build_rotation_matrix(&angles, &m);
                TransMatrix(&m, &offset);
                SetRotMatrix(&m);
                SetTransMatrix(&m);
                menu_draw_panel(i, menu_state_current->panels[i]->has_bar);
                PopMatrix();
            } else {
                menu_draw_panel(i, menu_state_current->panels[i]->has_bar);
            }
        }
    }
}

/* 801D0D90: Add the current quad of each top label whose party flag (+34) is set. */
void menu_draw_command_labels(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (menu_state_current->flags->labels_shown[i] != 0) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->labels[i].polys[menu_state_current->labels[i].buffer]);
        }
    }
}

/* 801D0E20: A short busy delay (eight iterations). */
void menu_spin_delay_8(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
    }
}

/* 801D0E38: Draw the sprites of labels 8-13 whose party flags (+14) and own flags are set. */
void menu_draw_row_labels(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (menu_state_current->flags->row_labels_shown[i] != 0 && menu_state_current->row_labels[i].projected != 0) {
            menu_draw_projected_quads(1, menu_state_current->row_labels[i].verts, menu_state_current->row_labels[i].polys,
                          menu_state_current->row_labels[i].buffer);
        }
    }
}

/* 801D0EBC: A short busy delay (six iterations). */
void menu_spin_delay_6(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
    }
}

/* 801D0ED4: Draw the sprites of labels 20-27 whose party flags (+38) are set. */
void menu_draw_item_arts_labels(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (menu_state_current->flags->labels10e0_shown[i] != 0) {
            menu_draw_projected_quads(1, menu_state_current->labels10e0[i].verts, menu_state_current->labels10e0[i].polys,
                          menu_state_current->labels10e0[i].buffer);
        }
    }
}

/* 801D0F54: Draw the sprites of labels 28-33 whose party flags (+40) are set. */
void menu_draw_equip_screen_labels(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (menu_state_current->flags->labels14e0_shown[i] != 0) {
            menu_draw_projected_quads(1, menu_state_current->labels14e0[i].verts, menu_state_current->labels14e0[i].polys,
                          menu_state_current->labels14e0[i].buffer);
        }
    }
}

/* 801D0FD4: While party flag +4e is set, add label 17e0's current quad to the frame. */
void menu_draw_deathblow_screen_label(void) {
    if (menu_state_current->flags->label17e0_shown != 0) {
        AddPrim(&menu_state_current->current->ot[4],
                      &menu_state_current->labels17e0[0].polys[menu_state_current->labels17e0[0].buffer]);
    }
}

/* 801D1030: While party flag +2e is set, draw the three notice labels (+1de0): their
 * sprites when visible, else their current quad. */
void menu_draw_notice_labels(void) {
    s32 i;
    MenuLabel *label;

    if (menu_state_current->flags->messages_shown != 0) {
        for (i = 0; i < 3; i++) {
            label = menu_state_current->message_labels[i];
            if (label->projected != 0) {
                menu_draw_projected_quads(1, label->verts, label->polys, label->buffer);
            } else {
                AddPrim(&menu_state_current->current->ot[4], &label->polys[label->buffer]);
            }
        }
    }
}

/* 801D10DC: Draw the visible labels at +18e0 whose party flags (+54) are set. */
void menu_draw_gear_command_labels(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (menu_state_current->flags->labels18e0_shown[i] != 0 && menu_state_current->labels18e0[i].projected != 0) {
            menu_draw_projected_quads(1, menu_state_current->labels18e0[i].verts, menu_state_current->labels18e0[i].polys,
                          menu_state_current->labels18e0[i].buffer);
        }
    }
}

/* 801D1160: Add the current quad of each sound label whose party flag (+5c) is set. */
void menu_draw_sound_labels(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (menu_state_current->flags->sound_labels_shown[i] != 0) {
            AddPrim(&menu_state_current->current->ot[4],
                          &menu_state_current->sound_labels[i].polys[menu_state_current->sound_labels[i].buffer]);
        }
    }
}

/* 801D11F0: Draw the label layers of the field menu screen. */
void menu_draw_label_layers(void) {
    menu_draw_command_labels();
    menu_spin_delay_8();
    menu_draw_row_labels();
    menu_spin_delay_6();
    menu_draw_gear_command_labels();
    menu_draw_sound_labels();
    menu_draw_item_arts_labels();
    menu_draw_equip_screen_labels();
    menu_draw_deathblow_screen_label();
    menu_draw_notice_labels();
}

/* 801D1258: Add this buffer's fill and mode primitives to the frame. */
void menu_draw_fade(void) {
    AddPrim(&menu_state_current->current->ot[8], &menu_state_current->prims->fade[menu_state_current->buffer_index]);
    AddPrim(&menu_state_current->current->ot[8], &menu_state_current->prims->mode_sprite[menu_state_current->buffer_index]);
}

/* 801D12D4: Draw `panel` when shown: its frame quads and part lists (and the extra list
 * when `extra`). */
void menu_draw_status_panel(MenuStatusPanel *panel, u8 extra) {
    if (panel->shown != 0) {
        AddPrim(&menu_state_current->current->ot[4], &panel->face[panel->buffer]);
        AddPrim(&menu_state_current->current->ot[4], &panel->label[panel->buffer]);
        menu_draw_quads(panel->layout_count, panel->layout, panel->buffer);
        menu_draw_quads(panel->level_count, panel->level, panel->buffer);
        menu_draw_quads(panel->level2_count, panel->level2, panel->buffer);
        menu_draw_quads(panel->hp_count, panel->hp, panel->buffer);
        menu_draw_quads(panel->hp_max_count, panel->hp_max, panel->buffer);
        menu_draw_quads(panel->ep_count, panel->ep, panel->buffer);
        menu_draw_quads(panel->ep_max_count, panel->ep_max, panel->buffer);
        if (extra) {
            menu_draw_quads(5, panel->extra, panel->buffer);
        }
    }
}

/* 801D13F8: While party flag +46 is set, draw the three party status panels (+1e08). */
void menu_draw_party_panels(void) {
    s32 i;

    if (menu_state_current->flags->party_panels_shown != 0) {
        for (i = 0; i < 3; i++) {
            menu_draw_status_panel(menu_state_current->party_panels[i], 1);
        }
    }
}

/* 801D1464: While party flag +49 is set, draw the sprites of the block at +43c. */
void menu_draw_scroll_bar(void) {
    MenuScrollBar *block;

    if (menu_state_current->flags->scroll_shown != 0) {
        block = menu_state_current->scroll;
        menu_draw_projected_quads(1, block->verts, block->polys, block->buffer);
    }
}

/* 801D14B0: While party flag +53 is set, draw the sprites of the block at +440. */
void menu_draw_member_marks(void) {
    MenuMarkerQuads *block;

    if (menu_state_current->flags->marks_shown != 0) {
        block = menu_state_current->marks;
        menu_draw_projected_quads(4, block->verts, block->polys, block->buffer);
    }
}

/* 801D14FC: While party flag +48 is set, draw the save/load screen list (+42c): each
 * shown row's name and value and, when shown, the three extra labels. */
void menu_draw_item_list(void) {
    s32 i;

    if (menu_state_current->flags->item_list_shown != 0) {
        for (i = 0; i < 16; i++) {
            if (menu_state_current->item_list->shown[i] != 0) {
                menu_draw_projected_quads(1, menu_state_current->item_list->names[i].verts, menu_state_current->item_list->names[i].polys,
                              menu_state_current->item_list->names[i].buffer);
                menu_draw_projected_quads(1, menu_state_current->item_list->values[i].verts, menu_state_current->item_list->values[i].polys,
                              menu_state_current->item_list->values[i].buffer);
            }
        }
        if (menu_state_current->item_list->extraShown != 0) {
            menu_draw_projected_quads(1, menu_state_current->item_list->extra[0].verts, menu_state_current->item_list->extra[0].polys,
                          menu_state_current->item_list->extra[0].buffer);
            menu_draw_projected_quads(1, menu_state_current->item_list->extra[1].verts, menu_state_current->item_list->extra[1].polys,
                          menu_state_current->item_list->extra[1].buffer);
            menu_draw_projected_quads(1, menu_state_current->item_list->extra[2].verts, menu_state_current->item_list->extra[2].polys,
                          menu_state_current->item_list->extra[2].buffer);
        }
    }
}

/* 801D1640: While party flag +4a is set, draw the file list (+430): each shown row's
 * name and value, when shown the header labels, and the footer. */
void menu_draw_arts_list(void) {
    s32 i;

    if (menu_state_current->flags->arts_list_shown != 0) {
        for (i = 0; i < 14; i++) {
            if (menu_state_current->arts_list->shown[i] != 0) {
                menu_draw_projected_quads(1, menu_state_current->arts_list->names[i].verts, menu_state_current->arts_list->names[i].polys,
                              menu_state_current->arts_list->names[i].buffer);
                menu_draw_projected_quads(1, menu_state_current->arts_list->values[i].verts, menu_state_current->arts_list->values[i].polys,
                              menu_state_current->arts_list->values[i].buffer);
            }
        }
        if (menu_state_current->arts_list->extraShown != 0) {
            menu_draw_projected_quads(1, menu_state_current->arts_list->headA.verts, menu_state_current->arts_list->headA.polys,
                          menu_state_current->arts_list->headA.buffer);
            menu_draw_projected_quads(1, menu_state_current->arts_list->headB.verts, menu_state_current->arts_list->headB.polys,
                          menu_state_current->arts_list->headB.buffer);
            for (i = 0; i < 2; i++) {
                menu_draw_projected_quads(1, menu_state_current->arts_list->extra[i].verts, menu_state_current->arts_list->extra[i].polys,
                              menu_state_current->arts_list->extra[i].buffer);
            }
        }
        menu_draw_projected_quads(1, menu_state_current->arts_list->footer.verts, menu_state_current->arts_list->footer.polys,
                      menu_state_current->arts_list->footer.buffer);
    }
}

/* 801D17C4: While party flag +4c is set, draw the list of the block at +434: each shown
 * row's name and value, the title and, when shown, the three extra labels. */
void menu_draw_equip_list(void) {
    s32 i;

    if (menu_state_current->flags->equip_list_shown != 0) {
        for (i = 0; i < 8; i++) {
            if (menu_state_current->equip_list->shown[i] != 0) {
                menu_draw_projected_quads(1, menu_state_current->equip_list->names[i].verts, menu_state_current->equip_list->names[i].polys,
                              menu_state_current->equip_list->names[i].buffer);
                menu_draw_projected_quads(1, menu_state_current->equip_list->values[i].verts, menu_state_current->equip_list->values[i].polys,
                              menu_state_current->equip_list->values[i].buffer);
            }
        }
        menu_draw_projected_quads(1, menu_state_current->equip_list->title.verts, menu_state_current->equip_list->title.polys,
                      menu_state_current->equip_list->title.buffer);
        if (menu_state_current->equip_list->extraShown != 0) {
            for (i = 0; i < 3; i++) {
                menu_draw_projected_quads(1, menu_state_current->equip_list->extra[i].verts, menu_state_current->equip_list->extra[i].polys,
                              menu_state_current->equip_list->extra[i].buffer);
            }
        }
    }
}

/* 801D1914: While party flag +4d is set, draw the status list (+438): each shown row's
 * name and value quads, the title, each row's parts and its gauge. */
void menu_draw_status_list(void) {
    s32 i;

    if (menu_state_current->flags->status_list_shown != 0) {
        for (i = 0; i < 13; i++) {
            if (menu_state_current->status_list->shown[i] != 0) {
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->status_list->names[i].polys[menu_state_current->status_list->names[i].buffer]);
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->status_list->values[i].polys[menu_state_current->status_list->names[i].buffer]);
            }
        }
        menu_draw_projected_quads(1, menu_state_current->status_list->title.verts, menu_state_current->status_list->title.polys,
                      menu_state_current->status_list->title.buffer);
        for (i = 0; i < 13; i++) {
            menu_draw_quads(menu_state_current->status_list->counts[i], menu_state_current->status_list->lists[i],
                          menu_state_current->status_list->starts[i]);
            if (menu_state_current->status_list->gaugeShown[i] != 0) {
                AddPrim(&menu_state_current->current->ot[4],
                              &menu_state_current->status_list->gauges[i][menu_state_current->status_list->gaugeBuffer[i]]);
            }
        }
    }
}

/* 801D1AAC: Draw the sprites of the two image blocks (+444) whose party flags are set. */
void menu_draw_list_cursors(void) {
    s32 i;
    MenuCursor *block;

    for (i = 0; i < 2; i++) {
        if (menu_state_current->flags->cursors_shown[i] != 0) {
            block = menu_state_current->cursors[i];
            menu_draw_projected_quads(1, block->verts, block->polys, block->buffer);
        }
    }
}

/* 801D1B20: Build the field menu screen, layer by layer. */
void menu_draw_field_menu_screen(void) {
    menu_panel_grow_opening();
    menu_draw_label_layers();
    menu_draw_markers();
    menu_draw_highlight_sprite();
    menu_draw_file_info();
    menu_file_screen_update();
    menu_draw_field_blocks();
    menu_draw_detail_panel();
    menu_draw_equip_panel();
    menu_draw_equip_labels();
    menu_draw_money_and_play_time();
    menu_draw_party_panels();
    menu_draw_list_cursors();
    menu_draw_item_list();
    menu_draw_arts_list();
    menu_draw_equip_list();
    menu_draw_status_list();
    menu_draw_scroll_bar();
    menu_draw_member_marks();
    menu_draw_panels();
    menu_draw_screen_images();
    menu_draw_sprite_lists();
}

/* 801D1BE8: Draw one layer set of the field menu screen. */
void menu_draw_title_load_screen(void) {
    menu_panel_grow_opening();
    menu_draw_label_layers();
    menu_draw_markers();
    menu_draw_highlight_sprite();
    menu_draw_file_info();
    menu_file_screen_update();
    menu_draw_screen_images();
    menu_draw_sprite_lists();
    menu_draw_panels();
}

/* 801D1C48: Draw the other layer set of the field menu screen. */
void menu_draw_cd_change_screen(void) {
    menu_panel_grow_opening();
    menu_draw_markers();
    menu_draw_label_layers();
    menu_draw_highlight_sprite();
    menu_draw_file_info();
    menu_file_screen_update();
    menu_draw_panels();
    menu_draw_sprite_lists();
}

/* 801D1CA0: Build the screen of the menu kind (when drawing), then the shared prims. */
void menu_draw_screen(void) {
    if (menu_state_current->drawing != 0) {
        switch (menu_state_screen) {
        case 0:
            menu_draw_field_menu_screen();
            break;
        case 2:
            menu_draw_title_load_screen();
            break;
        case 6:
            menu_draw_cd_change_screen();
            break;
        }
    }
    menu_draw_fade();
}

/* 801D1D40: Step the view motion (3/4 start moving in/out, 1/2 move) and load the view
 * rotation and translation into the GTE. */
void menu_view_update(void) {
    MenuState *state;
    u8 motion;

    state = menu_state_current;
    switch (state->view_motion) {
    case 4:
        state->offset.vz = 0x200;
        motion = 2;
        goto start;
    case 3:
        state->offset.vz = 0x800;
        motion = 1;
    start:
        state->angles.vz = 0;
        state->angles.vy = 0;
        state->angles.vx = 0;
        state->offset.vy = 0;
        state->offset.vx = 0;
        state->view_motion = motion;
        break;
    case 2:
        state->angles.vy -= 0x60;
        state->offset.vz += 0x40;
        if (state->offset.vz >= 0xe00) {
            state->view_motion = 0;
        }
        break;
    case 1:
        state->angles.vx += 0x7c;
        state->offset.vz -= 0x30;
        if (state->offset.vz < 0x200) {
            state->offset.vz = 0x200;
            state->angles.vz = 0;
            state->angles.vx = 0;
            state->angles.vy = 0;
            state->view_motion = 0;
        }
        break;
    }
    gpu_build_rotation_matrix(&menu_state_current->angles, &menu_state_current->matrix);
    TransMatrix(&menu_state_current->matrix, &menu_state_current->offset);
    SetRotMatrix(&menu_state_current->matrix);
    SetTransMatrix(&menu_state_current->matrix);
}

/* 801D1E80: Start the view moving in (3) with its sound. */
void menu_view_start_zoom_in(void) {
    menu_state_current->view_motion = 3;
    menu_play_sound(0x5b);
}

/* 801D1EB0: Start the view moving out (4) with its sound. */
void menu_view_start_zoom_out(void) {
    menu_state_current->view_motion = 4;
    menu_play_sound(0x5c);
}

/* 801D1EE0: Place the highlight sprite at position `index`; with `outline` also its
 * background box and outline around the text, as wide as the block's +15b. */
void menu_highlight_place(s32 index, u8 outline) {
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, menu_state_current->prims, menu_state_current->buffer_index, menu_highlight_x_table[index], menu_highlight_y_table[index], 0x1000);
    menu_state_current->prims->sprite_buffer = menu_state_current->buffer_index;
    if (outline) {
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x0 = menu_highlight_x_table[index] + 0x14;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y0 = menu_highlight_y_table[index] - 0x24;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x1 = menu_highlight_x_table[index] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y1 = menu_highlight_y_table[index] - 0x24;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x2 = menu_highlight_x_table[index] + 0x14;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y2 = menu_highlight_y_table[index] - 0x14;
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->x3 = menu_highlight_x_table[index] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->shade + menu_state_current->buffer_index)->y3 = menu_highlight_y_table[index] - 0x14;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->x0 = menu_highlight_x_table[index] + 0x14;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->y0 = menu_highlight_y_table[index] - 0x24;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->x1 = menu_highlight_x_table[index] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->y1 = menu_highlight_y_table[index] - 0x24;
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->x2 = menu_highlight_x_table[index] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->upper + menu_state_current->buffer_index)->y2 = menu_highlight_y_table[index] - 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->x0 = menu_highlight_x_table[index] + 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->y0 = menu_highlight_y_table[index] - 0x24;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->x1 = menu_highlight_x_table[index] + 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->y1 = menu_highlight_y_table[index] - 0x14;
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->x2 = menu_highlight_x_table[index] + (menu_state_current->prims->width + 0x14);
        (menu_state_current->prims->lower + menu_state_current->buffer_index)->y2 = menu_highlight_y_table[index] - 0x14;
        menu_state_current->prims->shade_buffer = menu_state_current->buffer_index;
        menu_state_current->flags->cursor_shown = 1;
    }
}

/* 801D22C4: Hide the party panels (redraw flags 3 and 4). */
void menu_highlight_hide(void) {
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
}

/* 801D22F4: Lay out the screen markers for `mode`: 0 all four and the two extra flags,
 * 2 all four, 3 the first at the origin, 1 none. */
void menu_markers_layout(u8 mode) {
    s32 i;

    menu_state_current->flags->markers_shown = 0;
    switch (mode) {
    case 0:
        menu_state_current->flags->markers_shown = 1;
        menu_state_current->markers->at_cursor[0] = 1;
        menu_state_current->markers->at_cursor[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, &menu_state_current->markers->polys[i * 2], menu_state_current->buffer_index,
                          menu_marker_x_table[i], menu_marker_y_table[i], 0x800);
            menu_state_current->markers->buffer[i] = menu_state_current->buffer_index;
        }
        break;
    case 3:
        sprite_sheet_draw_scaled(menu_state_current->sheet, 0x108, menu_state_current->markers->polys, menu_state_current->buffer_index, 0, 0, 0x800);
        menu_state_current->markers->buffer[0] = menu_state_current->buffer_index;
    case 1:
        break;
    }
}

/* 801D2484: Clear the party block flag at +2f. */
void menu_markers_hide(void) {
    menu_state_current->flags->markers_shown = 0;
}

/* 801D249C: Show (`show`) the six party labels, placing labels 3-5 as quads, or hide
 * the party name labels. */
void menu_party_labels_show(u8 show) {
    s32 i;

    if (show) {
        menu_label_render_pairs(menu_state_current->row_labels, menu_party_label_ids, 4, 6);
        for (i = 0; i < 3; i++) {
            menu_set_rect_verts(menu_state_current->row_labels[3 + i].verts, menu_party_label_x_table[i], menu_party_label_y_table[i],
                          menu_state_current->row_labels[3 + i].width, 0xd);
            menu_state_current->row_labels[3 + i].buffer = menu_state_current->buffer_index;
            menu_state_current->row_labels[3 + i].projected = 1;
            menu_state_current->flags->row_labels_shown[3 + i] = 1;
        }
    } else {
        for (i = 0; i < 3; i++) {
            menu_state_current->flags->row_labels_shown[i] = 0;
        }
    }
}

/* 801D25E4: Clear the party block's six bytes at +14. */
void menu_row_labels_hide(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        menu_state_current->flags->row_labels_shown[i] = 0;
    }
}

/* 801D261C: Hide the party names and place the name label of the current choice as a
 * quad beside its highlight. */
void menu_choice_label_place(void) {
    menu_party_labels_show(0);
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->x0 =
        menu_choice_highlight_x_table[menu_state_current->choice] + 0x16 + menu_choice_label_x_offsets[menu_state_current->choice];
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->y0 =
        menu_choice_highlight_y_table[menu_state_current->choice] - 0x22;
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->x1 =
        menu_choice_highlight_x_table[menu_state_current->choice] + 0x16 + menu_choice_label_x_offsets[menu_state_current->choice] +
        menu_state_current->row_labels[menu_state_current->choice].width;
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->y1 =
        menu_choice_highlight_y_table[menu_state_current->choice] - 0x22;
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->x2 =
        menu_choice_highlight_x_table[menu_state_current->choice] + 0x16 + menu_choice_label_x_offsets[menu_state_current->choice];
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->y2 =
        menu_choice_highlight_y_table[menu_state_current->choice] - 0x15;
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->x3 =
        menu_choice_highlight_x_table[menu_state_current->choice] + 0x16 + menu_choice_label_x_offsets[menu_state_current->choice] +
        menu_state_current->row_labels[menu_state_current->choice].width;
    ((menu_state_current->row_labels + menu_state_current->choice)->polys + menu_state_current->buffer_index)->y3 =
        menu_choice_highlight_y_table[menu_state_current->choice] - 0x15;
    menu_state_current->row_labels[menu_state_current->choice].buffer = menu_state_current->buffer_index;
    menu_state_current->flags->row_labels_shown[menu_state_current->choice] = 1;
}

/* 801D28A8: Draw the small window at (d4, b2) and its element at (d8, b6). */
void menu_money_window_open(void) {
    menu_panel_open(0, 0xd4, 0xb2, 0x60, 0x10, 0, 0, 4, 0);
    menu_money_window_layout_digits(0xd8, 0xb6);
}

/* 801D28FC: Open the small window at (cc, c6) with its element and show it. */
void menu_play_time_window_open(void) {
    menu_panel_open(1, 0xcc, 0xc6, 0x50, 0x10, 0, 0, 4, 0);
    menu_play_time_window_layout_digits(0xd0, 0xca);
    menu_state_current->flags->field_menu2_shown = 1;
}

/* 801D2968: Draw the element at (d0, ca) while party flag 6 is set. */
void menu_play_time_window_update(void) {
    if (menu_state_current->flags->field_menu2_shown != 0) {
        menu_play_time_window_layout_digits(0xd0, 0xca);
    }
}

/* 801D29A8: Slide the three party panels in (`open`) or out along their movers,
 * drawing each frame until one arrives; opening then pins them in place,
 * plays the open sound and (unless `keep`) redraws the status. */
void menu_field_blocks_slide(u8 open, u8 keep) {
    MenuFlags *party;
    s32 i;

    if (open) {
        menu_label_render_table(8, menu_state_current->list_labels, menu_field_command_label_ids, menu_state_current->flags->list_labels_shown);
        menu_mover_start(0x100, 0x86, 0x60, 6, 8, 0);
        menu_mover_start(0x108, 0x3e, 0x68, 0x3e, 8, 1);
        menu_mover_start(0x110, -10, 0x70, 0x76, 8, 2);
    } else {
        menu_label_clear_shown(8, menu_state_current->flags->list_labels_shown);
        menu_mover_start(0x60, 6, 0x100, 0x86, 8, 0);
        menu_mover_start(0x68, 0x3e, 0x108, 0x3e, 8, 1);
        menu_mover_start(0x70, 0x76, 0x110, -10, 8, 2);
    }
    while (menu_state_current->movers[0].done == 0 && menu_state_current->movers[1].done == 0 &&
           menu_state_current->movers[2].done == 0) {
        for (i = 0; i < 3; i++) {
            if (menu_state_current->flags->party[i] != 0xff) {
                menu_field_block_layout(i, menu_state_current->flags->party[i]);
            }
        }
        menu_run_frame();
        for (i = 0; i < 3; i++) {
            if (menu_state_current->flags->party[i] != 0xff) {
                menu_mover_step(i);
            }
        }
    }
    if (open) {
        menu_state_current->movers[0].x0 = 0x60;
        menu_state_current->movers[0].y0 = 6;
        menu_state_current->movers[1].x0 = 0x68;
        menu_state_current->movers[1].y0 = 0x3e;
        menu_state_current->movers[2].x0 = 0x70;
        menu_state_current->movers[2].y0 = 0x76;
        menu_state_current->movers[0].acc_y = 0;
        menu_state_current->movers[0].acc_x = 0;
        menu_state_current->movers[1].acc_y = 0;
        menu_state_current->movers[1].acc_x = 0;
        menu_state_current->movers[2].acc_y = 0;
        menu_state_current->movers[2].acc_x = 0;
        for (i = 0; i < 3; i++) {
            if (menu_state_current->flags->party[i] != 0xff) {
                menu_field_block_layout(i, menu_state_current->flags->party[i]);
            }
        }
        menu_play_sound(0x5d);
        if (!keep) {
            menu_money_window_open();
        }
        menu_state_current->flags->panels_shown[1] = 1;
        menu_state_current->flags->field_menu2_shown = 1;
    } else {
        party = menu_state_current->flags;
        party->field_blocks_shown[2] = 0;
        party->field_blocks_shown[1] = 0;
        party->field_blocks_shown[0] = 0;
        if (!keep) {
            menu_state_current->flags->panels_shown[0] = 0;
            menu_state_current->flags->field_menu_shown = 0;
        }
    }
    menu_run_frame();
}

/* 801D2D38: Open the field menu: the first two panels, then each party member's and
 * gear's name image, the command cursor, panels and money window. */
void menu_field_menu_open(void) {
    s32 i;
    s32 row;
    void *block;
    s32 gear;

    if (menu_state_screen == 0) {
        for (row = 0; row < 2; row++) {
            block = heap_alloc(0x720, 0);
            menu_state_current->panels[row] = block;
            bzero(block, 0x720);
            block = heap_alloc(0x18, 0);
            menu_state_current->growth[row] = block;
            bzero(block, 0x18);
            menu_panel_init(row);
        }
        menu_play_sound(0x5e);
    }
    for (i = 0; i < 3; i++) {
        if (menu_state_current->flags->party[i] != 0xff) {
            menu_name_image_render(menu_state_current->flags->party[i], i * 2);
            gear = game_data.characters[menu_state_current->flags->party[i]].gearId;
            if (gear != 0xff) {
                menu_name_image_render(gear + 11, (i + 3) * 2);
            } else {
                menu_name_image_render(0xff, (i + 3) * 2);
            }
        }
    }
    menu_command_window_open(8, menu_field_command_images);
    menu_field_blocks_slide(1, 0);
    menu_play_time_window_open();
}

/* 801D2EC0: Refresh the item/status panels of `slot` for `mode` over two frames. */
void menu_member_page_build(u8 slot, u8 mode) {
    menu_detail_build(slot, mode);
    menu_detail_layout_tabs(slot, mode, game_data.inGear[slot]);
    menu_run_frame();
    menu_equip_panel_build(slot, 0, 0, mode);
    menu_run_frame();
    menu_equip_labels_layout_parts(slot, 0, 0, mode);
}

/* 801D2F4C: Open the notice window (panel 2) and show three lines of label text
 * from entry `message`: four line labels (pairs share one render buffer)
 * rendered into VRAM and laid out as quads. */
void menu_notice_open(u8 message) {
    s32 x;
    MenuGrowth *mark;
    MenuLabel *line;
    s32 i;

    menu_panel_open(2, 0x7a, 0x96, 0xbc, 0x40, 1, 1, 4, 0);
    mark = menu_state_current->growth[2];
    while (mark->done == 0) {
        menu_run_frame();
    }
    x = 0x84;
    for (i = 0; i < 4; i++) {
        void *block = heap_alloc(0x80, 0);

        menu_state_current->message_labels[i] = block;
        bzero(block, 0x80);
        if (!(i & 1)) {
            menu_state_current->message_labels[i]->pixels = heap_alloc(0x5ca, 0);
            menu_state_current->message_labels[i]->rect.x = 0x140;
            menu_state_current->message_labels[i]->rect.y = (i / 2) * 13 + 0x4e;
            menu_state_current->message_labels[i]->rect.w = 0x3a;
            menu_state_current->message_labels[i]->rect.h = 13;
        } else {
            menu_state_current->message_labels[i]->pixels = menu_state_current->message_labels[i - 1]->pixels;
        }
    }
    for (i = 0; i < 3; i++) {
        line = menu_state_current->message_labels[i];
        line->width = window_render_text_line(text_get_resource_entry(menu_state_current->label_text, message + i), line->pixels, 0x36, i % 2);
        menu_label_init_quads(line, i, 0, 0);
        menu_quad_place(&line->polys[menu_state_current->buffer_index], (u16)x, (u16)(i * 16 + 0xa0), 0,
                      (u8)((i / 2) * 13 + 0x4e), line->width, 13);
        menu_set_rect_verts(line->verts, x, i * 16 + 0xa0, line->width, 13);
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
    menu_run_frame();
    menu_run_frame();
}

/* 801D32B4: Close the notice when open (party +22): free panel 2 and its four
 * blocks (+1de0), then finish a frame. */
void menu_notice_close(void) {
    s32 i;

    if (menu_state_current->flags->panels_shown[2] != 0) {
        menu_panel_close(2);
        menu_state_current->flags->messages_shown = 0;
        for (i = 0; i < 4; i++) {
            heap_free(menu_state_current->message_labels[i]);
        }
    }
    menu_run_frame();
}

/* 801D3344: Show the one-quad sprite (+43c; sheet image 107) at (x, y), `h` high. */
void menu_scroll_bar_show(s32 x, s32 y, s32 h) {
    void *block;

    if (menu_state_current->flags->scroll_shown == 0) {
        block = heap_alloc(0x74, 0);
        menu_state_current->scroll = block;
        bzero(block, 0x74);
    }
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x107, menu_state_current->scroll, menu_state_current->buffer_index, x, y, 0x1000);
    menu_set_rect_verts(menu_state_current->scroll->verts, x, y, 8, h);
    menu_state_current->scroll->buffer = menu_state_current->buffer_index;
    menu_state_current->flags->scroll_shown = 1;
}

/* 801D3444: Clear party flag +49 and free the block at state +43c. */
void menu_scroll_bar_hide(void) {
    menu_state_current->flags->scroll_shown = 0;
    heap_free(menu_state_current->scroll);
}

/* 801D3488: Show the two party window sprites (sheet 164, 165) at `row` and lay out
 * their quads, unless only one member is listed (+32b, or +33b when
 * `fighters`). The block at +440 is allocated on first use. */
void menu_member_marks_show(u8 row, u8 fighters) {
    s32 count;
    s32 i;
    void *block;

    if (!fighters) {
        count = menu_state_current->party_count;
    } else {
        count = menu_state_current->fighters;
    }
    if (count != 1) {
        if (menu_state_current->flags->unknown67 == 0) {
            block = heap_alloc(0x1c4, 0);
            menu_state_current->marks = block;
            bzero(block, 0x1c4);
            menu_state_current->flags->unknown67 = 1;
        }
        for (i = 0; i < 2; i++) {
            sprite_sheet_draw_scaled(menu_state_current->sheet, 0x164 + i, &menu_state_current->marks->polys[i * 4], menu_state_current->buffer_index,
                          menu_member_mark_x_table[i], menu_member_mark_y_table[row], 0x1000);
        }
        for (i = 0; i < 4; i++) {
            menu_set_rect_verts(&menu_state_current->marks->verts[i * 4],
                          menu_state_current->marks->polys[i * 2 + menu_state_current->buffer_index].x0,
                          menu_state_current->marks->polys[i * 2 + menu_state_current->buffer_index].y0,
                          menu_state_current->marks->polys[i * 2 + menu_state_current->buffer_index].x1 -
                              menu_state_current->marks->polys[i * 2 + menu_state_current->buffer_index].x0,
                          menu_state_current->marks->polys[i * 2 + menu_state_current->buffer_index].y3 -
                              menu_state_current->marks->polys[i * 2 + menu_state_current->buffer_index].y0);
        }
        menu_state_current->marks->buffer = menu_state_current->buffer_index;
        menu_state_current->flags->marks_shown = 1;
    }
}

/* 801D3674: Close the block at +440 when it is open (party +67), hiding its sprites. */
void menu_member_marks_hide(void) {
    MenuFlags *party;

    party = menu_state_current->flags;
    if (party->unknown67 != 0) {
        party->marks_shown = 0;
        menu_state_current->flags->unknown67 = 0;
        heap_free(menu_state_current->marks);
    }
}

/* 801D36E0: Lay out `label` as the name image of party slot `slot` (its character's,
 * or its gear's when `gear`) at the position of `mode`. */
void menu_name_label_layout(MenuLabel *label, u8 slot, u8 gear, u8 mode) {
    s32 x;
    s32 w;

    menu_quad_init(&label->polys[menu_state_current->buffer_index]);
    label->polys[menu_state_current->buffer_index].tpage = GetTPage(0, 0, 0x180, 0);
    if (!gear) {
        w = 0x48;
        label->polys[menu_state_current->buffer_index].clut = (menu_state_current->flags->party[slot] & 1) ? text_plane1_clut : text_plane0_clut;
        x = menu_name_label_x_table[mode] - 0x24;
        menu_quad_place(&label->polys[menu_state_current->buffer_index], (u16)x, (u16)menu_name_label_y_table[mode],
                      (u8)(menu_name_image_vram_x_table[slot] * 4), (u8)menu_name_image_vram_y_table[slot], w, 13);
    } else {
        w = 0x60;
        label->polys[menu_state_current->buffer_index].clut =
            ((game_data.characters[menu_state_current->flags->party[slot]].gearId + 11) & 1) ? text_plane1_clut : text_plane0_clut;
        x = menu_name_label_x_table[mode] - 0x30;
        menu_quad_place(&label->polys[menu_state_current->buffer_index], (u16)x, (u16)menu_name_label_y_table[mode],
                      (u8)(menu_name_image_vram_x_table[slot + 3] * 4), (u8)menu_name_image_vram_y_table[slot + 3], w, 13);
    }
    menu_set_rect_verts(label->verts, x, menu_name_label_y_table[mode], w, 13);
    label->buffer = menu_state_current->buffer_index;
}

/* 801D397C: Open panel `index` at (x, y) of w x h: from its centre (`grow`; the
 * opening keeps no scroll bar) or laid out at once. Panels from 2 get their
 * own blocks (801d2d38 allocates the first two). */
void menu_panel_open(u8 index, u16 x, u16 y, u16 w, u16 h, u8 grow, u8 flat, s32 ot_entry, u8 has_bar) {
    MenuGrowth *mark;
    void *block;

    if (index >= 2) {
        block = heap_alloc(0x720, 0);
        menu_state_current->panels[index] = block;
        bzero(block, 0x720);
        block = heap_alloc(0x18, 0);
        menu_state_current->growth[index] = block;
        bzero(block, 0x18);
        menu_panel_init(index);
    }
    mark = menu_state_current->growth[index];
    if (grow) {
        mark->index = index;
        mark->done = 0;
        mark->x = x;
        mark->y = y;
        mark->w = w;
        mark->h = h;
        mark->cur_w = 0;
        mark->cur_h = 0;
        menu_state_current->flags->panels_growing[index] = 1;
        mark->flat = flat;
        mark->ot_entry = ot_entry;
        return;
    }
    menu_panel_layout(index, x, y, w, h, flat, ot_entry, has_bar);
}

/* 801D3B00: Grow each opening panel by 32 per frame towards its full size (centred),
 * marking it done when both sides are full, and lay it out. */
void menu_panel_grow_opening(void) {
    s32 i;
    MenuGrowth *mark;
    u8 full;

    for (i = 0; i < 7; i++) {
        mark = menu_state_current->growth[i];
        if (menu_state_current->flags->panels_growing[i] != 0 && mark->done == 0) {
            full = 0;
            if (mark->cur_w + 0x20 >= mark->w) {
                mark->cur_w = mark->w;
                full = 1;
            } else {
                mark->cur_w = mark->cur_w + 0x20;
            }
            if (mark->cur_h + 0x20 >= mark->h) {
                mark->cur_h = mark->h;
                full++;
            } else {
                mark->cur_h = mark->cur_h + 0x20;
            }
            if (full == 2) {
                mark->done = 1;
            }
            menu_panel_layout(mark->index, mark->x + (mark->w >> 1) - (mark->cur_w >> 1),
                          mark->y + (mark->h >> 1) - (mark->cur_h >> 1), mark->cur_w, mark->cur_h, mark->flat,
                          mark->ot_entry, mark->has_bar);
        }
    }
}

/* 801D3C4C: Lay out panel `slot`'s scroll bar at (x, y), `h` high: the top end, the
 * flipped bottom end and the side piece. */
void menu_panel_layout_scroll_bar(u8 slot, u16 x, u16 y, s32 unused, u16 h) {
    MenuPanel *panel;

    panel = menu_state_current->panels[slot];
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x105, panel->bar_ends, menu_state_current->buffer_index, x, y, 0x1000);
    sprite_sheet_draw_scaled_flip(menu_state_current->sheet, 0x105, &panel->bar_ends[2], menu_state_current->buffer_index, x, y + h - 8, 0x1000,
                  0, 1);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x106, panel->bar_side, menu_state_current->buffer_index, x, y + 8, 0x1000);
    menu_set_rect_verts(&panel->ends_at[0], x, y, 8, 8);
    menu_set_rect_verts(&panel->ends_at[4], x, y + h, 8, -8);
    menu_set_rect_verts(panel->side_at, x, y + 8, 8, h - 8);
}

/* 801D3DB0: Build panel `index`'s four corner sprites (sheet fd, ff, 102,
 * 104) for this buffer and place them around the w x h rectangle at (x, y),
 * mirrored by negative extents; make them semi-transparent. */
void menu_panel_layout_corners(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPanel *panel = menu_state_current->panels[index];
    s32 i;

    panel->corner_parts = 0;
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0xfd, panel->corner, menu_state_current->buffer_index, 0, 0,
                                           0x1000);
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0xff, &panel->corner[panel->corner_parts * 2],
                                           menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0x102, &panel->corner[panel->corner_parts * 2],
                                           menu_state_current->buffer_index, 0, 0, 0x1000);
    panel->corner_parts += sprite_sheet_draw_scaled(menu_state_current->sheet, 0x104, &panel->corner[panel->corner_parts * 2],
                                           menu_state_current->buffer_index, 0, 0, 0x1000);
    menu_set_rect_verts(&panel->corner_at[0], x - 8, y + 8, 16, -16);
    menu_set_rect_verts(&panel->corner_at[4], x + w + 8, y + 8, -16, -16);
    menu_set_rect_verts(&panel->corner_at[8], x - 8, y + h - 8, 16, 16);
    menu_set_rect_verts(&panel->corner_at[12], x + w + 8, y + h - 8, -16, 16);
    for (i = 0; i < 4; i++) {
        menu_quad_set_semi_transparent(&panel->corner[i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801D3FF8: Map panel `index`'s top edge pieces for this buffer and place them in two
 * halves along the top of (x, y, w). */
void menu_panel_layout_top_edge(u8 index, u16 x, u16 y, u16 w) {
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
    menu_set_rect_verts(panel->edge_at[0][0], x + 8, y - 8, half, 16);
    menu_set_rect_verts(panel->edge_at[0][1], x + (half + 8), y - 8, half, 16);
    for (i = 0; i < 2; i++) {
        menu_quad_set_semi_transparent(&panel->edge[0][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801D433C: Map panel `index`'s bottom edge pieces for this buffer and place them in
 * two halves along the bottom of (x, y, w, h). */
void menu_panel_layout_bottom_edge(u8 index, u16 x, u16 y, u16 w, u16 h) {
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
    menu_set_rect_verts(panel->edge_at[1][0], x + 8, y + h - 8, half, 16);
    menu_set_rect_verts(panel->edge_at[1][1], x + (half + 8), y + h - 8, half, 16);
    for (i = 0; i < 2; i++) {
        menu_quad_set_semi_transparent(&panel->edge[1][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801D4688: Map panel `index`'s left edge pieces for this buffer and place them in two
 * halves down the left of (x, y, h). */
void menu_panel_layout_left_edge(u8 index, u16 x, u16 y, u16 h) {
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
    menu_set_rect_verts(panel->edge_at[2][0], x - 8, y + 8, 16, half);
    menu_set_rect_verts(panel->edge_at[2][1], x - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        menu_quad_set_semi_transparent(&panel->edge[2][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801D49D0: Map panel `index`'s right edge pieces for this buffer and place them in two
 * halves down the right of (x, y, w, h). */
void menu_panel_layout_right_edge(u8 index, u16 x, u16 y, u16 w, u16 h) {
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
    menu_set_rect_verts(panel->edge_at[3][0], x + w - 8, y + 8, 16, half);
    menu_set_rect_verts(panel->edge_at[3][1], x + w - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        menu_quad_set_semi_transparent(&panel->edge[3][i * 2 + menu_state_current->buffer_index]);
    }
}

/* 801D4D1C: Lay out panel `index` at (x, y) of w x h and show it. */
void menu_panel_layout(u8 index, u16 x, u16 y, u16 w, u16 h, u8 flat, s32 ot_entry, u8 has_bar) {
    MenuPanel *panel;

    panel = menu_state_current->panels[index];
    menu_state_current->flags->panels_shown[index] = 0;
    menu_set_rect_verts(panel->fill_at, x, y, w, h);
    menu_panel_layout_corners(index, x, y, w, h);
    menu_panel_layout_top_edge(index, x, y, w);
    menu_panel_layout_bottom_edge(index, x, y, w, h);
    menu_panel_layout_left_edge(index, x, y, h);
    menu_panel_layout_right_edge(index, x, y, w, h);
    if (has_bar) {
        menu_panel_layout_scroll_bar(index, x, y, w, h);
    }
    panel->has_bar = has_bar;
    panel->flat = flat;
    panel->ot_entry = ot_entry;
    panel->buffer = menu_state_current->buffer_index;
    menu_state_current->flags->panels_shown[index] = 1;
}

/* 801D4EA0: Hide and free panel `slot` and its opening record. */
void menu_panel_close(u8 slot) {
    menu_state_current->flags->panels_shown[slot] = 0;
    menu_state_current->flags->panels_growing[slot] = 0;
    heap_free(menu_state_current->panels[slot]);
    heap_free(menu_state_current->growth[slot]);
}

/* 801D4F2C: Build field block `index`'s portrait (sheet 14b + index) at (x, y) and its
 * character's name image quad. */
void menu_field_block_layout_portrait(u8 index, u8 mode, s32 x, s32 y) {
    MenuFieldBlock *block = menu_state_current->field_blocks[index];

    sprite_sheet_draw_scaled(menu_state_current->sheet, index + 0x14b, block->portrait, menu_state_current->buffer_index, x, y, 0x1000);
    menu_quad_init(&block->name[menu_state_current->buffer_index]);
    block->name[menu_state_current->buffer_index].tpage = GetTPage(0, 0, 0x180, 0);
    block->name[menu_state_current->buffer_index].clut = (menu_state_current->flags->party[index] & 1) ? text_plane1_clut : text_plane0_clut;
    menu_quad_place(&block->name[menu_state_current->buffer_index], (u16)(menu_field_block_name_x + x), (u16)(menu_field_block_name_y + y),
                  (u8)(menu_name_image_vram_x_table[index] * 4), (u8)menu_name_image_vram_y_table[index], 0x48, 13);
}

/* 801D50EC: Lay out the parts of field block `index` at (x, y) from the
 * menu_field_block_part_images sheet images (ffff none). */
void menu_field_block_layout_parts(u8 index, s32 x, s32 y) {
    MenuFieldBlock *block;
    s32 i;

    block = menu_state_current->field_blocks[index];
    block->count0 = 0;
    for (i = 0; i < 20; i++) {
        if (menu_field_block_part_images[i] != 0xffff) {
            block->count0 += sprite_sheet_draw_scaled(menu_state_current->sheet, menu_field_block_part_images[i], &block->list0[block->count0 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_part_x_table[i], y + menu_field_block_part_y_table[i],
                                           0x1000);
        }
    }
}

/* 801D51EC: Lay out character `ch`'s hp (by digit position) and hp maximum (packed)
 * on field block `index` at (x, y). */
void menu_field_block_layout_hp(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = menu_state_current->field_blocks[index];
    s32 i;
    s32 n;
    u8 digit;

    menu_split_digits(game_data.characters[ch].hp);
    block->count1 = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[6 + i];
        if (digit != 0xff) {
            block->count1 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list1[block->count1 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_hp_x + i * 8, y + menu_field_block_hp_y, 0x1000);
        }
    }
    menu_split_digits(game_data.characters[ch].maxHp);
    block->count2 = 0;
    for (i = 0, n = 0; i < 3; i++) {
        digit = menu_state_current->digits[6 + i];
        if (digit != 0xff) {
            block->count2 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list2[block->count2 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_max_hp_x + n * 8, y + menu_field_block_max_hp_y, 0x1000);
            n++;
        }
    }
}

/* 801D53D0: Lay out character `ch`'s ep (by digit position) and ep maximum (packed)
 * on field block `index` at (x, y). */
void menu_field_block_layout_ep(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = menu_state_current->field_blocks[index];
    s32 i;
    s32 n;
    u8 digit;

    menu_split_digits(game_data.characters[ch].ep);
    block->count3 = 0;
    for (i = 0; i < 2; i++) {
        digit = menu_state_current->digits[7 + i];
        if (digit != 0xff) {
            block->count3 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list3[block->count3 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_ep_x + i * 8, y + menu_field_block_ep_y, 0x1000);
        }
    }
    menu_split_digits(game_data.characters[ch].maxEp);
    block->count4 = 0;
    for (i = 0, n = 0; i < 2; i++) {
        digit = menu_state_current->digits[7 + i];
        if (digit != 0xff) {
            block->count4 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list4[block->count4 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_max_ep_x + n * 8, y + menu_field_block_max_ep_y, 0x1000);
            n++;
        }
    }
}

/* 801D55B4: Lay out character `ch`'s experience and experience to the next level
 * (seven digits each, by digit position) on field block `index` at (x, y). */
void menu_field_block_layout_exp_next(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = menu_state_current->field_blocks[index];
    s32 i;
    u8 digit;

    menu_split_digits(game_data.characters[ch].expNextA);
    block->count5 = 0;
    for (i = 0; i < 7; i++) {
        digit = menu_state_current->digits[2 + i];
        if (digit != 0xff) {
            block->count5 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list5[block->count5 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_exp_next_a_x + i * 8, y + menu_field_block_exp_next_a_y, 0x1000);
        }
    }
    menu_split_digits(game_data.characters[ch].expNextB);
    block->count7 = 0;
    for (i = 0; i < 7; i++) {
        digit = menu_state_current->digits[2 + i];
        if (digit != 0xff) {
            block->count7 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list7[block->count7 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_exp_next_b_x + i * 8, y + menu_field_block_exp_next_b_y, 0x1000);
        }
    }
}

/* 801D5794: Lay out character `ch`'s +62 and +63 values (three digits each, by digit
 * position) on field block `index` at (x, y), the second tinted green. */
void menu_field_block_layout_levels(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = menu_state_current->field_blocks[index];
    s32 i;
    u8 digit;

    menu_split_digits(game_data.characters[ch].level);
    block->count6 = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[6 + i];
        if (digit != 0xff) {
            block->count6 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list6[block->count6 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_level_x + i * 8, y + menu_field_block_level_y, 0x1000);
        }
    }
    menu_split_digits(game_data.characters[ch].level2);
    block->count8 = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[6 + i];
        if (digit != 0xff) {
            block->count8 += sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &block->list8[block->count8 * 2],
                                           menu_state_current->buffer_index, x + menu_field_block_level2_x + i * 8, y + menu_field_block_level2_y, 0x1000);
        }
    }
    for (i = 0; i < block->count8; i++) {
        SetShadeTex(&block->list8[i * 2 + menu_state_current->buffer_index], 0);
        (block->list8 + (i * 2 + menu_state_current->buffer_index))->r0 = 0;
        (block->list8 + (i * 2 + menu_state_current->buffer_index))->g0 = 0x80;
        (block->list8 + (i * 2 + menu_state_current->buffer_index))->b0 = 0;
    }
}

/* 801D5A50: Lay out field block `index` (none for ff) at its mover's position in
 * `mode` and show it. */
void menu_field_block_layout(u8 index, u8 mode) {
    MenuFieldBlock *block;
    s32 x;
    s32 y;

    if (index != 0xff) {
        block = menu_state_current->field_blocks[index];
        x = menu_state_current->movers[index].acc_x / 256 + menu_state_current->movers[index].x0;
        y = menu_state_current->movers[index].acc_y / 256 + menu_state_current->movers[index].y0;
        menu_field_block_layout_portrait(index, mode, x, y);
        menu_field_block_layout_parts(index, x, y);
        menu_field_block_layout_hp(index, mode, x, y);
        menu_field_block_layout_ep(index, mode, x, y);
        menu_field_block_layout_exp_next(index, mode, x, y);
        menu_field_block_layout_levels(index, mode, x, y);
        menu_state_current->flags->field_blocks_shown[index] = 1;
        block->buffer = menu_state_current->buffer_index;
    }
}

/* 801D5BA4: Lay out the money (game_data.gold) digits at (x, y) and its unit mark. */
void menu_money_window_layout_digits(s32 x, s32 y) {
    s32 i;
    s32 dx;
    u8 digit;

    menu_split_digits(game_data.gold);
    i = 0;
    dx = x;
    menu_state_current->field_menu->count = 0;
    for (; i < 9; i++) {
        digit = menu_state_current->digits[i];
        if (digit != 0xff) {
            menu_state_current->field_menu->count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &menu_state_current->field_menu->polys[menu_state_current->field_menu->count * 2],
                              menu_state_current->buffer_index, dx, y, 0x1000);
        }
        dx += 8;
    }
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0x10, menu_state_current->field_menu->cursor, menu_state_current->buffer_index, x + 0x50, y,
                  0x1000);
    menu_state_current->flags->field_menu_shown = 1;
    menu_state_current->field_menu->start = menu_state_current->buffer_index;
}

/* 801D5CF8: Lay out the play time digits (hours, minutes, seconds) at (x, y) with
 * their two separators. */
void menu_play_time_window_layout_digits(s32 x, s32 y) {
    s32 i;

    for (i = 0; i < 3; i++) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->time[i], &menu_state_current->field_menu2->polys[i * 2],
                      menu_state_current->buffer_index, x + i * 8, y, 0x1000);
    }
    for (i = 3; i < 5; i++) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->time[i], &menu_state_current->field_menu2->polys[i * 2],
                      menu_state_current->buffer_index, x + i * 8 + 8, y, 0x1000);
    }
    for (i = 5; i < 7; i++) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->time[i], &menu_state_current->field_menu2->polys[i * 2],
                      menu_state_current->buffer_index, x + i * 8 + 0x10, y, 0x1000);
    }
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xee, &menu_state_current->field_menu2->polys2[0], menu_state_current->buffer_index, x + 0x18, y,
                  0x1000);
    sprite_sheet_draw_scaled(menu_state_current->sheet, 0xee, &menu_state_current->field_menu2->polys2[4], menu_state_current->buffer_index, x + 0x30, y,
                  0x1000);
    menu_state_current->field_menu2->start = menu_state_current->buffer_index;
}

/* 801D5ED4: Lay out the detail panel's portrait (sheet 14b + slot) and the name image
 * of party slot `slot`: its character's, or its gear's (further left) when
 * `gear`. */
void menu_detail_layout_portrait(u8 slot, u8 gear) {
    s32 shift;
    s32 w;

    shift = gear ? 0x18 : 0;
    sprite_sheet_draw_scaled(menu_state_current->sheet, slot + 0x14b, menu_state_current->detail->portrait, menu_state_current->buffer_index,
                  0x18 - shift, 0xe, 0x1000);
    menu_set_rect_verts(menu_state_current->detail->portraitAt, menu_state_current->detail->portrait[menu_state_current->buffer_index].x0,
                  menu_state_current->detail->portrait[menu_state_current->buffer_index].y0, 0x30, 0x30);
    menu_quad_init(&menu_state_current->detail->name[menu_state_current->buffer_index]);
    menu_state_current->detail->name[menu_state_current->buffer_index].tpage = GetTPage(0, 0, 0x180, 0);
    if (!gear) {
        menu_state_current->detail->name[menu_state_current->buffer_index].clut =
            (menu_state_current->flags->party[slot] & 1) ? text_plane1_clut : text_plane0_clut;
    } else {
        menu_state_current->detail->name[menu_state_current->buffer_index].clut =
            ((game_data.characters[menu_state_current->flags->party[slot]].gearId + 11) & 1) ? text_plane1_clut : text_plane0_clut;
    }
    w = gear * 0x18 + 0x48;
    menu_quad_place(&menu_state_current->detail->name[menu_state_current->buffer_index], 0, 0,
                  (u8)(menu_name_image_vram_x_table[gear * 3 + slot] * 4), (u8)menu_name_image_vram_y_table[gear * 3 + slot], (u16)w, 13);
    menu_set_rect_verts(menu_state_current->detail->nameAt, menu_detail_name_x - shift, menu_detail_name_y, w, 13);
}

/* 801D6194: Build the detail panel's parts of `layout` from the sheet and place each
 * part's quad where the sheet put it. */
void menu_detail_layout_parts(u8 layout) {
    s32 i;

    menu_state_current->detail->count = 0;
    for (i = 0; i < 24; i++) {
        if (menu_detail_part_images[layout * 24 + i] != 0xffff) {
            menu_state_current->detail->count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, menu_detail_part_images[layout * 24 + i],
                              &menu_state_current->detail->parts[menu_state_current->detail->count * 2], menu_state_current->buffer_index,
                              menu_detail_part_x_table[layout * 24 + i], menu_detail_part_y_table[layout * 24 + i], 0x1000);
        }
    }
    for (i = 0; i < menu_state_current->detail->count; i++) {
        menu_set_rect_verts(menu_state_current->detail->partsAt[i],
                      menu_state_current->detail->parts[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->parts[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->parts[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->parts[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->parts[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->parts[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D6338: Lay out the detail panel's hp (by digit position) and hp maximum (packed)
 * of party slot `slot`: the character's (three digits) or, with `gear`, its
 * gear's (five digits). */
void menu_detail_layout_hp(u8 slot, u8 gear) {
    s32 digits;
    s32 first;
    s32 x;
    s32 i;
    u8 digit;

    if (!gear) {
        digits = 3;
        first = 6;
        x = menu_detail_hp_x;
        menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].hp);
    } else {
        digits = 5;
        first = 4;
        x = menu_detail_hp_x + 8;
        menu_split_digits(game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].hp);
    }
    menu_state_current->detail->hpCount = 0;
    for (i = 0; i < digits; i++) {
        digit = menu_state_current->digits[first + i];
        if (digit != 0xff) {
            menu_state_current->detail->hpCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &menu_state_current->detail->hp[menu_state_current->detail->hpCount * 2],
                              menu_state_current->buffer_index, x - gear * 0x18, menu_detail_hp_y, 0x1000);
        }
        x += 8;
    }
    for (i = 0; i < menu_state_current->detail->hpCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->hpAt[i], menu_state_current->detail->hp[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->hp[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->hp[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->hp[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->hp[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->hp[i * 2 + menu_state_current->buffer_index].y0);
    }
    if (!gear) {
        menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].maxHp);
        x = menu_detail_max_hp_x;
    } else {
        menu_split_digits(game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].maxHp);
        x = menu_detail_max_hp_x - 0x20;
    }
    menu_state_current->detail->hpMaxCount = 0;
    for (i = 0; i < digits; i++) {
        digit = menu_state_current->digits[first + i];
        if (digit != 0xff) {
            menu_state_current->detail->hpMaxCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->hpMax[menu_state_current->detail->hpMaxCount * 2],
                              menu_state_current->buffer_index, x - gear * 0x18, gear * 8 + menu_detail_max_hp_y, 0x1000);
            x += 8;
        }
    }
    for (i = 0; i < menu_state_current->detail->hpMaxCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->hpMaxAt[i],
                      menu_state_current->detail->hpMax[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->hpMax[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->hpMax[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->hpMax[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->hpMax[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->hpMax[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D680C: Lay out the detail panel's ep (by digit position) and ep maximum (packed)
 * of party slot `slot`: the character's (two digits) or, with `gear`, its
 * gear's +38 and +3a (four digits). */
void menu_detail_layout_ep_or_fuel(u8 slot, u8 gear) {
    s32 digits;
    s32 first;
    s32 x;
    s32 y;
    s32 i;
    u8 digit;

    if (!gear) {
        digits = 2;
        first = 7;
        x = menu_detail_ep_x;
        y = menu_detail_ep_y;
        menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].ep);
    } else {
        digits = 4;
        first = 5;
        x = menu_detail_ep_x;
        y = menu_detail_ep_y + 8;
        menu_split_digits(game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].fuel);
    }
    menu_state_current->detail->epCount = 0;
    for (i = 0; i < digits; i++) {
        digit = menu_state_current->digits[first + i];
        if (digit != 0xff) {
            menu_state_current->detail->epCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit, &menu_state_current->detail->ep[menu_state_current->detail->epCount * 2],
                              menu_state_current->buffer_index, x - gear * 0x18, y, 0x1000);
        }
        x += 8;
    }
    for (i = 0; i < menu_state_current->detail->epCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->epAt[i], menu_state_current->detail->ep[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->ep[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->ep[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->ep[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->ep[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->ep[i * 2 + menu_state_current->buffer_index].y0);
    }
    if (!gear) {
        menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].maxEp);
        x = menu_detail_max_ep_x;
        y = menu_detail_max_ep_y;
    } else {
        menu_split_digits(game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].maxFuel);
        x = menu_detail_max_ep_x - 0x20;
        y = menu_detail_max_ep_y + 0x10;
    }
    menu_state_current->detail->epMaxCount = 0;
    for (i = 0; i < digits; i++) {
        digit = menu_state_current->digits[first + i];
        if (digit != 0xff) {
            menu_state_current->detail->epMaxCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->epMax[menu_state_current->detail->epMaxCount * 2],
                              menu_state_current->buffer_index, x - gear * 0x18, y, 0x1000);
            x += 8;
        }
    }
    for (i = 0; i < menu_state_current->detail->epMaxCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->epMaxAt[i],
                      menu_state_current->detail->epMax[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->epMax[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->epMax[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->epMax[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->epMax[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->epMax[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D6CF4: Lay out the detail panel's level and +63 value (three digits each, by
 * digit position) of party slot `slot`, the second tinted green; `gear`
 * shifts them left. */
void menu_detail_layout_levels(u8 slot, u8 gear) {
    s32 i;
    u8 digit;

    menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].level);
    menu_state_current->detail->levelCount = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[6 + i];
        if (digit != 0xff) {
            menu_state_current->detail->levelCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->level[menu_state_current->detail->levelCount * 2],
                              menu_state_current->buffer_index, i * 8 + menu_detail_level_x - gear * 0x18, menu_detail_level_y, 0x1000);
        }
    }
    for (i = 0; i < menu_state_current->detail->levelCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->levelAt[i],
                      menu_state_current->detail->level[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->level[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->level[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->level[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->level[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->level[i * 2 + menu_state_current->buffer_index].y0);
    }
    menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].level2);
    menu_state_current->detail->level2Count = 0;
    for (i = 0; i < 3; i++) {
        digit = menu_state_current->digits[6 + i];
        if (digit != 0xff) {
            menu_state_current->detail->level2Count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->level2[menu_state_current->detail->level2Count * 2],
                              menu_state_current->buffer_index, i * 8 + menu_detail_level2_x - gear * 0x18, menu_detail_level2_y, 0x1000);
        }
    }
    for (i = 0; i < menu_state_current->detail->level2Count; i++) {
        SetShadeTex(&menu_state_current->detail->level2[i * 2 + menu_state_current->buffer_index], 0);
        (menu_state_current->detail->level2 + (i * 2 + menu_state_current->buffer_index))->r0 = 0;
        (menu_state_current->detail->level2 + (i * 2 + menu_state_current->buffer_index))->g0 = 0x80;
        (menu_state_current->detail->level2 + (i * 2 + menu_state_current->buffer_index))->b0 = 0;
        menu_set_rect_verts(menu_state_current->detail->level2At[i],
                      menu_state_current->detail->level2[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->level2[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->level2[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->level2[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->level2[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->level2[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D7154: Lay out the detail panel's +3c and +40 values (eight digits each, by
 * digit position) of party slot `slot`; `gear` shifts them left. */
void menu_detail_layout_exp_totals(u8 slot, u8 gear) {
    s32 i;
    u8 digit;

    menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].expTotalA);
    menu_state_current->detail->value3CCount = 0;
    for (i = 0; i < 8; i++) {
        digit = menu_state_current->digits[1 + i];
        if (digit != 0xff) {
            menu_state_current->detail->value3CCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->value3C[menu_state_current->detail->value3CCount * 2],
                              menu_state_current->buffer_index, i * 8 + menu_detail_exp_total_a_x - gear * 0x18, menu_detail_exp_total_a_y, 0x1000);
        }
    }
    for (i = 0; i < menu_state_current->detail->value3CCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->value3CAt[i],
                      menu_state_current->detail->value3C[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->value3C[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->value3C[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->value3C[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->value3C[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->value3C[i * 2 + menu_state_current->buffer_index].y0);
    }
    menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].expTotalB);
    menu_state_current->detail->value40Count = 0;
    for (i = 0; i < 8; i++) {
        digit = menu_state_current->digits[1 + i];
        if (digit != 0xff) {
            menu_state_current->detail->value40Count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->value40[menu_state_current->detail->value40Count * 2],
                              menu_state_current->buffer_index, i * 8 + menu_detail_exp_total_b_x - gear * 0x18, menu_detail_exp_total_b_y, 0x1000);
        }
    }
    for (i = 0; i < menu_state_current->detail->value40Count; i++) {
        menu_set_rect_verts(menu_state_current->detail->value40At[i],
                      menu_state_current->detail->value40[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->value40[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->value40[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->value40[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->value40[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->value40[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D74EC: Lay out the detail panel's experience and experience to the next level
 * (seven digits each, by digit position) of party slot `slot`; `gear` shifts
 * them left. */
void menu_detail_layout_exp_next(u8 slot, u8 gear) {
    s32 i;
    u8 digit;

    menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].expNextA);
    menu_state_current->detail->expCount = 0;
    for (i = 0; i < 7; i++) {
        digit = menu_state_current->digits[2 + i];
        if (digit != 0xff) {
            menu_state_current->detail->expCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->exp[menu_state_current->detail->expCount * 2],
                              menu_state_current->buffer_index, i * 8 + menu_detail_exp_next_a_x - gear * 0x18, menu_detail_exp_next_a_y, 0x1000);
        }
    }
    for (i = 0; i < menu_state_current->detail->expCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->expAt[i],
                      menu_state_current->detail->exp[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->exp[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->exp[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->exp[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->exp[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->exp[i * 2 + menu_state_current->buffer_index].y0);
    }
    menu_split_digits(game_data.characters[menu_state_current->flags->party[slot]].expNextB);
    menu_state_current->detail->expNextCount = 0;
    for (i = 0; i < 7; i++) {
        digit = menu_state_current->digits[2 + i];
        if (digit != 0xff) {
            menu_state_current->detail->expNextCount +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->expNext[menu_state_current->detail->expNextCount * 2],
                              menu_state_current->buffer_index, i * 8 + menu_detail_exp_next_b_x - gear * 0x18, menu_detail_exp_next_b_y, 0x1000);
        }
    }
    for (i = 0; i < menu_state_current->detail->expNextCount; i++) {
        menu_set_rect_verts(menu_state_current->detail->expNextAt[i],
                      menu_state_current->detail->expNext[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->expNext[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->expNext[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->expNext[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->expNext[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->expNext[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D7884: Lay out the detail panel's value derived from record +77..+79 of party
 * slot `slot` ((+78 + +77) * 10 + +79) * 22 shown in hundredths with one
 * decimal) or, with `gear`, its gear's +68 (five digits). */
void menu_detail_layout_field77_or_value68(u8 slot, u8 gear) {
    s32 digits;
    s32 first;
    s32 x;
    s32 i;
    u8 digit;
    u16 value;
    u16 whole;
    s32 tenth;

    if (!gear) {
        value = ((game_data.characters[menu_state_current->flags->party[slot]].field78 + game_data.characters[menu_state_current->flags->party[slot]].field77) *
                     10 +
                 game_data.characters[menu_state_current->flags->party[slot]].field79) *
                22;
        whole = value / 100;
        digits = 3;
        first = 6;
        tenth = value;
        tenth = (tenth - whole * 100) / 10;
        menu_split_digits(whole);
        x = menu_detail_field77_value_x;
    } else {
        digits = 5;
        menu_split_digits(game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].field68);
        first = 4;
        x = menu_detail_field77_value_x + 8;
    }
    menu_state_current->detail->list1C70Count = 0;
    for (i = 0; i < digits; i++) {
        digit = menu_state_current->digits[first + i];
        if (digit != 0xff) {
            menu_state_current->detail->list1C70Count +=
                sprite_sheet_draw_scaled(menu_state_current->sheet, digit,
                              &menu_state_current->detail->list1C70[menu_state_current->detail->list1C70Count * 2],
                              menu_state_current->buffer_index, i * 8 + x - gear * 0x18, gear * 8 + menu_detail_field77_value_y, 0x1000);
        }
    }
    if (!gear) {
        menu_state_current->detail->list1C70Count +=
            sprite_sheet_draw_scaled(menu_state_current->sheet, (u16)tenth,
                          &menu_state_current->detail->list1C70[menu_state_current->detail->list1C70Count * 2],
                          menu_state_current->buffer_index, menu_detail_field77_value_x + 0x20, menu_detail_field77_value_y, 0x1000);
    }
    for (i = 0; i < menu_state_current->detail->list1C70Count; i++) {
        menu_set_rect_verts(menu_state_current->detail->list1C70At[i],
                      menu_state_current->detail->list1C70[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->list1C70[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->detail->list1C70[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->detail->list1C70[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->detail->list1C70[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->detail->list1C70[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D7C3C: Build the status panels of party slot `slot` for `mode` and show them. */
void menu_detail_build(u8 slot, u8 mode) {
    menu_detail_layout_portrait(slot, mode);
    menu_detail_layout_parts(mode);
    menu_detail_layout_hp(slot, mode);
    menu_detail_layout_ep_or_fuel(slot, mode);
    menu_detail_layout_levels(slot, mode);
    menu_detail_layout_exp_next(slot, mode);
    menu_detail_layout_field77_or_value68(slot, mode);
    menu_detail_layout_exp_totals(slot, mode);
    menu_state_current->flags->detail_shown = 1;
    menu_state_current->detail->buffer = menu_state_current->buffer_index;
}

/* 801D7CFC: Show the detail panel's two tabs (when `shown`), the one not selected by
 * `second` dimmed and semi-transparent. */
void menu_detail_layout_tabs(u8 slot, u8 shown, u8 second) {
    s32 i;
    s32 dim;

    second = second == 0; /* now: the tab to dim */
    menu_state_current->detail->tabCount = 0;
    if (shown) {
        for (i = 0; i < 2; i++) {
            sprite_sheet_draw_scaled(menu_state_current->sheet, menu_detail_tab_images[i], &menu_state_current->detail->tabs[i * 2],
                          menu_state_current->buffer_index, i * 0x20 + 0x78, 0x5a, 0x1000);
            menu_set_rect_verts(menu_state_current->detail->tabsAt[i],
                          menu_state_current->detail->tabs[i * 2 + menu_state_current->buffer_index].x0,
                          menu_state_current->detail->tabs[i * 2 + menu_state_current->buffer_index].y0,
                          menu_state_current->detail->tabs[i * 2 + menu_state_current->buffer_index].x1 -
                              menu_state_current->detail->tabs[i * 2 + menu_state_current->buffer_index].x0,
                          menu_state_current->detail->tabs[i * 2 + menu_state_current->buffer_index].y3 -
                              menu_state_current->detail->tabs[i * 2 + menu_state_current->buffer_index].y0);
        }
        dim = second * 2;
        menu_quad_set_semi_transparent(menu_state_current->detail->tabs + (dim + menu_state_current->buffer_index));
        menu_state_current->detail->tabs[dim + menu_state_current->buffer_index].tpage |= 0x20;
        (menu_state_current->detail->tabs + (dim + menu_state_current->buffer_index))->r0 = 0x20;
        (menu_state_current->detail->tabs + (dim + menu_state_current->buffer_index))->g0 = 0x20;
        (menu_state_current->detail->tabs + (dim + menu_state_current->buffer_index))->b0 = 0x20;
        menu_state_current->detail->tabBuffer = menu_state_current->buffer_index;
        menu_state_current->detail->tabCount = 2;
    }
}

/* 801D7F50: Lay out the stat names of rows `first`..6 at (x, y) into the block at
 * +35c, dimming the odd rows, and place their quads. */
void menu_equip_panel_layout_stat_names(s32 x, s32 y, u8 first) {
    s32 clutX;
    s32 clutY;
    s32 pageX;
    s32 pageY;
    s32 u;
    s32 v;
    s32 i;
    s32 start;
    s32 part;

    sprite_sheet_get_texture(menu_state_current->sheet, 0xe0, &clutX, &clutY, &pageX, &pageY, &u, &v);
    menu_state_current->equip_panel->kind = 0;
    for (i = 0; i < 7 - first; i++) {
        start = menu_state_current->equip_panel->kind;
        menu_state_current->equip_panel->kind += sprite_sheet_draw_scaled(menu_state_current->sheet, menu_stat_name_images[first * 7 + i],
                                                    &menu_state_current->equip_panel->polys[start * 2], menu_state_current->buffer_index,
                                                    x + menu_stat_name_x_table[i], y + menu_stat_name_y_table[i], 0x1000);
        if (i & 1) {
            for (part = start; part < menu_state_current->equip_panel->kind; part++) {
                SetShadeTex(&menu_state_current->equip_panel->polys[part * 2 + menu_state_current->buffer_index], 0);
                (menu_state_current->equip_panel->polys + (part * 2 + menu_state_current->buffer_index))->r0 = 0x40;
                (menu_state_current->equip_panel->polys + (part * 2 + menu_state_current->buffer_index))->g0 = 0x40;
                (menu_state_current->equip_panel->polys + (part * 2 + menu_state_current->buffer_index))->b0 = 0x40;
            }
        }
    }
    for (i = 0; i < menu_state_current->equip_panel->kind; i++) {
        menu_set_rect_verts(&menu_state_current->equip_panel->verts[i * 4],
                      menu_state_current->equip_panel->polys[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->equip_panel->polys[i * 2 + menu_state_current->buffer_index].y0,
                      menu_state_current->equip_panel->polys[i * 2 + menu_state_current->buffer_index].x1 -
                          menu_state_current->equip_panel->polys[i * 2 + menu_state_current->buffer_index].x0,
                      menu_state_current->equip_panel->polys[i * 2 + menu_state_current->buffer_index].y3 -
                          menu_state_current->equip_panel->polys[i * 2 + menu_state_current->buffer_index].y0);
    }
}

/* 801D827C: Initialise the two gradient quads of a gauge in colour `colour` (0 pink,
 * 1 green, 2 red, 3 blue), fading to black. */
void menu_init_gauge_quads(POLY_G4 *polys, u8 colour) {
    u8 rgb[3];
    s32 i;

    switch (colour) {
    case 0:
        rgb[0] = 0xff;
        rgb[1] = 0x80;
        rgb[2] = 0x80;
        break;
    case 1:
        rgb[0] = 0x80;
        rgb[1] = 0xff;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0xff;
        rgb[1] = 0;
        rgb[2] = 0;
        break;
    case 3:
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0xff;
        break;
    }
    for (i = 0; i < 2; i++) {
        SetPolyG4(&polys[i]);
        polys[i].r0 = rgb[0];
        polys[i].g0 = rgb[1];
        polys[i].b0 = rgb[2];
        polys[i].r1 = rgb[0];
        polys[i].g1 = rgb[1];
        polys[i].b1 = rgb[2];
        polys[i].r2 = 0;
        polys[i].g2 = 0;
        polys[i].b2 = 0;
        polys[i].r3 = 0;
        polys[i].g3 = 0;
        polys[i].b3 = 0;
    }
}

/* 801D83AC: Tint `count` quads of `polys` (every other one from `first`): 0 red,
 * 1 blue, 2 grey. */
void menu_tint_quads(POLY_FT4 *polys, u8 colour, u8 count, u8 first) {
    u8 rgb[3];
    s32 i;
    POLY_FT4 *poly;

    rgb[1] = 0x40;
    switch (colour) {
    case 0:
        rgb[0] = 0x80;
        rgb[2] = 0x40;
        break;
    case 1:
        rgb[0] = 0x40;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0x40;
        rgb[2] = 0x40;
        break;
    }
    for (i = 0; i < count; i++) {
        poly = &polys[first + i * 2];
        SetShadeTex(poly, 0);
        poly->r0 = rgb[0];
        poly->g0 = rgb[1];
        poly->b0 = rgb[2];
    }
}

/* 801D84B4: Set up a gauge moving from `from` to `to` of `max`: its two lengths (of
 * 64) and the rising or falling look. */
void menu_stat_bar_init(u16 from, u16 to, s32 max) {
    s32 diff;

    menu_stat_bar_from = from;
    menu_stat_bar_unread_to = to;
    diff = to - from;
    menu_stat_bar_change = diff;
    menu_stat_bar_length = from * 100 / max * 0x1900 / 10000;
    if (diff >= 0) {
        menu_stat_bar_change_color = 2;
        menu_stat_bar_change_sign_image = 0xe3;
    } else {
        menu_stat_bar_change_color = 3;
        menu_stat_bar_change_sign_image = 0xe5;
        menu_stat_bar_change = from - to;
    }
    menu_stat_bar_change_length = menu_stat_bar_change * 100 / max * 0x1900 / 10000;
}

/* 801D85DC: The largest of the seven values in each of `a` and `b`. */
s32 menu_stat_bar_find_scale(s32 unused, u16 *a, u16 *b) {
    u16 max;
    s32 i;
    u8 reserved[24]; /* the original frame reserves 24 unused bytes */

    max = 0;
    for (i = 0; i < 7; i++) {
        if (max < *a) {
            max = *a;
        }
        a++;
    }
    for (i = 0; i < 7; i++) {
        if (max < *b) {
            max = *b;
        }
        b++;
    }
    return max;
}

/* 801D8644: Draw the stat bars at (`x`, `y`) for rows `first`..6, scaled to the
 * largest stat (the unused first argument holds it): each row's bar,
 * value digits and their quads; with `compare` also the change bar against
 * the kept stats and, for a change, its signed digits tinted by the change's
 * colour. */
void menu_equip_panel_layout_stat_bars(s32 scale, s32 x, s32 y, u8 compare, u8 first) {
    u16 *before;
    u16 *shown;
    s32 row;
    s32 i;
    s32 drawn;
    s32 start;

    if (!compare) {
        shown = menu_state_current->tables->stats;
        before = shown;
    } else {
        before = menu_state_current->equip_labels->stats;
        shown = menu_state_current->tables->stats;
    }
    scale = menu_stat_bar_find_scale(0, before, shown);
    for (row = 0; row < 7 - first; row++) {
        menu_state_current->equip_panel->rowACount[row] = 0;
        menu_state_current->equip_panel->rowBCount[row] = 0;
        menu_stat_bar_init(before[row], shown[row], scale);
        menu_init_gauge_quads(menu_state_current->equip_panel->bars[row], 0);
        menu_set_rect_verts(menu_state_current->equip_panel->barAt[row], menu_stat_bar_x_offset + x, y + menu_stat_bar_y_offset + row * 8, menu_stat_bar_length, 6);
        menu_state_current->equip_panel->barBuffer[row] = menu_state_current->buffer_index;
        menu_split_digits(menu_stat_bar_from);
        for (i = 0; i < 4; i++) {
            if (menu_state_current->digits[5 + i] != 0xff) {
                menu_state_current->equip_panel->rowACount[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, menu_state_current->digits[5 + i],
                                  &menu_state_current->equip_panel->rowA[row][menu_state_current->equip_panel->rowACount[row] * 2],
                                  menu_state_current->buffer_index, x + menu_stat_digits_x_offset + i * 8, y + menu_stat_digits_y_offset + row * 8, 0x1000);
            }
        }
        for (i = 0; i < menu_state_current->equip_panel->rowACount[row]; i++) {
            menu_set_rect_verts(&menu_state_current->equip_panel->rowAAt[row][i * 4],
                          menu_state_current->equip_panel->rowA[row][i * 2 + menu_state_current->buffer_index].x0,
                          menu_state_current->equip_panel->rowA[row][i * 2 + menu_state_current->buffer_index].y0,
                          menu_state_current->equip_panel->rowA[row][i * 2 + menu_state_current->buffer_index].x1 -
                              menu_state_current->equip_panel->rowA[row][i * 2 + menu_state_current->buffer_index].x0,
                          menu_state_current->equip_panel->rowA[row][i * 2 + menu_state_current->buffer_index].y3 -
                              menu_state_current->equip_panel->rowA[row][i * 2 + menu_state_current->buffer_index].y0);
        }
        if (row & 1) {
            menu_tint_quads(menu_state_current->equip_panel->rowA[row], 2, menu_state_current->equip_panel->rowACount[row],
                          menu_state_current->buffer_index);
        }
        menu_state_current->equip_panel->rowABuffer[row] = menu_state_current->buffer_index;
        if (compare) {
            menu_init_gauge_quads(menu_state_current->equip_panel->highlights[row], menu_stat_bar_change_color);
            if (menu_stat_bar_change_color == 2) {
                start = x + menu_stat_bar_x_offset + menu_stat_bar_length;
            } else {
                start = x + menu_stat_bar_x_offset + menu_stat_bar_length - menu_stat_bar_change_length;
            }
            menu_set_rect_verts(menu_state_current->equip_panel->highlightAt[row], start, y + menu_stat_bar_y_offset + row * 8, menu_stat_bar_change_length, 6);
            menu_state_current->equip_panel->highlightBuffer[row] = menu_state_current->buffer_index;
            if (compare && menu_stat_bar_change != 0) {
                menu_state_current->equip_panel->rowBCount[row] +=
                    sprite_sheet_draw_scaled(menu_state_current->sheet, menu_stat_bar_change_sign_image, menu_state_current->equip_panel->rowB[row],
                                  menu_state_current->buffer_index, x + menu_stat_digits_x_offset + 0x20, y + menu_stat_digits_y_offset + row * 8, 0x1000);
                menu_split_digits(menu_stat_bar_change);
                for (i = 0, drawn = 0; i < 3; i++) {
                    if (menu_state_current->digits[6 + i] != 0xff) {
                        menu_state_current->equip_panel->rowBCount[row] += sprite_sheet_draw_scaled(
                            menu_state_current->sheet, menu_state_current->digits[6 + i],
                            &menu_state_current->equip_panel->rowB[row][menu_state_current->equip_panel->rowBCount[row] * 2],
                            menu_state_current->buffer_index, x + menu_stat_digits_x_offset + 0x28 + drawn * 8, y + menu_stat_digits_y_offset + row * 8, 0x1000);
                        drawn++;
                    }
                }
                for (i = 0; i < menu_state_current->equip_panel->rowBCount[row]; i++) {
                    menu_set_rect_verts(&menu_state_current->equip_panel->rowBAt[row][i * 4],
                                  menu_state_current->equip_panel->rowB[row][i * 2 + menu_state_current->buffer_index].x0,
                                  menu_state_current->equip_panel->rowB[row][i * 2 + menu_state_current->buffer_index].y0,
                                  menu_state_current->equip_panel->rowB[row][i * 2 + menu_state_current->buffer_index].x1 -
                                      menu_state_current->equip_panel->rowB[row][i * 2 + menu_state_current->buffer_index].x0,
                                  menu_state_current->equip_panel->rowB[row][i * 2 + menu_state_current->buffer_index].y3 -
                                      menu_state_current->equip_panel->rowB[row][i * 2 + menu_state_current->buffer_index].y0);
                }
                menu_tint_quads(menu_state_current->equip_panel->rowB[row], menu_stat_bar_change_color - 2, menu_state_current->equip_panel->rowBCount[row],
                              menu_state_current->buffer_index);
                menu_state_current->equip_panel->rowBBuffer[row] = menu_state_current->buffer_index;
                menu_state_current->equip_panel->highlighted = 1;
            }
        }
        menu_state_current->equip_panel->rowShown[row] = 1;
    }
}

/* 801D8DE4: Build the equipment panels of `slot` at the upper or (`lower`) lower place. */
void menu_equip_panel_build(u8 slot, u8 lower, u8 compare, u8 mode) {
    s32 x;
    s32 y;

    x = 0x98;
    y = 0x26;
    if (lower) {
        x = 0x80;
        y = 0x90;
    }
    menu_equip_panel_layout_stat_names(x, y, mode);
    menu_equip_panel_layout_stat_bars(slot, x, y, compare, mode);
    menu_state_current->flags->equipment_shown = 1;
    menu_state_current->equip_panel->buffer = menu_state_current->buffer_index;
}

/* 801D8EA4: Draw the part panel of party slot `slot` on the equipment screen: rows of
 * part names (weapon and accessories for `mode` 0, the special parts in rows
 * of four for 1 and 2), from the kept parts with `kept` and from the gear
 * with `gear`; a character's row 4 shows its gear's name instead. */
void menu_equip_labels_layout_parts(u8 slot, u8 mode, u8 kept, u8 gear) {
    RECT rect;
    s32 rows;
    s32 column;
    s32 base;
    s32 i;
    u8 *weapons;
    u8 *specials;
    u8 *accessories;
    u8 *name;
    u8 *image;
    u8 load;

    rows = 5;
    column = 0xd0;
    base = 0;
    weapons = game_data.characters[menu_state_current->flags->party[slot]].weapons;
    specials = game_data.characters[menu_state_current->flags->party[slot]].entryItems;
    accessories = game_data.characters[menu_state_current->flags->party[slot]].accessories;
    if (mode) {
        rows = 4;
        column = 0x28;
        base = 9;
        if (mode == 1) {
            base = 5;
        }
        menu_state_current->equip_labels->visible[4] = 0;
    }
    if (gear) {
        weapons = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].weapons;
        specials = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].partItems;
        accessories = game_data.gears[game_data.characters[menu_state_current->flags->party[slot]].gearId].parts;
    }
    if (kept) {
        weapons = menu_state_current->equip_labels->parts[0];
        specials = menu_state_current->equip_labels->parts[1];
        accessories = menu_state_current->equip_labels->parts[2];
    }
    image = heap_alloc(0x3f6, 0);
    for (i = 0; i < rows; i++) {
        switch (i) {
        case 0:
            if (mode != 2) {
                if (!gear) {
                    menu_state_current->equip_labels->labels[0].width = window_render_text_line(text_get_weapon_name(*weapons), image, 0x24, 0);
                } else {
                    menu_state_current->equip_labels->labels[0].width = window_render_text_line(text_get_gear_part_name(*weapons), image, 0x24, 0);
                }
            } else if (!gear) {
                menu_state_current->equip_labels->labels[0].width = window_render_text_line(text_get_weapon_name(*specials), image, 0x24, 0);
            } else {
                menu_state_current->equip_labels->labels[0].width = window_render_text_line(text_get_gear_part_name(*specials), image, 0x24, 0);
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            if (i != 4 || gear) {
                if (mode < 2) {
                    if (!gear) {
                        name = text_get_accessory_name(accessories[i - 1]);
                    } else if (mode == 0) {
                        if (i == 1) {
                            menu_state_current->equip_labels->labels[1].width = window_render_text_line(text_get_gear_part_name(weapons[3]), image, 0x24, 1);
                            goto shown;
                        }
                        name = text_get_gear_accessory_name(accessories[i - 2]);
                    } else {
                        name = text_get_gear_accessory_name(accessories[i - 1]);
                    }
                } else if (!gear) {
                    name = text_get_weapon_name(specials[i]);
                } else {
                    name = text_get_gear_part_name(specials[i]);
                }
                menu_state_current->equip_labels->labels[i].width = window_render_text_line(name, image, 0x24, i % 2);
            }
            break;
        }
    shown:
        if (i & 1) {
            load = 1;
        } else if (!gear) {
            load = 0;
        } else if (mode == 0 && i == 4) {
            load = 1;
        }
        if (load) {
            rect.x = (i / 2 & 1) * 0x20 + 0x140;
            rect.y = i / 4 * 0xd + 0x27;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, (u_long *)image);
            DrawSync(0);
        }
        menu_label_init_quads(&menu_state_current->equip_labels->labels[i], i, 0xc, 0);
        if (i == 4 && !gear) {
            s32 page;

            page = GetTPage(0, 0, 0x180, 0);
            {
                s32 packetOffset = menu_state_current->buffer_index * sizeof(POLY_FT4) +
                    4 * sizeof(MenuLabel);
                ((POLY_FT4 *)((u8 *)menu_state_current->equip_labels + packetOffset))->tpage = page;
            }
            {
                s32 packetOffset = menu_state_current->buffer_index * sizeof(POLY_FT4) +
                    4 * sizeof(MenuLabel);
                ((POLY_FT4 *)((u8 *)menu_state_current->equip_labels + packetOffset))->clut =
                    ((game_data.characters[menu_state_current->flags->party[slot]].gearId + 11) & 1) ? text_plane1_clut : text_plane0_clut;
            }
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->u0 = menu_name_image_vram_x_table[slot + 3] * 4;
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->v0 = menu_name_image_vram_y_table[slot + 3];
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->u1 = menu_name_image_vram_x_table[slot + 3] * 4 + 0x60;
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->v1 = menu_name_image_vram_y_table[slot + 3];
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->u2 = menu_name_image_vram_x_table[slot + 3] * 4;
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->v2 = menu_name_image_vram_y_table[slot + 3] + 0xd;
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->u3 = menu_name_image_vram_x_table[slot + 3] * 4 + 0x60;
            (menu_state_current->equip_labels->labels[i].polys + menu_state_current->buffer_index)->v3 = menu_name_image_vram_y_table[slot + 3] + 0xd;
            menu_state_current->equip_labels->labels[i].width = 0x60;
        }
        menu_set_rect_verts(menu_state_current->equip_labels->labels[i].verts, column, menu_equip_labels_row_y_table[i + base],
                      menu_state_current->equip_labels->labels[i].width, 0xd);
        menu_state_current->equip_labels->visible[i] = 1;
    }
    menu_state_current->equip_labels->count = menu_state_current->buffer_index;
    menu_state_current->flags->equip_labels_shown = 1;
    heap_free(image);
}

/* 801D9704: Step party slot `slot` forward (`dir` 0) or back (1) to the next occupied
 * slot, or with `readyOnly` to the next ready one; wraps around the three. */
s32 menu_step_party_slot(s32 slot, u8 dir, u8 readyOnly) {
    switch (dir) {
    case 0:
        for (;;) {
            slot++;
            if (slot >= 3) {
                slot = 0;
            }
            if (!readyOnly) {
                if (menu_state_current->flags->party[slot] != 0xff) {
                    break;
                }
            } else if (menu_state_current->flags->ready[slot] != 0) {
                break;
            }
        }
        break;
    case 1:
        for (;;) {
            slot--;
            if (slot < 0) {
                slot = 2;
            }
            if (!readyOnly) {
                if (menu_state_current->flags->party[slot] != 0xff) {
                    break;
                }
            } else if (menu_state_current->flags->ready[slot] != 0) {
                break;
            }
        }
        break;
    }
    return slot;
}

/* 801D9808: The sound mode screen: choose one of the sound driver's output
 * modes (choice 0 is mode 0, 1 mode 2, 2 mode 1); confirm applies it, cancel
 * leaves. Always continues the menu. */
u8 menu_sound_mode_screen_run(void) {
    s32 mode;
    u8 first;
    u8 stay;
    u8 apply;

    stay = 1;
    first = 1;
    apply = 0;
    do {
        menu_run_frame();
        if (first) {
            menu_label_render_table(4, menu_state_current->sound_labels, menu_sound_mode_label_ids, menu_state_current->flags->sound_labels_shown);
            menu_choice_window_open(0);
            menu_state_current->choice_shown = 0xff;
            switch (sound_get_output_mode()) {
            case 0:
                mode = 0;
                break;
            case 1:
                mode = 2;
                break;
            case 2:
                mode = 1;
                break;
            }
            first = 0;
            menu_state_current->choice = mode;
        }
        if (menu_state_current->choice != menu_state_current->choice_shown) {
            menu_label_place(6, menu_state_current->sound_labels, (u8 *)menu_name_image_vram_x_table, menu_sound_mode_label_x_offsets, menu_state_current->flags->sound_labels_shown,
                          menu_state_current->choice, 7, 0);
            menu_choice_window_set_cursor(0);
            menu_state_current->choice_shown = menu_state_current->choice;
        }
        switch (menu_state_current->input) {
        case 4:
            apply = 1;
        case 5:
            stay = 0;
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
        }
    } while (stay);
    if (apply) {
        switch (menu_state_current->choice) {
        case 0:
            mode = 0;
            break;
        case 1:
            mode = 2;
            break;
        case 2:
            mode = 1;
            break;
        }
        sound_set_output_mode(mode);
    }
    menu_label_clear_shown(4, menu_state_current->flags->sound_labels_shown);
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    return 1;
}

/* 801D9B08: Restart memory-card access: close the card events, reinitialise the card
 * library and open and enable its four events (ioe, error, timeout, new card). */
void menu_card_restart_access(void) {
    menu_card_close_events();
    VSync(0);
    InitCARD(1);
    StartCARD();
    _bu_init();
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    menu_state_current->card->events[0] = OpenEvent(0xf4000001, 4, 0x2000, 0);
    menu_state_current->card->events[1] = OpenEvent(0xf4000001, 0x8000, 0x2000, 0);
    menu_state_current->card->events[2] = OpenEvent(0xf4000001, 0x100, 0x2000, 0);
    menu_state_current->card->events[3] = OpenEvent(0xf4000001, 0x2000, 0x2000, 0);
    EnableEvent(menu_state_current->card->events[0]);
    EnableEvent(menu_state_current->card->events[1]);
    EnableEvent(menu_state_current->card->events[2]);
    EnableEvent(menu_state_current->card->events[3]);
    ExitCriticalSection();
}

/* 801D9C84: Enter the file screen's card mode: show message 20, wait for the view to
 * stop, restart card access with the CD callbacks saved and cleared, forget
 * the card presence and listings and check the cards now. Returns nonzero
 * when the check fails (the message stays up); otherwise closes the message. */
u8 menu_file_screen_enter_card_mode(void) {
    MenuCard *card;
    u8 failed;

    failed = 1;
    menu_notice_open(0x20);
    menu_state_current->flags->card_message_shown = 1;
    while (menu_state_current->view_motion != 0) {
        menu_run_frame();
    }
    menu_state_current->card->busy = 1;
    menu_run_frame();
    menu_card_restart_access();
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    menu_saved_cd_sync_callback = CdSyncCallback(0);
    menu_saved_cd_ready_callback = CdReadyCallback(0);
    menu_saved_cd_read_callback = CdReadCallback(0);
    ExitCriticalSection();
    card = menu_state_current->card;
    card->present_shown[0] = card->present_shown[1] = 0xff;
    menu_state_current->card->scanned[0] = 0;
    menu_state_current->card->scanned[1] = 0;
    menu_state_current->card->mode = 2;
    menu_state_current->card_poll_timer = 0x3c;
    menu_run_frame();
    menu_run_frame();
    if (menu_card_refresh() == 0) {
        if (menu_state_current->flags->card_message_shown != 0) {
            menu_notice_close();
            menu_state_current->flags->card_message_shown = 0;
        }
        failed = 0;
    }
    return failed;
}

/* 801D9E3C: Leave the save/load screen: clear its images and listing, and close the
 * card events and handlers. */
void menu_file_screen_leave_card_mode(void) {
    s32 i;

    menu_state_current->load_state = 0;
    menu_file_info_clear();
    menu_file_slots_free();
    menu_file_info_free();
    for (i = 0; i < 32; i++) {
        menu_state_current->card->files[i].state = 0;
    }
    menu_state_current->card->unknown4f8c[0] = 0xff;
    menu_state_current->card->unknown4f8c[1] = 0xff;
    menu_card_blocks_used[1] = 0;
    menu_card_blocks_used[0] = 0;
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    CdSyncCallback(menu_saved_cd_sync_callback);
    CdReadyCallback(menu_saved_cd_ready_callback);
    CdReadCallback(menu_saved_cd_read_callback);
    ExitCriticalSection();
}

/* 801D9F34: Lay out the six file screen command labels (save or title variant). */
void menu_file_screen_layout_commands(void) {
    if (menu_state_screen != 2) {
        menu_label_render_table(6, menu_state_current->extra_labels, menu_file_command_label_ids, menu_state_current->flags->extra_labels_shown);
    } else {
        menu_label_render_table(6, menu_state_current->extra_labels, &menu_file_command_label_ids[6], menu_state_current->flags->extra_labels_shown);
    }
}

/* 801D9F98: The file screen (save or load by `save`): on the first pass build the card
 * panels, zoom in, show the header and enter card mode; then each frame
 * check the cards, show the choice labels and handle the input: confirm runs
 * the file command (and ends the screen outside the title), cancel ends it.
 * Returns 0 when a `loading` screen was left by the player, the cards failed
 * outside the field menu or there is no card outside the field menu. */
u8 menu_file_screen_run(u8 loading, u8 save) {
    u8 running;
    u8 first;
    u8 result;
    u8 header;

    running = 1;
    first = 1;
    result = 1;
    menu_soft_reset_enabled = 0;
    menu_state_current->choice = 2;
    header = 0;
    if (menu_state_screen == 0 && menu_state_screen_parameter == 0) {
        menu_state_current->choice = 1;
    }
    menu_state_current->choice_shown = 0xff;
    menu_file_screen_layout_commands();
    menu_run_frame();
    while (running) {
        menu_state_current->flags->file_info_shown = 0;
        menu_state_current->load_state = 1;
        if (first) {
            menu_file_info_alloc();
            menu_file_slots_alloc();
            menu_state_current->card->present[0] = 1;
            menu_state_current->card->present[1] = 1;
            menu_view_start_zoom_in();
            switch (menu_state_screen) {
            case 0:
                menu_field_blocks_slide(0, 0);
                if (menu_state_screen_parameter == 0) {
                    header = 9;
                }
                break;
            case 2:
                header = 7;
                break;
            case 6:
                break;
            }
            menu_choice_window_open(header);
            menu_state_current->flags->field_menu2_shown = 0;
            first = 0;
            menu_state_current->flags->panels_shown[1] = 0;
            if (menu_file_screen_enter_card_mode()) {
                if (menu_state_screen != 0) {
                    result = 0;
                }
                break;
            }
            menu_state_current->flags->card_mode = 1;
        }
        if (menu_card_changed != 0) {
            menu_notice_open(0x20);
        }
        if (menu_card_refresh()) {
            running = 0;
        }
        if (menu_card_changed != 0) {
            menu_notice_close();
            menu_card_changed = 0;
        }
        if (!running) {
            break;
        }
        if (menu_state_current->choice != menu_state_current->choice_shown) {
            menu_label_place(6, menu_state_current->extra_labels, &menu_file_command_label_ids[6], menu_file_command_label_x_offsets, menu_state_current->flags->extra_labels_shown,
                          menu_state_current->choice, 7, 0);
            if (menu_state_screen != 2) {
                menu_choice_window_set_cursor(0);
            } else {
                menu_choice_window_set_cursor(7);
            }
            menu_state_current->choice_shown = menu_state_current->choice;
        }
        switch (menu_state_current->input) {
        case 4:
            menu_highlight_hide();
            menu_label_clear_shown(6, menu_state_current->flags->extra_labels_shown);
            running = menu_file_command_run(save);
            menu_state_current->choice_shown = 0xff;
            menu_file_screen_layout_commands();
            if (menu_state_screen == 2) {
                break;
            }
        case 5:
            running = 0;
            if (loading) {
                result = 0;
            }
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
        }
    }
    if (*(u16 *)menu_state_current->card->present == 0 && menu_state_screen != 0) {
        result = 0;
    }
    menu_state_current->flags->card_mode = 0;
    menu_state_current->flags->file_info_shown = 0;
    menu_state_current->flags->sprite_shown = 0;
    menu_state_current->flags->cursor_shown = 0;
    menu_label_clear_shown(6, menu_state_current->flags->extra_labels_shown);
    menu_card_close_events();
    return result;
}

/* 801DA4A8: Open the item screen: its labels, its 1198-byte list block and data view 0
 * (the item table and descriptions). */
void menu_item_screen_open(void) {
    void *block;

    menu_markers_layout(2);
    menu_label_render_table(8, menu_state_current->labels10e0, menu_item_arts_label_ids, menu_state_current->flags->labels10e0_shown);
    block = heap_alloc(0x1198, 0);
    menu_state_current->item_list = block;
    bzero(block, 0x1198);
    menu_load_or_release_data_set(0);
}

/* 801DA518: Close the item screen: its labels, panels 3-4, its list block and the item
 * data (view 10). */
void menu_item_screen_close(void) {
    menu_scroll_bar_hide();
    menu_panel_close(3);
    menu_panel_close(4);
    menu_state_current->flags->item_list_shown = 0;
    menu_load_or_release_data_set(0x10);
    heap_free(menu_state_current->item_list->unk1180);
    heap_free(menu_state_current->item_list);
    heap_free(menu_state_current->tables->items);
}

/* Column of item entry `i`: its x offset and its name's left edge (8-aligned).
 * A statement macro (do/while (0)). */
#define ITEM_COLUMN(i, x, left)            \
    do {                                   \
        (x) = ((i) % 2) * 0x88;            \
        (left) = ((x) + 0x28) & 0xfff8;    \
    } while (0)

/* 801DA5BC: Build the 16 visible entries of the item list from scroll row `row`: an
 * entry without an id or count is emptied; otherwise the count is capped at
 * 99 and the item name and two-digit count are rendered into the image area
 * and laid out, greyed when the item cannot be used here. */
/* The counts are addressed from the ids (game_data.itemIds - 150), as the original
 * does. */
void menu_item_screen_build_list(s32 row) {
    u8 codes[4];
    u8 text[8];
    RECT rect;
    s32 i;
    u8 tens;
    s32 x;
    s32 left;
    u16 y;
    u8 kind;
    u8 grey;
    u8 *image;

    image = heap_alloc(0x3f6, 0);
    codes[1] = 0;
    codes[3] = 0;
    for (i = 0; i < 16; i++) {
        if (game_data.itemIds[row * 2 + i] != 0) {
            if ((game_data.itemIds - 150)[row * 2 + i] != 0) {
                if ((game_data.itemIds - 150)[row * 2 + i] >= 100) {
                    (game_data.itemIds - 150)[row * 2 + i] = 99;
                }
                menu_state_current->item_list->names[i].width = window_render_text_line(text_get_item_name(game_data.itemIds[row * 2 + i]), image, 0x24, 0);
                tens = (game_data.itemIds - 150)[row * 2 + i] / 10;
                if (tens != 0) {
                    codes[0] = tens + 0x10;
                } else {
                    codes[0] = 0xc3;
                }
                codes[2] = (game_data.itemIds - 150)[row * 2 + i] % 10 + 0x10;
                text_decode_codes(codes, text, 2);
                menu_state_current->item_list->values[i].width = window_render_text_line(text, image, 0x24, 1);
                rect.x = (i & 1) * 0x18 + 0x180;
                rect.y = (i / 2) * 0xd + 0x80;
                rect.w = 0x28;
                rect.h = 0xd;
                LoadImage(&rect, (u_long *)image);
                DrawSync(0);
                kind = menu_state_current->tables->items[game_data.itemIds[row * 2 + i]].use;
                if (kind & 0x20) {
                    grey = kind & 0x80;
                    if (menu_state_screen_parameter == 0) {
                        grey = 0;
                    }
                } else {
                    grey = kind & 0x80;
                }
                menu_label_init_quads(&menu_state_current->item_list->names[i], i, 0x80, grey | 1);
                menu_label_init_quads(&menu_state_current->item_list->values[i], i, 0x80, grey | 2);
                ITEM_COLUMN(i, x, left);
                y = (i / 2) * 0x10 | 0xe;
                menu_set_rect_verts(menu_state_current->item_list->names[i].verts, left, y, menu_state_current->item_list->names[i].width,
                              0xd);
                left = (x + 0x90) & 0xfff8;
                menu_set_rect_verts(menu_state_current->item_list->values[i].verts, left, y, menu_state_current->item_list->values[i].width,
                              0xd);
                menu_state_current->item_list->names[i].buffer = menu_state_current->buffer_index;
                menu_state_current->item_list->values[i].buffer = menu_state_current->buffer_index;
                menu_state_current->item_list->shown[i] = 1;
            } else {
                game_data.itemIds[row * 2 + i] = 0;
                menu_state_current->item_list->shown[i] = 0;
            }
        } else {
            (game_data.itemIds - 150)[row * 2 + i] = 0;
            menu_state_current->item_list->shown[i] = 0;
        }
    }
    heap_free(image);
    menu_state_current->flags->item_list_shown = 1;
}

/* 801DA9A8: Show the description of item list entry `entry` at scroll row `row`: the
 * item's message line, a copy of its name and count texts and, for items
 * used from the menu, its target labels. An empty entry hides it. */
void menu_item_screen_show_description(s32 entry, s32 row) {
    RECT rect;
    u8 *ids;
    u8 *id;
    u8 *image;
    s32 pad;
    u8 all;
    u8 kind;
    u16 target;
    ItemInfo *item;

    ids = game_data.itemIds;
    id = &ids[row * 2 + entry];
    if (*id != 0) {
        image = heap_alloc(0x618, 0);
        bzero(image, 0x618);
        menu_state_current->item_list->extra[2].width =
            window_render_text_line(text_get_resource_entry(menu_state_current->item_list->unk1180, *id), image, 0x39, 0);
        rect.x = 0x140;
        rect.y = 0x4e;
        rect.w = 0x3c;
        rect.h = 0xd;
        LoadImage(&rect, (u_long *)image);
        DrawSync(0);
        menu_label_init_quads(&menu_state_current->item_list->extra[2], 0, 0, 0);
        menu_quad_place(&menu_state_current->item_list->extra[2].polys[menu_state_current->buffer_index], 0x1c, 0xa1, 0, 0x4e,
                      menu_state_current->item_list->extra[2].width, 0xd);
        menu_set_rect_verts(menu_state_current->item_list->extra[2].verts, 0x1c, 0xa1, menu_state_current->item_list->extra[2].width, 0xd);
        heap_free(image);
        pad = menu_state_current->item_list->values[entry].width == 0x10 ? 4 : 0;
        memmove(&menu_state_current->item_list->extra[0], &menu_state_current->item_list->names[entry], sizeof(MenuLabel));
        memmove(&menu_state_current->item_list->extra[1], &menu_state_current->item_list->values[entry], sizeof(MenuLabel));
        menu_set_rect_verts(menu_state_current->item_list->extra[0].verts, 0x10, 0x93, menu_state_current->item_list->names[entry].width,
                      0xd);
        menu_set_rect_verts(menu_state_current->item_list->extra[1].verts, pad | 0x78, 0x93,
                      menu_state_current->item_list->values[entry].width, 0xd);
        (menu_state_current->item_list->extra[0].polys + menu_state_current->buffer_index)->r0 = 0x80;
        (menu_state_current->item_list->extra[0].polys + menu_state_current->buffer_index)->g0 = 0x80;
        (menu_state_current->item_list->extra[0].polys + menu_state_current->buffer_index)->b0 = 0x80;
        SetSemiTrans(&menu_state_current->item_list->extra[0].polys[menu_state_current->buffer_index], 0);
        (menu_state_current->item_list->extra[1].polys + menu_state_current->buffer_index)->r0 = 0x80;
        (menu_state_current->item_list->extra[1].polys + menu_state_current->buffer_index)->g0 = 0x80;
        (menu_state_current->item_list->extra[1].polys + menu_state_current->buffer_index)->b0 = 0x80;
        SetSemiTrans(&menu_state_current->item_list->extra[1].polys[menu_state_current->buffer_index], 0);
        menu_label_clear_shown(8, menu_state_current->flags->labels10e0_shown);
        item = &menu_state_current->tables->items[*id];
        if (item->use & 0xc0) {
            target = item->target;
            if (target & 0x4000) {
                all = 2;
            } else if (target & 0x1000) {
                all = 0;
            } else {
                all = 1;
            }
            menu_label_place(8, menu_state_current->labels10e0, menu_item_target_label_ids, menu_file_command_label_x_offsets, menu_state_current->flags->labels10e0_shown, all, 0, 1);
            kind = (item->target & 3) + 3;
            menu_label_place(8, menu_state_current->labels10e0, menu_item_target_label_ids, menu_file_command_label_x_offsets, menu_state_current->flags->labels10e0_shown, kind, 0, 1);
            (menu_state_current->labels10e0[all].polys + menu_state_current->buffer_index)->r0 = 0x80;
            (menu_state_current->labels10e0[all].polys + menu_state_current->buffer_index)->g0 = 0x80;
            (menu_state_current->labels10e0[all].polys + menu_state_current->buffer_index)->b0 = 0x80;
            SetSemiTrans(&menu_state_current->labels10e0[all].polys[menu_state_current->buffer_index], 0);
            (menu_state_current->labels10e0[kind].polys + menu_state_current->buffer_index)->r0 = 0x80;
            (menu_state_current->labels10e0[kind].polys + menu_state_current->buffer_index)->g0 = 0x80;
            (menu_state_current->labels10e0[kind].polys + menu_state_current->buffer_index)->b0 = 0x80;
            SetSemiTrans(&menu_state_current->labels10e0[kind].polys[menu_state_current->buffer_index], 0);
        }
        menu_state_current->item_list->extra[0].buffer = menu_state_current->buffer_index;
        menu_state_current->item_list->extra[1].buffer = menu_state_current->buffer_index;
        menu_state_current->item_list->extra[2].buffer = menu_state_current->buffer_index;
        menu_state_current->item_list->extraShown = 1;
    } else {
        menu_label_clear_shown(8, menu_state_current->flags->labels10e0_shown);
        menu_state_current->item_list->extraShown = 0;
    }
}

/* 801DB02C: Allocate image block `index` (+444). */
void menu_list_cursor_alloc(u8 index) {
    void *block;

    block = heap_alloc(0x78, 0);
    menu_state_current->cursors[index] = block;
    bzero(block, 0x78);
    menu_state_current->cursors[index]->frame = 4;
    menu_state_current->cursors[index]->timer = 0;
}

/* 801DB0A8: Animate list cursor `index` (image block +444) and place it at entry
 * `entry` of the list kind `kind` (0 item list, 1 scrolled item list at
 * scroll row `row`, hidden when off the page, 2 two-column list, 3 one
 * column). */
void menu_list_cursor_place(s32 entry, s32 row, u8 kind, u8 index) {
    MenuCursor *cursor;
    POLY_FT4 *poly;
    s32 x;
    s32 y;
    s32 shown;

    cursor = menu_state_current->cursors[index];
    shown = 1;
    if (++cursor->timer >= 6) {
        if (--cursor->frame < 0) {
            cursor->frame = 4;
        }
        cursor->timer = 0;
    }
    switch (kind) {
    case 0:
        x = (entry % 2) * 0x88 + 0x1c;
        y = (entry / 2) * 0x10 + 0x11;
        break;
    case 1:
        if (entry >= row * 2 && entry < row * 2 + 0x10) {
            x = (entry % 2) * 0x88 + 0x18;
            y = (entry - row * 2) / 2 * 0x10 + 0x11;
        } else {
            shown = 0;
        }
        break;
    case 2:
        x = (entry % 2) * 0x88 + 0x18;
        y = entry / 2 * 0x10 + 0x14;
        break;
    case 3:
        x = 0xa0;
        y = entry * 0xd + 0x14;
        shown = 2;
        break;
    }
    if (shown) {
        sprite_sheet_draw_scaled(menu_state_current->sheet, cursor->frame + 0x15b, cursor, menu_state_current->buffer_index, 0, 0, 0x1000);
        poly = &cursor->polys[menu_state_current->buffer_index];
        menu_set_rect_verts(cursor->verts, poly->x0 + x, poly->y0 + y, poly->x1 - poly->x0, poly->y3 - poly->y0);
        cursor->buffer = menu_state_current->buffer_index;
        menu_state_current->flags->cursors_shown[index] = 1;
    } else {
        menu_state_current->flags->cursors_shown[index] = 0;
    }
}

/* 801DB340: Free image block `index` (+444) and clear its party flag (+50). */
void menu_list_cursor_free(u8 index) {
    heap_free(menu_state_current->cursors[index]);
    menu_state_current->flags->cursors_shown[index] = 0;
}

/* 801DB39C: Shade the item screen's texts by `mode` (801e8eac): windows 3 and 4, the
 * window quad, each built entry name and count, the two cursors, the
 * selected entry, the description and the eight help labels. */
void menu_item_screen_set_dimmed(u8 mode) {
    s32 i;

    menu_panel_set_dimmed(3, mode);
    menu_panel_set_dimmed(4, mode);
    menu_quad_set_blending(&menu_state_current->scroll->polys[menu_state_current->scroll->buffer], mode);
    i = 0;
    do {
        if (menu_state_current->item_list->names[i].polys[menu_state_current->item_list->names[i].buffer].r0 != 0x20) {
            menu_quad_set_blending(&menu_state_current->item_list->names[i].polys[menu_state_current->item_list->names[i].buffer], mode);
            menu_quad_set_blending(&menu_state_current->item_list->values[i].polys[menu_state_current->item_list->values[i].buffer], mode);
        }
        i++;
    } while (i < 16);
    for (i = 0; i < 2; i++) {
        menu_quad_set_blending(&menu_state_current->cursors[i]->polys[menu_state_current->cursors[i]->buffer], mode);
    }
    menu_quad_set_blending(&menu_state_current->item_list->extra[0].polys[menu_state_current->item_list->extra[0].buffer], mode);
    menu_quad_set_blending(&menu_state_current->item_list->extra[1].polys[menu_state_current->item_list->extra[1].buffer], mode);
    menu_quad_set_blending(&menu_state_current->item_list->extra[2].polys[menu_state_current->item_list->extra[2].buffer], mode);
    for (i = 0; i < 8; i++) {
        menu_quad_set_blending(&menu_state_current->labels10e0[i].polys[menu_state_current->labels10e0[i].buffer], mode);
    }
}

/* 801DB5E4: Build the target selection's party panels: allocate the three panel
 * blocks once, clear them and build each occupied slot's panel (layout 1
 * for `mode` 2, where only characters with a gear qualify; `mode` then
 * becomes that layout flag); then place the three target cursor quads. */
void menu_target_panels_build(u8 mode) {
    MenuAnchor *xs;
    MenuAnchor *ys;
    MenuStatusPanel *panel;
    void *block;
    s32 i;
    u8 ok;
    u8 id;

    if (menu_target_panels_allocated == 0) {
        for (i = 0; i < 3; i++) {
            block = heap_alloc(0xbec, 0);
            menu_state_current->party_panels[i] = block;
            bzero(block, 0xbec);
        }
        menu_target_panels_allocated = 1;
    }
    for (i = 0; i < 3; i++) {
        bzero((u_char *)menu_state_current->party_panels[i], 0xbec);
    }
    if (mode != 2) {
        xs = menu_target_panel_x_anchors;
        ys = menu_target_panel_y_anchors;
        mode = 0;
    } else {
        xs = menu_target_panel_gear_x_anchors;
        ys = menu_target_panel_gear_y_anchors;
        mode = 1;
    }
    for (i = 0; i < 3; i++) {
        panel = menu_state_current->party_panels[i];
        id = menu_state_current->flags->party[i];
        ok = 1;
        if (id != 0xff) {
            if (mode) {
                ok = game_data.characters[id].gearId != 0xff;
            }
            if (ok) {
                menu_status_panel_build(panel, id, i, xs, ys, mode);
            }
        } else {
            panel->shown = 0;
        }
    }
    menu_state_current->flags->party_panels_shown = 1;
    for (i = 0; i < 3; i++) {
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x0 = 0x90;
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y0 = i * 0x38 + 0x30;
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x1 = 0xa0;
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y1 = i * 0x38 + 0x30;
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x2 = 0x90;
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y2 = i * 0x38 + 0x40;
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->x3 = 0xa0;
        (menu_state_current->markers->polys + (i * 2 + menu_state_current->markers->buffer[i]))->y3 = i * 0x38 + 0x40;
    }
}

/* 801DB920: Use the item at visible entry `entry` of scroll row `row`: for an item
 * usable here, select targets (the whole party for all-target items, else
 * the cursor's slot; left/right move it) and use the item on them on
 * confirm until it runs out or the player cancels. Returns the targets
 * marked last (0 when cancelled or unusable). The item ids are read through
 * `ids`, taken from the inventory record at entry and again for the clear. */
u8 menu_item_screen_use_item(s32 row, s32 entry) {
    u16 marks;
    u8 running;
    u8 redraw;
    s32 slot;
    u8 used;
    s32 all;
    s32 i;
    ItemInfo *item;
    u8 *ids;

    marks = 0;
    running = 1;
    slot = menu_state_current->first_member;
    menu_target_panels_allocated = 0;
    ids = game_data.itemIds;
    item = &menu_state_current->tables->items[ids[row * 2 + entry]];
    redraw = 1;
    if (item->use & 0x80) {
        marks = 1;
        if (item->use & 0x20) {
            marks = menu_state_screen_parameter != 0;
        }
    }
    all = item->target & 1;
    if (marks) {
        menu_panel_open(2, 0x10, 0xe, 0x90, 0xb0, 0, 0, 4, 0);
        while (running) {
            menu_run_frame();
            if (redraw) {
                redraw = 0;
                menu_item_screen_build_list(row);
                menu_item_screen_show_description(entry, row);
                menu_item_screen_set_dimmed(1);
                menu_target_panels_build(0);
            }
            marks = 0;
            menu_state_current->markers->shown[0] = menu_state_current->markers->shown[1] = menu_state_current->markers->shown[2] = 0;
            if (all) {
                for (i = 0; i < 3; i++) {
                    if (menu_state_current->flags->party[i] != 0xff) {
                        marks |= 1 << i;
                        menu_state_current->markers->shown[i] = 1;
                    }
                }
            } else {
                marks = 1 << slot;
                menu_state_current->markers->shown[slot] = 1;
            }
            menu_state_current->flags->markers_shown = 1;
            if (game_data.itemCounts[row * 2 + entry] == 0) {
                running = 0;
            }
            if (!running) {
                break;
            }
            switch (menu_state_current->input) {
            case 4:
                used = 0;
                for (i = 0; i < 3; i++) {
                    if (menu_test_bit(marks, i) &&
                        menu_use_item_on_character(menu_state_current->tables, menu_state_current->flags->party[i], game_data.itemIds[row * 2 + entry]) == 0) {
                        used |= 1;
                    }
                }
                if (used) {
                    menu_play_sound(0x37);
                    redraw = 1;
                    if (--game_data.itemCounts[row * 2 + entry] == 0) {
                        ids = game_data.itemIds;
                        ids[row * 2 + entry] = 0;
                    }
                } else {
                    menu_play_sound(4);
                    redraw = 1;
                }
                break;
            case 5:
                running = 0;
                marks = 0;
                break;
            case 1:
                slot = menu_step_party_slot(slot, 0, 0);
                break;
            case 3:
                slot = menu_step_party_slot(slot, 1, 0);
                break;
            }
        }
        menu_state_current->markers->shown[0] = menu_state_current->markers->shown[1] = menu_state_current->markers->shown[2] = 0;
        menu_item_screen_set_dimmed(0);
        menu_state_current->flags->party_panels_shown = 0;
        menu_run_frame();
        for (i = 0; i < 3; i++) {
            heap_free(menu_state_current->party_panels[i]);
        }
        menu_target_panels_allocated = 0;
        menu_panel_close(2);
    }
    return marks;
}

/* 801DBD4C: Swap inventory entries `a` and `b` (item id and count). */
void menu_item_screen_swap_entries(s32 a, s32 b) {
    u8 tmp;

    tmp = game_data.itemIds[a];
    game_data.itemIds[a] = game_data.itemIds[b];
    game_data.itemIds[b] = tmp;
    tmp = game_data.itemCounts[a];
    game_data.itemCounts[a] = game_data.itemCounts[b];
    game_data.itemCounts[b] = tmp;
}
