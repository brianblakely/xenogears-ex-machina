/* Resident and battle overlay symbols the ovl3087 source addresses from
 * their base, where the generated undefined-symbol list only names the
 * accessed fields. */
game_data = 0x8006D634; /* game data */
formation_active = 0x8006F9DC; /* the battle's formation (its script set at +3) */
battle_drawing_state = 0x800CCB04; /* battle drawing state */
battle_work_area = 0x800CCCE8; /* battle combatant records (0x170 each) */
battle_slot_flags = 0x800D32A0; /* battle per-slot flags (8 bytes each) */
