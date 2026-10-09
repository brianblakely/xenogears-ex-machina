#ifndef BATTLE_SPRITE_SCRIPT_H
#define BATTLE_SPRITE_SCRIPT_H

/* The battle's sprite script command handler (800B3F04): the resident
 * sprite runner calls it with a sprite, a command number (1-107) and the
 * command's argument bytes. */

#include "common.h"
#include "psyq.h"
#include "battle_core.h"
#include "sprite.h"
#include "frame.h"

extern u8 D_800C3564;    /* 1, or a slot + 2 */
extern BattleSprite *D_800C3568; /* the sprite the debugger follows */
extern u8 D_800C35D4;
extern u8 D_800C492A;
extern u16 D_800C3626; /* slots whose gear sound played */

/* Resident services. */
void func_8001D4E8(BattleSprite *sprite);
u8 *func_8001FBA4(BattleSprite *sprite, u8 *args); /* the colours of a command */
void *func_8001D164(void (*update)());           /* a task running update */
void *func_8001D0A4(void *owner, void (*update)()); /* the owner's child task running update */
void func_800222BC(BattleSprite *sprite, s32 data);
void func_80039EC4(s32 sound, s32 value);
void func_8003A14C(s32 sound);

/* The loaded battle module's entries (801FC000). */
void func_801FC6FC(BattleSprite *sprite, u8 *args);
void func_801FC7B0(BattleSprite *sprite, u8 *args);
void func_801FC898(void);

/* The command handlers of this unit, called with (sprite, args) whatever
 * they take (defined without using the rest). */
void func_800AA788(s32 value);
void func_800B3E04(void);
void func_800B4EDC(BattleSprite *sprite);
void func_800B572C(BattleSprite *sprite, u8 *colours);
void func_800B5B3C();
SpriteLink *func_800B5C18();
void func_800B5DC4(BattleSprite *sprite);
void func_800B5FBC();
void func_800B61B0();
void func_800B61F8();
void func_800B626C();
void func_800B62C8();
void func_800B639C();
void func_800B63F0();
void func_800B6438();
void func_800B6464();
void func_800B64D4();
void func_800B6518();
void func_800B65B0();
void func_800B6808();
void func_800B6930();
void func_800B6990();
void func_800B69E4();
void func_800B6A50();
void func_800B6A7C();
void func_800B6B98();
void func_800B6BFC();
void func_800B6C44();
void func_800B6C98();
void func_800B6CEC();
void func_800B6DC0();
void func_800B6E84();
void func_800BA768(BattleSprite *sprite);
void func_800BD1FC(s32 slot);
void func_800BF730(s32 value);
void func_800BF8CC(BattleSprite *sprite);

#endif
