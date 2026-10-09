#ifndef BATTLE_RESIDENT_VIEWS_H
#define BATTLE_RESIDENT_VIEWS_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/sprite.h"

/* The battle's own declarations of resident functions and data. Callers
 * convert arguments/result differently from the resident definition: narrow
 * parameters or results where the resident has words (or the reverse), other
 * parameter types, structures passed by value as other types; the resident
 * keeps its copies in its own_declarations.h, and the shared headers in
 * decomp/include/resident leave these out. The data are resident variables no
 * shared header declares, typed as the battle reads them. */

/* The random byte in low..high (8001bd40). */
u8 func_8001BD40(u8 low, u8 high);

/* Sprites (8001fbe4-800242f4). */
void func_8001FBE4(); /* run a sprite script command (the sprite VM calls it unprototyped) */
u8 func_80021AD8(u8 value, s32 delta); /* add, clamped to 0-255 */
void func_80021B04(SVECTOR *v, s32 x, s32 y, s32 z);
s32 func_80021C20(); /* a u8, taken as int */
void func_80021FB8(Sprite *sprite, s32 mode); /* set the idle mode */
void func_80021FE0(Sprite *sprite, s32 direction); /* turn a sprite to a direction */
void func_80022224(); /* upload an image: resource, image, its place and its CLUT's place
                       * (four-byte points by value), mode */
void func_800223B0(Sprite *sprite, s32 direction);
s16 func_80023124(GroundPoint to, GroundPoint from); /* the direction between points */
void func_800242F4(Sprite *sprite, s32 a, s16 b, s16 c, s32 d, s32 e, s32 f, s32 g);

/* Glyphs and images (80025fa8-80026dcc, 8002dde4). */
/* 80025fa8: the u16 coordinates, scale and angle passed as full words. */
s32 func_80025FA8(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y,
                  s32 scaleX, s32 scaleY, s32 angle);
s32 func_800263E4(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 w, s32 scale, s32 a, s32 b);
s32 func_8002675C(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y, s32 scale);
s32 func_80026DCC(void *font, s32 character, SpritePart *out, s16 x, s32 y); /* glyphs added */
void func_8002DDE4(void *images, s16 on, s32 a, s32 b, s16 c, s32 d, s32 e); /* upload images */

/* Disc streams (80029eb0). */
void func_80029EB0(s32 file, void *ring, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8, s32 a9);

/* Models (8002cb54-8002cc74). */
void func_8002CB54(SpriteModel *model, void **packets0, void **packets1); /* allocate packets */
void func_8002CC10(s16 x, s16 y);
void func_8002CC74(s16 x, s16 y);

/* Text (80033728-80034eac). */
void *func_80033728(void *table, s32 index);
void *func_80033784(u8 character, u8 id); /* a character text */
u8 *func_80033818(s32 id); /* item name */
u8 *func_80033848(s32 id); /* equipment name */
s32 func_80034EAC(void *text, u32 *pixels, s32 width, s32 mode);

/* Sound (800383ec-8003bdfc). */
s32 func_800383EC(u16 id);
s32 func_8003864C(SoundBank *bank, s32 mode); /* whether a sound bank is loaded */
s32 func_800397FC(u8 *a, s32 b, s32 c); /* start the battle music */
void func_80039DB8(s32 effect);
void func_8003A2E4(s32 sound, s32 volume); /* set its volume */
s16 func_8003BDFC(s32 wait); /* sound transfer busy */

/* Resident data no shared header declares. */
extern u8 D_8001C76C[];       /* a TMD model (an effect script file, battle/effect_script.h) */
extern u8 D_8004FC40[];       /* the byte widths of sprite VM commands 80-FF */
extern s32 D_80059488;        /* the vertical blank count */
extern s16 D_8005A3A0[];      /* the AI scripts' shared variables */
extern u8 D_800658DC[][0x20]; /* scene settings */

/* Views of the game data D_8006D634 (resident/gamedata.h) the battle indexes
 * by slot: the item durabilities from +0x2286 and the gear part durabilities
 * from its flags at +0x22B6 (word 0 the option flags). Their extent is not
 * settled, so they stay names of their own. */
extern u8 D_8006F8BA[]; /* item durability by slot */
extern u8 D_8006F8EA[]; /* gear part durability by slot */

#endif
