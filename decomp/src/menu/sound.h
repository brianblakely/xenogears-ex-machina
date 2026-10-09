#ifndef MENU_SOUND_H
#define MENU_SOUND_H

#include "common.h"
#include "psyq/libgte.h"
#include "actor.h"

/* Positional sound (menu7 8008E620-8008EE1C): four voices that pan and
 * attenuate a sound from its projected position every frame, the menu's
 * sound effects and the characters' command sounds. */

/* One of four positional voices: a sound placed in the scene, panned and
 * attenuated from its projected position every frame. */
typedef struct {
    u8 mode;        /* 0 unpositioned, 1 fixed position, 2 follows a vector */
    u8 active;
    u8 voice;       /* sound driver voice */
    u8 unk3;
    u16 age;        /* frames since started, saturating */
    s32 sound;      /* 0x08 */
    s32 mask;       /* 0x0C: the voice's key mask */
    VECTOR pos;     /* 0x10: snapshot of the position */
    VECTOR *follow; /* 0x20 */
} SoundVoice;

extern u8 D_80091EE0[]; /* command sounds: two effect ids (0: none) per entry */
extern u8 D_80091F60[];
extern u8 D_80091F70[];
extern u8 D_80091F80[];
extern u8 D_80091F90[];
extern u8 D_80091FA0[];

void func_8008E620(void);
void func_8008E6F8(Actor *owner);
void func_8008E78C(s32 sound, s32 mode, VECTOR *pos, s32 tag);
void func_8008E8B0(void);
void func_8008EADC(void);
void func_8008EB4C(s32 id);
void func_8008EB88(Actor *owner, s32 id, VECTOR *pos, s32 mode);
void func_8008EBD0(Actor *owner, s32 index, VECTOR *pos, s32 mode);
s32 func_8008ED6C(Actor *owner, s32 index);

#endif
