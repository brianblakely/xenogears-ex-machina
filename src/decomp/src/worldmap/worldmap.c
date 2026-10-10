/* World map unit 80070CFC-80072238 (rodata 8006FAF0-8006FAF4): the overlay
 * entry, the frame loop with its pause, menu, encounter and gear-boarding
 * checks, the open map's frame step, the area file sets and their loaders.
 *
 * The overlay's number opens the image as this unit's rodata, the first
 * word of each mode overlay (docs/matching.md). None of these functions
 * owns rodata, so where the next unit's text starts is not measured; the
 * split starts it at that unit's first rodata owner, worldmap_open_map_start. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "party.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* The frame loop and the area's file set choice, which the overlay entry
 * calls ahead of their definitions. */
void worldmap_run_frame_loop(void);
void worldmap_select_area_files(s32 index, s32 position);

/* The overlay's number, the first word of each mode overlay (field 4, world
 * map 5, battle 6, menu 7, movie 8). */
const s32 worldmap_overlay_number = 5; /* 8006FAF0 */

/* 80070CFC: Overlay entry: set up the display, start a new game's world state if none
 * is set, enter the requested mode and run its main loop until the world
 * map is left, then hand over to the next scene. */
void worldmap_main(void) {
    void (*step)(void);
    void *data;
    RECT rect;
    s32 mode;
    s32 i;

    worldmap_sync_and_flush_cache();
    DrawSync(0);
    VSync(0);
    VSyncCallback(pad_vblank_callback);
    InitGeom();
    sprite_in_worldmap = 1;
    if (game_data_worldmap_flag_word[0] == 0) {
        game_data.map = 0x400;
        game_data.entry[1] = 0xFFF;
        game_data.entry[0] = 0xC00;
        game_data_worldmap_flag_word[0] = 1;
        game_data.worldmap.flags = 0x4003;
        game_data.worldmap.unk6A = 1;
        game_data.worldmap.unk60 = 0x6680;
        game_data.worldmap.unk62 = 0xFF00;
        game_data.worldmap.unk64 = 0x2A00;
        game_data.worldmap.vehicle_heading = 0;
        game_data.worldmap.x = 0x7580;
        game_data.worldmap.z = 0x2C00;
        game_data.worldmap.heading = 0;
        game_data.party[1] = 0xA;
        game_data.party[2] = 5;
        game_data.party[0] = 0;
        game_data.inGear[0] = 0;
        game_data.inGear[1] = 0;
        game_data.inGear[2] = 0;
        game_data.characters[0].gearId = 0xF;
        game_data.characters[1].gearId = 2;
        game_data.characters[2].gearId = 3;
        game_data.characters[3].gearId = 4;
        game_data.characters[4].gearId = 5;
        game_data.characters[5].gearId = 6;
        game_data.characters[6].gearId = 9;
        game_data.characters[7].gearId = 7;
        game_data.characters[8].gearId = 8;
        game_data.characters[9].gearId = 3;
        game_data.characters[10].gearId = 9;
        VEHICLE_SPOTS[0].flags = 0x400;
        VEHICLE_SPOTS[0].x = 0x7500;
        VEHICLE_SPOTS[0].z = 0x2E58;
        VEHICLE_SPOTS[1].flags = 0x400;
        VEHICLE_SPOTS[1].x = 0x7580;
        VEHICLE_SPOTS[1].z = 0x2E58;
        VEHICLE_SPOTS[2].flags = 0x400;
        VEHICLE_SPOTS[2].x = 0x7600;
        VEHICLE_SPOTS[2].z = 0x2E58;
        game_data.unk1844[2] = 1;
        game_data.unk1844[0] = worldmap_ferry_waypoints_x[game_data.unk1844[1]];
        game_data.unk1844[1] = worldmap_ferry_waypoints_z[game_data.unk1844[1]];
        MAP_FLAGS = 0x7FFFFFF;
    }
    heap_select_owner_tag(3, 0);
    cd_select_directory(0x24, 0);
    worldmap_stream_reset();
    worldmap_alloc_ots();
    worldmap_choose_movement_mode();
    if (game_data_worldmap_flag_word[0] & 0x8000) {
        worldmap_resuming = 1;
    } else {
        worldmap_resuming = 0;
    }
    worldmap_next_scene_chosen = 0;
    mode = game_data_worldmap_flag_word[0] & 0x7FFF;
    worldmap_entry_map_offset = (game_data.map & 0x3FFF) - 0x400;
    worldmap_entry_index = game_data.entry[1];
    worldmap_player_heading = game_data.entry[0];
    worldmap_mode_index = mode;
    game_data.entry[2] = mode;
    worldmap_select_area_files(mode, game_data.vars[0]);
    step = worldmap_mode_handlers[worldmap_mode_index].enter;
    if (step != NULL) {
        step();
    }
    while (worldmap_loop_result >= 2) {
        worldmap_mode_handlers[worldmap_mode_index].start();
        worldmap_actor_run_all();
        DrawSync(0);
        VSync(0);
        pad_clear_queue();
        worldmap_resuming = worldmap_loop_result;
        worldmap_run_frame_loop();
        step = worldmap_mode_handlers[worldmap_mode_index].leave;
        step();
    }
    switch (worldmap_loop_result) {
    case 0:
        mode_load_overlay_block(1);
        mode_select_next_mode(1);
        if (worldmap_next_scene_chosen == 0) {
            if (worldmap_current_path->kind == 3) {
                worldmap_path_select_region_of_kind(&worldmap_camera_follow_target.target, 3, game_data.vars[0]);
            }
            game_data.map = worldmap_current_path->scene;
            game_data.entry[0] = worldmap_camera_angle.vy;
            game_data.entry[2] = worldmap_current_path->entry;
        }
        game_data.vars[2] = worldmap_entry_map_offset + 0x400;
        break;
    case 1:
        mode_load_overlay_block(2);
        mode_select_next_mode(2);
        mode_battle_standalone = 0;
        for (i = 0; i < 3; i++) {
            (&game_data.worldmap.unk70)[i] = game_data.inGear[i];
        }
        sound_stop_all_seqs();
        data = worldmap_battle_music;
        memcpy(mode_music_buffer, data, cd_get_aligned_file_size(worldmap_battle_music_file));
        mode_music_cached_seq = mode_music_seq;
        mode_music_seq = (s32)sound_create_seq((SoundSeqHeader *)mode_music_buffer);
        sound_play_seq((SoundSeq *)mode_music_seq, 0x7F, 0);
        break;
    default:
        mode_select_next_mode(0);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x13F;
        rect.h = 0x1AF;
        ClearImage(&rect, 0, 0, 0x40);
        DrawSync(0);
        break;
    }
    sprite_in_worldmap = 0;
    worldmap_sync_and_flush_cache();
    mode_dispatch(0);
}

/* 800712D0: The world-map main loop: gather input, flip the display buffers, run the
 * frame, and handle pause, encounters and leaving for another scene until
 * worldmap_loop_running clears. */
void worldmap_run_frame_loop(void) {
    DisplayBuffer *view;
    /* The live gear-byte base also reaches the party IDs 0x57D bytes earlier. */
    u8 *gear_state = game_data.inGear;
    RECT rect;
    s32 i;
    s32 found;

    worldmap_current_display_buffer = &worldmap_display_buffers[1];
    worldmap_display_buffer_index = 1;
    worldmap_loop_running = 1;
    do {
        worldmap_pad_unread_port1_repeated = 0;
        worldmap_pad_unread_port1_pressed = 0;
        worldmap_pad_unread_port1_held = 0;
        worldmap_pad_unread_port0_repeated = 0;
        worldmap_pad_port0_pressed = 0;
        worldmap_pad_port0_held = 0;
        while (pad_dequeue_state() != 0) {
            worldmap_pad_port0_held |= pad_port0_held;
            worldmap_pad_unread_port1_held |= pad_port1_held;
            worldmap_pad_port0_pressed |= pad_port0_pressed;
            worldmap_pad_unread_port1_pressed |= pad_port1_pressed;
            worldmap_pad_unread_port0_repeated |= pad_port0_repeated;
            worldmap_pad_unread_port1_repeated |= pad_port1_repeated;
        }
        while (worldmap_stream_step() == 3) {
            VSync(0);
        }
        CdSync(1, worldmap_cd_sync_result);
        view = worldmap_display_buffers;
        if (worldmap_current_display_buffer == view) {
            view = &worldmap_display_buffers[1];
        }
        worldmap_current_display_buffer = view;
        worldmap_display_buffer_index = worldmap_display_buffer_index == 0;
        ClearOTagR(view->ot, 0x400);
        sprite_queue_start_fill(worldmap_display_buffer_index);
        sprite_build_pending_frames();
        worldmap_actor_run_all();
        DrawSync(0);
        VSync(2);
        boot_check_soft_reset();
        PutDispEnv(&worldmap_current_display_buffer->disp);
        PutDrawEnv(&worldmap_current_display_buffer->draw);
        if (mode_gear_riding_lock == 0 && worldmap_button_combo_pressed != 0 && worldmap_screen_fade_active == 0 && worldmap_menu_requested == 0 &&
            worldmap_path_name_id == -1 && worldmap_destination_name_id == worldmap_path_name_id && worldmap_loop_running != 0 && worldmap_encounter_expired_count == 0) {
            worldmap_button_combo_pressed = 0;
            if (worldmap_terrain_get_layer(&worldmap_camera_follow_target.target) != 4) {
                for (i = 0; i < 3; i++) {
                    (&game_data.worldmap.unk70)[i] = game_data.inGear[i];
                }
                if (gear_state[0] != 0) {
                    gear_state[2] = 0;
                    gear_state[1] = 0;
                    gear_state[0] = 0;
                } else {
                    if (game_data.characters[(gear_state - 0x57D)[0]].gearId != 0xFF) {
                        gear_state[0] = 1;
                    }
                    if (game_data.characters[(gear_state - 0x57D)[1]].gearId != 0xFF) {
                        gear_state[1] = 1;
                    }
                    if (game_data.characters[(gear_state - 0x57D)[2]].gearId != 0xFF) {
                        gear_state[2] = 1;
                    }
                }
                worldmap_apply_party_riding_changes();
            }
        } else {
            worldmap_button_combo_pressed = 0;
        }
        if (worldmap_screen_fade_active == 0) {
            if (worldmap_menu_requested == 0 && worldmap_loop_running != 0 && worldmap_encounter_expired_count == 0 && (worldmap_pad_port0_pressed & 0x800)) {
                worldmap_run_pause_screen();
            }
            if (worldmap_screen_fade_active == 0) {
                if (worldmap_menu_requested == 0 && worldmap_loop_running != 0 && worldmap_encounter_expired_count == 0 && pad_get_controller_kind(0) == 0) {
                    worldmap_wait_for_controller();
                }
                if (worldmap_screen_fade_active == 0 && worldmap_menu_requested == 0 && worldmap_path_name_id == -1 &&
                    worldmap_destination_name_id == worldmap_path_name_id && worldmap_loop_running != 0 && worldmap_encounter_expired_count != 0) {
                    found = worldmap_encounter_roll(&worldmap_camera_follow_target.target, game_data.vars[0]);
                    if (found == 1) {
                        worldmap_loop_running = 0;
                        worldmap_loop_result = found;
                        mode_battle_kind = 0;
                        game_data.worldmap.unk70 = game_data.inGear[0];
                        game_data.worldmap.unk72 = game_data.inGear[1];
                        game_data.worldmap.unk74 = game_data.inGear[2];
                    }
                }
            }
        }
        worldmap_encounter_expired_count = 0;
        if (worldmap_pad_port0_pressed & 0x100) {
            u16 *camera_mode = &game_data.worldmap.unk76;
            *camera_mode ^= 1;
        }
        if (worldmap_screen_fade_active == 0 && worldmap_menu_requested != 0 && worldmap_loop_running != 0) {
            if (worldmap_movement_mode > 0) {
                if (worldmap_movement_mode < 4) {
                    worldmap_suspend_open_map();
                    menu_state_screen = 0;
                    menu_state_debug_start = 0;
                    menu_state_screen_parameter = 1;
                    worldmap_sync_and_flush_cache();
                    mode_run_menu();
                    worldmap_sync_and_flush_cache();
                    worldmap_resume_open_map();
                } else if (worldmap_movement_mode < 8) {
                    worldmap_loop_running = 0;
                    worldmap_loop_result = 0;
                    worldmap_current_path = &worldmap_fixed_path_regions[2];
                    game_data.worldmap.flags |= 0x2000;
                }
            }
        } else {
            worldmap_menu_requested = 0;
        }
        sprite_queue_run_uploads();
        worldmap_texture_anim_advance();
        worldmap_texture_anim2_advance();
        SetGeomOffset(0xA0, worldmap_view_center_y);
        DrawOTag(worldmap_current_display_buffer->ot + 0x3FF);
    } while (worldmap_loop_running != 0);
    ResetGraph(1);
    if (worldmap_display_buffer_index == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    worldmap_stream_drain();
    DrawSync(0);
    VSync(0);
    PutDispEnv(&worldmap_display_buffers[1].disp);
}

/* 80071A50: Mode step that has nothing to do; always reports done. */
s32 worldmap_open_map_frame_start(void) {
    return 1;
}

/* 80071A58: One world-map frame: input, actors, camera, terrain, sky and HUD. */
s32 worldmap_open_map_frame_update(void) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */

    if (worldmap_view_kind == 0) {
        worldmap_camera_build_from_angles(&worldmap_view_setup);
    } else {
        worldmap_camera_build_look_at(&worldmap_view_setup);
    }
    worldmap_effects_run_emitters();
    worldmap_effects_draw_particles();
    worldmap_actor_draw_sprites();
    worldmap_billboards_draw();
    worldmap_footprints_draw();
    worldmap_objects_draw();
    worldmap_terrain_wrap_origin(&worldmap_terrain_origin);
    if (worldmap_terrain_crossed_edges != 0) {
        worldmap_terrain_update_grid(&worldmap_camera);
        worldmap_stream_wait_for_slot();
        worldmap_terrain_load_new_edges();
    }
    worldmap_terrain_classify_blocks(&worldmap_camera);
    worldmap_terrain_draw(worldmap_current_display_buffer->ot, (s32)worldmap_current_display_buffer->packets, &worldmap_camera);
    worldmap_wave_phase_x += 0x40;
    worldmap_horizon_draw();
    worldmap_sky_draw();
    worldmap_clouds_draw();
    if (game_data.worldmap.unk76 == 0) {
        worldmap_map_overlay_draw();
    }
    return 1;
}

/* 80071B9C: Select the file set of an area (by index, or for the low indices by the
 * position against the threshold table) and derive its file numbers. */
void worldmap_select_area_files(s32 index, s32 position) {
    WorldmapArea *area;
    s32 i;

    if (index < 8) {
        for (i = 1; position >= worldmap_area_thresholds[i]; i++) {
        }
        area = &worldmap_area_file_sets[i - 1];
        worldmap_area_index = i - 1;
    } else {
        area = &worldmap_area_file_sets[index + 1];
    }
    worldmap_area_data_file = area->file + 1;
    worldmap_terrain_image_file = area->file + 2;
    worldmap_area_image_file = area->file + 3;
    worldmap_wave_bank_file = area->file + 4;
    worldmap_music_file = area->file + 5;
    worldmap_sound_bank_file = area->file + 6;
    worldmap_flight_music_file = area->file + 7;
    worldmap_battle_music_file = area->file + 8;
    worldmap_terrain_row_file = area->file + 9;
    worldmap_terrain_column_file = area->file + 10;
    worldmap_area_blocks_x = area->param2;
    worldmap_area_blocks_z = area->param4;
    worldmap_loop_result = area->param6;
}

/* 80071CDC: Allocate buffers for each party member's model and gear model, then read
 * them all with one list. */
void worldmap_read_party_models(void) {
    s32 i;
    s32 j;
    s32 member;
    u8 gear;

    for (i = 0; i < 3; i++) {
        member = game_data.party[i];
        if (member != 0xFF) {
            worldmap_character_models[i] = heap_alloc(cd_get_aligned_file_size(member + 2), 0);
            j = game_data.characters[member].gearId;
            if (j != 0xFF) {
                worldmap_gear_models[i] = heap_alloc(cd_get_aligned_file_size(j + 0x13), 0);
            } else {
                worldmap_gear_models[i] = NULL;
            }
        } else {
            worldmap_gear_models[i] = NULL;
            worldmap_character_models[i] = NULL;
        }
    }
    i = 0;
    worldmap_party_count = 0;
    for (j = 0; j < 3; j++) {
        member = game_data.party[j];
        if (member != 0xFF) {
            worldmap_read_list[i].file = member + 2;
            worldmap_read_list[i].destination = worldmap_character_models[j];
            i++;
            worldmap_party_count++;
            gear = game_data.characters[member].gearId;
            if (gear != 0xFF) {
                worldmap_read_list[i].file = gear + 0x13;
                worldmap_read_list[i].destination = worldmap_gear_models[j];
                i++;
            }
        }
    }
    worldmap_read_list[i].file = 0;
    worldmap_read_list[i].destination = NULL;
    cd_read_file_list(worldmap_read_list, 0, 0);
}

/* 80071EF0: Allocate and read the three area files into their resident buffers. */
void worldmap_read_area_files(void) {
    worldmap_terrain_image = heap_alloc(cd_get_aligned_file_size(worldmap_terrain_image_file), 1);
    worldmap_area_image = heap_alloc(cd_get_aligned_file_size(worldmap_area_image_file), 1);
    worldmap_area_data = heap_alloc(cd_get_aligned_file_size(worldmap_area_data_file), 1);
    worldmap_read_list[0].file = worldmap_area_data_file;
    worldmap_read_list[1].file = worldmap_terrain_image_file;
    worldmap_read_list[2].file = worldmap_area_image_file;
    worldmap_read_list[3].file = 0;
    worldmap_read_list[0].destination = worldmap_area_data;
    worldmap_read_list[1].destination = worldmap_terrain_image;
    worldmap_read_list[2].destination = worldmap_area_image;
    worldmap_read_list[3].destination = NULL;
    cd_read_file_list(worldmap_read_list, 0, 0);
}

/* 80071FEC: Allocate and read the two shared world-map files (0x25, 0x26). */
void worldmap_read_shared_files(void) {
    menu_state_resource_file = heap_alloc(cd_get_aligned_file_size(0x26), 1);
    worldmap_packed_menu_overlay = heap_alloc(cd_get_aligned_file_size(0x25), 1);
    worldmap_read_list[0].file = 0x25;
    worldmap_read_list[0].destination = worldmap_packed_menu_overlay;
    worldmap_read_list[1].file = 0x26;
    worldmap_read_list[1].destination = menu_state_resource_file;
    worldmap_read_list[2].file = 0;
    worldmap_read_list[2].destination = NULL;
    cd_read_file_list(WORLD_READ_LIST, 0, 0);
}

/* 80072090: Allocate the area's five file buffers and read the zero-terminated list. */
void worldmap_read_area_sound_files(void) {
    mode_worldmap_area_load_count++;
    worldmap_read_list[0].file = worldmap_wave_bank_file;
    worldmap_read_list[0].destination = worldmap_wave_bank = heap_alloc(cd_get_aligned_file_size(worldmap_wave_bank_file), 1);
    worldmap_read_list[1].file = worldmap_music_file;
    worldmap_read_list[1].destination = worldmap_music = heap_alloc(cd_get_aligned_file_size(worldmap_music_file), 0);
    worldmap_read_list[2].file = worldmap_sound_bank_file;
    worldmap_read_list[2].destination = sound_effect_bank = heap_alloc(cd_get_aligned_file_size(worldmap_sound_bank_file), 0);
    worldmap_read_list[3].file = worldmap_flight_music_file;
    worldmap_read_list[3].destination = worldmap_flight_music = heap_alloc(cd_get_aligned_file_size(worldmap_flight_music_file), 0);
    worldmap_read_list[4].file = worldmap_battle_music_file;
    worldmap_read_list[4].destination = worldmap_battle_music = heap_alloc(cd_get_aligned_file_size(worldmap_battle_music_file), 0);
    worldmap_read_list[5].file = 0;
    worldmap_read_list[5].destination = NULL;
    cd_read_file_list(WORLD_READ_LIST, 0, 0);
}

/* 800721E4: Allocate and read the area's sixth file (kept, mode 0). */
void worldmap_read_area_sound_bank(void) {
    sound_effect_bank = heap_alloc(cd_get_aligned_file_size(worldmap_sound_bank_file), 0);
    cd_read_file(worldmap_sound_bank_file, sound_effect_bank, 0, 0);
}
