#ifndef WORLDMAP_H
#define WORLDMAP_H

/* The world map overlay (mode 3: 8006faf0-8009bbb4, its uninitialized data to
 * 8009d810): the modes and their frame loop, the area files, the player, the
 * pad state, the display buffers with the sky, horizon, map and footprints,
 * the encounters, the actor slots and the state saved across another scene.
 * Beside it: camera.h, scene.h (scene objects, the areas' actors and the
 * scripted scene modes), effect.h (particles and drifting clouds), party.h
 * (the party and its vehicles), terrain.h (terrain, paths and movement),
 * screen.h (the fade and the name windows), stream.h (the terrain stream
 * reader) and gte.h (the cloud drawing's screen point reads). */

#include "common.h"
#include "psyq/abs.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/formation.h"
#include "resident/gamedata.h"
#include "resident/sprite.h"

/* World-map modes (worldmap_mode_handlers): 0-7 the open map, 8-18 the scripted scenes.
 * The overlay entry enters the mode, then starts it and runs the frame loop
 * for as long as the loop's result asks for it, leaving it after each run. */
typedef struct {
    void (*enter)(void);
    void (*start)(void);
    void (*leave)(void);
} WorldmapMode;

extern WorldmapMode worldmap_mode_handlers[]; /* per mode */
extern s32 worldmap_mode_index;               /* the current mode */

void worldmap_read_party_models(void);      /* modes 0-7: enter (read the party's models) */
void worldmap_read_area_files(void);        /* modes 8-18: enter (read the area files) */
void worldmap_open_map_start(void);         /* modes 0-7: start */
void worldmap_open_map_leave(void);         /* modes 0-7: leave */

/* The frame loop (worldmap_run_frame_loop) and its state. */
extern s32 worldmap_loop_running;                                       /* nonzero while the loop runs */
extern s32 worldmap_loop_result;                                        /* its result: 0 leave for a scene, 1 battle, 2 and more run the mode again */
extern s32 worldmap_next_scene_chosen;                                  /* nonzero once a leave handler chose the next scene */
extern s32 worldmap_resuming;                                           /* nonzero when resuming a saved state */
extern s32 worldmap_screen_fade_active;                                 /* nonzero while a screen fade runs */
extern s32 worldmap_menu_requested;                                     /* the menu was requested */
extern s32 worldmap_button_combo_pressed;                               /* the two-button combination was pressed (worldmap_latch_button_combo) */
extern s32 worldmap_button_combo_held, worldmap_button_combo_held_last; /* that combination held this frame and the last */
extern void (*worldmap_cloud_draw_hook)(void);                          /* per-frame hook */
extern MATRIX worldmap_unread_mode_matrix;                              /* set to the identity by each mode's set-up */
extern MATRIX worldmap_identity_matrix;                                 /* identity matrix */
extern s32 worldmap_entry_map_offset;                                   /* the saved map at entry less 0x400 (event variable 2 on leaving) */

/* The world map's first flag word, game data entry[2], by a name of its own
 * where the overlay entry tests and sets it before choosing the mode: as the
 * member it compiles differently there. */
extern u16 game_data_worldmap_flag_word[];

/* The map flags: event variables 254-255, one bit per map dot (bits 24-26
 * the vehicles). */
#define MAP_FLAGS (*(u32 *)&game_data.vars[254])

/* Per-area file set: the first disc file, the area's extent in blocks (x, z)
 * and the frame loop's first result. */
typedef struct {
    s16 file;
    s16 param2;
    s16 param4;
    s16 param6;
} WorldmapArea;

extern u16 worldmap_area_thresholds[];          /* open map area thresholds, indexed from 1 */
extern WorldmapArea worldmap_area_file_sets[];  /* area file sets */
extern s32 worldmap_area_index;                 /* open map area */
extern s32 worldmap_entry_index;                /* entry: arrival point, or the scene's entry */

/* The area's disc files (worldmap_select_area_files) and the buffers they are read into. */
extern s32 worldmap_area_data_file, worldmap_terrain_image_file, worldmap_area_image_file, worldmap_wave_bank_file, worldmap_music_file;              /* files 1-5 */
extern s32 worldmap_sound_bank_file, worldmap_flight_music_file, worldmap_battle_music_file, worldmap_terrain_row_file, worldmap_terrain_column_file; /* files 6-10 */
extern s32 worldmap_area_blocks_x, worldmap_area_blocks_z;                                                                                            /* extent in blocks: x, z */
extern void *worldmap_area_data;                                                                                                                      /* area data (file 1) */
extern void *worldmap_terrain_image;                                                                                                                  /* terrain texture image (file 2) */
extern void *worldmap_area_image;                                                                                                                     /* area image (file 3) */
extern void *worldmap_wave_bank;                                                                                                                      /* wave bank (file 4) */
extern void *worldmap_music, *worldmap_flight_music, *worldmap_battle_music;                                                                          /* music (files 5, 7, 8) */
extern void *worldmap_packed_menu_overlay;                                                                                                            /* the shared file 0x25 */
extern FileRequest worldmap_read_list[];                                                                                                              /* shared read list, a zero file ends it */
/* The list's first destination member by a name of its own (worldmap.data.ld):
 * two loaders pass the list from it. Formed from worldmap_read_list itself, the
 * constant lets cse store the first entry through the argument register. */
extern void *worldmap_read_list_first_destination;
#define WORLD_READ_LIST ((FileRequest *)((u8 *)&worldmap_read_list_first_destination - 4))

void worldmap_read_shared_files(void); /* read the shared files 0x25 and 0x26 */
void worldmap_read_area_sound_files(void); /* read the area's files 4-8 */
void worldmap_read_area_sound_bank(void); /* read its sound bank (file 6) */

/* The music files' sequence data (mode_music_buffer, resident/mode.h), as the
 * sequence header the sound driver reads. A declaration of its own for the
 * same symbol: worldmap_open_map_start forms the address anew rather than from the
 * destination of the copy before it, which cse keeps in a saved register
 * when both are mode_music_buffer. */
extern struct SoundSeqHeader mode_music_buffer_header __asm__("mode_music_buffer");

/* The area data's header (file 1): section offsets from its start. */
typedef struct {
    s32 unk0;
    s32 spots;          /* spot block */
    s32 models;         /* sprite models */
    s32 meshes;         /* their collision meshes */
    s32 placements;     /* scene object placements */
    s32 billboards;     /* the terrain blocks' billboard lists */
    s32 emitters;       /* area objects */
    s32 names;          /* path and destination names */
    s32 animations;     /* texture animations */
    s32 animations2;    /* the second set */
    s32 unk28;
    s32 encounters[16]; /* per terrain kind: its encounter table (worldmap_encounter_sets) */
} AreaHeader;

/* Spot block: arrival points and a table of four sections. */
typedef struct {
    s32 spots;
    s32 table;
} SpotHeader;

/* Named arrival point: position in world units and its id; -1 ends a list. */
typedef struct WorldmapSpot {
    s16 x;
    s16 id;
    s16 z;
    s16 pad;
} WorldmapSpot;

/* The area data's sections (worldmap_unpack_area_data). */
extern void *worldmap_object_models;                                        /* sprite models */
extern void *worldmap_object_meshes;                                        /* their collision meshes */
extern void *worldmap_object_placement_list;                                /* scene object placements */
extern void *worldmap_name_table;                                           /* path and destination names */
extern s32 *worldmap_texture_anim_section, *worldmap_texture_anim2_section; /* texture animations, both sets */
extern void *worldmap_encounter_sets[16];                                   /* encounter sets, per terrain kind */
extern s32 *worldmap_path_tables;                                           /* the four path tables */
extern WorldmapSpot *worldmap_arrival_points;                               /* arrival points */

void worldmap_unpack_scene_area_data(void); /* unpack a scene mode's area data */

/* Texture animations: each slot uploads one image of a frame sequence. */
typedef struct {
    s16 image;    /* image index in the animation's data */
    s16 duration; /* frames; negative ends the sequence */
} TexAnimFrame;

typedef struct {
    RECT rect;
    s32 unk8;
    TexAnimFrame *frames;
} TexAnimSlot;

typedef struct TexAnim {
    u8 *images;
    TexAnimSlot *slot;
    s16 frame;
    s16 timer;
} TexAnim;

extern TexAnim *worldmap_texture_anims, *worldmap_texture_anims2;
extern s32 worldmap_texture_anim_count, worldmap_texture_anim2_count; /* animations of each set */

void worldmap_texture_anim_create(void), worldmap_texture_anim_free(void), worldmap_texture_anim_advance(void); /* create, free, advance */
void worldmap_texture_anim2_create(void), worldmap_texture_anim2_free(void), worldmap_texture_anim2_advance(void); /* the second set */

/* The player: movement mode (1-3 on foot, 4-7 in a vehicle, 6-7 flying),
 * position (20.12) and heading. */
extern s32 worldmap_movement_mode;
extern VECTOR worldmap_player_position;
extern s32 worldmap_player_heading;

void worldmap_choose_movement_mode(void); /* choose the movement mode */
void worldmap_encounter_reset_timers(void); /* reset the movement state */
void worldmap_apply_party_riding_changes(void); /* apply the party's riding changes */

/* Pad buttons gathered per frame from the queued input events (resident/pad.h
 * words OR-ed together): held, pressed and repeated, each with the word that
 * follows it in the resident; and the resident's vertical blank count. */
extern u16 worldmap_pad_port0_held, worldmap_pad_unread_port1_held; /* held */
extern u16 worldmap_pad_port0_pressed, worldmap_pad_unread_port1_pressed; /* pressed */
extern u16 worldmap_pad_unread_port0_repeated, worldmap_pad_unread_port1_repeated; /* repeated */
extern s32 pad_vblank_count;

/* Per display buffer: its environments, its ordering table (0x400 entries)
 * and its terrain triangle packets (0x10000 bytes). */
typedef struct DisplayBuffer {
    DRAWENV draw;
    DISPENV disp;
    u_long *ot;    /* 0x70 */
    void *packets; /* 0x74 */
} DisplayBuffer;

extern DisplayBuffer worldmap_display_buffers[2];
extern DisplayBuffer *worldmap_current_display_buffer; /* the buffer being drawn */
extern s32 worldmap_display_buffer_index;              /* its index */
extern s32 worldmap_projection_distance;               /* projection distance */
extern u8 worldmap_background_color[3];                /* background colour */

void worldmap_alloc_ots(void); /* allocate the ordering tables */
void worldmap_init_display(void); /* set up the display */
void worldmap_fade_saved_screen(s32 a, s32 b, s32 c, s32 d); /* fade the saved screen */
void worldmap_sync_and_flush_cache(void); /* wait for the GPU, flush the cache */

/* A resident display call the world map declares itself: its calls pass
 * words where the resident's definition takes bytes. */
void model_set_color(s32 r, s32 g, s32 b);

/* Gouraud quad packet (PsyQ POLY_G4 layout); colour words carry the code
 * in their top byte. */
typedef struct PolyG4 {
    u32 tag;
    u32 rgb0;
    long xy0;
    u32 rgb1;
    long xy1;
    u32 rgb2;
    long xy2;
    u32 rgb3;
    long xy3;
} PolyG4;

/* The sky, the horizon, the map overlay and the footprints
 * (worldmap_open_map). */
extern PolyG4 worldmap_sky_bands[4][2];                             /* sky gradient bands, per buffer */
extern POLY_FT4 worldmap_horizon_quads[2][2];                       /* textured horizon quads, per buffer */
extern DR_TWIN worldmap_horizon_texture_windows[2];                 /* their texture windows */
extern POLY_FT4 worldmap_map_overlay_quads[2];                      /* overlay picture, per buffer */
extern DR_TPAGE worldmap_map_overlay_tpage;
extern POLY_G3 worldmap_map_marker_polys[8];                        /* player marker triangles */
extern TILE worldmap_map_dot_tiles[0x40];                           /* map dots */
extern u16 worldmap_map_dot_positions[64];                          /* 32 map dot positions: interleaved X/Z */
extern WorldmapSpot *worldmap_footprints;                           /* footprint ring of 16 positions */
extern s32 worldmap_footprint_count;                                /* footprints recorded */
extern void *worldmap_footprint_quads0, *worldmap_footprint_quads1; /* footprint quads, per buffer */

void worldmap_sky_init(void), worldmap_sky_draw(void);         /* the sky: set up, draw */
void worldmap_horizon_init(void), worldmap_horizon_draw(void); /* the horizon */
void worldmap_map_overlay_draw(void);                          /* draw the map overlay */
void worldmap_footprints_add(s16 id, VECTOR *position);        /* record a footprint */
void worldmap_footprints_draw(void);                           /* draw the footprints */

/* Random encounters: timers with distinct random delays per period, and the
 * terrain's encounter set copied for the battle. */
extern s16 worldmap_encounter_timers[16];
extern s32 worldmap_encounter_reroll_timer, worldmap_encounter_period, worldmap_encounter_timer_count; /* countdown, period, timers */
extern s32 worldmap_encounter_expired_count; /* timers that expired this frame */
extern u16 worldmap_encounter_level_brackets[]; /* scene id bracket thresholds, from 1 */

/* A terrain kind's encounter table (worldmap_encounter_sets) is its EncounterSet
 * (resident/formation.h), then 16 formation weights per scene id bracket. */

void worldmap_encounter_update_timers(void);                        /* count the timers down */
s32 worldmap_encounter_roll(VECTOR *position, s32 scene);           /* roll an encounter */

/* The pause and controller check screens, and the music. */
void worldmap_run_pause_screen(void);
void worldmap_wait_for_controller(void);
void worldmap_replace_music(void *data, s32 file);

/* Screen fade (worldmap_screen_fade_update): rate of its blend page and brightness step. */
extern s32 worldmap_screen_fade_rate, worldmap_screen_fade_step;

/* Actor slots (0x80 bytes each, 64 of them). */
typedef struct WorldmapActor {
    s16 command;     /* 0x00: pending command */
    s16 command_arg;
    s16 unk4;
    s16 unk6;
    s32 unk8;
    s32 unkC;
    s32 unk10;
    s32 unk14;
    s32 kind;        /* 0x18 */
    s32 update;      /* 0x1C: nonzero while the slot is in use */
    s16 state;       /* 0x20 */
    s16 wait;        /* 0x22: script wait counter */
    s16 unk24;
    s16 unk26;
    VECTOR position; /* 0x28 */
    VECTOR motion;   /* 0x38 */
    s16 heading;     /* 0x48 */
    s16 turn;        /* 0x4A: turn step */
    Sprite *handle;  /* 0x4C: its model sprite (sprite_create), NULL none */
    union {
        s16 *script; /* script position */
        u16 value;   /* low half of step */
        s32 step;
    } u;             /* 0x50 */
    s32 unk54;
    s32 unk58;
    s32 unk5C;
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6C;
    s32 unk70;
    s32 unk74;
    s32 unk78;
    s32 unk7C;
} WorldmapActor;

/* Actor slot entry points (kind: start, update: step); they return the
 * next command. */
typedef s32 (*ActorFunc)(s32 index);

extern WorldmapActor *worldmap_actor_slots;

void worldmap_actor_alloc_slots(void);                 /* allocate the slots */
void worldmap_actor_free_slots(void);                  /* free them */
void worldmap_actor_clear_slots(void);                 /* mark every slot free */
void worldmap_actor_set_kind(s32 kind, s32 index);     /* change a slot's kind */
void worldmap_actor_spawn(s32 kind, s32 update);       /* start an actor */
s32 worldmap_actor_request(s32 index, s32 arg);        /* send command 1 */
void worldmap_actor_run_all(void);                     /* run the pending commands */

/* An actor's model sprite: render bit 2 hides it (a new model starts hidden),
 * and the last byte of its motion word, read signed, is the animation
 * sprite_start_animation set. */
#define SPRITE_HIDDEN 4
#define SPRITE_ANIMATION(sprite) ((s8)(sprite)->motion.bytes[3])

void worldmap_actor_draw_sprites(void); /* draw the actors' model sprites */

/* Resident sprite calls the world map declares itself: its calls pass words
 * where the resident's definitions take halfwords. */
Sprite *sprite_create(s32 *data, s32 x, s32 y, s32 width, s32 height, s32 unused);
void sprite_set_facing(Sprite *sprite, s32 angle);

/* Scratchpad work area of the actor updaters. */
typedef struct {
    VECTOR work;      /* 0x00 */
    u8 pad10[0x20];
    VECTOR start;     /* 0x30: trail reset position */
    u8 pad40[0x50];
    VECTOR probe;     /* 0x90: move probe result */
    SVECTOR position; /* 0xA0 */
    SVECTOR angle;    /* 0xA8 */
} ActorScratch;

#define ACTOR_SCRATCH ((ActorScratch *)0x1F800000)
#define SCRIPT_VECTOR ((SVECTOR *)0x1F8000A0) /* scratchpad script vector */

/* Recent positions of the player's vehicle (ring of 32). */
typedef struct TrailPoint {
    VECTOR position;
    u16 heading;
    u16 pad;
} TrailPoint;

extern TrailPoint worldmap_trail_points[32];
extern s16 worldmap_trail_index; /* trail index */
extern u16 worldmap_camera_follow_heading; /* the leader's heading */

/* Suspending the open map for another scene (worldmap_suspend_open_map, worldmap_resume_open_map):
 * the memory kept free while away, the VRAM areas saved and the battle
 * flag. */
extern void *worldmap_suspend_memory;
extern void *worldmap_saved_vram_page, *worldmap_saved_vram_cluts;
extern s32 worldmap_saved_gear_riding_lock;

void worldmap_suspend_open_map(void);
void worldmap_resume_open_map(void);

/* The resident's save of the world-map state across a scene change (0x8005a4e4). */
typedef struct {
    WorldmapActor actor[64];
} ActorSet;

typedef struct {
    s16 timer[16];
} TimerSet;

typedef struct {
    TrailPoint points[32];
} WorldmapQueue; /* the vehicle trail, saved as a whole */

typedef struct {
    ActorSet actors;       /* 0x0000 */
    VECTOR position;       /* 0x2000 */
    s32 unk2010;           /* worldmap_camera_follow_heading */
    s32 timer_period;      /* worldmap_encounter_period */
    s32 timer_count;       /* worldmap_encounter_timer_count */
    s32 timer_countdown;   /* worldmap_encounter_reroll_timer */
    TimerSet timers;       /* 0x2020 */
    WorldmapQueue queue;   /* 0x2040 */
    s32 queue_count;       /* 0x22C0 */
    s32 camera_angle[2];   /* 0x22C4: SVECTOR worldmap_camera_angle as words */
    s32 camera_distance;   /* 0x22CC */
    s32 unk22D0;           /* worldmap_view_center_y */
    VECTOR unk22D4;        /* worldmap_terrain_origin */
    s32 unk22E4[2];        /* SVECTOR worldmap_camera_block_cell as words */
    VECTOR camera_target;  /* 0x22EC */
} WorldmapSave;

extern WorldmapSave mode_snapshot_block;

/* A resident window call the world map declares itself: its call passes
 * words where the resident's definition takes halfwords. */
void text_load_palette(s32 a, s32 b);

#endif
