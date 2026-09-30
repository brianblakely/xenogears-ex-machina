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
    Vector pos;     /* 0x10: snapshot of the position */
    Vector *follow; /* 0x20 */
} SoundVoice;

extern SoundVoice D_80096EA0[4];

/* A character record as far as the sound choice reads it. */
typedef struct {
    u8 unk0[0x909];
    u8 kind; /* 0x909 */
    u8 unk90A[0x1664 - 0x90A];
    u8 *sounds; /* 0x1664: command sound table */
} SoundOwner;

extern u8 D_80091F60[];
extern u8 D_80091F70[];
extern u8 D_80091F80[];
extern u8 D_80091F90[];
extern u8 D_80091FA0[];

void func_80039FF8(void);                                      /* sound driver reset */
void func_80039F9C(s32 sound, s32 voice, s16 volume, s16 pan); /* key on */
void func_8003A55C(s32 voice, s32 pan);
void func_8003A344(s32 voice, s32 volume);

#endif
