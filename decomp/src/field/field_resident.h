#ifndef FIELD_FIELD_RESIDENT_H
#define FIELD_FIELD_RESIDENT_H

/* Resident calls and variables the field declares itself.
 *
 * The functions' field callers convert arguments/results differently from
 * the resident definition (narrow parameters where the resident has words,
 * or the reverse; another result), so the shared headers leave them out and
 * decomp/src/resident/own_declarations.h holds the resident's own copies.
 * The variables are resident objects the shared headers do not declare,
 * with the field's views of them. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/model.h"
#include "resident/sprite.h"
#include "resident/window.h"

/* sprite.h: callers convert arguments/result differently from the resident definition. */
void func_80021B98(void *sprite, s32 r, s32 g, s32 b); /* colour the one-sided parts */
void func_80021BCC(Sprite *model, u16 value);           /* the gravity divisor */
void func_80021FE0(Sprite *model, s32 heading);         /* turn a sprite */
void func_800223B0(Sprite *model, s16 angle);           /* face a sprite at `angle` */
void func_80022A70(void *tim, s32 x, s32 y);            /* upload an image list */
Sprite *func_80024524(void *data, s16 a, s16 b, s16 x, s16 y, s32 c); /* build a sprite */
Sprite *func_80024294(void *data, s16 a, s16 b, s16 x, s16 y, s32 c, s32 bank); /* with a bank */

/* gpu.h and cd.h: callers convert arguments/result differently from the resident definition. */
Panorama *func_8002709C(s32 tex_x, s32 tex_y, s32 width, s32 height, s32 clut_x, s32 clut_y, s32 mode,
                        s32 turn, s32 *position, u8 *colours, s32 fill_scale, s32 fade_range,
                        s32 fade_start); /* create the panorama */
void func_80027D64(TextureScroll *scroll, s16 x, s16 y, s16 width, s16 height, s16 length, s16 a, s16 b,
                   u8 *buffer); /* set a texture scroll up */
void func_80029EB0(s32 file, void *ring, s32, s32, s32, s32, s32, s32, s32, s32); /* start a stream */

/* model.h: callers convert arguments/result differently from the resident definition. */
void func_8002C6E0(s32 r, s32 g, s32 b);   /* the model (fog) colour */
void func_8002CB54(SpriteModel *model, void **first, void **second); /* allocate its packets */
void func_80030A30(s32 index, ModelLight *light); /* set a light */
void func_80030C40(s32 r, s32 g, s32 b);   /* the background colour */

/* heap.h, text.h and window.h: callers convert arguments/result differently from the resident definition. */
void func_8003218C(s32 tag);               /* release the blocks with `tag` */
void func_80032F54(Window *window, s32 vram_x, s32 vram_y, s32 x, s32 y, s32 columns, s32 rows);
void func_80033698(s32 x, s32 y);          /* upload the text palette */
s32 func_80033728(void *messages, s32 message); /* a message of a resource */
s32 func_8003373C(void *table, s32 message);  /* message columns */
s32 func_80033760(void *table, s32 message);  /* message rows */
s32 func_80033CD0(Window *window);         /* chosen answer, 0 while open */
void func_80034800(Window *window, s32 r, s32 g, s32 b); /* colour the lines */
void func_80034874(Window *window, s32 line);
void func_8003633C(s32 value);

/* sound.h: callers convert arguments/result differently from the resident definition. */
void func_80039F9C(s32 id, s16 voice, s16 volume, s16 pan); /* play a sound effect */
void func_8003BDFC(s32 wait);              /* wait for the SPU transfer */

/* Resident objects the shared headers do not declare. */
extern s32 D_80059488;         /* vertical blank count (main2.c) */
extern CVECTOR D_80059598;     /* the model (fog) colour 8002c6e0 sets */
extern u8 D_8005061C[6];       /* option bytes of the field and the menu */
extern u8 D_80050622;          /* the arena bout's outcome (menu3 writes it) */
extern u8 D_8005A4E4[];        /* the field snapshot (0x22fc bytes, 800a3f4c) */

#endif
