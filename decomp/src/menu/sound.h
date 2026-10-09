#ifndef MENU_SOUND_H
#define MENU_SOUND_H

#include "menu.h"

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

extern u8 D_80091F60[];
extern u8 D_80091F70[];
extern u8 D_80091F80[];
extern u8 D_80091F90[];
extern u8 D_80091FA0[];
extern u8 D_80091EE0[]; /* command sounds: two effect ids (0: none) per entry */
extern volatile s32 D_80059488; /* vertical blanks counted */

void func_80039FF8(void);                                      /* sound driver reset */
void func_80039F9C(s32 sound, s32 voice, s16 volume, s16 pan); /* key on */
void func_8003A55C(s32 voice, s32 pan);
void func_8003A344(s32 voice, s32 volume);
s32 func_8003A5D0(s32 sound);  /* mask of the voices still playing */
void func_8003A20C(s32 voice); /* key off */
void func_8008E78C(s32 sound, s32 mode, VECTOR *pos, s32 tag);

#endif
