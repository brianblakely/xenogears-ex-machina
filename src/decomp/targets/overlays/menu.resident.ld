/* Resident symbols the menu source addresses from their base, where the
 * generated undefined-symbol list only names the accessed fields. */
game_data = 0x8006D634; /* game data */
/* The resident's option bytes of the field and the menu (u8[6], 8005061C),
 * each by a name of its own (resident_views.h); mode_arena_task_parameters
 * comes from splat's list. */
mode_arena_entry_kind = mode_arena_task_parameters + 0x1; /* 8005061D: [1] */
mode_arena_first_model = mode_arena_task_parameters + 0x2; /* 8005061E: [2] */
mode_arena_second_model = mode_arena_task_parameters + 0x3; /* 8005061F: [3] */
mode_arena_option6 = mode_arena_task_parameters + 0x4; /* 80050620: [4] */
mode_arena_level = mode_arena_task_parameters + 0x5; /* 80050621: [5] */
