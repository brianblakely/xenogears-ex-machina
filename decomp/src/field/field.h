#ifndef FIELD_FIELD_H
#define FIELD_FIELD_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libcd.h"
#include "psyq/libgte.h"
#include "psyq/libetc.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "resident/gamedata.h"
#include "resident/sprite.h"

/* A 16.16 fixed-point value; code also reads its whole part alone. */
typedef union {
    s32 value;
    struct {
        u16 fraction;
        s16 whole;
    } s;
} Fixed;

#define WHOLE(value) (((Fixed *)&(value))->s.whole)

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


/* One of an actor's eight script slots (analysis/formats/field-lifecycle.md). */
typedef struct ScriptSlot {
    u16 resume_pc;      /* 0 */
    u8 countdown;       /* 2 */
    u8 tag;             /* 3 */
    u32 value : 16;     /* 4: per-slot argument (e.g. move speed) */
    u32 unk16 : 2;
    u32 priority : 4;   /* bits 18-21 */
    u32 unk22 : 1;
    u32 move_mode : 2;  /* bits 23-24 */
    u32 unk25 : 7;
} ScriptSlot;

/* An actor's packed state word (+12c). */
typedef struct {
    u32 mode : 2;   /* 0-1 */
    u32 unk2 : 3;   /* 2-4 */
    u32 unk5 : 1;   /* 5 */
    u32 depth : 3;  /* 6-8: script call depth */
    u32 octant : 3; /* 9-11 */
    u32 unk12 : 1;  /* 12 */
    u32 layer : 3;  /* 13-15: 801e layer */
    u32 unk16 : 2;  /* 16-17 */
    u32 unk18 : 10; /* 18-27 */
    u32 unk28 : 4;
} ActorState;

/* One 0x138-byte event actor record. */
typedef struct FieldActor {
    u32 flags;           /* 000 */
    u32 layer_flags;     /* 004: bits 3+ switch collision layers off */
    s16 triangle[4];     /* 008: current collision triangle per layer */
    s16 layer;           /* 010 */
    u8 unk012[2];
    u32 unk014;          /* 014 */
    s16 unk18;           /* 018 */
    s16 height;          /* 01A */
    Fixed gravity;       /* 01C: 16.16 */
    s32 position[3];     /* 020: 16.16 x, y, z */
    u8 unk02C[4];
    s32 unk030[3];       /* 030 */
    u8 unk03C[4];
    s32 unk40[3];        /* 040 */
    u8 unk04C[4];
    s32 unk50[3];        /* 050 */
    u8 unk05C[4];
    s16 unk60;           /* 060 */
    s16 unk62;           /* 062 */
    s16 unk64;           /* 064 */
    u8 unk066[2];
    s16 last_position[3]; /* 068: whole x, y, z saved before the update */
    u16 stuck;           /* 06E: gather steps without moving */
    s16 unk70;           /* 070 */
    s16 unk72;           /* 072 */
    u8 unk074;           /* 074 */
    u8 unk075;           /* 075 */
    s16 unk76;           /* 076 */
    u16 call_stack[4];   /* 078: return PCs */
    u8 character;        /* 080 */
    u8 unk081;
    u8 unk82;            /* 082 */
    u8 unk83;            /* 083 */
    s32 unk84;           /* 084 */
    s16 unk88;           /* 088 */
    s16 unk8A;           /* 08A */
    ScriptSlot slots[8]; /* 08C */
    u16 pc;              /* 0CC: event working PC, relative to the bytecode */
    u8 slot;             /* 0CE: selected script slot */
    u8 unk0CF;
    s32 target[3];       /* 0D0: move target x, y, z */
    u8 unk0DC[0xE2 - 0xDC];
    u8 unkE2;            /* 0E2 */
    u8 unk0E3;
    s16 unkE4;           /* 0E4 */
    s16 unkE6;           /* 0E6 */
    s16 unkE8;           /* 0E8 */
    s16 unk0EA;          /* 0EA */
    s16 unkEC;           /* 0EC */
    s16 unkEE;           /* 0EE */
    s32 unkF0;           /* 0F0: 16.16 vertical (fall) or push speed */
    s16 scale[3];        /* 0F4 */
    u8 unk0FA[2];
    u8 color0[3];        /* 0FC */
    u8 color1[3];        /* 0FF */
    u16 unk102;          /* 102 */
    s16 heading;         /* 104 */
    s16 heading_goal;    /* 106: bit 15 once turned */
    s16 unk108;          /* 108 */
    u16 sound;           /* 10A */
    u8 sound_volume;     /* 10C */
    u8 sound_mode;       /* 10D: 0xff off */
    u8 unk10E[0x110 - 0x10E];
    struct PlatformLink *link; /* 110: linked platform state (allocated) */
    void *unk114;        /* 114 */
    s32 *list;           /* 118 */
    u16 unk11C;          /* 11C */
    s16 unk11E;          /* 11E */
    void *unk120;        /* 120 */
    s16 unk124;          /* 124: -1 when +120 is free */
    u8 unk126;           /* 126 */
    u8 unk127;           /* 127: sprite slot */
    u16 unk128;          /* 128 */
    u8 unk12A[2];
    union {
        u32 word;
        ActorState bits;
    } state;             /* 12C */
    u32 unk130 : 9;      /* 130: bits 0-8 */
    u32 unk130_9 : 10;   /* bits 9-18 */
    u32 unk130_19 : 9;   /* bits 19-27 */
    u32 sprite_kind : 2; /* bits 28-29 */
    u32 unk130_30 : 2;
    u32 unk134;          /* 134 */
} FieldActor;

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

/* A 14-byte collision triangle; +0c indexes the attribute table. */
typedef struct {
    s16 unk00[6];
    u8 attribute;
    u8 unk0D;
} CollisionTriangle;

/* A resident text box (80034614/800345e0/800346d4/80033cd0/80034800). */
typedef struct {
    u8 unk00[0x10];
    u16 flags;       /* 10: bit 2 keeps the window open */
    u8 unk12[0x68 - 0x12];
    u8 speed;        /* 68: 1 at text speed 8, else 2 */
    u8 unk69[3];
    u8 unk6C;        /* 6C */
    u8 unk6D[3];
    s32 vars[4];     /* 70: event variables 16-1c when opened */
    s16 unk80;       /* 80: variable 1c again */
    s16 unk82;       /* 82 */
    s16 unk84;       /* 84 */
    u8 unk86[0x90 - 0x86];
    s32 unk90;       /* 90 */
} TextBox;           /* the resident code reads the window area that follows at +94 */

/* A dialogue window's frame (window + 0ac): its area and the backing and
 * border packets. The window code addresses these members from the frame,
 * so its address (800c2744 + window) is the base it keeps. */
typedef struct {
    RECT rect;                  /* 00 (0AC): the window's area */
    u8 unk08[0x18 - 0x08];
    DR_MODE back_modes[2];      /* 18 (0C4): per buffer */
    TILE back[2];               /* 30 (0DC): the backing tile per buffer */
    DR_MODE border_modes[2][10]; /* 50 (0FC): per buffer, eight used */
    SPRT border[2][10];         /* 140 (1EC): per buffer, eight used */
} DialogueFrame;

/* A dialogue window's choice (window + 37c): its state, the selectable
 * lines and the cursor packets. The window code addresses the packets from
 * this struct's base. */
typedef struct {
    s16 status;       /* 00 (37C): zero while a choice is shown */
    s16 first;        /* 02 (37E): first selectable line */
    s16 count;        /* 04 (380): selectable line count */
    s16 index;        /* 06 (382): selected line */
    DR_MODE modes[2]; /* 08 (384): per buffer */
    SPRT cursor[2];   /* 20 (39C): the choice cursor per buffer */
} DialogueChoice;

/* A dialogue window's waiting prompt (window + 3c4). */
typedef struct {
    s16 status;       /* 00 (3C4): zero while waiting for the player */
    u8 unk02[2];
    DR_MODE modes[2]; /* 04 (3C8): per buffer */
    SPRT sprite[2];   /* 1C (3E0): the waiting prompt per buffer */
} DialoguePrompt;

/* One of the four 0x498-byte dialogue windows at 800c2698. */
typedef struct DialogueWindow {
    DR_MODE modes[2]; /* 000: per buffer */
    TextBox text;    /* 018 */
    DialogueFrame frame; /* 0AC */
    DialogueChoice choice; /* 37C */
    DialoguePrompt prompt; /* 3C4 */
    s16 timer;       /* 408: opening steps left */
    s16 prompt_delay; /* 40A */
    u16 style;       /* 40C: 1 above, 0x81 below the speaker; 0x20 portrait on
                      * the right, 0x40 no frame (kept) */
    s16 busy;        /* 40E */
    u16 age;         /* 410: 0xffff when free */
    s16 unk412;      /* 412 */
    s16 cleared;     /* 414: cleared when its owner hides */
    s16 owner;       /* 416: owning event actor */
    s16 unk418;      /* 418: descriptor index */
    u8 unk41A[2];
    Fixed slide[2];  /* 41C: x, y offset while opening (16.16) */
    s32 slide_step[2]; /* 424 */
    DR_MODE icon_modes[2]; /* 42C: per buffer */
    POLY_FT4 icon[2];  /* 444: per buffer */
    u8 unk494;       /* 494 */
    u8 unk495;       /* 495 */
    u8 unk496[0x498 - 0x496];
} DialogueWindow;

/* A model's mesh header; +20/+28 bound it. */
typedef struct {
    u8 unk00[6];
    u16 group_count; /* 06: primitive groups */
    u8 unk08[0x10 - 0x08];
    u32 *groups;     /* 10: group headers (code, flags, count << 16) and data */
    u8 unk14[0x20 - 0x14];
    s16 min[3];      /* 20 */
    s16 unk26;
    s16 max[3];      /* 28 */
    u8 unk2E[0x34 - 0x2E];
    s32 size;        /* 34: packet bytes per draw buffer */
} FieldMesh;

/* A model instance; +12 is its drawing mode. */
typedef struct {
    u8 unk00[4];
    FieldMesh *mesh; /* 04 */
    void *packets[2]; /* 08: per draw buffer */
    u8 unk10[2];
    s16 mode;        /* 12 */
    struct FieldAnimTable *anims; /* 14 */
    s16 center[3];   /* 18 */
    s16 unk1E;
    s16 radius;      /* 20 */
} FieldInstance;

/* One 0x5C-byte descriptor; one per event actor. */
typedef struct FieldDescriptor {
    FieldInstance *instance; /* 00 */
    Sprite *model;           /* 04 */
    struct FieldMarker *shadow; /* 08: drop shadow quad */
    MATRIX matrix;           /* 0C: its translation is the position */
    MATRIX transform;        /* 2C */
    FieldActor *actor;       /* 4C */
    SVECTOR rotation;        /* 50 */
    u16 flags;               /* 58 */
    u16 unk5A;               /* 5A: bit 0 while it owns a sprite */
} FieldDescriptor;

/* The shadow pass reads the flags halfword with its padding as one word. */
#define DESCRIPTOR_FLAGS_WORD(d) (*(u32 *)&(d)->flags)

/* A sprite's sequencer (Sprite +7C), as the resident's code casts it. */
#define SPRITE_SEQUENCER(sprite) ((SpriteSequencer *)(sprite)->sequencer)

/* A collision attribute word, also read by byte. */
typedef union {
    u32 word;
    u8 bytes[4];
} Attribute;

/* The camera view saved by 8009b9a0. */
typedef struct {
    s16 octant;
    s32 projection;
    s16 elevation;
} SavedView;

/* The loaded field components (80070cc8), one object: stores to structure
 * members do not pass loads of these pointers. */
typedef struct {
    s32 descriptor_count;                      /* 800afb0c */
    FieldDescriptor *descriptors;              /* 800afb10 */
    s32 *geometry;                             /* 800afb14: count, then offsets */
    s32 *collision;                            /* 800afb18 */
    s32 *sprites;                              /* 800afb1c: sprite resource (offset table) */
    Attribute *collision_attributes;           /* 800afb20 */
    CollisionTriangle *collision_triangles[4]; /* 800afb24 */
    void *collision_vertices[4];               /* 800afb34 */
    s32 triangle_counts[4];                    /* 800afb44 */
    s16 layer_count;                           /* 800afb54 */
} FieldComponents;

/* A field light: direction and 1.3.12 color (80030a30). */
typedef struct {
    s32 direction[3];
    u16 color[3];
    u16 pad;
} FieldLight;

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
    FieldLight lights[3];    /* 248 */
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
    SVECTOR layer_angles[4];   /* 21EC: per 801e layer */
    s32 layer_depths[4];       /* 220C: per 801e layer, from its +1c */
    s16 unk221C[5][3];         /* 221C */
    u8 unk223A[2];
    s16 unk223C[3][3];         /* 223C */
    u8 unk224E[0x225C - 0x224E];
    u8 unk225C[3];             /* 225C */
    u8 unk225F[0x2264 - 0x225F]; /* 225F: event byte table */
    s32 unk2264;               /* 2264: 801e layer enabled */
    s32 unk2268;               /* 2268 */
    s32 controlled;            /* 226C: controlled actor/descriptor index */
    u16 encounter_music[16];   /* 2270: per encounter kind */
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
    s16 battle_override;       /* 234C: battle-entry flag override, 0xff none */
    s16 followers_idle;        /* 234E */
    u32 unk2350;               /* 2350: saved player flags */
    u8 unk2354;                /* 2354 */
    u8 unk2355;                /* 2355 */
    u8 unk2356;                /* 2356 */
    u8 unk2357;                /* 2357 */
    u8 unk2358;                /* 2358 */
    u8 unk2359[0x235C - 0x2359];
} FieldWork;

/* A loaded sound-effect bank; +14 is its id. */
typedef struct FieldSoundBank {
    u8 unk00[0x14];
    u16 id;
} FieldSoundBank;

/* Parameters events set at 800b0080, one object. */
typedef struct FieldEventParams {
    s16 unk80[8]; /* 800b0080 */
    s32 unk90;    /* 800b0090 */
    s32 unk94;    /* 800b0094 */
    s32 unk98;    /* 800b0098 */
    u8 unk9C[4];
    u8 unkA0[3];  /* 800b00a0 */
    u8 unkA3;
    u8 unkA4[3];  /* 800b00a4 */
    u8 unkA7;
    u8 unkA8[3];  /* 800b00a8 */
    u8 unkAB;
    s16 unkAC;    /* 800b00ac */
    s16 unkAE;    /* 800b00ae */
    s16 unkB0;    /* 800b00b0 */
    s16 enabled;  /* 800b00b2 */
} FieldEventParams;

/* One 0x78-byte particle emitter: the eight at 800b02cc, and copies of
 * them per effect slot (*800c3918). */
typedef struct {
    s16 unk00;       /* 00: alive */
    u16 unk02;       /* 02: start delay */
    u16 unk04;       /* 04: life; set to 1 to release */
    u16 angle;       /* 06 */
    VECTOR position; /* 08 */
    VECTOR velocity; /* 18 */
    VECTOR unk28;    /* 28 */
    SVECTOR unk38;   /* 38 */
    SVECTOR unk40;   /* 40 */
    u8 unk48[4];     /* 48 */
    s8 unk4C[4];     /* 4C */
    POLY_FT4 quads[2];  /* 50: per draw buffer */
    SVECTOR corners[4]; /* A0 */
} Particle;

typedef struct Record78 {
    s16 unk00;       /* 00 */
    u16 unk02;       /* 02: start delay */
    u16 unk04;       /* 04: lifetime, 7fff lasting */
    s16 count;       /* 06: particles */
    s32 unk08;       /* 08 */
    SVECTOR unk0C;   /* 0C */
    SVECTOR unk14;   /* 14 */
    SVECTOR unk1C;   /* 1C */
    s16 unk24;       /* 24 */
    u16 unk26;       /* 26: spawn radius */
    u16 unk28;       /* 28: velocity spread */
    u16 flags;       /* 2A */
    Particle *particles; /* 2C */
    s16 unk30[8][2]; /* 30 */
    s16 unk50;       /* 50 */
    s16 unk52;       /* 52 */
    s16 unk54;       /* 54 */
    u16 unk56;       /* 56: spawn interval */
    u16 unk58;       /* 58: particle life */
    SVECTOR unk5A;   /* 5A */
    SVECTOR unk62;   /* 62 */
    u8 unk6A;        /* 6A */
    u8 unk6B;        /* 6B */
    u8 unk6C;        /* 6C */
    u8 unk6D;
    s8 unk6E;        /* 6E */
    s8 unk6F;        /* 6F */
    u8 unk70;        /* 70 */
    u8 unk71;
    s16 unk72;       /* 72 */
    s16 unk74;       /* 74 */
    u16 unk76;       /* 76: particle angle */
} Record78;

/* A pointer marker: its quad's corners and primitive per buffer. */
typedef struct FieldMarker {
    SVECTOR v[4];
    POLY_FT4 poly[2];
} FieldMarker;

/* One 2 KiB music-wave stream chunk. */
typedef struct {
    u32 words[0x200];
} WaveChunk;

/* One of the three positional sound-emitter slots (800afe88). */
typedef struct EmitterSlot {
    u16 actor;  /* descriptor the sound follows */
    u16 sound;  /* 0xffff when free */
    u16 unk4;
} EmitterSlot;

/* One of the three field light/lookup slots at 800b06a4. */
typedef struct FieldSlot6 {
    s16 a;
    s16 b;
    s16 c;
} FieldSlot6;

/* The event package (field component 5, *800adbf8). */
typedef struct {
    u32 unsigned_bits[32]; /* 00: bit per variable read unsigned */
    s32 count;             /* 80: actors */
    u16 entries[32];       /* 84: 32 event entry PCs per actor */
} EventPackage;

/* Resident services. */
extern void func_8001B66C(void);
extern void func_8002945C(WaveChunk *chunk);
extern void func_8003827C(void *bank, s32 size);
extern s32 func_800380D0(void *data, s32 size, s32);
extern void func_8003A948(s32 sequence, s32, s32);
extern void func_8003A9BC(s32 sequence, s32, s32);
extern void func_800345E0(void *text);
extern s32 func_80033CD0(TextBox *text);
extern void func_80034714(TextBox *text, s32);
extern void func_80034888(TextBox *text, u_long *ot, s32 buffer);
extern void func_8007E1C0(u_long *ot, s32 buffer, s32 window);
extern s32 D_800ADE94; /* dialogue cursor frame */
extern s32 D_800ADE98; /* dialogue ticks */
extern void func_80034614(void *text);
extern void func_800346D4(void *text);
extern void func_8002DFF0(s32 w, s32 h);
extern void func_80038428(void *bank);
extern void func_8003A344(s32 voice, s32 volume);
extern void func_8003A55C(s32 voice, s32 pan);
extern void func_800379B4(s32);
extern void func_8003A838(s32 sequence, s32, s32);
extern void func_8003A89C(s32 sequence, s32, s32);
extern void func_8003AAC4(s32 sequence, s32);
extern void func_80039EC4(s32 sound, s32 voice);
extern void func_800320A4(void *block); /* keep a block */
extern s32 func_8001ACF0(s32 member);
extern void func_8003633C(s32);
extern void func_80019CD0(void);
extern s32 func_800288EC(s32 file);
extern void *func_80031BDC(s32 size, s32);
extern s32 func_80037FD8(void *data, s32);
extern void func_80038310(s32 bank);
extern void func_800399D4(s32 sequence);
extern void func_80039C4C(s32 sequence);
extern void func_8003BDFC(s32);
extern s32 func_80028B14(void);
extern void func_800295D8(s32 file, void *ring, s32, s32);
extern void func_8003852C(void *bank);
extern void func_80039F9C(s32 id, s16 voice, s16 volume, s16 pan);
extern void func_80039FF8(void);
extern void func_8003A20C(s32 voice);
extern s32 func_8001B484(s32 file, s32);
extern s32 func_80028470(s32 directory, s32);
extern s32 func_800286CC(void);
extern void func_800320B8(void *block);
extern void func_80032498(s32 tag, s32);
extern void func_80033698(s32, s32);
extern void func_8003747C(s32);
extern void func_800374E8(s32, s32, s32, s32, s32, s32, s32, s32, s32, s32, s32);
extern void func_80021B98(void *, s32 r, s32 g, s32 b);
extern s32 func_80028A60(s32 mode); /* wait for the disc (1: poll) */
extern void func_80029EB0(s32 file, void *ring, s32, s32, s32, s32, s32, s32, s32, s32);
extern void *func_8002A260(s32 sectors, s32);
extern void func_800320E8(void *);
extern void func_80032EB4(void *source, void *destination);
extern MATRIX *func_8003F738(SVECTOR *angles, MATRIX *m); /* RotMatrix */
extern void func_80030A30(s32 index, FieldLight *light);
extern void func_80030B14(MATRIX *m);

/* Field overlay. */
extern s32 func_8008A790(s32 id, s32 *slot);
extern void func_8008A7DC(s32 member, s32 slot);
extern void func_80085560(s32 file, s32 unused, void (*callback)(s32));
extern void func_800859DC(WaveChunk *chunk);
extern void func_80086024(void);
extern s32 func_80085F30(void);
extern void func_80085FB8(void);
extern void func_800A47D4(void);
extern void func_800ACE24(void);
extern void func_800A30B4(void);
extern void func_80076AC0(s32 index, s32 a, void *sprite, s32 b, s32 c, s32 d, s32 e);
extern void func_800A0C94(void);
extern void func_800A0C4C(void);
extern void func_800931F8(void);
extern void func_800A31E8(void);
extern void func_800A4CC4(s32, s32, s32, s32, s32, s32, s32);
extern void func_80086BA8(void);
extern void func_801E7D14(MATRIX *world, s16 (*table)[3], u_long *ot, s32 buffer, s32);
extern void func_80281B00(char *name);
extern void func_80284EA4(void);
extern void func_800AABD8(void);
extern void func_800AAC08(void);
extern void func_800AADC8(s32, s32, s32, s32);
extern void func_800AAE4C(s32, s32, s32, s32);
extern void func_80070594(MATRIX *m);
extern void func_80086D8C(void);
extern void func_80086078(s32 distance, u32 *out, s32 volume);
extern void func_80086200(s32 index, s32 *x, s32 *y);
extern void func_800AA9DC(FieldInstance *instance);
extern s32 func_8008DB68(s32 member, s32 amount);
extern s32 func_8008DBF0(s32 member, s32 amount);
extern s32 func_8009D044(s32 offset, s32 flags); /* operand, immediate with flag 0x10 */
extern void func_800A94A4(s32 actor);
extern void func_8007AF74(s32 port);
extern void func_8007AA44(FieldMarker *marker);
extern void func_80080A74(s32 index);
extern u32 func_80080968(struct FieldActor *actor);
extern void func_8008B978(s32);
extern void func_80071D08(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr);
extern s32 func_8009D000(s32 offset, s32 flags); /* operand, immediate with flag 0x20 */
extern void func_80086590(VECTOR *target);
extern s32 func_8009CFBC(s32 offset, s32 flags); /* operand, immediate with flag 0x40 */
extern void func_80074700(void);
/* The field load (80070cc8). */
/* One sprite slot's VRAM area; slots with `shared` set keep their image. */
typedef struct {
    u16 x;
    u16 y;
    u16 unk4;
    s16 shared;
} SpriteSlot;

typedef struct SpriteSlotTable {
    SpriteSlot slot[32];
} SpriteSlotTable;

/* Components of the map bundle: sizes at +10c and offsets at +130. */
enum {
    BUNDLE_PALETTES,
    BUNDLE_COLLISION,
    BUNDLE_MODELS,
    BUNDLE_SPRITES,
    BUNDLE_IMAGES,
    BUNDLE_EVENTS,
    BUNDLE_MESSAGES,
    BUNDLE_8,
    BUNDLE_ZONES
};

/* The map bundle read ahead of the field load (*8005a4e0). */
typedef struct {
    SpriteSlotTable slots; /* 000 */
    u8 unk100[0x10C - 0x100];
    s32 sizes[9];          /* 10C */
    s32 offsets[9];        /* 130 */
    s16 view[0x1C];        /* 154: lights and background (8006fdec) */
    u16 descriptor_count;  /* 18C */
    u16 unk18E;
    u16 descriptors[1];    /* 190: flags, rotation[3], position[3], model */
} FieldBundle;

extern FieldBundle *D_8005A4E0;
/* The size and the data of component k, read as words at their byte offsets
 * in the header (the field load reads them so, not as struct members). */
#define BUNDLE_SIZE(k) (*(s32 *)((u8 *)D_8005A4E0 + 0x10C + (k) * 4))
#define BUNDLE_COMPONENT(k) ((void *)(*(s32 *)((u8 *)D_8005A4E0 + 0x130 + (k) * 4) + (s32)D_8005A4E0))
extern SpriteSlotTable D_800B1F78;
extern u8 D_800658DC[];      /* messages */
extern s32 D_800AFD10;       /* attributes before the first triangle */
extern s32 D_8004F330;
extern s32 D_8004F334;
extern Panorama *D_800B007C;
extern void func_80022A70(void *tim, s32 x, s32 y);
extern void func_8002C3E8(void *model);
extern void func_8002CB54(FieldMesh *mesh, void **packets, void **packets2);
extern void func_8002C8CC(FieldMesh *mesh, void *packets, s32 mode);
extern struct FieldAnimTable *func_800303C8(FieldMesh *mesh, s32);
extern void func_8002C644(FieldMesh *mesh);
extern void func_802812A4(void);
extern void func_800A28D4(void);
extern Panorama *func_8002709C(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y, s32 mode,
                               s32 turn, s32 *position, u8 *colours, s32 fill_scale, s32 fade_range,
                               s32 fade_start);
/* The sprite pass (80075b44). */
extern CVECTOR D_80059598; /* fog color */
extern s32 func_8009A514(void); /* camera octant */
/* The field sprite factory (80076ac0). */
extern s32 D_800AFC74;       /* sprites created */
extern Sprite *func_80024524(void *data, s16 a, s16 b, s16 x, s16 y, s32 c);
extern Sprite *func_80024294(void *data, s16 a, s16 b, s16 x, s16 y, s32 c, s32 bank);
extern s32 D_8004F37C; /* shadows off */
/* The model pass (800748e8). */
extern s32 D_80059578; /* models drawn */
extern s32 D_800595C0; /* primitives drawn */
extern s32 D_80050104; /* model level of detail */
extern void func_8002C6E0(s32 r, s32 g, s32 b); /* fog color */
extern void func_80030C40(s32 r, s32 g, s32 b); /* background color */
extern void func_800305D8(void *list);
extern void func_8002C700(FieldMesh *mesh, void *packets, u32 *ot, s32 mode);
extern s32 func_800AAA74(FieldInstance *instance);
extern void func_801E72CC(MATRIX *m, MATRIX *work, s32 a, s32 b);
/* The field frame (8007554c). */
extern s32 D_800ADB9C; /* frame start time */
extern s32 D_800ADBA0; /* frame draw time */
extern void func_800748E8(void);
extern void func_80086908(void);
extern void func_800A9688(void);
extern void func_800A4DAC(void);
extern void func_800A84C0(void);
extern void func_800ABEC8(void);
extern void func_800920D8(void);
extern void func_80281400(void);
extern void func_80281450(void);
extern void func_80032CB8(void);
/* The compass (80074108). */
extern u16 D_800ADC24[8];     /* heading octant bit per palette row */
extern DVECTOR D_800ADC34[4]; /* letter x, z offsets */
extern s16 D_800ADB48;        /* needle heading */
extern s16 D_800ADB4A;        /* needle goal */
extern u16 D_800AFC08[16];    /* compass colors */
extern u16 D_800AFD24[128];   /* compass palette */
extern RECT D_800B004C;       /* compass palette area */
extern FieldMarker D_800B06BC[25]; /* ring, letters, needle and pointer quads */
extern s32 D_8004F378;
extern void func_8008110C(void);
extern void func_800722F4(void);
extern s32 func_80073988(s32 angle, s32 goal, s32 step);
extern void func_800223B0(Sprite *model, s16 angle);
extern void func_80021FE0(Sprite *model, s32 heading); /* set its facing */
extern MATRIX D_800AFC30; /* sprite view rotation matrix */
extern s32 D_800B00B4;     /* camera pitch */
extern s32 func_8007B1C4(s32 x, s32 z, s32 layer, SVECTOR *point, VECTOR *normal); /* floor height */
extern s32 D_800ADBAC; /* camera frames settling */
extern s32 D_800ADBB0; /* camera frames releasing */
extern s32 func_8007CD80(VECTOR *point, SVECTOR *edge, DVECTOR *segment);
extern void func_800723E4(DVECTOR *a, DVECTOR *b, DVECTOR *out);
extern void func_80073684(VECTOR *point, VECTOR *center);
extern void func_80073750(MATRIX *view, VECTOR *eye, VECTOR *target, VECTOR *up);
extern void func_8008004C(u_long *ot, s32 buffer);
extern void func_800805F4(void);
extern s32 func_800A2030(void);
extern void func_80071F64(s32 x, s32 y, s32 w, s32 h);
extern void func_8008E0DC(s32 flags);
extern void func_8008E148(s32 flags);
extern void func_8008E498(s32 value);
extern void func_8008E718(void);
extern s32 func_8009CDB4(s32 offset); /* actor selector; 0xff when none */
extern s32 func_800A3018(s32 reference); /* read an event variable */
extern void func_80072254(s32 index);
extern void func_800863E8(s32 id);
extern s32 func_8008CF3C(s32 id);
extern void func_8008A93C(void);
extern void func_800A484C(s32);
extern void func_801E8330(s32, s32, s32);
extern s32 func_8008A558(void);
extern void func_800A98E8(s32 actor, s32 value);
extern s32 func_800A99A8(s32 owner);
extern void func_80088D38(s32);
extern void func_800A8BA4(void);
extern void func_800A915C(void);
extern void func_800A91F0(void);
extern s32 func_8009CF78(s32 offset, s32 flags); /* operand, immediate with flag 0x80 */
extern void func_800A3074(s32 reference, s32 value); /* write an event variable */
extern s32 func_800ACDB8(s32 offset); /* raw halfword operand */
extern s32 func_800ACDEC(s32 offset); /* operand: bit 15 immediate, else variable */
extern void func_80086A1C(s32 emitter, s32 *position);
extern void func_80081F80(Sprite *sprite, s16 heading, FieldDescriptor *descriptor);
extern void func_800821F4(void *model, s32 animation, FieldDescriptor *descriptor);
extern s32 func_8009FA00(s32 character);
extern s32 func_8008492C(FieldActor *actor);
extern s32 func_8007D3D4(FieldActor *actor, s32 layer, s32 *floor, VECTOR *normal, s16 *triangle, s32 *upper);
extern void func_80081C54(s32 index);
extern void func_800379C8(char *format, ...); /* debug print */
extern s32 func_800854D0(void);
extern void func_800855C8(s32 id, s32 volume, s32 pan, s32 channel);
extern s32 func_80099A4C(s32 dx, s32 dz);
extern s32 func_8007D8B4(s32 x, s32 y, s32 z);
extern s32 func_8007F6F8(s16 window);
extern void func_800775F8(void);
extern void func_80071EE8(void);
extern void func_80077884(void);
extern void func_80077AB4(void);
extern s32 func_80085C90(s32);
extern void func_8007D93C(s32 channel);
extern void func_8007AD8C(void *pad0, void *pad1);
extern void func_8007ADA4(s32 left, s32 right, s32 top, s32 bottom);
extern void func_8007AE14(s32 x_divisor, s32 y_divisor);
extern void func_8007AE2C(s32 port, s32 x, s32 y);
extern void func_8007DA44(u_long *ot, s32 buffer);
extern void func_80074038(MATRIX *to, MATRIX *from);
extern void func_80074078(MATRIX *to, MATRIX *from);
extern void func_8007409C(MATRIX *to, MATRIX *from);
extern void func_80072140(MATRIX *m);
extern s32 func_80073930(s32 angle, s32 goal, s32 step);
extern void func_80078C5C(void);
extern void func_802815B0(void);

/* Resident state. */
extern s32 D_8004F33C; /* current music wave */
extern s32 D_8004F354;
extern s32 D_8006258C; /* music wave bank */
extern s32 D_8006FABC[3]; /* party sprite ids per slot */
extern s32 D_8004F380;
extern void *D_8005A4BC; /* field sound-effect bank copy */
extern u8 D_8005061C[6];
extern s32 D_8004F36C; /* sequence playing */
extern s32 D_80062528; /* current sequence */
extern s32 D_80062590[3];
extern s32 D_8005A444[3]; /* party members */
extern s32 D_8004F300;
extern u8 D_80050622;
extern s32 D_8004F2FC; /* cached sequence */
extern s32 D_8004F338; /* loaded music sequence */
extern s32 D_8004F340; /* -1: start the sequence at full volume */
extern s32 D_8004F348; /* reuse the cached sequence */
extern s32 D_8004F358; /* sequence read pending */
extern s32 D_8004F35C; /* sequence active */
extern s32 D_8004F360; /* wave loaded this request */
extern u8 D_80062648[]; /* sequence buffer */
extern s32 func_80039850(void *data); /* load a music sequence */
extern void func_80039A80(s32 sequence, s32 volume, s32);
extern void func_80039B68(s32 sequence, s32 volume, s32 fade);
extern s32 D_8004F364;
extern s32 D_8004F368; /* shared wave bank released */
extern s16 D_8004F384;
extern s32 D_80059560;
extern s32 D_8006251C; /* shared wave bank */
extern s32 D_8004F32C;
extern void *D_8006259C; /* field sound-effect bank */
extern u8 D_80059179; /* battle-entry flag */
extern s32 D_8004F308; /* pending sound; -1 until resolved */
extern s32 D_8004F324;
extern void *D_8005A414[3];
extern s32 D_8004F34C; /* current map */

extern u8 D_800625FC[2][0x22]; /* pad buffers */

/* Field state. */
extern s32 D_800AFC54;
extern void *D_800C3A1C; /* music-wave gather buffer */
extern s16 D_800ADB54;
extern s32 D_800B068C[4];
extern s32 D_800ADB98;
extern s32 D_800ADC0C;
extern s16 D_800AEA2C[4]; /* party masks */
extern s32 D_800ADBDC;
extern s32 D_800ADBE4;
extern s32 D_800ADBE8;
extern EventPackage *D_800ADBF8;
extern s32 D_800ADBFC; /* event actor count */
extern void *D_800ADBC0; /* pending party sprite buffer */
extern s32 D_800ADBC8;
extern s32 D_800ADBCC; /* pending party slot */
extern FieldEventParams D_800B0080;
extern Record78 D_800B02CC[8]; /* the eight particle emitters */
extern u16 D_800AE060[][2]; /* movie sound timeline: time, sound */
/* The movie request block at 800c3a20, one object: the instructions that
 * fill it keep their stores ahead of loads through the actor pointer, and
 * the movie player (800a7c58) hoists one base for its fields. */
typedef struct FieldMovieRequest {
    s16 file;        /* 20 */
    u16 x;           /* 22: display position */
    u16 y;           /* 24 */
    u16 source_x;    /* 26 */
    u16 source_y;    /* 28 */
    u16 unk2A;       /* 2A */
    u16 sound_start; /* 2C: movie sound time origin */
    u16 unk2E;       /* 2E */
    u16 mode;        /* 30: low nibble layout, 0x40/0xc0 fade */
    u16 width;       /* 32 */
    u16 height;      /* 34 */
    u16 depth24;     /* 36: 1 for a 24-bit display */
    s16 sound_bank;  /* 38: 0xff none */
    u16 unk3A;       /* 3A */
} FieldMovieRequest;

extern FieldMovieRequest D_800C3A20;
#define FIELD_MOVIE D_800C3A20
extern s32 D_800ADB6C; /* movie stopped */
extern s32 D_800ADB74; /* movie mode */
extern s32 D_800ADB80;
extern s32 D_800AFE74;
extern s32 D_800B00E4;
extern s32 D_800C3A64; /* movie sound timeline position */
extern s32 D_800ADB50;
extern s32 D_800ADB78;
extern Panorama *D_800B007C;

extern s32 D_800ADB38; /* requested transition */
extern s32 D_800ADB3C; /* transition operand */
extern s32 D_800ADB7C;
extern s32 D_800ADB1C; /* 801e module loaded */
extern s32 D_800ADB88;
extern s32 D_800ADB8C;
extern s32 D_800AFC7C; /* batch limit */
extern s32 D_800AFD1C; /* current actor index */
extern s32 D_800AFE84;
extern s32 D_800B06A0;
extern FieldWork D_800B2078;
/* The variables after it are objects of their own: code addresses 800b235c,
 * 800b2360, 800b236c and 800b2370 from symbols of their own (as members of
 * the block, 800815f0, 80081c54, 80085738, 8008848c and 800859dc compile
 * differently), and the launch fields from 800b2374. */
extern FieldSoundBank *D_800B235C; /* movie sound-effect bank */
extern s32 D_800B2360[3];          /* movement history index per party slot */
extern u16 D_800B236C; /* menu parameter set by ext 99; the field loop and ext 55
                        * pass it to the menu (80059171) */
extern s32 D_800B2370; /* music-wave chunks gathered */
/* An effect launch (80088674, 80088790) for ext 90 and 93. */
typedef struct FieldLaunch {
    s32 actor;       /* 2374 */
    s32 frame;       /* 2378: launch frame kind << 4 */
    s32 layer_actor; /* 237C: 801e layer actor of frame 1 */
    s32 layer_node;  /* 2380: 801e layer node of frame 1 */
    s32 record;      /* 2384: emitter record (800b02cc) being set */
} FieldLaunch;
extern FieldLaunch D_800B2374;
extern u8 *D_800ADC00; /* event bytecode */
extern void (*D_800AE6A0[])(void); /* extended event instructions */
extern EmitterSlot D_800AFE88[3];
extern void func_800862CC(s32 sound, s32 volume, s32 unused, s32 distance, s32 actor);
extern FieldActor *D_800B0078; /* current event actor */
extern s32 D_800B00C0; /* yield */
extern RECT D_800AFC58;    /* screen band saved by event op dd */
extern u16 *D_800C3A48;    /* the band's saved pixels */
extern u16 *D_800AF87C;    /* the band's working pixels */
extern s32 D_800ADBB4;
extern void *D_800B00E0; /* shared wave bank buffer */
extern void *D_800ADBB8; /* music-wave stream ring */
extern s32 D_800ADBBC;   /* stream arrivals */
extern void (*D_800AFEA4)(s32); /* stream chunk callback */
extern s32 D_800ADB58; /* descriptor whose list is read */
extern s32 D_800ADB5C; /* list position */

extern u16 D_800B14AC;
extern DialogueWindow D_800C2698[4];
extern RECT D_800AFC80[16]; /* text texture windows */
extern RECT D_800AFE3C[2]; /* fade texture windows */
/* Draw modes per buffer, one per text texture window (D_800AFC80): 0 for
 * the text, 1 the compass, 3 the distortion, 4 the grid. */
extern DR_MODE D_800B1DF4[2][16];
extern void func_8007EE0C(s32 window);
extern u16 D_800C3900; /* pad buttons that move a window's choice */
extern void func_80034874(TextBox *text, s32 line);
extern void func_8003487C(TextBox *text);
extern s32 D_800ADC10; /* scratchpad words in use */
extern s8 *D_800B0054[2]; /* pointer pad buffers */
extern u16 D_800B005C; /* pointer X divisor */
extern u16 D_800B0060; /* pointer Y divisor */
extern s32 D_800B0068[2]; /* pointer X per port */
extern s32 D_800B0070[2]; /* pointer Y per port */
extern s32 D_800C3A44; /* pointer bounds */
extern s32 D_800C3A4C;
extern s32 D_800C3A50;
extern s32 D_800C3A54;
extern s32 D_800ADB2C;
extern s32 D_800ADB34;
extern s32 D_800ADB90;
extern s32 D_800ADBA4;
extern s32 D_800ADBC4;
extern s32 D_800ADBD0;
extern s32 D_800ADB08;
extern s32 D_800ADC04; /* fade mode; fades start only in mode 2 */
extern s16 D_800ADC08; /* fade started */
extern FieldView D_800AF880;
extern s32 D_800ADB4C;
extern FieldDrawBlock *D_800C426C; /* current draw block */

extern s32 D_800C268C;
extern s32 D_800ADC18;
extern u8 D_800ADC1C[8]; /* octant bits */
extern FieldDrawBlock D_800B249C[2];
extern s32 D_800ADB0C;
extern s32 D_800ADB60; /* field stream running */
extern void *D_800ADC14; /* field stream ring */
extern FieldSlot6 D_800B06A4[3];

/* Trigger zone (field component 8): four x, y, z corners. */
typedef struct {
    s16 x;
    s16 y;
    s16 z;
} ZonePoint;

typedef struct {
    ZonePoint corner[4];
} Zone;

extern Zone *D_800ADBF4;            /* trigger zones */

/* Up to 32 texture scrolls (80027d64) created by func_800921E8. */
typedef struct WindowList {
    s16 count;
    TextureScroll *scrolls[32];
    u8 *buffers[32];
    s16 lengths[32];
} WindowList;
extern WindowList D_800AFEA8;
extern u16 D_800ADB00;
extern s16 D_800ADB02;
extern u8 D_800ADB04;
extern u8 D_800ADB05; /* 1 while character drawing is off */
extern s32 D_800ADB18;
extern s32 D_800ADB24; /* screen effect buffers allocated */
extern s32 D_800ADB44; /* last effect owner */
extern s32 D_800ADB64;
extern s32 D_800ADB68;
extern s32 D_800ADB70;
extern s32 D_800ADB84;
extern s32 D_800ADB94;
extern s32 D_800ADBA8;
extern s32 D_800ADBB4;
extern s32 D_800ADBD4;
extern s32 D_800ADBEC;
extern s32 D_800AFD04;
extern s32 D_800AFD14;
extern u16 D_800AFE9C;
extern u16 D_800AFEA0;
extern s32 D_800B0048;
extern s32 D_800B0064;
extern u8 D_800B02C8;
extern s32 D_800B14A4;
extern u16 D_800C2694;
extern u16 D_800C38F8;
extern u16 D_800C3900;
extern u16 D_800C3908;
/* Resident pad state: held, pressed and repeated buttons per port. */
extern u16 D_80059570;
extern u16 D_80059574;
extern u16 D_8005948C;
extern u16 D_80059490;
extern u16 D_800594A4;
extern u16 D_800594A8;
extern s32 D_80065848[5]; /* port 2 pointer record */
extern u32 func_80035CDC(void); /* next queued pad entry, 0 when none */
extern s32 func_8007AE78(s32 port, s32 *out);
extern s32 D_800C3A5C;
extern s32 D_800C3A60;
extern s32 D_8004F30C;
extern s32 D_8006F990[3];
extern s32 D_80050100; /* ordering-table depth shift */
extern MATRIX D_800AF85C;
extern MATRIX D_800B00E8;
extern void func_8028125C(void);
extern void func_80035DB0(void);
extern void func_8007254C(void);
extern void func_80070C84(void);
extern void func_800864B4(void);
extern void func_800A9274(void);
extern void func_800ABD18(void);

/* The five overlay sprites 800abd18 sets up, per sprite and draw buffer. */
typedef struct OverlaySprites {
    DR_MODE modes[5][2];
    SPRT sprites[5][2];
} OverlaySprites;
extern OverlaySprites D_800B0188;

extern void *D_800ADBF0;
extern void *D_800ADB20;
extern void func_8002CBBC(void *mesh);
extern void func_800306D0(void *);
extern void func_8003218C(s32 tag);
extern void func_8003748C(void);
extern void func_801E7FD4(void);
extern void func_8008083C(s32 index);
extern void func_8007999C(void);
extern void func_800A83B4(void);
extern s16 D_800C3A68[0x400];       /* event variable bank */

/* Event operand readers; each takes the byte offset from the working PC. */
extern s32 func_800ACD7C(s32 offset);  /* signed halfword */
extern s32 func_8009CD7C(s32 offset);  /* actor selector */
/* Selected operands: when the given bit of `flags` is set the operand is a
 * signed immediate halfword, otherwise a variable reference. */
extern s32 func_8009D088(s32 offset, s32 flags); /* bit 0x08 */
extern s32 func_8009D0CC(s32 offset, s32 flags); /* bit 0x04 */
extern s32 func_8009D110(s32 offset, s32 flags); /* bit 0x02 */
extern s32 func_8009D154(s32 offset, s32 flags); /* bit 0x01 */

extern s32 func_80099A04(s32 dx, s32 dy, s32 dz); /* vector length */
extern void func_80085634(s32 a, s32 b);

#define EVENT_OPERAND_BYTE(offset) (D_800ADC00[D_800B0078->pc + (offset)])

extern s32 D_800C2684; /* piece scale, 0x1000 = 1 */

/* Resident file reads (80029afc): one entry of a file list, whose zero file
 * ends it. */
typedef struct FieldFileRequest {
    u16 file;
    void *destination;
} FieldFileRequest;
extern s32 func_80029AFC(FieldFileRequest *list, s32 mode, s32 a2);
extern s32 D_8004F370;         /* 1 when the 801e module and movie library files are resident */
extern u32 D_800ADB30;         /* heap top */

/* The 801e module (file 6b9) and its layers. */
typedef struct {
    u8 unk00[0x56];
    s16 facing;      /* 56 */
    u8 unk58[0x5C - 0x58];
    s32 x;           /* 5C */
    u8 unk60[0x64 - 0x60];
    s32 z;           /* 64 */
} LayerModel;

/* An object of the 801e module layer table. */
typedef struct {
    u8 unk000[4];
    LayerModel *model; /* 004 */
    u8 unk008[0x1C - 0x8];
    s16 scale;       /* 01C */
    u8 unk01E[0x34 - 0x1E];
    u8 active;       /* 034: drawn */
    u8 unk035[0x4A - 0x35];
    u16 unk4A;       /* 04A: bit 0 hidden */
    u8 unk04C[0x60 - 0x4C];
    s16 y;           /* 060 */
    u8 unk062[0x128 - 0x62];
    s32 speed_x;     /* 128 */
    u8 unk12C[4];
    s32 speed_z;     /* 130 */
} LayerObject;

extern LayerObject *D_801E8670[]; /* 801e module layers */
extern void *D_801E8644;
extern void *D_800ADB20;          /* the 801e module */
extern void *D_8005A420[4];       /* per layer: first resource (file 6ba + id) */
extern void *D_8005A450[4];       /* per layer: second resource (file 6bb + id) */
extern FieldFileRequest D_800B2394[10]; /* the module's file list */
extern void func_801E738C(s32 a0);
extern void func_801E742C(s32 layer, s32 a1, void *resource_a, void *resource_b, s32 y, s32 a5, s32 a6, s32 a7,
                          SVECTOR *angles);
extern void func_801E8030(s32 layer);

#endif
