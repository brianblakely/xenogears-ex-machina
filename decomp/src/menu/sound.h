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

extern u8 arena_sound_command_effect_pairs[]; /* command sounds: two effect ids (0: none) per entry */
extern u8 arena_sound_model29_command_sounds[];
extern u8 arena_sound_model36_command_sounds[];
extern u8 arena_sound_model27_command_sounds[];
extern u8 arena_sound_default_command_sounds[];
extern u8 arena_sound_model9_command_sounds[];

void arena_sound_reset(void);
void arena_sound_choose_command_table(Actor *owner);
void arena_sound_start_voice(s32 sound, s32 mode, VECTOR *pos, s32 tag);
void arena_sound_update_voices(void);
void arena_sound_free_stopped_voices(void);
void arena_sound_play_effect(s32 id);
void arena_sound_play_actor_effect(Actor *owner, s32 id, VECTOR *pos, s32 mode);
void arena_sound_play_command_sound(Actor *owner, s32 index, VECTOR *pos, s32 mode);
s32 arena_sound_stop_command_sound(Actor *owner, s32 index);

#endif
