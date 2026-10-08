#ifndef DEBUG595_DEBUG595_H
#define DEBUG595_DEBUG595_H

#include "common.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libetc.h"

/* One debug line: a segment in its own frame, drawn in both buffers. */
typedef struct {
    MATRIX matrix;   /* 0x00 */
    SVECTOR rot;     /* 0x20 */
    SVECTOR trans;   /* 0x28 */
    SVECTOR start;   /* 0x30 */
    SVECTOR end;     /* 0x38 */
    LINE_G2 line[2]; /* 0x40: one per draw buffer */
} DebugLine;         /* 0x68 */

/* The part of a field actor the monitor reads. */
typedef struct {
    u8 unk0[0x18];
    SVECTOR size; /* 0x18: collision half extents */
    VECTOR pos;   /* 0x20: 16.16 fixed point */
} DebugActor;

/* One CPU-time mark: scanlines spent before it. */
typedef struct {
    s32 unk0;
    s32 time;   /* 0x04 */
    char *name; /* 0x08 */
} CpuMark;

/* An emitter's drawing flags (the editor steps them as one halfword). */
typedef union {
    s16 value;
    struct {
        u16 randrot : 1;  /* RANDROT */
        u16 sort : 2;     /* SORT: top, mid, normal, back */
        u16 unk3 : 3;
        u16 rangemod : 2; /* RANGEMOD: random, line, circle */
        u16 colmode : 2;  /* COLMODE: semi-transparency rate */
        u16 unk10 : 6;
    } bits;
} EmitterFlags;

/* A field particle emitter (field overlay table, 8 entries). */
typedef struct {
    s16 unk0;
    u16 start_wait;           /* 0x02 SWAIT */
    u16 end_wait;             /* 0x04 EWAIT */
    s16 max;                  /* 0x06 MAX */
    s32 speed;                /* 0x08 SPEED */
    SVECTOR start_pos;        /* 0x0C SPOS */
    SVECTOR end_pos;          /* 0x14 EPOS */
    SVECTOR gravity;          /* 0x1C GRAVITE */
    s16 speed_scale;          /* 0x24 SPEED multiplier */
    u16 start_range;          /* 0x26 SRANGE */
    u16 end_range;            /* 0x28 ERANGE */
    EmitterFlags flags;       /* 0x2A */
    s16 unk2C[2];
    s16 angle_offsets[8][2];  /* 0x30 ANGOFFS */
    s16 unk50[2];
    s16 shape;                /* 0x54 SHAPE */
    u16 particle_start_wait;  /* 0x56 PSWAIT */
    u16 particle_end_wait;    /* 0x58 PEWAIT */
    SVECTOR scale;            /* 0x5A SCALE */
    SVECTOR scale_offset;     /* 0x62 SCALEOFS */
    u8 color[4];              /* 0x6A COLOR */
    s8 color_offset[4];       /* 0x6E COLOROFS */
    s16 unk72[2];
    s16 rot_angle;            /* 0x76 ROTANGLE */
} ParticleEmitter;            /* 0x78 */

/* Resident helpers. */
void func_800379C8(const char *format, ...); /* debug text print */
void func_80036DC8(s32 r, s32 g, s32 b);     /* debug text colour */
MATRIX *func_8003F738(SVECTOR *rot, MATRIX *m); /* RotMatrix */
void func_8004A6DC(SVECTOR *v, long *t, SVECTOR *r);
void func_80037324(u32 *ot);

/* Field state. */
extern s32 D_800C268C;
extern u32 *D_800C426C;  /* field ordering tables */
extern s32 D_800ADB08;   /* current draw buffer */
extern MATRIX D_800AFA64;
extern u16 D_800AFEA0;   /* buttons held (shoulder bits) */
extern u16 D_800C3908;   /* buttons pressed or repeating */
extern u16 D_800AFE9C;   /* buttons held */
extern s32 D_800B0044;   /* particle emitter being edited */
extern ParticleEmitter D_800B02CC[8];
extern s32 D_80065850;   /* camera control mode */
extern s32 D_80065854;   /* analog steps */
extern s32 D_80065858;
extern s32 D_800ADB94;   /* camera distance */
extern s32 D_800ADB98;
extern s32 D_800AF984;
extern s32 D_800AF988;
extern s32 D_800AF9F0;
extern s16 D_800AF9E6;
extern s16 D_800AF9FC;
extern u16 D_800AF9FE;

extern s32 D_800ADB9C;   /* scanline count at the last mark */

s32 func_80281B90(u32 *ot);
void func_802814D4(u32 *ot, DebugLine *line, MATRIX *m, s32 buffer);

/* Particle emitter editor. */
void func_80284354(s32 row, s32 cursor, s32 blink);
s32 func_8028439C(s32 row, s32 cursor, s32 *selected);
void func_802846CC(s32 axis, u32 item);

/* The monitor's view of the field (80281b90). */

/* A 16.16 fixed-point coordinate. */
typedef union {
    s32 raw;
    struct {
        u16 frac;
        s16 whole;
    } part;
} Fixed;

/* An event actor record; the members the monitor prints. */
typedef struct {
    u32 flags;         /* 00: MFflag; bits 8..10 the actor type */
    u32 flags2;        /* 04: MFlag2; bit 26 talk off */
    s16 triangle[4];   /* 08: current collision triangle per layer */
    s16 layer;         /* 10 */
    u8 unk12[2];
    u32 id;            /* 14: bits 5..7 P/G/C clear */
    u8 unk18[8];
    Fixed pos[3];      /* 20 */
    u8 unk2C[0x48];
    u8 count;          /* 74 */
    u8 unk75[0x17];
    struct {
        u16 pc;
        u8 unk2[6];
    } threads[8];      /* 8c: script threads */
    u16 pc;            /* cc */
    u8 thread;         /* ce: running thread */
} MonitorActor;

typedef struct {
    u8 unk0[0x14];
    s32 loaded;        /* 14 */
} MonitorInstance;

/* One 0x5c-byte field model descriptor per event actor. */
typedef struct {
    MonitorInstance *instance; /* 00 */
    u8 unk4[0x48];
    MonitorActor *actor;       /* 4c */
    u8 unk50[8];
    u16 flags;                 /* 58: 0x2000 mime */
    u8 unk5A[2];
} MonitorDescriptor;

/* A 14-byte collision triangle. */
typedef struct {
    s16 unk0[6];
    u8 attribute;      /* 0c */
    u8 unkD;
} MonitorTriangle;

/* One 0xa4-byte character slot of the game state. */
typedef struct {
    u8 unk0[0x4C];
    u16 hp;            /* 4c */
    u8 unk4E[2];
    u16 mp;            /* 50 */
    u8 unk52[0x4E];
    u8 gear;           /* a0 */
    u8 unkA1[3];
} MonitorSlot;

/* Resident persistent game state (*8005a39c); the members printed. */
typedef struct {
    u8 unk0[0x26C];
    MonitorSlot slots[11];   /* 026c */
    u8 unk978[0x1924 - 0x978];
    s32 gold;                /* 1924 */
    u8 unk1928[0x1D30 - 0x1928];
    u16 members;             /* 1d30 */
    u16 unmasked;            /* 1d32 */
    u8 unk1D34[0x1E00 - 0x1D34];
    u8 acc_count[0xC8];      /* 1e00 */
    u8 acc_id[0xC8];         /* 1ec8 */
    u8 item_count[0x96];     /* 1f90 */
    u8 item_id[0x96];        /* 2026 */
    u8 unk20BC[0x22B1 - 0x20BC];
    u8 ride[3];              /* 22b1 */
    u8 unk22B4[0x2318 - 0x22B4];
    u16 locked;              /* 2318 */
} MonitorState;

extern MonitorState *D_8005A39C;
extern s32 D_80062590[3];      /* party slots (0xff empty) */
extern s32 D_8005A444[3];      /* party members' actors */
/* The field's object tables: descriptor count and list, and the walkmesh
 * triangles of each layer. */
typedef struct {
    s32 count;                       /* 800afb0c */
    MonitorDescriptor *descriptors;  /* 800afb10 */
    u8 unk8[0x10];
    MonitorTriangle *triangles[4];   /* 800afb24 */
} FieldObjects;

extern FieldObjects D_800AFB0C;
extern s32 D_800B226C;         /* player actor */
extern s32 D_800ADBFC;         /* event actors */
extern s32 D_800ADB40;
extern s16 D_800ADB02;
extern s32 D_800ADAFC;
extern s32 D_800ADBA0;         /* CPU time */
extern s32 D_800ADBA4;         /* GPU time */
extern s32 D_80059578;         /* polygons */
extern s32 D_800595C0;         /* polygon limit */
extern void *D_80059558;       /* playing sequences */
extern void *D_80059440;
extern s32 D_8004F338;         /* music */
extern s32 D_8004F33C;         /* wave bank */
extern s32 D_8004F34C;         /* map number */
extern u16 D_800C3900;         /* buttons pressed */
extern u16 D_800C3A68;         /* scenario flag */
extern Fixed D_800AF880[3];    /* camera eye */
extern Fixed D_800AF890[3];    /* camera look-at */
extern Fixed D_800AF8B0[3];    /* second camera eye */
extern Fixed D_800AF8C0[3];    /* second camera look-at */
extern u8 D_800AF9F4;          /* dolly set */
extern u8 D_800AF9F5;          /* dolly stop */
extern s32 D_800AF9F8;         /* screen distance */
extern u8 D_800B2190[3];       /* fog near colour */
extern u8 D_800B2194[3];       /* fog far colour */
extern s16 D_800B2198[2];      /* fog near, far */
extern s16 D_800B218E;
extern s32 D_800B2298;         /* encounter timer */
extern s32 D_800B229C;         /* encounter number */
extern u8 D_80065ADC[16];
s32 func_80032340(void);       /* free heap size */
void func_8003278C(s32 mode, s32 top, s32 step, s32 flags); /* heap monitor */
void func_80071D08(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr);
void func_80073E38(void);
void func_8008E718(void);
s32 func_8009744C(void);       /* character direction */
s32 func_8009A514(void);       /* camera direction */
s32 func_800A3018(u32 reference); /* read an event variable */
void func_800A3F4C(void);
void func_800A98E8(s32 actor, s32 value);
void func_800A99A8(s32 actor);
void func_802835E0(void);

#endif
