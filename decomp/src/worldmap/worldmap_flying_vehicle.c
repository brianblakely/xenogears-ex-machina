/* World map unit 8008E190-80090A84 (rodata 800709F8-80070B54, data
 * 8009B1AC-8009B214): the flying vehicle (start, boarding, flight, landing
 * and the scripted take-offs), its rotors and the two-button combination
 * latch.
 *
 * worldmap_follower_vehicle_update's 65-entry table ends at 800709f8 and worldmap_flying_vehicle_start's
 * follows at once, 0 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after worldmap_follower_vehicle_update, at or before
 * worldmap_flying_vehicle_start. */
#include "common.h"
#include "psyq/libgte.h"
#include "resident/gamedata.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "party.h"
#include "scene.h"
#include "screen.h"
#include "terrain.h"

/* Scripted flight waypoints (x, z; -1 ends) of worldmap_flying_vehicle_update. */
SVECTOR worldmap_flying_vehicle_route_mode3[5] = { /* 8009B1AC */
    {0x4E00, 0, 0x2C80}, {0x4A00, 0, 0x2AC0}, {0x4600, 0, 0x2C00}, {0x4600, 0, 0x2D74}, {-1, 0, -1},
};
SVECTOR worldmap_flying_vehicle_route_mode4[8] = { /* 8009B1D4 */
    {0x22F7, 0, 0x4C1C}, {0x24F1, 0, 0x4AF5}, {0x2686, 0, 0x4749}, {0x2643, 0, 0x43BE},
    {0x24F4, 0, 0x4224}, {0x228E, 0, 0x4124}, {0x1F5C, 0, 0x410C}, {-1, 0, -1},
};

/* Compiled-out debug trace of the restored vehicle position. */
#define VEHICLE_TRACE_POSITION(actor) do { } while (0)

/* 8008E190: Start the flying vehicle: restore its saved spot and heading, set its
 * turn rate by kind, and place it by movement mode (landed, boarded or
 * flying with the player); scene objects 0 and 1 follow it. */
s32 worldmap_flying_vehicle_start(s32 index) {
    ActorScratch *scratch;
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    actor->unk24 = 0;
    worldmap_load_flying_vehicle_position(&actor->position);
    VEHICLE_TRACE_POSITION(actor);
    result = 1;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk64 = 0;
    actor->unk60 = 0;
    actor->unk74 = 0;
    actor->unk68 = -0x280000;
    actor->unk70 = 0;
    actor->unk6C = 0;
    actor->heading = game_data_flying_vehicle_heading;
    scratch = (ActorScratch *)0x1F800000;
    switch (game_data.worldmap.flags & 0x1FFF) {
    case 0:
        result = 3;
        break;
    case 1:
    case 2:
        actor->turn = 0xC;
        break;
    case 3:
    case 4:
        actor->turn = 0x20;
        break;
    }
    switch (worldmap_movement_mode) {
    case 1:
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        break;
    case 2:
    case 3:
        actor->state = 1;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        break;
    case 4:
    case 5:
        actor->state = 3;
        actor->position.vx = worldmap_player_position.vx;
        actor->position.vz = worldmap_player_position.vz;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz) - 0x40000;
        actor->heading = worldmap_player_heading;
        actor->unk74 = worldmap_party_count;
        worldmap_camera_follow_target.target = actor->position;
        worldmap_camera_follow_heading = actor->heading;
        worldmap_objects[3].visible = 1;
        worldmap_objects[2].visible = 1;
        worldmap_objects[1].visible = 1;
        break;
    case 7:
        actor->state = 2;
        actor->position.vx = worldmap_player_position.vx;
        actor->position.vz = worldmap_player_position.vz;
        actor->position.vy = worldmap_player_position.vy;
        actor->heading = worldmap_player_heading;
        actor->unk74 = worldmap_party_count;
        worldmap_camera_follow_target.target = actor->position;
        worldmap_camera_follow_heading = actor->heading;
        break;
    }
    worldmap_objects[0].position = actor->position;
    worldmap_objects[0].visible = actor->unk24;
    scratch->position.vx = -(actor->unk70 >> 12);
    scratch->position.vy = actor->heading;
    scratch->position.vz = worldmap_camera_angle.vz;
    RotMatrixYXZ(&scratch->position, &worldmap_objects[0].matrix);
    RotMatrixYXZ(&scratch->position, &worldmap_objects[1].matrix);
    worldmap_save_flying_vehicle_position(&actor->position);
    game_data.worldmap.vehicle_heading = actor->heading;
    switch (worldmap_mode_index) {
    case 2:
        actor->state = 0x24;
        break;
    case 3:
        actor->state = 0x28;
        actor->unk7C = 0;
        break;
    case 4:
        actor->state = 0x30;
        actor->unk7C = 0;
        break;
    case 5:
        actor->state = 0x34;
        actor->unk7C = 0;
        break;
    }
    return result;
}

/* 8008E4F4: Move scene object 0 to the vehicle actor and orient objects 0 and 1;
 * modes 4-5 show objects 1-3, modes 6-7 hide them. 3 once the vehicle kind
 * is cleared. */
s32 worldmap_flying_vehicle_resume(s32 index) {
    ActorScratch *scratch;
    WorldmapActor *actor;
    s32 result;

    scratch = (ActorScratch *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    worldmap_objects[0].position = actor->position;
    result = 1;
    worldmap_objects[0].visible = actor->unk24;
    if (!(game_data.worldmap.flags & 0x1FFF)) {
        result = 3;
    }
    switch (worldmap_movement_mode) {
    case 1:
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        break;
    case 2:
    case 3:
        actor->state = 1;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        break;
    case 4:
    case 5:
        worldmap_objects[3].visible = 1;
        worldmap_objects[2].visible = 1;
        worldmap_objects[1].visible = 1;
        break;
    case 6:
    case 7:
        worldmap_objects[3].visible = 0;
        worldmap_objects[2].visible = 0;
        worldmap_objects[1].visible = 0;
        break;
    }
    scratch->position.vx = -(actor->unk70 >> 12);
    scratch->position.vy = actor->heading;
    scratch->position.vz = worldmap_camera_angle.vz;
    RotMatrixYXZ(&scratch->position, &worldmap_objects[0].matrix);
    RotMatrixYXZ(&scratch->position, &worldmap_objects[1].matrix);
    return result;
}

/* 8008E680: Start the flying vehicle at its saved position, height 0x80 and heading;
 * the camera and scene object 0 follow it. */
s32 worldmap_flying_vehicle_start_airborne(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    worldmap_load_flying_vehicle_position(&actor->position);
    actor->position.vy = 0x80000;
    actor->heading = game_data.worldmap.vehicle_heading;
    actor->turn = 0x20;
    actor->unk74 = 3;
    actor->unk68 = -0x280000;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk24 = 0;
    actor->unk60 = 0;
    actor->unk64 = 0;
    actor->unk6C = 0;
    actor->state = 8;
    worldmap_camera_follow_target.target = actor->position;
    worldmap_camera_follow_heading = actor->heading;
    worldmap_objects[0].position = actor->position;
    return 1;
}

/* The vehicle helpers below are plain statement blocks: a do { } while (0)
 * wrapper would leave loop notes that keep the scheduler from interleaving
 * them with the surrounding stores as the original does. */

/* Turn the vehicle towards `goal` (heading units), by at most `step` per frame
 * from the heading it had last frame (worldmap.vehicle_heading). */
#define VEHICLE_TURN(goal, step)                                               \
    {                                                                          \
        delta = (goal) - game_data.worldmap.vehicle_heading;                  \
        if (ABS(delta) > 0x800) {                                              \
            if (delta < 0) {                                                   \
                delta += 0x1000;                                               \
            } else {                                                           \
                delta -= 0x1000;                                               \
            }                                                                  \
        }                                                                      \
        if (ABS(delta) > (step)) {                                             \
            if (delta < 0) {                                                   \
                actor->heading = game_data.worldmap.vehicle_heading - (step); \
            } else {                                                           \
                actor->heading = game_data.worldmap.vehicle_heading + (step); \
            }                                                                  \
        }                                                                      \
        actor->heading &= 0xFFF;                                               \
    }

/* Tilt and heading of the vehicle model into scene objects 0 and 1. */
#define VEHICLE_ORIENT()                                                     \
    {                                                                        \
        scratch->rotation.vx = -(actor->unk70 >> 12);                        \
        scratch->rotation.vy = actor->heading;                               \
        scratch->rotation.vz = worldmap_camera_angle.vz;                                \
        RotMatrixYXZ(&scratch->rotation, &worldmap_objects[0].matrix);            \
        RotMatrixYXZ(&scratch->rotation, &worldmap_objects[1].matrix);            \
    }

/* Point the camera at the vehicle. Expanded per state: each copy ends its
 * state, and the copies are merged into one tail by jump optimization. */
#define VEHICLE_CAMERA()                                                     \
    {                                                                        \
        worldmap_camera_follow_target.target = actor->position;                                 \
        worldmap_camera_follow_heading = actor->heading;                                         \
    }

/* Stop the vehicle after a landing and clear its take-off counter. */
#define VEHICLE_STOP()                                                       \
    do {                                                                     \
        actor->motion.vz = 0;                                                \
        actor->motion.vy = 0;                                                \
        actor->motion.vx = 0;                                                \
        actor->unk74 = 0;                                                    \
    } while (0)

/* The vehicle's position in world units (at height `y`) for an effect. */
#define VEHICLE_SPOT(scratch, y)                                             \
    {                                                                        \
        scratch->spot.vx = actor->position.vx >> 12;                         \
        scratch->spot.vy = (y) >> 12;                                        \
        scratch->spot.vz = actor->position.vz >> 12;                         \
    }

/* Scratchpad work area of the flying vehicle. */
typedef struct {
    VECTOR target;     /* 0x00: waypoint */
    u8 pad10[0x80];
    VECTOR hit;        /* 0x90: move probe (SCRATCH_HIT) */
    SVECTOR rotation;  /* 0xA0: model tilt/heading, or an effect spot */
    SVECTOR spot;      /* 0xA8: effect spot */
} VehicleScratch;

#define VEHICLE_SCRATCH ((VehicleScratch *)0x1F800000)

/* 8008E76C: The flying vehicle (Gear transport): boarding, flight with terrain and
 * landing checks, the scripted take-offs and landings, and the scene exits.
 * Returns 2 when the party leaves on foot. */
s32 worldmap_flying_vehicle_update(s32 index) {
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    s32 result;
    WorldmapActor *actor;
    VehicleScratch *scratch;
    s16 command;
    s32 kind;
    s32 hit;
    s32 delta;
    s32 terrain;
    s32 height;

    scratch = VEHICLE_SCRATCH;
    result = 1;
    actor = &worldmap_actor_slots[index];
    command = actor->unk4;
    if (command == 4) {
        actor->unk4 = 0;
        if (worldmap_party_count == ++actor->unk74) {
            worldmap_actor_request(8, 9);
            VEHICLE_CAMERA();
            game_data.worldmap.flags |= 0x4000;
            kind = game_data.worldmap.flags & 0x1FFF;
            switch (kind) {
            case 1:
                actor->state = 0xC;
                worldmap_movement_mode = command;
                actor->unk68 = worldmap_terrain_get_height(actor->position.vx, actor->position.vz) + 0x18000;
                VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
                VEHICLE_SCRATCH->rotation.vy = actor->heading;
                VEHICLE_SCRATCH->rotation.vz = worldmap_camera_angle.vz;
                VEHICLE_SPOT(VEHICLE_SCRATCH, actor->position.vy);
                worldmap_effects_start_emitters(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                worldmap_actor_cylinder_count = 0;
                break;
            case 2:
                actor->state = 0xC;
                worldmap_movement_mode = 5;
                actor->unk68 = worldmap_terrain_get_height(actor->position.vx, actor->position.vz) + 0x18000;
                VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
                VEHICLE_SCRATCH->rotation.vy = actor->heading;
                VEHICLE_SCRATCH->rotation.vz = worldmap_camera_angle.vz;
                VEHICLE_SPOT(VEHICLE_SCRATCH, actor->position.vy);
                worldmap_effects_start_emitters(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                worldmap_actor_cylinder_count = 0;
                break;
            case 3:
                sound_set_seq_fade((SoundSeq *)mode_music_seq, 0, 0xF0);
                VEHICLE_SCRATCH->rotation.vx = actor->position.vx >> 12;
                VEHICLE_SCRATCH->rotation.vy = actor->position.vy >> 12;
                VEHICLE_SCRATCH->rotation.vz = actor->position.vz >> 12;
                worldmap_effects_start_emitters(worldmap_terrain_get_layer(&actor->position) == kind ? 0x3E : 0x3D,
                              &VEHICLE_SCRATCH->rotation, NULL);
                worldmap_actor_request(9, 9);
                worldmap_actor_request(0xA, 9);
                worldmap_actor_request(0xB, 9);
                result = 2;
                actor->state = 8;
                actor->command_arg = 0x3C;
                worldmap_movement_mode = 7;
                worldmap_actor_cylinder_count = 0;
                break;
            }
            game_data.inGear[0] = 1;
            if (game_data.party[1] != 0xFF) {
                game_data.inGear[1] = 1;
            }
            if (game_data.party[2] != 0xFF) {
                game_data.inGear[2] = 1;
            }
            worldmap_encounter_reset_timers();
            worldmap_current_path = (PathRegion *)-1;
            worldmap_path_name_id = -1;
            worldmap_destination_name_id = -1;
        }
    }
    switch (actor->state) {
    case 0:
    case 1:
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz) - 0x1000;
        worldmap_queue_actor_cylinder(index, &actor->position, 0x40, 0x400);
        break;
    case 2:
        switch (worldmap_steer_free_flight(actor)) {
        case 1:
            worldmap_loop_running = 0;
            worldmap_loop_result = 0;
            break;
        case 4:
            if ((s16)worldmap_terrain_can_mode_enter_layer(2, worldmap_terrain_get_layer(&actor->position)) != 0) {
                hit = worldmap_find_clear_heading(&actor->position, (s32)&actor->motion, 0x68000);
                if (hit != -1) {
                    actor->state = 0x10;
                    actor->unk78 = hit;
                    actor->unk68 = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
                    worldmap_actor_request(0xB, 0xA);
                    sound_set_seq_fade((SoundSeq *)mode_music_seq, 0, 0xF0);
                    actor->unk7C = 1;
                    worldmap_effects_stop_emitters(0x3C);
                    worldmap_effects_stop_emitters(0x3F);
                    worldmap_current_path = (PathRegion *)-1;
                    worldmap_path_name_id = -1;
                    worldmap_destination_name_id = -1;
                    worldmap_cylinder_hit_actor = 0;
                    worldmap_cylinder_hit = 0;
                    worldmap_actor_cylinder_count = 0;
                }
            }
            break;
        default:
            hit = worldmap_move_flying(&actor->position, &actor->motion, &scratch->hit, actor->unk60, worldmap_movement_mode);
            if (hit == 0) {
                actor->unk60 = 0;
                actor->motion.vz = 0;
                actor->motion.vx = 0;
            }
            if (hit == 1) {
                worldmap_find_cylinder_hit(&scratch->hit, 0x40, 0x20, &worldmap_cylinder_hit, &worldmap_cylinder_hit_actor);
                if (worldmap_cylinder_hit != 2) {
                    actor->position = scratch->hit;
                } else {
                    actor->unk60 = 0;
                }
            }
            VEHICLE_CAMERA();
            worldmap_path_select_region(&actor->position, 2);
            worldmap_select_actor_path_region();
            VEHICLE_ORIENT();
            height = worldmap_terrain_get_height(actor->position.vx, actor->position.vz) - actor->position.vy;
            if (ABS(height) < 0x60000 && actor->unk60 != 0) {
                VEHICLE_SPOT(scratch, actor->position.vy);
                terrain = worldmap_terrain_get_layer(&actor->position);
                if (terrain == 2) {
                    worldmap_effects_start_emitters(0x3F, &scratch->spot, &scratch->rotation);
                    worldmap_effects_stop_emitters(0x3C);
                } else if (terrain == 3) {
                    worldmap_effects_start_emitters(0x3C, &scratch->spot, &scratch->rotation);
                    worldmap_effects_stop_emitters(0x3F);
                } else {
                    worldmap_effects_stop_emitters(0x3C);
                    worldmap_effects_stop_emitters(0x3F);
                }
            } else {
                worldmap_effects_stop_emitters(0x3C);
                worldmap_effects_stop_emitters(0x3F);
            }
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            worldmap_actor_cylinder_count = 0;
            break;
        }
        break;
    case 3:
        switch (worldmap_steer_flying(actor)) {
        case 1:
            if (worldmap_current_path->kind == 2) {
                actor->state = 0x20;
                actor->unk68 = 0x30000;
                VEHICLE_SPOT(scratch, actor->position.vy);
                worldmap_effects_start_emitters(2, &scratch->spot, &scratch->rotation);
            } else {
                worldmap_loop_running = 0;
                worldmap_loop_result = 0;
            }
            break;
        case 4:
            hit = worldmap_find_clear_heading(&actor->position, (s32)&actor->motion, 0x68000);
            if (hit != -1) {
                actor->state = 0x14;
                actor->unk78 = hit;
                actor->unk68 = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
                scratch->rotation.vx = -(actor->unk70 >> 12);
                scratch->rotation.vy = actor->heading;
                scratch->rotation.vz = worldmap_camera_angle.vz;
                VEHICLE_SPOT(scratch, actor->position.vy);
                worldmap_effects_start_emitters(2, &scratch->spot, &scratch->rotation);
                worldmap_effects_stop_emitters(1);
                worldmap_current_path = (PathRegion *)-1;
                worldmap_path_name_id = -1;
                worldmap_destination_name_id = -1;
            }
            break;
        default:
            VEHICLE_TURN(actor->heading, 0x80);
            hit = worldmap_move_walking(&actor->position, &actor->motion, &scratch->hit, actor->turn << 12, worldmap_movement_mode);
            if (hit == 0) {
                actor->motion = scratch->hit;
                hit = worldmap_move_walking(&actor->position, &actor->motion, &scratch->hit, actor->turn << 12,
                                    worldmap_movement_mode);
                if (hit == 0) {
                    actor->motion.vz = 0;
                    actor->motion.vx = 0;
                }
            }
            if (hit == 1) {
                scratch->hit.vy = worldmap_terrain_get_height(scratch->hit.vx, scratch->hit.vz) + 0x18000;
                worldmap_find_cylinder_hit(&scratch->hit, 0x40, 0x20, &worldmap_cylinder_hit, &worldmap_cylinder_hit_actor);
                if (worldmap_cylinder_hit != 2) {
                    actor->position = scratch->hit;
                } else {
                    actor->unk60 = 0;
                }
            }
            VEHICLE_CAMERA();
            worldmap_path_select_region(&actor->position, 2);
            VEHICLE_ORIENT();
            if (actor->motion.vx | actor->motion.vz) {
                VEHICLE_SPOT(scratch, actor->position.vy);
                worldmap_effects_start_emitters(1, &scratch->spot, &scratch->rotation);
            } else {
                worldmap_effects_stop_emitters(1);
            }
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            if (worldmap_terrain_get_layer(&actor->position) != 3) {
                worldmap_effects_stop_emitters(1);
                worldmap_effects_start_emitters(3, &scratch->spot, &scratch->rotation);
                actor->state = 4;
                actor->wait = 0x28;
            }
            break;
        }
        break;
    case 4:
        if (worldmap_steer_flying(actor) == 1) {
            if (worldmap_current_path->kind == 2) {
                actor->state = 0x20;
                actor->unk68 = 0x30000;
                VEHICLE_SPOT(scratch, actor->position.vy);
                worldmap_effects_start_emitters(2, &scratch->spot, &scratch->rotation);
            } else {
                worldmap_loop_running = 0;
                worldmap_loop_result = 0;
            }
            break;
        }
        VEHICLE_TURN(actor->heading, 0x80);
        hit = worldmap_move_walking(&actor->position, &actor->motion, &scratch->hit, actor->turn << 12, worldmap_movement_mode);
        if (hit == 0) {
            actor->motion = scratch->hit;
            hit = worldmap_move_walking(&actor->position, &actor->motion, &scratch->hit, actor->turn << 12,
                                worldmap_movement_mode);
            if (hit == 0) {
                actor->motion.vz = 0;
                actor->motion.vx = 0;
            }
        }
        if (hit == 1) {
            scratch->hit.vy = worldmap_terrain_get_wave_height(scratch->hit.vx, scratch->hit.vz) + 0x18000;
            worldmap_find_cylinder_hit(&scratch->hit, 0x40, 0x20, &worldmap_cylinder_hit, &worldmap_cylinder_hit_actor);
            if (worldmap_cylinder_hit != 2) {
                actor->position = scratch->hit;
                if (actor->motion.vx | actor->motion.vz) {
                    worldmap_encounter_update_timers();
                }
            } else {
                actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) + 0x18000;
                actor->unk60 = 0;
            }
        } else {
            actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) + 0x18000;
        }
        VEHICLE_CAMERA();
        worldmap_path_select_region(&actor->position, 2);
        worldmap_select_actor_path_region();
        VEHICLE_ORIENT();
        if (--actor->wait > 0) {
            VEHICLE_SPOT(scratch, actor->position.vy);
            worldmap_effects_start_emitters(3, &scratch->spot, &scratch->rotation);
        } else {
            actor->wait = 0;
            worldmap_effects_stop_emitters(3);
        }
        if (actor->motion.vx | actor->motion.vz) {
            VEHICLE_SPOT(scratch, actor->position.vy);
            worldmap_effects_start_emitters(4, &scratch->spot, &scratch->rotation);
        } else {
            worldmap_effects_stop_emitters(4);
            actor->wait = 0;
        }
        actor->motion.vz = 0;
        actor->motion.vy = 0;
        actor->motion.vx = 0;
        if (worldmap_terrain_get_layer(&scratch->hit) == 3) {
            worldmap_effects_stop_emitters(3);
            worldmap_effects_stop_emitters(4);
            actor->state = 3;
        }
        worldmap_actor_cylinder_count = 0;
        break;
    case 8:
        actor->position.vy -= 0x8000;
        if (actor->position.vy <= -0x1E0000) {
            actor->position.vy = -0x1E0000;
            if (actor->unk4 == 0xB) {
                actor->unk64 = 0;
                actor->state = 2;
                actor->unk4 = 0;
                worldmap_replace_music(worldmap_flight_music, worldmap_flight_music_file);
            }
        }
        VEHICLE_CAMERA();
        break;
    case 0xC:
        actor->position.vy += 0x800;
        if (actor->position.vy >= actor->unk68) {
            actor->position.vy = actor->unk68;
            actor->state = 3;
            worldmap_objects[3].visible = 1;
            worldmap_objects[2].visible = 1;
            worldmap_objects[1].visible = 1;
        }
        VEHICLE_CAMERA();
        break;
    case 0x10:
        actor->position.vy += 0x8000;
        if (actor->position.vy >= actor->unk68) {
            actor->position.vy = actor->unk68;
            actor->state = 1;
            worldmap_replace_music(worldmap_music, worldmap_music_file);
            worldmap_actor_request(8, 0xA);
            worldmap_actor_request(9, 0xA);
            worldmap_actor_request(0xA, 0xA);
            worldmap_actor_request(4, 3);
            worldmap_actor_request(1, 3);
            game_data.inGear[0] = 1;
            if (game_data.party[1] != 0xFF) {
                worldmap_actor_request(5, 3);
                worldmap_actor_request(2, 3);
                game_data.inGear[1] = 1;
            }
            if (game_data.party[2] != 0xFF) {
                worldmap_actor_request(6, 3);
                worldmap_actor_request(3, 3);
                game_data.inGear[2] = 1;
            }
            VEHICLE_STOP();
            game_data_worldmap_return_flags &= 0x3FFF;
            worldmap_effects_stop_emitters(0);
            worldmap_effects_stop_emitters(1);
            worldmap_encounter_reset_timers();
        } else if (actor->position.vy > -0x200000 && actor->unk7C != 0) {
            actor->unk7C = 0;
            scratch->rotation.vx = actor->position.vx >> 12;
            scratch->rotation.vy = actor->unk68 >> 12;
            scratch->rotation.vz = actor->position.vz >> 12;
            worldmap_effects_start_emitters(worldmap_terrain_get_layer(&actor->position) == 3 ? 0x3E : 0x3D, &scratch->rotation,
                          NULL);
        }
        worldmap_camera_follow_target.target.vy = actor->position.vy;
        break;
    case 0x14:
        actor->position.vy -= 0x800;
        if (actor->unk68 >= actor->position.vy) {
            actor->position.vy = actor->unk68;
            actor->state = 1;
            worldmap_actor_request(8, 0xA);
            worldmap_actor_request(1, 3);
            worldmap_actor_request(4, 3);
            game_data.inGear[0] = 1;
            if (game_data.party[1] != 0xFF) {
                worldmap_actor_request(2, 3);
                worldmap_actor_request(5, 3);
                game_data.inGear[1] = 1;
            }
            if (game_data.party[2] != 0xFF) {
                worldmap_actor_request(3, 3);
                worldmap_actor_request(6, 3);
                game_data.inGear[2] = 1;
            }
            VEHICLE_STOP();
            game_data_worldmap_return_flags &= 0x3FFF;
            worldmap_objects[3].visible = 0;
            worldmap_objects[2].visible = 0;
            worldmap_objects[1].visible = 0;
            worldmap_encounter_reset_timers();
        }
        worldmap_camera_follow_target.target.vy = actor->position.vy;
        break;
    case 0x20:
        actor->position.vy += 0x2000;
        if (actor->position.vy >= actor->unk68) {
            actor->position.vy = actor->unk68;
            worldmap_effects_stop_emitters(2);
            worldmap_objects[3].visible = 1;
            worldmap_objects[2].visible = 1;
            worldmap_objects[1].visible = 1;
            worldmap_objects[0].visible = 1;
            actor->state++;
            scratch->target.vx = 0x4B40000;
            scratch->target.vz = 0x1C00000;
            scratch->target.vy = actor->unk68;
            worldmap_get_heading_to(&actor->position, &scratch->target, &actor->motion, &actor->heading);
            actor->u.step = scratch->target.vx >> 12;
            actor->unk54 = scratch->target.vz >> 12;
        }
        VEHICLE_CAMERA();
        break;
    case 0x21:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state++;
        }
        actor->position.vy = actor->unk68;
        VEHICLE_CAMERA();
        break;
    case 0x22:
        actor->state = 0x40;
        worldmap_loop_running = 0;
        worldmap_loop_result = 0;
        break;
    case 0x24:
        actor->position.vy = 0x30000;
        actor->state++;
        scratch->target.vx = 0x4937000;
        scratch->target.vz = 0x21E2000;
        scratch->target.vy = 0x30000;
        worldmap_get_heading_to(&actor->position, &scratch->target, &actor->motion, &actor->heading);
        actor->u.step = scratch->target.vx >> 12;
        actor->unk54 = scratch->target.vz >> 12;
        VEHICLE_CAMERA();
        worldmap_objects[3].visible = 1;
        worldmap_objects[2].visible = 1;
        worldmap_objects[1].visible = 1;
        worldmap_objects[0].visible = 1;
        scratch->rotation.vx = -(actor->unk70 >> 12);
        scratch->rotation.vy = actor->heading;
        scratch->rotation.vz = worldmap_camera_angle.vz;
        RotMatrixYXZ(&scratch->rotation, &worldmap_objects[0].matrix);
        RotMatrixYXZ(&scratch->rotation, &worldmap_objects[1].matrix);
        break;
    case 0x25:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->position.vy = 0x30000;
            actor->state++;
            actor->unk68 = worldmap_terrain_get_height(actor->position.vx, actor->position.vz) + 0x18000;
            VEHICLE_SPOT(scratch, actor->unk68);
            worldmap_effects_start_emitters(2, &scratch->spot, &scratch->rotation);
            worldmap_objects[0].visible = 0;
        }
        actor->position.vy = 0x30000;
        VEHICLE_CAMERA();
        break;
    case 0x26:
        actor->position.vy -= 0x2000;
        if (actor->unk68 >= actor->position.vy) {
            actor->position.vy = actor->unk68;
            worldmap_effects_stop_emitters(2);
            actor->state = 3;
        }
        VEHICLE_CAMERA();
        break;
    case 0x28:
        actor->state++;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz) + 0x18000;
        scratch->target.vx = worldmap_flying_vehicle_route_mode3[actor->unk7C].vx << 12;
        scratch->target.vz = worldmap_flying_vehicle_route_mode3[actor->unk7C].vz << 12;
        worldmap_get_heading_to(&actor->position, &scratch->target, &actor->motion, &actor->heading);
        actor->u.step = scratch->target.vx >> 12;
        actor->unk54 = scratch->target.vz >> 12;
        actor->unk78 = actor->heading;
        if (++actor->unk7C == 2) {
            worldmap_actor_request(9, 0xF);
        }
        if (actor->unk7C == 4) {
            actor->wait = 0x40;
            actor->state++;
            worldmap_actor_request(0, 0xD);
            worldmap_screen_fade_rate = 2;
            worldmap_screen_fade_step = 4;
        }
    case 0x29:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state--;
        }
    circle:
        VEHICLE_TURN(actor->unk78, 0x10);
        scratch->rotation.vx = -(actor->unk70 >> 12);
        scratch->rotation.vy = actor->heading;
        scratch->rotation.vz = worldmap_camera_angle.vz;
        VEHICLE_SPOT(scratch, actor->position.vy);
        worldmap_effects_start_emitters(1, &scratch->spot, &scratch->rotation);
        VEHICLE_ORIENT();
        VEHICLE_CAMERA();
        break;
    case 0x2A:
        if (--actor->wait < 0) {
            game_data.map = 0x50;
            game_data.entry[2] = 1;
            worldmap_loop_running = 0;
            worldmap_loop_result = 0;
            worldmap_next_scene_chosen = 1;
            game_data.entry[0] = worldmap_camera_angle.vy;
        }
        goto circle;
    case 0x30:
        actor->state++;
        actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) + 0x18000;
        scratch->target.vx = worldmap_flying_vehicle_route_mode4[actor->unk7C].vx << 12;
        scratch->target.vz = worldmap_flying_vehicle_route_mode4[actor->unk7C].vz << 12;
        worldmap_get_heading_to(&actor->position, &scratch->target, &actor->motion, &actor->heading);
        actor->u.step = scratch->target.vx >> 12;
        actor->unk54 = scratch->target.vz >> 12;
        actor->unk78 = actor->heading;
        if (++actor->unk7C == 2) {
            worldmap_actor_request(9, 0x10);
        }
        if (actor->unk7C == 7) {
            actor->wait = 0x40;
            actor->state++;
            worldmap_actor_request(0, 0xD);
            worldmap_screen_fade_rate = 2;
            worldmap_screen_fade_step = 4;
        }
    case 0x31:
    walk:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state--;
        }
        VEHICLE_TURN(actor->unk78, 0x10);
        actor->position.vy = worldmap_terrain_get_wave_height(actor->position.vx, actor->position.vz) + 0x18000;
        scratch->rotation.vx = -(actor->unk70 >> 12);
        scratch->rotation.vy = actor->heading;
        scratch->rotation.vz = worldmap_camera_angle.vz;
        VEHICLE_SPOT(scratch, actor->position.vy);
        worldmap_effects_start_emitters(4, &scratch->spot, &scratch->rotation);
        VEHICLE_ORIENT();
        VEHICLE_CAMERA();
        break;
    case 0x32:
        if (--actor->wait < 0) {
            game_data.map = 0x120;
            game_data.entry[2] = 6;
            worldmap_loop_running = 0;
            worldmap_loop_result = 0;
            worldmap_next_scene_chosen = 1;
            game_data.entry[0] = worldmap_camera_angle.vy;
        }
        goto walk;
    case 0x34:
        actor->state++;
        worldmap_actor_request(0xA, 0x11);
        actor->position.vy = -0x100000;
        actor->heading = 0x200;
        actor->motion.vx = gpu_get_sin(0x200);
        actor->motion.vz = -gpu_get_cos(actor->heading);
        actor->wait = 0x5A;
        actor->unk78 = 0;
        VEHICLE_ORIENT();
    case 0x35:
        if (--actor->wait < 0) {
            actor->wait = 0xB4;
            actor->state++;
            worldmap_actor_request(9, 0x11);
        }
        goto depart;
    case 0x36:
        if (--actor->wait < 0) {
            actor->wait = 0x78;
            actor->state++;
            sound_play_effect((sound_effect_bank->id << 16) | 0x38);
        }
    depart:
        actor->position.vx += actor->motion.vx * 12;
        actor->position.vz += actor->motion.vz * 12;
        worldmap_wrap_position(&actor->position);
        actor->position.vy += 0x1000;
        if ((u32)(actor->position.vy + 0xFFFF) < 0x1FFFF) {
            scratch->rotation.vx = -(actor->unk70 >> 12);
            scratch->rotation.vy = actor->heading;
            scratch->rotation.vz = worldmap_camera_angle.vz;
            VEHICLE_SPOT(scratch, actor->position.vy);
            worldmap_effects_start_emitters(0x3F, &scratch->spot, &scratch->rotation);
        } else {
            worldmap_effects_stop_emitters(0x3F);
        }
        VEHICLE_CAMERA();
        break;
    case 0x37:
        if (--actor->wait < 0) {
            actor->wait = 0x5A;
            actor->state++;
            worldmap_effects_stop_emitters(0x2B);
        }
        actor->position.vx += actor->motion.vx * 8;
        actor->position.vz += actor->motion.vz * 8;
        worldmap_wrap_position(&actor->position);
        actor->position.vy += 0x1000;
        if (actor->state == 0x38) {
            worldmap_effects_stop_emitters(0x2B);
        } else {
            scratch->rotation.vx = -(actor->unk70 >> 12);
            scratch->rotation.vy = actor->heading;
            scratch->rotation.vz = worldmap_camera_angle.vz;
            VEHICLE_SPOT(scratch, 0);
            worldmap_effects_start_emitters(0x2B, &scratch->spot, &scratch->rotation);
        }
        VEHICLE_CAMERA();
        break;
    case 0x38:
        if (--actor->wait < 0) {
            actor->wait = 0x80;
            actor->state++;
            worldmap_actor_request(0, 0xD);
            worldmap_screen_fade_rate = 2;
            worldmap_screen_fade_step = 4;
        }
        goto glide;
    case 0x39:
        if (--actor->wait < 0) {
            game_data.map = 0x1F0;
            worldmap_loop_running = 0;
            worldmap_loop_result = 0;
            game_data.entry[2] = 0;
            worldmap_next_scene_chosen = 1;
            game_data.entry[0] = worldmap_camera_angle.vy;
        }
    glide:
        actor->position.vx += actor->motion.vx * 8;
        actor->position.vz += actor->motion.vz * 8;
        VEHICLE_CAMERA();
        break;
    case 0x40: /* Idle state at the end of the 0x41-entry dispatch. */
        break;
    }
    worldmap_objects[0].position.vx = worldmap_objects[1].position.vx = actor->position.vx >> 12;
    worldmap_objects[0].position.vy = worldmap_objects[1].position.vy = actor->position.vy >> 12;
    worldmap_objects[0].position.vz = worldmap_objects[1].position.vz = actor->position.vz >> 12;
    worldmap_save_flying_vehicle_position(&actor->position);
    game_data.worldmap.vehicle_heading = actor->heading;
    switch (actor->state) {
    case 2:
    case 8:
    case 0x10:
        worldmap_footprints_add(2, &actor->position);
        break;
    }
    return result;
}

/* 800906E0: Restore the actor from scene objects 2 and 3 after linking them to 0. */
s32 worldmap_flying_vehicle_rotors_start(s32 index) {
    WorldmapActor *actor;

    worldmap_objects_link(0, 2);
    worldmap_objects_link(0, 3);
    actor = &worldmap_actor_slots[index];
    actor->state = 0;
    actor->position = worldmap_objects[2].position;
    actor->motion = worldmap_objects[3].position;
    actor->u.step = 0;
    actor->unk54 = 0x40;
    switch (worldmap_movement_mode) {
    case 4:
    case 5:
    case 6:
    case 7:
        actor->state = 3;
        actor->unk5C = 0x80;
        actor->unk58 = 0x80;
        break;
    }
    return 1;
}

/* 800907C4: Link scene objects 2 and 3 to object 0. */
s32 worldmap_flying_vehicle_rotors_resume(void) {
    worldmap_objects_link(0, 2);
    worldmap_objects_link(0, 3);
    return 1;
}

/* 800907F4: Spin the vehicle's two rotors (scene objects 2 and 3): command 9 spins
 * up, 10 spins down; the second rotor follows the first. */
s32 worldmap_flying_vehicle_rotors_update(s32 index) {
    ActorScratch *scratch;
    WorldmapActor *actor;
    SceneObject *rotor;
    SceneObject *tail;

    scratch = (ActorScratch *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    rotor = &worldmap_objects[2];
    tail = &worldmap_objects[3];
    if (actor->unk4 == 9) {
        actor->unk4 = 0;
        actor->state = 1;
    } else if (actor->unk4 == 10) {
        actor->unk4 = 0;
        actor->state = 2;
    }
    switch (actor->state) {
    case 0:
        actor->unk58 = 0;
        actor->unk5C = 0;
        break;
    case 1:
        actor->unk58 += 4;
        if (actor->unk58 > 0x10) {
            actor->unk5C += 4;
        }
        if (actor->unk58 >= 0x80) {
            actor->unk58 = 0x80;
        }
        if (actor->unk5C >= 0x80) {
            actor->unk5C = 0x80;
        }
        if ((actor->unk58 >= 0x80) & (actor->unk5C >= 0x80)) {
            actor->state = 3;
        }
        break;
    case 2:
        actor->unk58 -= 4;
        if (actor->unk58 < 0x70) {
            actor->unk5C -= 4;
        }
        if (actor->unk58 < 0) {
            actor->unk58 = 0;
        }
        if (actor->unk5C < 0) {
            actor->unk5C = 0;
        }
        if ((actor->unk58 == 0) & (actor->unk5C == 0)) {
            actor->state = 0;
        }
        break;
    }
    actor->u.step += actor->unk58;
    actor->unk54 -= actor->unk5C;
    scratch->angle.vx = 0;
    scratch->position.vx = 0;
    scratch->position.vy = actor->u.step;
    scratch->angle.vy = actor->unk54;
    scratch->position.vz = rotor->angle.vz;
    scratch->angle.vz = tail->angle.vz;
    RotMatrix(&scratch->position, &rotor->matrix);
    RotMatrix(&scratch->angle, &tail->matrix);
    return 1;
}

/* 80090A18: Track the two-button combination and latch its press edge. */
void worldmap_latch_button_combo(void) {
    if ((worldmap_pad_port0_held & 1) && (worldmap_pad_port0_held & 2)) {
        worldmap_button_combo_held = 1;
    } else {
        worldmap_button_combo_held = 0;
    }
    worldmap_button_combo_pressed = (worldmap_button_combo_held ^ worldmap_button_combo_held_last) & worldmap_button_combo_held;
    worldmap_button_combo_held_last = worldmap_button_combo_held;
}
