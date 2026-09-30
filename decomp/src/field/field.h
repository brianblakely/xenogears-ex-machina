#ifndef FIELD_FIELD_H
#define FIELD_FIELD_H

#include "common.h"

/* GTE vector/matrix layouts (PsyQ libgte). */
typedef struct {
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

/* libgpu environments. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd, dfe, isbg, r0, g0, b0;
    u32 dr_env[16];
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

/* One of the two draw-buffer blocks (800b249c, 800ba590). */
typedef struct {
    DRAWENV draw;
    DRAWENV draw2;
    DISPENV disp;
    u32 ot[0x1001];      /* 0CC */
    u32 ot2[0x1001];     /* 40D0: cleared only while 800adb4c is set */
    u32 overlay_ot[8];   /* 80D4 */
} FieldDrawBlock;

typedef struct {
    u32 mode;
    RECT *crect;
    u32 *caddr;
    RECT *prect;
    u32 *paddr;
} TIM_IMAGE;

/* One screen fade channel (800b20c4 + 0x58 * channel). Levels are 8.8. */
typedef struct {
    u8 modes[2][12]; /* DR_MODE per draw buffer */
    u8 tiles[2][16]; /* TILE per draw buffer */
    s32 level[3];
    s32 step[3];
    u16 abr;
    s16 active;
    s16 steps;
    u16 pad;
} FadeChannel;

/* The field view and camera state (800af880..800afacc), one object: code
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
    u16 steps;               /* 19C */
    s32 start;               /* 1A0 */
    s32 step;                /* 1A4 */
    s16 shake;               /* 1A8 */
    s16 shake_time;          /* 1AA */
    s16 shake_stop;          /* 1AC */
    s32 shake_amplitude[3];  /* 1B0 */
    s32 shake_step[3];       /* 1BC */
    s32 unk1C8[3];           /* 1C8 */
    SVECTOR world_angles;    /* 1D4 */
    SVECTOR anchor;          /* 1DC */
    MATRIX scaled_world;     /* 1E4 */
    MATRIX unk204;           /* 204 */
    MATRIX world_matrix;     /* 224 */
    s32 scale;               /* 244 */
    s32 unk248;              /* 248 */
} FieldView;


/* One 0x138-byte event actor record. */
typedef struct FieldActor {
    u32 flags;       /* 000 */
    u32 layer_flags; /* 004: bits 3+ switch collision layers off */
    s16 triangle[4]; /* 008: current collision triangle per layer */
    s16 layer;       /* 010 */
    u8 unk012[2];
    u32 unk014;      /* 014 */
    u8 unk018[4];
    s32 gravity;     /* 01C */
    s32 position[3]; /* 020: 16.16 */
    u8 unk02C[4];
    s32 unk030;      /* 030 */
    u8 unk034[4];
    s32 unk038;      /* 038 */
    u8 unk03C[0x75 - 0x3C];
    u8 unk075;       /* 075 */
    u8 unk076[0x80 - 0x76];
    s8 character;    /* 080 */
    u8 unk081[0xCC - 0x81];
    u16 pc;          /* 0CC: event working PC */
    u8 slot;         /* 0CE */
    u8 unk0CF[0xEA - 0xCF];
    s16 unk0EA;      /* 0EA */
    u8 unk0EC[0xF4 - 0xEC];
    s16 scale[3];    /* 0F4 */
    u8 unk0FA[0x104 - 0xFA];
    s16 heading;     /* 104 */
    s16 heading_goal; /* 106: bit 15 once turned */
    u8 unk108[2];
    u16 sound;       /* 10A */
    u8 sound_volume; /* 10C */
    u8 sound_mode;   /* 10D: 0xff off */
    u8 unk10E[0x114 - 0x10E];
    void *unk114;    /* 114 */
    s32 *list;       /* 118 */
    u8 unk11C[2];
    s16 unk11E;      /* 11E */
    void *unk120;    /* 120 */
    s16 unk124;      /* 124: -1 when +120 is free */
    u8 unk126[2];
    s16 unk128;      /* 128 */
    u8 unk12A[2];
    u32 unk12C;      /* 12C */
    u8 unk130[0x138 - 0x130];
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
    u8 unk000[0xAC];
    RECT rect;       /* 0AC */
    u8 unk0B4[0x40E - 0xB4];
    s16 busy;        /* 40E */
    u16 age;         /* 410: 0xffff when free */
    u8 unk412[0x498 - 0x412];
} DialogueWindow;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} POLY_FT4;

/* One 0x5C-byte descriptor; one per event actor. */
typedef struct {
    u8 unk00[0x2C];
    s16 unk2C;
    u8 unk2E[0x82 - 0x2E];
    s16 unk82;
} FieldModel;

typedef struct FieldDescriptor {
    u8 unk00[4];
    FieldModel *model; /* 04 */
    u8 unk08[4];
    MATRIX matrix;     /* 0C */
    u8 unk2C[0x4C - 0x2C];
    FieldActor *actor; /* 4C */
    SVECTOR rotation;  /* 50 */
    u16 flags;         /* 58 */
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

/* The loaded field components (80070cc8), one object: stores to structure
 * members do not pass loads of these pointers. */
typedef struct {
    s32 descriptor_count;                      /* 800afb0c */
    FieldDescriptor *descriptors;              /* 800afb10 */
    void *geometry;                            /* 800afb14 */
    void *unk08;                               /* 800afb18 */
    void *unk0C;                               /* 800afb1c */
    u32 *collision_attributes;                 /* 800afb20 */
    CollisionTriangle *collision_triangles[4]; /* 800afb24 */
    void *collision_vertices[4];               /* 800afb34 */
} FieldComponents;

/* A 32-byte record of the game state (+16c0). */
typedef struct {
    u8 unk00[0x1A];
    u16 flags;
    u8 unk1C[4];
} GameRecord;

/* A 0xa4-byte party slot of the game state (+308). */
typedef struct {
    u8 unk00[4];
    u8 unk04;
    u8 unk05[0xA4 - 5];
} GameSlot;

/* A 0xa4-byte character of the game state (+9b0). */
typedef struct {
    u16 gauge;       /* 00 */
    u16 gauge_max;   /* 02 */
    u8 unk04[0x28 - 4];
    u32 points;      /* 28 */
    u32 points_max;  /* 2C */
    u8 unk30[0xA4 - 0x30];
} GameCharacter;

/* Resident persistent game state (*8005a39c). */
typedef struct GameState {
    u8 unk0000[0x308];
    GameSlot party[10];          /* 0308 */
    u8 unk0970[0x9B0 - 0x970];
    GameCharacter characters[20]; /* 09B0 */
    u8 unk1680[0x16C0 - 0x1680];
    GameRecord records[11]; /* 16C0 */
    u8 unk1820[0x1834 - 0x1820];
    u16 unk1834;         /* 1834 */
    u8 unk1836[0x1844 - 0x1836];
    u16 unk1844;         /* 1844 */
    u16 unk1846;         /* 1846 */
    u8 unk1848[0x184E - 0x1848];
    u16 unk184E;         /* 184E */
    u8 unk1850[2];
    u16 unk1852;         /* 1852 */
    u8 unk1854[0x1D32 - 0x1854];
    u16 unk1D32;         /* 1D32: bit per character */
    u8 unk1D34[0x22B6 - 0x1D34];
    u16 unk22B6;         /* 22B6 */
} GameState;

/* Field work state 800b218c..800b2388, one object: stores to its members do
 * not pass loads of other members. */
typedef struct {
    s16 scale;                 /* 218C: offset scale (8007b614) */
    s16 unk218E;               /* 218E: colour pass-through gate (80075b08) */

    u8 unk2190[0x219F - 0x2190];
    u8 party_bits;             /* 219F */
    s16 unk21A0[6];            /* 21A0: set by event 8008cfec */
    s16 emitter_range;         /* 21AC */

    u8 unk21AE[0x21B4 - 0x21AE];
    s16 unk21B4;               /* 21B4 */
    u8 unk21B6[2];
    s32 last_sound_effect;     /* 21B8 */
    u8 unk21BC[0x21CD - 0x21BC];
    u8 unk21CD;                /* 21CD */
    u8 unk21CE[0x21D2 - 0x21CE];
    s8 unk21D2;                /* 21D2 */
    u8 unk21D3;
    s16 unk21D4;               /* 21D4 */
    u8 unk21D6[0x21E4 - 0x21D6];
    s16 unk21E4[(0x225C - 0x21E4) / 2]; /* 21E4 */
    u8 unk225C[3];             /* 225C */
    u8 unk225F[0x2268 - 0x225F]; /* 225F: event byte table */
    s32 unk2268;               /* 2268 */
    s32 controlled;            /* 226C: controlled descriptor */
    u8 unk2270[0x2298 - 0x2270];
    s32 unk2298;               /* 2298 */
    s32 unk229C;               /* 229C: at most 32 */
    u8 unk22A0[0x22E0 - 0x22A0];
    s16 unk22E0;               /* 22E0 */
    s16 emitter_descriptor[3]; /* 22E2: descriptor each emitter follows, or -1 */
    u8 unk22E8[0x233C - 0x22E8];
    u16 effects_kept;          /* 233C: bit per effect pair still playing */
    s16 unk233E;               /* 233E */
    u8 unk2340[0x2344 - 0x2340];
    s16 unk2344;               /* 2344 */
    u8 unk2346[2];
    u16 unk2348;               /* 2348 */
    u8 unk234A[2];
    s16 battle_override;       /* 234C: battle-entry flag override, 0xff none */
    u8 unk234E[0x2354 - 0x234E];
    u8 unk2354;                /* 2354 */
    u8 unk2355;                /* 2355 */
    u8 unk2356;                /* 2356 */
    u8 unk2357;                /* 2357 */
    u8 unk2358;                /* 2358 */
    u8 unk2359[0x236C - 0x2359];
    s16 unk236C;               /* 236C */
    u8 unk236E[0x2384 - 0x236E];
    s32 unk2384;               /* 2384 */
} FieldWork;

/* One of the three positional sound-emitter slots (800afe88). */
typedef struct {
    u16 id;
    u16 owner;
    u16 unk4;
} EmitterSlot;

/* One of the three field light/lookup slots at 800b06a4. */
typedef struct {
    s16 a;
    s16 b;
    s16 c;
} FieldSlot6;

/* Resident services. */
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
extern void func_80040454(void);
extern void func_800404D4(void);
extern void func_800404E4(void);
extern void func_80044C44(DRAWENV *env);  /* PutDrawEnv */
extern void func_80044E9C(DISPENV *env);  /* PutDispEnv */
extern void func_8004495C(RECT *rect, s32 x, s32 y); /* MoveImage */
extern s32 func_8004B32C(s32 y, s32 x); /* ratan2 */
extern s32 func_8001B484(s32 file, s32);
extern void func_80028470(s32 directory, s32);
extern s32 func_800286CC(void);
extern void func_800320B8(void *block);
extern void func_80032498(s32 tag, s32);
extern void func_80033698(s32, s32);
extern void func_8003747C(s32);
extern void func_800374E8(s32, s32, s32, s32, s32, s32, s32, s32, s32, s32, s32);
extern void func_8003FA38(void);
extern s32 func_8004B54C(s32); /* VSync */
extern void func_80021B98(void *, s32 r, s32 g, s32 b);
extern void func_80043B84(void *ot, void *first, void *last); /* AddPrims */
extern void func_80044894(RECT *rect, u32 *pixels);           /* LoadImage */
extern void func_80044AD8(u32 *ot, s32 count);                 /* ClearOTagR */
extern void func_800471B4(u32 *tim);                           /* OpenTIM */
extern TIM_IMAGE *func_800471C4(TIM_IMAGE *image);             /* ReadTIM */
extern void func_80028A60(s32);
extern void func_80029EB0(s32 file, void *ring, s32, s32, s32, s32, s32, s32, s32, s32);
extern void *func_8002A260(s32 sectors, s32);
extern void func_800320E8(void *);
extern void func_80032EB4(s32 index, void *destination);
extern void func_8003F738(SVECTOR *angles, MATRIX *m); /* RotMatrix */
extern void func_800445D0(s32);
extern void func_8004931C(MATRIX *a, MATRIX *b, MATRIX *out); /* CompMatrix */
extern void func_80049BDC(MATRIX *a, MATRIX *b);            /* MulRotMatrix */
extern void func_80049DCC(MATRIX *m, VECTOR *scale);        /* ScaleMatrix */
extern void func_80049EFC(MATRIX *m);                       /* SetRotMatrix */
extern void func_80049F8C(MATRIX *m);                       /* SetTransMatrix */
extern void func_8004947C(MATRIX *m, VECTOR *in, VECTOR *out); /* ApplyMatrixLV */
extern void func_8004960C(void);                               /* PushMatrix */
extern void func_800496AC(void);                               /* PopMatrix */
extern void func_8004A6DC(SVECTOR *v, s32 *out, s32 *flag); /* RotTrans */

/* Field overlay. */
extern void func_80071F64(s32 x, s32 y, s32 w, s32 h);
extern void func_8008E0DC(s32 flags);
extern void func_8008E148(s32 flags);
extern void func_8008E498(s32 value);
extern void func_8008E718(void);
extern s32 func_8009CDB4(s32 offset); /* actor selector; 0xff when none */
extern s32 func_800A3018(u32 reference); /* read an event variable */
extern void func_80072254(s32 index);
extern void func_800863E8(s32 id);
extern s32 func_8008CF3C(s32 id);
extern void func_8008A93C(void);
extern void func_800A484C(s32);
extern void func_801E8330(s32, s32, s32);
extern s32 func_8008A558(void);
extern void func_800A98E8(s32 actor, s32 value);
extern void func_800A99A8(s32 actor);
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
extern void func_8007F6F8(s16 window);
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
extern FieldWork D_800B218C;
extern void *D_800B235C; /* movie sound-effect bank */
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
extern void *D_800B0054; /* pointer pad buffers */
extern void *D_800B0058;
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
extern FieldComponents D_800AFB0C;
extern FieldDrawBlock *D_800C426C; /* current draw block */

extern s32 D_800C268C;
extern s32 D_800ADC18;
extern u8 D_800ADC1C[8]; /* octant bits */
extern FadeChannel D_800B20C4[2];
extern FieldDrawBlock D_800B249C[2];
extern s32 D_800ADB0C;
extern s32 D_800ADB60; /* field stream running */
extern void *D_800ADC14; /* field stream ring */
extern FieldSlot6 D_800B06A4[3];

#endif
