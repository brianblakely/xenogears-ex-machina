#ifndef FIELD_FIELD_H
#define FIELD_FIELD_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libcd.h"
#include "psyq/libgte.h"

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
typedef struct {
    DRAWENV draw;
    DRAWENV draw2;
    DISPENV disp;
    u32 ot[0x1001];      /* 0CC */
    u32 ot2[0x1001];     /* 40D0: cleared only while 800adb4c is set */
    u32 overlay_ot[8];   /* 80D4 */
} FieldDrawBlock;

/* One screen fade channel (800b20c4 + 0x58 * channel). Levels are 8.8. */
typedef struct {
    u8 modes[2][12]; /* DR_MODE per draw buffer */
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
    u32 unk16 : 16;
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
    u8 unk04C[0x60 - 0x4C];
    s16 unk60;           /* 060 */
    u8 unk062[2];
    s16 unk64;           /* 064 */
    u8 unk066[0x70 - 0x66];
    s16 unk70;           /* 070 */
    s16 unk72;           /* 072 */
    u8 unk074;           /* 074 */
    u8 unk075;           /* 075 */
    s16 unk76;           /* 076 */
    u16 call_stack[4];   /* 078: return PCs */
    s8 character;        /* 080 */
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
    u8 unk0F0[4];
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
    void *unk110;        /* 110 */
    void *unk114;        /* 114 */
    s32 *list;           /* 118 */
    u16 unk11C;          /* 11C */
    s16 unk11E;          /* 11E */
    void *unk120;        /* 120 */
    s16 unk124;          /* 124: -1 when +120 is free */
    u8 unk126[2];
    s16 unk128;          /* 128 */
    u8 unk12A[2];
    union {
        u32 word;
        ActorState bits;
    } state;             /* 12C */
    u8 unk130[4];
    u32 unk134;          /* 134 */
} FieldActor;

/* The actor's two flag words as four halfwords; events test and store them
 * sixteen bits at a time. */
#define ACTOR_FLAG_HALF(actor, n) (((u16 *)(actor))[n])

/* A 14-byte collision triangle; +0c indexes the attribute table. */
typedef struct {
    s16 unk00[6];
    u8 attribute;
    u8 unk0D;
} CollisionTriangle;

/* One of the four 0x498-byte dialogue windows at 800c2698. */
typedef struct {
    u8 unk000[0x18];
    u8 text[0x10];   /* 018: text state (80034614/800345e0/800346d4) */
    u16 flags;       /* 028: bit 2 keeps the window open */
    u8 unk02A[0xAC - 0x2A];
    RECT rect;       /* 0AC */
    u8 unk0B4[0x37C - 0xB4];
    s16 status;      /* 37C: zero while displayed */
    u8 unk37E[0x408 - 0x37E];
    s16 timer;       /* 408 */
    u8 unk40A[0x40E - 0x40A];
    s16 busy;        /* 40E */
    u16 age;         /* 410: 0xffff when free */
    s16 unk412;      /* 412 */
    s16 cleared;     /* 414: cleared when its owner hides */
    s16 owner;       /* 416: owning event actor */
    s16 unk418;      /* 418: descriptor index */
    u8 unk41A[0x494 - 0x41A];
    u8 unk494;       /* 494 */
    u8 unk495;       /* 495 */
    u8 unk496[0x498 - 0x496];
} DialogueWindow;

/* The model object at descriptor offset 04. */
typedef struct {
    s32 position[3]; /* 00 */
    s32 unk0C;       /* 0C */
    s32 unk10;       /* 10 */
    s32 unk14;       /* 14 */
    s32 unk18;       /* 18 */
    u8 unk1C[0x2C - 0x1C];
    s16 unk2C;       /* 2C */
    u8 unk2E[0x82 - 0x2E];
    s16 unk82;       /* 82 */
    u16 unk84;       /* 84 */
} FieldModel;

/* A model's mesh header; +20/+28 bound it. */
typedef struct {
    u8 unk00[0x20];
    s16 min[3];      /* 20 */
    s16 unk26;
    s16 max[3];      /* 28 */
} FieldMesh;

/* A model instance; +12 is its drawing mode. */
typedef struct {
    u8 unk00[4];
    FieldMesh *mesh; /* 04 */
    u8 unk08[0x12 - 0x08];
    s16 mode;        /* 12 */
    u8 unk14[4];
    s16 center[3];   /* 18 */
    s16 unk1E;
    s16 radius;      /* 20 */
} FieldInstance;

/* One 0x5C-byte descriptor; one per event actor. */
typedef struct FieldDescriptor {
    FieldInstance *instance; /* 00 */
    FieldModel *model;       /* 04 */
    void *unk08;             /* 08 */
    MATRIX matrix;           /* 0C: its translation is the position */
    MATRIX transform;        /* 2C */
    FieldActor *actor;       /* 4C */
    SVECTOR rotation;        /* 50 */
    u16 flags;               /* 58 */
    u8 unk5A[0x5C - 0x5A];
} FieldDescriptor;

/* A sprite's sequencer; +14 names the descriptor it belongs to. */
typedef struct {
    u8 unk00[0x14];
    s16 actor;
} SpriteSequencer;

typedef struct {
    u8 unk00[0x7C];
    SpriteSequencer *sequencer;
} FieldSprite;

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
    void *geometry;                            /* 800afb14 */
    void *collision;                           /* 800afb18 */
    s32 *sprites;                              /* 800afb1c: sprite resource (offset table) */
    Attribute *collision_attributes;           /* 800afb20 */
    CollisionTriangle *collision_triangles[4]; /* 800afb24 */
    void *collision_vertices[4];               /* 800afb34 */
    s32 triangle_counts[4];                    /* 800afb44 */
    s16 layer_count;                           /* 800afb54 */
} FieldComponents;

/* The field view and camera state (800af880..800afb56), one object: code
 * addresses its members relative to one another. */
typedef struct {
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
    u32 heading_high;        /* 170 */
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
    u8 lights[0x3C];         /* 248 */
    s16 back_color[3];       /* 284 */
    u8 unk28A[2];
    FieldComponents components; /* 28C: 800afb0c */
} FieldView;

/* A 32-byte record of the game state (+16c0). */
typedef struct {
    u8 unk00[0x1A];
    u16 flags;
    u8 unk1C[4];
} GameRecord;

/* One of the game state's 31 0xa4-byte records (+26c): characters 0-10,
 * then gears. Records are copied whole, word by word. */
typedef struct {
    u32 unk00[0x4C / 4];
    u16 hp;          /* 4C */
    u16 max_hp;      /* 4E */
    u16 ep;          /* 50 */
    u16 max_ep;      /* 52 */
    u8 unk54[0x74 - 0x54];
    u8 unk74[3];
    u8 unk77;        /* 77 */
    u8 unk78;        /* 78 */
    u8 unk79[0xA0 - 0x79];
    u8 unkA0;        /* A0: gear */
    u8 unkA1[0xA4 - 0xA1];
} Character;

/* A gear record of the game state (+978, records 11-30). */
typedef struct {
    u8 unk00[0x38];
    u16 gauge;       /* 38 */
    u16 gauge_max;   /* 3A */
    u8 unk3C[0x60 - 0x3C];
    u32 points;      /* 60 */
    u32 points_max;  /* 64 */
    u8 unk68[0xA4 - 0x68];
} Gear;

/* Resident persistent game state (*8005a39c). */
typedef struct GameState {
    u8 unk0000[0x26C];
    Character characters[11]; /* 026C */
    Gear gears[20];           /* 0978 */
    u8 unk1648[0x16C0 - 0x1648];
    GameRecord records[11]; /* 16C0 */
    u8 unk1820[0x182C - 0x1820];
    u16 unk182C[4];      /* 182C */
    u16 unk1834;         /* 1834 */
    u8 unk1836[0x1844 - 0x1836];
    u16 unk1844;         /* 1844 */
    u16 unk1846;         /* 1846 */
    u8 unk1848[0x184E - 0x1848];
    u16 unk184E;         /* 184E */
    u16 unk1850;         /* 1850 */
    u16 unk1852;         /* 1852 */
    u16 unk1854;         /* 1854 */
    u16 unk1856;         /* 1856 */
    u8 unk1858[0x1924 - 0x1858];
    s32 gold;            /* 1924 */
    u8 unk1928[0x1930 - 0x1928];
    u16 vars[0x200];     /* 1930: saved event variables (800c3a68) */
    u16 unk1D30;         /* 1D30: characters waiting to join */
    u16 unk1D32;         /* 1D32: bit per character */
    u8 unk1D34[0x1D38 - 0x1D34];
    u8 count1[100];      /* 1D38: inventory list 1 */
    u8 id1[100];         /* 1D9C */
    u8 count2[200];      /* 1E00: inventory list 2 */
    u8 id2[200];         /* 1EC8 */
    u8 count0[150];      /* 1F90: inventory list 0 */
    u8 id0[150];         /* 2026 */
    u8 count3[100];      /* 20BC: inventory list 3 */
    u8 id3[100];         /* 2120 */
    u8 count4[150];      /* 2184: inventory list 4 */
    u8 id4[150];         /* 221A */
    u8 unk22B0;
    u8 unk22B1[3];       /* 22B1: per party slot */
    u8 unk22B4[2];
    u16 unk22B6;         /* 22B6 */
    u8 unk22B8[0x2318 - 0x22B8];
    u16 unk2318;         /* 2318: bit per character */
    u16 unk231A;         /* 231A: saved map */
    u16 unk231C;         /* 231C */
    u8 unk231E[2];
    s16 unk2320;         /* 2320 */
    u16 unk2322;         /* 2322 */
} GameState;

/* Field work state 800b2078..800b2388, one object: stores to its members do
 * not pass loads of other members, and code addresses members relative to
 * one another. */
typedef struct {
    s16 unk2078;               /* 2078: screen effect running (800a484c) */
    s16 unk207A;               /* 207A */
    s16 effect_steps;          /* 207C */
    u8 unk207E[2];
    s32 effect_value[6];       /* 2080: 16.16 */
    s32 effect_step[6];        /* 2098 */
    u8 unk20B0[4];
    void *effect_buffers[4];   /* 20B4 */
    FadeChannel fades[2];      /* 20C4: screen fade channels */
    u16 open_windows;          /* 2174: bit per open dialogue window; talk
                                * is inhibited while any is set */
    s16 encounter_inhibition;  /* 2176 */
    s16 terrain_angle;         /* 2178 */
    s16 input_mask;            /* 217A */
    s32 unk217C;               /* 217C */
    u8 unk2180[4];
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
    u8 unk21BC[0x21CC - 0x21BC];
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
    s16 unk21E4[(0x221C - 0x21E4) / 2]; /* 21E4 */
    s16 unk221C[5][3];         /* 221C */
    u8 unk223A[2];
    s16 unk223C[3][3];         /* 223C */
    u8 unk224E[0x225C - 0x224E];
    u8 unk225C[3];             /* 225C */
    u8 unk225F[0x2264 - 0x225F]; /* 225F: event byte table */
    s32 unk2264;               /* 2264: 801e layer enabled */
    s32 unk2268;               /* 2268 */
    s32 controlled;            /* 226C: controlled actor/descriptor index */
    u8 unk2270[0x2298 - 0x2270];
    s32 unk2298;               /* 2298 */
    s32 unk229C;               /* 229C: at most 32 */
    u8 unk22A0[0x22E0 - 0x22A0];
    s16 unk22E0;               /* 22E0 */
    s16 emitter_descriptor[3]; /* 22E2: descriptor each emitter follows, or -1 */
    s16 unk22E8[3][4];         /* 22E8 */
    s16 unk2300[3][4];         /* 2300 */
    s32 unk2318[3];            /* 2318 */
    s16 emitter_position[3][4]; /* 2324 */
    u16 effects_kept;          /* 233C: bit per effect pair still playing */
    s16 unk233E;               /* 233E */
    s16 repeat_delay;          /* 2340 */
    s16 repeat_remaining;      /* 2342 */
    s16 jump_mode;             /* 2344 */
    s16 animation_mode;        /* 2346 */
    u16 unk2348;               /* 2348: gather override */
    s16 unk234A;               /* 234A */
    s16 battle_override;       /* 234C: battle-entry flag override, 0xff none */
    s16 followers_idle;        /* 234E */
    u8 unk2350[0x2354 - 0x2350];
    u8 unk2354;                /* 2354 */
    u8 unk2355;                /* 2355 */
    u8 unk2356;                /* 2356 */
    u8 unk2357;                /* 2357 */
    u8 unk2358;                /* 2358 */
    u8 unk2359[0x2360 - 0x2359];
    s32 unk2360;               /* 2360 */
    s32 unk2364;               /* 2364 */
    s32 unk2368;               /* 2368 */
    u16 unk236C;               /* 236C */
    u8 unk236E[2];
    s32 wave_chunks;           /* 2370: music-wave chunks gathered */
    s32 unk2374;               /* 2374 */
    s32 unk2378;               /* 2378 */
    s32 unk237C;               /* 237C */
    s32 unk2380;               /* 2380 */
    s32 unk2384;               /* 2384 */
} FieldWork;

/* A loaded sound-effect bank; +14 is its id. */
typedef struct {
    u8 unk00[0x14];
    u16 id;
} FieldSoundBank;

/* Parameters events set at 800b0080, one object. */
typedef struct {
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
    s16 unk00;       /* 00 */
    u8 unk02[2];
    s16 unk04;       /* 04: set to 1 to release */
    u8 unk06[0xC0 - 6];
} Particle;

typedef struct {
    s16 unk00;       /* 00 */
    s16 unk02;       /* 02 */
    s16 unk04;       /* 04 */
    s16 count;       /* 06: particles */
    s32 unk08;       /* 08 */
    SVECTOR unk0C;   /* 0C */
    SVECTOR unk14;   /* 14 */
    SVECTOR unk1C;   /* 1C */
    s16 unk24;       /* 24 */
    s16 unk26;       /* 26 */
    s16 unk28;       /* 28 */
    u16 flags;       /* 2A */
    Particle *particles; /* 2C */
    s16 unk30[8][2]; /* 30 */
    s16 unk50;       /* 50 */
    s16 unk52;       /* 52 */
    s16 unk54;       /* 54 */
    s16 unk56;       /* 56 */
    s16 unk58;       /* 58 */
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
    s16 unk76;       /* 76 */
} Record78;

/* A pointer marker: its quad's corners and primitive per buffer. */
typedef struct {
    SVECTOR v[4];
    POLY_FT4 poly[2];
} FieldMarker;

/* One 2 KiB music-wave stream chunk. */
typedef struct {
    u32 words[0x200];
} WaveChunk;

/* One of the three positional sound-emitter slots (800afe88). */
typedef struct {
    u16 actor;  /* descriptor the sound follows */
    u16 sound;  /* 0xffff when free */
    u16 unk4;
} EmitterSlot;

/* One of the three field light/lookup slots at 800b06a4. */
typedef struct {
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
extern void func_800230A8(FieldModel *model);
extern void func_800345E0(void *text);
extern void func_80034614(void *text);
extern void func_800346D4(void *text);
extern void func_8002DFF0(s32 w, s32 h);
extern void func_80038428(void *bank);
extern void func_8003A344(s32 voice, s32 volume);
extern void func_8003A55C(s32 voice, s32 pan);
extern void memcpy(void *to, void *from, s32 size);
extern void func_800379B4(s32);
extern void func_8003A838(s32 sequence, s32, s32);
extern void func_8003A89C(s32 sequence, s32, s32);
extern void func_8003AAC4(s32 sequence, s32);
extern void func_80021BF0(FieldModel *model, void *block);
extern void func_80039EC4(s32 sound, s32 voice);
extern void func_800273C4(void *object, SVECTOR *eye, SVECTOR *target, MATRIX *world, u32 *ot, s32 buffer);
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
extern void func_80039F9C(s32 id, s32 voice, s16 volume, s16 pan);
extern void func_80039FF8(void);
extern void func_8003A20C(s32 voice);
extern void func_80048D7C(VECTOR *v, SVECTOR *out); /* VectorNormalS */
extern s32 func_8003F8B0(s32 angle); /* rcos */
extern s32 func_8003F8CC(s32 angle); /* rsin */
extern void FlushCache(void);
extern void EnterCriticalSection(void);
extern void ExitCriticalSection(void);
extern s32 func_8001B484(s32 file, s32);
extern void func_80028470(s32 directory, s32);
extern s32 func_800286CC(void);
extern void func_800320B8(void *block);
extern void func_80032498(s32 tag, s32);
extern void func_80033698(s32, s32);
extern void func_8003747C(s32);
extern void func_800374E8(s32, s32, s32, s32, s32, s32, s32, s32, s32, s32, s32);
extern s32 rand(void);
extern void func_80021B98(void *, s32 r, s32 g, s32 b);
extern void func_80028A60(s32);
extern void func_80029EB0(s32 file, void *ring, s32, s32, s32, s32, s32, s32, s32, s32);
extern void *func_8002A260(s32 sectors, s32);
extern void func_800320E8(void *);
extern void func_80032EB4(void *source, void *destination);
extern MATRIX *func_8003F738(SVECTOR *angles, MATRIX *m); /* RotMatrix */
extern void func_80049BDC(MATRIX *a, MATRIX *b);            /* MulRotMatrix */
extern void func_8004A6DC(SVECTOR *v, s32 *out, s32 *flag); /* RotTrans */

/* Field overlay. */
extern s32 func_8008A790(s32 id, s32 *slot);
extern void func_8008A7DC(s32 member, s32 slot);
extern void func_80085560(s32 file, s32 unused, void (*callback)(s32));
extern void func_800859DC(WaveChunk *chunk);
extern void func_80086024(void);
extern void func_800A47D4(void);
extern void func_800ACE24(void);
extern void func_800A4CC4(s32, s32, s32, s32, s32, s32, s32);
extern void func_80086BA8(void);
extern void func_801E7D14(MATRIX *world, s16 (*table)[3], u32 *ot, s32 buffer, s32);
extern void func_80281B00(char *name);
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
extern void func_8008B978(s32);
extern void func_80071D08(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr);
extern s32 func_8009D000(s32 offset, s32 flags); /* operand, immediate with flag 0x20 */
extern void func_80086590(VECTOR *target);
extern s32 func_8009CFBC(s32 offset, s32 flags); /* operand, immediate with flag 0x40 */
extern void func_80074700(void);
extern void func_8008004C(u32 *ot, s32 buffer);
extern void func_800805F4(void);
extern void func_800A2030(void);
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
extern void func_800A3074(u16 reference, s32 value); /* write an event variable */
extern s32 func_800ACDB8(s32 offset); /* raw halfword operand */
extern s32 func_800ACDEC(s32 offset); /* operand: bit 15 immediate, else variable */
extern void func_80086A1C(s32 emitter, s32 *position);
extern void func_80081F80(void *owner, s32 heading);
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
extern void func_8007DA44(void *ot, s32 buffer);
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
extern s32 D_80059198;
extern void *D_8005A4BC; /* field sound-effect bank copy */
extern u8 D_8005061C[6];
extern s32 D_8004F36C; /* sequence playing */
extern s32 D_80062528; /* current sequence */
extern s32 D_80062590[3];
extern s32 D_8005A444[3]; /* party members */
extern s32 D_8004F300;
extern u8 D_80050622;
extern GameState *D_8005A39C;
extern s32 D_8004F2FC; /* cached sequence */
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
extern u8 D_800ADFCC[][2]; /* per music: wave file, release shared bank */
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
extern Record78 D_800B02CC[];
extern u16 D_800AE060[][2]; /* movie sound timeline: time, sound */
/* Field movie parameters (800c3a20..800c3a3a), set by the movie events. */
extern s16 D_800C3A20; /* movie file */
extern u16 D_800C3A22;
extern u16 D_800C3A24;
extern u16 D_800C3A26;
extern u16 D_800C3A28;
extern u16 D_800C3A2A;
extern u16 D_800C3A2C; /* movie sound time origin */
extern u16 D_800C3A2E;
extern u16 D_800C3A36; /* 1: 24-bit display */
extern u16 D_800C3A3A;
extern s32 D_800ADB6C; /* movie stopped */
extern s32 D_800ADB74; /* movie mode */
extern s32 D_800ADB80;
extern s32 D_800AFE74;
extern s32 D_800B00E4;
extern s32 D_800C3A64; /* movie sound timeline position */
extern s32 D_800ADB50;
extern s32 D_800ADB78;
extern void *D_800B007C;

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
extern FieldSoundBank *D_800B235C; /* movie sound-effect bank */
extern u8 *D_800ADC00; /* event bytecode */
extern void (*D_800AE6A0[])(void); /* extended event instructions */
extern EmitterSlot D_800AFE88[3];
extern FieldActor *D_800B0078; /* current event actor */
extern s32 D_800B00C0; /* yield */
extern void *D_800B00E0; /* shared wave bank buffer */
extern void *D_800ADBB8; /* music-wave stream ring */
extern s32 D_800ADBBC;   /* stream arrivals */
extern void (*D_800AFEA4)(s32); /* stream chunk callback */
extern s16 D_800C3A38;
extern s32 D_800ADB58; /* descriptor whose list is read */
extern s32 D_800ADB5C; /* list position */

extern u16 D_800B14AC;
extern DialogueWindow D_800C2698[4];
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
extern s16 D_800C3A68[];            /* event variable bank */
extern s32 func_8004A70C(s32 a, s32 b, s32 point); /* side of edge a-b */
extern VECTOR *func_8004A414(VECTOR *v, VECTOR *squares); /* square each */

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

#endif
