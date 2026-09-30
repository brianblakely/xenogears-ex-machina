#include "common.h"
#include "sound.h"

extern void func_80039FF8(void);
extern s32 D_80059404;
extern void func_8003B644(s16 id, s32 channel, s16 volume, s16 pan);

void func_80039E18(s32 channel) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(0x600C, channel, 0x6000, 0x4000);
    }
}

extern s32 func_8003A65C(s32 channel, s32 b);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_80039F18);

void func_80039F9C(s32 channel, s32 sound, s32 volume, s32 pan) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(((sound & 0xFE) ^ 8) | 0x2000, channel, volume << 8, pan << 8);
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_80039FF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A094);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A14C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A20C);

void func_8003A2D4(void) {
}

void func_8003A2DC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A2E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A344);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A3B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A450);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A4FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A55C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A5D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A65C);

/* Whether a sequence is paused (flag bit 15). */
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
        seq->tick_step = seq->resolution * tempo;
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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003A89C);

extern void func_8003E680(s32 bits, SoundSeq *seq);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AA30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AAC4);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AC58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003ACC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AD20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AD98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003ADCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AE84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AF24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003AFFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B060);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B0AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B148);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B1FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B22C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B32C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B370);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B424);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B644);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B930);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B97C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003B9E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003BA38);

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

/* Nonmatching: the flag updates are scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CD08(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->unk5C = *data;
    channel->flags2 |= 2;
    channel->flags |= 0x400;
    return data + 1;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CD08);
#endif

/* Nonmatching: the flag update is scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CD30(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->flags |= 0x100;
    channel->unk5C = *data;
    return data + 1;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CD30);
#endif

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

/* Set the time signature.
 * Nonmatching: the stores are scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CE68(u8 *data, SoundSeq *seq) {
    u8 unit = data[1];
    u8 beats = data[0];

    seq->unk3A = 0xC0 / unit;
    seq->unk3C = unit;
    seq->unk38 = beats;
    seq->unk3E = beats;
    seq->unk36 = seq->unk3A;
    return data + 2;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CE68);
#endif

/* Nonmatching: the stores are scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CE9C(u8 *data, SoundSeq *seq) {
    seq->unk32 = data[0];
    seq->unk36 = seq->unk3A;
    seq->unk34 = data[1];
    return data + 2;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/sound", func_8003CE9C);
#endif

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
