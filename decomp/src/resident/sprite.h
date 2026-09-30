#ifndef RESIDENT_SPRITE_H
#define RESIDENT_SPRITE_H

#include "gpu.h"

/* A task's link word: its owner's serial and its state flags. */
typedef union {
    u32 word;
    struct {
        unsigned owner_serial : 29;  /* the owner's creation number */
        unsigned flag29 : 1;
        unsigned flag30 : 1;
        unsigned active : 1;
    } bits;
} TaskLink;

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
    TaskLink link;                       /* +0x14: flag tests read the word */
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
    s8 byte0;              /* x offset of the group */
    s8 byte1;              /* y offset */
    s16 half2;
    s16 half4;
    s16 half6;
} SpriteRendererEntry;

/* A sprite's renderer (its part list header). */
typedef struct {
    s16 angle_x, angle_y, angle_z; /* +0x0 */
    s16 scale_x, scale_y, scale_z; /* +0x6 */
    MATRIX matrix;                 /* +0xc: local screen matrix */
    SpritePart *parts[2];          /* +0x2c: two part lists (0x18 bytes per part); 80025718 draws the one of the queue being filled */
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
    s16 depth;               /* +0x2e: ordering-table depth of its last draw */
    s16 half30;              /* +0x30 */
    s16 direction;           /* +0x32 */
    u16 frame;               /* +0x34: pending frame, 0 none */
    u16 height;              /* +0x36: frame extent at its scale */
    u16 extent_depth;        /* +0x38 */
    u16 rate;                /* +0x3a: speed factor, 1024 = 1 */
    union {
        u32 word;
        struct {
        unsigned sides : 2;      /* 1: one-sided */
        unsigned unknown2 : 1;
        unsigned flip : 1;       /* mirrored frame */
        unsigned flip_y : 1;
        unsigned blend : 3;      /* blend rate + 1 */
        unsigned unknown8 : 8;
        unsigned field16 : 4;
        unsigned mode : 4;       /* resource binding mode (80022224) */
        unsigned no_view : 1;    /* drawn without the view matrix */
        unsigned unknown25 : 3;
        unsigned dirty : 1;      /* orientation needs rebuilding */
        unsigned unknown29 : 3;
        } bits;
    } render;                /* +0x3c: tests read the word, as the original does */
    u32 flags;               /* +0x40: bits 8-12 facing group */
    s32 *resource_block;     /* +0x44: the block the image's sections come from */
    s32 *animations;         /* +0x48: the animation block, NULL none */
    s32 resource;            /* +0x4c */
    s32 word50;              /* +0x50 */
    u16 *frame_table;        /* +0x54: the facing's frame table */
    u16 *animation;          /* +0x58: the animation header */
    u16 *facings;            /* +0x5c */
    u16 *word60;             /* +0x60: after the first section's count */
    u8 *script;              /* +0x64: the next animation command, NULL once finished */
    void *callback;          /* +0x68: completion callback */
    void *block;             /* +0x6c: the allocation holding the sprite */
    struct Sprite *word70;   /* +0x70: the sprite this one is attached to */
    struct Sprite *word74;   /* +0x74: the sprite this one aims at */
    u8 unknown78[4];
    void *sequencer;         /* +0x7c */
    u16 word80;              /* +0x80: facing angle */
    u16 word82;              /* +0x82 */
    s16 ground;              /* +0x84: floor height (whole units) */
    u16 size;                /* +0x86: bytes allocated for the sprite */
    u8 *frames;              /* +0x88 */
    s8 stack_top;            /* +0x8c: byte stack index, growing down */
    u8 unknown8d;
    u8 stack[0x10];          /* +0x8e */
    s16 countdown;           /* +0x9e: frames to the next command */
    s16 target_x, target_y, target_z; /* +0xa0: a position saved by command bc */
    u8 unknowna6[2];
    struct {
        unsigned sequencer_owned : 1; /* the sequencer buffer is allocated */
        unsigned bounce : 10;    /* rebound speed on landing, / 256 */
        unsigned frame : 6;      /* frame table index */
        unsigned step : 3;       /* facing group of the current angle */
        unsigned phase : 2;      /* facing groups: 0 one, 1 four, 2 eight */
        unsigned field22 : 6;    /* commands run in the current step */
        unsigned field28 : 2;
        unsigned unknown30 : 2;
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

typedef struct {
    u16 width;
    u16 height;
} SpriteImageSize;

/* A sprite's animation sequencer (0x1c bytes; inline at sprite + 0xf4). */
typedef struct {
    s32 word0;
    s32 word4;
    s32 word8;
    s16 halfc;
    SpriteImageSize size;  /* +0xe: image size for sequencer frames */
    u8 unknown12[6];
    u16 *buffer;           /* +0x18: allocated by 8002303c */
} SpriteSequencer;

/* A sprite's source data (sprite->image): its frame directory (a count, then
 * the offsets of the frame records from the directory) and its animations. */
typedef struct {
    u16 *frames;           /* +0x0 */
    DVECTOR origin;        /* +0x4: texture position of its cells */
    s16 clut_x;            /* +0x8 */
    s16 clut_y;            /* +0xa */
    u16 *palette;          /* +0xc */
    u16 *animations;       /* +0x10 */
} SpriteSource;

/* The texture placement of one sheet entry (after its first word). */
typedef struct {
    u16 u;             /* +0x0: texture column, in its top bits */
    s16 v;             /* +0x2 */
    u8 unknown4[0xC];
    s16 mode;          /* +0x10: nonzero: 8-bit texture (column / 4, else / 16) */
    s16 clut_x;        /* +0x12 */
    s16 clut_y;        /* +0x14 */
    u16 page_x;        /* +0x16 */
    u16 page_y;        /* +0x18 */
} SheetPart;

/* A sprite image header (inline at sprite + 0x110). */
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

/* Run the code between the two on the stack whose top is `top`. */
#define STACK_ENTER(top)                                                                           \
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"            \
                     :                                                                             \
                     : "r"(top)                                                                    \
                     : "$8", "memory")
#define STACK_LEAVE() __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory")

/* Small globals of other units: this unit addresses them absolutely (its
 * assembler ignored the `.extern` sizes GCC gives them). */
extern u8 D_800591AF;  /* allocation mode for sprite tasks */
extern s32 D_80059428; /* frames the main task list stays paused */
extern s16 D_80059494;
extern u8 D_800591AC;  /* new main-list tasks count as active */
extern s32 D_80059464; /* active main-list tasks */
extern s32 D_800591A8;
extern u8 D_800591AD;
extern u8 D_800591AE;
extern u8 D_800591B0;
extern u8 D_800591B3;
extern u8 D_8005A474[];
extern u8 *D_800594B8; /* end of the queue entry block */
extern s32 D_800591B8;    /* extra argument of 80024524/8002435c for one call */

/* A queued VRAM upload (LoadImage, or ClearImage without pixels), from the
 * queue block; 80025044 runs the list of the queue being filled. */
typedef struct ImageUpload {
    RECT rect;
    u_long *pixels;
    struct ImageUpload *next;
} ImageUpload;

/* The point and draw-mode primitives 8002541c takes from the queue block. */
typedef struct {
    u8 addr[3];
    u8 len;
    u32 colour;
    u32 xy;
} PointPrim;

typedef struct {
    u8 addr[3];
    u8 len;
    u32 code;
} ModePrim;

/* An entry of the two sprite queues (bump-allocated from 800594b4). */
typedef struct SpriteQueueEntry {
    u32 value;
    struct SpriteQueueEntry *next;
} SpriteQueueEntry;
extern u8 D_8006BE10[];
extern s32 D_8005956C;

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
s32 func_8001EE74(u16 *header); /* the part count of a frame header */
void func_80022000(Sprite *sprite, s32 scale);
void func_800239A0(Sprite *sprite);
void func_80023804(Sprite *sprite);
void func_8002393C(SpriteRenderer *renderer);

void func_8001F6B0(Sprite *sprite); /* recolour the parts */
void func_80022090(Sprite *sprite); /* rebuild the orientation */
void func_80022224(SpriteResource *resource, s32 *data, SVECTOR origin, s32 mode);
void func_800222BC(Sprite *sprite, s32 *data);
void func_800223B0(Sprite *sprite, s16 angle);
void func_80022660(Sprite *sprite, u8 *target, s32 count);
void func_80023538(Sprite *sprite, u16 *animation);
void func_80022974(Sprite *sprite); /* velocity from speed and direction */
void func_80023210(Sprite *sprite);
void func_800245D8(Sprite *sprite, s32 value);
void func_8001D2B0(Sprite *sprite, s32 frame);
void func_8001D53C(Sprite *sprite, s32 frame, SpriteSource *source);
void func_8001DAE8(Sprite *sprite, s32 frame, SpriteSource *source);
DVECTOR func_8001F530(s32 width);
void func_8001E148(Sprite *sprite);
void func_80022038(Sprite *sprite);
void func_8001E3D8(Sprite *sprite, u_long *ot);
void func_8001E9BC(Sprite *sprite, u_long *ot);
void func_8001EE88(Sprite *sprite, u_long *ot, s32 height);
void func_8001F1D4(Sprite *sprite, u_long *ot, s32 height);
void func_800BA8F4(Sprite *sprite); /* battle overlay: rest a sprite on the stage floor */
void func_8001F750(Sprite *sprite, s32 frame, SpriteSource *source);
void func_8001F8E8(Sprite *sprite, s32 frame, SpriteSource *source);
void func_800234AC(Sprite *sprite);
s32 func_8003F8B0(s32 angle); /* rcos */
s32 func_8003F8CC(s32 angle); /* rsin */
void func_800248D4(Sprite *sprite); /* run the next script command */
extern s32 D_80059198; /* extra frames per update */
void func_80022B2C(Sprite *sprite);
Sprite *func_80024524(s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused);
Sprite *func_8002435C(Sprite *sprite, s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused);
s32 func_80022CAC(Sprite *sprite, s32 value);
void func_80022CDC(Sprite *sprite);

/* An image cell of a sprite source (its pixels follow). */
typedef struct {
    u8 w, h;               /* +0x0: width in pixels, height */
    u16 kind;              /* +0x2: bit 0: 8-bit texture */
} SpriteCell;

void func_800251C8(u_long *pixels, s16 x, s16 y, s16 w, s16 h); /* queue an image upload */

/* A model sprite's renderer (render mode 2) as the script commands use it. */
typedef struct {
    s16 angle_x, angle_y, angle_z; /* +0x0 */
    u8 unknown6[0x26];
    u8 *packets[2];                /* +0x2c: the model's two packet buffers */
    void *model;                   /* +0x34 */
    s16 red, green, blue;          /* +0x38 */
} SpriteModelRenderer;

/* The sound owner of a sprite (word50) and of the scripts (8005919c). */
typedef struct {
    u8 unknown0[0x14];
    u16 bank;              /* +0x14: sound numbers of the owner are bank << 16 | number */
} SpriteVoice;

/* Positions of other modes the script can place a sprite at. */
typedef struct {
    u8 unknown0[0xE];
    s16 x, z;              /* +0xe */
    u8 unknown12[0xA];
} SpriteAnchor;

extern SpriteVoice *D_8005919C;
extern u8 *D_8006BE20;          /* the shared animation block */
extern VECTOR D_8006F99C;       /* positions (16.16) of two field points */
extern VECTOR D_8006F9AC;
extern Sprite *D_800C3E1C;      /* battle overlay: the acting sprite */
extern SpriteAnchor D_800C3EB0[]; /* battle overlay: formation places by side and slot */
extern Sprite *D_800D363C[];    /* battle overlay: the sprites of a group, NULL-terminated */
void func_800B2AEC(void *model, u8 *packets0, u8 *packets1, s16 red, s16 green, s16 blue); /* battle overlay: tint a model */

s32 func_80023124(DVECTOR from, DVECTOR to); /* the direction from `from` to `to` */
void func_80023290(Sprite *sprite, s32 rate);
void func_80023B84(Sprite *sprite, u8 *animation, void *image);
void func_80021B04(SVECTOR *vector, s16 x, s16 y, s16 z);
void func_80021B14(VECTOR *vector, s32 x, s32 y, s32 z);
s32 func_80021AD8(s32 value, s32 delta);
void func_80021CA0(Sprite *sprite, u8 value);
void func_80021FE0(Sprite *sprite, s16 direction);
void func_8001D4E8(Sprite *sprite);
void func_8001FB30(void);
u8 *func_8001FBA4(Sprite *sprite, u8 *code);

extern SpriteQueueEntry *D_80059580; /* the next free queue entry */
extern u8 *D_80059534;                /* its end */
extern u16 D_8004FAF8[8];  /* group masks tested against render byte 1 */
extern SVECTOR D_8004FB98[4]; /* the corners of the quad being drawn */
extern SVECTOR D_8004FAD8[4]; /* the corners of the shadow quad being drawn */

/* Texture positions of the resident cell pages (two-byte cell kinds). */
typedef struct {
    s16 x, y;
} TexturePosition;
extern TexturePosition D_8004FAB8[8];

#endif
