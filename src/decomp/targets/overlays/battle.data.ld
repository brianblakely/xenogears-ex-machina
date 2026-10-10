/* Names the battle source uses for addresses inside a C data object, each
 * from that object's linked address. battle.c indexes the timer reload table
 * battle_ap_timer_reload_table (maximum AP 3-7) by the maximum AP itself, from three 8-byte
 * rows before it, and the combo step table battle_combo_next_step_table by the AP paid (1-3),
 * from one byte before it: GCC 2.6.3 forms that base with `la` only for a
 * declared array (battle_combo_next_step_table[step][paid - 1] puts the -1 in the load). */
battle_ap_timer_reload_table_by_max_ap = battle_ap_timer_reload_table - 0x18; /* 800C31D4 */
battle_combo_next_step_table_by_paid = battle_combo_next_step_table - 0x1; /* 800C34B3 */

/* The same for the commons (battle_common.c). The units address parts of the
 * battle area battle_area (area.h), of its battle work (combatant.h) and of
 * the camera, image animation and message entries by names of their own,
 * with views of their own (BattleDraw from .ot, ActionCommit from
 * .work.targetMask). */
battle_area_slots = battle_area + 0x4;    /* 800C3EB4: .slots */
battle_area_events = battle_area + 0x138;  /* 800C3FE8: .events */
battle_area_event0_codes = battle_area + 0x150;  /* 800C4000: .events[0] + 0x18 */
battle_area_knocked_out = battle_area + 0xA38;  /* 800C48E8: .knockedOut */
battle_area_outcome = battle_area + 0xA3A;  /* 800C48EA: .outcome */
battle_acting_slot = battle_area + 0xA72;  /* 800C4922: .path[9] onwards */
battle_sound_bank_of_event_script = battle_area + 0xA74; /* 800C4924 */
battle_acting_with_partner = battle_area + 0xA78; /* 800C4928 */
battle_unread_gear_attack_step_flag = battle_area + 0xA79; /* 800C4929 */
battle_turn_hud_hidden = battle_area + 0xA7A; /* 800C492A */
battle_area_buffer0_background_color = battle_area + 0xB89;  /* 800C4A39: .buffers[0].drawEnv.r0 */
battle_drawing_state = battle_area + 0x8C54; /* 800CCB04: .ot */
battle_drawing_buffer_byte = battle_area + 0x8C84; /* 800CCB34: .buffer */
battle_light_matrix = battle_area + 0x8CE4; /* 800CCB94 */
battle_turns_active = battle_area + 0x8DA8; /* 800CCC58: .field8DA8 */
battle_frame_ticks = battle_area + 0x8DAC; /* 800CCC5C: .frameTicks */
battle_work_area = battle_area + 0x8E38; /* 800CCCE8: .work */
battle_item_effects = battle_work_area + 0x5518; /* 800D2200: .work.lists + 0x20: the item effects */
battle_gear_hud = battle_work_area + 0x5F24; /* 800D2C0C: .work.gearHud */
battle_gear_hud_attack = battle_work_area + 0x5F42; /* 800D2C2A */
battle_gear_hud_warning_flags = battle_work_area + 0x5F48; /* 800D2C30 */
battle_gear_hud_charge = battle_work_area + 0x5F4A; /* 800D2C32 */
battle_gear_hud_attack_level = battle_work_area + 0x5F4C; /* 800D2C34 */
battle_gear_hud_speed = battle_work_area + 0x5F4D; /* 800D2C35 */
battle_gear_hud_defense = battle_work_area + 0x5F4E; /* 800D2C36 */
battle_gear_hud_overheat = battle_work_area + 0x5F50; /* 800D2C38 */
battle_gear_hud_boost_chance = battle_work_area + 0x5F52; /* 800D2C3A */
battle_slot_damages = battle_work_area + 0x5F6C; /* 800D2C54: .work.damage */
battle_enemy_damages = battle_work_area + 0x5F78; /* 800D2C60: .work.damage[3] */
battle_slot_result_codes = battle_work_area + 0x5FA0; /* 800D2C88: .work.resultCode */
battle_enemy_result_codes = battle_work_area + 0x5FA3; /* 800D2C8B: .work.resultCode[3] */
battle_committed_action = battle_work_area + 0x5FAC; /* 800D2C94: .work.targetMask */
battle_held_slot_mask = battle_work_area + 0x5FB6; /* 800D2C9E: .work.revived */
battle_camera_wanted_points = battle_camera + 0x4;    /* 800D30A0: camera .eye, .target */
battle_camera_angles = battle_camera + 0x14;   /* 800D30B0: .rot */
battle_camera_distance = battle_camera + 0x1C;   /* 800D30B8: .range */
battle_camera_view_matrix = battle_camera + 0x20;   /* 800D30BC: .matrix */
battle_frame_start_time = battle_camera + 0x48;   /* 800D30E4: .start */
battle_stage_image_dirty = battle_stage_image_anim + 0x11;   /* 800D3611: image animation .dirty */
battle_message_image = battle_message_entries + 0x2F0;  /* 800D39B8: message [7].rect: the battle message image */

/* The resident's mode table (8001808c, mode 2) clears 800c3a70-800d39f4 for
 * this overlay; nothing in the image addresses the end of its last
 * variable, so the link checks it. */
ASSERT(battle_BSS_START == 0x800C3A70 && battle_BSS_END == 0x800D39F4,
       "the battle .bss differs from the resident mode table's bounds")
