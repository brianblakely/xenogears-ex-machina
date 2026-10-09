#ifndef WORLDMAP_PARTY_H
#define WORLDMAP_PARTY_H

/* The party on the world map: the leader on foot and the two followers
 * (worldmap_80083A00), the members' parked vehicles and the player's
 * (worldmap_8008C364), the flying vehicle (worldmap_8008E190), the pad
 * steering of each movement mode (worldmap_80090A84) and the queue of actor
 * placements the movers test against. */

#include "worldmap.h"

/* The party's model files: character and gear models per party slot. */
extern void *D_8009CD34[3]; /* character model buffers */
extern void *D_8009BDF8[3]; /* gear model buffers */
extern s32 D_8009C170;      /* loaded party members */

/* A parked vehicle's spot (world units), per party slot: event variables
 * 21-29 of the game data. */
typedef struct {
    u16 flags; /* 0x3FFF part >= 0x400: parked on the map */
    u16 x;
    u16 z;
} VehicleSpot;

#define VEHICLE_SPOTS ((VehicleSpot *)&D_8006D634.vars[21])

/* A halfword of the game data at an offset from the world map's return
 * state (D_8006D634.worldmap). */
#define STATE_U16(offset) (*(u16 *)((u8 *)&D_8006D634.worldmap + (offset)))

/* Game data words that some movers address by names of their own: as members
 * of D_8006D634 they compile differently there. The parked vehicles'
 * headings (worldmap.unk5A-unk5E; func_8008C75C, func_8008D590, func_8008DF0C)
 * and spots (func_8008C364), the flying vehicle's heading and the return
 * flags (worldmap.vehicle_heading and flags; worldmap_8008E190), and the spots
 * seen four bytes early, so that the followers' actor slots 1-3 index the x
 * and z of party slots 0-2 (func_8008B644). */
extern u16 D_8006EE5A, D_8006EE5C, D_8006EE5E;
extern VehicleSpot D_8006EF8E[3];
extern u16 D_8006EE66, D_8006EE68;

typedef struct {
    u16 x;
    u16 z;
    u16 flags;
} PartySpot;

extern PartySpot D_8006EF8A[];

/* Queued actor placement (0x18 bytes, ring of 32). */
typedef struct PlaceRequest {
    s16 actor;
    s16 pad2;
    s32 px, py, pz;
    s32 z;
    s16 x;
    s16 pad16;
} PlaceRequest;

extern PlaceRequest D_8009BE6C[32];
extern s16 D_8009BD04;
extern u8 D_8009D738, D_8009BD60; /* the last placement probe: hit, actor */

void func_8008BFD4(s32 index, VECTOR *position, s32 x, s32 z);
void func_8008C040(VECTOR *position, s32 radius, s32 height, u8 *hit, u8 *actor);

extern s16 D_8009B180[]; /* per landing kind: may stand there */

s32 func_8008BEC8(WorldmapActor *actor); /* step towards the target */
void func_8008C1DC(s32 effect, WorldmapActor *actor, ActorScratch *scratch); /* on terrain type 3 */
void func_8008C28C(WorldmapActor *actor, s32 member); /* create a member's gear sprite */
s32 func_8008C364(WorldmapActor *actor, s32 member); /* place a member's vehicle */
void func_8008DFF4(VECTOR *position); /* restore the saved vehicle position */
void func_8008E034(VECTOR *position); /* save it */
void func_8008E078(void); /* the path table of scenes 15 and 16 */
s32 func_8008E0F0(VECTOR *position, s32 unused, s32 range); /* the first heading a probe hits in */
void func_80090A18(void); /* latch the two-button combination (D_8009BD34) */

/* Pad steering per movement mode: on foot, vehicle, flying and free
 * flight. */
s32 func_80090A84(WorldmapActor *actor);
s32 func_80090C68(WorldmapActor *actor);
s32 func_80090E14(WorldmapActor *actor);
s32 func_80090FB4(WorldmapActor *actor);

/* The party actors of the common actor list (start, update) and the
 * updates installed again when resuming a saved state. */
s32 func_8008A2C8(s32 index), func_8008A72C(s32 index); /* the leader */
s32 func_8008B2BC(s32 index), func_8008BB40(s32 index), func_8008B644(s32 index); /* followers */
s32 func_8008C530(s32 index), func_8008C844(s32 index); /* the player's vehicle */
s32 func_8008D3F0(s32 index), func_8008DD6C(s32 index), func_8008D678(s32 index); /* the others */
s32 func_8008E190(s32 index), func_8008E76C(s32 index); /* the flying vehicle */
s32 func_800906E0(s32 index), func_800907F4(s32 index); /* its rotors */
s32 func_8008A52C(s32 index), func_8008B498(s32 index), func_8008BD1C(s32 index);
s32 func_8008C6EC(s32 index), func_8008D520(s32 index), func_8008DE9C(s32 index);
s32 func_8008E4F4(s32 index);
s32 func_800907C4(void);

#endif
