#ifndef BATTLE_ACTION_RESOLVE_H
#define BATTLE_ACTION_RESOLVE_H

/* Resolving the committed action: the per-target formula driver and its
 * formulas. */

#include "common.h"

extern u8 D_800D2DB8;             /* resolve status returned to the caller */
extern u8 D_800C34AE;             /* character 4's item bookkeeping done */
extern void (*D_800C348C[])(void); /* formula table */

void func_800946F4(void);
void func_800968C0(void);
void func_80094C78(void);
void func_800958D8(void);
void func_80095A78(void);
void func_80095B44(void);
void func_80099FB0(void);
void func_8009AFD8(void);
void func_8009C198(void);

#endif
