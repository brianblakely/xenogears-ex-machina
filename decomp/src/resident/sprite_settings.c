/* Initialized small globals between the first sprite unit's .sdata and the
 * second's: the sprite engine's settings and the battle overlay's module and
 * single-action requests. Every user loads and stores them absolutely: the
 * first three sprite units, which reach small data of their own through $gp,
 * and the overlays (tools/data_users.py --range 80059198:800591b8). GCC
 * writes a -G8 unit's data, commons and .externs ahead of its code, so a
 * one-pass ASPSX would have seen a user's own definition first: they are no
 * user's own. The owner is inferred from the link position alone, an object
 * linked between sprite.o and sprite_80022090.o that never uses them. A
 * data-only unit (GP 8 in the target fragment) is the simplest such owner; a
 * text seam at 80021EBC instead of 80022090 (the functions from there touch
 * no small data) would leave a code unit that fits as well. Whether
 * D_800591B4 ends it or opens sprite_80022090.o's .sdata is undetermined. */
#include "common.h"
#include "psyq/libgpu.h"
#include "resident/mode.h"
#include "resident/sprite.h"

s32 D_80059198 = 0; /* extra frames per update */
SpriteVoice *D_8005919C = NULL;
s32 D_800591A0 = 0; /* unreferenced; size from value and alignment */
s32 D_800591A4 = 0; /* unreferenced; size from value and alignment */
s32 D_800591A8 = 0x2000;
u8 D_800591AC = 0; /* new main-list tasks count as active */
u8 D_800591AD = 0;
u8 D_800591AE = 0;
u8 D_800591AF = 0; /* allocation mode for sprite tasks */
u8 D_800591B0 = 1; /* the battle module is loaded */
u8 D_800591B1 = 1; /* the battle's requested single action is done (800b8068) */
u8 D_800591B2 = 0; /* the loaded battle module */
u8 D_800591B3 = 0; /* the requested battle module */
u16 D_800591B4 = 0; /* the battle's single-action request (800b8054), run by its frame loop */
