#ifndef RESIDENT_SPRITE_H
#define RESIDENT_SPRITE_H

#include "gpu.h"

/* A task node of the sprite engine's lists. */
typedef struct Task {
    struct Task *owner;
    void *data;                          /* +0x4: the task's sprite */
    void (*update)(struct Task *task);  /* +0x8 */
    void (*destroy)(struct Task *task); /* +0xc */
    union {
        u32 word;
        struct {
            unsigned serial : 29;        /* creation number */
            unsigned flags : 3;
        } bits;
    } id;                                /* +0x10 */
    union {
        u32 word;
        struct {
            unsigned owner_serial : 29;  /* the owner's creation number */
            unsigned flag29 : 1;
            unsigned flag30 : 1;
            unsigned active : 1;
        } bits;
    } link;                              /* +0x14: flag tests read the word */
    struct Task *next;                   /* +0x18 */
} Task;

/* Task lists: the main list runs first each frame, then the second list. */
extern Task *D_8005958C;   /* main task list */
extern Task *D_80059594;   /* second task list */
extern Task *D_80059590;   /* the next task of the running pass */
extern Task *D_800594C0;   /* the running task */
extern u32 D_80059184;     /* next task serial */
extern s32 D_80059188;
extern s32 D_8005918C;     /* live tasks */
extern struct Sprite *D_80059190; /* sprites awaiting a frame (through renderer->next_pending) */

/* Resident sprite/actor engine (the unit around 0x8001c8dc-0x8002709c).
 * Only the fields the recovered functions use are named. */
/* One drawn part of a sprite (0x18 bytes; the renderer's part list). */
typedef struct {
    s16 x, y;              /* +0x0 */
    u8 u, v;               /* +0x4 */
    u8 w, h;               /* +0x6 */
    u8 byte8, byte9;       /* +0x8 */
    u16 tpage;             /* +0xa: bits 5-6 blend mode */
    u16 clut;              /* +0xc */
    u8 unknowne[2];
    u32 colour;            /* +0x10: rgb and primitive code */
    u32 flags;             /* +0x14 */
} SpritePart;

/* One of the eight 8-byte entries of a renderer's 0x40-byte block. */
typedef struct {
    u8 byte0;
    u8 byte1;
    s16 half2;
    s16 half4;
    s16 half6;
} SpriteRendererEntry;

/* A sprite's renderer (its part list header). */
typedef struct {
    s16 angle_x, angle_y, angle_z; /* +0x0 */
    s16 scale_x, scale_y, scale_z; /* +0x6 */
    MATRIX matrix;                 /* +0xc: local screen matrix */
    void *parts;                   /* +0x2c: 0x18 bytes per part */
    SpritePart *part_cursor;       /* +0x30 */
    SpriteRendererEntry *pointer34; /* +0x34: 8 entries */
    struct Sprite *next_pending;   /* +0x38 */
    s8 offset_x;                   /* +0x3c: screen offset, before scaling */
    s8 offset_y;                   /* +0x3d */
    u8 unknown3e[2];
    s32 word40;                    /* +0x40 */
} SpriteRenderer;

typedef struct Sprite {
    s32 x, y, z;                /* +0x0: position (16.16) */
    s32 speed_x, speed_y, speed_z; /* +0xc */
    s32 speed;                  /* +0x18: walking speed */
    s32 word1c;                 /* +0x1c */
    SpriteRenderer *renderer;   /* +0x20 */
    void *image;                /* +0x24 */
    u8 red, green, blue;     /* +0x28: colour of one-sided parts */
    u8 colour_flags;         /* +0x2b: bit 0 set: no colour */
    s16 scale;               /* +0x2c */
    u8 unknown2e[4];
    s16 direction;           /* +0x32 */
    u16 frame;               /* +0x34: pending frame, 0 none */
    u8 unknown36[4];
    u16 rate;                /* +0x3a: speed factor, 1024 = 1 */
    union {
        u32 word;
        struct {
        unsigned sides : 2;      /* 1: one-sided */
        unsigned unknown2 : 1;
        unsigned flip : 1;       /* mirrored frame */
        unsigned flip_y : 1;
        unsigned blend : 3;      /* blend rate + 1 */
        unsigned unknown8 : 20;
        unsigned dirty : 1;      /* orientation needs rebuilding */
        unsigned unknown29 : 3;
        } bits;
    } render;                /* +0x3c: tests read the word, as the original does */
    u32 flags;               /* +0x40: bits 8-12 facing group */
    u8 unknown44[8];
    s32 resource;            /* +0x4c */
    u8 unknown50[4];
    u16 *frame_table;        /* +0x54 */
    u8 unknown58[0xC];
    s32 frames_left;         /* +0x64 */
    void *callback;          /* +0x68: completion callback */
    u8 unknown6c[4];
    s32 word70;              /* +0x70 */
    u8 unknown74[8];
    void *sequencer;         /* +0x7c */
    u16 word80;              /* +0x80 */
    u16 word82;              /* +0x82 */
    u8 unknown84[4];
    u8 *frames;              /* +0x88 */
    s8 stack_top;            /* +0x8c: byte stack index, growing down */
    u8 unknown8d;
    u8 stack[0x10];          /* +0x8e */
    s16 countdown;           /* +0x9e: frames to the next command */
    u8 unknowna0[8];
    struct {
        unsigned sequencer_owned : 1; /* the sequencer buffer is allocated */
        unsigned unknown1 : 10;
        unsigned frame : 6;      /* frame table index */
        unsigned step : 3;
        unsigned phase : 2;
        unsigned field22 : 6;
        unsigned unknown28 : 4;
    } frame_bits;            /* +0xa8 */
    union {
        u32 word;
        struct {
        unsigned unknown0 : 2;
        unsigned mirror : 1;     /* mirror every frame */
        unsigned frame_flip : 1; /* the current frame is mirrored */
        unsigned unknown4 : 2;
        unsigned double_step : 1;
        unsigned divisor : 12;   /* gravity divisor */
        unsigned unknown19 : 13;
        } bits;
        u8 bytes[4];
    } motion;                /* +0xac */
    union {
        u32 wordb0;          /* bit 11: destroy flag-29 child tasks with the sprite */
        u8 byteb0;
    } b0;                    /* +0xb0 */
} Sprite; /* 0xb4 bytes; an inline renderer may follow */

/* A sprite's animation sequencer (0x1c bytes; inline at sprite + 0xf4). */
typedef struct {
    s32 word0;
    s32 word4;
    u8 unknown8[0x10];
    u16 *buffer;           /* +0x18: allocated by 8002303c */
} SpriteSequencer;

/* A sprite image header (inline at sprite + 0x110). */
typedef struct {
    u16 width;
    u16 height;
} SpriteImageSize;

typedef struct {
    u8 unknown0[4];
    SpriteImageSize size;  /* +0x4 */
} SpriteImage;


/* A resource block resolved by 80022224: words 1-3 of the data are offsets
 * of its sections. */
typedef struct {
    u8 *section2;          /* +0x0 */
    SVECTOR origin;        /* +0x4 */
    u8 *section3;          /* +0xc */
    u16 *section1;         /* +0x10 */
} SpriteResource;

/* A snapshot of a sprite's position and animation state (80021ebc). */
typedef struct {
    s32 x, y, z;           /* +0x0 */
    u8 unknownc[4];
    u16 word80;            /* +0x10 */
    s16 frame;             /* +0x12 */
    s16 byteaf;            /* +0x14 */
    s16 byteb0;            /* +0x16 */
    s16 field22;           /* +0x18 */
    u8 unknown1a[2];
    s32 sequencer0;        /* +0x1c */
    s32 sequencer4;        /* +0x20 */
    u16 scale_x;           /* +0x24: renderer scales */
    u16 scale_y;
    u16 scale_z;
    u16 scale;             /* +0x2a */
    u16 word82;            /* +0x2c */
} SpriteState;

/* A sprite with its two task nodes, as 800233a4 allocates it. */
typedef struct {
    Task task;
    Task auxiliary;
    Sprite sprite;
} SpriteTask;

/* The unit addresses these small globals absolutely, not through $gp: its
 * declarations carry no size (incomplete arrays), so they are not small data
 * under -G8. */
extern u8 D_800591AF[]; /* [0]: allocation mode for sprite tasks */
extern s32 D_80059428[];  /* [0]: frames the main task list stays paused */
extern s16 D_80059494[];
extern u8 D_800591AC[];   /* [0]: new main-list tasks count as active */
extern s32 D_80059464[];  /* [0]: active main-list tasks */
extern s32 D_800591A8[];
extern u8 D_800591AD[];
extern u8 D_800591AE[];
extern u8 D_800591B0[];
extern u8 D_800591B3[];
extern u8 D_8005A474[];
extern u8 *D_800594B8[];  /* [0]: end of the queue entry block */
extern s32 D_800591B8;    /* extra argument of 80024524/8002435c for one call */

/* An entry of the two sprite queues (bump-allocated from 800594b4). */
typedef struct SpriteQueueEntry {
    u32 value;
    struct SpriteQueueEntry *next;
} SpriteQueueEntry;
extern u8 D_8006BE10[];
extern s32 D_8005956C[];

/* The view matrix sprites are placed with (80024ff4 sets it). */
extern MATRIX D_8004FBB8;
extern u8 D_8004FBD8[]; /* packed image uploaded by 8001fab4 */
extern void (*D_8004FD40[])(Task *); /* task update callbacks by kind */
void func_8001CD64(Task *task, void (*update)(Task *));
void func_8001CA58(Task *owner, Task *node);
void func_8001CB48(Task *task);
void func_8001CBE8(Task *task);
void func_8001CD94(Task *task);
void func_8001CE44(Task *task);
void func_8001CC18(Task *owner, Task *node);
void func_8001CD6C(Task *task, void (*update)(Task *));
void func_8001CD74(Task *task, void (*destroy)(Task *));
void func_80022DF4(Task *task);
void func_80022EB8(Task *task);
void func_80025180(u32 value);
void func_8001CE74(Task *owner);
void func_8001D034(Task *owner);
void func_8001D3F4(Sprite *sprite);
void func_80023804(Sprite *sprite);
void func_8002393C(SpriteRenderer *renderer);

void func_8001F6B0(Sprite *sprite); /* recolour the parts */
void func_80022090(Sprite *sprite); /* rebuild the orientation */
void func_80022974(Sprite *sprite); /* velocity from speed and direction */
void func_80023210(Sprite *sprite);
void func_800245D8(Sprite *sprite, s32 value);
void func_8001D2B0(Sprite *sprite, s32 frame);
void func_8001DAE8(Sprite *sprite, s32 frame, void *image);
void func_8001E148(Sprite *sprite);
void func_80022038(Sprite *sprite);
void func_8001E3D8(Sprite *sprite, s32 frame);
void func_8001E9BC(Sprite *sprite, s32 frame);
void func_8001EE88(Sprite *sprite, s32 frame, void *image);
void func_8001F1D4(Sprite *sprite, s32 frame, void *image);
void func_8001F8E8(Sprite *sprite, s32 frame);
void func_800234AC(Sprite *sprite);
s32 func_8003F8B0(s32 angle); /* rcos */
s32 func_8003F8CC(s32 angle); /* rsin */
void func_800248D4(Sprite *sprite); /* run the next script command */
/* Frame timing; only the first word is known here. */
extern struct {
    s32 skip; /* extra frames per update */
    s32 unknown[2];
} D_80059198;
void func_80022B2C(Sprite *sprite);
void func_80024524(void *a0, s16 a1, s16 a2, s16 a3, s16 a4, s16 a5);
void func_8002435C(void *a0, s32 a1, s16 a2, s16 a3, s16 a4, s16 a5, s16 a6);
s32 func_80022CAC(Sprite *sprite, s32 value);
void func_80022CDC(Sprite *sprite);

#endif
