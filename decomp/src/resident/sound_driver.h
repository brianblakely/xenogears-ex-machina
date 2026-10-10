#ifndef RESIDENT_SOUND_DRIVER_H
#define RESIDENT_SOUND_DRIVER_H

#include "common.h"
#include "psyq/libspu.h"
#include "resident/sound.h"

/* The sound driver's own state and calls between its two units, the end of
 * console_and_sound_driver.c (80037b88-80039e18: start-up, banks, SPU memory, volumes)
 * and sound.c (80039e18-8003f738). No other unit or target uses them;
 * resident/sound.h has the driver's interface. */

/* A voice whose volume pair follows the output mode (sound_output_mode_voice). */
typedef struct SoundModeVoice {
    u16 flags;         /* bit 0: in use */
    u8 unk2[0x10];
    u16 volume;
    u8 unk14[0x22];
    s16 unk36;
    s16 left;
    s16 right;
    u8 unk3C[0x26];
    s16 unk62;
    s16 unk64;
    s16 unk66;
} SoundModeVoice;

/* One queued SPU transfer (the ring sound_transfer_ring holds eight). */
typedef struct SoundTransfer {
    u16 type;          /* 1: write, 2: read, 3/4: read decoded CD data */
    u16 unk2;
    u8 *data;
    u32 address;       /* SPU address */
    s32 size;
    void (*callback)(void);
} SoundTransfer;

/* The driver state both units use: commons, and sound.c's reverb sizes. */
extern SoundSeq *sound_playing_seq_list;      /* playing sequences */
extern SoundSeq *sound_effect_channels;      /* the sound effect channels */
extern u32 sound_tick_count;            /* the driver's tick count, the effects' start clock */
extern u32 sound_pending_key_on_mask;            /* voices held (keyed on) */
extern u32 sound_pending_key_off_mask;            /* voices to key off */
extern u32 sound_changed_voice_mask;            /* voices whose registers changed */
extern s32 sound_unread_spu_irq_count;            /* SPU interrupts counted */
extern void (*sound_spu_irq_hook)(void);  /* the SPU interrupt hook (8003c010) */
extern s32 sound_unread_tick_time_total;            /* root counter time spent in ticks */
extern s32 sound_unread_timed_tick_count;            /* timed ticks */
extern u16 sound_pending_irq_enable;            /* pending SPU IRQ re-enable */
extern s32 sound_random_state;            /* random state */
extern SoundTransfer *sound_transfer_ring; /* the SPU transfer ring */
extern u16 sound_transfer_ring_write_index;            /* transfer ring write index */
extern u16 sound_transfer_ring_read_index;            /* transfer ring read index */
extern s16 sound_unread_decoded_read_result;            /* result of the last decoded-data read */
extern s32 sound_reverb_clear_buffer;            /* the zeroed transfer buffer */
extern s32 sound_reverb_clear_address;            /* next SPU address to clear */
extern s32 sound_reverb_clear_bytes_left;            /* bytes left to clear */
extern s32 sound_wave_bank_stream_address;            /* SPU address of a streamed wave bank's next part */
extern s32 sound_wave_bank_stream_bytes_left;            /* bytes of it still missing */
extern u32 sound_reverb_work_address;            /* SPU address of the reverb work area, -1 none */
extern SpuVolume sound_reverb_depth;      /* reverb depth */
extern s32 sound_reverb_work_area_sizes[10];        /* reverb work area size of each reverb type */
extern SoundModeVoice *sound_output_mode_voice;
extern s32 sound_unread_memory_pool_size;            /* size of the driver memory pool */
extern u8 sound_memory_pool[0x6300];     /* the driver memory pool */
extern u8 sound_spu_malloc_table[0x28];       /* the SPU memory management table (SpuInitMalloc, 4 blocks) */

/* Calls between the two sound units. */
void sound_set_reverb(s32 type, s32 depth, s32 delay, s32 feedback); /* set the reverb */
s32 sound_run_tick(void);           /* the driver tick (root counter 2 event) */
void sound_complete_transfer(void);          /* SPU transfer callback */
void sound_dispatch_spu_irq(void);          /* SPU interrupt callback */
void sound_clear_voice_owners(void);
SoundSeq *sound_create_effect_channels(s32 count); /* create the sound effect channels */

#endif
