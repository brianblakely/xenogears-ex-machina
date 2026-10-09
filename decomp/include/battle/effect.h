#ifndef BATTLE_EFFECT_H
#define BATTLE_EFFECT_H

/* The records of the battle's model and effect library (8009E53C's unit):
 * keyframes and their tracks, tweens, effect sprites and their pools,
 * colour fades, image animations and surfaces (battle/model.h has the
 * hierarchies and effect pools). Most of the same code is linked into
 * ovl2143, the actor module at 0x801DC000, which uses these records too
 * (its overview pairs the functions). This header declares no variables, so
 * units outside the battle can include it. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"

/* An animation frame header (0x18 bytes; read as halfwords by 800A1B50). */
typedef struct {
    u8 pad0[2];
    u16 duration;         /* 0x02 */
    u16 flags;            /* 0x04: bit 0 no rotations, bit 1 no translations */
    u16 packed;           /* 0x06: 0: the values follow a skipped base block */
    u8 pad8[4];
    u16 rotationCount;    /* 0x0C */
    u16 translationCount; /* 0x0E */
    u8 pad10[8];
} AnimationFrame; /* followed by one TrackEntry per part, or packed values */

/* A part's tracks of a frame: byte offsets of its rotation and translation
 * tracks (0xFFFF none) and their entry types. */
typedef struct {
    u16 offsets[2];
    u8 types[2];
} TrackEntry;

/* An effect entry seen as a tween of a part (the EffectEntry layout): start
 * values and deltas or targets, or a keyframe track cursor. */
typedef struct {
    u8 used;
    u8 field1;     /* +1: smooth / looping */
    u8 kind;       /* +2: 3 rotation, 7 + n movement, 0-2 tracks */
    u8 tag;        /* +3: 0xFF persistent */
    union {
        s16 values[6]; /* +4 */
        struct {
            u8 *start;  /* +4: track data */
            u8 *cursor; /* +8 */
        } track;
    } u;
    s16 time;     /* +0x10 */
    s16 duration; /* +0x12 */
} Tween;

/* An effect sprite, a record of a sprite pool (0x7C bytes): a
 * quadrilateral of four vertices, a colour fading each tick, and its
 * primitive for both frame buffers. */
typedef struct {
    s16 x0, y0, z0, pad06;
    s16 x1, y1, z1;
    s16 projected; /* 0x0E: the vertices are 3D, projected with the GTE */
    s16 x2, y2, z2;
    s16 age;       /* 0x16: -1 free */
    s16 x3, y3, z3;
    s16 lifetime;  /* 0x1E */
    u16 color[3];  /* 0x20: 10.6 fixed point */
    s16 fade[3];   /* 0x26: per tick */
    POLY_FT4 packets[2]; /* 0x2C: one per frame buffer */
} EffectSprite;

/* A pool of effect sprites; next is the first record that may be free. */
typedef struct SpritePool {
    EffectSprite *records;
    s16 count;
    s16 next;
} SpritePool;

/* An object's trail channel (0x70 bytes): a sprite following two points of a
 * model part, with fading colours. Screen-space trails retain eight projected
 * positions per endpoint; world-space trails retain two full vectors. */
typedef struct ColorFade {
    s16 id;           /* the model part it follows; negative when idle */
    u8 solid;         /* 0x02: 0 screen-space history, otherwise world-space */
    u8 semiTrans;     /* 0x03: the sprites' semi-transparency */
    SpritePool *pool; /* 0x04 */
    EffectSprite *sprite; /* 0x08: the currently extended quad */
    SVECTOR ends[2];  /* 0x0C: the two points, in the part's space */
    union {
        struct {
            DVECTOR first[8];
            DVECTOR second[8];
        } screen;
        struct {
            VECTOR first[2];
            VECTOR second[2];
        } world;
    } history; /* 0x1C-0x5B */
    s16 time;     /* 0x5C: history cursor, decremented modulo 8 */
    s16 count;    /* 0x5E: age of the current quad, -1 before the first tick */
    s16 max;      /* 0x60: quad extension interval, at most 7 */
    s16 duration; /* 0x62 */
    s16 color[3]; /* 0x64: 10.6 fixed point */
    s16 step[3];  /* 0x6A */
} ColorFade;

typedef char ColorFadeLayoutCheck[sizeof(ColorFade) == 0x70 ? 1 : -1];

/* A frame curve mapping time to a frame (800A3490-800A35C8, called without
 * a prototype: time, divisor, base); negative ends the animation. */
typedef s32 (*FrameCurve)();

/* A row of three colours. */
typedef struct {
    u16 c[3];
} ColorRow;

/* An image animation (0x30 bytes): a VRAM rectangle whose pixels are rebuilt
 * each time the curve selects another frame. */
typedef struct ImageAnim {
    struct ImageAnim *target; /* 0x00: image the frames are copied into */
    u16 *pixels;              /* 0x04 */
    u16 *pixels2;             /* 0x08 */
    u16 *work;                /* 0x0C */
    u8 mode;                  /* 0x10: 0/1 resident decoders, 4/5 fades */
    u8 dirty;                 /* 0x11 */
    s16 size;                 /* 0x12: pixel count */
    u16 time;                 /* 0x14 */
    u16 speed;                /* 0x16 */
    u16 frame;                /* 0x18 */
    u16 active;               /* 0x1A */
    ColorRow *colors;         /* 0x1C */
    s16 divisor;              /* 0x20 */
    s16 base;                 /* 0x22 */
    FrameCurve curve;         /* 0x24 */
    RECT rect;                /* 0x28 */
} ImageAnim;

/* Animations: an object's animation header and the events it runs on its
 * frames (800AE2A4; ovl2143 runs the same records in 801E5D44). */

/* An animation header (fields as far as recovered; 800AE1BC, 801E5C74). */
typedef struct {
    u8 pad0[2];
    u16 loop; /* 0x02 */
    u8 pad4[0x12 - 0x4];
    u16 length;     /* 0x12: the event count */
    u32 dataOffset; /* 0x14: offset of the events (AnimEvent) */
} Animation;

/* The start of an animation event: the frame it runs on and its type. */
typedef struct {
    s16 time;
    u8 type;
    u8 index; /* 0x03 */
} EventHeader;

/* Animation event 1: create a sprite (0x14 bytes). */
typedef struct {
    s16 time;
    u8 type;
    u8 kind;       /* 0x03: with flag 0x80 plus the gear's variant less one */
    u8 flags;      /* 0x04: 0x80 at the acting object; event codes that skip it (800B12D0) */
    u8 part;       /* 0x05 */
    s16 offset[3]; /* 0x06 */
    u8 mode;       /* 0x0C: 1 on the ground, 2 at the scene's centre; 0x80 at the acting object */
    u8 absolute;   /* 0x0D: the angle is not relative to the object's */
    s16 angle;     /* 0x0E */
    s16 scale;     /* 0x10 */
    u8 resource;   /* 0x12: 0 D_8006BE10, else D_8005A474 */
    u8 follow;     /* 0x13 */
} SpriteCommand;

/* Animation event 2: a light following a part of the object (0x12 bytes, 6
 * when off). */
typedef struct {
    s16 time;
    u8 type;
    u8 light;       /* 0x03 */
    u8 on;          /* 0x04 */
    u8 free;        /* 0x05: not following the object */
    u8 part;        /* 0x06 */
    u8 r, g, b;     /* 0x07 */
    s16 offset[3];  /* 0x0A */
    s16 active;     /* 0x10 */
} LightEvent;

/* Animation events 3 and 4: an effect channel (0x1C bytes, 6 when off). */
typedef struct {
    s16 time;
    u8 type;
    u8 channel;     /* 0x03 */
    u8 on;          /* 0x04 */
    u8 field5;
    u8 bytes[8];    /* 0x06 */
    s16 values[6];  /* 0x0E */
    u8 last;        /* 0x1A */
} ChannelEvent;

/* Animation event 5: play a sound (8 bytes). */
typedef struct {
    s16 time;
    u8 type;
    u8 sound;   /* 0x03 */
    u8 flags;   /* 0x04: event codes that skip it (800B12D0) */
    u8 source;  /* 0x05: of the bank (800AE220) */
    u8 sound2;  /* 0x06: a second sound, 0 none */
    u8 kind;    /* 0x07 */
} SoundEvent;

/* Animation event 7: show or hide a part (6 bytes; 800AE2A4, 801E5D44). */
typedef struct {
    s16 time;
    u8 type;
    u8 index;
    u8 part;    /* 0x04 */
    u8 visible; /* 0x05: bit 0 */
} ShowEvent;

/* Animation event 8: start effect scripts on the selected slots (0xA bytes). */
typedef struct {
    s16 time;
    u8 type;
    u8 kinds;      /* 0x03: event codes that skip a slot (800B12D0) */
    u8 onTarget;   /* 0x04: run on this object's target */
    u8 scripts[5]; /* 0x05: by slot code: 0-1 (in a gear the next), 5, 4, 2-3 */
} SlotEvent;

/* Animation event 9: an image animation (0x1C bytes, 6 when off). */
typedef struct {
    s16 time;
    u8 type;
    u8 anim;       /* 0x03 */
    u8 on;         /* 0x04 */
    u8 target;     /* 0x05: the image its frames are copied into, 0xFF none */
    u8 mode;       /* 0x06: low 7 bits the mode, 0x80 at the object's images */
    u8 curve;      /* 0x07: the frame curve (800AA820) */
    s16 x;         /* 0x08 */
    s16 y;         /* 0x0A */
    s16 x2;        /* 0x0C */
    s16 y2;        /* 0x0E */
    s16 field10;   /* 0x10 */
    u8 field12;    /* 0x12: high nibble 1 moves x2, y2 with the images too */
    u8 field13;    /* 0x13 */
    u8 field14;    /* 0x14 */
    u8 pad15;
    s16 field16;   /* 0x16 */
    s16 field18;   /* 0x18 */
    s16 field1A;   /* 0x1A */
} ImageEvent;

/* An animation event (the object's event list). */
typedef union {
    EventHeader header;
    SpriteCommand sprite;
    LightEvent light;
    ChannelEvent channel;
    SoundEvent sound;
    ShowEvent show;
    SlotEvent slots;
    ImageEvent image;
} AnimEvent;

/* A collision sphere of a surface (0x10 bytes). */
typedef struct {
    s16 h0, h2, h4, h6, h8, hA, hC, hE;
} SurfaceEntry;

/* A point of a surface strand (0x18 bytes): the length of its segment to the
 * next point (0 ends the strand), a sag added to that segment, its position
 * and the normal accumulated from its triangles. */
typedef struct {
    s16 length;
    s16 sag;
    s16 pos[3];
    u16 normalCount; /* 0x0A */
    s32 normal[3];   /* 0x0C */
} SurfacePoint;

/* Two triangles' textured primitives (one per frame buffer) and their vertex
 * indices (0x58 bytes). */
typedef struct {
    s16 index[3];
    u8 pad6[2];
    POLY_GT3 prim[2];
} SurfacePoly;

/* A battle object's surface (0x24 bytes; hair or cloth): rings of point
 * strands. */
typedef struct Surface {
    u16 h0;
    u8 pad2[2];
    s16 rings;              /* 0x04 */
    s16 polys;              /* 0x06: twice the rings' first point counts */
    s16 points;             /* 0x08 */
    s16 entryCount;         /* 0x0A */
    u8 b[6];                /* 0x0C */
    u8 pad12[2];
    SVECTOR *centres;       /* 0x14: a centre per ring */
    SurfaceEntry *entries;  /* 0x18 */
    SurfacePoint **strands; /* 0x1C: each ring's first point */
    SurfacePoly *polyList;  /* 0x20 */
} Surface;

#endif
