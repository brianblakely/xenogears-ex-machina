#ifndef RESIDENT_FORMATION_H
#define RESIDENT_FORMATION_H

#include "common.h"

/* Battle formations and encounter sets (docs/scripts/formations.md). The
 * battle overlay copies formation formation_selected_index of the encounter set formation_encounter_set
 * into formation_active as it starts (battle battle_main); the battle setup, event
 * script and results overlays read it there. Party members take battle slots
 * 0-2, enemies slots 3-10 (battle/area.h BattleSlot); "slot byte n" below is
 * byte n of a slot's BattleSlot record. The places of each formation group
 * come from the stage's scene data (mode_battle_scene_file), not from here. */

/* A battle formation (0x20 bytes). */
typedef struct BattleFormation {
    u8 battle;         /* 0x00: n: enemy file (12, 1) 2n + 2 and enemy set file
                        * 2n + 3 (ovl2615 func_801E5384) */
    u8 flags;          /* 0x01: 0x08 no result screens, whose spoils list adds the
                        * drops (ovl2596 func_801E2280 skips func_801E1FB8), nor the
                        * fade after them (battle battle_main); 0x10 sets
                        * battle_uses_fixed_party (ovl2615 func_801E5384): the party is party
                        * slot 0's character and character 10 twice, members 1 and
                        * 2 with gear 17; every member fights in a gear
                        * (func_801E4048), so partyGroups is not read
                        * (func_801E4160); members 1 and 2 get fixed HP and stats
                        * (battle battle_set_debug_party_stats, from func_801E4AC0), character
                        * 11's portrait (battle battle_upload_party_portraits) and no place in the
                        * results (ovl2596 func_801E2280); 0x20 the event script runs
                        * (func_801E5014 sets battle_uses_event_script); 0x40 and 0x80 give every
                        * member command 7 and 8 (func_801E5014). No reader tests
                        * 0x01, 0x02 or 0x04. */
    u8 stage;          /* 0x02: s, the battle stage: its stage file (12, 3) 6 + 2s
                        * and scene data 7 + 2s (resident mode_load_current_battle_stage,
                        * mode_load_battle_stage; ovl2615 func_801E7210 sets them up) */
    u8 scriptSet;      /* 0x03: the event script set, read under flag 0x20 (ovl3087
                        * func_801E5160) */
    u8 partyGroups[3]; /* 0x04: per member, & 0x7f: its formation group unless it
                        * fights in its gear, as all do under flag 0x10 (ovl2615
                        * func_801E4160) */
    u8 unk7;           /* 0x07: no reader */
    u8 enemyIds[8];    /* 0x08: per enemy, & 0x7f: its id in the enemy file (0x7f
                        * none); 0x80: it fights in a gear, slot byte 4 (func_801E4160) */
    u8 enemyFlags[8];  /* 0x10: per enemy: 0x80 to slot byte 3 (BattleSlot.hidden),
                        * 0x01 to slot byte 5 (func_801E4160), which no reader is
                        * known to read; bits 0x02-0x40 unread */
    u8 enemyGroups[8]; /* 0x18: per enemy, & 0x7f: its formation group. Bit 7 of byte
                        * 0x18 + s goes to slot byte 6 for s = 3-10 (bytes 0x1b-0x22,
                        * past the record for slots 8-10, func_801E4160); setup phase
                        * 2 sets every slot's byte 6 anew (func_801E5014) */
} BattleFormation;

/* An encounter set: 16 formations. The field decodes a map's set from map
 * bundle component 6 (0x210 bytes: the set, then the 16 weights formation_encounter_weights;
 * field func_80070CC8), the world map copies a terrain kind's set from its area
 * file's tables (D_8009D73C, worldmap func_80075E7C) and the debug battle
 * selector the first 0x200 bytes of a (0x20, 3) file (ovl2606 func_801E0A34). */
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
                                    * (field func_80079288, which draws only after a
                                    * script of the map arms it with event f7,
                                    * func_8008E85C) */
extern u8 formation_selected_index;              /* the formation the battle copies: drawn by the field
                                    * (func_80079288) or the world map (func_80075E7C),
                                    * named by a field script (events 71, fe 84), or
                                    * mode_pending_battle_formation - 1 (battle battle_main) */
extern BattleFormation formation_active; /* the battle's formation */

#endif
