/* Battle unit from 80070E2C to 800792F8: the event script module's start, the
 * battle's main loop (80070F40: set-up, turns until the end), the ATB tick
 * and the turn's start, the party panel and the HUD's drawing, the glyph and
 * quad builders, the command names' text images, the windows' primitives and
 * the battle messages, the CLUT cycle, the portraits, and the action list's
 * event queueing and entry handlers (80078508-80079270). Its rodata starts at
 * 8006FAF4, after the overlay's number, with 800745EC's jump table at 4 mod 8
 * (docs/matching.md). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/formation.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "resident/window.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/command.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/item_command.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/resolver.h"
#include "battle/scene.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "gear_menu.h"
#include "overlays.h"
#include "own_declarations.h"
#include "resident_views.h"
#include "settle.h"

/* This unit's functions, declared before their first use. */
void battle_restart_held_member_timers(void);
void battle_run_next_turn(void);
void battle_update_alive_mask(void);
void battle_panel_start_opening(s32 member);
void battle_panel_step_opening(s32 member);
void battle_init_hud_textures(void);
void battle_init_messages(void);

/* The battle's shared tables and state, which open .data (800c2048-800c348c). The
 * flags battle_in_automatic_turn and battle_applying_item_results and the
 * unreferenced object at 800c3488 are followed by stray bytes, so they stay
 * original data. */
s32 battle_unreferenced_first_data_word = 0; /* 800C2048: unreferenced */
INCLUDE_ORIGINAL(".data", battle_in_automatic_turn, 0x800C204C, 4);
INCLUDE_ORIGINAL(".data", battle_applying_item_results, 0x800C2050, 4);
s32 battle_stepped_line_end_points[2][5] = {{160, 190, 130, 100, 220}, {100, 110, 90, 80, 120}}; /* 800C2054 */
u8 battle_stepped_line_ended = 1; /* 800C207C */
s32 battle_stepped_line_progress_x = 0; /* 800C2080 */
s32 battle_stepped_line_progress_y = 0; /* 800C2084 */
/* Eight glyph ids per glyph set, by the table after them (parallel to
 * battle_command_panel_glyph_sets); nothing reads them. */
u8 battle_command_layout_character0[8] = {0x1E, 0x1C, 0x22, 0x25, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C2088 */
u8 battle_command_layout_character1[8] = {0x1E, 0x1C, 0x22, 0x24, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C2090 */
u8 battle_command_layout_character2[8] = {0x1E, 0x1C, 0x22, 0x26, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C2098 */
u8 battle_command_layout_character3[8] = {0x1E, 0x1C, 0x22, 0x24, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C20A0 */
u8 battle_command_layout_character4[8] = {0x1E, 0x1C, 0x22, 0x27, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C20A8 */
u8 battle_command_layout_character5[8] = {0x1E, 0x1C, 0x22, 0x28, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C20B0 */
u8 battle_command_layout_character6[8] = {0x1E, 0x1C, 0x22, 0x24, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C20B8 */
u8 battle_command_layout_character7[8] = {0x1E, 0x1C, 0x22, 0x2A, 0x2C, 0x1C, 0x1A, 0x20}; /* 800C20C0 */
u8 battle_command_layout_character8[8] = {0x1E, 0x1C, 0x22, 0x29, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C20C8 */
u8 battle_unused_command_layout[8] = {0x1E, 0x1C, 0x22, 0x25, 0x2C, 0x1C, 0x1A, 0x2E}; /* 800C20D0 */
u8 battle_command_layout_gear[8] = {0x1F, 0x1D, 0x23, 0x2B, 0x1F, 0x1D, 0x23, 0x2B}; /* 800C20D8 */
u8 battle_command_layout_gear_second[8] = {0x2D, 0x1D, 0x1B, 0x2F, 0x2D, 0x1D, 0x1B, 0x2F}; /* 800C20E0 */
u8 battle_command_layout_gear_character7[8] = {0x1F, 0x1D, 0x22, 0x2A, 0x1F, 0x1D, 0x22, 0x2B}; /* 800C20E8 */
u8 *battle_command_layouts_by_character[19] = { /* 800C20F0 */
    battle_command_layout_character0, battle_command_layout_character1, battle_command_layout_character2, battle_command_layout_character3, battle_command_layout_character4, battle_command_layout_character5, battle_command_layout_character6, battle_command_layout_character7,
    battle_command_layout_character8, battle_command_layout_character2, battle_command_layout_character6, battle_command_layout_character0, battle_command_layout_character0, battle_command_layout_character0, battle_command_layout_character0, battle_command_layout_character0,
    battle_command_layout_gear, battle_command_layout_gear_second, battle_command_layout_gear_character7,
};
/* The glyph sets of the command panel pages: id, x, y and shade per glyph,
 * ended by id 0xFFFF. */
GlyphEntry battle_command_panel_glyph_set_00[25] = { /* 800C213C */
    {0x4000, 128, 72, 0x2009}, {0x4001, 96, 96, 0x208B}, {0x4002, 64, 72, 0x80},
    {0x4003, 96, 48, 0x208A}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_01[25] = { /* 800C2204 */
    {0x4000, 128, 72, 0x2089}, {0x4001, 96, 96, 0x200B}, {0x4002, 64, 72, 0x80},
    {0x4003, 96, 48, 0x208A}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_02[25] = { /* 800C22CC */
    {0x4000, 128, 72, 0x2089}, {0x4001, 96, 96, 0x208B}, {0x4002, 64, 72, 0},
    {0x4003, 96, 48, 0x208A}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_03[25] = { /* 800C2394 */
    {0x4000, 128, 72, 0x2089}, {0x4001, 96, 96, 0x208B}, {0x4002, 64, 72, 0x80},
    {0x4003, 96, 48, 0x200A}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_04[25] = { /* 800C245C */
    {0x3, 214, 110, 0}, {0, 176, 144, 0x2002}, {0x1, 140, 112, 0x2001}, {0x2, 176, 72, 0x2000},
    {0xB, 192, 112, 0}, {0xC, 176, 128, 0x2002}, {0xD, 160, 112, 0x2001}, {0xE, 176, 96, 0x2000},
    {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_05[25] = { /* 800C2524 */
    {0x4004, 128, 72, 0x2005}, {0x4005, 96, 96, 0x80}, {0x4006, 64, 72, 0x2088},
    {0x4007, 96, 48, 0x2087}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_06[25] = { /* 800C25EC */
    {0x4004, 128, 72, 0x2085}, {0x4005, 96, 96, 0}, {0x4006, 64, 72, 0x2088},
    {0x4007, 96, 48, 0x2087}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_07[25] = { /* 800C26B4 */
    {0x4004, 128, 72, 0x2085}, {0x4005, 96, 96, 0x80}, {0x4006, 64, 72, 0x2008},
    {0x4007, 96, 48, 0x2087}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_08[25] = { /* 800C277C */
    {0x4004, 128, 72, 0x2085}, {0x4005, 96, 96, 0x80}, {0x4006, 64, 72, 0x2088},
    {0x4007, 96, 48, 0x2007}, {0x56, 96, 64, 0x80}, {0x54, 104, 72, 0}, {0x57, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_09[25] = { /* 800C2844 */
    {0x4008, 128, 72, 0x2009}, {0x4009, 96, 96, 0x208B}, {0x400A, 64, 72, 0x80},
    {0x400B, 96, 48, 0x208A}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_0b[25] = { /* 800C290C */
    {0x4008, 128, 72, 0x2089}, {0x4009, 96, 96, 0x200B}, {0x400A, 64, 72, 0x80},
    {0x400B, 96, 48, 0x208A}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_0c[25] = { /* 800C29D4 */
    {0x4008, 128, 72, 0x2089}, {0x4009, 96, 96, 0x208B}, {0x400A, 64, 72, 0},
    {0x400B, 96, 48, 0x208A}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_0d[25] = { /* 800C2A9C */
    {0x4008, 128, 72, 0x2089}, {0x4009, 96, 96, 0x208B}, {0x400A, 64, 72, 0x80},
    {0x400B, 96, 48, 0x200A}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_0e[25] = { /* 800C2B64 */
    {0x400C, 128, 72, 0x200D}, {0x400D, 96, 96, 0x208B}, {0x400E, 64, 72, 0x80},
    {0x400F, 96, 48, 0x2084}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_0f[25] = { /* 800C2C2C */
    {0x400C, 128, 72, 0x208D}, {0x400D, 96, 96, 0x200B}, {0x400E, 64, 72, 0x80},
    {0x400F, 96, 48, 0x2084}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_10[25] = { /* 800C2CF4 */
    {0x400C, 128, 72, 0x208D}, {0x400D, 96, 96, 0x208B}, {0x400E, 64, 72, 0},
    {0x400F, 96, 48, 0x2084}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0},
    {0x59, 88, 72, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_11[25] = { /* 800C2DBC */
    {0x400C, 128, 72, 0x208D}, {0x400D, 96, 96, 0x208B}, {0x400E, 64, 72, 0x80},
    {0x400F, 96, 48, 0x2004}, {0x5A, 96, 64, 0x80}, {0x58, 104, 72, 0}, {0x5B, 96, 80, 0}, {0xFFFF},
};
GlyphEntry battle_command_panel_glyph_set_12[25] = { /* 800C2E84 */
    {0x6, 214, 110, 0x2002}, {0x4018, 168, 89, 0x2002}, {0x4019, 174, 89, 0x2002},
    {0x401A, 180, 89, 0x2002}, {0x3, 213, 112, 0}, {0x4, 140, 112, 0x2001},
    {0x4010, 168, 50, 0x2000}, {0x4011, 174, 50, 0x2000}, {0x4012, 180, 50, 0x2000},
    {0x5, 176, 72, 0x2000}, {0x4014, 143, 70, 0x2001}, {0x4015, 149, 70, 0x2001},
    {0x4016, 155, 70, 0x2001}, {0xB, 192, 112, 0}, {0xC, 176, 128, 0x2002}, {0xD, 160, 112, 0x2001},
    {0xE, 176, 96, 0x2000}, {0xFFFF},
};
GlyphEntry *battle_command_panel_glyph_sets[19] = { /* 800C2F4C */
    battle_command_panel_glyph_set_00, battle_command_panel_glyph_set_01, battle_command_panel_glyph_set_02, battle_command_panel_glyph_set_03, battle_command_panel_glyph_set_04, battle_command_panel_glyph_set_05, battle_command_panel_glyph_set_06, battle_command_panel_glyph_set_07,
    battle_command_panel_glyph_set_08, battle_command_panel_glyph_set_09, battle_command_panel_glyph_set_09, battle_command_panel_glyph_set_0b, battle_command_panel_glyph_set_0c, battle_command_panel_glyph_set_0d, battle_command_panel_glyph_set_0e, battle_command_panel_glyph_set_0f,
    battle_command_panel_glyph_set_10, battle_command_panel_glyph_set_11, battle_command_panel_glyph_set_12,
};
/* The two glyph lists of each command panel page: the +0x641c list each
 * fills (0xFF none) and its glyph set. */
GlyphPage battle_command_panel_page_00 = {{0, 0xFF}, {0, 0xFF}}; /* 800C2F98 */
GlyphPage battle_command_panel_page_01 = {{0, 0xFF}, {0, 0xFF}}; /* 800C2F9C */
GlyphPage battle_command_panel_page_02 = {{0, 0xFF}, {0x1, 0xFF}}; /* 800C2FA0 */
GlyphPage battle_command_panel_page_03 = {{0, 0xFF}, {0x2, 0xFF}}; /* 800C2FA4 */
GlyphPage battle_command_panel_page_04 = {{0, 0xFF}, {0x3, 0xFF}}; /* 800C2FA8 */
GlyphPage battle_command_panel_page_05 = {{0x1, 0}, {0, 0x4}}; /* 800C2FAC */
GlyphPage battle_command_panel_page_06 = {{0, 0xFF}, {0x5, 0xFF}}; /* 800C2FB0 */
GlyphPage battle_command_panel_page_07 = {{0, 0xFF}, {0x5, 0xFF}}; /* 800C2FB4 */
GlyphPage battle_command_panel_page_08 = {{0, 0xFF}, {0x6, 0xFF}}; /* 800C2FB8 */
GlyphPage battle_command_panel_page_09 = {{0, 0xFF}, {0x7, 0xFF}}; /* 800C2FBC */
GlyphPage battle_command_panel_page_0a = {{0, 0xFF}, {0x8, 0xFF}}; /* 800C2FC0 */
GlyphPage battle_command_panel_page_0b = {{0, 0x1}, {0x1, 0x8}}; /* 800C2FC4 */
GlyphPage battle_command_panel_page_0c = {{0, 0x1}, {0x1, 0x8}}; /* 800C2FC8 */
GlyphPage battle_command_panel_page_0d = {{0, 0x1}, {0x1, 0x8}}; /* 800C2FCC */
GlyphPage battle_command_panel_page_0e = {{0, 0xFF}, {0x9, 0xFF}}; /* 800C2FD0 */
GlyphPage battle_command_panel_page_0f = {{0, 0xFF}, {0xA, 0xFF}}; /* 800C2FD4 */
GlyphPage battle_command_panel_page_10 = {{0, 0xFF}, {0xA, 0xFF}}; /* 800C2FD8 */
GlyphPage battle_command_panel_page_11 = {{0, 0xFF}, {0xB, 0xFF}}; /* 800C2FDC */
GlyphPage battle_command_panel_page_12 = {{0, 0xFF}, {0xC, 0xFF}}; /* 800C2FE0 */
GlyphPage battle_command_panel_page_13 = {{0, 0xFF}, {0xD, 0xFF}}; /* 800C2FE4 */
GlyphPage battle_command_panel_page_14 = {{0, 0xFF}, {0xE, 0xFF}}; /* 800C2FE8 */
GlyphPage battle_command_panel_page_15 = {{0, 0xFF}, {0xE, 0xFF}}; /* 800C2FEC */
GlyphPage battle_command_panel_page_16 = {{0, 0xFF}, {0xF, 0xFF}}; /* 800C2FF0 */
GlyphPage battle_command_panel_page_17 = {{0, 0xFF}, {0x10, 0xFF}}; /* 800C2FF4 */
GlyphPage battle_command_panel_page_18 = {{0, 0xFF}, {0x11, 0xFF}}; /* 800C2FF8 */
GlyphPage battle_command_panel_page_19 = {{0x1, 0}, {0xA, 0x12}}; /* 800C2FFC */
GlyphPage *battle_command_panel_pages[26] = { /* 800C3000 */
    &battle_command_panel_page_00, &battle_command_panel_page_01, &battle_command_panel_page_02, &battle_command_panel_page_03, &battle_command_panel_page_04, &battle_command_panel_page_05, &battle_command_panel_page_06,
    &battle_command_panel_page_07, &battle_command_panel_page_08, &battle_command_panel_page_09, &battle_command_panel_page_0a, &battle_command_panel_page_0b, &battle_command_panel_page_0c, &battle_command_panel_page_0d,
    &battle_command_panel_page_0e, &battle_command_panel_page_0f, &battle_command_panel_page_10, &battle_command_panel_page_11, &battle_command_panel_page_12, &battle_command_panel_page_13, &battle_command_panel_page_14,
    &battle_command_panel_page_15, &battle_command_panel_page_16, &battle_command_panel_page_17, &battle_command_panel_page_18, &battle_command_panel_page_19,
};
/* The party panels' glyph x per member, 24 each (the name glyphs from
 * the eighth). */
s16 battle_panel_glyph_x[72] = { /* 800C3068 */
    44, 52, 60, 68, 76, 84, 92, 52, 60, 68, 76, 84,
    60, 66, 72, 78, 84, 90, 96, 44, 50, 56, 62, 68,
    140, 148, 156, 164, 172, 180, 188, 148, 156, 164, 172, 180,
    156, 162, 168, 174, 180, 186, 192, 140, 146, 152, 158, 164,
    236, 244, 252, 260, 268, 276, 284, 244, 252, 260, 268, 276,
    252, 258, 264, 270, 276, 282, 288, 236, 242, 248, 254, 260,
};
/* The combo input patterns (seven inputs, 0xFF none, and a 0). */
u8 battle_combo_pattern_ap1[8] = {0x2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C30F8 */
u8 battle_combo_pattern_ap2[8] = {0x3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3100 */
u8 battle_combo_pattern_ap3[8] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3108 */
u8 battle_combo_pattern_ap1_ap1[8] = {0x2, 0x2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3110 */
u8 battle_combo_pattern_ap1_ap2[8] = {0x2, 0x3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3118 */
u8 battle_combo_pattern_ap2_ap1[8] = {0x3, 0x2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3120 */
u8 battle_combo_pattern_ap2_ap2[8] = {0x3, 0x3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3128 */
u8 battle_combo_pattern_ap3_ap1[8] = {0, 0x2, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3130 */
u8 battle_combo_pattern_ap1_ap1_ap1[8] = {0x2, 0x2, 0x2, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3138 */
u8 battle_combo_pattern_ap1_ap1_ap2[8] = {0x2, 0x2, 0x3, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3140 */
u8 battle_combo_pattern_ap1_ap2_ap1[8] = {0x2, 0x3, 0x2, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3148 */
u8 battle_combo_pattern_ap2_ap1_ap1[8] = {0x3, 0x2, 0x2, 0xFF, 0xFF, 0xFF, 0xFF, 0}; /* 800C3150 */
u8 battle_combo_pattern_ap1_ap1_ap1_ap1[8] = {0x2, 0x2, 0x2, 0x2, 0xFF, 0xFF, 0xFF, 0}; /* 800C3158 */
u8 *battle_combo_patterns[13] = { /* 800C3160 */
    battle_combo_pattern_ap1, battle_combo_pattern_ap2, battle_combo_pattern_ap3, battle_combo_pattern_ap1_ap1, battle_combo_pattern_ap1_ap2, battle_combo_pattern_ap2_ap1, battle_combo_pattern_ap2_ap2, battle_combo_pattern_ap3_ap1,
    battle_combo_pattern_ap1_ap1_ap1, battle_combo_pattern_ap1_ap1_ap2, battle_combo_pattern_ap1_ap2_ap1, battle_combo_pattern_ap2_ap1_ap1, battle_combo_pattern_ap1_ap1_ap1_ap1,
};
u8 battle_combo_deathblows[24] = {0, 2, 6, 1, 4, 5, 11, 12, 3, 8, 9, 10, 7, 1, 5, 12, 3, 9, 10, 7, 4, 11, 8, 0}; /* 800C3194 */
u8 *battle_combo_deathblows_by_character[16] = { /* 800C31AC */
    battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows,
    battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows, battle_combo_deathblows,
};
/* The timer reload by maximum AP (3-7) and remaining AP; battle.c reads it
 * from 800c31d4, three rows before. */
u8 battle_ap_timer_reload_table[5][8] = { /* 800C31EC */
    {0x38, 0x2E, 0x1C, 0, 0, 0, 0, 0}, {0x38, 0x32, 0x27, 0x16, 0, 0, 0, 0},
    {0x38, 0x34, 0x2C, 0x21, 0x12, 0, 0, 0}, {0x38, 0x35, 0x30, 0x28, 0x1D, 0x10, 0, 0},
    {0x38, 0x36, 0x32, 0x2C, 0x24, 0x1A, 0xE, 0},
};
/* The list separator rows by list size (3-7); battle_ai_opcodes.c reads it
 * from 800c3200 + 2. */
u8 battle_separator_rows_by_list_size[5][6] = { /* 800C3214 */
    {0x5C, 0x38, 0, 0, 0, 0}, {0x64, 0x4E, 0x2C, 0, 0, 0}, {0x68, 0x58, 0x42, 0x24, 0, 0},
    {0x6A, 0x60, 0x50, 0x3A, 0x20, 0}, {0x6C, 0x64, 0x58, 0x48, 0x34, 0x1C},
};
u16 battle_command_seal_bits[16] = { /* 800C3234 */
    0x8000, 0x4000, 0x2000, 0x1000, 0x800, 0x400, 0x200, 0x100, 0x80, 0x40, 0x20, 0x10, 0x8, 0x4,
    0x2, 0x1,
};
/* The party panels' x by layout (one to three members). */
u16 battle_panel_x_by_layout[9] = {0x60, 0, 0, 0x20, 0x40, 0, 0, 0, 0}; /* 800C3254 */
/* Three glyph label groups (ids, x, y) like the gear page's below; nothing
 * reads them. */
u8 battle_member_card_label_glyphs[18] = { /* 800C3268 */
    0x15, 0x1F, 0x1D, 0x91, 0xE, 0x3B, 0x33, 0x33, 0xE, 0x3B, 0x33, 0x33, 0x11, 0x19, 0x3E, 0xE,
    0x19, 0x3E,
};
s16 battle_member_card_label_x[18] = { /* 800C327C */
    0x90, 0x98, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0, 0xD0, 0xF8, 0x100, 0x108, 0x108, 0x38, 0x40, 0x60,
    0x38, 0x40, 0x60,
};
s16 battle_member_card_label_y[18] = { /* 800C32A0 */
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x20, 0x20, 0x20, 0x28,
    0x28, 0x28,
};
u8 battle_summary_text_glyphs[27][3] = { /* 800C32C4 */
    {0x15, 0x1, 0}, {0x28, 0x1, 0}, {0x39, 0x1, 0}, {0x28, 0x1, 0}, {0x2F, 0x1, 0}, {0x1E, 0x1, 0},
    {0x33, 0x1, 0}, {0xED, 0x1, 0}, {0x11, 0, 0}, {0x19, 0, 0}, {0x30, 0, 0}, {0x24, 0, 0},
    {0x3B, 0, 0}, {0xE, 0, 0}, {0x19, 0, 0}, {0x30, 0, 0}, {0x24, 0, 0}, {0x3B, 0, 0}, {0xDE, 0, 0},
    {0xDF, 0x1, 0x1}, {0xFF, 0x1, 0x1}, {0xE0, 0, 0}, {0xE1, 0x1, 0x1}, {0xFF, 0x1, 0x1},
    {0xE2, 0, 0}, {0xE4, 0x1, 0x1}, {0xEB, 0, 0},
};
s16 battle_summary_text_x[27] = { /* 800C3318 */
    0x38, 0x40, 0x48, 0x50, 0x58, 0x68, 0x70, 0x78, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0x88, 0x90, 0x98,
    0xA0, 0xA8, 0x58, 0x58, 0xD0, 0x58, 0x58, 0xD0, 0x58, 0x58, 0x58,
};
s16 battle_summary_text_y[27] = { /* 800C3350 */
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x90, 0x98, 0x98, 0xA0, 0xA8, 0xA8, 0xB0, 0xB8, 0xC0,
};
u8 battle_spoils_label_glyphs[7] = {0x10, 0x10, 0x1D, 0x18, 0x1D, 0xA, 0x15}; /* 800C3388 */
s16 battle_spoils_label_x[7] = {0x110, 0x110, 0xB8, 0xC0, 0xC8, 0xD0, 0xD8}; /* 800C3390 */
s16 battle_spoils_label_y[7] = {0x50, 0x60, 0x58, 0x58, 0x58, 0x58, 0x58}; /* 800C33A0 */
u8 battle_gear_hud_fixed_glyph_ids[4] = {0x9E, 0x9F, 0xDA, 0xD9}; /* 800C33B0 */
u8 battle_combo_menu_glyph_ids[16] = { /* 800C33B4 */
    0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x38, 0x71, 0x69, 0x6F, 0xC, 0x33, 0xB, 0x32, 0x40,
};
s32 battle_combo_menu_glyph_x[16] = {46, 186, 46, 186, 46, 186, 46, 186, 102, 110, 118, 300, 276, 284, 272, 38}; /* 800C33C4 */
s32 battle_combo_menu_glyph_y[16] = {106, 106, 122, 122, 138, 138, 154, 154, 56, 56, 56, 110, 74, 98, 58, 56}; /* 800C3404 */
u8 battle_paused = 0; /* 800C3444 */
u16 battle_slot_bits[16] = { /* 800C3448 */
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000,
    0x8000,
};
u16 battle_flag_bits[16] = { /* 800C3468 */
    0x8000, 0x4000, 0x2000, 0x1000, 0x800, 0x400, 0x200, 0x100, 0x80, 0x40, 0x20, 0x10, 0x8, 0x4,
    0x2, 0x1,
};
/* 00 04 77 68: no code in any image forms an address in this word
 * (tools/data_users.py --range), so its object and padding are inferred, by
 * analogy with ovl2596's battle_results_fanfare_started (a byte 0 then a stray 04): a byte
 * object followed by stray bytes. Which unit it ends is not known; the units
 * up to 8008CCCC have no other data. */
INCLUDE_ORIGINAL(".data", battle_unreferenced_stray_byte, 0x800C3488, 4);

/* 80070E2C: Start the 801e5000 module: reserve its heap span and load it. */
void battle_start_event_script_module(void) {
    s32 block;

    if (battle_uses_event_script != 0) {
        battle_cd_select_event_script_directory();
        block = battle_heap_alloc(4, 1);
        battle_heap_mark_for_event_script = block;
        battle_heap_reserve_for_event_script = battle_heap_alloc(block - 0x801E5000, 1);
        cd_read_file(1, (void *)0x801E5000, 0, 0x80);
        battle_cd_wait_for_reads();
        battle_event_script_load();
    }
}

/* 80070EB0: Forward a byte argument to the 801e5000 module when it is loaded. */
void battle_run_event_script(s32 value) {
    if (battle_uses_event_script != 0) {
        battle_event_script_run(value & 0xFF);
    }
}

/* 80070EDC: Fade the battle music out once the escape outcome is set, unless the
 * 801e5000 module handled it. */
void battle_release_script_and_fade_music(void) {
    s32 handled = 0;

    if (battle_uses_event_script != 0) {
        handled = battle_event_script_release();
    }
    if ((handled & 0xFF) == 0 && battle_area_outcome == 0x81) {
        sound_set_seq_fade((SoundSeq *)battle_music_seq, 0, 0xF0);
    }
}

/* 80070F40: The battle: allocate the battle state, load the modules and the scene,
 * run turns until an outcome or an exit is set, then settle the outcome
 * (result code in mode_result_code) and hand over to the 801de000 module. */
void battle_main(void) {
    s32 span;
    s32 block;
    u8 result;
    u8 mode = 1;
    u8 *state;
    s32 i;
    u8 outcome;

    battle_graphics = (BattleGraphics *)battle_heap_alloc(0xA2B4, 0);
    battle_ui = (BattleUi *)battle_heap_alloc(0x10C, 0);
    battle_turn_state = (TurnState *)battle_heap_alloc(0x2F8, 0);
    bzero((u_char *)battle_graphics, 0xA2B4);
    bzero((u_char *)battle_ui, 0x10C);
    bzero((u_char *)battle_turn_state, 0x2F8);
    mode_battle_debug_page = 0;
    battle_direction_input[1] = 0xFF;
    battle_direction_input[0] = 0xFF;
    battle_command_menu_sounds_enabled = 0;
    battle_music_seq = mode_music_seq;
    if (mode_pending_battle_formation != 0) {
        formation_selected_index = mode_pending_battle_formation - 1;
        if (mode_result_fanfare_started != 0) {
            mode_result_fanfare_started = 0;
            sound_set_seq_fade((SoundSeq *)mode_music_seq, 0x7F, 0x3C);
        }
        if (mode_battle_standalone == 0) {
            mode_pending_battle_formation = 0;
        }
    }
    if (mode_battle_standalone != 0) {
        cd_select_directory(0x10, 2);
        block = battle_heap_alloc(4, 1);
        span = battle_heap_alloc(block - 0x801E0000, 1);
        cd_read_file(1, (void *)0x801E0000, 0, 0x80);
        cd_sync_reads(0);
        heap_free((void *)block);
        heap_free((void *)span);
        if (mode_pending_battle_formation == 0) {
            battle_scene_select_main();
        } else {
            formation_selected_index = mode_pending_battle_formation - 1;
            mode_pending_battle_formation = 0;
        }
    }
    if (*mode_disc_mode_pointer != -1) {
        cd_select_directory(0x10, 2);
        cd_read_file(6, (void *)0x80280000, 0, 0x80);
        cd_sync_reads(0);
    }
    memmove(&formation_active, &formation_encounter_set.formations[formation_selected_index], sizeof(BattleFormation));
    battle_start_intro(mode_battle_kind);
    battle_update_alive_mask();
    battle_frame_mode = 2;
    battle_enter(battle_enemy_set_file);
    battle_screen_fade_start(0, 2, 0xFF, 0xFF, 0xFF);
    if (battle_uses_event_script == 0) {
        battle_screen_fade_start(0x14, 2, 0, 0, 0);
    }
    if (mode_battle_standalone != 0) {
        battle_music_seq = sound_create_and_play_seq(mode_music_buffer, 0x7F, 0);
    }
    battle_formation = (Formation *)mode_battle_scene_file;
    battle_area.formation = (Formation *)mode_battle_scene_file;
    battle_init_hud_textures();
    battle_atb_enabled = 1;
    battle_camera_start_move(battle_alive_mask);
    battle_wait_frame();
    battle_init_messages();
    while (battle_turns_active == 0) {
        battle_wait_frame();
    }
    heap_free(mode_battle_effect_bank);
    if (mode_battle_kind != 4) {
        sound_stop_bank_effects(mode_battle_effect_bank);
    }
    sound_remove_effect_bank(mode_battle_effect_bank);
    heap_free(mode_battle_heap_marker);
    heap_free(mode_battle_heap_reservation);
    battle_start_event_script_module();
    battle_run_event_script(1);
    if (battle_exit_requested == 0) {
        battle_frame_mode = 1;
    }
    battle_adjust_party_at_start();
    for (i = 0; i < 3; i++) {
        if (battle_party_character_ids[i] != 0x7F) {
            battle_known_skills_at_start[i].counterSkills = game_data.skills[battle_party_character_ids[i]].counterSkills;
            battle_known_skills_at_start[i].levelSkills = game_data.skills[battle_party_character_ids[i]].levelSkills;
        }
    }
    while (battle_area_outcome == 0 && battle_exit_requested == 0) {
        if (battle_turns_active != 0) {
            battle_run_next_turn();
        }
        battle_wait_frame();
    }
    state = &battle_area_outcome;
    if (!(*state & 0xC0)) {
        result = 0;
    } else if (*state & 0x40) {
        result = 1;
    } else if (battle_uses_event_script == 0) {
        result = 2;
    } else if (battle_defeat_allowed_by_event_script != 0) {
        result = 3;
        mode_result_code = result;
        *state = 1;
    }
    switch (result) {
    case 0:
        mode_result_code = 0;
        mode = 0;
    case 3:
        if (battle_uses_event_script != 0) {
            battle_state_of_event_script->vars[0] = 0xFF;
            if (battle_resume_event_script_at_end != 0 || battle_exit_requested == 0) {
                u8 *battleOutcome = &battle_area_outcome;

                battle_state_of_event_script->halted = 0;
                outcome = *battleOutcome;
                *battleOutcome = 0;
                for (i = 0; i < 3; i++) {
                    battle_state_of_event_script->vars[0x10 + i] = battle_work_area.records[i].pilot.status7C & 0x8000;
                    battle_state_of_event_script->vars[0x10 + i] |= battle_work_area.records[i].gear.status7C & 0x8000;
                }
                battle_release_wave_bank();
                battle_run_event_script(1);
                battle_area_outcome = outcome;
            }
        }
        break;
    case 1:
        mode_result_code = 2;
        mode = 2;
        break;
    case 2:
        mode_result_code = 1;
        mode = 1;
        break;
    }
    battle_stop_reads_finish_loads();
    if (battle_skip_result_screens != 0) {
        mode = 1;
    }
    cd_select_directory(0x10, 0);
    battle_heap_mark_for_post_battle_module = battle_heap_alloc(4, 1);
    battle_heap_reserve_for_post_battle_module = battle_heap_alloc(battle_heap_mark_for_post_battle_module - 0x801DE000, 1);
    cd_read_file(4, (void *)0x801DE000, 0, 0x80);
    battle_close(mode);
    while (battle_turns_active != 0) {
        battle_wait_frame();
    }
    if (battle_uses_event_script == 0 && !(battle_area_outcome & 0x40) && !(formation_active.flags & 8)) {
        battle_screen_fade_start(0x40, 2, 0x40, 0x40, 0x40);
    }
    battle_release_script_and_fade_music();
    battle_results_leave_battle();
    heap_free((void *)battle_heap_mark_for_post_battle_module);
    heap_free((void *)battle_heap_reserve_for_post_battle_module);
}

/* 800716D8: One battle frame: the 80280000 module's hook when present, then the task
 * runner. */
s32 battle_wait_frame(void) {
    if (*mode_disc_mode_pointer != -1) {
        battle_debug_print_state_page();
    }
    battle_run_frame();
    return 0;
}

/* 8007171C: One ATB tick for every present slot that is not yet ready. */
void battle_tick_atb(void) {
    s32 slot;
    s32 step;
    u8 *ready;
    u16 flags;
    u8 delay;
    s16 *toggle;
    s16 *timer;

    if (battle_atb_enabled != 0) {
        slot = 0;
        ready = battle_turn_queue.ready;
        for (; slot < 11; ready++, slot++) {
            if (battle_turn_queue.present[slot] == 0 || *ready != 0) {
                continue;
            }
            step = 1;
            if ((battle_work_area.records[slot].pilot.status84.half.active | battle_work_area.records[slot].pilot.status84.half.permanent) & 0x8000) {
                step = 2;
            }
            if (battle_work_area.records[slot].pilot.status7C & 0x1000) {
                toggle = &battle_turn_queue.timers[2][slot];
                if ((*toggle ^= 1) != 0) {
                    continue;
                }
            }
            flags = battle_work_area.records[slot].pilot.status7C;
            if (flags & 0x2000) {
                delay = battle_work_area.records[slot].statusTimers[0] -= step;
                if (delay == 0) {
                    battle_work_area.records[slot].statusTimers[0] = 0;
                    battle_work_area.records[slot].pilot.status7C &= 0xDFFF;
                }
                continue;
            }
            if ((flags & 0x80) || (battle_work_area.records[slot].pilot.status80 & 0x1000)) {
                continue;
            }
            timer = &battle_turn_queue.timers[1][slot];
            if ((*timer -= step) <= 0) {
                *ready = 1;
                *timer = 0;
            }
        }
    }
}

/* 800718BC: Reload the acting slot's turn timer and clear its ready flag. */
void battle_reload_actor_turn_timer(void) {
    u8 actor = battle_turn_state->actor;

    if (battle_turn_queue.ready[actor] != 0xFF) {
        battle_turn_queue.ready[actor] = 0;
    }
    battle_turn_queue.timers[1][battle_turn_state->actor] = battle_compute_turn_timer(battle_turn_state->actor, 0);
    battle_turn_queue.timers[0][battle_turn_state->actor] = battle_turn_queue.timers[1][battle_turn_state->actor];
}

/* 80071964: Render the pending battle message into its image, upload it and hold it
 * for three frames. */
void battle_render_pending_message(void) {
    s32 frames;

    if (battle_committed_action.message != 0 && (battle_committed_action.targets & battle_area_knocked_out) == 0) {
        frames = 3;
        battle_message_image.width = window_render_text_line(text_get_resource_entry(battle_message_table, battle_committed_action.message),
                                         battle_message_image.pixels, 0x39, 1);
        LoadImage(&battle_message_image.rect, (u_long *)battle_message_image.pixels);
        do {
            frames--;
            battle_wait_frame();
        } while (frames != 0);
    }
}

/* 80071A08: Clear the party's reaction flags. */
void battle_clear_panel_refresh_flags(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        battle_turn_state->reaction[i] = 0;
    }
}

/* 80071A38: Publish the party's reaction flags to the UI, then wait a frame. */
void battle_publish_panel_refresh_flags(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        battle_ui->reaction[i] = battle_turn_state->reaction[i];
    }
    battle_wait_frame();
}

/* 80071A8C: Hide the eight battle messages and clear two UI bytes, then wait a
 * frame. */
/* Hide the eight battle messages and close windows 4 and 5, then run a
 * frame. */
void battle_hide_messages(void) {
    s32 offset;

    /* The loop steps a byte offset through the entries. */
    for (offset = 7 * sizeof(BattleMessage); offset >= 0; offset -= sizeof(BattleMessage)) {
        ((BattleMessage *)((u8 *)battle_message_entries + offset))->shown = 0;
    }
    battle_ui->windows[5] = 0;
    battle_ui->windows[4] = 0;
    battle_wait_frame();
}

/* 80071AE0: Show the pending battle message (window 7) until a button is pressed or
 * 59 frames pass. */
void battle_show_pending_message(void) {
    s32 frames;

    if (battle_committed_action.message != 0 && (battle_committed_action.targets & battle_area_knocked_out) == 0) {
        battle_show_message_window(7);
        frames = 0x3B;
        battle_turn_state->eventsDone = 0;
        do {
            battle_wait_frame();
        } while (battle_pressed_key == 8 && --frames != 0);
        battle_turn_state->eventsDone = 1;
        battle_hide_message_window(7);
        battle_wait_frame();
    }
}

/* 80071B94: Start the turn of the acting slot (turn state actor + 1; none when 0). An
 * enemy runs its AI script (unless mode is set) and shows its name; a party
 * member gets its panel highlight (a 24x24 square) and its command menu.
 * Then each slot's default target is chosen and the turn's actions play
 * out. */
void battle_run_actor_turn(u8 mode) {
    s32 i;
    s32 offset;
    u8 actor;
    u16 flags;
    u8 *message;
    u8 *bytes;

    battle_pending_message = 1;
    message = &battle_committed_action.message;
    *message = 0;
    if (battle_turn_state->actor == 0) {
        return;
    }
    battle_reset_running_results();
    battle_clear_panel_refresh_flags();
    battle_atb_enabled = 0;
    battle_turn_state->actor--;
    battle_acting_slot = battle_turn_state->actor;
    battle_turn_state->eventCount = 0;
    battle_turn_state->eventsDone = 0;
    for (i = 4, bytes = message - 7; i >= 0; i--) {
        *bytes-- = 0;
    }
    battle_committed_action.targets = 0;
    battle_status_count_down(battle_turn_state->actor);
    if (battle_turn_state->actor >= 3) {
        if (mode == 0) {
            battle_ai_run_turn_script(battle_turn_state->actor, battle_work_area.records[battle_turn_state->actor].pilot.status80 & 0x2000);
            actor = battle_turn_state->actor;
            if (!(battle_area_slots[actor].hidden & 0x80) && !(battle_work_area.records[actor].pilot.flags34 & 0x400)) {
                battle_ui->windows[5] = 1;
                battle_message_entries[0].width = window_render_text_line(text_get_resource_entry(battle_enemy_name_table, battle_enemy_name_indices[battle_turn_state->actor - 3]),
                                                    battle_message_entries[0].pixels, 0x39, 0);
                battle_upload_image_and_wait(&battle_message_entries[0].rect, battle_message_entries[0].pixels);
                battle_message_entries[0].shown = 1;
            }
        }
        actor = battle_turn_state->actor;
        if (!(battle_work_area.records[actor].pilot.status7C & 0x2080) &&
            !(battle_work_area.records[actor].pilot.status80 & 0x1000)) {
            battle_action_list_play(actor);
            battle_hide_messages();
            battle_render_pending_message();
            battle_show_pending_message();
        }
        battle_hide_messages();
        battle_run_event_script(0);
    } else {
        /* The loop steps a byte offset through the entries. */
        for (offset = 7 * sizeof(EnemyReaction); offset >= 0; offset -= sizeof(EnemyReaction)) {
            ((EnemyReaction *)((u8 *)battle_enemy_reactions + offset))->unk1[1] = 0;
        }
        setXYWH(&battle_graphics->panel[battle_drawing_state.buffer],
                battle_turn_state->actor * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + battle_turn_state->actor] + 0x10), 8, 0x18, 0x18);
        battle_graphics->unk6414 = battle_drawing_state.buffer;
        battle_graphics->unk6415 = 1;
        actor = battle_turn_state->actor;
        if (!(battle_work_area.records[actor].pilot.status7C & 0x2080)) {
            flags = battle_work_area.records[actor].pilot.status80;
            if (!(flags & 0x1000)) {
                if (!(flags & 0x2000)) {
                    battle_command_menu_run(actor);
                } else {
                    battle_take_automatic_turn(actor);
                }
                if (battle_turn_state->unk2EA != 0) {
                    mode_battle_turn_count++;
                }
                battle_unread_open_menu_member = battle_turn_state->actor;
                battle_ui->unk97 = 0;
                battle_highlight_slots(0);
            }
        }
        battle_render_pending_message();
        battle_show_pending_message();
        battle_hide_messages();
        battle_run_event_script(0);
        for (i = 0; i < 8; i++) {
            if (battle_is_slot_in_mask(battle_committed_action.targets, i + 3)) {
                battle_enemy_reactions[i].unk1[1] = 1;
            }
        }
        battle_ai_run_targeted_scripts();
    }
    battle_publish_panel_refresh_flags();
    battle_restart_held_member_timers();
    battle_graphics->unk6415 = 0;
    for (i = 0; i < 11; i++) {
        battle_turn_state->slots[i].defaultTarget = battle_order_attack_candidates(i);
        battle_area_slots[i].targetCode = battle_is_target_at_lower_x(i, battle_turn_state->slots[i].defaultTarget);
    }
    battle_end_turn_and_preload_slot(battle_find_next_turn_slot(battle_turn_state->actor));
    battle_clear_panel_refresh_flags();
    battle_update_alive_mask();
    if (battle_area_outcome == 0) {
        battle_apply_status_drains(battle_turn_state->actor);
    }
    battle_update_alive_mask();
    battle_publish_panel_refresh_flags();
    battle_return_sprites_home();
    battle_reload_actor_turn_timer();
    battle_atb_enabled = 1;
}

/* 80072270: Party members held by the mask 800d2c9e lose their ready flag, restart
 * their turn timer from its reload value and show the held marker. */
void battle_restart_held_member_timers(void) {
    s32 member;

    if (battle_committed_action.held & 7) {
        for (member = 0; member < 3; member++) {
            if (battle_is_slot_in_mask(battle_committed_action.held, member)) {
                battle_turn_queue.ready[member] = 0;
                battle_turn_queue.timers[1][member] = battle_turn_queue.timers[0][member];
                battle_ui->reaction[member] = 1;
            }
        }
    }
    battle_committed_action.held = 0;
}

/* 80072324: Give every enemy in the act-together mask its turn: clear the event types,
 * queue action 0x17 and run the turn procedure. The cleared 0x100-byte action
 * buffer pointer is never initialised in the original. */
void battle_run_joint_turns(void) {
    s32 slot;
    s32 offset;
    u8 *p;
    u8 *end;
    u8 *actions;

    for (slot = 3; slot < 11; slot++) {
        if (battle_is_slot_in_mask(battle_joint_action_slots, slot)) {
            p = actions;
            end = p + 0x100;
            do {
                *p++ = 0;
            } while (p < end);
            for (offset = 31 * sizeof(BattleEvent); offset >= 0; offset -= sizeof(BattleEvent)) {
                ((BattleEvent *)((u8 *)battle_area_events + offset))->type = 0xFF;
            }
            battle_action_list->type = 4;
            battle_action_list->param = 0x17;
            battle_revive_slot(slot);
            battle_run_actor_turn(1);
        }
    }
}

/* 800723E0: Select the next slot to act: the forced slot, else the next ready slot in
 * the turn order from the cursor; then run the turn procedure. With slots
 * acting together, run their pass instead. The ready state 1 is kept in
 * `one`, which also steps the actor number and the cursor; stmt.c rolls the
 * found test to the loop end and enters with a jump, and with the extra
 * copies jump.c no longer duplicates that test at the loop entry. */
void battle_run_next_turn(void) {
    s32 position;
    s32 slot;
    s32 one;

    if (battle_joint_action_slots == 0) {
        if (battle_forced_next_turn != 0) {
            battle_turn_state->actor = battle_forced_next_turn;
            battle_turn_queue.ready[battle_forced_next_turn - 1] = 1;
            slot = battle_forced_next_turn;
            battle_forced_next_turn = 0;
            battle_turn_queue.timers[1][slot - 1] = 0;
        } else {
            battle_turn_state->actor = 0;
            position = battle_turn_queue.cursor;
            for (;;) {
                if (battle_turn_queue.ready[battle_turn_queue.order[position]] == (one = 1)) {
                    battle_turn_state->actor = battle_turn_queue.order[position] + one;
                    if ((battle_turn_queue.cursor = position + one) == 11) {
                        battle_turn_queue.cursor = 0;
                        break;
                    }
                }
                position++;
                if (position == 11) {
                    position = 0;
                }
                if (position == battle_turn_queue.cursor) {
                    break;
                }
            }
        }
        battle_run_actor_turn(0);
    } else {
        battle_run_joint_turns();
    }
}

/* 8007252C: Rebuild the alive mask: knocked-out slots lose their HP (gear +0x104) and
 * leave the turn order unless held by 800c3608; set the outcome when a side
 * is defeated; when every alive slot waits (+0x80 bit 0x1000), release the
 * first. Pilot and gear slots handle their own knockout/held state before
 * checking ordinary participation. The outcome is the battle area's
 * member, so the mask is reloaded after it is set. */
void battle_update_alive_mask(void) {
    s32 slot;
    s32 waiting; /* The mask remains word sized between slot-bit calls. */

    battle_alive_mask = 0;
    for (slot = 0; slot < 3; slot++) {
        if (battle_turn_queue.present[slot] != 0) {
            if (battle_work_area.records[slot].pilot.status7C & 0xC000) {
                if (battle_work_area.records[slot].pilot.status7C & 0x8000) {
                    battle_work_area.records[slot].pilot.hp = 0;
                }
                battle_turn_queue.ready[slot] = 0xFF;
            } else if (battle_area_slots[slot].hidden == 0) {
                battle_alive_mask |= battle_get_slot_bit(slot);
            }
        }
    }
    for (slot = 3; slot < 11; slot++) {
        if (battle_turn_queue.present[slot] != 0) {
            if (battle_area_slots[slot].gear == 0) {
                if (battle_work_area.records[slot].pilot.status7C & 0xC000) {
                    if (!battle_is_slot_in_mask(battle_slots_counting_while_down, slot)) {
                        battle_work_area.records[slot].pilot.hp = 0;
                        battle_turn_queue.ready[slot] = 0xFF;
                    } else {
                        battle_alive_mask |= battle_get_slot_bit(slot);
                    }
                } else {
                    if (battle_area_slots[slot].hidden == 0 && battle_enemy_reactions[slot - 3].unk3 == 0) {
                        battle_alive_mask |= battle_get_slot_bit(slot);
                    }
                }
            } else {
                if (battle_work_area.records[slot].gear.status7C & 0xC000) {
                    if (!battle_is_slot_in_mask(battle_slots_counting_while_down, slot)) {
                        battle_work_area.records[slot].gear.hp = 0;
                        battle_turn_queue.ready[slot] = 0xFF;
                    } else {
                        battle_alive_mask |= battle_get_slot_bit(slot);
                    }
                } else {
                    if (battle_area_slots[slot].hidden == 0 && battle_enemy_reactions[slot - 3].unk3 == 0) {
                        battle_alive_mask |= battle_get_slot_bit(slot);
                    }
                }
            }
        }
    }
    if (!(battle_alive_mask & 0x7F8)) {
        battle_area.outcome = 1;
    }
    if (!(battle_alive_mask & 7)) {
        battle_area.outcome = 0x81;
    }
    if (battle_area.outcome == 0) {
        waiting = battle_alive_mask;
        for (slot = 0; slot < 11; slot++) {
            if (battle_is_slot_in_mask(waiting, slot) && (battle_work_area.records[slot].pilot.status80 & 0x1000)) {
                waiting &= battle_get_other_slot_bits(slot);
            }
        }
        if (waiting == 0) {
            for (slot = 0; slot < 11; slot++) {
                if (battle_is_slot_in_mask(battle_alive_mask, slot) && (battle_work_area.records[slot].pilot.status80 & 0x1000)) {
                    battle_work_area.records[slot].pilot.status80 &= 0xEFFF;
                    return;
                }
            }
        }
    }
}

/* 800728B8: Add every other primitive from `first` to the ordering table. */
void battle_add_prims_to_ot(POLY_FT4 *prims, s32 count, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        AddPrim(battle_drawing_state.ot + 1, &prims[first + i * 2]);
    }
}

/* 80072938: Tint the current buffer's primitives from `first` to `last`: yellow for
 * mode 1, red otherwise. */
void battle_panel_tint_hp_warning(POLY_FT4 *prims, s32 first, s32 last, u8 mode) {
    s32 i;

    for (i = first; i < last; i++) {
        SetShadeTex(&prims[i * 2 + battle_drawing_state.buffer], 0);
        if (mode != 1) {
            setRGB0(&prims[i * 2 + battle_drawing_state.buffer], 0x80, 0, 0);
        } else {
            setRGB0(&prims[i * 2 + battle_drawing_state.buffer], 0x80, 0x80, 0);
        }
    }
}

/* 80072A9C: Build the member's panel HP glyphs: the current value's digits (800c3e08),
 * a '/', and the maximum's digits (800d2d54 from index 4), and tint them by
 * `mode`. */
void battle_panel_build_hp_glyphs(member, mode)
s32 member;
u8 mode;
{
    s32 i;
    s32 column;
    s32 first;

    first = battle_ui->unkE0[member];
    for (i = 0; i < 3; i++) {
        if (battle_panel_hp_digits[i] != 0xFF) {
            battle_ui->unkE0[member] +=
                battle_build_glyph(battle_panel_hp_digits[i] + 0x67, &battle_graphics->unk3A88[member][battle_ui->unkE0[member] * 2],
                              battle_panel_glyph_x[member * 24 + i] + battle_panel_x_by_layout[battle_party_panel_layout * 3 + member], 0x10);
        }
    }
    battle_ui->unkE0[member] +=
        battle_build_glyph(0x71, &battle_graphics->unk3A88[member][battle_ui->unkE0[member] * 2],
                      battle_panel_glyph_x[member * 24 + 3] + battle_panel_x_by_layout[battle_party_panel_layout * 3 + member], 0x10);
    for (i = 4, column = 4; i < 7; i++) {
        if (battle_file2_block_and_max_hp_digits[i] != 0xFF) {
            battle_ui->unkE0[member] +=
                battle_build_glyph(battle_file2_block_and_max_hp_digits[i] + 0x67, &battle_graphics->unk3A88[member][battle_ui->unkE0[member] * 2],
                              battle_panel_glyph_x[member * 24 + column] + battle_panel_x_by_layout[battle_party_panel_layout * 3 + member], 0x10);
            column++;
        }
    }
    if (mode != 0) {
        battle_panel_tint_hp_warning(battle_graphics->unk3A88[member], first, battle_ui->unkE0[member], mode);
    }
}

/* 80072DA8: Build the member's panel name glyphs (800d2d88) and tint them by `mode`. */
void battle_panel_build_gear_hp_glyphs(member, mode)
s32 member;
u8 mode;
{
    s32 i;
    s32 first;

    i = 0;
    first = battle_ui->unkE0[member];
    for (; i < 5; i++) {
        if (battle_panel_gear_hp_digits[i] != 0xFF) {
            battle_ui->unkE0[member] +=
                battle_build_glyph(battle_panel_gear_hp_digits[i] + 0x67, &battle_graphics->unk3A88[member][battle_ui->unkE0[member] * 2],
                              battle_panel_glyph_x[member * 24 + 7 + i] + battle_panel_x_by_layout[battle_party_panel_layout * 3 + member], 0x10);
        }
    }
    if (mode != 0) {
        battle_panel_tint_hp_warning(battle_graphics->unk3A88[member], first, battle_ui->unkE0[member], mode);
    }
}

/* 80072F38: Split the member's panel values into digit glyph codes: HP (3 digits) and
 * maximum HP (3), or in a gear its HP (5); leading zeros become blanks (0xff).
 * Returns the warning level: 2 at an eighth of the maximum or less, 1 at a
 * quarter or less, else 0. Each digit is stored as soon as it is split
 * off; the leading-zero loops index the digit arrays. */
u8 battle_panel_split_hp_digits(s32 member, u8 inGear) {
    u8 warning;
    s32 i;
    s16 hp100;
    s16 hp10;
    s16 max100;
    s16 max10;
    s32 gear10000;
    s32 gear1000;
    s32 gear100;
    s32 gear10;

    battle_panel_max_hp_remainder = battle_panel_max_hp = battle_work_area.records[member].pilot.maxHp;
    battle_panel_hp_remainder = battle_panel_hp = battle_work_area.records[member].pilot.hp;
    battle_panel_gear_hp_remainder = battle_panel_gear_hp = battle_work_area.records[member].gear.hp;
    battle_panel_gear_max_hp = battle_work_area.records[member].gear.maxHp;
    warning = 0;
    if (inGear) {
        if (battle_panel_gear_max_hp / 8 >= battle_panel_gear_hp) {
            warning = 2;
        } else if (battle_panel_gear_max_hp / 4 >= battle_panel_gear_hp) {
            warning = 1;
        }
    } else {
        if (battle_panel_max_hp / 8 >= battle_panel_hp) {
            warning = 2;
        } else if (battle_panel_max_hp / 4 >= battle_panel_hp) {
            warning = 1;
        }
    }
    hp100 = battle_panel_hp_remainder / 100;
    battle_panel_hp_remainder -= hp100 * 100;
    battle_panel_hp_digits[0] = hp100;
    hp10 = battle_panel_hp_remainder / 10;
    battle_panel_hp_remainder -= hp10 * 10;
    battle_panel_hp_digits[1] = hp10;
    battle_panel_hp_digits[2] = battle_panel_hp_remainder;
    max100 = battle_panel_max_hp_remainder / 100;
    battle_panel_max_hp_remainder -= max100 * 100;
    battle_file2_block_and_max_hp_digits[4] = max100;
    max10 = battle_panel_max_hp_remainder / 10;
    battle_panel_max_hp_remainder -= max10 * 10;
    battle_file2_block_and_max_hp_digits[5] = max10;
    battle_file2_block_and_max_hp_digits[6] = battle_panel_max_hp_remainder;
    gear10000 = battle_panel_gear_hp_remainder / 10000;
    battle_panel_gear_hp_remainder -= gear10000 * 10000;
    battle_panel_gear_hp_digits[0] = gear10000;
    gear1000 = battle_panel_gear_hp_remainder / 1000;
    battle_panel_gear_hp_remainder -= gear1000 * 1000;
    battle_panel_gear_hp_digits[1] = gear1000;
    gear100 = battle_panel_gear_hp_remainder / 100;
    battle_panel_gear_hp_remainder -= gear100 * 100;
    battle_panel_gear_hp_digits[2] = gear100;
    gear10 = battle_panel_gear_hp_remainder / 10;
    battle_panel_gear_hp_remainder -= gear10 * 10;
    battle_panel_gear_hp_digits[3] = gear10;
    battle_panel_gear_hp_digits[4] = battle_panel_gear_hp_remainder;
    for (i = 0; i < 2; i++) {
        if (battle_panel_hp_digits[i] != 0) {
            break;
        }
        battle_panel_hp_digits[i] = 0xFF;
    }
    for (i = 0; i < 2; i++) {
        if (battle_file2_block_and_max_hp_digits[i + 4] != 0) {
            break;
        }
        battle_file2_block_and_max_hp_digits[i + 4] = 0xFF;
    }
    for (i = 0; i < 4; i++) {
        if (battle_panel_gear_hp_digits[i] != 0) {
            break;
        }
        battle_panel_gear_hp_digits[i] = 0xFF;
    }
    return warning;
}

/* 80073380: Build the member's four-digit panel value (the 800d32a0 value or record
 * +0xdc) as glyphs. */
void battle_panel_build_ap_or_fuel_glyphs(s32 member) {
    s16 value;
    s32 i;
    u8 digit;

    if (battle_graphics->panels[member].state == 1) {
        value = battle_slot_flags[member].unk0;
    } else {
        value = battle_work_area.records[member].gear.fuel;
    }
    battle_split_decimal_digits(value);
    for (i = 0; i < 4; i++) {
        digit = battle_decimal_digits[i + 5];
        if (digit != 0xFF) {
            battle_ui->unkEC[member] +=
                battle_build_glyph(digit + 0x83, &battle_graphics->unk6008[member][battle_ui->unkEC[member] * 2],
                              member * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + member] + 0x4A) + i * 6, 0x25);
        }
    }
    battle_ui->unk99[member] = battle_drawing_state.buffer;
}

/* 80073538: Build each present member's status glyphs by its panel state (0: the
 * status label, 1: the party-wide label, 2/3: the cursor window), and
 * record the draw buffer they were built for. In state 3 each outcome is
 * written out, so both stores address the panel arrays from the switch's
 * battle_ui + member (cross-jumping later merges the two identical
 * stores). The first new part index of the label (`start`, set once) is
 * doubled just before its glyph call. */
void battle_panel_build_status_glyphs(void) {
    s32 member;
    s32 first;
    s32 part;
    s32 start;

    battle_ui->statusParts[3] = 0;
    for (member = 0; member < 3; member++) {
        if (battle_area_slots[member].field2 == 0x7F) {
            continue;
        }
        switch (battle_ui->unk90[member]) {
        case 0:
            battle_ui->statusParts[member] = 0;
            battle_ui->statusParts[member] += battle_build_glyph(
                0x8F, battle_graphics->status[member][battle_ui->statusParts[member]],
                member * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + member] + 0x44), 0x1F);
            battle_ui->statusParts[member] += battle_build_glyph_half_scale(
                0x52, battle_graphics->status[member][battle_ui->statusParts[member]],
                member * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + member] + 0x48), 0x1C);
            first = battle_ui->statusParts[member];
            start = first * 2;
            battle_ui->statusParts[member] += battle_build_glyph_half_scale(
                0x53, battle_graphics->status[member][first],
                member * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + member] + 0x48), 0x1C);
            for (part = start; part < battle_ui->statusParts[member] * 2; part += 2) {
                battle_quad_init_half_subtractive(&battle_graphics->status[member][0][part + battle_drawing_state.buffer]);
            }
            battle_ui->statusBuffer[member] = battle_drawing_state.buffer;
            battle_ui->unkCC[member] = 1;
            break;
        case 1:
            battle_ui->statusParts[3] = sprite_sheet_draw_rotated(battle_glyph_table, 0x52, battle_graphics->status[3][0], battle_drawing_state.buffer,
                                            0x10, 0x98, 0x1000, 0x1000, 0xC00);
            first = battle_ui->statusParts[3];
            start = first * 2;
            battle_ui->statusParts[3] += sprite_sheet_draw_rotated(battle_glyph_table, 0x53, battle_graphics->status[3][first],
                                             battle_drawing_state.buffer, 0x10, 0x98, 0x1000, 0x1000, 0xC00);
            for (part = start; part < battle_ui->statusParts[3] * 2; part += 2) {
                battle_quad_init_half_subtractive(&battle_graphics->status[3][0][part + battle_drawing_state.buffer]);
            }
            battle_ui->statusBuffer[3] = battle_drawing_state.buffer;
            break;
        case 3:
            if (battle_slot_flags[member].unk1 != 0) {
                if (battle_party_character_ids[member] != 7) {
                    battle_ui->unkCC[member] = 0;
                } else {
                    battle_ui->statusParts[member] = 0;
                }
            } else {
                battle_ui->statusParts[member] = 0;
            }
            battle_panel_start_opening(member);
            /* fallthrough */
        case 2:
            battle_panel_step_opening(member);
            if (battle_slot_flags[member].unk1 == 0 || battle_party_character_ids[member] == 7) {
                battle_ui->barShown[member] = 0;
            }
            break;
        }
    }
}

/* 80073A58: When enabled, shade the current flat quad at graphics +0x63c8 grey by
 * +0x6410 and add it with its draw mode to the ordering table. */
void battle_panel_draw_highlight(void) {
    if (battle_graphics->unk6415 != 0) {
        setRGB0(&battle_graphics->panel[battle_graphics->unk6414], battle_graphics->panelAlpha, battle_graphics->panelAlpha,
                battle_graphics->panelAlpha);
        AddPrim(battle_drawing_state.ot + 1, &battle_graphics->panel[battle_graphics->unk6414]);
        AddPrim(battle_drawing_state.ot + 1, &battle_graphics->panelMode[battle_graphics->unk6414]);
    }
}

/* 80073B64: Draw the party panel: the list separator lines, the status glyphs, each
 * member's panel value and digits, the command portrait and name glyphs,
 * and each present member's portrait, gauge and gauge shade. */
void battle_panel_draw(void) {
    s32 i;

    for (i = 0; i < battle_ui->unk97; i++) {
        AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unk908[battle_ui->unk98 + i * 2]);
    }
    for (i = 0; i < 4; i++) {
        battle_add_prims_to_ot(battle_graphics->status[i][0], battle_ui->statusParts[i], battle_ui->statusBuffer[i]);
    }
    for (i = 0; i < 3; i++) {
        if (battle_ui->unkCC[i] != 0) {
            switch (battle_graphics->panels[i].state) {
            case 1:
                battle_add_prims_to_ot(battle_graphics->panels[i].value[0], battle_graphics->panels[i].parts[0],
                              battle_graphics->panels[i].buffer);
                break;
            case 2:
                battle_add_prims_to_ot(battle_graphics->panels[i].gear[0], battle_graphics->panels[i].parts[1],
                              battle_graphics->panels[i].buffer);
                break;
            }
            battle_add_prims_to_ot(battle_graphics->unk6008[i], battle_ui->unkEC[i], battle_ui->unk99[i]);
        }
    }
    if (battle_ui->unkAF != 0) {
        battle_add_prims_to_ot(battle_graphics->unk9C8[0], battle_ui->unk7B, battle_ui->unkA4);
    }
    for (i = 0; i < 3; i++) {
        battle_add_prims_to_ot(battle_graphics->unk3A88[i], battle_ui->unkE0[i], battle_ui->unk93[i]);
    }
    for (i = 0; i < 3; i++) {
        if (battle_area_slots[i].field2 != 0x7F) {
            battle_add_prims_to_ot(battle_graphics->portrait[i], 1, battle_ui->portraitBuffer);
            if (battle_ui->unkCC[i] != 0) {
                battle_add_prims_to_ot(battle_graphics->gauge[i][0], battle_ui->gaugeParts[i], battle_ui->gaugeBuffer);
                AddPrim(battle_drawing_state.ot + 1, &battle_graphics->shade[i * 2 + battle_drawing_state.buffer]);
            }
        }
    }
}

/* 80073E88: Draw both 100-primitive lists at graphics +0x641c when UI +0xcb is set. */
void battle_command_panel_draw(void) {
    s32 i;

    if (battle_ui->unkCB != 0) {
        for (i = 0; i < 2; i++) {
            battle_add_prims_to_ot(battle_graphics->unk641C[i], battle_ui->unkD0[i], battle_ui->unkA3);
        }
    }
}

/* 80073F08: Draw the three primitive lists the UI enables at +0x9c..+0x9e. */
void battle_draw_list_page_glyphs(void) {
    if (battle_ui->unk9C != 0) {
        battle_add_prims_to_ot(battle_graphics->unkBA8, battle_ui->unkF8, battle_ui->unkA5);
    }
    if (battle_ui->cursorShown != 0) {
        battle_add_prims_to_ot(battle_graphics->cursor, battle_ui->cursorParts, battle_ui->cursorBuffer);
    }
    if (battle_ui->unk9D != 0) {
        battle_add_prims_to_ot(battle_graphics->unk1E68, battle_ui->unkFC, battle_ui->unkA6);
    }
}

/* 80073FB8: Add the panel draw modes, the shown gauge bars (while the battle runs)
 * and every shown window's fill, frame, background and draw mode. */
void battle_draw_gauge_bars_and_windows(void) {
    s32 i;
    WindowBlock *window;

    AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unk8920[battle_drawing_state.buffer]);
    for (i = 0; i < 4; i++) {
        if (battle_ui->barShown[i] != 0 && battle_frame_mode == 1) {
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->gaugeBars[i * 2 + battle_ui->statusBuffer[i]]);
        }
    }
    AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unk8908[battle_drawing_state.buffer]);
    for (i = 0; i < 7; i++) {
        if (battle_ui->windows[i] != 0) {
            window = (WindowBlock *)battle_window_blocks[i];
            battle_add_prims_to_ot(window->corners, window->cornerCount, window->buffer);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[2][window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[2][2 + window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[3][window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[3][2 + window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[0][window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[0][2 + window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[1][window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->frame[1][2 + window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->shade[window->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &window->mode[window->buffer]);
        }
    }
}

/* 800742A0: Redraw the party markers flagged in UI +0x7c. */
void battle_panel_rebuild_flagged_members(void) {
    s32 member;
    u8 value;

    for (member = 0; member < 3; member++) {
        if (battle_ui->reaction[member] != 0) {
            battle_ui->unkE0[member] = 0;
            battle_ui->unkEC[member] = 0;
            if (battle_area_slots[member].field2 != 0x7F) {
                battle_panel_build_ap_or_fuel_glyphs(member);
                value = battle_panel_split_hp_digits(member, battle_slot_flags[member].unk1);
                if (battle_slot_flags[member].unk1 != 0) {
                    battle_panel_build_gear_hp_glyphs(member, value);
                } else {
                    battle_panel_build_hp_glyphs(member, value);
                }
                battle_ui->unk93[member] = battle_drawing_state.buffer;
                battle_ui->reaction[member] = 0;
            }
        }
    }
}

/* 800743A4: When enabled, add the graphics block's four current quads to the ordering
 * table. */
void battle_draw_list_page_image(void) {
    if (battle_graphics->unkA230->unk669 != 0) {
        AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk0[battle_graphics->unkA230->unk668]);
        AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk50[battle_graphics->unkA230->unk668]);
        AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unkA0[battle_graphics->unkA230->unk668]);
        AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unkF0[battle_graphics->unkA230->unk668]);
    }
}

/* 800744BC: Add the graphics block's current +0x320 and +0x370 quads to the ordering
 * table. */
void battle_draw_list_page_icons(void) {
    AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk320[battle_graphics->unkA230->buffer]);
    AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk370[battle_graphics->unkA230->buffer]);
}

/* 80074554: Add the graphics block's current +0x280 and +0x2d0 quads to the ordering
 * table. */
void battle_draw_list_page_text(void) {
    AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk280[battle_graphics->unkA230->buffer]);
    AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk2D0[battle_graphics->unkA230->buffer]);
}

/* 800745EC: Add the command window page's quads to the ordering table: the page
 * frame (800743a4), then per page (800d2d28 +0xb7) its EP digits, icons,
 * title and cost digits, or its gradient box. */
void battle_draw_list_page(void) {
    switch (battle_ui->unkB7) {
    case 1:
        battle_draw_list_page_image();
        if (battle_graphics->unkA230->unk669) {
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk410[battle_graphics->unkA230->unk66C]);
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk460[battle_graphics->unkA230->unk66C]);
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk4B0[battle_graphics->unkA230->unk66C]);
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk500[battle_graphics->unkA230->unk66C]);
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk550[battle_graphics->unkA230->unk66C]);
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk5A0[battle_graphics->unkA230->unk66C]);
        }
        if (battle_graphics->unkA230->unk66B) {
            if (battle_graphics->unkA230->unk66E) {
                AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk3C0[battle_graphics->unkA230->unk66C]);
            }
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk140[battle_graphics->unkA230->buffer]);
            battle_add_prims_to_ot(battle_graphics->unkA230->unk190, battle_graphics->unkA230->unk66E, battle_graphics->unkA230->buffer);
            battle_draw_list_page_text();
            battle_draw_list_page_icons();
        }
        break;
    case 2:
        battle_draw_list_page_image();
        if (battle_graphics->unkA230->unk66B) {
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk140[battle_graphics->unkA230->buffer]);
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk190[battle_graphics->unkA230->buffer]);
        }
        if (battle_graphics->unkA230->unk66D) {
            battle_draw_list_page_icons();
            battle_draw_list_page_text();
        }
        break;
    case 3:
        battle_draw_list_page_image();
        if (battle_graphics->unkA230->unk66F) {
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk5F0[battle_graphics->unkA230->unk66C]);
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk638[battle_graphics->unkA230->unk66C]);
        }
        break;
    case 4:
        battle_draw_list_page_image();
        if (battle_graphics->unkA230->unk66B) {
            if (battle_graphics->unkA230->unk66E) {
                AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk3C0[battle_graphics->unkA230->unk66C]);
            }
            AddPrim(battle_drawing_state.ot + 1, &battle_graphics->unkA230->unk140[battle_graphics->unkA230->buffer]);
            battle_draw_list_page_text();
            battle_draw_list_page_icons();
        }
        break;
    case 5:
        battle_draw_list_page_image();
        break;
    }
}

/* 80074AB8: Draw the *800d2db4 primitive lists the UI enables: the target lists
 * (UI +0xc7) and the menu lists (UI +0xad), one of which blinks: shown for
 * 15 of every 21 frames. */
void battle_draw_hud_lists(void) {
    if (battle_ui->unkC7 != 0) {
        battle_add_prims_to_ot(battle_hud_primitive_lists->list2, battle_hud_primitive_lists->counts[2], battle_hud_primitive_lists->buffers[2]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list7, battle_hud_primitive_lists->counts[7], battle_hud_primitive_lists->buffers[7]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list8, battle_hud_primitive_lists->counts[8], battle_hud_primitive_lists->buffers[8]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list10, battle_hud_primitive_lists->counts[10], battle_hud_primitive_lists->buffers[10]);
    }
    if (battle_ui->unkAD != 0) {
        battle_add_prims_to_ot(battle_hud_primitive_lists->unk46A0, 20, battle_hud_primitive_lists->buffer46A0);
        battle_add_prims_to_ot(battle_hud_primitive_lists->extra0, battle_hud_primitive_lists->extraCounts[0], battle_hud_primitive_lists->extraBuffers[0]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->extra4, battle_hud_primitive_lists->extraCounts[4], battle_hud_primitive_lists->extraBuffer4);
        if (++battle_hud_primitive_lists->blink < 15) {
            battle_add_prims_to_ot(battle_hud_primitive_lists->unk4CE0, battle_hud_primitive_lists->count4CE0, battle_hud_primitive_lists->buffer4CE0);
        } else if (battle_hud_primitive_lists->blink >= 21) {
            battle_hud_primitive_lists->blink = 0;
        }
        battle_add_prims_to_ot(battle_hud_primitive_lists->list0, battle_hud_primitive_lists->counts[0], battle_hud_primitive_lists->buffers[0]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list1, battle_hud_primitive_lists->counts[1], battle_hud_primitive_lists->buffers[1]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list3, battle_hud_primitive_lists->counts[3], battle_hud_primitive_lists->buffers[3]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list4, battle_hud_primitive_lists->counts[4], battle_hud_primitive_lists->buffers[4]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list5, battle_hud_primitive_lists->counts[5], battle_hud_primitive_lists->buffers[5]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list6, battle_hud_primitive_lists->counts[6], battle_hud_primitive_lists->buffers[6]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list9, battle_hud_primitive_lists->counts[9], battle_hud_primitive_lists->buffers[9]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->unk32A0, battle_hud_primitive_lists->count32A0, battle_hud_primitive_lists->buffer32A0);
        battle_add_prims_to_ot(battle_hud_primitive_lists->extra1, battle_hud_primitive_lists->extraCounts[1], battle_hud_primitive_lists->extraBuffers[1]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->extra2, battle_hud_primitive_lists->extraCounts[2], battle_hud_primitive_lists->extraBuffers[2]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->extra3, battle_hud_primitive_lists->extraCounts[3], battle_hud_primitive_lists->extraBuffers[3]);
    }
}

/* 80074D4C: While a target is being chosen, pulse the direction arrows (red between
 * 0x40 and 0xfc) and draw the ones pointing at targets. The block is
 * addressed as its leading primitive array. */
void battle_draw_direction_arrows(void) {
    s32 i;

    if (battle_ui->unkC6 != 0) {
        if (battle_direction_arrows->fading) {
            battle_direction_arrows->shade -= 4;
            if (battle_direction_arrows->shade < 0x40) {
                battle_direction_arrows->fading = 0;
                battle_direction_arrows->shade = 0x40;
            }
        } else {
            battle_direction_arrows->shade += 4;
            if (battle_direction_arrows->shade >= 0x100) {
                battle_direction_arrows->fading = 1;
                battle_direction_arrows->shade = 0xFC;
            }
        }
        for (i = 0; i < 4; i++) {
            if (battle_direction_arrows->arrows[i]) {
                ((POLY_G3 *)battle_direction_arrows)[i * 2 + battle_direction_arrows->buffer].r0 = battle_direction_arrows->shade;
                ((POLY_G3 *)battle_direction_arrows)[i * 2 + battle_direction_arrows->buffer].g0 = 0;
                ((POLY_G3 *)battle_direction_arrows)[i * 2 + battle_direction_arrows->buffer].b0 = 0;
                AddPrim(battle_drawing_state.ot + 1, &((POLY_G3 *)battle_direction_arrows)[i * 2 + battle_direction_arrows->buffer]);
            }
        }
    }
}

/* 80074EEC: Draw the three primitive lists of *800d2db4 when UI +0xa8 is set. */
void battle_draw_combo_chain_lists(void) {
    if (battle_ui->unkA8 != 0) {
        battle_add_prims_to_ot(battle_hud_primitive_lists->list11, battle_hud_primitive_lists->counts[11], battle_hud_primitive_lists->buffers[11]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list12, battle_hud_primitive_lists->counts[12], battle_hud_primitive_lists->buffers[12]);
        battle_add_prims_to_ot(battle_hud_primitive_lists->list13, battle_hud_primitive_lists->counts[13], battle_hud_primitive_lists->buffers[13]);
    }
}

/* 80074F70: Add the event script's current portrait quad (UI +0xc8) and draw the
 * message text window 800d2dac (UI +0xc9). */
void battle_draw_script_portrait_and_text(void) {
    if (battle_ui->scriptPortraitShown != 0) {
        AddPrim(battle_drawing_state.ot + 1, &battle_state_of_event_script->quads[battle_state_of_event_script->portraitBuffer]);
    }
    if (battle_ui->messageShown != 0) {
        window_draw_frame(battle_message_text_window, (u_long *)battle_drawing_state.ot + 1, battle_drawing_state.buffer);
    }
}

/* 8007500C: Place and add the shown battle messages: the first centred at (0x40,
 * 0x2c), the others centred at (0x9a, 0xca), texture rows 13 apart per
 * pair. */
void battle_draw_messages(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (battle_message_entries[i].shown != 0) {
            if (i == 0) {
                battle_quad_place_text_row(&battle_message_entries[i].prims[battle_drawing_state.buffer], 0x40 - (battle_message_entries[i].width >> 1), 0x2C, 0, 0,
                              battle_message_entries[i].width);
            } else {
                battle_quad_place_text_row(&battle_message_entries[i].prims[battle_drawing_state.buffer], 0x9A - (battle_message_entries[i].width >> 1), 0xCA, 0,
                              (i / 2) * 13, battle_message_entries[i].width);
            }
            AddPrim(battle_drawing_state.ot + 1, &battle_message_entries[i].prims[battle_drawing_state.buffer]);
        }
    }
}

/* 80075168: Size and colour each member's gauge shade in the current draw buffer:
 * panel state 1 shows the 800d32a0 value (green, two pixels per point),
 * state 2 the gear's fuel over 56 pixels (blue, yellow below a quarter,
 * pale red below an eighth). */
void battle_panel_update_gauge_shades(void) {
    s32 i;
    u16 fuel;
    u16 maxFuel;
    s32 width;

    for (i = 0; i < 3; i++) {
        switch (battle_graphics->panels[i].state) {
        case 1:
            setXY4(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer],
                   i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x28), 0x22,
                   i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x28) + battle_slot_flags[i].unk0 * 2, 0x22,
                   i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x28), 0x26,
                   i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x28) + battle_slot_flags[i].unk0 * 2, 0x26);
            setRGB0(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0, 0xFF, 0);
            setRGB1(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0, 0xFF, 0);
            break;
        case 2:
            fuel = battle_work_area.records[i].gear.fuel;
            maxFuel = battle_work_area.records[i].gear.maxFuel;
            width = (u32)(fuel * 100) / maxFuel * 5600 / 10000;
            setXY4(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer],
                   i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x28), 0x22,
                   i * 0x60 + 0x28 + battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + width, 0x22,
                   i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x28), 0x26,
                   i * 0x60 + 0x28 + battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + width, 0x26);
            if (fuel >= maxFuel >> 2) {
                setRGB0(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0, 0, 0xFF);
                setRGB1(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0, 0, 0xFF);
            } else if (fuel >= maxFuel >> 3) {
                setRGB0(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0xFF, 0xFF, 0);
                setRGB1(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0xFF, 0xFF, 0);
            } else {
                setRGB0(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0xFF, 0x7F, 0x7F);
                setRGB1(&battle_graphics->shade[i * 2 + battle_drawing_state.buffer], 0xFF, 0x7F, 0x7F);
            }
            break;
        }
    }
}

/* Texture u of the far end of a gauge bar `width` texels long: the bar
 * sprite's page x plus the width, in 4-bit texels (two per byte column).
 * The width is read whole into its own temporary (not narrowed to the
 * stored byte). */
#define GAUGE_BAR_U(width) ({ s32 width_ = (width); (width_ + ((u8)battle_graphics->sprites[0].pageX & 0x3F)) * 2; })

/* 80075938: Update the gauge shades, then each present member's time bar in the
 * current draw buffer: filled by the turn counter over 56 pixels, coloured
 * for haste or slow. With the member's panel open (state != 0) the bar is
 * the upright party-wide one instead, showing the fuel of a member in a
 * gear. */
void battle_panel_update_time_bars(void) {
    s32 widths[3];
    s32 i;
    u16 clut;

    battle_panel_update_gauge_shades();
    battle_ui->barShown[3] = 0;
    for (i = 0; i < 3; i++) {
        if (battle_area_slots[i].field2 != 0x7F) {
            if (widths[i] < 0) {
                widths[i] = 0;
            }
            if (battle_ui->unk90[i] == 0) {
                widths[i] = (100 - battle_turn_queue.timers[1][i] * 100 / battle_turn_queue.timers[0][i]) * 5600 / 10000;
                setXY4(&battle_graphics->gaugeBars[i * 2 + battle_drawing_state.buffer],
                       battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + i * 0x60 + 0x2D, 0x1A,
                       i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x2C) + ((u16)widths[i] + 1), 0x1A,
                       battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + i * 0x60 + 0x2D, 0x1E,
                       i * 0x60 + (battle_panel_x_by_layout[battle_party_panel_layout * 3 + i] + 0x2C) + ((u16)widths[i] + 1), 0x1E);
                setUV4(&battle_graphics->gaugeBars[i * 2 + battle_drawing_state.buffer],
                       (battle_graphics->sprites[0].pageX & 0x3F) << 1, battle_graphics->sprites[0].pageY,
                       GAUGE_BAR_U(widths[i]), battle_graphics->sprites[0].pageY,
                       (battle_graphics->sprites[0].pageX & 0x3F) << 1, battle_graphics->sprites[0].pageY + 4,
                       GAUGE_BAR_U(widths[i]), battle_graphics->sprites[0].pageY + 4);
                if ((battle_work_area.records[i].pilot.status84.half.active |
                     battle_work_area.records[i].pilot.status84.half.permanent) & 0x8000) {
                    clut = battle_graphics->barCluts[3];
                } else if (battle_work_area.records[i].pilot.status7C & 0x1000) {
                    clut = battle_graphics->barCluts[2];
                } else {
                    clut = battle_graphics->barCluts[0];
                }
                battle_graphics->gaugeBars[i * 2 + battle_drawing_state.buffer].clut = clut;
                battle_ui->barShown[i] = 1;
            } else {
                widths[i] = (100 - battle_turn_queue.timers[1][i]) * 5600 / 10000;
                if (battle_slot_flags[i].unk1 != 0 && battle_party_character_ids[i] != 7) {
                    widths[i] = battle_work_area.records[i].gear.fuel * 100 / battle_work_area.records[i].gear.maxFuel * 5600 /
                                10000;
                }
                setXY4(&battle_graphics->gaugeBars[6 + battle_drawing_state.buffer],
                       0xC, 0xCE,
                       0xC, 0xCE - widths[i] * 2,
                       0x14, 0xCE,
                       0x14, 0xCE - widths[i] * 2);
                setUV4(&battle_graphics->gaugeBars[6 + battle_drawing_state.buffer],
                       (battle_graphics->sprites[0].pageX & 0x3F) << 1, battle_graphics->sprites[0].pageY,
                       GAUGE_BAR_U(widths[i]), battle_graphics->sprites[0].pageY,
                       (battle_graphics->sprites[0].pageX & 0x3F) << 1, battle_graphics->sprites[0].pageY + 4,
                       GAUGE_BAR_U(widths[i]), battle_graphics->sprites[0].pageY + 4);
                battle_graphics->gaugeBars[6 + battle_drawing_state.buffer].clut = battle_graphics->barCluts[1];
                if (battle_ui->unkCB != 0) {
                    battle_ui->barShown[3] = 1;
                }
            }
        } else {
            battle_ui->barShown[i] = 0;
        }
    }
}

/* 80076418: The HUD of frame mode 1 (the turns), unless battle_turn_hud_hidden
 * (800c492a): the opening windows, the member panels, the command panel,
 * the list page and its glyphs, the messages, the combo chain lists, the
 * direction arrows, the gauge bars and windows, the stepped line and the HUD
 * lists. */
void battle_draw_hud_for_turns(void) {
    if (battle_turn_hud_hidden == 0) {
        battle_window_grow_opening();
        battle_panel_rebuild_flagged_members();
        battle_panel_update_time_bars();
        battle_panel_build_status_glyphs();
        battle_panel_draw_highlight();
        battle_panel_draw();
        battle_command_panel_draw();
        battle_draw_list_page_glyphs();
        battle_draw_messages();
        battle_draw_combo_chain_lists();
        battle_draw_list_page();
        battle_draw_direction_arrows();
        battle_draw_gauge_bars_and_windows();
        battle_stepped_line_draw();
        battle_draw_hud_lists();
    }
}

/* 800764B4: The HUD of frame mode 0 (the result screens): clear the debug page, grow
 * the opening windows, queue the result screens' primitives (ovl2596) and
 * draw the gauge bars and windows. */
void battle_draw_hud_for_result_screens(void) {
    mode_battle_debug_page = 0;
    battle_window_grow_opening();
    battle_results_queue_screens();
    battle_draw_gauge_bars_and_windows();
}

/* 800764EC: The HUD of frame mode 2 (the event script): the opening windows, the
 * status and list page glyphs, the messages, the script's portrait and text
 * window, the gauge bars and windows, the stepped line and the HUD lists. */
void battle_draw_hud_for_event_script(void) {
    battle_window_grow_opening();
    battle_panel_build_status_glyphs();
    battle_draw_list_page_glyphs();
    battle_draw_messages();
    battle_draw_script_portrait_and_text();
    battle_draw_gauge_bars_and_windows();
    battle_stepped_line_draw();
    battle_draw_hud_lists();
}

/* 80076544: Draw the HUD of battle_frame_mode (800c3e4c): the result screens (0),
 * the turns (1) or the event script (2). */
void battle_draw_hud(void) {
    switch (battle_frame_mode) {
    case 0:
        battle_draw_hud_for_result_screens();
        break;
    case 1:
        battle_draw_hud_for_turns();
        break;
    case 2:
        battle_draw_hud_for_event_script();
        break;
    }
}

/* 800765C4: Start the panel cursor window's opening over the member's panel (wider
 * for a member with the 800d32a0 flag unless it is character 7). The panel
 * x table is taken into a local after the index is formed, so its address
 * is loaded before the index is scaled. */
void battle_panel_start_opening(s32 member) {
    s32 index = battle_party_panel_layout * 3 + member;
    u16 *table = battle_panel_x_by_layout;
    u16 *panelX = &table[index];
    s32 x;

    x = *panelX + 0x48;
    battle_ui->unk34 = member * 0x60 + x;
    battle_ui->unk44 = 0x1C;
    if (battle_slot_flags[member].unk1 != 0 && battle_party_character_ids[member] != 7) {
        battle_ui->unk34 = member * 0x60 + 0x44 + *panelX;
        battle_ui->unk44 = 0x24;
    }
    battle_ui->unk3C = 0x10;
    battle_ui->unk4C = 0x98;
    battle_ui->unk54 = battle_ui->unk34 - (battle_ui->unk3C + 5);
    battle_ui->unk5C = battle_ui->unk4C - 5 - battle_ui->unk44;
    battle_ui->unk54 = (battle_ui->unk54 << 8) / battle_ui->unk5C;
    battle_ui->unk104 = 0x800;
    battle_ui->unk5C = 0x100;
    battle_ui->unk64 = 0;
    battle_ui->unk6C = 0;
    battle_ui->unk106 = 0;
    battle_ui->unkA9 = 6;
    battle_ui->unkAB = 1;
    battle_ui->unk90[member]--;
}

/* 80076710: Step the panel cursor window's opening: move the party-wide status label
 * by the pending steps, rebuild it semi-transparent at its growing scale,
 * and mark the member's panel open once the label reaches the window.
 * The original keeps x and y in memory (8-byte stack slots at sp+0x28 and
 * sp+0x30, so its y load is not forwarded from the +0x6c store); here they
 * are two-element u16 arrays only for that shape (the second element is
 * unused). The first new part index (`start`, set once) is doubled before
 * the second call, just ahead of it. */
void battle_panel_step_opening(s32 member) {
    u16 x[2];
    u16 y[2];
    s32 i;
    s32 first;
    s32 part;
    s32 start;

    for (i = 0; i < battle_ui->unkA9; i++) {
        battle_ui->unk64 -= battle_ui->unk54;
        battle_ui->unk6C += battle_ui->unk5C;
        x[0] = ((u32)battle_ui->unk64 >> 8) + battle_ui->unk34;
        y[0] = ((u32)battle_ui->unk6C >> 8) + battle_ui->unk44;
    }
    battle_ui->unkA9 = 0;
    battle_ui->statusParts[3] = 0;
    battle_ui->statusParts[3] = sprite_sheet_draw_rotated(battle_glyph_table, 0x52, battle_graphics->status[3][0], battle_drawing_state.buffer, x[0], y[0],
                                       battle_ui->unk104, battle_ui->unk104, battle_ui->unk106);
    first = battle_ui->statusParts[3];
    start = first * 2;
    battle_ui->statusParts[3] += sprite_sheet_draw_rotated(battle_glyph_table, 0x53, battle_graphics->status[3][first],
                                        battle_drawing_state.buffer, x[0], y[0], battle_ui->unk104, battle_ui->unk104,
                                        battle_ui->unk106);
    for (part = start; part < battle_ui->statusParts[3] * 2; part += 2) {
        SetSemiTrans(&battle_graphics->status[3][0][part + battle_drawing_state.buffer], 1);
    }
    battle_ui->statusBuffer[3] = battle_drawing_state.buffer;
    for (i = 0; i < battle_ui->unkAB; i++) {
        battle_ui->unk104 += 0x66;
        battle_ui->unk106 += 0x80;
    }
    battle_ui->unkAB = 0;
    if (battle_ui->unk3C >= x[0]) {
        battle_ui->unk90[member] = 1;
    }
    if (y[0] >= battle_ui->unk4C) {
        battle_ui->unk90[member] = 1;
    }
}

/* 800769E8: Upload an image and wait for the transfer. */
void battle_upload_image_and_wait(RECT *rect, u32 *pixels) {
    LoadImage(rect, (u_long *)pixels);
    DrawSync(0);
}

/* 80076A10: Build glyph `id` as primitives at `prims`, full scale. */
s32 battle_build_glyph(s32 id, POLY_FT4 *prims, s16 x, s16 y) {
    return sprite_sheet_draw_scaled(battle_glyph_table, id, prims, battle_drawing_state.buffer, x, y, 0x1000);
}

/* 80076A6C: Build glyph `id` as primitives at `prims`, half scale. */
s32 battle_build_glyph_half_scale(s32 id, POLY_FT4 *prims, s16 x, s16 y) {
    return sprite_sheet_draw_scaled(battle_glyph_table, id, prims, battle_drawing_state.buffer, x, y, 0x800);
}

/* 80076AC8: Initialise a textured quad: semi-transparent, its texture shaded by its
 * colour. */
void battle_quad_init_semi_transparent(POLY_FT4 *prim) {
    SetSemiTrans(prim, 1);
    SetShadeTex(prim, 0);
}

/* 80076B00: Initialise a textured quad at full brightness; tpage bit 0x40 follows
 * 800595a0. */
void battle_quad_init_full_window_blend(POLY_FT4 *prim) {
    battle_quad_init_semi_transparent(prim);
    prim->r0 = 0x80;
    prim->g0 = 0x80;
    prim->b0 = 0x80;
    if (window_semi_transparency_mode != 0) {
        prim->tpage |= 0x40;
    } else {
        prim->tpage &= ~0x40;
    }
}

/* 80076B68: Initialise a textured quad at full brightness with tpage bit 0x20. */
void battle_quad_init_full_additive(POLY_FT4 *prim) {
    battle_quad_init_semi_transparent(prim);
    prim->r0 = 0x80;
    prim->g0 = 0x80;
    prim->b0 = 0x80;
    prim->tpage |= 0x20;
}

/* 80076BAC: Initialise a textured quad at half brightness with tpage bit 0x20. */
void battle_quad_init_half_additive(POLY_FT4 *prim) {
    battle_quad_init_semi_transparent(prim);
    prim->r0 = 0x40;
    prim->g0 = 0x40;
    prim->b0 = 0x40;
    prim->tpage |= 0x20;
}

/* 80076BF0: Initialise a textured quad at full brightness with tpage bit 0x40. */
void battle_quad_init_full_subtractive(POLY_FT4 *prim) {
    battle_quad_init_semi_transparent(prim);
    prim->r0 = 0x80;
    prim->g0 = 0x80;
    prim->b0 = 0x80;
    prim->tpage |= 0x40;
}

/* 80076C34: Initialise a textured quad at half brightness with tpage bit 0x40. */
void battle_quad_init_half_subtractive(POLY_FT4 *prim) {
    battle_quad_init_semi_transparent(prim);
    prim->r0 = 0x40;
    prim->g0 = 0x40;
    prim->b0 = 0x40;
    prim->tpage |= 0x40;
}

/* 80076C78: Place a quad of width `w` and height 13 at (x, y) with texture (u, v). */
void battle_quad_place_text_row(POLY_FT4 *prim, u16 x, u16 y, u8 u, u8 v, u8 w) {
    prim->x0 = x;
    prim->y0 = y;
    prim->y1 = y;
    prim->x2 = x;
    prim->y2 = y + 13;
    prim->y3 = y + 13;
    prim->u0 = u;
    prim->u2 = u;
    prim->x1 = x + w;
    prim->x3 = x + w;
    prim->v0 = v;
    prim->u1 = u + w;
    prim->v1 = v;
    prim->v2 = v + 13;
    prim->u3 = u + w;
    prim->v3 = v + 13;
}

/* 80076CE8: Place a quad of `w` x `h` at (x, y) with texture (u, v). */
void battle_quad_place(POLY_FT4 *prim, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h) {
    prim->x0 = x;
    prim->y0 = y;
    prim->y1 = y;
    prim->x2 = x;
    prim->u0 = u;
    prim->u2 = u;
    prim->x1 = x + w;
    prim->y2 = y + h;
    prim->x3 = x + w;
    prim->y3 = y + h;
    prim->v0 = v;
    prim->u1 = u + w;
    prim->v1 = v;
    prim->v2 = v + h;
    prim->u3 = u + w;
    prim->v3 = v + h;
}

/* 80076D58: Initialise a quad pair: semi-transparent, raw texture, texture page by
 * `page` (0/1 at x 0x380, 2/3 at 0x3c0; odd pages at y 0x100) and the CLUT
 * chosen by `alternate`. */
void battle_init_text_quad_pair(POLY_FT4 *prims, u8 alternate, u8 page) {
    s32 i;

    for (i = 0; i < 2; i++) {
        SetPolyFT4(&prims[i]);
        prims[i].r0 = 0x80;
        prims[i].g0 = 0x80;
        prims[i].b0 = 0x80;
        SetSemiTrans(&prims[i], 0);
        SetShadeTex(&prims[i], 1);
        switch (page) {
        case 0:
            prims[i].tpage = GetTPage(0, 0, 0x380, 0);
            break;
        case 1:
            prims[i].tpage = GetTPage(0, 0, 0x380, 0x100);
            break;
        case 2:
            prims[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
            break;
        case 3:
            prims[i].tpage = GetTPage(0, 0, 0x3C0, 0);
            break;
        }
        prims[i].clut = alternate != 0 ? text_plane1_clut : text_plane0_clut;
    }
}

/* 80076EA4: Render the eleven command names (messages 10-20, and 21-31 in the
 * alternate colours) into text images, record their icon cells and upload
 * them to VRAM rows below (0x3de, 0x10d); then upload the ten message images
 * 0-9 side by side at (0x3de, 0x100). */
void battle_upload_command_name_images(void) {
    TextImage images[11];
    RECT rect;
    u32 *image;
    s32 i;

    for (i = 0; i < 11; i++) {
        image = (u32 *)battle_heap_alloc_text_image(0x1B);
        images[i].pixels = image;
        bzero((u_char *)image, 0x30C);
        battle_icon_cells[i].w = window_render_text_line(text_get_battle_message(i + 10), images[i].pixels, 0x1B, 0);
        battle_icon_cells[i + 11].w = window_render_text_line(text_get_battle_message(i + 21), images[i].pixels, 0x1B, 1);
        battle_icon_cells[i].u = battle_icon_cells[i + 11].u = 0x78;
        battle_icon_cells[i].v = battle_icon_cells[i + 11].v = i * 0xD + 0xD;
        battle_icon_cells[i].alternate = 0;
        battle_icon_cells[i + 11].alternate = 1;
        rect.x = 0x3DE;
        rect.y = i * 0xD + 0x10D;
        rect.w = 0x1E;
        rect.h = 0xD;
        battle_upload_image_and_wait(&rect, images[i].pixels);
    }
    for (i = 0; i < 11; i++) {
        heap_free(images[i].pixels);
    }
    for (i = 0; i < 10; i++) {
        rect.x = i * 2 + 0x3DE;
        rect.y = 0x100;
        rect.w = 6;
        rect.h = 0xD;
        battle_upload_image_and_wait(&rect, battle_digit_text_images[i].pixels);
    }
}

/* 80077074: Hide the command panel's quads and reset each quad pair's texture and
 * CLUT for its page kind. */
void battle_init_list_page_quads(void) {
    battle_graphics->unkA230->unk669 = 0;
    battle_graphics->unkA230->unk66B = 0;
    battle_graphics->unkA230->unk66D = 0;
    battle_init_text_quad_pair(battle_graphics->unkA230->unk0, 0, 1);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk50, 0, 1);
    battle_init_text_quad_pair(battle_graphics->unkA230->unkA0, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unkF0, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk140, 0, 1);
    battle_init_text_quad_pair(&battle_graphics->unkA230->unk190[0], 0, 2);
    battle_init_text_quad_pair(&battle_graphics->unkA230->unk190[2], 0, 2);
    battle_init_text_quad_pair(&battle_graphics->unkA230->unk190[4], 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk280, 0, 3);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk2D0, 1, 3);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk320, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk370, 1, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk3C0, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk410, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk460, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk4B0, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk500, 0, 2);
    battle_init_text_quad_pair(battle_graphics->unkA230->unk550, 0, 2);
}

/* 80077364: Initialise four quads, white and semi-transparent, with the texture page
 * and CLUT of graphics texture entry `index`. */
void battle_window_init_edge_quads(POLY_FT4 *prims, u8 index) {
    s32 i;

    for (i = 0; i < 4; i++) {
        SetPolyFT4(&prims[i]);
        SetShadeTex(&prims[i], 1);
        prims[i].r0 = 0xFF;
        prims[i].g0 = 0xFF;
        prims[i].b0 = 0xFF;
        prims[i].tpage = GetTPage(battle_graphics->sprites[index].tpageMode, 0, battle_graphics->sprites[index].pageX,
                                       battle_graphics->sprites[index].pageY);
        prims[i].clut = GetClut(battle_graphics->sprites[index].clutX, battle_graphics->sprites[index].clutY);
    }
}

/* 80077454: Initialise a window's primitives: its semi-transparent background in the
 * window colour (one per draw buffer, with its draw mode) and its four edge
 * piece sets (textures 1-4). The draw mode's texture window argument is the
 * UI block pointer. */
void battle_window_init_prims(u8 window) {
    WindowBlock *block;
    u8 i;

    block = battle_window_blocks[window];
    for (i = 0; i < 2; i++) {
        SetPolyG4(&block->shade[i]);
        (block->shade + i)->r0 = window_color[0];
        (block->shade + i)->g0 = window_color[1];
        (block->shade + i)->b0 = window_color[2];
        (block->shade + i)->r1 = window_color[0];
        (block->shade + i)->g1 = window_color[1];
        (block->shade + i)->b1 = window_color[2];
        (block->shade + i)->r2 = window_color[0];
        (block->shade + i)->g2 = window_color[1];
        (block->shade + i)->b2 = window_color[2];
        (block->shade + i)->r3 = window_color[0];
        (block->shade + i)->g3 = window_color[1];
        (block->shade + i)->b3 = window_color[2];
        SetSemiTrans(&block->shade[i], 1);
        SetDrawMode(&block->mode[i], 0, 0,
                    GetTPage(0, window_semi_transparency_mode, battle_graphics->sprites[1].pageX, battle_graphics->sprites[1].pageY),
                    (RECT *)battle_ui);
    }
    battle_window_init_edge_quads(block->frame[0], 1);
    battle_window_init_edge_quads(block->frame[1], 2);
    battle_window_init_edge_quads(block->frame[2], 3);
    battle_window_init_edge_quads(block->frame[3], 4);
}

/* 80077610: Allocate and clear the 0x670-byte graphics block, then initialise it. */
void battle_alloc_list_page_block(void) {
    GraphicsBlock *block = (GraphicsBlock *)battle_heap_alloc(0x670, 0);

    battle_graphics->unkA230 = block;
    bzero((u_char *)block, 0x670);
    battle_init_list_page_quads();
}

/* 8007765C: Wait a frame, then release the graphics block. */
void battle_release_list_page_block(void) {
    battle_wait_frame();
    heap_free(battle_graphics->unkA230);
}

/* 80077698: Set up the four direction arrows (down, left, up, right triangles, red
 * fading to dark) in both draw buffers and start their pulse. */
void battle_show_direction_arrows(void) {
    s32 dir;
    s32 buf;

    bzero((u_char *)battle_direction_arrows, sizeof(DirectionArrows));
    for (dir = 0; dir < 4; dir++) {
        for (buf = 0; buf < 2; buf++) {
            SetPolyG3(&battle_direction_arrows->prims[dir * 2 + buf]);
            switch (dir) {
            case 0:
                setXY3(&battle_direction_arrows->prims[dir * 2 + buf], 0xC0, 0x70, 0xB0, 0x78, 0xB0, 0x68);
                break;
            case 1:
                setXY3(&battle_direction_arrows->prims[dir * 2 + buf], 0xA0, 0x90, 0x98, 0x80, 0xA8, 0x80);
                break;
            case 2:
                setXY3(&battle_direction_arrows->prims[dir * 2 + buf], 0x80, 0x70, 0x90, 0x68, 0x90, 0x78);
                break;
            case 3:
                setXY3(&battle_direction_arrows->prims[dir * 2 + buf], 0xA0, 0x50, 0x98, 0x60, 0xA8, 0x60);
                break;
            }
            setRGB0(&battle_direction_arrows->prims[dir * 2 + buf], 0xFF, 0, 0);
            setRGB1(&battle_direction_arrows->prims[dir * 2 + buf], 0x40, 0, 0);
            setRGB2(&battle_direction_arrows->prims[dir * 2 + buf], 0x40, 0, 0);
        }
    }
    battle_direction_arrows->shade = 0xFF;
    battle_direction_arrows->fading = 1;
    battle_direction_arrows->buffer = battle_drawing_state.buffer;
    battle_ui->unkC6 = 1;
}

/* 80077980: Clear UI byte +0xc6. */
void battle_hide_direction_arrows(void) {
    battle_ui->unkC6 = 0;
}

/* 80077990: Save four copies of the four CLUT rows above the panel sprite's CLUT
 * (for the CLUT cycle), look up the cursor and arrow sprites, place two of
 * them in VRAM, and set the texture windows and draw modes that use them. */
void battle_init_hud_textures(void) {
    u_long *strip0 = battle_graphics->unk8970[0];
    u_long *strip1 = battle_graphics->unk8970[1];
    u_long *strip2 = battle_graphics->unk8970[2];
    u_long *strip3 = battle_graphics->unk8970[3];

    battle_graphics->unk8950[0].x = 1;
    battle_graphics->unk8950[0].y = battle_graphics->sprites[0].clutY - 1;
    battle_graphics->unk8950[0].w = 0xC6;
    battle_graphics->unk8950[0].h = 1;
    battle_graphics->unk8950[1].x = 1;
    battle_graphics->unk8950[1].y = battle_graphics->sprites[0].clutY;
    battle_graphics->unk8950[1].w = 0xC6;
    battle_graphics->unk8950[1].h = 1;
    battle_graphics->unk8950[2].x = 1;
    battle_graphics->unk8950[2].y = battle_graphics->sprites[0].clutY - 2;
    battle_graphics->unk8950[2].w = 0xC6;
    battle_graphics->unk8950[2].h = 1;
    battle_graphics->unk8950[3].x = 1;
    battle_graphics->unk8950[3].y = battle_graphics->sprites[0].clutY - 3;
    battle_graphics->unk8950[3].w = 0xC6;
    battle_graphics->unk8950[3].h = 1;
    StoreImage(&battle_graphics->unk8950[0], strip0);
    StoreImage(&battle_graphics->unk8950[1], strip1);
    StoreImage(&battle_graphics->unk8950[2], strip2);
    StoreImage(&battle_graphics->unk8950[3], strip3);
    StoreImage(&battle_graphics->unk8950[0], strip0 + 0x63);
    StoreImage(&battle_graphics->unk8950[1], strip1 + 0x63);
    StoreImage(&battle_graphics->unk8950[2], strip2 + 0x63);
    StoreImage(&battle_graphics->unk8950[3], strip3 + 0x63);
    StoreImage(&battle_graphics->unk8950[0], strip0 + 0x63 * 2);
    StoreImage(&battle_graphics->unk8950[1], strip1 + 0x63 * 2);
    StoreImage(&battle_graphics->unk8950[2], strip2 + 0x63 * 2);
    StoreImage(&battle_graphics->unk8950[3], strip3 + 0x63 * 2);
    StoreImage(&battle_graphics->unk8950[0], strip0 + 0x63 * 3);
    StoreImage(&battle_graphics->unk8950[1], strip1 + 0x63 * 3);
    StoreImage(&battle_graphics->unk8950[2], strip2 + 0x63 * 3);
    StoreImage(&battle_graphics->unk8950[3], strip3 + 0x63 * 3);
    sprite_sheet_get_texture(battle_glyph_table, 0x4B, &battle_graphics->sprites[1].unk0, &battle_graphics->sprites[1].tpageMode,
                  &battle_graphics->sprites[1].clutX, &battle_graphics->sprites[1].clutY,
                  &battle_graphics->sprites[1].pageX, &battle_graphics->sprites[1].pageY);
    sprite_sheet_get_texture(battle_glyph_table, 0x50, &battle_graphics->sprites[2].unk0, &battle_graphics->sprites[2].tpageMode,
                  &battle_graphics->sprites[2].clutX, &battle_graphics->sprites[2].clutY,
                  &battle_graphics->sprites[2].pageX, &battle_graphics->sprites[2].pageY);
    sprite_sheet_get_texture(battle_glyph_table, 0x4D, &battle_graphics->sprites[3].unk0, &battle_graphics->sprites[3].tpageMode,
                  &battle_graphics->sprites[3].clutX, &battle_graphics->sprites[3].clutY,
                  &battle_graphics->sprites[3].pageX, &battle_graphics->sprites[3].pageY);
    sprite_sheet_get_texture(battle_glyph_table, 0x4E, &battle_graphics->sprites[4].unk0, &battle_graphics->sprites[4].tpageMode,
                  &battle_graphics->sprites[4].clutX, &battle_graphics->sprites[4].clutY,
                  &battle_graphics->sprites[4].pageX, &battle_graphics->sprites[4].pageY);
    battle_ui->textureWindows[0].y = 0;
    battle_ui->textureWindows[0].x = 0;
    battle_ui->textureWindows[0].h = 0x100;
    battle_ui->textureWindows[0].w = 0x100;
    battle_graphics->sprites[1].pageX = 0x3C0;
    battle_graphics->sprites[2].pageX = 0x3C8;
    battle_graphics->sprites[1].pageY = 0x34;
    battle_graphics->sprites[2].pageY = 0x34;
    battle_ui->textureWindows[1].x = ((u16)battle_graphics->sprites[0].pageX & 0x3F) * 2;
    battle_ui->textureWindows[1].y = battle_graphics->sprites[0].pageY;
    battle_ui->textureWindows[1].w = 0x100;
    battle_ui->textureWindows[1].h = 0x100;
    battle_ui->textureWindows[2].x = ((u16)battle_graphics->sprites[1].pageX & 0x3F) * 2;
    battle_ui->textureWindows[2].y = battle_graphics->sprites[1].pageY;
    battle_ui->textureWindows[2].w = 8;
    battle_ui->textureWindows[2].h = 0x10;
    battle_ui->textureWindows[3].x = ((u16)battle_graphics->sprites[2].pageX & 0x3F) * 2;
    battle_ui->textureWindows[3].y = battle_graphics->sprites[2].pageY;
    battle_ui->textureWindows[3].w = 8;
    battle_ui->textureWindows[3].h = 0x10;
    battle_ui->textureWindows[4].x = ((u16)battle_graphics->sprites[3].pageX & 0x3F) * 2 + 0xE;
    battle_ui->textureWindows[4].y = battle_graphics->sprites[3].pageY;
    battle_ui->textureWindows[4].w = 0x10;
    battle_ui->textureWindows[4].h = 8;
    battle_ui->textureWindows[5].x = ((u16)battle_graphics->sprites[4].pageX & 0x3F) * 2 + 0xE;
    battle_ui->textureWindows[5].y = battle_graphics->sprites[4].pageY;
    battle_ui->textureWindows[5].w = 0x10;
    battle_ui->textureWindows[5].h = 8;
    SetDrawMode(&battle_graphics->unk8908[0], 0, 0,
                GetTPage(battle_graphics->sprites[0].tpageMode, 0, battle_graphics->sprites[0].pageX,
                         battle_graphics->sprites[0].pageY),
                &battle_ui->textureWindows[1]);
    SetDrawMode(&battle_graphics->unk8908[1], 0, 0,
                GetTPage(battle_graphics->sprites[0].tpageMode, 0, battle_graphics->sprites[0].pageX,
                         battle_graphics->sprites[0].pageY),
                &battle_ui->textureWindows[1]);
    SetDrawMode(&battle_graphics->unk8920[0], 0, 0,
                GetTPage(battle_graphics->sprites[0].tpageMode, 0, battle_graphics->sprites[0].pageX,
                         battle_graphics->sprites[0].pageY),
                &battle_ui->textureWindows[0]);
    SetDrawMode(&battle_graphics->unk8920[1], 0, 0,
                GetTPage(battle_graphics->sprites[0].tpageMode, 0, battle_graphics->sprites[0].pageX,
                         battle_graphics->sprites[0].pageY),
                &battle_ui->textureWindows[0]);
}

/* 800780A8: Initialise a battle message's quad pair for texture row `row` (13 pixels
 * per pair of rows; odd rows use the alternate CLUT) and hide it. */
void battle_message_init_quads(BattleMessage *message, u32 row) {
    s32 i;

    for (i = 0; i < 2; i++) {
        SetPolyFT4(&message->prims[i]);
        message->prims[i].r0 = 0x80;
        message->prims[i].g0 = 0x80;
        message->prims[i].b0 = 0x80;
        SetSemiTrans(&message->prims[i], 0);
        SetShadeTex(&message->prims[i], 1);
        message->alternate = row & 1;
        message->prims[i].clut = message->alternate ? text_plane1_clut : text_plane0_clut;
        message->prims[i].tpage = GetTPage(0, 0, 0x3C0, (s32)row / 2 * 13);
    }
    message->shown = 0;
}

/* 8007819C: Set up windows 5 and 4 (closed) and the eight battle messages: each pair
 * shares a text image block and a VRAM rectangle row. */
void battle_init_messages(void) {
    s32 i;

    battle_window_open(5, 8, 0x2A, 0x70, 0x12, 0, 0);
    battle_ui->windows[5] = 0;
    battle_window_open(4, 0x20, 0xC8, 0xF4, 0x12, 0, 0);
    battle_ui->windows[4] = 0;
    for (i = 0; i < 8; i += 2) {
        battle_message_entries[i].pixels = (u32 *)battle_heap_alloc_text_image(0x39);
        battle_message_entries[i + 1].pixels = battle_message_entries[i].pixels;
        setRECT(&battle_message_entries[i].rect, 0x3C0, (i / 2) * 13, 0x3C, 13);
        battle_message_entries[i + 1].rect = battle_message_entries[i].rect;
        battle_message_init_quads(&battle_message_entries[i], i);
        battle_message_init_quads(&battle_message_entries[i + 1], i + 1);
    }
}

/* 80078310: Upload each present member's portrait TIM (0x460 bytes per character in
 * `portraits`; character 0xb for the second and third member when 800d3294
 * is set) to the texture and CLUT places of sprite `glyph + member`, the
 * image moved 6 per member. `sprites` holds the six sprite_sheet_get_texture outputs
 * (SpriteInfo order) per member; the outputs are passed through a pointer
 * set before the loop and read back from the array, indexed flat. */
void battle_upload_party_portraits(u8 *portraits, u8 glyph) {
    TIM_IMAGE tim;
    s32 sprites[3 * 6];
    s32 i;
    u8 character;
    s32 *info = sprites;

    for (i = 0; i < 3; i++) {
        if (battle_party_character_ids[i] != 0x7F) {
            character = battle_party_character_ids[i];
            if (battle_uses_fixed_party != 0 && (i == 1 || i == 2)) {
                character = 0xB;
            }
            OpenTIM((u_long *)(portraits + character * 0x460));
            ReadTIM(&tim);
            sprite_sheet_get_texture(battle_glyph_table, glyph + i, info + i * 6, info + (i * 6 + 1), info + (i * 6 + 2),
                          info + (i * 6 + 3), info + (i * 6 + 4), info + (i * 6 + 5));
            tim.crect->x = sprites[i * 6 + 2];
            tim.crect->y = sprites[i * 6 + 3];
            tim.prect->x = sprites[i * 6 + 4] + i * 6;
            tim.prect->y = sprites[i * 6 + 5];
            LoadImage(tim.crect, tim.caddr);
            LoadImage(tim.prect, tim.paddr);
            DrawSync(0);
        }
    }
}

/* 80078508: Reset every slot's turn timers from its speed (unused slots 0xff), its
 * ready flag and slow alternation, and clear the order buffer. */
void battle_reset_turn_timers(u8 *order) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        if (battle_turn_queue.present[slot] != 0) {
            battle_turn_queue.timers[0][slot] = battle_turn_queue.timers[1][slot] = battle_compute_turn_timer(slot, 0);
        } else {
            battle_turn_queue.timers[0][slot] = battle_turn_queue.timers[1][slot] = 0xFF;
        }
        battle_turn_queue.ready[slot] = 0;
        battle_turn_queue.timers[2][slot] = 0;
        order[slot] = 0;
    }
}

/* 800785D4: Close the current event for `actor` with the action entry's parameter and
 * advance the event count. */
void battle_action_list_queue_event(u8 actor, u8 index) {
    u16 parameter;

    parameter = (battle_action_list[index].unk5 << 8) | battle_action_list[index].param;
    battle_area_events[battle_turn_state->eventCount].actor = actor;
    battle_area_events[battle_turn_state->eventCount].parameter = parameter;
    battle_turn_state->eventCount++;
}

/* 80078658: Show the name of action `index` in the next battle message (up to eight)
 * and queue its event (0xfa) for `actor`. */
void battle_action_list_name(u8 index, u8 actor) {
    if (battle_pending_message < 9) {
        battle_message_entries[battle_pending_message].width =
            window_render_text_line(text_get_resource_entry(battle_enemy_name_table, battle_action_list[index].named), battle_message_entries[battle_pending_message].pixels, 0x39,
                          battle_pending_message & 1);
        battle_upload_image_and_wait(&battle_message_entries[battle_pending_message].rect, battle_message_entries[battle_pending_message].pixels);
        battle_area_events[battle_turn_state->eventCount].actor = actor;
        battle_area_events[battle_turn_state->eventCount].type = 0xFA;
        battle_area_events[battle_turn_state->eventCount].parameter = battle_pending_message++;
        battle_turn_state->eventCount++;
    }
}

/* 800787E0: Queue event type 0xf7 for `actor` with parameter `value`. */
void battle_action_list_event_f7(u8 value, u8 actor) {
    battle_area_events[battle_turn_state->eventCount].actor = actor;
    battle_area_events[battle_turn_state->eventCount].type = 0xF7;
    battle_area_events[battle_turn_state->eventCount].parameter = value;
    battle_turn_state->eventCount++;
}

/* 8007887C: Queue event type 0xf8 for `actor` with the pending message 800c3e8c - 1. */
void battle_action_list_message_f8(u8 actor) {
    if (battle_pending_message != 0) {
        battle_area_events[battle_turn_state->eventCount].actor = actor;
        battle_area_events[battle_turn_state->eventCount].type = 0xF8;
        battle_area_events[battle_turn_state->eventCount].parameter = battle_pending_message - 1;
        battle_turn_state->eventCount++;
    }
}

/* 8007893C: For a named action entry: its name text (80078658), event 0xf7 with 0x1e
 * and the pending message event. */
void battle_action_list_queue_name(u8 index, u8 actor) {
    if (battle_action_list[index].named != 0) {
        battle_action_list_name(index, actor);
        battle_action_list_event_f7(0x1E, actor);
        battle_action_list_message_f8(actor);
    }
}

/* 80078998: Execute the actor's action `index`: commit it, queue its animation event
 * with the committed targets and, when those differ from the action's own,
 * retarget the queued move event (0xfd). */
void battle_action_list_act(u8 actor, u8 index, u8 target) {
    s32 i;

    battle_action_list_queue_name(index, actor);
    battle_turn_state->unk2DC = battle_action_list[index].operand + 1;
    battle_commit_action(actor, battle_action_list[index].targets, battle_action_list[index].animation);
    battle_accumulate_and_apply_results(battle_turn_state->eventCount);
    battle_area_events[battle_turn_state->eventCount].type = battle_action_list[index].animation;
    battle_area_events[battle_turn_state->eventCount].targetMask = battle_committed_action.targets;
    battle_action_list_queue_event(actor, index);
    if (battle_action_list[index].targets != battle_committed_action.targets) {
        for (i = 0; i < battle_turn_state->eventCount; i++) {
            if (battle_area_events[i].type == 0xFD) {
                battle_area_events[i].targetMask = battle_committed_action.targets;
                return;
            }
        }
    }
}

/* 80078B34: Queue a move event (0xfd) for the actor's action `index` toward target:
 * compute the move, then (unless the action's parameter is 1) choose the
 * on-foot or gear approach, and redraw the slots involved. */
void battle_action_list_approach(u8 actor, u8 index, u8 target) {
    battle_area_events[battle_turn_state->eventCount].type = 0xFD;
    battle_area_events[battle_turn_state->eventCount].parameter = 0;
    battle_area_events[battle_turn_state->eventCount].targetMask = battle_action_list[index].targets;
    battle_plan_approach_route(actor, target);
    if (battle_action_list[index].param != 1) {
        if (battle_slot_flags[actor].unk1 == 0) {
            battle_join_target_group(actor, target);
        } else {
            battle_join_empty_target_group(actor, target);
        }
    }
    battle_camera_start_move(battle_action_list[index].targets | battle_get_slot_bit(actor));
    battle_action_list_queue_event(actor, index);
}

/* 80078C9C: Queue event type 0xfc for the actor. */
void battle_action_list_event_fc(u8 actor, u8 index, u8 target) {
    battle_area_events[battle_turn_state->eventCount].type = 0xFC;
    battle_action_list_queue_event(actor, index);
}

/* 80078CEC: Queue the action entry's parameter as the event type for the actor. */
void battle_action_list_event(u8 actor, u8 index, u8 target) {
    battle_area_events[battle_turn_state->eventCount].type = battle_action_list[index].param;
    battle_action_list_queue_event(actor, index);
}

/* 80078D48: The action entry's targets act together. */
void battle_action_list_together(u8 actor, u8 index, u8 target) {
    battle_joint_action_slots = battle_action_list[index].targets;
}

/* 80078D6C: Action entry type: the actor leaves the battle (event 0xf9); its reaction
 * reaction byte +3 is set and only its 0x8000 flag is kept. */
void battle_action_list_leave(u8 actor, u8 index, u8 target) {
    battle_area_events[battle_turn_state->eventCount].type = 0xF9;
    battle_action_list_queue_event(actor, index);
    battle_turn_queue.ready[actor] = 0xFF;
    battle_enemy_reactions[actor - 3].unk3 = 1;
    battle_work_area.records[actor].pilot.status7C &= 0x8000;
}

/* 80078E24: Make enemy `slot` a copy of the first slot action `index` targets: its
 * name, AI scripts, reaction state and combatant record, and queue the
 * split event (0xfb) for it. */
void battle_action_list_split(u8 slot, u8 index, u8 target) {
    s32 i;
    u8 source;

    for (i = 0; i < 11; i++) {
        if (battle_is_slot_in_mask(battle_action_list[index].targets, i)) {
            source = i;
            break;
        }
    }
    battle_enemy_name_indices[slot - 3] = battle_enemy_name_indices[source - 3];
    battle_enemy_ai_blocks[slot - 3].script = battle_enemy_ai_blocks[source - 3].script;
    battle_enemy_ai_blocks[slot - 3].unk4 = battle_enemy_ai_blocks[source - 3].unk4;
    battle_enemy_ai_blocks[slot - 3].reaction = battle_enemy_ai_blocks[source - 3].reaction;
    battle_enemy_ai_blocks[slot - 3].turnScript = battle_enemy_ai_blocks[source - 3].turnScript;
    battle_enemy_reactions[slot - 3].armed = battle_enemy_reactions[source - 3].armed;
    battle_enemy_reactions[slot - 3].unk1[0] = battle_enemy_reactions[source - 3].unk1[0];
    memmove(&battle_work_area.records[slot], &battle_work_area.records[source], sizeof(Combatant));
    battle_area_events[battle_turn_state->eventCount].parameter = source;
    battle_area_events[battle_turn_state->eventCount].type = 0xFB;
    battle_area_events[battle_turn_state->eventCount].actor = slot;
    battle_turn_state->eventCount++;
}

/* 80079054: Set the actor's attribute `operand` to the entry's parameter byte. */
void battle_action_list_set_attr8(u8 actor, u8 index, u8 target) {
    battle_access_combatant_attr8(actor, battle_action_list[index].operand, battle_action_list[index].param, 0);
}

/* 80079098: Add the entry's parameter byte to the actor's attribute `operand`. */
void battle_action_list_add_attr8(u8 actor, u8 index, u8 target) {
    battle_access_combatant_attr8(actor, battle_action_list[index].operand,
                  battle_action_list[index].param + battle_access_combatant_attr8(actor, battle_action_list[index].operand, 0, 1), 0);
}

/* 80079114: Set the actor's 16-bit attribute `operand` to the entry's parameter halfword. */
void battle_action_list_set_attr16(u8 actor, u8 index, u8 target) {
    battle_access_combatant_attr16(actor, battle_action_list[index].operand, battle_action_list[index].param | (battle_action_list[index].unk5 << 8), 0);
}

/* 8007916C: Add the entry's parameter halfword to the actor's 16-bit attribute `operand`. */
void battle_action_list_add_attr16(u8 actor, u8 index, u8 target) {
    battle_access_combatant_attr16(actor, battle_action_list[index].operand,
                  battle_action_list[index].param +
                      (battle_access_combatant_attr16(actor, battle_action_list[index].operand, 0, 1) + (battle_action_list[index].unk5 << 8)),
                  0);
}

/* 800791FC: Named-action text, then event 0xf4 for the actor. */
void battle_action_list_named_f4(u8 actor, u8 index, u8 target) {
    battle_action_list_queue_name(index, actor);
    battle_area_events[battle_turn_state->eventCount].type = 0xF4;
    battle_action_list_queue_event(actor, index);
}

/* 80079270: Event 0xf6 for the actor with the entry's targets. */
void battle_action_list_event_f6(u8 actor, u8 index, u8 target) {
    battle_area_events[battle_turn_state->eventCount].type = 0xF6;
    battle_area_events[battle_turn_state->eventCount].targetMask = battle_action_list[index].targets;
    battle_action_list_queue_event(actor, index);
}
