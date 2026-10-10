#ifndef OVL2615_BATTLE_SETUP_H
#define OVL2615_BATTLE_SETUP_H

/* The battle setup unit (battle_setup_phases.c): the battle overlay's objects the setup
 * fills come from the shared battle headers (the area and work area, the
 * turn, menu, AI, graphics and UI state, the item lists and the set-up
 * flags); here are the setup's own views where its code reads an object
 * differently (the work area with the party ids past it, the slots' states,
 * the formation groups beside its scene data view), the formation record,
 * the command menu sources and the enemy files' read list. The scene data
 * comes from scene.h. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/actions.h"
#include "battle/area.h"
#include "battle/command.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/graphics.h"
#include "battle/groups.h"
#include "battle/item_command.h"
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
#include "resident/pad.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "ovl2615.h"
#include "scene.h"

/* Callers convert arguments/result differently from the resident definition
 * (a narrow result, u16 coordinates or another parameter count there):
 * random numbers, text rendering and the text palettes. */
s32 mode_get_random_byte_in_range(s32 low, s32 high); /* random number in [low, high] */
void window_render_text_line(void *text, void *image, s32 mode, s32 flags);
void text_load_palette(s32 x, s32 y);                 /* upload the text palettes */

/* The game data's inGear bytes (+0x22B1) as the setup reads them, one per
 * slot: past the three party entries they are the bytes that follow. */
extern u8 game_data_slot_in_gear[SLOT_COUNT];
/* The work area as the setup declares it: BattleWork, then (past it) the
 * battle overlay's item lists and more, to the party's character ids at
 * 0x603C (battle_party_character_ids, 0x7F none). The setup addresses the ids as members of
 * the work area, from its symbol or a register holding part of it, which
 * neither the separate symbol nor a cast of BattleWork reproduces, so this
 * view of the whole object shares the work area's assembler name. */
typedef struct {
    BattleWork work;           /* 0x0000 */
    u8 pad5FC8[0x603C - 0x5FC8];
    u8 partyIds[3];            /* 0x603C */
} SetupWork;
extern SetupWork battle_setup_work_area __asm__("battle_work_area");

extern u8 battle_unread_setup_flag;

/* The battle's allocators, which it declares with integer results. */
void *battle_heap_alloc_text_image(s32 kind);
void *battle_heap_alloc(s32 size, s32 flags); /* heap allocation */

void battle_setup_reset_outcome_and_slots(void);
void battle_setup_place_formation(void);
void battle_setup_copy_enemy_records_and_ai(void);
void battle_setup_derive_stats_and_slot_states(void);
void battle_setup_build_item_lists(void);
void battle_setup_init_turn_order_and_timers(void);
void battle_setup_init_command_menus(void);
void battle_setup_load_party_and_enemy_files(void);
void battle_setup_init_atb_gauges(void);
void battle_setup_init_panel_quads(void);
void battle_setup_render_digit_text_images(void);
void battle_setup_build_party_panel_glyphs(void);
void battle_setup_init_gauges_and_panel_glyphs(void);
void battle_setup_init_panel_quads_and_digits(void);

/* The game data's inventory as the item lists read it, and the battle's item
 * lists (0x30 entries, battle/actions.h). */
#define INVENTORY_SLOTS 150
#define BATTLE_ITEMS 48

extern u8 battle_work_command_index;

/* Per-slot battle state (0x800D32A1, 8 bytes per slot). */
typedef struct {
    u8 in_gear;     /* the slot fights in a gear (battle: battle_slot_flags.unk1) */
    u8 pad1[2];
    u8 character_b; /* from the character table */
    u8 stat62;      /* copied from the record */
    u8 stat63;
    u8 pad6[2];
} SlotState;

typedef struct {
    SlotState party[3];
    SlotState enemy[8];
} BattleSlotStates;

extern BattleSlotStates battle_slot_states;

void battle_derive_party_stats(void); /* derive the party's battle stats */
void battle_set_debug_party_stats(void); /* demo battle members */

/* Enemy data file: u16 script offsets per enemy id (each enemy's four AI
 * script offsets, battle/enemy_ai.h), the name table's offset at +0x30, then
 * 0x170-byte combatant records from +0x32. */
extern u8 *battle_enemy_data_file;

/* The battle's formation (formation_active, resident/formation.h), as the setup
 * reads it. FORMATION_FLAG6 indexes the enemy groups by slot (3-10), not by
 * enemy: slots 8-10 read the three bytes after the record. */
#define FORMATION_FLAGS formation_active.flags
#define FORMATION_PARTY_GROUP(member) formation_active.partyGroups[member]
#define FORMATION_ENEMY_ID(enemy) formation_active.enemyIds[enemy]
#define FORMATION_ENEMY_FLAGS(enemy) formation_active.enemyFlags[enemy]
#define FORMATION_ENEMY_GROUP(enemy) formation_active.enemyGroups[enemy]
#define FORMATION_FLAG6(slot) formation_active.enemyGroups[slot]
extern u8 *battle_command_layouts_by_character[];        /* command menu layouts */
extern u8 *battle_command_layout_gear_ptr;               /* command menu sources */
extern u8 *battle_command_layout_gear_second_ptr;
extern u8 *battle_command_layout_gear_character7_ptr;

u8 battle_is_target_at_lower_x(u8 slot, u8 target); /* facing towards the target */

/* The formation data battle_formation is the scene data (the resident's 8005949c)
 * to the setup, which reads its positions unsigned (the battle's Formation,
 * battle/formation.h, reads them signed). */
extern BattleScene *battle_formation;
extern u8 battle_enemy_name_indices_by_slot[SLOT_COUNT];

u16 battle_get_slot_bit(s32 index); /* bit of a group member index */

/* The enemy files' disc read list (0x800D33E8): entries of a file number
 * and a destination, ended by file 0. Its fields are separate variables. */
extern u16 battle_enemy_read_list;                /* entry 0 file */
extern void *battle_enemy_read_list_destination0; /* entry 0 destination */
extern u16 battle_enemy_read_list_file1;
extern void *battle_enemy_read_list_destination1;
extern u16 battle_enemy_read_list_end;
extern void *battle_enemy_read_list_end_destination;

void battle_upload_party_portraits(void *portraits, s32 glyph);

#endif
