#ifndef BATTLE_SETUP_H
#define BATTLE_SETUP_H

#include "common.h"

/* The battle's set-up and end: the scene settings, the battle's outcome and
 * exit (80070E2C's unit 80076544), the result screen step (battle.c
 * 8008A9C0, declared where it is called), the party's battle masks and their
 * adjustments at the start (8008CCCC's 8009892C), the music, and the battle
 * heap and disc helpers (battle.c 8008AB4C-8008AC50). */

extern u8 D_800C3D44;
extern u8 D_800C3D5C;
extern s32 D_800C3DEC;
extern s32 D_800C3E54;     /* the battle music's sequence */
extern u8 D_800C3E4C;      /* battle end state */
extern u8 D_800C48EA;      /* battle outcome (the battle area's outcome) */
extern u8 D_800C492A;      /* keep the battle's resources at its end (the battle area's +0xa7a) */
extern s32 D_800D2D3C;     /* 801de000 module blocks */
extern s32 D_800D2F60;
extern u8 D_800D2D50;
extern u8 D_800D2FC4;      /* battle exit requested; the results then skip the rewards */
/* The battle overlay's entry, which the resident's battle mode (2) runs, and
 * the flag the event script (with a movie request, ovl3087 opcode 27), the
 * result screens and the loader overlays set for it: the battle then
 * continues in the movie mode (6). */
void func_80070F40(void);
extern u8 D_800D3338;

/* The skills each party member knew at the battle's start (80070E2C's unit
 * copies them from the game data): the result screens (ovl2596) show those
 * it learnt since. */
typedef struct KnownSkills {
    u16 counterSkills;
    u16 levelSkills;
} KnownSkills;

extern KnownSkills D_800C3E0C[3];

void func_80076544(void);              /* end the battle by its outcome state */
void func_8009892C(void);              /* the party's adjustments at battle start */

/* The battle heap and the disc. */
void func_8008AB4C(void);              /* heap mode 0x20/0 */
void func_8008AB70(void);              /* heap mode 0x20/2 */
void func_8008AB94(void);              /* heap mode 0x20/3 */
void func_8008AC50(void);              /* wait until the disc reads finish */

#endif
