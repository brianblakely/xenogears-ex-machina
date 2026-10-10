/* Resident and battle overlay symbols the ovl2615 source addresses from their
 * base, where the generated undefined-symbol list only names the accessed
 * fields. */
game_data = 0x8006D634; /* game data */
game_data_slot_in_gear = game_data + 0x22B1; /* 8006F8E5: its inGear (resident/gamedata.h GameData) */
battle_enemy_reactions = 0x800C3D18; /* the enemies' reaction state (4 bytes each) */
/* Members of battle's objects, by names of ovl2615's own;
 * battle_command_layouts_by_character and battle_work_area come from splat's
 * list (ovl2615.symbols.txt). */
battle_command_layout_gear_ptr = battle_command_layouts_by_character + 0x40; /* 800C2130: [16] */
battle_command_layout_gear_second_ptr = battle_command_layouts_by_character + 0x44; /* 800C2134: [17] */
battle_command_layout_gear_character7_ptr = battle_command_layouts_by_character + 0x48; /* 800C2138: [18] */
battle_work_command_index = battle_work_area + 0x5FC2; /* 800D2CAA: its commandIndex (battle/work.h BattleWork) */
battle_slot_flags = 0x800D32A0; /* the per-slot flags (SlotFlags, 8 bytes each) */
battle_slot_states = battle_slot_flags + 0x1; /* 800D32A1: [0].unk1 */
battle_stage_image_anim = 0x800D3600; /* the stage's image animation (battle/effect.h ImageAnim) */
battle_stage_image_anim_active = battle_stage_image_anim + 0x1A; /* 800D361A: its active */
/* A base three bytes before battle's enemy name indices (u8[8], one per
 * enemy), from which the setup stores them by slot, 3-10 (ovl2615.mk
 * BASE_VIEWS). */
battle_enemy_name_indices = 0x800C3E40; /* enemy name per enemy slot (3-10) */
battle_enemy_name_indices_by_slot = battle_enemy_name_indices - 0x3; /* 800C3E3D */
