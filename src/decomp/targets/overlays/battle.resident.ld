/* Resident symbols the battle source addresses from their base, where the
 * generated undefined-symbol list only names the accessed fields. */
game_data = 0x8006D634; /* game data */
game_data_party_state = game_data + 0x1D30; /* 8006F364: game data party state */
sprite_task_draw_unlit_tmd = 0x80025A88; /* resident sprite drawer */
/* A base 0x80 bytes before the resident's command lengths of sprite VM
 * commands 80-ff (sprite_vm_command_lengths, u8[0x80], from splat's list),
 * which battle_sprite_vm.c indexes by the opcode itself (battle.mk
 * BASE_VIEWS). */
sprite_vm_command_lengths_by_opcode = sprite_vm_command_lengths - 0x80; /* 8004FC40 */
