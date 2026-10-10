/* Field unit 800854D0 onward: music and sound effects, the event
 * interpreter's operations, event actors' motion, screen effects, movies,
 * texture panels, particles and the text glyphs.
 *
 * Its rodata starts at 0x198 with 8008e59c's jump table (0 mod 8); the text
 * start (after 80084a40, at or before 8008e59c) is chosen with the previous
 * unit (see field_motion.c). The tables stay 0 mod 8 through 800a1bd0's
 * (0x268); 800a5c40's (0x2bc) is 4 mod 8 and 800ab748's (0x2e8) 0 mod 8
 * again, so further units start after 800a1bd0 (at or before 800a5c40) and
 * after 800a5c40 (at or before 800ab748). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/stream.h"
#include "resident/text.h"
#include "field/monitor.h"
#include "field.h"
#include "field_camera.h"
#include "field_dialogue.h"
#include "field_draw.h"
#include "field_effect.h"
#include "field_event.h"
#include "field_layer.h"
#include "field_load.h"
#include "field_mode.h"
#include "field_motion.h"
#include "field_movie.h"
#include "field_music.h"
#include "field_pad.h"
#include "field_panel.h"
#include "field_party.h"
#include "field_resident.h"
#include "field_screen.h"
#include "field_sound.h"

/* The movie sound timelines, user-supplied cue data (an asset in
 * field.classification.txt): u16 (frame, sound) pairs, one run per movie
 * sound-effect bank after a leading end, each ended by frame 0xffff.
 * 80085788 seeks past bank + 1 ends and 80085678 plays the run in order; a
 * sound holds its effect in the low byte and its voice pair in bits 8-10
 * (tools/analysis/overlay_scripts.py decodes them). */
INCLUDE_ASSET(".data", field_movie_sound_timelines, 0x800AE060, 0x180);

/* Portrait files per character (- 0x46): first and second image. */
u8 field_dialogue_portrait_files[90][2] = { /* 800AE1E0 */
    {0, 0}, {6, 6}, {17, 17}, {19, 20}, {21, 21}, {23, 23}, {24, 24}, {28, 28},
    {27, 27}, {17, 17}, {25, 25}, {34, 34}, {35, 35}, {36, 36}, {37, 37}, {79, 79},
    {82, 82}, {83, 83}, {26, 26}, {52, 52}, {81, 81}, {77, 77}, {78, 78}, {33, 33},
    {41, 41}, {29, 29}, {43, 43}, {50, 51}, {42, 42}, {53, 53}, {56, 56}, {38, 38},
    {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {7, 7}, {8, 8}, {9, 9},
    {10, 10}, {11, 11}, {12, 12}, {13, 13}, {14, 14}, {15, 15}, {16, 16}, {18, 18},
    {30, 30}, {31, 31}, {32, 32}, {39, 39}, {40, 40}, {44, 44}, {45, 45}, {46, 46},
    {47, 47}, {48, 48}, {49, 49}, {54, 54}, {22, 22}, {57, 57}, {58, 58}, {59, 59},
    {60, 60}, {61, 61}, {62, 62}, {63, 63}, {64, 64}, {65, 65}, {66, 66}, {67, 67},
    {68, 68}, {69, 69}, {70, 70}, {71, 71}, {72, 72}, {73, 73}, {74, 74}, {75, 75},
    {76, 76}, {80, 80}, {55, 55}, {84, 84}, {85, 85}, {86, 86}, {87, 87}, {88, 88},
    {89, 89}, {90, 90},
};

/* The sprite of each character slot. */
u8 field_character_sprite_ids[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 2, 6}; /* 800AE294 */

/* 800854D0: One music-wave stream step: pass arrivals to the chunk callback; -1 once
 * the stream finished and its ring is released. */
s32 field_music_step_stream(void) {
    s32 arrived = (s32)stream_get_next_chunk();

    field_music_stream_arrival_count = arrived;
    if (arrived != 0) {
        field_music_stream_callback(arrived);
        return 0;
    }
    if (cd_get_pending_read_count() != 0) {
        return 0;
    }
    if (field_music_stream_arrival_count != 0) {
        return 0;
    }
    heap_free(field_music_stream_ring);
    field_music_stream_running = 0;
    return -1;
}

/* 80085560: Start streaming music-wave `file` into an eight-sector ring with a chunk
 * callback. */
void field_music_start_stream(s32 file, s32 unused, void (*callback)(s32)) {
    void *ring;

    field_music_stream_running = 1;
    field_music_stream_ring = ring = stream_create_ring(8, unused);
    cd_read_file(file, ring, 0, 0x100);
    field_music_stream_callback = callback;
}

/* 800855C8: Play sound effect `id` on voice pair `channel` at a volume and pan. */
void field_sound_play_effect_volume_pan(s32 id, s32 volume, s32 pan, s32 channel) {
    channel &= 7;
    sound_stop_effect_on_channel(channel * 2);
    sound_play_effect_on_channel_volume_pan(id, channel * 2, volume, pan);
}

/* 80085634: Play sound effect `id` on `channel` at full volume and centre pan; id 0
 * stops the channel. */
void field_sound_play_effect(s32 id, s32 channel) {
    channel &= 7;
    if (id == 0) {
        sound_stop_effect_on_channel(channel * 2);
    } else {
        field_work.last_sound_effect = id;
        field_sound_play_effect_volume_pan(id, 0x7F, 0x40, channel);
    }
}

/* 80085678: Play the movie sound effects whose time (from 800c3a2c) has come: each
 * timeline entry holds a time and a sound (id in the low byte, voice pair
 * in bits 8-10). */
void field_movie_play_due_sounds(void) {
    u16 *times;
    u16 *sounds;
    s32 sound;

    if (FIELD_MOVIE.sound_bank == 0xFF) {
        return;
    }
    times = &field_movie_sound_timelines[0][0];
    sounds = &field_movie_sound_timelines[0][1];
    for (;;) {
        if (field_movie_frame < times[field_movie_sound_timeline_index * 2] + FIELD_MOVIE.sound_start) {
            return;
        }
        sound = sounds[field_movie_sound_timeline_index * 2];
        sound_play_effect_on_channel((sound & 0xFF) | (field_movie_sound_bank->id << 16), ((sound >> 8) & 7) * 2);
        field_movie_sound_timeline_index++;
    }
}

/* 80085738: Release a movie's sound-effect bank, when one is loaded. */
void field_movie_release_sound_bank(void) {
    if (FIELD_MOVIE.sound_bank != 0xFF) {
        sound_stop_all_effects();
        sound_remove_effect_bank(field_movie_sound_bank);
        heap_free(field_movie_sound_bank);
    }
}

/* 80085788: Load a movie's sound-effect bank (file 0x115 + bank) and seek the movie
 * sound timeline past the bank's 0xffff-terminated runs.
 * The value is read into `file` and copied to `bank` (the original keeps
 * the loaded copy in s0 for the file number), and the timeline is indexed
 * as a flat halfword table, which leaves bank + 1 in the loop test.
 */
void field_movie_load_sound_bank(void) {
    s32 bank;
    s32 file;
    s32 pos;
    s32 i;

    file = FIELD_MOVIE.sound_bank;
    if (file != 0xFF) {
        bank = file;
        sound_stop_all_effects();
        cd_select_directory(0x1C, 0);
        file = bank + 0x115;
        field_movie_sound_bank = heap_alloc(cd_get_aligned_file_size(file), 1);
        cd_read_file(file, field_movie_sound_bank, 0, 0x80);
        cd_sync_reads(0);
        sound_add_effect_bank(field_movie_sound_bank);
        sound_sync_transfer(0x10);
        cd_select_directory(4, 0);
        pos = 0;
        for (i = 0; i < bank + 1; i++) {
            while (1) {
                if (((u16 *)field_movie_sound_timelines)[pos * 2] == 0xFFFF) {
                    break;
                }
                pos++;
            }
            pos++;
            field_movie_sound_timeline_index = pos;
        }
    }
}

/* 80085890: Load the field's sound-effect bank (file 0xa8), from the disc or from the
 * copy at 8005a4bc, and open it. */
void field_sound_load_effect_bank(void) {
    s32 size;

    cd_select_directory(4, 0);
    size = cd_get_aligned_file_size(0xA8);
    sound_effect_bank = heap_alloc(size, 0);
    heap_protect_block(sound_effect_bank);
    if (mode_effect_bank_not_preloaded == -1) {
        cd_read_file(0xA8, sound_effect_bank, 0, 0x80);
        cd_sync_reads(0);
    } else {
        memcpy(sound_effect_bank, mode_preloaded_effect_bank, size);
        heap_unprotect_block(mode_preloaded_effect_bank);
        heap_free(mode_preloaded_effect_bank);
    }
    sound_add_effect_bank(sound_effect_bank);
    sound_sync_transfer(0x10);
    cd_select_directory(4, 0);
    mode_effect_bank_not_preloaded = -1;
}

/* 80085988: Unlink and release the field's sound-effect bank. */
void field_sound_release_effect_bank(void) {
    sound_remove_effect_bank(sound_effect_bank);
    heap_unprotect_block(sound_effect_bank);
    heap_free(sound_effect_bank);
    mode_effect_bank_not_preloaded = -1;
}

/* 800859DC: Music-wave chunk callback: gather four 2 KiB chunks and open them as a
 * wave bank; later chunks feed the bank. */
void field_music_gather_wave_chunk(WaveChunk *chunk) {
    switch (field_music_chunk_count) {
    case 0:
    case 1:
    case 2:
    case 3:
        ((WaveChunk *)field_music_gather_buffer)[field_music_chunk_count] = *chunk;
        field_music_chunk_count++;
        stream_release_chunk((u8 *)chunk);
        if (field_music_chunk_count == 4) {
            mode_music_wave_bank = sound_start_wave_bank_stream(field_music_gather_buffer, 0x2000, 0);
        }
        break;
    case 4:
        sound_sync_transfer(0x10);
        *(WaveChunk *)field_music_gather_buffer = *chunk;
        sound_transfer_wave_bank_part(field_music_gather_buffer, 0x800);
        stream_release_chunk((u8 *)chunk);
        break;
    }
}

/* 80085B20: Change the field music to `music` (0xff: none): release the shared wave
 * bank when the entry asks, then stream its wave file through 800859dc. */
void field_music_change_track(s32 music, s32 unused) {
    u8 wave;
    s32 file;

    cd_sync_reads(0);
    mode_stop_music();
    if (music == 0xFF) {
        mode_music_load_pending = 0;
        return;
    }
    cd_select_directory(0x1C, 0);
    if (field_music_wave_table[music * 2 + 1] == 1) {
        field_music_release_shared_wave_bank();
    }
    wave = field_music_wave_table[music * 2];
    if (wave != 0xFF) {
        file = wave * 2 + 0x13;
        if (mode_music_loaded_wave != wave) {
            field_music_start_stream(file, 1, (void (*)(s32))field_music_gather_wave_chunk);
            mode_music_wave_streaming = 1;
            field_music_chunk_count = 0;
            field_music_gather_buffer = heap_alloc(0x2000, 1);
        }
    }
    cd_select_directory(4, 0);
    mode_music_load_pending = -1;
    field_music_seq_read_pending = 1;
}

/* 80085C3C: Run up to five stream steps; 0 once the stream finished, else -1. */
s32 field_music_run_stream_steps(void) {
    s32 steps;

    for (steps = 0; steps < 5; steps++) {
        if (field_music_step_stream() == -1) {
            return 0;
        }
    }
    return -1;
}

/* 80085C90: Advance the load of music `music`: its wave chunks, the shared wave bank,
 * the deferred sequence read and the sequence start; 0 once complete, else
 * -1. */
s32 field_music_advance_track_load(s32 music) {
    s32 sequence;

    if (mode_music_wave_streaming == 1) {
        if (field_music_run_stream_steps() == -1) {
            return -1;
        }
        sound_sync_transfer(0x10);
        heap_free(field_music_gather_buffer);
        mode_music_wave_streaming = 0;
        mode_music_wave_bank_loaded = 1;
        mode_music_loaded_wave = field_music_wave_table[music * 2];
    }
    if (field_music_wave_table[music * 2 + 1] == 0) {
        if (mode_shared_wave_bank_state == 0) {
            field_music_read_shared_wave_bank();
            return -1;
        }
        if ((mode_shared_wave_bank_state & 0x80) && field_music_open_shared_wave_bank() == -1) {
            return -1;
        }
    }
    if (field_music_seq_read_pending == 1) {
        if (mode_music_loaded_track != music) {
            cd_select_directory(0x1C, 0);
            cd_read_file(music * 2 + 0x14, mode_music_buffer, 0, 0x80);
            mode_music_seq_read_pending = 1;
            cd_select_directory(4, 0);
        }
        field_music_seq_read_pending = 0;
        return -1;
    }
    if (cd_get_pending_read_count() != 0) {
        return -1;
    }
    if (mode_music_seq_read_pending == 1) {
        if (mode_music_reuse_seq == 0) {
            sequence = (s32)sound_create_seq((SoundSeqHeader *)mode_music_buffer);
            mode_music_seq = sequence;
            if (mode_music_start_full_volume == -1) {
                sound_play_seq((SoundSeq *)sequence, 0x7F, 0);
            } else {
                sound_play_seq((SoundSeq *)mode_music_seq, 0, 0);
                sound_set_seq_fade((SoundSeq *)mode_music_seq, 0, 0);
            }
        } else {
            mode_music_seq = mode_music_cached_seq;
            sound_restart_seq((SoundSeq *)mode_music_cached_seq, 0x7F, 0xF0);
            mode_music_reuse_seq = 0;
            mode_music_cached_seq = 0;
        }
        mode_music_seq_read_pending = 0;
        mode_music_seq_active = 1;
        mode_music_loaded_track = music;
    }
    mode_music_start_full_volume = -1;
    mode_music_started = 1;
    return 0;
}

/* 80085EEC: Stop and release the cached sequence. */
void field_music_release_cached_seq(void) {
    if (mode_music_cached_seq != 0) {
        sound_stop_seq((SoundSeq *)mode_music_cached_seq);
        sound_release_seq((SoundSeq *)mode_music_cached_seq);
        mode_music_cached_seq = 0;
    }
}

/* 80085F30: Once the disc is idle, open the shared wave bank from its buffer and
 * release the buffer; -1 while still reading. */
s32 field_music_open_shared_wave_bank(void) {
    s32 bank;

    if (cd_get_pending_read_count() != 0) {
        return -1;
    }
    bank = (s32)sound_load_wave_bank(field_music_shared_wave_bank, 0);
    mode_wave_bank_slots[1] = bank;
    mode_shared_wave_bank = (SoundSequence *)bank;
    sound_sync_transfer(0x10);
    heap_free(field_music_shared_wave_bank);
    mode_shared_wave_bank_state = 1;
    mode_shared_wave_bank_needs_reload = 0;
    mode_shared_wave_bank_released = 0;
    return 0;
}

/* 80085FB8: Start reading the shared wave bank (file 3 of directory 0x1c). */
void field_music_read_shared_wave_bank(void) {
    void *buffer;

    cd_select_directory(0x1C, 0);
    field_music_shared_wave_bank = buffer = heap_alloc(cd_get_aligned_file_size(3), 1);
    cd_read_file(3, buffer, 0, 0x80);
    cd_select_directory(4, 0);
    mode_shared_wave_bank_state = 0x80;
}

/* 80086024: Release the shared wave bank once and mark it unloaded. */
void field_music_release_shared_wave_bank(void) {
    if (mode_shared_wave_bank_released == 0) {
        mode_shared_wave_bank_needs_reload = 1;
        sound_release_wave_bank((SoundSequence *)mode_wave_bank_slots[1]);
        mode_shared_wave_bank_released = 1;
    }
    mode_shared_wave_bank_state = 0;
}

/* 80086078: A positional emitter's volume at `distance`: full at the source, falling
 * linearly to half at the range 800b21ac. */
void field_sound_compute_emitter_volume(s32 distance, u32 *out, s32 volume) {
    s32 level;

    if (distance > field_work.emitter_range) {
        distance = field_work.emitter_range;
    }
    level = 0x80 - (((0x7F0000 / field_work.emitter_range) * distance) >> 16);
    *out = ((u32)(level << 16) / 127 * volume) >> 16;
}

/* 800860F0: Update the voices of the emitter following descriptor `id`: volume by
 * distance, pan by the descriptor's screen X. */
void field_sound_update_emitter_voices(s32 unused0, s32 volume, s32 unused2, s32 distance, s32 id) {
    s32 i;
    s32 voice;
    s32 pan;
    u32 level;
    s32 x;
    s32 y;

    for (i = 0; i < 3; i++) {
        if (field_sound_emitter_slots[i].actor == id) {
            voice = i * 2;
            field_sound_compute_emitter_volume(distance, &level, volume);
            field_sound_get_descriptor_screen_xy(id, &x, &y);
            if (x > 0x140) {
                x = 0x13F;
            }
            if (x < 0) {
                x = 0;
            }
            pan = (x * 0x6666) >> 16;
            sound_set_effect_volume_on_channel(voice, level);
            sound_set_effect_pan_on_channel(voice, pan);
        }
    }
}

/* 80086200: The screen position of descriptor `index`. */
void field_sound_get_descriptor_screen_xy(s32 index, s32 *x, s32 *y) {
    SVECTOR point;
    MATRIX m;
    long screen;
    long depth;
    long flag;

    /* The selector call (result unused) sits inside the argument list. */
    CompMatrix(&field_view.scaled_world, (field_event_read_actor_index(1), &field_view.components.descriptors[index].matrix),
               &m);
    point.vx = 0;
    point.vy = 0;
    point.vz = 0;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    RotTransPers(&point, &screen, &depth, &flag);
    *y = screen >> 16;
    *x = (s16)screen;
}

/* 800862CC: Start `sound` on the first free emitter, following descriptor `actor`,
 * with volume by distance and pan by screen X. */
void field_sound_start_emitter(s32 sound, s32 volume, s32 unused, s32 distance, s32 actor) {
    s32 i;
    u32 level;
    s32 x;
    s32 y;
    s32 pan;

    for (i = 0; i < 3; i++) {
        if (field_sound_emitter_slots[i].sound == 0xFFFF) {
            field_sound_emitter_slots[i].sound = sound;
            field_sound_emitter_slots[i].actor = actor;
            field_sound_compute_emitter_volume(distance, &level, volume);
            field_sound_get_descriptor_screen_xy(actor, &x, &y);
            if (x > 0x140) {
                x = 0x13F;
            }
            if (x < 0) {
                x = 0;
            }
            pan = (x * 0x6666) >> 16;
            sound_stop_effect_on_channel(i * 2);
            sound_play_effect_on_channel_volume_pan(sound, i * 2, level, pan);
            return;
        }
    }
}

/* 800863E8: Stop the emitter following descriptor `id`, freeing its slot. */
void field_sound_stop_emitter(s32 id) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (field_sound_emitter_slots[i].actor == id) {
            sound_stop_effect_on_channel(i * 2);
            field_sound_emitter_slots[i].sound = 0xFFFF;
            field_sound_emitter_slots[i].actor = 0xFFFF;
            return;
        }
    }
}

/* 80086470: The emitter slot following descriptor `id`, or -1. */
s32 field_sound_find_emitter_slot(s32 owner, s32 id) {
    s32 i;

    if (owner == -1) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (field_sound_emitter_slots[i].actor == id) {
            return i;
        }
    }
    return -1;
}

/* 800864B4: Clear the emitter slots. */
void field_sound_clear_emitter_slots(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        field_sound_emitter_slots[i].actor = 0xFFFF;
        field_sound_emitter_slots[i].sound = 0xFFFF;
    }
}

/* 800864F0: Clear the emitter slots and stop the voices of the emitters in use. */
void field_sound_clear_emitters_stop_voices(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        field_sound_emitter_slots[i].sound = 0xFFFF;
        field_sound_emitter_slots[i].actor = 0xFFFF;
    }
    for (i = 0; i < 4; i++) {
        if (!(field_work.effects_kept & 1)) {
            sound_stop_effect_on_channel(i * 2);
        }
        field_work.effects_kept >>= 1;
    }
}

/* 80086590: Keep the sound emitters of the three actors nearest `listener` (actor
 * +10d not ff): update those already playing, start the others and stop
 * table entries no longer among them. */
void field_sound_keep_nearest_emitters(VECTOR *listener) {
    struct {
        s32 distance[4];
        s32 sound[4];
        s32 volume[4];
        s32 matched[4];
        s32 kept[4];
        s32 actor[4];
        SVECTOR offset[3];
    } near;
    SVECTOR *offset;
    FieldActor *emitter;
    s32 length;
    s32 far;
    s32 slot;
    s32 i;

    for (i = 0; i < 3; i++) {
        near.distance[i] = 0xFFFF;
        near.sound[i] = -1;
        near.kept[i] = 0;
        near.matched[i] = 0;
        near.actor[i] = 0;
        near.volume[i] = 0;
    }
    for (i = 0; i < field_event_actor_count; i++) {
        emitter = field_view.components.descriptors[i].actor;
        if (emitter->sound_mode != 0xFF) {
            length = field_compute_vector_length((listener->vx >> 16) - (emitter->position[0] >> 16),
                                   (listener->vy >> 16) - (emitter->position[1] >> 16),
                                   (listener->vz >> 16) - (emitter->position[2] >> 16));
            if (near.distance[0] < near.distance[1]) {
                far = 1;
                if (near.distance[1] < near.distance[2]) {
                    far = 2;
                }
            } else {
                far = (near.distance[0] < near.distance[2]) * 2;
            }
            if (length < near.distance[far]) {
                near.actor[far] = i;
                near.distance[far] = length;
                near.sound[far] = field_view.components.descriptors[i].actor->sound;
                near.volume[far] = field_view.components.descriptors[i].actor->sound_volume;
                (near.offset + far)->vx = (listener->vx >> 16) - (field_view.components.descriptors[i].actor->position[0] >> 16);
                (near.offset + far)->vy = (listener->vy >> 16) - (field_view.components.descriptors[i].actor->position[1] >> 16);
                (near.offset + far)->vz = (listener->vz >> 16) - (field_view.components.descriptors[i].actor->position[2] >> 16);
            }
        } else {
            emitter->sound_mode = 0xFF;
        }
    }
    for (i = 0; i < 3; i++) {
        slot = field_sound_find_emitter_slot(near.sound[i], near.actor[i]);
        if (slot != -1) {
            near.matched[slot] = 1;
            near.kept[i] = 1;
        }
    }
    for (i = 0; i < 3; i++) {
        if (near.matched[i] == 0 && field_sound_emitter_slots[i].sound != 0xFFFF) {
            sound_stop_effect_on_channel(i * 2);
            field_sound_emitter_slots[i].sound = 0xFFFF;
            field_sound_emitter_slots[i].actor = 0xFFFF;
        }
    }
    for (i = 0, offset = near.offset; i < 3; offset++, i++) {
        if (near.sound[i] != -1) {
            if (near.kept[i] == 1) {
                field_sound_update_emitter_voices(near.sound[i], near.volume[i], offset->vx, near.distance[i], near.actor[i]);
            } else {
                field_sound_start_emitter(near.sound[i], near.volume[i], offset->vx, near.distance[i], near.actor[i]);
            }
        }
    }
}

/* 80086908: Point the listener (80086590) at the controlled actor, the camera eye or
 * the camera target, as 800b22e0 selects. */
void field_sound_update_emitters(void) {
    switch (field_work.unk22E0) {
    case 0:
        field_sound_keep_nearest_emitters((VECTOR *)field_view.components.descriptors[field_work.controlled].actor->position);
        break;
    case 1:
        field_sound_keep_nearest_emitters(&field_view.eye);
        break;
    case 2:
        field_sound_keep_nearest_emitters(&field_view.target);
        break;
    }
}

/* 800869B8: Event opcode fe: advance the pc to the next byte and run the extended
 * handler it names (800ae6a0); those read operands relative to that byte,
 * and one that does not advance leaves the pc there, so the byte then runs
 * as a primary opcode. */
void field_event_run_extended_opcode(void) {
    field_event_extended_handlers[field_event_bytecode[++field_current_event_actor->pc]]();
}

/* 80086A1C: Scale emitter `emitter`'s per-step delta ((22e8 - 2300) / 2318) by the
 * steps left once its distance from `position` is covered, into 223c. */
void field_layer_set_emitter_light_color(s32 emitter, s32 *position) {
    s32 dx;
    s32 dy;
    s32 dz;
    s32 left;

    dx = ((field_work.unk22E8[emitter][0] - field_work.unk2300[emitter][0]) << 16) / field_work.unk2318[emitter];
    dy = ((field_work.unk22E8[emitter][1] - field_work.unk2300[emitter][1]) << 16) / field_work.unk2318[emitter];
    dz = ((field_work.unk22E8[emitter][2] - field_work.unk2300[emitter][2]) << 16) / field_work.unk2318[emitter];
    left = field_work.unk2318[emitter] - field_compute_vector_length(field_work.emitter_position[emitter][0] - WHOLE(position[0]),
                                                        field_work.emitter_position[emitter][1] - WHOLE(position[1]),
                                                        field_work.emitter_position[emitter][2] - WHOLE(position[2]));
    field_work.unk223C[0][emitter] = (dx * left) >> 16;
    field_work.unk223C[1][emitter] = (dy * left) >> 16;
    field_work.unk223C[2][emitter] = (dz * left) >> 16;
}

/* 80086BA8: Update the three positional emitters from their actors' positions. */
void field_layer_update_emitter_lights(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (field_work.emitter_descriptor[i] != -1) {
            field_layer_set_emitter_light_color(i, field_view.components.descriptors[field_work.emitter_descriptor[i]].actor->position);
        }
    }
}

/* 80086C34: Particle emitters by selector byte 1: 0 continues at +2; 1 resets the eight
 * template emitters for the current actor (800a94a4), sets up template 0 (16
 * particles, +74 = 0x20 when operand 4 is 0x27, else 0x22), starts an effect
 * from them (800a99a8) and continues at +8 (operands 2 and 6 are read but
 * unused). Other selectors never advance. */
void field_event_emitter(void) {
    s32 kind;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        field_current_event_actor->pc += 2;
        break;
    case 1:
        field_event_read_imm_or_var(2);
        kind = field_event_read_imm_or_var(4);
        field_event_read_imm_or_var(6);
        field_effect_reset_templates(field_current_event_actor_index);
        field_effect_templates[0].flags = 0x14;
        field_effect_templates[0].unk00 = 1;
        field_effect_templates[0].count = 0x10;
        field_effect_templates[0].unk72 = 0;
        field_effect_templates[0].unk74 = kind;
        if (kind == 0x27) {
            field_effect_templates[0].unk74 = 0x20;
        } else {
            field_effect_templates[0].unk74 = 0x22;
        }
        field_effect_templates[0].unk04 = 0x1000;
        field_effect_start(field_current_event_actor_index);
        field_current_event_actor->pc += 8;
        break;
    }
}

/* 80086D4C: Event opcode e2: resident 80019cd0 shuts the libraries down and restarts from
 * the entry point; then yield and advance one byte. */
void field_event_soft_reset(void) {
    boot_restart();
    field_event_yield_requested = 1;
    field_current_event_actor->pc++;
}

/* 80086D8C: Set both draw blocks' display screen rectangles (0, 10, 256, 216). */
void field_draw_set_display_areas(void) {
    field_draw_blocks[0].disp.screen.x = 0;
    field_draw_blocks[0].disp.screen.y = 10;
    field_draw_blocks[0].disp.screen.w = 0x100;
    field_draw_blocks[0].disp.screen.h = 0xD8;
    field_draw_blocks[1].disp.screen.x = 0;
    field_draw_blocks[1].disp.screen.y = 10;
    field_draw_blocks[1].disp.screen.w = 0x100;
    field_draw_blocks[1].disp.screen.h = 0xD8;
}

/* 80086DE0: Event opcode e0: set 800b2358 from byte 1: nonzero disables the field loop's
 * start-button pause. */
void field_event_set_pause_disabled(void) {
    field_work.unk2358 = field_event_bytecode[field_current_event_actor->pc + 1];
    field_current_event_actor->pc += 2;
}

/* 80086E1C: Event: operand 1 0 clears VRAM (0, 0, 0x500, 0x200) and switches both draw
 * and display environments to 640x224 (screens by 80086d8c); 1 and 2 show and
 * hide the five overlay sprites (800adb54, drawn by 800abec8). */
void field_event_display_mode(void) {
    RECT rect;

    switch (field_event_read_imm_or_var(1)) {
    case 0:
        rect.w = 0x500;
        rect.x = 0;
        rect.y = 0;
        rect.h = 0x200;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&field_draw_blocks[0].draw, 0, 0, 0x280, 0xE0);
        SetDefDrawEnv(&field_draw_blocks[1].draw, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&field_draw_blocks[0].disp, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&field_draw_blocks[1].disp, 0, 0, 0x280, 0xE0);
        field_draw_set_display_areas();
        break;
    case 1:
        field_wide_overlay_shown = 1;
        break;
    case 2:
        field_wide_overlay_shown = 0;
        break;
    }
    field_current_event_actor->pc += 3;
}

/* 80086F7C: Event: attach the current actor to node operand 3 of 801e layer actor operand
 * 1 (+128 = operand 1 << 12 | operand 3): its descriptor's transform then
 * follows that node's world matrix (801e72cc); 0xffff detaches. */
void field_event_attach_to_layer_node(void) {
    s32 high = field_event_read_imm_or_var(1);

    field_current_event_actor->unk128 = (high << 12) | field_event_read_imm_or_var(3);
    field_current_event_actor->pc += 5;
}

/* 80086FD0: Event: the 33 overlay sprites at 800afc68 by byte 1: 0 allocates them
 * (800aac08), 2 releases them (800aabd8), 1 places sprite operand 2 at (operand
 * 4, operand 6) with anchor operand 8 for this frame (800aae4c), 3 sets sprite
 * operand 2's colour to (operand 4, 6, 8) (800aadc8). Other values leave the pc
 * on the extended byte, which then runs as primary opcode d4. */
void field_event_overlay_sprites(void) {
    switch (field_event_bytecode[field_current_event_actor->pc + 1]) {
    case 0:
        field_overlay_sprite_alloc_all();
        field_current_event_actor->pc += 2;
        break;
    case 2:
        field_overlay_sprite_release_all();
        field_current_event_actor->pc += 2;
        break;
    case 1:
        field_overlay_sprite_place(field_event_read_imm_or_var(2), field_event_read_imm_or_var(4), field_event_read_imm_or_var(6), field_event_read_imm_or_var(8));
        field_current_event_actor->pc += 10;
        break;
    case 3:
        field_overlay_sprite_set_color(field_event_read_imm_or_var(2), field_event_read_imm_or_var(4), field_event_read_imm_or_var(6), field_event_read_imm_or_var(8));
        field_current_event_actor->pc += 10;
        break;
    }
}

/* 80087148: Event: set bits op3 in the flags of game record op1. */
void field_event_set_skill_record_flags(void) {
    s32 record = field_event_read_imm_or_var(1);
    s32 bits = field_event_read_imm_or_var(3);

    game_current_data->skills[record].flags1A |= bits;
    field_current_event_actor->pc += 5;
}

/* 800871B0: Event op dd, by byte 1: 0 allocates two buffers of operand 4 rows and saves
 * the 256-wide screen band at y operand 2 into one (yields); 1 runs resident
 * 80026f44 on operand 4 rows from row operand 2 of the saved band into the
 * working copy and has the frame load it back (800adbb4); 2 frees both buffers
 * (yields); 3 only yields. Other values leave the pc on the extended byte,
 * which then runs as primary opcode dd. */
void field_event_screen_band(void) {
    s32 y;
    s32 h;
    s32 row;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        field_event_batch_limit += 0x20;
        y = field_event_read_imm_or_var(2);
        h = field_event_read_imm_or_var(4);
        field_screen_band_saved_pixels = heap_alloc(h << 9, 0);
        field_screen_band_work_pixels = heap_alloc(h << 9, 0);
        field_screen_band_rect.x = 0;
        field_screen_band_rect.y = y;
        field_screen_band_rect.w = 0x100;
        field_screen_band_rect.h = h;
        StoreImage(&field_screen_band_rect, (u_long *)field_screen_band_saved_pixels);
        field_event_yield_requested = 1;
        field_current_event_actor->pc += 6;
        break;
    case 1:
        field_event_batch_limit += 0x20;
        row = field_event_read_imm_or_var(2);
        h = field_event_read_imm_or_var(4);
        sprite_darken_pixels(0x100, h, field_screen_band_work_pixels + (row << 8), field_screen_band_saved_pixels + (row << 8));
        field_screen_band_upload_pending = 1;
        field_current_event_actor->pc += 6;
        break;
    case 2:
        heap_free(field_screen_band_saved_pixels);
        heap_free(field_screen_band_work_pixels);
        field_event_yield_requested = 1;
        field_current_event_actor->pc += 2;
        break;
    case 3:
        field_current_event_actor->pc += 2;
        field_event_yield_requested = 1;
        break;
    }
}

/* 800873C4: Event: set entry operand 1 of the 801e layer row table 800b225f to operand 3:
 * that layer's actor is then created at x 0x240 - (layer + row) * 64 (801e742c,
 * via 80077ab4 and ext 5c). */
void field_event_set_layer_row(void) {
    s32 index = field_event_read_imm_or_var(1);

    field_work.unk225F[index] = field_event_read_imm_or_var(3);
    field_current_event_actor->pc += 5;
}

/* 80087420: Event: variables op13 and op15 receive op1 * op9 / op5 and
 * op3 * op11 / op7 (16.16 intermediate). */
void field_event_scale_pair(void) {
    s32 a = field_event_read_imm_or_var(1);
    s32 b = field_event_read_imm_or_var(3);
    s32 c = field_event_read_imm_or_var(5);
    s32 d = field_event_read_imm_or_var(7);
    s32 e = field_event_read_imm_or_var(9);
    s32 f = field_event_read_imm_or_var(11);
    s32 first = (((e << 16) / c) * a) >> 16;
    s32 second = (((f << 16) / d) * b) >> 16;

    field_event_write_variable(field_event_read_u16(13) & 0xFFFF, first);
    field_event_write_variable(field_event_read_u16(15) & 0xFFFF, second);
    field_current_event_actor->pc += 17;
}

/* 8008752C: Event: skip a two-byte operand. */
void field_event_skip_2(void) {
    field_current_event_actor->pc += 3;
}

/* 8008754C: Event: set game flag 0x4000 of +22b6. */
void field_event_unlock_boost_and_skills(void) {
    game_current_data->flags |= 0x4000;
    field_current_event_actor->pc++;
}

/* 80087580: Event: copy gear record operand 1 over gear record operand 3 (0xa4-byte
 * records at +978). */
void field_event_copy_gear(void) {
    s32 from = field_event_read_imm_or_var(1);

    game_current_data->gears[field_event_read_imm_or_var(3)] = game_current_data->gears[from];
    field_current_event_actor->pc += 5;
}

/* 8008764C: Event: copy character operand 1's 0xa4-byte record and 32-byte game record
 * over operand 3's; copying to 9 or 10 sets 0x2000 or 0x1000 in the game's
 * +22b6. */
void field_event_copy_character(void) {
    s32 from = field_event_read_imm_or_var(1);
    s32 to = field_event_read_imm_or_var(3);

    game_current_data->characters[to] = game_current_data->characters[from];
    game_current_data->skills[to] = game_current_data->skills[from];
    {
        GameData *state = game_current_data;

        if (to == 9) {
            state->flags |= 0x2000;
        }
    }
    if (to == 10) {
        game_current_data->flags |= 0x1000;
    }
    field_current_event_actor->pc += 5;
}

/* 80087800: Event: store the arena bout's outcome (80050622, the byte after the six
 * parameters ext bf sets; menu3 arena_bout_record_outcome writes it) in variable
 * operand 1. */
void field_event_store_bout_outcome(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, mode_arena_bout_outcome);
    field_current_event_actor->pc += 3;
}

/* 80087848: Event: once the field allows it (800adbdc and 800adbe4 set, 800adb2c clear,
 * music result 8004f308 not -1; else pc-- back to fe and yield), select menu
 * task 0 (800379b4 sets 80050618), set its six parameter bytes at 8005061c from
 * operands 1-11 and request leaving the field: 800adb88 set, 800adbe8 cleared,
 * so the field loop ends with kind 2 (game mode 4, 8007954c). */
void field_event_open_menu_task(void) {
    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc--;
        return;
    }
    mode_set_arena_task(0);
    mode_arena_task_parameters[0] = field_event_read_imm_or_var(1);
    mode_arena_task_parameters[1] = field_event_read_imm_or_var(3);
    mode_arena_task_parameters[2] = field_event_read_imm_or_var(5);
    mode_arena_task_parameters[3] = field_event_read_imm_or_var(7);
    mode_arena_task_parameters[4] = field_event_read_imm_or_var(9);
    mode_arena_task_parameters[5] = field_event_read_imm_or_var(11);
    field_exit_request_pending = 1;
    field_arena_exit_not_requested = 0;
    field_current_event_actor->pc += 13;
}

/* 80087960: Event: store the world map ferry's saved x and z (+1844, +1846;
 * 8006ee78) in variables op1 and op3. */
void field_event_store_ferry_place(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, game_current_data->unk1844[0]);
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, game_current_data->unk1844[1]);
    field_current_event_actor->pc += 5;
}

/* 800879D0: Event: store the circling flight's saved x and z (+184e, +1852;
 * 8006ee80) in variables op1 and op3, read unsigned (the world map's
 * halves are signed). */
void field_event_store_flight_place(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, (u16)game_current_data->flight.x);
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, (u16)game_current_data->flight.z);
    field_current_event_actor->pc += 5;
}

/* 80087A40: Event: set 800b2357 from byte 1: nonzero skips depth-cueing the actors' model
 * colour in the field draw pass. */
void field_event_set_depth_cue_off(void) {
    field_work.unk2357 = field_event_bytecode[field_current_event_actor->pc + 1];
    field_current_event_actor->pc += 2;
}

/* 80087A7C: Event: set 800b2354 from byte 1: player control (a7) takes its d-pad headings
 * from 800adf68 when zero, else from 800adf88. */
void field_event_set_dpad_table(void) {
    field_work.unk2354 = field_event_bytecode[field_current_event_actor->pc + 1];
    field_current_event_actor->pc += 2;
}

/* 80087AB8: Event: set the circling flight's saved x and z (+184e, +1852; 8006ee80)
 * from operands 1 and 3, immediate by flags 0x80/0x40 of byte 9 (past the
 * instruction's 6 bytes), clear +1850 and +1854 and set +1856 to 1. */
void field_event_set_flight_place(void) {
    game_current_data->flight.x = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 9]);
    game_current_data->flight.z = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 9]);
    game_current_data->flight.count = 0;
    game_current_data->flight.z_frac = 0;
    game_current_data->unk1856 = 1;
    field_current_event_actor->pc += 6;
}

/* 80087B5C: Event: store the world map vehicle's saved position (game +182c-+1830)
 * and heading (+1832; WorldmapReturn) in variables op1..op7. */
void field_event_store_vehicle_place(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, game_current_data->worldmap.unk60);
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, game_current_data->worldmap.unk62);
    field_event_write_variable(field_event_read_u16(5) & 0xFFFF, game_current_data->worldmap.unk64);
    field_event_write_variable(field_event_read_u16(7) & 0xFFFF, game_current_data->worldmap.vehicle_heading);
    field_current_event_actor->pc += 9;
}

/* 80087C0C: Event: set 8004f300, which enables the file 0xab sequence (800acc58) that
 * 800a7948 draws over movie frames 0x687-0x18e1. */
void field_event_enable_movie_overlay(void) {
    mode_staff_roll_enabled = 1;
    field_current_event_actor->pc++;
}

/* 80087C34: Event: set the world map vehicle's saved position and heading
 * (+182c-+1832) from operands 1..7 (immediate by flags 0x80/0x40/0x20/0x10
 * of byte 9). */
void field_event_set_vehicle_place(void) {
    game_current_data->worldmap.unk60 = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 9]);
    game_current_data->worldmap.unk62 = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 9]);
    game_current_data->worldmap.unk64 = field_event_read_selected_operand_20(5, field_event_bytecode[field_current_event_actor->pc + 9]);
    game_current_data->worldmap.vehicle_heading = field_event_read_selected_operand_10(7, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_current_event_actor->pc += 10;
}

/* 80087D30: Event: store the world map vehicle's flags (+1834, WorldmapReturn.flags)
 * in variable op1. */
void field_event_store_vehicle_flags(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, game_current_data->worldmap.flags);
    field_current_event_actor->pc += 3;
}

/* 80087D80: Event: set the world map vehicle's flags (+1834) from operand 1
 * (immediate when flag 0x80 of byte 3 is set). */
void field_event_set_vehicle_flags(void) {
    game_current_data->worldmap.flags = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 3]);
    field_current_event_actor->pc += 4;
}

/* 80087DE0: Event: set 800b2355 (byte 1 zero) or 800b2356 from operand 2: the 8005954c
 * value of random-encounter battles (field loop) and of scripted battles (71,
 * ext 84) respectively. */
void field_event_set_battle_sounds(void) {
    s32 value = field_event_read_imm_or_var(2);

    if (field_event_bytecode[field_current_event_actor->pc + 1] == 0) {
        field_work.unk2355 = value;
    } else {
        field_work.unk2356 = value;
    }
    field_current_event_actor->pc += 4;
}

/* 80087E5C: Event: set the battle-entry override (800b234c) from operand 1. */
void field_event_set_battle_override(void) {
    field_work.battle_override = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 80087E98: Event: make the actor byte 1 selects the controlled actor and the one the
 * camera follows (800b233e): clear flags 0x01004000 on every event actor and
 * set 0x4000 on it; the followers are idle unless it is the party leader's
 * actor. */
void field_event_set_controlled(void) {
    s32 index = field_event_read_actor_index(1);
    s32 i;

    if (index != 0xFF) {
        if (index == mode_party_actors[0]) {
            field_work.followers_idle = 0;
        } else {
            field_work.followers_idle = 1;
        }
        field_work.controlled = index;
        field_work.unk233E = index;
        for (i = 0; i < field_event_actor_count; i++) {
            field_view.components.descriptors[i].actor->flags &= ~0x01004000;
        }
        field_view.components.descriptors[index].actor->flags |= 0x4000;
    }
    field_current_event_actor->pc += 2;
}

/* 80087FA4: Event: count 800b2348 up: while it is nonzero, party gathering (8009aee0, ext
 * 23/24) warps members to their spots instead of walking; gathering clears
 * it. */
void field_event_warp_gathering(void) {
    field_work.unk2348++;
    field_current_event_actor->pc++;
}

/* 80087FD4: Event: build the status panel (800a8ba4): load file 0xaa's image to VRAM
 * (380, 0) (800a8314), allocate both buffers' piece quads and texture them. */
void field_event_build_status_panel(void) {
    field_status_panel_build();
    field_current_event_actor->pc++;
}

/* 8008800C: Event: transform the vector (operands 5, 7, 9) by the world matrix of node
 * operand 3 of 801e layer actor operand 1 (801e72cc; selected by flags byte 11)
 * and store its x, y, z in the variables operands 12, 14 and 16 name. */
void field_event_transform_vector(void) {
    MATRIX m;
    MATRIX work;
    SVECTOR in;
    SVECTOR out;
    long flag;
    s32 a;

    m.t[0] = m.t[1] = m.t[2] = 0;
    a = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xB));
    gear_model_get_node_matrix(&m, &work, a, field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0xB)));
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    in.vx = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xB));
    in.vy = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xB));
    in.vz = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xB));
    RotTransSV(&in, &out, &flag);
    field_event_write_variable(field_event_read_u16(0xC) & 0xFFFF, out.vx);
    field_event_write_variable(field_event_read_u16(0xE) & 0xFFFF, out.vy);
    field_event_write_variable(field_event_read_u16(0x10) & 0xFFFF, out.vz);
    field_current_event_actor->pc += 0x12;
}

/* 80088198: Event: restore every gear's points and gauge to their maxima (the 20 gear
 * records at +978). */
void field_event_restore_gears(void) {
    s32 i;
    GameData *state = game_current_data;

    for (i = 0; i < 20; i++) {
        state->gears[i].hp = state->gears[i].maxHp;
        state->gears[i].fuel = state->gears[i].maxFuel;
    }
    field_current_event_actor->pc++;
}

/* 800881E8: Event: byte 1 zero saves the 64x256 VRAM column at (3c0, 100) once
 * (800a915c); nonzero restores it and frees the copy (800a91f0). */
void field_event_save_vram_column(void) {
    if (field_event_bytecode[field_current_event_actor->pc + 1] == 0) {
        field_vram_column_save();
    } else {
        field_vram_column_restore();
    }
    field_current_event_actor->pc += 2;
}

/* 8008825C: Event: wait (pc-- back to fe), yielding, while a music change is pending
 * (8004f308 == -1, set by 8008f7b8 when it changes the track); then advance,
 * yielding. */
void field_event_wait_music_load(void) {
    if (mode_music_load_pending == -1) {
        field_current_event_actor->pc--;
    } else {
        field_current_event_actor->pc++;
    }
    field_event_yield_requested = 1;
}

/* 800882B8: Event: store the gear (+a0 of its record) of character operand 1 (8008cf3c:
 * fd-ff party slots 0-2, fc none) in variable operand 3, or 0xff for none. */
void field_event_store_gear(void) {
    s32 character = field_event_resolve_character(field_event_read_imm_or_var(1));

    if (character != 0xFF) {
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, game_current_data->characters[character].gearId);
    } else {
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, 0xFF);
    }
    field_current_event_actor->pc += 5;
}

/* 80088360: Event: set the gear (+a0 of the 0xa4-byte record) of character operand 1 to
 * operand 3. */
void field_event_set_gear(void) {
    s32 slot = field_event_read_imm_or_var(1);

    game_current_data->characters[slot].gearId = field_event_read_imm_or_var(3);
    field_current_event_actor->pc += 5;
}

/* 800883D4: Event: set (selector 0) or clear character op2's bit of the game's +2318. */
void field_event_set_party_lock(void) {
    s32 character = field_event_resolve_character(field_event_read_imm_or_var(2));

    if (character != 0xFF) {
        if (field_event_bytecode[field_current_event_actor->pc + 1] == 0) {
            game_current_data->locked |= 1 << character;
        } else {
            game_current_data->locked &= ~(1 << character);
        }
    }
    field_current_event_actor->pc += 4;
}

/* 8008848C: Event: set 800b236c to the inverse of its byte operand's low bit. */
void field_event_set_menu_parameter(void) {
    field_menu_parameter = EVENT_OPERAND_BYTE(1) ^ 1;
    field_current_event_actor->pc += 2;
}

/* 800884CC: Event: set the sound-emitter range from operand 1. */
void field_event_set_emitter_range(void) {
    field_work.emitter_range = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 80088508: Event: for party member operand 5 (0xff none), store its model's
 * animation +0c in variable operand 1 and its descriptor in variable
 * operand 3, clearing +0c unless it is 1; four batch steps. */
void field_event_store_member_animation(void) {
    s32 member;
    Sprite *model;

    member = mode_party_actors[field_event_read_imm_or_var(5)];
    field_event_batch_limit += 4;
    if (member != 0xFF) {
        model = field_view.components.descriptors[member].model;
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, (u16)SPRITE_SEQUENCER(model)->halfc);
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, member);
        if ((u16)SPRITE_SEQUENCER(model)->halfc != 1) {
            SPRITE_SEQUENCER(model)->halfc = 0;
        }
    } else {
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, 0);
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, 0);
    }
    field_current_event_actor->pc += 7;
}

/* 8008861C: Clear the eight pairs at +30 of the current emitter record. */
void field_effect_clear_template_pairs(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        field_effect_templates[field_effect_launch.record].unk30[i][0] = field_effect_templates[field_effect_launch.record].unk30[i][1] = 0;
    }
}

/* 80088674: Event: as ext 8f for actor operand 1 (0xff: actor 0 for the template reset,
 * while 800b2374 keeps operand 1): launch frame operand 3 and the 801e layer
 * actor and node operands 5 and 7; four batch steps. */
void field_event_begin_effect(void) {
    s32 actor = field_event_read_imm_or_var(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    field_effect_launch.actor = field_event_read_imm_or_var(1);
    field_effect_launch.frame = field_event_read_imm_or_var(3);
    field_effect_launch.layer_actor = field_event_read_imm_or_var(5);
    field_effect_launch.layer_node = field_event_read_imm_or_var(7);
    field_current_event_actor->pc += 9;
    field_effect_reset_templates(actor);
    switch (field_effect_launch.frame) {
    case 0:
        field_effect_launch.frame = 0;
        break;
    case 1:
        field_effect_launch.frame = 0x10;
        break;
    case 2:
        field_effect_launch.frame = 0x20;
        break;
    case 3:
        field_effect_launch.frame = 0x30;
        break;
    }
    field_event_batch_limit += 4;
}

/* 80088790: Event: begin an effect for the actor byte 1 selects (none: actor 0): reset
 * the eight emitter templates at 800b02cc to follow it (800a94a4) and keep its
 * launch frame operand 2 (0 owner-facing, 1 801e node, 2 owner's transform, 3
 * owner-facing and scaled; 0-3 kept as kind << 4) and operands 4 and 6 (the
 * 801e layer actor and node of frame 1) at 800b2374-2380 for ext 90 and 93;
 * four batch steps. */
void field_event_begin_effect_actor(void) {
    s32 actor = field_event_read_actor_index(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    field_effect_launch.actor = actor;
    field_effect_launch.frame = field_event_read_imm_or_var(2);
    field_effect_launch.layer_actor = field_event_read_imm_or_var(4);
    field_effect_launch.layer_node = field_event_read_imm_or_var(6);
    field_current_event_actor->pc += 8;
    field_effect_reset_templates(actor);
    switch (field_effect_launch.frame) {
    case 0:
        field_effect_launch.frame = 0;
        break;
    case 1:
        field_effect_launch.frame = 0x10;
        break;
    case 2:
        field_effect_launch.frame = 0x20;
        break;
    case 3:
        field_effect_launch.frame = 0x30;
        break;
    }
    field_event_batch_limit += 4;
}

/* 800888A4: Event: give the current actor's sprite a new two-word sequencer buffer
 * (8002303c) whose entries 2 and 3 are (operand 1 & 15) << 6 and (operand 1 >>
 * 4) << 8 + operand 3; the values and mode 1 are kept in the actor (+12c, +130)
 * for 800a28d4 to rebuild it. */
void field_event_set_sprite_sequence(void) {
    Sprite *model;
    u16 low;
    s32 frame;

    model = field_view.components.descriptors[field_current_event_actor_index].model;
    low = field_event_read_imm_or_var(1) & 0xF;
    frame = ((field_event_read_imm_or_var(1) >> 4) << 8) + field_event_read_imm_or_var(3);
    sprite_alloc_sequencer_buffer(model, 2, 0);
    SPRITE_SEQUENCER(model)->buffer[2] = low << 6;
    field_current_event_actor->state.bits.unk18 = low << 6;
    SPRITE_SEQUENCER(model)->buffer[3] = frame;
    field_current_event_actor->unk130 = frame;
    field_current_event_actor->state.bits.unk16 = 1;
    field_current_event_actor->pc += 5;
}

/* 800889BC: Event: as ext a6 with a three-word sequencer buffer: entries 2-3 from
 * operands 1 and 3, entries 4-5 from operands 5 and 7 (mode 2). */
void field_event_set_sprite_sequence2(void) {
    Sprite *model;
    u16 low0;
    s32 frame0;
    u16 low1;
    s32 frame1;

    model = field_view.components.descriptors[field_current_event_actor_index].model;
    low0 = field_event_read_imm_or_var(1) & 0xF;
    frame0 = ((field_event_read_imm_or_var(1) >> 4) << 8) + field_event_read_imm_or_var(3);
    low1 = field_event_read_imm_or_var(5) & 0xF;
    frame1 = ((field_event_read_imm_or_var(5) >> 4) << 8) + field_event_read_imm_or_var(7);
    sprite_alloc_sequencer_buffer(model, 3, 0);
    SPRITE_SEQUENCER(model)->buffer[2] = low0 << 6;
    field_current_event_actor->state.bits.unk18 = low0 << 6;
    SPRITE_SEQUENCER(model)->buffer[3] = frame0;
    field_current_event_actor->unk130 = frame0;
    SPRITE_SEQUENCER(model)->buffer[4] = low1 << 6;
    field_current_event_actor->unk130_9 = low1 << 6;
    SPRITE_SEQUENCER(model)->buffer[5] = frame1;
    field_current_event_actor->unk130_19 = frame1;
    field_current_event_actor->pc += 9;
    field_current_event_actor->state.bits.unk16 = 2;
}

/* 80088B68: Event: set flag 0x80 (op1 1) or 0x40 (op1 2) of the current record's +2a,
 * using four batch steps. */
void field_event_set_template_flag(void) {
    s32 bits = 0;

    switch (field_event_read_imm_or_var(1)) {
    case 1:
        bits = 0x80;
        break;
    case 2:
        bits = 0x40;
        break;
    }
    field_effect_templates[field_effect_launch.record].flags |= bits;
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 7;
}

/* 80088C1C: Event: set the current emitter template's +24 to operand 1, or operand 3 << 8
 * into its flags and set its particle angle (+76) to operand 5; four batch
 * steps. */
void field_event_set_template_24(void) {
    field_effect_templates[field_effect_launch.record].unk24 = field_event_read_imm_or_var(1);
    field_effect_templates[field_effect_launch.record].flags |= field_event_read_imm_or_var(3) << 8;
    field_effect_templates[field_effect_launch.record].unk76 = field_event_read_imm_or_var(5);
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 7;
}

/* 80088CF8: Event: set the current emitter template's +30 pairs 0-3 from the selected
 * operands 1-15 (flags byte 17; 80088d38); four batch steps. */
void field_event_set_template_pairs0(void) {
    field_event_set_template_pairs(0);
}

/* 80088D18: Event: set the current emitter template's +30 pairs 4-7 from the selected
 * operands 1-15 (flags byte 17; 80088d38); four batch steps. */
void field_event_set_template_pairs4(void) {
    field_event_set_template_pairs(4);
}

/* 80088D38: Set the current emitter record's four +30 pairs from index first on to the
 * selected operands 1..15 (flags byte 0x11); four batch steps. */
void field_event_set_template_pairs(s32 first) {
    field_effect_templates[field_effect_launch.record].unk30[first][0] = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0x11));
    field_effect_templates[field_effect_launch.record].unk30[first][1] = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0x11));
    field_effect_templates[field_effect_launch.record].unk30[first + 1][0] = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0x11));
    field_effect_templates[field_effect_launch.record].unk30[first + 1][1] = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0x11));
    field_effect_templates[field_effect_launch.record].unk30[first + 2][0] = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0x11));
    field_effect_templates[field_effect_launch.record].unk30[first + 2][1] = field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0x11));
    field_effect_templates[field_effect_launch.record].unk30[first + 3][0] = field_event_read_selected_operand_02(0xD, EVENT_OPERAND_BYTE(0x11));
    field_effect_templates[field_effect_launch.record].unk30[first + 3][1] = field_event_read_selected_operand_01(0xF, EVENT_OPERAND_BYTE(0x11));
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 0x12;
}

/* 80089004: Event: select emitter template operand 1 (800b2384) for the template
 * instructions that follow and set it up for the effect's actor (+52 and
 * 800adb40 = 800b2374): operand 3 particles, start delay operand 5, lifetime
 * operand 7 (7fff lasting), +24 = 1, +00 and +76 cleared and its +30 pairs
 * cleared (8008861c); four batch steps. */
void field_event_select_emitter_template(void) {
    s32 index;

    field_effect_launch.record = index = field_event_read_imm_or_var(1);
    field_effect_templates[index].unk24 = 1;
    field_effect_templates[field_effect_launch.record].unk52 = field_effect_launch.actor;
    field_effect_template_actor = field_effect_templates[field_effect_launch.record].unk52;
    field_effect_templates[field_effect_launch.record].unk00 = 0;
    field_effect_templates[field_effect_launch.record].unk76 = 0;
    field_effect_templates[field_effect_launch.record].count = field_event_read_imm_or_var(3);
    field_effect_templates[field_effect_launch.record].unk02 = field_event_read_imm_or_var(5);
    field_effect_templates[field_effect_launch.record].unk04 = field_event_read_imm_or_var(7);
    field_effect_clear_template_pairs();
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 9;
}

/* 80089174: Event: set the current emitter record's +0c and +14 vectors from the
 * selected operands 1..11 (flags byte 13); four batch steps. */
void field_event_set_template_vectors(void) {
    field_effect_templates[field_effect_launch.record].unk0C.vx = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk0C.vy = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk0C.vz = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk14.vx = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk14.vy = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk14.vz = field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0xD));
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 0xE;
}

/* 80089374: Event: set the current emitter template's +08 (operand 1), +1c vector
 * (operands 3, 5, 7), spawn radius +26 (operand 9) and velocity spread +28
 * (operand 11), selected by flags byte 13; four batch steps. */
void field_event_set_template_08(void) {
    field_effect_templates[field_effect_launch.record].unk08 = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk1C.vx = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk1C.vy = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk1C.vz = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk26 = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk28 = field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0xD));
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 0xE;
}

/* 80089574: Event: set the current emitter template's spawn interval +56, particle life
 * +58 and +54 from operands 1, 3 and 5, its flags to operand 7 | operand 9 * 2
 * | the effect's launch frame (800b2378), and +72/+74 (the 801e layer actor and
 * node of launch frame 1) from 800b237c/800b2380; four batch steps. */
void field_event_set_template_56(void) {
    s16 flags;

    field_effect_templates[field_effect_launch.record].unk56 = field_event_read_imm_or_var(1);
    field_effect_templates[field_effect_launch.record].unk58 = field_event_read_imm_or_var(3);
    field_effect_templates[field_effect_launch.record].unk54 = field_event_read_imm_or_var(5);
    flags = field_event_read_imm_or_var(7);
    field_effect_templates[field_effect_launch.record].flags = flags | (field_event_read_imm_or_var(9) * 2) | field_effect_launch.frame;
    field_effect_templates[field_effect_launch.record].unk72 = field_effect_launch.layer_actor;
    field_effect_templates[field_effect_launch.record].unk74 = field_effect_launch.layer_node;
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 11;
}

/* 800896D4: Event: set the current emitter record's +5a and +62 vectors from the
 * selected operands 1/3 and 5/7 (flags byte 9); four batch steps. */
void field_event_set_template_5a(void) {
    field_effect_templates[field_effect_launch.record].unk5A.vx = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(9));
    field_effect_templates[field_effect_launch.record].unk5A.vy = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(9));
    field_effect_templates[field_effect_launch.record].unk5A.vz = 0;
    field_effect_templates[field_effect_launch.record].unk62.vx = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(9));
    field_effect_templates[field_effect_launch.record].unk62.vy = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(9));
    field_effect_templates[field_effect_launch.record].unk62.vz = 0;
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 10;
}

/* 80089880: Event: set the current emitter record's bytes +6a..+6c and +6e..+70 from
 * the selected operands 1..11 (flags byte 13); four batch steps. */
void field_event_set_template_6a(void) {
    field_effect_templates[field_effect_launch.record].unk6A = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk6B = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk6C = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk6E = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk6F = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xD));
    field_effect_templates[field_effect_launch.record].unk70 = field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0xD));
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 0xE;
}

/* 80089A80: Event: unless 800adb8c is set, start the effect the templates define for the
 * current actor (800a99a8: copy the eight emitter templates into a free effect
 * slot it owns and allocate their particles); four batch steps. */
void field_event_start_effect(void) {
    field_event_batch_limit += 4;
    if (field_party_rebuilding == 0) {
        field_effect_start(field_current_event_actor_index);
    }
    field_current_event_actor->pc++;
}

/* 80089AE4: Event: stop the current actor's effects (800a98e8), also releasing their
 * particles when byte 1 is nonzero; four batch steps. */
void field_event_stop_effects(void) {
    field_event_batch_limit += 4;
    field_effect_stop_by_owner(field_current_event_actor_index, field_event_bytecode[field_current_event_actor->pc + 1]);
    field_current_event_actor->pc += 2;
}

/* 80089B54: Event: store the current actor's party position (or 0xff) in variable op1. */
void field_event_store_party_slot(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (mode_party_actors[i] == field_current_event_actor_index) {
            field_event_write_variable(field_event_read_u16(1) & 0xFFFF, i);
            goto done;
        }
    }
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, 0xFF);
done:
    field_current_event_actor->pc += 3;
}

/* 80089BF0: Event: set emitter operand 1's start (22e8) and end (2300) points and its
 * step count (2318) from the selected operands 3-15 (flags byte 0x11). */
void field_event_set_emitter_path(void) {
    s32 emitter;

    emitter = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0x11));
    field_work.unk22E8[emitter][0] = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0x11));
    field_work.unk22E8[emitter][1] = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0x11));
    field_work.unk22E8[emitter][2] = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0x11));
    field_work.unk2300[emitter][0] = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0x11));
    field_work.unk2300[emitter][1] = field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0x11));
    field_work.unk2300[emitter][2] = field_event_read_selected_operand_02(0xD, EVENT_OPERAND_BYTE(0x11));
    field_work.unk2318[emitter] = field_event_read_selected_operand_01(0xF, EVENT_OPERAND_BYTE(0x11));
    field_current_event_actor->pc += 0x12;
}

/* 80089DCC: Event: place sound emitter op1 at (op3, op7, op5) and attach it to the
 * actor byte 10 selects (-1 for none). */
void field_event_place_emitter(void) {
    s32 index = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 9]);
    s32 actor;

    field_work.emitter_position[index][0] = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_work.emitter_position[index][2] = field_event_read_selected_operand_20(5, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_work.emitter_position[index][1] = field_event_read_selected_operand_10(7, field_event_bytecode[field_current_event_actor->pc + 9]);
    actor = field_event_read_actor_index(10);
    if (actor != 0xFF) {
        field_work.emitter_descriptor[index] = actor;
    } else {
        field_work.emitter_descriptor[index] = -1;
    }
    field_current_event_actor->pc += 11;
}

/* 80089F18: Event: set the sound listener selector 800b22e0 from operand 1 (80086908
 * places the listener at 0 the controlled actor, 1 the camera eye, 2 the camera
 * target). */
void field_event_set_listener(void) {
    field_work.unk22E0 = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 80089F54: Event: set the piece drift mode 800b21d2 to operand 1 less 0x80: its low
 * bits pick which pieces the drift vector (fe 1d) moves each frame. */
void field_event_set_piece_drift_mode(void) {
    field_work.piece_drift_mode = field_event_read_imm_or_var(1) - 0x80;
    field_current_event_actor->pc += 3;
}

/* 80089F94: Event: store operand byte 1 in 800afe84: after a movie or a menu (800a7c58,
 * 800799d4) a nonzero value sets 800adb50 (the panorama is then not drawn), and
 * the menu return presents the screen under brightness tiles 0x20-0x3e
 * (80079784) instead of 0x1f-0. */
void field_event_set_no_panorama(void) {
    FieldActor *actor;

    actor = field_current_event_actor;
    field_no_panorama_after_return = field_event_bytecode[actor->pc + 1];
    actor->pc += 2;
}

/* 80089FD0: Event: set the panorama backdrop parameters at 800b0080 that the field load
 * passes to 8002709c: texture x operand 1, y operand 3, width operand 5 (0
 * becomes 1), height operand 7, CLUT x 0, CLUT y operand 9, mode operand 11 and
 * turn operand 13 (raw halfwords). */
void field_event_set_panorama(void) {
    s16 value;

    field_panorama_parameters.unk80[0] = field_event_read_u16(1);
    field_panorama_parameters.unk80[1] = field_event_read_u16(3);
    value = field_event_read_u16(5);
    field_panorama_parameters.unk80[2] = value;
    if (value == 0) {
        field_panorama_parameters.unk80[2] = value + 1;
    }
    field_panorama_parameters.unk80[3] = field_event_read_u16(7);
    field_panorama_parameters.unk80[4] = 0;
    field_panorama_parameters.unk80[5] = field_event_read_u16(9);
    field_panorama_parameters.unk80[6] = field_event_read_u16(11);
    field_panorama_parameters.unk80[7] = field_event_read_u16(13);
    field_current_event_actor->pc += 15;
}

/* 8008A08C: Event: set the panorama backdrop's position (800b0090: x operand 1, z operand
 * 3, y operand 5; immediate by flags 0x80/0x40/0x20 of byte 7). */
void field_event_set_panorama_position(void) {
    field_panorama_parameters.unk90 = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_panorama_parameters.unk98 = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_panorama_parameters.unk94 = field_event_read_selected_operand_20(5, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_current_event_actor->pc += 8;
}

/* 8008A148: Event: set the panorama backdrop's sky, horizon and ground colours (800b00a0:
 * three RGB triples from operands 1-17), fill scale operand 19, fade range
 * operand 21 and fade start operand 23, and enable it (800b00b2): the field
 * load creates it (8002709c) once the actors' event 0 scripts have run, and
 * 80075484 draws it. */
void field_event_set_panorama_colors(void) {
    field_panorama_parameters.unkA0[0] = field_event_read_imm_or_var(1);
    field_panorama_parameters.unkA0[1] = field_event_read_imm_or_var(3);
    field_panorama_parameters.unkA0[2] = field_event_read_imm_or_var(5);
    field_panorama_parameters.unkA4[0] = field_event_read_imm_or_var(7);
    field_panorama_parameters.unkA4[1] = field_event_read_imm_or_var(9);
    field_panorama_parameters.unkA4[2] = field_event_read_imm_or_var(11);
    field_panorama_parameters.unkA8[0] = field_event_read_imm_or_var(13);
    field_panorama_parameters.unkA8[1] = field_event_read_imm_or_var(15);
    field_panorama_parameters.unkA8[2] = field_event_read_imm_or_var(17);
    field_panorama_parameters.unkAC = field_event_read_imm_or_var(19);
    field_panorama_parameters.unkAE = field_event_read_imm_or_var(21);
    field_panorama_parameters.unkB0 = field_event_read_imm_or_var(23);
    field_current_event_actor->pc += 25;
    field_panorama_parameters.enabled = 1;
}

/* 8008A244: Event: wait while 800adb88 is set, yielding each time. */
void field_event_wait_battle_request(void) {
    if (field_exit_request_pending == 0) {
        field_current_event_actor->pc++;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008A2A0: Event: store the current movie frame (800b06a0, recorded by the movie frame
 * callback 800a7120) in variable operand 1. */
void field_event_store_movie_frame(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_movie_frame);
    field_current_event_actor->pc += 3;
}

/* 8008A2E8: Event fe 77: once the stream is stopped (8008a558; else wait: pc-- to the
 * fe), by byte 1: 0 loads TIM file 0x7fb + operand 5 (selected by flag 0x80 of
 * byte 13, i.e. operand 2 and the flags of the following fe 77 01) into
 * 800b1f74 and advances 2; 1 uploads it (80070340) at x, y = operands 4, 6 with
 * CLUT row operand 8 + 0xe8 (0xff: the TIM's own), selected by flags
 * 0x40/0x20/0x10 of byte 10, first saving the VRAM column at (3c0, 100)
 * (800a915c) when y >= 0x100 and x >= 0x2c0, and advances 11; other values free
 * the image and advance 2. Yields. */
void field_event_tim(void) {
    s32 index;
    s32 file;
    s32 clut_y;
    s32 x;
    s32 y;
    s32 row;
    u32 *image;

    if (field_music_is_stream_or_disc_busy() == -1) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc -= 1;
        return;
    }
    if (EVENT_OPERAND_BYTE(1) == 0) {
        index = field_event_read_selected_operand_80(5, EVENT_OPERAND_BYTE(0xD));
        cd_select_directory(4, 0);
        file = index + 0x7FB;
        image = heap_alloc(cd_get_aligned_file_size(file), 0);
        field_event_loaded_tim = image;
        cd_read_file(file, image, 0, 0x80);
        field_current_event_actor->pc += 2;
    } else if (EVENT_OPERAND_BYTE(1) == 1) {
        row = field_event_read_selected_operand_10(8, EVENT_OPERAND_BYTE(0xA));
        clut_y = row + 0xE8;
        if (row == 0xFF) {
            clut_y = -1;
        }
        x = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(0xA));
        y = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(0xA));
        if (y >= 0x100 && x >= 0x2C0) {
            field_vram_column_save();
        }
        field_load_tim_at(field_event_loaded_tim, x, y, 0, clut_y, 0, 0);
        field_current_event_actor->pc += 0xB;
    } else {
        heap_free(field_event_loaded_tim);
        field_current_event_actor->pc += 2;
    }
    field_event_yield_requested = 1;
}

/* 8008A4E0: Event fe 7a: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7a (restore every character's EP). */
void field_event_rerun_7a(void) {
}

/* 8008A4E8: Event fe 79: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 79 (restore every character's HP). */
void field_event_rerun_79(void) {
}

/* 8008A4F0: Event fe 78: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 78 (read a map's data ahead, re-running
 * until it is loaded). */
void field_event_rerun_78(void) {
}

/* 8008A4F8: Empty; neither instruction table nor any call reaches it. */
void field_event_empty_unreferenced(void) {
}

/* 8008A500: Event fe 7c: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7c (restore the masked party members' EP).
 */
void field_event_rerun_7c(void) {
}

/* 8008A508: Event fe 7d: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7d (reduce the masked party members' EP).
 */
void field_event_rerun_7d(void) {
}

/* 8008A510: Event fe 7e: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7e (restore the masked party members' EP).
 */
void field_event_rerun_7e(void) {
}

/* 8008A518: Event fe 7b: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7b (reduce the masked party members' HP).
 */
void field_event_rerun_7b(void) {
}

/* 8008A520: Wait (VSync) until the disc is idle and the stream is stopped. */
void field_music_wait_stream_and_disc_idle(void) {
    while (field_music_is_stream_or_disc_busy() != 0) {
        VSync(0);
    }
}

/* 8008A558: Stop the stream once no music-wave read runs and the disc is idle; -1
 * while busy. */
s32 field_music_is_stream_or_disc_busy(void) {
    if (field_music_stream_running == 0) {
        if (cd_get_pending_read_count() == 0) {
            cd_sync_reads(0);
            return 0;
        }
    }
    return -1;
}

/* 8008A5A0: Event fe 6c: when the byte after this instruction (the next instruction's
 * first byte, not consumed) is 0, clear the pad byte 8005938c (8003633c(0); the
 * controller start sets it to 1); advance 1. */
void field_event_clear_pad_byte(void) {
    if (field_event_bytecode[field_current_event_actor->pc + 1] == 0) {
        pad_set_unread_byte(0);
    }
    field_current_event_actor->pc++;
}

/* 8008A604: Event: set the ordering-table depth 800b21d4 (0x720 at load) at which the
 * panorama backdrop is drawn and the second ordering table is linked in,
 * from operand 1. */
void field_event_set_panorama_depth(void) {
    field_work.unk21D4 = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 8008A640: Event fe 6b: set byte +78 of character record operand 3 (resolved by
 * 8008cf3c; none: unchanged) to operand 1 less its byte +77, at least 0. */
void field_event_set_character_78(void) {
    s32 character = field_event_resolve_character(field_event_read_imm_or_var(3));
    s32 value;

    if (character != 0xFF) {
        value = field_event_read_imm_or_var(1) - game_current_data->characters[character].field77;
        if (value < 0) {
            value = 0;
        }
        game_current_data->characters[character].field78 = value;
    }
    field_current_event_actor->pc += 5;
}

/* 8008A6E0: Event fe 69: store the sum of bytes +77 and +78 of character record operand 3
 * (resolved by 8008cf3c; 0 for none) in variable operand 1. */
void field_event_store_character_sum(void) {
    s32 character = field_event_resolve_character(field_event_read_imm_or_var(3));
    s32 sum;

    if (character != 0xFF) {
        sum = game_current_data->characters[character].field77 + game_current_data->characters[character].field78;
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, sum);
    } else {
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, 0);
    }
    field_current_event_actor->pc += 5;
}

/* 8008A790: Find a free (0xff) slot of the table at 80062590 for `id`; -1 when `id`
 * is already there or no slot is free. */
s32 field_party_find_free_slot(s32 id, s32 *slot) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (mode_party_members[i] == id) {
            break;
        }
        if (mode_party_members[i] == 0xFF) {
            *slot = i;
            return 0;
        }
    }
    return -1;
}

/* 8008A7DC: Start reading party member `member`'s sprite file for slot `slot` (a
 * character substitute while 8004f34c has 0xc000). */
void field_party_read_member_sprite(s32 member, s32 slot) {
    s32 file;
    s32 size;
    s32 sprite;

    field_party_sprite_load_slot = slot;
    field_party_sprite_load_member = member;
    cd_select_directory(4, 0);
    if (field_event_runs_per_frame == 0) {
        cd_sync_reads(0);
    }
    if (!(mode_field_map_id & 0xC000)) {
        file = member + 5;
        size = cd_get_aligned_file_size(file);
        mode_party_file_ids[field_party_sprite_load_slot] = member;
        field_party_sprite_load_buffer = heap_alloc(size, 0);
        cd_read_file(file, field_party_sprite_load_buffer, 0, 0x80);
    } else {
        sprite = mode_get_character_gear_id(member);
        if (sprite == 0xFF) {
            sprite = 0;
        }
        sprite += 0x10;
        file = sprite + 5;
        field_party_sprite_load_buffer = heap_alloc(cd_get_aligned_file_size(file), 0);
        mode_party_file_ids[field_party_sprite_load_slot] = sprite;
        cd_read_file(file, field_party_sprite_load_buffer, 0, 0x80);
    }
    if (field_event_runs_per_frame == 0) {
        cd_sync_reads(0);
    }
    field_party_sprite_load_pending = 1;
}

/* 8008A93C: Event fe 4d: set the current actor's animation override (+ea, used instead of
 * its own animation unless 0xff) to the complement of byte 1 (byte 0 gives
 * 0xff: no override). */
void field_event_set_animation_complement(void) {
    FieldActor *actor;

    actor = field_current_event_actor;
    actor->unk0EA = ~field_event_bytecode[actor->pc + 1];
    actor->pc += 2;
}

/* 8008A974: Event fe 4c: set the current actor's animation override (+ea) to the
 * complement of byte 1 (as fe 4d) and clear layer bit 16, which its sprite's
 * completion callback (80076a74) sets again; opcode 5e waits for that. */
void field_event_play_animation_complement(void) {
    field_event_set_animation_complement();
    field_current_event_actor->layer_flags &= ~0x10000;
}

/* 8008A9AC: Event fe 4b: once the stream is stopped (8008a558), clear 800adb90 and make
 * the actor's loaded block (+120) its sprite's resource (80021bf0), advancing
 * 1; otherwise wait (pc-- to the fe). Yields. */
void field_event_apply_actor_block(void) {
    if (field_music_is_stream_or_disc_busy() == 0) {
        field_actor_block_loading = 0;
        sprite_set_alternate_resource(field_view.components.descriptors[field_current_event_actor_index].model, (s32)field_current_event_actor->unk120);
        field_current_event_actor->pc++;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008AA60: Event: release the actor's block at +120 once, then yield. */
void field_event_free_actor_block(void) {
    if (field_current_event_actor->unk124 != -1) {
        heap_free(field_current_event_actor->unk120);
        field_current_event_actor->unk124 = -1;
    }
    field_event_yield_requested = 1;
    field_current_event_actor->pc++;
}

/* 8008AACC: Event 0xb0: sound-effect bank, once the stream and disc are idle (8008a558;
 * else pc-- back to fe). Byte 1 1 loads the bank file read before into SPU
 * memory (80037fd8) as slot 800afd18's bank (slot 3 also becomes 800595ac),
 * waits for the transfer, frees the file, yields and advances 2. Other values
 * release slot operand 2's bank (80038310) and start reading file operand 4 + 2
 * of directory (0x1c, 0) (file 6 when bit 0x80 is set and 8004f370 is 1), or
 * with bit 0x80 file (operand 4 & 0x7f) + 0x1f of directory (0x2c, 1); advance
 * 6. */
void field_event_sound_bank(void) {
    s32 file;
    s32 slot;
    void *data;

    if (field_music_is_stream_or_disc_busy() == 0) {
        if (EVENT_OPERAND_BYTE(1) == 1) {
            mode_wave_bank_slots[field_sound_bank_load_slot] = (s32)sound_load_wave_bank(field_sound_bank_load_buffer, 0);
            sound_sync_transfer(0x10);
            heap_free(field_sound_bank_load_buffer);
            if (field_sound_bank_load_slot == 3) {
                mode_wave_bank_5 = (SoundSequence *)mode_wave_bank_slots[3];
            }
            field_event_yield_requested = 1;
            field_current_event_actor->pc += 2;
        } else {
            slot = field_event_read_imm_or_var(2);
            field_sound_bank_load_slot = slot;
            sound_release_wave_bank((SoundSequence *)mode_wave_bank_slots[slot]);
            file = field_event_read_imm_or_var(4);
            field_sound_bank_load_file = file;
            if (!(file & 0x80)) {
            standard:
                cd_select_directory(0x1C, 0);
                field_sound_bank_load_file += 2;
            } else if (mode_field_standalone == 1) {
                field_sound_bank_load_file = 4;
                goto standard;
            } else {
                field_sound_bank_load_file = (file & 0x7F) + 0x1F;
                cd_select_directory(0x2C, 1);
            }
            data = heap_alloc(cd_get_aligned_file_size(field_sound_bank_load_file), 0);
            field_sound_bank_load_buffer = data;
            cd_read_file(field_sound_bank_load_file, data, 0, 0x80);
            cd_select_directory(4, 0);
            field_current_event_actor->pc += 6;
        }
    } else {
        field_event_yield_requested = 1;
        field_current_event_actor->pc -= 1;
    }
}

/* 8008ACE8: Event: load file op1 + 0x77a as the actor's block (+120), once the
 * stream and disc are idle; yields. */
void field_event_load_actor_block(void) {
    s32 number;
    s32 file;
    s32 size;

    if (field_actor_block_loading == 0 && field_music_stream_running == 0) {
        if (field_music_is_stream_or_disc_busy() != 0) {
            field_event_yield_requested = 1;
            field_current_event_actor->pc--;
            return;
        }
        if (field_current_event_actor->unk124 != -1) {
            heap_free(field_current_event_actor->unk120);
            field_current_event_actor->unk124 = -1;
        }
        number = field_event_read_imm_or_var(1);
        cd_select_directory(4, 0);
        file = number + 0x77A;
        size = cd_get_aligned_file_size(file) + 8;
        field_current_event_actor->unk124 = file;
        field_current_event_actor->unk120 = heap_alloc(size, 0);
        cd_read_file(file, field_current_event_actor->unk120, 0, 0x80);
        if (field_event_runs_per_frame == 0) {
            cd_sync_reads(0);
        }
        field_actor_block_loading = 1;
        field_current_event_actor->pc += 3;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008AE5C: Event: set (selector 0) or clear the actor's layer bit 17. */
void field_event_set_layer_driven(void) {
    if (field_event_bytecode[field_current_event_actor->pc + 1] == 0) {
        field_current_event_actor->layer_flags |= 0x20000;
    } else {
        field_current_event_actor->layer_flags &= ~0x20000;
    }
    field_current_event_actor->pc += 2;
}

/* 8008AEC8: Event fe 3d: set row op1 of the 801e layers' light matrix (800b221c, passed
 * to 801e7d14) to operands 3, 5 and 7; all four are selected operands (flags
 * 0x80/0x40/0x20/0x10 of byte 9). */
void field_event_set_layer_light_row(void) {
    s32 index = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 9]);

    field_work.unk221C[index][0] = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_work.unk221C[index][1] = field_event_read_selected_operand_20(5, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_work.unk221C[index][2] = field_event_read_selected_operand_10(7, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_current_event_actor->pc += 10;
}

/* 8008AFD8: Event fe 3e: set column op1 of the 801e layers' colour matrix (800b223c, the
 * module's 801e8644) to operands 3, 5 and 7; all four are selected operands
 * (flags 0x80/0x40/0x20/0x10 of byte 9). */
void field_event_set_layer_color_column(void) {
    s32 index = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 9]);

    field_work.unk223C[0][index] = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_work.unk223C[1][index] = field_event_read_selected_operand_20(5, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_work.unk223C[2][index] = field_event_read_selected_operand_10(7, field_event_bytecode[field_current_event_actor->pc + 9]);
    field_current_event_actor->pc += 10;
}

/* 8008B0E8: Event fe 3f: set the back colour of the 801e layers (800b225c, passed to
 * SetBackColor) to operands 1, 3 and 5. */
void field_event_set_layer_back_color(void) {
    field_work.unk225C[0] = field_event_read_imm_or_var(1);
    field_work.unk225C[1] = field_event_read_imm_or_var(3);
    field_work.unk225C[2] = field_event_read_imm_or_var(5);
    field_current_event_actor->pc += 7;
}

/* 8008B144: Event fe 47: set 800b21b4 from operand 1: the step by which actors with layer
 * flag 0x2000 turn their facing (+108) toward its goal each frame (default
 * 0x80; other actors use their +11e). */
void field_event_set_layer_turn_step(void) {
    field_work.unk21B4 = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 8008B180: Event fe 3c: when 800adb1c is set, call script entry operand 3 of 801e layer
 * actor operand 1 (801e8330) and keep operand 3 in 800b21e4[operand 1], which
 * 800a24c4 replays after a return to the field. */
void field_event_call_layer_script(void) {
    s32 index;

    if (field_event_runs_per_frame != 0) {
        index = field_event_read_imm_or_var(1) & 0xFFFF;
        gear_model_select_and_call_entry(index, 0, field_event_read_imm_or_var(3));
        field_work.unk21E4[field_event_read_imm_or_var(1)] = field_event_read_imm_or_var(3);
    }
    field_current_event_actor->pc += 5;
}

/* 8008B210: Event fe 5b: set the current actor's turn step (+11e, default 0x200: the step
 * by which its facing (+108) turns toward its goal each frame; actors with
 * layer flag 0x2000 use 800b21b4) from operand 1. */
void field_event_set_turn_step(void) {
    field_current_event_actor->unk11E = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 8008B248: Event: fade channel 1 towards (op3, op5, op7) over op9 frames with blend
 * op1. */
void field_event_fade(void) {
    s32 steps = field_event_read_imm_or_var(9);
    s32 red = field_event_read_imm_or_var(3);
    s32 green = field_event_read_imm_or_var(5);
    s32 blue = field_event_read_imm_or_var(7);

    field_fade_start(1, steps, red, green, blue, field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 11;
}

/* 8008B2F0: Event fe 26: start the screen distortion (800a484c(0): its buffers and quad
 * grid on first use) and move its six values (x and y amplitude, x and y
 * frequency, x and y phase speed) to operands 1, 3, 5, 7, 9 and 11 over
 * operand-13 frames (800a4cc4); advance 15. */
void field_event_screen_distortion(void) {
    field_distortion_start(0);
    field_current_event_actor->pc += 15;
}

/* 8008B328: Event fe 27: screen distortion control by byte 1, yielding: 0 releases it
 * (800b207a), moving its six values to 0 over operand-2 frames (800a4cc4),
 * after which the drawer stops it, and advances 4; 1 waits (pc-- to the fe)
 * while it runs (800b2078), then advances 2; 2 clears 800b2078 and advances 2;
 * 3 stops it and frees its buffers (800a47d4) and advances 2. Other values do
 * not advance: the pc stays on the extended byte, which runs as primary 27 when
 * the slot resumes. */
void field_event_distortion(void) {
    switch (field_event_bytecode[field_current_event_actor->pc + 1]) {
    case 0:
        field_distortion_set_targets(0, 0, 0, 0, 0, 0, field_event_read_imm_or_var(2));
        field_work.unk207A = 1;
        field_current_event_actor->pc += 4;
        break;
    case 1:
        if (field_work.unk2078 == 0) {
            field_current_event_actor->pc += 2;
        } else {
            field_current_event_actor->pc--;
        }
        break;
    case 2:
        field_work.unk2078 = 0;
        field_current_event_actor->pc += 2;
        break;
    case 3:
        field_distortion_stop();
        field_current_event_actor->pc += 2;
        break;
    }
    field_event_yield_requested = 1;
}

/* 8008B45C: Event: set the sprite view rotation from operands 1, 3 and 5 (X, Z, Y;
 * immediate by flags 0x80/0x40/0x20 of byte 7). */
void field_event_set_sprite_angles(void) {
    field_work.sprite_angles.vx = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_work.sprite_angles.vz = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_work.sprite_angles.vy = field_event_read_selected_operand_20(5, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_current_event_actor->pc += 8;
}

/* 8008B518: Event: set the camera orbit angles from operands 1, 3 and 5 (X, Z, Y;
 * immediate by flags 0x80/0x40/0x20 of byte 7). */
void field_event_set_orbit_angles(void) {
    field_view.orbit_angles.vx = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_view.orbit_angles.vz = field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_view.orbit_angles.vy = field_event_read_selected_operand_20(5, field_event_bytecode[field_current_event_actor->pc + 7]);
    field_current_event_actor->pc += 8;
}

/* 8008B5D4: Event 0x1b: scroll the current actor's textured polygons by (op1, op3)
 * texels in both draw buffers. Each polygon's two group words are stepped
 * over one at a time (the two steps combine into one add, but count twice
 * in allocation, which gives group s1, other s2, prims s3). */
void field_event_scroll_texture(void) {
    FieldInstance *instance;
    POLY_FT3 *ft3;
    POLY_FT3 *ft3_other;
    POLY_FT4 *ft4;
    POLY_FT4 *ft4_other;
    u8 *prims;
    u8 *other;
    SpriteModel *mesh;
    u32 *group;
    s32 du;
    s32 dv;
    s32 groups;
    u32 header;
    s32 count;
    s32 code;
    s32 i;

    instance = field_view.components.descriptors[field_current_event_actor_index].instance;
    prims = instance->packets[field_draw_buffer_index];
    mesh = instance->mesh;
    other = instance->packets[(field_draw_buffer_index + 1) & 1];
    group = (u32 *)mesh->unk10;
    du = (s16)field_event_read_s16(1);
    dv = (s16)field_event_read_s16(3);
    for (groups = mesh->group_count; groups > 0; groups--) {
        header = *group;
        code = header & 0xFF;
        count = header >> 16;
        if (code == 0xC4 || code == 0xC8) {
            group++;
        } else {
            group++;
            if (!(header & 8)) {
                ft3 = (POLY_FT3 *)prims;
                ft3_other = (POLY_FT3 *)other;
                for (i = 0; i < count; i++) {
                    ft3->u0 += du;
                    ft3->u1 += du;
                    ft3->u2 += du;
                    ft3->v0 += dv;
                    ft3->v1 += dv;
                    ft3->v2 += dv;
                    ft3_other->u0 = ft3->u0;
                    ft3_other->u1 = ft3->u1;
                    ft3_other->u2 = ft3->u2;
                    ft3_other->v0 = ft3->v0;
                    ft3_other->v1 = ft3->v1;
                    ft3_other->v2 = ft3->v2;
                    group++;
                    group++;
                    ft3++;
                    ft3_other++;
                }
                prims = (u8 *)ft3;
                other = (u8 *)ft3_other;
            } else {
                ft4 = (POLY_FT4 *)prims;
                ft4_other = (POLY_FT4 *)other;
                for (i = 0; i < count; i++) {
                    ft4->u0 += du;
                    ft4->u1 += du;
                    ft4->u2 += du;
                    ft4->u3 += du;
                    ft4->v0 += dv;
                    ft4->v1 += dv;
                    ft4->v2 += dv;
                    ft4->v3 += dv;
                    ft4_other->u0 = ft4->u0;
                    ft4_other->u1 = ft4->u1;
                    ft4_other->u2 = ft4->u2;
                    ft4_other->u3 = ft4->u3;
                    ft4_other->v0 = ft4->v0;
                    ft4_other->v1 = ft4->v1;
                    ft4_other->v2 = ft4->v2;
                    ft4_other->v3 = ft4->v3;
                    group++;
                    group++;
                    ft4++;
                    ft4_other++;
                }
                prims = (u8 *)ft4;
                other = (u8 *)ft4_other;
            }
        }
    }
    field_current_event_actor->pc += 5;
}

/* 8008B894: Event fe 1a: once a party sprite load is pending (800adbc4) and the disc is
 * idle: stop the stream, unpack the member's sprite file into its slot block
 * (8005a414) and release the read buffer, run the member's join event
 * (8008b978: the actor event 0 that starts with opcode 16 for it), clear the
 * pending load and advance. Otherwise wait (pc-- to the fe), also when no load
 * is pending. Yields. */
void field_event_apply_party_sprite(void) {
    if (field_party_sprite_load_pending != 0xFF && cd_get_pending_read_count() == 0) {
        cd_sync_reads(0);
        text_unpack_lzss(field_party_sprite_load_buffer, mode_party_sprite_blocks[field_party_sprite_load_slot]);
        heap_free(field_party_sprite_load_buffer);
        field_party_run_join_event(field_party_sprite_load_member);
        field_party_sprite_load_pending = 0xFF;
        field_event_yield_requested = 1;
        field_current_event_actor->pc++;
        return;
    }
    field_event_yield_requested = 1;
    field_current_event_actor->pc--;
}

void field_actor_copy_placement(s32 to, s32 from);

/* 8008B978: Run the join event of party member `member`: the first event actor whose
 * event 0 starts with instruction 0x16 for that member is initialised and
 * run (with the member's own actor while 8004f34c has 0xc000); the running
 * actor's context is restored afterwards. */
void field_party_run_join_event(s32 member) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 yield;
    s32 current;
    s32 steps;
    s32 pc;
    s32 i;
    s32 entry;
    u8 *code;
    s32 pc_new;

    game_current_data->joined |= 1 << field_party_sprite_load_member;
    descriptor = field_current_event_descriptor;
    actor = field_current_event_actor;
    if (field_event_runs_per_frame != 0) {
        pc = actor->pc;
        yield = field_event_yield_requested;
        steps = field_event_batch_limit;
        current = field_current_event_actor_index;
        for (i = 0; i < field_event_actor_count; i++) {
            entry = field_event_get_entry_pc(i, 0);
            code = &field_event_bytecode[entry];
            if (code[0] == 0x16 && code[1] == member) {
                field_current_event_descriptor = &field_view.components.descriptors[i];
                field_current_event_actor = field_current_event_descriptor->actor;
                field_actor_reset(i);
                field_current_event_actor_index = i;
                field_view.components.descriptors[i].actor->pc = entry;
                pc_new = field_event_get_entry_pc(i, 0);
                field_event_yield_ends_run = 0;
                field_current_event_actor->pc = pc_new;
                entry = field_event_get_entry_pc(i, 0);
                field_actor_copy_placement(i, field_work.controlled);
                field_event_run_instructions(0xFFFF);
                field_party_place_at_controlled();
                game_current_data->joined |= 1 << field_party_sprite_load_member;
                if (mode_field_map_id & 0xC000) {
                    field_current_event_descriptor = &field_view.components.descriptors[mode_party_stand_in_actors[field_party_sprite_load_slot]];
                    field_current_event_actor = field_current_event_descriptor->actor;
                    field_actor_reset(mode_party_stand_in_actors[field_party_sprite_load_slot]);
                    field_view.components.descriptors[i].actor->pc = entry;
                    field_current_event_actor_index = mode_party_stand_in_actors[field_party_sprite_load_slot];
                    pc_new = field_event_get_entry_pc(mode_party_stand_in_actors[field_party_sprite_load_slot], 0);
                    field_event_yield_ends_run = 0;
                    field_current_event_actor->pc = pc_new;
                    field_event_run_instructions(0xFFFF);
                }
                break;
            }
        }
        field_current_event_descriptor = descriptor;
        field_current_event_actor = actor;
        field_event_yield_requested = yield;
        field_current_event_actor_index = current;
        field_event_batch_limit = steps;
        actor->pc = pc;
    }
}

/* 8008BC80: Event: once no party sprite load is pending, 800adb2c is clear and the disc
 * is idle (else pc-- back to fe and yield), add character operand 1 to the
 * party: with a free slot read its sprite file (8008a7dc) and advance 3; when
 * it is already in the party or the party is full, mark it waiting (+1d30 bit)
 * and advance 5, two bytes further; operand 1 0xff advances 5. */
void field_event_join_party(void) {
    s32 pending = field_party_sprite_load_pending;
    s32 member;
    s32 slot;

    if (pending == 0xFF && field_music_stream_running == 0 && field_music_is_stream_or_disc_busy() == 0) {
        cd_sync_reads(0);
        member = field_event_read_imm_or_var(1);
        if (member != pending) {
            if (field_party_find_free_slot(member, &slot) == 0) {
                game_current_data->inGear[slot] = 0;
                mode_party_members[slot] = member;
                field_party_read_member_sprite(member, slot);
                field_current_event_actor->pc += 3;
                return;
            }
            game_current_data->joined |= 1 << member;
            field_current_event_actor->pc += 5;
            return;
        }
        field_current_event_actor->pc += 5;
        return;
    }
    field_event_yield_requested = 1;
    field_current_event_actor->pc--;
}

/* 8008BDD8: Event fe 18: once no party sprite load is pending (800adbc4), no music-wave
 * read runs and the stream is stopped (8008a558; else yield and wait: pc-- to
 * the fe), add the character in byte 1 to the first free party slot (8008a790),
 * clearing the slot's +22b1, and start reading its sprite file (8008a7dc),
 * advancing 2. When it is already in the party or no slot is free, set its bit
 * in the game's +1d30 instead and advance 4, past the next two bytes. */
void field_event_join_party_byte(void) {
    s32 slot;
    s32 member;

    if (field_party_sprite_load_pending == 0xFF && field_music_stream_running == 0 && field_music_is_stream_or_disc_busy() == 0) {
        cd_sync_reads(0);
        if (field_party_find_free_slot(field_event_bytecode[field_current_event_actor->pc + 1], &slot) == 0) {
            game_current_data->inGear[slot] = 0;
            member = field_event_bytecode[field_current_event_actor->pc + 1];
            mode_party_members[slot] = member;
            field_party_read_member_sprite(member, slot);
            field_current_event_actor->pc += 2;
            return;
        }
        game_current_data->joined |= 1 << field_event_bytecode[field_current_event_actor->pc + 1];
        field_current_event_actor->pc += 4;
        return;
    }
    field_event_yield_requested = 1;
    field_current_event_actor->pc--;
}

/* 8008BF38: Close party slot `slot` up: the next slot's member, sprite data and ids
 * move down (its actor's sprite is set up again for this slot) and the next
 * slot is emptied. */
void field_party_close_up_slot(s32 slot) {
    s32 member;
    s32 kind;
    s32 *sprites;

    member = mode_party_actors[slot + 1];
    if (member == 0xFF) {
        mode_party_file_ids[slot] = mode_party_file_ids[slot + 1];
        mode_party_members[slot] = mode_party_members[slot + 1];
        mode_party_actors[slot] = mode_party_actors[slot + 1];
        mode_party_members[slot + 1] = member;
        mode_party_file_ids[slot + 1] = member;
        mode_party_actors[slot + 1] = member;
        return;
    }
    *(PartySprite *)mode_party_sprite_blocks[slot] = *(PartySprite *)mode_party_sprite_blocks[slot + 1];
    mode_party_file_ids[slot] = mode_party_file_ids[slot + 1];
    mode_party_members[slot] = mode_party_members[slot + 1];
    mode_party_actors[slot] = mode_party_actors[slot + 1];
    kind = field_view.components.descriptors[member].actor->unk126;
    if (!(kind & 0x80)) {
        field_actor_create_sprite(member, slot, mode_party_sprite_blocks[slot], 1, 0, slot, 1);
    } else {
        sprites = field_view.components.sprites;
        field_actor_create_sprite(member, field_view.components.descriptors[member].actor->unk127,
                      (u8 *)(sprites[(kind & 0x7F) + 1] + (s32)sprites),
                      field_view.components.descriptors[member].actor->sprite_kind,
                      field_view.components.descriptors[member].actor->unk134 & 0xF,
                      field_view.components.descriptors[member].actor->unk126,
                      (field_view.components.descriptors[member].actor->unk134 >> 4) & 1);
    }
    mode_party_members[slot + 1] = 0xFF;
    mode_party_file_ids[slot + 1] = 0xFF;
    mode_party_actors[slot + 1] = 0xFF;
}

/* 8008C180: Take party slot `slot`'s member out of the party: its actor gets the
 * lead's sprite and is hidden, and the slot's ids are cleared. The
 * descriptor table pointer is read through its address, which the original
 * keeps in s0 across the calls.
 * An empty slot only clears its two other ids (an early return; the
 * duplicated stores give slot * 4 the original's allocation priority). */
void field_party_remove_slot_member(s32 slot) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    FieldActor *member;
    s32 current;
    u16 pc;
    s32 index;
    FieldDescriptor **table;
    FieldDescriptor *member_descriptor;

    if (mode_party_actors[slot] == 0xFF) {
        mode_party_members[slot] = 0xFF;
        mode_party_file_ids[slot] = 0xFF;
        return;
    }
    table = &field_view.components.descriptors;
    descriptor = field_current_event_descriptor;
    actor = field_current_event_actor;
    current = field_current_event_actor_index;
    pc = actor->pc;
    field_current_event_descriptor = &(*table)[mode_party_actors[slot]];
    field_current_event_actor = field_current_event_descriptor->actor;
    field_actor_reset(mode_party_actors[slot]);
    index = mode_party_actors[slot];
    field_current_event_actor_index = index;
    member_descriptor = &(*table)[index];
    member_descriptor->flags = (member_descriptor->flags & 0xF07F) | 0x200;
    field_actor_create_sprite(index, 0, mode_party_sprite_blocks[0], 1, 0, 0, 1);
    member = field_current_event_actor;
    member->flags |= 1;
    member->layer_flags |= 0x100000;
    field_event_yield_requested = 0;
    field_current_event_actor = actor;
    field_current_event_descriptor = descriptor;
    field_current_event_actor_index = current;
    member->pc = pc;
    member->flags |= 0x20000;
    member->layer_flags |= 0x400;
    mode_party_actors[slot] = 0xFF;
    mode_party_members[slot] = 0xFF;
    mode_party_file_ids[slot] = 0xFF;
}

/* 8008C334: Event 0x19: remove character op1 from the party once no sprite load is
 * pending. Before the field is set up only the slot tables and sprite data
 * close up; afterwards the member's actor is also released and the lead
 * actor becomes the party leader again. */
void field_event_leave_party(void) {
    s32 slot;

    if (field_party_sprite_load_pending != 0xFF) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc -= 1;
        return;
    }
    DrawSync(0);
    slot = field_party_find_character_slot(field_event_resolve_character(EVENT_OPERAND_BYTE(1)));
    if (slot != -1) {
        if (field_event_runs_per_frame == 0) {
            switch (slot) {
            case 0:
                if (mode_party_members[1] == 0xFF) {
                    mode_party_members[0] = 0xFF;
                    mode_party_file_ids[0] = 0xFF;
                    game_current_data->inGear[0] = 0;
                } else {
                    *(PartySprite *)mode_party_sprite_blocks[0] = *(PartySprite *)mode_party_sprite_blocks[1];
                    mode_party_members[0] = mode_party_members[1];
                    mode_party_file_ids[0] = mode_party_file_ids[1];
                    mode_party_members[1] = 0xFF;
                    mode_party_file_ids[1] = 0xFF;
                    game_current_data->inGear[0] = game_current_data->inGear[1];
                    if (mode_party_members[2] != 0xFF) {
                        *(PartySprite *)mode_party_sprite_blocks[1] = *(PartySprite *)mode_party_sprite_blocks[2];
                        mode_party_members[1] = mode_party_members[2];
                        mode_party_file_ids[1] = mode_party_file_ids[2];
                        mode_party_members[2] = 0xFF;
                        mode_party_file_ids[2] = 0xFF;
                        game_current_data->inGear[1] = game_current_data->inGear[2];
                    }
                }
                break;
            case 1:
                if (mode_party_members[2] == 0xFF) {
                    mode_party_members[1] = 0xFF;
                    mode_party_file_ids[1] = 0xFF;
                    game_current_data->inGear[1] = 0;
                } else {
                    *(PartySprite *)mode_party_sprite_blocks[1] = *(PartySprite *)mode_party_sprite_blocks[2];
                    mode_party_members[1] = mode_party_members[2];
                    mode_party_file_ids[1] = mode_party_file_ids[2];
                    mode_party_members[2] = 0xFF;
                    mode_party_file_ids[2] = 0xFF;
                    game_current_data->inGear[1] = game_current_data->inGear[2];
                    game_current_data->inGear[2] = 0;
                }
                break;
            case 2:
                mode_party_members[2] = 0xFF;
                mode_party_file_ids[2] = 0xFF;
                game_current_data->inGear[2] = 0;
                break;
            }
        } else {
            switch (slot) {
            case 0:
                game_current_data->inGear[0] = game_current_data->inGear[1];
                game_current_data->inGear[1] = game_current_data->inGear[2];
                game_current_data->inGear[2] = 0;
                field_party_remove_slot_member(0);
                field_party_close_up_slot(0);
                field_party_close_up_slot(1);
                break;
            case 1:
                game_current_data->inGear[1] = game_current_data->inGear[2];
                game_current_data->inGear[2] = 0;
                field_party_remove_slot_member(1);
                field_party_close_up_slot(1);
                break;
            case 2:
                game_current_data->inGear[2] = 0;
                field_party_remove_slot_member(2);
                break;
            }
            if (mode_party_members[0] != 0xFF && mode_party_actors[0] != 0xFF) {
                field_work.controlled = mode_party_actors[0];
                field_view.components.descriptors[mode_party_actors[0]].actor->flags =
                    (field_view.components.descriptors[mode_party_actors[0]].actor->flags | 0x4400) & ~0x80;
            } else {
                field_work.controlled = 0;
            }
        }
    }
    field_current_event_actor->pc += 2;
}

/* 8008C7D8: Event fe 16: release the current actor's boundary quadrilateral (+114,
 * allocated by opcode 17 and marked by +12c bit 12) when it holds one. */
void field_event_free_boundary(void) {
    if (field_current_event_actor->state.word & 0x1000) {
        heap_free(field_current_event_actor->unk114);
        field_current_event_actor->state.word &= ~0x1000;
    }
    field_current_event_actor->pc++;
}

/* 8008C84C: Event fe 0e: with a sequence playing (8004f36c), fade the current music
 * sequence (80062528) to level operand 1 over operand-3 frames (8003a89c: at
 * once for 0 frames; raising the level of a sequence a fade stopped resumes it)
 * and advance. With none playing, advance when no field track is selected
 * (8004f324 0xff) or 800adb1c is clear, else wait (pc-- to the fe). Yields. */
void field_event_music_fade(void) {
    s32 a;

    if (mode_music_started != 0) {
        a = field_event_read_imm_or_var(1);
        sound_set_seq_fade((SoundSeq *)mode_music_seq, a, field_event_read_imm_or_var(3));
        field_current_event_actor->pc += 5;
    } else if (mode_music_selected_track == 0xFF) {
        field_current_event_actor->pc += 5;
    } else if (field_event_runs_per_frame == 0) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008C938: Event fe 0f: with a sequence playing, shift the current music sequence's
 * pitch to operand 1 semitones over operand-3 frames (8003a948; selected
 * operands, flags 0x80/0x40 of byte 5) and advance; otherwise advance or wait
 * (pc-- to the fe) as fe 0e. Yields. */
void field_event_music_pitch(void) {
    s32 a;

    if (mode_music_started != 0) {
        a = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 5]);
        sound_set_seq_pitch((SoundSeq *)mode_music_seq, a, field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 5]));
        field_current_event_actor->pc += 6;
    } else if (mode_music_selected_track == 0xFF) {
        field_current_event_actor->pc += 6;
    } else if (field_event_runs_per_frame == 0) {
        field_current_event_actor->pc += 6;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008CA60: Event fe 10: with a sequence playing, set the current music sequence's tempo
 * to operand 1 (0: 0x100) over operand-3 frames (8003a838) and advance;
 * otherwise advance or wait (pc-- to the fe) as fe 0e. Yields. */
void field_event_music_tempo(void) {
    s32 a;

    if (mode_music_started != 0) {
        a = field_event_read_imm_or_var(1);
        sound_set_seq_tempo((SoundSeq *)mode_music_seq, a, field_event_read_imm_or_var(3));
        field_current_event_actor->pc += 5;
    } else if (mode_music_selected_track == 0xFF) {
        field_current_event_actor->pc += 5;
    } else if (field_event_runs_per_frame == 0) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008CB4C: Event fe 11: with a sequence playing, set the current music sequence's pan to
 * operand 1 over operand-3 frames (8003a9bc; selected operands, flags 0x80/0x40
 * of byte 5) and advance; otherwise advance or wait (pc-- to the fe) as fe 0e.
 * Yields. */
void field_event_music_pan(void) {
    s32 a;

    if (mode_music_started != 0) {
        a = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 5]);
        sound_set_seq_pan((SoundSeq *)mode_music_seq, a, field_event_read_selected_operand_40(3, field_event_bytecode[field_current_event_actor->pc + 5]));
        field_current_event_actor->pc += 6;
    } else if (mode_music_selected_track == 0xFF) {
        field_current_event_actor->pc += 6;
    } else if (field_event_runs_per_frame == 0) {
        field_current_event_actor->pc += 6;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008CC74: Event fe 12: with a sequence playing, mute the channels of the current music
 * sequence whose bit is set in operand 1 and unmute the others (8003aac4) and
 * advance; otherwise advance or wait (pc-- to the fe) as fe 0e. Yields. */
void field_event_music_mute(void) {
    if (mode_music_started != 0) {
        sound_set_seq_mute_mask((SoundSeq *)mode_music_seq, field_event_read_imm_or_var(1));
        field_current_event_actor->pc += 3;
    } else if (mode_music_selected_track == 0xFF) {
        field_current_event_actor->pc += 3;
    } else if (field_event_runs_per_frame == 0) {
        field_current_event_actor->pc += 3;
    } else {
        field_current_event_actor->pc--;
    }
    field_event_yield_requested = 1;
}

/* 8008CD48: Event fe 13: make the current actor a sound emitter: sound operand 1 at
 * volume operand 3 (+10a, +10c), mode 0 (+10d; 0xff, off, when the sound is 0),
 * first stopping the emitter slot that follows it (800863e8). The emitter
 * update 80086590 plays the three nearest emitters. */
void field_event_set_sound_emitter(void) {
    field_current_event_actor->sound = field_event_read_imm_or_var(1);
    field_current_event_actor->sound_mode = 0;
    field_current_event_actor->sound_volume = field_event_read_imm_or_var(3);
    field_current_event_actor->pc += 5;
    field_sound_stop_emitter(field_current_event_actor_index);
    if (field_current_event_actor->sound == 0) {
        field_current_event_actor->sound_mode = 0xFF;
    }
}

/* 8008CDD4: Event fe 14: as fe 13 with mode 0x80: sound operand 1 at volume operand 3
 * (+10a, +10c), mode 0x80 (+10d; 0xff when the sound is 0), stopping the
 * emitter slot that follows the actor (800863e8). The field overlay only tests
 * the mode against 0xff (80086590). */
void field_event_set_sound_emitter_80(void) {
    field_current_event_actor->sound = field_event_read_imm_or_var(1);
    field_current_event_actor->sound_mode = 0x80;
    field_current_event_actor->sound_volume = field_event_read_imm_or_var(3);
    field_current_event_actor->pc += 5;
    field_sound_stop_emitter(field_current_event_actor_index);
    if (field_current_event_actor->sound == 0) {
        field_current_event_actor->sound_mode = 0xFF;
    }
}

/* 8008CE64: Event fe 3b: clear the bit of character operand 1 (resolved by 8008cf3c:
 * fd/fe/ff the characters in party slots 0/1/2, fc none: unchanged) in the
 * game's +1d32. */
void field_event_clear_party_bit(void) {
    s32 member = field_event_resolve_character(field_event_read_imm_or_var(1));

    if (member != 0xFF) {
        game_current_data->available &= ~(1 << member);
    }
    field_current_event_actor->pc += 3;
}

/* 8008CED0: Event fe 3a: set the bit of character operand 1 (resolved by 8008cf3c:
 * fd/fe/ff the characters in party slots 0/1/2, fc none: unchanged) in the
 * game's +1d32. */
void field_event_set_party_bit(void) {
    s32 member = field_event_resolve_character(field_event_read_imm_or_var(1));

    if (member != 0xFF) {
        game_current_data->available |= 1 << member;
    }
    field_current_event_actor->pc += 3;
}

/* 8008CF3C: Resolve a character id: 0xfd..0xff name the party members, 0xfc none
 * (0xff). */
s32 field_event_resolve_character(s32 id) {
    if (id == 0xFF) {
        return mode_party_members[2];
    }
    if (id == 0xFE) {
        return mode_party_members[1];
    }
    if (id == 0xFD) {
        return mode_party_members[0];
    }
    if (id == 0xFC) {
        return 0xFF;
    }
    return id;
}

/* 8008CF9C: Event fe 0d: set the current actor's character (+80, the dialogue portrait
 * 8009c5a8 shows) to operand 1 resolved by 8008cf3c (fd/fe/ff: the characters
 * in party slots 0/1/2, fc: none). */
void field_event_set_character(void) {
    field_current_event_actor->character = field_event_resolve_character(field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 3;
}

/* 8008CFEC: Event fe 0c: set the six halfwords at 800b21a0 (initially 0x100 three times,
 * then 0x200 three times) from raw operands: [0] op1, [1] op5, [2] op3, [3]
 * op7, [4] op11, [5] op9. */
void field_event_set_unread_work_halfwords(void) {
    field_work.unk21A0[0] = field_event_read_u16(1);
    field_work.unk21A0[2] = field_event_read_u16(3);
    field_work.unk21A0[1] = field_event_read_u16(5);
    field_work.unk21A0[3] = field_event_read_u16(7);
    field_work.unk21A0[5] = field_event_read_u16(9);
    field_work.unk21A0[4] = field_event_read_u16(11);
    field_current_event_actor->pc += 13;
}

/* 8008D078: Event: clear (op1 zero) or set the actor's layer bit 11. */
void field_event_set_shadow_hidden(void) {
    if (field_event_read_imm_or_var(1) == 0) {
        field_current_event_actor->layer_flags &= ~0x800;
    } else {
        field_current_event_actor->layer_flags |= 0x800;
    }
    field_current_event_actor->pc += 3;
}

/* 8008D0F4: Event: scale the current actor by op1 and rebuild its matrix. */
void field_event_set_scale(void) {
    s32 scale = field_event_read_imm_or_var(1);

    field_view.components.descriptors[field_current_event_actor_index].model->scale = (u32)(scale * 3) >> 2;
    field_current_event_actor->scale[0] = scale;
    field_current_event_actor->scale[1] = scale;
    field_current_event_actor->scale[2] = scale;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
    field_current_event_actor->pc += 3;
}

/* 8008D180: Event: scale the current actor by (op1, op3, op5) and rebuild its matrix. */
void field_event_set_scale_xyz(void) {
    s16 x = field_event_read_imm_or_var(1);
    s16 y = field_event_read_imm_or_var(3);
    s16 z = field_event_read_imm_or_var(5);

    field_view.components.descriptors[field_current_event_actor_index].model->scale = 0xC00;
    field_current_event_actor->scale[0] = x;
    field_current_event_actor->scale[1] = y;
    field_current_event_actor->scale[2] = z;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
    field_current_event_actor->pc += 7;
}

/* 8008D230: Event: set the planar offset scale 800b218c (0x1000 at load; 8007b614
 * scales the x/z offsets it builds by it) from operand 1. */
void field_event_set_offset_scale(void) {
    field_work.scale = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 8008D26C: Event: set the current model's +82 to twice operand 1. */
void field_event_set_model_82(void) {
    field_view.components.descriptors[field_current_event_actor_index].model->word82 = field_event_read_imm_or_var(1) * 2;
    field_current_event_actor->pc += 3;
}

/* 8008D2D8: Event fe 00: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 00, which ends the script slot. */
void field_event_rerun_00(void) {
}

/* 8008D2E0: Write a halfword into the event bytecode at `offset`. */
void field_event_write_code_half(s32 value, s32 offset) {
    field_event_bytecode[offset + 1] = value >> 8;
    field_event_bytecode[offset] = value;
}

/* 8008D30C: -1 when two descriptors are at least 16 apart in X/Z, else 0. */
s32 field_actor_is_apart_in_xz(s32 a, s32 b) {
    return -(field_compute_planar_length(field_view.components.descriptors[a].matrix.t[0] - field_view.components.descriptors[b].matrix.t[0],
                           field_view.components.descriptors[a].matrix.t[2] - field_view.components.descriptors[b].matrix.t[2]) >= 0x10);
}

/* 8008D380: Copy descriptor `from`'s actor placement (collision triangles, layer,
 * position, +50, +14, +72, +ec), model position and +84, and matrix
 * translation onto descriptor `to`. */
void field_actor_copy_placement(s32 to, s32 from) {
    FieldActor *target;
    FieldActor *source;
    s32 i;

    source = field_view.components.descriptors[from].actor;
    target = field_view.components.descriptors[to].actor;
    for (i = 0; i < 4; i++) {
        target->triangle[i] = source->triangle[i];
    }
    target->layer = source->layer;
    target->unk50[0] = source->unk50[0];
    target->unk50[1] = source->unk50[1];
    target->unk50[2] = source->unk50[2];
    target->position[0] = source->position[0];
    target->position[1] = source->position[1];
    target->position[2] = source->position[2];
    target->unkEC = source->unkEC;
    target->unk72 = source->unk72;
    target->unk014 = source->unk014;
    field_view.components.descriptors[to].model->ground = field_view.components.descriptors[from].model->ground;
    field_view.components.descriptors[to].model->x = field_view.components.descriptors[from].model->x;
    field_view.components.descriptors[to].model->y = field_view.components.descriptors[from].model->y;
    field_view.components.descriptors[to].model->z = field_view.components.descriptors[from].model->z;
    field_view.components.descriptors[to].matrix.t[0] = field_view.components.descriptors[from].matrix.t[0];
    field_view.components.descriptors[to].matrix.t[1] = field_view.components.descriptors[from].matrix.t[1];
    field_view.components.descriptors[to].matrix.t[2] = field_view.components.descriptors[from].matrix.t[2];
}

/* 8008D570: Clear the current actor's party bit in 800b219f. */
void field_party_clear_current_actor_bit(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (mode_party_actors[i] == field_current_event_actor_index) {
            field_work.party_bits &= ~(1 << i);
        }
    }
}

/* 8008D5C8: Event: set 800b21cd from byte 1: while it is nonzero the camera target's
 * goal height follows the actor (its y - 0x20) instead of the floor. */
void field_event_set_camera_height_follow(void) {
    field_work.camera_floor_fixed = field_event_bytecode[field_current_event_actor->pc + 1];
    field_current_event_actor->pc += 2;
}

/* 8008D604: Event: clear (selector 0) or set (selector 1) the actor's layer bit 10. */
void field_event_set_offscreen_update(void) {
    switch (field_event_bytecode[field_current_event_actor->pc + 1]) {
    case 0:
        field_current_event_actor->layer_flags &= ~0x400;
        break;
    case 1:
        field_current_event_actor->layer_flags |= 0x400;
        break;
    }
    field_current_event_actor->pc += 2;
}

/* 8008D684: Event: set flag bit op1 (variable op1 >> 4, bit op1 & 15). */
void field_event_ext_set_variable_bit(void) {
    u16 reference = field_event_read_u16(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (field_event_read_u16(1) & 0xF);

    field_event_write_variable(variable & 0xFFFF, field_event_read_variable(variable) | bit);
    field_current_event_actor->pc += 3;
}

/* 8008D700: Event: clear flag bit op1 (variable op1 >> 4, bit op1 & 15). */
void field_event_ext_clear_variable_bit(void) {
    u16 reference = field_event_read_u16(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (field_event_read_u16(1) & 0xF);

    field_event_write_variable(variable & 0xFFFF, field_event_read_variable(variable) & ~bit);
    field_current_event_actor->pc += 3;
}

/* 8008D780: Continue (pc + 5) when bit operand 1 & 15 of variable operand 1 >> 4 is
 * set, otherwise jump to operand 3. */
void field_event_branch_unless_bit(void) {
    u16 reference = field_event_read_u16(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (field_event_read_u16(1) & 0xF);

    if (field_event_read_variable(variable) & bit) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

/* 8008D808: Write a 0x57 (arc move, mode 0x81) instruction with operands `a`, `b`,
 * `c` and 0x0c at the pc, followed by ff 57 8f 26 01 80 57 0f. */
void field_event_write_arc_jump(s32 a, s32 b, s32 c) {
    EVENT_OPERAND_BYTE(0) = 0x57;
    EVENT_OPERAND_BYTE(1) = 0x81;
    field_event_write_code_half(a, field_current_event_actor->pc + 2);
    field_event_write_code_half(b, field_current_event_actor->pc + 4);
    field_event_write_code_half(c, field_current_event_actor->pc + 6);
    field_event_write_code_half(0xC, field_current_event_actor->pc + 8);
    EVENT_OPERAND_BYTE(0xA) = 0xFF;
    EVENT_OPERAND_BYTE(0xB) = 0x57;
    EVENT_OPERAND_BYTE(0xC) = 0x8F;
    EVENT_OPERAND_BYTE(0xD) = 0x26;
    EVENT_OPERAND_BYTE(0xE) = 1;
    EVENT_OPERAND_BYTE(0xF) = 0x80;
    EVENT_OPERAND_BYTE(0x10) = 0x57;
    EVENT_OPERAND_BYTE(0x11) = 0xF;
}

/* 8008DA04: Write a 0x4b instruction with operands `a` and `b` into the event code at
 * pc+0xc (followed by ff ff 80) and advance the pc by 0xc. */
void field_event_write_turn_move_to_limited(s32 a, s32 b) {
    EVENT_OPERAND_BYTE(0xC) = 0x4B;
    field_event_write_code_half(a, field_current_event_actor->pc + 0xD);
    field_event_write_code_half(b, field_current_event_actor->pc + 0xF);
    EVENT_OPERAND_BYTE(0x11) = 0xFF;
    EVENT_OPERAND_BYTE(0x12) = 0xFF;
    EVENT_OPERAND_BYTE(0x13) = 0x80;
    field_current_event_actor->pc += 0xC;
}

/* 8008DAFC: Event: clear the actor's +75 (0xff). */
void field_event_clear_link_actor(void) {
    field_current_event_actor->unk075 = 0xFF;
    field_current_event_actor->pc++;
}

/* 8008DB2C: Event: make the actor byte 1 selects the one the camera follows
 * (800b233e). */
void field_event_set_camera_actor(void) {
    field_work.unk233E = field_event_read_actor_index(1);
    field_current_event_actor->pc += 2;
}

/* 8008DB68: Add to party member `member`'s points, capped at its maximum. Declared
 * int but returns nothing. */
s32 field_party_add_gear_hp(s32 member, s32 amount) {
    s32 character = mode_get_character_gear_id(mode_party_members[member]);

    if (character != 0xFF) {
        game_current_data->gears[character].hp += amount;
        if (game_current_data->gears[character].maxHp < game_current_data->gears[character].hp) {
            game_current_data->gears[character].hp = game_current_data->gears[character].maxHp;
        }
    }
}

/* 8008DBF0: Take from party member `member`'s points, leaving at least one. Declared
 * int but returns nothing. */
s32 field_party_take_gear_hp(s32 member, s32 amount) {
    s32 character = mode_get_character_gear_id(mode_party_members[member]);
    s32 points;

    if (character != 0xFF) {
        points = game_current_data->gears[character].hp - amount;
        if (points <= 0) {
            points = 1;
        }
        game_current_data->gears[character].hp = points;
    }
}

/* 8008DC74: Event: add operand 1 (immediate by flag 0x80 of byte 3) to the points of the
 * gears (character record byte 0) of the party members that mask table entry
 * byte 3 & 3 names (800aea2c: all, slot 0, 1, 2), capped at their maximum. */
void field_event_add_gear_hp(void) {
    s32 i;
    s32 amount = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 3]);
    s32 mask = field_event_party_slot_masks[field_event_bytecode[field_current_event_actor->pc + 3] & 3];

    for (i = 0; i < 3; i++) {
        if (mode_party_members[i] != 0xFF && (mask & 1)) {
            field_party_add_gear_hp(i, amount);
        }
        mask >>= 1;
    }
    field_current_event_actor->pc += 4;
}

/* 8008DD6C: Event: take operand 1 (immediate by flag 0x80 of byte 3) from the points of
 * the gears (character record byte 0) of the party members that mask table
 * entry byte 3 & 3 names (800aea2c: all, slot 0, 1, 2), leaving at least 1. */
void field_event_take_gear_hp(void) {
    s32 i;
    s32 amount = field_event_read_selected_operand_80(1, field_event_bytecode[field_current_event_actor->pc + 3]);
    s32 mask = field_event_party_slot_masks[field_event_bytecode[field_current_event_actor->pc + 3] & 3];

    for (i = 0; i < 3; i++) {
        if (mode_party_members[i] != 0xFF && (mask & 1)) {
            field_party_take_gear_hp(i, amount);
        }
        mask >>= 1;
    }
    field_current_event_actor->pc += 4;
}

/* 8008DE64: Set the current actor's +75 to the actor selected by byte 1, if any. */
void field_event_set_link_actor(void) {
    s32 actor = field_event_read_actor_index(1);

    if (actor != 0xFF) {
        field_current_event_actor->unk075 = actor;
    }
    field_current_event_actor->pc += 2;
}

/* 8008DEBC: Event fe 2c: store the flags (+000, low 16 bits) of the actor selected by
 * byte 1 in variable operand 1; the selector byte is the low byte of that same
 * operand. No actor: nothing is stored. */
void field_event_store_actor_flags(void) {
    s32 index = field_event_read_actor_index(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = field_view.components.descriptors[index].actor;
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, actor->flags);
    }
    field_current_event_actor->pc += 3;
}

/* 8008DF44: Event fe 2d: store flag halfword 1 (+002: flags bits 16-31) of the actor
 * selected by byte 1 in variable operand 1; the selector byte is the low byte
 * of that same operand. No actor: nothing is stored. */
void field_event_store_actor_flags1(void) {
    s32 index = field_event_read_actor_index(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = field_view.components.descriptors[index].actor;
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, ACTOR_FLAG_HALF(actor, 1));
    }
    field_current_event_actor->pc += 3;
}

/* 8008DFCC: Event fe 2e: store the layer flags (+004, low 16 bits) of the actor selected
 * by byte 1 in variable operand 1; the selector byte is the low byte of that
 * same operand. No actor: nothing is stored. */
void field_event_store_actor_layer_flags(void) {
    s32 index = field_event_read_actor_index(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = field_view.components.descriptors[index].actor;
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, actor->layer_flags);
    }
    field_current_event_actor->pc += 3;
}

/* 8008E054: Event fe 2f: store flag halfword 3 (+006: layer flags bits 16-31) of the
 * actor selected by byte 1 in variable operand 1; the selector byte is the low
 * byte of that same operand. No actor: nothing is stored. */
void field_event_store_actor_flags3(void) {
    s32 index = field_event_read_actor_index(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = field_view.components.descriptors[index].actor;
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, ACTOR_FLAG_HALF(actor, 3));
    }
    field_current_event_actor->pc += 3;
}

/* 8008E0DC: Continue past a 5-byte instruction when `flags` has any bit of op1, else
 * jump to op4. */
void field_event_branch_unless_actor_flags(s32 flags) {
    if (field_event_read_u16(1) & flags & 0xFFFF) {
        field_current_event_actor->pc += 6;
    } else {
        field_current_event_actor->pc = field_event_read_u16(4);
    }
}

/* 8008E148: Continue past a 4-byte instruction when `flags` has any bit of op1, else
 * jump to op3. */
void field_event_branch_unless_flags(s32 flags) {
    if (field_event_read_u16(1) & flags & 0xFFFF) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

/* 8008E1B4: Event: store the planar distance between the actors selected by bytes 3
 * and 4 (0 when either is missing) in variable op1. */
void field_event_store_actor_distance(void) {
    s32 distance = 0;
    s32 a = field_event_read_actor_index(3);
    s32 b = field_event_read_actor_index(4);
    s32 ax, az, bx, bz;

    if (a != 0xFF && b != 0xFF) {
        ax = field_view.components.descriptors[a].actor->position[0] >> 16;
        bx = field_view.components.descriptors[b].actor->position[0] >> 16;
        az = field_view.components.descriptors[a].actor->position[2] >> 16;
        bz = field_view.components.descriptors[b].actor->position[2] >> 16;
        distance = field_compute_planar_length(ax - bx, az - bz);
    }
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, distance);
    field_current_event_actor->pc += 5;
}

/* 8008E298: Event fe 34: continue (advance 6) when flag halfword 0 (+000: flags bits
 * 0-15) of the actor selected by byte 3 (not checked for none) has any bit of
 * raw operand 1, else jump to operand 4 (8008e0dc). */
void field_event_branch_unless_actor_flags0(void) {
    field_event_branch_unless_actor_flags(ACTOR_FLAG_HALF(field_view.components.descriptors[field_event_read_actor_index(3)].actor, 0));
}

/* 8008E2EC: Event fe 35: continue (advance 6) when flag halfword 1 (+002: flags bits
 * 16-31) of the actor selected by byte 3 (not checked for none) has any bit of
 * raw operand 1, else jump to operand 4 (8008e0dc). */
void field_event_branch_unless_actor_flags1(void) {
    field_event_branch_unless_actor_flags(ACTOR_FLAG_HALF(field_view.components.descriptors[field_event_read_actor_index(3)].actor, 1));
}

/* 8008E340: Event fe 36: continue (advance 6) when flag halfword 2 (+004: layer flags
 * bits 0-15) of the actor selected by byte 3 (not checked for none) has any bit
 * of raw operand 1, else jump to operand 4 (8008e0dc). */
void field_event_branch_unless_actor_flags2(void) {
    field_event_branch_unless_actor_flags(ACTOR_FLAG_HALF(field_view.components.descriptors[field_event_read_actor_index(3)].actor, 2));
}

/* 8008E394: Event fe 37: continue (advance 6) when flag halfword 3 (+006: layer flags
 * bits 16-31) of the actor selected by byte 3 (not checked for none) has any
 * bit of raw operand 1, else jump to operand 4 (8008e0dc). */
void field_event_branch_unless_actor_flags3(void) {
    field_event_branch_unless_actor_flags(ACTOR_FLAG_HALF(field_view.components.descriptors[field_event_read_actor_index(3)].actor, 3));
}

/* 8008E3E8: Event fe 30: continue (advance 5) when the current actor's flag halfword 0
 * (+000: flags bits 0-15) has any bit of raw operand 1, else jump to operand 3
 * (8008e148). */
void field_event_branch_unless_flags0(void) {
    field_event_branch_unless_flags(ACTOR_FLAG_HALF(field_current_event_actor, 0));
}

/* 8008E414: Event fe 31: continue (advance 5) when the current actor's flag halfword 1
 * (+002: flags bits 16-31) has any bit of raw operand 1, else jump to operand 3
 * (8008e148). */
void field_event_branch_unless_flags1(void) {
    field_event_branch_unless_flags(ACTOR_FLAG_HALF(field_current_event_actor, 1));
}

/* 8008E440: Event fe 32: continue (advance 5) when the current actor's flag halfword 2
 * (+004: layer flags bits 0-15) has any bit of raw operand 1, else jump to
 * operand 3 (8008e148). */
void field_event_branch_unless_flags2(void) {
    field_event_branch_unless_flags(ACTOR_FLAG_HALF(field_current_event_actor, 2));
}

/* 8008E46C: Event fe 33: continue (advance 5) when the current actor's flag halfword 3
 * (+006: layer flags bits 16-31) has any bit of raw operand 1, else jump to
 * operand 3 (8008e148). */
void field_event_branch_unless_flags3(void) {
    field_event_branch_unless_flags(ACTOR_FLAG_HALF(field_current_event_actor, 3));
}

/* 8008E498: Store `value` in variable op1. */
void field_event_store_flags(s32 value) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value & 0xFFFF);
    field_current_event_actor->pc += 3;
}

/* 8008E4EC: Event: store the current actor's flag halfword 0 in variable op1. */
void field_event_store_flags0(void) {
    field_event_store_flags(ACTOR_FLAG_HALF(field_current_event_actor, 0));
}

/* 8008E518: Event: store the current actor's flag halfword 1 in variable op1. */
void field_event_store_flags1(void) {
    field_event_store_flags(ACTOR_FLAG_HALF(field_current_event_actor, 1));
}

/* 8008E544: Event: store the current actor's flag halfword 2 in variable op1. */
void field_event_store_flags2(void) {
    field_event_store_flags(ACTOR_FLAG_HALF(field_current_event_actor, 2));
}

/* 8008E570: Event: store the current actor's flag halfword 3 in variable op1. */
void field_event_store_flags3(void) {
    field_event_store_flags(ACTOR_FLAG_HALF(field_current_event_actor, 3));
}

/* 8008E59C: Event: by selector byte 1, set (0-3) or clear (4-7) raw operand 2 in the
 * low or high half of the actor's flag word or layer flag word. */
void field_event_set_flag_bits(void) {
    u32 bits = field_event_read_u16(2) & 0xFFFF;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        field_current_event_actor->flags |= bits;
        break;
    case 1:
        field_current_event_actor->flags |= bits << 16;
        break;
    case 2:
        field_current_event_actor->layer_flags |= bits;
        break;
    case 3:
        field_current_event_actor->layer_flags |= bits << 16;
        break;
    case 4:
        field_current_event_actor->flags &= ~bits;
        break;
    case 5:
        field_current_event_actor->flags &= ~(bits << 16);
        break;
    case 6:
        field_current_event_actor->layer_flags &= ~bits;
        break;
    case 7:
        field_current_event_actor->layer_flags &= ~(bits << 16);
        break;
    }
    field_current_event_actor->pc += 4;
}

/* 8008E718: Draw 800b229c distinct random numbers 1..(800b2298 + 1) into 800b22a0
 * (none when the count is zero, which also clears the range). */
void field_encounter_draw_steps(void) {
    s32 i;
    s32 j;
    s32 pick;

    field_work.unk2294 = field_work.unk2298;
    if (field_work.unk229C == 0) {
        field_work.unk2298 = 0;
        return;
    }
    for (i = 0; i < 32; i++) {
        field_work.unk22A0[i] = 0xFFFF;
    }
    for (i = 0; i < field_work.unk229C; i++) {
    retry:
        pick = (rand() * (field_work.unk2298 + 1)) >> 15 & 0xFFFF;
        for (j = 0; j < 32; j++) {
            if (field_work.unk22A0[j] == pick) {
                goto retry;
            }
        }
        field_work.unk22A0[i] = pick;
    }
    for (i = 0; i < field_work.unk229C; i++) {
        field_work.unk22A0[i]++;
    }
}

/* 8008E85C: Set the range 800b2298 (operand 1) and count 800b229c (operand 3, at most
 * 32), then draw that many distinct random numbers 1..range + 1 into
 * 800b22a0 (8008e718; a zero count only clears the range). */
void field_event_draw_random_picks(void) {
    field_work.unk2298 = field_event_read_imm_or_var(1);
    field_work.unk229C = field_event_read_imm_or_var(3);
    if (field_work.unk229C > 0x20) {
        field_work.unk229C = 0x20;
    }
    field_encounter_draw_steps();
    field_current_event_actor->pc += 5;
}

/* 8008E8C8: Event: by selector byte, clear (0) or set (1, keeping the heading goal)
 * flag 0x8000, or set layer bit 19 (2); clearing also stops a model moving
 * under bit 19. */
void field_event_heading_lock(void) {
    Sprite *model;

    switch (field_event_bytecode[field_current_event_actor->pc + 1]) {
    case 0:
        if (field_current_event_actor->flags & 0x8000) {
            field_current_event_actor->flags &= ~0x8000;
        }
        if (field_current_event_actor->layer_flags & 0x80000) {
            model = field_view.components.descriptors[field_current_event_actor_index].model;
            model->speed = 0;
            model->speed_z = 0;
            model->speed_x = 0;
            field_current_event_actor->layer_flags &= ~0x80000;
        }
        break;
    case 1:
        field_current_event_actor->flags |= 0x8000;
        field_current_event_actor->unk11C = field_current_event_actor->heading_goal;
        break;
    case 2:
        field_current_event_actor->layer_flags |= 0x80000;
        break;
    }
    field_current_event_actor->pc += 2;
}

/* 8008E9F8: Event 61: wait until the field movie player (800a7c58) has started
 * presenting frames (800adb7c), clearing the flag once seen; yield each
 * time. */
void field_event_wait_movie_start(void) {
    if (field_movie_presenting == 0) {
        field_current_event_actor->pc--;
    } else {
        field_movie_presenting = 0;
        field_current_event_actor->pc++;
    }
    field_event_yield_requested = 1;
}

/* 8008EA58: Event 0xa0: while 800adbdc is clear (a battle request is ending the field)
 * pc-- back to fe and yield; otherwise request a movie: file op1, the
 * parameters at 800c3a2a/2c/2e from op3/op5/op7 and its sound bank op9
 * (selected operands, flags byte 11), with the default window and fade;
 * yields. */
void field_event_play_movie_sound(void) {
    if (field_battle_not_requested == 0) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc -= 1;
        return;
    }
    FIELD_MOVIE.file = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.unk2A = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.sound_start = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.unk2E = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.sound_bank = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.width = 0x140;
    field_movie_fade_bits = 0x40;
    FIELD_MOVIE.height = 0x100;
    FIELD_MOVIE.source_x = 0;
    FIELD_MOVIE.depth24 = 1;
    FIELD_MOVIE.y = 0;
    FIELD_MOVIE.x = 0;
    FIELD_MOVIE.source_y = 0x100;
    FIELD_MOVIE.unk3A = 0;
    FIELD_MOVIE.mode &= 0xF;
    field_movie_mode = 0;
    field_movie_requested = 1;
    field_event_yield_requested = 1;
    field_current_event_actor->pc += 0xC;
}

/* 8008EC30: Event 0x60: once sound is available, request movie op1 with parameters
 * op3/op5; op7's low nibble picks the display layout (0: half-width at
 * x 0x140, 1: full 16-bit, 2: full 24-bit) and its 0xc0 bits the fade. */
void field_event_play_movie(void) {
    s32 mode;

    if (field_battle_not_requested == 0) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc -= 1;
        return;
    }
    FIELD_MOVIE.file = field_event_read_imm_or_var(1);
    FIELD_MOVIE.unk2A = field_event_read_imm_or_var(3);
    FIELD_MOVIE.unk2E = field_event_read_imm_or_var(5);
    mode = FIELD_MOVIE.mode = field_event_read_imm_or_var(7);
    FIELD_MOVIE.width = 0x140;
    FIELD_MOVIE.height = 0x100;
    FIELD_MOVIE.mode &= 0xF;
    field_movie_fade_bits = mode & 0xC0;
    FIELD_MOVIE.sound_start = 1;
    switch (FIELD_MOVIE.mode) {
    case 0:
        FIELD_MOVIE.x = 0x140;
        FIELD_MOVIE.y = 0;
        FIELD_MOVIE.source_x = 0x140;
        FIELD_MOVIE.source_y = 0x100;
        field_movie_mode = 1;
        FIELD_MOVIE.depth24 = 0;
        break;
    case 1:
        FIELD_MOVIE.source_x = 0;
        FIELD_MOVIE.y = 0;
        FIELD_MOVIE.x = 0;
        FIELD_MOVIE.source_y = 0x100;
        field_movie_mode = 0;
        FIELD_MOVIE.depth24 = 0;
        break;
    case 2:
        FIELD_MOVIE.source_x = 0;
        FIELD_MOVIE.y = 0;
        FIELD_MOVIE.x = 0;
        FIELD_MOVIE.source_y = 0x100;
        field_movie_mode = 0;
        FIELD_MOVIE.depth24 = 1;
        break;
    }
    FIELD_MOVIE.sound_bank = 0xFF;
    FIELD_MOVIE.unk3A = 0;
    field_movie_requested = 1;
    field_event_yield_requested = 1;
    field_current_event_actor->pc += 9;
}

/* 8008EE14: Event 0x67: request movie op1 with parameters op3/op5/op7, mode op9 (0xfe
 * marks 800c3a3a, 0x40 the fade), window position op11/op13 and size
 * op15/op17, without a sound bank. */
void field_event_play_movie_window(void) {
    s32 mode;
    u16 x;
    u16 y;

    FIELD_MOVIE.file = field_event_read_imm_or_var(1);
    FIELD_MOVIE.unk2A = field_event_read_imm_or_var(3);
    FIELD_MOVIE.sound_start = field_event_read_imm_or_var(5);
    FIELD_MOVIE.unk2E = field_event_read_imm_or_var(7);
    FIELD_MOVIE.mode = field_event_read_imm_or_var(9);
    mode = FIELD_MOVIE.mode;
    if (mode == 0xFE) {
        FIELD_MOVIE.unk3A = 1;
    } else {
        FIELD_MOVIE.unk3A = 0;
    }
    x = field_event_read_imm_or_var(0xB);
    FIELD_MOVIE.x = x;
    FIELD_MOVIE.source_x = x;
    y = field_event_read_imm_or_var(0xD);
    FIELD_MOVIE.y = y;
    FIELD_MOVIE.source_y = y;
    FIELD_MOVIE.width = field_event_read_imm_or_var(0xF);
    FIELD_MOVIE.height = field_event_read_imm_or_var(0x11);
    field_movie_fade_bits = mode & 0x40;
    FIELD_MOVIE.sound_bank = 0xFF;
    field_movie_mode = 2;
    FIELD_MOVIE.depth24 = 0;
    FIELD_MOVIE.mode &= 0xF;
    field_movie_requested = 1;
    field_current_event_actor->pc += 0x13;
}

/* 8008EF5C: Event: request screen transition 1 over operand 1 frames (800adb38/800adb3c):
 * 800a5924 resets the screen effect view (800a4748) and fades the screen pieces
 * out. */
void field_event_request_transition1(void) {
    field_transition_frames = field_event_read_imm_or_var(1);
    field_transition_kind = 1;
    field_current_event_actor->pc += 3;
}

/* 8008EFA0: Event: request screen transition 2 over operand 1 frames (800adb38/800adb3c):
 * 800a5924 fades the screen pieces out. */
void field_event_request_transition2(void) {
    field_transition_frames = field_event_read_imm_or_var(1);
    field_transition_kind = 2;
    field_current_event_actor->pc += 3;
}

/* 8008EFE4: Event: set both draw buffers' clip areas to x operand 1, y operand 3, width
 * operand 5 and height operand 7 (80071f64; the second buffer's lies 0x100
 * lines lower). */
void field_event_set_clip(void) {
    s32 x = field_event_read_imm_or_var(1);
    s32 y = field_event_read_imm_or_var(3);
    s32 w = field_event_read_imm_or_var(5);

    field_draw_set_clip_areas(x, y, w, field_event_read_imm_or_var(7));
    field_current_event_actor->pc += 9;
}

/* 8008F070: Event: request screen transition 3 over operand 1 frames (800adb38/800adb3c):
 * 800a5924 fades the screen pieces in and holds them while the request stays
 * 3. */
void field_event_request_transition3(void) {
    field_transition_frames = field_event_read_imm_or_var(1);
    field_transition_kind = 3;
    field_current_event_actor->pc += 3;
}

/* 8008F0B4: Set the sprite tints of the actor byte 2 selects: byte 1 bit 0 sets its first
 * triple (+fc) and bit 1 its second (+ff) to operands 3, 5 and 7. */
void field_event_set_actor_colors(void) {
    FieldActor *actor;
    s32 index;

    index = field_event_read_actor_index(2);
    if (index != 0xFF) {
        actor = field_view.components.descriptors[index].actor;
        if (EVENT_OPERAND_BYTE(1) & 1) {
            actor->color0[0] = field_event_read_imm_or_var(3);
            actor->color0[1] = field_event_read_imm_or_var(5);
            actor->color0[2] = field_event_read_imm_or_var(7);
        }
        if (EVENT_OPERAND_BYTE(1) & 2) {
            actor->color1[0] = field_event_read_imm_or_var(3);
            actor->color1[1] = field_event_read_imm_or_var(5);
            actor->color1[2] = field_event_read_imm_or_var(7);
        }
    }
    field_current_event_actor->pc += 9;
}

/* 8008F1C8: Event fe 5f: set the current actor's colour triple +fc (bit 0 of byte 1)
 * and/or +ff (bit 1) to operands 2, 4 and 6. */
void field_event_set_colors(void) {
    if (EVENT_OPERAND_BYTE(1) & 1) {
        field_current_event_actor->color0[0] = field_event_read_imm_or_var(2);
        field_current_event_actor->color0[1] = field_event_read_imm_or_var(4);
        field_current_event_actor->color0[2] = field_event_read_imm_or_var(6);
    }
    if (EVENT_OPERAND_BYTE(1) & 2) {
        field_current_event_actor->color1[0] = field_event_read_imm_or_var(2);
        field_current_event_actor->color1[1] = field_event_read_imm_or_var(4);
        field_current_event_actor->color1[2] = field_event_read_imm_or_var(6);
    }
    field_current_event_actor->pc += 8;
}

/* 8008F2D8: Event fe 5e: set the blend rate of the current actor's sprite to operand 1 &
 * 7 (80023290; 0 clears its blending flag). */
void field_event_set_sprite_blend(void) {
    s32 value = field_event_read_imm_or_var(1);

    sprite_set_blend_rate(field_view.components.descriptors[field_current_event_actor_index].model, value);
    field_current_event_actor->pc += 3;
}

/* 8008F348: Set the screen margins 800c3a5c (x) and 800c3a60 (y) from operands 1 and 3:
 * 800aaa74 widens the screen by them when testing whether a field model
 * instance is visible. */
void field_event_set_visibility_margins(void) {
    field_model_cull_margin_x = field_event_read_imm_or_var(1);
    field_model_cull_margin_y = field_event_read_imm_or_var(3);
    field_current_event_actor->pc += 5;
}

/* 8008F394: Set 800b233c from operand 1: bit i keeps sound effect 2 * i playing when
 * 800864f0 stops the field's sounds (it stops the other effects' two channels,
 * 8003a20c, and shifts the bits out). */
void field_event_set_effects_kept(void) {
    field_work.effects_kept = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 8008F3D0: Event: slide the volume of the two effect channels of sound 2 * operand 3 to
 * operand 1 over operand 5 frames (resident 8003a450). */
void field_event_slide_voice_volume(void) {
    s32 a;
    s32 b;

    a = field_event_read_imm_or_var(3) * 2;
    b = field_event_read_imm_or_var(1);
    sound_slide_effect_volume_on_channel(a, b, field_event_read_imm_or_var(5));
    field_current_event_actor->pc += 7;
}

/* 8008F444: Event fe 62: set the volume of the two effect channels of voice pair operand
 * 3 (channels (2 * op3) ^ 8 and the next, while they play) to operand 1
 * (8003a344). */
void field_event_set_voice_volume(void) {
    s32 a;

    a = field_event_read_imm_or_var(3) * 2;
    sound_set_effect_volume_on_channel(a, field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 5;
}

/* 8008F4A0: Event fe 63: set the pan of the two effect channels of voice pair operand 3
 * (channels (2 * op3) ^ 8 and the next, while they play) to operand 1
 * (8003a55c). */
void field_event_set_voice_pan(void) {
    s32 a;

    a = field_event_read_imm_or_var(3) * 2;
    sound_set_effect_pan_on_channel(a, field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 5;
}

/* 8008F4FC: Event fe 65: play sound effect operand 1 on effect voice pair operand 3 at
 * full volume and centre pan, recording it in 800b2078 last_sound_effect;
 * effect 0 stops the pair instead (80085634). */
void field_event_play_sound_effect_pair(void) {
    s32 a;

    a = field_event_read_imm_or_var(1);
    field_sound_play_effect(a, field_event_read_imm_or_var(3));
    field_current_event_actor->pc += 5;
}

/* 8008F558: Event fe 66: play sound effect operand 1 on effect voice pair operand 7 at
 * volume operand 5 and pan operand 3, stopping the pair first (800855c8). */
void field_event_play_sound_effect_full(void) {
    s32 a;
    s32 b;
    s32 c;

    a = field_event_read_imm_or_var(1);
    b = field_event_read_imm_or_var(5);
    c = field_event_read_imm_or_var(3);
    field_sound_play_effect_volume_pan(a, b, c, field_event_read_imm_or_var(7));
    field_current_event_actor->pc += 9;
}

/* 8008F5E4: Event fe 64: wait (pc-- to the fe) while any effect channel whose bit is set
 * in operand 1 << 8 is playing (8003a5d0(-1): the mask of active effect
 * channels), then advance. Yields. */
void field_event_wait_sound_channels(void) {
    s32 buttons;

    buttons = sound_get_active_effect_mask(-1);
    if (!(buttons & (field_event_read_imm_or_var(1) << 8))) {
        field_current_event_actor->pc += 3;
    } else {
        field_current_event_actor->pc -= 1;
    }
    field_event_yield_requested = 1;
}

/* 8008F668: Play sound effect operand 1 on voice pair 3 at full volume and centre pan
 * (80085634; 0 stops the pair). */
void field_event_play_sound(void) {
    field_sound_play_effect(field_event_read_imm_or_var(1), 3);
    field_current_event_actor->pc += 3;
}

/* 8008F6AC: Event fe 5d: play sound effect operand 1 on effect voice pair 3 at volume
 * operand 5 and pan operand 3, stopping the pair first (800855c8). */
void field_event_play_sound_effect(void) {
    s32 a;
    s32 b;

    a = field_event_read_imm_or_var(1);
    b = field_event_read_imm_or_var(5);
    field_sound_play_effect_volume_pan(a, b, field_event_read_imm_or_var(3), 3);
    field_current_event_actor->pc += 7;
}

void field_event_select_music_track(void);

/* 8008F724: Select field music track operand 1 (8008f7b8) with 8004f340 = 0 (its sequence
 * starts silent). With 800adb1c clear (scripts run at setup) the track is only
 * recorded, stopping the old music when it differs; otherwise it yields while
 * the disc stream or a wave load is busy or a change is pending, then stops the
 * old track and starts the new one when it differs (80085b20). Continues at +3;
 * yields while music is disabled (800adbdc clear). */
void field_event_select_music(void) {
    if (field_battle_not_requested == 0) {
        field_event_yield_requested = 1;
        return;
    }
    mode_music_start_full_volume = 0;
    field_event_select_music_track();
}

/* 8008F76C: Select field music track operand 1 as 72 with 8004f340 = -1 (its sequence
 * starts at full volume); yields while music is disabled (800adbdc clear). */
void field_event_select_music_keep(void) {
    if (field_battle_not_requested == 0) {
        field_event_yield_requested = 1;
        return;
    }
    mode_music_start_full_volume = -1;
    field_event_select_music_track();
}

/* 8008F7B8: Select the field music track (operand 1). Without field_event_runs_per_frame the track is
 * only recorded; otherwise yield until the music system can take a change.
 * The two "busy" yields are separate branches that GCC merges into one tail
 * after the change branch, storing the comparison's constant 1. */
void field_event_select_music_track(void) {
    s32 track;

    track = field_event_read_imm_or_var(1);
    if (field_event_runs_per_frame == 0) {
        field_music_release_cached_seq();
        if (track != mode_music_selected_track) {
            mode_stop_music();
            mode_music_load_pending = -1;
        }
        mode_music_selected_track = track;
        field_current_event_actor->pc += 3;
    } else if (field_music_is_stream_or_disc_busy() != 0 || field_battle_not_requested == 0) {
        field_event_yield_requested = 1;
    } else if (mode_music_wave_streaming == 1) {
        field_event_yield_requested = 1;
    } else if (mode_music_load_pending != -1) {
        if (track != mode_music_selected_track) {
            mode_stop_music();
            mode_music_selected_track = track;
            mode_music_load_pending = -1;
            field_music_change_track(track, 0);
        }
        field_current_event_actor->pc += 3;
    } else {
        field_event_yield_requested = 1;
    }
}

/* 8008F90C: Start a camera shake toward amplitudes x operand 1, y operand 3 and z
 * operand 5 over operand-7 frames (at least 1); all zero ends the shake
 * (stop flag, two extra frames). */
void field_event_shake_camera(void) {
    s32 x;
    s32 y;
    s32 z;
    s32 frames;

    x = field_event_read_imm_or_var(1);
    y = field_event_read_imm_or_var(3);
    z = field_event_read_imm_or_var(5);
    frames = field_event_read_imm_or_var(7);
    if (frames == 0) {
        frames = 1;
    }
    field_current_event_actor->pc += 9;
    field_view.shake = 1;
    field_view.shake_time = frames;
    field_view.shake_step[0] = ((x << 16) - field_view.shake_amplitude[0]) / frames;
    field_view.shake_step[1] = ((z << 16) - field_view.shake_amplitude[1]) / frames;
    field_view.shake_step[2] = ((y << 16) - field_view.shake_amplitude[1]) / frames;
    if (x == 0 && z == 0 && y == 0) {
        field_view.shake_time = frames + 2;
        field_view.shake_stop = 1;
        return;
    }
    field_view.shake_stop = 0;
}

/* 8008FA38: Yield; advance once the camera moves operand 1 names (bit 1: target, bit
 * 0: eye) have no steps left. */
void field_event_wait_camera_move(void) {
    s32 mask;
    s32 wanted;

    wanted = field_event_read_imm_or_var(1);
    mask = 3;
    if (field_view.target_steps == 0) {
        mask = 2;
    }
    if (field_view.eye_steps == 0) {
        mask &= 1;
    }
    field_event_yield_requested = 1;
    if (!(mask & wanted)) {
        field_current_event_actor->pc += 3;
    }
}

/* 8008FABC: Event fe 6e: set the camera heading (heading_angles.vy and heading) from
 * selected operand 1 (immediate by flag 0x80 of byte 3). */
void field_event_set_camera_heading(void) {
    s16 heading;

    heading = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(3));
    field_view.heading_angles.vy = heading;
    field_view.heading = heading;
    field_current_event_actor->pc += 4;
}

/* 8008FB28: Copy the working elevation/heading/zoom into the scripted camera. */
void field_event_copy_camera_to_scripted(void) {
    field_view.scripted_scale = 0x1000;
    field_view.scripted_heading = field_view.heading_angles.vy;
    field_view.scripted_elevation = field_view.elevation;
    field_view.scripted_zoom = (field_view.projection * field_view.distance) >> 12;
    field_current_event_actor->pc += 1;
}

/* 8008FB98: Switch to the scripted camera now, from the working parameters. */
void field_event_scripted_camera_on(void) {
    field_view.mode = 1;
    field_event_batch_limit += 4;
    field_current_event_actor->pc += 1;
    field_view.scripted_scale = 0x1000;
    field_view.target_a = 12;
    field_view.target_b = 12;
    field_view.flags |= 0x8000;
    field_view.scripted_heading = field_view.heading_angles.vy;
    field_view.scripted_elevation = field_view.elevation;
    field_view.scripted_zoom = (field_view.projection * field_view.distance) >> 12;
}

/* 8008FC4C: Leave the scripted camera by its mode: 0 clears the hold flag; 1 with
 * operand 1 zero returns to mode 0 at once (hold flag cleared, camera
 * counter 2) and also skips the next 3 bytes (pc + 6), otherwise blends
 * back over operand-1 frames (mode 2). In mode 2 it neither advances nor
 * yields until the blend ends. */
void field_event_scripted_camera_off(void) {
    s32 frames;

    switch (field_view.mode) {
    case 0:
        field_view.flags &= 0x7FFF;
        field_current_event_actor->pc += 3;
        break;
    case 1:
        frames = field_event_read_imm_or_var(1);
        if (frames == 0) {
            field_view.mode = 0;
            field_view.flags &= 0x7FFF;
            field_current_event_actor->pc += 3;
            field_work.camera_counter = 2;
        } else {
            field_view.mode = 2;
            field_view.target_a = frames;
            field_view.target_b = frames;
        }
        field_current_event_actor->pc += 3;
        break;
    case 2:
        break;
    }
}

/* 8008FD40: Set the scripted camera's two blend frame counts (800af880 target_a and
 * target_b) to operands 1 and 3, at least 1; raises the batch limit. */
void field_event_set_camera_blend_frames(void) {
    s32 frames;

    field_view.target_a = field_event_read_imm_or_var(1);
    frames = field_event_read_imm_or_var(3);
    field_view.target_b = frames;
    if (field_view.target_a == 0) {
        field_view.target_a = 1;
    }
    if (frames == 0) {
        field_view.target_b = 1;
    }
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 5;
}

/* 8008FDD0: Save the target goal. */
void field_event_save_target_goal(void) {
    field_view.saved_target.vx = field_view.target_goal.vx;
    field_view.saved_target.vy = field_view.target_goal.vy;
    field_view.saved_target.vz = field_view.target_goal.vz;
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 1;
}

/* 8008FE2C: Set the saved camera target to selected operands 1/3/5 as x/z/y (flags byte
 * 7, bits 0x80/0x40/0x20; whole units). */
void field_event_set_saved_target(void) {
    field_view.saved_target.vx = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.saved_target.vz = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.saved_target.vy = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7)) << 16;
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 8;
}

/* 8008FF04: Set camera point A to the position of the actor of selector byte 1 (the party
 * leader when none, 8009cd7c). */
void field_event_point_a_at_actor(void) {
    FieldActor *actor;

    actor = field_view.components.descriptors[field_event_read_actor_index_or_leader(1)].actor;
    field_view.point_actor_a.vx = actor->position[0];
    field_view.point_actor_a.vy = actor->position[1];
    field_view.point_actor_a.vz = actor->position[2];
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 2;
}

/* 8008FF90: Set camera point A to selected operands 1/3/5 as x/z/y (flags byte 7, bits
 * 0x80/0x40/0x20; whole units). */
void field_event_set_point_a(void) {
    field_view.point_actor_a.vx = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.point_actor_a.vz = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.point_actor_a.vy = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7)) << 16;
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 8;
}

/* 80090068: Save the eye goal. */
void field_event_save_eye_goal(void) {
    field_view.saved_eye.vx = field_view.eye_goal.vx;
    field_view.saved_eye.vy = field_view.eye_goal.vy;
    field_view.saved_eye.vz = field_view.eye_goal.vz;
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 1;
}

/* 800900C4: Set the saved camera eye to selected operands 1/3/5 as x/z/y (flags byte 7,
 * bits 0x80/0x40/0x20; whole units). */
void field_event_set_saved_eye(void) {
    field_view.saved_eye.vx = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.saved_eye.vz = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.saved_eye.vy = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7)) << 16;
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 8;
}

/* 8009019C: Set camera point B to the position of the actor of selector byte 1 (the party
 * leader when none, 8009cd7c). */
void field_event_point_b_at_actor(void) {
    FieldActor *actor;

    actor = field_view.components.descriptors[field_event_read_actor_index_or_leader(1)].actor;
    field_view.point_actor_b.vx = actor->position[0];
    field_view.point_actor_b.vy = actor->position[1];
    field_view.point_actor_b.vz = actor->position[2];
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 2;
}

/* 80090228: Set point B to selected operands 1, 3, 5 as x, z, y in whole units
 * (flags byte 7: 0x80, 0x40, 0x20); raises the batch limit. */
void field_event_set_point_b(void) {
    field_view.point_actor_b.vx = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.point_actor_b.vz = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.point_actor_b.vy = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7)) << 16;
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 8;
}

/* 80090300: Reset the saved/point targets to the target goal and eyes to the eye goal. */
void field_event_reset_camera_points(void) {
    field_view.point_actor_a.vx = field_view.saved_target.vx = field_view.target_goal.vx;
    field_view.point_actor_a.vy = field_view.saved_target.vy = field_view.target_goal.vy;
    field_view.point_actor_a.vz = field_view.saved_target.vz = field_view.target_goal.vz;
    field_view.saved_eye.vx = field_view.eye_goal.vx;
    field_view.saved_eye.vy = field_view.eye_goal.vy;
    field_view.saved_eye.vz = field_view.eye_goal.vz;
    field_view.point_actor_b.vx = field_view.eye_goal.vx;
    field_view.point_actor_b.vy = field_view.eye_goal.vy;
    field_view.point_actor_b.vz = field_view.eye_goal.vz;
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 1;
}

/* 800903BC: Event 0xac: start a scripted camera move. Mode 0 (1) moves the target
 * (eye) from its saved point to point A (B) over op2 steps; mode 2 (3) moves
 * it along the same line at op2 units per step. Byte-1 bit 0x80 also snaps
 * the live target (eye) to the start. */
void field_event_move_camera(void) {
    s32 mode;
    VECTOR delta;
    VECTOR direction;
    s32 distance;
    s32 speed;

    mode = EVENT_OPERAND_BYTE(1) & 0xF;
    switch (mode) {
    case 0:
        field_view.target_steps = field_event_read_imm_or_var(2);
        if (field_view.target_steps == 0) {
            field_view.target_steps++;
            field_view.target_a = 1;
        }
        field_view.target_step.vx = (field_view.point_actor_a.vx - field_view.saved_target.vx) / field_view.target_steps;
        field_view.target_step.vy = (field_view.point_actor_a.vy - field_view.saved_target.vy) / field_view.target_steps;
        field_view.target_step.vz = (field_view.point_actor_a.vz - field_view.saved_target.vz) / field_view.target_steps;
        field_view.scripted_target.vx = field_view.saved_target.vx;
        field_view.scripted_target.vy = field_view.saved_target.vy;
        field_view.scripted_target.vz = field_view.saved_target.vz;
        field_view.scripted |= 1;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            field_view.target.vx = field_view.saved_target.vx;
            field_view.target.vy = field_view.saved_target.vy;
            field_view.target.vz = field_view.saved_target.vz;
        }
        break;
    case 2:
        delta.vx = (field_view.saved_target.vx - field_view.point_actor_a.vx) >> 16;
        delta.vy = (field_view.saved_target.vy - field_view.point_actor_a.vy) >> 16;
        delta.vz = (field_view.saved_target.vz - field_view.point_actor_a.vz) >> 16;
        VectorNormal(&delta, &direction);
        distance = field_compute_vector_length((field_view.saved_target.vx - field_view.point_actor_a.vx) >> 16,
                                 (field_view.saved_target.vy - field_view.point_actor_a.vy) >> 16,
                                 (field_view.saved_target.vz - field_view.point_actor_a.vz) >> 16);
        speed = field_event_read_imm_or_var(2);
        field_view.target_step.vx = -(direction.vx * speed) * 16;
        field_view.target_step.vy = -(direction.vy * speed) * 16;
        field_view.target_step.vz = -(direction.vz * speed) * 16;
        field_view.scripted_target.vx = field_view.saved_target.vx;
        field_view.scripted_target.vy = field_view.saved_target.vy;
        field_view.scripted_target.vz = field_view.saved_target.vz;
        field_view.scripted |= 1;
        field_view.target_steps = distance / speed;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            field_view.target.vx = field_view.saved_target.vx;
            field_view.target.vy = field_view.saved_target.vy;
            field_view.target.vz = field_view.saved_target.vz;
        }
        break;
    case 3:
        delta.vx = (field_view.saved_eye.vx - field_view.point_actor_b.vx) >> 16;
        delta.vy = (field_view.saved_eye.vy - field_view.point_actor_b.vy) >> 16;
        delta.vz = (field_view.saved_eye.vz - field_view.point_actor_b.vz) >> 16;
        VectorNormal(&delta, &direction);
        distance = field_compute_vector_length((field_view.saved_eye.vx - field_view.point_actor_b.vx) >> 16,
                                 (field_view.saved_eye.vy - field_view.point_actor_b.vy) >> 16,
                                 (field_view.saved_eye.vz - field_view.point_actor_b.vz) >> 16);
        speed = field_event_read_imm_or_var(2);
        field_view.eye_step[0] = -(direction.vx * speed) * 16;
        field_view.eye_step[1] = -(direction.vy * speed) * 16;
        field_view.eye_step[2] = -(direction.vz * speed) * 16;
        field_view.scripted_eye[0] = field_view.saved_eye.vx;
        field_view.scripted_eye[1] = field_view.saved_eye.vy;
        field_view.scripted_eye[2] = field_view.saved_eye.vz;
        field_view.scripted |= 2;
        field_view.eye_steps = distance / speed;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            field_view.eye.vx = field_view.saved_eye.vx;
            field_view.eye.vy = field_view.saved_eye.vy;
            field_view.eye.vz = field_view.saved_eye.vz;
        }
        break;
    case 1:
        field_view.eye_steps = field_event_read_imm_or_var(2);
        if (field_view.eye_steps == 0) {
            field_view.eye_steps++;
            field_view.target_b = 1;
        }
        field_view.eye_step[0] = (field_view.point_actor_b.vx - field_view.saved_eye.vx) / field_view.eye_steps;
        field_view.eye_step[1] = (field_view.point_actor_b.vy - field_view.saved_eye.vy) / field_view.eye_steps;
        field_view.eye_step[2] = (field_view.point_actor_b.vz - field_view.saved_eye.vz) / field_view.eye_steps;
        field_view.scripted_eye[0] = field_view.saved_eye.vx;
        field_view.scripted_eye[1] = field_view.saved_eye.vy;
        field_view.scripted_eye[2] = field_view.saved_eye.vz;
        field_view.scripted |= 2;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            field_view.eye.vx = field_view.saved_eye.vx;
            field_view.eye.vy = field_view.saved_eye.vy;
            field_view.eye.vz = field_view.saved_eye.vz;
        }
        break;
    }
    field_current_event_actor->pc += 4;
}

/* 80090A10: Store the camera target's whole x, z, y in three variables. */
void field_event_store_camera_target(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, WHOLE(field_view.target.vx));
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, WHOLE(field_view.target.vz));
    field_event_write_variable(field_event_read_u16(5) & 0xFFFF, WHOLE(field_view.target.vy));
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 7;
}

/* 80090A94: Store the camera eye's whole x, z, y in three variables. */
void field_event_store_camera_eye(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, WHOLE(field_view.eye.vx));
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, WHOLE(field_view.eye.vz));
    field_event_write_variable(field_event_read_u16(5) & 0xFFFF, WHOLE(field_view.eye.vy));
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 7;
}

/* 80090B18: Store the camera target goal's whole x, z, y in variables operands 1, 3
 * and 5; raises the batch limit. */
void field_event_store_target_goal(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, WHOLE(field_view.target_goal.vx));
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, WHOLE(field_view.target_goal.vz));
    field_event_write_variable(field_event_read_u16(5) & 0xFFFF, WHOLE(field_view.target_goal.vy));
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 7;
}

/* 80090B9C: Store the camera eye goal's whole x, z, y in variables operands 1, 3 and
 * 5; raises the batch limit. */
void field_event_store_eye_goal(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, WHOLE(field_view.eye_goal.vx));
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, WHOLE(field_view.eye_goal.vz));
    field_event_write_variable(field_event_read_u16(5) & 0xFFFF, WHOLE(field_view.eye_goal.vy));
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 7;
}

/* 80090C20: With byte 3 zero store the scripted camera heading in variable operand 1,
 * otherwise set it to raw operand 1; raises the batch limit. */
void field_event_scripted_heading(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_view.scripted_heading);
    } else {
        field_view.scripted_heading = field_event_read_u16(1);
    }
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 4;
}

/* 80090CB8: With byte 3 zero store the scripted camera elevation in variable operand
 * 1, otherwise set it to raw operand 1; raises the batch limit. */
void field_event_scripted_elevation(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_view.scripted_elevation);
    } else {
        field_view.scripted_elevation = field_event_read_u16(1);
    }
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 4;
}

/* 80090D50: With byte 3 zero store the scripted camera zoom in variable operand 1,
 * otherwise set it to raw operand 1; raises the batch limit. */
void field_event_scripted_zoom(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_view.scripted_zoom);
    } else {
        field_view.scripted_zoom = (u16)field_event_read_u16(1);
    }
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 4;
}

/* 80090DEC: Store the scripted camera heading, elevation and zoom in variables
 * operands 1, 3 and 5. */
void field_event_store_scripted_camera(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_view.scripted_heading);
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, field_view.scripted_elevation);
    field_event_write_variable(field_event_read_u16(5) & 0xFFFF, field_view.scripted_zoom);
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 7;
}

/* 80090E70: Derive the scripted heading, pitch and zoom (half the distance) from
 * point A looking at point B, reset the scripted scale and store them in
 * variables operands 1, 3 and 5. */
void field_event_look_from_points(void) {
    VECTOR a;
    VECTOR b;
    s32 heading;
    s32 pitch;
    s32 zoom;

    a.vx = field_view.point_actor_a.vx;
    a.vy = field_view.point_actor_a.vy;
    a.vz = field_view.point_actor_a.vz;
    b.vx = field_view.point_actor_b.vx;
    b.vy = field_view.point_actor_b.vy;
    b.vz = field_view.point_actor_b.vz;
    zoom = field_compute_vector_length((b.vx - a.vx) >> 16, (b.vy - a.vy) >> 16,
                              (b.vz - a.vz) >> 16) / 2;
    field_view.scripted_scale = 0x1000;
    heading = ((-ratan2(a.vz - b.vz, a.vx - b.vx) & 0xFFFF) - 0x400) & 0xFFF;
    pitch = ((-ratan2(field_compute_planar_length((b.vx - a.vx) >> 16, (b.vz - a.vz) >> 16),
                             (a.vy - b.vy) >> 16) * 360) >> 12) + 91;
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, heading);
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, pitch);
    field_event_write_variable(field_event_read_u16(5) & 0xFFFF, zoom);
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 7;
}

/* 80091008: Rotate `point` about `center` in the XZ plane by `angle` (result mirrored
 * through the center, as the original subtracts center - point). */
void field_camera_rotate_point_about_center(VECTOR *point, VECTOR *center, s32 angle) {
    MATRIX m;
    VECTOR offset;
    VECTOR rotated;
    SVECTOR angles;

    angles.vx = 0;
    angles.vy = angle;
    angles.vz = 0;
    PushMatrix();
    gpu_build_rotation_matrix(&angles, &m);
    offset.vx = center->vx - point->vx;
    offset.vy = center->vy - point->vy;
    offset.vz = center->vz - point->vz;
    ApplyMatrixLV(&m, &offset, &rotated);
    point->vx = rotated.vx + center->vx;
    point->vz = rotated.vz + center->vz;
    PopMatrix();
}

void field_camera_rotate_point_about_center(VECTOR *point, VECTOR *center, s32 angle);

/* 800910C0: Place a point around the centre (selected operands 1, 3, 5: x, z, y) at
 * heading 7, elevation 9 and distance 11 (selected, flags byte 13) and store
 * its whole x, z, y in variables operands 14, 16 and 18. */
void field_event_point_at_angle(void) {
    VECTOR center;
    VECTOR point;
    s32 heading;
    s32 elevation;
    s32 angle;
    s32 distance;

    center.vx = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(13)) << 16;
    center.vz = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(13)) << 16;
    center.vy = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(13)) << 16;
    heading = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(13));
    elevation = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(13));
    distance = field_event_read_selected_operand_04(11, EVENT_OPERAND_BYTE(13));
    angle = ((elevation * 0xB60) >> 8) + 0xC00;
    point.vy = ((-((gpu_get_cos(angle) * distance) << 5)) >> 16) * field_view.scripted_scale * 16 + center.vy;
    point.vz = (((gpu_get_sin(angle) * distance) << 5) >> 16) * field_view.scripted_scale * 16 + center.vz;
    point.vx = center.vx;
    field_camera_rotate_point_about_center(&point, &center, heading);
    field_event_write_variable(field_event_read_u16(14) & 0xFFFF, WHOLE(point.vx));
    field_event_write_variable(field_event_read_u16(16) & 0xFFFF, WHOLE(point.vz));
    field_event_write_variable(field_event_read_u16(18) & 0xFFFF, WHOLE(point.vy));
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 20;
}

/* 80091318: As eb around camera point byte 1 (0 saved target, 1 point A, 2 saved eye,
 * 3 point B): heading, elevation and distance from selected operands 2, 4,
 * 6 (flags byte 8); x, z, y go to variables operands 9, 11 and 13. */
void field_event_point_around(void) {
    VECTOR center;
    VECTOR point;
    s32 heading;
    s32 elevation;
    s32 angle;
    s32 distance;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        center.vx = field_view.saved_target.vx;
        center.vy = field_view.saved_target.vy;
        center.vz = field_view.saved_target.vz;
        break;
    case 1:
        center.vx = field_view.point_actor_a.vx;
        center.vy = field_view.point_actor_a.vy;
        center.vz = field_view.point_actor_a.vz;
        break;
    case 2:
        center.vx = field_view.saved_eye.vx;
        center.vy = field_view.saved_eye.vy;
        center.vz = field_view.saved_eye.vz;
        break;
    case 3:
        center.vx = field_view.point_actor_b.vx;
        center.vy = field_view.point_actor_b.vy;
        center.vz = field_view.point_actor_b.vz;
        break;
    }
    heading = field_event_read_selected_operand_80(2, EVENT_OPERAND_BYTE(8));
    elevation = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(8));
    distance = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(8));
    angle = ((elevation * 0xB60) >> 8) + 0xC00;
    point.vy = ((-((gpu_get_cos(angle) * distance) << 5)) >> 16) * field_view.scripted_scale * 16 + center.vy;
    point.vz = (((gpu_get_sin(angle) * distance) << 5) >> 16) * field_view.scripted_scale * 16 + center.vz;
    point.vx = center.vx;
    field_camera_rotate_point_about_center(&point, &center, heading);
    field_event_write_variable(field_event_read_u16(9) & 0xFFFF, WHOLE(point.vx));
    field_event_write_variable(field_event_read_u16(11) & 0xFFFF, WHOLE(point.vz));
    field_event_write_variable(field_event_read_u16(13) & 0xFFFF, WHOLE(point.vy));
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 15;
}

/* 800915C4: Store camera point byte 1 (0 saved target, 1 point A, 2 saved eye, 3
 * point B) as whole x, z, y in variables operands 2, 4 and 6. */
void field_event_store_camera_point(void) {
    VECTOR point;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        point.vx = field_view.saved_target.vx;
        point.vy = field_view.saved_target.vy;
        point.vz = field_view.saved_target.vz;
        break;
    case 1:
        point.vx = field_view.point_actor_a.vx;
        point.vy = field_view.point_actor_a.vy;
        point.vz = field_view.point_actor_a.vz;
        break;
    case 2:
        point.vx = field_view.saved_eye.vx;
        point.vy = field_view.saved_eye.vy;
        point.vz = field_view.saved_eye.vz;
        break;
    case 3:
        point.vx = field_view.point_actor_b.vx;
        point.vy = field_view.point_actor_b.vy;
        point.vz = field_view.point_actor_b.vz;
        break;
    }
    field_event_write_variable(field_event_read_u16(2) & 0xFFFF, WHOLE(point.vx));
    field_event_write_variable(field_event_read_u16(4) & 0xFFFF, WHOLE(point.vz));
    field_event_write_variable(field_event_read_u16(6) & 0xFFFF, WHOLE(point.vy));
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 8;
}

/* 80091720: Copy camera point operand 1 into camera point operand 2. */
void field_event_copy_camera_point(void) {
    VECTOR point;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        point.vx = field_view.saved_target.vx;
        point.vy = field_view.saved_target.vy;
        point.vz = field_view.saved_target.vz;
        break;
    case 1:
        point.vx = field_view.point_actor_a.vx;
        point.vy = field_view.point_actor_a.vy;
        point.vz = field_view.point_actor_a.vz;
        break;
    case 2:
        point.vx = field_view.saved_eye.vx;
        point.vy = field_view.saved_eye.vy;
        point.vz = field_view.saved_eye.vz;
        break;
    case 3:
        point.vx = field_view.point_actor_b.vx;
        point.vy = field_view.point_actor_b.vy;
        point.vz = field_view.point_actor_b.vz;
        break;
    }
    switch (EVENT_OPERAND_BYTE(2)) {
    case 0:
        field_view.saved_target.vx = point.vx;
        field_view.saved_target.vy = point.vy;
        field_view.saved_target.vz = point.vz;
        break;
    case 1:
        field_view.point_actor_a.vx = point.vx;
        field_view.point_actor_a.vy = point.vy;
        field_view.point_actor_a.vz = point.vz;
        break;
    case 2:
        field_view.saved_eye.vx = point.vx;
        field_view.saved_eye.vy = point.vy;
        field_view.saved_eye.vz = point.vz;
        break;
    case 3:
        field_view.point_actor_b.vx = point.vx;
        field_view.point_actor_b.vy = point.vy;
        field_view.point_actor_b.vz = point.vz;
        break;
    }
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 3;
}

/* 80091944: Set the fog colour (operands 1, 3, 5), far colour (7, 9, 11) and fog
 * range (13, 15), set 800b218e (sprite gate) and reselect every shown
 * model's drawing mode (80073e38). */
void field_event_set_fog(void) {
    field_work.fog_color[0] = field_event_read_imm_or_var(1);
    field_work.fog_color[1] = field_event_read_imm_or_var(3);
    field_work.fog_color[2] = field_event_read_imm_or_var(5);
    field_work.far_color[0] = field_event_read_imm_or_var(7);
    field_work.far_color[1] = field_event_read_imm_or_var(9);
    field_work.far_color[2] = field_event_read_imm_or_var(11);
    field_work.fog_range[0] = field_event_read_imm_or_var(13);
    field_work.fog_range[1] = field_event_read_imm_or_var(15);
    field_work.sprite_gate = 1;
    field_instance_refresh_bounds_modes();
    field_current_event_actor->pc += 17;
}

/* 80091A08: Set the four camera bounds to signed operands 1, 3, 5 and minus operand
 * 7. */
void field_event_set_camera_bounds(void) {
    field_view.bounds[0] = field_event_read_s16(1);
    field_view.bounds[1] = field_event_read_s16(3);
    field_view.bounds[2] = field_event_read_s16(5);
    field_view.bounds[3] = -field_event_read_s16(7);
    field_current_event_actor->pc += 9;
}

/* 80091A78: Set the clear colour (800b219c) from operands 1, 3 and 5. */
void field_event_set_clear_color(void) {
    field_work.clear_color[0] = field_event_read_imm_or_var(1);
    field_work.clear_color[1] = field_event_read_imm_or_var(3);
    field_work.clear_color[2] = field_event_read_imm_or_var(5);
    field_current_event_actor->pc += 7;
}

/* 80091AD4: Event e4: empty. It neither advances nor yields, so the interpreter runs
 * it again until the pass's batch limit (or the 0x400 loop error); the
 * script stays on it. */
void field_event_halt_e4(void) {
}

/* The unit's own uninitialized variables, which only 80091adc reads: the
 * first of the field BSS (800af5e8-800af76c), the resident clearing it from
 * 800af5e4 with a pre-increment loop. */
static RECT field_vram_ring_rects[32]; /* 800AF5E8 */
static s16 field_vram_ring_destination_x_table[32]; /* 800AF6E8 */
static s16 field_vram_ring_unread_destination_y_table[32]; /* 800AF728 */
static u16 field_vram_ring_count; /* 800AF768 */

/* 80091ADC: Record a VRAM rectangle in the 32-slot ring at 800af5e8 and move it to
 * (dx, dy), or clear it to black when `clear` is set. */
void field_vram_move_or_clear_rect(s32 x, s32 y, s32 w, s32 h, s32 dx, s32 dy, s32 clear) {
    s32 slot;

    slot = field_vram_ring_count & 0x1F;
    field_vram_ring_rects[slot].y = y;
    field_vram_ring_rects[slot].x = x;
    field_vram_ring_rects[slot].w = w;
    field_vram_ring_rects[slot].h = h;
    field_vram_ring_destination_x_table[slot] = dx;
    field_vram_ring_unread_destination_y_table[slot] = dy;
    if (clear == 0) {
        MoveImage(&field_vram_ring_rects[slot], field_vram_ring_destination_x_table[slot], (s16)dy);
    } else {
        ClearImage(&field_vram_ring_rects[slot], 0, 0, 0);
    }
    field_vram_ring_count++;
}

/* 80091BBC: Event 0xe1: with op1 and op3 both zero, clear the VRAM rectangle
 * (op5, op7, op9, op11) to black; otherwise move the rectangle at (op1, op3)
 * of size op5 x op7 to (op9, op11). Selected operands, flags byte 13. */
void field_event_vram_rectangle(void) {
    s32 x;
    s32 y;
    RECT unused; /* the original frame holds an unused 8-byte local */

    x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xD));
    y = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0xD));
    if (x == 0 && y == 0) {
        field_vram_move_or_clear_rect(field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xD)), field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xD)),
                      field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xD)), field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0xD)), 0, 0, 1);
    } else {
        field_vram_move_or_clear_rect(x, y, field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xD)), field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xD)),
                      field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xD)), field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0xD)), 0);
    }
    field_current_event_actor->pc += 0xE;
}

/* 80091E00: Set the current actor's sprite draw mode (+134 bits 5-6) to selected
 * operand 1 & 3 and +ee to selected operand 3 (flags byte 5: 0x80, 0x40);
 * bit 5 draws the sprite with colour 0 through 8001e2f8 and bit 6 with
 * colour 1 through 8001e368, both at height +ee. */
void field_event_set_sprite_draw_mode(void) {
    field_current_event_actor->unk134 = (field_current_event_actor->unk134 & ~0x60) | ((field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(5)) & 3) << 5);
    field_current_event_actor->unkEE = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_current_event_actor->pc += 6;
}

/* 80091E98: As dd for the actor selected by byte 1: sprite draw mode (+134 bits 5-6)
 * = selected operand 2 & 3, +ee = selected operand 4 (flags byte 6). */
void field_event_set_actor_sprite_draw_mode(void) {
    FieldActor *actor;

    if (field_event_read_actor_index(1) != 0xFF) {
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        actor->unk134 = (actor->unk134 & ~0x60) | ((field_event_read_selected_operand_80(2, EVENT_OPERAND_BYTE(6)) & 3) << 5);
        actor->unkEE = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(6));
    }
    field_current_event_actor->pc += 7;
}

/* 80091F84: When the current descriptor is animated (flag 0x2000), set entry operand
 * 1 of the actor's channel word list (+118, which its model's animation
 * channels read through 80080a18) to operand 3, at most 0xfff. */
void field_event_set_channel_word(void) {
    s32 index;
    s32 value;

    index = field_event_read_imm_or_var(1);
    value = field_event_read_imm_or_var(3);
    if (value >= 0x1000) {
        value = 0xFFF;
    }
    if (field_view.components.descriptors[field_current_event_actor_index].flags & 0x2000) {
        field_current_event_actor->list[index] = value;
    }
    field_current_event_actor->pc += 5;
}

/* 80092044: Swap variables operand 1 and operand 3. */
void field_event_swap_variables(void) {
    s32 first;
    s32 second;

    first = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    second = field_event_read_variable(field_event_read_u16(3) & 0xFFFF);
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, first);
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, second);
    field_current_event_actor->pc += 5;
}

/* 800920D8: Pass every handle in the list at 800afea8 to resident 80027EAC. */
void field_run_texture_scrolls(void) {
    s32 i;

    for (i = 0; i < field_texture_scrolls.count; i++) {
        gpu_update_texture_scroll(field_texture_scrolls.scrolls[i]);
    }
}

/* 80092148: Event fe 40: store the low byte of raw operand 5 at index operand 3 of the
 * buffer of window-list entry operand 1 (800afea8, created by opcode da), when
 * the index is below the entry's length. */
void field_event_set_scroll_byte(void) {
    s32 table;
    s32 index;
    s32 value;

    table = field_event_read_imm_or_var(1);
    index = field_event_read_imm_or_var(3);
    value = field_event_read_u16(5) & 0xFFFF;
    if (index < field_texture_scrolls.lengths[table]) {
        field_texture_scrolls.buffers[table][index] = value;
    }
    field_current_event_actor->pc += 7;
}

/* 800921E8: Add a texture scroll (80027d64) to the list at 800afea8 (at most 32, not
 * while 800adb8c is set): area operands 1, 3, 5, 7 (x, y, w, h), operand 9
 * bands, source x operand 11 and y operand 13, every band's speed byte set
 * to operand 15. */
void field_event_add_texture_scroll(void) {
    s32 length;
    s32 fill;
    s32 i;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 a;

    if (field_party_rebuilding == 0 && field_texture_scrolls.count < 32) {
        length = field_event_read_u16(9) & 0xFFFF;
        field_texture_scrolls.lengths[field_texture_scrolls.count] = length;
        field_texture_scrolls.buffers[field_texture_scrolls.count] = heap_alloc(length + 1, 0);
        field_texture_scrolls.scrolls[field_texture_scrolls.count] = heap_alloc(0x18, 0);
        fill = field_event_read_u16(15) & 0xFFFF;
        for (i = 0; i < length; i++) {
            field_texture_scrolls.buffers[field_texture_scrolls.count][i] = fill;
        }
        x = (s16)field_event_read_u16(1);
        y = (s16)field_event_read_u16(3);
        width = (s16)field_event_read_u16(5);
        height = (s16)field_event_read_u16(7);
        a = (s16)field_event_read_u16(11);
        gpu_init_texture_scroll(field_texture_scrolls.scrolls[field_texture_scrolls.count], x, y, width, height, length, a,
                      field_event_read_u16(13), field_texture_scrolls.buffers[field_texture_scrolls.count]);
        field_texture_scrolls.count++;
    }
    field_current_event_actor->pc += 17;
}

/* 800923E4: No operation. */
void field_event_nop_0f(void) {
    field_current_event_actor->pc += 1;
}

/* 80092404: No operation. */
void field_event_nop_0e(void) {
    field_current_event_actor->pc += 1;
}

/* 80092424: Return byte `which` of collision attribute `index`. */
s32 field_collision_get_attribute_byte(s32 index, s32 which) {
    switch (which) {
    case 0:
        return field_view.components.collision_attributes[index].bytes[0];
    case 1:
        return field_view.components.collision_attributes[index].bytes[1];
    case 2:
        return field_view.components.collision_attributes[index].bytes[2];
    case 3:
        return field_view.components.collision_attributes[index].bytes[3];
    }
    return 0;
}

/* 800924D4: Replace byte `which` of collision attribute `index`. */
void field_collision_set_attribute_byte(s32 index, s32 which, s32 value) {
    switch (which) {
    case 0:
        field_view.components.collision_attributes[index].word = (field_view.components.collision_attributes[index].word & ~0xFF) | value;
        break;
    case 1:
        value <<= 8;
        field_view.components.collision_attributes[index].word = (field_view.components.collision_attributes[index].word & 0xFFFF00FF) | value;
        break;
    case 2:
        value <<= 16;
        field_view.components.collision_attributes[index].word = (field_view.components.collision_attributes[index].word & 0xFF00FFFF) | value;
        break;
    case 3:
        value <<= 24;
        field_view.components.collision_attributes[index].word = (field_view.components.collision_attributes[index].word & 0x00FFFFFF) | value;
        break;
    }
}

/* 800925A0: Set 800b217c to operand 1 and the text speed to 8, 6 or 4 for values 0,
 * 1 and 2 (others keep it). */
void field_event_set_text_speed(void) {
    s32 mode;

    mode = field_event_read_imm_or_var(1);
    field_work.unk217C = mode;
    switch (mode) {
    case 0:
        field_work.text_speed = 8;
        break;
    case 1:
        field_work.text_speed = 6;
        break;
    case 2:
        field_work.text_speed = 4;
        break;
    }
    field_current_event_actor->pc += 3;
}

/* 80092628: Set the field input mask (800b217a; ANDed into the held, pressed and
 * repeated buttons each frame) to raw operand 1. */
void field_event_set_input_mask(void) {
    field_work.input_mask = field_event_read_u16(1);
    field_current_event_actor->pc += 3;
}

/* 80092664: Replace byte (byte 2, 0-3) of collision attribute record (byte 1) with
 * operand 3 (800924d4). */
void field_event_set_collision_attribute(void) {
    field_collision_set_attribute_byte(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), field_event_read_imm_or_var(3));
    field_current_event_actor->pc += 5;
}

/* 800926C8: OR operand 3 into byte (byte 2) of collision attribute record (byte 1). */
void field_event_or_collision_attribute(void) {
    s32 value;

    value = field_collision_get_attribute_byte(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value |= field_event_read_imm_or_var(3);
    field_collision_set_attribute_byte(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    field_current_event_actor->pc += 5;
}

/* 80092768: AND operand 3 into byte (byte 2) of collision attribute record (byte 1). */
void field_event_and_collision_attribute(void) {
    s32 value;

    value = field_collision_get_attribute_byte(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value &= field_event_read_imm_or_var(3);
    field_collision_set_attribute_byte(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    field_current_event_actor->pc += 5;
}

/* 80092808: Offset the current actor's interaction point (+60/+64, added to its position
 * by the talk and touch triggers and the 80092894 walk goal) 36 units along the
 * published descriptor's facing (rotation y) and set its layer flag 0x800. */
void field_event_set_interaction_offset(void) {
    FieldActor *actor;
    s32 step;

    field_current_event_actor->unk60 = (gpu_get_cos(field_current_event_descriptor->rotation.vy) * 36) >> 12;
    step = -(gpu_get_sin(field_current_event_descriptor->rotation.vy) * 36) >> 12;
    actor = field_current_event_actor;
    actor->unk64 = step;
    actor->layer_flags |= 0x800;
    actor->pc++;
}

/* 80092894: Walk the controlled actor one step toward (x, z); with `mode` 0 the goal
 * is 40 units along direction `angle` from the running actor (publishing
 * the field id first when 800adbec asks). Returns -1 while walking (mode 1
 * retries the instruction) and 0 once it has arrived or is stuck, when it
 * stops, turns and the instruction continues. */
s32 field_event_walk_player(s32 angle, s32 mode, s32 x, s32 z) {
    FieldActor *player;
    Sprite *model;
    s32 from_x;
    s32 from_z;
    s32 reach;
    s32 value;
    s32 field;
    s32 direction;
    s32 dx;
    s32 dz;
    s32 goal_x;
    s32 goal_z;
    VECTOR delta;

    field_work.encounter_inhibition = -1;
    player = field_view.components.descriptors[field_work.controlled].actor;
    model = field_view.components.descriptors[field_work.controlled].model;
    player->layer_flags |= 0x38;
    model->speed = 0x80000;
    reach = field_compute_abs_via_gte(8) * 2;
    from_x = WHOLE(player->position[0]);
    from_z = WHOLE(player->position[2]);
    if (mode == 0) {
        if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0) {
            field_event_yield_requested = 1;
        }
        if (field_map_change_not_requested != 0) {
            value = field_event_read_imm_or_var(4);
            field = field_event_read_imm_or_var(2);
            field_event_record_departure();
            field_map_change_not_requested = 0;
            field_event_write_variable(2, value);
            mode_field_map_id = field;
        }
        direction = field_current_event_descriptor->rotation.vy + angle - 0x400;
        goal_x = field_current_event_actor->unk60 + WHOLE(field_current_event_actor->position[0]) + ((gpu_get_cos(direction) * 40) >> 12);
        goal_z = field_current_event_actor->unk64 + WHOLE(field_current_event_actor->position[2]) + (-(gpu_get_sin(direction) * 40) >> 12);
    } else {
        goal_x = x;
        goal_z = z;
    }
    dx = goal_x - from_x;
    dz = goal_z - from_z;
    delta.vx = dx;
    delta.vy = 0;
    delta.vz = dz;
    if (reach < field_compute_planar_length(dx, dz)) {
        goto walk;
    }
arrive:
    player->heading_goal = player->heading = player->heading_goal | 0x8000;
    model->speed = 0;
    player->unkE8 = 0;
    field_actor_start_animation(model, 0, &field_view.components.descriptors[field_work.controlled]);
    field_event_yield_requested = 1;
    player->slots[player->slot].value = 0xFFFF;
    player->slots[player->slot].move_mode = 0;
    player->flags &= ~0x200000;
    if (mode == 1) {
        player->layer_flags &= ~0x38;
    }
    player->stuck = 0;
    field_current_event_actor->pc += 6;
    return 0;
walk:
    if (player->last_position[0] == WHOLE(player->position[0]) &&
        player->last_position[1] == WHOLE(player->position[1]) &&
        player->last_position[2] == WHOLE(player->position[2])) {
        player->stuck++;
    } else {
        player->stuck = 0;
    }
    if ((s16)player->stuck > 0x40) {
        goto arrive;
    }
    player->heading_goal = player->heading = field_compute_xz_heading(&delta);
    field_event_yield_requested = 1;
    if (mode != 0) {
        field_current_event_actor->pc -= 1;
    }
    return -1;
}

/* 80092C20: Event fe 68: once field control allows it (800adbdc and 800adbe4 set,
 * 800adb2c and 800adb90 clear, 8004f308 not -1; else wait: pc-- to the fe),
 * walk the controlled actor one step toward x, z = selected operands 1 and 3
 * (flags 0x80/0x40 of byte 5; 800a0c4c sets its flag 0x80, 80092894 mode 1
 * waits with pc-- while walking), saving its flags in 800b2350 first; on
 * arrival advance 6 and clear its flag 0x80 again unless the saved flags had
 * it. */
void field_event_walk_player_to(void) {
    s32 x;
    s32 z;

    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 || field_actor_block_loading != 0) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc--;
        return;
    }
    x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(5));
    z = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    if (field_work.unk2350 == 0) {
        field_work.unk2350 = field_view.components.descriptors[field_work.controlled].actor->flags;
    }
    field_actor_set_controlled_flag_80();
    if (field_event_walk_player(0, 1, x, z) == 0) {
        if (!(field_work.unk2350 & 0x80)) {
            field_view.components.descriptors[field_work.controlled].actor->flags &= ~0x80;
        }
        field_work.unk2350 = 0;
    }
}

s32 field_event_walk_player(s32 a, s32 b, s32 c, s32 d);

/* 80092DFC: Once field control allows it (yielding until then), set flag 0x80 on the
 * controlled actor and walk it a step per frame toward the point 40 units
 * from the running actor (plus its +60/+64 step) along its descriptor's
 * facing (80092894 mode 0, angle 0); on arrival or after 0x40 stuck steps
 * continue (pc + 6). With 800adbec set it first records the departure as
 * 98 does (field operand 2, entry operand 4). */
void field_event_walk_player_ahead_ea(void) {
    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 || field_actor_block_loading != 0) {
        field_event_yield_requested = 1;
    } else {
        field_actor_set_controlled_flag_80();
        field_event_walk_player(0, 0, 0, 0);
    }
}

/* 80092EA0: Once field control allows it (yield otherwise), set the controlled actor's
 * flag 0x80 (800a0c4c) and walk it (80092894 mode 0) toward the point 40 units
 * from this actor (plus its +60/+64) along this descriptor's facing - 0x20,
 * first requesting the change to map operand 2, entry operand 4 while none is
 * pending (800adbec); yields while walking and continues at +6 on arrival or
 * once stuck for more than 64 frames. Byte 1 is not read. */
void field_event_walk_player_ahead(void) {
    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 || field_actor_block_loading != 0) {
        field_event_yield_requested = 1;
    } else {
        field_actor_set_controlled_flag_80();
        field_event_walk_player(0x3E0, 0, 0, 0);
    }
}

/* 80092F44: Publish the current field id and two values in variables 4, 6 and 8 and
 * count variable 0x12 up. */
void field_event_record_departure(void) {
    field_event_write_variable(4, mode_field_map_id & 0x3FFF);
    field_event_write_variable(6, field_actor_get_controlled_facing_octant() & 0xFFFF);
    field_event_write_variable(8, field_camera_get_octant() & 0xFFFF);
    field_event_write_variable(0x12, (s16)(field_event_read_variable(0x12) + 1));
}

/* 80092FB4: Event: when 800adbd8 is set, inhibit encounters, clear it and set 800b0064 to
 * operand 1: the field loop then ends with kind 3, which starts game mode
 * operand 1 & 0x7f (bit 7 first runs 8001bb50; 8007954c). Advances 3 either
 * way. */
void field_event_end_field_mode(void) {
    if (field_mode_exit_not_requested != 0) {
        field_work.encounter_inhibition = -1;
        field_mode_exit_not_requested = 0;
        field_exit_game_mode = field_event_read_imm_or_var(1);
    }
    field_current_event_actor->pc += 3;
}

/* 80093014: Leave the field once field control allows it (yield otherwise): a last
 * play-record update (800a31e8), encounters inhibited, and the game state's
 * destination set from selected operands 1 (map, +231a), 3 (+231e), 5 (heading
 * +231c, plus 0x800; 0xffff keeps the camera's) and 7 (entry, +2320) with flags
 * byte 9; then clear 800adbe4 (the field loop exits with code 1), stop the play
 * record (800b02c8 = 1), yield and continue at +10. */
void field_event_change_map(void) {
    s32 heading;

    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 || field_actor_block_loading != 0) {
        field_event_yield_requested = 1;
        return;
    }
    field_update_play_record();
    field_work.encounter_inhibition = -1;
    field_worldmap_exit_not_requested = 0;
    game_current_data->map = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(9));
    game_current_data->entry[1] = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(9));
    heading = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(9));
    if (((heading & 0xFFFF) == 0xFFFF) | (heading == -1)) {
        game_current_data->entry[0] = (field_view.heading_angles.vy + 0x800) & 0xFFF;
    } else {
        game_current_data->entry[0] = (heading + 0x800) & 0xFFF;
    }
    game_current_data->entry[2] = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(9));
    field_event_empty_map_change_hook();
    field_play_record_stopped = 1;
    field_event_yield_requested = 1;
    field_current_event_actor->pc += 10;
}

/* 800931F8: Empty; the map-change events call it before they leave. */
void field_event_empty_map_change_hook(void) {
}

void field_event_change_map_entry(void);

/* 80093200: Once field control allows it (yield otherwise), request a map change as 98
 * (800932d0: map operand 1, entry operand 3, advancing 5), then set the
 * transition kind 800b0048 from operand 5 and its frames 800afd14 from operand
 * 7 and continue at +9. With a movie requested (800adb70) 800932d0 neither
 * requests nor advances, so these reads come from +0/+2 and the PC advances by
 * 4 only. */
void field_event_change_map_transition(void) {
    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 || field_actor_block_loading != 0) {
        field_event_yield_requested = 1;
    } else {
        field_event_change_map_entry();
        field_event_empty_map_change_hook();
        field_map_change_kind = field_event_read_imm_or_var(0);
        field_map_change_frames = field_event_read_imm_or_var(2);
        field_current_event_actor->pc += 4;
    }
}

/* 800932D0: Request a change to field operand 1 at entry operand 3: once field
 * control allows it and no movie is requested, inhibit encounters and,
 * when 800adbec is set, record the departure (variables 4, 6, 8: field id,
 * the controlled actor's and the camera's octants; variable 0x12 counted
 * up), store the entry in variable 2 and the field in 8004f34c; then yield
 * and advance. Yields without advancing until allowed. */
void field_event_change_map_entry(void) {
    s32 entry;
    s32 field;

    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 || field_actor_block_loading != 0 ||
        field_movie_requested != 0) {
        field_event_yield_requested = 1;
    } else {
        field_work.encounter_inhibition = -1;
        if (field_map_change_not_requested != 0) {
            entry = field_event_read_imm_or_var(3);
            field = field_event_read_imm_or_var(1);
            field_event_record_departure();
            field_map_change_not_requested = 0;
            field_event_write_variable(2, entry);
            mode_field_map_id = field;
            field_event_empty_map_change_hook();
        }
        field_event_yield_requested = 1;
        field_current_event_actor->pc += 5;
    }
}

/* 800933F8: Event: once the field allows it (800adbdc, 800adbe4 and 800adbec set,
 * 800adb2c and 800adb90 clear, music result 8004f308 not -1; else pc-- back to
 * fe and yield), request a battle as opcode 71 does: 8005954c = 800b2356,
 * formation 80059508 = operand 1, 800594f8, 800adbdc and 800adbe0 cleared,
 * 800adb88 set. Unless operand 5 is 0x7fff, also set the field to enter:
 * publish the current one (80092f44), variable 2 = entry operand 7, field id
 * 8004f34c = operand 5, 800adb18 = 1. Yields; bytes 3-4 are not read. */
void field_event_request_battle_field(void) {
    s32 field;
    s32 entry;

    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_map_change_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 ||
        field_actor_block_loading != 0) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc -= 1;
        return;
    }
    mode_battle_kind = field_work.unk2356;
    formation_selected_index = field_event_read_imm_or_var(1);
    mode_battle_standalone = 0;
    field_battle_not_requested = 0;
    field_scripted_battle_not_requested = 0;
    field_exit_request_pending = 1;
    field = field_event_read_imm_or_var(5);
    if (field != 0x7FFF) {
        entry = field_event_read_imm_or_var(7);
        field_event_record_departure();
        field_event_write_variable(2, entry);
        mode_field_map_id = field;
        field_skip_exit_snapshot = 1;
    }
    field_event_yield_requested = 1;
    field_current_event_actor->pc += 9;
}

/* 80093568: Request a battle once field control allows it (yield otherwise):
 * formation 80059508 = operand 1, its sound programs 8005954c (8001bbac) =
 * 800b2356, 800594f8 = 0; clear 800adbdc/800adbe0 and set 800adb88 (which ext
 * 7f waits on), then yield and continue at +3. */
void field_event_request_battle(void) {
    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_map_change_not_requested == 0 || field_music_stream_running != 0 || mode_music_load_pending == -1 ||
        field_actor_block_loading != 0) {
        field_event_yield_requested = 1;
    } else {
        mode_battle_kind = field_work.unk2356;
        formation_selected_index = field_event_read_imm_or_var(1);
        mode_battle_standalone = 0;
        field_battle_not_requested = 0;
        field_scripted_battle_not_requested = 0;
        field_exit_request_pending = 1;
        field_event_yield_requested = 1;
        field_current_event_actor->pc += 3;
    }
}

/* 80093664: Store byte (byte 2) of collision attribute record (byte 1) in variable
 * operand 3. */
void field_event_store_collision_attribute(void) {
    s32 value;

    value = field_collision_get_attribute_byte(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, value);
    field_current_event_actor->pc += 5;
}

/* 800936E4: Event: wait (pc-- back to fe), yielding, until 8004f350 is zero: the menus
 * requested by ext 55-5a, cf and da (800adb64) have been run by 800799d4, which
 * clears it. */
void field_event_wait_menus_done(void) {
    if (mode_menu_request_count == 0) {
        field_current_event_actor->pc += 1;
    } else {
        field_current_event_actor->pc -= 1;
    }
    field_event_yield_requested = 1;
}

/* 80093740: Event fe 55: request menu kind 0 (800adb64, run by 800799d4) with the menu
 * parameter 800b236c (ext 99) in 80059171; count 8004f350 up and yield. */
void field_event_open_menu0(void) {
    field_event_yield_requested = 1;
    field_menu_request = 0;
    menu_state_screen_parameter = field_menu_parameter;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 1;
}

/* 80093790: Request menu kind 6 (800adb64, run by 800799d4) with parameter 1 (80059171),
 * count 8004f350 up and yield. */
void field_event_open_menu6(void) {
    menu_state_screen_parameter = 1;
    field_menu_request = 6;
    field_event_yield_requested = 1;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 1;
}

/* 800937E0: Event fe 57: request menu kind 2 (800adb64); count 8004f350 up and yield. */
void field_event_open_menu2(void) {
    field_menu_request = 2;
    field_event_yield_requested = 1;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 1;
}

/* 80093824: Event fe 58: request menu kind 3 (800adb64) with parameter operand 1
 * (80059171); count 8004f350 up and yield. */
void field_event_open_menu3(void) {
    menu_state_screen_parameter = field_event_read_imm_or_var(1);
    field_menu_request = 3;
    field_event_yield_requested = 1;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 3;
}

/* 80093888: Request a field change through menu kind 1 (800adb64 = 1, run by 800799d4):
 * inhibit encounters, publish the current field (80092f44), variable 2 = entry
 * operand 3, field id 8004f34c = operand 1; counts 8004f350 up and yields. */
void field_event_change_map_menu(void) {
    s32 entry;
    s32 field;

    field_work.encounter_inhibition = -1;
    entry = field_event_read_imm_or_var(3);
    field = field_event_read_imm_or_var(1);
    field_event_record_departure();
    field_event_write_variable(2, entry);
    mode_field_map_id = field;
    field_event_empty_map_change_hook();
    field_menu_request = 1;
    field_event_yield_requested = 1;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 5;
}

/* 80093930: Event fe 56: request menu kind 1 (800adb64) with entry operand 1, also
 * stored in the game's +2320 and vars[1] and in variable 2; count 8004f350
 * up and yield. */
void field_event_open_menu1(void) {
    s16 entry;

    entry = field_event_read_imm_or_var(1);
    field_menu_request = 1;
    field_event_yield_requested = 1;
    game_current_data->vars[1] = entry;
    game_current_data->entry[2] = entry;
    field_event_variables[1] = entry;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 3;
}

/* 800939A0: Event fe 59: request menu kind 4 (800adb64) with parameter operand 1
 * (80059171); count 8004f350 up and yield. */
void field_event_open_menu4(void) {
    menu_state_screen_parameter = field_event_read_imm_or_var(1);
    field_menu_request = 4;
    field_event_yield_requested = 1;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 3;
}

/* 80093A04: Event fe 5a: request menu kind 5 (800adb64) with parameter operand 1
 * (80059171); count 8004f350 up and yield. */
void field_event_open_menu5(void) {
    menu_state_screen_parameter = field_event_read_imm_or_var(1);
    field_menu_request = 5;
    field_event_yield_requested = 1;
    mode_menu_request_count += 1;
    field_current_event_actor->pc += 3;
}

/* 80093A68: Clear the camera hold flag (0x8000, which stops 800726e8's shoulder-button
 * turns). */
void field_event_release_camera_hold(void) {
    field_view.flags &= 0x7FFF;
    field_current_event_actor->pc += 1;
}

/* 80093A98: Set the camera hold flag (0x8000). */
void field_event_hold_camera(void) {
    field_view.flags |= 0x8000;
    field_current_event_actor->pc += 1;
}

/* 80093AC8: Release script control: clear the encounter inhibition, both control
 * bytes and the camera's hold (0x8000) and eye unclamp (0x4000) flags; with
 * 0x4000 clear the follow camera (80073230) keeps its eye goal from sinking
 * below the floor of the last collision layer. */
void field_event_release_script_control(void) {
    field_work.encounter_inhibition = 0;
    field_work.script_control[0] = 0;
    field_work.script_control[1] = 0;
    field_view.flags &= 0x3FFF;
    field_current_event_actor->pc += 1;
}

/* 80093B10: Take script control (both control bytes, the camera's hold and eye unclamp
 * flags); re-runs while the field is not ready. */
void field_event_take_script_control(void) {
    field_work.encounter_inhibition = -1;
    field_work.script_control[0] = 1;
    field_work.script_control[1] = 1;
    field_view.flags |= 0xC000;
    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc -= 1;
        return;
    }
    field_current_event_actor->pc += 1;
}

/* 80093BB0: Clear script control byte 0. */
void field_event_clear_script_control0(void) {
    field_work.script_control[0] = 0;
    field_current_event_actor->pc += 1;
}

/* 80093BD4: Set script control byte 0. */
void field_event_set_script_control0(void) {
    field_work.script_control[0] = 1;
    field_current_event_actor->pc += 1;
}

/* 80093BFC: Clear script control byte 1. */
void field_event_clear_script_control1(void) {
    field_work.script_control[1] = 0;
    field_current_event_actor->pc += 1;
}

/* 80093C20: Set script control byte 1. */
void field_event_set_script_control1(void) {
    field_work.script_control[1] = 1;
    field_current_event_actor->pc += 1;
}

/* 80093C48: Clear the encounter inhibition. */
void field_event_allow_encounters(void) {
    field_work.encounter_inhibition = 0;
    field_current_event_actor->pc += 1;
}

/* 80093C6C: Inhibit encounters once the field is ready; yield until then. */
void field_event_inhibit_encounters(void) {
    if (field_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0) {
        field_event_yield_requested = 1;
    } else {
        field_work.encounter_inhibition = -1;
        field_current_event_actor->pc += 1;
    }
}

/* 80093CD0: Set variable operand 3 to the bytecode byte at raw offset operand 1 plus
 * index operand 5 (a data table inside the script). */
void field_event_load_code_byte(void) {
    u16 offset;

    offset = field_event_read_u16(1);
    offset += field_event_read_imm_or_var(5);
    field_event_write_variable(field_event_read_u16(3) & 0xFFFF, field_event_bytecode[offset]);
    field_current_event_actor->pc += 7;
}

/* 80093D48: Set variable operand 3 to the bytecode halfword at raw offset operand 1 plus
 * index operand 5, read unsigned when byte 7 is zero, else signed. */
void field_event_load_code_half(void) {
    u16 offset;

    offset = field_event_read_u16(1);
    offset += field_event_read_imm_or_var(5);
    if (EVENT_OPERAND_BYTE(7) == 0) {
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, field_event_bytecode[offset] | (field_event_bytecode[offset + 1] << 8));
    } else {
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, (s16)(field_event_bytecode[offset] + (field_event_bytecode[offset + 1] << 8)));
    }
    field_current_event_actor->pc += 8;
}

/* 80093E30: Door-style swing while flag 0x100000 is clear: the first run plays sound
 * effect 8 on channel 3; each later run turns the current descriptor's y
 * rotation by +0x20 (byte 1 zero) or -0x20, and after 30 turns it sets the
 * flag and advances. It does not yield, so the interpreter repeats it
 * within a pass. With the flag set it just advances. */
void field_event_swing_open(void) {
    FieldActor *actor;

    if (!(field_current_event_actor->flags & 0x100000)) {
        if (!(field_current_event_actor->state.word & 0x20)) {
            field_current_event_actor->state.word |= 0x20;
            field_current_event_actor->unkE2 = 0;
            field_sound_play_effect(8, 3);
        } else {
            field_current_event_actor->unkE2++;
            actor = field_current_event_actor;
            if (actor->unkE2 < 31) {
                if (field_event_bytecode[actor->pc + 1] == 0) {
                    field_view.components.descriptors[field_current_event_actor_index].rotation.vy += 0x20;
                } else {
                    field_view.components.descriptors[field_current_event_actor_index].rotation.vy -= 0x20;
                }
            } else {
                actor->unkE2 = 0;
                actor->flags |= 0x100000;
                actor->state.word &= ~0x20;
                field_current_event_actor->pc += 2;
            }
        }
    } else {
        field_current_event_actor->pc += 2;
    }
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80093FC0: The reverse swing while flag 0x100000 is set: the first run plays sound
 * effect 8 on channel 3; each later run turns the current descriptor's y
 * rotation by -0x20 (byte 1 zero) or +0x20, and after 30 turns it clears
 * the flag and advances. It does not yield. With the flag clear it just
 * advances. */
void field_event_swing_close(void) {
    FieldActor *actor;

    if (field_current_event_actor->flags & 0x100000) {
        if (!(field_current_event_actor->state.word & 0x20)) {
            field_current_event_actor->state.word |= 0x20;
            field_current_event_actor->unkE2 = 0;
            field_sound_play_effect(8, 3);
        } else {
            field_current_event_actor->unkE2++;
            actor = field_current_event_actor;
            if (actor->unkE2 < 31) {
                if (field_event_bytecode[actor->pc + 1] == 0) {
                    field_view.components.descriptors[field_current_event_actor_index].rotation.vy -= 0x20;
                } else {
                    field_view.components.descriptors[field_current_event_actor_index].rotation.vy += 0x20;
                }
            } else {
                actor->unkE2 = 0;
                actor->flags &= ~0x100000;
                actor->state.word &= ~0x20;
                field_current_event_actor->pc += 2;
            }
        }
    } else {
        field_current_event_actor->pc += 2;
    }
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094158: Event 0xe8 while flag 0x100000 is clear: the first run plays sound effect
 * 8 on channel 3 and copies the position to the target; each later run
 * moves the target (op5 0x1000/0x1001: target y minus/plus op1 * 16, the
 * original copying target z into the matrix y; otherwise op1 along the
 * descriptor's y rotation + op5 - 0x400) until op3 runs have passed, then
 * sets the flag and advances. It does not yield. With the flag set it just
 * advances. */
void field_event_shake_actor(void) {
    s32 angle;
    s32 sine;
    s32 cosine;
    FieldActor *actor;

    if (!(field_current_event_actor->flags & 0x100000)) {
        if (!(field_current_event_actor->state.word & 0x20)) {
            field_current_event_actor->state.word |= 0x20;
            field_current_event_actor->unkE2 = 0;
            field_sound_play_effect(8, 3);
            field_current_event_actor->target[0] = field_current_event_actor->position[0];
            field_current_event_actor->target[1] = field_current_event_actor->position[1];
            field_current_event_actor->target[2] = field_current_event_actor->position[2];
        } else {
            field_current_event_actor->unkE2++;
            if (field_current_event_actor->unkE2 < field_event_read_imm_or_var(3)) {
                switch (field_event_read_imm_or_var(5)) {
                case 0x1000:
                    field_current_event_actor->target[1] -= field_event_read_imm_or_var(1) * 16;
                    field_current_event_descriptor->matrix.t[1] = WHOLE(field_current_event_actor->target[2]);
                    break;
                case 0x1001:
                    field_current_event_actor->target[1] += field_event_read_imm_or_var(1) * 16;
                    field_current_event_descriptor->matrix.t[1] = WHOLE(field_current_event_actor->target[2]);
                    break;
                default:
                    angle = field_current_event_descriptor->rotation.vy + field_event_read_imm_or_var(5) - 0x400;
                    sine = gpu_get_cos(angle);
                    field_current_event_actor->target[0] += sine * field_event_read_imm_or_var(1);
                    cosine = gpu_get_sin(angle);
                    field_current_event_actor->target[2] -= cosine * field_event_read_imm_or_var(1);
                    field_current_event_descriptor->matrix.t[0] = WHOLE(field_current_event_actor->target[0]);
                    field_current_event_descriptor->matrix.t[2] = WHOLE(field_current_event_actor->target[2]);
                    break;
                }
            } else {
                actor = field_current_event_actor;
                actor->unkE2 = 0;
                actor->flags |= 0x100000;
                actor->state.word &= ~0x20;
                field_current_event_actor->pc += 7;
            }
        }
    } else {
        field_current_event_actor->pc += 7;
    }
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 800943AC: Event 0xe9, the reverse of 0xe8 while flag 0x100000 is set: the first run
 * plays sound effect 8 on channel 3 (the target is not reset); each later
 * run moves the target as 0xe8 does with the direction offsets negated (the
 * 0x1000/0x1001 y steps are the same) until op3 runs have passed, then
 * clears the flag and advances. It does not yield. With the flag clear it
 * just advances. */
void field_event_shake_actor_back(void) {
    FieldActor *actor;
    FieldActor *done;
    s32 angle;
    s32 sine;
    s32 cosine;

    actor = field_current_event_actor;
    if (actor->flags & 0x100000) {
        if (!(actor->state.word & 0x20)) {
            actor->state.word |= 0x20;
            actor->unkE2 = 0;
            field_sound_play_effect(8, 3);
        } else {
            actor->unkE2++;
            if (field_current_event_actor->unkE2 < field_event_read_imm_or_var(3)) {
                switch (field_event_read_imm_or_var(5)) {
                case 0x1000:
                    field_current_event_actor->target[1] -= field_event_read_imm_or_var(1) * 16;
                    field_current_event_descriptor->matrix.t[1] = WHOLE(field_current_event_actor->target[2]);
                    break;
                case 0x1001:
                    field_current_event_actor->target[1] += field_event_read_imm_or_var(1) * 16;
                    field_current_event_descriptor->matrix.t[1] = WHOLE(field_current_event_actor->target[2]);
                    break;
                default:
                    angle = field_current_event_descriptor->rotation.vy + field_event_read_imm_or_var(5) - 0x400;
                    sine = gpu_get_cos(angle);
                    field_current_event_actor->target[0] -= sine * field_event_read_imm_or_var(1);
                    cosine = gpu_get_sin(angle);
                    field_current_event_actor->target[2] += cosine * field_event_read_imm_or_var(1);
                    field_current_event_descriptor->matrix.t[0] = WHOLE(field_current_event_actor->target[0]);
                    field_current_event_descriptor->matrix.t[2] = WHOLE(field_current_event_actor->target[2]);
                    break;
                }
            } else {
                done = field_current_event_actor;
                done->unkE2 = 0;
                done->flags &= ~0x100000;
                done->state.word &= ~0x20;
                field_current_event_actor->pc += 7;
            }
        }
    } else {
        actor->pc += 7;
    }
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 800945D4: Set the play clock in variable 0x0a to operand 1 minutes : operand 3
 * seconds (low bytes), restart its frame count (8004f318) and stop it
 * (8004f328 = 0xff; 800a31e8 counts down with bit 2 and stops on bit 7). */
void field_event_set_play_clock(void) {
    s32 high;

    mode_play_clock_frame_count = 0;
    mode_play_clock_flags = 0xFF;
    high = field_event_read_imm_or_var(1);
    field_event_write_variable(10, ((high << 8) & 0xFF00) | (field_event_read_imm_or_var(3) & 0xFF));
    field_current_event_actor->pc += 5;
}

/* 80094650: Set the play clock mode 8004f328 to byte 1 (800a31e8: bit 2 counts the
 * clock in variable 0x0a down, bit 7 stops it). */
void field_event_set_play_clock_mode(void) {
    mode_play_clock_flags = EVENT_OPERAND_BYTE(1);
    field_current_event_actor->pc += 2;
}

/* 8009468C: Restart the play clock's frame count (8004f318) and stop the clock
 * (8004f328 = 0xff). */
void field_event_stop_play_clock(void) {
    mode_play_clock_frame_count = 0;
    mode_play_clock_flags = 0xFF;
    field_current_event_actor->pc += 1;
}

/* 800946BC: Turn the current actor's model by angle operand 1 about its x axis (state
 * mode 1, +70), applied on top of its matrix when drawn and in collision
 * tests. */
void field_event_rotate_model_x(void) {
    field_current_event_actor->state.word = (field_current_event_actor->state.word & ~3) | 1;
    field_current_event_actor->unk70 = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 80094710: Turn the current actor's model by angle operand 1 about its y axis (state
 * mode 2, +70), applied on top of its matrix when drawn and in collision
 * tests. */
void field_event_rotate_model_y(void) {
    field_current_event_actor->state.word = (field_current_event_actor->state.word & ~3) | 2;
    field_current_event_actor->unk70 = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 80094764: Turn the current actor's model by angle operand 1 about its z axis (state
 * mode 3, +70), applied on top of its matrix when drawn and in collision
 * tests. */
void field_event_rotate_model_z(void) {
    field_current_event_actor->state.word |= 3;
    field_current_event_actor->unk70 = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 800947B0: Turn the descriptor of the actor selected by byte 2 by operand 3 about the
 * axis byte 1 picks (0/1: x+/-, 2/3: y+/-, 4/5: z+/-) and rebuild its
 * matrix. */
void field_event_rotate_actor(void) {
    FieldDescriptor *descriptor;

    if (field_event_read_actor_index(2) != 0xFF) {
        descriptor = &field_view.components.descriptors[field_event_read_actor_index(2)];
        switch (EVENT_OPERAND_BYTE(1)) {
        case 0:
            descriptor->rotation.vx += field_event_read_imm_or_var(3);
            break;
        case 1:
            descriptor->rotation.vx -= field_event_read_imm_or_var(3);
            break;
        case 2:
            descriptor->rotation.vy += field_event_read_imm_or_var(3);
            break;
        case 3:
            descriptor->rotation.vy -= field_event_read_imm_or_var(3);
            break;
        case 4:
            descriptor->rotation.vz += field_event_read_imm_or_var(3);
            break;
        case 5:
            descriptor->rotation.vz -= field_event_read_imm_or_var(3);
            break;
        }
        field_descriptor_rebuild_matrix(field_event_read_actor_index(2));
    }
    field_current_event_actor->pc += 5;
}

/* 80094918: Set rotation axis byte 3 (0 x, 1 y, 2 z) of the current descriptor to operand
 * 1 and rebuild its matrix (80072254). */
void field_event_set_rotation(void) {
    switch (EVENT_OPERAND_BYTE(3)) {
    case 0:
        field_view.components.descriptors[field_current_event_actor_index].rotation.vx = field_event_read_imm_or_var(1);
        break;
    case 1:
        field_view.components.descriptors[field_current_event_actor_index].rotation.vy = field_event_read_imm_or_var(1);
        break;
    case 2:
        field_view.components.descriptors[field_current_event_actor_index].rotation.vz = field_event_read_imm_or_var(1);
        break;
    }
    field_current_event_actor->pc += 4;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094A5C: Add operand 1 to the current descriptor's x rotation and rebuild its
 * matrix (80072254). */
void field_event_rotate_x_add(void) {
    s32 delta;

    delta = field_event_read_imm_or_var(1);
    field_view.components.descriptors[field_current_event_actor_index].rotation.vx += delta;
    field_current_event_actor->pc += 3;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094ACC: Subtract operand 1 from the current descriptor's x rotation and rebuild
 * its matrix (80072254). */
void field_event_rotate_x_sub(void) {
    s32 delta;

    delta = field_event_read_imm_or_var(1);
    field_view.components.descriptors[field_current_event_actor_index].rotation.vx -= delta;
    field_current_event_actor->pc += 3;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094B3C: Add operand 1 to the current descriptor's y rotation and rebuild its
 * matrix (80072254). */
void field_event_rotate_y_add(void) {
    s32 delta;

    delta = field_event_read_imm_or_var(1);
    field_view.components.descriptors[field_current_event_actor_index].rotation.vy += delta;
    field_current_event_actor->pc += 3;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094BAC: Subtract operand 1 from the current descriptor's y rotation and rebuild
 * its matrix (80072254). */
void field_event_rotate_y_sub(void) {
    s32 delta;

    delta = field_event_read_imm_or_var(1);
    field_view.components.descriptors[field_current_event_actor_index].rotation.vy -= delta;
    field_current_event_actor->pc += 3;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094C1C: Add operand 1 to the current descriptor's z rotation and rebuild its
 * matrix (80072254). */
void field_event_rotate_z_add(void) {
    s32 delta;

    delta = field_event_read_imm_or_var(1);
    field_view.components.descriptors[field_current_event_actor_index].rotation.vz += delta;
    field_current_event_actor->pc += 3;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094C8C: Subtract operand 1 from the current descriptor's z rotation and rebuild
 * its matrix (80072254). */
void field_event_rotate_z_sub(void) {
    s32 delta;

    delta = field_event_read_imm_or_var(1);
    field_view.components.descriptors[field_current_event_actor_index].rotation.vz -= delta;
    field_current_event_actor->pc += 3;
    field_descriptor_rebuild_matrix(field_current_event_actor_index);
}

/* 80094CFC: First free slot of inventory list 0, or -1. */
s32 field_inventory_find_free_item_slot(void) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (game_current_data->itemCounts[i] == 0 || game_current_data->itemIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* 80094D4C: First free slot of inventory list 1, or -1. */
s32 field_inventory_find_free_weapon_slot(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (game_current_data->weaponCounts[i] == 0 || game_current_data->weaponIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* 80094D9C: First free slot of inventory list 2, or -1. */
s32 field_inventory_find_free_accessory_slot(void) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (game_current_data->accessoryCounts[i] == 0 || game_current_data->accessoryIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* 80094DEC: First free slot of inventory list 3, or -1. */
s32 field_inventory_find_free_gear_part_slot(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (game_current_data->gearPartCounts[i] == 0 || game_current_data->gearPartIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* 80094E3C: First free slot of inventory list 4, or -1. */
s32 field_inventory_find_free_gear_accessory_slot(void) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (game_current_data->gearAccessoryCounts[i] == 0 || game_current_data->gearAccessoryIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* 80094E8C: Slot of item `id` in inventory list 0, or -1. */
s32 field_inventory_find_item_slot(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (game_current_data->itemIds[i] == id && game_current_data->itemCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* 80094EDC: Slot of item `id` in inventory list 2, or -1. */
s32 field_inventory_find_accessory_slot(s32 id) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (game_current_data->accessoryIds[i] == id && game_current_data->accessoryCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* 80094F2C: Slot of item `id` in inventory list 1, or -1. */
s32 field_inventory_find_weapon_slot(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (game_current_data->weaponIds[i] == id && game_current_data->weaponCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* 80094F7C: Slot of item `id` in inventory list 3, or -1. */
s32 field_inventory_find_gear_part_slot(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (game_current_data->gearPartIds[i] == id && game_current_data->gearPartCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* 80094FCC: Slot of item `id` in inventory list 4, or -1. */
s32 field_inventory_find_gear_accessory_slot(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (game_current_data->gearAccessoryIds[i] == id && game_current_data->gearAccessoryCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* 8009501C: Id array of the inventory list selected by item >> 8. */
u8 *field_inventory_get_id_array(s32 item) {
    switch (item >> 8) {
    case 0:
        return game_current_data->itemIds;
    case 1:
        return game_current_data->weaponIds;
    case 2:
        return game_current_data->accessoryIds;
    case 3:
        return game_current_data->gearPartIds;
    case 4:
        return game_current_data->gearAccessoryIds;
    }
    return NULL;
}

/* 800950A0: Count array of the inventory list selected by item >> 8. */
u8 *field_inventory_get_count_array(s32 item) {
    switch (item >> 8) {
    case 0:
        return game_current_data->itemCounts;
    case 1:
        return game_current_data->weaponCounts;
    case 2:
        return game_current_data->accessoryCounts;
    case 3:
        return game_current_data->gearPartCounts;
    case 4:
        return game_current_data->gearAccessoryCounts;
    }
    return NULL;
}

/* 80095124: Slot holding `item` (list in the high byte), or -1; 0 for no list. */
s32 field_inventory_find_slot(s32 item) {
    switch (item >> 8) {
    case 0:
        return field_inventory_find_item_slot(item);
    case 1:
        return field_inventory_find_weapon_slot(item - 0x100);
    case 2:
        return field_inventory_find_accessory_slot(item - 0x200);
    case 3:
        return field_inventory_find_gear_part_slot(item - 0x300);
    case 4:
        return field_inventory_find_gear_accessory_slot(item - 0x400);
    }
    return 0;
}

/* 800951B8: First free slot of the list selected by item >> 8, or -1; 0 for no list. */
s32 field_inventory_find_free_slot(s32 item) {
    switch (item >> 8) {
    case 0:
        return field_inventory_find_free_item_slot();
    case 1:
        return field_inventory_find_free_weapon_slot();
    case 2:
        return field_inventory_find_free_accessory_slot();
    case 3:
        return field_inventory_find_free_gear_part_slot();
    case 4:
        return field_inventory_find_free_gear_accessory_slot();
    }
    return 0;
}

void field_event_stop_hold(void);

/* 8009524C: Stop the current actor (field_event_stop_hold) and advance. */
void field_event_stop(void) {
    field_event_stop_hold();
    field_current_event_actor->pc += 1;
}

/* 80095284: Stop the current actor: clear its motion (+30, +40 and its model's velocity
 * and speed), mark its heading turned (0x8000) and yield without advancing, so
 * the instruction holds the actor every frame. */
void field_event_stop_hold(void) {
    Sprite *model;
    u16 state;

    model = field_view.components.descriptors[field_current_event_actor_index].model;
    field_event_yield_requested = 1;
    field_current_event_actor->unk030[0] = 0;
    field_current_event_actor->unk030[1] = 0;
    field_current_event_actor->unk030[2] = 0;
    field_current_event_actor->unk40[0] = 0;
    field_current_event_actor->unk40[1] = 0;
    field_current_event_actor->unk40[2] = 0;
    state = field_current_event_actor->heading | 0x8000;
    field_current_event_actor->heading_goal = state;
    field_current_event_actor->heading = state;
    model->speed_x = 0;
    model->speed_z = 0;
    model->speed = 0;
}

/* 80095300: Set the terrain angle from operand 1. */
void field_event_set_terrain_angle(void) {
    field_work.terrain_angle = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* Trigger zone operand 1 and one of its corners packed as (z << 16) + x,
 * the point format of NormalClip.  The zone is addressed inside each
 * access (not through a Zone pointer): GCC then forms the address as
 * (scaled index + table), which ties the zone byte's register differently
 * from `zone = &field_trigger_zones[i]' (base + scaled index). */
#define EVENT_ZONE (field_trigger_zones[EVENT_OPERAND_BYTE(1)])
#define EVENT_ZONE_CORNER(k) ((EVENT_ZONE.corner[k].z << 16) + EVENT_ZONE.corner[k].x)
/* The actor the player controls. */
#define CONTROLLED_ACTOR (field_view.components.descriptors[field_work.controlled].actor)
/* The trigger zone named by the operand byte code[1]. */
#define CODE_ZONE(code) (field_trigger_zones[(code)[1]])
#define CODE_ZONE_CORNER(code, k) ((CODE_ZONE(code).corner[k].z << 16) + CODE_ZONE(code).corner[k].x)

/* 8009533C: Call (operand 2) when the controlled actor stands inside trigger zone
 * operand 1 and the call stack has room; otherwise skip. */
void field_event_call_in_zone(void) {
    FieldActor *player;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    a = EVENT_ZONE_CORNER(0);
    player = field_view.components.descriptors[field_work.controlled].actor;
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    b = EVENT_ZONE_CORNER(1);
    c = EVENT_ZONE_CORNER(2);
    d = EVENT_ZONE_CORNER(3);
    if (NormalClip(a, b, point) >= 0 && NormalClip(b, c, point) >= 0 &&
        NormalClip(c, d, point) >= 0 && NormalClip(d, a, point) >= 0 &&
        (field_current_event_actor->state.word & 0x1C0) != 0x100) {
        field_current_event_actor->call_stack[(field_current_event_actor->state.word >> 6) & 7] = field_current_event_actor->pc + 4;
        field_current_event_actor->pc = field_event_read_u16(2);
        field_current_event_actor->state.word = (field_current_event_actor->state.word & ~0x1C0) | (((((field_current_event_actor->state.word >> 6) & 7) + 1) & 7) << 6);
        return;
    }
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 4;
}

/* 80095520: Call the script at operand 2 (pushing pc + 4) when the controlled actor
 * stands inside trigger zone byte 1, the zone's corner-0 height lies within
 * its body (y - height .. y) and the call stack has room; otherwise continue
 * (pc + 4).
 * The operand address is formed from pc and then rebased on the bytecode
 * (set twice, so sched keeps it where the original has it). */
void field_event_call_in_zone_height(void) {
    u8 *code;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    code = (u8 *)field_current_event_actor->pc;
    code += (s32)field_event_bytecode;
    if (WHOLE(CONTROLLED_ACTOR->position[1]) > CODE_ZONE(code).corner[0].y &&
        WHOLE(CONTROLLED_ACTOR->position[1]) - (u16)CONTROLLED_ACTOR->height < CODE_ZONE(code).corner[0].y) {
        a = CODE_ZONE_CORNER(code, 0);
        b = CODE_ZONE_CORNER(code, 1);
        point = (WHOLE(CONTROLLED_ACTOR->position[2]) << 16) + WHOLE(CONTROLLED_ACTOR->position[0]);
        c = CODE_ZONE_CORNER(code, 2);
        d = CODE_ZONE_CORNER(code, 3);
        if (NormalClip(a, b, point) >= 0 && NormalClip(b, c, point) >= 0 &&
            NormalClip(c, d, point) >= 0 && NormalClip(d, a, point) >= 0 &&
            (field_current_event_actor->state.word & 0x1C0) != 0x100) {
            field_current_event_actor->call_stack[(field_current_event_actor->state.word >> 6) & 7] = field_current_event_actor->pc + 4;
            field_current_event_actor->pc = field_event_read_u16(2);
            field_current_event_actor->state.word = (field_current_event_actor->state.word & ~0x1C0) | (((((field_current_event_actor->state.word >> 6) & 7) + 1) & 7) << 6);
            return;
        }
    }
    field_event_batch_limit += 1;
    field_current_event_actor->pc += 4;
}

/* 80095734: Continue when the controlled actor is inside trigger zone operand 1,
 * else jump to operand 2. */
void field_event_branch_unless_in_zone(void) {
    FieldActor *player;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    a = EVENT_ZONE_CORNER(0);
    player = field_view.components.descriptors[field_work.controlled].actor;
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    b = EVENT_ZONE_CORNER(1);
    c = EVENT_ZONE_CORNER(2);
    d = EVENT_ZONE_CORNER(3);
    if (NormalClip(a, b, point) >= 0 && NormalClip(b, c, point) >= 0 &&
        NormalClip(c, d, point) >= 0 && NormalClip(d, a, point) >= 0) {
        field_current_event_actor->pc += 4;
        return;
    }
    field_current_event_actor->pc = field_event_read_u16(2);
    field_event_batch_limit += 1;
}

/* 800958C0: Continue when the controlled actor is inside trigger zone operand 1 and
 * the zone's height lies within the actor's body, else jump to operand 2.
 * The operand address is formed from pc and then rebased on the bytecode
 * (set twice, so sched keeps it where the original has it). */
void field_event_branch_unless_in_zone_height(void) {
    u8 *code;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    code = (u8 *)field_current_event_actor->pc;
    code += (s32)field_event_bytecode;
    if (WHOLE(CONTROLLED_ACTOR->position[1]) > CODE_ZONE(code).corner[0].y &&
        WHOLE(CONTROLLED_ACTOR->position[1]) - (u16)CONTROLLED_ACTOR->height < CODE_ZONE(code).corner[0].y) {
        a = CODE_ZONE_CORNER(code, 0);
        b = CODE_ZONE_CORNER(code, 1);
        point = (WHOLE(CONTROLLED_ACTOR->position[2]) << 16) + WHOLE(CONTROLLED_ACTOR->position[0]);
        c = CODE_ZONE_CORNER(code, 2);
        d = CODE_ZONE_CORNER(code, 3);
        if (NormalClip(a, b, point) >= 0 && NormalClip(b, c, point) >= 0 &&
            NormalClip(c, d, point) >= 0 && NormalClip(d, a, point) >= 0) {
            field_current_event_actor->pc += 4;
            return;
        }
    }
    field_current_event_actor->pc = field_event_read_u16(2);
    field_event_batch_limit += 1;
}

/* 80095A7C: Project a selected actor's origin to the screen. */
void field_event_project_actor_to_screen(s32 *x, s32 *y) {
    SVECTOR origin;
    MATRIX m;
    union {
        long word;
        DVECTOR xy;
    } screen;
    long depth;
    long flag;

    CompMatrix(&field_view.scaled_world, &field_view.components.descriptors[field_event_read_actor_index_or_leader(1)].transform, &m);
    origin.vx = 0;
    origin.vy = 0;
    origin.vz = 0;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    RotTransPers(&origin, &screen.word, &depth, &flag);
    *y = screen.xy.vy;
    *x = screen.xy.vx;
}

/* 80095B3C: Event fe 02: with 800adc18 set, advance at once. Otherwise yield and continue
 * when the origin of the actor selected by byte 1 (8009cd7c: the party leader
 * when none) projects inside the screen with a 32-pixel margin (x 33..287, y
 * 33..191), else jump to operand 2. */
void field_event_branch_unless_well_on_screen(void) {
    s32 x;
    s32 y;

    field_event_project_actor_to_screen(&x, &y);
    if (field_camera_cut_timer != 0) {
        field_current_event_actor->pc += 4;
        return;
    }
    if (y > 32 && y < 192 && x > 32 && x < 288) {
        field_current_event_actor->pc += 4;
    } else {
        field_current_event_actor->pc = field_event_read_u16(2);
    }
    field_event_yield_requested = 1;
}

/* 80095C00: Continue (pc + 4) when the origin of the actor selected by byte 1 (the
 * party leader by default) projects inside the 320x224 screen, otherwise
 * jump to operand 2; yields. While 800adc18 (the field start countdown)
 * runs it continues at once without yielding. */
void field_event_branch_unless_on_screen(void) {
    s32 x;
    s32 y;

    field_event_project_actor_to_screen(&x, &y);
    if (field_camera_cut_timer != 0) {
        field_current_event_actor->pc += 4;
        return;
    }
    if (y > 0 && y < 224 && x > 0 && x < 320) {
        field_current_event_actor->pc += 4;
    } else {
        field_current_event_actor->pc = field_event_read_u16(2);
    }
    field_event_yield_requested = 1;
}

/* 80095CC4: Event fe 05: continue (advance 6) when the actor selected by byte 1 is on
 * collision layer operand 2, else (also with no actor) jump to operand 4. */
void field_event_branch_unless_on_layer(void) {
    FieldActor *actor;

    if (field_event_read_actor_index(1) != 0xFF) {
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        if (field_event_read_imm_or_var(2) == actor->layer) {
            field_current_event_actor->pc += 6;
            return;
        }
    }
    field_current_event_actor->pc = field_event_read_u16(4);
}

/* 80095D6C: Event fe 06: continue (advance 6) when the collision triangle under the actor
 * selected by byte 1 has attribute operand 2, else (also with no actor) jump to
 * operand 4. */
void field_event_branch_unless_on_attribute(void) {
    FieldActor *actor;
    u8 attribute;

    if (field_event_read_actor_index(1) != 0xFF) {
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        attribute = field_view.components.collision_triangles[actor->layer][actor->triangle[actor->layer]].attribute;
        if (field_event_read_imm_or_var(2) == attribute) {
            field_current_event_actor->pc += 6;
            return;
        }
    }
    field_current_event_actor->pc = field_event_read_u16(4);
}

/* 80095E48: Continue (pc + 6) when the actor selected by byte 1 is nearer than
 * operand 2 (3D distance) to the running actor; otherwise, or with no such
 * actor, jump to operand 4. */
void field_event_branch_unless_near(void) {
    FieldActor *other;
    FieldDescriptor *descriptor;
    s32 distance;

    if (field_event_read_actor_index(1) != 0xFF) {
        descriptor = &field_view.components.descriptors[field_event_read_actor_index(1)];
        other = descriptor->actor;
        distance = field_compute_vector_length(WHOLE(field_current_event_descriptor->actor->position[0]) - WHOLE(other->position[0]),
                                 WHOLE(field_current_event_descriptor->actor->position[1]) - WHOLE(other->position[1]),
                                 WHOLE(field_current_event_descriptor->actor->position[2]) - WHOLE(other->position[2]));
        if (distance < field_event_read_imm_or_var(2)) {
            field_current_event_actor->pc += 6;
            return;
        }
    }
    field_current_event_actor->pc = field_event_read_u16(4);
}

/* 80095F24: Continue when the party's gold is at least the 32-bit operand 1,
 * otherwise jump to operand 5. */
void field_event_branch_unless_gold(void) {
    u8 *operand;

    operand = &field_event_bytecode[field_current_event_actor->pc];
    if (game_current_data->gold >=
        operand[1] + (operand[2] << 8) + (operand[3] << 16) + (operand[4] << 24)) {
        field_current_event_actor->pc += 7;
        return;
    }
    field_current_event_actor->pc = field_event_read_u16(5);
}

/* 80095FB8: Add operand 1 to the party's gold, capped at 9999999. */
void field_event_add_gold(void) {
    s32 gold;

    gold = game_current_data->gold + field_event_read_imm_or_var(1);
    if (gold > 9999999) {
        gold = 9999999;
    }
    game_current_data->gold = gold;
    field_current_event_actor->pc += 3;
}

/* 8009601C: Take operand 1 from the party's gold, not below zero. */
void field_event_remove_gold(void) {
    s32 amount;
    s32 gold;

    amount = field_event_read_imm_or_var(1);
    gold = game_current_data->gold;
    gold -= amount;
    if (gold < 0) {
        gold = 0;
    }
    game_current_data->gold = gold;
    field_current_event_actor->pc += 3;
}

/* 80096078: Continue when raw operand 1 shares a bit with `bits`, otherwise jump to
 * operand 3. */
void field_event_branch_unless_operand_has_bits(s32 bits) {
    if (field_event_read_u16(1) & bits & 0xFFFF) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

/* 800960E4: Continue when raw operand 1 equals `value`, otherwise jump to operand 3. */
void field_event_branch_unless_operand_equals(s32 value) {
    if ((field_event_read_u16(1) & 0xFFFF) == (value & 0xFFFF)) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

void field_event_branch_unless_operand_has_bits(s32 bits);
void field_event_branch_unless_operand_equals(s32 value);

/* 80096150: Continue (pc + 5) when the held buttons (800afe9c) equal raw operand 1,
 * otherwise jump to operand 3. */
void field_event_branch_unless_buttons_equal(void) {
    field_event_branch_unless_operand_equals(field_pad_port0_held);
}

/* 80096178: Continue (pc + 5) when the buttons held since the last record (800afc6c,
 * gathered by 800a31e8, cleared by 33) equal raw operand 1, otherwise jump
 * to operand 3. */
void field_event_branch_unless_buttons_seen_equal(void) {
    field_event_branch_unless_operand_equals(field_play_record_buttons);
}

/* 800961A0: Continue at +5 when the held buttons (800afe9c) share a bit with raw operand
 * 1, otherwise jump to operand 3 (80096078). */
void field_event_branch_unless_buttons(void) {
    field_event_branch_unless_operand_has_bits(field_pad_port0_held);
}

/* 800961C8: Continue at +5 when the buttons seen held since 33 last cleared them
 * (800afc6c, gathered each frame by 800a31e8) share a bit with raw operand 1,
 * otherwise jump to operand 3 (80096078). */
void field_event_branch_unless_buttons_seen(void) {
    field_event_branch_unless_operand_has_bits(field_play_record_buttons);
}

/* 800961F0: Forget the buttons seen held (800afc6c = 0), which 32 and e3 test. */
void field_event_forget_buttons_seen(void) {
    field_play_record_buttons = 0;
    field_current_event_actor->pc += 1;
}

s32 field_inventory_find_slot(s32 item);
u8 *field_inventory_get_count_array(s32 item);
u8 *field_inventory_get_id_array(s32 item);

/* 80096214: Store the carried count of item operand 1 (its list in the high byte) in
 * variable operand 3 (0 when not carried). */
void field_event_store_item_count(void) {
    s32 item;
    s32 slot;
    u8 *counts;

    item = field_event_read_imm_or_var(1);
    slot = field_inventory_find_slot(item);
    counts = field_inventory_get_count_array(item);
    field_inventory_get_id_array(item);
    if (slot != -1) {
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, counts[slot]);
    } else {
        field_event_write_variable(field_event_read_u16(3) & 0xFFFF, 0);
    }
    field_current_event_actor->pc += 5;
}

/* 800962C0: Continue when item operand 1 is carried, otherwise jump to operand 3. */
void field_event_branch_unless_item(void) {
    if (field_inventory_find_slot(field_event_read_imm_or_var(1)) != -1) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

s32 field_inventory_add_item(s32 item);

/* 8009631C: Give one of item operand 1. */
void field_event_give_item(void) {
    field_inventory_add_item(field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 3;
}

s32 field_inventory_find_free_slot(s32 item);

/* 8009635C: Add one of `item` (list in the high byte) up to 99, or take a free slot.
 * Declared int without a return value, as the original's unfilled branch
 * delay slot shows (v0 stays live to the exit). */
s32 field_inventory_add_item(s32 item) {
    s32 slot;
    u8 *counts;
    u8 *ids;

    slot = field_inventory_find_slot(item);
    counts = field_inventory_get_count_array(item);
    ids = field_inventory_get_id_array(item);
    if (slot != -1) {
        if (counts[slot] < 99) {
            counts[slot]++;
        }
    } else {
        slot = field_inventory_find_free_slot(item);
        if (slot != -1) {
            ids[slot] = item;
            counts[slot] = 1;
        }
    }
}

/* 8009640C: Take one of item operand 1; an emptied slot's id becomes 0xFF. */
void field_event_take_item(void) {
    s32 item;
    s32 slot;
    u8 *ids;
    u8 *counts;

    item = field_event_read_imm_or_var(1);
    slot = field_inventory_find_slot(item);
    if (slot != -1) {
        ids = field_inventory_get_id_array(item);
        counts = field_inventory_get_count_array(item);
        if (--counts[slot] == 0) {
            ids[slot] = 0xFF;
        }
    }
    field_current_event_actor->pc += 3;
}

/* 800964B0: Continue when character operand 1 is in the party, otherwise jump to
 * operand 2. */
void field_event_branch_unless_in_party(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (EVENT_OPERAND_BYTE(1) == mode_party_members[i]) {
            field_current_event_actor->pc += 4;
            return;
        }
    }
    field_current_event_actor->pc = field_event_read_u16(2);
}

/* 80096534: Continue when game flag bit operand 1 (of unk1D30) is set, otherwise
 * jump to operand 2. */
void field_event_branch_unless_game_flag(void) {
    if ((game_current_data->joined >> EVENT_OPERAND_BYTE(1)) & 1) {
        field_current_event_actor->pc += 4;
        return;
    }
    field_current_event_actor->pc = field_event_read_u16(2);
}

/* 800965A8: Set game flag bit operand 1 of unk1D30. */
void field_event_set_game_flag(void) {
    game_current_data->joined |= 1 << EVENT_OPERAND_BYTE(1);
    field_current_event_actor->pc += 2;
}

/* 800965F4: Clear game flag bit operand 1 of unk1D30. */
void field_event_clear_game_flag(void) {
    game_current_data->joined &= ~(1 << EVENT_OPERAND_BYTE(1));
    field_current_event_actor->pc += 2;
}

/* 80096644: Continue (pc + 5) when variable 0 is below operand 1, otherwise jump to
 * operand 3. */
void field_event_branch_unless_var0_below(void) {
    s32 value;

    value = field_event_read_imm_or_var(1);
    if (field_event_read_variable(0) < value) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

/* 800966B4: Continue (pc + 5) when variable 0 is above operand 1, otherwise jump to
 * operand 3. */
void field_event_branch_unless_var0_above(void) {
    s32 value;

    value = field_event_read_imm_or_var(1);
    if (value < field_event_read_variable(0)) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

/* 80096724: Continue (pc + 5) when variable 0 equals operand 1, otherwise jump to
 * operand 3. */
void field_event_branch_unless_var0_equal(void) {
    s32 value;

    value = field_event_read_imm_or_var(1);
    if (field_event_read_variable(0) == value) {
        field_current_event_actor->pc += 5;
    } else {
        field_current_event_actor->pc = field_event_read_u16(3);
    }
}

/* 80096790: Set variable 0 to operand 1, raising the batch limit by 32. */
void field_event_set_var0(void) {
    field_event_batch_limit += 32;
    field_event_write_variable(0, field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 3;
}

/* 800967E8: Copy variable 0 into variable operand 1. */
void field_event_store_var0(void) {
    s32 reference;

    reference = field_event_read_u16(1) & 0xFFFF;
    field_event_write_variable(reference & 0xFFFF, field_event_read_variable(0));
    field_current_event_actor->pc += 3;
}

/* 80096844: Restore HP of party slot `slot`, up to its maximum. */
void field_party_restore_slot_hp(s32 slot, s32 amount) {
    game_current_data->characters[mode_party_members[slot]].hp += amount;
    if (game_current_data->characters[mode_party_members[slot]].maxHp < game_current_data->characters[mode_party_members[slot]].hp) {
        game_current_data->characters[mode_party_members[slot]].hp = game_current_data->characters[mode_party_members[slot]].maxHp;
    }
}

/* 800968CC: Reduce HP of party slot `slot`, leaving at least 1. */
void field_party_reduce_slot_hp(s32 slot, s32 amount) {
    s32 hp;

    hp = game_current_data->characters[mode_party_members[slot]].hp - amount;
    if (hp <= 0) {
        hp = 1;
    }
    game_current_data->characters[mode_party_members[slot]].hp = hp;
}

/* 80096920: Restore EP of party slot `slot`, up to its maximum. */
void field_party_restore_slot_ep(s32 slot, s32 amount) {
    game_current_data->characters[mode_party_members[slot]].ep += amount;
    if (game_current_data->characters[mode_party_members[slot]].maxEp < game_current_data->characters[mode_party_members[slot]].ep) {
        game_current_data->characters[mode_party_members[slot]].ep = game_current_data->characters[mode_party_members[slot]].maxEp;
    }
}

/* 800969A8: Reduce EP of party slot `slot`, leaving at least 1. */
void field_party_reduce_slot_ep(s32 slot, s32 amount) {
    s32 ep;

    ep = game_current_data->characters[mode_party_members[slot]].ep - amount;
    if (ep <= 0) {
        ep = 1;
    }
    game_current_data->characters[mode_party_members[slot]].ep = ep;
}

/* 800969FC: Reduce the HP of the party members masked by entry byte 3 & 3 of 800aea2c (7
 * all, then slot 0, 1, 2) by selected operand 1 (bit 0x80 of byte 3), leaving
 * at least 1 (800968cc). */
void field_event_reduce_party_hp(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = field_event_party_slot_masks[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (mode_party_members[slot] != 0xFF && (mask & 1)) {
            field_party_reduce_slot_hp(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    field_current_event_actor->pc += 4;
}

/* 80096AF4: Set player control's jump mode (800b2344), animation mode and jump repeat
 * delay (800b2340) from operands 1, 3 and 5 and clear the repeat countdown
 * (800b2342). */
void field_event_set_jump_mode(void) {
    field_work.jump_mode = field_event_read_imm_or_var(1);
    field_work.animation_mode = field_event_read_imm_or_var(3);
    field_work.repeat_delay = field_event_read_imm_or_var(5);
    field_work.repeat_remaining = 0;
    field_current_event_actor->pc += 7;
}

/* 80096B58: Store the HP of the character in party slot byte 3 in variable operand 1
 * (nothing when the slot is empty). */
void field_event_store_party_hp(void) {
    if (mode_party_members[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(3)]].hp);
    }
    field_current_event_actor->pc += 4;
}

/* 80096C40: Store the EP of the character in party slot byte 3 in variable operand 1
 * (nothing when the slot is empty). */
void field_event_store_party_ep(void) {
    if (mode_party_members[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        field_event_write_variable(field_event_read_u16(1) & 0xFFFF, game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(3)]].ep);
    }
    field_current_event_actor->pc += 4;
}

/* 80096D28: Set the HP of the character in party slot byte 1 to operand 2, capped at its
 * maximum, when party slot byte 3 is occupied; byte 3 is also the high byte of
 * operand 2. */
void field_event_set_party_hp(void) {
    s32 hp;

    if (mode_party_members[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        hp = field_event_read_imm_or_var(2);
        if (game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(1)]].maxHp < hp) {
            hp = game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(1)]].maxHp;
        }
        game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(1)]].hp = hp;
    }
    field_current_event_actor->pc += 4;
}

/* 80096E20: Set the EP of the character in party slot byte 1 to operand 2, capped at its
 * maximum, when party slot byte 3 is occupied; byte 3 is also the high byte of
 * operand 2. */
void field_event_set_party_ep(void) {
    s32 ep;

    if (mode_party_members[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        ep = field_event_read_imm_or_var(2);
        if (game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(1)]].maxEp < ep) {
            ep = game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(1)]].maxEp;
        }
        game_current_data->characters[mode_party_members[EVENT_OPERAND_BYTE(1)]].ep = ep;
    }
    field_current_event_actor->pc += 4;
}

/* 80096F18: Restore the EP of the party members masked by entry byte 3 & 3 of 800aea2c by
 * selected operand 1 (bit 0x80 of byte 3), up to their maximum (80096920); the
 * same as 7e (the HP restore 80096844 has no caller). */
void field_event_restore_party_ep_7c(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = field_event_party_slot_masks[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (mode_party_members[slot] != 0xFF && (mask & 1)) {
            field_party_restore_slot_ep(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    field_current_event_actor->pc += 4;
}

/* 80097010: Reduce the EP of the party members masked by entry byte 3 & 3 of 800aea2c by
 * selected operand 1 (bit 0x80 of byte 3), leaving at least 1 (800969a8). */
void field_event_reduce_party_ep(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = field_event_party_slot_masks[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (mode_party_members[slot] != 0xFF && (mask & 1)) {
            field_party_reduce_slot_ep(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    field_current_event_actor->pc += 4;
}

/* 80097108: Restore the EP of the party members masked by entry byte 3 & 3 of 800aea2c by
 * selected operand 1 (bit 0x80 of byte 3), up to their maximum (80096920). */
void field_event_restore_party_ep(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = field_event_party_slot_masks[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (mode_party_members[slot] != 0xFF && (mask & 1)) {
            field_party_restore_slot_ep(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    field_current_event_actor->pc += 4;
}

/* 80097200: Fully restore HP and EP of character operand 1. */
void field_event_restore_character(void) {
    s32 id;

    id = field_event_read_imm_or_var(1);
    game_current_data->characters[id].hp = game_current_data->characters[id].maxHp;
    game_current_data->characters[id].ep = game_current_data->characters[id].maxEp;
    field_current_event_actor->pc += 3;
}

/* 80097264: Fully restore every character's HP. */
void field_event_restore_all_hp(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        game_current_data->characters[i].hp = game_current_data->characters[i].maxHp;
    }
    field_current_event_actor->pc += 1;
}

/* 800972AC: Fully restore every character's EP. */
void field_event_restore_all_ep(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        game_current_data->characters[i].ep = game_current_data->characters[i].maxEp;
    }
    field_current_event_actor->pc += 1;
}

/* 800972F4: Yield once. */
void field_event_yield(void) {
    field_event_yield_requested = 1;
    field_current_event_actor->pc += 1;
}

/* 8009731C: Reset fade channel 0 (8007d93c) and fade the screen back in from full over
 * operand-1 frames (80071e58: only while 800adc08 marks a fade-out, and
 * only in fade mode 2). */
void field_event_fade_in(void) {
    field_fade_init_channel(0);
    field_fade_in(field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 3;
}

/* 80097364: Fade the screen out on channel 0 over operand-1 frames (80071dcc: levels
 * rise from 0 to full with blend 2, unless 800adc08 already marks a
 * fade-out, and only in fade mode 2). */
void field_event_fade_out(void) {
    field_fade_out(field_event_read_imm_or_var(1));
    field_current_event_actor->pc += 3;
}

/* 800973A4: Start reading file 0xb8 + operand 2 of the current directory ahead as map
 * data (resident 8001b484, slot byte 1); retried without yielding until that
 * read is under way or done, then continue at +4. */
void field_event_read_map_ahead(void) {
    if (mode_read_map_ahead(field_event_read_u16(2) & 0xFFFF, EVENT_OPERAND_BYTE(1)) == 0) {
        field_current_event_actor->pc += 4;
    }
}

/* 80097410: Jump table: continue at three-byte entry operand 1 after this instruction
 * (pc + 3 + 3 * operand 1). */
void field_event_jump_table(void) {
    field_current_event_actor->pc += field_event_read_imm_or_var(1) * 3 + 3;
}

/* 8009744C: Facing octant (0..7) of the controlled actor. */
s32 field_actor_get_controlled_facing_octant(void) {
    return (((field_view.components.descriptors[field_work.controlled].actor->heading_goal + 0x100) >> 9) + 2) & 7;
}

s32 field_event_move(s32 speed);

/* 8009749C: Walk the current actor as 54 toward descriptor byte 1 at height selected
 * operand 2 (flags byte 4) for at most operand-5 steps (latched in the slot);
 * continues at +7 once within reach or out of steps, else yields. */
void field_event_move_to_actor_limited(void) {
    FieldActor *other;

    field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 2;
    other = field_view.components.descriptors[EVENT_OPERAND_BYTE(1)].actor;
    field_current_event_actor->target[0] = WHOLE(other->position[0]);
    field_current_event_actor->target[2] = WHOLE(other->position[2]);
    field_current_event_actor->target[1] = WHOLE(other->position[1]);
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(5);
    }
    if (field_event_move(field_event_read_imm_or_var(5)) == 0) {
        field_current_event_actor->pc += 7;
    }
}

/* 800975C0: Walk the current actor (80097a50 move mode 2) toward the position of
 * descriptor byte 1 (a raw index here; the reach check reads it as an actor
 * selector and widens the reach by both actors' +1e radii) at height selected
 * operand 2 (flags byte 4, bit 0x80) without a step limit; continues at +5 once
 * within reach or when no actor is selected, else yields. */
void field_event_move_to_actor(void) {
    FieldActor *other;

    field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 2;
    other = field_view.components.descriptors[EVENT_OPERAND_BYTE(1)].actor;
    field_current_event_actor->target[0] = WHOLE(other->position[0]);
    field_current_event_actor->target[2] = WHOLE(other->position[2]);
    field_current_event_actor->target[1] = WHOLE(other->position[1]);
    field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
    if (field_event_move(0xFFFF) == 0) {
        field_current_event_actor->pc += 5;
    }
}

/* 800976A8: Walk the current actor (80097a50 move mode 1) toward its start position
 * offset by selected x/z/y operands 1/3/6 (flags byte 5) for at most operand-8
 * steps (latched in the slot); continues at +10 once within reach or out of
 * steps, else yields. */
void field_event_move_by_limited(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].move_mode == 0) {
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 1;
        field_current_event_actor->target[0] = WHOLE(field_current_event_actor->position[0]);
        field_current_event_actor->target[1] = WHOLE(field_current_event_actor->position[1]);
        field_current_event_actor->target[2] = WHOLE(field_current_event_actor->position[2]);
    }
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(8);
    }
    if (field_event_move(field_event_read_imm_or_var(8)) == 0) {
        field_current_event_actor->pc += 10;
    }
}

/* 800977A4: Walk the current actor (80097a50 move mode 1) toward its start position
 * offset by selected x/z/y operands 1/3/6 (flags byte 5) without a step limit;
 * continues at +8 once within reach, else yields. */
void field_event_move_by(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].move_mode == 0) {
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 1;
        field_current_event_actor->target[0] = WHOLE(field_current_event_actor->position[0]);
        field_current_event_actor->target[1] = WHOLE(field_current_event_actor->position[1]);
        field_current_event_actor->target[2] = WHOLE(field_current_event_actor->position[2]);
    }
    field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
    if (field_event_move(0xFFFF) == 0) {
        field_current_event_actor->pc += 8;
    }
}

/* 80097864: Walk the current actor (80097a50 move mode 3; the start position is kept as
 * the target) toward a point along angle operand 1 (x as (start x + (cos << 5))
 * >> 12 and z as start z - (sin << 5) >> 12, as 80097a50 computes them) at
 * height offset selected operand 3 (flags byte 7, bit 0x80), for at most
 * operand-5 steps (latched in the slot); continues at +8 once within reach or
 * out of steps, else yields. */
void field_event_move_angle(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].move_mode == 0) {
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 3;
        field_current_event_actor->target[0] = WHOLE(field_current_event_actor->position[0]);
        field_current_event_actor->target[1] = WHOLE(field_current_event_actor->position[1]);
        field_current_event_actor->target[2] = WHOLE(field_current_event_actor->position[2]);
    }
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(5);
    }
    if (field_event_move(field_event_read_imm_or_var(5)) == 0) {
        field_current_event_actor->pc += 8;
    }
}

/* 80097954: Walk the current actor as 4c toward selected x/z/y operands 1/3/6 (flags byte
 * 5) for at most operand-8 steps (latched in the slot); continues at +10 once
 * within reach or out of steps, else yields. */
void field_event_move_to_limited(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(8);
    }
    if (field_event_move(field_event_read_imm_or_var(8)) == 0) {
        field_current_event_actor->pc += 10;
    }
}

/* 800979F0: Walk the current actor (80097a50 in the slot's move mode, 0 after a finished
 * move) toward selected x/z/y operands 1/3/6 (flags byte 5, bits
 * 0x80/0x40/0x20) without a step limit; continues at +8 once within reach, else
 * yields. */
void field_event_move_to(void) {
    field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
    if (field_event_move(0xFFFF) == 0) {
        field_current_event_actor->pc += 8;
    }
}

/* 80097A50: Walk the current actor toward its move target (move modes 0-3: operand
 * position, offset from the target, another actor's reach, or a point at an
 * angle): set the step from its speed, face along it and return 0 once within
 * reach or out of steps, else -1. */
s32 field_event_move(s32 speed) {
    VECTOR unused; /* unused in the original; reserves 16 bytes */
    VECTOR delta;
    VECTOR direction;
    VECTOR step;
    Sprite *model;
    s32 reach;
    s32 extra;
    s32 turning;
    s32 x;
    s32 y;
    s32 z;
    s32 from_x;
    s32 from_y;
    s32 from_z;
    s32 angle;
    s32 distance;
    s32 scale;

    turning = -1;
    y = 0;
    z = 0;
    model = field_view.components.descriptors[field_current_event_actor_index].model;
    x = 0;
    if (field_view.components.descriptors[field_current_event_actor_index].actor->layer_flags & 0x2000) {
        model->speed = 0x8000000 / (u16)field_current_event_actor->unk76;
    } else {
        model->speed = 0x4000000 / (u16)field_current_event_actor->unk76;
    }
    reach = field_compute_abs_via_gte(model->speed >> 15) + 1;
    extra = 0;
    switch (field_current_event_actor->slots[field_current_event_actor->slot].move_mode) {
    case 0:
        x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(5));
        z = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
        y = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(5));
        break;
    case 1:
        x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(5)) + field_current_event_actor->target[0];
        z = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5)) + field_current_event_actor->target[2];
        y = field_current_event_actor->target[1] + field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(5));
        break;
    case 2:
        if (field_event_read_actor_index(1) == 0xFF) {
            return 0;
        }
        extra = field_compute_abs_via_gte((u16)field_view.components.descriptors[field_event_read_actor_index(1)].actor->gravity.part.whole +
                              (u16)field_current_event_actor->gravity.part.whole);
        x = field_current_event_actor->target[0];
        z = field_current_event_actor->target[2];
        y = field_event_read_selected_operand_80(2, EVENT_OPERAND_BYTE(4));
        break;
    case 3:
        angle = field_event_read_imm_or_var(1) & 0xFFF;
        x = (field_current_event_actor->target[0] + (gpu_get_cos(angle) << 5)) >> 12;
        z = field_current_event_actor->target[2] + (-(gpu_get_sin(angle) << 5) >> 12);
        y = field_current_event_actor->target[1] + field_event_read_selected_operand_80(3, EVENT_OPERAND_BYTE(7));
        break;
    }
    from_x = WHOLE(field_current_event_actor->position[0]);
    from_z = WHOLE(field_current_event_actor->position[2]);
    from_y = WHOLE(field_current_event_actor->position[1]);
    delta.vx = from_x - x;
    delta.vy = from_y - y;
    delta.vz = from_z - z;
    VectorNormal(&delta, &direction);
    scale = model->speed >> 8;
    step.vx = -((direction.vx * scale) >> 4);
    step.vy = -((direction.vy * scale) >> 4);
    step.vz = -((direction.vz * scale) >> 4);
    field_current_event_actor->unk40[0] = step.vx;
    field_current_event_actor->unk40[1] = step.vy;
    field_current_event_actor->unk40[2] = step.vz;
    field_current_event_actor->unk40[1] = 0;
    distance = field_compute_vector_length(x - from_x, y - from_y, z - from_z);
    if (WHOLE(field_current_event_actor->unk40[0]) == 0 && WHOLE(field_current_event_actor->unk40[2]) == 0) {
        turning = 0;
    }
    field_current_event_actor->flags |= 0x400000;
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0 || reach + extra >= distance) {
        if (turning == -1) {
            if (speed != 0) {
                if (!(field_current_event_actor->flags & 0x8000)) {
                    field_current_event_actor->heading_goal = field_current_event_actor->heading = (u16)field_current_event_actor->heading_goal | 0x8000;
                } else {
                    field_current_event_actor->heading_goal = field_current_event_actor->heading = field_current_event_actor->unk11C | 0x8000;
                }
            } else {
                field_current_event_actor->heading_goal = field_current_event_actor->heading = field_compute_xz_heading(&step) | 0x8000;
            }
        }
        field_current_event_actor->unkEC = (field_current_event_actor->position[1] + step.vy) >> 16;
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 0;
        field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
        return 0;
    }
    if (turning == -1) {
        field_current_event_actor->heading_goal = field_current_event_actor->heading = field_compute_xz_heading(&step) | 0x8000;
    }
    field_current_event_actor->unkEC = (field_current_event_actor->position[1] + step.vy) >> 16;
    field_current_event_actor->flags |= 0x40000;
    field_current_event_actor->slots[field_current_event_actor->slot].value--;
    field_event_yield_requested = 1;
    return -1;
}

s32 field_event_turn_move(s32 speed);

/* 80098038: Turn-move the current actor (80099ac0 move mode 2) toward the actor of
 * selector byte 1 for at most operand-2 steps (latched in the slot); continues
 * at +4 once within reach, out of steps or when no actor is selected, else
 * yields. */
void field_event_turn_move_to_actor_limited(void) {
    field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 2;
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(2);
    }
    if (field_event_turn_move(field_event_read_imm_or_var(2)) == 0) {
        field_current_event_actor->pc += 4;
    }
}

/* 800980FC: Turn-move the current actor (80099ac0 move mode 2) toward the actor of
 * selector byte 1 (reach widened by both actors' +1e radii) without a step
 * limit; continues at +2 once within reach or when no actor is selected, else
 * yields. */
void field_event_turn_move_to_actor(void) {
    field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 2;
    field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
    if (field_event_turn_move(0xFFFF) == 0) {
        field_current_event_actor->pc += 2;
    }
}

/* 80098184: Turn-move the current actor (80099ac0 move mode 3; the start position is kept
 * as the target) toward the point 4096 units along angle operand 1 from its
 * start, for at most operand-3 steps (latched in the slot); continues at +5
 * once within reach or out of steps, else yields. */
void field_event_turn_move_angle(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].move_mode == 0) {
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 3;
        field_current_event_actor->target[0] = WHOLE(field_current_event_actor->position[0]);
        field_current_event_actor->target[1] = WHOLE(field_current_event_actor->position[1]);
        field_current_event_actor->target[2] = WHOLE(field_current_event_actor->position[2]);
    }
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(3);
    }
    if (field_event_turn_move(field_event_read_imm_or_var(3)) == 0) {
        field_current_event_actor->pc += 5;
    }
}

/* 80098274: Turn-move the current actor (80099ac0 move mode 1) toward its start position
 * offset by selected x/z operands 1/3 (flags byte 5) for at most operand-6
 * steps (latched in the slot); continues at +8 once within reach or out of
 * steps, else yields. */
void field_event_turn_move_by_limited(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].move_mode == 0) {
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 1;
        field_current_event_actor->target[0] = WHOLE(field_current_event_actor->position[0]);
        field_current_event_actor->target[1] = WHOLE(field_current_event_actor->position[1]);
        field_current_event_actor->target[2] = WHOLE(field_current_event_actor->position[2]);
    }
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(6);
    }
    if (field_event_turn_move(field_event_read_imm_or_var(6)) == 0) {
        field_current_event_actor->pc += 8;
    }
}

/* 80098370: Turn-move the current actor (80099ac0 move mode 1) toward its start position
 * offset by selected x/z operands 1/3 (flags byte 5) without a step limit;
 * continues at +6 once within reach, else yields. */
void field_event_turn_move_by(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].move_mode == 0) {
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 1;
        field_current_event_actor->target[0] = WHOLE(field_current_event_actor->position[0]);
        field_current_event_actor->target[1] = WHOLE(field_current_event_actor->position[1]);
        field_current_event_actor->target[2] = WHOLE(field_current_event_actor->position[2]);
    }
    field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
    if (field_event_turn_move(0xFFFF) == 0) {
        field_current_event_actor->pc += 6;
    }
}

/* 80098430: Turn-move the current actor (80099ac0 move mode 0) toward selected x/z
 * operands 1/3 (flags byte 5) for at most operand-6 steps (latched in the
 * slot); continues at +8 once within reach or out of steps, else yields. */
void field_event_turn_move_to_limited(void) {
    field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 0;
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(6);
    }
    if (field_event_turn_move(field_event_read_imm_or_var(6)) == 0) {
        field_current_event_actor->pc += 8;
    }
}

/* 800984EC: Event fe 1d: set the piece drift vector (800b2078 piece_drift) to selected
 * operands 1, 3, 5 (flags 0x80/0x40/0x20 of byte 7) and enable it
 * (piece_drift_mode bit 0x80). */
void field_event_set_piece_drift(void) {
    field_work.piece_drift[0] = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(7));
    field_work.piece_drift[1] = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7));
    field_work.piece_drift[2] = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7));
    field_work.piece_drift_mode |= 0x80;
    field_current_event_actor->pc += 8;
}

/* 800985BC: Event 0x74 (debug): print variable op1 unless 800c268c is set. */
void field_event_debug_print(void) {
    s32 value;

    if (field_monitor_absent == 0) {
        value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
        console_report_printf("DEB=%xh %d \n", value, value);
    }
    field_current_event_actor->pc += 3;
}

/* 8009861C: Event fe 73: store the planar length (80099a4c) of (op7 - op3, op9 - op5) in
 * variable operand 1; x1, z1, x2, z2 are selected operands by flags
 * 0x40/0x20/0x10/0x08 of byte 11. */
void field_event_store_planar_distance(void) {
    s32 x1;
    s32 z1;
    s32 x2;
    s32 z2;

    x1 = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(11));
    z1 = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(11));
    x2 = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(11));
    z2 = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(11));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_compute_planar_length(x2 - x1, z2 - z1));
    field_current_event_actor->pc += 12;
}

/* 80098738: Event fe 76: store the distance (80099a04) between the points (op3, op5, op7)
 * and (op9, op11, op13) in variable operand 1; selected operands by flags 0x40,
 * 0x20, 0x20, 0x10, 0x08 and 0x08 of byte 15, the pairs as the code reads them.
 */
void field_event_store_distance(void) {
    s32 x1;
    s32 y1;
    s32 z1;
    s32 x2;
    s32 y2;
    s32 z2;

    x1 = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(15));
    y1 = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(15));
    z1 = field_event_read_selected_operand_20(7, EVENT_OPERAND_BYTE(15));
    x2 = field_event_read_selected_operand_10(9, EVENT_OPERAND_BYTE(15));
    y2 = field_event_read_selected_operand_08(11, EVENT_OPERAND_BYTE(15));
    z2 = field_event_read_selected_operand_08(13, EVENT_OPERAND_BYTE(15));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_compute_vector_length(x2 - x1, z2 - z1, y2 - y1));
    field_current_event_actor->pc += 16;
}

/* 800988B8: Event fe 72: store 80073930(op3, op5, op7) in variable operand 1: angle op3
 * turned toward op5 by step op7 the short way round, stopping at the goal
 * (12-bit); selected operands by flags 0x40/0x20/0x10 of byte 9. */
void field_event_store_turn_step(void) {
    s32 a;
    s32 b;
    s32 value;

    a = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(9));
    b = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(9));
    value = field_turn_angle_toward(a, b, field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(9)));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 10;
}

/* 8009899C: Event fe 71: store the current actor's facing goal (+106, 12 bits) in
 * variable operand 1. */
void field_event_store_facing(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_current_event_actor->heading_goal & 0xFFF);
    field_current_event_actor->pc += 3;
}

/* 800989F0: Event fe 75: store the facing goal (12 bits) of the actor selected by byte 1
 * in variable operand 2 (no actor: nothing is stored). */
void field_event_store_actor_facing(void) {
    s32 index;
    FieldActor *actor;

    index = field_event_read_actor_index(1);
    if (index != 0xFF) {
        actor = field_view.components.descriptors[index].actor;
        field_event_write_variable(field_event_read_u16(2) & 0xFFFF, actor->heading_goal & 0xFFF);
    }
    field_current_event_actor->pc += 4;
}

/* 80098A7C: Event fe 1c: place the current actor at x, z, y = selected operands 1, 3, 5
 * (whole units; flags 0x80/0x40/0x20 of byte 7), set its flag 0x10000 and layer
 * flag 0x200000, and mirror the position into its descriptor and model. */
void field_event_set_position(void) {
    Sprite *model;

    model = field_view.components.descriptors[field_current_event_actor_index].model;
    field_current_event_actor->flags |= 0x10000;
    field_current_event_actor->layer_flags |= 0x200000;
    field_current_event_actor->position[0] = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(7)) << 16;
    field_current_event_actor->position[2] = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7)) << 16;
    field_current_event_actor->position[1] = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7)) << 16;
    field_view.components.descriptors[field_current_event_actor_index].matrix.t[0] = WHOLE(field_current_event_actor->position[0]);
    field_view.components.descriptors[field_current_event_actor_index].matrix.t[1] = WHOLE(field_current_event_actor->position[1]);
    field_view.components.descriptors[field_current_event_actor_index].matrix.t[2] = WHOLE(field_current_event_actor->position[2]);
    model->x = field_current_event_actor->position[0];
    model->y = field_current_event_actor->position[1];
    model->z = field_current_event_actor->position[2];
    field_current_event_actor->pc += 8;
}

void field_event_walk_straight(s32 mode);

/* 80098C00: Walk the current actor in a straight line without a step limit (80098cac mode
 * 0; the slot value is 0xffff). Byte 1 zero (9 bytes): set up a walk to
 * selected x/z/y operands 2/4/6 (flags byte 8, bits 0x80/0x40/0x20) over
 * distance / speed steps (speed (0x4000000 / +76) >> 16, doubled with layer
 * flag 0x2000) and continue at +9, the step instruction. Byte 1 non-zero (2
 * bytes): step the walk set up 9 bytes earlier (re-reading its operands),
 * yielding each frame; on arrival snap to the target, play the arrival
 * animation (+e6) and continue at +2. */
void field_event_walk(void) {
    field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
    field_event_walk_straight(0);
}

/* 80098C3C: Walk as 10 with a step limit (80098cac mode 1): the setup form (byte 1 zero,
 * 9 bytes) latches operand 11 in the slot, which is operand 2 of the following
 * step form (byte 1 non-zero, 4 bytes). A walk stopped by the limit does not
 * snap to the target; the step form continues at +4. */
void field_event_walk_limited(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].value == 0xFFFF) {
        field_current_event_actor->slots[field_current_event_actor->slot].value = field_event_read_imm_or_var(11);
    }
    field_event_walk_straight(1);
}

/* 80098CAC: Walk the current actor to an operand position over a step count derived
 * from its speed (first call sets the step, later calls advance it); at the
 * end snap to the target (when the slot asks) and continue with the next
 * instruction, then update the model matrix and its animation. */
void field_event_walk_straight(s32 mode) {
    VECTOR from;
    Sprite *model;
    s32 speed;
    s32 animation;
    s32 x;
    s32 y;
    s32 z;
    s32 steps;
    u16 pc;
    u8 *code;

    model = field_view.components.descriptors[field_current_event_actor_index].model;
    if (field_view.components.descriptors[field_current_event_actor_index].actor->layer_flags & 0x2000) {
        speed = (0x8000000 / (u16)field_current_event_actor->unk76) >> 16;
    } else {
        speed = (0x4000000 / (u16)field_current_event_actor->unk76) >> 16;
    }
    if (speed == 0) {
        speed = 1;
    }
    field_current_event_actor->flags |= 0x10000;
    pc = field_current_event_actor->pc;
    code = pc + field_event_bytecode;
    animation = 1;
    if (code[1] == 0) {
        x = field_event_read_selected_operand_80(2, code[8]) << 16;
        z = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(8)) << 16;
        y = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(8)) << 16;
        steps = field_compute_vector_length((x - field_current_event_actor->position[0]) >> 16, (y - field_current_event_actor->position[1]) >> 16,
                              (z - field_current_event_actor->position[2]) >> 16) / speed;
        field_current_event_actor->unk102 = steps;
        if ((s16)steps == 0) {
            field_current_event_actor->unk102 = steps + 1;
        }
        field_current_event_actor->target[0] = (x - field_current_event_actor->position[0]) / (s16)field_current_event_actor->unk102;
        field_current_event_actor->target[1] = (y - field_current_event_actor->position[1]) / (s16)field_current_event_actor->unk102;
        field_current_event_actor->target[2] = (z - field_current_event_actor->position[2]) / (s16)field_current_event_actor->unk102;
        if (x >> 16 != WHOLE(field_current_event_actor->position[0]) || z >> 16 != WHOLE(field_current_event_actor->position[2])) {
            field_current_event_actor->heading_goal = field_current_event_actor->heading = -ratan2(field_current_event_actor->target[2] >> 16, WHOLE(field_current_event_actor->target[0]));
        }
        field_current_event_actor->pc += 9;
    } else {
        if ((s16)field_current_event_actor->unk102 <= 0 || field_current_event_actor->slots[field_current_event_actor->slot].value == 0) {
            field_current_event_actor->pc = pc - 9;
            if (field_current_event_actor->slots[field_current_event_actor->slot].value != 0) {
                from.vx = field_current_event_actor->position[0];
                from.vy = field_current_event_actor->position[1];
                from.vz = field_current_event_actor->position[2];
                field_current_event_actor->position[0] = field_event_read_selected_operand_80(2, EVENT_OPERAND_BYTE(8)) << 16;
                field_current_event_actor->position[2] = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(8)) << 16;
                field_current_event_actor->position[1] = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(8)) << 16;
                field_current_event_actor->unk030[0] = field_current_event_actor->position[0] - from.vx;
                field_current_event_actor->unk030[1] = field_current_event_actor->position[1] - from.vy;
                field_current_event_actor->unk030[2] = field_current_event_actor->position[2] - from.vz;
            }
            animation = field_current_event_actor->unkE6;
            if (mode == 0) {
                field_current_event_actor->pc += 11;
            } else {
                field_current_event_actor->pc += 13;
            }
            field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
        } else {
            field_current_event_actor->position[0] += field_current_event_actor->target[0];
            field_current_event_actor->position[2] += field_current_event_actor->target[2];
            field_current_event_actor->position[1] += field_current_event_actor->target[1];
            field_current_event_actor->unk030[0] = field_current_event_actor->target[0];
            field_current_event_actor->unk030[1] = field_current_event_actor->target[1];
            field_current_event_actor->unk030[2] = field_current_event_actor->target[2];
            field_current_event_actor->slots[field_current_event_actor->slot].value--;
            field_event_yield_requested = animation;
        }
        field_current_event_actor->unk102--;
        field_view.components.descriptors[field_current_event_actor_index].matrix.t[0] = WHOLE(field_current_event_actor->position[0]);
        field_view.components.descriptors[field_current_event_actor_index].matrix.t[1] = WHOLE(field_current_event_actor->position[1]);
        field_view.components.descriptors[field_current_event_actor_index].matrix.t[2] = WHOLE(field_current_event_actor->position[2]);
        model->x = field_current_event_actor->position[0];
        model->y = field_current_event_actor->position[1];
        model->z = field_current_event_actor->position[2];
    }
    if (field_current_event_actor->unk0EA != 0xFF) {
        animation = field_current_event_actor->unk0EA;
    }
    if (field_current_event_actor->unkE8 != animation && !(field_current_event_actor->flags & 0x2000000)) {
        field_current_event_actor->unkE8 = animation;
        field_actor_start_animation(model, animation, field_current_event_descriptor);
    }
    ((void (*)(void *, s32, FieldDescriptor *))field_actor_set_planar_velocity)(model, field_current_event_actor->heading, field_current_event_descriptor);
}

/* 80099214: Arc jump of the current actor. Byte 1 & 3 = 0-2 sets one up (11 bytes):
 * target x/z selected operands 2/4 and y operand 6 (with byte 1 bit 0x80 a
 * layer whose floor gives y), flags byte 10; operand 8 gives the steps (mode
 * 0), a speed (1: steps = planar distance / it) or the peak height (2); yields
 * and continues at +11. Byte 1 & 3 = 3 (2 bytes) steps the jump set up 11 bytes
 * earlier, yielding each frame, and once done lands on that target and
 * continues at +2; byte 1 = 0xf instead re-reads the floor triangles and
 * continues.
 * Case 2 copies the height difference before testing its sign and negates the
 * difference itself; `value` is the function's scratch variable, reused for the
 * heading in case 3. */
void field_event_arc(void) {
    VECTOR normals[4];
    SVECTOR points[4];
    SVECTOR unused[4]; /* unused in the original; reserves 0x20 bytes */
    Sprite *model;
    u8 *code;
    u16 pc;
    u8 mode;
    s32 steps;
    s32 x;
    s32 z;
    s32 y;
    s32 layer;
    s32 peak;
    s32 value;
    s32 magnitude;

    pc = field_current_event_actor->pc;
    model = field_view.components.descriptors[field_current_event_actor_index].model;
    code = pc + field_event_bytecode;
    field_current_event_actor->flags |= 0x10000;
    mode = code[1];
    switch (mode & 3) {
    case 0:
        steps = field_event_read_selected_operand_10(8, code[10]);
    setup:
        if (steps == 0) {
            steps = 1;
        }
        x = field_event_read_selected_operand_80(2, EVENT_OPERAND_BYTE(10));
        z = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(10));
        if (!(EVENT_OPERAND_BYTE(1) & 0x80)) {
            y = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(10));
        } else {
            layer = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(10));
            field_collision_find_floor_triangle(x, z, layer, &points[layer], &normals[layer]);
            y = points[layer].vy;
            field_current_event_actor->layer = layer;
        }
        model->speed_y = -(model->gravity * steps / 2);
        model->speed_y += ((y << 16) - field_current_event_actor->position[1]) / steps;
        field_current_event_actor->target[1] = 0;
        ACTOR_ARC_STEPS(field_current_event_actor) = steps;
        field_current_event_actor->unk102 = 0;
        field_current_event_actor->pc += 11;
        field_current_event_actor->target[0] = ((x << 16) - field_current_event_actor->position[0]) / (steps + 1);
        field_current_event_actor->target[2] = ((z << 16) - field_current_event_actor->position[2]) / (steps + 1);
        break;
    case 1:
        x = field_event_read_selected_operand_80(2, code[10]);
        z = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(10));
        x = (x << 16) - field_current_event_actor->position[0];
        z = (z << 16) - field_current_event_actor->position[2];
        steps = field_event_read_selected_operand_10(8, EVENT_OPERAND_BYTE(10));
        steps = field_compute_planar_length(x >> 16, z >> 16) / steps;
        goto setup;
    case 2:
        field_event_read_selected_operand_80(2, code[10]);
        field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(10));
        y = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(10));
        peak = -field_event_read_selected_operand_10(8, EVENT_OPERAND_BYTE(10));
        y = (y << 16) - field_current_event_actor->position[1];
        model->speed_y = -(SquareRoot0(WHOLE(model->gravity) * (peak << 1)) << 16);
        SquareRoot0(peak);
        value = peak - (y >> 16);
        magnitude = value;
        if (value < 0) {
            magnitude = -value;
        }
        steps = SquareRoot0(magnitude);
        if (steps < 0) {
            steps = -steps;
        }
        goto setup;
    case 3:
        if (mode == 0xF) {
            for (layer = 0; layer < field_view.components.layer_count - 1; layer++) {
                field_current_event_actor->triangle[layer] = field_collision_find_floor_triangle(WHOLE(field_current_event_actor->position[0]), WHOLE(field_current_event_actor->position[2]),
                                                            layer, &points[layer], &normals[layer]);
            }
            field_current_event_actor->flags &= ~0x10000;
            field_current_event_actor->layer_flags &= ~0x200000;
            field_current_event_actor->pc += 2;
            break;
        }
        if ((s16)field_current_event_actor->unk102 < ACTOR_ARC_STEPS(field_current_event_actor)) {
            field_current_event_actor->position[0] += field_current_event_actor->target[0];
            field_current_event_actor->position[2] += field_current_event_actor->target[2];
            field_current_event_actor->position[1] += model->speed_y;
            model->speed_y += model->gravity;
            if ((field_current_event_actor->target[0] != 0 || field_current_event_actor->target[2] != 0) && !(field_current_event_actor->flags & 0x8000)) {
                value = field_compute_xz_heading((VECTOR *)field_current_event_actor->target) | 0x8000;
                field_current_event_actor->heading = value;
                field_current_event_actor->heading_goal = value;
            }
        } else {
            field_current_event_actor->pc = pc - 11;
            x = field_event_read_selected_operand_80(2, EVENT_OPERAND_BYTE(10));
            z = field_event_read_selected_operand_40(4, EVENT_OPERAND_BYTE(10));
            if (EVENT_OPERAND_BYTE(1) & 0x80) {
                layer = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(10));
                field_current_event_actor->triangle[layer] = field_collision_find_floor_triangle(x, z, layer, &points[layer], &normals[layer]);
                y = points[layer].vy;
            } else {
                y = field_event_read_selected_operand_20(6, EVENT_OPERAND_BYTE(10));
            }
            model->speed_y = 0;
            field_current_event_actor->position[0] = x << 16;
            field_current_event_actor->position[1] = y << 16;
            field_current_event_actor->position[2] = z << 16;
            field_current_event_actor->flags &= ~0x10000;
            field_current_event_actor->layer_flags &= ~0x200000;
            field_current_event_actor->pc += 13;
        }
        field_view.components.descriptors[field_current_event_actor_index].matrix.t[0] = WHOLE(field_current_event_actor->position[0]);
        field_view.components.descriptors[field_current_event_actor_index].matrix.t[1] = WHOLE(field_current_event_actor->position[1]);
        field_view.components.descriptors[field_current_event_actor_index].matrix.t[2] = WHOLE(field_current_event_actor->position[2]);
        model->x = field_current_event_actor->position[0];
        model->y = field_current_event_actor->position[1];
        model->z = field_current_event_actor->position[2];
        field_current_event_actor->unk102++;
        break;
    }
    field_event_yield_requested = 1;
}

/* 80099980: Turn-move the current actor (80099ac0 move mode 0) toward selected x/z
 * operands 1/3 (flags byte 5) without a step limit; continues at +6 once within
 * reach, else yields. */
void field_event_turn_move_to(void) {
    field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 0;
    field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
    if (field_event_turn_move(0xFFFF) == 0) {
        field_current_event_actor->pc += 6;
    }
}

/* 80099A04: Length of (dx, dy, dz). */
s32 field_compute_vector_length(s32 dx, s32 dy, s32 dz) {
    VECTOR v;
    VECTOR squares;

    v.vx = dx;
    v.vy = dy;
    v.vz = dz;
    Square0(&v, &squares);
    return SquareRoot0(squares.vx + squares.vy + squares.vz);
}

/* 80099A4C: Length of (dx, dz). */
s32 field_compute_planar_length(s32 dx, s32 dz) {
    VECTOR v;
    VECTOR squares;

    v.vx = dx;
    v.vy = dz;
    v.vz = 0;
    Square0(&v, &squares);
    return SquareRoot0(squares.vx + squares.vy);
}

/* 80099A8C: Absolute value through the GTE square and square root. */
s32 field_compute_abs_via_gte(s32 x) {
    VECTOR v;
    VECTOR squares;

    v.vx = x;
    Square0(&v, &squares);
    return SquareRoot0(squares.vx);
}

/* Compiled-out debug trace of the actor a move targets. */
#define MOVE_TRACE_TARGET(actor) do { } while (0)

/* 80099AC0: Turn-move toward the slot's target (mode 1: operand position plus the
 * target offset, 2: another actor, 3: an angle from the target offset,
 * 0/4: operand position). Within reach (or when the slot's step count is
 * 0) set the final heading, clear the slot and return 0; otherwise count
 * down, face the target and return -1.
 * `value` is the function's scratch variable (the slot's step count, the
 * facing, and in case 2 the current actor: all three are $a1 in the
 * original), and case 2 keeps the descriptor table in `model` ($a0, as at
 * the top). Both case-2 loads go to variables set elsewhere, so sched1 does
 * not sink them next to their use as it does a once-set pseudo (a register
 * birth): they stay live across the index multiply, where the call result
 * and the multiply hold $v0/$v1, and sched2 then drops them just ahead of
 * the descriptor add. The trace's loop note keeps both gravity loads after
 * the other actor's load, with its load-delay nop. */
s32 field_event_turn_move(s32 speed) {
    VECTOR delta;
    Sprite *model;
    s32 reach;
    s32 extra;
    s32 from_x;
    s32 from_z;
    s32 x;
    s32 z;
    s32 angle;
    s32 distance;
    FieldActor *other;
    s32 index;
    s32 value;

    extra = 0;
    z = 0;
    model = field_view.components.descriptors[field_current_event_actor_index].model;
    x = 0;
    if (field_view.components.descriptors[field_current_event_actor_index].actor->layer_flags & 0x2000) {
        model->speed = 0x8000000 / (u16)field_current_event_actor->unk76;
    } else if (model->speed == 0) {
        model->speed = 0x4000000 / (u16)field_current_event_actor->unk76;
    }
    reach = field_compute_abs_via_gte(model->speed >> 15) + 1;
    from_x = WHOLE(field_current_event_actor->position[0]);
    from_z = WHOLE(field_current_event_actor->position[2]);
    switch (field_current_event_actor->slots[field_current_event_actor->slot].move_mode) {
    case 1:
        x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(5)) + field_current_event_actor->target[0];
        z = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5)) + field_current_event_actor->target[2];
        break;
    case 2:
        if (field_event_read_actor_index(1) == 0xFF) {
            return 0;
        }
        index = field_event_read_actor_index(1);
        value = (s32)field_current_event_actor;
        model = (Sprite *)field_view.components.descriptors;
        other = ((FieldDescriptor *)model)[index].actor;
        MOVE_TRACE_TARGET(other);
        extra = field_compute_abs_via_gte((u16)other->gravity.part.whole + (u16)((FieldActor *)value)->gravity.part.whole);
        x = WHOLE(other->position[0]);
        z = WHOLE(other->position[2]);
        if (EVENT_OPERAND_BYTE(1) == field_work.controlled) {
            field_current_event_actor->flags |= 0x200000;
        }
        break;
    case 3:
        angle = field_event_read_imm_or_var(1) & 0xFFF;
        x = field_current_event_actor->target[0] + ((gpu_get_cos(angle) << 12) >> 12);
        z = field_current_event_actor->target[2] + (-(gpu_get_sin(angle) << 12) >> 12);
        break;
    case 0:
    case 4:
        x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(5));
        z = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
        break;
    }
    delta.vx = x - from_x;
    delta.vy = 0;
    delta.vz = z - from_z;
    distance = field_compute_planar_length(delta.vx, delta.vz);
    field_current_event_actor->flags |= 0x400000;
    value = field_current_event_actor->slots[field_current_event_actor->slot].value;
    if (value == 0 || reach + extra >= distance) {
        if (speed != 0) {
            if (!(field_current_event_actor->flags & 0x8000)) {
                field_current_event_actor->heading_goal = field_current_event_actor->heading = (u16)field_current_event_actor->heading_goal | 0x8000;
            } else {
                field_current_event_actor->heading_goal = field_current_event_actor->heading = field_current_event_actor->unk11C | 0x8000;
            }
        } else {
            field_current_event_actor->heading_goal = field_current_event_actor->heading = field_compute_xz_heading(&delta);
        }
        field_current_event_actor->slots[field_current_event_actor->slot].value = 0xFFFF;
        field_current_event_actor->slots[field_current_event_actor->slot].move_mode = 0;
        field_current_event_actor->flags &= 0xFDDFF7FF;
        return 0;
    }
    field_current_event_actor->slots[field_current_event_actor->slot].value = value - 1;
    value = field_compute_xz_heading(&delta);
    field_event_yield_requested = 1;
    field_current_event_actor->heading_goal = field_current_event_actor->heading = value;
    return -1;
}

/* 80099EF8: Store the current actor's party character (+e4, set by 16) in variable
 * operand 1. */
void field_event_store_own_character(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_current_event_actor->unkE4);
    field_current_event_actor->pc += 3;
}

/* 80099F48: Store the controlled actor's party character (+e4, set by 16) in variable
 * operand 1. */
void field_event_store_controlled_character(void) {
    FieldActor *player;

    player = field_view.components.descriptors[field_work.controlled].actor;
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, player->unkE4);
    field_current_event_actor->pc += 3;
}

/* 80099FC4: Store the current actor's facing octant in variable operand 1. */
void field_event_store_facing_octant(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, (((field_current_event_actor->heading_goal + 0x100) >> 9) + 2) & 7);
    field_current_event_actor->pc += 3;
}

/* 8009A024: Store the translation x, z, y of the descriptor of selector byte 1 in
 * variables operand 2, 4 and 6 (nothing when no actor is selected). */
void field_event_store_actor_position(void) {
    s32 index;

    index = field_event_read_actor_index(1);
    if (index != 0xFF) {
        field_event_write_variable(field_event_read_u16(2) & 0xFFFF, field_view.components.descriptors[index].transform.t[0]);
        field_event_write_variable(field_event_read_u16(4) & 0xFFFF, field_view.components.descriptors[index].transform.t[2]);
        field_event_write_variable(field_event_read_u16(6) & 0xFFFF, field_view.components.descriptors[index].transform.t[1]);
    }
    field_current_event_actor->pc += 8;
}

/* 8009A0FC: Event fe 45: set the current actor's idle animation (+e6, which a walk
 * (80098cac) ends with) from byte 1. */
void field_event_set_idle_animation(void) {
    field_current_event_actor->unkE6 = EVENT_OPERAND_BYTE(1);
    field_current_event_actor->pc += 2;
}

/* 8009A130: Set the current actor's requested animation (+ea; the frame update and walks
 * play it while it is not 0xff) to byte 1 and clear its layer flag 0x1000000.
 */
void field_event_set_animation(void) {
    field_current_event_actor->layer_flags &= ~0x1000000;
    field_current_event_actor->unk0EA = EVENT_OPERAND_BYTE(1);
    field_current_event_actor->pc += 2;
}

/* 8009A174: Set the requested animation (+ea) from byte 1 as 2c and clear layer flag
 * 0x10000, which the sprite's completion callback (80076a74) sets and 5e waits
 * for. */
void field_event_play_animation(void) {
    field_event_set_animation();
    field_current_event_actor->layer_flags &= ~0x10000;
}

/* 8009A1AC: Wait, retried without yielding, until the current actor's sprite completion
 * callback (80076a74) has set layer flag 0x10000, then clear the requested
 * animation (+ea = 0xff) and continue at +1. */
void field_event_wait_animation(void) {
    if (field_current_event_actor->layer_flags & 0x10000) {
        field_current_event_actor->unk0EA = 0xFF;
        field_current_event_actor->pc += 1;
    }
}

/* 8009A1E4: Face the actor of party slot operand 1. */
void field_event_face_party_member(void) {
    s32 index;
    FieldActor *other;
    s16 facing;

    index = mode_party_actors[EVENT_OPERAND_BYTE(1)];
    if (index != 0xFF) {
        other = field_view.components.descriptors[index].actor;
        facing = -ratan2(other->position[2] - field_current_event_actor->position[2],
                                other->position[0] - field_current_event_actor->position[0]) | 0x8000;
        field_current_event_actor->heading = facing;
        field_current_event_actor->heading_goal = facing;
    }
    field_current_event_actor->pc += 2;
}

/* 8009A2A8: Face a selected actor. */
void field_event_face_actor(void) {
    s32 index;
    FieldActor *other;
    s16 facing;

    index = field_event_read_actor_index(1);
    if (index != 0xFF) {
        other = field_view.components.descriptors[index].actor;
        facing = -ratan2(other->position[2] - field_current_event_actor->position[2],
                                other->position[0] - field_current_event_actor->position[0]) | 0x8000;
        field_current_event_actor->heading = facing;
        field_current_event_actor->heading_goal = facing;
    }
    field_current_event_actor->pc += 2;
}

/* 8009A34C: Blend the camera distance toward operand 1 over byte-3 frames (0: one
 * frame, with the camera counter raised by 2). */
void field_event_blend_camera_distance(void) {
    s32 step;

    field_view.steps = EVENT_OPERAND_BYTE(3);
    if (field_view.steps == 0) {
        field_view.steps++;
        field_work.camera_counter += 2;
    }
    step = -((field_view.distance - field_event_read_imm_or_var(1)) << 16) / field_view.steps;
    field_view.start = field_view.distance << 16;
    field_view.flags |= 1;
    field_view.step = step;
    field_current_event_actor->pc += 4;
}

/* 8009A420: Blend the camera elevation toward `target` over `steps` frames. */
void field_camera_blend_elevation(s32 target, s32 steps) {
    if (steps == 0) {
        field_work.camera_counter = 2;
        steps = 1;
    }
    field_view.elevation_steps = steps;
    field_view.elevation_value = (s16)field_view.elevation << 16;
    field_view.flags |= 8;
    field_view.elevation_step = -(((s16)field_view.elevation - target) << 16) / steps;
}

/* 8009A490: Blend the camera elevation (selected operand 1, steps operand 3 & 0x7F). */
void field_event_blend_camera_elevation(void) {
    field_camera_blend_elevation(field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(3)), EVENT_OPERAND_BYTE(3) & 0x7F);
    field_current_event_actor->pc += 4;
}

/* 8009A514: Camera angle octant (0..7). */
s32 field_camera_get_octant(void) {
    return (7 - ((field_view.angle - 0x100) >> 9)) & 7;
}

/* 8009A534: Store the camera heading octant (8009a514) in variable operand 1. */
void field_event_store_camera_octant(void) {
    s32 reference;

    reference = field_event_read_u16(1) & 0xFFFF;
    field_event_write_variable(reference & 0xFFFF, field_camera_get_octant() & 0xFFFF);
    field_current_event_actor->pc += 3;
}

/* 8009A58C: Yield while any camera flag in operand byte 1 is set. */
void field_event_wait_camera_flags(void) {
    if (!(field_view.flags & EVENT_OPERAND_BYTE(1))) {
        field_current_event_actor->pc += 2;
        return;
    }
    field_event_yield_requested = 1;
}

/* 8009A5E0: Yield while any camera flag in operand byte 1 is set (second opcode). */
void field_event_wait_camera_flags_b2(void) {
    if (!(field_view.flags & EVENT_OPERAND_BYTE(1))) {
        field_current_event_actor->pc += 2;
        return;
    }
    field_event_yield_requested = 1;
}

/* 8009A634: Set camera heading mask 0 (800af880 +174) to operand 1: while the camera
 * faces an octant whose bit is set it turns on by one octant; 0xff in
 * either mask stops these and the shoulder-button turns (800726e8). */
void field_event_set_camera_heading_mask0(void) {
    field_view.heading_blocks[0] = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 8009A670: Set camera heading mask 1 (800af880 +175) to operand 1: the camera turns
 * away from octants whose bit is set and shoulder-button turns skip them;
 * 0xff in either mask stops these turns (800726e8). */
void field_event_set_camera_heading_mask1(void) {
    field_view.heading_blocks[1] = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 8009A6AC: Store (sin(angle) * length) >> 12 in variable operand 1: angle selected
 * operand 3 (bit 0x40 of byte 7), length selected operand 5 (bit 0x20). */
void field_event_sine_variable(void) {
    s32 reference;
    s32 angle;
    s32 length;

    reference = field_event_read_u16(1) & 0xFFFF;
    angle = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7));
    length = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7));
    field_event_write_variable(reference & 0xFFFF, (gpu_get_sin(angle) * length) >> 12);
    field_current_event_actor->pc += 8;
}

/* 8009A768: Store (cos(angle) * length) >> 12 in variable operand 1: angle selected
 * operand 3 (bit 0x40 of byte 7), length selected operand 5 (bit 0x20). */
void field_event_cosine_variable(void) {
    s32 reference;
    s32 angle;
    s32 length;

    reference = field_event_read_u16(1) & 0xFFFF;
    angle = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7));
    length = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7));
    field_event_write_variable(reference & 0xFFFF, (gpu_get_cos(angle) * length) >> 12);
    field_current_event_actor->pc += 8;
}

/* 8009A824: Store atan2(selected operand 3, selected operand 5) in variable operand 1
 * (flags byte 7: 0x40, 0x20). */
void field_event_atan_variable(void) {
    s32 reference;
    s32 y;

    reference = field_event_read_u16(1) & 0xFFFF;
    y = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(7));
    field_event_write_variable(reference & 0xFFFF, (s16)ratan2(y, field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(7))));
    field_current_event_actor->pc += 8;
}

/* 8009A8DC: The current actor's facing octant (0..7). */
s32 field_event_get_facing_octant(void) {
    return (((field_current_event_actor->heading_goal + 0x100) >> 9) + 2) & 7;
}

/* 8009A904: Face `angle`; outside field_event_runs_per_frame also sets the rest facing. */
void field_event_face_angle(u16 angle) {
    if (field_event_runs_per_frame == 0) {
        field_current_event_actor->heading = angle | 0x8000;
        field_current_event_actor->heading_goal = angle | 0x8000;
        field_current_event_actor->unk108 = angle | 0x8000;
    }
    field_current_event_actor->heading = angle | 0x8000;
    field_current_event_actor->heading_goal = angle | 0x8000;
    field_current_event_actor->pc += 3;
}

/* 8009A958: A selected actor faces `angle`. */
void field_event_actor_face_angle(u16 angle) {
    FieldActor *actor;

    if (field_event_read_actor_index(1) != 0xFF) {
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        if (field_event_runs_per_frame == 0) {
            actor->heading = angle | 0x8000;
            actor->heading_goal = angle | 0x8000;
            actor->unk108 = angle | 0x8000;
        }
        actor->heading = angle | 0x8000;
        actor->heading_goal = angle | 0x8000;
    }
    field_current_event_actor->pc += 4;
}

/* 8009AA00: Selected actor operand 1 faces selected actor operand 2. */
void field_event_actor_face_actor(void) {
    FieldActor *actor;
    FieldActor *other;
    s16 facing;

    if (field_event_read_actor_index(1) != 0xFF && field_event_read_actor_index(2) != 0xFF) {
        other = field_view.components.descriptors[field_event_read_actor_index(2)].actor;
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        facing = -ratan2(other->position[2] - actor->position[2],
                                other->position[0] - actor->position[0]) | 0x8000;
        if (field_event_runs_per_frame == 0) {
            actor->heading = facing;
            actor->heading_goal = facing;
            actor->unk108 = facing;
        }
        actor->heading = facing;
        actor->heading_goal = facing;
    }
    field_current_event_actor->pc += 3;
}

/* 8009AB08: Face `angle` relative to the camera. */
void field_event_face_view_angle(u16 angle) {
    s16 facing;

    facing = ((angle - field_view.angle) & 0xFFF) | 0x8000;
    field_current_event_actor->heading = facing;
    field_current_event_actor->heading_goal = facing;
    if (field_event_runs_per_frame == 0) {
        field_current_event_actor->unk108 = facing;
    }
    field_current_event_actor->pc += 3;
}

/* 8009AB5C: Turn clockwise by operand-1 octants. */
void field_event_turn_clockwise(void) {
    s32 turn;

    turn = field_event_read_imm_or_var(1);
    field_event_face_angle(field_event_direction_table[(turn + field_event_get_facing_octant()) & 7]);
}

/* 8009ABAC: Turn counter-clockwise by operand-1 octants. */
void field_event_turn_counterclockwise(void) {
    s32 turn;

    turn = field_event_read_imm_or_var(1);
    field_event_face_angle(field_event_direction_table[(field_event_get_facing_octant() - turn) & 7]);
}

void field_event_actor_face_angle(u16 angle);

/* 8009ABFC: A selected actor faces direction table entry operand 2. */
void field_event_actor_face_direction(void) {
    field_event_actor_face_angle(field_event_direction_table[field_event_read_imm_or_var(2)]);
}

/* 8009AC34: A selected actor faces direction entry operand 2, camera-relative. */
void field_event_actor_face_view_direction(void) {
    field_event_actor_face_angle((field_event_direction_table[field_event_read_imm_or_var(2)] - field_view.angle) & 0xFFF);
}

/* 8009AC7C: Face direction table entry operand 1. */
void field_event_face_direction(void) {
    field_event_face_angle(field_event_direction_table[field_event_read_imm_or_var(1)]);
}

void field_event_face_view_angle(u16 angle);

/* 8009ACB4: Face direction table entry operand 1, camera-relative. */
void field_event_face_view_direction(void) {
    field_event_face_view_angle(field_event_direction_table[field_event_read_imm_or_var(1)]);
}

extern s16 field_event_direction_table2[8];

/* 8009ACEC: Face second-table direction operand byte 1, camera-relative. */
void field_event_face_view_direction_table2(void) {
    s16 facing;

    facing = ((field_event_direction_table2[EVENT_OPERAND_BYTE(1)] - field_view.angle) & 0xFFF) | 0x8000;
    field_current_event_actor->heading = facing;
    field_current_event_actor->heading_goal = facing;
    if (field_event_runs_per_frame == 0) {
        field_current_event_actor->unk108 = facing;
    }
    field_current_event_actor->pc += 2;
}

extern s16 field_event_direction_table3[8];

/* 8009AD6C: Face third-table direction operand byte 1. */
void field_event_face_direction_table3(void) {
    s16 facing;

    facing = field_event_direction_table3[EVENT_OPERAND_BYTE(1)] | 0x8000;
    field_current_event_actor->heading = facing;
    field_current_event_actor->heading_goal = facing;
    if (field_event_runs_per_frame == 0) {
        field_current_event_actor->unk108 = facing;
    }
    field_current_event_actor->pc += 2;
}

/* 8009ADDC: Set camera flag 0x4000. */
void field_event_unclamp_camera_eye(void) {
    field_view.flags |= 0x4000;
    field_current_event_actor->pc += 1;
}

/* 8009AE0C: Clear camera flag 0x4000 (and the upper half). */
void field_event_clamp_camera_eye(void) {
    field_view.flags &= 0xBFFF;
    field_current_event_actor->pc += 1;
}

/* 8009AE3C: Blend the camera projection toward `target` over `steps` frames (at
 * once when zero). */
void field_camera_blend_projection(s32 target, s32 steps) {
    if (steps != 0) {
        field_view.flags |= 0x10;
        field_view.projection_steps = steps;
        field_view.projection_value = field_view.projection << 16;
        field_view.projection_step = -((field_view.projection - target) << 16) / steps;
    } else {
        field_view.projection = target;
        field_view.projection_steps = 0;
        field_work.camera_counter += 2;
    }
    field_view.flags &= 0xDFFF;
}

/* 8009AEE0: Walk party slot `slot`'s member one gather step toward (x, z). Returns 0
 * when the member is absent, disabled or has arrived (it is then placed at
 * (x, z) facing `facing`, or its own heading for 0xff) and -1 while it is
 * still walking; a member stuck for 0x40 steps or an override warps. */
s32 field_party_walk_member_toward(s32 slot, s32 x, s32 z, s32 facing) {
    Sprite *model;
    FieldActor *actor;
    FieldActor *saved;
    s32 saved_index;
    s32 member;
    s32 step;
    s32 dx;
    s32 distance;
    s32 dz;
    VECTOR delta;

    member = mode_party_actors[slot];
    if (member == 0xFF) {
        return 0;
    }
    if (field_view.components.descriptors[member].flags & 0x20) {
        return 0;
    }
    model = field_view.components.descriptors[member].model;
    actor = field_view.components.descriptors[member].actor;
    if (model->speed == 0) {
        model->speed = 0x4000000 / (u16)actor->unk76;
    }
    step = field_compute_abs_via_gte(model->speed >> 15) + 1;
    dx = x - WHOLE(actor->position[0]);
    dz = z - WHOLE(actor->position[2]);
    delta.vx = dx;
    delta.vy = 0;
    delta.vz = dz;
    distance = field_compute_planar_length(dx, dz);
    actor->flags |= 0x400000;
    if (step >= distance) {
    arrive:
        if (!(actor->flags & 0x8000)) {
            if (facing == 0xFF) {
                actor->heading_goal = actor->heading = actor->heading_goal | 0x8000;
            } else {
                actor->heading_goal = actor->heading = field_event_direction_table[facing] | 0x8000;
            }
        } else {
            actor->heading_goal = actor->heading = actor->unk11C | 0x8000;
        }
        actor->position[0] = x << 16;
        actor->position[2] = z << 16;
        actor->stuck = 0;
        actor->flags &= 0xFDDFF7FF;
        return 0;
    }
    if (actor->last_position[0] == WHOLE(actor->position[0]) &&
        actor->last_position[1] == WHOLE(actor->position[1]) &&
        actor->last_position[2] == WHOLE(actor->position[2])) {
        actor->stuck++;
    } else {
        actor->stuck = 0;
    }
    actor->heading_goal = actor->heading = field_compute_xz_heading(&delta);
    if ((s16)actor->stuck > 0x40 || (s16)field_work.unk2348 != 0) {
        saved = field_current_event_actor;
        saved_index = field_current_event_actor_index;
        field_current_event_actor = actor;
        field_current_event_actor_index = mode_party_actors[slot];
        field_event_place_actor_on_floor(x, z);
        field_current_event_actor = saved;
        field_current_event_actor_index = saved_index;
        goto arrive;
    }
    return -1;
}

/* 8009B15C: Force the party position. */
void field_event_force_party_position(void) {
    field_work.forced_position = 1;
    field_current_event_actor->pc += 1;
}

/* 8009B184: Event fe 44: release forced positioning and party processing, clear the three
 * movement-history indices and preserve_nonplayer_motion, and record the
 * controlled actor's state in its movement history 32 times (80081c54). */
void field_event_release_party_position(void) {
    s32 i;

    i = 0;
    field_work.forced_position = 0;
    field_work.party_processing_mode = 0;
    field_movement_history_indices[2] = 0;
    field_movement_history_indices[1] = 0;
    field_movement_history_indices[0] = 0;
    field_work.preserve_nonplayer_motion = 0;
    do {
        i++;
        field_record_movement_history(field_work.controlled);
    } while (i < 32);
    field_current_event_actor->pc += 1;
}

s32 field_party_walk_member_toward(s32 member, s32 x, s32 z, s32 range);
void field_party_release_motion_overrides(void);

/* 8009B210: Event fe 24: walk each party member one gather step (8009aee0) toward the
 * leader's position, keeping its heading, and yield. Once all three have
 * arrived, clear party processing mode and 800b2348, release the motion
 * overrides and refill the controlled actor's movement history (8009b338) and
 * advance; else set party processing mode 1 and wait (pc-- to the fe). */
void field_event_wait_party_gathered(void) {
    FieldActor *leader;
    s32 x;
    s32 z;
    s32 near;

    leader = field_view.components.descriptors[mode_party_actors[0]].actor;
    x = WHOLE(leader->position[0]);
    z = WHOLE(leader->position[2]);
    near = field_party_walk_member_toward(0, x, z, 0xFF) == 0;
    if (field_party_walk_member_toward(1, x, z, 0xFF) == 0) {
        near |= 2;
    }
    if (field_party_walk_member_toward(2, x, z, 0xFF) == 0) {
        near |= 4;
    }
    field_event_yield_requested = 1;
    if (near == 7) {
        field_current_event_actor->pc++;
        field_work.party_processing_mode = 0;
        field_work.unk2348 = 0;
        field_party_release_motion_overrides();
    } else {
        field_work.party_processing_mode = 1;
        field_current_event_actor->pc--;
    }
}

/* 8009B338: Release the party motion overrides and resettle the controlled actor. */
void field_party_release_motion_overrides(void) {
    s32 i;

    i = 0;
    field_movement_history_indices[2] = 0;
    field_movement_history_indices[1] = 0;
    field_movement_history_indices[0] = 0;
    field_work.preserve_nonplayer_motion = 0;
    do {
        i++;
        field_record_movement_history(field_work.controlled);
    } while (i < 32);
}

/* 8009B398: Event fe 23: gather the party: walk the members of party slots 0, 1 and 2 one
 * step each (8009aee0) toward (op1, op3), (op5, op7) and (op9, op11) (selected
 * operands, flags 0x80..0x04 of byte 13), each facing direction-table entry
 * op14, op16 or op18 on arrival (0xff: its own heading). Until all three have
 * arrived set party processing mode 1, yield and wait (pc-- to the fe); then
 * clear 800b2348 and the mode, yield and advance 20. Op1 = 0x7fff instead marks
 * every member's heading as turned (bit 15) and advances 20 at once. Both set
 * preserve_nonplayer_motion. */
void field_event_gather_party(void) {
    FieldActor *actor;
    s32 i;
    s32 near;

    if (field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xD)) == 0x7FFF) {
        field_current_event_actor->pc += 0x14;
        field_work.preserve_nonplayer_motion = 1;
        i = 0;
        do {
            if (mode_party_actors[i] != 0xFF) {
                actor = field_view.components.descriptors[mode_party_actors[i]].actor;
                actor->heading_goal = actor->heading = actor->heading_goal | 0x8000;
            }
            i++;
        } while (i < 3);
        return;
    }
    near = field_party_walk_member_toward(0, field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(0xD)), field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(0xD)),
                         field_event_read_imm_or_var(0xE)) == 0;
    if (field_party_walk_member_toward(1, field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(0xD)), field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(0xD)),
                      field_event_read_imm_or_var(0x10)) == 0) {
        near |= 2;
    }
    if (field_party_walk_member_toward(2, field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(0xD)), field_event_read_selected_operand_04(0xB, EVENT_OPERAND_BYTE(0xD)),
                      field_event_read_imm_or_var(0x12)) == 0) {
        near |= 4;
    }
    field_event_yield_requested = 1;
    if (near == 7) {
        field_work.unk2348 = 0;
        field_current_event_actor->pc += 0x14;
        field_work.party_processing_mode = 0;
    } else {
        field_work.party_processing_mode = 1;
        field_current_event_actor->pc -= 1;
    }
    field_work.preserve_nonplayer_motion = 1;
}

/* 8009B664: Event fe 22: store the camera projection in variable operand 1. */
void field_event_store_projection(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, field_view.projection);
    field_current_event_actor->pc += 3;
}

void field_camera_blend_projection(s32 target, s32 steps);

/* 8009B6AC: Blend the camera projection toward operand 1 over operand-3 frames
 * (8009ae3c; at once when zero). */
void field_event_blend_camera_projection(void) {
    s32 target;

    target = field_event_read_imm_or_var(1);
    field_camera_blend_projection(target, field_event_read_imm_or_var(3));
    field_current_event_actor->pc += 5;
}

extern s16 field_camera_octant_turn_table[64]; /* octant turn table [from * 8 + to] */

/* 8009B708: Turn the camera to octant `octant` over `steps` frames. */
void field_camera_turn_to_octant(s32 octant, s32 steps) {
    s32 current;
    s32 velocity;

    current = field_camera_get_octant() & 0xFFFF;
    if (steps == 0) {
        steps = 1;
        field_work.camera_counter += 2;
    }
    velocity = (field_camera_octant_turn_table[current * 8 + octant] << 25) / steps;
    field_view.heading_steps = steps;
    field_view.heading = ((octant + 4) & 7) << 9;
    field_view.heading_velocity = velocity;
}

/* 8009B7A8: Turn the camera one octant (direction 0: positive) over `steps` frames. */
void field_camera_turn_one_octant(s32 direction, s32 steps) {
    s32 velocity;
    s32 heading;

    if (steps == 0) {
        steps = 1;
        field_work.camera_counter += 2;
    }
    if (direction == 0) {
        field_view.heading_velocity = 0x2000000 / steps;
        field_view.heading += 0x200;
    } else {
        field_view.heading_velocity = (s32)0xFE000000 / steps;
        field_view.heading -= 0x200;
    }
    field_view.heading_steps = steps;
}

/* 8009B824: Once the camera heading is idle (yielding until then), turn it one octant
 * in the positive direction over operand-1 frames (8009b7a8). */
void field_event_turn_camera_step(void) {
    if (field_view.heading_steps == 0) {
        field_camera_turn_one_octant(0, field_event_read_imm_or_var(1));
        field_current_event_actor->pc += 3;
    }
    field_event_yield_requested = 1;
}

/* 8009B884: Same as c7 (the original also passes direction 0): once the camera
 * heading is idle, turn it one octant positive over operand-1 frames. */
void field_event_turn_camera_step_c8(void) {
    if (field_view.heading_steps == 0) {
        field_camera_turn_one_octant(0, field_event_read_imm_or_var(1));
        field_current_event_actor->pc += 3;
    }
    field_event_yield_requested = 1;
}

void field_camera_turn_to_octant(s32 octant, s32 steps);

/* 8009B8E4: Turn the camera to octant operand 1 over operand-3 frames (8009b708) once
 * its heading is idle, waiting (yielding) until then; with 800adb1c clear
 * (the immediate runs at load) the heading is set at once. Yields. */
void field_event_turn_camera_octant(void) {
    s32 octant;
    s32 steps;
    s16 heading;

    octant = field_event_read_imm_or_var(1);
    steps = field_event_read_imm_or_var(3);
    if (field_event_runs_per_frame == 0) {
        heading = (octant + 4) & 7;
        field_view.heading_angles.vy = heading << 9;
        field_view.heading = heading << 9;
        field_current_event_actor->pc += 5;
    } else if (field_view.heading_steps == 0) {
        field_camera_turn_to_octant(octant, steps);
        field_current_event_actor->pc += 5;
    }
    field_event_yield_requested = 1;
}

/* 8009B9A0: Once the camera is idle, save its octant, projection and elevation. */
void field_event_save_camera_view(void) {
    if (field_view.heading_steps == 0) {
        field_view.saved_view.octant = field_camera_get_octant();
        field_view.saved_view.projection = field_view.projection;
        field_view.saved_view.elevation = field_view.elevation;
        field_current_event_actor->pc += 1;
    }
}

/* 8009BA0C: Once the camera is idle, blend back to the saved view over 32 frames. */
void field_event_restore_camera_view(void) {
    if (field_view.heading_steps == 0) {
        field_camera_turn_to_octant(field_view.saved_view.octant, 32);
        field_camera_blend_projection(field_view.saved_view.projection, 32);
        field_camera_blend_elevation(field_view.saved_view.elevation, 32);
        field_current_event_actor->pc += 1;
    }
}

/* 8009BA7C: Set the camera at once: heading octant operand 1, elevation operand 3 and
 * projection operand 5 (SetGeomScreen). */
void field_event_set_camera_view(void) {
    s16 heading;
    s16 angle;

    field_view.elevation = field_event_read_imm_or_var(3);
    heading = (field_event_read_imm_or_var(1) + 4) & 7;
    angle = heading << 9;
    field_view.heading = angle;
    field_view.heading_angles.vy = heading << 9;
    field_view.heading_high = angle << 16;
    field_view.projection = field_event_read_imm_or_var(5);
    SetGeomScreen(field_view.projection);
    field_current_event_actor->pc += 7;
}

s32 field_event_find_own_window(s32 *window);
void field_event_end_slot(void);

/* 8009BB0C: Wait for this actor's dialogue window to close. While it has an open one
 * yield without advancing; once that window's speaker has layer flag 0x200
 * and bit 0 of the actor's window style (+84) is clear, close the window
 * and end the current script slot (unless its priority is 7). With none
 * open, store +81 (the line chosen, see a9) in variable 0x14 and
 * continue. */
void field_event_wait_dialogue(void) {
    s32 window;
    u32 value;
    s32 bits;

    if (field_event_find_own_window(&window) == -1) {
        field_event_batch_limit += 8;
        field_event_write_variable(0x14, field_current_event_actor->unk081);
        field_current_event_actor->pc++;
        return;
    }
    if (field_view.components.descriptors[field_dialogue_windows[window].unk418].actor->layer_flags & 0x200) {
        value = field_current_event_actor->unk84;
        if (value >> 16) {
            bits = (value >> 16) & 0xFFFF;
        } else {
            bits = value & 0xFFFF;
        }
        if (!(bits & 1)) {
            if (field_current_event_actor->slots[field_current_event_actor->slot].priority != 7) {
                field_event_end_slot();
            }
            field_dialogue_windows[window].cleared = 0;
        }
    }
    field_event_yield_requested = 1;
}

/* 8009BC98: Event 0xa9: with an open dialogue window of its own, wait (yielding) until
 * its text box is ready (80033cd0 returns 1, or text box +84 and +6c are
 * set), then offer lines byte1 >> 4 .. byte1 & 0xf as a choice (+81 = 0xff
 * until one is taken) and continue; with none, just continue. Yields. */
void field_event_start_choice(void) {
    s32 window;
    u32 first;

    if (field_event_find_own_window(&window) == 0) {
        field_event_batch_limit += 8;
        if (window_get_wait_state(&field_dialogue_windows[window].text) == 1 ||
            ((&field_dialogue_windows[window].text)->unk84 != 0 && (&field_dialogue_windows[window].text)->unk6C != 0)) {
            field_dialogue_windows[window].choice.status = 0;
            field_current_event_actor->unk081 = 0xFF;
            first = EVENT_OPERAND_BYTE(1) >> 4;
            field_dialogue_windows[window].choice.first = first;
            field_dialogue_windows[window].choice.count = (EVENT_OPERAND_BYTE(1) & 0xF) - first + 1;
            field_dialogue_windows[window].choice.index = 0;
            window_set_color(&field_dialogue_windows[window].text, 0xEF, 0x1E, 0xF0);
            field_current_event_actor->pc += 2;
        }
    } else {
        field_current_event_actor->pc += 2;
    }
    field_event_yield_requested = 1;
}

/* 8009BE58: Whether the current actor's octant (state bits 9-11) is within four
 * octants past the camera's. */
s32 field_event_is_octant_within_4_past_camera(void) {
    return ((((s32)(field_current_event_actor->state.word >> 9) & 7) - (field_camera_get_octant() & 0xFFFF)) & 7) < 5;
}

/* 8009BE9C: With byte 1 zero close this actor's open dialogue window (if any);
 * otherwise clear its window overrides (+82, +83, +84, +88, +8a). Advances
 * and yields. */
void field_event_close_window(void) {
    FieldActor *actor;
    s32 window;

    actor = field_current_event_actor;
    if (field_event_bytecode[actor->pc + 1] == 0) {
        if (field_event_find_own_window(&window) == 0) {
            field_dialogue_windows[window].cleared = 0;
            field_current_event_actor->pc += 2;
        } else {
            field_current_event_actor->pc += 2;
        }
    } else {
        actor->unk82 = 0;
        actor->unk88 = 0;
        actor->unk8A = 0;
        field_current_event_actor->unk83 = 0;
        field_current_event_actor->unk84 = 0;
        field_current_event_actor->pc += 2;
    }
    field_event_yield_requested = 1;
}

void field_event_message_actor(void);

/* 8009BF8C: With an actor selected by byte 1, copy its character (+80, the portrait)
 * to the current actor and open the message as d4 does (message operand 2,
 * style byte 4; pc + 5 once open); with none, skip 6 bytes. */
void field_event_message_as_actor(void) {
    if (field_event_read_actor_index(1) != 0xFF) {
        field_current_event_actor->character = field_view.components.descriptors[field_event_read_actor_index(1)].actor->character;
        field_event_message_actor();
        return;
    }
    field_current_event_actor->pc += 6;
}

s32 field_event_open_message_window(s32 index, s32 mode);

/* 8009C01C: Open this actor's dialogue window for message operand 2 by the speaker
 * the actor selector byte 1 picks (8009c5a8 mode 0), byte 4 overriding the
 * style; retried (pc back on this opcode) until it opens (pc + 5). With no
 * such actor (an empty party slot) it skips 6 bytes. */
void field_event_message_actor(void) {
    if (field_event_read_actor_index(1) != 0xFF) {
        s32 index = field_event_read_actor_index(1);

        field_current_event_actor->pc += 1;
        if (field_event_open_message_window(index, 0) == -1) {
            field_current_event_actor->pc -= 1;
        }
    } else {
        field_current_event_actor->pc += 6;
    }
}

/* 8009C0B4: Open this actor's dialogue window for message operand 1 by the speaker
 * (the current actor; 8009c5a8 mode 0), byte 3 overriding the style; an
 * open window of its own is closed first, and it is retried (yielding)
 * until the window opens (pc + 4). */
void field_event_message(void) {
    field_event_open_message_window(field_current_event_actor_index, 0);
}

/* 8009C0DC: Open this actor's dialogue window for message operand 1 in the fixed
 * full-width box (8009c5a8 mode 1), byte 3 overriding the style; an open
 * window of its own is closed first, and it is retried (yielding) until the
 * window opens (pc + 4). */
void field_event_message_fixed(void) {
    field_event_open_message_window(field_current_event_actor_index, 1);
}

/* 8009C104: Open the current actor's dialogue window for message operand 1 in mode 2
 * (8009c5a8: the fixed full-width box; byte 3, when non-zero, overrides the
 * style) and continue at +4; retried, yielding, until the window opens. */
void field_event_message_box(void) {
    field_event_open_message_window(field_current_event_actor_index, 2);
}

/* 8009C12C: Open this actor's dialogue window for message operand 1 centred (8009c5a8
 * mode 3), byte 3 overriding the style; an open window of its own is closed
 * first, and it is retried (yielding) until the window opens (pc + 4). */
void field_event_message_centred(void) {
    field_event_open_message_window(field_current_event_actor_index, 3);
}

#define PLACE(i, k) (((s16 *)field_dialogue_portrait_places)[(i) * 8 + (k)])
#define FILES(c, k) (((u8 *)field_dialogue_portrait_files)[(c) * 2 + (k)])

/* 8009C154: Show the dialogue portrait of `character`: finish a pending slot first
 * (upload loaded images, or release shown ones) and return -1; a slot
 * already holding it is selected (bits 2-4 of the actor state) and 0
 * returned; otherwise the next free slot starts loading its image files and
 * -1 is returned. The placement (800aeae4) and file (800ae1e0) tables are
 * indexed as flat arrays, which keeps their bases in registers as the
 * original does. One `file` variable serves both images: set in two blocks
 * it is allocated globally, and the second file's address, which dies where
 * it is loaded into `file`, takes the argument register `file` prefers. */
s32 field_dialogue_show_portrait(s32 character) {
    s32 i;
    s32 found;
    s32 file;

    for (i = 0; i < 3; i++) {
        if (field_dialogue_portrait_slots[i].b == 1) {
            if (cd_sync_reads(1) == 0) {
                field_dialogue_portrait_slots[i].b = 2;
                field_load_tim_at(field_dialogue_portrait_first_image, PLACE(i, 0), PLACE(i, 1), PLACE(i, 2),
                              PLACE(i, 3), 0x100, 1);
                if (field_dialogue_portrait_slots[i].c == 0) {
                    field_load_tim_at(field_dialogue_portrait_first_image, PLACE(i, 4), PLACE(i, 5), PLACE(i, 6),
                                  PLACE(i, 7), 0x100, 1);
                } else {
                    field_load_tim_at(field_dialogue_portrait_second_image, PLACE(i, 4), PLACE(i, 5), PLACE(i, 6),
                                  PLACE(i, 7), 0x100, 1);
                }
            }
            return -1;
        }
        if (field_dialogue_portrait_slots[i].b == 2) {
            field_dialogue_portrait_slots[i].b = 0;
            heap_free(field_dialogue_portrait_first_image);
            if (field_dialogue_portrait_slots[i].c == 1) {
                heap_free(field_dialogue_portrait_second_image);
            }
            return -1;
        }
    }
    for (i = 0; i < 3; i++) {
        if (field_dialogue_portrait_slots[i].a == character) {
            field_current_event_actor->state.bits.unk2 = i;
            return 0;
        }
    }
    found = 0;
    for (i = 0; i < 3; i++) {
        field_dialogue_portrait_last_slot++;
        if (field_dialogue_portrait_last_slot >= 3) {
            field_dialogue_portrait_last_slot = 0;
        }
        if (field_dialogue_is_portrait_shown(field_dialogue_portrait_slots[field_dialogue_portrait_last_slot].a) == 0) {
            found++;
            break;
        }
    }
    if (found == 0) {
        return -1;
    }
    field_current_event_actor->state.bits.unk2 = field_dialogue_portrait_last_slot;
    cd_select_directory(4, 0);
    field_dialogue_portrait_slots[field_dialogue_portrait_last_slot].a = character;
    field_dialogue_portrait_slots[field_dialogue_portrait_last_slot].b = 1;
    field_dialogue_portrait_slots[field_dialogue_portrait_last_slot].c = 0;
    i = 0;
    file = FILES(character, 0);
    field_dialogue_portrait_file_requests[i].file = file + 0x46;
    field_dialogue_portrait_file_requests[i].destination = field_dialogue_portrait_first_image = heap_alloc(cd_get_aligned_file_size(file + 0x46), 0);
    i++;
    if (FILES(character, 1) != FILES(character, 0)) {
        field_dialogue_portrait_slots[field_dialogue_portrait_last_slot].c = 1;
        file = FILES(character, 1);
        field_dialogue_portrait_file_requests[i].file = file + 0x46;
        field_dialogue_portrait_file_requests[i].destination = field_dialogue_portrait_second_image = heap_alloc(cd_get_aligned_file_size(file + 0x46), 0);
        i++;
    }
    field_dialogue_portrait_file_requests[i].file = 0;
    field_dialogue_portrait_file_requests[i].destination = NULL;
    cd_read_file_list(field_dialogue_portrait_file_requests, 0, 0);
    return -1;
}

/* 8009C538: -1 when an idle window shows message kind 1 for `id`, else 0. */
s32 field_dialogue_is_portrait_shown(s32 id) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (field_dialogue_windows[i].busy == 0 && field_dialogue_windows[i].unk494 == 1 && field_dialogue_windows[i].unk495 == id) {
            return -1;
        }
    }
    return 0;
}

/* The window opener (8007f8dc), as this caller passes the message id. */

/* 8009C5A8: Open this actor's dialogue window for message op1 above/below speaker
 * `speaker` (mode 0 follows the speaker, mode 3 is centred, others use the
 * fixed full-width box); op3 overrides the style byte. Returns -1 while the
 * window cannot open yet (the instruction is retried) and 0 once opened.
 * The "above" placement copies y into its own variable first, which keeps
 * the load ahead of rows * 14 as the original schedules it. */
s32 field_event_open_message_window(s32 speaker, s32 mode) {
    s32 owned;
    s32 x;
    s32 y;
    s32 anchor;
    u16 message;
    s32 window;
    s32 i;
    s32 idle;
    s32 combined;
    s32 columns;
    s32 rows;
    s32 progress;
    s32 low;
    u32 style;
    s32 top;
    s32 left;
    s32 right;
    s32 bottom;
    s32 flags;

    field_event_batch_limit += 0x20;
    if (field_music_stream_running != 0 || field_dialogue_open_blocked != 0 || field_dialogue_pass_open_count != 0 || field_menu_request != 0xFF ||
        (field_movie_requested == 0 && field_music_is_stream_or_disc_busy() != 0)) {
        field_event_yield_requested = 1;
        return -1;
    }
    if (field_current_event_actor->character != 0xFF && field_dialogue_show_portrait(field_current_event_actor->character) == -1) {
        field_event_yield_requested = 1;
        return -1;
    }
    field_dialogue_pass_open_count++;
    if (field_event_find_own_window(&owned) == -1) {
        field_event_batch_limit += 8;
        message = field_event_read_u16(1);
        if (field_dialogue_is_full() != 0) {
            window = field_dialogue_find_oldest_window();
            if (window != 0xFFFF) {
                field_dialogue_windows[window].cleared = 0;
                field_event_yield_requested = 1;
                return -1;
            }
        } else {
            window = field_dialogue_take_free_window();
        }
        for (i = 0, idle = 0, combined = 0; i < 4; i++) {
            if (field_dialogue_windows[i].busy == 0) {
                idle++;
                combined |= (s16)field_dialogue_windows[i].style;
            }
        }
        columns = text_get_message_columns(field_message_table, message);
        rows = text_get_message_rows(field_message_table, message);
        if (mode == 0 || mode == 3) {
            if (field_current_event_actor->unk82 != 0) {
                columns = field_current_event_actor->unk82;
            }
            if (field_current_event_actor->unk83 != 0) {
                rows = field_current_event_actor->unk83;
            }
        }
        progress = field_current_event_actor->unk84;
        low = progress & 0xFFFF;
        field_current_event_actor->unk84 = low;
        style = low;
        if (EVENT_OPERAND_BYTE(3) != 0) {
            style = (progress & 0xFF00) | EVENT_OPERAND_BYTE(3);
            field_current_event_actor->unk84 = low | (style << 16);
        }
        top = 0x10;
        switch ((style >> 4) & 3) {
        case 0:
            /* Automatic: below unless the camera faces the speaker's side
             * and a free window is above. */
            if (((field_current_event_actor->state.bits.octant - (u16)field_camera_get_octant()) & 7) >= 5) {
                if (!(combined & 0x80) && idle == 0) {
                    goto above;
                }
                goto below;
            }
            if (!(combined & 0x80)) {
                goto below;
            }
            /* fall through */
        case 1:
        above:
            field_dialogue_windows[window].style = 1;
            if (mode == 0 || mode == 3) {
                field_descriptor_get_screen_point(speaker, &x, &y, -0x40);
                if (mode == 0) {
                    anchor = y;
                    top = anchor - rows * 14 - 0x24;
                } else {
                    top = 0x14;
                    x = 0xA0;
                }
                if (field_current_event_actor->character != 0xFF && !(style & 2)) {
                    rows = 4;
                    if (columns < 0x18) {
                        columns = 0x18;
                    }
                    columns += 0x11;
                    top = 0x10;
                }
            } else {
                columns = 0x48;
                rows = 4;
                top = 0x10;
                x = 0xA0;
            }
            break;
        case 2:
        below:
            field_dialogue_windows[window].style = 0x81;
            if (mode == 0 || mode == 3) {
                field_descriptor_get_screen_point(speaker, &x, &y, -0x40);
                if (mode == 0) {
                    top = y + 0x30;
                } else {
                    top = 0x94;
                    x = 0xA0;
                }
                if (field_current_event_actor->character != 0xFF && !(style & 2)) {
                    if (columns < 0x18) {
                        columns = 0x18;
                    }
                    columns += 0x11;
                    rows = 4;
                    top = 0x94;
                }
            } else {
                top = 0x94;
                columns = 0x48;
                rows = 4;
                x = 0xA0;
            }
            break;
        }
        left = x - (columns * 2 + 8);
        if (left < 0xC) {
            left = 0xC;
        }
        right = left + 0x10;
        if (right + columns * 4 >= 0x135) {
            left = 0x124 - columns * 4;
        }
        if (top < 0x10) {
            top = 0x10;
        }
        bottom = top + 8;
        if (bottom + rows * 14 >= 0xD5) {
            top = 0xCC - rows * 14;
        }
        if (mode == 0 || mode == 3) {
            if (field_current_event_actor->unk88 != 0) {
                left = field_current_event_actor->unk88;
            }
            if (field_current_event_actor->unk8A != 0) {
                top = field_current_event_actor->unk8A;
            }
            if (field_current_event_actor->unk82 != 0) {
                columns = field_current_event_actor->unk82;
            }
            if (field_current_event_actor->unk83 != 0) {
                rows = field_current_event_actor->unk83;
            }
            if (field_current_event_actor->character != 0xFF && !(style & 2)) {
                rows = 4;
            }
        }
        if (style & 0x40) {
            field_dialogue_windows[window].style |= 0x40;
        }
        flags = 0;
        if (!(style & 0xC)) {
            flags = ((((((s16)field_view.components.descriptors[speaker].actor->heading_goal >> 9) -
                        (u16)field_camera_get_octant()) + 1) & 7) >= 4) << 10;
        } else if (style & 4) {
            flags = 0x400;
        }
        field_dialogue_open_window(left, top, message, window, columns, rows, field_current_event_actor_index, speaker, mode, flags, style);
        field_dialogue_mark_window_open(window);
        field_current_event_actor->heading |= 0x8000;
        field_current_event_actor->pc += 4;
        return 0;
    }
    field_event_yield_requested = 1;
    field_dialogue_windows[owned].cleared = 0;
    return -1;
}

/* 8009CCF8: Set talk-inhibit bit `bit`. */
void field_dialogue_mark_window_open(s32 bit) {
    field_work.open_windows |= 1 << bit;
}

/* 8009CD18: Find the idle window owned by the current actor. */
s32 field_event_find_own_window(s32 *window) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (field_dialogue_windows[i].owner == field_current_event_actor_index && field_dialogue_windows[i].busy == 0) {
            *window = i;
            return 0;
        }
    }
    return -1;
}

/* 8009CD7C: Actor selector at `offset`, defaulting to the party leader. */
s32 field_event_read_actor_index_or_leader(s32 offset) {
    s32 index;

    index = field_event_read_actor_index(offset);
    if (index == 0xFF) {
        return mode_party_actors[0];
    }
    return index;
}

/* 8009CDB4: Actor selector at `offset`: 0xFF/0xFE/0xFD pick party slots, 0xFB the
 * current actor. */
s32 field_event_read_actor_index(s32 offset) {
    s32 index;

    index = field_event_bytecode[field_current_event_actor->pc + offset];
    if (index == 0xFF) {
        index = mode_party_actors[0];
    } else if (index == 0xFE) {
        index = mode_party_actors[1];
    } else if (index == 0xFD) {
        index = mode_party_actors[2];
    } else if (index == 0xFB) {
        index = field_current_event_actor_index;
    }
    return index;
}

/* 8009CE48: Set the current actor's dialogue window overrides (modes 0 and 3 of
 * 8009c5a8; 0 keeps the computed value): left = byte 1 * 2, top = byte 2,
 * columns = byte 3 * 3, rows = byte 4. */
void field_event_set_window_layout(void) {
    field_current_event_actor->unk88 = EVENT_OPERAND_BYTE(1) * 2;
    field_current_event_actor->unk8A = EVENT_OPERAND_BYTE(2);
    field_current_event_actor->unk82 = EVENT_OPERAND_BYTE(3) * 3;
    field_current_event_actor->unk83 = EVENT_OPERAND_BYTE(4);
    field_current_event_actor->pc += 5;
}

/* 8009CEE0: Set the current actor's dialogue window overrides as cf from operands:
 * left = operand 1, top = operand 3, columns = operand 5 * 3, rows =
 * operand 7, and its window style word (+84) = operand 9. */
void field_event_set_window_layout_operands(void) {
    field_current_event_actor->unk88 = field_event_read_imm_or_var(1);
    field_current_event_actor->unk8A = field_event_read_imm_or_var(3);
    field_current_event_actor->unk82 = field_event_read_imm_or_var(5) * 3;
    field_current_event_actor->unk83 = field_event_read_imm_or_var(7);
    field_current_event_actor->unk84 = field_event_read_imm_or_var(9);
    field_current_event_actor->pc += 11;
}

/* 8009CF70: Event d1: empty. It neither advances nor yields, so the interpreter runs
 * it again until the pass's batch limit (or the 0x400 loop error); the
 * script stays on it. */
void field_event_halt(void) {
}

/* 8009CF78: Selected operand, immediate when flags bit 0x80 is set. */
s32 field_event_read_selected_operand_80(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x80) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009CFBC: Selected operand, immediate when flags bit 0x40 is set. */
s32 field_event_read_selected_operand_40(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x40) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009D000: Selected operand, immediate when flags bit 0x20 is set. */
s32 field_event_read_selected_operand_20(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x20) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009D044: Selected operand, immediate when flags bit 0x10 is set. */
s32 field_event_read_selected_operand_10(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x10) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009D088: Selected operand, immediate when flags bit 0x08 is set. */
s32 field_event_read_selected_operand_08(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x08) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009D0CC: Selected operand, immediate when flags bit 0x04 is set. */
s32 field_event_read_selected_operand_04(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x04) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009D110: Selected operand, immediate when flags bit 0x02 is set. */
s32 field_event_read_selected_operand_02(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x02) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009D154: Selected operand, immediate when flags bit 0x01 is set. */
s32 field_event_read_selected_operand_01(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x01) {
        value = (s16)field_event_read_s16(offset);
    } else {
        value = field_event_read_variable(field_event_read_u16(offset) & 0xFFFF);
    }
    return value;
}

/* 8009D198: Store a random number (rand) in variable operand 1. */
void field_event_random_variable(void) {
    s32 reference;

    reference = field_event_read_u16(1) & 0xFFFF;
    field_event_write_variable(reference & 0xFFFF, rand());
    field_current_event_actor->pc += 3;
}

/* 8009D1F0: Store a random number 0..operand 3 ((rand() * (operand 3 + 1)) >> 15) in
 * variable operand 1. */
void field_event_random_below(void) {
    s32 value;

    value = (rand() * (field_event_read_imm_or_var(3) + 1)) >> 15;
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 5;
}

/* 8009D260: Shift variable operand 1 right by operand 3. */
void field_event_shift_right_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value = value >> field_event_read_imm_or_var(3);
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 5;
}

/* 8009D2D0: Shift variable operand 1 left by operand 3. */
void field_event_shift_left_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value = value << field_event_read_imm_or_var(3);
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 5;
}

/* 8009D340: Increment variable operand 1. */
void field_event_increment_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF) + 1;
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 3;
}

/* 8009D3A4: Decrement variable operand 1. */
void field_event_decrement_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF) - 1;
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 3;
}

/* 8009D408: Clear bit (selected operand 3, immediate when byte 5 has bit 0x40) of
 * variable operand 1. */
void field_event_clear_variable_bit(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value &= ~(1 << field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5)));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D4A0: XOR variable operand 1 with selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void field_event_xor_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value ^= field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D52C: OR variable operand 1 with selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void field_event_or_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value |= field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D5B8: AND variable operand 1 with selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void field_event_and_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value &= field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D644: Set bit (selected operand 3, immediate when byte 5 has bit 0x40) of variable
 * operand 1. */
void field_event_set_variable_bit(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value |= 1 << field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D6D8: Multiply a variable by a selected operand. */
void field_event_multiply_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value *= field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D768: Divide a variable by a selected operand (zero treated as one). */
void field_event_divide_variable(void) {
    s32 value;
    s32 divisor;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    divisor = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    if (divisor == 0) {
        divisor = 1;
    }
    value /= divisor;
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D804: Subtract selected operand 3 (immediate when byte 5 has bit 0x40) from
 * variable operand 1. */
void field_event_subtract_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value -= field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D890: Add selected operand 3 (immediate when byte 5 has bit 0x40) to variable
 * operand 1. */
void field_event_add_variable(void) {
    s32 value;

    value = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
    value += field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5));
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, value);
    field_current_event_actor->pc += 6;
}

/* 8009D91C: Set variable operand 1 to zero. */
void field_event_set_variable_zero(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, 0);
    field_current_event_actor->pc += 3;
}

/* 8009D960: Set variable operand 1 to one. */
void field_event_set_variable_one(void) {
    field_event_write_variable(field_event_read_u16(1) & 0xFFFF, 1);
    field_current_event_actor->pc += 3;
}

/* 8009D9A4: Set variable operand 1 to selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void field_event_set_variable(void) {
    s32 reference;

    reference = field_event_read_u16(1) & 0xFFFF;
    field_event_write_variable(reference & 0xFFFF, field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5)));
    field_current_event_actor->pc += 6;
}

/* 8009DA1C: Set the current actor's flag 0x20000, which keeps the player's talk and touch
 * triggers (8008399c) from starting its events 2 and 3. */
void field_event_block_talk_touch(void) {
    FieldActor *actor = field_current_event_actor;

    actor->flags |= 0x20000;
    actor->pc++;
}

/* 8009DA44: Clear the current actor's flag 0x20000 (see 2a): talk and touch can start its
 * events again. */
void field_event_unblock_talk_touch(void) {
    FieldActor *actor = field_current_event_actor;

    actor->flags &= ~0x20000;
    actor->pc++;
}

/* 8009DA70: Set the current actor's flag 0x800000, which keeps the player's touch
 * trigger (8008399c) from starting its event 3. */
void field_event_block_touch(void) {
    FieldActor *actor = field_current_event_actor;

    actor->flags |= 0x800000;
    actor->pc++;
}

/* 8009DA98: Clear the current actor's flag 0x800000 (see cd). */
void field_event_unblock_touch(void) {
    FieldActor *actor = field_current_event_actor;

    actor->flags &= ~0x800000;
    actor->pc++;
}

/* 8009DAC4: Hide the actor of selector byte 1 (flag 1), remove it from the event schedule
 * (layer flag 0x100000), set its descriptor flag 0x20 and release the current
 * actor's idle dialogue window. */
void field_event_remove_actor(void) {
    FieldActor *actor;
    FieldDescriptor *descriptor;
    s32 window;

    if (field_event_read_actor_index(1) != 0xFF) {
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        actor->flags |= 1;
        actor->layer_flags |= 0x100000;
        descriptor = &field_view.components.descriptors[field_event_read_actor_index(1)];
        descriptor->flags |= 0x20;
        if (field_event_find_own_window(&window) == 0) {
            field_dialogue_windows[window].cleared = 0;
        }
    }
    field_current_event_actor->pc += 2;
}

/* 8009DBC8: Show the actor of selector byte 1 again: clear its flag 1 (see 27). */
void field_event_show_actor(void) {
    FieldActor *actor;

    if (field_event_read_actor_index(1) != 0xFF) {
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        actor->flags &= ~1;
    }
    field_current_event_actor->pc += 2;
}

/* 8009DC4C: Stop the actor of selector byte 1 (clear its +30/+40 motion, mark its heading
 * turned) and hide it (flag 1: not drawn, its scripts not run), then release
 * the current actor's idle dialogue window. */
void field_event_stop_hide_actor(void) {
    FieldActor *actor;
    u16 state;
    s32 window;

    if (field_event_read_actor_index(1) != 0xFF) {
        actor = field_view.components.descriptors[field_event_read_actor_index(1)].actor;
        actor->unk030[0] = 0;
        actor->unk030[1] = 0;
        actor->unk030[2] = 0;
        actor->unk40[0] = 0;
        actor->unk40[1] = 0;
        actor->unk40[2] = 0;
        state = actor->heading | 0x8000;
        actor->flags |= 1;
        actor->heading_goal = state;
        actor->heading = state;
        if (field_event_find_own_window(&window) == 0) {
            field_dialogue_windows[window].cleared = 0;
        }
    }
    field_current_event_actor->pc += 2;
}

/* 8009DD34: Wait operand-1 frames (counted in the slot), yielding each frame. */
void field_event_wait_countdown(void) {
    if (field_current_event_actor->slots[field_current_event_actor->slot].countdown == 0) {
        field_current_event_actor->slots[field_current_event_actor->slot].countdown = field_event_read_imm_or_var(1);
    } else {
        field_current_event_actor->slots[field_current_event_actor->slot].countdown--;
    }
    if (field_current_event_actor->slots[field_current_event_actor->slot].countdown == 0) {
        field_current_event_actor->pc += 3;
    }
    field_event_yield_requested = 1;
}

/* 8009DDEC: Clear the descriptor flag 0x20 and layer flag 0x2000000 of the actor of
 * selector byte 1, unless it is removed from the schedule (layer flag
 * 0x100000). */
void field_event_enable_actor(void) {
    FieldDescriptor *descriptor;

    if (field_event_read_actor_index(1) != 0xFF) {
        descriptor = &field_view.components.descriptors[field_event_read_actor_index(1)];
        if (!(descriptor->actor->layer_flags & 0x100000)) {
            descriptor->flags &= 0xFFDF;
            descriptor->actor->layer_flags &= ~0x2000000;
        }
    }
    field_current_event_actor->pc += 2;
}

/* 8009DE94: Set the descriptor flag 0x20 of the actor of selector byte 1 (disabling it).
 */
void field_event_disable_actor(void) {
    FieldDescriptor *descriptor;

    if (field_event_read_actor_index(1) != 0xFF) {
        descriptor = &field_view.components.descriptors[field_event_read_actor_index(1)];
        descriptor->flags |= 0x20;
    }
    field_current_event_actor->pc += 2;
}

/* 8009DF10: Clear the current descriptor's flag 0x20 (re-enabling it), reset the actor's
 * current animation (+e8 = 0xff) and clear its layer flag 0x2000000. */
void field_event_enable_self(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    descriptor->flags &= 0xFFDF;
    actor = field_current_event_actor;
    actor->unkE8 = 0xFF;
    actor->layer_flags &= ~0x2000000;
    actor->pc++;
}

/* 8009DF78: Set layer flags 0x2000000 and 0x800 on a selected actor. */
void field_event_hide_actor_sprite_and_shadow(void) {
    FieldDescriptor *descriptor;

    if (field_event_read_actor_index(1) != 0xFF) {
        descriptor = &field_view.components.descriptors[field_event_read_actor_index(1)];
        descriptor->actor->layer_flags |= 0x2000000;
        descriptor->actor->layer_flags |= 0x800;
    }
    field_current_event_actor->pc += 2;
}

/* 8009E014: Set layer flags 0x2000000 and 0x800 on the current actor. */
void field_event_hide_sprite_and_shadow(void) {
    FieldActor *actor = field_current_event_actor;

    actor->layer_flags |= 0x2000800;
    actor->pc++;
}

/* 8009E040: Disable the current descriptor (flag 0x20). */
void field_event_disable_self(void) {
    FieldDescriptor *descriptor;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    descriptor->flags |= 0x20;
    field_current_event_actor->pc += 1;
}

/* 8009E094: Set the current actor's motion divisor (+76: walks and moves advance
 * 0x4000000 / it per step, 16.16) from operand 1 and pass it to its sprite as
 * the gravity divisor (80021bcc). */
void field_event_set_motion_divisor(void) {
    s16 value;

    value = field_event_read_imm_or_var(1);
    field_current_event_actor->unk76 = value;
    sprite_set_gravity_divisor(field_view.components.descriptors[field_current_event_actor_index].model, value);
    field_current_event_actor->pc += 3;
}

/* 8009E10C: Map operand bits onto actor flags (1->0x80, 4->0x20, 8->0x10, 0x10->8,
 * 0x20->4, 0x40->0x8000000). */
void field_event_set_actor_flags(void) {
    s32 bits;
    s32 flags;

    bits = field_event_read_imm_or_var(1);
    flags = (bits & 1) << 7;
    if (bits & 4) {
        flags |= 0x20;
    }
    if (bits & 8) {
        flags |= 0x10;
    }
    if (bits & 0x10) {
        flags |= 8;
    }
    if (bits & 0x20) {
        flags |= 4;
    }
    if (bits & 0x40) {
        flags |= 0x8000000;
    }
    field_current_event_actor->flags = (field_current_event_actor->flags & 0xF7FFFF43) | flags;
    field_current_event_actor->pc += 3;
}

/* 8009E1A0: Set the actor layer bits 0-2 from operand bits 0-2 and 3-5 from operand bits 4-6. */
void field_event_set_layer_mask(void) {
    FieldActor *actor = field_current_event_actor;
    u8 *code = field_event_bytecode;
    s32 bits;

    bits = code[actor->pc + 1] & 7;
    actor->layer_flags = (actor->layer_flags & ~7) | bits;
    bits = (code[actor->pc + 1] >> 1) & 0x38;
    actor->layer_flags = (actor->layer_flags & ~0x38) | bits;
    actor->pc += 2;
}

/* 8009E208: Enter mode 0x400000 (clearing 0x40000) from the current height. */
void field_event_start_fall(void) {
    FieldActor *actor = field_current_event_actor;
    s32 y;

    y = WHOLE(actor->position[1]);
    actor->unkEC = 0;
    actor->flags = (actor->flags & ~0x40000) | 0x400000;
    actor->pc++;
    actor->unk72 = y;
}

/* Defined below without a prototype (an s16 `y`), so callers pass a word. */
void field_event_set_actor_y(s32 y);

/* 8009E248: Place the current actor at x/z signed operands 1/3 on its layer's floor
 * (8009e574, which clears its motion), then set its height to signed operand 5
 * (8009e810) and its flag 0x40000. */
void field_event_place_at_height(void) {
    s32 a;

    a = (s16)field_event_read_s16(1);
    field_event_place_actor_on_floor(a, (s16)field_event_read_s16(3));
    field_event_set_actor_y((s16)field_event_read_s16(5));
    field_current_event_actor->flags |= 0x40000;
    field_current_event_actor->pc += 7;
}

/* 8009E2C8: Set the current actor's height to selected operand 1 (flags byte 3, bit 0x80;
 * 8009e810 also sets +ec and +72) and set its flag 0x40000. */
void field_event_set_height(void) {
    field_event_set_actor_y(field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(3)));
    field_current_event_actor->flags |= 0x40000;
    field_current_event_actor->pc += 4;
}

/* 8009E330: Signed halfword of the bytecode at `offset`. */
s16 field_event_read_code_half(s32 offset) {
    return field_event_bytecode[offset] + (field_event_bytecode[offset + 1] << 8);
}

/* 8009E35C: Place the current actor on collision layer byte 5 at selected x/z operands
 * 1/3 (flags byte 6) on that layer's floor (8009e574, which clears its motion)
 * and clear its layer flag 0x200000 and flag 0x10000. */
void field_event_place_on_layer(void) {
    s32 x;

    field_current_event_actor->layer = EVENT_OPERAND_BYTE(5);
    x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(6));
    field_event_place_actor_on_floor(x, field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(6)));
    field_current_event_actor->layer_flags &= ~0x200000;
    field_current_event_actor->flags &= ~0x10000;
    field_current_event_actor->pc += 7;
}

/* 8009E428: Move the current actor to layer operand 1 at its own x/z. */
void field_event_set_layer(void) {
    FieldActor *actor;

    field_current_event_actor->layer = EVENT_OPERAND_BYTE(1);
    actor = field_view.components.descriptors[field_current_event_actor_index].actor;
    field_event_place_actor_on_floor(WHOLE(actor->position[0]), WHOLE(actor->position[2]));
    field_current_event_actor->pc += 2;
}

/* 8009E4BC: Place the current actor at selected x/z operands 1/3 (flags byte 5) on the
 * floor of its collision layer (8009e574, which clears its motion) and clear
 * its layer flag 0x200000 and flag 0x10000. */
void field_event_place(void) {
    s32 x;

    x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(5));
    field_event_place_actor_on_floor(x, field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(5)));
    field_current_event_actor->layer_flags &= ~0x200000;
    field_current_event_actor->flags &= ~0x10000;
    field_current_event_actor->pc += 6;
}

/* 8009E574: Place the current actor at integer (x, z) on the floor of its layer:
 * locate the floor triangle of every layer, take its terrain, normal and
 * height, move the descriptor and model there and clear the motion. */
void field_event_place_actor_on_floor(s32 x, s32 z) {
    VECTOR normals[4];
    SVECTOR points[4];
    Sprite *model;
    s32 layer;

    model = field_view.components.descriptors[field_current_event_actor_index].model;
    for (layer = 0; layer < field_view.components.layer_count - 1; layer++) {
        field_current_event_actor->triangle[layer] = field_collision_find_floor_triangle(x, z, layer, &points[layer], &normals[layer]);
    }
    field_current_event_actor->unk014 = field_actor_get_floor_attribute(field_current_event_actor);
    field_current_event_actor->unk50[0] = (normals + field_current_event_actor->layer)->vx;
    field_current_event_actor->unk50[1] = (normals + field_current_event_actor->layer)->vy;
    field_current_event_actor->unk50[2] = (normals + field_current_event_actor->layer)->vz;
    field_view.components.descriptors[field_current_event_actor_index].transform.t[0] = field_view.components.descriptors[field_current_event_actor_index].matrix.t[0] = x;
    field_view.components.descriptors[field_current_event_actor_index].transform.t[1] = field_view.components.descriptors[field_current_event_actor_index].matrix.t[1] = points[field_current_event_actor->layer].vy;
    field_view.components.descriptors[field_current_event_actor_index].transform.t[2] = field_view.components.descriptors[field_current_event_actor_index].matrix.t[2] = z;
    model->ground = points[field_current_event_actor->layer].vy;
    field_current_event_actor->position[0] = x << 16;
    field_current_event_actor->position[1] = points[field_current_event_actor->layer].vy << 16;
    field_current_event_actor->position[2] = z << 16;
    field_current_event_actor->unk72 = points[field_current_event_actor->layer].vy;
    model->x = field_current_event_actor->position[0];
    model->y = field_current_event_actor->position[1];
    model->z = field_current_event_actor->position[2];
    field_current_event_actor->unk40[0] = 0;
    field_current_event_actor->unk40[1] = 0;
    field_current_event_actor->unk40[2] = 0;
    field_current_event_actor->unk030[0] = 0;
    field_current_event_actor->unk030[1] = 0;
    field_current_event_actor->unk030[2] = 0;
    field_current_event_actor->target[0] = 0;
    field_current_event_actor->target[1] = 0;
    field_current_event_actor->target[2] = 0;
    field_current_event_actor->unk62 = 0;
    field_current_event_actor->unk60 = 0;
    field_current_event_actor->unk64 = 0;
    model->speed_x = 0;
    model->speed_y = 0;
    model->speed_z = 0;
    field_current_event_actor->unkF0 = 0;
    field_current_event_actor->unkEC = 0;
    field_current_event_actor->unk72 = field_current_event_actor->position[1] >> 16;
    field_current_event_actor->flags = (field_current_event_actor->flags & ~0x40000) | 0x400000;
}

/* 8009E810: Set the current actor's height `y` (whole units). Defined K&R: callers
 * pass the operand unconverted and only its low halfword is used. */
void field_event_set_actor_y(y)
    s16 y;
{
    SVECTOR unused[3]; /* unused in the original; reserves 0x18 bytes */

    field_current_event_actor->position[1] = y << 16;
    field_current_event_actor->unkEC = y;
    field_current_event_actor->unk72 = y;
}

/* 8009E83C: Set the current actor's dimensions from bytes 1-4, each doubled and only when
 * non-zero: +18, +1c, +1a (height) and +1e (the radius the talk, touch and move
 * reach checks add). */
void field_event_set_extents(void) {
    if (EVENT_OPERAND_BYTE(1) != 0) {
        field_current_event_actor->unk18 = EVENT_OPERAND_BYTE(1) * 2;
    }
    if (EVENT_OPERAND_BYTE(2) != 0) {
        field_current_event_actor->gravity.part.fraction = EVENT_OPERAND_BYTE(2) * 2;
    }
    if (EVENT_OPERAND_BYTE(3) != 0) {
        field_current_event_actor->height = EVENT_OPERAND_BYTE(3) * 2;
    }
    if (EVENT_OPERAND_BYTE(4) != 0) {
        field_current_event_actor->gravity.part.whole = EVENT_OPERAND_BYTE(4) * 2;
    }
    field_current_event_actor->pc += 5;
}

/* 8009E91C: Give the current actor a boundary quadrilateral (+114, allocated once and
 * marked by state bit 12): corner x/z pairs from selected operands 1-15 (flags
 * byte 17, bits 0x80 down to 0x01). */
void field_event_set_boundary(void) {
    if (!(field_current_event_actor->state.word & 0x1000)) {
        field_current_event_actor->unk114 = heap_alloc(0x10, 0);
    }
    field_current_event_actor->state.word |= 0x1000;
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[0].x = field_event_read_selected_operand_80(1, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[0].z = field_event_read_selected_operand_40(3, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[1].x = field_event_read_selected_operand_20(5, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[1].z = field_event_read_selected_operand_10(7, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[2].x = field_event_read_selected_operand_08(9, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[2].z = field_event_read_selected_operand_04(11, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[3].x = field_event_read_selected_operand_02(13, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)field_current_event_actor->unk114)->corners[3].z = field_event_read_selected_operand_01(15, EVENT_OPERAND_BYTE(17));
    field_current_event_actor->pc += 18;
}

/* 8009EB48: -1 when one of the actor's slots carries event tag `tag`, else 0. */
s32 field_event_has_slot_tag(FieldActor *actor, s32 tag) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (tag == actor->slots[i].tag) {
            return -1;
        }
    }
    return 0;
}

/* 8009EB78: Request event (low five bits of byte 2) at priority (its high three bits) on
 * the actor of selector byte 1, in its first free script slot (priority 15, not
 * linked), and continue at +3. Retried without yielding while no slot is free;
 * skipped when one of its slots already carries that event or no actor is
 * selected. A target removed from the schedule (layer flag 0x100000) instead
 * drops the request link (this slot's bits 16-17, the target's slot +cf bit
 * 22). */
void field_event_request_event(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (field_event_read_actor_index(1) != 0xFF) {
        index = field_event_read_actor_index(1);
        other = field_view.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 0;
            other->slots[field_current_event_actor->unk0CF].unk22 = 0;
        } else if (field_event_has_slot_tag(other, EVENT_OPERAND_BYTE(2) & 0x1F) != -1) {
            for (i = 0; i < 8; i++) {
                if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                    other->slots[i].resume_pc = field_event_get_entry_pc(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                    other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                    other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                    goto done;
                }
            }
            return;
        }
    }
done:
    field_current_event_actor->pc += 3;
}

/* 8009ED68: Request event byte 2 on the actor of selector byte 1 as 07 and wait until it
 * has started: phase 0 (this slot's bits 16-17) requests it, linking the target
 * slot through +cf (retried without yielding while no slot is free), and phase
 * 1 yields until the target runs that slot or it has ended, then continues at
 * +3. Skipped when the event is already requested or no actor is selected; a
 * target removed from the schedule (layer flag 0x100000) drops the link and
 * continues. */
void field_event_request_event_started(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (field_event_read_actor_index(1) != 0xFF) {
        index = field_event_read_actor_index(1);
        other = field_view.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 0;
            other->slots[field_current_event_actor->unk0CF].unk22 = 0;
        } else {
            switch (field_current_event_actor->slots[field_current_event_actor->slot].unk16) {
            case 0:
                if (field_event_has_slot_tag(other, EVENT_OPERAND_BYTE(2) & 0x1F) == -1) {
                    break;
                }
                for (i = 0; i < 8; i++) {
                    if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                        other->slots[i].resume_pc = field_event_get_entry_pc(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                        other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                        other->slots[field_current_event_actor->unk0CF].unk22 = 1;
                        other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                        field_current_event_actor->unk0CF = i;
                        field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 1;
                        return;
                    }
                }
                return;
            case 1:
                if (other->slot == field_current_event_actor->unk0CF || other->slots[field_current_event_actor->unk0CF].priority == 0xF) {
                    field_current_event_actor->pc += 3;
                    field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 0;
                    other->slots[field_current_event_actor->unk0CF].unk22 = 0;
                    return;
                }
                field_event_yield_requested = 1;
                return;
            default:
                return;
            }
        }
    }
    field_current_event_actor->pc += 3;
}

/* 8009F0A0: Request event byte 2 on the actor of selector byte 1 as 08 and wait until it
 * has finished: phase 1 yields until it has started, phase 2 until the linked
 * slot has ended (priority 15), then continues at +3. Skipped as 08 (event
 * already requested, no actor, or a target removed from the schedule, which
 * drops the link). */
void field_event_request_event_finished(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (field_event_read_actor_index(1) != 0xFF) {
        index = field_event_read_actor_index(1);
        other = field_view.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 0;
            other->slots[field_current_event_actor->unk0CF].unk22 = 0;
        } else {
            switch (field_current_event_actor->slots[field_current_event_actor->slot].unk16) {
            case 0:
                if (field_event_has_slot_tag(other, EVENT_OPERAND_BYTE(2) & 0x1F) == -1) {
                    break;
                }
                for (i = 0; i < 8; i++) {
                    if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                        other->slots[i].resume_pc = field_event_get_entry_pc(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                        other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                        other->slots[field_current_event_actor->unk0CF].unk22 = 1;
                        field_current_event_actor->unk0CF = i;
                        field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 1;
                        other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                        return;
                    }
                }
                return;
            case 1:
                if (other->slot == field_current_event_actor->unk0CF || other->slots[field_current_event_actor->unk0CF].priority == 0xF) {
                    field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 2;
                    return;
                }
                field_event_yield_requested = 1;
                return;
            case 2:
                if (other->slots[field_current_event_actor->unk0CF].priority == 0xF) {
                    field_current_event_actor->slots[field_current_event_actor->slot].unk16 = 0;
                    other->slots[field_current_event_actor->unk0CF].unk22 = 0;
                    field_current_event_actor->pc += 3;
                    return;
                }
                field_event_yield_requested = 1;
                return;
            default:
                return;
            }
        }
    }
    field_current_event_actor->pc += 3;
}

/* 8009F424: Event fe 01: wander for one frame: every 16th run (counted in +102) point the
 * heading an octant (0x200) left or right of the facing goal at random,
 * otherwise along the goal; yields and advances. */
void field_event_wander(void) {
    s32 facing;

    facing = field_current_event_actor->heading_goal;
    if ((++field_current_event_actor->unk102 & 0xF) == 0) {
        if (!(rand() & 1)) {
            facing = (field_current_event_actor->heading_goal + 0x200) & 0xFFF;
        } else {
            facing = (field_current_event_actor->heading_goal - 0x200) & 0xFFF;
        }
    }
    field_event_yield_requested = 1;
    field_current_event_actor->heading = facing;
    field_current_event_actor->pc += 1;
}

/* 8009F4CC: Wander with pauses: every 16th pass (+102 counts) either hold the facing
 * (rand & 0x30: mark the heading goal turned) or turn one octant either way at
 * random; set the heading, yield and continue at +1. */
void field_event_wander_pause(void) {
    s32 facing;
    s32 random;

    facing = field_current_event_actor->heading_goal;
    if ((++field_current_event_actor->unk102 & 0xF) == 0) {
        random = rand();
        if (random & 0x30) {
            facing = field_current_event_actor->heading_goal |= 0x8000;
        } else if (!(random & 1)) {
            facing = (field_current_event_actor->heading_goal + 0x200) & 0xFFF;
        } else {
            facing = (field_current_event_actor->heading_goal - 0x200) & 0xFFF;
        }
    }
    field_event_yield_requested = 1;
    field_current_event_actor->heading = facing;
    field_current_event_actor->pc += 1;
}

void field_event_request_player_control(void);

/* 8009F5A8: Run player control for this frame and repeat this opcode. */
void field_event_loop_player_control(void) {
    u16 pc;

    pc = field_current_event_actor->pc;
    field_event_request_player_control();
    field_event_yield_requested = 1;
    field_current_event_actor->pc = pc;
}

/* 8009F5F4: Event a7: player control. With dialogue closed and encounters allowed,
 * poll the pad, count frames stuck against terrain, start a jump (0x800)
 * on the jump button or after 32 stuck frames, and face the d-pad
 * direction relative to the camera (0x8000 when none). Non-player actors
 * are marked 0x1000000 instead. */
void field_event_request_player_control(void) {
    u8 unused[0x48]; /* the original frame holds 0x48 unused bytes */
    s32 i;
    s32 idle;
    s32 direction = 0;

    if (field_current_event_actor->flags & 0x4000) {
        for (i = 0; i < 4; i++) {
            if (field_dialogue_windows[i].choice.status == 0) {
                break;
            }
        }
        idle = (i == 4) ? -1 : 0;
        if (idle == -1 && field_work.encounter_inhibition == 0) {
            if (field_pad_port0_held >> 12) {
                field_encounter_count_down();
            }
            field_player_control_polled = 1;
            if (field_current_event_actor->unk014 & 0x400000) {
                if (ACTOR_CACHED_POSITION(field_current_event_actor)[0] == WHOLE(field_current_event_actor->position[0])
                    && ACTOR_CACHED_POSITION(field_current_event_actor)[1] == WHOLE(field_current_event_actor->position[1])
                    && ACTOR_CACHED_POSITION(field_current_event_actor)[2] == WHOLE(field_current_event_actor->position[2])) {
                    field_player_stuck_frames++;
                }
            } else {
                field_player_stuck_frames = 0;
            }
            if (field_player_stuck_frames > 32 && (field_player_stuck_frames = 32, field_pad_port0_held & 0x80) && !(field_current_event_actor->flags & 0x1800) && field_menu_request == 0xFF) {
                goto jump;
            }
            if (field_work.jump_mode == 0) {
                if ((field_pad_port0_pressed & 0x80) && !(field_current_event_actor->flags & 0x1800) && !(field_current_event_actor->unk014 & 0x400000) && field_menu_request == 0xFF) {
                jump:
                    if (field_actor_is_jump_blocked_by_floor(field_current_event_actor) == 0) {
                        field_current_event_actor->flags |= 0x800;
                        field_unread_jump_start_history_index = field_movement_history_indices[0];
                    }
                }
            } else {
                if (field_pad_port0_pressed & 0x80) {
                    if (field_work.repeat_remaining != 0) {
                        goto count;
                    }
                    if (field_menu_request == 0xFF && field_actor_is_jump_blocked_by_floor(field_current_event_actor) == 0) {
                        field_current_event_actor->flags |= 0x800;
                        field_unread_jump_start_history_index = field_movement_history_indices[0];
                        field_current_event_actor->unkE8 = 0xFF;
                        field_work.repeat_remaining = field_work.repeat_delay;
                    }
                }
                if (field_work.repeat_remaining != 0) {
                count:
                    field_work.repeat_remaining--;
                }
            }
            if (field_work.unk2354 == 0) {
                direction = field_dpad_headings[(field_pad_port0_held >> 12) ^ 0xF];
            } else {
                direction = field_dpad_alt_headings[(field_pad_port0_held >> 12) ^ 0xF];
            }
            if (!(direction & 0x8000)) {
                direction = (direction - field_view.angle) & 0xFFF;
            }
            field_current_event_actor->heading = direction;
        } else {
            field_current_event_actor->heading = direction | 0x8000;
        }
    } else if (field_work.preserve_nonplayer_motion == 0) {
        field_current_event_actor->flags |= 0x1000000;
    }
    field_current_event_actor->pc += 1;
}

/* 8009FA00: Party slot of character `id`, or -1. */
s32 field_party_find_character_slot(s32 id) {
    s32 i;

    if (id == 0xFF) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (mode_party_members[i] == 0xFF) {
            return -1;
        }
        if (mode_party_members[i] == id) {
            return i;
        }
    }
    return -1;
}

s16 field_event_read_code_half(s32 offset);

/* 8009FA54: Place the current actor at entry point `entry` of the bytecode's entry
 * table (when present): layer, x/z, camera octant and facing (0xFF: from
 * variables 8 and 6). One angle variable holds the camera heading and then
 * the facing; because the facing sets bit 15, the heading's sign extension
 * for the 32-bit copy stays in the code. */
s32 field_event_place_at_map_entry(s32 entry) {
    s32 marker;
    s32 record;
    s32 x;
    s32 angle;

    marker = field_event_bytecode[0];
    if (marker != 0xFF) {
        return 0;
    }
    record = entry * 7;
    field_current_event_actor->layer = field_event_bytecode[record + 5];
    x = field_event_read_code_half(record + 1);
    field_event_place_actor_on_floor(x, field_event_read_code_half(record + 3));
    angle = ((field_event_bytecode[record + 6] + 4) & 7) << 9;
    if (field_event_bytecode[record + 6] == marker) {
        angle = ((field_event_read_variable(8) + 4) & 7) << 9;
    }
    field_view.heading_angles.vy = angle;
    field_view.heading = (s16)angle;
    field_view.heading_high = angle << 16;
    angle = (((field_event_bytecode[record + 7] - 2) & 7) << 9) | 0x8000;
    if (field_event_bytecode[record + 7] == marker) {
        angle = (((field_event_read_variable(6) - 2) & 7) << 9) | 0x8000;
    }
    field_current_event_actor->unk108 = field_current_event_actor->heading_goal = field_current_event_actor->heading = angle;
    return 0;
}

/* 8009FB98: Event fe 1e: switch the party files to gears: set 0xc000 in the field id
 * (8004f34c), wait for the disc and its pending read (8001ad1c), make the party
 * files match (8001b044: the gear files with 0xc000 set) and unpack them into
 * the party sprite blocks (8001b3a8); record byte 1 as the alternate sprite set
 * (800b2268) that opcode 16 uses. */
void field_event_switch_party_to_gears(void) {
    mode_field_map_id |= 0xC000;
    mode_wait_for_disc_idle();
    mode_sync_party_files();
    mode_unpack_party_files();
    field_work.unk2268 = EVENT_OPERAND_BYTE(1);
    field_current_event_actor->pc += 2;
}

/* 8009FC10: Party slot whose field actor is `index`, or 0xFF. */
s32 field_party_find_stand_in_slot(s32 index) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (mode_party_stand_in_actors[i] == index) {
            return i;
        }
    }
    return 0xFF;
}

void field_party_record_slot_map(s32 slot);

/* 8009FC48: Event fe 41: set the stand-in flag (+22b1) of party slot operand 1 (clamped
 * to 2), then record the field id in the slot's variable triple (2a/30/36) and
 * clear its other two variables (8009fd10). */
void field_event_set_in_gear(void) {
    s32 slot;

    slot = field_event_read_imm_or_var(1);
    if (slot >= 3) {
        slot = 2;
    }
    game_current_data->inGear[slot] = 1;
    field_party_record_slot_map(slot);
    field_current_event_actor->pc += 3;
}

/* 8009FCAC: Event fe 42: clear the stand-in flag (+22b1) of party slot operand 1 (clamped
 * to 2), then record the field id in the slot's variable triple (2a/30/36) and
 * clear its other two variables (8009fd10). */
void field_event_clear_in_gear(void) {
    s32 slot;

    slot = field_event_read_imm_or_var(1);
    if (slot >= 3) {
        slot = 2;
    }
    game_current_data->inGear[slot] = 0;
    field_party_record_slot_map(slot);
    field_current_event_actor->pc += 3;
}

/* 8009FD10: Record the field id in the slot's variable triple and clear the rest. */
void field_party_record_slot_map(s32 slot) {
    switch (slot) {
    case 0:
        field_event_write_variable(0x2A, mode_field_map_id & 0xFFF);
        field_event_write_variable(0x2C, 0);
        field_event_write_variable(0x2E, 0);
        break;
    case 1:
        field_event_write_variable(0x30, mode_field_map_id & 0xFFF);
        field_event_write_variable(0x32, 0);
        field_event_write_variable(0x34, 0);
        break;
    case 2:
        field_event_write_variable(0x36, mode_field_map_id & 0xFFF);
        field_event_write_variable(0x38, 0);
        field_event_write_variable(0x3A, 0);
        break;
    }
}

/* 8009FDD4: Event fe 1f: when the current actor is a party slot's field actor (8009fc10:
 * 8006f990) and that slot's stand-in flag (+22b1) is clear, stand it in for the
 * slot's member (800ad4d4: swap their models, set the flag, restart both
 * animations). */
void field_event_board_gear(void) {
    s32 slot;

    slot = field_party_find_stand_in_slot(field_current_event_actor_index);
    if (slot != 0xFF && game_current_data->inGear[slot] == 0) {
        field_party_board_gear(slot);
    }
    field_current_event_actor->pc += 1;
}

/* 8009FE4C: Event fe 20: when party slot byte 1 is occupied (80062590) and its stand-in
 * flag (+22b1) is set, return the slot to its member (800acfd0: swap the models
 * back, hand the heading over, restart both animations, clear the flag). */
void field_event_leave_gear(void) {
    u8 slot;

    slot = EVENT_OPERAND_BYTE(1);
    if (mode_party_members[slot] != 0xFF && game_current_data->inGear[slot] != 0) {
        field_party_leave_gear(slot);
    }
    field_current_event_actor->pc += 2;
}

/* 8009FEE4: Record party slot `slot`'s map (its layer in bits 14 up) and integer x/z
 * in event variables 2a/2c/2e, 30/32/34 or 36/38/3a. Declared int without
 * a return value: the original keeps $v0 live on exit. */
s32 field_party_record_slot_position(s32 slot) {
    s32 layer;

    if (mode_party_actors[slot] != 0xFF) {
        layer = field_view.components.descriptors[mode_party_actors[slot]].actor->layer << 14;
        switch (slot) {
        case 0:
            field_event_write_variable(0x2A, (mode_field_map_id & 0xFFF) | layer);
            field_event_write_variable(0x2C, WHOLE(field_view.components.descriptors[mode_party_actors[slot]].actor->position[0]));
            field_event_write_variable(0x2E, WHOLE(field_view.components.descriptors[mode_party_actors[slot]].actor->position[2]));
            break;
        case 1:
            field_event_write_variable(0x30, (mode_field_map_id & 0xFFF) | layer);
            field_event_write_variable(0x32, WHOLE(field_view.components.descriptors[mode_party_actors[slot]].actor->position[0]));
            field_event_write_variable(0x34, WHOLE(field_view.components.descriptors[mode_party_actors[slot]].actor->position[2]));
            break;
        case 2:
            field_event_write_variable(0x36, (mode_field_map_id & 0xFFF) | layer);
            field_event_write_variable(0x38, WHOLE(field_view.components.descriptors[mode_party_actors[slot]].actor->position[0]));
            field_event_write_variable(0x3A, WHOLE(field_view.components.descriptors[mode_party_actors[slot]].actor->position[2]));
            break;
        }
    }
}

/* 800A0158: Read party slot `slot`'s variable triple (see field_party_record_slot_map). */
void field_party_read_slot_position(s32 slot, s32 *a, s32 *b, s32 *c) {
    switch (slot) {
    case 0:
        *a = field_event_read_variable(0x2A);
        *b = field_event_read_variable(0x2C);
        *c = field_event_read_variable(0x2E);
        break;
    case 1:
        *a = field_event_read_variable(0x30);
        *b = field_event_read_variable(0x32);
        *c = field_event_read_variable(0x34);
        break;
    case 2:
        *a = field_event_read_variable(0x36);
        *b = field_event_read_variable(0x38);
        *c = field_event_read_variable(0x3A);
        break;
    }
}

/* 800A0228: Event 5c: the current actor becomes party slot operand 1 (at most 2):
 * take its member's sprite and shown it at the slot's recorded map
 * position (variables 2a..3a) when that is this map; with no member, show
 * the field's first sprite instead. Members away from this map stay hidden
 * (descriptor flag 0x20). */
void field_event_become_party_slot(void) {
    FieldDescriptor *descriptor;
    s32 slot;
    s32 shown;
    s32 map;
    s32 x;
    s32 z;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    slot = field_event_read_imm_or_var(1);
    if (slot >= 3) {
        slot = 2;
    }
    shown = 1;
    mode_party_stand_in_actors[slot] = field_current_event_actor_index;
    if (mode_party_members[slot] != 0xFF && mode_get_character_gear_id(mode_party_members[slot]) != 0xFF) {
        field_party_read_slot_position(slot, &map, &x, &z);
        field_current_event_actor->layer = (map >> 14) & 3;
        if ((mode_field_map_id & 0xFFF) != (map & 0x3FFF)) {
            shown = 0;
            x = 0;
            z = 0;
            field_current_event_actor->layer = 0;
        }
        if (game_current_data->inGear[slot] != 0) {
            shown = 0;
        } else if (mode_party_members[slot] == 7) {
            shown = 0;
        }
        descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
        field_actor_create_sprite(field_current_event_actor_index, slot, mode_party_sprite_blocks[slot], 1, 0, slot, 1);
        field_view.components.descriptors[field_current_event_actor_index].flags &= 0xFFDF;
        if ((mode_field_map_id & 0xFFF) != (map & 0x3FFF)) {
            field_current_event_actor->layer = 0;
        }
        field_event_place_actor_on_floor(x, z);
        field_event_sync_actor_position();
        field_current_event_actor->flags = (field_current_event_actor->flags | 0x400) & ~0x300;
        if (shown == 0) {
            field_view.components.descriptors[field_current_event_actor_index].flags |= 0x20;
        }
    } else {
        field_event_show_first_sprite();
        field_current_event_actor->pc += 2;
        field_current_event_actor->layer_flags |= 0x800;
        return;
    }
    if (field_view.components.layer_count - 1 < field_current_event_actor->layer) {
        field_current_event_actor->layer = 0;
    }
    field_current_event_actor->flags |= 0x20000;
    field_current_event_actor->layer_flags |= 0xC00;
    field_current_event_actor->pc += 3;
}

/* 800A0524: Copy actor `from`'s collision state, height, +50 words, position and
 * matrix to actor `to` and move `to`'s model to it. */
void field_actor_copy_position_state(s32 to, s32 from) {
    FieldActor *target;
    FieldActor *source;
    s32 i;

    target = field_view.components.descriptors[to].actor;
    source = field_view.components.descriptors[from].actor;
    for (i = 0; i < 4; i++) {
        target->triangle[i] = source->triangle[i];
    }
    target->layer = source->layer;
    target->unkEC = source->unkEC;
    target->unk72 = source->unk72;
    target->unk50[0] = source->unk50[0];
    target->unk50[1] = source->unk50[1];
    target->unk50[2] = source->unk50[2];
    target->position[0] = source->position[0];
    target->position[1] = source->position[1];
    target->position[2] = source->position[2];
    field_matrix_copy_rotation(&field_view.components.descriptors[to].matrix, &field_view.components.descriptors[from].matrix);
    field_matrix_copy_translation(&field_view.components.descriptors[to].matrix, &field_view.components.descriptors[from].matrix);
    field_view.components.descriptors[to].model->x = field_view.components.descriptors[from].actor->position[0];
    field_view.components.descriptors[to].model->y = field_view.components.descriptors[from].actor->position[1];
    field_view.components.descriptors[to].model->z = field_view.components.descriptors[from].actor->position[2];
}

/* 800A06E8: Give the current actor the sprite of party member operand 1, or hide it
 * (flag 1, layer flag 0x100000) and end its script when absent. */
void field_event_set_party_sprite(void) {
    FieldDescriptor *descriptor;
    s32 slot;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    slot = field_party_find_character_slot(field_event_resolve_character(field_event_read_imm_or_var(1)));
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    if (slot != -1) {
        field_actor_create_sprite(field_current_event_actor_index, slot, mode_party_sprite_blocks[slot], 2, 0, slot, 1);
        field_unread_party_sprite_take_mark = -0xC0;
        field_view.components.descriptors[field_current_event_actor_index].flags &= 0xFFDF;
        field_event_sync_actor_position();
        field_current_event_actor->flags = (field_current_event_actor->flags | 0x100) & ~0x80;
        field_view.components.descriptors[field_current_event_actor_index].flags &= 0xFFDF;
    } else {
        field_actor_create_sprite(field_current_event_actor_index, 0, mode_party_sprite_blocks[0], 1, 0, 0, 1);
        field_current_event_actor->flags |= 1;
        field_event_yield_ends_run = 1;
        field_event_yield_requested = 1;
        field_current_event_actor->layer_flags |= 0x100000;
    }
    field_current_event_actor->pc += 3;
}

/* 800A08B8: Event 16: the current actor becomes party character operand 1 (ff, fe, fd: party slots 2, 1, 0). A party member takes its slot (slot 0 becomes the controlled actor), its sprite (or sprite 800ae294[character] of the alternate set 800b2268) and map entry variable 2; others hide and end their script.
 * The alternate sprite's offset entry is addressed before the call. */
void field_event_become_party_character(void) {
    FieldDescriptor *descriptor;
    s32 character;
    s32 slot;
    Sprite *model;
    s32 *sprites;
    s32 *entry;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    character = field_event_resolve_character(field_event_read_imm_or_var(1));
    slot = field_party_find_character_slot(character);
    field_current_event_actor->unkE4 = character;
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    if (slot != -1) {
        if (slot == 0) {
            field_work.controlled = field_current_event_actor_index;
            field_work.unk233E = field_current_event_actor_index;
            field_current_event_actor->flags = (field_current_event_actor->flags | 0x4400) & ~0x80;
        }
        mode_party_actors[slot] = field_current_event_actor_index;
        if (field_work.unk2268 != 0) {
            sprites = field_view.components.sprites;
            entry = &sprites[field_character_sprite_ids[character] + 1] + field_work.unk2268;
            field_actor_create_sprite(field_current_event_actor_index, field_character_sprite_ids[character] + field_work.unk2268, (u8 *)(*entry + (s32)sprites),
                          0, 0, (field_character_sprite_ids[character] + field_work.unk2268) | 0x80, 1);
            field_current_event_actor->flags = (field_current_event_actor->flags | 0x400) & ~0x300;
            if (game_current_data->inGear[slot] != 0) {
                model = field_view.components.descriptors[field_current_event_actor_index].model;
                field_view.components.descriptors[field_current_event_actor_index].model = field_view.components.descriptors[mode_party_stand_in_actors[slot]].model;
                field_view.components.descriptors[mode_party_stand_in_actors[slot]].model = model;
                field_current_event_actor->flags = (field_current_event_actor->flags | 0x200) & ~0x500;
            }
        } else {
            field_actor_create_sprite(field_current_event_actor_index, slot, mode_party_sprite_blocks[slot], 1, 0, slot, 1);
            field_current_event_actor->flags = (field_current_event_actor->flags | 0x400) & ~0x300;
        }
        field_unread_party_sprite_take_mark = -0xC0;
        field_view.components.descriptors[field_current_event_actor_index].flags &= 0xFFDF;
        field_event_place_at_map_entry(field_event_read_variable(2));
        field_event_sync_actor_position();
        field_current_event_actor->layer_flags &= ~0x800;
    } else {
        field_actor_create_sprite(field_current_event_actor_index, 0, mode_party_sprite_blocks[0], 1, 0, 0, 1);
        field_current_event_actor->flags |= 1;
        field_event_yield_ends_run = 1;
        field_event_yield_requested = 1;
        field_current_event_actor->layer_flags |= 0x100000;
    }
    field_current_event_actor->flags |= 0x20000;
    field_current_event_actor->layer_flags |= 0x400;
    field_current_event_actor->pc += 3;
}

/* 800A0C4C: Set flag 0x80 on the controlled actor. */
void field_actor_set_controlled_flag_80(void) {
    FieldActor *player;

    player = field_view.components.descriptors[field_work.controlled].actor;
    player->flags |= 0x80;
}

/* 800A0C94: Mirror the current actor's position into its descriptor and model. */
void field_event_sync_actor_position(void) {
    Sprite *model;

    model = field_view.components.descriptors[field_current_event_actor_index].model;
    field_view.components.descriptors[field_current_event_actor_index].transform.t[0] = field_view.components.descriptors[field_current_event_actor_index].matrix.t[0] =
        WHOLE(field_current_event_actor->position[0]);
    field_view.components.descriptors[field_current_event_actor_index].transform.t[1] = field_view.components.descriptors[field_current_event_actor_index].matrix.t[1] =
        WHOLE(field_current_event_actor->position[1]);
    field_view.components.descriptors[field_current_event_actor_index].transform.t[2] = field_view.components.descriptors[field_current_event_actor_index].matrix.t[2] =
        WHOLE(field_current_event_actor->position[2]);
    model->x = field_current_event_actor->position[0];
    model->y = field_current_event_actor->position[1];
    model->z = field_current_event_actor->position[2];
    model->speed_y = 0;
    field_current_event_actor->unk72 = model->ground = WHOLE(field_current_event_actor->position[1]);
}

/* 800A0D3C: Give the current actor the field's first sprite and show it. */
void field_event_show_first_sprite(void) {
    FieldActor *actor;
    s32 *sprites;

    sprites = field_view.components.sprites;
    field_actor_create_sprite(field_current_event_actor_index, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 1);
    field_event_sync_actor_position();
    actor = field_current_event_actor;
    actor->flags |= 0x100;
    actor->layer_flags |= 0x800;
    actor->pc++;
}

/* 800A0DC0: Set 800b234a from operand 1: the slot count the 801e module's tween pool is
 * reset with (801e738c) when the layers are rebuilt (80077ab4). */
void field_event_set_tween_slots(void) {
    field_work.unk234A = field_event_read_imm_or_var(1);
    field_current_event_actor->pc += 3;
}

/* 800A0DFC: Store the disc number (80028530: directory table word 0x3c) in variable
 * operand 1. */
void field_event_store_disc_number(void) {
    s32 reference;

    reference = field_event_read_u16(1) & 0xFFFF;
    field_event_write_variable(reference & 0xFFFF, cd_get_disc_number());
    field_current_event_actor->pc += 3;
}

/* 800A0E54: Wait (pc-- back to fe), yielding, until the movie mode 800adb74 is clear
 * (800a7c58 clears it when the movie ends). */
void field_event_wait_movie_mode(void) {
    if (field_movie_mode == 0) {
        field_current_event_actor->pc += 1;
    } else {
        field_current_event_actor->pc -= 1;
    }
    field_event_yield_requested = 1;
}

/* 800A0EB0: Count 800adb84 up and yield: a nonzero 800adb84 ends a window movie (movie
 * mode 2, ext 67) in 800a7c58. */
void field_event_end_window_movie(void) {
    field_event_yield_requested = 1;
    field_movie_end_count += 1;
    field_current_event_actor->pc += 1;
}

/* 800A0EE8: Close the current actor's 801e layer (+12c bits 13-15), clearing its layer
 * flag 0x2000: byte 1 0 deactivates the layer object, 1 releases the layer's
 * actor (801e8030) and counts 800b2264 down. Yields. Other byte values leave
 * the pc on the extended byte, which then runs as primary opcode ca. */
void field_event_layer(void) {
    FieldActor *actor;
    s32 layer;

    actor = field_current_event_actor;
    layer = actor->state.bits.layer;
    actor->layer_flags &= ~0x2000;
    switch (field_event_bytecode[actor->pc + 1]) {
    case 0:
        gear_model_actors[layer]->active = 0;
        field_current_event_actor->pc += 2;
        break;
    case 1:
        gear_model_free_actor(actor->state.bits.layer);
        field_work.unk2264--;
        field_current_event_actor->pc += 2;
        break;
    }
    field_event_yield_requested = 1;
}

/* 800A0FD8: Event fe 5c: 801e layer model of the current actor's layer (+12c bits 13-15)
 * by byte 1; waits (pc-- to the fe) while a music-wave read or the stream is
 * busy (800adb2c, 8008a558). 0 deactivates the layer's object; 1 releases the
 * layer's actor (801e8030) and starts reading its two files 0x6ba/0x6bb + 2 *
 * operand 5, which is the operand of the following fe 5c 02; 2 waits for that
 * read, then creates the layer's actor from the files (801e742c) at the actor's
 * position and scale, sets layer flag 0x2000 and advances 4 (operand 2 is read
 * but unused). 0 and 1 advance 2. Other values do not advance: the pc stays on
 * the extended byte, which runs as primary 5c when the slot resumes. Yields. */
void field_event_layer_model(void) {
    s32 layer;

    if (field_music_stream_running != 0 || field_music_is_stream_or_disc_busy() != 0) {
        field_event_yield_requested = 1;
        field_current_event_actor->pc--;
        return;
    }
    layer = field_current_event_actor->state.bits.layer;
    field_current_event_actor->layer_flags &= ~0x2000;
    cd_select_directory(4, 0);
    switch (field_event_bytecode[field_current_event_actor->pc + 1]) {
    case 0:
        gear_model_actors[layer]->active = 0;
        field_current_event_actor->pc += 2;
        break;
    case 1:
        gear_model_free_actor(field_current_event_actor->state.bits.layer);
        field_work.unk21DC[layer] = field_event_read_imm_or_var(5) * 2;
        field_layer_file_requests[0].file = field_work.unk21DC[layer] + 0x6BA;
        field_layer_file_requests[0].destination = mode_field_layer_script_files[layer] = heap_alloc(cd_get_aligned_file_size(field_work.unk21DC[layer] + 0x6BA), 0);
        field_layer_file_requests[1].file = field_work.unk21DC[layer] + 0x6BB;
        field_layer_file_requests[1].destination = mode_field_layer_model_files[layer] = heap_alloc(cd_get_aligned_file_size(field_work.unk21DC[layer] + 0x6BB), 1);
        field_layer_file_requests[2].file = 0;
        field_layer_file_requests[2].destination = 0;
        cd_read_file_list(field_layer_file_requests, 0, 0);
        field_current_event_actor->pc += 2;
        break;
    case 2:
        if (cd_sync_reads(1) == 0) {
            field_event_read_imm_or_var(2);
            gear_model_create_actor(layer, 0, mode_field_layer_script_files[layer], mode_field_layer_model_files[layer],
                          (s16)(0x240 - (layer + field_work.unk225F[layer]) * 64), 0x100, 0,
                          (s16)(layer + 0xFC), &field_work.layer_positions[layer].vx);
            field_work.layer_scales[layer] = gear_model_actors[layer]->scale;
            heap_free(mode_field_layer_model_files[layer]);
            field_current_event_actor->pc += 4;
            field_current_event_actor->layer_flags |= 0x2000;
            gear_model_actors[layer]->scale = (field_current_event_actor->scale[0] * 5) >> 6;
            gear_model_actors[layer]->groundY = field_current_event_actor->position[1] >> 16;
            gear_model_actors[layer]->parts->translation[0] = WHOLE(field_current_event_actor->position[0]);
            gear_model_actors[layer]->parts->translation[2] = WHOLE(field_current_event_actor->position[2]);
        } else {
            field_current_event_actor->pc--;
        }
        break;
    }
    field_event_yield_requested = 1;
}

/* 800A1364: Make the current actor the next 801e layer (800b2264, counted up): give
 * it the field's first sprite, mirror its position, set flag 0x100 and
 * layer flag 0x2000 (clearing 0x800), record operand 1 doubled as the
 * layer's resource pair (files 0x6ba/0x6bb + it, which fe 5c loads) and
 * clear its 800b225f entry. */
void field_event_set_layer_sprite(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 *sprites;
    s32 value;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    value = field_event_read_imm_or_var(1);
    sprites = field_view.components.sprites;
    field_actor_create_sprite(field_current_event_actor_index, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 1);
    field_event_sync_actor_position();
    actor = field_current_event_actor;
    actor->pc += 3;
    actor->flags |= 0x100;
    field_view.components.descriptors[field_current_event_actor_index].flags &= 0xFFDF;
    actor->layer_flags = (actor->layer_flags | 0x2000) & ~0x800;
    field_work.unk21DC[field_work.unk2264] = value * 2;
    field_work.unk225F[field_work.unk2264] = 0;
    field_current_event_actor->state.bits.layer = field_work.unk2264;
    field_work.unk2264++;
}

/* 800A14F0: Event fe 15: give the current actor field sprite operand 1 with bank operand
 * 3 (80076ac0), mirror its position into its descriptor and model (800a0c94)
 * and enable it: descriptor bit 0x200 set and 0x20 cleared, flag 0x100 set and
 * 0x80 cleared, layer bit 11 cleared. */
void field_event_set_sprite_parameter(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 sprite;
    s32 *sprites;
    u8 *data;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    sprite = field_event_read_imm_or_var(1);
    sprites = field_view.components.sprites;
    data = (u8 *)(sprites[sprite + 1] + (s32)sprites);
    field_actor_create_sprite(field_current_event_actor_index, sprite, data, 0, field_event_read_imm_or_var(3), sprite | 0x80, 1);
    field_event_sync_actor_position();
    actor = field_current_event_actor;
    actor->pc += 5;
    actor->flags = (actor->flags | 0x100) & ~0x80;
    actor->layer_flags &= ~0x800;
    field_view.components.descriptors[field_current_event_actor_index].flags &= 0xFFDF;
}

/* 800A1624: Give the current actor a sprite built from field sprite sheet operand 1
 * (80076ac0: kind 0, bank 0 and flag 0, which updates the sprite once), mirror
 * its position into its descriptor and model (800a0c94), set its flag 0x100
 * (clearing 0x80), clear its layer flag 0x800 and its descriptor flag 0x20; its
 * descriptor flags 0x0f80 first become 0x200 (scheduled by 800a2030). */
void field_event_set_sprite(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 sprite;
    s32 *sprites;

    descriptor = &field_view.components.descriptors[field_current_event_actor_index];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    sprite = field_event_read_imm_or_var(1);
    sprites = field_view.components.sprites;
    field_actor_create_sprite(field_current_event_actor_index, sprite, (u8 *)(sprites[sprite + 1] + (s32)sprites), 0, 0, sprite | 0x80, 0);
    field_event_sync_actor_position();
    actor = field_current_event_actor;
    actor->pc += 3;
    actor->flags = (actor->flags | 0x100) & ~0x80;
    actor->layer_flags &= ~0x800;
    field_view.components.descriptors[field_current_event_actor_index].flags &= 0xFFDF;
}

/* 800A1730: Call the script at operand 1, pushing the return PC (after the 5-byte
 * instruction); with the four-entry call stack full, report and yield. */
void field_event_call_long(void) {
    FieldActor *actor;

    actor = field_current_event_actor;
    if ((actor->state.word & 0x1C0) != 0x100) {
        actor->call_stack[(actor->state.word >> 6) & 7] = actor->pc + 5;
        field_current_event_actor->pc = field_event_read_u16(1);
        field_current_event_actor->state.word = (field_current_event_actor->state.word & ~0x1C0) | ((((field_current_event_actor->state.word >> 6) & 7) + 1) & 7) << 6;
    } else {
        if (field_monitor_absent == 0) {
            console_report_printf("STACKERR ACT=%d\n", field_current_event_actor_index);
        }
        field_event_yield_requested = 1;
    }
}

/* 800A17F4: Call the script at operand 1, pushing the return PC (after this 3-byte
 * instruction); with the four-entry call stack full, report and yield without
 * advancing. */
void field_event_call(void) {
    FieldActor *actor;

    actor = field_current_event_actor;
    if ((actor->state.word & 0x1C0) != 0x100) {
        actor->call_stack[(actor->state.word >> 6) & 7] = actor->pc + 3;
        field_current_event_actor->pc = field_event_read_u16(1);
        field_current_event_actor->state.word = (field_current_event_actor->state.word & ~0x1C0) | ((((field_current_event_actor->state.word >> 6) & 7) + 1) & 7) << 6;
    } else {
        if (field_monitor_absent == 0) {
            console_report_printf("STACKERR ACT=%d\n", field_current_event_actor_index);
        }
        field_event_yield_requested = 1;
    }
}

/* 800A18B8: Return from a script call; with the call stack empty, report, end the
 * current script slot (priority 15, tag 0xff) and yield. */
void field_event_return(void) {
    FieldActor *actor;

    actor = field_current_event_actor;
    if ((actor->state.word & 0x1C0) == 0) {
        if (field_monitor_absent == 0) {
            console_report_printf("STACKERR ACT=%d\n", field_current_event_actor_index);
        }
        field_current_event_actor->slots[field_current_event_actor->slot].priority = 15;
        field_current_event_actor->slots[field_current_event_actor->slot].tag = 0xFF;
        field_event_yield_ends_run = 1;
        field_event_yield_requested = 1;
    } else {
        actor->state.word = (actor->state.word & ~0x1C0) | ((((actor->state.word >> 6) & 7) - 1) & 7) << 6;
        actor->pc = actor->call_stack[(actor->state.word >> 6) & 7];
    }
}

/* 800A19B0: Reset the current actor's eight script slots (priority 15, tag ff, resume
 * pc ffff), its call depth and +84, select slot 0 and yield without
 * advancing: all its scripts end, so the scheduler next starts event 1. */
void field_event_reset_slots(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        field_current_event_actor->slots[i].countdown = 0;
        field_current_event_actor->slots[i].unk16 = 0;
        field_current_event_actor->slots[i].priority = 15;
        field_current_event_actor->slots[i].resume_pc = 0xFFFF;
        field_current_event_actor->slots[i].unk22 = 0;
        field_current_event_actor->slots[i].tag = 0xFF;
        field_current_event_actor->slots[i].value = 0xFFFF;
        field_current_event_actor->slots[i].move_mode = 0;
    }
    field_current_event_actor->slot = 0;
    field_current_event_actor->unk0CF = 0;
    field_event_yield_requested = 1;
    field_current_event_actor->unk84 = 0;
    field_current_event_actor->state.bits.depth = 0;
}

/* 800A1A8C: Point every priority-7 script slot at the actor's script 1, end the
 * current slot and yield. */
void field_event_reset_idle_and_end(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (field_current_event_actor->slots[i].priority == 7) {
            field_current_event_actor->slots[i].resume_pc = field_event_get_entry_pc(field_current_event_actor_index, 1);
        }
    }
    field_current_event_actor->slots[field_current_event_actor->slot].priority = 15;
    field_current_event_actor->slots[field_current_event_actor->slot].tag = 0xFF;
    field_event_yield_requested = 1;
}

/* 800A1B70: End the current script slot and yield. */
void field_event_end_slot(void) {
    field_current_event_actor->slots[field_current_event_actor->slot].priority = 15;
    field_current_event_actor->slots[field_current_event_actor->slot].tag = 0xFF;
    field_event_yield_ends_run = 1;
    field_event_yield_requested = 1;
}

/* 800A1BD0: Event 02: compare two halfword operands (bits 7/6 of operand byte 5
 * select an event variable or a signed immediate; variables compare
 * unsigned when flagged so) by condition bits 0-3 of byte 5, and jump to
 * operand 6 unless it holds. */
void field_event_branch_if_false(void) {
    s32 left;
    s32 right;
    s32 result;

    right = 0;
    left = 0;
    switch (EVENT_OPERAND_BYTE(5) & 0xF0) {
    case 0x00:
        left = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
        right = field_event_read_variable(field_event_read_u16(3) & 0xFFFF);
        if (field_event_is_variable_unsigned(field_event_read_u16(1) & 0xFFFF) != 0) {
            right &= 0xFFFF;
        } else {
            right = (s16)right;
        }
        break;
    case 0x40:
        left = field_event_read_variable(field_event_read_u16(1) & 0xFFFF);
        right = (s16)field_event_read_s16(3);
        if (field_event_is_variable_unsigned(field_event_read_u16(1) & 0xFFFF) != 0) {
            right &= 0xFFFF;
        }
        break;
    case 0x80:
        left = (s16)field_event_read_s16(1);
        right = field_event_read_variable(field_event_read_u16(3) & 0xFFFF);
        if (field_event_is_variable_unsigned(field_event_read_u16(3) & 0xFFFF) != 0) {
            left &= 0xFFFF;
        }
        break;
    case 0xC0:
        left = (s16)field_event_read_s16(1);
        right = (s16)field_event_read_s16(3);
        break;
    }
    result = 0;
    switch (EVENT_OPERAND_BYTE(5) & 0xF) {
    case 0:
        if (left == right) {
            result++;
        }
        break;
    case 1:
        if (left != right) {
            result++;
        }
        break;
    case 2:
        if (left > right) {
            result++;
        }
        break;
    case 3:
        if (left < right) {
            result++;
        }
        break;
    case 4:
        if (left >= right) {
            result++;
        }
        break;
    case 5:
        if (left <= right) {
            result++;
        }
        break;
    case 6:
        if (left & right) {
            result++;
        }
        break;
    case 7:
        if (left != right) {
            result++;
        }
        break;
    case 8:
        if (left | right) {
            result++;
        }
        break;
    case 9:
        if (left & right) {
            result++;
        }
        break;
    case 10:
        if (~left & right) {
            result++;
        }
        break;
    }
    if (result == 1) {
        field_current_event_actor->pc += 8;
    } else {
        field_current_event_actor->pc = field_event_read_u16(6);
    }
}

/* 800A1E74: Jump to operand 1. */
void field_event_jump(void) {
    field_current_event_actor->pc = field_event_read_u16(1);
}

/* 800A1E9C: Advance, raising the batch limit by 32. */
void field_event_raise_batch_limit(void) {
    field_event_batch_limit += 32;
    field_current_event_actor->pc++;
}

extern void (*field_event_primary_handlers[])(void); /* event instructions */

/* 800A1EC8: Run the current actor's event instructions until one yields, its script
 * slot ends, the field starts a transition or `limit` (raised by some
 * instructions) runs out; 1024 is an error. Declared int without a
 * value, as the original keeps $v0 live (its loop delay slot stays empty). */
s32 field_event_run_instructions(s32 limit) {
    s32 count;

    field_event_yield_requested = 0;
    field_event_batch_limit = limit;
    for (count = 0; count < field_event_batch_limit; count++) {
        if (count > 0x400) {
            if (field_monitor_absent == 0) {
                console_report_printf("EVENTLOOP ERROR ACT=%d\n", field_current_event_actor_index);
            }
            return;
        }
        field_event_primary_handlers[field_event_bytecode[field_current_event_actor->pc]]();
        if (field_event_yield_ends_run == 0) {
            field_event_batch_limit = 0xFFFF;
        }
        if (field_event_runs_per_frame != 0 && (field_scripted_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_map_change_not_requested == 0)) {
            return;
        }
        if (field_event_yield_requested == 1 && field_event_yield_ends_run == field_event_yield_requested) {
            return;
        }
    }
}

/* 800A2030: Run every active actor's event script for this frame (only the first while
 * field_movie_mode is 1): pick its highest-priority slot (or start event 1), run it
 * and keep its resume PC; stop once the field starts a transition. Declared
 * int without a value like field_event_run_instructions. The running descriptor is
 * published before its actor is read (the actor pointer is loaded again). */
s32 field_event_run_all_actors(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 count;
    s32 index;
    s32 i;
    s32 priority;

    if (field_movie_mode == 1) {
        count = 1;
    } else {
        count = field_event_actor_count;
    }
    field_player_control_polled = 0;
    field_dialogue_pass_open_count = 0;
    for (index = 0; index < count; index++) {
        if (!(field_view.components.descriptors[index].flags & 0xF00)
            || (field_view.components.descriptors[index].actor->layer_flags & 0x100000)) {
            continue;
        }
        if (field_event_runs_per_frame != 0 && (field_scripted_battle_not_requested == 0 || field_worldmap_exit_not_requested == 0 || field_map_change_not_requested == 0)) {
            return;
        }
        descriptor = &field_view.components.descriptors[index];
        field_current_event_descriptor = descriptor;
        actor = descriptor->actor;
        actor->flags &= ~0x1000000;
        field_current_event_actor_index = index;
        field_current_event_actor = actor;
        priority = 0xF;
        if (field_work.party_processing_mode != 0) {
            for (i = 0; i < 3; i++) {
                if (mode_party_actors[i] != 0xFF && mode_party_actors[i] == index) {
                    goto next;
                }
            }
        }
        for (i = 0; i < 8; i++) {
            if (priority >= field_current_event_actor->slots[i].priority) {
                priority = field_current_event_actor->slots[i].priority;
                field_current_event_actor->slot = i;
            }
        }
        if (priority == 0xF) {
            field_current_event_actor->slots[0].resume_pc = field_event_get_entry_pc(index, 1);
            field_current_event_actor->slots[0].priority = 7;
            field_current_event_actor->slot = 0;
        }
        field_current_event_actor->pc = field_current_event_actor->slots[field_current_event_actor->slot].resume_pc;
        field_event_yield_ends_run = 1;
        if (!(field_current_event_actor->flags & 1)) {
            field_event_run_instructions(8);
        }
        field_current_event_actor->slots[field_current_event_actor->slot].resume_pc = field_current_event_actor->pc;
    next:;
    }
}

/* 800A22AC: Run event `event` of actor 0 immediately with fresh script slots, then
 * restore the actor's record. */
void field_event_run_actor0_event(s32 event) {
    FieldActor *saved;
    s32 i;

    field_current_event_actor = (field_current_event_descriptor = field_view.components.descriptors)->actor;
    saved = heap_alloc(sizeof(FieldActor), 1);
    *saved = *field_current_event_descriptor->actor;
    for (i = 0; i < 8; i++) {
        field_current_event_actor->slots[i].countdown = 0;
        field_current_event_actor->slots[i].unk16 = 0;
        field_current_event_actor->slots[i].priority = 15;
        field_current_event_actor->slots[i].resume_pc = 0xFFFF;
        field_current_event_actor->slots[i].unk22 = 0;
        field_current_event_actor->slots[i].tag = 0xFF;
        field_current_event_actor->slots[i].value = 0xFFFF;
        field_current_event_actor->slots[i].move_mode = 0;
    }
    field_current_event_actor_index = 0;
    field_event_runs_per_frame = 0;
    field_event_yield_ends_run = 0;
    field_current_event_actor->pc = field_event_get_entry_pc(0, event);
    field_event_run_instructions(0xFFFF);
    field_event_runs_per_frame = 1;
    *field_current_event_descriptor->actor = *saved;
    heap_free(saved);
}

void field_event_run_actor0_event(s32 mode);

/* 800A2488: Rebuild the party (mode 3) with 800adb8c set. */
void field_event_rebuild_party(void) {
    field_party_rebuilding = 1;
    field_event_run_actor0_event(3);
    field_party_apply_gear_changes();
    field_party_rebuilding = 0;
}

/* 800A24C4: After a return to the field: run actor 0's event 2, show reassigned
 * party members, restart each 801e layer's animation, lift the controlled
 * actor 8 units unless its +74 is 0xff, and move the pieces by their
 * accumulated drift (mode 0: non-event pieces, mode 1: all but moving
 * event actors). `i` also holds the +74 byte, as in the original. */
void field_finish_return_to_field(void) {
    u8 unused[0x10]; /* the original frame holds 0x10 unused bytes */
    s32 i;

    if (mode_field_return_pending != 0) {
        field_event_run_actor0_event(2);
        field_party_set_gear_rider_flags();
        for (i = 0; i < field_work.unk2264; i++) {
            gear_model_select_and_call_entry((u16)i, 0, field_work.unk21E4[i]);
        }
        i = field_view.components.descriptors[field_work.controlled].actor->unk074;
        if (i != 0xFF) {
            field_view.components.descriptors[field_work.controlled].actor->position[1] -= 8;
        }
        for (i = 0; i < field_view.components.descriptor_count; i++) {
            if (i < field_event_actor_count) {
                if (field_view.components.descriptors[i].actor->state.word & 3) {
                    continue;
                }
            } else if ((field_work.piece_drift_mode & 0x7F) == 0) {
                field_view.components.descriptors[i].matrix.t[0] += PIECE_DRIFT_TOTAL[0];
                field_view.components.descriptors[i].matrix.t[1] += PIECE_DRIFT_TOTAL[1];
                field_view.components.descriptors[i].matrix.t[2] += PIECE_DRIFT_TOTAL[2];
            }
            if ((field_work.piece_drift_mode & 0x7F) == 1) {
                field_view.components.descriptors[i].matrix.t[0] += PIECE_DRIFT_TOTAL[0];
                field_view.components.descriptors[i].matrix.t[1] += PIECE_DRIFT_TOTAL[1];
                field_view.components.descriptors[i].matrix.t[2] += PIECE_DRIFT_TOTAL[2];
            }
        }
    }
}

void field_restore_snapshot_sprites(void);

/* 800A2714: Reload the actors' extra blocks (file +124 into +120) and hand them to
 * their models, then refresh the field state. */
void field_reload_actor_blocks(void) {
    FieldActor *actor;
    s32 i;

    if (mode_field_return_pending != 0) {
        for (i = 0; i < field_event_actor_count; i++) {
            cd_select_directory(4, 0);
            actor = field_view.components.descriptors[i].actor;
            if (actor->unk124 != -1) {
                field_current_event_actor = actor;
                field_current_event_actor->unk120 = heap_alloc(cd_get_aligned_file_size(actor->unk124) + 8, 0);
                cd_read_file(field_current_event_actor->unk124, field_current_event_actor->unk120, 0, 0x80);
                cd_sync_reads(0);
            }
        }
        for (i = 0; i < field_event_actor_count; i++) {
            if (field_view.components.descriptors[i].actor->unk124 != -1) {
                sprite_set_alternate_resource(field_view.components.descriptors[i].model,
                              (s32)field_view.components.descriptors[i].actor->unk120);
            }
        }
        field_restore_snapshot_sprites();
        if (field_work.unk2078 != 0) {
            field_distortion_start(1);
        }
        field_event_write_variable(0x10, 0);
        field_event_store_party_members();
        for (i = 0; i < field_event_actor_count; i++) {
            field_descriptor_rebuild_matrix(i);
        }
    }
}

/* 800A28D4: When mode_field_return_pending is set: rebuild every actor's sprite and animation state,
 * swap in the party models and rerun the actors' setup scripts. Each sprite
 * table pointer is a block-local variable. */
void field_event_init_actors(void) {
    s32 i;
    Sprite *model;
    FieldActor *actor;

    if (mode_field_return_pending != 0) {
        field_restore_snapshot();
        for (i = 0; i < field_event_actor_count; i++) {
            actor = field_view.components.descriptors[i].actor;
            if (!(actor->unk126 & 0x80)) {
                field_actor_create_sprite(i, actor->unk127, mode_party_sprite_blocks[actor->unk126], actor->sprite_kind & 3,
                              actor->unk134 & 0xF, field_view.components.descriptors[i].actor->unk126,
                              (field_view.components.descriptors[i].actor->unk134 >> 4) & 1);
            } else {
                s32 *sprites = field_view.components.sprites;

                field_actor_create_sprite(i, actor->unk127, (u8 *)(sprites[(actor->unk126 & 0x7F) + 1] + (s32)sprites),
                              actor->sprite_kind & 3, actor->unk134 & 0xF,
                              field_view.components.descriptors[i].actor->unk126,
                              (field_view.components.descriptors[i].actor->unk134 >> 4) & 1);
                switch (field_view.components.descriptors[i].actor->state.bits.unk16) {
                case 1:
                    sprite_alloc_sequencer_buffer(field_view.components.descriptors[i].model, 2, 0);
                    SPRITE_SEQUENCER(field_view.components.descriptors[i].model)->buffer[2] = field_view.components.descriptors[i].actor->state.bits.unk18;
                    SPRITE_SEQUENCER(field_view.components.descriptors[i].model)->buffer[3] = field_view.components.descriptors[i].actor->unk130;
                    break;
                case 2:
                    sprite_alloc_sequencer_buffer(field_view.components.descriptors[i].model, 3, 0);
                    SPRITE_SEQUENCER(field_view.components.descriptors[i].model)->buffer[2] = field_view.components.descriptors[i].actor->state.bits.unk18;
                    SPRITE_SEQUENCER(field_view.components.descriptors[i].model)->buffer[3] = field_view.components.descriptors[i].actor->unk130;
                    SPRITE_SEQUENCER(field_view.components.descriptors[i].model)->buffer[4] = field_view.components.descriptors[i].actor->unk130_9;
                    SPRITE_SEQUENCER(field_view.components.descriptors[i].model)->buffer[5] = field_view.components.descriptors[i].actor->unk130_19;
                    break;
                }
            }
        }
        if (field_work.unk2268 != 0) {
            for (i = 0; i < 3; i++) {
                if (mode_party_actors[i] != 0xFF) {
                    if (game_current_data->inGear[i] != 0) {
                        model = field_view.components.descriptors[mode_party_actors[i]].model;
                        field_view.components.descriptors[mode_party_actors[i]].model = field_view.components.descriptors[mode_party_stand_in_actors[i]].model;
                        field_view.components.descriptors[mode_party_stand_in_actors[i]].model = model;
                        field_view.components.descriptors[mode_party_stand_in_actors[i]].actor->flags |= 0x200;
                        field_view.components.descriptors[mode_party_stand_in_actors[i]].actor->flags &= ~0x500;
                        field_view.components.descriptors[mode_party_stand_in_actors[i]].flags |= 0x20;
                    } else {
                        field_view.components.descriptors[mode_party_stand_in_actors[i]].actor->flags |= 0x400;
                        field_view.components.descriptors[mode_party_stand_in_actors[i]].actor->flags &= ~0x300;
                    }
                }
            }
        }
    } else {
        field_event_write_variable(0x10, 0);
        field_event_store_party_members();
        for (i = 0; i < field_event_actor_count; i++) {
            field_current_event_actor_index = i;
            field_current_event_descriptor = &field_view.components.descriptors[i];
            field_current_event_actor = field_current_event_descriptor->actor;
            field_current_event_actor->pc = field_event_get_entry_pc(i, 2);
            if (field_event_bytecode[field_current_event_actor->pc] == 0) {
                field_current_event_actor->layer_flags |= 0x4000000;
            }
            field_current_event_actor_index = i;
            field_current_event_descriptor = &field_view.components.descriptors[i];
            field_current_event_actor = field_current_event_descriptor->actor;
            field_current_event_actor->pc = field_event_get_entry_pc(i, 0);
        }
        for (i = 0; i < field_event_actor_count; i++) {
            field_current_event_actor_index = i;
            field_sprite_created_count = 0;
            field_event_yield_ends_run = 0;
            field_current_event_descriptor = &field_view.components.descriptors[i];
            field_current_event_actor = field_current_event_descriptor->actor;
            field_event_run_instructions(0xFFFF);
            if (field_sprite_created_count == 0) {
                s32 *sprites = field_view.components.sprites;

                field_actor_create_sprite(i, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 0);
                field_current_event_actor->layer_flags |= 0x800;
            }
        }
    }
}

/* 800A2FC0: Advance one byte. */
void field_event_nop(void) {
    field_current_event_actor->pc++;
}

/* 800A2FE0: -1 when event variable `reference` is read unsigned, else 0. */
s32 field_event_is_variable_unsigned(s32 reference) {
    if (field_event_package->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        return -1;
    }
    return 0;
}

/* 800A3018: Read event variable `reference` (a byte offset into the bank). */
s32 field_event_read_variable(s32 reference) {
    s32 value;

    if (field_event_package->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        value = (u16)field_event_variables[reference >> 1];
    } else {
        value = field_event_variables[reference >> 1];
    }
    return value;
}

/* 800A3074: Write event variable `reference` (both bank cases store the same halfword). */
void field_event_write_variable(s32 reference, s32 value) {
    if (field_event_package->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        field_event_variables[reference >> 1] = (u16)value;
    } else {
        field_event_variables[reference >> 1] = value;
    }
}

/* 800A3090: Entry PC of event `event` of actor `actor`. */
s32 field_event_get_entry_pc(s32 actor, s32 event) {
    u16 *entries = field_event_package->entries;

    return entries[actor * 32 + event];
}

/* 800A30B4: Store the three party members in variables 3e, 40, 42. */
void field_event_store_party_members(void) {
    field_event_write_variable(0x3E, mode_party_members[0]);
    field_event_write_variable(0x40, mode_party_members[1]);
    field_event_write_variable(0x42, mode_party_members[2]);
}

/* 800A30FC: Record the current map and camera in the game state and variables and
 * save the event variable bank. */
void field_event_save_map_and_variables(void) {
    s32 i;

    game_current_data->map = mode_field_map_id;
    game_current_data->flagWords[0] = mode_music_selected_track;
    game_current_data->entry[2] = game_current_data->vars[1];
    game_current_data->entry[0] = game_current_data->vars[4] << 9;
    field_event_write_variable(0x44, mode_battle_turn_count);
    field_event_write_variable(0x46, mode_result_code);
    field_event_write_variable(6, field_actor_get_controlled_facing_octant() & 0xFFFF);
    field_event_write_variable(8, field_camera_get_octant() & 0xFFFF);
    field_event_write_variable(0x24, (s16)field_view.elevation);
    field_event_write_variable(0x3C, mode_field_map_id);
    field_event_store_party_members();
    for (i = 0; i < 0x200; i++) {
        game_current_data->vars[i] = field_event_variables[i];
    }
}

/* 800A31E8: Update the play record once per frame (not while 800b02c8 is 1): the
 * held buttons seen, the party, the departure data, the party slots'
 * positions, the play clock in variable 10 (minutes:seconds stepped
 * every 31 frames; counting down with 8004f328 bit 2, stopped by bit 7)
 * and the controlled actor's position in variables 1e-22. */
void field_update_play_record(void) {
    s32 i;
    s32 value;
    s32 seconds;
    s32 minutes;

    if (field_play_record_stopped == 1) {
        return;
    }
    field_play_record_buttons |= field_pad_port0_held;
    for (i = 0; i < 3; i++) {
        game_current_data->party[i] = mode_party_members[i];
    }
    field_event_save_map_and_variables();
    mode_unread_play_record_word = 0;
    mode_play_clock_frame_count++;
    for (i = 0; i < 3; i++) {
        if (game_current_data->inGear[i] == 1) {
            field_party_record_slot_position(i);
        }
    }
    if (mode_play_clock_frame_count > 30) {
        mode_play_clock_frame_count = 0;
        if (!(mode_play_clock_flags & 0x80)) {
            value = field_event_read_variable(0xA);
            seconds = value & 0xFF;
            minutes = (value >> 8) & 0xFF;
            if (!(mode_play_clock_flags & 4)) {
                if (seconds != 0xFF3B) { /* never equal: the original's limit check */
                    seconds++;
                    if (seconds > 60) {
                        seconds = 0;
                        minutes++;
                    }
                }
            } else if (seconds == 0) {
                if (minutes != 0) {
                    seconds = 59;
                    minutes--;
                }
            } else {
                seconds--;
            }
            field_event_write_variable(0xA, (minutes << 8) | (seconds & 0xFF));
        }
    }
    field_event_write_variable(0xC, pad_play_time_seconds | (pad_play_time_minutes << 8));
    field_event_write_variable(0xE, pad_play_time_hours);
    field_event_write_variable(0x1E, WHOLE(field_view.components.descriptors[field_work.controlled].actor->position[0]));
    field_event_write_variable(0x20, WHOLE(field_view.components.descriptors[field_work.controlled].actor->position[2]));
    field_event_write_variable(0x22, WHOLE(field_view.components.descriptors[field_work.controlled].actor->position[1]));
}

/* 800A3474: Read the field state block at mode_snapshot_block back (the inverse of
 * field_save_snapshot): descriptor count, view, collision attributes, field_work
 * and the per-actor records, keeping each actor's list pointer and
 * allocating its link and unk114 blocks as the record says. */
void field_restore_snapshot(void) {
    FieldDescriptor *descriptor;
    s32 i;
    s32 flags;
    s32 *list;

    field_snapshot_cursor = mode_snapshot_block;
    field_view.components.descriptor_count = *field_snapshot_cursor;
    field_snapshot_cursor += 4;
    COPY_BLOCK(&field_panorama, field_snapshot_cursor, 0x38);
    field_snapshot_cursor += 0x38;
    COPY_BLOCK(&field_view.world_angles, field_snapshot_cursor, 0x74);
    field_snapshot_cursor += 0x74;
    COPY_BLOCK(field_view.components.collision_attributes, field_snapshot_cursor, 0x400);
    field_snapshot_cursor += 0x400;
    COPY_BLOCK(&field_work, field_snapshot_cursor, sizeof(FieldWork));
    field_snapshot_cursor += sizeof(FieldWork);
    COPY_BLOCK(&field_view, field_snapshot_cursor, 0x1C8);
    field_snapshot_cursor += 0x1C8;
    for (i = 0; i < field_event_actor_count; i++) {
        descriptor = &field_view.components.descriptors[i];
        COPY_BLOCK(&descriptor->rotation, field_snapshot_cursor, 8);
        field_snapshot_cursor += 8;
        COPY_BLOCK(&flags, field_snapshot_cursor, 4);
        field_view.components.descriptors[i].flags = flags;
        field_snapshot_cursor += 4;
        field_snapshot_cursor += 0x30;
        list = field_view.components.descriptors[i].actor->list;
        COPY_BLOCK(field_view.components.descriptors[i].actor, field_snapshot_cursor, 0x138);
        field_view.components.descriptors[i].actor->list = list;
        field_snapshot_cursor += 0x138;
        if (field_view.components.descriptors[i].actor->unk134 & 0x80) {
            field_view.components.descriptors[i].actor->link = heap_alloc(0xC, 0);
            COPY_BLOCK(field_view.components.descriptors[i].actor->link, field_snapshot_cursor, 0xC);
            field_snapshot_cursor += 0xC;
        }
        if (field_view.components.descriptors[i].actor->state.word & 0x1000) {
            field_view.components.descriptors[i].actor->unk114 = heap_alloc(0x10, 0);
            COPY_BLOCK(field_view.components.descriptors[i].actor->unk114, field_snapshot_cursor, 0x10);
            field_snapshot_cursor += 0x10;
        }
    }
    COPY_BLOCK(field_event_variables, field_snapshot_cursor, 0x800);
    field_snapshot_cursor += 0x800;
}

/* 800A3C8C: Read the descriptor count, view block and per-actor records from the
 * block at mode_snapshot_block, passing each model its record (sprite_restore_state). */
void field_restore_snapshot_sprites(void) {
    s32 changed;
    s32 i;
    u8 *record;

    field_snapshot_cursor = mode_snapshot_block;
    field_view.components.descriptor_count = *field_snapshot_cursor;
    field_snapshot_cursor += 0x3C;
    *(ViewSnapshot *)&field_view.world_angles = *(ViewSnapshot *)field_snapshot_cursor;
    field_snapshot_cursor += 0x920;
    changed = 0;
    for (i = 0; i < 3; i++) {
        if (mode_snapshot_party_in_gear[i] != game_current_data->inGear[i]) {
            changed++;
        }
    }
    for (i = 0; i < field_event_actor_count; i++) {
        field_snapshot_cursor += 0xC;
        record = field_snapshot_cursor;
        if (field_view.components.descriptors[i].actor->unk124 != -1 && field_view.components.descriptors[i].actor->unk0EA != 0xFF) {
            *(s16 *)(record + 0x14) = field_view.components.descriptors[i].actor->unk0EA;
        }
        if (!(field_view.components.descriptors[i].actor->layer_flags & 0x1000000)) {
            if (field_work.unk2268 == 0 || !(field_view.components.descriptors[i].actor->flags & 0x600)) {
                sprite_restore_state(field_view.components.descriptors[i].model, (SpriteState *)field_snapshot_cursor);
            } else if (changed == 0) {
                sprite_restore_state(field_view.components.descriptors[i].model, (SpriteState *)field_snapshot_cursor);
            }
        }
        record = field_snapshot_cursor;
        field_snapshot_cursor = record + 0x168;
        if (field_view.components.descriptors[i].actor->unk134 & 0x80) {
            field_snapshot_cursor = record + 0x174;
        }
        if (field_view.components.descriptors[i].actor->state.word & 0x1000) {
            field_snapshot_cursor += 0x10;
        }
    }
}

/* 800A3F4C: Write the field state block at mode_snapshot_block (descriptor count, view,
 * collision attributes, field_work, per-actor records and field_event_variables) and
 * print its size. The counter variable is reused for the block address. */
void field_save_snapshot(void) {
    s32 i;
    s32 flags;
    s32 size;
    FieldDescriptor *descriptor;

    field_snapshot_cursor = mode_snapshot_block;
    *field_snapshot_cursor = field_view.components.descriptor_count;
    field_snapshot_cursor += 4;
    COPY_BLOCK(field_snapshot_cursor, &field_panorama, 0x38);
    field_snapshot_cursor += 0x38;
    COPY_BLOCK(field_snapshot_cursor, &field_view.world_angles, 0x74);
    field_snapshot_cursor += 0x74;
    COPY_BLOCK(field_snapshot_cursor, field_view.components.collision_attributes, 0x400);
    field_snapshot_cursor += 0x400;
    COPY_BLOCK(field_snapshot_cursor, &field_work, sizeof(FieldWork));
    field_snapshot_cursor += sizeof(FieldWork);
    COPY_BLOCK(field_snapshot_cursor, &field_view, 0x1C8);
    field_snapshot_cursor += 0x1C8;
    for (i = 0; i < field_event_actor_count; i++) {
        descriptor = &field_view.components.descriptors[i];
        COPY_BLOCK(field_snapshot_cursor, &descriptor->rotation, 8);
        field_snapshot_cursor += 8;
        flags = field_view.components.descriptors[i].flags;
        COPY_BLOCK(field_snapshot_cursor, &flags, 4);
        field_snapshot_cursor += 4;
        sprite_save_state(field_view.components.descriptors[i].model, (SpriteState *)field_snapshot_cursor);
        field_snapshot_cursor += 0x30;
        COPY_BLOCK(field_snapshot_cursor, field_view.components.descriptors[i].actor, 0x138);
        field_snapshot_cursor += 0x138;
        if (field_view.components.descriptors[i].actor->unk134 & 0x80) {
            COPY_BLOCK(field_snapshot_cursor, field_view.components.descriptors[i].actor->link, 0xC);
            field_snapshot_cursor += 0xC;
        }
        if (field_view.components.descriptors[i].actor->state.word & 0x1000) {
            COPY_BLOCK(field_snapshot_cursor, field_view.components.descriptors[i].actor->unk114, 0x10);
            field_snapshot_cursor += 0x10;
        }
    }
    COPY_BLOCK(field_snapshot_cursor, field_event_variables, 0x800);
    field_snapshot_cursor += 0x800;
    for (i = 0; i < 3; i++) {
        mode_snapshot_party_in_gear[i] = game_current_data->inGear[i];
    }
    size = (s32)field_snapshot_cursor;
    i = (s32)mode_snapshot_block;
    if (field_monitor_absent == 0) {
        size -= i;
        console_report_printf("SAVESIZE=%d %x\n", size, size);
    }
}

/* The event instructions by opcode (run by 800a1ec8), and the extended
 * instructions that opcode fe (800869b8) runs by the following byte. */
void (*field_event_primary_handlers[256])(void) = { /* 800AE2A0 */
    /* 00 */ field_event_end_slot, field_event_jump, field_event_branch_if_false, field_event_message_box,
    /* 04 */ field_event_reset_idle_and_end, field_event_call, field_event_call_long, field_event_request_event,
    /* 08 */ field_event_request_event_started, field_event_request_event_finished, field_event_call_in_zone, field_event_set_sprite,
    /* 0C */ field_event_loop_player_control, field_event_return, field_event_nop_0e, field_event_nop_0f,
    /* 10 */ field_event_walk, field_event_walk_limited, field_event_change_map_transition, field_event_nop,
    /* 14 */ field_event_allow_encounters, field_event_inhibit_encounters, field_event_become_party_character, field_event_set_boundary,
    /* 18 */ field_event_set_extents, field_event_place, field_event_set_layer, field_event_place_on_layer,
    /* 1C */ field_event_set_height, field_event_place_at_height, field_event_start_fall, field_event_set_layer_mask,
    /* 20 */ field_event_set_actor_flags, field_event_set_motion_divisor, field_event_enable_self, field_event_disable_self,
    /* 24 */ field_event_enable_actor, field_event_disable_actor, field_event_wait_countdown, field_event_stop_hide_actor,
    /* 28 */ field_event_show_actor, field_event_remove_actor, field_event_block_talk_touch, field_event_unblock_talk_touch,
    /* 2C */ field_event_set_animation, field_event_store_actor_position, field_event_store_facing_octant, field_event_store_own_character,
    /* 30 */ field_event_store_controlled_character, field_event_branch_unless_buttons, field_event_branch_unless_buttons_seen, field_event_forget_buttons_seen,
    /* 34 */ field_event_store_item_count, field_event_set_variable, field_event_set_variable_one, field_event_set_variable_zero,
    /* 38 */ field_event_add_variable, field_event_subtract_variable, field_event_set_variable_bit, field_event_clear_variable_bit,
    /* 3C */ field_event_increment_variable, field_event_decrement_variable, field_event_and_variable, field_event_or_variable,
    /* 40 */ field_event_xor_variable, field_event_shift_left_variable, field_event_shift_right_variable, field_event_random_variable,
    /* 44 */ field_event_turn_move_angle, field_event_move_angle, field_event_set_interaction_offset, field_event_walk_player_ahead,
    /* 48 */ field_event_load_code_byte, field_event_load_code_half, field_event_turn_move_to, field_event_turn_move_to_limited,
    /* 4C */ field_event_move_to, field_event_move_to_limited, field_event_turn_move_by, field_event_turn_move_by_limited,
    /* 50 */ field_event_move_by, field_event_move_by_limited, field_event_turn_move_to_actor, field_event_turn_move_to_actor_limited,
    /* 54 */ field_event_move_to_actor, field_event_move_to_actor_limited, field_event_change_map, field_event_arc,
    /* 58 */ field_event_set_rotation, field_event_wander_pause, field_event_stop, field_event_stop_hold,
    /* 5C */ field_event_become_party_slot, field_event_play_animation, field_event_wait_animation, field_event_face_direction_table3,
    /* 60 */ field_event_save_target_goal, field_event_set_saved_target, field_event_point_a_at_actor, field_event_set_point_a,
    /* 64 */ field_event_save_eye_goal, field_event_set_saved_eye, field_event_point_b_at_actor, field_event_actor_face_direction,
    /* 68 */ field_event_actor_face_view_direction, field_event_face_direction, field_event_face_view_direction, field_event_turn_clockwise,
    /* 6C */ field_event_turn_counterclockwise, field_event_sine_variable, field_event_cosine_variable, field_event_face_actor,
    /* 70 */ field_event_face_party_member, field_event_request_battle, field_event_select_music, field_event_emitter,
    /* 74 */ field_event_play_sound, field_event_select_music_keep, field_event_release_camera_hold, field_event_hold_camera,
    /* 78 */ field_event_read_map_ahead, field_event_restore_all_hp, field_event_restore_all_ep, field_event_reduce_party_hp,
    /* 7C */ field_event_restore_party_ep_7c, field_event_reduce_party_ep, field_event_restore_party_ep, field_event_set_terrain_angle,
    /* 80 */ field_event_set_collision_attribute, field_event_or_collision_attribute, field_event_store_collision_attribute, field_event_and_collision_attribute,
    /* 84 */ field_event_branch_unless_var0_below, field_event_branch_unless_var0_above, field_event_branch_unless_var0_equal, field_event_set_var0,
    /* 88 */ field_event_store_var0, field_event_branch_unless_near, field_event_branch_unless_on_screen, field_event_branch_unless_item,
    /* 8C */ field_event_give_item, field_event_take_item, field_event_branch_unless_gold, field_event_add_gold,
    /* 90 */ field_event_remove_gold, field_event_branch_unless_in_party, field_event_reset_slots, field_event_set_layer_sprite,
    /* 94 */ field_event_set_play_clock, field_event_set_play_clock_mode, field_event_stop_play_clock, field_event_set_camera_heading_mask0,
    /* 98 */ field_event_change_map_entry, field_event_scripted_camera_on, field_event_scripted_camera_off, field_event_set_camera_blend_frames,
    /* 9C */ field_event_wait_dialogue, field_event_blend_camera_distance, field_event_save_camera_view, field_event_restore_camera_view,
    /* A0 */ field_event_set_camera_view, field_event_set_camera_heading_mask1, field_event_wait_camera_flags, field_event_set_point_b,
    /* A4 */ field_event_blend_camera_elevation, field_event_store_camera_octant, field_event_jump_table, field_event_request_player_control,
    /* A8 */ field_event_random_below, field_event_start_choice, field_event_face_view_direction_table2, field_event_reset_camera_points,
    /* AC */ field_event_move_camera, field_event_store_target_goal, field_event_store_eye_goal, field_event_scripted_heading,
    /* B0 */ field_event_scripted_elevation, field_event_scripted_zoom, field_event_wait_camera_flags_b2, field_event_fade_in,
    /* B4 */ field_event_fade_out, field_event_turn_camera_octant, field_event_blend_camera_projection, field_event_unclamp_camera_eye,
    /* B8 */ field_event_clamp_camera_eye, field_event_branch_unless_game_flag, field_event_set_game_flag, field_event_clear_game_flag,
    /* BC */ field_event_show_first_sprite, field_event_rotate_x_add, field_event_rotate_x_sub, field_event_rotate_y_add,
    /* C0 */ field_event_rotate_y_sub, field_event_rotate_z_add, field_event_rotate_z_sub, field_event_yield,
    /* C4 */ field_event_swing_open, field_event_swing_close, field_event_raise_batch_limit, field_event_turn_camera_step,
    /* C8 */ field_event_turn_camera_step_c8, field_event_branch_unless_in_zone, field_event_atan_variable, field_event_branch_unless_in_zone_height,
    /* CC */ field_event_call_in_zone_height, field_event_block_touch, field_event_unblock_touch, field_event_set_window_layout,
    /* D0 */ field_event_set_window_layout_operands, field_event_halt, field_event_message, field_event_message_fixed,
    /* D4 */ field_event_message_actor, field_event_set_input_mask, field_event_set_text_speed, field_event_rotate_model_x,
    /* D8 */ field_event_rotate_model_y, field_event_rotate_model_z, field_event_add_texture_scroll, field_event_set_channel_word,
    /* DC */ field_event_swap_variables, field_event_set_sprite_draw_mode, field_event_multiply_variable, field_event_divide_variable,
    /* E0 */ field_event_set_actor_sprite_draw_mode, field_event_vram_rectangle, field_event_branch_unless_buttons_equal, field_event_branch_unless_buttons_seen_equal,
    /* E4 */ field_event_halt_e4, field_event_set_fog, field_event_set_camera_bounds, field_event_set_clear_color,
    /* E8 */ field_event_shake_actor, field_event_shake_actor_back, field_event_walk_player_ahead_ea, field_event_point_at_angle,
    /* EC */ field_event_point_around, field_event_store_camera_point, field_event_copy_camera_point, field_event_wait_camera_move,
    /* F0 */ field_event_store_scripted_camera, field_event_fade, field_event_shake_camera, field_event_look_from_points,
    /* F4 */ field_event_close_window, field_event_message_centred, field_event_heading_lock, field_event_draw_random_picks,
    /* F8 */ field_event_set_flag_bits, field_event_set_link_actor, field_event_rotate_actor, field_event_branch_unless_bit,
    /* FC */ field_event_message_as_actor, field_event_nop, field_event_run_extended_opcode, field_event_nop,
};
void (*field_event_extended_handlers[227])(void) = { /* 800AE6A0 */
    /* 00 */ field_event_rerun_00, field_event_wander, field_event_branch_unless_well_on_screen, field_event_set_scale,
    /* 04 */ field_event_set_model_82, field_event_branch_unless_on_layer, field_event_branch_unless_on_attribute, field_event_set_offscreen_update,
    /* 08 */ field_event_set_scale_xyz, field_event_set_shadow_hidden, field_event_ext_set_variable_bit, field_event_ext_clear_variable_bit,
    /* 0C */ field_event_set_unread_work_halfwords, field_event_set_character, field_event_music_fade, field_event_music_pitch,
    /* 10 */ field_event_music_tempo, field_event_music_pan, field_event_music_mute, field_event_set_sound_emitter,
    /* 14 */ field_event_set_sound_emitter_80, field_event_set_sprite_parameter, field_event_free_boundary, field_event_actor_face_actor,
    /* 18 */ field_event_join_party_byte, field_event_leave_party, field_event_apply_party_sprite, field_event_scroll_texture,
    /* 1C */ field_event_set_position, field_event_set_piece_drift, field_event_switch_party_to_gears, field_event_board_gear,
    /* 20 */ field_event_leave_gear, field_event_set_party_sprite, field_event_store_projection, field_event_gather_party,
    /* 24 */ field_event_wait_party_gathered, field_event_set_camera_height_follow, field_event_screen_distortion, field_event_distortion,
    /* 28 */ field_event_store_flags0, field_event_store_flags1, field_event_store_flags2, field_event_store_flags3,
    /* 2C */ field_event_store_actor_flags, field_event_store_actor_flags1, field_event_store_actor_layer_flags, field_event_store_actor_flags3,
    /* 30 */ field_event_branch_unless_flags0, field_event_branch_unless_flags1, field_event_branch_unless_flags2, field_event_branch_unless_flags3,
    /* 34 */ field_event_branch_unless_actor_flags0, field_event_branch_unless_actor_flags1, field_event_branch_unless_actor_flags2, field_event_branch_unless_actor_flags3,
    /* 38 */ field_event_store_actor_distance, field_event_set_offset_scale, field_event_set_party_bit, field_event_clear_party_bit,
    /* 3C */ field_event_call_layer_script, field_event_set_layer_light_row, field_event_set_layer_color_column, field_event_set_layer_back_color,
    /* 40 */ field_event_set_scroll_byte, field_event_set_in_gear, field_event_clear_in_gear, field_event_force_party_position,
    /* 44 */ field_event_release_party_position, field_event_set_idle_animation, field_event_set_layer_driven, field_event_set_layer_turn_step,
    /* 48 */ field_event_set_orbit_angles, field_event_clear_link_actor, field_event_load_actor_block, field_event_apply_actor_block,
    /* 4C */ field_event_play_animation_complement, field_event_set_animation_complement, field_event_free_actor_block, field_event_clear_script_control0,
    /* 50 */ field_event_set_script_control0, field_event_clear_script_control1, field_event_set_script_control1, field_event_release_script_control,
    /* 54 */ field_event_take_script_control, field_event_open_menu0, field_event_open_menu1, field_event_open_menu2,
    /* 58 */ field_event_open_menu3, field_event_open_menu4, field_event_open_menu5, field_event_set_turn_step,
    /* 5C */ field_event_layer_model, field_event_play_sound_effect, field_event_set_sprite_blend, field_event_set_colors,
    /* 60 */ field_event_play_movie, field_event_wait_movie_start, field_event_set_voice_volume, field_event_set_voice_pan,
    /* 64 */ field_event_wait_sound_channels, field_event_play_sound_effect_pair, field_event_play_sound_effect_full, field_event_play_movie_window,
    /* 68 */ field_event_walk_player_to, field_event_store_character_sum, field_event_set_panorama_depth, field_event_set_character_78,
    /* 6C */ field_event_clear_pad_byte, field_event_copy_camera_to_scripted, field_event_set_camera_heading, field_event_set_sprite_angles,
    /* 70 */ field_event_set_piece_drift_mode, field_event_store_facing, field_event_store_turn_step, field_event_store_planar_distance,
    /* 74 */ field_event_debug_print, field_event_store_actor_facing, field_event_store_distance, field_event_tim,
    /* 78 */ field_event_rerun_78, field_event_rerun_79, field_event_rerun_7a, field_event_rerun_7b,
    /* 7C */ field_event_rerun_7c, field_event_rerun_7d, field_event_rerun_7e, field_event_wait_battle_request,
    /* 80 */ field_event_set_panorama, field_event_set_panorama_position, field_event_set_panorama_colors, field_event_end_field_mode,
    /* 84 */ field_event_request_battle_field, field_event_store_movie_frame, field_event_set_no_panorama, field_event_wait_menus_done,
    /* 88 */ field_event_set_emitter_path, field_event_place_emitter, field_event_set_listener, field_event_store_party_slot,
    /* 8C */ field_event_slide_voice_volume, field_event_set_effects_kept, field_event_set_visibility_margins, field_event_begin_effect_actor,
    /* 90 */ field_event_select_emitter_template, field_event_set_template_vectors, field_event_set_template_08, field_event_set_template_56,
    /* 94 */ field_event_set_template_5a, field_event_set_template_6a, field_event_start_effect, field_event_stop_effects,
    /* 98 */ field_event_set_emitter_range, field_event_set_menu_parameter, field_event_set_actor_colors, field_event_request_transition1,
    /* 9C */ field_event_request_transition2, field_event_request_transition3, field_event_set_clip, field_event_set_party_lock,
    /* A0 */ field_event_play_movie_sound, field_event_set_gear, field_event_wait_music_load, field_event_save_vram_column,
    /* A4 */ field_event_restore_gears, field_event_set_template_24, field_event_set_sprite_sequence, field_event_set_sprite_sequence2,
    /* A8 */ field_event_store_camera_target, field_event_store_camera_eye, field_event_set_camera_actor, field_event_add_gear_hp,
    /* AC */ field_event_take_gear_hp, field_event_store_party_hp, field_event_set_jump_mode, field_event_transform_vector,
    /* B0 */ field_event_sound_bank, field_event_build_status_panel, field_event_set_party_hp, field_event_set_party_ep,
    /* B4 */ field_event_store_party_ep, field_event_warp_gathering, field_event_set_controlled, field_event_set_battle_override,
    /* B8 */ field_event_set_battle_sounds, field_event_store_vehicle_place, field_event_set_vehicle_place, field_event_store_vehicle_flags,
    /* BC */ field_event_set_vehicle_flags, field_event_set_template_flag, field_event_enable_movie_overlay, field_event_open_menu_task,
    /* C0 */ field_event_store_bout_outcome, field_event_store_member_animation, field_event_begin_effect, field_event_hide_sprite_and_shadow,
    /* C4 */ field_event_hide_actor_sprite_and_shadow, field_event_attach_to_layer_node, field_event_join_party, field_event_store_gear,
    /* C8 */ field_event_set_template_pairs0, field_event_set_template_pairs4, field_event_layer, field_event_end_window_movie,
    /* CC */ field_event_wait_movie_mode, field_event_store_disc_number, field_event_set_tween_slots, field_event_change_map_menu,
    /* D0 */ field_event_copy_character, field_event_unlock_boost_and_skills, field_event_skip_2, field_event_scale_pair,
    /* D4 */ field_event_overlay_sprites, field_event_store_ferry_place, field_event_store_flight_place, field_event_set_flight_place,
    /* D8 */ field_event_set_depth_cue_off, field_event_set_dpad_table, field_event_open_menu6, field_event_restore_character,
    /* DC */ field_event_set_layer_row, field_event_screen_band, field_event_set_skill_record_flags, field_event_display_mode,
    /* E0 */ field_event_set_pause_disabled, field_event_copy_gear, field_event_soft_reset,
};

/* Party slot masks by event operand (& 3). */
s16 field_event_party_slot_masks[4] = {7, 1, 2, 4}; /* 800AEA2C */

/* Facings (0x8000 | angle) per direction: the direction table (turns and
 * faces), the second (camera-relative faces) and the third. */
s16 field_event_direction_table[8] = {0x8C00, 0x8E00, 0x8000, 0x8200, 0x8400, 0x8600, 0x8800, 0x8A00}; /* 800AEA34 */
s16 field_event_direction_table2[8] = {0x8C00, 0x8E00, 0x8000, 0x8200, 0x8400, 0x8600, 0x8800, 0x8A00}; /* 800AEA44 */
s16 field_event_direction_table3[8] = {0x8C00, 0x8400, 0x8800, 0x8000, 0x8A00, 0x8E00, 0x8600, 0x8200}; /* 800AEA54 */

/* Octant turn table [from * 8 + to]: the signed octants to turn. */
s16 field_camera_octant_turn_table[64] = { /* 800AEA64 */
    0, 1, 2, 3, 4, -3, -2, -1,
    -1, 0, 1, 2, 3, 4, -3, -2,
    -2, -1, 0, 1, 2, 3, 4, -3,
    -3, -2, -1, 0, 1, 2, 3, 4,
    4, -3, -2, -1, 0, 1, 2, 3,
    3, 4, -3, -2, -1, 0, 1, 2,
    2, 3, 4, -3, -2, -1, 0, 1,
    1, 2, 3, 4, -3, -2, -1, 0,
};

/* Portrait VRAM places per slot and image (x, y, palette x, y); 8009c154
 * uses the first three slots. */
PortraitPlace field_dialogue_portrait_places[4][2] = { /* 800AEAE4 */
    {{0x2C0, 0x100, 0, 0xE1}, {0x2E0, 0x100, 0, 0xE0}},
    {{0x2C0, 0x140, 0, 0xE3}, {0x2E0, 0x140, 0, 0xE2}},
    {{0x2C0, 0x180, 0, 0xE5}, {0x2E0, 0x180, 0, 0xE4}},
    {{0x2C0, 0x1C0, 0, 0xE7}, {0x2E0, 0x1C0, 0, 0xE6}},
};
