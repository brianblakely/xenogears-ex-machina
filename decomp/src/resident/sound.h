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

/* A voice's claim on a hardware SPU voice (D_8006252C holds the owner of
 * each hardware voice). */
typedef struct {
    u16 voice;         /* hardware voice */
    u16 mode;
    s16 priority;
    u16 flags;         /* registers to update */
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

/* A sound track (list through `next`); flag 1 marks it paused. */
typedef struct SoundTrack {
    struct SoundTrack *next;
    u8 unk4[0xC];
    u16 flags;
} SoundTrack;

/* The effect id of a channel: bank id in the high half, effect in the low. */
typedef union {
    s32 full;
    struct {
        u16 effect;
        s16 bank;
    } part;
} SoundEffectId;

/* One channel of a playing sequence (0x158 bytes). */
typedef struct {
    u16 flags;         /* bit 0: active */
    u16 flags2;
    u16 flags3;        /* bit 0x20: volume slide */
    u8 voice_bit;      /* bit of the channel in the sequence's voice mask */
    u8 unk7;
    SoundEffectId id;
    u32 stamp;
    u8 unk10[8];
    u8 *loop;          /* sequence data position to return to */
    u8 unk1C[4];
    u16 unk20;
    u8 unk22;
    u8 unk23;
    u8 unk24[3];
    u8 voice;          /* hardware voice */
    u8 unk28[8];
    SoundChannel state;
    u8 unk38[0x24];
    s16 unk5C;
    u8 unk5E[8];
    s16 transpose;     /* in semitones */
    u8 unk68[0xC];
    s16 pan;
    s16 volume;
    u8 unk78[0x14];
    s16 volume_step;
    s16 volume_target;
    u8 unk90[0xA];
    s16 volume_frames;
    u8 unk9C[0xBC];
} SoundSeqChannel;

/* A linear slide of a 16.16 value. */
typedef struct {
    s32 value;
    s32 step;
    s16 frames;
    s16 target;
} SoundSlide;

/* A sequence being played: header, then its channels. Sequences are
 * listed through `next` (D_80059564). */
typedef struct SoundSeq {
    struct SoundSeq *next;
    struct SoundSeq *snapshot; /* saved copy of the sequence to restart from */
    u8 *data;          /* sequence data */
    u32 *table;        /* per-sequence table after the channels */
    u16 flags;         /* bit 15: playing, bit 8: stopped by a fade, bit 4: started */
    u8 unk12[2];
    u8 channels;
    u8 unk15[5];
    u8 unk1A;
    u8 unk1B;
    u8 unk1C[2];
    u16 unk1E;
    u8 unk20[4];
    s32 unk24;
    u32 ticks;
    s32 unk2C;
    u16 unk30;
    s16 unk32;
    s16 unk34;
    u16 unk36;
    s16 unk38;
    u16 unk3A;
    s16 unk3C;
    s16 unk3E;
    u8 unk40;
    u8 reverb_type;
    u8 reverb_delay;
    u8 reverb_feedback;
    s16 reverb_depth;
    u8 unk46[2];
    u32 voices;        /* mask of the channels holding a voice */
    u32 muted;         /* mask of the muted channels */
    u8 unk50[4];
    s32 tick_step;
    u8 unk58[2];
    s16 resolution;
    u8 unk5C[8];
    s32 tempo;         /* 16.16 */
    s32 tempo_step;
    s16 tempo_frames;
    s16 tempo_target;
    s32 fade;          /* 8.24 */
    s32 fade_step;
    s16 fade_frames;
    s16 fade_target;
    s32 volume;        /* 8.24 */
    s32 volume_step;
    s16 volume_frames;
    s16 volume_target;
    s32 pan;
    s32 pan_step;
    s16 pan_frames;
    s16 pan_target;
    SoundSeqChannel channel[1];
} SoundSeq;

/* Some tests read a channel's flags and flags2 as one word. */
#define SEQ_CHANNEL_FLAGS32(channel) (*(u32 *)&(channel)->flags)

/* The header of sequence data. */
typedef struct {
    u8 unk0[0x14];
    u8 channels;
    u8 entries;        /* entries of the table at `table` */
    u8 unk16[8];
    u16 unk1E;
    u16 table;         /* offset of 5-byte (index, word) entries */
} SoundSeqHeader;

typedef struct {
    s32 unk0;
    s16 frames;
    s16 seconds;
    s16 minutes;
} SoundTime;

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
