#ifndef FIELD_FIELD_SOUND_H
#define FIELD_FIELD_SOUND_H

/* Field music and sound effects (800854d0-80086d8c, field_event.c): the
 * music-wave stream and the shared wave bank, the field's sound-effect bank,
 * sound effects and the three positional emitters. */

#include "common.h"
#include "psyq/libgte.h"

/* One 2 KiB music-wave stream chunk. */
typedef struct {
    u32 words[0x200];
} WaveChunk;

/* The music-wave stream: an eight-sector ring whose arrivals go to a chunk
 * callback (800859dc gathers four chunks into a wave bank). */
extern s32 field_music_stream_running;           /* the stream is running */
extern void *field_music_stream_ring;            /* music-wave stream ring */
extern s32 field_music_stream_arrival_count;     /* stream arrivals */
extern void (*field_music_stream_callback)(s32); /* stream chunk callback */
extern void *field_music_gather_buffer;          /* music-wave gather buffer */
extern s32 field_music_chunk_count;              /* music-wave chunks gathered */
extern s32 field_music_seq_read_pending;         /* the music's sequence read is still to start */
extern void *field_music_shared_wave_bank;       /* shared wave bank buffer */

s32 field_music_step_stream(void);                                          /* one stream step; -1 once finished */
void field_music_start_stream(s32 file, s32 unused, void (*callback)(s32)); /* start a stream */
void field_music_gather_wave_chunk(WaveChunk *chunk);                       /* the music-wave chunk callback */
void field_music_change_track(s32 music, s32 unused);                       /* change the field music */
s32 field_music_advance_track_load(s32 music);                              /* advance its load; 0 once complete */
void field_music_release_cached_seq(void);                                  /* stop and release the cached sequence */
s32 field_music_open_shared_wave_bank(void);                                /* open the shared wave bank once read */
void field_music_read_shared_wave_bank(void);                               /* start reading the shared wave bank */
void field_music_release_shared_wave_bank(void);                            /* release the shared wave bank */

s32 field_music_is_stream_or_disc_busy(void);                 /* stop the stream once idle; -1 while busy */
void field_music_wait_stream_and_disc_idle(void);             /* wait until the disc and the stream are idle */

/* Sound effects. */
void field_sound_load_effect_bank();                    /* load the field's bank; called with an argument it ignores */
/* Instruction 0xb0 loads a wave bank into a resident slot (mode_wave_bank_slots). */
extern void *field_sound_bank_load_buffer;                 /* bank file being loaded */
extern s32 field_sound_bank_load_file;                     /* bank file number */
extern s32 field_sound_bank_load_slot;                     /* bank slot being loaded */
void field_sound_release_effect_bank(void);                /* release the field's bank */
void field_sound_play_effect_volume_pan(s32 id, s32 volume, s32 pan, s32 channel);
void field_sound_play_effect(s32 id, s32 channel);         /* at full volume and centre pan; 0 stops */

/* The three positional emitters (800afe88): each follows a descriptor, with
 * a volume by distance and a pan by screen x. */
typedef struct EmitterSlot {
    u16 actor;  /* descriptor the sound follows */
    u16 sound;  /* 0xffff when free */
    u16 unk4;
} EmitterSlot;

extern EmitterSlot field_sound_emitter_slots[3];

void field_sound_start_emitter(s32 sound, s32 volume, s32 unused, s32 distance, s32 actor); /* start one */
void field_sound_stop_emitter(s32 id);                                                      /* stop the one following descriptor `id` */
void field_sound_clear_emitter_slots(void);                                                 /* clear the slots */
void field_sound_clear_emitters_stop_voices(void);                                          /* clear them and stop their voices */
void field_sound_keep_nearest_emitters(VECTOR *target);                                     /* keep those of the three actors nearest `target` */
void field_sound_update_emitters(void);                                                     /* point the listener */
void field_layer_update_emitter_lights(void);                                               /* follow the actors' positions */
void field_layer_set_emitter_light_color(s32 emitter, s32 *position);
void field_sound_compute_emitter_volume(s32 distance, u32 *out, s32 volume);                /* volume at `distance` */
void field_sound_get_descriptor_screen_xy(s32 index, s32 *x, s32 *y);                       /* screen position of descriptor `index` */

#endif
