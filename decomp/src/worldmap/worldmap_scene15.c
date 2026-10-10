/* World map unit 8007DE98-80080370 (rodata 8006FD8C-800701E0, data
 * 8009A5A0-8009A698): the director of scene mode 15 and its actors (the
 * camera director, the flying vehicle, the falling objects, the exhaust
 * flames and the growing objects), the set-up and leave handlers of mode 13
 * and the sequence start of its director.
 *
 * worldmap_scene12_camera_shots_update's seven-entry table ends at 8006fd8c and worldmap_scene15_director_update's
 * follows at once, 4 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after worldmap_scene12_camera_shots_update, at or before
 * worldmap_scene15_director_update. Its data opens with the tables of mode 15's set-up and leave
 * handlers and sequence start (worldmap_scene15_start, worldmap_scene15_leave, worldmap_scene15_director_start),
 * which the text split leaves in the preceding unit. */
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

/* Data of the scene this director runs; its mode handlers worldmap_scene15_start and
 * worldmap_scene15_leave and sequence start worldmap_scene15_director_start precede this unit. Per
 * entry (worldmap_entry_index): the three ambient sounds, the player's start position
 * and the resident flag word set on leaving. */
u16 worldmap_scene15_ambient_sounds[3][3] = {{0x25, 0x26, 0x27}, {0x1F, 0x20, 0x21}, {0x22, 0x23, 0x24}}; /* 8009A5A0 */
SVECTOR worldmap_scene15_start_positions[3] = {{14976, -640, 11690}, {15412, -640, 11957}, {14976, -640, 11690}}; /* 8009A5B4 */
u16 worldmap_scene15_exit_entry_parameters[3] = {2, 3, 4}; /* 8009A5CC */

/* The director's cue sequences per entry, user-supplied script data (an
 * asset in worldmap.classification.txt): s16 states and u16 waits, 14, 9
 * and 9 entries (tools/analysis/overlay_scripts.py decodes them). */
extern s16 worldmap_scene15_entry0_cue_states[], worldmap_scene15_entry1_cue_states[], worldmap_scene15_entry2_cue_states[];
extern u16 worldmap_scene15_entry0_cue_waits[], worldmap_scene15_entry1_cue_waits[], worldmap_scene15_entry2_cue_waits[];
INCLUDE_ASSET(".data", worldmap_scene15_entry0_cue_states, 0x8009A5D4, 0x1C);
INCLUDE_ASSET(".data", worldmap_scene15_entry0_cue_waits, 0x8009A5F0, 0x1C);
INCLUDE_ASSET(".data", worldmap_scene15_entry1_cue_states, 0x8009A60C, 0x12);
INCLUDE_ASSET(".data", worldmap_scene15_entry1_cue_waits, 0x8009A620, 0x12);
INCLUDE_ASSET(".data", worldmap_scene15_entry2_cue_states, 0x8009A634, 0x12);
INCLUDE_ASSET(".data", worldmap_scene15_entry2_cue_waits, 0x8009A648, 0x12);
Sequence worldmap_scene15_cue_sequences[3] = { /* 8009A65C */
    {worldmap_scene15_entry0_cue_states, worldmap_scene15_entry0_cue_waits}, {worldmap_scene15_entry1_cue_states, worldmap_scene15_entry1_cue_waits}, {worldmap_scene15_entry2_cue_states, worldmap_scene15_entry2_cue_waits},
};

/* Per entry: the flight path start. */
SVECTOR worldmap_scene15_vehicle_stop_points[3] = {{14307, 0, 12781}, {14743, 0, 13048}, {14307, 0, 12781}}; /* 8009A674 */

/* Exhaust flame sizes, per flame (actors 4-8): 0x800, 0x700, 0x600, 0x500
 * and 0x300. A stray halfword (65 79, "ey") that nothing reads follows them
 * at the end of the unit's data, so the table stays original data
 * (worldmap.classification.txt). */
INCLUDE_ORIGINAL(".data", worldmap_scene15_flame_sizes, 0x8009A68C, 12);
extern u16 worldmap_scene15_flame_sizes[5];

/* 8007DE98: Scene director (mode 15): worldmap_scene14_director_update's cue sequencer on the sequence
 * worldmap_scene15_director_start picks from worldmap_scene15_cue_sequences by worldmap_entry_index (states at unk54,
 * durations at unk58). Actor slots (worldmap_scene15_start): 0 the screen fade, 2 the
 * camera (worldmap_scene15_camera_update), 3 the flying vehicle (worldmap_scene15_flying_vehicle_update), 4-8 its
 * exhaust flames (worldmap_scene15_flame_update), 9 the growing objects 9 and 10
 * (worldmap_scene15_grow_objects_9_10_update). A fade with rate 1 adds the fade quad (white), with
 * rate 2 subtracts it (black). */
s32 worldmap_scene15_director_update(s32 index) {
    WorldmapActor *actor;
    s32 unused[4]; /* unreferenced; the original frame reserves it */

    actor = &worldmap_actor_slots[index];
    switch (actor->state) {
    /* 0: idle. */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = ((u16 *)actor->unk54)[actor->u.step];
            actor->wait = ((u16 *)actor->unk58)[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: area sounds 0x1C-0x1E; slots 2 and 3 request 2, the flames 3. */
    case 2:
        sound_play_effect((sound_effect_bank->id << 16) | 0x1C);
        sound_play_effect((sound_effect_bank->id << 16) | 0x1D);
        sound_play_effect((sound_effect_bank->id << 16) | 0x1E);
        worldmap_actor_request(2, 2);
        worldmap_actor_request(3, 2);
        worldmap_actor_request(4, 3);
        worldmap_actor_request(5, 3);
        worldmap_actor_request(6, 3);
        worldmap_actor_request(7, 3);
        worldmap_actor_request(8, 3);
        actor->state = 1;
        break;
    /* 3: stop the effects of emitter groups 0x22-0x24; slots 2 and 3
     * request 3, the flames 3. */
    case 3:
        worldmap_effects_stop_particles(0x22);
        worldmap_effects_stop_particles(0x23);
        worldmap_effects_stop_particles(0x24);
        worldmap_actor_request(2, 3);
        worldmap_actor_request(3, 3);
        worldmap_actor_request(4, 3);
        worldmap_actor_request(5, 3);
        worldmap_actor_request(6, 3);
        worldmap_actor_request(7, 3);
        worldmap_actor_request(8, 3);
        actor->state = 1;
        break;
    /* 4: slots 2 and 3 request 4, the flames 3. */
    case 4:
        worldmap_actor_request(2, 4);
        worldmap_actor_request(3, 4);
        worldmap_actor_request(4, 3);
        worldmap_actor_request(5, 3);
        worldmap_actor_request(6, 3);
        worldmap_actor_request(7, 3);
        worldmap_actor_request(8, 3);
        actor->state = 1;
        break;
    /* 5: slot 2 request 5. */
    case 5:
        worldmap_actor_request(2, 5);
        actor->state = 1;
        break;
    /* 6: fade out at rate 1, 0x40 per frame. */
    case 6:
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 1;
        worldmap_screen_fade_step = 0x40;
        actor->state = 1;
        break;
    /* 7: slot 3 request 5, the flames 4; fade in at rate 1, 0x40 per frame. */
    case 7:
        worldmap_actor_request(3, 5);
        worldmap_actor_request(4, 4);
        worldmap_actor_request(5, 4);
        worldmap_actor_request(6, 4);
        worldmap_actor_request(7, 4);
        worldmap_actor_request(8, 4);
        worldmap_actor_request(0, 0xC);
        worldmap_screen_fade_rate = 1;
        worldmap_screen_fade_step = 0x40;
        actor->state = 1;
        break;
    /* 8: slot 2 request 6. */
    case 8:
        worldmap_actor_request(2, 6);
        actor->state = 1;
        break;
    /* 9: fade out at rate 1, 0x80 per frame. */
    case 9:
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 1;
        worldmap_screen_fade_step = 0x80;
        actor->state = 1;
        break;
    /* 10: fade in at rate 1, 0x80 per frame; slot 9 request 1, slot 2
     * request 7. */
    case 10:
        worldmap_actor_request(0, 0xC);
        worldmap_screen_fade_rate = 1;
        worldmap_screen_fade_step = 0x80;
        worldmap_actor_request(9, 1);
        worldmap_actor_request(2, 7);
        actor->state = 1;
        break;
    /* 16: slot 2 request 4, slot 3 request 0x10, the flames 3. */
    case 16:
        worldmap_actor_request(2, 4);
        worldmap_actor_request(3, 0x10);
        worldmap_actor_request(4, 3);
        worldmap_actor_request(5, 3);
        worldmap_actor_request(6, 3);
        worldmap_actor_request(7, 3);
        worldmap_actor_request(8, 3);
        actor->state = 1;
        break;
    /* 17: slot 2 request 0x10. */
    case 17:
        worldmap_actor_request(2, 0x10);
        actor->state = 1;
        break;
    /* 18: slot 2 request 0x11 (no sequence holds it). */
    case 18:
        worldmap_actor_request(2, 0x11);
        actor->state = 1;
        break;
    /* 24: slot 2 request 4, slot 3 request 0x18, the flames 3. */
    case 24:
        worldmap_actor_request(2, 4);
        worldmap_actor_request(3, 0x18);
        worldmap_actor_request(4, 3);
        worldmap_actor_request(5, 3);
        worldmap_actor_request(6, 3);
        worldmap_actor_request(7, 3);
        worldmap_actor_request(8, 3);
        actor->state = 1;
        break;
    /* 25: slot 2 request 0x18. */
    case 25:
        worldmap_actor_request(2, 0x18);
        actor->state = 1;
        break;
    /* 61: the three ambient sounds of worldmap_scene15_ambient_sounds[worldmap_entry_index]. */
    case 61:
        sound_play_effect((sound_effect_bank->id << 16) | worldmap_scene15_ambient_sounds[worldmap_entry_index][0]);
        sound_play_effect((sound_effect_bank->id << 16) | worldmap_scene15_ambient_sounds[worldmap_entry_index][1]);
        sound_play_effect((sound_effect_bank->id << 16) | worldmap_scene15_ambient_sounds[worldmap_entry_index][2]);
        actor->state = 1;
        break;
    /* 62: area sounds 0x19-0x1B. */
    case 62:
        sound_play_effect((sound_effect_bank->id << 16) | 0x19);
        sound_play_effect((sound_effect_bank->id << 16) | 0x1A);
        sound_play_effect((sound_effect_bank->id << 16) | 0x1B);
        actor->state = 1;
        break;
    /* 63: fade out at rate 2, 4 per frame. */
    case 63:
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 2;
        worldmap_screen_fade_step = 4;
        actor->state = 1;
        break;
    /* 64: end the world-map loop with exit 0; idle. */
    case 64:
        worldmap_loop_running = 0;
        worldmap_loop_result = 0;
        actor->state = 0;
        break;
    }
    return 1;
}

/* 8007E450: Start a scripted camera at the player position (step 0, speed 0x40). */
s32 worldmap_scene15_camera_start(s32 index) {
    WorldmapActor *actor;

    worldmap_view_kind = 0;
    actor = &worldmap_actor_slots[index];
    actor->unk7C = 0x1000;
    worldmap_view_center_y = 0x78;
    worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = worldmap_player_position.vx;
    worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = worldmap_player_position.vy;
    worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = worldmap_player_position.vz;
    actor->unk4 = 1;
    actor->unk58 = 0x40;
    actor->state = 0;
    actor->u.step = 0;
    return 1;
}

/* 8007E4E4: Scene camera director: commands set up camera shots; the states fly the
 * camera target across the map, zoom and shake the view by unk7C. */
s32 worldmap_scene15_camera_update(s32 index) {
    WorldmapActor *actor;
    CameraScratch *scratch;
    s32 shake;

    actor = &worldmap_actor_slots[index];
    scratch = (CameraScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        worldmap_camera_distance = 0x500000;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = 0x150;
        worldmap_camera_angle.vy = 0xF50;
        worldmap_camera_angle.vz = 0;
        break;
    case 2:
        worldmap_camera_distance = 0x4D0000;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = 0xC60;
        worldmap_camera_angle.vy = 0x380;
        worldmap_camera_angle.vz = 0;
        break;
    case 3:
        worldmap_camera_distance = 0x400000;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = 0x30;
        worldmap_camera_angle.vy = 0x160;
        worldmap_camera_angle.vz = 0;
        break;
    case 4:
        worldmap_camera_distance = 0x400000;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = 0x30;
        worldmap_camera_angle.vy = 0x960;
        worldmap_camera_angle.vz = 0;
        break;
    case 5:
        actor->state = 1;
        actor->unk4 = 0;
        actor->unk5C = -0x8000;
        break;
    case 6:
        actor->state = 3;
        actor->unk7C = 0x1000;
        worldmap_camera_distance = 0x200000;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = -0x58;
        worldmap_camera_angle.vy = 0xE58;
        worldmap_camera_angle.vz = 0;
        break;
    case 7:
        actor->state = 5;
        actor->unk4 = 0;
        actor->unk7C = 0x40000;
        break;
    case 16:
        actor->state = 0x10;
        actor->unk4 = 0;
        actor->unk5C = -0x8000;
        break;
    case 17:
        actor->state = 0x10;
        actor->unk7C = 0x1000;
        worldmap_camera_distance = 0x400000;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = 0xE0;
        worldmap_camera_angle.vy = 0x798;
        worldmap_camera_angle.vz = 0;
        break;
    case 24:
        actor->state = 0x18;
        actor->unk4 = 0;
        actor->unk5C = -0x8000;
        break;
    }
    if (worldmap_view_kind == 0) {
        worldmap_wrap_position(&actor->position);
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        worldmap_camera.target.vx -= 0x42D00;
        worldmap_camera.target.vz += 0x6D300;
        if (worldmap_camera.target.vx < 0x1F9E000 && worldmap_camera.target.vz > 0x5998000) {
            worldmap_camera.target.vx = 0x1F9E000;
            worldmap_camera.target.vz = 0x5998000;
            actor->state = 2;
            actor->unk7C = 0x10000;
        } else {
            GROUND_SCROLL[0] -= 0x42D00;
            GROUND_SCROLL[2] += 0x6D300;
        }
        worldmap_wrap_position(&actor->position);
        worldmap_camera_angle.vy -= 8;
        worldmap_camera_angle.vx += actor->unk5C >> 12;
        actor->unk5C += 0x300;
        worldmap_camera_distance += 0x8000;
        if (worldmap_camera_angle.vx > 0x80) {
            worldmap_camera_angle.vx = 0x80;
        }
        break;
    case 2:
        actor->unk7C -= 0x80;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
        }
        break;
    case 3:
        worldmap_camera.target.vx -= 0x4C400;
        worldmap_camera.target.vy += 0xFF80;
        worldmap_camera.target.vz -= 0x65580;
        if (worldmap_camera.target.vx < 0x1498000 && worldmap_camera.target.vz < 0x4AF2000) {
            worldmap_camera.target.vx = 0x1498000;
            worldmap_camera.target.vz = 0x4AF2000;
            actor->state = 4;
            actor->unk5C = 0x20000;
        } else {
            GROUND_SCROLL[0] -= 0x4C400;
            GROUND_SCROLL[2] -= 0x65580;
        }
        worldmap_wrap_position(&actor->position);
        worldmap_camera_distance += 0x20000;
        worldmap_camera_angle.vy += 0x20;
        worldmap_camera_angle.vx -= 4;
        break;
    case 4:
        worldmap_camera_angle.vy += actor->unk5C >> 12;
        actor->unk5C -= 0x800;
        if (actor->unk5C < 0) {
            actor->unk5C = 0;
            actor->state = 0;
        }
        break;
    case 5:
        actor->unk7C -= 0x2000;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
            actor->state = 0;
        }
        break;
    case 16:
        worldmap_camera.target.vx -= 0x42D00;
        worldmap_camera.target.vz += 0x6D300;
        if (worldmap_camera.target.vx < 0x1F9E000 && worldmap_camera.target.vz > 0x5998000) {
            worldmap_camera.target.vx = 0x1F9E000;
            worldmap_camera.target.vz = 0x5998000;
            actor->state = 0x11;
            actor->unk7C = 0x8000;
        } else {
            GROUND_SCROLL[0] -= 0x42D00;
            GROUND_SCROLL[2] += 0x6D300;
        }
        worldmap_wrap_position(&actor->position);
        worldmap_camera_angle.vy -= 8;
        worldmap_camera_angle.vx += actor->unk5C >> 12;
        actor->unk5C += 0x300;
        worldmap_camera_distance += 0x8000;
        if (worldmap_camera_angle.vx > 0x80) {
            worldmap_camera_angle.vx = 0x80;
        }
        break;
    case 17:
        actor->unk7C -= 0x200;
        if (actor->unk7C < 0x1000) {
            actor->unk7C = 0x1000;
        }
        break;
    case 24:
        worldmap_camera.target.vx -= 0x42D00;
        worldmap_camera.target.vz += 0x6D300;
        if (worldmap_camera.target.vx < 0x2152000 && worldmap_camera.target.vz > 0x5AA3000) {
            worldmap_camera.target.vx = 0x2152000;
            worldmap_camera.target.vz = 0x5AA3000;
            actor->state = 0x19;
            actor->unk60 = 0x10000;
        } else {
            GROUND_SCROLL[0] -= 0x42D00;
            GROUND_SCROLL[2] += 0x6D300;
        }
        worldmap_wrap_position(&actor->position);
        worldmap_camera_angle.vy -= 8;
        worldmap_camera_angle.vx += actor->unk5C >> 12;
        actor->unk5C += 0x300;
        worldmap_camera_distance += 0x8000;
        if (worldmap_camera_angle.vx > 0x80) {
            worldmap_camera_angle.vx = 0x80;
        }
        break;
    case 25:
        worldmap_camera_angle.vy -= actor->unk60 >> 12;
        actor->unk60 -= 0x200;
        if (actor->unk60 < 0) {
            actor->unk60 = 0;
        }
        break;
    }
    shake = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    scratch->view.vy = shake;
    VIEW.eye.vy += shake;
    VIEW_VECTORS[1].vy += scratch->view.vy;
    return 1;
}

/* 8007EBBC: Set up `count` translucent blue textured quads of a scene object and copy them to its second buffer. */
void worldmap_build_blue_translucent_quads(SceneObject *object, POLY_FT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = GetTPage(0, abr, 0x300, 0x100);
        quads->clut = GetClut(0, 0x1FF);
        setSemiTrans(quads, 1);
        setRGB0(quads, 0x3C, 0x3C, 0xC0);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT4));
}

/* 8007ECA4: Start the flight: link objects 2-3 to 1, build their quads, hide 1 and place the actor behind the player on its entry path. */
s32 worldmap_scene15_flying_vehicle_start(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    worldmap_objects_link(1, 2);
    worldmap_objects_link(1, 3);
    objects = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    worldmap_build_blue_translucent_quads(&objects[1], objects[1].prims, objects[1].def->primitive_count, 3);
    worldmap_build_blue_translucent_quads(&objects[2], objects[2].prims, objects[2].def->primitive_count, 3);
    worldmap_build_blue_translucent_quads(&objects[3], objects[3].prims, objects[3].def->primitive_count, 1);
    worldmap_objects[1].visible = 0;
    worldmap_objects[1].angle.vz = 0;
    worldmap_objects[1].angle.vy = 0;
    worldmap_objects[1].angle.vx = 0;
    RotMatrixYXZ(&worldmap_objects[1].angle, &worldmap_objects[1].matrix);
    actor->motion.vx = -0x85A;
    actor->state = 0;
    actor->motion.vy = 0;
    actor->motion.vz = 0xDA6;
    actor->u.step = worldmap_scene15_vehicle_stop_points[worldmap_entry_index].vx << 12;
    actor->unk54 = worldmap_scene15_vehicle_stop_points[worldmap_entry_index].vz << 12;
    actor->position.vx = worldmap_player_position.vx - actor->motion.vx * 0x3680;
    actor->position.vy = worldmap_player_position.vy;
    actor->position.vz = worldmap_player_position.vz - actor->motion.vz * 0x3680;
    return 1;
}

/* Scratchpad work area of the flight-track actor. */
typedef struct {
    VECTOR axis[3];    /* 0x00: forward (or scale), up, side */
    u8 pad30[0x70];
    SVECTOR position;  /* 0xA0 */
    SVECTOR angle;     /* 0xA8 */
    u8 padB0[0x40];
    MATRIX base;       /* 0xF0 */
    MATRIX rotation;   /* 0x110 */
    u8 pad130[0x20];
    MATRIX frame;      /* 0x150 */
} TrackScratch;

/* 8007EE34: Flying vehicle (scene object 1): commands place it on its approach track;
 * it flies along its motion vector, stops at the landing point, trails
 * effect 0x22 and faces its direction. */
s32 worldmap_scene15_flying_vehicle_update(s32 index) {
    s32 result;
    WorldmapActor *actor;
    SceneObject *object;
    TrackScratch *scratch;

    result = 1;
    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[1];
    scratch = (TrackScratch *)0x1F800000;
    if (actor->unk4 != 0) {
        actor->motion.vx = -0x85A;
        actor->motion.vy = 0;
        actor->motion.vz = 0xDA6;
        actor->u.step = worldmap_scene15_vehicle_stop_points[worldmap_entry_index].vx << 12;
        actor->unk54 = worldmap_scene15_vehicle_stop_points[worldmap_entry_index].vz << 12;
    }
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 0;
        actor->position.vx = worldmap_player_position.vx - actor->motion.vx * 0x3680;
        actor->position.vy = worldmap_player_position.vy;
        actor->position.vz = worldmap_player_position.vz - actor->motion.vz * 0x3680;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0;
        actor->position.vx = worldmap_player_position.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x2D00;
        actor->position.vz = worldmap_player_position.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x2D00;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 0;
        actor->position.vx = worldmap_player_position.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x1E00;
        actor->position.vz = worldmap_player_position.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x1E00;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 1;
        actor->position.vx = worldmap_player_position.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x3300;
        actor->position.vz = worldmap_player_position.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x3300;
        break;
    case 5:
        actor->state = 2;
        actor->motion.vx = -0x988;
        actor->motion.vy = 0x227;
        actor->unk4 = 0;
        actor->motion.vz = -0xCAB;
        actor->position.vx = 0x1F9E000;
        actor->position.vz = 0x5998000;
        break;
    case 16:
        actor->unk4 = 0;
        actor->state = 0x10;
        actor->position.vx = actor->motion.vx * 0x3300 + 0x56F2000;
        actor->position.vz = actor->motion.vz * 0x3300 + 0x7F2C000;
        break;
    case 24:
        actor->unk4 = 0;
        actor->state = 0x18;
        actor->position.vx = worldmap_player_position.vx - actor->motion.vx * 0x3680 + actor->motion.vx * 0x3300;
        actor->position.vz = worldmap_player_position.vz - actor->motion.vz * 0x3680 + actor->motion.vz * 0x3300;
        break;
    }
    actor->position.vx += actor->motion.vx << 7;
    actor->position.vy += actor->motion.vy << 7;
    actor->position.vz += actor->motion.vz << 7;
    switch (actor->state) {
    case 0:
        if (actor->position.vx < actor->u.step && actor->position.vz > actor->unk54) {
            actor->position.vx = actor->u.step;
            actor->position.vz = actor->unk54;
            worldmap_actor_request(4, 2);
            worldmap_actor_request(5, 2);
            worldmap_actor_request(6, 2);
            worldmap_actor_request(7, 2);
            worldmap_actor_request(8, 2);
        }
        break;
    case 1:
        if (actor->position.vx < 0x1F9E000 && actor->position.vz > 0x5998000) {
            actor->position.vx = 0x1F9E000;
            actor->position.vz = 0x5998000;
            actor->state = 0x40;
            scratch->position.vx = actor->position.vx >> 12;
            scratch->position.vy = actor->position.vy >> 12;
            scratch->position.vz = actor->position.vz >> 12;
            worldmap_effects_start_emitters(0x20, &scratch->position, NULL);
        }
        break;
    case 2:
        if (actor->position.vy > -0x80000) {
            scratch->position.vx = 0x1498;
            scratch->position.vz = 0x4AF2;
            scratch->position.vy = worldmap_terrain_get_height(0x1498000, 0x4AF2000) >> 12;
            worldmap_effects_start_emitters(0x25, &scratch->position, NULL);
            worldmap_effects_start_emitters(0x26, &scratch->position, NULL);
            worldmap_effects_start_emitters(0x27, &scratch->position, NULL);
            actor->state = 3;
        }
        break;
    case 3:
        if (actor->position.vy > 0x100000) {
            actor->position.vx -= actor->motion.vx << 7;
            actor->position.vy -= actor->motion.vy << 7;
            actor->position.vz -= actor->motion.vz << 7;
            object[0].visible = object[1].visible = object[2].visible = 1;
            actor->state = 0x41;
        }
        break;
    case 16:
        if (actor->position.vx < 0x1F9E000 && actor->position.vz > 0x5998000) {
            actor->state = 0x11;
            scratch->position.vx = 0x1F9E;
            scratch->position.vz = 0x5998;
            scratch->position.vy = actor->position.vy >> 12;
            worldmap_effects_start_emitters(0x20, &scratch->position, NULL);
            worldmap_effects_start_emitters(0x21, &scratch->position, NULL);
        }
        break;
    case 17:
        if (actor->position.vx < 0x199D000 && actor->position.vz > 0x6367000) {
            actor->position.vx = 0x199D000;
            actor->position.vz = 0x6367000;
            object[0].visible = object[1].visible = object[2].visible = 1;
            worldmap_actor_request(4, 5);
            worldmap_actor_request(5, 5);
            worldmap_actor_request(6, 5);
            worldmap_actor_request(7, 5);
            worldmap_actor_request(8, 5);
            result = 3;
        }
        break;
    case 24:
        if (actor->position.vx < 0x1EB5000 && actor->position.vz > 0x5EE6000) {
            object[0].visible = object[1].visible = object[2].visible = 1;
            worldmap_actor_request(4, 2);
            worldmap_actor_request(5, 2);
            worldmap_actor_request(6, 2);
            worldmap_actor_request(7, 2);
            worldmap_actor_request(8, 2);
            actor->state = 0x41;
        }
        break;
    case 0x40:
        actor->position.vx = 0x1F9E000;
        actor->position.vz = 0x5998000;
        break;
    case 0x41:
        result = 3;
        break;
    }
    worldmap_wrap_position(&actor->position);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    switch (actor->state) {
    case 0:
    case 1:
    case 16:
    case 24:
        scratch->base.m[0][0] = 0xDA6;
        scratch->base.m[0][1] = 0;
        scratch->base.m[0][2] = 0x85A;
        scratch->base.m[1][0] = 0;
        scratch->base.m[1][1] = -0x1000;
        scratch->base.m[1][2] = 0;
        scratch->base.m[2][0] = -0x85A;
        scratch->base.m[2][1] = 0;
        scratch->base.m[2][2] = 0xDA6;
        object->angle.vz = (object->angle.vz + 0x100) & 0xFFF;
        RotMatrixYXZ(&object->angle, &scratch->rotation);
        MulMatrix0(&scratch->base, &scratch->rotation, &object->matrix);
        scratch->axis[0].vx = scratch->axis[0].vy = 0x800;
        scratch->axis[0].vz = 0x1800;
        ScaleMatrix(&object->matrix, &scratch->axis[0]);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->angle.vx = scratch->angle.vz = 0;
        scratch->angle.vy = ratan2(0x85A, 0xDA6) & 0xFFF;
        worldmap_effects_start_emitters(0x22, &scratch->position, &scratch->angle);
        break;
    case 4:
    case 17:
    case 0x41:
        worldmap_effects_stop_emitters(0x22);
        break;
    case 2:
    case 3:
        scratch->axis[0].vx = 0x988;
        scratch->axis[0].vy = -0x227;
        scratch->axis[0].vz = -0xCAB;
        scratch->axis[1].vx = scratch->axis[1].vz = 0;
        scratch->axis[1].vy = 0x1000;
        OuterProduct12(&scratch->axis[1], &scratch->axis[0], &scratch->axis[2]);
        VectorNormal(&scratch->axis[2], &scratch->axis[2]);
        OuterProduct12(&scratch->axis[0], &scratch->axis[2], &scratch->axis[1]);
        VectorNormal(&scratch->axis[1], &scratch->axis[1]);
        scratch->frame.m[0][0] = scratch->axis[2].vx;
        scratch->frame.m[0][1] = scratch->axis[2].vy;
        scratch->frame.m[0][2] = scratch->axis[2].vz;
        scratch->frame.m[1][0] = scratch->axis[1].vx;
        scratch->frame.m[1][1] = scratch->axis[1].vy;
        scratch->frame.m[1][2] = scratch->axis[1].vz;
        scratch->frame.m[2][0] = scratch->axis[0].vx;
        scratch->frame.m[2][1] = scratch->axis[0].vy;
        scratch->frame.m[2][2] = scratch->axis[0].vz;
        worldmap_get_matrix_angles(&scratch->frame, &scratch->angle);
        libgte_transpose_matrix(&scratch->frame, &scratch->base);
        object->angle.vz = (object->angle.vz + 0x100) & 0xFFF;
        RotMatrixYXZ(&object->angle, &scratch->rotation);
        MulMatrix0(&scratch->base, &scratch->rotation, &object->matrix);
        scratch->axis[0].vx = scratch->axis[0].vy = 0x800;
        scratch->axis[0].vz = 0x1800;
        ScaleMatrix(&object->matrix, &scratch->axis[0]);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        worldmap_effects_start_emitters(0x22, &scratch->position, &scratch->angle);
        break;
    }
    return result;
}

/* 8007F8AC: Build scene object `index` and start its fall. */
s32 worldmap_scene15_flame_start(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 slot;
    s32 abr;

    slot = index - 4;
    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[slot + 4];
    abr = 3;
    if (slot == 4) {
        abr = 1;
    }
    worldmap_build_blue_translucent_quads(object, object->prims, object->def->primitive_count, abr);
    actor->motion.vx = -0x85A;
    actor->motion.vz = 0xDA6;
    actor->state = 0;
    actor->motion.vy = 0;
    actor->unk5C = worldmap_scene15_flame_sizes[slot];
    actor->wait = 0x3C;
    return 1;
}

/* Scratchpad work area of the exhaust-flame actors. */
typedef struct {
    VECTOR scale;      /* 0x00 */
    u8 pad10[0x90];
    SVECTOR position;  /* 0xA0 */
    SVECTOR angle;     /* 0xA8 */
    u8 padB0[0x40];
    MATRIX base;       /* 0xF0 */
    MATRIX rotation;   /* 0x110 */
} FlameScratch;

/* 8007F968: Exhaust flame on scene object `index`: commands 1-5 stop, start or restart
 * it; it follows actor 3, emits effects 0x23/0x24 and shrinks away; done (3)
 * once its size runs out. */
s32 worldmap_scene15_flame_update(s32 index) {
    WorldmapActor *actor;
    WorldmapActor *leader;
    SceneObject *object;
    FlameScratch *scratch;
    s32 unused[2]; /* unreferenced; the original frame reserves it */

    actor = &worldmap_actor_slots[index];
    leader = &worldmap_actor_slots[3];
    object = &worldmap_objects[index];
    scratch = (FlameScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 3:
        actor->wait = 0x3C;
        actor->motion.vx = -0x85A;
        actor->motion.vz = 0xDA6;
        actor->unk4 = 0;
        actor->state = 0;
        actor->motion.vy = 0;
        actor->unk5C = worldmap_scene15_flame_sizes[index - 4];
        object->visible = 0;
        object->angle.vx = object->angle.vy = object->angle.vz = 0;
        RotMatrixYXZ(&object->angle, &object->matrix);
        worldmap_effects_stop_emitters(0x23);
        worldmap_effects_stop_emitters(0x24);
        break;
    case 4:
        actor->wait = 4;
        actor->state = 1;
        actor->unk4 = 0;
        actor->unk5C = worldmap_scene15_flame_sizes[index - 4];
        object->visible = 0;
        object->angle.vx = object->angle.vy = object->angle.vz = 0;
        RotMatrixYXZ(&object->angle, &object->matrix);
        worldmap_effects_stop_emitters(0x23);
        worldmap_effects_stop_emitters(0x24);
        break;
    case 5:
        actor->state = 1;
        actor->unk4 = 0;
        actor->wait = 0x1E;
        break;
    }
    switch (actor->state) {
    case 0:
        actor->position.vx = leader->position.vx;
        actor->position.vy = leader->position.vy;
        actor->position.vz = leader->position.vz;
        scratch->position.vx = (actor->position.vx + 0x42D000) >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = (actor->position.vz - 0x6D3000) >> 12;
        scratch->angle.vx = scratch->angle.vz = 0;
        scratch->angle.vy = ratan2(0x85A, 0xDA6) & 0xFFF;
        worldmap_effects_start_emitters(0x23, &scratch->position, &scratch->angle);
        worldmap_effects_start_emitters(0x24, &scratch->position, &scratch->angle);
        break;
    case 1:
        if (--actor->wait < 0) {
            actor->wait = 0;
            actor->unk5C -= 0x80;
        }
        break;
    }
    scratch->base.m[0][0] = 0xDA6;
    scratch->base.m[0][1] = 0;
    scratch->base.m[0][2] = 0x85A;
    scratch->base.m[1][0] = 0;
    scratch->base.m[1][1] = -0x1000;
    scratch->base.m[1][2] = 0;
    scratch->base.m[2][0] = -0x85A;
    scratch->base.m[2][1] = 0;
    scratch->base.m[2][2] = 0xDA6;
    object->angle.vz = (object->angle.vz + 0x100) & 0xFFF;
    RotMatrixYXZ(&object->angle, &scratch->rotation);
    MulMatrix0(&scratch->base, &scratch->rotation, &object->matrix);
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    scratch->scale.vx = scratch->scale.vy = actor->unk5C;
    scratch->scale.vz = 0x2000;
    ScaleMatrix(&object->matrix, &scratch->scale);
    if (actor->unk5C < 0) {
        actor->unk5C = 0;
        object->visible = 1;
        return 3;
    }
    return 1;
}

/* 8007FC8C: Rebuild scene objects 9 and 10 and start a descent at a fixed point. */
s32 worldmap_scene15_grow_objects_9_10_start(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    worldmap_build_translucent_quads(&objects[9], objects[9].prims, objects[9].def->primitive_count);
    worldmap_build_translucent_quads(&objects[10], objects[10].prims, objects[10].def->primitive_count);
    actor->state = 0;
    actor->position.vx = 0x1498000;
    actor->position.vy = -0x80000;
    actor->position.vz = 0x4AF2000;
    actor->u.step = 0;
    actor->unk54 = -0x800;
    actor->unk58 = 0x80;
    return 3;
}

/* 8007FD30: Grow and fade scene objects 9 and 10 at the actor; ends the step when faded out. */
s32 worldmap_scene15_grow_objects_9_10_update(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[9];
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
    if ((actor->unk58 -= 3) < 0) {
        actor->unk58 = 0;
        return 3;
    }
    return 1;
}

/* 8007FF70: Set up the third cutscene mode: display, terrain loader, scene objects and
 * its actors. */
void worldmap_scene13_start(void) {
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
    worldmap_player_position.vx = 0x4100000;
    worldmap_player_position.vy = -0x80000;
    worldmap_player_position.vz = 0x13C0000;
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
    cd_sync_reads(0);
    sound_add_effect_bank(sound_effect_bank);
    cd_select_directory(0x24, 0);
    worldmap_terrain_reset_at(&worldmap_player_position);
    do {
        worldmap_stream_step();
        VSync(0);
    } while (worldmap_stream_count_queued() > 0);
    worldmap_actor_spawn((s32)worldmap_screen_fade_start, (s32)worldmap_screen_fade_update);
    worldmap_actor_spawn((s32)worldmap_scene13_director_start, (s32)worldmap_scene13_director_update);
    worldmap_actor_spawn((s32)worldmap_scene13_camera_start, (s32)worldmap_scene13_camera_update);
    worldmap_actor_spawn((s32)worldmap_scene13_effects_start, (s32)worldmap_scene13_effects_update);
    worldmap_actor_spawn((s32)worldmap_scene13_grow_objects_0_1_start, (s32)worldmap_scene13_grow_objects_0_1_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_billboards_start, (s32)worldmap_scene_frame_billboards_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_billboards_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 80080218: Leave the world map for scene 0x84 (flag word 2). */
void worldmap_scene13_leave(void) {
    sound_stop_all_effects();
    sound_remove_effect_bank(sound_effect_bank);
    heap_free(sound_effect_bank);
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
    game_data.map = 0x84;
    game_data.entry[2] = 2;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 8008032C: Restart an actor's timed sequence at its first step. */
s32 worldmap_scene13_director_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->state = worldmap_scene13_cue_states[0];
    actor->wait = worldmap_scene13_cue_waits[actor->u.step];
    return 1;
}
