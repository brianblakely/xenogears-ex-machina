/* World map unit 8007C3B8-8007DE98 (rodata 8006FC50-8006FD8C, data
 * 8009A4D8-8009A5A0): the director of scene mode 12 and its actors (the
 * camera shots and the drifting scene objects), the set-up and leave
 * handlers of mode 15 and the sequence start of its director.
 *
 * worldmap_scene14_rig_flight_update's nine-entry table ends at 8006fc50 and
 * worldmap_scene12_director_update's follows at once, 0 mod 8, a phase change
 * without a pad word: this unit's rodata starts there and its text after
 * worldmap_scene14_rig_flight_update, at or before
 * worldmap_scene12_director_update. Its data opens with the cue sequence that
 * worldmap_scene12_director_start, left in the preceding unit by the split, starts. */
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
 * worldmap.classification.txt): 8 u16 states and 8 u16 waits (started by
 * worldmap_scene12_director_start; tools/analysis/overlay_scripts.py decodes it). */
INCLUDE_ASSET(".data", worldmap_scene12_cue_states, 0x8009A4D8, 0x10);
INCLUDE_ASSET(".data", worldmap_scene12_cue_waits, 0x8009A4E8, 0x10);

/* Camera shot paths: control points. */
SVECTOR worldmap_scene12_shot_path1_points[14] = { /* 8009A4F8 */
    {1629, -160, 1629}, {0, -160, 2304}, {-1629, -160, 1629}, {-2304, -160, 0},
    {-1621, -386, -1621}, {0, -525, -1623}, {814, -160, -814}, {1022, -110, 0},
    {515, -401, 516}, {0, -260, 503}, {-267, -235, 267}, {-383, -125, -6}, {-244, -55, -251},
    {6, -55, -352},
};
SVECTOR worldmap_scene12_shot_path2_points[7] = { /* 8009A568 */
    {4, -387, -369}, {-252, -507, -257}, {-272, -637, -2}, {-176, -892, 172}, {-209, -991, 360},
    {-495, -1063, 852}, {-703, -707, 1647},
};

/* 8007C3B8: Vehicle scene director (mode 12): worldmap_scene14_director_update's
 * cue sequencer on worldmap_scene12_cue_states/worldmap_scene12_cue_waits, whose
 * starter runs entry 0 at once. Actor slots (worldmap_scene12_start): 0 the screen
 * fade, 2 the camera shots (worldmap_scene12_camera_shots_update), 3-8 the drifting
 * scene objects
 * (worldmap_scene12_drift_object4_update-worldmap_scene12_drift_object13_update), 9
 * object 16 (worldmap_scene12_drift_object16_update). A fade with rate 1 adds the
 * fade quad (white), with rate 2 subtracts it (black). */
s32 worldmap_scene12_director_update(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;

    actor = &worldmap_actor_slots[index];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->state) {
    /* 0: idle (no entry holds it). */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = worldmap_scene12_cue_states[actor->u.step];
            actor->wait = worldmap_scene12_cue_waits[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: slot 2 request 1; area sounds 0xD-0xF. */
    case 2:
        worldmap_actor_request(2, 1);
        actor->state = 1;
        sound_play_effect((sound_effect_bank->id << 16) | 0xD);
        sound_play_effect((sound_effect_bank->id << 16) | 0xE);
        sound_play_effect((sound_effect_bank->id << 16) | 0xF);
        break;
    /* 0x10: start emitter group 0x14 at the camera target's x, z; area
     * sounds 0x10-0x12. */
    case 0x10:
        scratch->position.vy = 0;
        scratch->position.vx = worldmap_camera.target.vx >> 12;
        scratch->position.vz = worldmap_camera.target.vz >> 12;
        worldmap_effects_start_emitters(0x14, &scratch->position, NULL);
        sound_play_effect((sound_effect_bank->id << 16) | 0x10);
        sound_play_effect((sound_effect_bank->id << 16) | 0x11);
        sound_play_effect((sound_effect_bank->id << 16) | 0x12);
        actor->state = 1;
        break;
    /* 0x11: slot 2 request 2; fade out at rate 1, 0x40 per frame; area
     * sounds 0x13-0x15. */
    case 0x11:
        worldmap_actor_request(2, 2);
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 1;
        actor->state = 1;
        worldmap_screen_fade_step = 0x40;
        sound_play_effect((sound_effect_bank->id << 16) | 0x13);
        sound_play_effect((sound_effect_bank->id << 16) | 0x14);
        sound_play_effect((sound_effect_bank->id << 16) | 0x15);
        break;
    /* 0x12: slot 2 request 3, slots 3-7 request 1; fade in at rate 1, 1 per
     * frame. */
    case 0x12:
        worldmap_actor_request(2, 3);
        worldmap_actor_request(3, 1);
        worldmap_actor_request(4, 1);
        worldmap_actor_request(5, 1);
        worldmap_actor_request(6, 1);
        worldmap_actor_request(7, 1);
        worldmap_actor_request(0, 0xC);
        worldmap_screen_fade_rate = 1;
        worldmap_screen_fade_step = 1;
        actor->state = 1;
        break;
    /* 0x13: slot 2 request 4, slot 9 request 1. */
    case 0x13:
        worldmap_actor_request(2, 4);
        worldmap_actor_request(9, 1);
        actor->state = 1;
        break;
    /* 0x14: slot 2 request 6, slots 3 and 9 request 2. */
    case 0x14:
        worldmap_actor_request(2, 6);
        worldmap_actor_request(3, 2);
        worldmap_actor_request(9, 2);
        actor->state = 1;
        break;
    /* 0x16: fade out at rate 2, 4 per frame. */
    case 0x16:
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 2;
        worldmap_screen_fade_step = 4;
        actor->state = 1;
        break;
    /* 0x40: end the world-map loop with exit 0 (the state stays 1). */
    case 0x40:
        worldmap_loop_running = 0;
        worldmap_loop_result = 0;
        actor->state = 1;
        break;
    }
    return 1;
}

/* 8007C724: Start a scripted camera close behind the player. */
s32 worldmap_scene12_camera_shots_start(s32 index) {
    WorldmapActor *actor;

    worldmap_view_center_y = 0x78;
    worldmap_camera_distance = 0x400000;
    worldmap_view_kind = 1;
    actor = &worldmap_actor_slots[index];
    actor->unk7C = 0x1000;
    worldmap_camera_angle.vx = -0x40;
    worldmap_camera_angle.vy = 0;
    worldmap_camera_angle.vz = 0;
    worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = worldmap_player_position.vx;
    worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = worldmap_player_position.vy;
    worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = worldmap_player_position.vz;
    actor->unk58 = 0x40;
    actor->u.step = 0;
    return 1;
}

/* Scratchpad work area of the camera shot director. */
typedef struct {
    VECTOR point;     /* 0x00 */
    u8 pad10[0x90];
    SVECTOR spot;     /* 0xA0 */
} ShotScratch;

/* 8007C7D8: Camera shot director: move the camera along a shot path (u.step, speed
 * unk58), keep the saved camera target, and shake the view by unk7C. */
s32 worldmap_scene12_camera_shots_update(s32 index) {
    WorldmapActor *actor;
    ShotScratch *scratch;
    SVECTOR *points;
    s32 shake;

    actor = &worldmap_actor_slots[index];
    scratch = (ShotScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->state = 1;
        actor->unk5C = 0x8200;
        actor->unk4 = 0;
        actor->unk60 = 1;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 3;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 4;
        break;
    case 4:
        actor->state = 5;
        actor->unk5C = 0xA800;
        actor->unk4 = 0;
        actor->unk60 = 8;
        break;
    case 6:
        actor->state = 6;
        actor->unk4 = 0;
        actor->u.step = 0;
        actor->unk58 = 0x40;
        break;
    }
    if (worldmap_view_kind == 0) {
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    }
    switch (actor->state) {
    case 2:
        actor->unk58 -= actor->unk60;
        if (actor->unk58 < 0) {
            actor->unk58 = 0;
            actor->state = 0;
        }
    case 1:
        actor->u.step += actor->unk58;
        if (actor->u.step > actor->unk5C) {
            actor->state = 2;
        }
    case 0:
    follow:
        scratch->point.vy = 0;
        scratch->point.vx = 0;
        scratch->point.vz = worldmap_camera_follow_target.target.vz - worldmap_camera.target.vz;
        worldmap_wrap_offset(&scratch->point);
        GROUND_SCROLL[2] += scratch->point.vz;
        worldmap_camera.target.vz = worldmap_camera_follow_target.target.vz;
        points = &worldmap_scene12_shot_path1_points[actor->u.step >> 12];
        if (points[2].pad != -1) {
            worldmap_eval_quadratic_bspline(actor->u.step & 0xFFF, &points[0], &points[1], &points[2], &scratch->point);
            VIEW.at.vz = 0;
            VIEW.at.vx = 0;
            VIEW.up.vz = 0;
            VIEW.up.vx = 0;
            VIEW.eye.vx = scratch->point.vx >> 16;
            VIEW.up.vy = -0x1000;
            VIEW.eye.vz = -(scratch->point.vz >> 16);
            VIEW.eye.vy = scratch->point.vy >> 16;
            VIEW.at.vy = worldmap_camera.target.vy >> 12;
        }
        worldmap_camera_build_look_at(&worldmap_view_setup);
        worldmap_get_matrix_angles(&worldmap_camera_matrix, &worldmap_camera_angle);
        break;
    case 3:
        actor->unk7C = 0xF000;
        actor->state = 0;
        goto follow;
    case 4:
        scratch->spot.vy = 0;
        scratch->spot.vx = worldmap_camera.target.vx >> 12;
        scratch->spot.vz = worldmap_camera.target.vz >> 12;
        worldmap_effects_start_emitters(0x1A, &scratch->spot, NULL);
        worldmap_effects_start_emitters(0x1B, &scratch->spot, NULL);
        actor->unk7C -= 0x120;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
            actor->state = 0;
            worldmap_effects_stop_emitters(0x1A);
            worldmap_effects_stop_emitters(0x1B);
        }
        goto follow;
    case 5:
        actor->unk58 = 0x100;
        actor->state = 1;
        goto follow;
    case 6:
        scratch->point.vy = 0;
        scratch->point.vx = 0;
        scratch->point.vz = worldmap_camera_follow_target.target.vz - worldmap_camera.target.vz;
        worldmap_wrap_offset(&scratch->point);
        GROUND_SCROLL[2] += scratch->point.vz;
        worldmap_camera.target.vx = worldmap_camera_follow_target.target.vx;
        worldmap_camera.target.vy = worldmap_camera_follow_target.target.vy;
        worldmap_camera.target.vz = worldmap_camera_follow_target.target.vz;
        actor->u.step += actor->unk58;
        if (actor->u.step > 0x4800) {
            actor->unk58--;
            if (actor->unk58 < 0) {
                actor->unk58 = 0;
            }
        }
        points = &worldmap_scene12_shot_path2_points[actor->u.step >> 12];
        if (points[2].pad != -1) {
            worldmap_eval_quadratic_bspline(actor->u.step & 0xFFF, &points[0], &points[1], &points[2], &scratch->point);
            VIEW.at.vz = 0;
            VIEW.at.vx = 0;
            VIEW.up.vz = 0;
            VIEW.up.vx = 0;
            VIEW.eye.vx = scratch->point.vx >> 16;
            VIEW.up.vy = -0x1000;
            VIEW.eye.vz = -(scratch->point.vz >> 16);
            VIEW.eye.vy = scratch->point.vy >> 16;
            VIEW.at.vy = worldmap_camera.target.vy >> 12;
        }
        worldmap_camera_build_look_at(&worldmap_view_setup);
        worldmap_get_matrix_angles(&worldmap_camera_matrix, &worldmap_camera_angle);
        break;
    }
    shake = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    scratch->spot.vy = shake;
    VIEW.eye.vy += shake;
    VIEW.at.vy += scratch->spot.vy;
    return 1;
}

/* 8007CC6C: Link scene objects 0-3 to 4, hide 4 and reset its rotation; place the actor. */
s32 worldmap_scene12_drift_object4_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(4, 0);
    worldmap_objects_link(4, 2);
    worldmap_objects_link(4, 1);
    worldmap_objects_link(4, 3);
    worldmap_objects[4].visible = 0;
    worldmap_objects[4].angle.vz = 0;
    worldmap_objects[4].angle.vy = 0;
    worldmap_objects[4].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[4].angle, &worldmap_objects[4].matrix);
    actor = &worldmap_actor_slots[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xD00000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x400000;
    return 1;
}

/* 8007CD20: Drift scene object 4 with the actor along z; commands 1/2 start its
 * effects (state 1 also follows with the camera). */
s32 worldmap_scene12_drift_object4_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    ActorScratch *scratch;

    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[4];
    scratch = (ActorScratch *)0x1F800000;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->state = 1;
    } else if (actor->unk4 == 2) {
        actor->unk4 = 0;
        actor->state = 2;
    }
    actor->position.vz += actor->motion.vz;
    worldmap_wrap_position(&actor->position);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    scratch->position.vx = actor->position.vx >> 12;
    scratch->position.vy = actor->position.vy >> 12;
    scratch->position.vz = actor->position.vz >> 12;
    worldmap_effects_start_emitters(0x13, &scratch->position, NULL);
    switch (actor->state) {
    case 1:
        worldmap_effects_start_emitters(0x1F, &scratch->position, NULL);
    case 0:
        worldmap_camera_follow_target.target.vz = actor->position.vz;
        break;
    case 2:
        worldmap_effects_start_emitters(0x1F, &scratch->position, NULL);
        break;
    }
    return 1;
}

/* 8007CE84: Link scene object 6 to 5, hide 5 and reset its rotation; place the actor. */
s32 worldmap_scene12_drift_object5_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(5, 6);
    worldmap_objects[5].visible = 0;
    worldmap_objects[5].angle.vz = 0;
    worldmap_objects[5].angle.vy = 0;
    worldmap_objects[5].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[5].angle, &worldmap_objects[5].matrix);
    actor = &worldmap_actor_slots[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xB00000;
    actor->state = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x200000;
    return 1;
}

/* 8007CF18: Drift scene object 5 with the actor over the terrain along z; command 1
 * starts its trail effect. */
s32 worldmap_scene12_drift_object5_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    SVECTOR *vector;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->state = 1;
    }
    actor->position.vz += actor->motion.vz;
    worldmap_wrap_position(&actor->position);
    actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) - 0x4000;
    objects[5].position.vx = actor->position.vx >> 12;
    objects[5].position.vy = actor->position.vy >> 12;
    objects[5].position.vz = actor->position.vz >> 12;
    ACTOR_SCRATCH->position.vx = actor->position.vx >> 12;
    vector = SCRIPT_VECTOR;
    ACTOR_SCRATCH->position.vy = actor->position.vy >> 12;
    ACTOR_SCRATCH->position.vz = actor->position.vz >> 12;
    worldmap_effects_start_emitters(0x15, SCRIPT_VECTOR, NULL);
    if (actor->state == 1) {
        vector->vx = actor->position.vx >> 12;
        vector->vy = actor->position.vy >> 12;
        vector->vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0x1E, SCRIPT_VECTOR, NULL);
    }
    return 1;
}

/* 8007D078: Link scene objects 7 and 8 to 9, hide 9 and reset its rotation; place the actor. */
s32 worldmap_scene12_drift_object9_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(9, 7);
    worldmap_objects_link(9, 8);
    worldmap_objects[9].visible = 0;
    worldmap_objects[9].angle.vz = 0;
    worldmap_objects[9].angle.vy = 0;
    worldmap_objects[9].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[9].angle, &worldmap_objects[9].matrix);
    actor = &worldmap_actor_slots[index];
    actor->motion.vz = -0x4000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vx = 0xC00000;
    actor->position.vy = 0;
    actor->position.vz = 0;
    return 1;
}

/* 8007D110: Drift scene object 9 with the actor over the terrain; command 1 shows
 * objects 7-9 and ends the step. */
s32 worldmap_scene12_drift_object9_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        objects[9].visible = 1;
        objects[7].visible = 1;
        objects[8].visible = 1;
        worldmap_effects_stop_emitters(0x16);
        return 3;
    }
    actor->position.vz += actor->motion.vz;
    worldmap_wrap_position(&actor->position);
    actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) - 0x4000;
    objects[9].position.vx = actor->position.vx >> 12;
    objects[9].position.vy = actor->position.vy >> 12;
    objects[9].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    worldmap_effects_start_emitters(0x16, SCRIPT_VECTOR, NULL);
    return 1;
}

/* 8007D228: Link scene object 11 to 10, hide 10 and reset its rotation; place the actor. */
s32 worldmap_scene12_drift_object10_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(0xA, 0xB);
    worldmap_objects[10].visible = 0;
    worldmap_objects[10].angle.vz = 0;
    worldmap_objects[10].angle.vy = 0;
    worldmap_objects[10].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[10].angle, &worldmap_objects[10].matrix);
    actor = &worldmap_actor_slots[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xE00000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x100000;
    return 1;
}

/* 8007D2B8: Drift scene object 10 with the actor over the terrain; command 1 shows
 * objects 10-11, bursts and ends the step. */
s32 worldmap_scene12_drift_object10_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        objects[10].visible = 1;
        objects[11].visible = 1;
        worldmap_effects_stop_emitters(0x17);
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0x1C, SCRIPT_VECTOR, NULL);
        return 3;
    }
    actor->position.vz += actor->motion.vz;
    worldmap_wrap_position(&actor->position);
    actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) - 0x4000;
    objects[10].position.vx = actor->position.vx >> 12;
    objects[10].position.vy = actor->position.vy >> 12;
    objects[10].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    worldmap_effects_start_emitters(0x17, SCRIPT_VECTOR, NULL);
    return 1;
}

/* 8007D414: Link scene object 15 to 12, hide 12 and reset its rotation; place the actor. */
s32 worldmap_scene12_drift_object12_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(0xC, 0xF);
    worldmap_objects[12].visible = 0;
    worldmap_objects[12].angle.vz = 0;
    worldmap_objects[12].angle.vy = 0;
    worldmap_objects[12].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[12].angle, &worldmap_objects[12].matrix);
    actor = &worldmap_actor_slots[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xE80000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x280000;
    return 1;
}

/* 8007D4A4: Drift scene object 12 with the actor over the terrain; command 1 shows
 * objects 12 and 15, bursts and ends the step. */
s32 worldmap_scene12_drift_object12_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        objects[12].visible = 1;
        objects[15].visible = 1;
        worldmap_effects_stop_emitters(0x18);
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0x1D, SCRIPT_VECTOR, NULL);
        return 3;
    }
    actor->position.vz += actor->motion.vz;
    worldmap_wrap_position(&actor->position);
    actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) - 0x4000;
    objects[12].position.vx = actor->position.vx >> 12;
    objects[12].position.vy = actor->position.vy >> 12;
    objects[12].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    worldmap_effects_start_emitters(0x18, SCRIPT_VECTOR, NULL);
    return 1;
}

/* 8007D600: Link scene object 14 to 13, hide 13 and reset its rotation; place the actor. */
s32 worldmap_scene12_drift_object13_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(0xD, 0xE);
    worldmap_objects[13].visible = 0;
    worldmap_objects[13].angle.vz = 0;
    worldmap_objects[13].angle.vy = 0;
    worldmap_objects[13].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[13].angle, &worldmap_objects[13].matrix);
    actor = &worldmap_actor_slots[index];
    actor->motion.vz = -0x4000;
    actor->position.vx = 0xF80000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->position.vy = 0;
    actor->position.vz = 0x380000;
    return 1;
}

/* 8007D690: Drift scene object 13 with the actor over the terrain. */
s32 worldmap_scene12_drift_object13_update(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    actor->position.vz += actor->motion.vz;
    worldmap_wrap_position(&actor->position);
    actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) - 0x4000;
    objects[13].position.vx = actor->position.vx >> 12;
    objects[13].position.vy = actor->position.vy >> 12;
    objects[13].position.vz = actor->position.vz >> 12;
    SCRIPT_VECTOR->vx = actor->position.vx >> 12;
    SCRIPT_VECTOR->vy = actor->position.vy >> 12;
    SCRIPT_VECTOR->vz = actor->position.vz >> 12;
    worldmap_effects_start_emitters(0x19, SCRIPT_VECTOR, NULL);
    return 1;
}

/* 8007D774: Show scene object 16, reset its rotation and place the actor. */
s32 worldmap_scene12_drift_object16_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects[16].visible = 1;
    worldmap_objects[16].angle.vz = 0;
    worldmap_objects[16].angle.vy = 0;
    worldmap_objects[16].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[16].angle, &worldmap_objects[16].matrix);
    actor = &worldmap_actor_slots[index];
    actor->position.vx = 0xD00000;
    actor->position.vz = 0x400000;
    actor->position.vy = -0x280000;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->motion.vz = -0x6000;
    return 3;
}

/* 8007D7FC: Drift scene object 16 with the actor; command 1 hides it and moves
 * ahead of the camera, command 2 makes the camera follow. */
s32 worldmap_scene12_drift_object16_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = &worldmap_objects[16];
    actor = &worldmap_actor_slots[index];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        worldmap_objects[16].visible = 0;
        actor->position.vx = worldmap_camera.target.vx;
        actor->position.vz = worldmap_camera.target.vz + 0x400000;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 2;
        break;
    }
    actor->position.vz += actor->motion.vz;
    worldmap_wrap_position(&actor->position);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    if (actor->state == 2) {
        worldmap_camera_follow_target.target.vx = actor->position.vx;
        worldmap_camera_follow_target.target.vy = actor->position.vy;
        worldmap_camera_follow_target.target.vz = actor->position.vz;
    }
    return 1;
}

/* 8007D918: Set up the vehicle scene: load its area, place the player at the entry,
 * start music and its scripted actors. */
void worldmap_scene15_start(void) {
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
    worldmap_player_position.vx = worldmap_scene15_start_positions[worldmap_entry_index].vx << 12;
    worldmap_player_position.vy = worldmap_scene15_start_positions[worldmap_entry_index].vy << 12;
    worldmap_player_position.vz = worldmap_scene15_start_positions[worldmap_entry_index].vz << 12;
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
    worldmap_actor_spawn((s32)worldmap_scene15_director_start, (s32)worldmap_scene15_director_update);
    worldmap_actor_spawn((s32)worldmap_scene15_camera_start, (s32)worldmap_scene15_camera_update);
    worldmap_actor_spawn((s32)worldmap_scene15_flying_vehicle_start, (s32)worldmap_scene15_flying_vehicle_update);
    worldmap_actor_spawn((s32)worldmap_scene15_flame_start, (s32)worldmap_scene15_flame_update);
    worldmap_actor_spawn((s32)worldmap_scene15_flame_start, (s32)worldmap_scene15_flame_update);
    worldmap_actor_spawn((s32)worldmap_scene15_flame_start, (s32)worldmap_scene15_flame_update);
    worldmap_actor_spawn((s32)worldmap_scene15_flame_start, (s32)worldmap_scene15_flame_update);
    worldmap_actor_spawn((s32)worldmap_scene15_flame_start, (s32)worldmap_scene15_flame_update);
    worldmap_actor_spawn((s32)worldmap_scene15_grow_objects_9_10_start, (s32)worldmap_scene15_grow_objects_9_10_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_start, (s32)worldmap_scene_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 8007DCE0: Leave the world map: release its sound, subsystems and buffers, and
 * request scene 0x1A1 with the exit's flag word. */
void worldmap_scene15_leave(void) {
    sound_set_seq_fade((SoundSeq *)mode_music_seq, 0, 0xF0);
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
    game_data.map = 0x1A1;
    game_data.entry[2] = worldmap_scene15_exit_entry_parameters[worldmap_entry_index];
    game_data.entry[0] = worldmap_camera_angle.vy;
    worldmap_next_scene_chosen = 1;
}

/* 8007DE14: Start the selected timed sequence on an actor. */
s32 worldmap_scene15_director_start(s32 index) {
    s32 unused[2]; /* unreferenced local: the original frame reserves it */
    WorldmapActor *actor;
    s32 sequence;

    sequence = worldmap_entry_index;
    actor = &worldmap_actor_slots[index];
    actor->unk54 = (s32)worldmap_scene15_cue_sequences[sequence].states;
    actor->unk58 = (s32)worldmap_scene15_cue_sequences[sequence].durations;
    actor->u.step = 0;
    actor->state = *(s16 *)actor->unk54;
    actor->wait = ((u16 *)actor->unk58)[actor->u.step];
    actor->u.step++;
    return 1;
}
