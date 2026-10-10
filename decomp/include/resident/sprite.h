#ifndef RESIDENT_SPRITE_H
#define RESIDENT_SPRITE_H

#include "gpu.h"
#include "task.h"

/* Resident sprite/actor engine (the four sprite units, 0x8001c8dc-0x8002709c;
 * the task lists they run on are in task.h). Only the fields the recovered
 * functions use are named. */
extern struct Sprite *sprite_pending_list; /* sprites awaiting a frame (through renderer->next_pending) */

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

/* A screen offset in pixels (copied as one pair). */
typedef struct {
    s8 x, y;
} SpriteOffset;

/* A sprite's renderer. The group/part fields below are the one-sided
 * view ((render.word & 3) == 1); model sprites use SpriteModelRenderer. */
typedef struct {
    s16 angle_x, angle_y, angle_z; /* +0x0 */
    s16 scale_x, scale_y, scale_z; /* +0x6 */
    MATRIX matrix;                 /* +0xc: local screen matrix */
    SpritePart *parts[2];          /* +0x2c: two part lists (0x18 bytes per part) */
    SpriteRendererEntry *pointer34; /* +0x34: eight group entries for one-sided sprites */
    struct Sprite *next_pending;   /* +0x38 */
    SpriteOffset offset;           /* +0x3c: screen offset, before scaling */
    u8 unknown3e[2];
    s32 word40;                    /* +0x40 */
    SVECTOR light_angles;          /* +0x44: lit models (800257f0) */
    u16 light_colour[3];           /* +0x4c */
} SpriteRenderer;

typedef struct Sprite {
    s32 x, y, z;                /* +0x0: position (16.16) */
    s32 speed_x, speed_y, speed_z; /* +0xc */
    s32 speed;                  /* +0x18: walking speed */
    s32 gravity;                /* +0x1c: 16.16, added to speed_y per step */
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
        u8 bytes[4];         /* [1]: the field stores its draw mode as a byte */
    } render;                /* +0x3c: tests read the word, as the original does */
    u32 flags;               /* +0x40: SpriteFlagBits (bits 8-12 the scale shift) */
    s32 *resource_block;     /* +0x44: the block the image's sections come from */
    s32 *animations;         /* +0x48: the animation block, NULL none */
    s32 resource;            /* +0x4c */
    s32 word50;              /* +0x50 */
    u16 *frame_table;        /* +0x54: the facing's frame table */
    u16 *animation;          /* +0x58: the animation header */
    u16 *facings;            /* +0x5c */
    u16 *word60;             /* +0x60: after the first section's count */
    u8 *script;              /* +0x64: the next animation command, NULL once finished */
    void (*callback)(struct Sprite *sprite); /* +0x68: completion callback */
    void *block;             /* +0x6c: the allocation holding the sprite */
    struct Sprite *parent;   /* +0x70: the sprite this one is attached to */
    struct Sprite *partner;  /* +0x74: the sprite this one aims at */
    s32 word78;              /* +0x78 */
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
        unsigned unknown4 : 1;
        unsigned owns_children : 1; /* the tasks it created end with it (80022eb8) */
        unsigned double_step : 1;
        unsigned divisor : 12;   /* gravity divisor */
        unsigned unknown19 : 13;
        } bits;
        u8 bytes[4];
    } motion;                /* +0xac */
    union {
        u32 wordb0;          /* bit 11: destroy flag-29 child tasks with the sprite */
        u8 byteb0;
        struct {
            unsigned unknown0 : 8;
            unsigned passive_children : 1; /* its child sprites start inactive */
            unsigned share_rate : 1;       /* its child sprites take its speed factor */
            unsigned unknown10 : 22;
        } bits;
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
    u8 unknown12[2];
    s16 actor;             /* +0x14: the field's event actor (descriptor) it belongs to */
    u8 unknown16[2];
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
    u16 w, h;          /* +0x4: size */
    u16 x, y;          /* +0x8: placement from the sheet origin */
    u8 unknownc[4];
    s16 mode;          /* +0x10: nonzero: 8-bit texture (column / 4, else / 16) */
    s16 clut_x;        /* +0x12 */
    s16 clut_y;        /* +0x14 */
    u16 page_x;        /* +0x16 */
    u16 page_y;        /* +0x18 */
    u8 flip_x, flip_y; /* +0x1a */
} SheetPart; /* 0x1c bytes */

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
extern s32 sprite_default_scale;
extern u8 sprite_in_battle;
extern u8 sprite_in_worldmap;
extern u8 sprite_battle_module_loaded;
extern u8 sprite_requested_battle_module;
extern u8 sprite_effect_source[];
extern u8 *sprite_queue_unread_second_entry_block; /* the second queue's half of the entry block */
extern s32 sprite_palette_bank;                    /* extra argument of 80024524/8002435c for one call */

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
extern u8 sprite_shared_source[];
extern s32 sprite_ot;

/* The view matrix sprites are placed with (80024ff4 sets it). */
extern MATRIX sprite_view_matrix;
extern u8 sprite_packed_pause_image[]; /* packed image uploaded by 8001fab4 */
extern void (*sprite_draw_callbacks[])(Task *); /* task update callbacks by kind */
void sprite_task_update(Task *task);
void sprite_task_destroy(Task *task);
void sprite_queue_free_later(u32 value);
void sprite_clear_pending_list(void);
void sprite_clear_pending_list_on_release(void);
void sprite_remove_pending(Sprite *sprite);
s32 sprite_get_part_count(u16 *header); /* the part count of a frame header */
void sprite_set_scale(Sprite *sprite, s32 scale);
void sprite_attach_inline_storage(Sprite *sprite);
void sprite_reset_defaults(Sprite *sprite);
void sprite_reset_renderer(SpriteRenderer *renderer);

void sprite_recolor_parts(Sprite *sprite); /* recolour the parts */
void sprite_rebuild_orientation(Sprite *sprite); /* rebuild the orientation */
void sprite_bind_resource(Sprite *sprite, s32 *data);
void sprite_vm_replay_frames(Sprite *sprite, u8 *target, s32 count);
void sprite_apply_animation_header(Sprite *sprite, u16 *animation);
void sprite_update_velocity(Sprite *sprite); /* velocity from speed and direction */
void sprite_vm_tick(Sprite *sprite);
void sprite_start_animation(Sprite *sprite, s32 animation);
void sprite_request_frame(Sprite *sprite, s32 frame);
void sprite_build_cell_frame(Sprite *sprite, s32 frame, SpriteSource *source);
void sprite_build_frame(Sprite *sprite, s32 frame, SpriteSource *source);
DVECTOR sprite_reserve_texture_columns(s32 width);
void sprite_set_draw_matrix(Sprite *sprite);
void sprite_update_orientation(Sprite *sprite);
void sprite_draw_parts(Sprite *sprite, u_long *ot);
void sprite_draw_shadow(Sprite *sprite, u_long *ot);
void sprite_draw_parts_cut_below(Sprite *sprite, u_long *ot, s32 height);
void sprite_draw_parts_cut_above(Sprite *sprite, u_long *ot, s32 height);
void sprite_apply_cell_frame_controls(Sprite *sprite, s32 frame, SpriteSource *source);
void sprite_apply_frame_controls(Sprite *sprite, s32 frame, SpriteSource *source);
void sprite_clear_group_entries(Sprite *sprite);
s32 gpu_get_sin(s32 angle); /* sine (4096 = 1.0) */
s32 gpu_get_cos(s32 angle); /* cosine (4096 = 1.0) */
void sprite_vm_run(Sprite *sprite); /* run the next script command */
extern s32 sprite_frame_skip; /* extra frames per update */
void sprite_move_vertically(Sprite *sprite);
Sprite *sprite_construct(Sprite *self, s32 *data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s16 unused);
s32 sprite_scale_by_rate(Sprite *sprite, s32 value);
void sprite_move(Sprite *sprite);

/* An image cell of a sprite source (its pixels follow). */
typedef struct {
    u8 w, h;               /* +0x0: width in pixels, height */
    u16 kind;              /* +0x2: bit 0: 8-bit texture */
} SpriteCell;

void sprite_queue_upload(u_long *pixels, s16 x, s16 y, s16 w, s16 h); /* queue an image upload */

/* A model sprite's alternate renderer view ((render.word & 3) == 2).
 * Resident F5-F7 bind a SpriteModel; battle F3 binds a ScriptEntry. */
typedef struct {
    s16 angle_x, angle_y, angle_z; /* +0x0 */
    u8 unknown6[0x26];
    u8 *packets[2];                /* +0x2c: the model's two packet buffers */
    void *model;                   /* +0x34: bound model or effect-script entry */
    s16 red, green, blue;          /* +0x38 */
} SpriteModelRenderer;

/* The sound owner of a sprite (word50) and of the scripts (8005919c). */
typedef struct {
    u8 unknown0[0x14];
    u16 bank;              /* +0x14: sound numbers of the owner are bank << 16 | number */
} SpriteVoice;

/* Positions of other modes the script can place a sprite at. */
extern SpriteVoice *sprite_script_sound_bank;
extern VECTOR sprite_camera_eye;       /* positions (16.16) of two field points */
extern VECTOR sprite_camera_look_at;

void sprite_set_blend_rate(Sprite *sprite, s32 rate);
Sprite *sprite_create_child(Sprite *parent, u16 *header, SpriteSource *source);
void sprite_set_vector(VECTOR *vector, s32 x, s32 y, s32 z);
void sprite_stack_push_byte(Sprite *sprite, u8 value);
void sprite_alloc_group_entries(Sprite *sprite);
void sprite_upload_image_list(void);
u8 *sprite_vm_resolve_variable(Sprite *sprite, u8 *code);

extern SpriteQueueEntry *sprite_queue_next_free; /* the next free queue entry */
extern u8 *sprite_queue_block_end;               /* its end */
extern u16 sprite_halfword_bit_masks[16];        /* bit masks; the facing groups test render byte 1 */
extern SVECTOR sprite_quad_corners[4];           /* the corners of the quad being drawn */
extern SVECTOR sprite_shadow_corners[4];         /* the corners of the shadow quad being drawn */

/* Texture positions of the resident cell pages (two-byte cell kinds). */
typedef struct {
    s16 x, y;
} TexturePosition;
extern TexturePosition sprite_cell_page_positions[8];
/* Second sprite unit (80022090-8002709c). */
/* The bits of a sprite's flags word (+0x40) that the original writes as fields
 * (the battle modules read part_bytes and shift as fields too). */
typedef struct {
    unsigned part_bytes : 8; /* four per drawn part: bits 2-7 the part count */
    unsigned shift : 5;      /* scale shift of the parts' offsets and sizes (8001e148) */
    unsigned type : 4;       /* sprite kind (80023440) */
    unsigned unknown17 : 1;
    unsigned flag18 : 1;     /* inherited by child sprites (80023b84) */
    unsigned unknown19 : 13;
} SpriteFlagBits;
extern u8 sprite_vm_command_lengths[0x80]; /* lengths of the frame commands 0x80-0xff */
void sprite_stack_push_three_bytes(Sprite *sprite, s32 value); /* push three bytes */
void sprite_show_indexed_frame(Sprite *sprite);
s32 sprite_get_header_kind(u16 *entry);
s32 sprite_get_render_kind(s32 kind, s32 fallback);
SpriteTask *sprite_task_create(s32 kind, s32 mode, SpriteSource *source, s32 extra, Task *owner);
void sprite_task_init_by_kind(SpriteTask *task);
void sprite_task_update_counted(Task *task);
void sprite_task_set_draw_by_kind(Task *task, s32 kind);
s32 sprite_stack_pop_three_bytes(Sprite *sprite);
void sprite_draw(Sprite *sprite, u_long *ot); /* draw into the ordering table entry at `ot` */
/* The scratchpad work area of the pixel colour scaling (80025c04). */
typedef struct {
    u16 colour;     /* +0x0: the scaled pixel */
    u16 unused2;
    VECTOR in;      /* +0x4 */
    VECTOR out;     /* +0x14 */
} ColourScratch;
#define COLOUR_SCRATCH ((ColourScratch *)0x1F800000)
extern SVECTOR sprite_sheet_corners[4]; /* corners of a sheet part being drawn */
extern MATRIX sprite_light_color_matrix; /* light colour matrix of lit sprite models */
extern MATRIX sprite_light_direction_matrix; /* light direction matrix of lit sprite models */

/* More of the sprite services and their state. */
void sprite_build_pending_frames(void);
void sprite_draw_cut_below(Sprite *sprite, u_long *ot, s32 height);
void sprite_draw_cut_above(Sprite *sprite, u_long *ot, s32 height);
s32 sprite_is_cell_directory(u8 *frame);
void sprite_get_extent(Sprite *sprite, s32 unused, s32 *width, s32 *height, s32 *depth);
void sprite_upload_pause_image(s32 x, s32 y);
void sprite_copy_svector(SVECTOR *to, SVECTOR *from);
void sprite_set_alternate_resource(Sprite *sprite, s32 resource);
void sprite_set_completion_callback(Sprite *sprite, void *callback);
void sprite_set_scale_shift(Sprite *sprite, s32 shift);
void sprite_restore_state(Sprite *sprite, SpriteState *state);
void sprite_save_state(Sprite *sprite, SpriteState *state);
void sprite_set_walk_speed(Sprite *sprite, s32 speed);
s32 sprite_read_word(s32 *word);
void sprite_alloc_sequencer_buffer(Sprite *sprite, s32 count, s32 mode);
void sprite_destroy(Sprite *sprite);
void sprite_realloc_parts_from_bottom(Sprite *sprite, s32 count);
SpriteTask *sprite_create_effect(s32 index, SpriteSource *source, SVECTOR *position, s32 extra);
void sprite_alloc_queues(s32 size, s32 mode);
void sprite_free_queues(void);
void sprite_set_ot(s32 value);
void sprite_set_view_matrix(MATRIX *view);
void sprite_queue_run_uploads(void);
void sprite_queue_start_fill(s32 queue);
void sprite_task_draw_unlit_tmd(Task *task);
void sprite_tint_blend_pixels(s32 count, u16 *src, u16 *base, u16 *dst, s32 red, s32 green, s32 blue, s32 mode, s32 factor);
void sprite_sheet_get_texture(u16 *sheet, s32 id, s32 *first, s32 *mode, s32 *clut_x, s32 *clut_y, s32 *x, s32 *y);
void sprite_sheet_queue_quads(u16 *sheet, s32 id, s32 x, s32 y, u_long *ot);
extern u8 sprite_single_action_done;
extern u8 sprite_loaded_battle_module;
extern u16 sprite_single_action_request;
extern u8 mode_battle_party_ids[3];
extern void *mode_battle_action_stream_ring;
extern u8 menu_state_saved_cursor;
extern s32 window_semi_transparency_mode;
extern void *mode_battle_setup_archive;

/* Darken `count` RGB555 pixels of `source` into `out` by level / 32, and
 * blend them from `base` towards `target` by level / 32, through the GTE
 * (handwritten). */
void sprite_darken_pixels(s32 count, s32 level, u16 *out, u16 *source);
void sprite_blend_pixels(s32 count, s32 level, u16 *out, u16 *base, u16 *target);

#endif
