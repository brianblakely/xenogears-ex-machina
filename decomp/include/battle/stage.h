#ifndef BATTLE_STAGE_H
#define BATTLE_STAGE_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/model.h"

/* The battle stage: its hierarchy, geometry, lighting and colours, drawn
 * each frame (8009E53C's unit, 800A4654-800A7064 but for the scene's ground
 * triangles, and 800A9A50). */

/* Run the calls between the two on a stack at the top of the scratchpad. */
#define SPAD_STACK_ENTER()                                                                         \
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"            \
                     :                                                                             \
                     : "r"(0x1F8003FC)                                                             \
                     : "$8", "memory")
#define SPAD_STACK_LEAVE() __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory")

/* The stage sky (battle_stage_sky): a scrolling textured ceiling of 8 x 8 tiles
 * over a 9 x 9 vertex grid, with horizon bands (the flats and quads; the
 * second half of each for the far band) per frame buffer. */
typedef struct {
    s16 scrollX;         /* 0x00: texture scroll, 1/16 texel */
    s16 scrollY;         /* 0x02 */
    s16 speedX;          /* 0x04 */
    s16 speedY;          /* 0x06 */
    u16 tileSize;        /* 0x08: texels per tile pair, a power of 2 */
    s16 height;          /* 0x0A: subtracted from the eye height / 4 */
    s16 farScale;        /* 0x0C: 8.8 */
    s16 nearScale;       /* 0x0E: 8.8 */
    s16 screen;          /* 0x10: projection distance */
    s16 tilt;            /* 0x12: pitch scale */
    u8 pad14[2];
    s16 horizon;         /* 0x16: horizon height */
    s16 distance;        /* 0x18: horizon distance */
    u8 pad1A[0x24 - 0x1A];
    DR_MODE modes[2];    /* 0x24: after the sky, per frame buffer */
    DR_MODE modes2[2];   /* 0x3C: before it */
    SVECTOR grid[9][9];  /* 0x54 */
    POLY_FT4 tiles[128]; /* 0x2DC: 64 per frame buffer */
    POLY_F4 flats[4];    /* 0x16DC */
    POLY_G4 quads[4];    /* 0x173C */
} StageGeometry;

/* The stage's lit colours in their saved order (0x88 bytes): per side the
 * geometry quad's four corners, its flat and the backdrop's flat, then the
 * backdrop quads' corners and the two colours of battle_buffer0_background_color_ptr/battle_buffer1_background_color_ptr. */
typedef struct {
    struct {
        CVECTOR quad[4];
        CVECTOR flat;
        CVECTOR backdropFlat;
    } sides[4];
    CVECTOR backdropQuads[2][4];
    CVECTOR extra[2];
} StageColors;

extern ModelTable *battle_stage_model_table; /* the stage's models (hierarchy battle_stage_model_parts) */
extern s32 battle_frame_ticks;               /* frame steps */
extern s16 battle_stage_image_width;         /* stage image width */
extern s16 battle_stage_image_x;             /* stage image x */
extern s16 battle_stage_image_y;             /* stage image y */
extern s16 battle_stage_image_height;        /* stage image height */

/* Draw the stage. */
void battle_draw_stage(MATRIX *view, MATRIX *light, s32 unused_mode, u32 *ot, s32 buffer, SVECTOR *eye, SVECTOR *target,
                   s32 depth);
void battle_init_stage_lighting(void); /* set up the stage lighting */
void battle_set_light_slot(s32 index, s32 r, s32 g, s32 b, s32 field4, s32 field5); /* set a light slot */
void battle_release_stage_image(void); /* release the stage image */
void battle_update_stage(MATRIX *m, s32 light, u32 *ot, s32 buffer); /* run the stage for the elapsed frames */

#endif
