/* World map unit 8007A9F8-8007C3B8 (rodata 8006FBC4-8006FC50, data
 * 8009A450-8009A4D8): the director of scene mode 14 and its actors (camera
 * shake, growing objects, exhaust trail, the rig's flight), the set-up and
 * leave handlers of mode 12 and the sequence start of its director.
 *
 * worldmap_scene10_landing_rig_update's five-entry table ends at 8006fbc4 and
 * worldmap_scene14_director_update's follows at once, 4 mod 8, a phase change
 * without a pad word: this unit's rodata starts there and its text after
 * worldmap_scene10_landing_rig_update, at or before
 * worldmap_scene14_director_update. Its data opens with the cue sequence that
 * worldmap_scene14_director_start, left in the preceding unit by the split, starts. */
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

/* The director's cue sequence, user-supplied script data (an asset in
 * worldmap.classification.txt): 14 u16 states and 14 u16 waits (started by
 * worldmap_scene14_director_start; tools/analysis/overlay_scripts.py decodes it). */
INCLUDE_ASSET(".data", worldmap_scene14_cue_states, 0x8009A450, 0x1C);
INCLUDE_ASSET(".data", worldmap_scene14_cue_waits, 0x8009A46C, 0x1C);

/* Exhaust effect angle. */
SVECTOR worldmap_scene14_exhaust_angle = {0, 128, 0}; /* 8009A488 */

/* The rig's flight path control points. */
SVECTOR worldmap_scene14_rig_path_points[9] = { /* 8009A490 */
    {22989, -240, 25600}, {23421, -200, 25192}, {23749, -192, 24992}, {24445, -168, 24816},
    {24709, -232, 25456}, {24245, -360, 26024}, {23797, -160, 25648}, {23389, -48, 25192},
    {23251, -248, 24648},
};

/* 8007A9F8: Scene director (mode 14), a cue sequencer: state 1 counts the wait down
 * and, once it drops below 0, loads the next entry's state and wait from
 * worldmap_scene14_cue_states/worldmap_scene14_cue_waits; each other state runs its
 * cue on the next update and returns to 1. Requests (worldmap_actor_request) go to
 * the setup's actor slots (worldmap_scene14_start): 0 the screen fade, 2 the camera
 * (worldmap_scene14_camera_update), 3 and 4 the growing objects
 * (worldmap_scene14_grow_objects_4_5_update,
 * worldmap_scene14_grow_objects_6_7_update), 5 the exhaust trail, 6 the rig's
 * flight. tools/analysis/overlay_scripts.py decodes the sequence. */
s32 worldmap_scene14_director_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    switch (actor->state) {
    /* 0: idle. */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = worldmap_scene14_cue_states[actor->u.step];
            actor->wait = worldmap_scene14_cue_waits[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: start emitter group 0x11; area sound 1. */
    case 2:
        worldmap_effects_start_emitters(0x11, NULL, NULL);
        actor->state = 1;
        sound_play_effect((sound_effect_bank->id << 16) | 1);
        break;
    /* 3: slot 2 request 2; emitter groups 0xF and 0x10; area sounds 4-6. */
    case 3:
        worldmap_actor_request(2, 2);
        actor->state = 1;
        worldmap_effects_start_emitters(0xF, NULL, NULL);
        worldmap_effects_start_emitters(0x10, NULL, NULL);
        sound_play_effect((sound_effect_bank->id << 16) | 4);
        sound_play_effect((sound_effect_bank->id << 16) | 5);
        sound_play_effect((sound_effect_bank->id << 16) | 6);
        break;
    /* 4: slot 2 request 3, slot 3 request 1. */
    case 4:
        worldmap_actor_request(2, 3);
        worldmap_actor_request(3, 1);
        actor->state = 1;
        break;
    /* 5: slot 2 request 6, slots 4 and 5 request 1; area sounds 7-9. */
    case 5:
        worldmap_actor_request(2, 6);
        worldmap_actor_request(4, 1);
        worldmap_actor_request(5, 1);
        actor->state = 1;
        sound_play_effect((sound_effect_bank->id << 16) | 7);
        sound_play_effect((sound_effect_bank->id << 16) | 8);
        sound_play_effect((sound_effect_bank->id << 16) | 9);
        break;
    /* 6: slot 2 request 4; area sound 0xA, and 0xB and 0xC on voices 12-13. */
    case 6:
        worldmap_actor_request(2, 4);
        actor->state = 1;
        sound_play_effect((sound_effect_bank->id << 16) | 0xA);
        sound_play_effect_on_channels_12_13((sound_effect_bank->id << 16) | 0xB);
        sound_play_effect_on_channels_12_13((sound_effect_bank->id << 16) | 0xC);
        break;
    /* 7: slot 2 request 5. */
    case 7:
        worldmap_actor_request(2, 5);
        actor->state = 1;
        break;
    /* 8: slot 2 request 1. */
    case 8:
        worldmap_actor_request(2, 1);
        actor->state = 1;
        break;
    /* 9: slot 6 request 1, slot 2 request 7. */
    case 9:
        worldmap_actor_request(6, 1);
        worldmap_actor_request(2, 7);
        actor->state = 1;
        break;
    /* 10: fade out (slot 0 request 13) at 4 per frame. */
    case 10:
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_step = 4;
        actor->state = 1;
        break;
    /* 11: end the world-map loop with exit 0. */
    case 11:
        worldmap_loop_running = 0;
        worldmap_loop_result = 0;
        actor->state = 1;
        break;
    }
    return 1;
}

/* 8007AD34: Start a scripted camera looking at the player from yaw 0x480. */
s32 worldmap_scene14_camera_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    worldmap_view_center_y = 0x78;
    worldmap_camera_distance = 0x1C0000;
    worldmap_camera_angle.vx = 0x40;
    worldmap_camera_angle.vy = 0x480;
    worldmap_camera_angle.vz = 0;
    worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = worldmap_player_position.vx;
    worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = worldmap_player_position.vy;
    worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = worldmap_player_position.vz;
    actor->u.step = 0x1000;
    return 1;
}

/* 8007ADD4: Scene camera shake: commands pick a shot or a shake ramp (u.step is the
 * shake amplitude, 20.12); every frame jitter both view vectors by it. */
s32 worldmap_scene14_camera_update(s32 index) {
    WorldmapActor *actor;
    CameraScratch *scratch;
    s32 originZ;

    actor = &worldmap_actor_slots[index];
    scratch = (CameraScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 2:
        actor->state = 2;
        actor->unk4 = 0;
        actor->u.step = 0x40000;
        break;
    case 3:
        worldmap_camera_distance = 0x640000;
        actor->unk4 = 0;
        actor->state = 0;
        worldmap_camera_angle.vx = -0x1E0;
        worldmap_camera_angle.vy = 0x480;
        worldmap_camera_angle.vz = 0;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 0;
        worldmap_camera_distance = 0x1E0000;
        worldmap_camera_angle.vx = 0x40;
        worldmap_camera_angle.vy = 0x418;
        worldmap_camera_angle.vz = 0;
        worldmap_view_kind = 0;
        worldmap_camera.target.vz = worldmap_player_position.vz;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 3;
        break;
    case 6:
        actor->unk4 = 0;
        actor->state = 4;
        actor->unk58 = worldmap_player_position.vx + 0xC7C00;
        worldmap_view_kind = 1;
        actor->unk5C = worldmap_player_position.vz + 0x3EC400;
        break;
    case 7:
        actor->unk4 = 0;
        actor->state = 5;
        break;
    }
    switch (actor->state) {
    case 0:
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
        break;
    case 1:
        actor->u.step += 0x200;
        if (actor->u.step > 0x8000) {
            actor->u.step = 0x8000;
            actor->state = 0;
        }
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
        break;
    case 2:
        actor->u.step -= 0x200;
        if (actor->u.step < 0x8000) {
            actor->u.step = 0x8000;
            actor->state = 0;
        }
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
        break;
    case 3:
        actor->u.step -= 0x100;
        if (actor->u.step < 0x1000) {
            actor->u.step = 0x1000;
            actor->state = 0;
        }
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
        break;
    case 4:
        worldmap_camera.target.vz += 0x3A000;
        if (worldmap_camera.target.vz > 0x77FFFFF) {
            actor->state = 0;
        }
        VIEW.eye.vx = (actor->unk58 - worldmap_camera.target.vx) >> 12;
        originZ = actor->unk5C;
        VIEW.at.vx = VIEW.at.vz = 0;
        VIEW.at.vy = worldmap_camera.target.vy >> 12;
        VIEW.eye.vy = (worldmap_camera.target.vy >> 12) - 0x40;
        VIEW.eye.vz = (worldmap_camera.target.vz - originZ) >> 12;
        worldmap_camera_build_look_at(&worldmap_view_setup);
        worldmap_get_matrix_angles(&worldmap_camera_matrix, &worldmap_camera_angle);
        break;
    case 5:
        worldmap_camera_angle.vy += 4;
        if (worldmap_camera_angle.vy > 0x600) {
            actor->state = 0;
        }
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
        break;
    }
    scratch->view.vx = rand() % (actor->u.step >> 12) - (actor->u.step >> 13);
    scratch->view.vy = rand() % (actor->u.step >> 12) - (actor->u.step >> 13);
    VIEW.eye.vx += scratch->view.vx;
    VIEW.at.vx += scratch->view.vx;
    VIEW.eye.vy += scratch->view.vy;
    VIEW.at.vy += scratch->view.vy;
    return 1;
}

/* 8007B200: Rebuild scene objects 4-5 and show objects 6-7 at zero scale. */
s32 worldmap_scene14_grow_objects_4_5_start(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    ScaleScratch *scratch;
    s32 i;

    scratch = SCALE_SCRATCH;
    i = 0;
    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[4];
    do {
        worldmap_build_translucent_quads(object, object->prims, object->def->primitive_count);
        object++;
        i++;
    } while (i < 2);
    actor->unk54 = 0;
    actor->u.step = 0;
    scratch->matrix[0] = worldmap_identity_matrix;
    scratch->scale[0].vx = scratch->scale[0].vz = actor->u.step;
    scratch->scale[0].vy = 0x1000;
    ScaleMatrix(&scratch->matrix[0], &scratch->scale[0]);
    object[0].matrix = scratch->matrix[0];
    object[1].matrix = scratch->matrix[0];
    object[1].visible = 1;
    object[0].visible = 1;
    return 3;
}

/* 8007B394: Grow scene objects 4 and 5 at the player: widen their scale each frame up to 0x7F00. */
s32 worldmap_scene14_grow_objects_4_5_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    s32 x;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    if (actor->unk4 != 0) {
        actor->unk4 = 0;
        objects[5].visible = 0;
        objects[4].visible = 0;
    }
    x = worldmap_player_position.vx >> 12;
    objects[4].position.vy = objects[5].position.vy = -0x40;
    objects[4].position.vx = objects[5].position.vx = x;
    objects[4].position.vz = objects[5].position.vz = worldmap_player_position.vz >> 12;
    if ((actor->u.step += 0x180) > 0x800) {
        actor->unk54 += 0x180;
    }
    if (actor->u.step > 0x7F00) {
        actor->u.step = 0x7F00;
    }
    if (actor->unk54 > 0x7F00) {
        actor->unk54 = 0x7F00;
    }
    SCALE_SCRATCH->matrix[0] = worldmap_identity_matrix;
    SCALE_SCRATCH->matrix[1] = SCALE_SCRATCH->matrix[0];
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->u.step;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk54;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&SCALE_SCRATCH->matrix[0], &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&SCALE_SCRATCH->matrix[1], &SCALE_SCRATCH->scale[1]);
    objects[4].matrix = SCALE_SCRATCH->matrix[0];
    objects[5].matrix = SCALE_SCRATCH->matrix[1];
    return 1;
}

/* 8007B604: Rebuild scene objects 6-7 and show objects 8-9 at zero scale. */
s32 worldmap_scene14_grow_objects_6_7_start(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    ScaleScratch *scratch;
    s32 i;

    scratch = SCALE_SCRATCH;
    i = 0;
    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[6];
    do {
        worldmap_build_translucent_quads(object, object->prims, object->def->primitive_count);
        object++;
        i++;
    } while (i < 2);
    actor->unk54 = 0;
    actor->u.step = 0;
    scratch->matrix[0] = worldmap_identity_matrix;
    scratch->scale[0].vx = scratch->scale[0].vz = actor->u.step;
    scratch->scale[0].vy = 0x1000;
    ScaleMatrix(&scratch->matrix[0], &scratch->scale[0]);
    object[0].matrix = scratch->matrix[0];
    object[1].matrix = scratch->matrix[0];
    object[1].visible = 1;
    object[0].visible = 1;
    return 3;
}

/* 8007B798: Grow scene objects 6 and 7 at the player (see
 * worldmap_scene14_grow_objects_4_5_update). */
s32 worldmap_scene14_grow_objects_6_7_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    s32 x;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    if (actor->unk4 != 0) {
        actor->unk4 = 0;
        objects[7].visible = 0;
        objects[6].visible = 0;
    }
    x = worldmap_player_position.vx >> 12;
    objects[6].position.vy = objects[7].position.vy = -0x40;
    objects[6].position.vx = objects[7].position.vx = x;
    objects[6].position.vz = objects[7].position.vz = worldmap_player_position.vz >> 12;
    if ((actor->u.step += 0x180) > 0x800) {
        actor->unk54 += 0x180;
    }
    if (actor->u.step > 0x7F00) {
        actor->u.step = 0x7F00;
    }
    if (actor->unk54 > 0x7F00) {
        actor->unk54 = 0x7F00;
    }
    SCALE_SCRATCH->matrix[0] = worldmap_identity_matrix;
    SCALE_SCRATCH->matrix[1] = SCALE_SCRATCH->matrix[0];
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = actor->u.step;
    SCALE_SCRATCH->scale[1].vx = SCALE_SCRATCH->scale[1].vz = actor->unk54;
    SCALE_SCRATCH->scale[0].vy = SCALE_SCRATCH->scale[1].vy = 0x1000;
    ScaleMatrix(&SCALE_SCRATCH->matrix[0], &SCALE_SCRATCH->scale[0]);
    ScaleMatrix(&SCALE_SCRATCH->matrix[1], &SCALE_SCRATCH->scale[1]);
    objects[6].matrix = SCALE_SCRATCH->matrix[0];
    objects[7].matrix = SCALE_SCRATCH->matrix[1];
    return 1;
}

/* 8007BA08: Scene step with nothing to do. */
s32 worldmap_scene14_exhaust_trail_start(void) {
    return 3;
}

/* 8007BA10: Exhaust trail: move the emitter along its path for 60 frames, then
 * reset to the player and end the step. */
s32 worldmap_scene14_exhaust_trail_update(s32 index) {
    WorldmapActor *actor;
    s32 result;

    result = 1;
    actor = &worldmap_actor_slots[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->wait = 0x3C;
        actor->position.vx = worldmap_player_position.vx;
        actor->position.vy = worldmap_player_position.vy;
        actor->position.vz = worldmap_player_position.vz;
    }
    if (--actor->wait > 0) {
        actor->position.vx -= 0x95D0;
        actor->position.vz += 0x2F130;
        SCRIPT_VECTOR->vx = actor->position.vx >> 12;
        SCRIPT_VECTOR->vy = actor->position.vy >> 12;
        SCRIPT_VECTOR->vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(9, SCRIPT_VECTOR, &worldmap_scene14_exhaust_angle);
    } else {
        actor->wait = 0x3C;
        actor->position = worldmap_player_position;
        worldmap_effects_stop_emitters(9);
        result = 3;
    }
    return result;
}

/* 8007BB60: Link scene objects 1-3 to object 0 and reset its rotation. */
s32 worldmap_scene14_rig_flight_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(0, 1);
    worldmap_objects_link(0, 2);
    worldmap_objects_link(0, 3);
    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->unk54 = 0x100;
    actor->unk58 = 0x100;
    worldmap_objects[0].angle.vx = 0;
    worldmap_objects[0].angle.vy = 0;
    worldmap_objects[0].angle.vz = 0;
    RotMatrixYXZ(&worldmap_objects[0].angle, &worldmap_objects[0].matrix);
    return 3;
}

/* Scratchpad work area of the rig path follower. */
typedef struct {
    VECTOR axis[4];   /* 0x00 */
    u8 pad40[0x60];
    SVECTOR angle;    /* 0xA0 */
    SVECTOR heading;  /* 0xA8 */
    u8 padB0[0x40];
    MATRIX frame;     /* 0xF0 */
} FollowScratch;

/* 8007BBEC: Fly scene object 0 along the rig path (speeding up, braking, then rolling
 * out); orient it to the path and emit exhaust while low. Done (3) at the end
 * of the path. */
s32 worldmap_scene14_rig_flight_update(s32 index) {
    s32 result;
    WorldmapActor *actor;
    FollowScratch *scratch;
    SVECTOR *points;
    SceneObject *object;

    result = 1;
    actor = &worldmap_actor_slots[index];
    scratch = (FollowScratch *)0x1F800000;
    switch (actor->u.step >> 12) {
    case 0:
    case 1:
        actor->u.step += actor->unk58;
        actor->unk54 += 4;
        break;
    case 2:
    case 3:
    case 4:
        actor->u.step += actor->unk58;
        actor->unk58 -= 8;
        if (actor->unk58 < 0x80) {
            actor->unk58 = 0x80;
        }
        actor->unk54 += 4;
        break;
    case 5:
    case 6:
    case 7:
        actor->u.step += actor->unk58;
        actor->unk58 += 8;
        if (actor->unk58 > 0x100) {
            actor->unk58 = 0x100;
        }
        actor->unk54 -= 0x10;
        break;
    case 8:
        result = 3;
        break;
    }
    points = &worldmap_scene14_rig_path_points[actor->u.step >> 12];
    if (points[2].pad != -1) {
        worldmap_eval_quadratic_bspline(actor->u.step & 0xFFF, &points[0], &points[1], &points[2], &scratch->axis[0]);
    }
    points = &worldmap_scene14_rig_path_points[(actor->u.step + 0x80) >> 12];
    worldmap_eval_quadratic_bspline((actor->u.step + 0x80) & 0xFFF, &points[0], &points[1], &points[2], &scratch->axis[1]);
    object = worldmap_objects;
    object->position.vx = scratch->axis[0].vx >> 16;
    object->position.vy = scratch->axis[0].vy >> 16;
    object->position.vz = scratch->axis[0].vz >> 16;
    scratch->axis[1].vx = (scratch->axis[1].vx - scratch->axis[0].vx) >> 12;
    scratch->axis[1].vy = (scratch->axis[1].vy - scratch->axis[0].vy) >> 12;
    scratch->axis[1].vz = -((scratch->axis[1].vz - scratch->axis[0].vz) >> 12);
    VectorNormal(&scratch->axis[1], &scratch->axis[0]);
    scratch->angle.vx = 0;
    scratch->angle.vy = ratan2(scratch->axis[0].vx, scratch->axis[0].vz) & 0xFFF;
    scratch->angle.vz = actor->unk54;
    RotMatrixYXZ(&scratch->angle, &scratch->frame);
    scratch->angle.vx = 0;
    scratch->angle.vy = -0x1000;
    scratch->angle.vz = 0;
    ApplyMatrix(&scratch->frame, &scratch->angle, &scratch->axis[1]);
    OuterProduct12(&scratch->axis[0], &scratch->axis[1], &scratch->axis[3]);
    VectorNormal(&scratch->axis[3], &scratch->axis[2]);
    OuterProduct12(&scratch->axis[0], &scratch->axis[2], &scratch->axis[3]);
    VectorNormal(&scratch->axis[3], &scratch->axis[1]);
    scratch->frame.m[0][0] = scratch->axis[2].vx;
    scratch->frame.m[0][1] = scratch->axis[2].vy;
    scratch->frame.m[0][2] = scratch->axis[2].vz;
    scratch->frame.m[1][0] = scratch->axis[1].vx;
    scratch->frame.m[1][1] = scratch->axis[1].vy;
    scratch->frame.m[1][2] = scratch->axis[1].vz;
    scratch->frame.m[2][0] = scratch->axis[0].vx;
    scratch->frame.m[2][1] = scratch->axis[0].vy;
    scratch->frame.m[2][2] = scratch->axis[0].vz;
    libgte_transpose_matrix(&scratch->frame, &object->matrix);
    worldmap_get_matrix_angles(&scratch->frame, &scratch->heading);
    if (object->position.vy >= -0x7F) {
        scratch->angle.vx = object->position.vx;
        scratch->angle.vy = object->position.vy;
        scratch->angle.vz = object->position.vz;
        scratch->heading.vz = -scratch->heading.vz;
        worldmap_effects_start_emitters(0x12, &scratch->angle, &scratch->heading);
    }
    return result;
}

/* 8007BF50: Set up the second vehicle scene: fixed start position, its director and
 * object actors. */
void worldmap_scene12_start(void) {
    RECT rect;

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
    worldmap_read_area_sound_bank();
    worldmap_player_position.vx = 0xD00000;
    worldmap_player_position.vy = -0xA0000;
    worldmap_player_position.vz = 0x400000;
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
    worldmap_actor_spawn((s32)worldmap_scene12_director_start, (s32)worldmap_scene12_director_update);
    worldmap_actor_spawn((s32)worldmap_scene12_camera_shots_start, (s32)worldmap_scene12_camera_shots_update);
    worldmap_actor_spawn((s32)worldmap_scene12_drift_object4_start, (s32)worldmap_scene12_drift_object4_update);
    worldmap_actor_spawn((s32)worldmap_scene12_drift_object5_start, (s32)worldmap_scene12_drift_object5_update);
    worldmap_actor_spawn((s32)worldmap_scene12_drift_object9_start, (s32)worldmap_scene12_drift_object9_update);
    worldmap_actor_spawn((s32)worldmap_scene12_drift_object10_start, (s32)worldmap_scene12_drift_object10_update);
    worldmap_actor_spawn((s32)worldmap_scene12_drift_object12_start, (s32)worldmap_scene12_drift_object12_update);
    worldmap_actor_spawn((s32)worldmap_scene12_drift_object13_start, (s32)worldmap_scene12_drift_object13_update);
    worldmap_actor_spawn((s32)worldmap_scene12_drift_object16_start, (s32)worldmap_scene12_drift_object16_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_start, (s32)worldmap_scene_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 8007C260: Leave the world map for scene 0x111 (flag word 2), releasing its sound,
 * subsystems and buffers. */
void worldmap_scene12_leave(void) {
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
    game_data.map = 0x111;
    game_data.entry[2] = 2;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 8007C36C: Start an actor's timed sequence: first state and its duration. */
s32 worldmap_scene12_director_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->state = worldmap_scene12_cue_states[0];
    actor->wait = worldmap_scene12_cue_waits[actor->u.step];
    actor->u.step++;
    return 1;
}
