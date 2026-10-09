#ifndef RESIDENT_OWN_DECLARATIONS_H
#define RESIDENT_OWN_DECLARATIONS_H

/* The resident's declarations of its functions whose callers in other
 * targets were built against different ones: narrow parameters or results
 * where the resident has words (or the reverse), another parameter count
 * (8002cb54: the world map passes four), a by-value structure split
 * differently (80022224, 80023124). The shared headers in
 * decomp/include/resident leave these out, and each target declares them
 * for its own calls. They also leave out the overlay area D_8006FAF0: each
 * mode overlay's first unit defines that address as its number (a const
 * s32). */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/window.h"

/* cd.h */
void func_8002A428(u8 mode);

/* gpu.h */
Panorama *func_8002709C(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y,
                        s32 mode, s32 turn, VECTOR *position, u8 *colours, u16 fill_scale,
                        u16 fade_range, u16 fade_start);

/* heap.h */
void func_8003218C(u8 tag);
void func_800324B8(s16 kind);

/* mode.h */
extern u8 D_8006FAF0[]; /* the overlay area */

/* model.h */
void func_8002CB54(ModelBuffer *buffer, u8 **first, u8 **second);
s32 func_8002DDE4(s32 *images, s16 mode, s32 x, s32 y, s16 mode2, u16 x2, u16 y2); /* upload an image list */

/* sound.h */
SoundSeq *func_800397FC(SoundSeqHeader *header, s32 fade, s32 frames); /* start a sequence */
s32 func_8003BDFC(s32 wait);

/* sprite.h */
void func_8001FBE4(Sprite *sprite, u8 op, u8 *args); /* run a script command */
s32 func_80021AD8(s32 value, s32 delta);
void func_80021B04(SVECTOR *vector, s16 x, s16 y, s16 z);
u8 func_80021C20(Sprite *sprite);
void func_80021FE0(Sprite *sprite, s16 direction);
void func_80022224(SpriteResource *resource, s32 *data, SVECTOR origin, s32 mode);
void func_800223B0(Sprite *sprite, s16 angle);
s32 func_80023124(DVECTOR from, DVECTOR to); /* the direction from `from` to `to` */
Sprite *func_80024524(s32 *data, s16 x, s16 y, s16 width, s16 height, s16 unused);

/* text.h */
u8 *func_80033728(u8 *resource, s32 index);

/* window.h */
void func_80032F54(Window *window, s16 vram_x, s16 vram_y, s16 x, u16 y,
                   u16 columns, u16 rows); /* allocate and initialize */

/* The battle overlay's objects and calls the resident's sprite code uses, by
 * its own views (the battle overlay declares them with its types): the
 * acting sprite, the slots' formation places read from the battle area
 * D_800C3EB0 in strides of a slot, a group's sprites. 800b2aec has a K&R
 * definition there. */
typedef struct {
    u8 unknown0[0xE];
    s16 x, z;              /* +0xe */
    u8 unknown12[0xA];
} SpriteAnchor;

extern Sprite *D_800C3E1C;        /* the acting sprite */
extern SpriteAnchor D_800C3EB0[]; /* formation places by side and slot */
extern Sprite *D_800D363C[];      /* the sprites of a group, NULL-terminated */
extern u8 D_800C3664;
struct ScriptEntry;
void func_800B1F6C(struct ScriptEntry *entry, u8 *packets, u32 *ot, s32 unused, s32 depth, s32 blend); /* draw an effect-script entry with its packet buffer */
void func_800B2AEC(void *model, u8 *packets0, u8 *packets1, s16 red, s16 green, s16 blue); /* tint a model */
void func_800BA8F4(Sprite *sprite);   /* rest a sprite on the stage floor */
void func_800BC158(SpriteTask *task); /* register a camera marker */
void func_800C11CC(Sprite *sprite);   /* run a sprite's script */

#endif
