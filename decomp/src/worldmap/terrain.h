#ifndef WORLDMAP_TERRAIN_H
#define WORLDMAP_TERRAIN_H

/* The world map terrain: the queries of its cells, heights and slopes, the
 * path regions, the wrapping of positions on the area (worldmap_steering_camera_terrain),
 * the movement over terrain and solid objects (worldmap_movement_terrain), the 9x9
 * blocks streamed around the camera and drawn in 5x5 (worldmap_movement_terrain) and
 * the billboards standing on them (worldmap_objects_effects_party). */

#include "worldmap.h"
#include "camera.h"

/* A path region (16 bytes) of an area file's path tables or the fixed ones:
 * its bounds, the scene and entry a world-map exit from it stores (game data
 * map and entry[2]), its path link (-1 none) and its kind: 1 leaves
 * without the button (worldmap_steer_on_foot), 2 makes a flying vehicle descend first
 * (worldmap_flying_vehicle_start), 3 takes the exit from the camera target's region of path
 * table 3 (worldmap_main), 4 only records a destination (worldmap_path_select_region). A
 * scene of -1 ends a table. */
typedef struct PathRegion {
    s16 x, z, w, h;
    s16 scene;
    s16 entry;
    s16 link; /* 0x0C: path or destination id */
    s16 kind; /* 0x0E */
} PathRegion;

extern PathRegion worldmap_fixed_path_regions[3];
extern PathRegion *worldmap_current_path; /* the current path, -1 none */

s32 worldmap_path_select_region(VECTOR *position, s32 table);
s32 worldmap_path_select_region_of_kind(VECTOR *position, s32 table, s32 kind);

/* Terrain queries (worldmap_steering_camera_terrain). */
void worldmap_wrap_position(VECTOR *position);  /* wrap a position (20.12) onto the area */
void worldmap_wrap_offset(VECTOR *offset);    /* wrap an offset (20.12) */
void worldmap_wrap_world_offset(VECTOR *delta);     /* wrap a world-unit offset */
void worldmap_set_height_on_plane(VECTOR *point, VECTOR *origin, VECTOR *normal);
u8 *worldmap_terrain_get_cell(s32 x, s32 z);       /* terrain cell at a position */
void worldmap_terrain_get_normal(VECTOR *normal, s32 x, s32 z); /* ground normal */
s32 worldmap_terrain_get_height(s32 x, s32 z);       /* ground height at a position */
s32 worldmap_terrain_get_wave_height(s32 x, s32 z);       /* terrain height */
s32 worldmap_terrain_get_attribute(VECTOR *position);   /* terrain attribute at a position */
s16 worldmap_terrain_get_layer(VECTOR *position);
s32 worldmap_terrain_get_cell_flags(VECTOR *position);
s16 worldmap_terrain_can_mode_enter_layer(s16 row, s16 column);
s32 worldmap_terrain_slide_on_slope(VECTOR *position, VECTOR *direction, VECTOR *out);
s32 worldmap_get_ground_distance(VECTOR *a, VECTOR *b); /* distance */
void worldmap_get_heading_to(VECTOR *from, VECTOR *to, VECTOR *direction, s16 *heading);
s32 worldmap_terrain_step_boundary_plus_x(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 worldmap_terrain_step_boundary_neg_x(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 worldmap_terrain_step_boundary_plus_z(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 worldmap_terrain_step_boundary_neg_z(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);

extern s16 worldmap_terrain_passable_layers[]; /* passable terrain layers per movement mode (worldmap_terrain_can_mode_enter_layer) */
extern s32 worldmap_wave_phase_x, worldmap_wave_phase_z; /* water wave phases: x, z */

/* Movement over the terrain and the solid scene objects (worldmap_movement_terrain). */
s32 worldmap_terrain_probe_move(VECTOR *position, VECTOR *direction, s32 scale, s32 mode);
s32 worldmap_move_walking(VECTOR *position, VECTOR *direction, VECTOR *hit, s32 range, s32 mode);
s32 worldmap_move_flying(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s32 mode);

/* Terrain streaming origin (world units, wrapped to the map) and the block
 * cell the camera is in. */
extern VECTOR worldmap_terrain_origin;
#define TERRAIN_ORIGIN worldmap_terrain_origin
#define GROUND_SCROLL ((s32 *)&worldmap_terrain_origin) /* ground scroll offset x, y, z */
extern SVECTOR worldmap_camera_block_cell; /* block cell */
extern s16 worldmap_terrain_crossed_edges;     /* the map edges the camera crossed (8/4 x, 2/1 z) */

/* 9x9 terrain blocks around the camera: block numbers, row-major. */
typedef struct BlockGrid {
    s16 cells[81];
} BlockGrid;

extern BlockGrid worldmap_terrain_grid; /* current */
extern BlockGrid worldmap_terrain_previous_grid; /* previous */
extern void *worldmap_terrain_blocks[0x100]; /* terrain block buffers */
extern s16 worldmap_terrain_visible_blocks[25];      /* 5x5 visible blocks; -1 empty */
extern s16 worldmap_terrain_quarter_visibility[25][4];   /* per block: visibility of its 4 quarters */
extern s32 worldmap_terrain_packet_count;          /* terrain POLY_FT3 packets used this frame */
extern MATRIX worldmap_terrain_matrix;
extern VECTOR worldmap_terrain_cull_normal_3, worldmap_terrain_cull_normal_0, worldmap_terrain_cull_normal_1, worldmap_terrain_cull_normal_2; /* horizon plane normals */

/* Terrain palettes: 64 CLUT ids (two 256-colour palettes faded in 32 steps
 * towards the background colour) and seven texture pages. */
extern u16 worldmap_terrain_cluts[0x40];
extern u16 worldmap_terrain_tpages[7];

extern s16 rcossin_tbl[0x1000][2]; /* PsyQ rcossin_tbl: sine, cosine */

void worldmap_terrain_alloc_packets(void); /* allocate the terrain packets */
void worldmap_terrain_upload_image(void); /* upload the terrain image, build its palettes */
void worldmap_terrain_reset_at(VECTOR *position); /* reset the loader around a position */
void worldmap_terrain_reset_at_camera(Camera *camera);   /* ... around the camera */
void worldmap_terrain_free_blocks(void); /* free the loaded blocks */
void worldmap_terrain_compute_cull_normals(void); /* the horizon plane normals */
void worldmap_terrain_wrap_origin(void *);
void worldmap_terrain_update_grid(Camera *);
void worldmap_terrain_classify_blocks(Camera *);
void worldmap_terrain_load_new_edges(void);
void worldmap_terrain_draw(u_long *ot, s32, Camera *);

/* The billboards standing on the terrain blocks (worldmap_objects_effects_party): each
 * of the 256 blocks' list in the area data's billboard section,
 * their quads per display buffer and their CLUTs. */
typedef struct BillboardList {
    u8 *data;  /* from the section, a null list empty */
    s32 count;
} BillboardList;

extern BillboardList *worldmap_billboard_lists;
extern void *worldmap_billboard_quads[2]; /* billboard quads, per display buffer */
extern s16 worldmap_billboard_quad_count;     /* quads used this frame; a word in 80099BFC */
extern u16 worldmap_billboard_cluts[16]; /* billboard CLUTs */

void worldmap_billboards_resolve_lists(void); /* resolve the lists, create the CLUTs */
void worldmap_billboards_alloc_quads(void); /* allocate the quads */
void worldmap_billboards_free_quads(void); /* free them */
void worldmap_billboards_draw(void); /* draw the billboards */
void worldmap_billboards_draw_block(u8 *data, s32 count, u_long *ot, POLY_FT4 *quads); /* draw billboards (assembly) */

#endif
