#ifndef RESIDENT_SOUND_H
#define RESIDENT_SOUND_H

#include "common.h"
#include "psyq/libspu.h"

/* Resident sound driver: SPU voices, channels and loaded sound banks. Field
 * names follow their observed use; unknown bytes keep their offsets. */

/* One SPU voice's registers. */
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

/* The SPU registers (the driver's sound_spu_registers base, 0x1F801C00). */
typedef struct {
    SpuVoice voice[24];
    s16 main_volume[2];
    s16 reverb_volume[2];
    u16 key_on[2];
    u16 key_off[2];
    u16 pitch_mod[2];
    u16 noise[2];
    u16 reverb[2];
} SpuRegs;

/* A voice envelope in parts (SPU ADSR fields). */
typedef struct {
    u8 attack_mode;
    u8 sustain_mode;
    u8 release_mode;
    u8 attack_rate;
    u8 decay_rate;
    u8 sustain_rate;
    u8 release_rate;
    u8 sustain_level;
} SoundEnvelope;

/* A channel's claim on a hardware SPU voice and its staged register
 * values (sound_voice_owners holds the owner of each hardware voice). */
typedef struct SoundChannel {
    u16 voice;         /* hardware voice */
    u16 mode;          /* 0x10 pitch modulation, 0x20 noise, 0x40 reverb */
    s16 priority;
    u16 flags;         /* registers to update: 1 volume, 4 pitch, 8 sample
                        * addresses, 0x10-0x100 envelope parts, 0x1000-0x4000
                        * mode bits */
    s16 volume_left;
    s16 volume_right;
    u16 unkC;
    u16 unkE;
    u8 unk10[4];
    u16 pitch;
    u8 unk16[6];
    u32 sample_start;  /* SPU address */
    u32 sample_loop;
    SoundEnvelope envelope;
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

/* The effect id of a channel: bank id in the high half, effect in the low. */
typedef union {
    s32 full;
    struct {
        u16 effect;
        s16 bank;
    } part;
} SoundEffectId;

/* A per-channel low-frequency modulator (four per channel; the first
 * modulates the pitch). */
typedef struct SoundModulator {
    s32 (*wave)(struct SoundModulator *modulator);
    s32 phase;         /* current output (16.16) */
    s32 slope;
    s32 step;
    u16 count;         /* frames to the next wave segment */
    u16 rate;          /* frames per wave segment */
    u16 delay_count;
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
    u8 *start;         /* initial sequence data */
    u8 *position;      /* current sequence data position */
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
    s16 unk5C;         /* ticks to the next note */
    u16 unk5E;         /* ticks to the key off */
    u8 duration_adjust; /* signed tick bias, accumulated when a note is too short */
    u8 unk61;
    u16 gate_fraction; /* sixteenths; 15 means duration - 1, 16 the full duration */
    u8 previous_note;
    u8 current_note;
    s16 transpose;     /* in semitones */
    Fixed note;   /* 8.8 semitones in the high half */
    s16 unk6C;
    s16 detune;
    u16 unk70;
    u16 loop_depth;    /* innermost entry of `loops`, 0xFFFF when none */
    s16 pan;           /* 0 left, 0x4000 centre, 0x7F00 right */
    s16 volume;
    Fixed level;  /* its whole part scales the volume */
    s32 unk7C;
    s16 unk80;
    s16 unk82;
    s32 unk84;
    s32 unk88;
    s16 volume_step;
    s16 volume_target;
    s16 pan_step;
    s16 pan_target;
    u16 unk94;         /* frames of the note slide */
    u16 unk96;         /* frames of the level slide */
    u16 pan_frames;
    u16 volume_frames;
    SoundLoop loops[4];
    u16 modulator_index; /* modulator the generic opcodes address */
    u16 modulators;    /* mask of the running modulators */
    s16 pitch_mod;     /* modulator outputs */
    s16 level_mod;
    s16 pan_mod;
    u8 unkD6[2];
    SoundModulator modulator[4];
} SoundSeqChannel;

/* A linear slide of a 16.16 value. */
typedef struct {
    Fixed value;
    s32 step;
    s16 frames;
    s16 target;
} SoundSlide;

/* The driver's SPU common attributes and the volumes they are built from
 * (sound_volumes). */
typedef struct SoundVolumes {
    SpuCommonAttr attr;
    s16 master;
    s16 cd;
    s16 unk2C;
    s16 cd_request;
    SoundSlide master_slide; /* stepped every other tick */
    SoundSlide cd_slide;
} SoundVolumes;

extern SoundVolumes sound_volumes;

/* The reverb settings (sound_reverb_settings): type (0xff none), delay and feedback;
 * nothing addresses the first byte. */
typedef struct SoundReverb {
    u8 unk0;
    u8 type;
    u8 delay;
    u8 feedback;
} SoundReverb;

extern SoundReverb sound_reverb_settings;

/* A sequence being played: header, then its channels. Sequences are
 * listed through `next` (sound_playing_seq_list). */
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
    u32 unk24;
    u32 ticks;
    u32 unk2C;
    u16 unk30;
    u16 unk32;
    u16 unk34;
    u16 unk36;
    u16 unk38;
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
    Fixed rate;   /* 16.16 ticks per frame at tempo 1 */
    s32 rate_step;
    u16 rate_frames;
    u16 rate_target;
    Fixed tempo;  /* 16.16, 1.0 = 0x100 */
    s32 tempo_step;
    s16 tempo_frames;
    s16 tempo_target;
    Fixed fade;   /* 8.24 level scaling every voice */
    s32 fade_step;
    s16 fade_frames;
    s16 fade_target;
    Fixed pitch;  /* 8.24 semitones added to every voice */
    s32 pitch_step;
    s16 pitch_frames;
    s16 pitch_target;
    Fixed pan;    /* 8.24 added to every voice's pan */
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

/* An instrument of a wave bank (16 bytes). */
typedef struct {
    u32 start;         /* sample start, 8-byte units from the bank */
    u16 loop;          /* loop start, 8-byte units from the sample */
    s16 note;          /* note offset */
    u32 envelope;      /* rates and sustain level */
    u16 modes;         /* envelope modes */
    u8 unkE[2];
} SoundInstrument;

/* A loaded wave bank (list through `next`): a copy of the bank file's
 * header, whose samples were transferred to SPU memory at `address`. */
typedef struct SoundSequence {
    u8 unk0[0x10];
    s32 header_size;   /* bytes of this header, instruments included */
    s32 size;          /* sample bytes */
    s32 offset;        /* file offset of the samples */
    u8 unk1C[2];
    u16 volume;
    u16 key;
    u8 unk22[6];
    s32 address;       /* SPU address of the samples (in 8-byte units) */
    struct SoundSequence *next;
    SoundInstrument instrument[1];
} SoundSequence;

/* The header of a block of the driver's memory pool (80038EC0); the block's
 * data follows it. */
typedef struct SoundBlock {
    u16 flags;         /* 0x8000: the pool head; 2: allocated */
    u16 unk2;
    u32 unk4;
    u32 end;           /* end of the block's data */
    struct SoundBlock *next;
} SoundBlock;

/* An entry of the SPU memory map (12 entries, chained by index from the
 * first). */
typedef struct SpuMemBlock {
    u8 flags;          /* 0: unused */
    u8 unk1;
    s16 next;          /* index of the next entry, 0 at the end */
    u32 address;       /* SPU address */
    u32 size;
    u32 unkC;
} SpuMemBlock;

extern SpuMemBlock sound_spu_memory_map[12];

u32 sound_free_spu_memory(u32 address);                        /* release SPU memory */
SpuMemBlock *sound_find_spu_block(u32 address);

extern SoundBlock *sound_memory_pool_head;        /* the pool head */
extern u32 sound_memory_pool_end;                /* end of the pool */

extern SpuRegs *sound_spu_registers;           /* SPU registers */
extern s16 sound_driver_flags;                /* driver state flags */
extern s32 sound_channels_per_effect;
extern s32 sound_effect_channel_count;                /* voice count of the effect channels */
extern s32 sound_effect_voice_count;                /* voices kept for music */
extern s32 sound_tick_event;                /* driver event */
extern s16 sound_unread_last_error;                /* last driver error */
extern SoundChannel *sound_voice_owners[24];  /* channel of each voice */
extern SoundBank *sound_effect_bank_list;         /* loaded banks */
extern SoundSequence *sound_wave_bank_list;     /* loaded wave banks */

/* Driver interface (0x80037e8c-0x8003f738). */
SoundSequence *sound_load_wave_bank(SoundSequence *bank, s32 mode);
s32 sound_alloc_wave_bank_spu_memory(SoundSequence *bank, s32 mode);
void *sound_alloc_memory_high(s32 size);                         /* allocate driver memory */
void sound_free_memory(void *data);                        /* release driver memory */
void sound_copy_memory(void *dst, void *src, s32 size);    /* copy */
void sound_clear_memory(void *data, s32 size);             /* clear */
void sound_release_wave_bank(SoundSequence *bank); /* release a wave bank */
void sound_play_effect(s32 sound);
void sound_clear_reverb_work_part(void);
void sound_set_stereo_volume(s32 volume, SpuVolume *out, s32 channel);
void *sound_alloc_memory_low(s32 size);
s32 sound_alloc_spu_memory(s32 size, u16 mode);                 /* allocate SPU memory */
s32 sound_alloc_spu_memory_at(s32 size, s32 address, u16 mode);    /* allocate SPU memory at */
SoundSeq *sound_create_seq(SoundSeqHeader *header);
SoundSeq *sound_create_seq_in_place(SoundSeqHeader *header, SoundSeq *seq);
void sound_release_seq(SoundSeq *seq);  /* release a sequence */
void sound_play_seq(SoundSeq *seq, s32 fade, s32 frames); /* play from the start */
void sound_stop_seq(SoundSeq *seq);  /* stop a sequence */
void sound_stop_all_seqs(void);
void sound_stop_all_effects(void);
u32 sound_find_effect_channels(s32 id, s32 width);
void sound_set_seq_fade(SoundSeq *seq, s32 fade, s32 frames);
void sound_release_seq_voices(SoundSeq *seq);
void sound_load_seq_table(SoundSeq *seq, SoundSeqHeader *header); /* take a snapshot */
void sound_read_seq_header(SoundSeq *seq);
void sound_start_seq_channels(SoundSeq *seq);
void sound_free_seq_snapshots(SoundSeq *seq);
void sound_link_seq(SoundSeq *seq);
s32 sound_unlink_seq(SoundSeq *seq);
s32 sound_get_seq_size(s32 channels);   /* size of a sequence with `channels` */
s16 sound_check_seq_header(SoundSeqHeader *header); /* error code of sequence data, 0 when valid */
s32 sound_check_file(u32 *data, u32 magic, s32 id); /* check a sound file */
void sound_stop_bank_effects(SoundBank *bank);
void sound_start_effect(s16 id, s32 channel, s16 volume, s16 pan);
void sound_queue_transfer(u32 address, u8 *data, s32 size, void (*callback)(void), u16 type);
void sound_request_seq_channel_updates(s32 bits, SoundSeq *seq);
void sound_release_voice(SoundChannel *state, u32 voice);
void sound_write_key_off(u32 voices);   /* key off */
void sound_write_voice_release(s32 voice, s32 rate, s32 mode); /* set a voice's release */
void sound_report_error(s32 error);
void sound_queue_spu_write(u32 address, u8 *data, s32 size, void (*callback)(void)); /* SPU transfer */

/* More of the sound services and their state. */
SoundSequence *sound_start_wave_bank_stream(SoundSequence *bank, s32 size, s32 mode);
s32 sound_transfer_wave_bank_part(u8 *data, s32 size);
void sound_add_effect_bank(SoundBank *bank);
void sound_remove_effect_bank(SoundBank *bank);
void sound_set_output_mode(s32 mode);
s32 sound_get_output_mode(void);
void sound_set_cd_volume(s32 volume, s32 frames);
void sound_restart_seq(SoundSeq *seq, s32 fade, s32 frames);
void sound_play_effect_on_channels_12_13(s32 channel);
void sound_play_effect_on_channel(s32 channel, s32 sound);
void sound_stop_effect(s32 id);
void sound_stop_effect_on_channel(s32 sound);
void sound_set_effect_volume_on_channel(s32 sound, s32 volume);
void sound_slide_effect_volume(s32 id, s32 volume, s32 frames);
void sound_slide_effect_volume_on_channel(s32 sound, s32 volume, s32 frames);
void sound_set_effect_pan_on_channel(s32 sound, s32 pan);
s32 sound_get_active_effect_mask(s32 id);
void sound_set_seq_tempo(SoundSeq *seq, s32 tempo, s32 frames);
void sound_set_seq_pitch(SoundSeq *seq, s32 pitch, s32 frames);
void sound_set_seq_pan(SoundSeq *seq, s32 pan, s32 frames);
void sound_set_seq_mute_mask(SoundSeq *seq, u32 mask);
extern u16 mode_battle_camera_range;
extern u8 mode_battle_kind;
extern s32 mode_snapshot_party_in_gear[3];
extern u8 *mode_battle_scene_data; /* the battle scene data (mode_load_battle_stage, ovl2615 func_801E7210) */
extern struct SoundBank *mode_battle_effect_bank;

extern struct SoundBank *sound_effect_bank; /* the effect sound bank of the field, the world map and the menus */

#endif
