/* World map unit 80072238-80077E68 (rodata 8006FAF4-8006FB40, data
 * 80099E8C-8009A3F0): the open map's start and leave handlers with its actor
 * lists and the mode table, the display set-up and screen fade, the player's
 * placement, the area data's sections, the sky, the horizon, the map overlay,
 * the footprints, the texture animations, the encounters, the state saved
 * across another scene, the pause screens, the actor script interpreter,
 * the scripted camera helpers and the set-up of scene modes 8, 9 and 11.
 *
 * GCC aligns jump tables to 8 within a unit's rodata (docs/matching.md).
 * worldmap_open_map_start's table sits at 8006faf4, 4 mod 8, right after the overlay
 * number, where one unit holding both would have aligned it to 8006faf8;
 * so this unit's rodata starts there and its text no later than
 * worldmap_open_map_start. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "ovl2143/actors.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "party.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* Actor script commands (dispatched by worldmap_actor_script_run), declared for their
 * table worldmap_actor_script_handlers in this unit's data, ahead of their code. */
s32 worldmap_actor_script_exit_worldmap(void);
s32 worldmap_actor_script_wait(WorldmapActor *actor, s16 frames);
s32 worldmap_actor_script_send_actor(WorldmapActor *actor, s32 a, s32 b);
s32 worldmap_actor_script_place_player(WorldmapActor *actor, s32 x, s32 y, s32 z);
s32 worldmap_actor_script_set_script_vector(WorldmapActor *actor, s16 x, s16 y, s16 z);
s32 worldmap_actor_script_start_emitters(WorldmapActor *actor, s32 a);
s32 worldmap_actor_script_stop_emitters(WorldmapActor *actor, s32 a);
s32 worldmap_actor_script_stop_effects(WorldmapActor *actor, s32 a);
s32 worldmap_actor_script_fade_music(WorldmapActor *actor, s32 a, s32 b);
s32 worldmap_actor_script_play_sound(WorldmapActor *actor, s32 sound);
s32 worldmap_actor_script_slide_sound_volume(WorldmapActor *actor, s32 sound, s32 b, s32 c);
s32 worldmap_actor_script_set_fade(WorldmapActor *actor, s32 a, s32 b);

/* Called by the open map's start and leave handlers ahead of their
 * definitions. */
void worldmap_restore_player_position(void);
void worldmap_place_player_at_arrival_point(s32 id);
void worldmap_unpack_area_data(void);
void worldmap_map_overlay_init(void);
void worldmap_footprints_alloc(void);
void worldmap_footprints_free(void);
void worldmap_save_state(void);
void worldmap_restore_state(void);

/* Actor spawn list entry; a zero kind ends a list. */
typedef struct {
    s32 kind;
    s32 update;
} ActorSpawn;

/* The actors started in every area (the world map's common actors). */
ActorSpawn worldmap_open_map_actors[16] = { /* 80099E8C */
    {(s32)worldmap_screen_fade_start, (s32)worldmap_screen_fade_update}, {(s32)worldmap_leader_start, (s32)worldmap_leader_update},
    {(s32)worldmap_second_member_start, (s32)worldmap_follower_update}, {(s32)worldmap_third_member_start, (s32)worldmap_follower_update},
    {(s32)worldmap_player_vehicle_start, (s32)worldmap_player_vehicle_update}, {(s32)worldmap_second_vehicle_start, (s32)worldmap_follower_vehicle_update},
    {(s32)worldmap_third_vehicle_start, (s32)worldmap_follower_vehicle_update}, {(s32)worldmap_flying_vehicle_start, (s32)worldmap_flying_vehicle_update},
    {(s32)worldmap_flying_vehicle_rotors_start, (s32)worldmap_flying_vehicle_rotors_update}, {(s32)worldmap_camera_follow_start, (s32)worldmap_camera_follow_update},
    {(s32)worldmap_camera_orbit_start, (s32)worldmap_camera_orbit_update}, {(s32)worldmap_view_center_start, (s32)worldmap_view_center_update},
    {(s32)worldmap_path_window_open, (s32)worldmap_path_window_update}, {(s32)worldmap_destination_window_open, (s32)worldmap_destination_window_update},
    {(s32)worldmap_open_map_frame_start, (s32)worldmap_open_map_frame_update}, {0, 0},
};

/* Each area's own actors, by area (worldmap_area_actor_lists). */
ActorSpawn worldmap_area0_actors[2] = {{(s32)worldmap_spinning_pair_start, (s32)worldmap_spinning_pair_update}, {0, 0}}; /* 80099F0C */
ActorSpawn worldmap_area1_actors[1] = {{0, 0}}; /* 80099F1C */
ActorSpawn worldmap_area3_actors[3] = { /* 80099F24 */
    {(s32)worldmap_ferry_start, (s32)worldmap_ferry_update}, {(s32)worldmap_airship_start, (s32)worldmap_airship_update}, {0, 0},
};
ActorSpawn worldmap_area4_actors[7] = { /* 80099F3C */
    {(s32)worldmap_ferry_start, (s32)worldmap_ferry_update}, {(s32)worldmap_airship_start, (s32)worldmap_airship_update},
    {(s32)worldmap_area_rolling_pair_start, (s32)worldmap_area_rolling_pair_update}, {(s32)worldmap_area_spinning_pair_start, (s32)worldmap_area_spinning_pair_update},
    {(s32)worldmap_object69_start, (s32)worldmap_object69_update}, {(s32)worldmap_ground_placement_start, (s32)worldmap_ground_placement_update}, {0, 0},
};
ActorSpawn worldmap_area5_actors[7] = { /* 80099F74 */
    {(s32)worldmap_ferry_start, (s32)worldmap_ferry_update}, {(s32)worldmap_airship_start, (s32)worldmap_airship_update},
    {(s32)worldmap_area_rolling_pair_start, (s32)worldmap_area_rolling_pair_update}, {(s32)worldmap_area_spinning_pair_start, (s32)worldmap_area_spinning_pair_update},
    {(s32)worldmap_area_idle_actor_start, (s32)worldmap_area_idle_actor_update}, {(s32)worldmap_ground_placement_start, (s32)worldmap_ground_placement_update}, {0, 0},
};
ActorSpawn worldmap_area6_actors[8] = { /* 80099FAC */
    {(s32)worldmap_ferry_start, (s32)worldmap_ferry_update}, {(s32)worldmap_airship_start, (s32)worldmap_airship_update},
    {(s32)worldmap_area_rolling_pair_start, (s32)worldmap_area_rolling_pair_update}, {(s32)worldmap_area_spinning_pair_start, (s32)worldmap_area_spinning_pair_update},
    {(s32)worldmap_area_idle_actor_start, (s32)worldmap_area_idle_actor_update}, {(s32)worldmap_ground_placement_start, (s32)worldmap_ground_placement_update},
    {(s32)worldmap_raised_placement_start, (s32)worldmap_raised_placement_update}, {0, 0},
};
ActorSpawn worldmap_area8_actors[9] = { /* 80099FEC */
    {(s32)worldmap_area_idle_actor_start, (s32)worldmap_area_idle_actor_update}, {(s32)worldmap_area_idle_actor_start, (s32)worldmap_area_idle_actor_update},
    {(s32)worldmap_area_rolling_pair_start, (s32)worldmap_area_rolling_pair_update}, {(s32)worldmap_area_spinning_pair_start, (s32)worldmap_area_spinning_pair_update},
    {(s32)worldmap_area_idle_actor_start, (s32)worldmap_area_idle_actor_update}, {(s32)worldmap_ground_placement_start, (s32)worldmap_ground_placement_update},
    {(s32)worldmap_raised_placement_start, (s32)worldmap_raised_placement_update}, {(s32)worldmap_object75_placement_start, (s32)worldmap_object75_placement_update}, {0, 0},
};
ActorSpawn *worldmap_area_actor_lists[9] = { /* 8009A034 */
    worldmap_area0_actors, worldmap_area1_actors, worldmap_area1_actors, worldmap_area3_actors, worldmap_area4_actors,
    worldmap_area5_actors, worldmap_area6_actors, worldmap_area6_actors, worldmap_area8_actors,
};

/* Handlers per world-map mode (worldmap.c's frame loop): 0-7 the open map,
 * the others the scripted scenes. */
WorldmapMode worldmap_mode_handlers[19] = { /* 8009A058 */
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_party_models, worldmap_open_map_start, worldmap_open_map_leave},
    {worldmap_read_area_files, worldmap_scene8_start, worldmap_scene8_leave},
    {worldmap_read_area_files, worldmap_scene9_start, worldmap_scene9_leave},
    {worldmap_read_area_files, worldmap_scene10_start, worldmap_scene10_leave},
    {worldmap_read_area_files, worldmap_scene8_start, worldmap_scene8_leave},
    {worldmap_read_area_files, worldmap_scene12_start, worldmap_scene12_leave},
    {worldmap_read_area_files, worldmap_scene13_start, worldmap_scene13_leave},
    {worldmap_read_area_files, worldmap_scene14_start, worldmap_scene14_leave},
    {worldmap_read_area_files, worldmap_scene15_start, worldmap_scene15_leave},
    {worldmap_read_area_files, worldmap_scene16_start, worldmap_scene16_leave},
    {worldmap_read_area_files, worldmap_scene17_start, worldmap_scene17_leave},
    {worldmap_read_area_files, worldmap_scene18_start, worldmap_scene18_leave},
};

/* Unreferenced: the extent positions wrap at (see worldmap_terrain_wrap_origin). */
s32 worldmap_unused_wrap_extent = 0x800000; /* 8009A13C */

/* Light colour and direction matrices of the scene objects (worldmap_objects_build)
 * and the landmark model (worldmap_draw_distant_landmark), and the identity matrix. */
MATRIX worldmap_light_color_matrix = {{{1536, 0, 0}, {1536, 0, 0}, {1536, 0, 0}}, {0, 0, 0}}; /* 8009A140 */
MATRIX worldmap_light_direction_matrix = {{{0, -4096, 0}, {0, 0, 0}, {0, 0, 0}}, {0, 0, 0}}; /* 8009A160 */
MATRIX worldmap_identity_matrix = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}}; /* 8009A180 */

/* Terrain texture animations: the frame sequences and the slots they upload
 * to, for the two animation sections of an area. The sequences are
 * user-supplied cue data (assets in worldmap.classification.txt): (image,
 * duration) frames ended by a negative duration, stepped by worldmap_texture_anim_advance
 * and worldmap_texture_anim2_advance (tools/analysis/overlay_scripts.py decodes them). */
extern TexAnimFrame worldmap_texture_anim_slot0_frames[], worldmap_texture_anim_slot1_frames[], worldmap_texture_anim2_slot0_frames[], worldmap_texture_anim2_slot1_frames[], worldmap_texture_anim2_slot2_frames[];
INCLUDE_ASSET(".data", worldmap_texture_anim_slot0_frames, 0x8009A1A0, 0x24);
INCLUDE_ASSET(".data", worldmap_texture_anim_slot1_frames, 0x8009A1C4, 0x24);
TexAnimSlot worldmap_texture_anim_slots[2] = { /* 8009A1E8 */
    {{0xF8, 0x1B0, 8, 1}, 0, worldmap_texture_anim_slot0_frames},
    {{0xF8, 0x1D0, 8, 1}, 1, worldmap_texture_anim_slot1_frames},
};
INCLUDE_ASSET(".data", worldmap_texture_anim2_slot0_frames, 0x8009A208, 0x18);
INCLUDE_ASSET(".data", worldmap_texture_anim2_slot1_frames, 0x8009A220, 0x18);
INCLUDE_ASSET(".data", worldmap_texture_anim2_slot2_frames, 0x8009A238, 0x18);
TexAnimSlot worldmap_texture_anim2_slots[3] = { /* 8009A250 */
    {{0x280, 0xC0, 0x20, 0x40}, 0, worldmap_texture_anim2_slot0_frames},
    {{0x2A0, 0xC0, 0x10, 0x20}, 1, worldmap_texture_anim2_slot1_frames},
    {{0x2B8, 0xC0, 0x10, 0x40}, 2, worldmap_texture_anim2_slot2_frames},
};

/* Sky band corners. */
SVECTOR worldmap_sky_band_corners[4][4] = { /* 8009A280 */
    {{-4096, -768, 4096}, {4096, -768, 4096}, {-4096, 1024, 4096}, {4096, 1024, 4096}},
    {{-4096, -1152, 4096}, {4096, -1152, 4096}, {-4096, -768, 4096}, {4096, -768, 4096}},
    {{-4096, -3200, 3072}, {4096, -3200, 3072}, {-4096, -1152, 4096}, {4096, -1152, 4096}},
    {{-4096, -4096, 1024}, {4096, -4096, 1024}, {-4096, -3200, 3072}, {4096, -3200, 3072}},
};

/* Horizon quad corners. */
SVECTOR worldmap_horizon_quad_corners[2][4] = { /* 8009A300 */
    {{-4096, -896, 4032}, {0, -896, 4032}, {-4096, -640, 4032}, {0, -640, 4032}},
    {{0, -896, 4032}, {4096, -896, 4032}, {0, -640, 4032}, {4096, -640, 4032}},
};

/* Map player marker triangles. */
SVECTOR worldmap_map_marker_triangles[4][3] = { /* 8009A340 */
    {{0, 0, 0}, {-8, -8, 0}, {-4, -11, 0}},
    {{0, 0, 0}, {-4, -11, 0}, {0, -12, 0}},
    {{0, 0, 0}, {0, -12, 0}, {4, -11, 0}},
    {{0, 0, 0}, {4, -11, 0}, {8, -8, 0}},
};

/* Terrain kind substitutes. */
s16 worldmap_layer4_encounter_kinds[16] = {3, 3, 3, 3, 3, 3, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10}; /* 8009A3A0 */

/* Script opcode handler: returns the halfwords to advance, 0 to yield. */
typedef s32 (*ScriptOp)(WorldmapActor *actor, s32 arg1, s32 arg2, s32 arg3);

/* Actor script commands, by command number. */
ScriptOp worldmap_actor_script_handlers[12] = { /* 8009A3C0 */
    (ScriptOp)worldmap_actor_script_exit_worldmap, (ScriptOp)worldmap_actor_script_wait, (ScriptOp)worldmap_actor_script_send_actor, worldmap_actor_script_place_player,
    (ScriptOp)worldmap_actor_script_set_script_vector, (ScriptOp)worldmap_actor_script_start_emitters, (ScriptOp)worldmap_actor_script_stop_emitters,
    (ScriptOp)worldmap_actor_script_stop_effects, (ScriptOp)worldmap_actor_script_fade_music, (ScriptOp)worldmap_actor_script_play_sound, worldmap_actor_script_slide_sound_volume,
    (ScriptOp)worldmap_actor_script_set_fade,
};

/* 80072238: Enter the world map: set up the display, load or restore the area, start the
 * subsystems, the music and the area's actors. */
void worldmap_open_map_start(void) {
    RECT rect;
    ActorSpawn *spawn;
    SoundSeq *seq;
    void *data;
    s32 file;
    s32 i;

    worldmap_init_display();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    worldmap_fade_saved_screen(0x40, 0, 4, 2);
    cd_sync_reads(0);
    worldmap_read_area_files();
    while (cd_get_pending_read_count() >= 3) {
    }
    worldmap_unpack_area_data();
    worldmap_actor_alloc_slots();
    worldmap_unread_mode_matrix = worldmap_identity_matrix;
    worldmap_menu_requested = 0;
    worldmap_button_combo_held = 0;
    worldmap_button_combo_held_last = 0;
    worldmap_button_combo_pressed = 0;
    worldmap_view_kind = 0;
    sprite_frame_skip = 1;
    worldmap_screen_fade_rate = 2;
    worldmap_screen_fade_step = 4;
    worldmap_screen_fade_active = 1;
    worldmap_cloud_draw_hook = worldmap_clouds_move;
    worldmap_terrain_compute_cull_normals();
    if (worldmap_resuming == 0) {
        mode_stop_music();
    } else {
        sound_stop_all_seqs();
        sound_release_seq((SoundSeq *)mode_music_seq);
        seq = (SoundSeq *)mode_music_cached_seq;
        mode_music_cached_seq = 0;
        mode_music_seq = (s32)seq;
    }
    if ((u16)game_data.worldmap.unk6A != 0) {
        worldmap_restore_player_position();
    } else if (worldmap_resuming == 0) {
        worldmap_place_player_at_arrival_point(worldmap_entry_index);
    } else {
        worldmap_restore_state();
        worldmap_apply_party_riding_changes();
    }
    cd_sync_reads(0);
    worldmap_upload_area_image();
    worldmap_terrain_upload_image();
    worldmap_objects_build();
    worldmap_read_area_sound_files();
    worldmap_sky_init();
    worldmap_map_overlay_init();
    worldmap_billboards_resolve_lists();
    sprite_alloc_queues(0x1400, 0);
    worldmap_footprints_alloc();
    worldmap_clouds_scatter();
    worldmap_texture_anim_create();
    worldmap_texture_anim2_create();
    worldmap_horizon_init();
    worldmap_effects_alloc_slots();
    cd_sync_reads(0);
    if (worldmap_resuming == 0) {
        mode_music_wave_bank = sound_load_wave_bank(worldmap_wave_bank, 0);
    }
    cd_select_directory(0x24, 0);
    if (worldmap_resuming == 0) {
        worldmap_terrain_reset_at(&worldmap_player_position);
        do {
            worldmap_stream_step();
            VSync(0);
        } while (worldmap_stream_count_queued() >= 2);
    } else {
        worldmap_terrain_reset_at_camera(&worldmap_camera);
        worldmap_stream_drain();
    }
    if (worldmap_resuming == 0) {
        /* The original debug halt tests this flag once and then spins. */
        while (sound_driver_flags & 0x10) {
        }
        heap_free(worldmap_wave_bank);
        sound_add_effect_bank(sound_effect_bank);
        if (worldmap_movement_mode == 7) {
            file = worldmap_flight_music_file;
            data = worldmap_flight_music;
        } else {
            file = worldmap_music_file;
            data = worldmap_music;
        }
        memcpy(mode_music_buffer, data, cd_get_aligned_file_size(file));
        seq = sound_create_seq(&mode_music_buffer_header);
        mode_music_seq = (s32)seq;
        sound_play_seq((SoundSeq *)mode_music_seq, 0x7F, 0);
    } else {
        heap_free(worldmap_wave_bank);
        sound_add_effect_bank(sound_effect_bank);
        if (worldmap_movement_mode == 7) {
            file = worldmap_flight_music_file;
            data = worldmap_flight_music;
        } else {
            file = worldmap_music_file;
            data = worldmap_music;
        }
        memcpy(mode_music_buffer, data, cd_get_aligned_file_size(file));
        sound_restart_seq((SoundSeq *)mode_music_seq, 0x7F, 0xF0);
    }
    switch (worldmap_resuming) {
    case 0:
        for (i = 0; worldmap_open_map_actors[i].kind != 0; i++) {
            worldmap_actor_spawn(worldmap_open_map_actors[i].kind, worldmap_open_map_actors[i].update);
        }
        spawn = worldmap_area_actor_lists[worldmap_area_index];
        if (spawn->kind != 0) {
            ActorSpawn *actor = spawn;
            do {
                worldmap_actor_spawn(actor->kind, actor->update);
                actor++;
            } while (actor->kind != 0);
        }
        break;
    case 1:
        worldmap_actor_set_kind((s32)worldmap_screen_fade_start, 0);
        worldmap_actor_set_kind((s32)worldmap_leader_resume, 1);
        worldmap_actor_set_kind((s32)worldmap_second_member_resume, 2);
        worldmap_actor_set_kind((s32)worldmap_third_member_resume, 3);
        worldmap_actor_set_kind((s32)worldmap_player_vehicle_resume, 4);
        worldmap_actor_set_kind((s32)worldmap_second_vehicle_resume, 5);
        worldmap_actor_set_kind((s32)worldmap_third_vehicle_resume, 6);
        worldmap_actor_set_kind((s32)worldmap_flying_vehicle_resume, 7);
        worldmap_actor_set_kind((s32)worldmap_flying_vehicle_rotors_resume, 8);
        worldmap_actor_set_kind((s32)worldmap_path_window_open, 0xC);
        worldmap_actor_set_kind((s32)worldmap_destination_window_open, 0xD);
        worldmap_actor_set_kind((s32)worldmap_open_map_frame_start, 0xE);
        switch (worldmap_area_index) {
        case 3:
            worldmap_actor_set_kind((s32)worldmap_ferry_link_objects, 0xF);
            worldmap_actor_set_kind((s32)worldmap_airship_link_objects, 0x10);
            break;
        case 4:
            worldmap_actor_set_kind((s32)worldmap_ferry_link_objects, 0xF);
            worldmap_actor_set_kind((s32)worldmap_airship_link_objects, 0x10);
            worldmap_actor_set_kind((s32)worldmap_area_rolling_pair_rebuild, 0x11);
            worldmap_actor_set_kind((s32)worldmap_object69_resume, 0x13);
            break;
        case 5:
        case 6:
        case 7:
            worldmap_actor_set_kind((s32)worldmap_ferry_link_objects, 0xF);
            worldmap_actor_set_kind((s32)worldmap_airship_link_objects, 0x10);
        case 8:
            worldmap_actor_set_kind((s32)worldmap_area_rolling_pair_rebuild, 0x11);
            break;
        }
        break;
    }
    mode_gear_riding_lock = 0;
    if (worldmap_area_index == 0) {
        worldmap_effects_start_emitters(0xE, NULL, NULL);
        mode_gear_riding_lock = 1;
    }
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_billboards_alloc_quads();
    if (worldmap_resuming == 0) {
        worldmap_encounter_reset_timers();
    }
    text_load_palette(0x130, 0x1E0);
}


/* 8007299C: Leave the world map: stop audio, release actor handles, shut down each
 * subsystem and free the area buffers. */
void worldmap_open_map_leave(void) {
    s32 i;

    if (worldmap_loop_result == 0) {
        sound_set_seq_fade((SoundSeq *)mode_music_seq, 0, 0xF0);
    }
    sound_stop_all_effects();
    sound_remove_effect_bank(sound_effect_bank);
    heap_free(sound_effect_bank);
    for (i = 0; i < 0x40; i++) {
        if (worldmap_actor_slots[i].handle != NULL) {
            sprite_destroy(worldmap_actor_slots[i].handle);
            worldmap_actor_slots[i].handle = NULL;
        }
    }
    if (worldmap_loop_result == 1) {
        worldmap_save_state();
        game_data.entry[2] |= 0x8000;
    }
    worldmap_path_window_close();
    worldmap_destination_window_close();
    worldmap_objects_free();
    worldmap_billboards_free_quads();
    sprite_free_queues();
    worldmap_clouds_free();
    worldmap_clouds_free_quads();
    worldmap_footprints_free();
    worldmap_texture_anim_free();
    worldmap_texture_anim2_free();
    worldmap_effects_free_slots();
    worldmap_effects_free_quads();
    worldmap_terrain_free_blocks();
    heap_free(worldmap_display_buffers[0].ot);
    heap_free(worldmap_display_buffers[1].ot);
    heap_free(worldmap_display_buffers[0].packets);
    heap_free(worldmap_display_buffers[1].packets);
    heap_free(worldmap_area_data);
    for (i = 0; i < 3; i++) {
        if (worldmap_character_models[i] != NULL) {
            heap_free(worldmap_character_models[i]);
        }
        if (worldmap_gear_models[i] != NULL) {
            heap_free(worldmap_gear_models[i]);
        }
    }
    worldmap_actor_free_slots();
    worldmap_stream_free();
}

/* 80072BB0: Set up the double-buffered 320x216 display and the background colour. */
void worldmap_init_display(void) {
    ResetGraph(1);
    worldmap_projection_distance = 0x100;
    SetGeomScreen(0x100);
    SetDefDrawEnv(&worldmap_display_buffers[0].draw, 0, 0, 0x140, 0xD8);
    SetDefDrawEnv(&worldmap_display_buffers[1].draw, 0, 0xD8, 0x140, 0xD8);
    SetDefDispEnv(&worldmap_display_buffers[0].disp, 0, 0xD8, 0x140, 0xD8);
    SetDefDispEnv(&worldmap_display_buffers[1].disp, 0, 0, 0x140, 0xD8);
    worldmap_display_buffers[1].draw.isbg = 1;
    worldmap_display_buffers[0].draw.isbg = 1;
    worldmap_display_buffers[1].draw.dtd = 1;
    worldmap_display_buffers[0].draw.dtd = 1;
    if (worldmap_loop_result == 2) {
        worldmap_display_buffers[0].draw.r0 = 0;
        worldmap_display_buffers[0].draw.g0 = 0;
        worldmap_display_buffers[0].draw.b0 = 0;
        worldmap_display_buffers[1].draw.r0 = 0;
        worldmap_display_buffers[1].draw.g0 = 0;
        worldmap_display_buffers[1].draw.b0 = 0;
    } else {
        worldmap_display_buffers[0].draw.r0 = 0;
        worldmap_display_buffers[0].draw.g0 = 0;
        worldmap_display_buffers[0].draw.b0 = 0x70;
        worldmap_display_buffers[1].draw.r0 = 0;
        worldmap_display_buffers[1].draw.g0 = 0;
        worldmap_display_buffers[1].draw.b0 = 0x70;
    }
    worldmap_display_buffers[1].disp.screen.y = 0xA;
    worldmap_display_buffers[0].disp.screen.y = 0xA;
    worldmap_display_buffers[1].disp.screen.w = 0x100;
    worldmap_display_buffers[0].disp.screen.w = 0x100;
    worldmap_display_buffers[1].disp.screen.x = 0;
    worldmap_display_buffers[0].disp.screen.x = 0;
    worldmap_display_buffers[1].disp.screen.h = 0xD8;
    worldmap_display_buffers[0].disp.screen.h = 0xD8;
    model_set_color(0x80, 0x80, 0x80);
    SetBackColor(0x80, 0x80, 0x80);
    SetFarColor(worldmap_background_color[0], worldmap_background_color[1], worldmap_background_color[2]);
    SetFogNearFar(worldmap_loop_result == 2 ? 0xB00 : 0x800, 0xE80, worldmap_projection_distance);
}

/* 80072DB4: Fade the saved screen (VRAM x 0x2C0) for `frames` frames: redraw it as three
 * textured quads under a translucent black quad whose level starts at `level`
 * and changes by `step`, blended with mode `abr`. */
void worldmap_fade_saved_screen(s32 frames, s32 level, s32 step, s32 abr) {
    POLY_FT4 *quads;
    POLY_G4 *shades;
    DR_TPAGE *mode;
    DisplayBuffer *buffer;
    POLY_G4 *shade;
    s32 side;
    s32 i;

    quads = heap_alloc(3 * sizeof(POLY_FT4), 1);
    shades = heap_alloc(2 * sizeof(POLY_G4), 1);
    mode = heap_alloc(sizeof(DR_TPAGE), 1);
    for (i = 0; i < 3; i++) {
        setPolyFT4(&quads[i]);
        setRGB0(&quads[i], 0x80, 0x80, 0x80);
        setShadeTex(&quads[i], 1);
    }
    setXY4(&quads[0], 0, 0, 0x80, 0, 0, 0xEF, 0x80, 0xEF);
    setXY4(&quads[1], 0x80, 0, 0x100, 0, 0x80, 0xEF, 0x100, 0xEF);
    setXY4(&quads[2], 0x100, 0, 0x140, 0, 0x100, 0xEF, 0x140, 0xEF);
    setUV4(&quads[0], 0, 0, 0x80, 0, 0, 0xEF, 0x80, 0xEF);
    setUV4(&quads[1], 0, 0, 0x80, 0, 0, 0xEF, 0x80, 0xEF);
    setUV4(&quads[2], 0, 0, 0x40, 0, 0, 0xEF, 0x40, 0xEF);
    quads[0].tpage = GetTPage(2, 0, 0x2C0, 0x100);
    quads[1].tpage = GetTPage(2, 0, 0x340, 0x100);
    quads[2].tpage = GetTPage(2, 0, 0x3C0, 0x100);
    SetDrawTPage(mode, 0, 1, GetTPage(0, abr, 0, 0));
    setPolyG4(&shades[0]);
    setXY4(&shades[0], 0, 0, 0x140, 0, 0, 0xF0, 0x140, 0xF0);
    setRGB0(&shades[0], 0, 0, 0);
    setRGB1(&shades[0], 0, 0, 0);
    setRGB2(&shades[0], 0, 0, 0);
    setRGB3(&shades[0], 0, 0, 0);
    SetSemiTrans(&shades[0], 1);
    setShadeTex(&shades[0], 1);
    shades[1] = shades[0];
    DrawSync(0);
    VSync(0);
    PutDispEnv(&worldmap_display_buffers[1].disp);
    PutDrawEnv(&worldmap_display_buffers[1].draw);
    buffer = &worldmap_display_buffers[0];
    side = 0;
    i = level;
    for (frames--; frames != -1; frames--) {
        buffer = (buffer == worldmap_display_buffers) ? buffer + 1 : worldmap_display_buffers;
        ClearOTagR(buffer->ot, 0x400);
        addPrim(buffer->ot + 1, &quads[0]);
        addPrim(buffer->ot + 1, &quads[1]);
        addPrim(buffer->ot + 1, &quads[2]);
        side ^= 1;
        shade = (POLY_G4 *)(side * sizeof(POLY_G4) + (u32)shades);
        setRGB0(shade, i, i, i);
        setRGB1(shade, i, i, i);
        setRGB2(shade, i, i, i);
        setRGB3(shade, i, i, i);
        addPrim(buffer->ot, shade);
        addPrim(buffer->ot, mode);
        DrawSync(0);
        VSync(0);
        PutDispEnv(&buffer->disp);
        PutDrawEnv(&buffer->draw);
        i += step;
        DrawOTag(buffer->ot + 0x3FF);
    }
    DrawSync(0);
    VSync(0);
    PutDispEnv(&worldmap_display_buffers[1].disp);
    heap_free(quads);
    heap_free(shades);
    heap_free(mode);
}

/* 80073300: Choose the movement mode from the saved state: a vehicle kind, or on foot
 * (1 when no party flag is set, else 2). */
void worldmap_choose_movement_mode(void) {
    if (game_data.worldmap.flags & 0x4000) {
        switch (game_data.worldmap.flags & 0x1FFF) {
        case 0:
            break;
        case 1:
            worldmap_movement_mode = 4;
            break;
        case 2:
            worldmap_movement_mode = 5;
            break;
        case 3:
            worldmap_movement_mode = 7;
            break;
        case 4:
            worldmap_movement_mode = 7;
            break;
        }
    } else if ((game_data.inGear[0] | game_data.inGear[1] | game_data.inGear[2]) == 0) {
        worldmap_movement_mode = 1;
    } else {
        worldmap_movement_mode = 2;
    }
}

/* 80073398: Restore the player position and heading for the current movement mode. */
void worldmap_restore_player_position(void) {
    game_data.worldmap.unk6A = 0;
    switch (worldmap_movement_mode) {
    case 1:
    case 2:
        worldmap_player_position.vx = game_data.worldmap.x << 12;
        worldmap_player_position.vz = game_data.worldmap.z << 12;
        worldmap_player_heading = game_data.worldmap.heading;
        break;
    case 4:
    case 5:
    case 7:
        worldmap_load_flying_vehicle_position(&worldmap_player_position);
        worldmap_player_heading = game_data.worldmap.vehicle_heading;
        break;
    }
}

/* 80073448: Place the player at the arrival point with the given id (or restore a
 * saved vehicle position); unknown ids place it at the origin. */
void worldmap_place_player_at_arrival_point(s32 id) {
    s32 unused; /* unreferenced; the original frame reserves it */
    WorldmapSpot *spot;

    if (game_data.worldmap.flags & 0x2000) {
        game_data.worldmap.flags &= ~0x2000;
        worldmap_load_flying_vehicle_position(&worldmap_player_position);
        worldmap_player_heading = game_data.worldmap.vehicle_heading;
        return;
    }
    for (spot = worldmap_arrival_points; spot->id != -1; spot++) {
        if (spot->id == id) {
            worldmap_player_position.vx = spot->x << 12;
            worldmap_player_position.vy = 0;
            worldmap_player_position.vz = spot->z << 12;
            return;
        }
    }
    worldmap_player_position.vx = 0;
    worldmap_player_position.vy = 0;
    worldmap_player_position.vz = 0;
}

/* 80073530: Unpack the area file and resolve its section offsets to pointers. */
void worldmap_unpack_area_data(void) {
    u8 *block;
    u8 *base;
    AreaHeader *area;
    s32 *table;
    s32 i;

    block = worldmap_area_data;
    worldmap_area_data = text_unpack_lzss_alloc(block, 0);
    heap_free(block);
    base = worldmap_area_data;
    area = (AreaHeader *)base;
    block = base + area->spots;
    worldmap_object_models = base + area->models;
    worldmap_object_meshes = base + area->meshes;
    worldmap_object_placement_list = base + area->placements;
    worldmap_billboard_lists = (BillboardList *)(base + area->billboards);
    worldmap_effect_emitters = (AreaObject *)(base + area->emitters);
    worldmap_name_table = base + area->names;
    worldmap_texture_anim_section = (s32 *)(base + area->animations);
    worldmap_texture_anim2_section = (s32 *)(base + area->animations2);
    for (i = 0; i < 16; i++) {
        worldmap_encounter_sets[i] = worldmap_area_data + area->encounters[i];
    }
    worldmap_arrival_points = (WorldmapSpot *)(block + ((SpotHeader *)block)->spots);
    worldmap_path_tables = table = (s32 *)(block + ((SpotHeader *)block)->table);
    table[0] = (s32)block + table[0];
    worldmap_path_tables[1] = (s32)block + table[1];
    worldmap_path_tables[2] = (s32)block + table[2];
    worldmap_path_tables[3] = (s32)block + table[3];
}

/* 8007369C: Allocate the two display buffers' ordering tables (4 KiB each). */
void worldmap_alloc_ots(void) {
    worldmap_display_buffers[0].ot = heap_alloc(0x1000, 0);
    worldmap_display_buffers[1].ot = heap_alloc(0x1000, 0);
}

/* 800736DC: Initialise the sky gradient: four bands of Gouraud quads in both buffers. */
void worldmap_sky_init(void) {
    worldmap_sky_bands[0][0].rgb0 = worldmap_sky_bands[0][0].rgb1 = worldmap_sky_bands[0][1].rgb0 = worldmap_sky_bands[0][1].rgb1 = 0xFF7A70;
    worldmap_sky_bands[0][0].rgb2 = worldmap_sky_bands[0][0].rgb3 = worldmap_sky_bands[0][1].rgb2 = worldmap_sky_bands[0][1].rgb3 = 0xFFF5E0;
    setPolyG4(&worldmap_sky_bands[0][0]);
    setPolyG4(&worldmap_sky_bands[0][1]);
    worldmap_sky_bands[1][0].rgb0 = worldmap_sky_bands[1][0].rgb1 = worldmap_sky_bands[1][1].rgb0 = worldmap_sky_bands[1][1].rgb1 = 0xC03745;
    worldmap_sky_bands[1][0].rgb2 = worldmap_sky_bands[1][0].rgb3 = worldmap_sky_bands[1][1].rgb2 = worldmap_sky_bands[1][1].rgb3 = 0xFF7A70;
    setPolyG4(&worldmap_sky_bands[1][0]);
    setPolyG4(&worldmap_sky_bands[1][1]);
    worldmap_sky_bands[2][0].rgb0 = worldmap_sky_bands[2][0].rgb1 = worldmap_sky_bands[2][0].rgb2 = worldmap_sky_bands[2][0].rgb3 =
        worldmap_sky_bands[2][1].rgb0 = worldmap_sky_bands[2][1].rgb1 = worldmap_sky_bands[2][1].rgb2 = worldmap_sky_bands[2][1].rgb3 = 0xC03745;
    setPolyG4(&worldmap_sky_bands[2][0]);
    setPolyG4(&worldmap_sky_bands[2][1]);
    worldmap_sky_bands[3][0].rgb0 = worldmap_sky_bands[3][0].rgb1 = worldmap_sky_bands[3][0].rgb2 = worldmap_sky_bands[3][0].rgb3 =
        worldmap_sky_bands[3][1].rgb0 = worldmap_sky_bands[3][1].rgb1 = worldmap_sky_bands[3][1].rgb2 = worldmap_sky_bands[3][1].rgb3 = 0xC03745;
    setPolyG4(&worldmap_sky_bands[3][0]);
    setPolyG4(&worldmap_sky_bands[3][1]);
}

/* Scratchpad work area of the sky renderer. */
typedef struct {
    SVECTOR angle;
    MATRIX view;
    MATRIX rotation;
    long p;
    long flag;
} SkyScratch;

#define SKY_SCRATCH ((SkyScratch *)0x1F800000)

/* 800737EC: Transform the four sky bands with the camera yaw and link them into the
 * ordering table. */
void worldmap_sky_draw(void) {
    SVECTOR *corners;
    PolyG4 *band;
    s32 otz;
    s32 i;
    SkyScratch *scratch;

    scratch = SKY_SCRATCH;
    scratch->angle.vz = 0;
    scratch->angle.vx = 0;
    scratch->angle.vy = worldmap_camera_angle.vy;
    RotMatrixYXZ(&scratch->angle, &scratch->rotation);
    corners = worldmap_sky_band_corners[0];
    scratch->rotation.t[2] = 0;
    scratch->rotation.t[1] = 0;
    scratch->rotation.t[0] = 0;
    CompMatrix(&worldmap_camera_matrix, &scratch->rotation, &scratch->view);
    SetRotMatrix(&scratch->view);
    SetTransMatrix(&scratch->view);
    for (i = 0; i < 4; i++, corners += 4) {
        band = &worldmap_sky_bands[i][worldmap_display_buffer_index];
        otz = RotTransPers4(&corners[0], &corners[1], &corners[2], &corners[3], &band->xy0, &band->xy1,
                            &band->xy2, &band->xy3, &scratch->p, &scratch->flag);
        if (scratch->flag >= 0) {
            addPrim(&worldmap_current_display_buffer->ot[otz >> model_ot_depth_shift], band);
        }
    }
}

/* 800739B8: Initialise the four textured horizon quads and the two texture windows. */
void worldmap_horizon_init(void) {
    RECT window;
    POLY_FT4 *quad;
    u16 tpage;
    u16 clut;
    s32 i;

    tpage = GetTPage(0, 1, 0x380, 0x100);
    clut = GetClut(0x110, 0x1FE);
    quad = worldmap_horizon_quads[0];
    for (i = 0; i < 4; i++, quad++) {
        ((u8 *)quad)[3] = 9;
        quad->code = 0x2C;
        quad->r0 = 0x30;
        quad->g0 = 0x30;
        quad->b0 = 0x30;
        quad->u0 = 0;
        quad->v0 = 0;
        quad->u1 = 0xFF;
        quad->v1 = 0;
        quad->u2 = 0;
        quad->v2 = 0x3F;
        quad->u3 = 0xFF;
        quad->v3 = 0x3F;
        quad->tpage = tpage;
        quad->clut = clut;
        SetSemiTrans(quad, 1);
    }
    window.x = 0;
    window.y = 0;
    window.w = 0x80;
    window.h = 0;
    SetTexWindow(&worldmap_horizon_texture_windows[0], &window);
    window.x = 0;
    window.y = 0;
    window.w = 0;
    window.h = 0;
    SetTexWindow(&worldmap_horizon_texture_windows[1], &window);
}

/* Scratchpad work area of the horizon renderer. */
typedef struct {
    SVECTOR angle;    /* 0x00 */
    u8 pad8[0x10];
    MATRIX view;      /* 0x18 */
    MATRIX rotation;  /* 0x38 */
    long p;           /* 0x58 */
    long flag;        /* 0x5C */
} HorizonScratch;

#define HORIZON_SCRATCH ((HorizonScratch *)0x1F800000)

/* 80073B04: Scroll the horizon texture with the camera yaw, transform the two horizon
 * quads of this buffer and link them, inside their texture windows, into the
 * ordering table. Each texture coordinate pair is stored as one (v << 8 | u)
 * halfword: u0/u2 at the scroll, u1/u3 128 texels right, v2/v3 at 0x3F. */
void worldmap_horizon_draw(void) {
    SVECTOR *corners;
    HorizonScratch *scratch;
    POLY_FT4 *quad;
    s32 u;
    s32 right;
    s32 i;
    s32 otz;
    u_long *ot;

    u = (worldmap_camera_angle.vy >> 2) & 0x7F;
    right = u | 0x80;
    *(u16 *)&worldmap_horizon_quads[0][worldmap_display_buffer_index].u0 = *(u16 *)&worldmap_horizon_quads[1][worldmap_display_buffer_index].u0 = u;
    *(u16 *)&worldmap_horizon_quads[0][worldmap_display_buffer_index].u1 = *(u16 *)&worldmap_horizon_quads[1][worldmap_display_buffer_index].u1 = right;
    *(u16 *)&worldmap_horizon_quads[0][worldmap_display_buffer_index].u2 = *(u16 *)&worldmap_horizon_quads[1][worldmap_display_buffer_index].u2 = u + 0x3F00;
    *(u16 *)&worldmap_horizon_quads[0][worldmap_display_buffer_index].u3 = *(u16 *)&worldmap_horizon_quads[1][worldmap_display_buffer_index].u3 = right + 0x3F00;
    HORIZON_SCRATCH->angle.vz = 0;
    scratch = HORIZON_SCRATCH;
    scratch->angle.vx = 0;
    scratch->angle.vy = worldmap_camera_angle.vy;
    RotMatrixYXZ(&scratch->angle, &scratch->rotation);
    scratch->rotation.t[2] = 0;
    scratch->rotation.t[1] = 0;
    scratch->rotation.t[0] = 0;
    CompMatrix(&worldmap_camera_matrix, &scratch->rotation, &scratch->view);
    SetRotMatrix(&scratch->view);
    SetTransMatrix(&scratch->view);
    corners = worldmap_horizon_quad_corners[0];
    for (i = 0; i < 2; i++, corners += 4) {
        quad = &worldmap_horizon_quads[i][worldmap_display_buffer_index];
        otz = RotTransPers4(&corners[0], &corners[1], &corners[2], &corners[3], (long *)&quad->x0,
                            (long *)&quad->x1, (long *)&quad->x2, (long *)&quad->x3, &scratch->p,
                            &scratch->flag);
    }
    if (scratch->flag >= 0) {
        ot = &worldmap_current_display_buffer->ot[otz >> model_ot_depth_shift];
        addPrim(ot, &worldmap_horizon_texture_windows[1]);
        addPrim(ot, &worldmap_horizon_quads[0][worldmap_display_buffer_index]);
        addPrim(ot, &worldmap_horizon_quads[1][worldmap_display_buffer_index]);
        addPrim(ot, &worldmap_horizon_texture_windows[0]);
    }
}

/* 80073E30: Initialise the overlay picture quad (both buffers), its texture page, eight
 * red Gouraud triangles and 64 small tiles. */
void worldmap_map_overlay_init(void) {
    POLY_G3 *triangle;
    TILE *tile;
    s32 i;

    setPolyFT4(&worldmap_map_overlay_quads[0]);
    setXY4(&worldmap_map_overlay_quads[0], 0xD0, 0x78, 0x137, 0x78, 0xD0, 0xD7, 0x137, 0xD7);
    setUV4(&worldmap_map_overlay_quads[0], 0, 0x80, 0x7F, 0x80, 0, 0xFF, 0x7F, 0xFF);
    setRGB0(&worldmap_map_overlay_quads[0], 0x80, 0x80, 0x80);
    worldmap_map_overlay_quads[0].tpage = GetTPage(0, 0, 0x380, 0x100);
    worldmap_map_overlay_quads[0].clut = GetClut(0x100, 0x1FE);
    SetSemiTrans(&worldmap_map_overlay_quads[0], 1);
    worldmap_map_overlay_quads[1] = worldmap_map_overlay_quads[0];
    SetDrawTPage(&worldmap_map_overlay_tpage, 1, 0, GetTPage(0, 1, 0x380, 0x100));
    triangle = worldmap_map_marker_polys;
    for (i = 0; i < 8; i++, triangle++) {
        setPolyG3(triangle);
        setRGB0(triangle, 0xFF, 0x40, 0x40);
        setRGB1(triangle, 0, 0, 0);
        setRGB2(triangle, 0, 0, 0);
        SetSemiTrans(triangle, 1);
    }
    tile = worldmap_map_dot_tiles;
    for (i = 0; i < 0x40; i++, tile++) {
        setTile(tile);
        setRGB0(tile, 0x80, 0x80, 0x10);
        setWH(tile, 2, 2);
    }
}

/* Scratchpad work area of the map overlay. */
typedef struct {
    u8 pad0[0xB8];
    SVECTOR angle;    /* 0xB8 */
    u8 padC0[0x30];
    MATRIX matrix;    /* 0xF0 */
} MapScratch;

/* 800740B8: Draw the map overlay: the player marker (four triangles rotated by the camera
 * yaw at the player's map position) and one dot per set bit of the resident
 * map flags; bits 24-26 are the vehicles. */
void worldmap_map_overlay_draw(void) {
    MapScratch *scratch;
    MATRIX *matrix;
    VECTOR *target;
    SVECTOR *corners;
    POLY_G3 *marker;
    TILE *dot;
    u32 bits;
    u16 *position_x;
    u16 *position_z;
    s32 i;

    scratch = (MapScratch *)0x1F800000;
    corners = worldmap_map_marker_triangles[0];
    i = 0;
    matrix = &scratch->matrix;
    target = &worldmap_camera_follow_target.target;
    marker = &worldmap_map_marker_polys[worldmap_display_buffer_index * 4];
    do {
        scratch->angle.vy = 0;
        scratch->angle.vx = 0;
        scratch->angle.vz = worldmap_camera_angle.vy;
        gpu_build_rotation_matrix(&scratch->angle, matrix);
        scratch->matrix.t[0] = (target->vx >> 12) / 315 + 0x30;
        scratch->matrix.t[2] = worldmap_projection_distance;
        scratch->matrix.t[1] = 0x78 - worldmap_view_center_y + (target->vz >> 12) / 341;
        gte_SetRotMatrix(matrix);
        gte_SetTransMatrix(matrix);
        gte_ldv3(&corners[0], &corners[1], &corners[2]);
        gte_rtpt();
        gte_stsxy3(&marker->x0, &marker->x1, &marker->x2);
        i++;
        corners += 3;
        addPrim(worldmap_current_display_buffer->ot, marker);
        marker++;
    } while (i < 4);
    dot = &worldmap_map_dot_tiles[worldmap_display_buffer_index * 32];
    addPrim(worldmap_current_display_buffer->ot, &worldmap_map_overlay_tpage);
    bits = MAP_FLAGS;
    /* Interleaved X/Z halfwords: index each coordinate with a stride of two. */
    position_x = worldmap_map_dot_positions;
    position_z = position_x + 1;
    for (i = 0; i < 32; i++, dot++) {
        if (bits & 1) {
            switch (i) {
            case 24:
                dot->x0 = game_data.worldmap.unk60 / 315 + 0xCF;
                dot->y0 = game_data.worldmap.unk64 / 341 + 0x77;
                break;
            case 25:
                dot->x0 = (u16)game_data.flight.x / 315 + 0xCF;
                dot->y0 = (u16)game_data.flight.z / 341 + 0x77;
                break;
            case 26:
                dot->x0 = game_data.unk1844[0] / 315 + 0xCF;
                dot->y0 = game_data.unk1844[1] / 341 + 0x77;
                break;
            default:
                dot->x0 = position_x[i * 2] + 0xD0;
                dot->y0 = position_z[i * 2] + 0x78;
                break;
            }
            addPrim(worldmap_current_display_buffer->ot, dot);
        }
        bits >>= 1;
    }
    addPrim(worldmap_current_display_buffer->ot, &worldmap_map_overlay_quads[worldmap_display_buffer_index]);
}

/* Sixteen footprint quads, copied between display buffers as a whole. */
typedef struct {
    POLY_FT4 quad[16];
} QuadSet;

/* 80074594: Allocate the recent-position ring and the two buffers of 16 footprint quads,
 * and initialise them. */
void worldmap_footprints_alloc(void) {
    WorldmapSpot *spot;
    POLY_FT4 *quad;
    s32 i;

    worldmap_footprints = heap_alloc(0x80, 0);
    worldmap_footprint_quads0 = heap_alloc(0x280, 0);
    worldmap_footprint_quads1 = heap_alloc(0x280, 0);
    spot = worldmap_footprints;
    for (i = 15; i != -1; i--) {
        spot->z = 0;
        spot->id = 0;
        spot->x = 0;
        spot++;
    }
    quad = worldmap_footprint_quads0;
    for (i = 15; i != -1; i--) {
        setPolyFT4(quad);
        setRGB0(quad, 0x40, 0x40, 0x48);
        quad->u0 = 0x80;
        quad->v0 = 0xF0;
        quad->u1 = 0x8F;
        quad->v1 = 0xF0;
        quad->u2 = 0x80;
        quad->v2 = 0xFF;
        quad->u3 = 0x8F;
        quad->v3 = 0xFF;
        quad->clut = GetClut(0x120, 0x1FE);
        quad->tpage = GetTPage(0, 0, 0x380, 0x100);
        SetSemiTrans(quad, 1);
        quad++;
    }
    *(QuadSet *)worldmap_footprint_quads1 = *(QuadSet *)worldmap_footprint_quads0;
    worldmap_footprint_count = 0;
}

/* 8007474C: Free the footprint ring and its two quad buffers. */
void worldmap_footprints_free(void) {
    heap_free(worldmap_footprint_quads1);
    heap_free(worldmap_footprint_quads0);
    heap_free(worldmap_footprints);
}

/* 80074794: Record a position (in world units) with an id in the 16-entry ring. */
void worldmap_footprints_add(s16 id, VECTOR *position) {
    worldmap_footprints[worldmap_footprint_count].x = position->vx >> 12;
    worldmap_footprints[worldmap_footprint_count].z = position->vz >> 12;
    worldmap_footprints[worldmap_footprint_count].id = id;
    worldmap_footprint_count = (worldmap_footprint_count + 1) & 0xF;
}

/* Scratchpad work area of the footprint pass. */
typedef struct {
    u8 pad0[0x30];
    VECTOR normal;     /* 0x30: ground normal */
    VECTOR up;         /* 0x40 */
    VECTOR side;       /* 0x50 */
    VECTOR forward;    /* 0x60 */
    VECTOR position;   /* 0x70 */
    VECTOR scale;      /* 0x80: also a cross product and the corner depths */
    u8 pad90[0x10];
    SVECTOR corner[3]; /* 0xA0 */
    u8 padB8[0x20];
    SVECTOR corner3;   /* 0xD8 */
    u8 padE0[0x10];
    MATRIX local;      /* 0xF0 */
    MATRIX screen;     /* 0x110 */
    MATRIX view;       /* 0x130 */
    MATRIX heading;    /* 0x150 */
} FootprintScratch;

#define FOOTPRINT_SCRATCH ((FootprintScratch *)0x1F800000)

/* 800747DC: Draw the recorded footprints: lay a quad on the ground at each ring position
 * (sized by the spot id, turned with the vehicle for id 2) and add it to the
 * ordering table; then empty the ring. */
void worldmap_footprints_draw(void) {
    FootprintScratch *scratch;
    WorldmapSpot *spot;
    POLY_FT4 *quad;
    s32 i;
    s32 z;
    s32 flag;
    s32 camera_x;
    s32 camera_z;

    if (worldmap_footprint_count != 0) {
        scratch = FOOTPRINT_SCRATCH;
        scratch->corner[0].vx = -0x10;
        scratch->corner[0].vz = 0x10;
        scratch->corner[1].vx = 0x10;
        scratch->corner[1].vz = 0x10;
        scratch->corner[2].vx = -0x10;
        scratch->corner[2].vz = -0x10;
        scratch->corner3.vx = 0x10;
        scratch->corner3.vz = -0x10;
        scratch->corner[0].vy = scratch->corner[1].vy = scratch->corner[2].vy = scratch->corner3.vy = 0;
        scratch->view = worldmap_camera_matrix;
        spot = worldmap_footprints;
        i = worldmap_footprint_count;
        scratch->up.vz = 0x1000;
        scratch->up.vx = 0;
        scratch->up.vy = 0;
        quad = (&worldmap_footprint_quads0)[worldmap_display_buffer_index];
        camera_x = worldmap_camera.target.vx;
        camera_z = worldmap_camera.target.vz;
        for (i--; i != -1; i--) {
            scratch->position.vx = spot->x << 12;
            scratch->position.vz = spot->z << 12;
            scratch->position.vy = worldmap_terrain_get_height(scratch->position.vx, scratch->position.vz);
            worldmap_terrain_get_normal(&scratch->normal, scratch->position.vx, scratch->position.vz);
            OuterProduct12(&scratch->up, &scratch->normal, &scratch->side);
            VectorNormal(&scratch->side, &scratch->forward);
            OuterProduct12(&scratch->normal, &scratch->forward, &scratch->scale);
            VectorNormal(&scratch->scale, &scratch->side);
            scratch->local.m[0][0] = scratch->forward.vx;
            scratch->local.m[0][1] = scratch->forward.vy;
            scratch->local.m[0][2] = scratch->forward.vz;
            scratch->local.m[1][0] = scratch->normal.vx;
            scratch->local.m[1][1] = scratch->normal.vy;
            scratch->local.m[1][2] = scratch->normal.vz;
            scratch->local.m[2][0] = scratch->side.vx;
            scratch->local.m[2][1] = scratch->side.vy;
            scratch->local.m[2][2] = scratch->side.vz;
            switch (spot->id) {
            case 0:
                break;
            case 1:
                scratch->scale.vx = scratch->scale.vy = scratch->scale.vz = 0x1800;
                ScaleMatrix(&scratch->local, &scratch->scale);
                break;
            case 2:
                scratch->heading = worldmap_identity_matrix;
                RotMatrixY(game_data.worldmap.vehicle_heading, &scratch->heading);
                libgte_multiply_matrix_in_place(&scratch->local, &scratch->heading);
                scratch->scale.vx = 0x1800;
                scratch->scale.vy = 0x1000;
                scratch->scale.vz = 0x4800;
                ScaleMatrix(&scratch->local, &scratch->scale);
                break;
            }
            scratch->local.t[0] = (scratch->position.vx - camera_x) >> 12;
            scratch->local.t[2] = (camera_z - scratch->position.vz) >> 12;
            scratch->local.t[1] = scratch->position.vy >> 12;
            gte_CompMatrix(&scratch->view, &scratch->local, &scratch->screen);
            gte_SetRotMatrix(&scratch->screen);
            gte_SetTransMatrix(&scratch->screen);
            gte_ldv3(&scratch->corner[0], &scratch->corner[1], &scratch->corner[2]);
            gte_rtpt();
            gte_stflg(&flag);
            if (flag >= 0) {
                gte_stsxy3(&quad->x0, &quad->x1, &quad->x2);
                gte_stsz3(&scratch->scale.vx, &scratch->scale.vy, &scratch->scale.vz);
                z = scratch->scale.vx;
                if (scratch->scale.vy < z) {
                    z = scratch->scale.vy;
                }
                if (scratch->scale.vz < z) {
                    z = scratch->scale.vz;
                }
                gte_ldv0(&scratch->corner3);
                gte_rtps();
                gte_stsxy(&quad->x3);
                gte_stsz(&scratch->scale.vx);
                if (scratch->scale.vx < z) {
                    z = scratch->scale.vx;
                }
                if (z < 0x1000) {
                    addPrim(worldmap_current_display_buffer->ot + (z >> 4), quad);
                    quad++;
                }
            }
            spot++;
        }
        worldmap_footprint_count = 0;
    }
}

/* 80074E58: Create the terrain texture animations from their area section. */
void worldmap_texture_anim_create(void) {
    TexAnim *anim;
    s32 i;
    s32 count;

    count = *worldmap_texture_anim_section;
    worldmap_texture_anim_count = count;
    worldmap_texture_anims = anim = heap_alloc(count * sizeof(TexAnim), 0);
    for (i = 0; i < worldmap_texture_anim_count; i++, anim++) {
        anim->images = (u8 *)worldmap_texture_anim_section + worldmap_texture_anim_section[i + 1];
        anim->slot = &worldmap_texture_anim_slots[i];
        anim->frame = 0;
        anim->timer = 1;
    }
}

/* 80074F04: Free the terrain texture animations. */
void worldmap_texture_anim_free(void) {
    heap_free(worldmap_texture_anims);
}

/* 80074F2C: Advance the terrain texture animations, uploading each new image. */
void worldmap_texture_anim_advance(void) {
    TexAnim *anim;
    s32 i;

    anim = worldmap_texture_anims;
    for (i = 0; i < worldmap_texture_anim_count; i++, anim++) {
        if (--anim->timer == 0) {
            anim->frame++;
            anim->timer = anim->slot->frames[anim->frame].duration;
            if (anim->timer < 0) {
                anim->frame = 0;
                anim->timer = anim->slot->frames[0].duration;
            }
            LoadImage(&anim->slot->rect,
                      (u_long *)(anim->images + anim->slot->frames[anim->frame].image * 16));
        }
    }
}

/* 80075030: Create the second set of texture animations from their area section. */
void worldmap_texture_anim2_create(void) {
    TexAnim *anim;
    s32 i;
    s32 count;

    count = *worldmap_texture_anim2_section;
    worldmap_texture_anim2_count = count;
    worldmap_texture_anims2 = anim = heap_alloc(count * sizeof(TexAnim), 0);
    for (i = 0; i < worldmap_texture_anim2_count; i++, anim++) {
        anim->images = (u8 *)worldmap_texture_anim2_section + worldmap_texture_anim2_section[i + 1];
        anim->slot = &worldmap_texture_anim2_slots[i];
        anim->frame = 0;
        anim->timer = 1;
    }
}

/* 800750DC: Free the second set of texture animations. */
void worldmap_texture_anim2_free(void) {
    heap_free(worldmap_texture_anims2);
}

/* 80075104: Advance the second texture animations; images are rect-sized. */
void worldmap_texture_anim2_advance(void) {
    TexAnim *anim;
    RECT *rect;
    s32 size;
    s32 i;

    anim = worldmap_texture_anims2;
    for (i = 0; i < worldmap_texture_anim2_count; i++, anim++) {
        if (--anim->timer == 0) {
            anim->frame++;
            anim->timer = anim->slot->frames[anim->frame].duration;
            if (anim->timer < 0) {
                anim->frame = 0;
                anim->timer = anim->slot->frames[0].duration;
            }
            rect = &anim->slot->rect;
            size = rect->h * rect->w * 2;
            LoadImage(rect,
                      (u_long *)(anim->images + anim->slot->frames[anim->frame].image * size));
        }
    }
}

/* 80075228: Reset the movement state; vehicles move twice as fast as on foot. */
void worldmap_encounter_reset_timers(void) {
    s32 i;

    for (i = 15; i >= 0; i--) {
        worldmap_encounter_timers[i] = 0;
    }
    worldmap_encounter_reroll_timer = 1;
    if (game_data.worldmap.flags & 0x4000) {
        worldmap_encounter_period = 0x300;
    } else {
        worldmap_encounter_period = 0x180;
    }
    worldmap_encounter_timer_count = 1;
    worldmap_encounter_expired_count = 0;
}

/* 8007528C: Every worldmap_encounter_period frames give each of worldmap_encounter_timer_count timers a distinct random
 * delay (1..worldmap_encounter_period); count down the timers and count those expiring. */
void worldmap_encounter_update_timers(void) {
    s32 i;
    s32 j;
    s32 value;

    if (--worldmap_encounter_reroll_timer == 0) {
        for (i = 0; i < worldmap_encounter_timer_count; i++) {
            if (i != 0) {
                do {
                    value = rand() % worldmap_encounter_period + 1;
                    for (j = 0; j < i; j++) {
                        if (worldmap_encounter_timers[j] == value) {
                            break;
                        }
                    }
                } while (j < i);
            } else {
                value = rand() % worldmap_encounter_period + 1;
            }
            worldmap_encounter_timers[i] = value;
        }
        worldmap_encounter_reroll_timer = worldmap_encounter_period;
    }
    worldmap_encounter_expired_count = 0;
    for (i = 0; i < worldmap_encounter_timer_count; i++) {
        if (--worldmap_encounter_timers[i] == 0) {
            worldmap_encounter_expired_count++;
        }
    }
}

/* 80075460: Save the world-map state to the resident save area. */
void worldmap_save_state(void) {
    WorldmapSave *save;

    save = &mode_snapshot_block;
    save->actors = *(ActorSet *)worldmap_actor_slots;
    save->position.vx = worldmap_camera_follow_target.target.vx;
    save->position.vy = worldmap_camera_follow_target.target.vy;
    save->position.vz = worldmap_camera_follow_target.target.vz;
    save->unk2010 = (s16)worldmap_camera_follow_heading;
    save->timer_period = worldmap_encounter_period;
    save->timer_count = worldmap_encounter_timer_count;
    save->timer_countdown = worldmap_encounter_reroll_timer;
    save->timers = *(TimerSet *)worldmap_encounter_timers;
    save->queue = *(WorldmapQueue *)worldmap_trail_points;
    save->queue_count = worldmap_trail_index;
    save->camera_angle[0] = ((s32 *)&worldmap_camera_angle)[0];
    save->camera_angle[1] = ((s32 *)&worldmap_camera_angle)[1];
    save->camera_distance = worldmap_camera_distance;
    save->unk22D0 = worldmap_view_center_y;
    save->unk22E4[0] = ((s32 *)&worldmap_camera_block_cell)[0];
    save->unk22E4[1] = ((s32 *)&worldmap_camera_block_cell)[1];
    save->unk22D4.vx = worldmap_terrain_origin.vx;
    save->unk22D4.vy = worldmap_terrain_origin.vy;
    save->unk22D4.vz = worldmap_terrain_origin.vz;
    save->camera_target.vx = worldmap_camera.target.vx;
    save->camera_target.vy = worldmap_camera.target.vy;
    save->camera_target.vz = worldmap_camera.target.vz;
}

/* 8007565C: Restore the world-map state from the resident save area. */
void worldmap_restore_state(void) {
    WorldmapSave *save;

    save = &mode_snapshot_block;
    *(ActorSet *)worldmap_actor_slots = save->actors;
    worldmap_player_position = save->position;
    worldmap_camera_follow_target.target = save->position;
    *(TimerSet *)worldmap_encounter_timers = save->timers;
    worldmap_camera_follow_heading = save->unk2010;
    worldmap_encounter_period = save->timer_period;
    worldmap_encounter_timer_count = save->timer_count;
    worldmap_encounter_reroll_timer = save->timer_countdown;
    *(WorldmapQueue *)worldmap_trail_points = save->queue;
    worldmap_trail_index = save->queue_count;
    worldmap_camera_angle = *(SVECTOR *)save->camera_angle;
    worldmap_camera_distance = save->camera_distance;
    worldmap_view_center_y = save->unk22D0;
    worldmap_terrain_origin = save->unk22D4;
    worldmap_camera_block_cell = *(SVECTOR *)save->unk22E4;
    worldmap_camera.target = save->camera_target;
}

/* 800758C0: Suspend the world map for another scene: record the return state, release
 * the area and save the VRAM areas the other scene overwrites. */
void worldmap_suspend_open_map(void) {
    RECT rect;
    void *block;
    s32 i;

    game_data.worldmap.unk6A = 1;
    game_data.entry[0] = (worldmap_camera_angle.vy + 0x2000) & 0x3FFF;
    for (i = 0; i < 3; i++) {
        (&game_data.worldmap.unk70)[i] = game_data.inGear[i];
    }
    worldmap_saved_gear_riding_lock = mode_gear_riding_lock;
    if (worldmap_terrain_get_layer(&worldmap_camera_follow_target.target) == 4) {
        mode_gear_riding_lock = 1;
    }
    worldmap_stream_drain();
    worldmap_billboards_free_quads();
    worldmap_clouds_free_quads();
    worldmap_effects_free_quads();
    heap_free(worldmap_display_buffers[0].packets);
    heap_free(worldmap_display_buffers[1].packets);
    block = heap_alloc(4, 1);
    heap_free(block);
    block = (void *)((u32)block & 0xFFFFFF);
    worldmap_suspend_memory = heap_alloc((u32)block - 0x1C4FFC, 1);
    worldmap_read_shared_files();
    worldmap_saved_vram_page = heap_alloc(0x10000, 0);
    worldmap_saved_vram_cluts = heap_alloc(0xC800, 0);
    rect.x = 0x180;
    rect.y = 0x100;
    rect.w = 0x80;
    rect.h = 0x100;
    StoreImage(&rect, worldmap_saved_vram_page);
    rect.x = 0;
    rect.y = 0x1B0;
    rect.w = 0x140;
    rect.h = 0x50;
    StoreImage(&rect, worldmap_saved_vram_cluts);
    if (worldmap_display_buffer_index == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    worldmap_fade_saved_screen(0x10, 0, 8, 2);
    while (cd_get_pending_read_count() >= 2) {
    }
    text_unpack_lzss(worldmap_packed_menu_overlay, worldmap_suspend_memory);
    heap_free(worldmap_packed_menu_overlay);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0xE0);
    DrawSync(0);
    cd_sync_reads(0);
}

/* 80075B58: Resume the world map after another scene: reload the area, restore the saved
 * VRAM areas and bring the systems back up. */
void worldmap_resume_open_map(void) {
    RECT rect;

    heap_select_owner_tag(3, 0);
    cd_select_directory(0x24, 0);
    heap_free(menu_state_resource_file);
    heap_free(worldmap_suspend_memory);
    worldmap_init_display();
    worldmap_area_image = heap_alloc(cd_get_aligned_file_size(worldmap_area_image_file), 1);
    cd_read_file(worldmap_area_image_file, worldmap_area_image, 0, 0);
    worldmap_fade_saved_screen(0x10, 0x80, -8, 2);
    rect.x = 0x2C0;
    rect.y = 0x100;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0);
    MoveImage(&rect, 0, 0xD8);
    ClearOTagR(worldmap_current_display_buffer->ot, 0x400);
    cd_sync_reads(0);
    worldmap_upload_area_image();
    rect.x = 0x180;
    rect.y = 0x100;
    rect.w = 0x80;
    rect.h = 0x100;
    LoadImage(&rect, worldmap_saved_vram_page);
    rect.x = 0;
    rect.y = 0x1B0;
    rect.w = 0x140;
    rect.h = 0x50;
    LoadImage(&rect, worldmap_saved_vram_cluts);
    DrawSync(0);
    heap_free(worldmap_saved_vram_page);
    heap_free(worldmap_saved_vram_cluts);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_billboards_alloc_quads();
    text_load_palette(0x130, 0x1E0);
    VSync(0);
    pad_clear_queue();
    worldmap_menu_requested = 0;
    game_data.worldmap.unk6A = 0;
    mode_gear_riding_lock = worldmap_saved_gear_riding_lock;
    worldmap_apply_party_riding_changes();
}

/* 80075D4C: Apply the party slots' in-gear changes since the last update: a member who
 * left the gear (actor slots 4-6) stands where it is (slots 1-3), and one who
 * boarded marks its gear's spot parked and brings the gear to the member.
 * Then pick the movement mode from the members present. */
void worldmap_apply_party_riding_changes(void) {
    WorldmapActor *actors;
    u8 *applied;  /* state last applied per slot: the low bytes of the saved
                   * in-gear flags (worldmap.unk70-unk74) */
    s32 i;
    s32 count;
    u8 state;

    i = 0;
    actors = worldmap_actor_slots;
    applied = (u8 *)&game_data.worldmap.unk70;
    do {
        state = game_data.inGear[i];
        if (state != applied[i * 2]) {
            if (state == 0) {
                actors[i + 1].position.vx = actors[i + 4].position.vx;
                actors[i + 1].position.vy = actors[i + 4].position.vy;
                actors[i + 1].position.vz = actors[i + 4].position.vz;
                actors[i + 1].unk58 = actors[i + 4].unk58;
            } else {
                VEHICLE_SPOTS[i].flags = 0x400;
                actors[i + 4].unk24 = 0;
                actors[i + 4].position.vx = actors[i + 1].position.vx;
                actors[i + 4].position.vy = actors[i + 1].position.vy;
                actors[i + 4].position.vz = actors[i + 1].position.vz;
                actors[i + 4].unk58 = actors[i + 1].unk58;
            }
        }
        i++;
    } while (i < 3);
    count = 0;
    for (i = 0; i < 3; i++) {
        if (game_data.party[i] != 0xFF && game_data.inGear[i] == 1) {
            count++;
        }
    }
    if (!(game_data.worldmap.flags & 0x4000)) {
        worldmap_movement_mode = count != 0 ? 2 : 1;
    }
}

/* 80075E7C: Roll an encounter for the terrain at a position and the scene id (event
 * variable 0, game data 8006ef64): pick a formation by the weights of the scene id's
 * bracket and copy the terrain's encounter set. Returns 0 when the bracket
 * has no formations. */
s32 worldmap_encounter_roll(VECTOR *position, s32 scene) {
    u8 weights[16];
    s32 kind;
    s32 bracket;
    s32 total;
    s32 roll;
    s32 formation;
    s32 result;
    s32 i;
    u8 *row;

    kind = (s16)worldmap_terrain_get_cell_flags(position);
    if (worldmap_terrain_get_layer(position) == 4) {
        kind = worldmap_layer4_encounter_kinds[kind];
    }
    bracket = 1;
    while (scene >= worldmap_encounter_level_brackets[bracket]) {
        bracket++;
    }
    bracket--;
    total = 0;
    row = (u8 *)worldmap_encounter_sets[kind] + 0x200; /* weights: 16 per bracket */
    row += bracket * 16;
    for (i = 0; i < 16; i++) {
        weights[i] = row[i];
        total += row[i];
    }
    result = 0;
    if (total > 0) {
        roll = rand() % total + 1;
        formation = 0;
        do {
            roll--;
        next:
            if (weights[formation] == 0) {
                formation++;
                goto next;
            }
            weights[formation]--;
        } while (roll > 0);
        formation_encounter_set = *(EncounterSet *)worldmap_encounter_sets[kind];
        formation_selected_index = formation;
        result = 1;
    }
    return result;
}

/* Scratchpad work area of the distant landmark. */
typedef struct {
    VECTOR position;  /* 0x00 */
    u8 pad10[0x10];
    s32 flag;         /* 0x20 */
    u8 pad24[4];
    s32 depth;        /* 0x28 */
    u8 pad2C[0x74];
    SVECTOR origin;   /* 0xA0 */
    u8 padA8[0x48];
    MATRIX local;     /* 0xF0 */
    MATRIX view;      /* 0x110 */
} LandmarkScratch;

#define LANDMARK_SCRATCH ((LandmarkScratch *)0x1F800000)

/* The actor module's draw, which each target declares itself
 * (ovl2143/actors.h says why). */
void gear_model_step_and_draw(MATRIX *m, MATRIX *light, u_long *ot, s32 buffer, s32 elapsed);

/* 80076098: Place the distant landmark model (actor 0 of the actor module, ovl2143)
 * relative to the camera and draw it when it is in front and nearer than
 * depth 0xD00. No code calls this function, and none in the world map
 * creates the actor. */
void worldmap_draw_distant_landmark(void) {
    s32 *flag;
    s32 *depth;

    LANDMARK_SCRATCH->position.vy = 0xA0;
    LANDMARK_SCRATCH->position.vx = 0x4E0E - (worldmap_camera.target.vx >> 12);
    LANDMARK_SCRATCH->position.vz = 0x1B68 - (worldmap_camera.target.vz >> 12);
    worldmap_wrap_world_offset(&LANDMARK_SCRATCH->position);
    gear_model_actors[0]->parts->translation[0] = LANDMARK_SCRATCH->position.vx;
    gear_model_actors[0]->parts->translation[1] = 0;
    gear_model_actors[0]->parts->translation[2] = -LANDMARK_SCRATCH->position.vz;
    gear_model_actors[0]->parts->rotation.vx = gear_model_actors[0]->parts->rotation.vy = gear_model_actors[0]->parts->rotation.vz = 0;
    /* The carrier byte and its rotation flag are stored as one halfword (sh):
     * the bytes 0 and 0xFF, as two members two stores. */
    *(s16 *)&gear_model_actors[0]->parent = -0x100;
    gear_model_actors[0]->scale = 0x40;
    LANDMARK_SCRATCH->local = worldmap_identity_matrix;
    LANDMARK_SCRATCH->local.t[0] = LANDMARK_SCRATCH->position.vx;
    LANDMARK_SCRATCH->local.t[1] = LANDMARK_SCRATCH->position.vy;
    LANDMARK_SCRATCH->local.t[2] = -LANDMARK_SCRATCH->position.vz;
    CompMatrix(&worldmap_camera_matrix, &LANDMARK_SCRATCH->local, &LANDMARK_SCRATCH->view);
    SetRotMatrix(&LANDMARK_SCRATCH->view);
    SetTransMatrix(&LANDMARK_SCRATCH->view);
    LANDMARK_SCRATCH->origin.vx = LANDMARK_SCRATCH->origin.vy = LANDMARK_SCRATCH->origin.vz = 0;
    gte_ldv0(&LANDMARK_SCRATCH->origin);
    gte_rtps();
    flag = &LANDMARK_SCRATCH->flag;
    gte_stflg(flag);
    if (*flag >= 0) {
        depth = &LANDMARK_SCRATCH->depth;
        gte_stsz(depth);
        if (*depth < 0xD00) {
            gear_model_color_matrix = &worldmap_light_color_matrix;
            SetBackColor(0x40, 0x40, 0x40);
            gear_model_step_and_draw(&worldmap_camera_matrix, &worldmap_light_direction_matrix, worldmap_current_display_buffer->ot, worldmap_display_buffer_index, 1);
        }
    }
}

/* 800762FC: Wait for drawing and the vertical blank, then flush the instruction
 * cache inside a critical section. */
void worldmap_sync_and_flush_cache(void) {
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    DrawSync(0);
    VSync(0);
    FlushCache();
    ExitCriticalSection();
}

/* 8007634C: Pause: show the pause screen on the other buffer until button 0x800 is
 * pressed, then restore the display. */
void worldmap_run_pause_screen(void) {
    RECT rect;
    s32 saved;

    saved = pad_vblank_count;
    DrawSync(0);
    VSync(0);
    if (worldmap_display_buffer_index == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    PutDispEnv(&worldmap_display_buffers[1].disp);
    PutDrawEnv(&worldmap_display_buffers[1].draw);
    sound_silence_voices();
    do {
        DrawSync(0);
        VSync(0);
        sprite_upload_pause_image(0x88, 0x64);
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
    } while (!(worldmap_pad_port0_pressed & 0x800));
    sound_restore_voices();
    DrawSync(0);
    VSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0xD8);
    PutDispEnv(&worldmap_display_buffers[worldmap_display_buffer_index].disp);
    PutDrawEnv(&worldmap_display_buffers[worldmap_display_buffer_index].draw);
    pad_vblank_count = saved;
}

/* 80076594: Wait on the other buffer until the pad check (pad_get_controller_kind) succeeds,
 * then restore the display. */
void worldmap_wait_for_controller(void) {
    RECT rect;
    s32 saved;

    saved = pad_vblank_count;
    DrawSync(0);
    VSync(0);
    if (worldmap_display_buffer_index == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    PutDispEnv(&worldmap_display_buffers[1].disp);
    PutDrawEnv(&worldmap_display_buffers[1].draw);
    sound_silence_voices();
    do {
        DrawSync(0);
        VSync(0);
        sprite_upload_pause_image(0x88, 0x64);
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
    } while (pad_get_controller_kind(0) == 0);
    sound_restore_voices();
    DrawSync(0);
    VSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0, 0xD8);
    PutDispEnv(&worldmap_display_buffers[worldmap_display_buffer_index].disp);
    PutDrawEnv(&worldmap_display_buffers[worldmap_display_buffer_index].draw);
    pad_vblank_count = saved;
}

/* 800767D4: Replace the music: stop the current sequence and start `data` (the
 * contents of disc file `file`). */
void worldmap_replace_music(void *data, s32 file) {
    sound_stop_all_seqs();
    sound_release_seq((SoundSeq *)mode_music_seq);
    memcpy(mode_music_buffer, data, cd_get_aligned_file_size(file));
    mode_music_seq = (s32)sound_create_seq((SoundSeqHeader *)mode_music_buffer);
    sound_play_seq((SoundSeq *)mode_music_seq, 0x7F, 0);
}

/* 80076858: The point at t (0..0x1000) of the uniform quadratic B-spline over three
 * control points (weights s^2/2, st + 1/2 and t^2/2 with s = 1 - t), scaled
 * by 0x10000, to *out. */
void worldmap_eval_quadratic_bspline(s32 t, SVECTOR *p0, SVECTOR *p1, SVECTOR *p2, VECTOR *out) {
    s32 w0;
    s32 w1;
    s32 w2;
    s32 s;

    s = 0x1000 - t;
    w0 = (s * s * 8) >> 12;
    w1 = ((s * t) >> 8) + 0x8000;
    w2 = (t * t * 8) >> 12;
    out->vx = p0->vx * w0 + p1->vx * w1 + p2->vx * w2;
    out->vy = p0->vy * w0 + p1->vy * w1 + p2->vy * w2;
    out->vz = p0->vz * w0 + p1->vz * w1 + p2->vz * w2;
}

/* 80076954: Unpack a replacement area file and resolve its sections (no spots or
 * models). */
void worldmap_unpack_scene_area_data(void) {
    void *block;

    block = worldmap_area_data;
    worldmap_area_data = text_unpack_lzss_alloc(block, 0);
    heap_free(block);
    worldmap_object_models = (u8 *)worldmap_area_data + ((AreaHeader *)worldmap_area_data)->models;
    worldmap_object_meshes = (u8 *)worldmap_area_data + ((AreaHeader *)worldmap_area_data)->meshes;
    worldmap_object_placement_list = (u8 *)worldmap_area_data + ((AreaHeader *)worldmap_area_data)->placements;
    worldmap_billboard_lists = (BillboardList *)((u8 *)worldmap_area_data + ((AreaHeader *)worldmap_area_data)->billboards);
    worldmap_effect_emitters = (AreaObject *)((u8 *)worldmap_area_data + ((AreaHeader *)worldmap_area_data)->emitters);
    worldmap_texture_anim_section = (s32 *)((u8 *)worldmap_area_data + ((AreaHeader *)worldmap_area_data)->animations);
    worldmap_texture_anim2_section = (s32 *)((u8 *)worldmap_area_data + ((AreaHeader *)worldmap_area_data)->animations2);
}

/* 80076A14: Mode step that has nothing to do; always reports done. */
s32 worldmap_scene_frame_billboards_start(void) {
    return 1;
}

/* 80076A1C: One world-map frame of a scripted scene (no player input). */
s32 worldmap_scene_frame_billboards_update(void) {
    if (worldmap_view_kind == 0) {
        worldmap_camera_build_from_angles(&worldmap_view_setup);
    } else {
        worldmap_camera_build_look_at(&worldmap_view_setup);
    }
    worldmap_effects_run_emitters();
    worldmap_effects_draw_particles();
    worldmap_billboards_draw();
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
    return 1;
}

/* 80076B34: Run an actor's script until an opcode yields. The script is a stream of
 * halfwords: the word at the script position holds the opcode (low half, an
 * unchecked index into worldmap_actor_script_handlers) and the first argument (high half); the
 * next two halfwords are passed as the other two arguments whatever the
 * opcode. A handler returns the halfwords to advance, 0 to yield until the
 * next update. tools/analysis/overlay_scripts.py disassembles the scripts. */
s32 worldmap_actor_script_run(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    s32 step;
    s32 word;

    actor = &worldmap_actor_slots[index];
    step = 0;
    do {
        actor->u.script += step;
        word = *(s32 *)actor->u.script;
        step = worldmap_actor_script_handlers[word & 0xFFFF](actor, word >> 16, actor->u.script[2], actor->u.script[3]);
    } while (step != 0);
    return 1;
}

/* 80076BC4: Script opcode 0 (2 halfwords, never advances): end the world-map loop
 * (worldmap_loop_running) with exit 0 (worldmap_loop_result); the script stays on it. */
s32 worldmap_actor_script_exit_worldmap(void) {
    worldmap_loop_running = 0;
    worldmap_loop_result = 0;
    return 0;
}

/* 80076BDC: Script opcode 1 (2 halfwords: frames): wait. The first update loads the
 * actor's wait counter and yields; later updates count it down and advance
 * once it reaches 0. */
s32 worldmap_actor_script_wait(WorldmapActor *actor, s16 frames) {
    if (actor->wait == 0) {
        actor->wait = frames;
        return 0;
    }
    if (--actor->wait <= 0) {
        return 2;
    }
    return 0;
}

/* 80076C18: Script opcode 2 (4 halfwords: actor a, argument b, unused): send command 1
 * with argument b to actor slot a (worldmap_actor_request; dropped while that actor
 * has an argument pending). */
s32 worldmap_actor_script_send_actor(WorldmapActor *actor, s32 a, s32 b) {
    worldmap_actor_request(a, b);
    return 4;
}

/* 80076C3C: Script opcode 3 (4 halfwords: x, y, z): place the player at x, y, z world
 * units (worldmap_player_position, 20.12). */
s32 worldmap_actor_script_place_player(WorldmapActor *actor, s32 x, s32 y, s32 z) {
    worldmap_player_position.vx = x << 12;
    worldmap_player_position.vy = y << 12;
    worldmap_player_position.vz = z << 12;
    return 4;
}

/* 80076C68: Script opcode 4 (4 halfwords: x, y, z): set the scratchpad script vector,
 * where opcode 5 places emitters. */
s32 worldmap_actor_script_set_script_vector(WorldmapActor *actor, s16 x, s16 y, s16 z) {
    SCRIPT_VECTOR->vx = x;
    SCRIPT_VECTOR->vy = y;
    SCRIPT_VECTOR->vz = z;
    return 4;
}

/* 80076C88: Script opcode 5 (2 halfwords: group a): place emitter group a at the
 * script vector, unrotated, and start it unless one of its emitters is live
 * (worldmap_effects_start_emitters). */
s32 worldmap_actor_script_start_emitters(WorldmapActor *actor, s32 a) {
    worldmap_effects_start_emitters(a, SCRIPT_VECTOR, 0);
    return 2;
}

/* 80076CB4: Script opcode 6 (2 halfwords: group a): deactivate emitter group a
 * (worldmap_effects_stop_emitters). */
s32 worldmap_actor_script_stop_emitters(WorldmapActor *actor, s32 a) {
    worldmap_effects_stop_emitters(a);
    return 2;
}

/* 80076CD4: Script opcode 7 (2 halfwords: group a): stop the live effects of emitter
 * group a (worldmap_effects_stop_particles). */
s32 worldmap_actor_script_stop_effects(WorldmapActor *actor, s32 a) {
    worldmap_effects_stop_particles(a);
    return 2;
}

/* 80076CF4: Script opcode 8 (4 halfwords: level a, frames b, unused): fade the music
 * sequence mode_music_seq to level a over b frames, at once when b is 0
 * (sound_set_seq_fade). */
s32 worldmap_actor_script_fade_music(WorldmapActor *actor, s32 a, s32 b) {
    sound_set_seq_fade((SoundSeq *)mode_music_seq, a, b);
    return 4;
}

/* 80076D1C: Script opcode 9 (2 halfwords: sound): play effect `sound` of the area
 * sound bank at the default volume and pan (sound_play_effect). */
s32 worldmap_actor_script_play_sound(WorldmapActor *actor, s32 sound) {
    sound_play_effect((sound_effect_bank->id << 16) | sound);
    return 2;
}

/* 80076D50: Script opcode 10 (4 halfwords: sound, volume b, frames c): slide the volume
 * of the channels playing area-bank effect `sound` to b over c frames
 * (sound_slide_effect_volume). */
s32 worldmap_actor_script_slide_sound_volume(WorldmapActor *actor, s32 sound, s32 b, s32 c) {
    sound_slide_effect_volume((sound_effect_bank->id << 16) | sound, b, c);
    return 4;
}

/* 80076D8C: Script opcode 11 (4 halfwords: rate a, step b, unused): set the screen
 * fade's semi-transparency rate (worldmap_screen_fade_rate, of its draw-mode page) and its
 * per-frame brightness step (worldmap_screen_fade_step). */
s32 worldmap_actor_script_set_fade(WorldmapActor *actor, s32 a, s32 b) {
    worldmap_screen_fade_rate = a;
    worldmap_screen_fade_step = b;
    return 4;
}

/* 80076DA4: Move the actor's current point an eighth of the way to its target (snapping
 * when close) and aim the camera from it. */
void worldmap_camera_ease_angle(WorldmapActor *actor, VECTOR *work) {
    if ((actor->motion.vx != actor->u.step) | (actor->motion.vy != actor->unk54) |
        (actor->motion.vz != actor->unk58)) {
        work[0].vx = actor->u.step - actor->motion.vx;
        work[0].vy = actor->unk54 - actor->motion.vy;
        work[0].vz = actor->unk58 - actor->motion.vz;
        worldmap_wrap_offset(&work[0]);
        work[1].vx = work[0].vx >> 3;
        work[1].vy = work[0].vy >> 3;
        work[1].vz = work[0].vz >> 3;
        if (abs(work[1].vx) < 0x40) {
            actor->motion.vx = actor->u.step;
        } else {
            actor->motion.vx += work[1].vx;
        }
        if (abs(work[1].vy) < 0x40) {
            actor->motion.vy = actor->unk54;
        } else {
            actor->motion.vy += work[1].vy;
        }
        if (abs(work[1].vz) < 0x40) {
            actor->motion.vz = actor->unk58;
        } else {
            actor->motion.vz += work[1].vz;
        }
    }
    worldmap_camera_angle.vx = actor->motion.vx >> 12;
    worldmap_camera_angle.vy = actor->motion.vy >> 12;
    worldmap_camera_angle.vz = actor->motion.vz >> 12;
}

/* 80076F54: Ease the camera distance towards the actor's, snapping when close. */
void worldmap_camera_ease_distance(WorldmapActor *actor) {
    s32 target;
    s32 step;

    target = actor->unk5C;
    if (target != worldmap_camera_distance) {
        step = (target - worldmap_camera_distance) >> 3;
        if ((step < 0 ? -step : step) < 0x40) {
            worldmap_camera_distance = target;
        } else {
            worldmap_camera_distance += step;
        }
    }
}

/* 80076FA8: Move the actor an eighth of the way to the saved camera target (snapping when
 * close), scrolling the ground offset with it, and look at the actor. */
void worldmap_camera_ease_position(WorldmapActor *actor, VECTOR *work) {
    if ((actor->position.vx != worldmap_camera_follow_target.target.vx) | (actor->position.vy != worldmap_camera_follow_target.target.vy) |
        (actor->position.vz != worldmap_camera_follow_target.target.vz)) {
        work[0].vx = worldmap_camera_follow_target.target.vx - actor->position.vx;
        work[0].vy = worldmap_camera_follow_target.target.vy - actor->position.vy;
        work[0].vz = worldmap_camera_follow_target.target.vz - actor->position.vz;
        worldmap_wrap_offset(&work[0]);
        work[1].vx = work[0].vx >> 3;
        work[1].vy = work[0].vy >> 3;
        work[1].vz = work[0].vz >> 3;
        if (abs(work[1].vx) < 0x200) {
            GROUND_SCROLL[0] += work[0].vx;
            actor->position.vx = worldmap_camera_follow_target.target.vx;
        } else {
            GROUND_SCROLL[0] += work[1].vx;
            actor->position.vx += work[1].vx;
        }
        if (abs(work[1].vy) < 0x200) {
            actor->position.vy = worldmap_camera_follow_target.target.vy;
        } else {
            actor->position.vy += work[1].vy;
        }
        if (abs(work[1].vz) < 0x200) {
            GROUND_SCROLL[2] += work[0].vz;
            actor->position.vz = worldmap_camera_follow_target.target.vz;
        } else {
            GROUND_SCROLL[2] += work[1].vz;
            actor->position.vz += work[1].vz;
        }
    }
    worldmap_camera.target = actor->position;
}

/* 800771D8: Step `value` towards `target` by `delta`, stopping on it. */
s32 worldmap_step_value_toward(s32 value, s32 target, s32 delta) {
    s32 distance;
    s32 size;

    if (value != target) {
        distance = abs(target - value);
        size = abs(delta);
        if (distance < size) {
            value = target;
        } else {
            value += delta;
        }
    }
    return value;
}

/* 80077214: Set up the scripted flight scene: display, terrain loader, scene objects and
 * its four actors. */
void worldmap_scene8_start(void) {
    RECT rect;

    worldmap_init_display();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    worldmap_fade_saved_screen(0x40, 0, 4, 2);
    while (cd_get_pending_read_count() >= 3) {
    }
    worldmap_unpack_area_data();
    worldmap_actor_alloc_slots();
    worldmap_unread_mode_matrix = worldmap_identity_matrix;
    worldmap_screen_fade_rate = 2;
    worldmap_screen_fade_step = 4;
    worldmap_menu_requested = 0;
    worldmap_view_kind = 0;
    worldmap_cloud_draw_hook = worldmap_clouds_move;
    worldmap_terrain_compute_cull_normals();
    cd_sync_reads(0);
    worldmap_player_position.vx = 0x7702000;
    worldmap_player_position.vy = -0x300000;
    worldmap_player_position.vz = 0x27C0000;
    worldmap_objects_build();
    worldmap_upload_area_image();
    worldmap_terrain_upload_image();
    worldmap_sky_init();
    worldmap_billboards_resolve_lists();
    worldmap_clouds_scatter();
    worldmap_texture_anim_create();
    worldmap_texture_anim2_create();
    worldmap_horizon_init();
    worldmap_effects_alloc_slots();
    cd_select_directory(0x24, 0);
    worldmap_terrain_reset_at(&worldmap_player_position);
    do {
        worldmap_stream_step();
        VSync(0);
    } while (worldmap_stream_count_queued() > 0);
    worldmap_actor_spawn((s32)worldmap_screen_fade_start, (s32)worldmap_screen_fade_update);
    worldmap_actor_spawn((s32)worldmap_scene8_camera_start, (s32)worldmap_scene8_camera_update);
    worldmap_actor_spawn((s32)worldmap_spinning_pair_start, (s32)worldmap_spinning_pair_update);
    worldmap_actor_spawn((s32)worldmap_open_map_frame_start, (s32)worldmap_open_map_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_billboards_alloc_quads();
    worldmap_encounter_reset_timers();
    worldmap_effects_start_emitters(0xE, NULL, NULL);
}

/* 80077480: Leave for scene 0x11: shut down the subsystems and free the area. */
void worldmap_scene8_leave(void) {
    worldmap_objects_free();
    worldmap_billboards_free_quads();
    worldmap_clouds_free();
    worldmap_clouds_free_quads();
    worldmap_texture_anim_free();
    worldmap_texture_anim2_free();
    worldmap_effects_free_slots();
    worldmap_effects_free_quads();
    worldmap_terrain_free_blocks();
    heap_free(worldmap_display_buffers[0].ot);
    heap_free(worldmap_display_buffers[1].ot);
    heap_free(worldmap_display_buffers[0].packets);
    heap_free(worldmap_display_buffers[1].packets);
    heap_free(worldmap_area_data);
    worldmap_actor_free_slots();
    game_data.map = 0x11;
    game_data.entry[2] = 7;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 8007756C: Start a scripted camera looking along the player's heading from above. */
s32 worldmap_scene8_camera_start(s32 index) {
    WorldmapActor *actor;

    worldmap_camera_angle.vx = -0x80;
    worldmap_camera_angle.vy = 0x200;
    worldmap_camera_distance = 0x400000;
    worldmap_camera_angle.vz = 0;
    actor = &worldmap_actor_slots[index];
    actor->position.vx = -0x80000;
    worldmap_view_center_y = 0x78;
    actor->position.vy = worldmap_camera_angle.vy << 12;
    actor->motion = actor->position;
    worldmap_view_kind = 0;
    worldmap_camera.target.vx = worldmap_player_position.vx;
    worldmap_camera.target.vy = worldmap_player_position.vy;
    worldmap_camera.target.vz = worldmap_player_position.vz;
    worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    *SCRIPT_VECTOR = VIEW_VECTORS[0];
    VIEW_VECTORS[0] = VIEW_VECTORS[1];
    VIEW_VECTORS[1] = *SCRIPT_VECTOR;
    return 1;
}

/* 800776E0: Steer the scripted camera with the pad (clamped yaw range, pitch), ease its
 * angles and swap the view vectors. */
s32 worldmap_scene8_camera_update(s32 index) {
    CameraScratch *scratch;
    WorldmapActor *actor;

    scratch = (CameraScratch *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    if (worldmap_pad_port0_pressed & 0x40) {
        worldmap_loop_running = 0;
        worldmap_loop_result = 0;
    }
    if (worldmap_pad_port0_held & 0x1000) {
        actor->position.vx += 0x8000;
    }
    if (worldmap_pad_port0_held & 0x4000) {
        actor->position.vx -= 0x8000;
    }
    if (actor->position.vx < -0x180000) {
        actor->position.vx = -0x180000;
    } else if (actor->position.vx > 0x18000) {
        actor->position.vx = 0x18000;
    }
    if (worldmap_pad_port0_held & 0x8004) {
        actor->position.vy -= 0x10000;
    }
    if (worldmap_pad_port0_held & 0x2008) {
        actor->position.vy += 0x10000;
    }
    if ((actor->motion.vx != actor->position.vx) | (actor->motion.vy != actor->position.vy)) {
        scratch->delta.vx = actor->position.vx - actor->motion.vx;
        scratch->delta.vy = actor->position.vy - actor->motion.vy;
        actor->motion.vx += scratch->delta.vx >> 3;
        actor->motion.vy += scratch->delta.vy >> 3;
    }
    worldmap_camera_angle.vx = actor->motion.vx >> 12;
    worldmap_camera_angle.vy = actor->motion.vy >> 12;
    worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    scratch->view = VIEW_VECTORS[0];
    VIEW_VECTORS[0] = VIEW_VECTORS[1];
    VIEW_VECTORS[1] = scratch->view;
    worldmap_camera_angle.vy = (worldmap_camera_angle.vy + 0x800) & 0xFFF;
    SetGeomScreen(worldmap_projection_distance);
    return 1;
}

/* 80077954: Mode step that has nothing to do; always reports done. */
s32 worldmap_scene_frame_no_effects_start(void) {
    return 1;
}

/* 8007795C: One world-map frame of a scripted scene without actor updates. */
s32 worldmap_scene_frame_no_effects_update(void) {
    if (worldmap_view_kind == 0) {
        worldmap_camera_build_from_angles(&worldmap_view_setup);
    } else {
        worldmap_camera_build_look_at(&worldmap_view_setup);
    }
    worldmap_billboards_draw();
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
    return 1;
}

/* 80077A64: Set up the scene mode: display, terrain loader, scene objects and its four
 * actors. */
void worldmap_scene9_start(void) {
    RECT rect;

    worldmap_init_display();
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x140;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    worldmap_fade_saved_screen(0x40, 0, 4, 1);
    while (cd_get_pending_read_count() >= 3) {
    }
    worldmap_unpack_scene_area_data();
    worldmap_actor_alloc_slots();
    worldmap_unread_mode_matrix = worldmap_identity_matrix;
    worldmap_screen_fade_rate = 1;
    worldmap_screen_fade_step = 4;
    worldmap_menu_requested = 0;
    worldmap_view_kind = 0;
    worldmap_cloud_draw_hook = worldmap_clouds_move;
    worldmap_terrain_compute_cull_normals();
    cd_sync_reads(0);
    worldmap_read_area_sound_bank();
    worldmap_player_position.vx = 0x2000000;
    worldmap_player_position.vy = -0x200000;
    worldmap_player_position.vz = 0x2000000;
    worldmap_objects_build();
    worldmap_upload_area_image();
    worldmap_terrain_upload_image();
    worldmap_sky_init();
    worldmap_clouds_scatter();
    worldmap_texture_anim_create();
    worldmap_texture_anim2_create();
    worldmap_horizon_init();
    worldmap_effects_alloc_slots();
    cd_sync_reads(0);
    sound_add_effect_bank(sound_effect_bank);
    cd_select_directory(0x24, 0);
    worldmap_terrain_reset_at(&worldmap_player_position);
    do {
        worldmap_stream_step();
        VSync(0);
    } while (worldmap_stream_count_queued() > 0);
    worldmap_actor_spawn((s32)worldmap_screen_fade_start, (s32)worldmap_screen_fade_update);
    worldmap_actor_spawn((s32)worldmap_scene9_camera_flight_start, (s32)worldmap_scene9_camera_flight_update);
    worldmap_actor_spawn((s32)worldmap_scene9_rig_start, (s32)worldmap_scene9_rig_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_start, (s32)worldmap_scene_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
}

/* 80077CC0: Leave for scene 0x10E: stop the sound bank, shut down and free the area. */
void worldmap_scene9_leave(void) {
    sound_stop_all_effects();
    sound_remove_effect_bank(sound_effect_bank);
    heap_free(sound_effect_bank);
    worldmap_objects_free();
    worldmap_clouds_free();
    worldmap_clouds_free_quads();
    worldmap_texture_anim_free();
    worldmap_texture_anim2_free();
    worldmap_effects_free_slots();
    worldmap_effects_free_quads();
    worldmap_terrain_free_blocks();
    heap_free(worldmap_display_buffers[0].ot);
    heap_free(worldmap_display_buffers[1].ot);
    heap_free(worldmap_display_buffers[0].packets);
    heap_free(worldmap_display_buffers[1].packets);
    heap_free(worldmap_area_data);
    worldmap_actor_free_slots();
    game_data.map = 0x10E;
    game_data.entry[2] = 0;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 80077DC8: Start a scripted camera: reset the actor and camera, play a sound. */
s32 worldmap_scene9_camera_flight_start(s32 index) {
    WorldmapActor *actor;

    worldmap_view_center_y = 0x78;
    worldmap_camera_distance = 0x200000;
    actor = &worldmap_actor_slots[index];
    actor->state = 0;
    actor->unk58 = 0;
    actor->unk54 = 0;
    actor->u.script = NULL;
    worldmap_camera_angle.vz = 0;
    worldmap_camera_angle.vy = 0;
    worldmap_camera_angle.vx = 0;
    worldmap_view_kind = 1;
    sound_play_effect((sound_effect_bank->id << 16) | 0xA4);
    actor->wait = 0x18;
    return 1;
}
