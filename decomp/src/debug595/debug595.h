#ifndef DEBUG595_DEBUG595_H
#define DEBUG595_DEBUG595_H

/* The field debug monitor: its debug lines, CPU-time marks and the field
 * overlay's objects it reads and edits. What it shares with the field is in
 * field/monitor.h; its views of the field's own objects are below. */

#include "common.h"

#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libetc.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "field/monitor.h"

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

s32 func_80281B90(u_long *ot);
void func_802814D4(u_long *ot, DebugLine *line, MATRIX *m, s32 buffer);

/* Particle emitter editor. */
void func_802835E0(void);
void func_80284354(s32 row, s32 cursor, s32 blink);
s32 func_8028439C(s32 row, s32 cursor, s32 *selected);
void func_802846CC(s32 axis, u32 item);

/* The monitor's views of the field's own objects, whose types stay in the
 * field's headers (src/field). The view at 800af880 (FieldView) and the
 * work block at 800b2078 (FieldWork) it addresses member by member through
 * symbols of its own: as FieldView members, func_80281B90 and func_80284EA4
 * compile differently (a member's address is kept in a register), and it
 * reads 800af9fc and 800af9fe as s16 and u16 where the field has u16 and
 * s16; the work block's members build the same either way. It reads the
 * current draw block as its ordering table words (FieldDrawBlock +0xcc),
 * names the emitter templates (the field's Record78) from its editor's
 * labels, reading their flags as bit-fields and their colour offsets as one
 * s8 array (the field reads +0x70 as u8), and takes the actors and their
 * descriptors as below (80281b90). */
extern u_long *D_800C426C;     /* the current draw block */
extern ParticleEmitter D_800B02CC[8]; /* the eight template emitters */

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

/* The field's object tables: descriptor count and list, and the walkmesh
 * triangles of each layer. */
typedef struct {
    s32 count;                       /* 800afb0c */
    MonitorDescriptor *descriptors;  /* 800afb10 */
    u8 unk8[0x10];
    MonitorTriangle *triangles[4];   /* 800afb24 */
} FieldObjects;

/* FieldView members (800af880). */
extern Fixed D_800AF880[3];    /* camera eye */
extern Fixed D_800AF890[3];    /* camera look-at */
extern Fixed D_800AF8B0[3];    /* second camera eye */
extern Fixed D_800AF8C0[3];    /* second camera look-at */
extern s32 D_800AF984;
extern s32 D_800AF988;
extern s16 D_800AF9E6;
extern s32 D_800AF9F0;
extern u8 D_800AF9F4;          /* dolly set */
extern u8 D_800AF9F5;          /* dolly stop */
extern s32 D_800AF9F8;         /* screen distance */
extern s16 D_800AF9FC;
extern u16 D_800AF9FE;
extern MATRIX D_800AFA64;
extern FieldObjects D_800AFB0C;
/* FieldWork members (800b2078). */
extern s16 D_800B218E;
extern u8 D_800B2190[3];       /* fog near colour */
extern u8 D_800B2194[3];       /* fog far colour */
extern s16 D_800B2198[2];      /* fog near, far */
extern s32 D_800B226C;         /* player actor */
extern s32 D_800B2298;         /* encounter timer */
extern s32 D_800B229C;         /* encounter number */

#endif
