#ifndef FIELD_FIELD_H
#define FIELD_FIELD_H

/* The field overlay's world: the view with the loaded components, the work
 * block and the two draw blocks; the event actors and their descriptors,
 * model instances, collision triangles and the components' record, which
 * the debug monitor reads too, are in field/actors.h. Every unit reads
 * these. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "field/actors.h"
#include "field/monitor.h"

/* One of the two draw-buffer blocks (800b249c, 800ba590). */
typedef struct FieldDrawBlock {
    DRAWENV draw;
    DRAWENV draw2;
    DISPENV disp;
    u_long ot[0x1001];   /* 0CC */
    u_long ot2[0x1001];  /* 40D0: cleared only while 800adb4c is set */
    u_long overlay_ot[8]; /* 80D4 */
} FieldDrawBlock;

/* One screen fade channel (800b20c4 + 0x58 * channel). Levels are 8.8. */
typedef struct {
    DR_MODE modes[2]; /* per draw buffer */
    TILE tiles[2];   /* per draw buffer */
    s32 level[3];
    s32 step[3];
    u16 abr;
    s16 active;
    s16 steps;
    u16 pad;
} FadeChannel;

/* The actor's two flag words as four halfwords; events test and store them
 * sixteen bits at a time. */
#define ACTOR_FLAG_HALF(actor, n) (((u16 *)(actor))[n])

/* Bit-field view of the actor's +134 word: bits 0-3 animation bank, bit 4
 * its flag, bits 5-6 a two-bit field cleared on reset, bit 7 the platform
 * link flag. */
typedef struct {
    u32 bank : 4;
    u32 bank_flag : 1;
    u32 unk5 : 2;
    u32 linked : 1;
    u32 unk8 : 24;
} ActorBits134;

#define ACTOR_BITS134(actor) (*(ActorBits134 *)&(actor)->unk134)

/* The shadow pass reads the flags halfword with its padding as one word. */
#define DESCRIPTOR_FLAGS_WORD(d) (*(u32 *)&(d)->flags)

/* A sprite's sequencer (Sprite +7C), as the resident's code casts it. */
#define SPRITE_SEQUENCER(sprite) ((SpriteSequencer *)(sprite)->sequencer)

/* The camera view saved by 8009b9a0. */
typedef struct {
    s16 octant;
    s32 projection;
    s16 elevation;
} SavedView;

/* The field view and camera state (800af880..800afb56), one object: code
 * addresses its members relative to one another. */
typedef struct FieldView {
    VECTOR eye;              /* 000 */
    VECTOR target;           /* 010 */
    VECTOR up;               /* 020 */
    VECTOR eye_goal;         /* 030 */
    VECTOR target_goal;      /* 040 */
    VECTOR unk050;           /* 050 */
    VECTOR shake_offset;     /* 060 */
    VECTOR saved_target;     /* 070 */
    VECTOR point_actor_a;    /* 080 */
    VECTOR saved_eye;        /* 090 */
    VECTOR point_actor_b;    /* 0A0 */
    s32 scripted_zoom;       /* 0B0 */
    s16 mode;                /* 0B4 */
    s16 scripted_elevation;  /* 0B6 */
    s16 scripted_heading;    /* 0B8 */
    s16 scripted_scale;      /* 0BA */
    u16 scripted;            /* 0BC */
    s16 target_steps;        /* 0BE */
    VECTOR scripted_target;  /* 0C0 */
    VECTOR target_step;      /* 0D0 */
    s16 eye_steps;           /* 0E0 */
    s32 scripted_eye[3];     /* 0E4 */
    s32 unk0F0;              /* 0F0 */
    s32 eye_step[3];         /* 0F4 */
    s32 unk100;              /* 100 */
    s32 target_a;            /* 104: target follow divisor */
    s32 target_b;            /* 108: eye follow divisor */
    s16 angle;               /* 10C */
    s16 view_angle;          /* 10E */
    MATRIX previous_view;    /* 110 */
    MATRIX orbit;            /* 130 */
    SVECTOR orbit_angles;    /* 150 */
    u32 flags;               /* 158 */
    s16 bounds[4];           /* 15C */
    SVECTOR heading_angles;  /* 164 */
    s32 heading_velocity;    /* 16C */
    s32 heading_high;        /* 170 */
    u8 heading_blocks[2];    /* 174 */
    s16 heading_steps;       /* 176 */
    s32 projection;          /* 178 */
    u16 elevation;           /* 17C */
    s16 distance;            /* 17E */
    s16 elevation_steps;     /* 180 */
    s32 elevation_value;     /* 184 */
    s32 elevation_step;      /* 188 */
    u32 heading;             /* 18C */
    s16 projection_steps;    /* 190 */
    s32 projection_value;    /* 194 */
    s32 projection_step;     /* 198 */
    s16 steps;               /* 19C */
    s32 start;               /* 1A0 */
    s32 step;                /* 1A4 */
    s16 shake;               /* 1A8 */
    s16 shake_time;          /* 1AA */
    s16 shake_stop;          /* 1AC */
    s32 shake_amplitude[3];  /* 1B0 */
    s32 shake_step[3];       /* 1BC */
    SavedView saved_view;    /* 1C8: saved by 8009b9a0 */
    SVECTOR world_angles;    /* 1D4 */
    SVECTOR anchor;          /* 1DC */
    MATRIX scaled_world;     /* 1E4 */
    MATRIX unk204;           /* 204 */
    MATRIX world_matrix;     /* 224 */
    s32 scale;               /* 244 */
    ModelLight lights[3];    /* 248: direction and 1.3.12 colour (80030a30) */
    s16 back_color[3];       /* 284 */
    u8 unk28A[2];
    FieldComponents components; /* 28C: 800afb0c */
} FieldView;

/* Field work state 800b2078..800b235c, one object: the block 800a3f4c saves
 * whole; stores to its members do not pass loads of other members, and code
 * addresses members relative to one another (never past 800b2358). */
typedef struct FieldWork {
    s16 unk2078;               /* 2078: screen effect running (800a484c) */
    s16 unk207A;               /* 207A */
    s16 effect_steps;          /* 207C */
    u8 unk207E[2];
    s32 effect_value[6];       /* 2080: 16.16 */
    s32 effect_step[6];        /* 2098 */
    s16 unk20B0[2];            /* 20B0 */
    void *effect_buffers[4];   /* 20B4 */
    FadeChannel fades[2];      /* 20C4: screen fade channels */
    u16 open_windows;          /* 2174: bit per open dialogue window; talk
                                * is inhibited while any is set */
    s16 encounter_inhibition;  /* 2176 */
    s16 terrain_angle;         /* 2178 */
    u16 input_mask;            /* 217A */
    s32 unk217C;               /* 217C */
    s32 unk2180;               /* 2180 */
    SVECTOR sprite_angles;     /* 2184: sprite view rotation */
    s16 scale;                 /* 218C: offset scale (8007b614) */
    s16 sprite_gate;           /* 218E: colour pass-through gate (80075b08) */
    u8 fog_color[4];           /* 2190 */
    u8 far_color[4];           /* 2194 */
    s16 fog_range[2];          /* 2198 */
    u8 clear_color[3];         /* 219C */
    u8 party_bits;             /* 219F */
    s16 unk21A0[6];            /* 21A0: set by event 8008cfec */
    s16 emitter_range;         /* 21AC */
    s16 piece_drift[3];        /* 21AE */
    s16 unk21B4;               /* 21B4 */
    u8 unk21B6[2];
    s32 last_sound_effect;     /* 21B8 */
    s32 unk21BC[3];            /* 21BC */
    u8 unk21C8[0x21CC - 0x21C8];
    u8 party_processing_mode;  /* 21CC */
    u8 camera_floor_fixed;     /* 21CD */
    u8 preserve_nonplayer_motion; /* 21CE */
    u8 forced_position;        /* 21CF */
    u8 script_control[2];      /* 21D0 */
    u8 piece_drift_mode;       /* 21D2 */
    u8 unk21D3;
    s16 unk21D4;               /* 21D4 */
    s16 text_speed;            /* 21D6 */
    s32 camera_counter;        /* 21D8 */
    s16 unk21DC[4];            /* 21DC: per 801e layer */
    s16 unk21E4[4];            /* 21E4 */
    SVECTOR layer_positions[4]; /* 21EC: per 801e layer, its root's first position (801e742c) */
    s32 layer_scales[4];       /* 220C: per 801e layer, its actor's scale */
    s16 unk221C[5][3];         /* 221C */
    u8 unk223A[2];
    s16 unk223C[3][3];         /* 223C */
    u8 unk224E[0x225C - 0x224E];
    u8 unk225C[3];             /* 225C */
    u8 unk225F[0x2264 - 0x225F]; /* 225F: event byte table */
    s32 unk2264;               /* 2264: 801e layer enabled */
    s32 unk2268;               /* 2268 */
    s32 controlled;            /* 226C: controlled actor/descriptor index */
    u16 encounter_music[16];   /* 2270: per formation of the map's set */
    s16 battle_music;          /* 2290: the chosen encounter's battle music */
    u8 unk2292[2];
    s32 unk2294;               /* 2294 */
    s32 unk2298;               /* 2298 */
    s32 unk229C;               /* 229C: at most 32 */
    u16 unk22A0[32];           /* 22A0: distinct random picks */
    s16 unk22E0;               /* 22E0 */
    s16 emitter_descriptor[3]; /* 22E2: descriptor each emitter follows, or -1 */
    s16 unk22E8[3][4];         /* 22E8 */
    s16 unk2300[3][4];         /* 2300 */
    s32 unk2318[3];            /* 2318 */
    s16 emitter_position[3][4]; /* 2324 */
    u16 effects_kept;          /* 233C: bit per effect pair still playing */
    u16 unk233E;               /* 233E */
    s16 repeat_delay;          /* 2340 */
    s16 repeat_remaining;      /* 2342 */
    s16 jump_mode;             /* 2344 */
    s16 animation_mode;        /* 2346 */
    u16 unk2348;               /* 2348: gather override */
    s16 unk234A;               /* 234A */
    s16 gear_riding_lock_override; /* 234C: mode_gear_riding_lock's value, 0xff none */
    s16 followers_idle;        /* 234E */
    u32 unk2350;               /* 2350: saved player flags */
    u8 unk2354;                /* 2354 */
    u8 unk2355;                /* 2355 */
    u8 unk2356;                /* 2356 */
    u8 unk2357;                /* 2357 */
    u8 unk2358;                /* 2358 */
    u8 unk2359[0x235C - 0x2359];
} FieldWork;

/* The objects of these types. */
extern FieldView field_view;         /* the view, with the loaded components */
extern FieldWork field_work;         /* the work block */
/* The commons after the work block are objects of their own: code addresses
 * 800b235c, 800b2360, 800b236c and 800b2370 from symbols of their own (as
 * members of the block, 800815f0, 80081c54, 80085738, 8008848c and 800859dc
 * compile differently), and the launch fields from 800b2374. */
extern FieldDrawBlock field_draw_blocks[2]; /* the two draw blocks */
extern FieldDrawBlock *field_current_draw_block;   /* the current draw block (the buffer,
                                      * field_draw_buffer_index, is in field/monitor.h) */

#endif
