#ifndef RESIDENT_SOUND_H
#define RESIDENT_SOUND_H

#include "common.h"

/* Resident sound driver: SPU voices, channels and loaded sound banks. Field
 * names follow their observed use; unknown bytes keep their offsets. */

/* One SPU voice's registers (the driver's D_800508E4 base). */
typedef struct {
    s16 volume_left;
    s16 volume_right;
    u16 pitch;
    u16 address;
    u16 adsr1;
    u16 adsr2;
    u16 adsr_volume;
    u16 repeat;
} SpuVoice;

typedef struct {
    u8 unk0[6];
    u16 flags;
} SoundChannel;

/* A loaded sound bank (list through `next`). */
typedef struct SoundBank {
    u8 unk0[0x10];
    u16 flags;
    u8 unk12[2];
    u16 id;
    u8 unk16[6];
    struct SoundBank *next;
} SoundBank;

/* A playing sequence (list through `next`). */
typedef struct SoundSequence {
    u8 unk0[0x14];
    s32 voice;
    u8 unk18[6];
    u16 volume;
    u16 key;
    u8 unk22[6];
    s32 fade;
    struct SoundSequence *next;
} SoundSequence;

/* PsyQ libspu common attributes. */
typedef struct {
    s16 left;
    s16 right;
} SpuVolume;

typedef struct {
    SpuVolume volume;
    s32 reverb;
    s32 mix;
} SpuExtAttr;

typedef struct {
    u32 mask;
    SpuVolume mvol;
    SpuVolume mvolmode;
    SpuVolume mvolx;
    SpuExtAttr cd;
    SpuExtAttr ext;
} SpuCommonAttr;

/* A block of the driver's SPU memory pool. */
typedef struct {
    u16 flags;
    u16 unk2;
    u32 unk4;
    u32 next;
    u32 unkC;
} SpuBlock;

extern SpuVoice *D_800508E4;          /* SPU voice registers */
extern u16 D_8005957C;                /* driver state flags */
extern SoundChannel *D_8006252C[24];  /* channel of each voice */
extern SoundBank *D_80059440;         /* loaded banks */
extern SoundSequence *D_80059558;     /* playing sequences */

#endif
