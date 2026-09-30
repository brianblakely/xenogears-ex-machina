#ifndef BATTLE_SOUND_H
#define BATTLE_SOUND_H

#include "common.h"

/* A loaded sound bank: its id and data; a list of them ends with id 0. */
typedef struct {
    s16 id;
    void *data;
} SoundBank;

/* The gears' sound bank sets, two bytes per gear id: the base bank and the
 * variant count. A gear loads banks base + 1, base + 2 and, with a sound
 * variant (1..count), base + 2 + variant. */
extern u8 D_800C3508[];

#endif
