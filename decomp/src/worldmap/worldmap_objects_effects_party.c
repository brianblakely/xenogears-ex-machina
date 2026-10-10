/* World map unit 80083A00-8008C364 (rodata 80070490-800707AC, data
 * 8009AD2C-8009B1AC): mode 18's actors, the scene objects (build, link,
 * draw, collision probes), the actors' model sprites, the terrain
 * billboards, the clouds, the areas' own actors (spinning objects, the
 * ferry, the airship), the particle effects and the party on foot (leader
 * and followers, placements).
 *
 * worldmap_scene17_pulse_update's five-entry table ends at 80070490 and worldmap_scene18_camera_update's
 * follows at once, 0 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after worldmap_scene17_pulse_update, at or before
 * worldmap_scene18_camera_update. PsyQ's CC1PSX 2.7.2.SN32.3.7.0002 gives this unit the same
 * text and relocations as the build's cc1 (docs/matching.md). */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "gte.h"
#include "party.h"
#include "scene.h"
#include "screen.h"
#include "terrain.h"

/* Scene object draw mode, by the object's flags (worldmap_objects_draw). */
s16 worldmap_object_draw_modes[10] = {4, 4, 5, 5, 0, 0, 2, 2, 3, 3}; /* 8009AD2C */

/* Drifting sprites (clouds, worldmap_clouds_draw): texture origins, the far sprite
 * corners and the near sprites' quads (four corners each), in three layers. */
u16 worldmap_cloud_texture_origins[8] = {0, 0x40, 0x80, 0, 0xC0, 0x4000, 0x4040, 0}; /* 8009AD40 */
SVECTOR worldmap_cloud_far_corners[12] = { /* 8009AD50 */
    {-192, 0, 192}, {192, 0, 192}, {-192, 0, -192}, {192, 0, -192},
    {-192, -8, 192}, {192, -8, 192}, {-192, -8, -192}, {192, -8, -192},
    {-192, -16, 192}, {192, -16, 192}, {-192, -16, -192}, {192, -16, -192},
};
SVECTOR worldmap_cloud_near_corners[48] = { /* 8009ADB0 */
    {-192, 0, 192}, {0, 0, 192}, {-192, 0, 0}, {0, 0, 0},
    {0, 0, 192}, {192, 0, 192}, {0, 0, 0}, {192, 0, 0},
    {-192, 0, 0}, {0, 0, 0}, {-192, 0, -192}, {0, 0, -192},
    {0, 0, 0}, {192, 0, 0}, {0, 0, -192}, {192, 0, -192},
    {-192, -8, 192}, {0, -8, 192}, {-192, -8, 0}, {0, -8, 0},
    {0, -8, 192}, {192, -8, 192}, {0, -8, 0}, {192, -8, 0},
    {-192, -8, 0}, {0, -8, 0}, {-192, -8, -192}, {0, -8, -192},
    {0, -8, 0}, {192, -8, 0}, {0, -8, -192}, {192, -8, -192},
    {-192, -16, 192}, {0, -16, 192}, {-192, -16, 0}, {0, -16, 0},
    {0, -16, 192}, {192, -16, 192}, {0, -16, 0}, {192, -16, 0},
    {-192, -16, 0}, {0, -16, 0}, {-192, -16, -192}, {0, -16, -192},
    {0, -16, 0}, {192, -16, 0}, {0, -16, -192}, {192, -16, -192},
};

/* Drift template points (worldmap_clouds_scatter). */
Drift worldmap_cloud_template_points[5] = { /* 8009AF30 */
    {0xC0, 0, 0xC0}, {0x240, 0, 0x340}, {0x4C0, 0, 0x140}, {0x5C0, 0, 0x440}, {0x2C0, 0, 0x640},
};

/* Ferry waypoints (x, z). */
u16 worldmap_ferry_waypoints_x[8] = {23296, 18208, 14968, 11491, 3072, 31144, 28148, 25620}; /* 8009AF80 */
u16 worldmap_ferry_waypoints_z[8] = {23736, 16576, 16008, 16554, 16200, 14552, 14612, 16928}; /* 8009AF90 */

/* Scene object links (parent, child pairs; -1 ends). */
s16 worldmap_airship_object_links[30] = { /* 8009AFA0 */
    7, 0, 7, 1, 7, 2, 7, 3, 5, 7, 5, 4, 12, 10, 12, 5, 12, 11, 12, 13, 12, 6, 12, 9, 12, 8,
    12, 13, -1, 0,
};

/* Scene objects to show (-1 ends). */
s16 worldmap_object69_children[10] = {67, 68, 70, 71, 72, 73, 74, 75, 76, -1}; /* 8009AFDC */

/* Particle kinds (worldmap_effects_draw_particles): four packed u,v corners and the quad shape. */
u16 worldmap_effect_particle_uvs[10 * 4] = { /* 8009AFF0 */
    0, 0, 0, 0,
    0, 0x3F, 0x3F00, 0x3F3F,
    0x4000, 0x403F, 0x7F00, 0x7F3F,
    0x40, 0x5F, 0x1F40, 0x1F5F,
    0x2040, 0x205F, 0x3F40, 0x3F5F,
    0x4040, 0x405F, 0x5F40, 0x5F5F,
    0xF000, 0xF05F, 0xFF00, 0xFF5F,
    0x60, 0x7F, 0xFF60, 0xFF7F,
    0x8000, 0x801F, 0xDF00, 0xDF1F,
    0xB020, 0xB05F, 0xEF20, 0xEF5F,
};
/* Particle shapes and texture coordinates, per kind. */
typedef struct {
    SVECTOR v[4];
} ParticleShape;

ParticleShape worldmap_effect_particle_shapes[10] = { /* 8009B040 */
    {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{-32, -64, 0}, {32, -64, 0}, {-32, 0, 0}, {32, 0, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
    {{{0, -8, 0}, {96, -8, 0}, {0, 8, 0}, {96, 8, 0}}},
    {{{-16, -255, 0}, {16, -255, 0}, {-16, 0, 0}, {16, 0, 0}}},
    {{{-16, -96, 0}, {16, -96, 0}, {-16, 0, 0}, {16, 0, 0}}},
    {{{-32, -32, 0}, {32, -32, 0}, {-32, 32, 0}, {32, 32, 0}}},
};

/* Per landing kind: whether the player may stand there. */
s16 worldmap_cylinder_hit_standable[6] = {1, 1, 0, 1, 1, 1}; /* 8009B180 */

/* Per party member: the parameters worldmap_create_gear_sprite passes with its gear
 * model. The last table (0x140, 0x140, 0x100) ends the unit's data before a
 * stray halfword (00 3c) that nothing reads, so it stays original data
 * (worldmap.classification.txt). */
s16 worldmap_gear_sprite_x[3] = {0x100, 0x100, 0x100}; /* 8009B18C */
s16 worldmap_gear_sprite_y[3] = {0x1FD, 0x1FC, 0x1FB}; /* 8009B194 */
s16 worldmap_gear_sprite_width[3] = {0x140, 0x160, 0x280}; /* 8009B19C */
INCLUDE_ORIGINAL(".data", worldmap_gear_sprite_height, 0x8009B1A4, 8);
extern s16 worldmap_gear_sprite_height[3];

/* 80083A00: Scripted camera stages 1-6 around the player (commands set the angle, distance
 * and position of each stage), with easing and a random vertical shake. */
s32 worldmap_scene18_camera_update(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;

    actor = &worldmap_actor_slots[index];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->state = 1;
        actor->unk4 = 0;
        actor->unk5C = 0x960000;
        worldmap_camera_angle.vx = -0x20;
        worldmap_camera_angle.vy = 0x400;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x20 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x960000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        break;
    case 2:
        actor->state = 2;
        actor->unk4 = 0;
        actor->unk5C = 0x180000;
        worldmap_camera_angle.vx = 0x40;
        worldmap_camera_angle.vy = 0x1A0;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = 0x40 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x180000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vy = worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = -0xA0000;
        break;
    case 3:
        actor->state = 3;
        actor->unk4 = 0;
        actor->unk5C = 0x120000;
        worldmap_camera_angle.vx = -0x2A0;
        worldmap_camera_angle.vy = 0x6A0;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x2A0 << 12;
        worldmap_camera_distance = 0x120000;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vx = worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = 0x1700000;
        GROUND_SCROLL[0] += 0x1700000 - worldmap_player_position.vx;
        break;
    case 4:
        actor->state = 4;
        actor->unk4 = 0;
        actor->unk5C = 0x180000;
        worldmap_camera_angle.vx = -0x10;
        worldmap_camera_angle.vy = 0xA50;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x10 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x180000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vx = worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = 0x1800000;
        actor->position.vz = worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = 0x1900000;
        actor->position.vy = worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = -0x80000;
        GROUND_SCROLL[0] += 0x100;
        GROUND_SCROLL[2] -= 0x100;
        break;
    case 5:
        actor->state = 5;
        actor->unk4 = 0;
        actor->unk5C = 0x260000;
        worldmap_camera_angle.vx = -0x100;
        worldmap_camera_angle.vy = 0x670;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x100 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vy = worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = -0x110000;
        actor->position.vz = worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = 0x1A80000;
        worldmap_camera_distance = 0x260000;
        GROUND_SCROLL[2] += 0x180;
        break;
    case 6:
        actor->state = 6;
        actor->unk4 = 0;
        actor->unk5C = 0x620000;
        worldmap_camera_angle.vx = -0x80;
        worldmap_camera_angle.vy = 0xA70;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x80 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vy = worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = -0x120000;
        actor->position.vz = worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = 0x1980000;
        worldmap_camera_distance = 0x620000;
        GROUND_SCROLL[2] -= 0x100;
        break;
    }
    if (worldmap_view_kind == 0) {
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    }
    switch (actor->state) {
    case 0:
    case 2:
    case 3:
    case 4:
    case 6:
        break;
    case 1:
        actor->u.step = worldmap_step_value_toward(actor->u.step, -0x400000, -0x8000);
        actor->unk54 = worldmap_step_value_toward(actor->unk54, 0, -0x8000);
        actor->unk5C = worldmap_step_value_toward(actor->unk5C, 0x380000, -0xD000);
        break;
    case 5:
        actor->u.step = worldmap_step_value_toward(actor->u.step, 0x200000, 0x2000);
        break;
    }
    worldmap_camera_ease_angle(actor, scratch);
    worldmap_camera_ease_distance(actor, scratch);
    worldmap_camera_ease_position(actor, scratch);
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    VIEW.eye.vy += scratch->position.vy;
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* 80083FE4: Place the actor and show scene objects 7 and 8 at its position. */
s32 worldmap_scene18_lift_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->position.vx = 0x1800000;
    actor->position.vy = 0x80000;
    actor->position.vz = 0x1A00000;
    actor->state = 0;
    worldmap_objects[8].visible = 1;
    worldmap_objects[7].visible = 1;
    worldmap_objects[7].position.vx = worldmap_objects[8].position.vx = actor->position.vx >> 12;
    worldmap_objects[7].position.vy = worldmap_objects[8].position.vy = actor->position.vy >> 12;
    worldmap_objects[7].position.vz = worldmap_objects[8].position.vz = actor->position.vz >> 12;
    return 1;
}

/* 80084068: Drive the lift of scene objects 7 and 8: a command (1-4) selects the
 * route state and stops the effect groups of the previous one; each state
 * lowers the actor and moves its effect groups with it. */
s32 worldmap_scene18_lift_update(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;
    SceneObject *objects;
    SceneObject *lift;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    lift = &objects[7];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        objects[8].visible = 0;
        objects[7].visible = 0;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 2;
        worldmap_effects_stop_emitters(3);
        worldmap_effects_stop_emitters(5);
        worldmap_effects_stop_emitters(6);
        worldmap_effects_stop_emitters(7);
        worldmap_effects_stop_emitters(10);
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 3;
        worldmap_effects_stop_emitters(7);
        worldmap_effects_stop_emitters(8);
        worldmap_effects_stop_emitters(9);
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 4;
        worldmap_effects_stop_emitters(5);
        worldmap_effects_stop_emitters(6);
        worldmap_effects_stop_emitters(7);
        worldmap_effects_stop_emitters(10);
        worldmap_effects_stop_emitters(11);
        worldmap_effects_stop_emitters(12);
        break;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->position.vy = worldmap_step_value_toward(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        worldmap_effects_start_emitters(3, &scratch->position, NULL);
        worldmap_effects_start_emitters(5, &scratch->position, NULL);
        worldmap_effects_start_emitters(6, &scratch->position, NULL);
        worldmap_effects_start_emitters(7, &scratch->position, NULL);
        worldmap_effects_start_emitters(10, &scratch->position, NULL);
        break;
    case 2:
        actor->position.vy = worldmap_step_value_toward(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        worldmap_effects_start_emitters(7, &scratch->position, NULL);
        worldmap_effects_start_emitters(8, &scratch->position, NULL);
        worldmap_effects_start_emitters(9, &scratch->position, NULL);
        break;
    case 3:
        actor->position.vy = worldmap_step_value_toward(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        worldmap_effects_start_emitters(5, &scratch->position, NULL);
        worldmap_effects_start_emitters(6, &scratch->position, NULL);
        worldmap_effects_start_emitters(7, &scratch->position, NULL);
        worldmap_effects_start_emitters(10, &scratch->position, NULL);
        worldmap_effects_start_emitters(11, &scratch->position, NULL);
        worldmap_effects_start_emitters(12, &scratch->position, NULL);
        break;
    case 4:
        actor->position.vy = worldmap_step_value_toward(actor->position.vy, -0x180000, -0x400);
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        worldmap_effects_start_emitters(13, &scratch->position, NULL);
        scratch->position.vy = 0;
        worldmap_effects_start_emitters(14, &scratch->position, NULL);
        break;
    }
    lift[0].position.vx = lift[1].position.vx = actor->position.vx >> 12;
    lift[0].position.vy = lift[1].position.vy = actor->position.vy >> 12;
    lift[0].position.vz = lift[1].position.vz = actor->position.vz >> 12;
    return 1;
}

/* 8008440C: Unpack the area image to VRAM, then build 15 CLUT rows fading the
 * 0,0x1F0 CLUT row towards a pale blue and record the 16 CLUT ids. */
void worldmap_upload_area_image(void) {
    RECT rect;
    CVECTOR fade = {0xE0, 0xF5, 0xFF, 0x00};
    s32 i;
    u16 *clut;
    void *source;
    void *faded;

    source = text_unpack_lzss_alloc(worldmap_area_image, 1);
    i = 0;
    model_load_tim_list(source);
    DrawSync(0);
    heap_free(source);
    clut = worldmap_area_image_cluts;
    heap_free(worldmap_area_image);
    source = heap_alloc(0x200, 1);
    faded = heap_alloc(0x2000, 1);
    rect.x = 0;
    rect.y = 0x1F0;
    rect.w = 0x100;
    rect.h = 1;
    StoreImage(&rect, source);
    DrawSync(0);
    worldmap_build_palette_fades(source, faded, 0x10, (u8 *)&fade);
    rect.x = 0;
    rect.y = 0x1F0;
    rect.w = 0x100;
    rect.h = 0xF;
    LoadImage(&rect, faded);
    DrawSync(0);
    do {
        i++;
        *clut = GetClut(rect.x, rect.y);
        rect.y++;
        clut++;
    } while (i < 0x10);
    heap_free(faded);
    heap_free(source);
}

/* Scene object placement (16 bytes; the list follows a count halfword). */
typedef struct {
    u16 def;
    u16 flags;
    s16 x, y, z;
    s16 ax, ay, az;
} ScenePlacement;

/* The area file's sprite models (a resident ModelGroup, whose models it
 * relocates): a 16-byte header, then the models. */
typedef struct {
    u8 header[0x10];
    SpriteModel defs[1];
} SpriteDefTable;

/* 80084580: Build the scene objects from the area's placement list: resolve the
 * animation offsets, then place, orient and build each object's
 * primitives (two buffers) from its sprite definition. */
void worldmap_objects_build(void) {
    ScenePlacement *placement;
    s32 *base;
    s32 *offsets;
    s32 i;

    i = 0;
    worldmap_object_model_count = model_relocate_group(worldmap_object_models);
    base = worldmap_object_meshes;
    offsets = base + 1;
    worldmap_object_meshes = offsets;
    for (; i < worldmap_object_model_count; i++) {
        offsets[i] = (s32)base + offsets[i];
    }
    worldmap_object_count = *(s16 *)worldmap_object_placement_list;
    worldmap_objects = heap_alloc(worldmap_object_count * sizeof(SceneObject), 0);
    placement = (ScenePlacement *)((u8 *)worldmap_object_placement_list + 2);
    SetColorMatrix(&worldmap_light_color_matrix);
    SetLightMatrix(&worldmap_light_direction_matrix);
    for (i = 0; i < worldmap_object_count; i++, placement++) {
        worldmap_objects[i].visible = 0;
        worldmap_objects[i].unk2 = placement->def;
        worldmap_objects[i].flags = placement->flags;
        worldmap_objects[i].position.vx = placement->x;
        worldmap_objects[i].position.vy = placement->y;
        worldmap_objects[i].position.vz = -placement->z;
        worldmap_objects[i].angle.vx = placement->ax;
        worldmap_objects[i].angle.vy = placement->ay;
        worldmap_objects[i].angle.vz = placement->az;
        RotMatrixYXZ(&worldmap_objects[i].angle, &worldmap_objects[i].matrix);
        worldmap_objects[i].def = &((SpriteDefTable *)worldmap_object_models)->defs[worldmap_objects[i].unk2];
        model_alloc_packet_buffers(worldmap_objects[i].def, &worldmap_objects[i].prims, &worldmap_objects[i].prims2, &worldmap_objects[i]);
        model_build_packets(worldmap_objects[i].def, worldmap_objects[i].prims, 1);
        memcpy(worldmap_objects[i].prims2, worldmap_objects[i].prims, worldmap_objects[i].def->packet_size);
        worldmap_objects[i].unk44 = ((s32 *)worldmap_object_meshes)[worldmap_objects[i].unk2];
        ((s32 *)worldmap_objects[i].unk44)[1] = worldmap_objects[i].unk44 + ((s32 *)worldmap_objects[i].unk44)[1];
        worldmap_objects[i].parent = NULL;
    }
    model_ot_depth_shift = 2;
    worldmap_walker_face = -1;
    worldmap_walker_object = -1;
    worldmap_objects[4].visible = 1;
}

/* 80084818: Free the scene objects' primitives and definitions, then the objects. */
void worldmap_objects_free(void) {
    s32 i;

    for (i = 0; i < worldmap_object_count; i++) {
        heap_free(worldmap_objects[i].prims);
        model_free_owned_block((ModelBuffer *)worldmap_objects[i].def);
    }
    heap_free(worldmap_objects);
}

/* 800848B4: Link scene object `child` to `parent`. */
void worldmap_objects_link(s32 parent, s32 child) {
    worldmap_objects[child].parent = &worldmap_objects[parent];
}

/* Scratchpad work area of the scene object pass. */
typedef struct {
    VECTOR offset;  /* 0x00 */
    VECTOR scale;   /* 0x10 */
    s32 flag;       /* 0x20 */
    s32 pad24;
    s32 sz;         /* 0x28 */
    u8 pad2C[0x74];
    SVECTOR origin; /* 0xA0 */
    u8 padA8[0x48];
    MATRIX m;       /* 0xF0 */
    MATRIX out;     /* 0x110 */
} SceneScratch;

/* 800848F4: Draw the visible scene objects: build each one's matrix through its
 * parent chain, place it relative to the camera target, and add its sprite
 * set to the ordering table when it projects in front and near enough. */
void worldmap_objects_draw(void) {
    SceneScratch *scratch;
    SceneObject *parent;
    s32 i;
    s32 x;
    s32 z;

    scratch = (SceneScratch *)0x1F800000;
    scratch->scale.vz = 0x800;
    scratch->scale.vy = 0x800;
    scratch->scale.vx = 0x800;
    model_box_test_mode = 3;
    x = worldmap_camera.target.vx >> 12;
    z = worldmap_camera.target.vz >> 12;
    model_submitted_primitive_count = 0;
    model_drawn_primitive_count = 0;
    scratch->origin.vz = 0;
    scratch->origin.vy = 0;
    scratch->origin.vx = 0;
    for (i = 0; i < worldmap_object_count; i++) {
        if (worldmap_objects[i].visible == 0) {
            scratch->m = worldmap_objects[i].matrix;
            scratch->m.t[0] = worldmap_objects[i].position.vx;
            scratch->m.t[1] = worldmap_objects[i].position.vy;
            scratch->m.t[2] = -worldmap_objects[i].position.vz;
            parent = &worldmap_objects[i];
            while (parent->parent != NULL) {
                parent = parent->parent;
                parent->matrix.t[0] = parent->position.vx;
                parent->matrix.t[1] = parent->position.vy;
                parent->matrix.t[2] = -parent->position.vz;
                gte_CompMatrix(&parent->matrix, &scratch->m, &scratch->m);
            }
            scratch->offset.vx = scratch->m.t[0] - x;
            scratch->offset.vz = -scratch->m.t[2] - z;
            worldmap_wrap_world_offset(&scratch->offset);
            scratch->m.t[0] = scratch->offset.vx;
            scratch->m.t[2] = -scratch->offset.vz;
            ScaleMatrix(&scratch->m, &scratch->scale);
            CompMatrix(&worldmap_camera_matrix, &scratch->m, &scratch->out);
            SetRotMatrix(&scratch->out);
            SetTransMatrix(&scratch->out);
            gte_ldv0(&scratch->origin);
            gte_rtps();
            gte_stflg(&scratch->flag);
            if (scratch->flag >= 0) {
                gte_stsz(&scratch->sz);
                if (scratch->sz < 0xD80) {
                    model_draw_sprite_model(worldmap_objects[i].def, (&worldmap_objects[i].prims)[worldmap_display_buffer_index],
                                  (u32 *)worldmap_current_display_buffer->ot, worldmap_object_draw_modes[(s16)worldmap_objects[i].flags]);
                }
            }
        }
    }
}

s32 worldmap_mesh_collect_faces_at(s32 probe, s32 index);

/* 80084D00: Probe the solid scene objects; the first hit's result, with its index. */
s16 worldmap_objects_probe_solid(s32 probe, s16 *hit) {
    SceneObject *object;
    s16 i;
    s16 result;

    object = worldmap_objects;
    i = 0;
    while (i < worldmap_object_count) {
        if (object->flags & 1) {
            result = worldmap_mesh_collect_faces_at(probe, i);
            if (result != 0) {
                *hit = i;
                return result;
            }
        }
        i++;
        object++;
    }
    return 0;
}

/* Scratchpad work area of the face containment test. */
typedef struct {
    VECTOR p[3];    /* 0x00: transformed corners; p[0] first holds the scale */
    union {
        struct {
            s32 edge[2];  /* 0x30: packed (x, z) corner pairs */
            s32 point;    /* 0x38: packed (x, z) probe */
            s32 pad3C;
            VECTOR delta; /* 0x40: probe relative to the object */
        } test;
        VECTOR side[3];   /* 0x30: normalised edge directions */
    } u;
    u8 pad60[0x90];
    MATRIX m;       /* 0xF0 */
} FaceTestScratch;

#define FACE_TEST_SCRATCH ((FaceTestScratch *)0x1F800000)

/* 80084DB8: Test the probe position against scene object `index`: transform its
 * collision faces flat (x, z) and record every face whose outline contains
 * the probe (face number and its attribute) in worldmap_mesh_probe_hits. Returns a word-sized
 * count of table entries; the caller narrows it to s16. */
s32 worldmap_mesh_collect_faces_at(s32 probe, s32 index) {
    s32 flag;
    SceneObject *object;
    FaceTestScratch *scratch;
    Mesh *mesh;
    SVECTOR *vertices;
    MeshFace *face;
    s32 count;
    s32 i;
    s32 dx, dz, far;
    s32 hits;

    object = &worldmap_objects[index];
    dx = (((VECTOR *)probe)->vx >> 12) - object->position.vx;
    FACE_TEST_SCRATCH->u.test.delta.vx = dx;
    dx = dx >= 0 ? dx : -dx;
    far = dx >= 0x800;
    dz = object->position.vz - (((VECTOR *)probe)->vz >> 12);
    FACE_TEST_SCRATCH->u.test.delta.vz = dz;
    dz = dz >= 0 ? dz : -dz;
    far |= dz >= 0x800;
    scratch = FACE_TEST_SCRATCH;
    if (far) {
        return 0;
    }
    scratch->m = object->matrix;
    scratch->m.t[2] = 0;
    scratch->m.t[0] = 0;
    scratch->m.t[1] = object->position.vy;
    scratch->p[0].vz = 0x800;
    scratch->p[0].vy = 0x800;
    scratch->p[0].vx = 0x800;
    hits = 0;
    ScaleMatrix(&scratch->m, &scratch->p[0]);
    SetRotMatrix(&scratch->m);
    SetTransMatrix(&scratch->m);
    mesh = (Mesh *)object->unk44;
    count = mesh->unk0;
    vertices = mesh->vertices;
    scratch->u.test.point = (scratch->u.test.delta.vz << 16) | (scratch->u.test.delta.vx & 0xFFFF);
    face = mesh->faces;
    for (i = 0; i < count; i++, face++) {
        gte_RotTrans(&vertices[face->corner[0]], &scratch->p[0], &flag);
        gte_RotTrans(&vertices[face->corner[1]], &scratch->p[1], &flag);
        gte_RotTrans(&vertices[face->corner[2]], &scratch->p[2], &flag);
        scratch->u.test.edge[0] = (scratch->p[0].vz << 16) | (scratch->p[0].vx & 0xFFFF);
        scratch->u.test.edge[1] = (scratch->p[1].vz << 16) | (scratch->p[1].vx & 0xFFFF);
        if (NormalClip(scratch->u.test.edge[0], scratch->u.test.edge[1], scratch->u.test.point) > 0) {
            continue;
        }
        scratch->u.test.edge[0] = (scratch->p[1].vz << 16) | (scratch->p[1].vx & 0xFFFF);
        scratch->u.test.edge[1] = (scratch->p[2].vz << 16) | (scratch->p[2].vx & 0xFFFF);
        if (NormalClip(scratch->u.test.edge[0], scratch->u.test.edge[1], scratch->u.test.point) > 0) {
            continue;
        }
        scratch->u.test.edge[0] = (scratch->p[2].vz << 16) | (scratch->p[2].vx & 0xFFFF);
        scratch->u.test.edge[1] = (scratch->p[0].vz << 16) | (scratch->p[0].vx & 0xFFFF);
        if (NormalClip(scratch->u.test.edge[0], scratch->u.test.edge[1], scratch->u.test.point) > 0) {
            continue;
        }
        worldmap_mesh_probe_hits[hits] = i;
        worldmap_mesh_probe_hits[hits + 1] = face->kind;
        hits += 2;
    }
    return hits;
}

/* Scratchpad work area of the face probe. */
typedef struct {
    VECTOR p[3];      /* face corners; p[1] first holds the scale */
    VECTOR normal;    /* 0x30 */
    VECTOR side;      /* 0x40: probe ends against the plane */
    u8 pad50[0xA0];
    MATRIX m;         /* 0xF0 */
    MATRIX probe;     /* 0x110: rows are the probe segment ends */
} FaceScratch;

#define FACE_SCRATCH ((FaceScratch *)0x1F800000)

/* 80085158: Project `position` onto face `face` of scene object `index`: `offset` gets
 * the object-relative x/z, `normal` the face normal and offset->vy the
 * height of the face plane there. */
void worldmap_mesh_project_onto_face(VECTOR *position, VECTOR *offset, VECTOR *normal, u16 index, u16 face) {
    long flag;
    VECTOR *edge1;
    VECTOR *edge2;
    SceneObject *object;
    MeshFace *corners;
    SVECTOR *vertices;
    s32 y;

    object = &worldmap_objects[index];
    offset->vx = (position->vx >> 12) - object->position.vx;
    offset->vz = object->position.vz - (position->vz >> 12);
    FACE_SCRATCH->m = object->matrix;
    FACE_SCRATCH->m.t[2] = 0;
    FACE_SCRATCH->m.t[0] = 0;
    y = object->position.vy;
    FACE_SCRATCH->p[1].vz = 0x800;
    FACE_SCRATCH->p[1].vy = 0x800;
    FACE_SCRATCH->p[1].vx = 0x800;
    FACE_SCRATCH->m.t[1] = y;
    edge1 = &FACE_SCRATCH->p[1];
    ScaleMatrix(&FACE_SCRATCH->m, edge1);
    SetRotMatrix(&FACE_SCRATCH->m);
    SetTransMatrix(&FACE_SCRATCH->m);
    edge2 = &FACE_SCRATCH->p[2];
    corners = ((Mesh *)object->unk44)->faces;
    corners += face;
    vertices = ((Mesh *)object->unk44)->vertices;
    RotTrans(&vertices[corners->corner[0]], &FACE_SCRATCH->p[0], &flag);
    RotTrans(&vertices[corners->corner[1]], &FACE_SCRATCH->p[1], &flag);
    RotTrans(&vertices[corners->corner[2]], &FACE_SCRATCH->p[2], &flag);
    edge1->vx -= FACE_SCRATCH->p[0].vx;
    edge1->vy -= FACE_SCRATCH->p[0].vy;
    edge1->vz -= FACE_SCRATCH->p[0].vz;
    edge2->vx -= FACE_SCRATCH->p[0].vx;
    edge2->vy -= FACE_SCRATCH->p[0].vy;
    edge2->vz -= FACE_SCRATCH->p[0].vz;
    OuterProduct0(&FACE_SCRATCH->p[2], &FACE_SCRATCH->p[1], &FACE_SCRATCH->p[2]);
    edge2->vx >>= 2;
    edge2->vy >>= 2;
    edge2->vz >>= 2;
    VectorNormal(&FACE_SCRATCH->p[2], normal);
    worldmap_set_height_on_plane(offset, &FACE_SCRATCH->p[0], normal);
}

/* 80085418: Does the vertical segment from `position` down by `height` cross the plane
 * of face `face` of scene object `index`? Returns -1 if so, else 0. */
s32 worldmap_mesh_is_face_plane_crossed(VECTOR *position, s32 height, u16 index, u16 face) {
    long flag;
    SceneObject *object;
    MeshFace *corners;
    SVECTOR *vertices;
    VECTOR *origin;
    VECTOR *edge1;
    VECTOR *edge2;
    s32 depth;
    s32 y;
    s16 z;

    object = &worldmap_objects[index];
    FACE_SCRATCH->m = object->matrix;
    FACE_SCRATCH->m.t[2] = 0;
    FACE_SCRATCH->m.t[0] = 0;
    FACE_SCRATCH->m.t[1] = object->position.vy;
    FACE_SCRATCH->p[1].vz = 0x800;
    FACE_SCRATCH->p[1].vy = 0x800;
    FACE_SCRATCH->p[1].vx = 0x800;
    edge1 = &FACE_SCRATCH->p[1];
    ScaleMatrix(&FACE_SCRATCH->m, edge1);
    SetRotMatrix(&FACE_SCRATCH->m);
    SetTransMatrix(&FACE_SCRATCH->m);
    edge2 = &FACE_SCRATCH->p[2];
    corners = ((Mesh *)object->unk44)->faces;
    corners += face;
    vertices = ((Mesh *)object->unk44)->vertices;
    RotTrans(&vertices[corners->corner[0]], &FACE_SCRATCH->p[0], &flag);
    RotTrans(&vertices[corners->corner[1]], &FACE_SCRATCH->p[1], &flag);
    RotTrans(&vertices[corners->corner[2]], &FACE_SCRATCH->p[2], &flag);
    origin = &FACE_SCRATCH->p[0];
    edge1->vx -= origin->vx;
    edge1->vy -= origin->vy;
    edge1->vz -= origin->vz;
    edge2->vx -= origin->vx;
    edge2->vy -= origin->vy;
    edge2->vz -= origin->vz;
    OuterProduct0(&FACE_SCRATCH->p[2], &FACE_SCRATCH->p[1], &FACE_SCRATCH->p[2]);
    edge2->vx >>= 2;
    edge2->vy >>= 2;
    edge2->vz >>= 2;
    VectorNormal(&FACE_SCRATCH->p[2], &FACE_SCRATCH->normal);
    FACE_SCRATCH->probe.m[1][0] = FACE_SCRATCH->probe.m[0][0] =
        (position->vx >> 12) - object->position.vx - origin->vx;
    y = (position->vy >> 12) - origin->vy;
    FACE_SCRATCH->probe.m[0][1] = y;
    z = object->position.vz;
    depth = position->vz >> 12;
    FACE_SCRATCH->probe.m[1][1] = y - height;
    z -= depth;
    z -= origin->vz;
    FACE_SCRATCH->probe.m[1][2] = FACE_SCRATCH->probe.m[0][2] = z;
    ApplyMatrixLV(&FACE_SCRATCH->probe, &FACE_SCRATCH->normal, &FACE_SCRATCH->side);
    return (FACE_SCRATCH->side.vx ^ FACE_SCRATCH->side.vy) >> 31;
}

/* 80085760: Classify the move from `from` to `to` against face `face` of scene object
 * `index`: bit n is set when `to` lies outside edge n (flat x, z); when it
 * leaves through a corner, keep only the edge the move crosses. Also leave
 * the normalised edge directions in the scratchpad. */
s32 worldmap_mesh_classify_face_exit(VECTOR *from, VECTOR *to, s32 index, s32 face) {
    u32 sides;
    s32 y;
    SceneObject *object;
    FaceTestScratch *scratch;
    VECTOR *p1;
    VECTOR *p2;
    Mesh *mesh;
    MeshFace *corners;
    SVECTOR *vertices;

    scratch = FACE_TEST_SCRATCH;
    object = &worldmap_objects[index];
    FACE_TEST_SCRATCH->u.test.delta.vx = (to->vx >> 12) - object->position.vx;
    FACE_TEST_SCRATCH->u.test.delta.vz = object->position.vz - (to->vz >> 12);
    FACE_TEST_SCRATCH->m = object->matrix;
    FACE_TEST_SCRATCH->m.t[2] = 0;
    FACE_TEST_SCRATCH->m.t[0] = 0;
    y = object->position.vy;
    FACE_TEST_SCRATCH->p[0].vz = 0x800;
    FACE_TEST_SCRATCH->p[0].vy = 0x800;
    scratch->p[0].vx = 0x800;
    FACE_TEST_SCRATCH->m.t[1] = y;
    ScaleMatrix(&FACE_TEST_SCRATCH->m, &FACE_TEST_SCRATCH->p[0]);
    SetRotMatrix(&FACE_TEST_SCRATCH->m);
    SetTransMatrix(&FACE_TEST_SCRATCH->m);
    mesh = (Mesh *)object->unk44;
    corners = mesh->faces;
    corners += face;
    vertices = mesh->vertices;
    gte_RotTrans(&vertices[corners->corner[0]], &scratch->p[0], &sides);
    gte_ldv0(&vertices[corners->corner[1]]);
    gte_rt();
    p1 = &FACE_TEST_SCRATCH->p[1];
    gte_stlvnl(p1);
    gte_stflg(&sides);
    gte_ldv0(&vertices[corners->corner[2]]);
    gte_rt();
    p2 = &FACE_TEST_SCRATCH->p[2];
    gte_stlvnl(p2);
    gte_stflg(&sides);
    sides = 0;
    FACE_TEST_SCRATCH->u.test.edge[0] = (FACE_TEST_SCRATCH->p[0].vz << 16) | (u16)scratch->p[0].vx;
    FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[1].vz << 16) | (u16)p1->vx;
    FACE_TEST_SCRATCH->u.test.point =
        (FACE_TEST_SCRATCH->u.test.delta.vz << 16) | (u16)FACE_TEST_SCRATCH->u.test.delta.vx;
    if (NormalClip(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                      FACE_TEST_SCRATCH->u.test.point) > 0) {
        sides |= 1;
    }
    FACE_TEST_SCRATCH->u.test.edge[0] = (FACE_TEST_SCRATCH->p[1].vz << 16) | (u16)p1->vx;
    FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[2].vz << 16) | (u16)p2->vx;
    if (NormalClip(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                      FACE_TEST_SCRATCH->u.test.point) > 0) {
        sides |= 2;
    }
    FACE_TEST_SCRATCH->u.test.edge[0] = (FACE_TEST_SCRATCH->p[2].vz << 16) | (u16)p2->vx;
    FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[0].vz << 16) | (u16)scratch->p[0].vx;
    if (NormalClip(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                      FACE_TEST_SCRATCH->u.test.point) > 0) {
        sides |= 4;
    }
    switch (sides) {
    case 3:
        FACE_TEST_SCRATCH->u.test.edge[0] = ((object->position.vz - (from->vz >> 12)) << 16) |
                                            (((from->vx >> 12) - object->position.vx) & 0xFFFF);
        FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[1].vz << 16) | (u16)p1->vx;
        if (NormalClip(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                          FACE_TEST_SCRATCH->u.test.point) != 0) {
            sides = 1;
        }
        break;
    case 5:
        FACE_TEST_SCRATCH->u.test.edge[0] = ((object->position.vz - (from->vz >> 12)) << 16) |
                                            (((from->vx >> 12) - object->position.vx) & 0xFFFF);
        FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[0].vz << 16) | (u16)scratch->p[0].vx;
        if (NormalClip(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                          FACE_TEST_SCRATCH->u.test.point) != 0) {
            sides = 4;
        }
        break;
    case 6:
        FACE_TEST_SCRATCH->u.test.edge[0] = ((object->position.vz - (from->vz >> 12)) << 16) |
                                            (((from->vx >> 12) - object->position.vx) & 0xFFFF);
        FACE_TEST_SCRATCH->u.test.edge[1] = (FACE_TEST_SCRATCH->p[2].vz << 16) | (u16)p2->vx;
        if (NormalClip(FACE_TEST_SCRATCH->u.test.edge[0], FACE_TEST_SCRATCH->u.test.edge[1],
                          FACE_TEST_SCRATCH->u.test.point) != 0) {
            sides = 2;
        }
        break;
    }
    scratch->u.side[0].vx = scratch->p[1].vx - scratch->p[0].vx;
    scratch->u.side[0].vy = 0;
    scratch->u.side[0].vz = scratch->p[1].vz - scratch->p[0].vz;
    VectorNormal(&scratch->u.side[0], &scratch->u.side[0]);
    scratch->u.side[1].vx = scratch->p[2].vx - scratch->p[1].vx;
    scratch->u.side[1].vy = 0;
    scratch->u.side[1].vz = scratch->p[2].vz - scratch->p[1].vz;
    VectorNormal(&scratch->u.side[1], &scratch->u.side[1]);
    scratch->u.side[2].vx = scratch->p[0].vx - scratch->p[2].vx;
    scratch->u.side[2].vy = 0;
    scratch->u.side[2].vz = scratch->p[0].vz - scratch->p[2].vz;
    VectorNormal(&scratch->u.side[2], &scratch->u.side[2]);
    return sides;
}

/* Scratchpad work area of the actor sprite pass. */
typedef struct {
    SVECTOR vertex;
    VECTOR offset;
    s32 depth[64];
} DepthScratch;

#define DEPTH_SCRATCH ((DepthScratch *)0x1F800000)

/* 80085CDC: Draw the actors' model sprites: place each visible model relative to the
 * camera target, project it for its depth, then add it to the ordering
 * table and turn its facing towards the actor heading, 0x100 per frame. */
void worldmap_actor_draw_sprites(void) {
    WorldmapActor *actor;
    Sprite *model;
    s32 i;
    s32 offset;
    s32 diff;
    s32 facing;
    s32 heading;
    DepthScratch *scratch;

    scratch = DEPTH_SCRATCH;
    actor = worldmap_actor_slots;
    for (i = 0; i < 0x40; i++, actor++) {
        if ((actor->unk24 == 0) & (actor->handle != NULL)) {
            scratch->offset.vx = actor->position.vx - worldmap_camera.target.vx;
            scratch->offset.vz = actor->position.vz - worldmap_camera.target.vz;
            worldmap_wrap_offset(&scratch->offset);
            actor->handle->x = scratch->offset.vx * 16;
            actor->handle->z = -scratch->offset.vz * 16;
            actor->handle->y = actor->position.vy * 16;
        }
    }
    SetRotMatrix(&worldmap_camera_matrix);
    SetTransMatrix(&worldmap_camera_matrix);
    actor = worldmap_actor_slots;
    for (i = 0; i < 0x40; i++, actor++) {
        model = actor->handle;
        if ((actor->unk24 == 0) & (model != NULL)) {
            scratch->vertex.vx = model->x >> 16;
            scratch->vertex.vy = actor->handle->y >> 16;
            scratch->vertex.vz = actor->handle->z >> 16;
            gte_ldv0(&scratch->vertex);
            gte_rtps();
            gte_stsz(&scratch->depth[i]);
        }
    }
    sprite_set_view_matrix(&worldmap_camera_matrix);
    actor = worldmap_actor_slots;
    for (i = 0; i < 0x40; i++, actor++) {
        if ((actor->unk24 == 0) & (actor->handle != NULL) & (scratch->depth[i] < 0xB00)) {
            sprite_draw(actor->handle, &worldmap_current_display_buffer->ot[scratch->depth[i] >> 4]);
            heading = actor->heading;
            facing = actor->unk5C;
            diff = heading - facing;
            if (diff < 0) {
                diff += 0x1000;
            }
            /* diff becomes the new facing, turned at most 0x100 */
            if (diff < 0x801) {
                if (diff < 0x100) {
                    diff = heading;
                } else {
                    diff = facing + 0x100;
                }
            } else {
                diff -= 0x1000;
                if (diff >= -0xFF) {
                    diff = heading;
                } else {
                    diff = facing - 0x100;
                }
            }
            actor->unk5C = diff;
            sprite_set_facing(actor->handle, (diff - (u16)worldmap_camera_angle.vy - 0x400) & 0xFFF);
            sprite_vm_tick(actor->handle);
        }
    }
}

/* 80085F58: Resolve the billboard lists' offsets and create the billboard CLUTs. */
void worldmap_billboards_resolve_lists(void) {
    BillboardList *texture;
    s32 i;

    texture = worldmap_billboard_lists;
    for (i = 0; i < 0x100; i++, texture++) {
        if (texture->data != NULL) {
            texture->data += (s32)worldmap_billboard_lists;
        }
    }
    for (i = 0; i < 0x10; i++) {
        worldmap_billboard_cluts[i] = GetClut(0xF0, i + 0x1F0);
    }
}

/* The billboard quads, copied between display buffers as a whole. */
typedef struct {
    POLY_FT4 quads[0x200];
} QuadBlock512;

/* 80085FE0: Allocate the two buffers of 512 opaque textured 32x48 quads on the
 * 0x380,0x100 page, the second a copy of the first. */
void worldmap_billboards_alloc_quads(void) {
    POLY_FT4 *quad;
    s32 i;

    worldmap_billboard_quads[0] = heap_alloc(sizeof(QuadBlock512), 1);
    worldmap_billboard_quads[1] = heap_alloc(sizeof(QuadBlock512), 1);
    quad = worldmap_billboard_quads[0];
    for (i = 0; i < 0x200; i++, quad++) {
        setPolyFT4(quad);
        setRGB0(quad, 0x80, 0x80, 0x80);
        quad->u0 = 0;
        quad->v0 = 0x40;
        quad->u1 = 0x1F;
        quad->v1 = 0x40;
        quad->u2 = 0;
        quad->v2 = 0x6F;
        quad->u3 = 0x1F;
        quad->v3 = 0x6F;
        quad->clut = GetClut(0xF0, 0x1FF);
        quad->tpage = GetTPage(0, 0, 0x380, 0x100);
    }
    *(QuadBlock512 *)worldmap_billboard_quads[1] = *(QuadBlock512 *)worldmap_billboard_quads[0];
}

/* 80086124: Free the two billboard quad buffers. */
void worldmap_billboards_free_quads(void) {
    heap_free(worldmap_billboard_quads[1]);
    heap_free(worldmap_billboard_quads[0]);
}

/* Scratchpad work area of the billboard pass. */
typedef struct {
    SVECTOR corner[4]; /* the billboard quad */
    u8 pad20[8];
    MATRIX view;       /* 0x28 */
    MATRIX roll;       /* 0x48 */
    u16 clut[16];      /* 0x68 */
} BillboardScratch;

#define BILLBOARD_SCRATCH ((BillboardScratch *)0x1F800000)

/* 8008615C: Draw the billboards of the 5x5 visible terrain blocks: set up the
 * scratchpad quad (upright, 48x72), the camera and roll matrices and the
 * CLUTs, then submit each visible block's billboard list (worldmap_billboards_draw_block)
 * into this display buffer's quads. */
void worldmap_billboards_draw(void) {
    BillboardList *textures;
    s32 index;
    s32 i;
    s32 row;

    BILLBOARD_SCRATCH->corner[0].vx = -0x18;
    BILLBOARD_SCRATCH->corner[0].vy = -0x48;
    BILLBOARD_SCRATCH->corner[0].vz = 0;
    BILLBOARD_SCRATCH->corner[1].vx = 0x18;
    BILLBOARD_SCRATCH->corner[1].vy = -0x48;
    BILLBOARD_SCRATCH->corner[1].vz = 0;
    BILLBOARD_SCRATCH->corner[2].vx = -0x18;
    BILLBOARD_SCRATCH->corner[2].vy = 0;
    BILLBOARD_SCRATCH->corner[2].vz = 0;
    BILLBOARD_SCRATCH->corner[3].vx = 0x18;
    BILLBOARD_SCRATCH->corner[3].vy = 0;
    BILLBOARD_SCRATCH->corner[3].vz = 0;
    BILLBOARD_SCRATCH->view = worldmap_camera_matrix;
    BILLBOARD_SCRATCH->roll = worldmap_identity_matrix;
    RotMatrixZ(-worldmap_camera_angle.vz, &BILLBOARD_SCRATCH->roll);
    for (i = 0; i < 0x10; i++) {
        BILLBOARD_SCRATCH->clut[i] = worldmap_billboard_cluts[i];
    }
    textures = worldmap_billboard_lists;
    worldmap_billboard_quad_count = 0;
    for (row = 0; row < 5; row++) {
        for (i = 0; i < 5; i++) {
            if (worldmap_terrain_visible_blocks[row * 5 + i] != -1) {
                index = worldmap_terrain_grid.cells[(row + worldmap_camera_block_cell.vz) * 9 + i + worldmap_camera_block_cell.vx];
                if (textures[index].count != 0) {
                    worldmap_billboards_draw_block(textures[index].data, textures[index].count, worldmap_current_display_buffer->ot,
                                  (POLY_FT4 *)worldmap_billboard_quads[worldmap_display_buffer_index] + worldmap_billboard_quad_count);
                }
            }
        }
    }
}

/* 800863E0: Scatter the 80 drifting positions: five template points repeated over a
 * 4x4 grid of 0x800-unit cells at random heights, with random velocities. */
void worldmap_clouds_scatter(void) {
    Drift *drift;
    DriftVelocity *velocity;
    s32 row;
    s32 column;
    s32 i;

    worldmap_cloud_positions = heap_alloc(0x50 * sizeof(Drift), 0);
    worldmap_cloud_velocities = heap_alloc(0x50 * sizeof(DriftVelocity), 0);
    drift = worldmap_cloud_positions;
    for (row = 0; row < 4; row++) {
        for (column = 0; column < 4; column++) {
            for (i = 0; i < 5; i++, drift++) {
                drift->x = (worldmap_cloud_template_points[i].x + (column << 11)) << 12;
                drift->z = (worldmap_cloud_template_points[i].z + (row << 11)) << 12;
                drift->unk4 = (-(rand() >> 10) * 8 - 0x200) << 12;
            }
        }
    }
    velocity = worldmap_cloud_velocities;
    for (i = 0; i < 0x50; i++) {
        column = (rand() & 3) + 1; /* reused as the speed */
        velocity->dx = column * 0xDDB;
        velocity->dz = -(column << 11);
        velocity->unk2 = rand() & 1;
        velocity++;
    }
}

/* 80086568: Free the clouds' positions and velocities. */
void worldmap_clouds_free(void) {
    heap_free(worldmap_cloud_velocities);
    heap_free(worldmap_cloud_positions);
}

/* The cloud quads, copied between display buffers as a whole. */
typedef struct {
    POLY_FT4 quads[0x120];
} QuadBlock288;

/* 800865A0: Allocate the two buffers of 0x120 semi-transparent grey textured quads
 * on the 0x3C0,0x100 page, the second a copy of the first. */
void worldmap_clouds_alloc_quads(void) {
    POLY_FT4 *quad;
    s32 i;
    s32 code;
    s32 colour;
    s32 length;

    worldmap_cloud_quads[0] = heap_alloc(sizeof(QuadBlock288), 1);
    worldmap_cloud_quads[1] = heap_alloc(sizeof(QuadBlock288), 1);
    colour = 0x26;
    i = 0;
    quad = worldmap_cloud_quads[0];
    length = 9;
    code = 0x2C;
    for (; i < 0x120; i++, quad++) {
        setlen(quad, length);
        setcode(quad, code);
        setRGB0(quad, colour, colour, colour);
        quad->tpage = GetTPage(0, 1, 0x3C0, 0x100);
        quad->clut = GetClut(0x130, 0x1FE);
        SetSemiTrans(quad, 1);
    }
    *(QuadBlock288 *)worldmap_cloud_quads[1] = *(QuadBlock288 *)worldmap_cloud_quads[0];
}

/* 800866C8: Free the two cloud quad buffers. */
void worldmap_clouds_free_quads(void) {
    heap_free(worldmap_cloud_quads[1]);
    heap_free(worldmap_cloud_quads[0]);
}

/* 80086700: Move the 80 drifting positions, wrapping them on the 0x2000-unit world. */
void worldmap_clouds_move(void) {
    Drift *drift;
    DriftVelocity *velocity;
    s32 x;
    s32 z;
    s32 i;

    for (i = 0; i < 0x50; i++) {
        drift = &worldmap_cloud_positions[i];
        velocity = &worldmap_cloud_velocities[i];
        x = drift->x + velocity->dx;
        z = drift->z + velocity->dz;
        if (x > 0x1FFFFFF) {
            x -= 0x2000000;
        }
        if (x < 0) {
            x += 0x2000000;
        }
        if (z > 0x1FFFFFF) {
            z -= 0x2000000;
        }
        if (z < 0) {
            z += 0x2000000;
        }
        drift->x = x;
        drift->z = z;
    }
}

/* Scratchpad work area of the drifting sprites (clouds). */
typedef struct {
    SVECTOR far[12];      /* 0x000: far sprite corners (worldmap_cloud_far_corners) */
    SVECTOR near[48];     /* 0x060: near sprite quads, 4 corners each (worldmap_cloud_near_corners) */
    u16 uv[8];            /* 0x1E0: texture origins (worldmap_cloud_texture_origins) */
    SVECTOR corner[4];    /* 0x1F0: built quad corners */
    VECTOR origin;        /* 0x210: camera target on the 0x2000-unit world */
    VECTOR cell;          /* 0x220 */
    s32 edge[2];          /* 0x230: packed view-cone edges */
    s32 pad238[2];
    VECTOR view;          /* 0x240: view point relative to the eye */
    MATRIX local;         /* 0x250 */
    MATRIX screen;        /* 0x270 */
    MATRIX turn;          /* 0x290 */
    MATRIX work;          /* 0x2B0 */
    s32 uv_index;         /* 0x2D0 */
    s32 flag;             /* 0x2D4 */
    s32 z;                /* 0x2D8 */
    s32 sz[4];            /* 0x2DC */
    SVECTOR *vertices;    /* 0x2EC */
    s32 count;            /* 0x2F0: quads added this frame */
    s32 quads;            /* 0x2F4: quads considered */
} DriftScratch;

#define DRIFT_SCRATCH ((DriftScratch *)0x1F800000)

/* Link a 9-word primitive into an ordering-table entry. */
#define addPrimLen9(ot, p) \
    __asm__ volatile("lw $12, 0(%0);" \
                     "lui $13, 0x0900;" \
                     "or $12, $12, $13;" \
                     "lui $13, 0x00FF;" \
                     "ori $13, $13, 0xFFFF;" \
                     "and $13, %1, $13;" \
                     "sw $13, 0(%0);" \
                     "sw $12, 0(%1)" \
                     : \
                     : "r"(ot), "r"(p) \
                     : "$12", "$13", "memory")

/* The projected quad lies partly on the 320x216 screen. */
#define ON_SCREEN(a, b, c, d)                                                                 \
    (((u16)(a) < 0x140 || (u16)(b) < 0x140 || (u16)(c) < 0x140 || (u16)(d) < 0x140) &&         \
     ((u32)(a) >> 16 < 0xD8 || (u32)(b) >> 16 < 0xD8 || (u32)(c) >> 16 < 0xD8 || (u32)(d) >> 16 < 0xD8))

/* 80086798: Draw the 80 drifting cloud sprites around the camera. Each is culled against
 * the view cone, then drawn by distance: far as one to three 64-texel quads,
 * middle as three 4-quad layers of 32 texels, near as three layers of 4x4
 * generated 16-texel quads. At most 0xF1 quads are added per frame. x and z
 * hold the sprite's offset from the camera, then a near layer's base corner,
 * and count the middle rows and the near rows and columns; layer first holds
 * the packed view-cone test point. */
/* The four projected corners are register variables: the original keeps them
 * in t6-t9 ($14/$15/$24/$25) in all three branches. In the near loop these are
 * the only temporaries left: gte_stflg and addPrimLen9 use $12/$13 (t4/t5), and
 * t0-t3 hold the quad's store pointer, layer, the column offset and quad. As
 * plain locals the corners (48 loop-weighted refs over 176-199 insns each) rank
 * in global allocation above layer (53 over 380), the column offset (19 over
 * 118) and quad (46 over 493) and take t1/t2/t3/t6. Bound to t6-t9 the
 * function matches exactly. No compiler release or flag places plain locals
 * there (a replay of GCC 2.7.2's global allocation shows they cannot reach
 * t6-t9), and the corners are read with the three-mfc2-plus-nop shapes of
 * LIBGTE.H's register-argument read_sxsy macros, so the original most likely
 * declared them as register variables too. */
void worldmap_clouds_draw(void) {
    DriftScratch *scratch;
    POLY_FT4 *quad;
    s32 i;
    s32 layer;
    s32 x;
    s32 z;
    s32 cx;
    s32 cz;
    u16 uv;
    register s32 sxy0 asm("$14");
    register s32 sxy1 asm("$15");
    register s32 sxy2 asm("$24");
    register s32 sxy3 asm("$25");

    (*worldmap_cloud_draw_hook)();
    scratch = DRIFT_SCRATCH;
    for (i = 0; i < 0x30; i++) {
        scratch->near[i] = worldmap_cloud_near_corners[i];
    }
    for (i = 0; i < 0xC; i++) {
        scratch->far[i] = worldmap_cloud_far_corners[i];
    }
    for (i = 0; i < 8; i++) {
        scratch->uv[i] = worldmap_cloud_texture_origins[i];
    }
    scratch->origin.vx = worldmap_camera.target.vx & 0x1FFFFFF;
    scratch->origin.vz = worldmap_camera.target.vz & 0x1FFFFFF;
    scratch->cell.vx = (worldmap_camera.target.vx >> 12) & 0x7FF;
    scratch->cell.vz = (worldmap_camera.target.vz >> 12) & 0x7FF;
    scratch->local = worldmap_identity_matrix;
    scratch->screen = scratch->local;
    scratch->turn = scratch->local;
    RotMatrixX((worldmap_camera_angle.vx + 0x400) / 8, &scratch->local);
    RotMatrixY(-worldmap_camera_angle.vy, &scratch->screen);
    RotMatrixY(worldmap_camera_angle.vy, &scratch->turn);
    MulMatrix0(&scratch->local, &scratch->screen, &scratch->work);
    MulMatrix0(&scratch->turn, &scratch->work, &scratch->local);
    scratch->edge[0] = (gpu_get_cos(worldmap_camera_angle.vy - 0x169) << 16) | (gpu_get_sin(worldmap_camera_angle.vy - 0x169) & 0xFFFF);
    scratch->edge[1] = (gpu_get_cos(worldmap_camera_angle.vy + 0x169) << 16) | (gpu_get_sin(worldmap_camera_angle.vy + 0x169) & 0xFFFF);
    scratch->view.vx = VIEW_VECTORS[0].vx * 2 + (-gpu_get_sin(worldmap_camera_angle.vy) >> 1);
    scratch->view.vz = VIEW_VECTORS[0].vz * 2 + (-gpu_get_cos(worldmap_camera_angle.vy) >> 1);
    quad = worldmap_cloud_quads[worldmap_display_buffer_index];
    scratch->quads = 0;
    scratch->count = 0;
    for (i = 0; i < 0x50; i++) {
        if (scratch->count > 0xF0) {
            break;
        }
        x = (worldmap_cloud_positions[i].x - scratch->origin.vx) >> 12;
        z = (worldmap_cloud_positions[i].z - scratch->origin.vz) >> 12;
        if (x < -0x1000) {
            x += 0x2000;
        } else if (x >= 0x1000) {
            x -= 0x2000;
        }
        if (z < -0x1000) {
            z += 0x2000;
        } else if (z >= 0x1000) {
            z -= 0x2000;
        }
        z = -z;
        layer = ((z - scratch->view.vz) << 16) | ((x - scratch->view.vx) & 0xFFFF);
        gte_ldsxy3(layer, scratch->edge[1], 0);
        gte_nclip();
        gte_stopz(&scratch->flag);
        if (scratch->flag > 0) {
            continue;
        }
        layer = ((z - scratch->view.vz) << 16) | ((x - scratch->view.vx) & 0xFFFF);
        gte_ldsxy3(0, scratch->edge[0], layer);
        gte_nclip();
        gte_stopz(&scratch->flag);
        if (scratch->flag > 0) {
            continue;
        }
        scratch->local.t[0] = x;
        scratch->local.t[1] = worldmap_cloud_positions[i].unk4 >> 12;
        scratch->local.t[2] = z;
        gte_CompMatrix(&worldmap_camera_matrix, &scratch->local, &scratch->screen);
        gte_SetRotMatrix(&scratch->screen);
        gte_SetTransMatrix(&scratch->screen);
        scratch->corner[0].vx = scratch->corner[0].vy = scratch->corner[0].vz = 0;
        gte_ldv0(&scratch->corner[0]);
        gte_rtps();
        scratch->uv_index = worldmap_cloud_velocities[i].unk2 * 4;
        gte_stflg(&scratch->flag);
        if (scratch->flag & 0x7F85E000) {
            continue;
        }
        gte_stsz(&scratch->sz[0]);
        if (scratch->sz[0] > 0x580) {
            /* far: up to three 64-texel quads, fewer as the depth grows */
            scratch->vertices = scratch->far;
            for (layer = 0; layer < 3; layer++) {
                gte_ldv3c(scratch->vertices);
                gte_rtpt();
                uv = scratch->uv[scratch->uv_index++];
                gte_stflg(&scratch->flag);
                if (!(scratch->flag & 0x80000000)) {
                    gte_getsxy3(sxy0, sxy1, sxy2);
                    gte_ldv0(&scratch->vertices[3]);
                    gte_rtps();
                    gte_stflg(&scratch->flag);
                    if (!(scratch->flag & 0x80000000)) {
                        gte_getsxy2(sxy3);
                        if (ON_SCREEN(sxy0, sxy1, sxy2, sxy3)) {
                            gte_stsz4c(scratch->sz);
                            if (scratch->sz[0] > scratch->sz[1]) {
                                scratch->z = scratch->sz[0];
                            } else {
                                scratch->z = scratch->sz[1];
                            }
                            if (scratch->sz[2] > scratch->z) {
                                scratch->z = scratch->sz[2];
                            } else if (scratch->sz[3] > scratch->z) {
                                scratch->z = scratch->sz[3];
                            }
                            if (scratch->z > 0xD00) {
                                break;
                            }
                            addPrimLen9(worldmap_current_display_buffer->ot + (scratch->z >> 4), quad);
                            *(u16 *)&quad->u1 = uv | 0x3F;
                            *(u16 *)&quad->u2 = uv | 0x3F00;
                            *(s32 *)&quad->x0 = sxy0;
                            *(s32 *)&quad->x1 = sxy1;
                            *(s32 *)&quad->x2 = sxy2;
                            *(s32 *)&quad->x3 = sxy3;
                            *(u16 *)&quad->u0 = uv;
                            *(u16 *)&quad->u3 = uv | 0x3F3F;
                            quad++;
                            scratch->count++;
                            if (scratch->z > 0xB00) {
                                break;
                            }
                        }
                    }
                }
                scratch->vertices += 4;
                scratch->quads++;
            }
        } else if (scratch->sz[0] > 0x400) {
            /* middle: three layers of four 32-texel quads */
            scratch->vertices = scratch->near;
            for (layer = 0; layer < 3; layer++) {
                for (x = 0; x < 4; x++) {
                    gte_ldv3c(scratch->vertices);
                    gte_rtpt();
                    uv = scratch->uv[layer + scratch->uv_index] + ((x & 2) << 12) + ((x & 1) << 5);
                    gte_stflg(&scratch->flag);
                    if (!(scratch->flag & 0x80000000)) {
                        gte_getsxy3(sxy0, sxy1, sxy2);
                        gte_ldv0(&scratch->vertices[3]);
                        gte_rtps();
                        gte_stflg(&scratch->flag);
                        if (!(scratch->flag & 0x80000000)) {
                            gte_getsxy2(sxy3);
                            if (ON_SCREEN(sxy0, sxy1, sxy2, sxy3)) {
                                gte_stsz4c(scratch->sz);
                                if (scratch->sz[0] > scratch->sz[1]) {
                                    scratch->z = scratch->sz[0];
                                } else {
                                    scratch->z = scratch->sz[1];
                                }
                                if (scratch->sz[2] > scratch->z) {
                                    scratch->z = scratch->sz[2];
                                } else if (scratch->sz[3] > scratch->z) {
                                    scratch->z = scratch->sz[3];
                                }
                                addPrimLen9(worldmap_current_display_buffer->ot + (scratch->z >> 4), quad);
                                *(u16 *)&quad->u1 = uv | 0x1F;
                                *(u16 *)&quad->u2 = uv | 0x1F00;
                                *(s32 *)&quad->x0 = sxy0;
                                *(s32 *)&quad->x1 = sxy1;
                                *(s32 *)&quad->x2 = sxy2;
                                *(s32 *)&quad->x3 = sxy3;
                                *(u16 *)&quad->u0 = uv;
                                *(u16 *)&quad->u3 = uv | 0x1F1F;
                                quad++;
                                scratch->count++;
                            }
                        }
                    }
                    scratch->vertices += 4;
                    scratch->quads++;
                }
            }
        } else {
            /* near: three layers of 4x4 16-texel quads over a 0x180 square */
            for (layer = 0; layer < 3; layer++) {
                x = scratch->corner[0].vx = scratch->corner[2].vx = scratch->far[0].vx;
                z = scratch->corner[0].vz = scratch->corner[1].vz = scratch->far[0].vz;
                scratch->corner[1].vx = scratch->corner[3].vx = x + 0x180;
                scratch->corner[2].vz = scratch->corner[3].vz = z - 0x180;
                scratch->corner[0].vy = scratch->corner[1].vy = scratch->corner[2].vy = scratch->corner[3].vy =
                    scratch->far[0].vy - layer * 8;
                gte_ldv0(&scratch->corner[0]);
                gte_rtps();
                gte_stflg(&scratch->flag);
                if (scratch->flag & 0x7F85E000) {
                    continue;
                }
                gte_ldv3c(&scratch->corner[1]);
                gte_rtpt();
                gte_stflg(&scratch->flag);
                if (scratch->flag & 0x7F85E000) {
                    continue;
                }
                for (x = 0; x < 4; x++) {
                    for (z = 0; z < 4; z++) {
                        cx = scratch->corner[0].vx = scratch->corner[2].vx = scratch->far[0].vx + z * 0x60;
                        cz = scratch->corner[0].vz = scratch->corner[1].vz = scratch->far[0].vz - x * 0x60;
                        scratch->corner[1].vx = scratch->corner[3].vx = cx + 0x60;
                        scratch->corner[2].vz = scratch->corner[3].vz = cz - 0x60;
                        gte_ldv3c(&scratch->corner[0]);
                        gte_rtpt();
                        uv = scratch->uv[layer + scratch->uv_index] + ((x << 12) + (z << 4));
                        gte_stflg(&scratch->flag);
                        if (!(scratch->flag & 0x7F85E000)) {
                            gte_getsxy3(sxy0, sxy1, sxy2);
                            gte_ldv0(&scratch->corner[3]);
                            gte_rtps();
                            gte_stflg(&scratch->flag);
                            if (!(scratch->flag & 0x7F85E000)) {
                                gte_getsxy2(sxy3);
                                if (ON_SCREEN(sxy0, sxy1, sxy2, sxy3)) {
                                    gte_stsz4c(scratch->sz);
                                    if (scratch->sz[0] > scratch->sz[1]) {
                                        scratch->z = scratch->sz[0];
                                    } else {
                                        scratch->z = scratch->sz[1];
                                    }
                                    if (scratch->sz[2] > scratch->z) {
                                        scratch->z = scratch->sz[2];
                                    } else if (scratch->sz[3] > scratch->z) {
                                        scratch->z = scratch->sz[3];
                                    }
                                    addPrimLen9(worldmap_current_display_buffer->ot + (scratch->z >> 4), quad);
                                    *(u16 *)&quad->u1 = uv | 0xF;
                                    *(u16 *)&quad->u2 = uv | 0xF00;
                                    *(s32 *)&quad->x0 = sxy0;
                                    *(s32 *)&quad->x1 = sxy1;
                                    *(s32 *)&quad->x2 = sxy2;
                                    *(s32 *)&quad->x3 = sxy3;
                                    *(u16 *)&quad->u0 = uv;
                                    *(u16 *)&quad->u3 = uv | 0xF0F;
                                    quad++;
                                    scratch->count++;
                                }
                            }
                        }
                        scratch->quads++;
                    }
                }
            }
        }
    }
}

/* 80087710: Reset an actor to step 0 with parameter 8. */
s32 worldmap_spinning_pair_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->unk54 = 8;
    return 1;
}

/* 80087734: Spin scene objects 12 and 14 about their own axis. */
s32 worldmap_spinning_pair_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;
    SVECTOR *first;
    SVECTOR *second;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    first = SCRIPT_VECTOR;
    second = SCRIPT_VECTOR + 1;
    second->vx = 0;
    first->vx = 0;
    first->vy = objects[12].angle.vy;
    second->vy = objects[14].angle.vy;
    first->vz = second->vz = actor->u.step;
    gpu_build_rotation_matrix(first, &objects[12].matrix);
    gpu_build_rotation_matrix(second, &objects[14].matrix);
    return 1;
}

/* 800877E0: Reset an actor to step 0 with parameter 8. */
s32 worldmap_area_spinning_pair_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->unk54 = 8;
    return 1;
}

/* 80087804: Spin the current area's two scene objects about their own axis. */
s32 worldmap_area_spinning_pair_update(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    SceneObject *objects;
    SceneObject *first;
    SceneObject *second;
    s32 first_id;
    s32 second_id;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    first_id = worldmap_area_spinning_pair_objects[worldmap_area_index][0];
    second_id = worldmap_area_spinning_pair_objects[worldmap_area_index][1];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    SCRIPT_VECTOR[1].vx = 0;
    SCRIPT_VECTOR[0].vx = 0;
    first = &objects[first_id];
    SCRIPT_VECTOR[0].vy = first->angle.vy;
    second = &objects[second_id];
    SCRIPT_VECTOR[1].vy = second->angle.vy;
    SCRIPT_VECTOR[0].vz = SCRIPT_VECTOR[1].vz = actor->u.step;
    gpu_build_rotation_matrix(&SCRIPT_VECTOR[0], &first->matrix);
    gpu_build_rotation_matrix(&SCRIPT_VECTOR[1], &second->matrix);
    return 1;
}

/* 80087904: Give `count` quads the semi-transparent 0x1A0,0xA0 texture page. */
void worldmap_rolling_pair_make_translucent(SceneObject *object, POLY_FT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        quads->tpage = GetTPage(0, abr, 0x1A0, 0xA0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT4));
}

/* 800879A8: Reset an actor to step 0 with parameter 0x10 and rebuild the area's two
 * scene objects. */
s32 worldmap_area_rolling_pair_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->u.step = 0;
    actor->unk54 = 0x10;
    worldmap_area_rolling_pair_rebuild(index);
    return 1;
}

/* 800879E0: Rebuild the primitives of the current area's two scene objects. */
s32 worldmap_area_rolling_pair_rebuild(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    SceneObject *first;
    SceneObject *second;

    first = &worldmap_objects[worldmap_area_rolling_pair_objects[worldmap_area_index][0]];
    second = &worldmap_objects[worldmap_area_rolling_pair_objects[worldmap_area_index][1]];
    worldmap_rolling_pair_make_translucent(first, first->prims, first->def->primitive_count, 3);
    worldmap_rolling_pair_make_translucent(second, second->prims, second->def->primitive_count, 3);
    return 1;
}

/* 80087A8C: Roll the current area's two scene objects together. */
s32 worldmap_area_rolling_pair_update(s32 index) {
    s32 unused[2]; /* unreferenced; the original frame reserves it */
    WorldmapActor *actor;
    SceneObject *objects;
    SceneObject *first;
    SceneObject *second;
    s32 first_id;
    s32 second_id;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    first_id = worldmap_area_rolling_pair_objects[worldmap_area_index][0];
    second_id = worldmap_area_rolling_pair_objects[worldmap_area_index][1];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    first = &objects[first_id];
    SCRIPT_VECTOR[1].vz = 0;
    SCRIPT_VECTOR[0].vz = 0;
    SCRIPT_VECTOR[1].vx = 0;
    SCRIPT_VECTOR[0].vx = 0;
    SCRIPT_VECTOR[0].vy = SCRIPT_VECTOR[1].vy = actor->u.step;
    second = &objects[second_id];
    gpu_build_rotation_matrix(&SCRIPT_VECTOR[0], &first->matrix);
    gpu_build_rotation_matrix(&SCRIPT_VECTOR[1], &second->matrix);
    return 1;
}

/* 80087B84: Build a rotation matrix whose third row faces `direction`, using `up`
 * as scratch for the first two rows. */
void worldmap_build_facing_matrix(VECTOR *direction, VECTOR *up, MATRIX *m) {
    up->vz = 0;
    up->vx = 0;
    up->vy = 0x1000;
    OuterProduct12(up, direction, up);
    VectorNormal(up, up);
    m->m[0][0] = up->vx;
    m->m[0][1] = up->vy;
    m->m[0][2] = up->vz;
    OuterProduct12(direction, up, up);
    VectorNormal(up, up);
    m->m[1][0] = up->vx;
    m->m[1][1] = up->vy;
    m->m[1][2] = up->vz;
    m->m[2][0] = direction->vx;
    m->m[2][1] = direction->vy;
    m->m[2][2] = direction->vz;
    libgte_transpose_matrix(m, m);
}

/* Compiled-out debug trace of the ferry's resumed position. */
#define FERRY_TRACE_POSITION(actor) do { } while (0)

/* Scratchpad work area of the ferry update. */
typedef struct {
    VECTOR work;
    VECTOR up;           /* 0x10 */
    u8 pad20[0x80];
    SVECTOR wake;        /* 0xA0 */
    SVECTOR wake_angle;  /* 0xA8 */
    u8 padB0[0x40];
    MATRIX m;            /* 0xF0 */
    u8 pad110[0x40];
    MATRIX m2;           /* 0x150 */
} FerryScratch;

#define FERRY_SCRATCH ((FerryScratch *)0x1F800000)

/* 80087C6C: Start the area's ferry: before scene 0xCD it rests at a fixed dock;
 * otherwise it resumes its route (first time: at waypoint 0), advancing
 * when within 8 units of the waypoint, and heads for the next one. */
s32 worldmap_ferry_start(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    FerryScratch *scratch;
    s32 id;
    s32 distance;
    s32 i;
    u16 z;

    id = worldmap_area_ferry_objects[worldmap_area_index];
    worldmap_ferry_link_objects(index);
    scratch = FERRY_SCRATCH;
    actor = &worldmap_actor_slots[index];
    actor->turn = 0x4000;
    actor->state = 0;
    object = &worldmap_objects[id];
    if (game_data.vars[0] < 0xCD) {
        actor->position.vx = 0xD80000;
        actor->position.vz = 0x7280000;
        object->matrix = worldmap_identity_matrix;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    } else {
        if (game_data.unk184A == 0) {
            game_data.unk184A++;
            actor->u.step = 0;
            actor->position.vx = worldmap_ferry_waypoints_x[actor->u.step] << 12;
            z = worldmap_ferry_waypoints_z[actor->u.step];
        } else {
            actor->u.step = game_data.unk1844[2];
            actor->position.vx = game_data.unk1844[0] << 12;
            z = game_data.unk1844[1];
        }
        actor->position.vz = z << 12;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        FERRY_TRACE_POSITION(actor);
        scratch->work.vx = worldmap_ferry_waypoints_x[actor->u.step] << 12;
        scratch->work.vz = worldmap_ferry_waypoints_z[actor->u.step] << 12;
        distance = worldmap_get_ground_distance(&actor->position, &scratch->work);
        if (distance < 0) {
            distance = -distance;
        }
        if (distance < 8) {
            actor->u.step = (actor->u.step + 1) & 7;
        }
        scratch->work.vx = worldmap_ferry_waypoints_x[actor->u.step] - (actor->position.vx >> 12);
        scratch->work.vz = worldmap_ferry_waypoints_z[actor->u.step] - (actor->position.vz >> 12);
        scratch->work.vy = 0;
        worldmap_wrap_world_offset(&scratch->work);
        VectorNormal(&scratch->work, &scratch->work);
        for (i = 0; i < 0x20; i++) {
            worldmap_ferry_headings[i].dx = scratch->work.vx;
            worldmap_ferry_headings[i].dz = scratch->work.vz;
        }
        actor->unk58 = 1;
        actor->unk54 = 0;
        actor->motion.vx = worldmap_ferry_headings[1].dx;
        actor->motion.vz = worldmap_ferry_headings[actor->unk58].dz;
        actor->motion.vy = 0;
    }
    return 1;
}
/* 80087F60: Link the four objects before the area's scene object to it. The actor
 * dispatcher supplies an index, which this area-wide handler does not use. */
s32 worldmap_ferry_link_objects(s32 index) {
    u16 object;

    object = worldmap_area_ferry_objects[worldmap_area_index];
    worldmap_objects_link(object, object - 4);
    worldmap_objects_link(object, object - 3);
    worldmap_objects_link(object, object - 2);
    worldmap_objects_link(object, object - 1);
    return 1;
}

/* 80087FD0: Run the ferry: before scene 0xCD it only follows the ground; otherwise
 * it steers along its waypoints, turns its model, moves by its delayed
 * heading history at a speed that eases in and out near the dock, sprays
 * its wake above speed 0x1000 and saves its route state. */
s32 worldmap_ferry_update(s32 index) {
    FerryScratch *scratch;
    WorldmapActor *actor;
    SceneObject *object;
    s32 distance;
    s32 dock;

    scratch = FERRY_SCRATCH;
    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[worldmap_area_ferry_objects[worldmap_area_index]];
    if (game_data.vars[0] < 0xCD) {
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
    } else {
        scratch->work.vx = worldmap_ferry_waypoints_x[actor->u.step] << 12;
        scratch->work.vz = worldmap_ferry_waypoints_z[actor->u.step] << 12;
        distance = worldmap_get_ground_distance(&actor->position, &scratch->work);
        if (distance < 0) {
            distance = -distance;
        }
        if (distance < 8) {
            actor->u.step = (actor->u.step + 1) & 7;
        }
        scratch->work.vx = worldmap_ferry_waypoints_x[actor->u.step] - (actor->position.vx >> 12);
        scratch->work.vz = worldmap_ferry_waypoints_z[actor->u.step] - (actor->position.vz >> 12);
        scratch->work.vy = 0;
        worldmap_wrap_world_offset(&scratch->work);
        VectorNormal(&scratch->work, &scratch->work);
        actor->motion.vx = ((scratch->work.vx + actor->motion.vx * 63) << 6) >> 12;
        actor->motion.vz = ((scratch->work.vz + actor->motion.vz * 63) << 6) >> 12;
        VectorNormal(&actor->motion, &actor->motion);
        worldmap_ferry_headings[actor->unk54].dx = actor->motion.vx;
        worldmap_ferry_headings[actor->unk54].dz = actor->motion.vz;
        actor->unk54 = (actor->unk54 + 1) & 0x1F;
        scratch->work.vx = actor->motion.vx;
        scratch->work.vy = actor->motion.vy;
        scratch->work.vz = -actor->motion.vz;
        worldmap_build_facing_matrix(&scratch->work, &scratch->up, &scratch->m);
        object->matrix = scratch->m;
        actor->position.vx += worldmap_ferry_headings[actor->unk58].dx * (actor->turn >> 12);
        actor->position.vz += worldmap_ferry_headings[actor->unk58].dz * (actor->turn >> 12);
        actor->unk58 = (actor->unk58 + 1) & 0x1F;
        worldmap_wrap_position(&actor->position);
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
        scratch->work.vx = game_data.worldmap.unk60 - (actor->position.vx >> 12);
        scratch->work.vz = game_data.worldmap.unk64 - (actor->position.vz >> 12);
        scratch->work.vy = (s16)game_data.worldmap.unk62;
        worldmap_wrap_world_offset(&scratch->work);
        dock = SquareRoot0(scratch->work.vx * scratch->work.vx + scratch->work.vz * scratch->work.vz);
        if (dock < 0x100 && scratch->work.vy >= -0xBF) {
            actor->turn = 0;
        } else if (dock < 0x300 && scratch->work.vy >= -0xBF) {
            actor->turn -= 0x100;
            if (actor->turn < 0) {
                actor->turn = 0;
            }
        } else {
            actor->turn += 0x200;
            if (actor->turn > 0x4000) {
                actor->turn = 0x4000;
            }
        }
        if (actor->turn > 0x1000) {
            scratch->wake.vx = object->position.vx;
            scratch->wake.vy = object->position.vy;
            scratch->wake.vz = object->position.vz;
            libgte_transpose_matrix(&object->matrix, &scratch->m2);
            worldmap_get_matrix_angles(&scratch->m2, &scratch->wake_angle);
            worldmap_effects_start_emitters(0x13, &scratch->wake, &scratch->wake_angle);
        } else {
            worldmap_effects_stop_emitters(0x13);
        }
    }
    game_data.unk1844[0] = actor->position.vx >> 12;
    game_data.unk1844[1] = actor->position.vz >> 12;
    game_data.unk1844[2] = actor->u.step;
    scratch->work.vx = actor->position.vx;
    scratch->work.vy = actor->position.vy + 0x30000;
    scratch->work.vz = actor->position.vz;
    worldmap_queue_actor_cylinder(index, &scratch->work, 0x80, 0xB0);
    return 1;
}

/* 80088570: Start the actor circling above the map from the saved position (first
 * time: from 0, 0x480), and save it back. */
s32 worldmap_airship_start(s32 index) {
    WorldmapActor *actor;

    worldmap_airship_link_objects();
    actor = &worldmap_actor_slots[index];
    actor->unk54 = 4;
    actor->state = 0;
    actor->u.step = 0;
    actor->unk58 = 0;
    actor->unk5C = 0xC;
    if (game_data.flight.count == 0) {
        game_data.flight.count++;
        actor->position.vy = -0x280000;
        actor->position.vx = 0;
        actor->position.vz = 0x4800000;
    } else {
        actor->position.vx = (game_data.flight.x << 12) + game_data.flight.x_frac;
        actor->position.vy = -0x280000;
        actor->position.vz = (game_data.flight.z << 12) + game_data.flight.z_frac;
    }
    actor->motion.vz = 0xB50;
    actor->motion.vx = 0xB50;
    actor->motion.vy = 0;
    actor->turn = 1;
    game_data.flight.x_frac = actor->position.vx;
    game_data.flight.x = actor->position.vx >> 12;
    game_data.flight.z_frac = actor->position.vz;
    game_data.flight.z = actor->position.vz >> 12;
    return 1;
}

/* 8008868C: Link the area's scene objects in the listed (parent, child) pairs. */
s32 worldmap_airship_link_objects(void) {
    s32 i;
    s32 base;

    base = worldmap_area_airship_objects[worldmap_area_index];
    for (i = 0; worldmap_airship_object_links[i] != -1; i += 2) {
        worldmap_objects_link(base + worldmap_airship_object_links[i], base + worldmap_airship_object_links[i + 1]);
    }
    return 1;
}

/* Scratchpad work area of the airship update. */
typedef struct {
    VECTOR work;
    u8 pad10[0x90];
    SVECTOR rotor;       /* 0xA0 */
    SVECTOR tail;        /* 0xA8 */
    u8 padB0[0x40];
    MATRIX rotor_matrix; /* 0xF0 */
    MATRIX tail_matrix;  /* 0x110 */
} FlightScratch;

#define FLIGHT_SCRATCH ((FlightScratch *)0x1F800000)

/* 80088720: Fly the airship: spin its rotors, stop over the saved landing point when
 * low enough, move, place its shadow object and save the position. */
s32 worldmap_airship_update(s32 index) {
    WorldmapActor *actor;
    FlightScratch *scratch;
    s32 base;

    actor = &worldmap_actor_slots[index];
    base = worldmap_area_airship_objects[worldmap_area_index];
    actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
    actor->unk58 = (actor->unk58 + actor->unk5C) & 0xFFF;
    FLIGHT_SCRATCH->rotor.vx = FLIGHT_SCRATCH->rotor.vz = 0;
    FLIGHT_SCRATCH->rotor.vy = actor->u.step;
    FLIGHT_SCRATCH->tail.vx = FLIGHT_SCRATCH->tail.vz = 0;
    FLIGHT_SCRATCH->tail.vy = actor->unk58;
    RotMatrixYXZ(&FLIGHT_SCRATCH->rotor, &FLIGHT_SCRATCH->rotor_matrix);
    RotMatrixYXZ(&FLIGHT_SCRATCH->tail, &FLIGHT_SCRATCH->tail_matrix);
    worldmap_objects[base + 5].matrix = FLIGHT_SCRATCH->rotor_matrix;
    scratch = FLIGHT_SCRATCH;
    worldmap_objects[base].matrix = worldmap_objects[base + 1].matrix = worldmap_objects[base + 2].matrix =
        worldmap_objects[base + 3].matrix = FLIGHT_SCRATCH->tail_matrix;
    scratch->work.vx = game_data.worldmap.unk60 - (actor->position.vx >> 12);
    scratch->work.vz = game_data.worldmap.unk64 - (actor->position.vz >> 12);
    scratch->work.vy = (s16)game_data.worldmap.unk62;
    worldmap_wrap_world_offset(&scratch->work);
    if (SquareRoot0(scratch->work.vx * scratch->work.vx + scratch->work.vz * scratch->work.vz) < 0x300 && scratch->work.vy < -0x240) {
        actor->turn = 0;
    } else {
        actor->turn = 1;
    }
    actor->position.vx += actor->motion.vx * actor->turn;
    actor->position.vz += actor->motion.vz * actor->turn;
    worldmap_wrap_position(&actor->position);
    scratch->work.vx = actor->position.vx >> 12;
    scratch->work.vy = actor->position.vy >> 12;
    scratch->work.vz = actor->position.vz >> 12;
    worldmap_objects[base + 12].position = scratch->work;
    worldmap_queue_actor_cylinder(index, &actor->position, 0x180, 0xC0);
    game_data.flight.x_frac = actor->position.vx;
    game_data.flight.x = actor->position.vx >> 12;
    game_data.flight.z_frac = actor->position.vz;
    game_data.flight.z = actor->position.vz >> 12;
    return 1;
}

/* 80088B40: Link the listed scene objects to object 69 and place it at the actor;
 * outside scene 0x99 also show them. */
s32 worldmap_object69_start(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 i;
    s32 result;

    for (i = 0; worldmap_object69_children[i] != -1; i++) {
        worldmap_objects_link(69, worldmap_object69_children[i]);
    }
    actor = &worldmap_actor_slots[index];
    object = worldmap_objects;
    actor->state = 0;
    if (game_data.vars[0] == 0x99) {
        result = 1;
        actor->position.vx = 0x2000000;
        actor->position.vz = 0x4120000;
        actor->position.vy = 0;
    } else {
        object[69].visible = 1;
        for (i = 0; worldmap_object69_children[i] != -1; i++) {
            object[worldmap_object69_children[i]].visible = 1;
        }
        result = 3;
    }
    object += 69;
    object->position.vx = actor->position.vx >> 12;
    object->position.vy = actor->position.vy >> 12;
    object->position.vz = actor->position.vz >> 12;
    return result;
}

/* 80088C90: Show scene object 69 and the listed objects. */
s32 worldmap_object69_resume(void) {
    SceneObject *objects;
    s32 i;

    objects = worldmap_objects;
    objects[69].visible = 1;
    for (i = 0; worldmap_object69_children[i] != -1; i++) {
        objects[worldmap_object69_children[i]].visible = 1;
    }
    return 3;
}

/* 80088D00: In scene 0x99, move scene object 69 to the actor (world units). */
s32 worldmap_object69_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    object = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    object += 69;
    if (game_data.vars[0] == 0x99) {
        object->position.vx = actor->position.vx >> 12;
        object->position.vy = actor->position.vy >> 12;
        object->position.vz = actor->position.vz >> 12;
    }
    return 1;
}

/* 80088D64: Place the actor just above the current area's scene object. */
s32 worldmap_raised_placement_start(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = &worldmap_objects[worldmap_area_raised_placement_objects[worldmap_area_index]];
    actor = &worldmap_actor_slots[index];
    actor->position.vx = object->position.vx << 12;
    actor->position.vy = (object->position.vy << 12) + 0x40000;
    actor->position.vz = object->position.vz << 12;
    return 1;
}

/* 80088DE4: Place the actor at (0x68, 0x60). */
s32 worldmap_raised_placement_update(s32 index) {
    worldmap_queue_actor_cylinder(index, &worldmap_actor_slots[index].position, 0x68, 0x60);
    return 1;
}

/* 80088E1C: Place the actor at scene object 75. */
s32 worldmap_object75_placement_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->position.vx = worldmap_objects[75].position.vx << 12;
    actor->position.vy = worldmap_objects[75].position.vy << 12;
    actor->position.vz = worldmap_objects[75].position.vz << 12;
    return 1;
}

/* 80088E68: Place the actor at (0x10C, 0x1A6). */
s32 worldmap_object75_placement_update(s32 index) {
    worldmap_queue_actor_cylinder(index, &worldmap_actor_slots[index].position, 0x10C, 0x1A6);
    return 1;
}

/* 80088EA0: Place the actor at the current area's scene object. */
s32 worldmap_ground_placement_start(s32 index) {
    SceneObject *object;
    WorldmapActor *actor;

    object = &worldmap_objects[worldmap_area_ground_placement_objects[worldmap_area_index]];
    actor = &worldmap_actor_slots[index];
    actor->position.vx = object->position.vx << 12;
    actor->position.vy = object->position.vy << 12;
    actor->position.vz = object->position.vz << 12;
    return 1;
}

/* 80088F1C: Place the actor at (0xC9, 0x392). */
s32 worldmap_ground_placement_update(s32 index) {
    worldmap_queue_actor_cylinder(index, &worldmap_actor_slots[index].position, 0xC9, 0x392);
    return 1;
}

/* 80088F54: Scene step with nothing to do. */
s32 worldmap_area_idle_actor_start(void) {
    return 3;
}

/* 80088F5C: Scene step with nothing to do. */
s32 worldmap_area_idle_actor_update(void) {
    return 3;
}

/* 80088F64: Clear the area objects and allocate the effect slots. */
void worldmap_effects_alloc_slots(void) {
    AreaObject *object;
    EffectSlot *slot;
    s32 i;

    object = worldmap_effect_emitters;
    for (i = 0x1FF; i != -1; i--) {
        object->unk4 = 0;
        object->unkA = 0;
        object->unk12 = 0;
        object->position.vz = 0;
        object->position.vy = 0;
        object->position.vx = 0;
        object->angle.vz = 0;
        object->angle.vy = 0;
        object->angle.vx = 0;
        object++;
    }
    worldmap_effect_slots = slot = heap_alloc(0x4C00, 0);
    for (i = 0xFF; i != -1; i--) {
        EFFECT_ENABLED(slot) = 0;
        EFFECT_COUNT(slot) = 0;
        slot++;
    }
}

/* 80088FF4: Free the effect table. */
void worldmap_effects_free_slots(void) {
    heap_free(worldmap_effect_slots);
}

/* The particle quads, copied between display buffers as a whole. */
typedef struct {
    POLY_FT4 quads[256];
} EffectQuads;

/* 8008901C: Allocate the two effect quad buffers: semi-transparent textured quads
 * on the 0x340,0x100 page, the second a copy of the first. */
void worldmap_effects_alloc_quads(void) {
    POLY_FT4 *quad;
    s32 i;

    worldmap_effect_quads[0] = heap_alloc(sizeof(EffectQuads), 1);
    worldmap_effect_quads[1] = heap_alloc(sizeof(EffectQuads), 1);
    quad = worldmap_effect_quads[0];
    for (i = 0xFF; i != -1; i--) {
        setPolyFT4(quad);
        quad->tpage = GetTPage(1, 1, 0x340, 0x100);
        quad->clut = GetClut(0x100, 0x1FF);
        setSemiTrans(quad, 1);
        quad++;
    }
    *(EffectQuads *)worldmap_effect_quads[1] = *(EffectQuads *)worldmap_effect_quads[0];
}

/* 80089128: Free the two particle quad buffers. */
void worldmap_effects_free_quads(void) {
    heap_free(worldmap_effect_quads[0]);
    heap_free(worldmap_effect_quads[1]);
}

/* Short vectors handled as a word (vx, vy) plus vz. */
#define SVECTOR_ZERO(v) (*(s32 *)&(v)->vx = 0, (v)->vz = 0)

/* 80089160: Place the eight emitters of group `group` at `position` facing `angle`
 * (either may be NULL for zero); start them unless one is already live. */
void worldmap_effects_start_emitters(s32 group, SVECTOR *position, SVECTOR *angle) {
    AreaObject *object;
    s32 live;
    s32 i;

    live = 0;
    object = &worldmap_effect_emitters[group * 8];
    for (i = 7; i != -1; i--) {
        if (object->flags & 0x80) {
            live++;
            break;
        }
    }
    object = &worldmap_effect_emitters[group * 8];
    if ((position == NULL) & (angle == NULL)) {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            *(s32 *)&object->position.vx = *(s32 *)&object->angle.vx = 0;
            object->position.vz = object->angle.vz = 0;
        }
    } else if ((position != NULL) & (angle == NULL)) {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            object->position = *position;
            SVECTOR_ZERO(&object->angle);
        }
    } else if ((position == NULL) & (angle != NULL)) {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            SVECTOR_ZERO(&object->position);
            object->angle.vx = -angle->vx;
            object->angle.vy = -angle->vy;
            object->angle.vz = -angle->vz;
        }
    } else {
        for (i = 7; i != -1; object++, i--) {
            if (live == 0) {
                object->flags |= 0x80;
                object->unk4 = object->unk0;
                object->unk12 = object->unk10;
                object->unkA = 0;
            }
            object->position = *position;
            object->angle.vx = -angle->vx;
            object->angle.vy = -angle->vy;
            object->angle.vz = -angle->vz;
        }
    }
}

/* 800893E0: Place the eight emitters of group `group` at the origin, aimed along
 * `direction` and turned by `angle`; start them unless one is already live. */
void worldmap_effects_aim_emitters(s32 group, SVECTOR *direction, SVECTOR *angle) {
    AreaObject *object;
    s32 live;
    s32 i;

    live = 0;
    object = &worldmap_effect_emitters[group * 8];
    for (i = 7; i != -1; i--) {
        if (object->flags & 0x80) {
            live++;
            break;
        }
    }
    object = &worldmap_effect_emitters[group * 8];
    for (i = 7; i != -1; object++, i--) {
        if (live == 0) {
            object->flags |= 0x80;
            object->unk4 = object->unk0;
            object->unk12 = object->unk10;
            object->unkA = 0;
        }
        *(s32 *)&object->direction.vx = *(s32 *)&direction->vx;
        *(s32 *)&object->position.vx = 0;
        *(s32 *)&object->angle.vx = *(s32 *)&angle->vx;
        object->direction.vz = direction->vz;
        object->position.vz = 0;
        object->angle.vz = angle->vz;
    }
}

/* 800894C8: Deactivate the eight area objects of group `group`. */
void worldmap_effects_stop_emitters(s32 group) {
    AreaObject *object;
    s32 i;

    object = &worldmap_effect_emitters[group * 8];
    for (i = 7; i != -1; i--) {
        object->flags &= 0x7F;
        object++;
    }
}

/* 80089514: Stop the effects that belong to group `group`. */
void worldmap_effects_stop_particles(s32 group) {
    EffectSlot *slot;
    s32 i;
    s32 j;

    slot = worldmap_effect_slots;
    for (i = 0xFF; i != -1; i--) {
        for (j = 0; j < 8; j++) {
            if (slot->id == group * 8 + j && EFFECT_ENABLED(slot) != 0) {
                EFFECT_COUNT(slot) = 0;
                break;
            }
        }
        slot++;
    }
}

/* 80089580: Age the particles: move, accelerate, spin and fade the live ones; when a
 * particle's life runs out, release it from its emitter. */
void worldmap_effects_age_particles(void) {
    EffectSlot *slot;
    s32 step;   /* life word, then the colour step */
    s32 colour; /* live flag, then the colour */
    s32 i;
    s32 r;
    s32 g;
    s32 b;
    s32 px, py, pz;
    s32 vx, vy, vz;

    slot = worldmap_effect_slots;
    for (i = 0xFF; i != -1; slot++, i--) {
        step = slot->timer;
        colour = step >> 16;
        step = (s16)step;
        if (colour != 0) {
            if (step > 0) {
                px = slot->position.vx;
                py = slot->position.vy;
                pz = slot->position.vz;
                vx = slot->velocity.vx;
                vy = slot->velocity.vy;
                vz = slot->velocity.vz;
                colour = slot->colour;
                EFFECT_COUNT(slot)--;
                step = slot->fade;
                px += vx;
                py += vy;
                pz += vz;
                vx += slot->accel.vx;
                vy += slot->accel.vy;
                vz += slot->accel.vz;
                r = (colour & 0xFF) + (s8)step;
                g = ((colour >> 8) & 0xFF) + (s8)(step >> 8);
                b = ((colour >> 16) & 0xFF) + (s8)(step >> 16);
                slot->rot[0] += slot->spin[0];
                slot->rot[1] += slot->spin[1];
                if (r < 0) {
                    r = 0;
                }
                if (r > 0xFF) {
                    r = 0xFF;
                }
                if (g < 0) {
                    g = 0;
                }
                if (g > 0xFF) {
                    g = 0xFF;
                }
                if (b < 0) {
                    b = 0;
                }
                if (b > 0xFF) {
                    b = 0xFF;
                }
                slot->colour = (colour & 0xFF000000) | (b << 16) | (g << 8) | r;
                slot->position.vx = px;
                slot->position.vy = py;
                slot->position.vz = pz;
                slot->velocity.vx = vx;
                slot->velocity.vy = vy;
                slot->velocity.vz = vz;
            } else {
                worldmap_effect_emitters[slot->id].unkA--;
                slot->id = 0;
                slot->timer = 0;
            }
        }
    }
}

/* Scratchpad work area of the emitters. */
typedef struct {
    VECTOR normal;  /* 0x00 */
    VECTOR random;  /* 0x10 */
    VECTOR offset;  /* 0x20 */
    u8 pad30[0xC0];
    MATRIX m;       /* 0xF0 */
} EmitScratch;

#define EMIT_SCRATCH ((EmitScratch *)0x1F800000)

/* 80089748: Run the emitters: count down their timers and every interval spawn one
 * particle into a free effect slot, starting at a random point around the
 * emitter and flying towards a random point around its target. */
void worldmap_effects_run_emitters(void) {
    AreaObject *object;
    EffectSlot *slot;
    EmitScratch *scratch;
    s32 i;
    s32 flags;
    /* The original initializes these timer locals only for flag 0x10. */
    s32 delay;
    s32 repeats;
    s32 value;

    object = worldmap_effect_emitters;
    scratch = EMIT_SCRATCH;
    for (i = 0; i < 0x200; i++, object++) {
        flags = object->flags;
        if (!(flags & 0x80)) {
            continue;
        }
        if (flags & 0x10) {
            delay = object->unk4;
            repeats = delay >> 16;
            delay = (s16)delay;
            if (delay != 0) {
                delay--;
                goto store;
            }
            if (repeats == 0) {
                goto expire;
            }
            repeats--;
        }
        if (object->unk12 != 0) {
            object->unk12--;
            goto store;
        }
        object->unk12 = object->unk10;
        if ((((s16 *)&object->life)[1] != 0) & (object->unk8 > 0) & (object->unkA < object->unk8)) {
            value = 0xFF;
            slot = worldmap_effect_slots;
            for (; value != -1; value--, slot++) {
                if (EFFECT_ENABLED(slot) == 0) {
                    slot->id = i;
                    slot->timer = object->life;
                    RotMatrixYXZ(&object->angle, &scratch->m);
                    ApplyMatrix(&scratch->m, &object->unk24, &scratch->offset);
                    if (!(flags & 0x20)) {
                        scratch->random.vy = (rand() & 0xFFF) - 0x800;
                    } else {
                        scratch->random.vy = 0;
                    }
                    scratch->random.vx = (rand() & 0xFFF) - 0x800;
                    scratch->random.vz = (rand() & 0xFFF) - 0x800;
                    VectorNormal(&scratch->random, &scratch->normal);
                    if (flags & 4) {
                        value = object->spread[0];
                    } else {
                        value = rand() % object->spread[0];
                    }
                    slot->position.vx = (scratch->offset.vx << 12) + scratch->normal.vx * value +
                                        (object->position.vx << 12);
                    slot->position.vy = (scratch->offset.vy << 12) + scratch->normal.vy * value +
                                        (object->position.vy << 12);
                    slot->position.vz = (scratch->offset.vz << 12) + scratch->normal.vz * value +
                                        (object->position.vz << 12);
                    ApplyMatrix(&scratch->m, &object->direction, &scratch->offset);
                    if (!(flags & 0x40)) {
                        scratch->random.vy = (rand() & 0xFFF) - 0x800;
                    } else {
                        scratch->random.vy = 0;
                    }
                    scratch->random.vx = (rand() & 0xFFF) - 0x800;
                    scratch->random.vz = (rand() & 0xFFF) - 0x800;
                    VectorNormal(&scratch->random, &scratch->normal);
                    if (flags & 8) {
                        value = object->spread[1];
                    } else {
                        value = rand() % object->spread[1];
                    }
                    scratch->normal.vx = (scratch->offset.vx << 12) + scratch->normal.vx * value +
                                         (object->position.vx << 12);
                    scratch->normal.vy = (scratch->offset.vy << 12) + scratch->normal.vy * value +
                                         (object->position.vy << 12);
                    scratch->normal.vz = (scratch->offset.vz << 12) + scratch->normal.vz * value +
                                         (object->position.vz << 12);
                    scratch->normal.vx = (scratch->normal.vx - slot->position.vx) >> 12;
                    scratch->normal.vy = (scratch->normal.vy - slot->position.vy) >> 12;
                    scratch->normal.vz = (scratch->normal.vz - slot->position.vz) >> 12;
                    VectorNormal(&scratch->normal, &scratch->random);
                    slot->velocity.vx = (scratch->random.vx * object->speed) >> 12;
                    slot->velocity.vy = (scratch->random.vy * object->speed) >> 12;
                    slot->velocity.vz = (scratch->random.vz * object->speed) >> 12;
                    flags &= 3; /* only the blend mode is used from here */
                    slot->unk2 = ratan2(scratch->random.vy, scratch->random.vx);
                    slot->accel.vx = object->accel[0];
                    slot->accel.vy = object->accel[1];
                    slot->accel.vz = object->accel[2];
                    slot->colour = *(s32 *)object->rgb;
                    slot->fade = object->fade;
                    *(s32 *)slot->rot = object->rot;
                    *(s32 *)slot->spin = object->spin;
                    slot->code = (flags << 5) | 0x9D;
                    object->unkA++;
                    goto store;
                }
            }
        }
        goto store;
    expire:
        object->flags ^= 0x80;
    store:
        object->unk4 = (repeats << 16) | delay;
    }
    worldmap_effects_age_particles();
}

/* Scratchpad work area of the particle pass. */
typedef struct {
    SVECTOR v[4];       /* 0x00: quad corners */
    SVECTOR centre;     /* 0x20 */
    MATRIX view;        /* 0x28 */
    MATRIX m;           /* 0x48 */
    MATRIX identity;    /* 0x68 */
    VECTOR offset;      /* 0x88 */
    VECTOR scale;       /* 0x98 */
    s32 padA8;
    s32 flag;           /* 0xAC */
    s32 sz;             /* 0xB0 */
} ParticleScratch;

#define PARTICLE_SCRATCH ((ParticleScratch *)0x1F800000)

/* 80089C78: Draw the live particles: build each one's billboard quad (kind shape,
 * scaled and optionally rolled), place it relative to the camera target,
 * project it and add the visible ones to the ordering table. */
void worldmap_effects_draw_particles(void) {
    ParticleScratch *scratch;
    EffectSlot *slot;
    POLY_FT4 *quad;
    s32 camera_x;
    s32 camera_z;
    s32 i;

    scratch = (ParticleScratch *)0x1F800000;
    PARTICLE_SCRATCH->view = worldmap_camera_matrix;
    PARTICLE_SCRATCH->identity = worldmap_identity_matrix;
    i = 0;
    camera_x = worldmap_camera.target.vx >> 12;
    camera_z = worldmap_camera.target.vz >> 12;
    quad = worldmap_effect_quads[worldmap_display_buffer_index];
    slot = worldmap_effect_slots;
    for (; i < 0x100; i++, slot++) {
        if (EFFECT_ENABLED(slot) == 0) {
            continue;
        }
        scratch->scale.vx = (u16)slot->rot[0];
        scratch->scale.vy = (u16)slot->rot[1];
        scratch->scale.vz = 0x1000;
        scratch->m = scratch->identity;
        if (((u8 *)&slot->fade)[3] & 1) {
            RotMatrixZ(slot->unk2, &scratch->m);
        }
        ScaleMatrix(&scratch->m, &scratch->scale);
        scratch->v[0] = worldmap_effect_particle_shapes[EFFECT_ENABLED(slot)].v[0];
        scratch->v[1] = worldmap_effect_particle_shapes[EFFECT_ENABLED(slot)].v[1];
        scratch->v[2] = worldmap_effect_particle_shapes[EFFECT_ENABLED(slot)].v[2];
        scratch->v[3] = worldmap_effect_particle_shapes[EFFECT_ENABLED(slot)].v[3];
        scratch->offset.vx = (slot->position.vx >> 12) - camera_x;
        scratch->offset.vz = (slot->position.vz >> 12) - camera_z;
        worldmap_wrap_world_offset(&scratch->offset);
        scratch->centre.vx = scratch->offset.vx;
        scratch->centre.vy = slot->position.vy >> 12;
        scratch->centre.vz = -scratch->offset.vz;
        gte_SetRotMatrix(&scratch->view);
        gte_ldv0(&scratch->centre);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        scratch->m.t[0] = scratch->offset.vx + scratch->view.t[0];
        scratch->m.t[1] = scratch->offset.vy + scratch->view.t[1];
        scratch->m.t[2] = scratch->offset.vz + scratch->view.t[2];
        gte_SetRotMatrix(&scratch->m);
        gte_SetTransMatrix(&scratch->m);
        gte_ldv3(&scratch->v[0], &scratch->v[1], &scratch->v[2]);
        gte_rtpt();
        gte_stflg(&scratch->flag);
        if (scratch->flag & 0x80000000) {
            continue;
        }
        gte_stsxy3(&quad->x0, &quad->x1, &quad->x2);
        gte_ldv0(&scratch->v[3]);
        gte_rtps();
        gte_stflg(&scratch->flag);
        if (scratch->flag & 0x80000000) {
            continue;
        }
        gte_stsxy(&quad->x3);
        if (!(quad->x0 < 0x140 || quad->x1 < 0x140 || quad->x2 < 0x140 || quad->x3 < 0x140)) {
            continue;
        }
        if (!(quad->y0 < 0xD8 || quad->y1 < 0xD8 || quad->y2 < 0xD8 || quad->y3 < 0xD8)) {
            continue;
        }
        gte_stsz(&scratch->sz);
        if (scratch->sz < 0xC00) {
            quad->r0 = ((u8 *)&slot->colour)[0];
            quad->g0 = ((u8 *)&slot->colour)[1];
            quad->b0 = ((u8 *)&slot->colour)[2];
            quad->tpage = slot->code;
            *(u16 *)&quad->u0 = worldmap_effect_particle_uvs[EFFECT_ENABLED(slot) * 4];
            *(u16 *)&quad->u1 = worldmap_effect_particle_uvs[EFFECT_ENABLED(slot) * 4 + 1];
            *(u16 *)&quad->u2 = worldmap_effect_particle_uvs[EFFECT_ENABLED(slot) * 4 + 2];
            *(u16 *)&quad->u3 = worldmap_effect_particle_uvs[EFFECT_ENABLED(slot) * 4 + 3];
            addPrim(&worldmap_current_display_buffer->ot[scratch->sz >> 4], quad);
            quad++;
        }
    }
}

/* 8008A2C8: Create the party leader's model sprite at the scene's entry position; in
 * movement modes 1-7 follow the player or start hidden. Fill the position
 * trail and save the position as the world-map return point. */
s32 worldmap_leader_start(s32 index) {
    WorldmapActor *actor;
    TrailPoint *point;
    s32 i;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_character_models[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x1800);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    actor->unk24 = 0;
    actor->position.vx = game_data.vars[0] << 12;
    actor->position.vz = game_data.vars[1] << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = worldmap_player_heading;
    actor->turn = 8;
    actor->unk5C = actor->heading;
    switch (worldmap_movement_mode) {
    case 1:
    case 2:
    case 3:
        if (game_data.inGear[0] == 0) {
            actor->position.vx = worldmap_player_position.vx;
            actor->position.vz = worldmap_player_position.vz;
            actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
            actor->heading = worldmap_player_heading;
            worldmap_camera_follow_target.target = actor->position;
            worldmap_camera_follow_heading = actor->heading;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (game_data.party[0] != 0xFF) {
            game_data.inGear[0] = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (game_data.party[0] != 0xFF) {
            game_data.inGear[0] = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    worldmap_trail_index = 0;
    i = 0;
    point = worldmap_trail_points;
    do {
        point->position = actor->position;
        i++;
        point->heading = actor->heading;
        point++;
    } while (i < 0x20);
    game_data.worldmap.x = actor->position.vx >> 12;
    game_data.worldmap.z = actor->position.vz >> 12;
    game_data.worldmap.heading = actor->heading;
    return 1;
}

/* 8008A52C: Create the lead party member's model sprite for the actor. */
s32 worldmap_leader_resume(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_character_models[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x1800);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    return 1;
}

/* 8008A5B8: Create the party leader's model sprite at the saved world-map position
 * and fill the position trail and saved camera target with it. */
s32 worldmap_leader_start_at_saved_spot(s32 index) {
    WorldmapActor *actor;
    TrailPoint *point;
    s32 i;
    s16 heading;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_character_models[0], 0x100, 0x1E0, 0x140, 0x100, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x1800);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    actor->position.vx = game_data.worldmap.x << 12;
    actor->position.vz = game_data.worldmap.z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    i = 0;
    point = worldmap_trail_points;
    heading = game_data.worldmap.heading;
    actor->unk24 = 1;
    actor->turn = 8;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->state = 2;
    worldmap_trail_index = 0;
    actor->heading = heading;
    do {
        point->position = actor->position;
        i++;
        point->heading = actor->heading;
        point++;
    } while (i < 0x20);
    worldmap_camera_follow_target.target = actor->position;
    worldmap_camera_follow_heading = actor->heading;
    return 1;
}

/* Scratchpad work area of the party leader. */
typedef struct {
    VECTOR target;  /* 0x00 */
    u8 pad10[0x20];
    VECTOR start;   /* 0x30 */
    u8 pad40[0x50];
    VECTOR probe;   /* 0x90: move probe result */
    u16 heading;    /* 0xA0 */
} LeaderScratch;

/* 8008A72C: Update the party leader on foot: walk by the pad and record the trail,
 * gather the others into a vehicle or let them out on command, walk out of
 * the parked vehicle, and save the return spot and heading. */
s32 worldmap_leader_update(s32 index) {
    WorldmapActor *actor;
    WorldmapActor *target;
    LeaderScratch *scratch;
    TrailPoint *point;
    s32 result;
    s16 value;
    u16 heading;

    result = 1;
    scratch = (LeaderScratch *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    switch (actor->unk4) {
    case 2:
        actor->unk4 = 0;
        actor->state = 0x28;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 6:
        actor->unk4 = 0;
        if (worldmap_party_count == ++actor->unk58) {
            actor->state = 0;
            worldmap_movement_mode = 1;
            worldmap_actor_cylinder_count = 0;
        }
        break;
    }
    switch (actor->state) {
    case 0:
    case 1:
        if (game_data.inGear[0] == 0) {
            switch (worldmap_steer_on_foot(actor)) {
            case 2:
            case 4:
            case 5:
                break;
            case 1:
                actor->state = 0x40;
                worldmap_loop_running = 0;
                worldmap_loop_result = 0;
                break;
            case 3:
                if (worldmap_cylinder_hit_actor == 7) {
                    actor->state = 8;
                } else {
                    worldmap_cylinder_hit_actor = 4;
                    actor->state = 0x10;
                }
                break;
            default:
                if ((actor->motion.vx == 0) & (actor->motion.vy == 0) & (actor->motion.vz == 0)) {
                    if (SPRITE_ANIMATION(actor->handle) != 0) {
                        sprite_start_animation(actor->handle, 0);
                        worldmap_effects_stop_emitters(0x2F);
                    }
                } else {
                    if (SPRITE_ANIMATION(actor->handle) != 1) {
                        sprite_start_animation(actor->handle, 1);
                    }
                    worldmap_actor_emit_on_layer3(0x2F, actor, (ActorScratch *)scratch);
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
                    worldmap_find_cylinder_hit(&scratch->probe, 0x10, 0x20, &worldmap_cylinder_hit, &worldmap_cylinder_hit_actor);
                    if (worldmap_cylinder_hit_actor == 7) {
                        value = worldmap_cylinder_hit + 3;
                    } else {
                        value = worldmap_cylinder_hit;
                    }
                    if ((u16)worldmap_cylinder_hit_standable[value] != 0) {
                        actor->position = scratch->probe;
                        if (actor->motion.vx | actor->motion.vz) {
                            value = (worldmap_trail_index + 1) & 0x1F;
                            point = &worldmap_trail_points[value];
                            worldmap_trail_index = value;
                            point->position = actor->position;
                            point->heading = actor->heading;
                            worldmap_encounter_update_timers();
                        }
                    }
                } else {
                    worldmap_find_cylinder_hit(&actor->position, 0x10, 0x20, &worldmap_cylinder_hit, &worldmap_cylinder_hit_actor);
                }
                worldmap_path_select_region(&actor->position, 0);
                actor->motion.vz = 0;
                actor->motion.vy = 0;
                actor->motion.vx = 0;
                worldmap_camera_follow_target.target = actor->position;
                worldmap_camera_follow_heading = actor->heading;
                break;
            }
            worldmap_actor_cylinder_count = 0;
            actor->unk24 = 0;
        } else {
            actor->unk24 = 1;
            actor->position.vx = worldmap_actor_slots[4].position.vx;
            actor->position.vy = worldmap_actor_slots[4].position.vy;
            actor->position.vz = worldmap_actor_slots[4].position.vz;
            actor->heading = worldmap_actor_slots[4].heading;
            worldmap_effects_stop_emitters(0x2F);
        }
        break;
    case 2:
    case 3:
        actor->position.vx = worldmap_actor_slots[7].position.vx;
        actor->position.vy = worldmap_actor_slots[7].position.vy;
        actor->position.vz = worldmap_actor_slots[7].position.vz;
        actor->heading = worldmap_actor_slots[7].heading;
        break;
    case 8:
        if (game_data.party[1] != 0xFF) {
            if (game_data.inGear[1] == 0) {
                worldmap_actor_request(2, 1);
                actor[1].unk6 = worldmap_cylinder_hit_actor;
                worldmap_actor_request(5, 8);
            } else {
                worldmap_actor_request(5, 1);
                worldmap_actor_slots[5].unk6 = worldmap_cylinder_hit_actor;
            }
        }
        actor->state++;
        break;
    case 9:
        if (game_data.party[2] != 0xFF) {
            if (game_data.inGear[2] == 0) {
                worldmap_actor_request(3, 1);
                actor[2].unk6 = worldmap_cylinder_hit_actor;
                worldmap_actor_request(6, 8);
            } else {
                worldmap_actor_request(6, 1);
                worldmap_actor_slots[6].unk6 = worldmap_cylinder_hit_actor;
            }
        }
        actor->state++;
        break;
    case 10:
        if (game_data.party[0] != 0xFF) {
            if (worldmap_actor_request(4, 8) != 0) {
                actor->state = 0xD;
            }
        } else {
            actor->state = 0xD;
        }
        break;
    case 0xD:
        worldmap_get_heading_to(&actor->position, &worldmap_actor_slots[worldmap_cylinder_hit_actor].position, &actor->motion, &actor->heading);
        target = (WorldmapActor *)((worldmap_cylinder_hit_actor * sizeof(*target)) + (u32)worldmap_actor_slots);
        actor->u.step = target->position.vx >> 12;
        actor->unk54 = target->position.vz >> 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
        /* fallthrough */
    case 0xE:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state++;
        }
        worldmap_camera_follow_target.target = actor->position;
        worldmap_camera_follow_heading = actor->heading;
        worldmap_actor_emit_on_layer3(index + 0x2E, actor, (ActorScratch *)scratch);
        break;
    case 0xF:
        if (worldmap_actor_request(worldmap_cylinder_hit_actor, 4) != 0) {
            actor->unk24 = 1;
            actor->state = 2;
            worldmap_effects_stop_emitters(0x2F);
        }
        break;
    case 0x10:
        if (game_data.party[1] != 0xFF) {
            if (worldmap_actor_request(2, 1) != 0) {
                actor[1].unk6 = 5;
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 0x11:
        if (game_data.party[2] != 0xFF) {
            if (worldmap_actor_request(3, 1) != 0) {
                actor[2].unk6 = 6;
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 0x12:
        point = worldmap_trail_points;
        worldmap_trail_index = 0;
        scratch->start.vx = VEHICLE_SPOTS[0].x << 12;
        scratch->start.vz = VEHICLE_SPOTS[0].z << 12;
        scratch->start.vy = worldmap_terrain_get_height(scratch->start.vx, scratch->start.vz);
        scratch->heading = game_data.worldmap.unk5A;
        value = 0x1F;
        do {
            point->position = scratch->start;
            point->heading = scratch->heading;
            point++;
        } while (--value != -1);
        actor->state = 0xD;
        break;
    case 0x28:
        actor->position.vx = VEHICLE_SPOTS[0].x << 12;
        actor->position.vz = VEHICLE_SPOTS[0].z << 12;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        heading = game_data.worldmap.unk5A;
        actor->unk5C = heading;
        actor->heading = heading;
        scratch->target.vx = actor->position.vx + gpu_get_sin(actor->heading) * 0x30;
        scratch->target.vz = actor->position.vz + -gpu_get_cos(actor->heading) * 0x30;
        worldmap_get_heading_to(&actor->position, &scratch->target, &actor->motion, &actor->heading);
        actor->u.step = scratch->target.vx >> 12;
        actor->unk54 = scratch->target.vz >> 12;
        actor->unk24 = 0;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
        break;
    case 0x29:
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
    case 0x2A:
        point = worldmap_trail_points;
        game_data.inGear[0] = 0;
        actor->unk58 = 1;
        worldmap_trail_index = 0;
        scratch->target = actor->position;
        scratch->heading = actor->heading;
        value = 0x1F;
        do {
            point->position = scratch->target;
            point->heading = scratch->heading;
            point++;
        } while (--value != -1);
        actor->state++;
        /* fallthrough */
    case 0x2B:
        switch (worldmap_party_count) {
        case 1:
            actor->state = 1;
            break;
        case 2:
            if (game_data.inGear[1] == 0) {
                actor->state++;
            }
            break;
        case 3:
            if ((game_data.inGear[1] | game_data.inGear[2]) == 0) {
                actor->state++;
            }
            break;
        }
        break;
    case 0x2C:
        if (game_data.party[1] != 0xFF) {
            if (worldmap_actor_request(2, 5) != 0) {
                actor->state++;
            }
        } else {
            actor->state++;
        }
        break;
    case 0x2D:
        if (game_data.party[2] == 0xFF || worldmap_actor_request(3, 5) != 0) {
            actor->state = 0x40;
        }
        break;
    case 0x40:
        break;
    }
    game_data.worldmap.x = actor->position.vx >> 12;
    game_data.worldmap.z = actor->position.vz >> 12;
    game_data.worldmap.heading = actor->heading;
    if (actor->unk24 == 0) {
        worldmap_footprints_add(0, &actor->position);
    }
    return result;
}

/* 8008B2BC: Create party member 2's model sprite (if present) at the saved world-map
 * position; in movement modes 1-7 follow the player or start hidden. */
s32 worldmap_second_member_start(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = 1;
    if (game_data.party[1] != 0xFF) {
        actor->handle = sprite_create(worldmap_character_models[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
        sprite_start_animation(actor->handle, 0);
        sprite_set_scale(actor->handle, 0x1800);
        actor->handle->render.word &= ~SPRITE_HIDDEN;
        actor->unk24 = 0;
    } else {
        actor->unk24 = 1;
        result = 3;
    }
    actor->position.vx = game_data.worldmap.x << 12;
    actor->position.vz = game_data.worldmap.z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = game_data.worldmap.heading;
    actor->turn = 8;
    actor->unk58 = 0xF;
    actor->unk5C = actor->heading;
    switch (worldmap_movement_mode) {
    case 1:
    case 2:
    case 3:
        if (game_data.inGear[1] == 0) {
            actor->position.vx = worldmap_player_position.vx;
            actor->position.vz = worldmap_player_position.vz;
            actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
            actor->heading = worldmap_player_heading;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (game_data.party[1] != 0xFF) {
            game_data.inGear[1] = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (game_data.party[1] != 0xFF) {
            game_data.inGear[1] = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    return result;
}

/* 8008B498: Create the second party member's model sprite, if present. */
s32 worldmap_second_member_resume(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = 1;
    if (game_data.party[1] != 0xFF) {
        actor->handle = sprite_create(worldmap_character_models[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
        sprite_start_animation(actor->handle, 0);
        sprite_set_scale(actor->handle, 0x1800);
        actor->handle->render.word &= ~SPRITE_HIDDEN;
    } else {
        result = 3;
    }
    return result;
}

/* 8008B54C: Create party member 2's model sprite at the saved world-map position. */
s32 worldmap_second_member_start_at_saved_spot(s32 index) {
    WorldmapActor *actor;
    s16 heading;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_character_models[1], 0x110, 0x1E0, 0x150, 0x100, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x1800);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    actor->position.vx = game_data.worldmap.x << 12;
    actor->position.vz = game_data.worldmap.z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    heading = game_data.worldmap.heading;
    actor->unk24 = 1;
    actor->turn = 8;
    actor->state = 2;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk58 = 0xF;
    actor->heading = heading;
    return 1;
}

/* 8008B644: Party follower: a command (1-3, 5) starts a scripted walk; otherwise
 * follow the vehicle trail a fixed delay behind (or stand at the leader
 * while riding), walk to a target, walk out of or back to the vehicle,
 * and keep the model on the ground. */
s32 worldmap_follower_update(s32 index) {
    WorldmapActor *actor;
    WorldmapActor *target;
    TrailPoint *point;
    VECTOR *work;
    s32 result;
    u16 heading;
    VECTOR unused; /* unreferenced; the original frame reserves it */

    result = 1;
    work = (VECTOR *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 8;
        target = &worldmap_actor_slots[actor->unk6];
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0x28;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 0x30;
        break;
    }
    switch (actor->state) {
    case 0:
    case 1:
        if (game_data.inGear[index - 1] == 0) {
            point = &worldmap_trail_points[(worldmap_trail_index - actor->unk58) & 0x1F];
            if ((actor->position.vx == point->position.vx) & (actor->position.vy == point->position.vy) &
                (actor->position.vz == point->position.vz)) {
                if (((s8 *)actor->handle)[0xAF] != 0) {
                    sprite_start_animation(actor->handle, 0);
                    worldmap_effects_stop_emitters(index + 0x2E);
                }
            } else {
                if (((s8 *)actor->handle)[0xAF] != 1) {
                    sprite_start_animation(actor->handle, 1);
                }
                worldmap_actor_emit_on_layer3(index + 0x2E, actor, (ActorScratch *)work);
            }
            actor->position.vx = point->position.vx;
            actor->position.vy = point->position.vy;
            actor->position.vz = point->position.vz;
            actor->heading = point->heading;
            actor->unk24 = 0;
        } else {
            target = &worldmap_actor_slots[index + 3];
            actor->position.vx = target->position.vx;
            actor->position.vy = target->position.vy;
            actor->position.vz = target->position.vz;
            actor->heading = target->heading;
            actor->unk24 = 1;
            worldmap_effects_stop_emitters(index + 0x2E);
        }
        break;
    case 2:
        actor->position.vx = worldmap_actor_slots[7].position.vx;
        actor->position.vy = worldmap_actor_slots[7].position.vy;
        actor->position.vz = worldmap_actor_slots[7].position.vz;
        actor->heading = worldmap_actor_slots[7].heading;
        break;
    case 8:
        worldmap_get_heading_to(&actor->position, &target->position, &actor->motion, &actor->heading);
        actor->u.step = target->position.vx >> 12;
        actor->unk54 = target->position.vz >> 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
        /* fallthrough */
    case 9:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state++;
        }
        worldmap_actor_emit_on_layer3(index + 0x2E, actor, (ActorScratch *)work);
        break;
    case 10:
        if (worldmap_actor_request(actor->unk6, 4) != 0) {
            actor->unk24 = 1;
            actor->state = 2;
            worldmap_effects_stop_emitters(index + 0x2E);
        }
        break;
    case 0x28:
        actor->position.vx = game_data_vehicle_spots_by_follower[index].x << 12;
        actor->position.vz = game_data_vehicle_spots_by_follower[index].z << 12;
        actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
        heading = (&game_data.worldmap.unk5A)[index - 1];
        actor->unk5C = heading;
        actor->heading = heading;
        work->vx = actor->position.vx + gpu_get_sin(actor->heading) * 0x30;
        work->vz = actor->position.vz + -gpu_get_cos(actor->heading) * 0x30;
        worldmap_get_heading_to(&actor->position, work, &actor->motion, &actor->heading);
        actor->u.step = work->vx >> 12;
        actor->unk54 = work->vz >> 12;
        actor->unk24 = 0;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
        goto walk;
    case 0x2A:
        game_data.inGear[index - 1] = 0;
        actor->state = 0x40;
        break;
    case 0x30:
        worldmap_get_heading_to(&actor->position, &worldmap_actor_slots[1].position, &actor->motion, &actor->heading);
        actor->u.step = worldmap_actor_slots[1].position.vx >> 12;
        actor->unk54 = worldmap_actor_slots[1].position.vz >> 12;
        sprite_start_animation(actor->handle, 1);
        actor->state++;
        /* fallthrough */
    case 0x29:
    case 0x31:
    walk:
        if (worldmap_actor_step_to_target(actor) == 3) {
            actor->state++;
        }
        break;
    case 0x32:
        if (worldmap_actor_request(1, 6) != 0) {
            actor->state = 0;
        }
        break;
    case 0x40: /* parked */
        break;
    }
    if (actor->unk24 == 0) {
        worldmap_footprints_add(0, &actor->position);
    }
    return result;
}

/* 8008BB40: Create party member 3's model sprite (if present) at the saved world-map
 * position; in movement modes 1-7 follow the player or start hidden. */
s32 worldmap_third_member_start(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = 1;
    if (game_data.party[2] != 0xFF) {
        actor->handle = sprite_create(worldmap_character_models[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
        sprite_start_animation(actor->handle, 0);
        sprite_set_scale(actor->handle, 0x1800);
        actor->handle->render.word &= ~SPRITE_HIDDEN;
        actor->unk24 = 0;
    } else {
        actor->unk24 = 1;
        result = 3;
    }
    actor->position.vx = game_data.worldmap.x << 12;
    actor->position.vz = game_data.worldmap.z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->heading = game_data.worldmap.heading;
    actor->turn = 8;
    actor->unk58 = 0x1E;
    actor->unk5C = actor->heading;
    switch (worldmap_movement_mode) {
    case 1:
    case 2:
    case 3:
        if (game_data.inGear[2] == 0) {
            actor->position.vx = worldmap_player_position.vx;
            actor->position.vz = worldmap_player_position.vz;
            actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
            actor->heading = worldmap_player_heading;
        } else {
            actor->state = 1;
            actor->unk24 = 1;
        }
        break;
    case 4:
    case 5:
    case 6:
        actor->state = 3;
        actor->unk24 = 1;
        if (game_data.party[2] != 0xFF) {
            game_data.inGear[2] = 1;
        }
        break;
    case 7:
        actor->state = 2;
        actor->unk24 = 1;
        if (game_data.party[2] != 0xFF) {
            game_data.inGear[2] = 1;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        break;
    }
    return result;
}

/* 8008BD1C: Create the third party member's model sprite, if present. */
s32 worldmap_third_member_resume(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &worldmap_actor_slots[index];
    result = 1;
    if (game_data.party[2] != 0xFF) {
        actor->handle = sprite_create(worldmap_character_models[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
        sprite_start_animation(actor->handle, 0);
        sprite_set_scale(actor->handle, 0x1800);
        actor->handle->render.word &= ~SPRITE_HIDDEN;
    } else {
        result = 3;
    }
    return result;
}

/* 8008BDD0: Create party member 3's model sprite at the saved world-map position. */
s32 worldmap_third_member_start_at_saved_spot(s32 index) {
    WorldmapActor *actor;
    s16 heading;

    actor = &worldmap_actor_slots[index];
    actor->handle = sprite_create(worldmap_character_models[2], 0x120, 0x1E0, 0x160, 0x100, 0x40);
    sprite_start_animation(actor->handle, 0);
    sprite_set_scale(actor->handle, 0x1800);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
    actor->position.vx = game_data.worldmap.x << 12;
    actor->position.vz = game_data.worldmap.z << 12;
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    heading = game_data.worldmap.heading;
    actor->unk24 = 1;
    actor->turn = 8;
    actor->state = 2;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk58 = 0x1E;
    actor->heading = heading;
    return 1;
}

/* 8008BEC8: Step the actor towards its target (x, z in world units) at half its
 * speed; bit 0/1 of the result report x/z arrival. */
s32 worldmap_actor_step_to_target(WorldmapActor *actor) {
    s32 arrived;
    s32 distance;
    s32 position;
    s32 cell;

    position = actor->position.vx;
    cell = position >> 12;
    distance = actor->u.step - cell;
    arrived = 0;
    if (ABS(distance) >= 5) {
        actor->position.vx = position + actor->motion.vx * (actor->turn / 2);
    } else {
        arrived = 1;
    }
    position = actor->position.vz;
    cell = position >> 12;
    distance = actor->unk54 - cell;
    if (ABS(distance) >= 5) {
        actor->position.vz = position + actor->motion.vz * (actor->turn / 2);
    } else {
        arrived |= 2;
    }
    worldmap_wrap_position(&actor->position);
    actor->position.vy = worldmap_terrain_get_height(actor->position.vx, actor->position.vz);
    return arrived;
}

/* 8008BFD4: Queue a placement of actor `index` at `position`, facing (x, z). */
void worldmap_queue_actor_cylinder(s32 index, VECTOR *position, s32 x, s32 z) {
    PlaceRequest *request;

    request = &worldmap_actor_cylinders[worldmap_actor_cylinder_count];
    request->actor = index;
    request->px = position->vx;
    request->py = position->vy;
    request->pz = position->vz;
    request->z = (s16)z;
    request->x = x;
    worldmap_actor_cylinder_count = (worldmap_actor_cylinder_count + 1) & 0x1F;
}

/* 8008C040: Find the first queued placement that overlaps a cylinder at `position`
 * (radius, height): *hit is 2 inside its radius, 1 within 16 more units. */
void worldmap_find_cylinder_hit(VECTOR *position, s32 radius, s32 height, u8 *hit, u8 *actor) {
    VECTOR delta;
    VECTOR unused; /* unreferenced; the original frame reserves it */
    PlaceRequest *request;
    s32 i;
    s32 top;
    s32 bottom;
    s32 distance;
    s32 reach;

    *hit = 0;
    *actor = 0;
    request = worldmap_actor_cylinders;
    if (worldmap_actor_cylinder_count <= 0) {
        return;
    }
    for (i = 0; i < worldmap_actor_cylinder_count; i++, request++) {
        top = position->vy;
        if (top < request->py) {
            bottom = top - (height << 12);
            top = request->py;
        } else {
            bottom = request->py - (request->z << 12);
        }
        if ((top - bottom) >> 12 < height + request->z) {
            delta.vx = (request->px - position->vx) >> 12;
            delta.vz = (request->pz - position->vz) >> 12;
            worldmap_wrap_world_offset(&delta);
            distance = SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz);
            reach = request->x + radius;
            if (distance < reach) {
                *hit = 2;
                *actor = request->actor;
                return;
            }
            if (distance < reach + 0x10) {
                *hit = 1;
                *actor = request->actor;
                return;
            }
        }
    }
}

/* 8008C1DC: Emit effect `effect` at the actor while it stands on terrain type 3,
 * otherwise stop the effect group. */
void worldmap_actor_emit_on_layer3(s32 effect, WorldmapActor *actor, ActorScratch *scratch) {
    if (worldmap_terrain_get_layer(&actor->position) == 3) {
        scratch->position.vx = actor->position.vx >> 12;
        scratch->position.vy = actor->position.vy >> 12;
        scratch->position.vz = actor->position.vz >> 12;
        scratch->angle.vz = 0;
        scratch->angle.vx = 0;
        scratch->angle.vy = actor->unk5C;
        worldmap_effects_start_emitters(effect, &scratch->position, &scratch->angle);
        return;
    }
    worldmap_effects_stop_emitters(effect);
}

/* 8008C28C: Create a party member's gear sprite for the actor. */
void worldmap_create_gear_sprite(WorldmapActor *actor, s32 member) {
    actor->handle = sprite_create(worldmap_gear_models[member], worldmap_gear_sprite_x[member], worldmap_gear_sprite_y[member],
                                  worldmap_gear_sprite_width[member], worldmap_gear_sprite_height[member], 0x40);
    if (game_data.inGear[member] == 1) {
        sprite_start_animation(actor->handle, 0);
    } else {
        sprite_start_animation(actor->handle, 3);
    }
    sprite_set_scale(actor->handle, 0x2000);
    actor->handle->render.word &= ~SPRITE_HIDDEN;
}
