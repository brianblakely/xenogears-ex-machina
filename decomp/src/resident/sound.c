#include "common.h"
#include "psyq/libapi.h"
#include "sound.h"

extern void func_80039FF8(void);
extern s32 D_80059404;
extern s32 D_80059478;           /* voice count of the effect channels */
extern void func_8003B644(s16 id, s32 channel, s16 volume, s16 pan);

void func_80039E18(s32 channel) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(0x600C, channel, 0x6000, 0x4000);
    }
}

extern u32 func_8003A65C(s32 id, s32 width);

void func_80039E60(s32 channel) {
    if (D_8005957C & 0x800) {
        s32 id = func_8003A65C(channel, 2);

        D_80059404 = 2;
        func_8003B644(id | 0x2000, channel, 0x6000, 0x4000);
    }
}

void func_80039EC4(s32 channel, s32 sound) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(((sound & 0xFE) ^ 8) | 0x2000, channel, 0x6000, 0x4000);
    }
}

/* Play the two-voice effect of `channel` on free effect voices, with a
 * volume and pan. */
void func_80039F18(s32 channel, s32 volume, s32 pan) {
    if (D_8005957C & 0x800) {
        s32 id = func_8003A65C(channel, 2);

        D_80059404 = 2;
        func_8003B644(id | 0x2000, channel, volume << 8, pan << 8);
    }
}

void func_80039F9C(s32 channel, s32 sound, s32 volume, s32 pan) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(((sound & 0xFE) ^ 8) | 0x2000, channel, volume << 8, pan << 8);
    }
}

extern s32 D_800595BC;           /* driver event */
extern SoundSeq *D_800595D8;     /* sound effect channels */
extern void func_8003E83C(SoundChannel *state, u32 voice);

/* Stop every sound effect channel and release its voice. */
void func_80039FF8(void) {
    s32 count = D_80059478;
    SoundSeq *effects = D_800595D8;
    SoundSeqChannel *channel = effects->channel;

    DisableEvent(D_800595BC);
    do {
        count--;
        if (channel->flags & 1) {
            channel->flags = 0;
            func_8003E83C(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
    effects->voices = 0;
    EnableEvent(D_800595BC);
}

/* Stop the effect channels playing effects of `bank`. */
void func_8003A094(SoundBank *bank) {
    s32 count = D_80059478;
    SoundSeq *effects = D_800595D8;
    s16 id = bank->id;
    SoundSeqChannel *channel = effects->channel;

    do {
        count--;
        if ((channel->flags & 1) && channel->id.part.bank == id) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            func_8003E83C(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
}

/* Stop the effect channels playing effect `id`. */
void func_8003A14C(s32 id) {
    SoundSeq *effects = D_800595D8;
    s32 count = D_80059478;
    SoundSeqChannel *channel = effects->channel;

    do {
        count--;
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            func_8003E83C(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
}

/* Stop the two effect channels of `sound`.
 * Nonmatching: the original computes the channel index before the loop
 * constants. */
#ifdef NON_MATCHING
void func_8003A20C(s32 sound) {
    SoundSeq *effects = D_800595D8;
    SoundSeqChannel *channel = &effects->channel[(sound & 0xFE) ^ 8];
    s32 count = 2;

    do {
        count--;
        if (channel->flags & 1) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            func_8003E83C(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A20C);
#endif

void func_8003A2D4(void) {
}

void func_8003A2DC(void) {
}

/* Set the volume of the effect channels playing effect `id`. */
void func_8003A2E4(s32 id, s32 volume) {
    s32 count = D_80059478;
    SoundSeqChannel *channel = D_800595D8->channel;

    do {
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->volume = volume << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}

/* Set the volume of the two effect channels of `sound`.
 * Nonmatching: the original computes the channel index before the loop
 * constants. */
#ifdef NON_MATCHING
void func_8003A344(s32 sound, s32 volume) {
    SoundSeqChannel *channel = &D_800595D8->channel[(sound & 0xFE) ^ 8];
    s32 count = 2;

    do {
        if (channel->flags & 1) {
            channel->volume = volume << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A344);
#endif

/* Slide the volume of the effect channels playing effect `id` over
 * `frames` (at least one). */
void func_8003A3B8(s32 id, s32 volume, s32 frames) {
    s32 count = D_80059478;
    SoundSeqChannel *channel = D_800595D8->channel;
    s32 delta;

    volume <<= 8;
    do {
        if ((channel->flags & 1) && channel->id.full == id) {
            delta = volume - channel->volume;
            if (delta != 0) {
                if (frames == 0) {
                    frames = 1;
                }
                channel->volume_target = volume;
                channel->volume_frames = frames;
                channel->volume_step = delta / frames;
                channel->flags3 |= 0x20;
            }
        }
        channel++;
        count--;
    } while (count != 0);
}

/* Slide the volume of the two effect channels of `sound`.
 * Nonmatching: the original computes the channel index before the loop
 * constants. */
#ifdef NON_MATCHING
void func_8003A450(s32 sound, s32 volume, s32 frames) {
    SoundSeqChannel *channel = &D_800595D8->channel[(sound & 0xFE) ^ 8];
    s32 count = 2;
    s32 delta;

    volume <<= 8;
    do {
        if (channel->flags & 1) {
            delta = volume - channel->volume;
            if (delta != 0) {
                if (frames == 0) {
                    frames = 1;
                }
                channel->volume_target = volume;
                channel->volume_frames = frames;
                channel->volume_step = delta / frames;
                channel->flags3 |= 0x20;
            }
        }
        channel++;
        count--;
    } while (count != 0);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A450);
#endif

/* Set the pan of the effect channels playing effect `id`. */
void func_8003A4FC(s32 id, s32 pan) {
    s32 count = D_80059478;
    SoundSeqChannel *channel = D_800595D8->channel;

    do {
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->pan = pan << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}

/* Set the pan of the two effect channels of `sound`.
 * Nonmatching: the original computes the channel index before the loop
 * constants. */
#ifdef NON_MATCHING
void func_8003A55C(s32 sound, s32 pan) {
    SoundSeqChannel *channel = &D_800595D8->channel[(sound & 0xFE) ^ 8];
    s32 count = 2;

    do {
        if (channel->flags & 1) {
            channel->pan = pan << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A55C);
#endif

/* Mask of the active effect channels (playing effect `id`, or any for -1). */
s32 func_8003A5D0(s32 id) {
    s32 bit = 1;
    s32 count = D_80059478;
    SoundSeqChannel *channel = D_800595D8->channel;
    s32 mask = 0;

    if (id == -1) {
        do {
            if (channel->flags & 1) {
                mask |= bit;
            }
            channel++;
            count--;
            bit <<= 1;
        } while (count != 0);
    } else {
        do {
            count--;
            if ((channel->flags & 1) && channel->id.full == id) {
                mask |= bit;
            }
            channel++;
            bit <<= 1;
        } while (count != 0);
    }
    return mask;
}

extern s32 D_80059544;           /* voices kept for music */

/* Stop the effect channels playing effect `id`, then choose `width`
 * adjacent effect channels for it: the highest free group below the
 * reserved top pair, else the oldest channel of low priority seen. */
u32 func_8003A65C(s32 id, s32 width) {
    SoundSeq *effects = D_800595D8;
    s32 count = D_80059478;
    u32 reserved = 0;
    SoundSeqChannel *channel = effects->channel;
    u32 limit;
    u32 index;
    u32 mask;
    u32 used;
    u32 group;
    u32 oldest;
    u32 found;
    s32 span;

    do {
        count--;
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            func_8003E83C(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);

    span = width + 2;
    limit = effects->channels - D_80059544;
    index = D_80059478 - span;
    effects = D_800595D8;
    group = 0xFFFFFFFF >> (32 - width);
    mask = group << index;
    channel = &effects->channel[index];
    used = ~reserved & effects->voices;
    oldest = 0xFFFFFFFF;
    if (used & mask) {
        do {
            if (channel->stamp < oldest && channel->unk7 < 0x21) {
                oldest = channel->stamp;
                found = index;
            }
            mask >>= width;
            if (mask < group || index <= limit) {
                index = found;
                break;
            }
            channel -= width;
            index -= width;
        } while (used & mask);
    }
    return index;
}

/* Whether a sequence is playing (flag bit 15). */
u32 func_8003A82C(SoundSeq *seq) {
    return seq->flags >> 15;
}

/* Set a sequence's tempo (0 means 0x100), at once or over `frames`. */
void func_8003A838(SoundSeq *seq, s32 tempo, s32 frames) {
    s32 delta;

    if (tempo == 0) {
        tempo = 0x100;
    }
    seq->tempo_target = tempo;
    if (frames == 0) {
        seq->tick_step = seq->rate.part.whole * tempo;
        seq->tempo_frames = 0;
        seq->tempo = tempo << 16;
        return;
    }
    delta = (tempo << 16) - seq->tempo;
    if (delta != 0) {
        seq->tempo_frames = frames;
        seq->tempo_step = delta / frames;
    }
}

extern void func_8003E680(s32 bits, SoundSeq *seq);
extern void func_8003AA30(SoundSeq *seq);

/* Set a sequence's fade level, at once or over `frames`; raising the level
 * of a sequence a fade stopped resumes it. */
void func_8003A89C(SoundSeq *seq, s32 fade, s32 frames) {
    s32 delta;

    seq->fade_target = fade << 8;
    if (frames == 0) {
        seq->fade = fade << 24;
        seq->fade_frames = 0;
        func_8003E680(0x100, seq);
    } else {
        delta = (fade << 16) - (seq->fade >> 8);
        if (delta == 0) {
            return;
        }
        seq->fade_frames = frames;
        seq->fade_step = (delta / frames) << 8;
    }
    if ((seq->flags & 0x100) && fade != 0) {
        func_8003AA30(seq);
    }
}

/* Set a sequence's volume, at once or over `frames`. */
void func_8003A948(SoundSeq *seq, s32 volume, s32 frames) {
    s32 delta;

    seq->volume_target = volume << 8;
    if (frames == 0) {
        seq->volume = volume << 24;
        seq->volume_frames = 0;
        func_8003E680(0x200, seq);
        return;
    }
    delta = (volume << 16) - (seq->volume >> 8);
    if (delta != 0) {
        seq->volume_frames = frames;
        seq->volume_step = (delta / frames) << 8;
    }
}

/* Set a sequence's pan, at once or over `frames`. */
void func_8003A9BC(SoundSeq *seq, s32 pan, s32 frames) {
    s32 delta;

    seq->pan_target = pan << 8;
    if (frames == 0) {
        seq->pan = pan << 24;
        seq->pan_frames = 0;
        func_8003E680(0x100, seq);
        return;
    }
    delta = (pan << 16) - (seq->pan >> 8);
    if (delta != 0) {
        seq->pan_frames = frames;
        seq->pan_step = (delta / frames) << 8;
    }
}

extern void func_80038934(s32 type, s32 depth, s32 delay, s32 feedback);
extern void func_8003E6C0(SoundSeq *seq, s32 voices);
extern void func_8003AFA0(SoundSeq *seq);

/* Resume a sequence: apply its reverb (when the driver owns the reverb),
 * refresh all its voices and mark it playing. */
void func_8003AA30(SoundSeq *seq) {
    DisableEvent(D_800595BC);
    if (D_8005957C & 0x1000) {
        func_80038934(seq->reverb_type, seq->reverb_depth, seq->reverb_delay,
                      seq->reverb_feedback);
    }
    func_8003E6C0(seq, 0xFFFF);
    func_8003AFA0(seq);
    seq->flags = (seq->flags & ~0x100) | 0x8000;
    EnableEvent(D_800595BC);
}

extern void func_8003EFA0(SoundChannel *state, u32 voice);
extern void func_8003EF04(SoundChannel *state, u32 voice);

/* Mute the channels of a sequence whose bit is set in `mask` (keying their
 * voices off while it plays) and unmute the others (keying on the ones
 * still sounding). The unmute test reads the flag word as 32 bits. */
void func_8003AAC4(SoundSeq *seq, u32 mask) {
    SoundSeqChannel *channel;
    s32 count;

    if (seq == NULL) {
        return;
    }
    channel = seq->channel;
    count = seq->channels;
    seq->muted = mask;
    do {
        if (channel->flags != 0) {
            if (mask & 1) {
                if (!(channel->flags & 0x20)) {
                    channel->flags |= 0x20;
                    if ((s16)seq->flags & 0x8000) {
                        func_8003EFA0(&channel->state, channel->voice);
                    }
                }
            } else if (channel->flags & 0x20) {
                channel->flags &= ~0x20;
                if ((SEQ_CHANNEL_FLAGS32(channel) & 0x110) == 0x100 &&
                    ((s16)seq->flags & 0x8000)) {
                    func_8003EF04(&channel->state, channel->voice);
                }
            }
        }
        channel++;
        count--;
        mask >>= 1;
    } while (count != 0);
}

void func_8003ABE8(SoundSeq *seq, u8 value) {
    seq->unk1B = value;
}

/* A sequence's position: its first word, then frames, seconds and minutes
 * of its tick counter. */
void func_8003ABF0(SoundSeq *seq, SoundTime *time) {
    u32 ticks = seq->ticks >> 8;
    u32 seconds = ticks / 240;

    time->unk0 = seq->unk24;
    time->frames = ticks % 240;
    time->seconds = seconds % 60;
    time->minutes = seconds / 60;
}

/* Store (and return a pointer to) the lowest `unk20` of the active channels
 * of a sequence, 0 when none is active. */
u16 *func_8003AC58(SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    u16 count = seq->channels;
    u16 *out = &seq->unk30;
    u16 lowest = 0xFFFF;

    do {
        if (channel->flags != 0 && channel->unk20 < lowest) {
            lowest = channel->unk20;
        }
        channel++;
    } while (--count != 0);
    if (lowest == 0xFFFF) {
        lowest = 0;
    }
    *out = lowest;
    return out;
}

extern SoundSeq *D_80059564;     /* playing sequences */

/* The block at header offset `unk1E` of a playing sequence (the first one when
 * `seq` is NULL); NULL when it is not playing. */
u8 *func_8003ACC8(SoundSeq *seq) {
    SoundSeq *it = D_80059564;

    if (seq != NULL) {
        while (it != NULL) {
            if (it == seq) {
                break;
            }
            it = it->next;
        }
    }
    if (it == NULL) {
        return NULL;
    }
    return (u8 *)it->header + it->header->unk1E;
}

extern void func_8003AD98(SoundSeq *seq);
extern void func_8003ADCC(SoundSeq *seq);
extern void func_8003AE84(SoundSeq *seq);

/* Snapshot operations: 0 discards, 1 takes, 2 restores. */
void func_8003AD20(SoundSeq *seq, s32 op) {
    switch (op) {
    case 0:
        func_8003AD98(seq);
        break;
    case 1:
        func_8003ADCC(seq);
        break;
    case 2:
        func_8003AE84(seq);
        break;
    }
}

extern void func_8003B930(SoundSeq *seq);

/* Discard a sequence's snapshot. */
void func_8003AD98(SoundSeq *seq) {
    if (seq->flags & 0x10) {
        seq->flags &= ~0x10;
        func_8003B930(seq);
    }
}

extern void *func_80038F18(s32 size);
extern void func_80039248(void *dst, void *src, s32 size);
extern s32 func_8003BB40(s32 index);

/* Save a copy of a sequence to restart from (reusing an earlier one). The
 * original leaves the copy pointer unset when one already exists. */
void func_8003ADCC(SoundSeq *seq) {
    s32 size;
    SoundSeq *snapshot;

    DisableEvent(D_800595BC);
    size = func_8003BB40(seq->channels);
    if (seq->snapshot == NULL) {
        snapshot = func_80038F18(size);
    }
    if (snapshot == NULL) {
        EnableEvent(D_800595BC);
        return;
    }
    seq->snapshot = snapshot;
    seq->flags |= 0x10;
    func_80039248(snapshot, seq, size);
    snapshot->next = NULL;
    snapshot->snapshot = NULL;
    seq->unk2C = 0;
    EnableEvent(D_800595BC);
}

extern void func_8003B060(SoundSeq *seq);
extern void func_8003B97C(SoundSeq *seq, SoundSeq *snapshot);

/* Restart a sequence from its snapshot. */
void func_8003AE84(SoundSeq *seq) {
    s32 position;

    if (seq->snapshot != NULL && (seq->flags & 0x10)) {
        DisableEvent(D_800595BC);
        func_8003B060(seq);
        position = seq->unk24;
        func_8003B97C(seq, seq->snapshot);
        seq->unk2C = position;
        func_8003E6C0(seq, 0xFFFF);
        func_8003AFA0(seq);
        EnableEvent(D_800595BC);
    }
}

/* Stop a sequence's voices and reset its tempo slide to 0x7F00, keeping
 * `value` in `unk1E`. */
void func_8003AF24(SoundSeq *seq, u16 value) {
    s32 tempo = 0x7F00;

    seq->unk1E = value;
    seq->flags |= 0x20;
    DisableEvent(D_800595BC);
    func_8003B060(seq);
    EnableEvent(D_800595BC);
    seq->tempo_target = tempo;
    seq->tempo_frames = 0;
    seq->tempo = tempo << 16;
    seq->tick_step = seq->rate.part.whole * tempo;
}

/* Request key-on for the unmuted channels that hold a sounding note. */
void func_8003AFA0(SoundSeq *seq) {
    s32 count = seq->channels;
    SoundSeqChannel *channel = seq->channel;

    do {
        count--;
        if ((SEQ_CHANNEL_FLAGS32(channel) & 0x101) == 0x101 && !(channel->flags & 0x30)) {
            channel->flags2 |= 1;
        }
        channel++;
    } while (count != 0);
}

extern void func_8003E8A4(SoundChannel *state, u32 voice);

/* Apply 8003e8a4 to the voice of every active channel of a sequence. */
void func_8003AFFC(SoundSeq *seq) {
    s32 count = seq->channels;
    SoundSeqChannel *channel = seq->channel;

    do {
        if (channel->flags != 0) {
            func_8003E8A4(&channel->state, channel->voice);
        }
        channel++;
        count--;
    } while (count != 0);
}

/* Release the voices of every channel of a sequence. */
void func_8003B060(SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    s32 count = seq->channels;

    do {
        func_8003E83C(&channel->state, channel->voice);
        channel++;
        count--;
    } while (count != 0);
}

/* Fill a sequence's table (placed after its channels) from the 5-byte
 * (index, little-endian word) entries of its header. */
void func_8003B0AC(SoundSeq *seq, SoundSeqHeader *header) {
    u32 *table = (u32 *)((u8 *)seq + func_8003BB40(header->channels));
    s32 count;
    u8 *entry;

    seq->table = table;
    count = header->entries;
    entry = (u8 *)header + header->table;
    do {
        u32 value = entry[1] | (entry[2] << 8) | (entry[3] << 16) | (entry[4] << 24);
        u32 *slot = &table[entry[0]];

        *slot = value;
        entry += 5;
        count--;
    } while (count != 0);
}

extern void func_8003F6B0(s32 error);
extern void func_8003B32C(SoundSeq *seq);
extern void func_8003B9E4(SoundSeq *seq);

/* Create the sound effect channel set with `count` (made even) channels on
 * the top hardware voices. */
SoundSeq *func_8003B148(s32 count) {
    SoundSeq *effects;
    SoundSeqChannel *channel;
    s32 i;
    s32 index;
    s32 voice;

    count &= ~1;
    D_80059478 = count;
    effects = func_80038F18(func_8003BB40(count));
    if (effects == NULL) {
        func_8003F6B0(0x1E);
        return NULL;
    }
    func_8003B32C(effects);
    channel = effects->channel;
    voice = 24 - count;
    i = count;
    index = 0;
    do {
        channel->flags = 0;
        channel->voice_bit = index++;
        channel->voice = voice;
        channel++;
        i--;
        voice++;
    } while (i != 0);
    func_8003B9E4(effects);
    return effects;
}

extern s32 func_8003BA38(SoundSeq *seq);
extern void func_80039144(void *block);

void func_8003B1FC(SoundSeq *seq) {
    func_8003BA38(seq);
    func_80039144(seq);
}

extern s16 func_8003F67C(SoundSeqHeader *header);
extern void func_8003B370(SoundSeq *seq);

/* Read a sequence's header: channel count, reverb settings (applied when
 * the driver owns the reverb), then reset its playback state. */
void func_8003B22C(SoundSeq *seq) {
    SoundSeqHeader *header = seq->header;

    if (func_8003F67C(header) != 0) {
        func_8003F6B0(0xA);
        return;
    }
    seq->flags |= 1;
    seq->unk12 = header->unk10;
    seq->channels = header->channels;
    seq->unk16 = header->unk16;
    seq->unk18 = header->unk18;
    seq->reverb_type = header->reverb_type;
    seq->reverb_depth = header->reverb_depth << 8;
    seq->reverb_delay = header->reverb_delay;
    seq->reverb_feedback = header->reverb_feedback;
    if (D_8005957C & 0x1000) {
        func_80038934((s8)seq->reverb_type, seq->reverb_depth, seq->reverb_delay,
                      seq->reverb_feedback);
    }
    func_8003B370(seq);
}

/* Initialise the sound effect channel set. */
void func_8003B32C(SoundSeq *seq) {
    seq->flags = 2;
    seq->unk12 = 0x7FFF;
    seq->unk16 = 0;
    seq->unk18 = 0x7F;
    seq->channels = D_80059478;
    func_8003B370(seq);
}

/* Reset a sequence's playback state: 4/4 time, tempo 1, rate 0x66, full
 * fade level, no snapshot.
 * Nonmatching: the original keeps the constant 4 in its own register. */
#ifdef NON_MATCHING
void func_8003B370(SoundSeq *seq) {
    func_8003B930(seq);
    seq->unk32 = 1;
    seq->unk36 = 1;
    seq->unk3A = 0x30;
    seq->tempo = 0x1000000;
    seq->fade = 0x7F000000;
    seq->rate.value = 0x660000;
    seq->tick_step = 0x6600;
    seq->unk1A = 0;
    seq->unk1B = 0;
    seq->unk30 = 0;
    seq->unk34 = 0;
    seq->unk38 = 4;
    seq->unk3C = 4;
    seq->unk3E = 4;
    seq->ticks = 0;
    seq->unk24 = 0;
    seq->unk20 = 0;
    seq->voices = 0;
    seq->volume = 0;
    seq->pan = 0;
    seq->tempo_frames = 0;
    seq->fade_frames = 0;
    seq->volume_frames = 0;
    seq->pan_frames = 0;
    seq->rate_step = 0;
    seq->rate_frames = 0;
    seq->unk50 = 0x10000;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B370);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B424);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B644);

/* Free a sequence's snapshot chain. */
void func_8003B930(SoundSeq *seq) {
    SoundSeq *snapshot = seq->snapshot;

    if (snapshot != NULL) {
        seq->snapshot = NULL;
        do {
            seq = snapshot->snapshot;
            func_80039144(snapshot);
            snapshot = seq;
        } while (seq != NULL);
    }
}

/* Copy a snapshot over a sequence, keeping its list and snapshot links. */
void func_8003B97C(SoundSeq *seq, SoundSeq *snapshot) {
    SoundSeq *next = seq->next;
    SoundSeq *saved = seq->snapshot;

    func_80039248(seq, snapshot, func_8003BB40(seq->channels));
    seq->next = next;
    seq->snapshot = saved;
}

/* Link a sequence at the head of the playing list. */
void func_8003B9E4(SoundSeq *seq) {
    DisableEvent(D_800595BC);
    seq->next = D_80059564;
    D_80059564 = seq;
    EnableEvent(D_800595BC);
}

/* Unlink a sequence from the playing list, stopping it first; -1 when it
 * is not listed. */
s32 func_8003BA38(SoundSeq *seq) {
    SoundSeq *it = D_80059564;
    SoundSeq *prev = NULL;

    while (it != NULL && it != seq) {
        prev = it;
        it = it->next;
    }
    if (it == NULL) {
        func_8003F6B0(0xF);
        return -1;
    }
    if ((s16)seq->flags & 0x8000) {
        if (seq == NULL) {
            func_8003F6B0(5);
        } else {
            seq->flags &= 0x7FFF;
            func_8003B060(seq);
        }
    }
    if (prev != NULL) {
        prev->next = seq->next;
    } else {
        D_80059564 = seq->next;
    }
    return 0;
}

/* Set `bits` in every active channel of a sequence. */
void func_8003BB08(s32 bits, SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    s32 count = seq->channels;

    do {
        count--;
        if (channel->flags != 0) {
            channel->flags = bits | channel->flags;
        }
        channel++;
    } while (count != 0);
}

/* Byte offset of channel `index` in a sequence. */
s32 func_8003BB40(s32 index) {
    return index * sizeof(SoundSeqChannel) + 0x94;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003BB64);

extern void func_8003BCA0(s32 a, s32 b, s32 c, s32 d, s32 mode);

/* 8003bca0 with modes 1-4, passing the other arguments through. */
void func_8003BC10(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 1);
}

void func_8003BC34(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 2);
}

void func_8003BC58(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 3);
}

void func_8003BC7C(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 4);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003BCA0);

extern u16 D_800594F4; /* command ring write index */
extern u16 D_80059510; /* command ring read index */

/* Whether the eight-entry command ring has at least six entries queued. */
s32 func_8003BDBC(void) {
    u16 write = D_800594F4;

    if (write < D_80059510) {
        write += 8;
    }
    return write - D_80059510 >= 6;
}

void func_8003BDF4(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003BDFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003BE68);

extern void (*D_8005950C)(void);
extern s32 D_80059514;

/* Driver tick: count it and run the tick callback, flagged busy. */
void func_8003BFA0(void) {
    D_8005957C |= 4;
    D_80059514++;
    if (D_8005950C != NULL) {
        D_8005950C();
    }
    D_8005957C &= ~4;
}

void func_8003C010(void (*callback)(void)) {
    D_8005950C = callback;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003C020);

/* Step a linear slide; on its last frame land exactly on the target. */
void func_8003C484(SoundSlide *slide) {
    if (--slide->frames != 0) {
        slide->value += slide->step;
    } else {
        slide->value = slide->target << 16;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003C4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003C6E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CC84);

/* Sequence opcode handlers: each takes the opcode's operands, the sequence
 * and the channel, and returns the position after the operands. */

/* No operands, no effect. */
u8 *func_8003CD00(u8 *data) {
    return data;
}

/* Set `unk5C` from the operand and flag it for update. */
u8 *func_8003CD08(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->unk5C = *data++;
    channel->flags |= 0x400;
    channel->flags2 |= 2;
    return data;
}

u8 *func_8003CD30(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->flags |= 0x100;
    channel->unk5C = value;
    return data;
}

u8 *func_8003CD4C(u8 *data) {
    return data;
}

/* Mark the loop point when the operand matches the sequence's selector. */
u8 *func_8003CD54(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if (*data++ == seq->unk1B) {
        channel->loop = data;
        channel->unk23 = channel->transpose;
    }
    return data;
}

/* Skip three operand bytes. */
u8 *func_8003CD7C(u8 *data) {
    return data + 3;
}

u8 *func_8003CD84(u8 *data) {
    return data;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CD8C);

/* Mark the loop point. */
u8 *func_8003CE04(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->loop = data;
    channel->unk23 = channel->transpose;
    return data;
}

/* Set the octave. */
u8 *func_8003CE18(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose = *data * 12;
    return data + 1;
}

/* Octave up. */
u8 *func_8003CE38(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose += 12;
    return data;
}

/* Octave down. */
u8 *func_8003CE50(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose -= 12;
    return data;
}

/* Set the time signature. */
u8 *func_8003CE68(u8 *data, SoundSeq *seq) {
    s16 beats = data[0];
    s16 unit = data[1];

    seq->unk3A = 0xC0 / unit;
    seq->unk3C = unit;
    seq->unk38 = beats;
    seq->unk3E = beats;
    seq->unk36 = seq->unk3A;
    return data + 2;
}

u8 *func_8003CE9C(u8 *data, SoundSeq *seq) {
    u16 length;

    seq->unk32 = data[0];
    length = seq->unk3A;
    seq->unk34 = data[1];
    seq->unk36 = length;
    return data + 2;
}

u8 *func_8003CEC0(u8 *data, SoundSeq *seq) {
    seq->unk1A = *data;
    return data + 1;
}

u8 *func_8003CED4(u8 *data, SoundSeq *seq) {
    seq->unk1A += *data;
    return data + 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CEF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CFA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CFF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D034);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D070);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D0E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D110);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D13C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D17C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D1BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D208);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D21C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D298);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D2D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D300);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D328);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D340);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D358);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D370);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D3A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D3D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D438);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D4A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D4E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D53C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D59C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D5C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D5CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D5D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D60C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D640);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D65C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D678);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D694);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D6B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D6D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D6F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D714);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D74C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D770);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D79C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D7C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D7FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D854);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D86C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D884);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D8B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003D9A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DAB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DAEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DB0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DB2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DB58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DB98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DBE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DC50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DD24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DE18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DE54);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DE74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DE94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DEB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DEE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DF3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003DF78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E04C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E140);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E160);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E1F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E290);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E358);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E360);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E3E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E40C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E44C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E4BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E4F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E54C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E680);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E6C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E724);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E7E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E83C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E8A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003E900);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003EB5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003EBF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003EEA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003EF04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003EFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003EFE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F190);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F1A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F1EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F240);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F2A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F354);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F3C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F42C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F43C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F468);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F4A0);

void func_8003F4BC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F4E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F4FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F518);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F560);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F588);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F5EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F614);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F67C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F684);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003F6B0);
