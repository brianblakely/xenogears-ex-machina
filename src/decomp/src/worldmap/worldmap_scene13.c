/* World map unit 80080370-800811C0 (rodata 800701E0-800702E4, data
 * 8009A698-8009A6C0): the director of scene mode 13 and its actors (the
 * camera, the effects and the growing objects), the set-up and leave
 * handlers of mode 16 and the sequence start of its director.
 *
 * worldmap_scene15_flame_update's five-entry table ends at 800701e0 and
 * worldmap_scene13_director_update's follows at once, 0 mod 8, a phase change
 * without a pad word: this unit's rodata starts there and its text after
 * worldmap_scene15_flame_update, at or before worldmap_scene13_director_update. Its
 * data opens with the cue sequence that worldmap_scene13_director_start, left in
 * the preceding unit by the split, starts. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* The director's cue sequence, user-supplied script data (an asset in
 * worldmap.classification.txt): 9 u16 states and 9 u16 waits (started by
 * worldmap_scene13_director_start; tools/analysis/overlay_scripts.py decodes it). The waits'
 * 0x14 bytes end with the stray halfword (0x7542) that follows the nine
 * waits at the end of the unit's data. */
INCLUDE_ASSET(".data", worldmap_scene13_cue_states, 0x8009A698, 0x12);
INCLUDE_ASSET(".data", worldmap_scene13_cue_waits, 0x8009A6AC, 0x14);

/* 80080370: Flight scene director (mode 13): worldmap_scene14_director_update's cue
 * sequencer on worldmap_scene13_cue_states/worldmap_scene13_cue_waits; its starter
 * does not step, so entry 0 runs twice. Actor slots (worldmap_scene13_start): 0 the
 * screen fade, 2 the camera (worldmap_scene13_camera_update), 3 effects 0x28-0x2A
 * (worldmap_scene13_effects_update), 4 the growing objects 0 and 1
 * (worldmap_scene13_grow_objects_0_1_update). A fade with rate 1 adds the fade quad
 * (white), with rate 2 subtracts it (black). */
s32 worldmap_scene13_director_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    switch (actor->state) {
    /* 0: idle. */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = worldmap_scene13_cue_states[actor->u.step];
            actor->wait = worldmap_scene13_cue_waits[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: slot 3 request 1. */
    case 2:
        worldmap_actor_request(3, 1);
        actor->state = 1;
        break;
    /* 3: slot 2 request 2. */
    case 3:
        worldmap_actor_request(2, 2);
        actor->state = 1;
        break;
    /* 4: fade out at rate 1, 0x80 per frame. */
    case 4:
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 1;
        worldmap_screen_fade_step = 0x80;
        actor->state = 1;
        break;
    /* 5: fade in at rate 1, 0x80 per frame; slot 4 request 1. */
    case 5:
        worldmap_actor_request(0, 0xC);
        worldmap_actor_request(4, 1);
        worldmap_screen_fade_rate = 1;
        worldmap_screen_fade_step = 0x80;
        actor->state = 1;
        break;
    /* 6: slot 2 request 3. */
    case 6:
        worldmap_actor_request(2, 3);
        actor->state = 1;
        break;
    /* 7: fade out at rate 2, 4 per frame. */
    case 7:
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 2;
        worldmap_screen_fade_step = 4;
        actor->state = 1;
        break;
    /* 8: area sounds 0x16-0x18. */
    case 8:
        sound_play_effect((sound_effect_bank->id << 16) | 0x16);
        sound_play_effect((sound_effect_bank->id << 16) | 0x17);
        sound_play_effect((sound_effect_bank->id << 16) | 0x18);
        actor->state = 1;
        break;
    /* 0x40: end the world-map loop with exit 0; idle. */
    case 0x40:
        worldmap_loop_running = 0;
        worldmap_loop_result = 0;
        actor->state = 0;
        break;
    }
    return 1;
}

/* 80080578: Start a scripted camera at the player position. */
s32 worldmap_scene13_camera_start(s32 index) {
    WorldmapActor *actor;

    worldmap_view_kind = 0;
    actor = &worldmap_actor_slots[index];
    actor->unk7C = 0x1000;
    worldmap_view_center_y = 0x78;
    worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = worldmap_player_position.vx;
    worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = worldmap_player_position.vy;
    worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = worldmap_player_position.vz;
    actor->unk4 = 1;
    actor->state = 0;
    return 1;
}

/* 80080600: Scripted camera: pull in and tilt (command 1), shake (2) or settle the shake (3). */
s32 worldmap_scene13_camera_update(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;
    s32 delta;

    scratch = (ActorScratch *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    switch (actor->unk4) {
    case 1:
        actor->state = 1;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = 0x10;
        worldmap_camera_angle.vy = 0x8E0;
        worldmap_camera_angle.vz = 0;
        actor->u.step = 0x800000;
        actor->unk54 = actor->unk58 = worldmap_camera_angle.vx << 12;
        worldmap_camera_distance = 0x800000;
        actor->unk5C = actor->unk60 = worldmap_camera_angle.vy << 12;
        break;
    case 2:
        actor->state = 2;
        actor->unk4 = 0;
        actor->unk7C = 0x8000;
        break;
    case 3:
        actor->state = 3;
        actor->unk4 = 0;
        actor->unk7C = 0x80000;
        break;
    }
    if (worldmap_view_kind == 0) {
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    }
    switch (actor->state) {
    case 0:
    case 2:
        break;
    case 1:
        if ((actor->u.step -= 0x10000) < 0x300000) {
            actor->u.step = 0x300000;
        }
        delta = actor->u.step - worldmap_camera_distance;
        if (delta != 0) {
            worldmap_camera_distance += delta >> 5;
        }
        if ((actor->unk54 -= 0x4000) < -0x100000) {
            actor->unk54 = -0x100000;
        }
        delta = actor->unk54 - actor->unk58;
        if (delta != 0) {
            actor->unk58 += delta >> 4;
            worldmap_camera_angle.vx = actor->unk58 >> 12;
        }
        if ((actor->unk5C -= 0x10000) < 0x2E0000) {
            actor->unk5C = 0x2E0000;
        }
        delta = actor->unk5C - actor->unk60;
        if (delta != 0) {
            actor->unk60 += delta >> 5;
            worldmap_camera_angle.vy = actor->unk60 >> 12;
        }
        break;
    case 3:
        if ((actor->unk7C -= 0x4000) < 0x1000) {
            actor->unk7C = 0x1000;
            actor->state = 0;
        }
        break;
    }
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    VIEW.eye.vy += scratch->position.vy;
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* 80080900: Copy the player position into the actor. */
s32 worldmap_scene13_effects_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->position.vx = worldmap_player_position.vx;
    actor->position.vy = worldmap_player_position.vy;
    actor->position.vz = worldmap_player_position.vz;
    return 1;
}

/* 80080944: On command, emit effects 0x28-0x2A at the actor. */
s32 worldmap_scene13_effects_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0x28, SCRIPT_VECTOR, 0);
        worldmap_effects_start_emitters(0x29, SCRIPT_VECTOR, 0);
        worldmap_effects_start_emitters(0x2A, SCRIPT_VECTOR, 0);
    }
    return 1;
}

/* 800809EC: Set the colour of `count` textured quads. */
void worldmap_set_quad_colors(POLY_FT4 *quads, s32 count, s32 r, s32 g, s32 b) {
    s32 i;

    for (i = 0; i < count; i++, quads++) {
        setRGB0(quads, r, g, b);
    }
}

/* 80080A28: Start a descent at the player and rebuild scene objects 0 and 1. */
s32 worldmap_scene13_grow_objects_0_1_start(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    actor->position.vx = worldmap_player_position.vx;
    actor->position.vy = -0x80000;
    actor->position.vz = worldmap_player_position.vz;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    worldmap_build_translucent_quads(object, object->prims, object->def->primitive_count);
    object++;
    worldmap_build_translucent_quads(object, object->prims, object->def->primitive_count);
    return 3;
}

/* 80080AC4: Grow and fade scene objects 0 and 1 at the actor; ends the step when faded out. */
s32 worldmap_scene13_grow_objects_0_1_update(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
    }
    object[0].position.vx = object[1].position.vx = actor->position.vx >> 12;
    object[0].position.vy = object[1].position.vy = actor->position.vy >> 12;
    object[0].position.vz = object[1].position.vz = actor->position.vz >> 12;
    object[1].matrix = worldmap_identity_matrix;
    object[0].matrix = object[1].matrix;
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->u.step;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk54;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&object[0].matrix, &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&object[1].matrix, &SCALE_SCRATCH->scale[1]);
    if ((actor->u.step += 0x180) > 0x7FFF) {
        actor->u.step = 0x7FFF;
    }
    if ((actor->unk54 += 0x180) > 0x7FFF) {
        actor->unk54 = 0x7FFF;
    }
    worldmap_set_quad_colors((&object->prims)[worldmap_display_buffer_index], object->def->primitive_count, actor->unk58, actor->unk58, actor->unk58);
    object++;
    worldmap_set_quad_colors((&object->prims)[worldmap_display_buffer_index], object->def->primitive_count, actor->unk58, actor->unk58, actor->unk58);
    if ((actor->unk58 -= 4) < 0) {
        actor->unk58 = 0;
        return 3;
    }
    return 1;
}

/* 80080D00: Set up the heat-haze scene: fixed start position, music, its director
 * and effect actors. */
void worldmap_scene16_start(void) {
    RECT rect;
    SoundSeq *sequence;
    void *data;
    u16 debug;

    worldmap_init_display();
    rect.w = 0x140;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    worldmap_fade_saved_screen(0x40, 0, 4, 2);
    while (cd_get_pending_read_count() >= 3) {
    }
    worldmap_unpack_scene_area_data();
    worldmap_actor_alloc_slots();
    worldmap_unread_mode_matrix = worldmap_identity_matrix;
    worldmap_screen_fade_rate = 2;
    worldmap_screen_fade_step = 0x10;
    worldmap_menu_requested = 0;
    worldmap_view_kind = 0;
    worldmap_cloud_draw_hook = worldmap_clouds_move;
    worldmap_terrain_compute_cull_normals();
    cd_sync_reads(0);
    mode_stop_music();
    worldmap_player_position.vx = 0x4000000;
    worldmap_player_position.vy = -0xC0000;
    worldmap_player_position.vz = 0x4000000;
    worldmap_objects_build();
    worldmap_upload_area_image();
    worldmap_terrain_upload_image();
    worldmap_read_area_sound_files();
    worldmap_sky_init();
    worldmap_clouds_scatter();
    worldmap_texture_anim_create();
    worldmap_texture_anim2_create();
    worldmap_horizon_init();
    worldmap_effects_alloc_slots();
    cd_sync_reads(0);
    mode_music_wave_bank = sound_load_wave_bank(worldmap_wave_bank, 0);
    cd_select_directory(0x24, 0);
    worldmap_terrain_reset_at(&worldmap_player_position);
    do {
        worldmap_stream_step();
        VSync(0);
    } while (worldmap_stream_count_queued() > 0);
    debug = sound_driver_flags & 0x10;
    if (debug) {
        while (debug) {
        }
    }
    heap_free(worldmap_wave_bank);
    sound_add_effect_bank(sound_effect_bank);
    data = worldmap_music;
    memcpy(mode_music_buffer, data, cd_get_aligned_file_size(worldmap_music_file));
    sequence = sound_create_seq((SoundSeqHeader *)mode_music_buffer);
    mode_music_seq = (s32)sequence;
    sound_play_seq(sequence, 0x7F, 0);
    worldmap_actor_spawn((s32)worldmap_screen_fade_start, (s32)worldmap_screen_fade_update);
    worldmap_actor_spawn((s32)worldmap_scene16_director_start, (s32)worldmap_scene16_director_update);
    worldmap_actor_spawn((s32)worldmap_scene16_camera_start, (s32)worldmap_scene16_camera_update);
    worldmap_actor_spawn((s32)worldmap_scene16_fade_object2_start, (s32)worldmap_scene16_fade_object2_update);
    worldmap_actor_spawn((s32)worldmap_scene16_fade_objects_0_1_start, (s32)worldmap_scene16_fade_objects_0_1_update);
    worldmap_actor_spawn((s32)worldmap_scene16_haze_start, (s32)worldmap_scene16_haze_update);
    worldmap_actor_spawn((s32)worldmap_scene16_haze_strength_start, (s32)worldmap_scene16_haze_strength_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_start, (s32)worldmap_scene_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 8008106C: Leave the world map for scene 0x1FA (flag word 0). */
void worldmap_scene16_leave(void) {
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
    game_data.map = 0x1FA;
    game_data.entry[2] = 0;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 80081174: Start an actor's timed sequence: first state and its duration. */
s32 worldmap_scene16_director_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->state = worldmap_scene16_cue_states[0];
    actor->wait = worldmap_scene16_cue_waits[actor->u.step];
    actor->u.step++;
    return 1;
}
