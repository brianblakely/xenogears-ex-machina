#ifndef RESIDENT_FORMATION_H
#define RESIDENT_FORMATION_H

#include "common.h"

/* Battle formations and encounter sets (docs/scripts/formations.md). The battle
 * overlay copies formation formation_selected_index of the encounter set
 * formation_encounter_set into formation_active as it starts (battle battle_main);
 * the battle setup, event script and results overlays read it there. Party members
 * take battle slots 0-2, enemies slots 3-10 (battle/area.h BattleSlot); "slot byte
 * n" below is byte n of a slot's BattleSlot record. The places of each formation
 * group come from the stage's scene data (mode_battle_scene_file), not from here. */

/* A battle formation (0x20 bytes). */
typedef struct BattleFormation {
    u8 battle;         /* 0x00: n: enemy file (12, 1) 2n + 2 and enemy set file
                        * 2n + 3 (ovl2615 battle_setup_load_party_and_enemy_files) */
    u8 flags;          /* 0x01: 0x08 no result screens, whose spoils list adds the
                        * drops (ovl2596 battle_results_grant_rewards skips battle_results_run_screens), nor the
                        * fade after them (battle battle_main); 0x10 sets
                        * battle_uses_fixed_party (ovl2615 battle_setup_load_party_and_enemy_files): the party is party
                        * slot 0's character and character 10 twice, members 1 and
                        * 2 with gear 17; every member fights in a gear
                        * (battle_setup_reset_outcome_and_slots), so partyGroups is not read
                        * (battle_setup_place_formation); members 1 and 2 get fixed HP and stats
                        * (battle battle_set_debug_party_stats, from battle_setup_derive_stats_and_slot_states), character
                        * 11's portrait (battle battle_upload_party_portraits) and no place in the
                        * results (ovl2596 battle_results_grant_rewards); 0x20 the event script runs
                        * (battle_setup_init_command_menus sets battle_uses_event_script); 0x40 and 0x80 give every
                        * member command 7 and 8 (battle_setup_init_command_menus). No reader tests
                        * 0x01, 0x02 or 0x04. */
    u8 stage;          /* 0x02: s, the battle stage: its stage file (12, 3) 6 + 2s
                        * and scene data 7 + 2s (resident mode_load_current_battle_stage,
                        * mode_load_battle_stage; ovl2615 battle_setup_build_stage sets them up) */
    u8 scriptSet;      /* 0x03: the event script set, read under flag 0x20 (ovl3087
                        * battle_event_script_load) */
    u8 partyGroups[3]; /* 0x04: per member, & 0x7f: its formation group unless it
                        * fights in its gear, as all do under flag 0x10 (ovl2615
                        * battle_setup_place_formation) */
    u8 unk7;           /* 0x07: no reader */
    u8 enemyIds[8];    /* 0x08: per enemy, & 0x7f: its id in the enemy file (0x7f
                        * none); 0x80: it fights in a gear, slot byte 4 (battle_setup_place_formation) */
    u8 enemyFlags[8];  /* 0x10: per enemy: 0x80 to slot byte 3 (BattleSlot.hidden),
                        * 0x01 to slot byte 5 (battle_setup_place_formation), which no reader is
                        * known to read; bits 0x02-0x40 unread */
    u8 enemyGroups[8]; /* 0x18: per enemy, & 0x7f: its formation group. Bit 7 of byte
                        * 0x18 + s goes to slot byte 6 for s = 3-10 (bytes 0x1b-0x22,
                        * past the record for slots 8-10, battle_setup_place_formation); setup phase
                        * 2 sets every slot's byte 6 anew (battle_setup_init_command_menus) */
} BattleFormation;

/* An encounter set: 16 formations. The field decodes a map's set from map
 * bundle component 6 (0x210 bytes: the set, then the 16 weights formation_encounter_weights;
 * field field_load_from_bundle), the world map copies a terrain kind's set from its area
 * file's tables (worldmap_encounter_sets, worldmap worldmap_encounter_roll) and the debug battle
 * selector the first 0x200 bytes of a (0x20, 3) file (ovl2606 battle_scene_select_main). */
typedef struct EncounterSet {
    BattleFormation formations[16];
} EncounterSet;

LAYOUT_CHECK(BattleFormationLayout, sizeof(BattleFormation) == 0x20 &&
                                        OFFSET_OF(BattleFormation, partyGroups) == 0x04 &&
                                        OFFSET_OF(BattleFormation, enemyIds) == 0x08 &&
                                        OFFSET_OF(BattleFormation, enemyFlags) == 0x10 &&
                                        OFFSET_OF(BattleFormation, enemyGroups) == 0x18 &&
                                        sizeof(EncounterSet) == 0x200);

extern EncounterSet formation_encounter_set;    /* the set of the next battle */
extern u8 formation_encounter_weights[16];          /* the field's random-encounter weight per formation
                                    * (field field_encounter_count_down, which draws only after a
                                    * script of the map arms it with event f7,
                                    * field_event_draw_random_picks) */
extern u8 formation_selected_index;              /* the formation the battle copies: drawn by the field
                                    * (field_encounter_count_down) or the world map (worldmap_encounter_roll),
                                    * named by a field script (events 71, fe 84), or
                                    * mode_pending_battle_formation - 1 (battle battle_main) */
extern BattleFormation formation_active; /* the battle's formation */

#endif
