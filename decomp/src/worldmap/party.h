#ifndef WORLDMAP_PARTY_H
#define WORLDMAP_PARTY_H

/* The party on the world map: the leader on foot and the two followers
 * (worldmap_objects_effects_party), the members' parked vehicles and the player's
 * (worldmap_vehicles), the flying vehicle (worldmap_flying_vehicle), the pad
 * steering of each movement mode (worldmap_steering_camera_terrain) and the queue of actor
 * placements the movers test against. */

#include "worldmap.h"

/* The party's model files: character and gear models per party slot. */
extern void *worldmap_character_models[3]; /* character model buffers */
extern void *worldmap_gear_models[3]; /* gear model buffers */
extern s32 worldmap_party_count;      /* loaded party members */

/* A parked vehicle's spot (world units), per party slot: event variables
 * 21-29 of the game data. */
typedef struct {
    u16 flags; /* 0x3FFF part >= 0x400: parked on the map */
    u16 x;
    u16 z;
} VehicleSpot;

#define VEHICLE_SPOTS ((VehicleSpot *)&game_data.vars[21])

/* A halfword of the game data at an offset from the world map's return
 * state (game_data.worldmap). */
#define STATE_U16(offset) (*(u16 *)((u8 *)&game_data.worldmap + (offset)))

/* Game data words that some movers address by names of their own: as members
 * of game_data they compile differently there. The parked vehicles'
 * headings (worldmap.unk5A-unk5E; worldmap_player_vehicle_start_parked, worldmap_second_vehicle_start_parked, worldmap_third_vehicle_start_parked)
 * and spots (worldmap_place_vehicle), the flying vehicle's heading where it starts
 * (worldmap.vehicle_heading; worldmap_flying_vehicle_start) and the return flags it masks
 * on its two landings (worldmap.flags; worldmap_flying_vehicle_update), and the spots
 * seen four bytes early, so that the followers' actor slots 1-3 index the x
 * and z of party slots 0-2 (worldmap_follower_update). */
extern u16 game_data_parked_vehicle0_heading, game_data_parked_vehicle1_heading, game_data_parked_vehicle2_heading;
extern VehicleSpot game_data_vehicle_spots[3];
extern u16 game_data_flying_vehicle_heading, game_data_worldmap_return_flags;

typedef struct {
    u16 x;
    u16 z;
    u16 flags;
} PartySpot;

extern PartySpot game_data_vehicle_spots_by_follower[];

/* Queued actor placement (0x18 bytes, ring of 32). */
typedef struct PlaceRequest {
    s16 actor;
    s16 pad2;
    s32 px, py, pz;
    s32 z;
    s16 x;
    s16 pad16;
} PlaceRequest;

extern PlaceRequest worldmap_actor_cylinders[32];
extern s16 worldmap_actor_cylinder_count;
extern u8 worldmap_cylinder_hit, worldmap_cylinder_hit_actor; /* the last placement probe: hit, actor */

void worldmap_queue_actor_cylinder(s32 index, VECTOR *position, s32 x, s32 z);
void worldmap_find_cylinder_hit(VECTOR *position, s32 radius, s32 height, u8 *hit, u8 *actor);

extern s16 worldmap_cylinder_hit_standable[]; /* per landing kind: may stand there */

s32 worldmap_actor_step_to_target(WorldmapActor *actor); /* step towards the target */
void worldmap_actor_emit_on_layer3(s32 effect, WorldmapActor *actor, ActorScratch *scratch); /* on terrain type 3 */
void worldmap_create_gear_sprite(WorldmapActor *actor, s32 member); /* create a member's gear sprite */
s32 worldmap_place_vehicle(WorldmapActor *actor, s32 member); /* place a member's vehicle */
void worldmap_load_flying_vehicle_position(VECTOR *position); /* restore the saved vehicle position */
void worldmap_save_flying_vehicle_position(VECTOR *position); /* save it */
void worldmap_select_actor_path_region(void); /* the path table of scenes 15 and 16 */
s32 worldmap_find_clear_heading(VECTOR *position, s32 unused, s32 range); /* the first heading a probe hits in */
void worldmap_latch_button_combo(void); /* latch the two-button combination (worldmap_button_combo_pressed) */

/* Pad steering per movement mode: on foot, vehicle, flying and free
 * flight. */
s32 worldmap_steer_on_foot(WorldmapActor *actor);
s32 worldmap_steer_vehicle(WorldmapActor *actor);
s32 worldmap_steer_flying(WorldmapActor *actor);
s32 worldmap_steer_free_flight(WorldmapActor *actor);

/* The party actors of the common actor list (start, update) and the
 * updates installed again when resuming a saved state. */
s32 worldmap_leader_start(s32 index), worldmap_leader_update(s32 index); /* the leader */
s32 worldmap_second_member_start(s32 index), worldmap_third_member_start(s32 index), worldmap_follower_update(s32 index); /* followers */
s32 worldmap_player_vehicle_start(s32 index), worldmap_player_vehicle_update(s32 index); /* the player's vehicle */
s32 worldmap_second_vehicle_start(s32 index), worldmap_third_vehicle_start(s32 index), worldmap_follower_vehicle_update(s32 index); /* the others */
s32 worldmap_flying_vehicle_start(s32 index), worldmap_flying_vehicle_update(s32 index); /* the flying vehicle */
s32 worldmap_flying_vehicle_rotors_start(s32 index), worldmap_flying_vehicle_rotors_update(s32 index); /* its rotors */
s32 worldmap_leader_resume(s32 index), worldmap_second_member_resume(s32 index), worldmap_third_member_resume(s32 index);
s32 worldmap_player_vehicle_resume(s32 index), worldmap_second_vehicle_resume(s32 index), worldmap_third_vehicle_resume(s32 index);
s32 worldmap_flying_vehicle_resume(s32 index);
s32 worldmap_flying_vehicle_rotors_resume(void);

#endif
