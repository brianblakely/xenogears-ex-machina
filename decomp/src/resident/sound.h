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
    u16 mode;          /* 0x10 pitch modulation, 0x20 noise, 0x40 reverb */
    s16 priority;
    u16 flags;         /* registers to update (0x1000-0x4000: mode bits) */
} SoundChannel;

/* A loaded sound bank (list through `next`). */
typedef struct SoundBank {
    u8 unk0[0x10];
    u16 flags;
    u8 unk12[2];
    u16 id;
    u16 unk16;         /* instrument set key */
    u16 volumes;       /* offset of the per-effect volume bytes */
    u8 unk1A[2];
    struct SoundBank *next;
    u16 effect[1];     /* data offsets of each effect's two channels */
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

/* A 16.16 value whose whole part is also read on its own. */
typedef union {
    s32 value;
    struct {
        u16 fraction;
        s16 whole;
    } part;
} SoundFixed;

/* A per-channel low-frequency modulator (four per channel; the first
 * modulates the pitch). */
typedef struct {
    void (*wave)(void *modulator);
    s32 phase;
    u8 unk8[4];
    s32 step;
    u16 unk10;
    s16 rate;
    s16 delay_count;
    s16 delay;
    s16 period_count;
    s16 period;
    u8 target;         /* 0 pitch, 1 volume, 2 pan */
    u8 shape;
    u16 flags;         /* bit 0: on */
} SoundModulator;

/* A repeat of a channel's sequence data. */
typedef struct {
    u8 count;          /* repeats left */
    u8 unk1;
    u8 transpose;      /* at the start of the repeat */
    u8 exit_transpose; /* at its end */
    u8 *start;
    u8 *end;
} SoundLoop;

/* One channel of a playing sequence (0x158 bytes). */
typedef struct {
    u16 flags;         /* bit 0: active, 0x20: muted */
    u16 flags2;        /* registers to update */
    u16 flags3;        /* bit 0x20: volume slide */
    u8 voice_bit;      /* bit of the channel in the sequence's voice mask */
    u8 priority;
    SoundEffectId id;
    u32 stamp;         /* start time (effect channels) */
    u8 *position;      /* sequence data position */
    u8 *start;
    u8 *loop;          /* sequence data position to return to */
    s32 unk1C;
    u16 unk20;
    u8 unk22;
    u8 unk23;
    u8 unk24;
    u8 unk25;
    u8 instrument;
    u8 voice;          /* hardware voice */
    u8 unk28;
    u8 unk29[3];
    struct SoundSequence *instruments;
    SoundChannel state;
    u8 unk38[4];
    u16 unk3C;
    u16 unk3E;
    u8 unk40[0x14];
    u8 envelope[8];    /* ADSR parameters (state.flags 0x10-0x100 update them) */
    s16 unk5C;
    u8 unk5E[2];
    u8 unk60;
    u8 unk61;
    u16 unk62;
    u8 unk64;
    u8 unk65;
    s16 transpose;     /* in semitones */
    s32 note;          /* 16.16 */
    s16 unk6C;
    s16 detune;
    u16 unk70;
    u16 loop_depth;    /* innermost entry of `loops`, 0xFFFF when none */
    s16 pan;           /* 0 left, 0x4000 centre, 0x7F00 right */
    s16 volume;
    SoundFixed level;  /* its whole part scales the volume */
    s32 unk7C;
    s16 unk80;
    s16 unk82;
    s32 unk84;
    s32 unk88;
    s16 volume_step;
    s16 volume_target;
    s16 pan_step;
    s16 pan_target;
    s16 unk94;
    s16 unk96;
    s16 pan_frames;
    s16 volume_frames;
    SoundLoop loops[4];
    u16 modulator_index; /* modulator the generic opcodes address */
    u16 modulators;    /* mask of the running modulators */
    u16 unkD0;
    s16 unkD2;
    s16 unkD4;
    u8 unkD6[2];
    SoundModulator modulator[4];
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
    struct SoundSeqHeader *header; /* sequence data */
    u32 *table;        /* per-sequence table after the channels */
    u16 flags;         /* bit 15: playing, bit 8: stopped by a fade,
                        * bit 4: has a snapshot, bit 0: header read */
    u16 unk12;
    u8 channels;
    u8 unk15;
    s16 unk16;         /* instrument set key */
    u16 unk18;
    u8 unk1A;
    u8 unk1B;
    u16 noise_clock;
    u16 unk1E;
    s32 unk20;
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
    s32 unk50;
    s32 tick_step;     /* rate * tempo */
    SoundFixed rate;   /* 16.16 ticks per frame at tempo 1 */
    s32 rate_step;
    s16 rate_frames;
    s16 rate_target;
    SoundFixed tempo;  /* 16.16, 1.0 = 0x100 */
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
typedef struct SoundSeqHeader {
    u8 unk0[0x10];
    u16 unk10;
    u8 unk12[2];
    u8 channels;
    u8 entries;        /* entries of the table at `table` */
    u16 unk16;
    u16 unk18;
    u8 reverb_type;
    u8 reverb_depth;   /* high byte of the depth */
    u8 reverb_delay;
    u8 reverb_feedback;
    u16 unk1E;
    u16 table;         /* offset of 5-byte (index, word) entries */
    u16 channel[1];    /* data offset of each channel (0: unused) */
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
