/* World map unit 8008C364-8008E190 (rodata 800707AC-800709F8): the party's
 * vehicles: placing them, the player's vehicle and the members' parked
 * ones, the saved vehicle position, the path table choice and a probe for a
 * clear heading.
 *
 * worldmap_third_member_start's 13-entry table ends at 800707ac and worldmap_place_vehicle's
 * follows at once, 4 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after worldmap_third_member_start, at or before
 * worldmap_place_vehicle. The unit has no data. */
#include "common.h"
#include "psyq/libgte.h"
#include "resident/gamedata.h"
#include "resident/sprite.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "party.h"
#include "screen.h"
#include "terrain.h"

/* 8008C364: Place a party member's vehicle actor: parked at its spot, with the
 * player when riding, or hidden (3) when the member has no vehicle. */
s32 worldmap_place_vehicle(WorldmapActor *actor, s32 member) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    s32 result;
    u32 state;

    result = 1;
    state = game_data.party[member] != 0xFF;
    if ((game_data_vehicle_spots[member].flags & 0x3FFF) >= 0x400) {
        state |= 2;
    }
    if (game_data.inGear[member] == 1) {
        state |= 4;
    }
    switch (state) {
    case 0:
    case 2:
    case 4:
    case 6:
    hidden:
        result = 3;
        actor->unk24 = 1;
        actor->position.vy = 0;
        actor->position.vz = 0;
        actor->position.vx = 0;
        break;
    case 1:
        if (game_data.characters[game_data.party[member]].gearId == 0xFF) {
            goto hidden;
        }
        worldmap_create_gear_sprite(actor, member);
        actor->unk24 = 1;
        actor->position.vy = 0;
        actor->position.vz = 0;
        actor->position.vx = 0;
        break;
    case 3:
        worldmap_create_gear_sprite(actor, member);
        actor->unk24 = 0;
        actor->position.vx = game_data_vehicle_spots[member].x << 12;
        actor->position.vz = game_data_vehicle_spots[member].z << 12;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        break;
    case 5:
    case 7:
        worldmap_create_gear_sprite(actor, member);
        actor->unk24 = 0;
        actor->position.vx = worldmap_player_position.vx;
        actor->position.vz = worldmap_player_position.vz;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        game_data_vehicle_spots[member].flags = 0x400;
        break;
    }
    return result;
}

/* 8008C530: Start party vehicle 0: place it, and while its member rides (movement
 * modes 1-3) put it under the player, reset the saved camera target and the
 * trail; modes 4-7 mark it boarded. Save its spot and heading. */
s32 worldmap_player_vehicle_start(s32 index) {
    WorldmapActor *actor;
    TrailPoint *point;
    s32 result;
    s32 i;

    actor = &worldmap_actor_slots[index];
    result = worldmap_place_vehicle(actor, 0);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = game_data.worldmap.unk5A;
    actor->turn = 0xC;
    actor->unk5C = actor->heading;
    switch (worldmap_movement_mode) {
    case 1 ... 3:
        if (game_data.inGear[0] == 1) {
            actor->state = 1;
            actor->position.vx = worldmap_player_position.vx;
            actor->position.vz = worldmap_player_position.vz;
            actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
            actor->heading = worldmap_player_heading;
            worldmap_camera_follow_target.target = actor->position;
            worldmap_trail_index = 0;
            worldmap_camera_follow_heading = actor->heading;
            for (i = 0, point = worldmap_trail_points; i < 0x20; i++, point++) {
                point->position = actor->position;
                point->heading = actor->heading;
            }
        }
        break;
    case 4 ... 7:
        actor->state = 2;
        actor->unk24 = 1;
        break;
    }
    VEHICLE_SPOTS[0].x = actor->position.vx >> 12;
    VEHICLE_SPOTS[0].z = actor->position.vz >> 12;
    game_data.worldmap.unk5A = actor->heading;
    return result;
}

/* 8008C6EC: Update a kind-0 actor; flag it while riding a vehicle. */
s32 worldmap_player_vehicle_resume(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = worldmap_place_vehicle(actor, 0);
    switch (worldmap_movement_mode) {
    case 1:
    case 2:
    case 3:
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        actor->unk24 = 1;
        break;
    }
    return result;
}

/* 8008C75C: Start the parked vehicle 0: its model at the saved spot and heading. */
s32 worldmap_player_vehicle_start_parked(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_gear_models[0], 0x100, 0x1FD, 0x140, 0x140, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x2000);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    actor->position.vx = VEHICLE_SPOTS[0].x << 12;
    actor->position.vz = VEHICLE_SPOTS[0].z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    actor->unk24 = 1;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->heading = game_data_parked_vehicle0_heading;
    return 1;
}

/* 8008C844: Update the player's vehicle (actor slot 4): drive it by the pad, record
 * the trail, board and park the other party vehicles on command, and save
 * its spot and heading. */
s32 worldmap_player_vehicle_update(s32 index) {
    MATRIX unused; /* unused in the original; reserves 32 bytes */
    WorldmapActor *actor;
    ActorScratch *scratch;
    TrailPoint *point;
    s32 value;
    u16 trail;
    s32 result = 1;

    scratch = (ActorScratch *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    switch (actor->unk4) {
    case 4:
        actor->state = 0x10;
        actor->unk4 = 0;
        actor->unk58 = result;
        game_data.inGear[0] = 1;
        break;
    case 7:
        actor->unk4 = 0;
        if (worldmap_party_count == ++actor->unk58) {
            actor->state = 1;
            worldmap_movement_mode = 2;
            worldmap_actor_cylinder_count = 0;
        }
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 0x30;
        break;
    case 8:
        actor->unk4 = 0;
        actor->state = 0x18;
        break;
    }
    switch (actor->state) {
    case 0:
    case 1:
        if (game_data.inGear[0] == 1) {
            switch (worldmap_steer_vehicle(actor)) {
            case 1:
                worldmap_loop_running = 0;
                worldmap_loop_result = 0;
                break;
            case 3:
                actor->state = 8;
                break;
            case 4:
                if (worldmap_terrain_can_mode_enter_layer(1, worldmap_terrain_get_layer(&actor->position)) != 0) {
                    actor->state = 0x20;
                }
                break;
            default:
                if ((actor->motion.vx == 0) & (actor->motion.vy == 0) & (actor->motion.vz == 0)) {
                    if (SPRITE_ANIMATION(actor->handle) != 0) {
                        sprite_start_animation(actor->handle, 0);
                        worldmap_effects_stop_emitters(0x2C);
                    }
                } else {
                    if (SPRITE_ANIMATION(actor->handle) != 1) {
                        sprite_start_animation(actor->handle, 1);
                    }
                    worldmap_actor_emit_on_layer3(0x2C, actor, scratch);
                }
                value = worldmap_move_walking(&actor->position, &actor->motion, &scratch->probe, actor->turn << 12,
                                    worldmap_movement_mode);
                if (value == 0) {
                    actor->motion = scratch->probe;
                    value = worldmap_move_walking(&actor->position, &actor->motion, &scratch->probe, actor->turn << 12,
                                        worldmap_movement_mode);
                    if (value == 0) {
                        actor->motion.vz = 0;
                        actor->motion.vx = 0;
                    }
                }
                if (value == 1) {
                    worldmap_find_cylinder_hit(&scratch->probe, 0x18, 0x30, &worldmap_cylinder_hit, &worldmap_cylinder_hit_actor);
                    value = worldmap_cylinder_hit;
                    if (worldmap_cylinder_hit_actor == 7) {
                        value += 3;
                    }
                    if ((u16)worldmap_cylinder_hit_standable[value] != 0) {
                        actor->position = scratch->probe;
                        if (actor->motion.vx | actor->motion.vz) {
                            trail = (worldmap_trail_index + 1) & 0x1F;
                            point = &worldmap_trail_points[trail];
                            worldmap_trail_index = trail;
                            point->position = actor->position;
                            point->heading = actor->heading;
                            worldmap_encounter_update_timers();
                        }
                    }
                } else {
                    worldmap_find_cylinder_hit(&actor->position, 0x18, 0x30, &worldmap_cylinder_hit, &worldmap_cylinder_hit_actor);
                }
                worldmap_path_select_region(&actor->position, 1);
                actor->motion.vz = 0;
                actor->motion.vy = 0;
                actor->motion.vx = 0;
                worldmap_camera_follow_target.target = actor->position;
                worldmap_camera_follow_heading = actor->heading;
                break;
            }
            worldmap_actor_cylinder_count = 0;
        } else if (SPRITE_ANIMATION(actor->handle) != 3) {
            sprite_start_animation(actor->handle, 3);
            worldmap_effects_stop_emitters(0x2C);
        }
        break;
    case 2:
        actor->position.vx = worldmap_actor_slots[7].position.vx;
        actor->position.vz = worldmap_actor_slots[7].position.vz;
        actor->heading = worldmap_actor_slots[7].heading;
        break;
    case 8:
        if (game_data.party[1] != 0xFF) {
            if (game_data.inGear[1] == 1) {
                if (worldmap_actor_request(5, 1) != 0) {
                    actor[1].unk6 = worldmap_cylinder_hit_actor;
                    actor->state++;
                }
            } else {
                worldmap_actor_request(2, 1);
                worldmap_actor_slots[2].unk6 = worldmap_cylinder_hit_actor;
                worldmap_actor_request(5, 8);
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 9:
        if (game_data.party[2] != 0xFF) {
            if (game_data.inGear[2] == 1) {
                if (worldmap_actor_request(6, 1) != 0) {
                    actor[2].unk6 = worldmap_cylinder_hit_actor;
                    actor->state++;
                }
            } else {
                worldmap_actor_request(3, 1);
                worldmap_actor_slots[3].unk6 = worldmap_cylinder_hit_actor;
                worldmap_actor_request(6, 8);
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 10:
        worldmap_get_heading_to(&actor->position, &worldmap_actor_slots[worldmap_cylinder_hit_actor].position, &actor->motion, &actor->heading);
        actor->u.step = worldmap_actor_slots[worldmap_cylinder_hit_actor].position.vx >> 12;
        actor->unk54 = worldmap_actor_slots[worldmap_cylinder_hit_actor].position.vz >> 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
    case 11:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state++;
        }
        worldmap_camera_follow_target.target = actor->position;
        worldmap_camera_follow_heading = actor->heading;
        worldmap_actor_emit_on_layer3(0x2C, actor, scratch);
        break;
    case 12:
        if (worldmap_actor_request(worldmap_cylinder_hit_actor, 4) != 0) {
            actor->unk24 = 1;
            actor->state = 2;
            worldmap_effects_stop_emitters(0x2C);
        }
        break;
    case 0x10:
        switch (worldmap_party_count) {
        case 1:
            actor->state = 1;
            break;
        case 2:
            if (game_data.inGear[1] != 0) {
                actor->state++;
            }
            break;
        case 3:
            if (game_data.inGear[1] & game_data.inGear[2]) {
                actor->state++;
            }
            break;
        }
        break;
    case 0x11:
        if (game_data.party[1] == 0xFF || worldmap_actor_request(5, 5) != 0) {
            actor->state++;
        }
        break;
    case 0x12:
        if (game_data.party[2] == 0xFF || worldmap_actor_request(6, 5) != 0) {
            actor->state = 0x40;
        }
        break;
    case 0x18:
        if (game_data.party[index - 4] == 7) {
            actor->wait = 1;
            actor->state = 0x1A;
        } else {
            scratch->position.vx = actor->position.vx >> 12;
            scratch->position.vy = actor->position.vy >> 12;
            scratch->position.vz = actor->position.vz >> 12;
            worldmap_effects_start_emitters(5, &scratch->position, NULL);
            actor->wait = 8;
            actor->state++;
        }
        break;
    case 0x19:
        if (--actor->wait <= 0) {
            actor->unk24 = 1;
            actor->wait = 0x10;
            actor->state++;
        }
        break;
    case 0x1A:
        if (--actor->wait <= 0) {
            sprite_start_animation(actor->handle, 3);
            actor->unk24 = 1;
            actor->state = 2;
        }
        break;
    case 0x20:
        sprite_start_animation(actor->handle, 0);
        if (game_data.party[1] != 0xFF) {
            worldmap_actor_request(5, 2);
        }
        if (game_data.party[2] != 0xFF) {
            worldmap_actor_request(6, 2);
        }
        actor->motion.vz = 0;
        actor->motion.vy = 0;
        actor->motion.vx = 0;
        actor->state++;
    case 0x21:
        if (worldmap_actor_request(1, 2) != 0) {
            worldmap_actor_slots[1].unk6 = index;
            actor->state++;
        }
        break;
    case 0x22:
        actor->state = 0;
        break;
    case 0x30:
        worldmap_load_flying_vehicle_position(&actor->position);
        actor->unk24 = 0;
        actor->heading = worldmap_actor_slots[7].unk78;
        scratch->work.vx = actor->position.vx + gpu_get_sin(actor->heading) * 0x60;
        scratch->work.vz = actor->position.vz + -gpu_get_cos(actor->heading) * 0x60;
        worldmap_get_heading_to(&actor->position, &scratch->work, &actor->motion, &actor->heading);
        actor->u.step = scratch->work.vx >> 12;
        actor->unk54 = scratch->work.vz >> 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
    case 0x31:
        if (worldmap_actor_step_to_target(actor) == 3) {
            sprite_start_animation(actor->handle, 0);
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            actor->state++;
        }
        worldmap_camera_follow_target.target = actor->position;
        worldmap_camera_follow_heading = actor->heading;
        break;
    case 0x32:
        point = worldmap_trail_points;
        worldmap_trail_index = 0;
        scratch->start = actor->position;
        scratch->position.vx = actor->heading;
        for (value = 0x1F; value != -1; value--, point++) {
            point->position = scratch->start;
            point->heading = (u16)scratch->position.vx;
        }
        actor->state = 1;
        worldmap_movement_mode = 2;
        break;
    case 0x40:
        break;
    }
    if (actor->state != 2 && (worldmap_movement_mode == 2 || game_data.party[0] != 7)) {
        worldmap_footprints_add(1, &actor->position);
    }
    VEHICLE_SPOTS[0].x = actor->position.vx >> 12;
    VEHICLE_SPOTS[0].z = actor->position.vz >> 12;
    game_data.worldmap.unk5A = actor->heading;
    return result;
}

/* 8008D3F0: Start party vehicle 1: place it, and while its member rides (movement
 * modes 1-3) put it under the player; modes 4-7 mark it boarded. Save its
 * spot and heading. */
s32 worldmap_second_vehicle_start(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = worldmap_place_vehicle(actor, 1);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = game_data.worldmap.unk5C;
    actor->turn = 0xC;
    actor->unk58 = 0xF;
    actor->unk5C = actor->heading;
    if (worldmap_movement_mode > 0) {
        if (worldmap_movement_mode < 4) {
            if (game_data.inGear[1] == 1) {
                actor->state = 1;
                actor->position.vx = worldmap_player_position.vx;
                actor->position.vz = worldmap_player_position.vz;
                actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
                actor->heading = worldmap_player_heading;
            }
        } else if (worldmap_movement_mode < 8) {
            actor->state = 2;
            actor->unk24 = 1;
        }
    }
    VEHICLE_SPOTS[1].x = actor->position.vx >> 12;
    VEHICLE_SPOTS[1].z = actor->position.vz >> 12;
    game_data.worldmap.unk5C = actor->heading;
    return result;
}

/* 8008D520: Update a kind-1 actor; flag it while riding a vehicle. */
s32 worldmap_second_vehicle_resume(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = worldmap_place_vehicle(actor, 1);
    switch (worldmap_movement_mode) {
    case 1:
    case 2:
    case 3:
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        actor->unk24 = 1;
        break;
    }
    return result;
}

/* 8008D590: Start the parked vehicle 1: its model at the saved spot and heading. */
s32 worldmap_second_vehicle_start_parked(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_gear_models[1], 0x100, 0x1FC, 0x160, 0x140, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x2000);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    actor->position.vx = VEHICLE_SPOTS[1].x << 12;
    actor->position.vz = VEHICLE_SPOTS[1].z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    actor->unk24 = 1;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->heading = game_data_parked_vehicle1_heading;
    return 1;
}

/* 8008D678: Update party vehicle actor `index` (slots 4-6): take commands (1 go to an
 * actor, 2 park, 3 leave the flying vehicle, 5 go to the player, 8 board),
 * follow the player's trail while ridden, and save its spot and heading. */
s32 worldmap_follower_vehicle_update(s32 index) {
    WorldmapActor *actor;
    WorldmapActor *target;
    ActorScratch *scratch;
    TrailPoint *point;
    s32 flag;
    s32 slot;

    actor = &worldmap_actor_slots[index];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->state = 8;
        actor->unk4 = 0;
        target = &worldmap_actor_slots[actor->unk6];
        break;
    case 4:
        actor->state = 0x40;
        actor->unk4 = 0;
        game_data.inGear[index - 4] = 1;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 0x10;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0x20;
        break;
    case 8:
        actor->unk4 = 0;
        actor->state = 0x18;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 0x30;
        break;
    }
    switch (actor->state) {
    case 0:
    case 1:
        flag = game_data.inGear[index - 4];
        if (flag == 1) {
            point = &worldmap_trail_points[(worldmap_trail_index - actor->unk58) & 0x1F];
            if ((actor->position.vx == point->position.vx) & (actor->position.vy == point->position.vy) &
                (actor->position.vz == point->position.vz)) {
                if (SPRITE_ANIMATION(actor->handle) != 0) {
                    sprite_start_animation(actor->handle, 0);
                    worldmap_effects_stop_emitters(index + 0x28);
                }
            } else {
                if (SPRITE_ANIMATION(actor->handle) != flag) {
                    sprite_start_animation(actor->handle, 1);
                }
                worldmap_actor_emit_on_layer3(index + 0x28, actor, scratch);
            }
            actor->position.vx = point->position.vx;
            actor->position.vy = point->position.vy;
            actor->position.vz = point->position.vz;
            actor->heading = point->heading;
        } else if (SPRITE_ANIMATION(actor->handle) != 3) {
            sprite_start_animation(actor->handle, 3);
            worldmap_effects_stop_emitters(index + 0x28);
        }
        break;
    case 2:
        actor->position.vx = worldmap_actor_slots[7].position.vx;
        actor->position.vz = worldmap_actor_slots[7].position.vz;
        actor->heading = worldmap_actor_slots[7].heading;
        break;
    case 8:
        worldmap_get_heading_to(&actor->position, &target->position, &actor->motion, &actor->heading);
        actor->u.step = target->position.vx >> 12;
        actor->unk54 = target->position.vz >> 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
    case 9:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state++;
        }
        worldmap_actor_emit_on_layer3(index + 0x28, actor, scratch);
        break;
    case 10:
        if (worldmap_actor_request(actor->unk6, 4) != 0) {
            actor->unk24 = 1;
            actor->state = 2;
            worldmap_effects_stop_emitters(index + 0x28);
        }
        break;
    case 0x10:
        actor->u.step = worldmap_actor_slots[4].position.vx;
        actor->unk54 = worldmap_actor_slots[4].position.vz;
        scratch->work.vx = actor->u.step - actor->position.vx;
        scratch->work.vz = actor->unk54 - actor->position.vz;
        actor->heading = (ratan2(scratch->work.vz, scratch->work.vx) + 0x400) & 0xFFF;
        actor->motion.vx = gpu_get_sin(actor->heading);
        actor->motion.vz = -gpu_get_cos(actor->heading);
        actor->u.step >>= 12;
        actor->unk54 >>= 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
    case 0x11:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state++;
        }
        break;
    case 0x12:
        if (worldmap_actor_request(4, 7) != 0) {
            actor->state = 1;
        }
        break;
    case 0x18:
        if (game_data.party[index - 4] == 7) {
            actor->wait = 1;
            actor->state = 0x1A;
        } else {
            scratch->position.vx = actor->position.vx >> 12;
            scratch->position.vy = actor->position.vy >> 12;
            scratch->position.vz = actor->position.vz >> 12;
            worldmap_effects_start_emitters(index + 1, &scratch->position, NULL);
            actor->wait = 8;
            actor->state++;
        }
        break;
    case 0x19:
        if (--actor->wait <= 0) {
            actor->unk24 = 1;
            actor->wait = 0x10;
            actor->state++;
        }
        break;
    case 0x1A:
        if (--actor->wait <= 0) {
            sprite_start_animation(actor->handle, 3);
            actor->unk24 = 1;
            actor->state = 2;
        }
        break;
    case 0x20:
        sprite_start_animation(actor->handle, 0);
        actor->motion.vz = 0;
        actor->motion.vy = 0;
        actor->motion.vx = 0;
        actor->state++;
        break;
    case 0x21:
        if (worldmap_actor_request(index - 3, 2) != 0) {
            worldmap_actor_slots[index - 3].unk6 = index;
            actor->state++;
        }
        break;
    case 0x22:
        actor->state = 0;
        break;
    case 0x30:
        worldmap_load_flying_vehicle_position(&actor->position);
        actor->unk24 = 0;
        actor->heading = worldmap_actor_slots[7].unk78;
        scratch->work.vx = actor->position.vx + gpu_get_sin(actor->heading) * 0x60;
        scratch->work.vz = actor->position.vz + -gpu_get_cos(actor->heading) * 0x60;
        worldmap_get_heading_to(&actor->position, &scratch->work, &actor->motion, &actor->heading);
        actor->u.step = scratch->work.vx >> 12;
        actor->unk54 = scratch->work.vz >> 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
        break;
    case 0x31:
        if (worldmap_actor_step_to_target(actor) == 3) {
            sprite_start_animation(actor->handle, 0);
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            actor->state++;
        }
        break;
    case 0x32:
        actor->state = 1;
        break;
    case 0x40:
        break;
    }
    if (actor->state != 2 && (worldmap_movement_mode == 2 || game_data.party[index - 4] != 7)) {
        worldmap_footprints_add(1, &actor->position);
    }
    /* Save the spot (the vehicle spot's x and z) as offsets from the return
     * state, the base the original addresses both from (as members they
     * compile differently), and the heading (worldmap.unk5A on). */
    slot = index - 4;
    STATE_U16(0x13C + slot * 6) = actor->position.vx >> 12;
    STATE_U16(0x13E + slot * 6) = actor->position.vz >> 12;
    (&game_data.worldmap.unk5A)[index - 4] = actor->heading;
    return 1;
}

/* 8008DD6C: Start party vehicle 2: place it, and while its member rides (movement
 * modes 1-3) put it under the player; modes 4-7 mark it boarded. Save its
 * spot and heading. */
s32 worldmap_third_vehicle_start(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = worldmap_place_vehicle(actor, 2);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = game_data.worldmap.unk5E;
    actor->turn = 0xC;
    actor->unk58 = 0x1F;
    actor->unk5C = actor->heading;
    if (worldmap_movement_mode > 0) {
        if (worldmap_movement_mode < 4) {
            if (game_data.inGear[2] == 1) {
                actor->state = 1;
                actor->position.vx = worldmap_player_position.vx;
                actor->position.vz = worldmap_player_position.vz;
                actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
                actor->heading = worldmap_player_heading;
            }
        } else if (worldmap_movement_mode < 8) {
            actor->state = 2;
            actor->unk24 = 1;
        }
    }
    VEHICLE_SPOTS[2].x = actor->position.vx >> 12;
    VEHICLE_SPOTS[2].z = actor->position.vz >> 12;
    game_data.worldmap.unk5E = actor->heading;
    return result;
}

/* 8008DE9C: Update a kind-2 actor; flag it while riding a vehicle. */
s32 worldmap_third_vehicle_resume(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = worldmap_place_vehicle(actor, 2);
    switch (worldmap_movement_mode) {
    case 1:
    case 2:
    case 3:
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        actor->unk24 = 1;
        break;
    }
    return result;
}

/* 8008DF0C: Start the parked vehicle 2: its model at the saved spot and heading. */
s32 worldmap_third_vehicle_start_parked(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_gear_models[2], 0x100, 0x1FB, 0x280, 0x100, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x2000);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    actor->position.vx = VEHICLE_SPOTS[2].x << 12;
    actor->position.vz = VEHICLE_SPOTS[2].z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    actor->unk24 = 1;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->heading = game_data_parked_vehicle2_heading;
    return 1;
}

/* 8008DFF4: Restore the saved vehicle position (world units to 20.12). */
void worldmap_load_flying_vehicle_position(VECTOR *position) {
    position->vx = (s16)game_data.worldmap.unk60 << 12;
    position->vy = (s16)game_data.worldmap.unk62 << 12;
    position->vz = (s16)game_data.worldmap.unk64 << 12;
}

/* 8008E034: Save the vehicle position in world units. */
void worldmap_save_flying_vehicle_position(VECTOR *position) {
    game_data.worldmap.unk60 = position->vx >> 12;
    game_data.worldmap.unk62 = position->vy >> 12;
    game_data.worldmap.unk64 = position->vz >> 12;
}

/* 8008E078: Select the path table of scenes 15 and 16. The link is read unsigned here
 * (lhu at 8008e0b4), signed by worldmap_path_select_region. */
void worldmap_select_actor_path_region(void) {
    if (worldmap_cylinder_hit != 0) {
        switch (worldmap_cylinder_hit_actor) {
        case 15:
            worldmap_current_path = &worldmap_fixed_path_regions[0];
            worldmap_path_name_id = (u16)worldmap_fixed_path_regions[0].link;
            break;
        case 16:
            worldmap_current_path = &worldmap_fixed_path_regions[1];
            worldmap_path_name_id = (u16)worldmap_fixed_path_regions[1].link;
            break;
        }
    }
}

/* 8008E0F0: First of sixteen headings in which a probe from the position hits; -1 if
 * none does. */
s32 worldmap_find_clear_heading(VECTOR *position, s32 unused, s32 range) {
    VECTOR hit;
    VECTOR direction;
    s32 heading;

    for (heading = 0; heading < 0x1000; heading += 0x100) {
        direction.vx = gpu_get_sin(heading);
        direction.vz = -gpu_get_cos(heading);
        if (worldmap_move_walking(position, &direction, &hit, range, 2) == 1) {
            return heading;
        }
    }
    return -1;
}
