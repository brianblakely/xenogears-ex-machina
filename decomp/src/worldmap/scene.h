#ifndef WORLDMAP_SCENE_H
#define WORLDMAP_SCENE_H

/* The world map's scene objects, the areas' own actors and the scripted
 * scene modes. Scene objects (worldmap_objects_effects_party) are the area data's sprite
 * models placed in the world and linked into hierarchies; the solid ones
 * carry collision meshes. Each open map area starts actors of its own (the
 * spinning objects, the ferry, the airship). The scene modes 8-18 each start
 * a set of actors: the screen fade, a cue sequencer (the director) or an
 * actor script, and the camera and object actors they command. The text
 * split leaves the set-up and leave handlers of each mode with a director,
 * and the director's sequence start, at the end of the unit before the
 * director's (worldmap_scene15). */

#include "worldmap.h"
#include "resident/model.h"

/* Scene object (0x54 bytes): a sprite model of the area data, transformed
 * and linked to a parent. */
typedef struct SceneObject {
    s16 visible;
    s16 unk2;
    u16 flags;                  /* 0x04: 1 solid */
    s16 unk6;
    VECTOR position;            /* 0x08 */
    SVECTOR angle;              /* 0x18 */
    MATRIX matrix;              /* 0x20 */
    SpriteModel *def;           /* 0x40 */
    s32 unk44;                  /* its collision mesh (Mesh) */
    void *prims;                /* 0x48 */
    void *prims2;               /* 0x4C: second buffer's copy */
    struct SceneObject *parent; /* 0x50 */
} SceneObject;

extern SceneObject *worldmap_objects; /* scene objects */
extern s16 worldmap_object_count;          /* scene object count */
extern s16 worldmap_object_model_count;          /* animation count */
extern u16 worldmap_area_image_cluts[16];      /* the area image's faded CLUT ids */
extern MATRIX worldmap_light_color_matrix, worldmap_light_direction_matrix; /* colour and light matrices */

void worldmap_upload_area_image(void); /* upload the area image, build its faded CLUTs */
void worldmap_objects_build(void); /* build the scene objects */
void worldmap_objects_free(void); /* free them */
void worldmap_objects_link(s32 parent, s32 child); /* link `child` to `parent` */
void worldmap_objects_draw(void); /* draw them */

/* A resident model call the world map declares itself: it passes the scene
 * object as a fourth argument the resident's definition does not take. */
void model_alloc_packet_buffers(SpriteModel *def, void **prims, void **prims2, SceneObject *object);

/* Collision mesh of a scene object (behind SceneObject.unk44). */
typedef struct {
    s16 corner[3];
    s16 next[3]; /* neighbouring face across each edge; -1 at the border */
    u16 kind;    /* 1: wall */
} MeshFace;

typedef struct {
    s32 unk0;
    SVECTOR *vertices;
    MeshFace faces[1];
} Mesh;

extern s16 worldmap_mesh_probe_hits[];        /* probe hits: face and kind pairs */
extern s32 worldmap_walker_face, worldmap_walker_object; /* the face and object the walker stands on, -1 none */

void worldmap_mesh_project_onto_face(VECTOR *position, VECTOR *offset, VECTOR *normal, u16 index, u16 face);
s32 worldmap_mesh_is_face_plane_crossed(VECTOR *probe, s32 radius, u16 object, u16 other);
s32 worldmap_mesh_classify_face_exit(VECTOR *from, VECTOR *to, s32 index, s32 face);

/* Per area: the scene objects of the area's actors (a spinning pair, a
 * further pair and single objects). */
extern u16 worldmap_area_spinning_pair_objects[][2], worldmap_area_rolling_pair_objects[][2];
extern u16 worldmap_area_ferry_objects[], worldmap_area_airship_objects[], worldmap_area_raised_placement_objects[], worldmap_area_ground_placement_objects[];

/* The areas' own actors (start, update; worldmap_area_actor_lists) and the updates
 * installed again when resuming a saved state. */
s32 worldmap_spinning_pair_start(s32 index), worldmap_spinning_pair_update(s32 index); /* spinning pair */
s32 worldmap_area_spinning_pair_start(s32 index), worldmap_area_spinning_pair_update(s32 index); /* the area's spinning pair */
s32 worldmap_area_rolling_pair_start(s32 index), worldmap_area_rolling_pair_update(s32 index); /* the area's rolling pair */
s32 worldmap_ferry_start(s32 index), worldmap_ferry_update(s32 index); /* the ferry */
s32 worldmap_airship_start(s32 index), worldmap_airship_update(s32 index); /* the airship */
s32 worldmap_object69_start(s32 index), worldmap_object69_update(s32 index);
s32 worldmap_raised_placement_start(s32 index), worldmap_raised_placement_update(s32 index);
s32 worldmap_object75_placement_start(s32 index), worldmap_object75_placement_update(s32 index);
s32 worldmap_ground_placement_start(s32 index), worldmap_ground_placement_update(s32 index);
s32 worldmap_area_idle_actor_start(void), worldmap_area_idle_actor_update(void);
s32 worldmap_area_rolling_pair_rebuild(s32 index);
s32 worldmap_ferry_link_objects(s32 index);
s32 worldmap_airship_link_objects(void);
s32 worldmap_object69_resume(void);

/* The ferry's saved route state is game_data.unk1844 (x, z in world units,
 * next waypoint), and unk184A counts its runs started (worldmap_ferry_start). */
extern u16 worldmap_ferry_waypoints_x[8], worldmap_ferry_waypoints_z[8]; /* ferry waypoints (x, z) */

/* Ferry heading history (ring of 32). */
typedef struct FerryHeading {
    s16 dx;
    s16 pad2;
    s16 dz;
    s16 pad6;
} FerryHeading;

extern FerryHeading worldmap_ferry_headings[32];

/* Timed sequence: state per step and the step durations. */
typedef struct {
    s16 *states;
    u16 *durations;
} Sequence;

/* The directors' cue sequences, user-supplied script data (INCLUDE_ASSET):
 * states and waits of modes 14 (worldmap_scene14), 12 (worldmap_scene12),
 * 15 (worldmap_scene15_cue_sequences per entry, worldmap_scene15), 13 (worldmap_scene13) and
 * 16 (worldmap_scenes_16_18), and the actor scripts of modes 17 and 18. */
extern u16 worldmap_scene14_cue_states[], worldmap_scene14_cue_waits[];
extern u16 worldmap_scene12_cue_states[], worldmap_scene12_cue_waits[];
extern Sequence worldmap_scene15_cue_sequences[];
extern u16 worldmap_scene13_cue_states[], worldmap_scene13_cue_waits[];
extern u16 worldmap_scene16_cue_states[], worldmap_scene16_cue_waits[];
extern s16 worldmap_scene17_actor_script[], worldmap_scene18_actor_script[];

/* Mode 15's entries: the player's start position and the resident flag word
 * set on leaving. */
extern SVECTOR worldmap_scene15_start_positions[];
extern u16 worldmap_scene15_exit_entry_parameters[];

/* Scene sprite quads (worldmap_scene13): set their colour. */
void worldmap_set_quad_colors(POLY_FT4 *quads, s32 count, s32 r, s32 g, s32 b);
/* worldmap_build_translucent_quads (void, worldmap_scenes_9_10) has no prototype here: the other units
 * call it undeclared (implicit int), which lets the caller schedule its return value
 * early after the call (8007FC8C). */

/* Scratchpad work area of the scaled scene objects. */
typedef struct {
    VECTOR scale[2];
    u8 pad20[0x80];
    SVECTOR angle;    /* 0xA0 */
    u8 padA8[0x48];
    MATRIX matrix[2]; /* 0xF0 */
} ScaleScratch;

#define SCALE_SCRATCH ((ScaleScratch *)0x1F800000)

/* The heat haze of mode 16: the shared quad pool (192 quads, one copy per
 * display buffer), the per-row wobble and the copy-back per buffer. */
typedef struct QuadBuffer {
    POLY_FT4 quads[0xC0];
} QuadBuffer;

extern QuadBuffer *worldmap_scene16_haze_quads[2];
extern u16 *worldmap_scene16_haze_spreads;
extern DR_MOVE worldmap_scene16_haze_copies[2];

/* The scene modes' set-up and leave handlers (worldmap_mode_handlers). */
void worldmap_scene8_start(void), worldmap_scene8_leave(void); /* modes 8 and 11 */
void worldmap_scene9_start(void), worldmap_scene9_leave(void); /* mode 9 */
void worldmap_scene10_start(void), worldmap_scene10_leave(void); /* mode 10 */
void worldmap_scene12_start(void), worldmap_scene12_leave(void); /* mode 12 */
void worldmap_scene13_start(void), worldmap_scene13_leave(void); /* mode 13 */
void worldmap_scene14_start(void), worldmap_scene14_leave(void); /* mode 14 */
void worldmap_scene15_start(void), worldmap_scene15_leave(void); /* mode 15 */
void worldmap_scene16_start(void), worldmap_scene16_leave(void); /* mode 16 */
void worldmap_scene17_start(void), worldmap_scene17_leave(void); /* mode 17 */
void worldmap_scene18_start(void), worldmap_scene18_leave(void); /* mode 18 */

/* A resident sound call the world map declares itself (the camera flight's
 * engine volume): the shared headers leave it out, since other targets'
 * calls convert its arguments differently. */
void sound_set_effect_volume(s32 sound, s32 volume);

/* Their actors (start, update), by mode. The frame steps that end each
 * mode's list draw the scene. */
s32 worldmap_open_map_frame_start(void), worldmap_open_map_frame_update(void);           /* the open map's frame step */
s32 worldmap_scene_frame_billboards_start(void), worldmap_scene_frame_billboards_update(void);           /* frame step without input */
s32 worldmap_scene_frame_start(void), worldmap_scene_frame_update(void);           /* frame step */
s32 worldmap_scene8_camera_start(s32 index), worldmap_scene8_camera_update(s32 index); /* 8, 11: the pad-steered camera */
s32 worldmap_scene9_camera_flight_start(s32 index), worldmap_scene9_camera_flight_update(s32 index); /* 9: the camera flight */
s32 worldmap_scene9_rig_start(s32 index), worldmap_scene9_rig_update(s32 index); /* 9: the rig */
s32 worldmap_scene10_script_start(s32 index), worldmap_scene10_script_update(s32 index); /* 10 */
s32 worldmap_scene10_ground_object16_start(s32 index), worldmap_scene10_ground_object16_update(s32 index);
s32 worldmap_scene10_landing_rig_start(s32 index), worldmap_scene10_landing_rig_update(s32 index);
s32 worldmap_scene10_grow_sprites_14_15_start(s32 index), worldmap_scene10_grow_sprites_14_15_update(s32 index);
s32 worldmap_scene10_group9_trail_start(s32 index), worldmap_scene10_group9_trail_update(s32 index);
s32 worldmap_scene10_group10_burst_start(void), worldmap_scene10_group10_burst_update(s32 index);
s32 worldmap_scene14_director_start(s32 index), worldmap_scene14_director_update(s32 index); /* 14: the director */
s32 worldmap_scene14_camera_start(s32 index), worldmap_scene14_camera_update(s32 index);
s32 worldmap_scene14_grow_objects_4_5_start(s32 index), worldmap_scene14_grow_objects_4_5_update(s32 index);
s32 worldmap_scene14_grow_objects_6_7_start(s32 index), worldmap_scene14_grow_objects_6_7_update(s32 index);
s32 worldmap_scene14_exhaust_trail_start(void), worldmap_scene14_exhaust_trail_update(s32 index);
s32 worldmap_scene14_rig_flight_start(s32 index), worldmap_scene14_rig_flight_update(s32 index);
s32 worldmap_scene12_director_start(s32 index), worldmap_scene12_director_update(s32 index); /* 12: the director */
s32 worldmap_scene12_camera_shots_start(s32 index), worldmap_scene12_camera_shots_update(s32 index);
s32 worldmap_scene12_drift_object4_start(s32 index), worldmap_scene12_drift_object4_update(s32 index);
s32 worldmap_scene12_drift_object5_start(s32 index), worldmap_scene12_drift_object5_update(s32 index);
s32 worldmap_scene12_drift_object9_start(s32 index), worldmap_scene12_drift_object9_update(s32 index);
s32 worldmap_scene12_drift_object10_start(s32 index), worldmap_scene12_drift_object10_update(s32 index);
s32 worldmap_scene12_drift_object12_start(s32 index), worldmap_scene12_drift_object12_update(s32 index);
s32 worldmap_scene12_drift_object13_start(s32 index), worldmap_scene12_drift_object13_update(s32 index);
s32 worldmap_scene12_drift_object16_start(s32 index), worldmap_scene12_drift_object16_update(s32 index);
s32 worldmap_scene15_director_start(s32 index), worldmap_scene15_director_update(s32 index); /* 15: the director */
s32 worldmap_scene15_camera_start(s32 index), worldmap_scene15_camera_update(s32 index);
s32 worldmap_scene15_flying_vehicle_start(s32 index), worldmap_scene15_flying_vehicle_update(s32 index);
s32 worldmap_scene15_flame_start(s32 index), worldmap_scene15_flame_update(s32 index);
s32 worldmap_scene15_grow_objects_9_10_start(s32 index), worldmap_scene15_grow_objects_9_10_update(s32 index);
s32 worldmap_scene13_director_start(s32 index), worldmap_scene13_director_update(s32 index); /* 13: the director */
s32 worldmap_scene13_camera_start(s32 index), worldmap_scene13_camera_update(s32 index);
s32 worldmap_scene13_effects_start(s32 index), worldmap_scene13_effects_update(s32 index);
s32 worldmap_scene13_grow_objects_0_1_start(s32 index), worldmap_scene13_grow_objects_0_1_update(s32 index);
s32 worldmap_scene16_director_start(s32 index), worldmap_scene16_director_update(s32 index); /* 16: the director */
s32 worldmap_scene16_camera_start(s32 index), worldmap_scene16_camera_update(s32 index);
s32 worldmap_scene16_fade_object2_start(s32 index), worldmap_scene16_fade_object2_update(s32 index);
s32 worldmap_scene16_fade_objects_0_1_start(s32 index), worldmap_scene16_fade_objects_0_1_update(s32 index);
s32 worldmap_scene16_haze_start(void), worldmap_scene16_haze_update(void);
s32 worldmap_scene16_haze_strength_start(s32 index), worldmap_scene16_haze_strength_update(s32 index);
s32 worldmap_scene17_script_start(s32 index), worldmap_actor_script_run(s32 index); /* 17: the actor script */
s32 worldmap_scene17_camera_start(s32 index), worldmap_scene17_camera_update(s32 index);
s32 worldmap_scene17_pulse_start(s32 index), worldmap_scene17_pulse_update(s32 index);
s32 worldmap_scene17_terrain_reload_start(void), worldmap_scene17_terrain_reload_update(s32 index);
s32 worldmap_scene18_script_start(s32 index);                           /* 18: the actor script */
s32 worldmap_scene18_camera_start(s32 index), worldmap_scene18_camera_update(s32 index);
s32 worldmap_scene18_lift_start(s32 index), worldmap_scene18_lift_update(s32 index);

#endif
