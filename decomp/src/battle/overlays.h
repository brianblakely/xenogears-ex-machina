#ifndef BATTLE_OVERLAYS_H
#define BATTLE_OVERLAYS_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* The entries of the overlays the battle loads and calls, declared as the
 * battle calls them: some definitions take fewer arguments or other types
 * (801E879C, the 801FC000 modules' sprite script commands), so these stay
 * out of the overlays' own headers. The event script overlay's other entries
 * are in battle/event_script.h. */

/* The result screens (ovl2596, 801DE000). */
void func_801DE594(void); /* queue the result screens' primitives */
void func_801DF270(void); /* build each present member's first numbers */
void func_801DF4C0(void); /* build their seven-digit numbers */
void func_801E252C(void); /* leave the battle */

/* The battle's start from the scene select (ovl2606, 801E0000). */
void func_801E0A34(void);

/* The battle loader (ovl2615, 801E4000): set-up phases, the enemy set file,
 * the stage and the load modes. */
void func_801E5840(u8 phase); /* the battle module's set-up phase */
void func_801E62E0(s32 arg0); /* start loading the enemy set file */
u8 func_801E7210(u8 **scene, s32 unused, u8 *stage, u8 *origin, u8 *colours, u8 *tint); /* set up the stage */
void func_801E8588(void);
void func_801E893C(void);
void func_801E91E8(void);
void func_801E9594(void);

/* The event script interpreter's pass (ovl3087, 801E5000), which takes no
 * argument. */
void func_801E879C(s32);

/* The battle modules at 0x801FC000, one loaded at a time: break a model into
 * pieces (ovl3384: the model bound at a model sprite's renderer +0x34, its
 * packets and matrix), start an effect circling a sprite (ovl3383), and the
 * sprite script commands of ovl3385, ovl3386 and ovl3387. */
void func_801FC4C4(void *model, void *prims, MATRIX *m, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_801FC53C(Sprite *sprite, s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
void func_801FC6FC(Sprite *sprite, u8 *args);
void func_801FC7B0(Sprite *sprite, u8 *args);
void func_801FC898(void);

/* The debug pages (debug2611, 80280000). */
void func_8028022C(void);
void func_80280A9C(void); /* the debugger's frame hook */

#endif
