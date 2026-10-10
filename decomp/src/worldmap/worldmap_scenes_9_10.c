/* World map unit 80077E68-8007A9F8 (rodata 8006FB40-8006FBC4, data
 * 8009A3F0-8009A450): the actors of scene modes 9 (the camera flight and the
 * rig) and 10 (its script, the landing rig and the growing sprites), the
 * frame step of most scene modes, the set-up and leave handlers of modes 10
 * and 14 and the sequence start of mode 14's director.
 *
 * worldmap_restore_player_position's seven-entry table ends at 8006fb40 and worldmap_scene9_camera_flight_update's
 * follows at once, 0 mod 8: within one unit an odd-length table followed by
 * another keeps its phase with a pad word, so this unit's rodata starts at
 * 8006fb40 and its text after worldmap_restore_player_position, at or before worldmap_scene9_camera_flight_update. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* The scene camera's flight path: control points, the last marked by pad -1. */
SVECTOR worldmap_scene9_camera_path_points[12] = { /* 8009A3F0 */
    {0, -228, -425}, {301, -228, -301}, {490, -364, 0}, {358, -437, 358}, {0, -512, 512},
    {-400, -625, 400}, {-577, -675, -577}, {0, -904, -946}, {96, -608, 0}, {0, -1157, 1106},
    {-793, -899, -771}, {0, 0, 0, -1},
};

/* Scratchpad work area of the camera path. */
typedef struct {
    VECTOR at;         /* 0x00: path point, then look-at */
    VECTOR eye;        /* 0x10 */
    s32 distance;      /* 0x20 */
    u8 pad24[0x7C];
    SVECTOR points[3]; /* 0xA0 */
} PathScratch;

#define PATH_SCRATCH ((PathScratch *)0x1F800000)

/* 80077E68: Scene camera flight along the path: follow the path points, speed up and
 * slow down by state, fade out at the end, and set the engine volume from the
 * eye's distance. */
s32 worldmap_scene9_camera_flight_update(s32 index) {
    WorldmapActor *actor;
    SVECTOR *points;
    s32 segment;
    s32 distance;
    PathScratch *scratch;

    actor = &worldmap_actor_slots[index];
    scratch = PATH_SCRATCH;
    points = worldmap_scene9_camera_path_points;
    segment = actor->u.step >> 12;
    points += segment;
    actor->unk54 = segment;
    if (points[2].pad != -1) {
        PATH_SCRATCH->points[0].vx = points[0].vx;
        PATH_SCRATCH->points[0].vz = points[0].vz;
        PATH_SCRATCH->points[1].vx = points[1].vx;
        PATH_SCRATCH->points[1].vz = points[1].vz;
        PATH_SCRATCH->points[2].vx = points[2].vx;
        PATH_SCRATCH->points[2].vz = points[2].vz;
        PATH_SCRATCH->points[0].vy = points[0].vy;
        PATH_SCRATCH->points[1].vy = points[1].vy;
        PATH_SCRATCH->points[2].vy = points[2].vy;
        worldmap_eval_quadratic_bspline(actor->u.step & 0xFFF, &PATH_SCRATCH->points[0], &PATH_SCRATCH->points[1],
                      &PATH_SCRATCH->points[2], &scratch->at);
        VIEW.at.vz = 0;
        VIEW.at.vx = 0;
        VIEW.up.vz = 0;
        VIEW.up.vx = 0;
        VIEW.up.vy = -0x1000;
        VIEW.eye.vx = scratch->at.vx >> 16;
        VIEW.eye.vz = -(scratch->at.vz >> 16);
        VIEW.eye.vy = scratch->at.vy >> 16;
        VIEW.at.vy = worldmap_camera.target.vy >> 12;
    }
    worldmap_camera_build_look_at(&worldmap_view_setup);
    worldmap_get_matrix_angles(&worldmap_camera_matrix, &worldmap_camera_angle);
    switch (actor->state) {
    case 0:
        actor->unk58 += 5;
        if (--actor->wait < 0) {
            actor->unk58 = 0x80;
            actor->state++;
            scratch->points[0].vy = -0x20D;
            scratch->points[0].vx = (worldmap_camera.target.vx >> 12) + 0x32;
            scratch->points[0].vz = (worldmap_camera.target.vz >> 12) - 0x70;
            worldmap_effects_start_emitters(8, &scratch->points[0], NULL);
        }
        break;
    case 1:
        if (actor->u.step >= 0x6000) {
            actor->state++;
        }
        break;
    case 2:
        actor->unk58 -= 2;
        if (actor->unk58 < 0x19) {
            actor->unk58 = 0x18;
            actor->state++;
        }
        break;
    case 3:
        if (actor->u.step > 0x7FFF) {
            actor->state++;
        }
        break;
    case 4:
        actor->unk58 -= 1;
        if (actor->unk58 < 5) {
            actor->unk58 = 4;
            actor->state++;
        }
        break;
    case 5:
        if (actor->u.step > 0x83FF) {
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0xA4, 0, 0x100);
            worldmap_actor_request(0, 0xD);
            worldmap_screen_fade_rate = 2;
            worldmap_screen_fade_step = 4;
            actor->wait = 0x50;
            actor->unk58 = 0;
            actor->state++;
        }
        break;
    case 6:
        if (--actor->wait <= 0) {
            worldmap_loop_running = 0;
            worldmap_loop_result = 0;
        }
        break;
    }
    actor->u.step += actor->unk58;
    if (actor->state < 6) {
        scratch->at.vx = VIEW.at.vx << 12;
        scratch->at.vy = VIEW.at.vy << 12;
        scratch->at.vz = VIEW.at.vz << 12;
        scratch->eye.vx = VIEW.eye.vx << 12;
        scratch->eye.vy = VIEW.eye.vy << 12;
        scratch->eye.vz = VIEW.eye.vz << 12;
        distance = worldmap_get_ground_distance(&scratch->at, &scratch->eye) >> 3;
        scratch->distance = distance;
        sound_set_effect_volume((sound_effect_bank->id << 16) | 0xA4, 0x87 - distance);
    }
    return 1;
}

/* 8007828C: Link scene objects 1-13 to object 0 and put the scene camera on the player. */
s32 worldmap_scene9_rig_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(0, 1);
    worldmap_objects_link(0, 2);
    worldmap_objects_link(0, 3);
    worldmap_objects_link(0, 4);
    worldmap_objects_link(0, 5);
    worldmap_objects_link(0, 6);
    worldmap_objects_link(0, 7);
    worldmap_objects_link(0, 8);
    worldmap_objects_link(0, 9);
    worldmap_objects_link(0, 0xA);
    worldmap_objects_link(0, 0xB);
    worldmap_objects_link(0, 0xC);
    worldmap_objects_link(0, 0xD);
    actor = &worldmap_actor_slots[index];
    actor->state = 0;
    actor->position = worldmap_player_position;
    actor->unk54 = 0x40;
    actor->unk58 = 0x200;
    actor->unk5C = 0x60;
    actor->unk60 = 0x300;
    actor->u.script = NULL;
    actor->unk64 = 0x50;
    worldmap_camera.target = actor->position;
    return 1;
}

/* Scratchpad work area of the scene rigs. */
typedef struct {
    VECTOR position;   /* 0x00 */
    u8 pad10[0x90];
    SVECTOR angle[4];  /* 0xA0 */
    u8 padC0[0x30];
    MATRIX matrix[4];  /* 0xF0 */
} RigScratch;

#define RIG_SCRATCH ((RigScratch *)0x1F800000)

/* 800783E8: Fly the scene rig forward: move the actor and the ground scroll, place scene
 * object 0 and the camera on it, spin the three rotors and share their
 * matrices across the rig's objects. */
s32 worldmap_scene9_rig_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->position.vz -= 0x4000;
    GROUND_SCROLL[2] -= 0x4000;
    worldmap_wrap_position(&actor->position);
    RIG_SCRATCH->position.vx = actor->position.vx >> 12;
    RIG_SCRATCH->position.vy = actor->position.vy >> 12;
    RIG_SCRATCH->position.vz = actor->position.vz >> 12;
    worldmap_objects[0].position = RIG_SCRATCH->position;
    worldmap_camera.target = actor->position;
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    actor->unk58 = (actor->unk58 + actor->unk5C) & 0xFFF;
    actor->unk60 = (actor->unk60 + actor->unk64) & 0xFFF;
    RIG_SCRATCH->angle[0].vx = RIG_SCRATCH->angle[0].vz = RIG_SCRATCH->angle[1].vx =
        RIG_SCRATCH->angle[1].vz = RIG_SCRATCH->angle[2].vx = RIG_SCRATCH->angle[2].vz = 0;
    RIG_SCRATCH->angle[0].vy = actor->u.step;
    RIG_SCRATCH->angle[1].vy = actor->unk58;
    RIG_SCRATCH->angle[2].vy = actor->unk60;
    RIG_SCRATCH->angle[3].vx = RIG_SCRATCH->angle[3].vy = 0;
    RIG_SCRATCH->angle[3].vz = actor->unk60;
    RotMatrixYXZ(&RIG_SCRATCH->angle[0], &RIG_SCRATCH->matrix[0]);
    RotMatrixYXZ(&RIG_SCRATCH->angle[1], &RIG_SCRATCH->matrix[1]);
    RotMatrixYXZ(&RIG_SCRATCH->angle[2], &RIG_SCRATCH->matrix[2]);
    RotMatrixYXZ(&RIG_SCRATCH->angle[3], &RIG_SCRATCH->matrix[3]);
    worldmap_objects[1].matrix = worldmap_objects[2].matrix = worldmap_objects[3].matrix = RIG_SCRATCH->matrix[3];
    worldmap_objects[4].matrix = worldmap_objects[5].matrix = RIG_SCRATCH->matrix[2];
    worldmap_objects[6].matrix = worldmap_objects[10].matrix = worldmap_objects[8].matrix = worldmap_objects[12].matrix =
        RIG_SCRATCH->matrix[0];
    worldmap_objects[7].matrix = worldmap_objects[11].matrix = worldmap_objects[9].matrix = worldmap_objects[13].matrix =
        RIG_SCRATCH->matrix[1];
    return 1;
}

/* 80078948: Mode step that has nothing to do; always reports done. */
s32 worldmap_scene_frame_start(void) {
    return 1;
}

/* 80078950: Per-frame update and draw of the scene mode. */
s32 worldmap_scene_frame_update(void) {
    if (worldmap_view_kind == 0) {
        worldmap_camera_build_from_angles(&worldmap_view_setup);
    } else {
        worldmap_camera_build_look_at(&worldmap_view_setup);
    }
    worldmap_effects_run_emitters();
    worldmap_effects_draw_particles();
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

/* 80078A60: Set up the first cutscene mode: display, terrain loader, scene objects and
 * its actors. */
void worldmap_scene10_start(void) {
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
    worldmap_read_area_sound_bank();
    worldmap_player_position.vx = 0x2000000;
    worldmap_player_position.vy = -0x300000;
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
    worldmap_actor_spawn((s32)worldmap_scene10_script_start, (s32)worldmap_scene10_script_update);
    worldmap_actor_spawn((s32)worldmap_scene10_landing_rig_start, (s32)worldmap_scene10_landing_rig_update);
    worldmap_actor_spawn((s32)worldmap_scene10_grow_sprites_14_15_start, (s32)worldmap_scene10_grow_sprites_14_15_update);
    worldmap_actor_spawn((s32)worldmap_scene10_group9_trail_start, (s32)worldmap_scene10_group9_trail_update);
    worldmap_actor_spawn((s32)worldmap_scene10_group10_burst_start, (s32)worldmap_scene10_group10_burst_update);
    worldmap_actor_spawn((s32)worldmap_scene10_ground_object16_start, (s32)worldmap_scene10_ground_object16_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_start, (s32)worldmap_scene_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 80078D24: Leave the scene: release its resources and continue in scene 0x110. */
void worldmap_scene10_leave(void) {
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
    game_data.map = 0x110;
    game_data.entry[2] = 0;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 80078E2C: Start a scripted camera looking down from yaw 0x680. */
s32 worldmap_scene10_script_start(s32 index) {
    WorldmapActor *actor;

    worldmap_view_center_y = 0x78;
    worldmap_camera_distance = 0x200000;
    worldmap_view_kind = 0;
    actor = &worldmap_actor_slots[index];
    actor->state = 0x10;
    actor->unk58 = 0;
    actor->unk54 = 0;
    actor->u.script = NULL;
    worldmap_camera_angle.vx = -0xC0;
    worldmap_camera_angle.vy = 0x680;
    worldmap_camera_angle.vz = 0;
    actor->wait = 0x40;
    actor->unk74 = 0;
    return 1;
}

/* 80078EA4: Scene script: start the effects in turn, shake the view while the engines
 * run, then fade out; states 16-18 are the scene's opening. */
s32 worldmap_scene10_script_update(s32 index) {
    CameraScratch *scratch;
    WorldmapActor *actor;
    s16 trigger;

    actor = &worldmap_actor_slots[index];
    scratch = (CameraScratch *)0x1F800000;
    switch (actor->state) {
    case 0:
        if (--actor->wait <= 0) {
            actor->wait = 0x20;
            actor->state++;
            worldmap_actor_request(5, 1);
            sound_play_effect((sound_effect_bank->id << 16) | 0x62);
            sound_play_effect((sound_effect_bank->id << 16) | 0x63);
        }
        break;
    case 1:
        if (--actor->wait <= 0) {
            actor->wait = 0x20;
            actor->state++;
            worldmap_actor_request(3, 1);
        }
        break;
    case 2:
        if (--actor->wait <= 0) {
            actor->wait = 0x30;
            actor->state++;
            worldmap_actor_request(4, 1);
            worldmap_actor_request(2, 1);
            worldmap_actor_request(6, 1);
        }
        break;
    case 3:
        if (--actor->wait <= 0) {
            actor->wait = 0x28;
            actor->state++;
        }
        break;
    case 4:
        actor->wait--;
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
        scratch->view.vx = rand() % 12 - 6;
        scratch->view.vy = rand() % 12 - 6;
        VIEW_VECTORS[0].vx += scratch->view.vx;
        VIEW_VECTORS[1].vx += scratch->view.vx;
        VIEW_VECTORS[0].vy += scratch->view.vy;
        VIEW_VECTORS[1].vy += scratch->view.vy;
        if (actor->wait <= 0) {
            actor->wait = 0x78;
            actor->state++;
            sound_play_effect((sound_effect_bank->id << 16) | 0x79);
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x62, 0, 0x100);
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x63, 0, 0x100);
        }
        break;
    case 5:
        actor->wait--;
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
        scratch->view.vx = rand() % 4 - 2;
        scratch->view.vy = rand() % 4 - 2;
        VIEW_VECTORS[0].vx += scratch->view.vx;
        VIEW_VECTORS[0].vy += scratch->view.vy;
        VIEW_VECTORS[1].vx += scratch->view.vx;
        VIEW_VECTORS[1].vy += scratch->view.vy;
        if (actor->wait <= 0) {
            actor->wait = 0x5A;
            actor->state++;
        }
        break;
    case 6:
        if (--actor->wait <= 0) {
            actor->wait = 0x50;
            actor->state++;
            worldmap_actor_request(0, 0xD);
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x79, 0, 0x100);
            worldmap_screen_fade_rate = 2;
            worldmap_screen_fade_step = 4;
        }
        break;
    case 7:
        if (--actor->wait <= 0) {
            actor->wait = 0;
            worldmap_loop_running = 0;
            worldmap_loop_result = 0;
        }
        break;
    case 16:
        trigger = actor->unk4;
        if (trigger == 1) {
            sound_slide_effect_volume((sound_effect_bank->id << 16) | 0x36, 0, 8);
            worldmap_actor_request(0, 0xD);
            worldmap_screen_fade_step = 0x20;
            worldmap_screen_fade_rate = trigger;
            actor->wait = 8;
            actor->unk4 = 0;
            actor->state++;
        }
        break;
    case 17:
        if (--actor->wait <= 0) {
            worldmap_camera_distance = 0x960000;
            worldmap_camera_angle.vx = -0x30;
            worldmap_camera_angle.vy = 0x40;
            worldmap_camera_angle.vz = 0;
            actor->wait = 0x10;
            actor->state++;
        }
        break;
    case 18:
        if (--actor->wait <= 0) {
            worldmap_actor_request(0, 0xC);
            worldmap_screen_fade_rate = 1;
            worldmap_screen_fade_step = 0x80;
            actor->state = 0;
            actor->wait = 1;
        }
        break;
    }
    if (actor->state != 4 && actor->state != 5) {
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    }
    return 1;
}

/* 800794D8: Place the scene camera target and set the scene yaw. */
s32 worldmap_scene10_ground_object16_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->position.vx = 0x2000000;
    actor->position.vz = 0x1500000;
    actor->state = 0;
    worldmap_objects[16].angle.vx = 0;
    worldmap_objects[16].angle.vy = 0x780;
    worldmap_objects[16].angle.vz = 0;
    RotMatrixYXZ(&worldmap_objects[16].angle, &worldmap_objects[16].matrix);
    return 1;
}

/* 80079538: Keep the actor on the ground and scene object 16 at it. */
s32 worldmap_scene10_ground_object16_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    if (actor->unk4 != 0) {
        actor->unk4 = 0;
        worldmap_objects[16].visible = 1;
    }
    worldmap_wrap_position(&actor->position);
    actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) + 0x18000;
    worldmap_objects[16].position.vx = actor->position.vx >> 12;
    worldmap_objects[16].position.vy = actor->position.vy >> 12;
    worldmap_objects[16].position.vz = actor->position.vz >> 12;
    return 1;
}

/* 800795E4: Link scene objects 1-13 to object 0, put the camera on the player, tilt
 * object 0 and play sound 0x36 of the area bank. */
s32 worldmap_scene10_landing_rig_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(0, 1);
    worldmap_objects_link(0, 2);
    worldmap_objects_link(0, 3);
    worldmap_objects_link(0, 4);
    worldmap_objects_link(0, 5);
    worldmap_objects_link(0, 6);
    worldmap_objects_link(0, 7);
    worldmap_objects_link(0, 8);
    worldmap_objects_link(0, 9);
    worldmap_objects_link(0, 0xA);
    worldmap_objects_link(0, 0xB);
    worldmap_objects_link(0, 0xC);
    worldmap_objects_link(0, 0xD);
    actor = &worldmap_actor_slots[index];
    actor->state = 0;
    actor->position = worldmap_player_position;
    actor->unk54 = 0x40;
    actor->unk58 = 0x200;
    actor->unk5C = 0x60;
    actor->unk60 = 0x300;
    actor->u.script = NULL;
    actor->unk64 = 0x50;
    worldmap_camera.target = actor->position;
    worldmap_objects[0].angle.vx = -0x100;
    worldmap_objects[0].angle.vy = 0;
    worldmap_objects[0].angle.vz = -0x40;
    sound_play_effect((sound_effect_bank->id << 16) | 0x36);
    return 1;
}

/* 80079778: Scene rig landing: drop and brake the rig with exhaust effects, then show all
 * its objects; every frame place it and spin its rotors. */
s32 worldmap_scene10_landing_rig_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    RigScratch *scratch;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    scratch = (RigScratch *)0x1F800000;
    switch (actor->state) {
    case 0:
        scratch->angle[0].vx = actor->position.vx >> 12;
        scratch->angle[0].vy = actor->position.vy >> 12;
        scratch->angle[0].vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0xB, &scratch->angle[0], NULL);
        actor->position.vy += 0x4000;
        actor->position.vz -= 0x8000;
        GROUND_SCROLL[2] -= 0x8000;
        if (actor->position.vy >= -0x18000) {
            actor->motion.vy = -0x4000;
            actor->state++;
            worldmap_effects_start_emitters(0xC, &scratch->angle[0], NULL);
            sound_play_effect((sound_effect_bank->id << 16) | 0x71);
            actor->wait = 0x20;
        }
        break;
    case 1:
        scratch->angle[0].vx = actor->position.vx >> 12;
        scratch->angle[0].vy = actor->position.vy >> 12;
        scratch->angle[0].vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0xB, &scratch->angle[0], NULL);
        worldmap_effects_start_emitters(0xC, &scratch->angle[0], NULL);
        if (--actor->wait <= 0) {
            worldmap_effects_stop_emitters(0xC);
        }
        objects[0].angle.vx += 4;
        objects[0].angle.vz += 2;
        actor->position.vy += actor->motion.vy;
        actor->motion.vy += 0x100;
        actor->position.vz -= 0x8000;
        GROUND_SCROLL[2] -= 0x8000;
        if (actor->position.vy >= -0x18000) {
            actor->motion.vz = -0x8000;
            actor->state++;
            worldmap_effects_start_emitters(0xC, &scratch->angle[0], NULL);
            sound_play_effect((sound_effect_bank->id << 16) | 0x71);
        }
        break;
    case 2:
        scratch->angle[0].vx = actor->position.vx >> 12;
        scratch->angle[0].vy = actor->position.vy >> 12;
        scratch->angle[0].vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0xB, &scratch->angle[0], NULL);
        worldmap_effects_start_emitters(0xC, &scratch->angle[0], NULL);
        objects[0].angle.vx -= 2;
        objects[0].angle.vz -= 1;
        actor->position.vz += actor->motion.vz;
        GROUND_SCROLL[2] += actor->motion.vz;
        actor->motion.vz += 0x100;
        if (actor->position.vz <= 0x1580000) {
            actor->wait = 8;
            actor->state++;
            worldmap_effects_stop_emitters(0xC);
            worldmap_actor_request(1, 1);
        }
        break;
    case 3:
        objects[0].angle.vx -= 2;
        objects[0].angle.vz -= 1;
        actor->position.vz += actor->motion.vz;
        GROUND_SCROLL[2] += actor->motion.vz;
        actor->motion.vz += 0x100;
        if (--actor->wait <= 0) {
            worldmap_actor_request(1, 1);
            actor->state++;
        }
        break;
    case 4:
        if (actor->unk4 != 0) {
            actor->unk4 = 0;
            objects[0].visible = objects[1].visible = objects[2].visible = objects[3].visible =
                objects[4].visible = objects[5].visible = objects[6].visible = objects[7].visible =
                    objects[8].visible = objects[9].visible = objects[10].visible = objects[11].visible =
                        objects[12].visible = objects[13].visible = 1;
            worldmap_effects_stop_emitters(0xB);
        }
        break;
    }
    worldmap_wrap_position(&actor->position);
    worldmap_objects[0].position.vx = actor->position.vx >> 12;
    worldmap_objects[0].position.vy = actor->position.vy >> 12;
    worldmap_objects[0].position.vz = actor->position.vz >> 12;
    worldmap_camera.target = actor->position;
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    actor->unk58 = (actor->unk58 + actor->unk5C) & 0xFFF;
    actor->unk60 = (actor->unk60 + actor->unk64) & 0xFFF;
    scratch->angle[0].vx = scratch->angle[0].vz = scratch->angle[1].vx = scratch->angle[1].vz =
        scratch->angle[2].vx = scratch->angle[2].vz = 0;
    scratch->angle[0].vy = actor->u.step;
    scratch->angle[1].vy = actor->unk58;
    scratch->angle[2].vy = actor->unk60;
    scratch->angle[3].vx = scratch->angle[3].vy = 0;
    scratch->angle[3].vz = actor->unk60;
    RotMatrixYXZ(&scratch->angle[0], &scratch->matrix[0]);
    RotMatrixYXZ(&scratch->angle[1], &scratch->matrix[1]);
    RotMatrixYXZ(&scratch->angle[2], &scratch->matrix[2]);
    RotMatrixYXZ(&scratch->angle[3], &scratch->matrix[3]);
    worldmap_objects[1].matrix = worldmap_objects[2].matrix = worldmap_objects[3].matrix = scratch->matrix[3];
    worldmap_objects[4].matrix = worldmap_objects[5].matrix = scratch->matrix[2];
    worldmap_objects[6].matrix = worldmap_objects[10].matrix = worldmap_objects[8].matrix = worldmap_objects[12].matrix =
        scratch->matrix[0];
    worldmap_objects[7].matrix = worldmap_objects[11].matrix = worldmap_objects[9].matrix = worldmap_objects[13].matrix =
        scratch->matrix[1];
    RotMatrixYXZ(&worldmap_objects[0].angle, &scratch->matrix[0]);
    worldmap_objects[0].matrix = scratch->matrix[0];
    return 1;
}

/* 8007A06C: Build `count` semi-transparent textured quads for a scene sprite. */
void worldmap_build_translucent_quads(SceneObject *object, POLY_FT4 *quads, s32 count) {
    POLY_FT4 *quad;
    s32 i;

    quad = quads;
    for (i = 0; i < count; i++) {
        setPolyFT4(quad);
        quad->tpage = GetTPage(1, 3, 0x340, 0x100);
        quad->clut = GetClut(0x100, 0x1FF);
        setSemiTrans(quad, 1);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT4));
}

/* 8007A144: Rebuild both scene sprites' quads. */
s32 worldmap_scene10_grow_sprites_14_15_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->state = 0;
    actor->unk70 = 0;
    actor->unk6C = 0;
    worldmap_build_translucent_quads(&worldmap_objects[14], worldmap_objects[14].prims, worldmap_objects[14].def->primitive_count);
    worldmap_build_translucent_quads(&worldmap_objects[15], worldmap_objects[15].prims, worldmap_objects[15].def->primitive_count);
    return 3;
}

/* 8007A1B4: Grow scene sprites 14 and 15 at scene object 0: sprite 14 widens each frame,
 * sprite 15 steps once 14 is full; done (3) when 15 reaches 0x6000. */
s32 worldmap_scene10_grow_sprites_14_15_update(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;
    s32 result;

    objects = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    objects[14].position = objects[0].position;
    objects[15].position = objects[14].position;
    actor->unk6C += 0xC0;
    result = 1;
    if (actor->unk6C >= 0x800) {
        actor->unk70 = (actor->unk70 + 0x100) & 0x7FFF;
    }
    if (actor->unk70 >= 0x6000) {
        result = 3;
        actor->unk4 = 0;
        actor->unk70 = 0;
        actor->unk6C = 0;
    }
    SCALE_SCRATCH->matrix[0] = worldmap_identity_matrix;
    SCALE_SCRATCH->matrix[1] = SCALE_SCRATCH->matrix[0];
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->unk6C;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk70;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&SCALE_SCRATCH->matrix[0], &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&SCALE_SCRATCH->matrix[1], &SCALE_SCRATCH->scale[1]);
    objects[14].matrix = SCALE_SCRATCH->matrix[0];
    objects[15].matrix = SCALE_SCRATCH->matrix[1];
    return result;
}

/* 8007A410: Wait 0x60 frames. */
s32 worldmap_scene10_group9_trail_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->wait = 0x60;
    return 3;
}

/* 8007A430: Carry effect 9 forward from scene object 0 for `wait` frames, then stop it. */
s32 worldmap_scene10_group9_trail_update(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = 1;
    if (actor->unk4 == result) {
        actor->unk4 = 0;
        actor->position.vx = worldmap_objects[0].position.vx << 12;
        actor->position.vy = worldmap_objects[0].position.vy << 12;
        actor->position.vz = worldmap_objects[0].position.vz << 12;
    }
    if (--actor->wait > 0) {
        actor->position.vz += 0x20000;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(9, SCRIPT_VECTOR, NULL);
    } else {
        actor->wait = 0x60;
        actor->position = worldmap_player_position;
        worldmap_effects_stop_emitters(9);
        result = 3;
    }
    return result;
}

/* 8007A568: Scene step with nothing to do. */
s32 worldmap_scene10_group10_burst_start(void) {
    return 3;
}

/* 8007A570: Emit effect 0xA at the scene position. */
s32 worldmap_scene10_group10_burst_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->unk4 = 0;
    SCRIPT_VECTOR->vx = worldmap_objects[0].position.vx;
    SCRIPT_VECTOR->vy = worldmap_objects[0].position.vy;
    SCRIPT_VECTOR->vz = worldmap_objects[0].position.vz;
    worldmap_effects_start_emitters(0xA, SCRIPT_VECTOR, 0);
    return 3;
}

/* 8007A5DC: Set up the second cutscene mode: display, terrain loader, scene objects and
 * its actors. */
void worldmap_scene14_start(void) {
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
    worldmap_read_area_sound_bank();
    worldmap_player_position.vx = 0x5BED000;
    worldmap_player_position.vy = -0xA0000;
    worldmap_player_position.vz = 0x62A8000;
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
    worldmap_actor_spawn((s32)worldmap_scene14_director_start, (s32)worldmap_scene14_director_update);
    worldmap_actor_spawn((s32)worldmap_scene14_camera_start, (s32)worldmap_scene14_camera_update);
    worldmap_actor_spawn((s32)worldmap_scene14_grow_objects_4_5_start, (s32)worldmap_scene14_grow_objects_4_5_update);
    worldmap_actor_spawn((s32)worldmap_scene14_grow_objects_6_7_start, (s32)worldmap_scene14_grow_objects_6_7_update);
    worldmap_actor_spawn((s32)worldmap_scene14_exhaust_trail_start, (s32)worldmap_scene14_exhaust_trail_update);
    worldmap_actor_spawn((s32)worldmap_scene14_rig_flight_start, (s32)worldmap_scene14_rig_flight_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_start, (s32)worldmap_scene_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 8007A8AC: Leave the scene: release its resources and continue in scene 0x11A. */
void worldmap_scene14_leave(void) {
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
    game_data.map = 0x11A;
    game_data.entry[2] = 0;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 8007A9B4: Restart an actor's timed sequence at its first step. */
s32 worldmap_scene14_director_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->state = worldmap_scene14_cue_states[0];
    actor->wait = worldmap_scene14_cue_waits[actor->u.step];
    return 1;
}
