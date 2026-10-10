#ifndef OVL2596_BATTLE_RESULTS_H
#define OVL2596_BATTLE_RESULTS_H

/* The post-battle module: the battle overlay's result screen primitives and
 * digit buffer as it reads them, the growth data file, the module's own
 * functions, and the battle overlay and resident calls no shared header
 * declares. The battle overlay's other objects and calls (its area, work
 * area, UI state, turn state, item lists and the set-up and exit state) come
 * from the shared battle headers, the game data from resident/gamedata.h. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "battle/actions.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/event_script.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "resident/cd.h"
#include "resident/formation.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"

/* Callers convert arguments/result differently from the resident definition
 * (u8 tags, ids and codes passed where the resident takes words, or the
 * reverse): heap tag reset, sound effect, text rendering and item names. */
void heap_free_tag(s32 tag);
void sound_play_effect_on_last_channels(s32 effect);
s32 window_render_text_line(void *text, void *image, s32 width, s32 flags); /* render text */
void *text_get_system_resource_entry(u8 table, s32 index);                        /* counter skill names */
void *text_get_accessory_name(u8 index);                                      /* item names per list */
void *text_get_item_name(u8 index);
void *text_get_weapon_name(u8 index);
void *text_get_gear_accessory_name(u8 index);
void *text_get_gear_part_name(u8 index);

/* --- The result screens --------------------------------------------------- */

/* A glyph-table sprite part, one primitive per draw buffer. */
typedef POLY_FT4 Glyph[2];

/* The member cards and their glyph runs (GlyphRun, MemberCard, battle_member_cards)
 * are in battle/ui.h: the battle counts them up. */

/* The result summary windows' primitives (pointer 800d334c). */
typedef struct {
    Glyph title[4];           /* 0x0000, runs[0] */
    POLY_FT4 text[134];       /* 0x0140, runs[1]; two per glyph part */
    POLY_FT4 glyphs1630[6];   /* 0x1630, runs[2] */
    POLY_FT4 glyphs1720[4];   /* 0x1720, runs[4] */
    POLY_FT4 glyphs17C0[8];   /* 0x17C0, runs[3] */
    POLY_FT4 glyphs1900[6];   /* 0x1900, runs[5] */
    POLY_FT4 rowA[7][6];      /* 0x19F0 */
    POLY_FT4 rowB[7][8];      /* 0x2080 */
    POLY_G4 barA[7][2];       /* 0x2940 */
    POLY_G4 barB[7][2];       /* 0x2B38 */
    Glyph glyphs2D30[7];      /* 0x2D30 */
    Glyph glyphs2F60[6];      /* 0x2F60 */
    Glyph glyphs3140[9];      /* 0x3140 */
    Glyph glyphs3410[2];      /* 0x3410 */
    Glyph glyphs34B0[2];      /* 0x34B0 */
    Glyph listA[8];           /* 0x3550 */
    Glyph listB[8];           /* 0x37D0 */
    GlyphRun runs[6];         /* 0x3A50 */
    u8 rowACount[7];          /* 0x3A5C */
    u8 rowABuffer[7];         /* 0x3A63 */
    u8 rowBCount[7];          /* 0x3A6A */
    u8 rowBBuffer[7];         /* 0x3A71 */
    u8 barBuffer[7];          /* 0x3A78 */
    u8 count2D30;             /* 0x3A7F */
    u8 buffer2D30;            /* 0x3A80 */
    GlyphRun run2F60;         /* 0x3A81 */
    GlyphRun run3140;         /* 0x3A83 */
    u8 listCount;             /* 0x3A85 */
    u8 listBuffer;            /* 0x3A86 */
    u8 buffer3410;            /* 0x3A87 */
    u8 buffer34B0[2];         /* 0x3A88 */
} ResultSummary;

extern ResultSummary *battle_summary_window_prims;

/* battle_split_decimal_digits writes a value's nine decimal digits to 800c3cf4, leading
 * zeros as 0xff; the screens read the last digits through these views. */
extern u8 battle_decimal_digits_minus_3[12];
extern u8 battle_decimal_digits_minus_7[16];
extern u8 battle_finished_motion_count[];     /* as the level numbers address it */
extern u8 battle_decimal_digits_minus_17[];
extern u8 battle_decimal_digits_plus_6[];
extern u8 battle_decimal_digits_plus_7[];
extern u8 battle_decimal_digits_minus_21[];
extern u8 battle_decimal_digits_minus_29[];
extern u8 battle_camera_framed_range[];       /* the spoils window's */

/* Battle slot levels (8 bytes per slot). */
typedef struct {
    u8 level;
    u8 level2;
    u8 pad[6];
} SlotLevels;

extern SlotLevels battle_slot_levels[3];

/* The summary window's text: 27 glyph entries of three bytes (glyph, 0xff:
 * none; shaded flag; index into the shading colours) and their positions. */
extern u8 battle_summary_text_glyphs[27 * 3];
extern s16 battle_summary_text_x[27];
extern s16 battle_summary_text_y[27];

/* The seven glyphs of the summary's 2d30 label: ids and positions. */
extern u8 battle_spoils_label_glyphs[8];
extern s16 battle_spoils_label_x[8];
extern s16 battle_spoils_label_y[8];

/* The member card's label glyphs: ids and positions. */
extern u8 battle_member_card_label_glyphs[18];
extern s16 battle_member_card_label_x[18];
extern s16 battle_member_card_label_y[18];

void battle_window_open(s32 window, u16 x, u16 y, u16 w, u16 h, s32 animate, s32 wait); /* open a window */
void battle_window_close(s32 window);                                                          /* close a window */
void battle_init_text_quad_pair(POLY_FT4 *prims, s32 alternate, s32 page);
void *battle_heap_alloc_text_image(s32 count);                                             /* allocate a text image */
void battle_wait_frame(void);                                                              /* run one battle frame */

extern u8 battle_spoils_icon_cells[8];        /* two icon records: width, -, u, v */
extern u8 battle_skill_mark_icon_cell[4];     /* the skill mark icon: width, -, u, v */
extern void *battle_work_growth_file[1];      /* BattleWork.growth, the growth data file; the original
                                               * addresses it as a table */

void battle_results_queue_summary_and_new_skill(void);
void battle_results_queue_spoils_window(void);
void battle_results_build_summary_max_hp_change(u8 member);
void battle_results_init_gauge_bar(POLY_G4 *bar, u8 colour);
void battle_results_shade_glyphs(POLY_FT4 *prims, u8 blue, u8 count, u8 buffer);
void battle_results_compute_gauge(u8 from, u8 to, s32 max);
s32 battle_results_get_highest_stat(u8 slot);
void battle_results_build_summary_max_ep_change(u8 member);
void battle_results_show_new_deathblows_and_arts(u8 member);
void battle_results_add_to_inventory_list(u8 id, u8 count, u8 *ids, u8 *counts, u8 size);
void battle_results_build_spoils_item_list(void);

/* --- Experience, growth and skills --------------------------------------- */

/* Per character growth data (0x110 each; the block 801e44e8 points to). */
typedef struct {
    u16 requirements[13][7];  /* 0x00: counter thresholds per counter skill */
    u8 padB6[2];
    u16 maxHpTargets[2];      /* 0xB8: below level 100, from 100 */
    u8 statTargets[6][2];     /* 0xBC: stats 58 59 5e 5f 5b 5c, per level range */
    u8 maxEpTargets[2];       /* 0xC8 */
    u8 padCA[2];
    u8 tierLevels[3];         /* 0xCC: levels for tiers 4, 5 and 6 */
    u8 padCF;
    u8 unlocksA[16];          /* 0xD0: 0xff ends */
    u8 unlocksB[16];          /* 0xE0: 0 ends */
    u8 levelSkills[16];       /* 0xF0: 0xff ends */
    u8 counterLevels[16];     /* 0x100 */
} Growth;

/* The growth data file (BattleWork.growth): one block per character. */
typedef struct GrowthFile {
    Growth characters[11];
    s32 experience[99];       /* 0xBB0: experience to the next level, per level - 1 */
} GrowthFile;

void battle_results_grant_exp_skills_and_drops(void);
void battle_results_distribute_exp(void);
void battle_results_split_exp_into_pools(u32 experience, s16 slot, s16 reserve);
void battle_results_add_exp_and_level_up(void);          /* experience pool for level B */
void battle_results_grow_level_a_stats(void);
void battle_results_grow_level_b_stats(void);
u8 battle_results_grow_stat(u8 stat, u8 target, u8 cap, u8 level);
u16 battle_results_grow_max_hp(u16 maxHp, u8 level);
u8 battle_results_grow_max_ep(u8 maxEp, u8 level);
void battle_results_learn_skills(void);
u8 battle_results_learn_deathblow(u8 id);
u8 battle_results_learn_art(u8 id);
void battle_results_unlock_by_known_deathblows(u8 id);
void battle_results_unlock_character8_by_level(void);
void battle_results_unlock_by_known_arts(u8 id);
void battle_results_derive_character7_gear_stats(void);
void battle_results_advance_tiers(void);
void battle_results_unlock_at_levels_50_60_70(void);
void battle_results_roll_drops(void);
void battle_results_write_party_to_game_data(void);

/* --- Rewards, the results resources and the battle exit ------------------ */

extern u8 battle_enemy_no_reward_flags[8][4];     /* per enemy: [0] nonzero, no rewards */
u16 battle_get_slot_bit(u8 slot);
void battle_highlight_slots(s32 mask);
void battle_results_run_screens(u32 experience);
void battle_results_load_resources(void);
void battle_results_write_back_item_counts(void);

/* The results archive (directory 0x10 file 2): a count, then its items. */
typedef struct {
    s32 count;
    void *items[4];
} ResultArchive;
void battle_upload_party_portraits(void *portraits, s32 glyph);
void *battle_heap_alloc(s32 size, s32 mode);        /* heap allocate */

/* Battle exit (battle_results_leave_battle). */
typedef struct {
    void *data;
    u8 pad[0x5C];
} BattleBlock;
extern BattleBlock battle_message_pixel_blocks[8]; /* every other one is released */
void battle_leave(void);                           /* levels A and B per slot before the battle */

#endif
