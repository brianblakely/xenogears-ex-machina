#ifndef BATTLE_OVERLAYS_H
#define BATTLE_OVERLAYS_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* The modules the battle runs. */
void func_801DE594(void);
void func_801DF270(void);
void func_801DF4C0(void);
void func_801E0A34(void);
void func_801E252C(void);
void func_801E5840(u8 phase);                    /* the battle module's set-up phase */
void func_801E62E0(s32 arg0);
u8 func_801E7210(u8 **scene, s32 unused, u8 *stage, u8 *origin, u8 *colours, u8 *tint); /* set up the stage (ovl2615) */
void func_801E8588(void);
void func_801E879C(s32);
void func_801E893C(void);
void func_801E91E8(void);
void func_801E9594(void);
/* The entries of the battle modules at 0x801fc000: break a model into pieces
 * (ovl3384: the model bound at a model sprite's renderer +0x34, its packets and
 * matrix), start an effect circling a sprite (ovl3383). */
void func_801FC4C4(void *model, void *prims, MATRIX *m, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_801FC53C(Sprite *sprite, s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
/* The loaded battle module's entries (801FC000). */
void func_801FC6FC(Sprite *sprite, u8 *args);
void func_801FC7B0(Sprite *sprite, u8 *args);
void func_801FC898(void);
void func_8028022C(void);
/* Resident services. */
void func_80280A9C(void); /* the debugger's frame hook */

/* A party member's result screen block (the 801de000 module). */
typedef struct ResultPanel {
    u8 pad0[0x15F8];
    u8 unk15F8;   /* the second value is counted too */
    u8 counting;  /* +0x15F9 */
    u8 done[2];   /* +0x15FA per value */
} ResultPanel;
extern ResultPanel *D_800D32F8[3];

#endif
