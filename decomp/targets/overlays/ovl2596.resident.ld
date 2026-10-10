/* Resident and battle overlay symbols the ovl2596 source addresses from their
 * base, where the generated undefined-symbol list only names the accessed
 * fields. */
game_data = 0x8006D634; /* game data */
formation_active = 0x8006F9DC; /* the battle's formation (resident/formation.h) */
battle_area = 0x800C3EB0; /* battle area: slots, outcome, ordering table, draw buffer */
battle_work_area = 0x800CCCE8; /* battle work area: combatants and results state */
battle_work_growth_file = battle_work_area + 0x5F20; /* 800D2C08: its growth (battle/work.h BattleWork) */
/* Members of battle's objects, by names of ovl2596's own. */
battle_enemy_reactions = 0x800C3D18; /* the enemies' reaction state (EnemyReaction, 4 bytes each) */
battle_enemy_no_reward_flags = battle_enemy_reactions + 0x3; /* 800C3D1B: [0].unk3 */
battle_icon_cells = 0x800D2F68; /* the menu icon cells (IconCell, 4 bytes each) */
battle_spoils_icon_cells = battle_icon_cells + 0x28; /* 800D2F90: [10] */
battle_skill_mark_icon_cell = battle_icon_cells + 0x38; /* 800D2FA0: [14] */
battle_slot_flags = 0x800D32A0; /* the per-slot flags (SlotFlags, 8 bytes each) */
battle_slot_levels = battle_slot_flags + 0x5; /* 800D32A5: [0].unk2[3] */
battle_message_entries = 0x800D36C8; /* the message entries (BattleMessage, 0x60 bytes each) */
battle_message_pixel_blocks = battle_message_entries + 0x58; /* 800D3720: [0].pixels */
/* The nine decimal digits battle_split_decimal_digits writes (u8[9]); the
 * screens read the last ones by digit position from bases of their own
 * (battle_results.h), most of them before the digits (ovl2596.mk
 * BASE_VIEWS). */
battle_decimal_digits = 0x800C3CF4; /* battle's decimal digits */
battle_decimal_digits_minus_29 = battle_decimal_digits - 0x1D; /* 800C3CD7: [2]-[8] at 31-37 */
battle_decimal_digits_minus_21 = battle_decimal_digits - 0x15; /* 800C3CDF: [1]-[8] at 22-29 */
battle_decimal_digits_minus_17 = battle_decimal_digits - 0x11; /* 800C3CE3: [6]-[8] at 23-25 */
battle_decimal_digits_minus_7 = battle_decimal_digits - 0x7; /* 800C3CED: [6]-[8] at 13-15 */
battle_decimal_digits_minus_3 = battle_decimal_digits - 0x3; /* 800C3CF1: [6]-[8] at 9-11 */
battle_decimal_digits_plus_6 = battle_decimal_digits + 0x6; /* 800C3CFA: [6]-[8] at 0-2 */
battle_decimal_digits_plus_7 = battle_decimal_digits + 0x7; /* 800C3CFB: [7]-[8] at 0-1 */
