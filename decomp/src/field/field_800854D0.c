/* Field unit 800854D0 onward: music and sound effects, the event
 * interpreter's operations, event actors' motion, screen effects, movies,
 * texture panels, particles and the text glyphs.
 *
 * Its rodata starts at 0x198 with 8008e59c's jump table (0 mod 8); the text
 * start (after 80084a40, at or before 8008e59c) is chosen with the previous
 * unit (see field_8007A44C.c). The tables stay 0 mod 8 through 800a1bd0's
 * (0x268); 800a5c40's (0x2bc) is 4 mod 8 and 800ab748's (0x2e8) 0 mod 8
 * again, so further units start after 800a1bd0 (at or before 800a5c40) and
 * after 800a5c40 (at or before 800ab748). */
#include "common.h"
#include "field.h"
#include "field_gte.h"
#include "field_motion.h"
#include "field_music.h"

/* The movie sound timelines: one run per movie sound-effect bank, each of
 * (time, sound) entries ended by 0xffff; 80085788 skips bank + 1 ends. A
 * sound holds its id in the low byte and its voice pair in bits 8-10. */
u16 D_800AE060[96][2] = {
    {0xFFFF, 0},
    {1, 0x301}, {41, 0xB}, {85, 0x202}, {152, 0x203}, {154, 0xC}, {275, 0x204},
    {284, 0x105}, {292, 0x206}, {299, 0x107}, {345, 0x208}, {370, 0xD}, {421, 0xE},
    {518, 0xF}, {638, 0x10}, {677, 0x11}, {735, 0x12}, {760, 0x209}, {787, 0x10A},
    {0xFFFF, 0},
    {0, 0x301}, {0xFFFF, 0},
    {0, 0x301}, {0, 0x202}, {0, 0x103}, {60, 0x11}, {84, 0x304}, {84, 0x205}, {84, 0x106},
    {118, 0x30A}, {118, 0x20B}, {118, 0x10C}, {142, 0x10}, {320, 0x307}, {320, 0x208},
    {320, 0x109}, {335, 0x30D}, {335, 0x20E}, {335, 0x10F}, {0xFFFF, 0},
    {1, 0x301}, {1, 0x202}, {1, 0x103}, {1, 0x4}, {1, 0x405}, {360, 0x306}, {360, 0x207},
    {360, 0x108}, {360, 0x9}, {360, 0x40A}, {360, 0x50B}, {360, 0x60C}, {360, 0x70D},
    {0xFFFF, 0},
    {1, 0x301}, {1, 0x202}, {1, 0x103}, {1, 0x4}, {1, 0x405}, {1, 0x506}, {1, 0x607},
    {1, 0x708}, {0xFFFF, 0},
    {1, 0x301}, {1, 0x202}, {1, 0x103}, {1, 0x4}, {1, 0x405}, {1, 0x506}, {1, 0x607},
    {1, 0x708}, {0xFFFF, 0},
    {1, 0x301}, {1, 0x202}, {1, 0x103}, {1, 0x4}, {0xFFFF, 0},
    {1, 0x301}, {1, 0x202}, {1, 0x103}, {1, 0x4}, {1, 0x405}, {1, 0x506}, {1, 0x607},
    {1, 0x708}, {0xFFFF, 0},
    {1, 0x301}, {0xFFFF, 0},
    {1, 0x301}, {1, 0x202}, {1, 0x103}, {1, 0x4}, {1, 0x405}, {1, 0x506}, {1, 0x607},
    {0xFFFF, 0},
};

/* Portrait files per character (- 0x46): first and second image. */
u8 D_800AE1E0[90][2] = {
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
u8 D_800AE294[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 2, 6};

/* One music-wave stream step: pass arrivals to the chunk callback; -1 once
 * the stream finished and its ring is released. */
s32 func_800854D0(void) {
    s32 arrived = (s32)func_80028B14();

    D_800ADBBC = arrived;
    if (arrived != 0) {
        D_800AFEA4(arrived);
        return 0;
    }
    if (func_800286CC() != 0) {
        return 0;
    }
    if (D_800ADBBC != 0) {
        return 0;
    }
    func_800320E8(D_800ADBB8);
    D_800ADB2C = 0;
    return -1;
}

/* Start streaming music-wave `file` into an eight-sector ring with a chunk
 * callback. */
void func_80085560(s32 file, s32 unused, void (*callback)(s32)) {
    void *ring;

    D_800ADB2C = 1;
    D_800ADBB8 = ring = func_8002A260(8, unused);
    func_800295D8(file, ring, 0, 0x100);
    D_800AFEA4 = callback;
}

/* Play sound effect `id` on voice pair `channel` at a volume and pan. */
void func_800855C8(s32 id, s32 volume, s32 pan, s32 channel) {
    channel &= 7;
    func_8003A20C(channel * 2);
    func_80039F9C(id, channel * 2, volume, pan);
}

/* Play sound effect `id` on `channel` at full volume and centre pan; id 0
 * stops the channel. */
void func_80085634(s32 id, s32 channel) {
    channel &= 7;
    if (id == 0) {
        func_8003A20C(channel * 2);
    } else {
        D_800B2078.last_sound_effect = id;
        func_800855C8(id, 0x7F, 0x40, channel);
    }
}

/* Play the movie sound effects whose time (from 800c3a2c) has come: each
 * timeline entry holds a time and a sound (id in the low byte, voice pair
 * in bits 8-10). */
void func_80085678(void) {
    u16 *times;
    u16 *sounds;
    s32 sound;

    if (FIELD_MOVIE.sound_bank == 0xFF) {
        return;
    }
    times = &D_800AE060[0][0];
    sounds = &D_800AE060[0][1];
    for (;;) {
        if (D_800B06A0 < times[D_800C3A64 * 2] + FIELD_MOVIE.sound_start) {
            return;
        }
        sound = sounds[D_800C3A64 * 2];
        func_80039EC4((sound & 0xFF) | (D_800B235C->id << 16), ((sound >> 8) & 7) * 2);
        D_800C3A64++;
    }
}

/* Release a movie's sound-effect bank, when one is loaded. */
void func_80085738(void) {
    if (FIELD_MOVIE.sound_bank != 0xFF) {
        func_80039FF8();
        func_8003852C(D_800B235C);
        func_800320E8(D_800B235C);
    }
}

/* Load a movie's sound-effect bank (file 0x115 + bank) and seek the movie
 * sound timeline past the bank's 0xffff-terminated runs.
 * The value is read into `file` and copied to `bank` (the original keeps
 * the loaded copy in s0 for the file number), and the timeline is indexed
 * as a flat halfword table, which leaves bank + 1 in the loop test.
 */
void func_80085788(void) {
    s32 bank;
    s32 file;
    s32 pos;
    s32 i;

    file = FIELD_MOVIE.sound_bank;
    if (file != 0xFF) {
        bank = file;
        func_80039FF8();
        func_80028470(0x1C, 0);
        file = bank + 0x115;
        D_800B235C = func_80031BDC(func_800288EC(file), 1);
        func_800295D8(file, D_800B235C, 0, 0x80);
        func_80028A60(0);
        func_80038428(D_800B235C);
        func_8003BDFC(0x10);
        func_80028470(4, 0);
        pos = 0;
        for (i = 0; i < bank + 1; i++) {
            while (1) {
                if (((u16 *)D_800AE060)[pos * 2] == 0xFFFF) {
                    break;
                }
                pos++;
            }
            pos++;
            D_800C3A64 = pos;
        }
    }
}

/* Load the field's sound-effect bank (file 0xa8), from the disc or from the
 * copy at 8005a4bc, and open it. */
void func_80085890(void) {
    s32 size;

    func_80028470(4, 0);
    size = func_800288EC(0xA8);
    D_8006259C = func_80031BDC(size, 0);
    func_800320A4(D_8006259C);
    if (D_8004F32C == -1) {
        func_800295D8(0xA8, D_8006259C, 0, 0x80);
        func_80028A60(0);
    } else {
        memcpy(D_8006259C, D_8005A4BC, size);
        func_800320B8(D_8005A4BC);
        func_800320E8(D_8005A4BC);
    }
    func_80038428(D_8006259C);
    func_8003BDFC(0x10);
    func_80028470(4, 0);
    D_8004F32C = -1;
}

/* Unlink and release the field's sound-effect bank. */
void func_80085988(void) {
    func_8003852C(D_8006259C);
    func_800320B8(D_8006259C);
    func_800320E8(D_8006259C);
    D_8004F32C = -1;
}

/* Music-wave chunk callback: gather four 2 KiB chunks and open them as a
 * wave bank; later chunks feed the bank. */
void func_800859DC(WaveChunk *chunk) {
    switch (D_800B2370) {
    case 0:
    case 1:
    case 2:
    case 3:
        ((WaveChunk *)D_800C3A1C)[D_800B2370] = *chunk;
        D_800B2370++;
        func_8002945C((u8 *)chunk);
        if (D_800B2370 == 4) {
            D_8006258C = func_800380D0(D_800C3A1C, 0x2000, 0);
        }
        break;
    case 4:
        func_8003BDFC(0x10);
        *(WaveChunk *)D_800C3A1C = *chunk;
        func_8003827C(D_800C3A1C, 0x800);
        func_8002945C((u8 *)chunk);
        break;
    }
}

/* Change the field music to `music` (0xff: none): release the shared wave
 * bank when the entry asks, then stream its wave file through 800859dc. */
void func_80085B20(s32 music, s32 unused) {
    u8 wave;
    s32 file;

    func_80028A60(0);
    func_8001B66C();
    if (music == 0xFF) {
        D_8004F308 = 0;
        return;
    }
    func_80028470(0x1C, 0);
    if (D_800ADFCC[music * 2 + 1] == 1) {
        func_80086024();
    }
    wave = D_800ADFCC[music * 2];
    if (wave != 0xFF) {
        file = wave * 2 + 0x13;
        if (D_8004F33C != wave) {
            func_80085560(file, 1, (void (*)(s32))func_800859DC);
            D_8004F354 = 1;
            D_800B2370 = 0;
            D_800C3A1C = func_80031BDC(0x2000, 1);
        }
    }
    func_80028470(4, 0);
    D_8004F308 = -1;
    D_800AFC54 = 1;
}

/* Run up to five stream steps; 0 once the stream finished, else -1. */
s32 func_80085C3C(void) {
    s32 steps;

    for (steps = 0; steps < 5; steps++) {
        if (func_800854D0() == -1) {
            return 0;
        }
    }
    return -1;
}

/* Advance the load of music `music`: its wave chunks, the shared wave bank,
 * the deferred sequence read and the sequence start; 0 once complete, else
 * -1. */
s32 func_80085C90(s32 music) {
    s32 sequence;

    if (D_8004F354 == 1) {
        if (func_80085C3C() == -1) {
            return -1;
        }
        func_8003BDFC(0x10);
        func_800320E8(D_800C3A1C);
        D_8004F354 = 0;
        D_8004F360 = 1;
        D_8004F33C = D_800ADFCC[music * 2];
    }
    if (D_800ADFCC[music * 2 + 1] == 0) {
        if (D_8004F364 == 0) {
            func_80085FB8();
            return -1;
        }
        if ((D_8004F364 & 0x80) && func_80085F30() == -1) {
            return -1;
        }
    }
    if (D_800AFC54 == 1) {
        if (D_8004F338 != music) {
            func_80028470(0x1C, 0);
            func_800295D8(music * 2 + 0x14, D_80062648, 0, 0x80);
            D_8004F358 = 1;
            func_80028470(4, 0);
        }
        D_800AFC54 = 0;
        return -1;
    }
    if (func_800286CC() != 0) {
        return -1;
    }
    if (D_8004F358 == 1) {
        if (D_8004F348 == 0) {
            sequence = (s32)func_80039850((SoundSeqHeader *)D_80062648);
            D_80062528 = sequence;
            if (D_8004F340 == -1) {
                func_80039A80((SoundSeq *)sequence, 0x7F, 0);
            } else {
                func_80039A80((SoundSeq *)D_80062528, 0, 0);
                func_8003A89C((SoundSeq *)D_80062528, 0, 0);
            }
        } else {
            D_80062528 = D_8004F2FC;
            func_80039B68((SoundSeq *)D_8004F2FC, 0x7F, 0xF0);
            D_8004F348 = 0;
            D_8004F2FC = 0;
        }
        D_8004F358 = 0;
        D_8004F35C = 1;
        D_8004F338 = music;
    }
    D_8004F340 = -1;
    D_8004F36C = 1;
    return 0;
}

/* Stop and release the cached sequence. */
void func_80085EEC(void) {
    if (D_8004F2FC != 0) {
        func_80039C4C((SoundTrack *)D_8004F2FC);
        func_800399D4((SoundSeq *)D_8004F2FC);
        D_8004F2FC = 0;
    }
}

/* Once the disc is idle, open the shared wave bank from its buffer and
 * release the buffer; -1 while still reading. */
s32 func_80085F30(void) {
    s32 bank;

    if (func_800286CC() != 0) {
        return -1;
    }
    bank = (s32)func_80037FD8(D_800B00E0, 0);
    D_8006251C = bank;
    D_80059560 = (SoundSequence *)bank;
    func_8003BDFC(0x10);
    func_800320E8(D_800B00E0);
    D_8004F364 = 1;
    D_8004F384 = 0;
    D_8004F368 = 0;
    return 0;
}

/* Start reading the shared wave bank (file 3 of directory 0x1c). */
void func_80085FB8(void) {
    void *buffer;

    func_80028470(0x1C, 0);
    D_800B00E0 = buffer = func_80031BDC(func_800288EC(3), 1);
    func_800295D8(3, buffer, 0, 0x80);
    func_80028470(4, 0);
    D_8004F364 = 0x80;
}

/* Release the shared wave bank once and mark it unloaded. */
void func_80086024(void) {
    if (D_8004F368 == 0) {
        D_8004F384 = 1;
        func_80038310((SoundSequence *)D_8006251C);
        D_8004F368 = 1;
    }
    D_8004F364 = 0;
}

/* A positional emitter's volume at `distance`: full at the source, falling
 * linearly to half at the range 800b21ac. */
void func_80086078(s32 distance, u32 *out, s32 volume) {
    s32 level;

    if (distance > D_800B2078.emitter_range) {
        distance = D_800B2078.emitter_range;
    }
    level = 0x80 - (((0x7F0000 / D_800B2078.emitter_range) * distance) >> 16);
    *out = ((u32)(level << 16) / 127 * volume) >> 16;
}

/* Update the voices of the emitter following descriptor `id`: volume by
 * distance, pan by the descriptor's screen X. */
void func_800860F0(s32 unused0, s32 volume, s32 unused2, s32 distance, s32 id) {
    s32 i;
    s32 voice;
    s32 pan;
    u32 level;
    s32 x;
    s32 y;

    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].actor == id) {
            voice = i * 2;
            func_80086078(distance, &level, volume);
            func_80086200(id, &x, &y);
            if (x > 0x140) {
                x = 0x13F;
            }
            if (x < 0) {
                x = 0;
            }
            pan = (x * 0x6666) >> 16;
            func_8003A344(voice, level);
            func_8003A55C(voice, pan);
        }
    }
}

/* The screen position of descriptor `index`. */
void func_80086200(s32 index, s32 *x, s32 *y) {
    SVECTOR point;
    MATRIX m;
    s32 screen;
    s32 depth;
    s32 flag;

    /* The selector call (result unused) sits inside the argument list. */
    CompMatrix(&D_800AF880.scaled_world, (func_8009CDB4(1), &D_800AF880.components.descriptors[index].matrix),
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

/* Start `sound` on the first free emitter, following descriptor `actor`,
 * with volume by distance and pan by screen X. */
void func_800862CC(s32 sound, s32 volume, s32 unused, s32 distance, s32 actor) {
    s32 i;
    u32 level;
    s32 x;
    s32 y;
    s32 pan;

    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].sound == 0xFFFF) {
            D_800AFE88[i].sound = sound;
            D_800AFE88[i].actor = actor;
            func_80086078(distance, &level, volume);
            func_80086200(actor, &x, &y);
            if (x > 0x140) {
                x = 0x13F;
            }
            if (x < 0) {
                x = 0;
            }
            pan = (x * 0x6666) >> 16;
            func_8003A20C(i * 2);
            func_80039F9C(sound, i * 2, level, pan);
            return;
        }
    }
}

/* Stop the emitter following descriptor `id`, freeing its slot. */
void func_800863E8(s32 id) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].actor == id) {
            func_8003A20C(i * 2);
            D_800AFE88[i].sound = 0xFFFF;
            D_800AFE88[i].actor = 0xFFFF;
            return;
        }
    }
}

/* The emitter slot following descriptor `id`, or -1. */
s32 func_80086470(s32 owner, s32 id) {
    s32 i;

    if (owner == -1) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (D_800AFE88[i].actor == id) {
            return i;
        }
    }
    return -1;
}

/* Clear the emitter slots. */
void func_800864B4(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800AFE88[i].actor = 0xFFFF;
        D_800AFE88[i].sound = 0xFFFF;
    }
}

/* Clear the emitter slots and stop the voices of the emitters in use. */
void func_800864F0(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800AFE88[i].sound = 0xFFFF;
        D_800AFE88[i].actor = 0xFFFF;
    }
    for (i = 0; i < 4; i++) {
        if (!(D_800B2078.effects_kept & 1)) {
            func_8003A20C(i * 2);
        }
        D_800B2078.effects_kept >>= 1;
    }
}

/* Keep the sound emitters of the three actors nearest `listener` (actor
 * +10d not ff): update those already playing, start the others and stop
 * table entries no longer among them. */
void func_80086590(VECTOR *listener) {
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
    for (i = 0; i < D_800ADBFC; i++) {
        emitter = D_800AF880.components.descriptors[i].actor;
        if (emitter->sound_mode != 0xFF) {
            length = func_80099A04((listener->vx >> 16) - (emitter->position[0] >> 16),
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
                near.sound[far] = D_800AF880.components.descriptors[i].actor->sound;
                near.volume[far] = D_800AF880.components.descriptors[i].actor->sound_volume;
                (near.offset + far)->vx = (listener->vx >> 16) - (D_800AF880.components.descriptors[i].actor->position[0] >> 16);
                (near.offset + far)->vy = (listener->vy >> 16) - (D_800AF880.components.descriptors[i].actor->position[1] >> 16);
                (near.offset + far)->vz = (listener->vz >> 16) - (D_800AF880.components.descriptors[i].actor->position[2] >> 16);
            }
        } else {
            emitter->sound_mode = 0xFF;
        }
    }
    for (i = 0; i < 3; i++) {
        slot = func_80086470(near.sound[i], near.actor[i]);
        if (slot != -1) {
            near.matched[slot] = 1;
            near.kept[i] = 1;
        }
    }
    for (i = 0; i < 3; i++) {
        if (near.matched[i] == 0 && D_800AFE88[i].sound != 0xFFFF) {
            func_8003A20C(i * 2);
            D_800AFE88[i].sound = 0xFFFF;
            D_800AFE88[i].actor = 0xFFFF;
        }
    }
    for (i = 0, offset = near.offset; i < 3; offset++, i++) {
        if (near.sound[i] != -1) {
            if (near.kept[i] == 1) {
                func_800860F0(near.sound[i], near.volume[i], offset->vx, near.distance[i], near.actor[i]);
            } else {
                func_800862CC(near.sound[i], near.volume[i], offset->vx, near.distance[i], near.actor[i]);
            }
        }
    }
}

/* Point the listener (80086590) at the controlled actor, the camera eye or
 * the camera target, as 800b22e0 selects. */
void func_80086908(void) {
    switch (D_800B2078.unk22E0) {
    case 0:
        func_80086590((VECTOR *)D_800AF880.components.descriptors[D_800B2078.controlled].actor->position);
        break;
    case 1:
        func_80086590(&D_800AF880.eye);
        break;
    case 2:
        func_80086590(&D_800AF880.target);
        break;
    }
}

/* Event opcode fe: advance the pc to the next byte and run the extended
 * handler it names (800ae6a0); those read operands relative to that byte,
 * and one that does not advance leaves the pc there, so the byte then runs
 * as a primary opcode. */
void func_800869B8(void) {
    D_800AE6A0[D_800ADC00[++D_800B0078->pc]]();
}

/* Scale emitter `emitter`'s per-step delta ((22e8 - 2300) / 2318) by the
 * steps left once its distance from `position` is covered, into 223c. */
void func_80086A1C(s32 emitter, s32 *position) {
    s32 dx;
    s32 dy;
    s32 dz;
    s32 left;

    dx = ((D_800B2078.unk22E8[emitter][0] - D_800B2078.unk2300[emitter][0]) << 16) / D_800B2078.unk2318[emitter];
    dy = ((D_800B2078.unk22E8[emitter][1] - D_800B2078.unk2300[emitter][1]) << 16) / D_800B2078.unk2318[emitter];
    dz = ((D_800B2078.unk22E8[emitter][2] - D_800B2078.unk2300[emitter][2]) << 16) / D_800B2078.unk2318[emitter];
    left = D_800B2078.unk2318[emitter] - func_80099A04(D_800B2078.emitter_position[emitter][0] - WHOLE(position[0]),
                                                        D_800B2078.emitter_position[emitter][1] - WHOLE(position[1]),
                                                        D_800B2078.emitter_position[emitter][2] - WHOLE(position[2]));
    D_800B2078.unk223C[0][emitter] = (dx * left) >> 16;
    D_800B2078.unk223C[1][emitter] = (dy * left) >> 16;
    D_800B2078.unk223C[2][emitter] = (dz * left) >> 16;
}

/* Update the three positional emitters from their actors' positions. */
void func_80086BA8(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_800B2078.emitter_descriptor[i] != -1) {
            func_80086A1C(i, D_800AF880.components.descriptors[D_800B2078.emitter_descriptor[i]].actor->position);
        }
    }
}

/* Particle emitters by selector byte 1: 0 continues at +2; 1 resets the eight
 * template emitters for the current actor (800a94a4), sets up template 0 (16
 * particles, +74 = 0x20 when operand 4 is 0x27, else 0x22), starts an effect
 * from them (800a99a8) and continues at +8 (operands 2 and 6 are read but
 * unused). Other selectors never advance. */
void func_80086C34(void) {
    s32 kind;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        D_800B0078->pc += 2;
        break;
    case 1:
        func_800ACDEC(2);
        kind = func_800ACDEC(4);
        func_800ACDEC(6);
        func_800A94A4(D_800AFD1C);
        D_800B02CC[0].flags = 0x14;
        D_800B02CC[0].unk00 = 1;
        D_800B02CC[0].count = 0x10;
        D_800B02CC[0].unk72 = 0;
        D_800B02CC[0].unk74 = kind;
        if (kind == 0x27) {
            D_800B02CC[0].unk74 = 0x20;
        } else {
            D_800B02CC[0].unk74 = 0x22;
        }
        D_800B02CC[0].unk04 = 0x1000;
        func_800A99A8(D_800AFD1C);
        D_800B0078->pc += 8;
        break;
    }
}

/* Event opcode e2: resident 80019cd0 shuts the libraries down and restarts from
 * the entry point; then yield and advance one byte. */
void func_80086D4C(void) {
    func_80019CD0();
    D_800B00C0 = 1;
    D_800B0078->pc++;
}

/* Set both draw blocks' display screen rectangles (0, 10, 256, 216). */
void func_80086D8C(void) {
    D_800B249C[0].disp.screen.x = 0;
    D_800B249C[0].disp.screen.y = 10;
    D_800B249C[0].disp.screen.w = 0x100;
    D_800B249C[0].disp.screen.h = 0xD8;
    D_800B249C[1].disp.screen.x = 0;
    D_800B249C[1].disp.screen.y = 10;
    D_800B249C[1].disp.screen.w = 0x100;
    D_800B249C[1].disp.screen.h = 0xD8;
}

/* Event opcode e0: set 800b2358 from byte 1: nonzero disables the field loop's
 * start-button pause. */
void func_80086DE0(void) {
    D_800B2078.unk2358 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: operand 1 0 clears VRAM (0, 0, 0x500, 0x200) and switches both draw
 * and display environments to 640x224 (screens by 80086d8c); 1 and 2 show and
 * hide the five overlay sprites (800adb54, drawn by 800abec8). */
void func_80086E1C(void) {
    RECT rect;

    switch (func_800ACDEC(1)) {
    case 0:
        rect.w = 0x500;
        rect.x = 0;
        rect.y = 0;
        rect.h = 0x200;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x280, 0xE0);
        SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x280, 0xE0);
        SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x280, 0xE0);
        func_80086D8C();
        break;
    case 1:
        D_800ADB54 = 1;
        break;
    case 2:
        D_800ADB54 = 0;
        break;
    }
    D_800B0078->pc += 3;
}

/* Event: attach the current actor to node operand 3 of 801e layer actor operand
 * 1 (+128 = operand 1 << 12 | operand 3): its descriptor's transform then
 * follows that node's world matrix (801e72cc); 0xffff detaches. */
void func_80086F7C(void) {
    s32 high = func_800ACDEC(1);

    D_800B0078->unk128 = (high << 12) | func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: the 33 overlay sprites at 800afc68 by byte 1: 0 allocates them
 * (800aac08), 2 releases them (800aabd8), 1 places sprite operand 2 at (operand
 * 4, operand 6) with anchor operand 8 for this frame (800aae4c), 3 sets sprite
 * operand 2's colour to (operand 4, 6, 8) (800aadc8). Other values leave the pc
 * on the extended byte, which then runs as primary opcode d4. */
void func_80086FD0(void) {
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        func_800AAC08();
        D_800B0078->pc += 2;
        break;
    case 2:
        func_800AABD8();
        D_800B0078->pc += 2;
        break;
    case 1:
        func_800AAE4C(func_800ACDEC(2), func_800ACDEC(4), func_800ACDEC(6), func_800ACDEC(8));
        D_800B0078->pc += 10;
        break;
    case 3:
        func_800AADC8(func_800ACDEC(2), func_800ACDEC(4), func_800ACDEC(6), func_800ACDEC(8));
        D_800B0078->pc += 10;
        break;
    }
}

/* Event: set bits op3 in the flags of game record op1. */
void func_80087148(void) {
    s32 record = func_800ACDEC(1);
    s32 bits = func_800ACDEC(3);

    D_8005A39C->skills[record].flags1A |= bits;
    D_800B0078->pc += 5;
}

/* Event op dd, by byte 1: 0 allocates two buffers of operand 4 rows and saves
 * the 256-wide screen band at y operand 2 into one (yields); 1 runs resident
 * 80026f44 on operand 4 rows from row operand 2 of the saved band into the
 * working copy and has the frame load it back (800adbb4); 2 frees both buffers
 * (yields); 3 only yields. Other values leave the pc on the extended byte,
 * which then runs as primary opcode dd. */
void func_800871B0(void) {
    s32 y;
    s32 h;
    s32 row;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        D_800AFC7C += 0x20;
        y = func_800ACDEC(2);
        h = func_800ACDEC(4);
        D_800C3A48 = func_80031BDC(h << 9, 0);
        D_800AF87C = func_80031BDC(h << 9, 0);
        D_800AFC58.x = 0;
        D_800AFC58.y = y;
        D_800AFC58.w = 0x100;
        D_800AFC58.h = h;
        StoreImage(&D_800AFC58, (u_long *)D_800C3A48);
        D_800B00C0 = 1;
        D_800B0078->pc += 6;
        break;
    case 1:
        D_800AFC7C += 0x20;
        row = func_800ACDEC(2);
        h = func_800ACDEC(4);
        func_80026F44(0x100, h, D_800AF87C + (row << 8), D_800C3A48 + (row << 8));
        D_800ADBB4 = 1;
        D_800B0078->pc += 6;
        break;
    case 2:
        func_800320E8(D_800C3A48);
        func_800320E8(D_800AF87C);
        D_800B00C0 = 1;
        D_800B0078->pc += 2;
        break;
    case 3:
        D_800B0078->pc += 2;
        D_800B00C0 = 1;
        break;
    }
}

/* Event: set entry operand 1 of the 801e layer row table 800b225f to operand 3:
 * that layer's actor is then created at x 0x240 - (layer + row) * 64 (801e742c,
 * via 80077ab4 and ext 5c). */
void func_800873C4(void) {
    s32 index = func_800ACDEC(1);

    D_800B2078.unk225F[index] = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: variables op13 and op15 receive op1 * op9 / op5 and
 * op3 * op11 / op7 (16.16 intermediate). */
void func_80087420(void) {
    s32 a = func_800ACDEC(1);
    s32 b = func_800ACDEC(3);
    s32 c = func_800ACDEC(5);
    s32 d = func_800ACDEC(7);
    s32 e = func_800ACDEC(9);
    s32 f = func_800ACDEC(11);
    s32 first = (((e << 16) / c) * a) >> 16;
    s32 second = (((f << 16) / d) * b) >> 16;

    func_800A3074(func_800ACDB8(13) & 0xFFFF, first);
    func_800A3074(func_800ACDB8(15) & 0xFFFF, second);
    D_800B0078->pc += 17;
}

/* Event: skip a two-byte operand. */
void func_8008752C(void) {
    D_800B0078->pc += 3;
}

/* Event: set game flag 0x4000 of +22b6. */
void func_8008754C(void) {
    D_8005A39C->flags |= 0x4000;
    D_800B0078->pc++;
}

/* Event: copy gear record operand 1 over gear record operand 3 (0xa4-byte
 * records at +978). */
void func_80087580(void) {
    s32 from = func_800ACDEC(1);

    D_8005A39C->gears[func_800ACDEC(3)] = D_8005A39C->gears[from];
    D_800B0078->pc += 5;
}

/* Event: copy character operand 1's 0xa4-byte record and 32-byte game record
 * over operand 3's; copying to 9 or 10 sets 0x2000 or 0x1000 in the game's
 * +22b6. */
void func_8008764C(void) {
    s32 from = func_800ACDEC(1);
    s32 to = func_800ACDEC(3);

    D_8005A39C->characters[to] = D_8005A39C->characters[from];
    D_8005A39C->skills[to] = D_8005A39C->skills[from];
    {
        GameData *state = D_8005A39C;

        if (to == 9) {
            state->flags |= 0x2000;
        }
    }
    if (to == 10) {
        D_8005A39C->flags |= 0x1000;
    }
    D_800B0078->pc += 5;
}

/* Event: store the arena bout's outcome (80050622, the byte after the six
 * parameters ext bf sets; menu3 func_80075060 writes it) in variable
 * operand 1. */
void func_80087800(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_80050622);
    D_800B0078->pc += 3;
}

/* Event: once the field allows it (800adbdc and 800adbe4 set, 800adb2c clear,
 * music result 8004f308 not -1; else pc-- back to fe and yield), select menu
 * task 0 (800379b4 sets 80050618), set its six parameter bytes at 8005061c from
 * operands 1-11 and request leaving the field: 800adb88 set, 800adbe8 cleared,
 * so the field loop ends with kind 2 (game mode 4, 8007954c). */
void func_80087848(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1) {
        D_800B00C0 = 1;
        D_800B0078->pc--;
        return;
    }
    func_800379B4(0);
    D_8005061C[0] = func_800ACDEC(1);
    D_8005061C[1] = func_800ACDEC(3);
    D_8005061C[2] = func_800ACDEC(5);
    D_8005061C[3] = func_800ACDEC(7);
    D_8005061C[4] = func_800ACDEC(9);
    D_8005061C[5] = func_800ACDEC(11);
    D_800ADB88 = 1;
    D_800ADBE8 = 0;
    D_800B0078->pc += 13;
}

/* Event: store the world map ferry's saved x and z (+1844, +1846;
 * D_8006EE78) in variables op1 and op3. */
void func_80087960(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk1844[0]);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->unk1844[1]);
    D_800B0078->pc += 5;
}

/* Event: store the circling flight's saved x and z (+184e, +1852;
 * D_8006EE80) in variables op1 and op3, read unsigned (the world map's
 * halves are signed). */
void func_800879D0(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, (u16)D_8005A39C->flight.x);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, (u16)D_8005A39C->flight.z);
    D_800B0078->pc += 5;
}

/* Event: set 800b2357 from byte 1: nonzero skips depth-cueing the actors' model
 * colour in the field draw pass. */
void func_80087A40(void) {
    D_800B2078.unk2357 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: set 800b2354 from byte 1: player control (a7) takes its d-pad headings
 * from 800adf68 when zero, else from 800adf88. */
void func_80087A7C(void) {
    D_800B2078.unk2354 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: set the circling flight's saved x and z (+184e, +1852; D_8006EE80)
 * from operands 1 and 3, immediate by flags 0x80/0x40 of byte 9 (past the
 * instruction's 6 bytes), clear +1850 and +1854 and set +1856 to 1. */
void func_80087AB8(void) {
    D_8005A39C->flight.x = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->flight.z = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->flight.count = 0;
    D_8005A39C->flight.z_frac = 0;
    D_8005A39C->unk1856 = 1;
    D_800B0078->pc += 6;
}

/* Event: store the world map vehicle's saved position (game +182c-+1830)
 * and heading (+1832; WorldmapReturn) in variables op1..op7. */
void func_80087B5C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->worldmap.unk60);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->worldmap.unk62);
    func_800A3074(func_800ACDB8(5) & 0xFFFF, D_8005A39C->worldmap.unk64);
    func_800A3074(func_800ACDB8(7) & 0xFFFF, D_8005A39C->worldmap.vehicle_heading);
    D_800B0078->pc += 9;
}

/* Event: set 8004f300, which enables the file 0xab sequence (800acc58) that
 * 800a7948 draws over movie frames 0x687-0x18e1. */
void func_80087C0C(void) {
    D_8004F300 = 1;
    D_800B0078->pc++;
}

/* Event: set the world map vehicle's saved position and heading
 * (+182c-+1832) from operands 1..7 (immediate by flags 0x80/0x40/0x20/0x10
 * of byte 9). */
void func_80087C34(void) {
    D_8005A39C->worldmap.unk60 = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->worldmap.unk62 = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->worldmap.unk64 = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->worldmap.vehicle_heading = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event: store the world map vehicle's flags (+1834, WorldmapReturn.flags)
 * in variable op1. */
void func_80087D30(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->worldmap.flags);
    D_800B0078->pc += 3;
}

/* Event: set the world map vehicle's flags (+1834) from operand 1
 * (immediate when flag 0x80 of byte 3 is set). */
void func_80087D80(void) {
    D_8005A39C->worldmap.flags = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 3]);
    D_800B0078->pc += 4;
}

/* Event: set 800b2355 (byte 1 zero) or 800b2356 from operand 2: the 8005954c
 * value of random-encounter battles (field loop) and of scripted battles (71,
 * ext 84) respectively. */
void func_80087DE0(void) {
    s32 value = func_800ACDEC(2);

    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        D_800B2078.unk2355 = value;
    } else {
        D_800B2078.unk2356 = value;
    }
    D_800B0078->pc += 4;
}

/* Event: set the battle-entry override (800b234c) from operand 1. */
void func_80087E5C(void) {
    D_800B2078.battle_override = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: make the actor byte 1 selects the controlled actor and the one the
 * camera follows (800b233e): clear flags 0x01004000 on every event actor and
 * set 0x4000 on it; the followers are idle unless it is the party leader's
 * actor. */
void func_80087E98(void) {
    s32 index = func_8009CDB4(1);
    s32 i;

    if (index != 0xFF) {
        if (index == D_8005A444[0]) {
            D_800B2078.followers_idle = 0;
        } else {
            D_800B2078.followers_idle = 1;
        }
        D_800B2078.controlled = index;
        D_800B2078.unk233E = index;
        for (i = 0; i < D_800ADBFC; i++) {
            D_800AF880.components.descriptors[i].actor->flags &= ~0x01004000;
        }
        D_800AF880.components.descriptors[index].actor->flags |= 0x4000;
    }
    D_800B0078->pc += 2;
}

/* Event: count 800b2348 up: while it is nonzero, party gathering (8009aee0, ext
 * 23/24) warps members to their spots instead of walking; gathering clears
 * it. */
void func_80087FA4(void) {
    D_800B2078.unk2348++;
    D_800B0078->pc++;
}

/* Event: build the status panel (800a8ba4): load file 0xaa's image to VRAM
 * (380, 0) (800a8314), allocate both buffers' piece quads and texture them. */
void func_80087FD4(void) {
    func_800A8BA4();
    D_800B0078->pc++;
}

extern void func_801E72CC(MATRIX *m, MATRIX *work, s32 a, s32 b);
/* Event: transform the vector (operands 5, 7, 9) by the world matrix of node
 * operand 3 of 801e layer actor operand 1 (801e72cc; selected by flags byte 11)
 * and store its x, y, z in the variables operands 12, 14 and 16 name. */
void func_8008800C(void) {
    MATRIX m;
    MATRIX work;
    SVECTOR in;
    SVECTOR out;
    s32 flag;
    s32 a;

    m.t[0] = m.t[1] = m.t[2] = 0;
    a = func_8009CF78(1, EVENT_OPERAND_BYTE(0xB));
    func_801E72CC(&m, &work, a, func_8009CFBC(3, EVENT_OPERAND_BYTE(0xB)));
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    in.vx = func_8009D000(5, EVENT_OPERAND_BYTE(0xB));
    in.vy = func_8009D044(7, EVENT_OPERAND_BYTE(0xB));
    in.vz = func_8009D088(9, EVENT_OPERAND_BYTE(0xB));
    RotTransSV(&in, &out, &flag);
    func_800A3074(func_800ACDB8(0xC) & 0xFFFF, out.vx);
    func_800A3074(func_800ACDB8(0xE) & 0xFFFF, out.vy);
    func_800A3074(func_800ACDB8(0x10) & 0xFFFF, out.vz);
    D_800B0078->pc += 0x12;
}

/* Event: restore every gear's points and gauge to their maxima (the 20 gear
 * records at +978). */
void func_80088198(void) {
    s32 i;
    GameData *state = D_8005A39C;

    for (i = 0; i < 20; i++) {
        state->gears[i].hp = state->gears[i].maxHp;
        state->gears[i].fuel = state->gears[i].maxFuel;
    }
    D_800B0078->pc++;
}

/* Event: byte 1 zero saves the 64x256 VRAM column at (3c0, 100) once
 * (800a915c); nonzero restores it and frees the copy (800a91f0). */
void func_800881E8(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        func_800A915C();
    } else {
        func_800A91F0();
    }
    D_800B0078->pc += 2;
}

/* Event: wait (pc-- back to fe), yielding, while a music change is pending
 * (8004f308 == -1, set by 8008f7b8 when it changes the track); then advance,
 * yielding. */
void func_8008825C(void) {
    if (D_8004F308 == -1) {
        D_800B0078->pc--;
    } else {
        D_800B0078->pc++;
    }
    D_800B00C0 = 1;
}

/* Event: store the gear (+a0 of its record) of character operand 1 (8008cf3c:
 * fd-ff party slots 0-2, fc none) in variable operand 3, or 0xff for none. */
void func_800882B8(void) {
    s32 character = func_8008CF3C(func_800ACDEC(1));

    if (character != 0xFF) {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->characters[character].gearId);
    } else {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, 0xFF);
    }
    D_800B0078->pc += 5;
}

/* Event: set the gear (+a0 of the 0xa4-byte record) of character operand 1 to
 * operand 3. */
void func_80088360(void) {
    s32 slot = func_800ACDEC(1);

    D_8005A39C->characters[slot].gearId = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: set (selector 0) or clear character op2's bit of the game's +2318. */
void func_800883D4(void) {
    s32 character = func_8008CF3C(func_800ACDEC(2));

    if (character != 0xFF) {
        if (D_800ADC00[D_800B0078->pc + 1] == 0) {
            D_8005A39C->locked |= 1 << character;
        } else {
            D_8005A39C->locked &= ~(1 << character);
        }
    }
    D_800B0078->pc += 4;
}

#include "field_script.h"

/* Event: set 800b236c to the inverse of its byte operand's low bit. */
void func_8008848C(void) {
    D_800B236C = EVENT_OPERAND_BYTE(1) ^ 1;
    D_800B0078->pc += 2;
}

/* Event: set the sound-emitter range from operand 1. */
void func_800884CC(void) {
    D_800B2078.emitter_range = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: for party member operand 5 (0xff none), store its model's
 * animation +0c in variable operand 1 and its descriptor in variable
 * operand 3, clearing +0c unless it is 1; four batch steps. */
void func_80088508(void) {
    s32 member;
    Sprite *model;

    member = D_8005A444[func_800ACDEC(5)];
    D_800AFC7C += 4;
    if (member != 0xFF) {
        model = D_800AF880.components.descriptors[member].model;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, (u16)SPRITE_SEQUENCER(model)->halfc);
        func_800A3074(func_800ACDB8(3) & 0xFFFF, member);
        if ((u16)SPRITE_SEQUENCER(model)->halfc != 1) {
            SPRITE_SEQUENCER(model)->halfc = 0;
        }
    } else {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, 0);
        func_800A3074(func_800ACDB8(3) & 0xFFFF, 0);
    }
    D_800B0078->pc += 7;
}

/* Clear the eight pairs at +30 of the current emitter record. */
void func_8008861C(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        D_800B02CC[D_800B2374.record].unk30[i][0] = D_800B02CC[D_800B2374.record].unk30[i][1] = 0;
    }
}

/* Event: as ext 8f for actor operand 1 (0xff: actor 0 for the template reset,
 * while 800b2374 keeps operand 1): launch frame operand 3 and the 801e layer
 * actor and node operands 5 and 7; four batch steps. */
void func_80088674(void) {
    s32 actor = func_800ACDEC(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    D_800B2374.actor = func_800ACDEC(1);
    D_800B2374.frame = func_800ACDEC(3);
    D_800B2374.layer_actor = func_800ACDEC(5);
    D_800B2374.layer_node = func_800ACDEC(7);
    D_800B0078->pc += 9;
    func_800A94A4(actor);
    switch (D_800B2374.frame) {
    case 0:
        D_800B2374.frame = 0;
        break;
    case 1:
        D_800B2374.frame = 0x10;
        break;
    case 2:
        D_800B2374.frame = 0x20;
        break;
    case 3:
        D_800B2374.frame = 0x30;
        break;
    }
    D_800AFC7C += 4;
}

/* Event: begin an effect for the actor byte 1 selects (none: actor 0): reset
 * the eight emitter templates at 800b02cc to follow it (800a94a4) and keep its
 * launch frame operand 2 (0 owner-facing, 1 801e node, 2 owner's transform, 3
 * owner-facing and scaled; 0-3 kept as kind << 4) and operands 4 and 6 (the
 * 801e layer actor and node of frame 1) at 800b2374-2380 for ext 90 and 93;
 * four batch steps. */
void func_80088790(void) {
    s32 actor = func_8009CDB4(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    D_800B2374.actor = actor;
    D_800B2374.frame = func_800ACDEC(2);
    D_800B2374.layer_actor = func_800ACDEC(4);
    D_800B2374.layer_node = func_800ACDEC(6);
    D_800B0078->pc += 8;
    func_800A94A4(actor);
    switch (D_800B2374.frame) {
    case 0:
        D_800B2374.frame = 0;
        break;
    case 1:
        D_800B2374.frame = 0x10;
        break;
    case 2:
        D_800B2374.frame = 0x20;
        break;
    case 3:
        D_800B2374.frame = 0x30;
        break;
    }
    D_800AFC7C += 4;
}

/* Event: give the current actor's sprite a new two-word sequencer buffer
 * (8002303c) whose entries 2 and 3 are (operand 1 & 15) << 6 and (operand 1 >>
 * 4) << 8 + operand 3; the values and mode 1 are kept in the actor (+12c, +130)
 * for 800a28d4 to rebuild it. */
void func_800888A4(void) {
    Sprite *model;
    u16 low;
    s32 frame;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    low = func_800ACDEC(1) & 0xF;
    frame = ((func_800ACDEC(1) >> 4) << 8) + func_800ACDEC(3);
    func_8002303C(model, 2, 0);
    SPRITE_SEQUENCER(model)->buffer[2] = low << 6;
    D_800B0078->state.bits.unk18 = low << 6;
    SPRITE_SEQUENCER(model)->buffer[3] = frame;
    D_800B0078->unk130 = frame;
    D_800B0078->state.bits.unk16 = 1;
    D_800B0078->pc += 5;
}

/* Event: as ext a6 with a three-word sequencer buffer: entries 2-3 from
 * operands 1 and 3, entries 4-5 from operands 5 and 7 (mode 2). */
void func_800889BC(void) {
    Sprite *model;
    u16 low0;
    s32 frame0;
    u16 low1;
    s32 frame1;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    low0 = func_800ACDEC(1) & 0xF;
    frame0 = ((func_800ACDEC(1) >> 4) << 8) + func_800ACDEC(3);
    low1 = func_800ACDEC(5) & 0xF;
    frame1 = ((func_800ACDEC(5) >> 4) << 8) + func_800ACDEC(7);
    func_8002303C(model, 3, 0);
    SPRITE_SEQUENCER(model)->buffer[2] = low0 << 6;
    D_800B0078->state.bits.unk18 = low0 << 6;
    SPRITE_SEQUENCER(model)->buffer[3] = frame0;
    D_800B0078->unk130 = frame0;
    SPRITE_SEQUENCER(model)->buffer[4] = low1 << 6;
    D_800B0078->unk130_9 = low1 << 6;
    SPRITE_SEQUENCER(model)->buffer[5] = frame1;
    D_800B0078->unk130_19 = frame1;
    D_800B0078->pc += 9;
    D_800B0078->state.bits.unk16 = 2;
}

/* Event: set flag 0x80 (op1 1) or 0x40 (op1 2) of the current record's +2a,
 * using four batch steps. */
void func_80088B68(void) {
    s32 bits = 0;

    switch (func_800ACDEC(1)) {
    case 1:
        bits = 0x80;
        break;
    case 2:
        bits = 0x40;
        break;
    }
    D_800B02CC[D_800B2374.record].flags |= bits;
    D_800AFC7C += 4;
    D_800B0078->pc += 7;
}

/* Event: set the current emitter template's +24 to operand 1, or operand 3 << 8
 * into its flags and set its particle angle (+76) to operand 5; four batch
 * steps. */
void func_80088C1C(void) {
    D_800B02CC[D_800B2374.record].unk24 = func_800ACDEC(1);
    D_800B02CC[D_800B2374.record].flags |= func_800ACDEC(3) << 8;
    D_800B02CC[D_800B2374.record].unk76 = func_800ACDEC(5);
    D_800AFC7C += 4;
    D_800B0078->pc += 7;
}

/* Event: set the current emitter template's +30 pairs 0-3 from the selected
 * operands 1-15 (flags byte 17; 80088d38); four batch steps. */
void func_80088CF8(void) {
    func_80088D38(0);
}

/* Event: set the current emitter template's +30 pairs 4-7 from the selected
 * operands 1-15 (flags byte 17; 80088d38); four batch steps. */
void func_80088D18(void) {
    func_80088D38(4);
}

/* Set the current emitter record's four +30 pairs from index first on to the
 * selected operands 1..15 (flags byte 0x11); four batch steps. */
void func_80088D38(s32 first) {
    D_800B02CC[D_800B2374.record].unk30[first][0] = func_8009CF78(1, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2374.record].unk30[first][1] = func_8009CFBC(3, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2374.record].unk30[first + 1][0] = func_8009D000(5, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2374.record].unk30[first + 1][1] = func_8009D044(7, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2374.record].unk30[first + 2][0] = func_8009D088(9, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2374.record].unk30[first + 2][1] = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2374.record].unk30[first + 3][0] = func_8009D110(0xD, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2374.record].unk30[first + 3][1] = func_8009D154(0xF, EVENT_OPERAND_BYTE(0x11));
    D_800AFC7C += 4;
    D_800B0078->pc += 0x12;
}

extern s32 D_800ADB40;
/* Event: select emitter template operand 1 (800b2384) for the template
 * instructions that follow and set it up for the effect's actor (+52 and
 * 800adb40 = 800b2374): operand 3 particles, start delay operand 5, lifetime
 * operand 7 (7fff lasting), +24 = 1, +00 and +76 cleared and its +30 pairs
 * cleared (8008861c); four batch steps. */
void func_80089004(void) {
    s32 index;

    D_800B2374.record = index = func_800ACDEC(1);
    D_800B02CC[index].unk24 = 1;
    D_800B02CC[D_800B2374.record].unk52 = D_800B2374.actor;
    D_800ADB40 = D_800B02CC[D_800B2374.record].unk52;
    D_800B02CC[D_800B2374.record].unk00 = 0;
    D_800B02CC[D_800B2374.record].unk76 = 0;
    D_800B02CC[D_800B2374.record].count = func_800ACDEC(3);
    D_800B02CC[D_800B2374.record].unk02 = func_800ACDEC(5);
    D_800B02CC[D_800B2374.record].unk04 = func_800ACDEC(7);
    func_8008861C();
    D_800AFC7C += 4;
    D_800B0078->pc += 9;
}

/* Event: set the current emitter record's +0c and +14 vectors from the
 * selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089174(void) {
    D_800B02CC[D_800B2374.record].unk0C.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk0C.vy = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk0C.vz = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk14.vx = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk14.vy = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk14.vz = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: set the current emitter template's +08 (operand 1), +1c vector
 * (operands 3, 5, 7), spawn radius +26 (operand 9) and velocity spread +28
 * (operand 11), selected by flags byte 13; four batch steps. */
void func_80089374(void) {
    D_800B02CC[D_800B2374.record].unk08 = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk1C.vx = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk1C.vy = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk1C.vz = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk26 = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk28 = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: set the current emitter template's spawn interval +56, particle life
 * +58 and +54 from operands 1, 3 and 5, its flags to operand 7 | operand 9 * 2
 * | the effect's launch frame (800b2378), and +72/+74 (the 801e layer actor and
 * node of launch frame 1) from 800b237c/800b2380; four batch steps. */
void func_80089574(void) {
    s16 flags;

    D_800B02CC[D_800B2374.record].unk56 = func_800ACDEC(1);
    D_800B02CC[D_800B2374.record].unk58 = func_800ACDEC(3);
    D_800B02CC[D_800B2374.record].unk54 = func_800ACDEC(5);
    flags = func_800ACDEC(7);
    D_800B02CC[D_800B2374.record].flags = flags | (func_800ACDEC(9) * 2) | D_800B2374.frame;
    D_800B02CC[D_800B2374.record].unk72 = D_800B2374.layer_actor;
    D_800B02CC[D_800B2374.record].unk74 = D_800B2374.layer_node;
    D_800AFC7C += 4;
    D_800B0078->pc += 11;
}

/* Event: set the current emitter record's +5a and +62 vectors from the
 * selected operands 1/3 and 5/7 (flags byte 9); four batch steps. */
void func_800896D4(void) {
    D_800B02CC[D_800B2374.record].unk5A.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2374.record].unk5A.vy = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2374.record].unk5A.vz = 0;
    D_800B02CC[D_800B2374.record].unk62.vx = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2374.record].unk62.vy = func_8009D044(7, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2374.record].unk62.vz = 0;
    D_800AFC7C += 4;
    D_800B0078->pc += 10;
}

/* Event: set the current emitter record's bytes +6a..+6c and +6e..+70 from
 * the selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089880(void) {
    D_800B02CC[D_800B2374.record].unk6A = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk6B = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk6C = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk6E = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk6F = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2374.record].unk70 = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: unless 800adb8c is set, start the effect the templates define for the
 * current actor (800a99a8: copy the eight emitter templates into a free effect
 * slot it owns and allocate their particles); four batch steps. */
void func_80089A80(void) {
    D_800AFC7C += 4;
    if (D_800ADB8C == 0) {
        func_800A99A8(D_800AFD1C);
    }
    D_800B0078->pc++;
}

/* Event: stop the current actor's effects (800a98e8), also releasing their
 * particles when byte 1 is nonzero; four batch steps. */
void func_80089AE4(void) {
    D_800AFC7C += 4;
    func_800A98E8(D_800AFD1C, D_800ADC00[D_800B0078->pc + 1]);
    D_800B0078->pc += 2;
}

/* Event: store the current actor's party position (or 0xff) in variable op1. */
void func_80089B54(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_8005A444[i] == D_800AFD1C) {
            func_800A3074(func_800ACDB8(1) & 0xFFFF, i);
            goto done;
        }
    }
    func_800A3074(func_800ACDB8(1) & 0xFFFF, 0xFF);
done:
    D_800B0078->pc += 3;
}

/* Event: set emitter operand 1's start (22e8) and end (2300) points and its
 * step count (2318) from the selected operands 3-15 (flags byte 0x11). */
void func_80089BF0(void) {
    s32 emitter;

    emitter = func_8009CF78(1, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk22E8[emitter][0] = func_8009CFBC(3, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk22E8[emitter][1] = func_8009D000(5, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk22E8[emitter][2] = func_8009D044(7, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2300[emitter][0] = func_8009D088(9, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2300[emitter][1] = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2300[emitter][2] = func_8009D110(0xD, EVENT_OPERAND_BYTE(0x11));
    D_800B2078.unk2318[emitter] = func_8009D154(0xF, EVENT_OPERAND_BYTE(0x11));
    D_800B0078->pc += 0x12;
}

/* Event: place sound emitter op1 at (op3, op7, op5) and attach it to the
 * actor byte 10 selects (-1 for none). */
void func_80089DCC(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    s32 actor;

    D_800B2078.emitter_position[index][0] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.emitter_position[index][2] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.emitter_position[index][1] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    actor = func_8009CDB4(10);
    if (actor != 0xFF) {
        D_800B2078.emitter_descriptor[index] = actor;
    } else {
        D_800B2078.emitter_descriptor[index] = -1;
    }
    D_800B0078->pc += 11;
}

/* Event: set the sound listener selector 800b22e0 from operand 1 (80086908
 * places the listener at 0 the controlled actor, 1 the camera eye, 2 the camera
 * target). */
void func_80089F18(void) {
    D_800B2078.unk22E0 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set the piece drift mode 800b21d2 to operand 1 less 0x80: its low
 * bits pick which pieces the drift vector (fe 1d) moves each frame. */
void func_80089F54(void) {
    D_800B2078.piece_drift_mode = func_800ACDEC(1) - 0x80;
    D_800B0078->pc += 3;
}

/* Event: store operand byte 1 in 800afe84: after a movie or a menu (800a7c58,
 * 800799d4) a nonzero value sets 800adb50 (the panorama is then not drawn), and
 * the menu return presents the screen under brightness tiles 0x20-0x3e
 * (80079784) instead of 0x1f-0. */
void func_80089F94(void) {
    FieldActor *actor;

    actor = D_800B0078;
    D_800AFE84 = D_800ADC00[actor->pc + 1];
    actor->pc += 2;
}

/* Event: set the panorama backdrop parameters at 800b0080 that the field load
 * passes to 8002709c: texture x operand 1, y operand 3, width operand 5 (0
 * becomes 1), height operand 7, CLUT x 0, CLUT y operand 9, mode operand 11 and
 * turn operand 13 (raw halfwords). */
void func_80089FD0(void) {
    s16 value;

    D_800B0080.unk80[0] = func_800ACDB8(1);
    D_800B0080.unk80[1] = func_800ACDB8(3);
    value = func_800ACDB8(5);
    D_800B0080.unk80[2] = value;
    if (value == 0) {
        D_800B0080.unk80[2] = value + 1;
    }
    D_800B0080.unk80[3] = func_800ACDB8(7);
    D_800B0080.unk80[4] = 0;
    D_800B0080.unk80[5] = func_800ACDB8(9);
    D_800B0080.unk80[6] = func_800ACDB8(11);
    D_800B0080.unk80[7] = func_800ACDB8(13);
    D_800B0078->pc += 15;
}

/* Event: set the panorama backdrop's position (800b0090: x operand 1, z operand
 * 3, y operand 5; immediate by flags 0x80/0x40/0x20 of byte 7). */
void func_8008A08C(void) {
    D_800B0080.unk90 = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0080.unk98 = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0080.unk94 = func_8009D000(5, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0078->pc += 8;
}

/* Event: set the panorama backdrop's sky, horizon and ground colours (800b00a0:
 * three RGB triples from operands 1-17), fill scale operand 19, fade range
 * operand 21 and fade start operand 23, and enable it (800b00b2): the field
 * load creates it (8002709c) once the actors' event 0 scripts have run, and
 * 80075484 draws it. */
void func_8008A148(void) {
    D_800B0080.unkA0[0] = func_800ACDEC(1);
    D_800B0080.unkA0[1] = func_800ACDEC(3);
    D_800B0080.unkA0[2] = func_800ACDEC(5);
    D_800B0080.unkA4[0] = func_800ACDEC(7);
    D_800B0080.unkA4[1] = func_800ACDEC(9);
    D_800B0080.unkA4[2] = func_800ACDEC(11);
    D_800B0080.unkA8[0] = func_800ACDEC(13);
    D_800B0080.unkA8[1] = func_800ACDEC(15);
    D_800B0080.unkA8[2] = func_800ACDEC(17);
    D_800B0080.unkAC = func_800ACDEC(19);
    D_800B0080.unkAE = func_800ACDEC(21);
    D_800B0080.unkB0 = func_800ACDEC(23);
    D_800B0078->pc += 25;
    D_800B0080.enabled = 1;
}

/* Event: wait while 800adb88 is set, yielding each time. */
void func_8008A244(void) {
    if (D_800ADB88 == 0) {
        D_800B0078->pc++;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: store the current movie frame (800b06a0, recorded by the movie frame
 * callback 800a7120) in variable operand 1. */
void func_8008A2A0(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B06A0);
    D_800B0078->pc += 3;
}

/* Event fe 77: once the stream is stopped (8008a558; else wait: pc-- to the
 * fe), by byte 1: 0 loads TIM file 0x7fb + operand 5 (selected by flag 0x80 of
 * byte 13, i.e. operand 2 and the flags of the following fe 77 01) into
 * 800b1f74 and advances 2; 1 uploads it (80070340) at x, y = operands 4, 6 with
 * CLUT row operand 8 + 0xe8 (0xff: the TIM's own), selected by flags
 * 0x40/0x20/0x10 of byte 10, first saving the VRAM column at (3c0, 100)
 * (800a915c) when y >= 0x100 and x >= 0x2c0, and advances 11; other values free
 * the image and advance 2. Yields. */
void func_8008A2E8(void) {
    s32 index;
    s32 file;
    s32 clut_y;
    s32 x;
    s32 y;
    s32 row;
    u32 *image;

    if (func_8008A558() == -1) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    if (EVENT_OPERAND_BYTE(1) == 0) {
        index = func_8009CF78(5, EVENT_OPERAND_BYTE(0xD));
        func_80028470(4, 0);
        file = index + 0x7FB;
        image = func_80031BDC(func_800288EC(file), 0);
        D_800B1F74 = image;
        func_800295D8(file, image, 0, 0x80);
        D_800B0078->pc += 2;
    } else if (EVENT_OPERAND_BYTE(1) == 1) {
        row = func_8009D044(8, EVENT_OPERAND_BYTE(0xA));
        clut_y = row + 0xE8;
        if (row == 0xFF) {
            clut_y = -1;
        }
        x = func_8009CFBC(4, EVENT_OPERAND_BYTE(0xA));
        y = func_8009D000(6, EVENT_OPERAND_BYTE(0xA));
        if (y >= 0x100 && x >= 0x2C0) {
            func_800A915C();
        }
        func_80070340(D_800B1F74, x, y, 0, clut_y, 0, 0);
        D_800B0078->pc += 0xB;
    } else {
        func_800320E8(D_800B1F74);
        D_800B0078->pc += 2;
    }
    D_800B00C0 = 1;
}

/* Event fe 7a: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7a (restore every character's EP). */
void func_8008A4E0(void) {
}

/* Event fe 79: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 79 (restore every character's HP). */
void func_8008A4E8(void) {
}

/* Event fe 78: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 78 (read a map's data ahead, re-running
 * until it is loaded). */
void func_8008A4F0(void) {
}

void func_8008A4F8(void) {
}

/* Event fe 7c: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7c (restore the masked party members' EP).
 */
void func_8008A500(void) {
}

/* Event fe 7d: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7d (reduce the masked party members' EP).
 */
void func_8008A508(void) {
}

/* Event fe 7e: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7e (restore the masked party members' EP).
 */
void func_8008A510(void) {
}

/* Event fe 7b: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 7b (reduce the masked party members' HP).
 */
void func_8008A518(void) {
}

/* Wait (VSync) until the disc is idle and the stream is stopped. */
void func_8008A520(void) {
    while (func_8008A558() != 0) {
        VSync(0);
    }
}

/* Stop the stream once no music-wave read runs and the disc is idle; -1
 * while busy. */
s32 func_8008A558(void) {
    if (D_800ADB2C == 0) {
        if (func_800286CC() == 0) {
            func_80028A60(0);
            return 0;
        }
    }
    return -1;
}

/* Event fe 6c: when the byte after this instruction (the next instruction's
 * first byte, not consumed) is 0, clear the pad byte 8005938c (8003633c(0); the
 * controller start sets it to 1); advance 1. */
void func_8008A5A0(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        func_8003633C(0);
    }
    D_800B0078->pc++;
}

/* Event: set the ordering-table depth 800b21d4 (0x720 at load) at which the
 * panorama backdrop is drawn and the second ordering table is linked in,
 * from operand 1. */
void func_8008A604(void) {
    D_800B2078.unk21D4 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event fe 6b: set byte +78 of character record operand 3 (resolved by
 * 8008cf3c; none: unchanged) to operand 1 less its byte +77, at least 0. */
void func_8008A640(void) {
    s32 character = func_8008CF3C(func_800ACDEC(3));
    s32 value;

    if (character != 0xFF) {
        value = func_800ACDEC(1) - D_8005A39C->characters[character].field77;
        if (value < 0) {
            value = 0;
        }
        D_8005A39C->characters[character].field78 = value;
    }
    D_800B0078->pc += 5;
}

/* Event fe 69: store the sum of bytes +77 and +78 of character record operand 3
 * (resolved by 8008cf3c; 0 for none) in variable operand 1. */
void func_8008A6E0(void) {
    s32 character = func_8008CF3C(func_800ACDEC(3));
    s32 sum;

    if (character != 0xFF) {
        sum = D_8005A39C->characters[character].field77 + D_8005A39C->characters[character].field78;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, sum);
    } else {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, 0);
    }
    D_800B0078->pc += 5;
}

/* Find a free (0xff) slot of the table at 80062590 for `id`; -1 when `id`
 * is already there or no slot is free. */
s32 func_8008A790(s32 id, s32 *slot) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_80062590[i] == id) {
            break;
        }
        if (D_80062590[i] == 0xFF) {
            *slot = i;
            return 0;
        }
    }
    return -1;
}

/* Start reading party member `member`'s sprite file for slot `slot` (a
 * character substitute while 8004f34c has 0xc000). */
void func_8008A7DC(s32 member, s32 slot) {
    s32 file;
    s32 size;
    s32 sprite;

    D_800ADBCC = slot;
    D_800ADBC8 = member;
    func_80028470(4, 0);
    if (D_800ADB1C == 0) {
        func_80028A60(0);
    }
    if (!(D_8004F34C & 0xC000)) {
        file = member + 5;
        size = func_800288EC(file);
        D_8006FABC[D_800ADBCC] = member;
        D_800ADBC0 = func_80031BDC(size, 0);
        func_800295D8(file, D_800ADBC0, 0, 0x80);
    } else {
        sprite = func_8001ACF0(member);
        if (sprite == 0xFF) {
            sprite = 0;
        }
        sprite += 0x10;
        file = sprite + 5;
        D_800ADBC0 = func_80031BDC(func_800288EC(file), 0);
        D_8006FABC[D_800ADBCC] = sprite;
        func_800295D8(file, D_800ADBC0, 0, 0x80);
    }
    if (D_800ADB1C == 0) {
        func_80028A60(0);
    }
    D_800ADBC4 = 1;
}

/* Event fe 4d: set the current actor's animation override (+ea, used instead of
 * its own animation unless 0xff) to the complement of byte 1 (byte 0 gives
 * 0xff: no override). */
void func_8008A93C(void) {
    FieldActor *actor;

    actor = D_800B0078;
    actor->unk0EA = ~D_800ADC00[actor->pc + 1];
    actor->pc += 2;
}

/* Event fe 4c: set the current actor's animation override (+ea) to the
 * complement of byte 1 (as fe 4d) and clear layer bit 16, which its sprite's
 * completion callback (80076a74) sets again; opcode 5e waits for that. */
void func_8008A974(void) {
    func_8008A93C();
    D_800B0078->layer_flags &= ~0x10000;
}

/* Event fe 4b: once the stream is stopped (8008a558), clear 800adb90 and make
 * the actor's loaded block (+120) its sprite's resource (80021bf0), advancing
 * 1; otherwise wait (pc-- to the fe). Yields. */
void func_8008A9AC(void) {
    if (func_8008A558() == 0) {
        D_800ADB90 = 0;
        func_80021BF0(D_800AF880.components.descriptors[D_800AFD1C].model, (s32)D_800B0078->unk120);
        D_800B0078->pc++;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: release the actor's block at +120 once, then yield. */
void func_8008AA60(void) {
    if (D_800B0078->unk124 != -1) {
        func_800320E8(D_800B0078->unk120);
        D_800B0078->unk124 = -1;
    }
    D_800B00C0 = 1;
    D_800B0078->pc++;
}

/* Event 0xb0: sound-effect bank, once the stream and disc are idle (8008a558;
 * else pc-- back to fe). Byte 1 1 loads the bank file read before into SPU
 * memory (80037fd8) as slot 800afd18's bank (slot 3 also becomes 800595ac),
 * waits for the transfer, frees the file, yields and advances 2. Other values
 * release slot operand 2's bank (80038310) and start reading file operand 4 + 2
 * of directory (0x1c, 0) (file 6 when bit 0x80 is set and 8004f370 is 1), or
 * with bit 0x80 file (operand 4 & 0x7f) + 0x1f of directory (0x2c, 1); advance
 * 6. */
void func_8008AACC(void) {
    s32 file;
    s32 slot;
    void *data;

    if (func_8008A558() == 0) {
        if (EVENT_OPERAND_BYTE(1) == 1) {
            D_80062518[D_800AFD18] = (s32)func_80037FD8(D_800AFD08, 0);
            func_8003BDFC(0x10);
            func_800320E8(D_800AFD08);
            if (D_800AFD18 == 3) {
                D_800595AC = (SoundSequence *)D_80062524;
            }
            D_800B00C0 = 1;
            D_800B0078->pc += 2;
        } else {
            slot = func_800ACDEC(2);
            D_800AFD18 = slot;
            func_80038310((SoundSequence *)D_80062518[slot]);
            file = func_800ACDEC(4);
            D_800AFD0C = file;
            if (!(file & 0x80)) {
            standard:
                func_80028470(0x1C, 0);
                D_800AFD0C += 2;
            } else if (D_8004F370 == 1) {
                D_800AFD0C = 4;
                goto standard;
            } else {
                D_800AFD0C = (file & 0x7F) + 0x1F;
                func_80028470(0x2C, 1);
            }
            data = func_80031BDC(func_800288EC(D_800AFD0C), 0);
            D_800AFD08 = data;
            func_800295D8(D_800AFD0C, data, 0, 0x80);
            func_80028470(4, 0);
            D_800B0078->pc += 6;
        }
    } else {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
    }
}

/* Event: load file op1 + 0x77a as the actor's block (+120), once the
 * stream and disc are idle; yields. */
void func_8008ACE8(void) {
    s32 number;
    s32 file;
    s32 size;

    if (D_800ADB90 == 0 && D_800ADB2C == 0) {
        if (func_8008A558() != 0) {
            D_800B00C0 = 1;
            D_800B0078->pc--;
            return;
        }
        if (D_800B0078->unk124 != -1) {
            func_800320E8(D_800B0078->unk120);
            D_800B0078->unk124 = -1;
        }
        number = func_800ACDEC(1);
        func_80028470(4, 0);
        file = number + 0x77A;
        size = func_800288EC(file) + 8;
        D_800B0078->unk124 = file;
        D_800B0078->unk120 = func_80031BDC(size, 0);
        func_800295D8(file, D_800B0078->unk120, 0, 0x80);
        if (D_800ADB1C == 0) {
            func_80028A60(0);
        }
        D_800ADB90 = 1;
        D_800B0078->pc += 3;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event: set (selector 0) or clear the actor's layer bit 17. */
void func_8008AE5C(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        D_800B0078->layer_flags |= 0x20000;
    } else {
        D_800B0078->layer_flags &= ~0x20000;
    }
    D_800B0078->pc += 2;
}

/* Event fe 3d: set row op1 of the 801e layers' light matrix (800b221c, passed
 * to 801e7d14) to operands 3, 5 and 7; all four are selected operands (flags
 * 0x80/0x40/0x20/0x10 of byte 9). */
void func_8008AEC8(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);

    D_800B2078.unk221C[index][0] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk221C[index][1] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk221C[index][2] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event fe 3e: set column op1 of the 801e layers' colour matrix (800b223c, the
 * module's 801e8644) to operands 3, 5 and 7; all four are selected operands
 * (flags 0x80/0x40/0x20/0x10 of byte 9). */
void func_8008AFD8(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);

    D_800B2078.unk223C[0][index] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk223C[1][index] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk223C[2][index] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event fe 3f: set the back colour of the 801e layers (800b225c, passed to
 * SetBackColor) to operands 1, 3 and 5. */
void func_8008B0E8(void) {
    D_800B2078.unk225C[0] = func_800ACDEC(1);
    D_800B2078.unk225C[1] = func_800ACDEC(3);
    D_800B2078.unk225C[2] = func_800ACDEC(5);
    D_800B0078->pc += 7;
}

/* Event fe 47: set 800b21b4 from operand 1: the step by which actors with layer
 * flag 0x2000 turn their facing (+108) toward its goal each frame (default
 * 0x80; other actors use their +11e). */
void func_8008B144(void) {
    D_800B2078.unk21B4 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event fe 3c: when 800adb1c is set, call script entry operand 3 of 801e layer
 * actor operand 1 (801e8330) and keep operand 3 in 800b21e4[operand 1], which
 * 800a24c4 replays after a return to the field. */
void func_8008B180(void) {
    s32 index;

    if (D_800ADB1C != 0) {
        index = func_800ACDEC(1) & 0xFFFF;
        func_801E8330(index, 0, func_800ACDEC(3));
        D_800B2078.unk21E4[func_800ACDEC(1)] = func_800ACDEC(3);
    }
    D_800B0078->pc += 5;
}

/* Event fe 5b: set the current actor's turn step (+11e, default 0x200: the step
 * by which its facing (+108) turns toward its goal each frame; actors with
 * layer flag 0x2000 use 800b21b4) from operand 1. */
void func_8008B210(void) {
    D_800B0078->unk11E = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: fade channel 1 towards (op3, op5, op7) over op9 frames with blend
 * op1. */
void func_8008B248(void) {
    s32 steps = func_800ACDEC(9);
    s32 red = func_800ACDEC(3);
    s32 green = func_800ACDEC(5);
    s32 blue = func_800ACDEC(7);

    func_80071D08(1, steps, red, green, blue, func_800ACDEC(1));
    D_800B0078->pc += 11;
}

/* Event fe 26: start the screen distortion (800a484c(0): its buffers and quad
 * grid on first use) and move its six values (x and y amplitude, x and y
 * frequency, x and y phase speed) to operands 1, 3, 5, 7, 9 and 11 over
 * operand-13 frames (800a4cc4); advance 15. */
void func_8008B2F0(void) {
    func_800A484C(0);
    D_800B0078->pc += 15;
}

/* Event fe 27: screen distortion control by byte 1, yielding: 0 releases it
 * (800b207a), moving its six values to 0 over operand-2 frames (800a4cc4),
 * after which the drawer stops it, and advances 4; 1 waits (pc-- to the fe)
 * while it runs (800b2078), then advances 2; 2 clears 800b2078 and advances 2;
 * 3 stops it and frees its buffers (800a47d4) and advances 2. Other values do
 * not advance: the pc stays on the extended byte, which runs as primary 27 when
 * the slot resumes. */
void func_8008B328(void) {
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        func_800A4CC4(0, 0, 0, 0, 0, 0, func_800ACDEC(2));
        D_800B2078.unk207A = 1;
        D_800B0078->pc += 4;
        break;
    case 1:
        if (D_800B2078.unk2078 == 0) {
            D_800B0078->pc += 2;
        } else {
            D_800B0078->pc--;
        }
        break;
    case 2:
        D_800B2078.unk2078 = 0;
        D_800B0078->pc += 2;
        break;
    case 3:
        func_800A47D4();
        D_800B0078->pc += 2;
        break;
    }
    D_800B00C0 = 1;
}

/* Event: set the sprite view rotation from operands 1, 3 and 5 (X, Z, Y;
 * immediate by flags 0x80/0x40/0x20 of byte 7). */
void func_8008B45C(void) {
    D_800B2078.sprite_angles.vx = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 7]);
    D_800B2078.sprite_angles.vz = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 7]);
    D_800B2078.sprite_angles.vy = func_8009D000(5, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0078->pc += 8;
}

/* Event: set the camera orbit angles from operands 1, 3 and 5 (X, Z, Y;
 * immediate by flags 0x80/0x40/0x20 of byte 7). */
void func_8008B518(void) {
    D_800AF880.orbit_angles.vx = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 7]);
    D_800AF880.orbit_angles.vz = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 7]);
    D_800AF880.orbit_angles.vy = func_8009D000(5, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0078->pc += 8;
}

/* Event 0x1b: scroll the current actor's textured polygons by (op1, op3)
 * texels in both draw buffers. Each polygon's two group words are stepped
 * over one at a time (the two steps combine into one add, but count twice
 * in allocation, which gives group s1, other s2, prims s3). */
void func_8008B5D4(void) {
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

    instance = D_800AF880.components.descriptors[D_800AFD1C].instance;
    prims = instance->packets[D_800ADB08];
    mesh = instance->mesh;
    other = instance->packets[(D_800ADB08 + 1) & 1];
    group = (u32 *)mesh->unk10;
    du = (s16)func_800ACD7C(1);
    dv = (s16)func_800ACD7C(3);
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
    D_800B0078->pc += 5;
}

/* Event fe 1a: once a party sprite load is pending (800adbc4) and the disc is
 * idle: stop the stream, unpack the member's sprite file into its slot block
 * (8005a414) and release the read buffer, run the member's join event
 * (8008b978: the actor event 0 that starts with opcode 16 for it), clear the
 * pending load and advance. Otherwise wait (pc-- to the fe), also when no load
 * is pending. Yields. */
void func_8008B894(void) {
    if (D_800ADBC4 != 0xFF && func_800286CC() == 0) {
        func_80028A60(0);
        func_80032EB4(D_800ADBC0, D_8005A414[D_800ADBCC]);
        func_800320E8(D_800ADBC0);
        func_8008B978(D_800ADBC8);
        D_800ADBC4 = 0xFF;
        D_800B00C0 = 1;
        D_800B0078->pc++;
        return;
    }
    D_800B00C0 = 1;
    D_800B0078->pc--;
}

/* Run the join event of party member `member`: the first event actor whose
 * event 0 starts with instruction 0x16 for that member is initialised and
 * run (with the member's own actor while 8004f34c has 0xc000); the running
 * actor's context is restored afterwards. */
void func_8008B978(s32 member) {
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

    D_8005A39C->joined |= 1 << D_800ADBC8;
    descriptor = D_800B06B8;
    actor = D_800B0078;
    if (D_800ADB1C != 0) {
        pc = actor->pc;
        yield = D_800B00C0;
        steps = D_800AFC7C;
        current = D_800AFD1C;
        for (i = 0; i < D_800ADBFC; i++) {
            entry = func_800A3090(i, 0);
            code = &D_800ADC00[entry];
            if (code[0] == 0x16 && code[1] == member) {
                D_800B06B8 = &D_800AF880.components.descriptors[i];
                D_800B0078 = D_800B06B8->actor;
                func_80080A74(i);
                D_800AFD1C = i;
                D_800AF880.components.descriptors[i].actor->pc = entry;
                pc_new = func_800A3090(i, 0);
                D_800AFFEC = 0;
                D_800B0078->pc = pc_new;
                entry = func_800A3090(i, 0);
                func_8008D380(i, D_800B2078.controlled);
                func_800A1EC8(0xFFFF);
                func_80077268();
                D_8005A39C->joined |= 1 << D_800ADBC8;
                if (D_8004F34C & 0xC000) {
                    D_800B06B8 = &D_800AF880.components.descriptors[D_8006F990[D_800ADBCC]];
                    D_800B0078 = D_800B06B8->actor;
                    func_80080A74(D_8006F990[D_800ADBCC]);
                    D_800AF880.components.descriptors[i].actor->pc = entry;
                    D_800AFD1C = D_8006F990[D_800ADBCC];
                    pc_new = func_800A3090(D_8006F990[D_800ADBCC], 0);
                    D_800AFFEC = 0;
                    D_800B0078->pc = pc_new;
                    func_800A1EC8(0xFFFF);
                }
                break;
            }
        }
        D_800B06B8 = descriptor;
        D_800B0078 = actor;
        D_800B00C0 = yield;
        D_800AFD1C = current;
        D_800AFC7C = steps;
        actor->pc = pc;
    }
}

/* Event: once no party sprite load is pending, 800adb2c is clear and the disc
 * is idle (else pc-- back to fe and yield), add character operand 1 to the
 * party: with a free slot read its sprite file (8008a7dc) and advance 3; when
 * it is already in the party or the party is full, mark it waiting (+1d30 bit)
 * and advance 5, two bytes further; operand 1 0xff advances 5. */
void func_8008BC80(void) {
    s32 pending = D_800ADBC4;
    s32 member;
    s32 slot;

    if (pending == 0xFF && D_800ADB2C == 0 && func_8008A558() == 0) {
        func_80028A60(0);
        member = func_800ACDEC(1);
        if (member != pending) {
            if (func_8008A790(member, &slot) == 0) {
                D_8005A39C->inGear[slot] = 0;
                D_80062590[slot] = member;
                func_8008A7DC(member, slot);
                D_800B0078->pc += 3;
                return;
            }
            D_8005A39C->joined |= 1 << member;
            D_800B0078->pc += 5;
            return;
        }
        D_800B0078->pc += 5;
        return;
    }
    D_800B00C0 = 1;
    D_800B0078->pc--;
}

/* Event fe 18: once no party sprite load is pending (800adbc4), no music-wave
 * read runs and the stream is stopped (8008a558; else yield and wait: pc-- to
 * the fe), add the character in byte 1 to the first free party slot (8008a790),
 * clearing the slot's +22b1, and start reading its sprite file (8008a7dc),
 * advancing 2. When it is already in the party or no slot is free, set its bit
 * in the game's +1d30 instead and advance 4, past the next two bytes. */
void func_8008BDD8(void) {
    s32 slot;
    s32 member;

    if (D_800ADBC4 == 0xFF && D_800ADB2C == 0 && func_8008A558() == 0) {
        func_80028A60(0);
        if (func_8008A790(D_800ADC00[D_800B0078->pc + 1], &slot) == 0) {
            D_8005A39C->inGear[slot] = 0;
            member = D_800ADC00[D_800B0078->pc + 1];
            D_80062590[slot] = member;
            func_8008A7DC(member, slot);
            D_800B0078->pc += 2;
            return;
        }
        D_8005A39C->joined |= 1 << D_800ADC00[D_800B0078->pc + 1];
        D_800B0078->pc += 4;
        return;
    }
    D_800B00C0 = 1;
    D_800B0078->pc--;
}

/* Close party slot `slot` up: the next slot's member, sprite data and ids
 * move down (its actor's sprite is set up again for this slot) and the next
 * slot is emptied. */
void func_8008BF38(s32 slot) {
    s32 member;
    s32 kind;
    s32 *sprites;

    member = D_8005A444[slot + 1];
    if (member == 0xFF) {
        D_8006FABC[slot] = D_8006FABC[slot + 1];
        D_80062590[slot] = D_80062590[slot + 1];
        D_8005A444[slot] = D_8005A444[slot + 1];
        D_80062590[slot + 1] = member;
        D_8006FABC[slot + 1] = member;
        D_8005A444[slot + 1] = member;
        return;
    }
    *(PartySprite *)D_8005A414[slot] = *(PartySprite *)D_8005A414[slot + 1];
    D_8006FABC[slot] = D_8006FABC[slot + 1];
    D_80062590[slot] = D_80062590[slot + 1];
    D_8005A444[slot] = D_8005A444[slot + 1];
    kind = D_800AF880.components.descriptors[member].actor->unk126;
    if (!(kind & 0x80)) {
        func_80076AC0(member, slot, D_8005A414[slot], 1, 0, slot, 1);
    } else {
        sprites = D_800AF880.components.sprites;
        func_80076AC0(member, D_800AF880.components.descriptors[member].actor->unk127,
                      (u8 *)(sprites[(kind & 0x7F) + 1] + (s32)sprites),
                      D_800AF880.components.descriptors[member].actor->sprite_kind,
                      D_800AF880.components.descriptors[member].actor->unk134 & 0xF,
                      D_800AF880.components.descriptors[member].actor->unk126,
                      (D_800AF880.components.descriptors[member].actor->unk134 >> 4) & 1);
    }
    D_80062590[slot + 1] = 0xFF;
    D_8006FABC[slot + 1] = 0xFF;
    D_8005A444[slot + 1] = 0xFF;
}

/* Take party slot `slot`'s member out of the party: its actor gets the
 * lead's sprite and is hidden, and the slot's ids are cleared. The
 * descriptor table pointer is read through its address, which the original
 * keeps in s0 across the calls.
 * An empty slot only clears its two other ids (an early return; the
 * duplicated stores give slot * 4 the original's allocation priority). */
void func_8008C180(s32 slot) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    FieldActor *member;
    s32 current;
    u16 pc;
    s32 index;
    FieldDescriptor **table;
    FieldDescriptor *member_descriptor;

    if (D_8005A444[slot] == 0xFF) {
        D_80062590[slot] = 0xFF;
        D_8006FABC[slot] = 0xFF;
        return;
    }
    table = &D_800AF880.components.descriptors;
    descriptor = D_800B06B8;
    actor = D_800B0078;
    current = D_800AFD1C;
    pc = actor->pc;
    D_800B06B8 = &(*table)[D_8005A444[slot]];
    D_800B0078 = D_800B06B8->actor;
    func_80080A74(D_8005A444[slot]);
    index = D_8005A444[slot];
    D_800AFD1C = index;
    member_descriptor = &(*table)[index];
    member_descriptor->flags = (member_descriptor->flags & 0xF07F) | 0x200;
    func_80076AC0(index, 0, D_8005A414[0], 1, 0, 0, 1);
    member = D_800B0078;
    member->flags |= 1;
    member->layer_flags |= 0x100000;
    D_800B00C0 = 0;
    D_800B0078 = actor;
    D_800B06B8 = descriptor;
    D_800AFD1C = current;
    member->pc = pc;
    member->flags |= 0x20000;
    member->layer_flags |= 0x400;
    D_8005A444[slot] = 0xFF;
    D_80062590[slot] = 0xFF;
    D_8006FABC[slot] = 0xFF;
}

/* Event 0x19: remove character op1 from the party once no sprite load is
 * pending. Before the field is set up only the slot tables and sprite data
 * close up; afterwards the member's actor is also released and the lead
 * actor becomes the party leader again. */
void func_8008C334(void) {
    s32 slot;

    if (D_800ADBC4 != 0xFF) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    DrawSync(0);
    slot = func_8009FA00(func_8008CF3C(EVENT_OPERAND_BYTE(1)));
    if (slot != -1) {
        if (D_800ADB1C == 0) {
            switch (slot) {
            case 0:
                if (D_80062590[1] == 0xFF) {
                    D_80062590[0] = 0xFF;
                    D_8006FABC[0] = 0xFF;
                    D_8005A39C->inGear[0] = 0;
                } else {
                    *(PartySprite *)D_8005A414[0] = *(PartySprite *)D_8005A414[1];
                    D_80062590[0] = D_80062590[1];
                    D_8006FABC[0] = D_8006FABC[1];
                    D_80062590[1] = 0xFF;
                    D_8006FABC[1] = 0xFF;
                    D_8005A39C->inGear[0] = D_8005A39C->inGear[1];
                    if (D_80062590[2] != 0xFF) {
                        *(PartySprite *)D_8005A414[1] = *(PartySprite *)D_8005A414[2];
                        D_80062590[1] = D_80062590[2];
                        D_8006FABC[1] = D_8006FABC[2];
                        D_80062590[2] = 0xFF;
                        D_8006FABC[2] = 0xFF;
                        D_8005A39C->inGear[1] = D_8005A39C->inGear[2];
                    }
                }
                break;
            case 1:
                if (D_80062590[2] == 0xFF) {
                    D_80062590[1] = 0xFF;
                    D_8006FABC[1] = 0xFF;
                    D_8005A39C->inGear[1] = 0;
                } else {
                    *(PartySprite *)D_8005A414[1] = *(PartySprite *)D_8005A414[2];
                    D_80062590[1] = D_80062590[2];
                    D_8006FABC[1] = D_8006FABC[2];
                    D_80062590[2] = 0xFF;
                    D_8006FABC[2] = 0xFF;
                    D_8005A39C->inGear[1] = D_8005A39C->inGear[2];
                    D_8005A39C->inGear[2] = 0;
                }
                break;
            case 2:
                D_80062590[2] = 0xFF;
                D_8006FABC[2] = 0xFF;
                D_8005A39C->inGear[2] = 0;
                break;
            }
        } else {
            switch (slot) {
            case 0:
                D_8005A39C->inGear[0] = D_8005A39C->inGear[1];
                D_8005A39C->inGear[1] = D_8005A39C->inGear[2];
                D_8005A39C->inGear[2] = 0;
                func_8008C180(0);
                func_8008BF38(0);
                func_8008BF38(1);
                break;
            case 1:
                D_8005A39C->inGear[1] = D_8005A39C->inGear[2];
                D_8005A39C->inGear[2] = 0;
                func_8008C180(1);
                func_8008BF38(1);
                break;
            case 2:
                D_8005A39C->inGear[2] = 0;
                func_8008C180(2);
                break;
            }
            if (D_80062590[0] != 0xFF && D_8005A444[0] != 0xFF) {
                D_800B2078.controlled = D_8005A444[0];
                D_800AF880.components.descriptors[D_8005A444[0]].actor->flags =
                    (D_800AF880.components.descriptors[D_8005A444[0]].actor->flags | 0x4400) & ~0x80;
            } else {
                D_800B2078.controlled = 0;
            }
        }
    }
    D_800B0078->pc += 2;
}

/* Event fe 16: release the current actor's boundary quadrilateral (+114,
 * allocated by opcode 17 and marked by +12c bit 12) when it holds one. */
void func_8008C7D8(void) {
    if (D_800B0078->state.word & 0x1000) {
        func_800320E8(D_800B0078->unk114);
        D_800B0078->state.word &= ~0x1000;
    }
    D_800B0078->pc++;
}

/* Event fe 0e: with a sequence playing (8004f36c), fade the current music
 * sequence (80062528) to level operand 1 over operand-3 frames (8003a89c: at
 * once for 0 frames; raising the level of a sequence a fade stopped resumes it)
 * and advance. With none playing, advance when no field track is selected
 * (8004f324 0xff) or 800adb1c is clear, else wait (pc-- to the fe). Yields. */
void func_8008C84C(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_800ACDEC(1);
        func_8003A89C((SoundSeq *)D_80062528, a, func_800ACDEC(3));
        D_800B0078->pc += 5;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 5;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event fe 0f: with a sequence playing, shift the current music sequence's
 * pitch to operand 1 semitones over operand-3 frames (8003a948; selected
 * operands, flags 0x80/0x40 of byte 5) and advance; otherwise advance or wait
 * (pc-- to the fe) as fe 0e. Yields. */
void func_8008C938(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 5]);
        func_8003A948((SoundSeq *)D_80062528, a, func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 5]));
        D_800B0078->pc += 6;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 6;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 6;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event fe 10: with a sequence playing, set the current music sequence's tempo
 * to operand 1 (0: 0x100) over operand-3 frames (8003a838) and advance;
 * otherwise advance or wait (pc-- to the fe) as fe 0e. Yields. */
void func_8008CA60(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_800ACDEC(1);
        func_8003A838((SoundSeq *)D_80062528, a, func_800ACDEC(3));
        D_800B0078->pc += 5;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 5;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event fe 11: with a sequence playing, set the current music sequence's pan to
 * operand 1 over operand-3 frames (8003a9bc; selected operands, flags 0x80/0x40
 * of byte 5) and advance; otherwise advance or wait (pc-- to the fe) as fe 0e.
 * Yields. */
void func_8008CB4C(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 5]);
        func_8003A9BC((SoundSeq *)D_80062528, a, func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 5]));
        D_800B0078->pc += 6;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 6;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 6;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event fe 12: with a sequence playing, mute the channels of the current music
 * sequence whose bit is set in operand 1 and unmute the others (8003aac4) and
 * advance; otherwise advance or wait (pc-- to the fe) as fe 0e. Yields. */
void func_8008CC74(void) {
    if (D_8004F36C != 0) {
        func_8003AAC4((SoundSeq *)D_80062528, func_800ACDEC(1));
        D_800B0078->pc += 3;
    } else if (D_8004F324 == 0xFF) {
        D_800B0078->pc += 3;
    } else if (D_800ADB1C == 0) {
        D_800B0078->pc += 3;
    } else {
        D_800B0078->pc--;
    }
    D_800B00C0 = 1;
}

/* Event fe 13: make the current actor a sound emitter: sound operand 1 at
 * volume operand 3 (+10a, +10c), mode 0 (+10d; 0xff, off, when the sound is 0),
 * first stopping the emitter slot that follows it (800863e8). The emitter
 * update 80086590 plays the three nearest emitters. */
void func_8008CD48(void) {
    D_800B0078->sound = func_800ACDEC(1);
    D_800B0078->sound_mode = 0;
    D_800B0078->sound_volume = func_800ACDEC(3);
    D_800B0078->pc += 5;
    func_800863E8(D_800AFD1C);
    if (D_800B0078->sound == 0) {
        D_800B0078->sound_mode = 0xFF;
    }
}

/* Event fe 14: as fe 13 with mode 0x80: sound operand 1 at volume operand 3
 * (+10a, +10c), mode 0x80 (+10d; 0xff when the sound is 0), stopping the
 * emitter slot that follows the actor (800863e8). The field overlay only tests
 * the mode against 0xff (80086590). */
void func_8008CDD4(void) {
    D_800B0078->sound = func_800ACDEC(1);
    D_800B0078->sound_mode = 0x80;
    D_800B0078->sound_volume = func_800ACDEC(3);
    D_800B0078->pc += 5;
    func_800863E8(D_800AFD1C);
    if (D_800B0078->sound == 0) {
        D_800B0078->sound_mode = 0xFF;
    }
}

/* Event fe 3b: clear the bit of character operand 1 (resolved by 8008cf3c:
 * fd/fe/ff the characters in party slots 0/1/2, fc none: unchanged) in the
 * game's +1d32. */
void func_8008CE64(void) {
    s32 member = func_8008CF3C(func_800ACDEC(1));

    if (member != 0xFF) {
        D_8005A39C->available &= ~(1 << member);
    }
    D_800B0078->pc += 3;
}

/* Event fe 3a: set the bit of character operand 1 (resolved by 8008cf3c:
 * fd/fe/ff the characters in party slots 0/1/2, fc none: unchanged) in the
 * game's +1d32. */
void func_8008CED0(void) {
    s32 member = func_8008CF3C(func_800ACDEC(1));

    if (member != 0xFF) {
        D_8005A39C->available |= 1 << member;
    }
    D_800B0078->pc += 3;
}

/* Resolve a character id: 0xfd..0xff name the party members, 0xfc none
 * (0xff). */
s32 func_8008CF3C(s32 id) {
    if (id == 0xFF) {
        return D_80062590[2];
    }
    if (id == 0xFE) {
        return D_80062590[1];
    }
    if (id == 0xFD) {
        return D_80062590[0];
    }
    if (id == 0xFC) {
        return 0xFF;
    }
    return id;
}

/* Event fe 0d: set the current actor's character (+80, the dialogue portrait
 * 8009c5a8 shows) to operand 1 resolved by 8008cf3c (fd/fe/ff: the characters
 * in party slots 0/1/2, fc: none). */
void func_8008CF9C(void) {
    D_800B0078->character = func_8008CF3C(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

/* Event fe 0c: set the six halfwords at 800b21a0 (initially 0x100 three times,
 * then 0x200 three times) from raw operands: [0] op1, [1] op5, [2] op3, [3]
 * op7, [4] op11, [5] op9. */
void func_8008CFEC(void) {
    D_800B2078.unk21A0[0] = func_800ACDB8(1);
    D_800B2078.unk21A0[2] = func_800ACDB8(3);
    D_800B2078.unk21A0[1] = func_800ACDB8(5);
    D_800B2078.unk21A0[3] = func_800ACDB8(7);
    D_800B2078.unk21A0[5] = func_800ACDB8(9);
    D_800B2078.unk21A0[4] = func_800ACDB8(11);
    D_800B0078->pc += 13;
}

/* Event: clear (op1 zero) or set the actor's layer bit 11. */
void func_8008D078(void) {
    if (func_800ACDEC(1) == 0) {
        D_800B0078->layer_flags &= ~0x800;
    } else {
        D_800B0078->layer_flags |= 0x800;
    }
    D_800B0078->pc += 3;
}

/* Event: scale the current actor by op1 and rebuild its matrix. */
void func_8008D0F4(void) {
    s32 scale = func_800ACDEC(1);

    D_800AF880.components.descriptors[D_800AFD1C].model->scale = (u32)(scale * 3) >> 2;
    D_800B0078->scale[0] = scale;
    D_800B0078->scale[1] = scale;
    D_800B0078->scale[2] = scale;
    func_80072254(D_800AFD1C);
    D_800B0078->pc += 3;
}

/* Event: scale the current actor by (op1, op3, op5) and rebuild its matrix. */
void func_8008D180(void) {
    s16 x = func_800ACDEC(1);
    s16 y = func_800ACDEC(3);
    s16 z = func_800ACDEC(5);

    D_800AF880.components.descriptors[D_800AFD1C].model->scale = 0xC00;
    D_800B0078->scale[0] = x;
    D_800B0078->scale[1] = y;
    D_800B0078->scale[2] = z;
    func_80072254(D_800AFD1C);
    D_800B0078->pc += 7;
}

/* Event: set the planar offset scale 800b218c (0x1000 at load; 8007b614
 * scales the x/z offsets it builds by it) from operand 1. */
void func_8008D230(void) {
    D_800B2078.scale = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set the current model's +82 to twice operand 1. */
void func_8008D26C(void) {
    D_800AF880.components.descriptors[D_800AFD1C].model->word82 = func_800ACDEC(1) * 2;
    D_800B0078->pc += 3;
}

/* Event fe 00: empty. It leaves the pc on the extended byte, so the next
 * dispatch runs that byte as primary 00, which ends the script slot. */
void func_8008D2D8(void) {
}

/* Write a halfword into the event bytecode at `offset`. */
void func_8008D2E0(s32 value, s32 offset) {
    D_800ADC00[offset + 1] = value >> 8;
    D_800ADC00[offset] = value;
}

/* -1 when two descriptors are at least 16 apart in X/Z, else 0. */
s32 func_8008D30C(s32 a, s32 b) {
    return -(func_80099A4C(D_800AF880.components.descriptors[a].matrix.t[0] - D_800AF880.components.descriptors[b].matrix.t[0],
                           D_800AF880.components.descriptors[a].matrix.t[2] - D_800AF880.components.descriptors[b].matrix.t[2]) >= 0x10);
}

/* Copy descriptor `from`'s actor placement (collision triangles, layer,
 * position, +50, +14, +72, +ec), model position and +84, and matrix
 * translation onto descriptor `to`. */
void func_8008D380(s32 to, s32 from) {
    FieldActor *target;
    FieldActor *source;
    s32 i;

    source = D_800AF880.components.descriptors[from].actor;
    target = D_800AF880.components.descriptors[to].actor;
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
    D_800AF880.components.descriptors[to].model->ground = D_800AF880.components.descriptors[from].model->ground;
    D_800AF880.components.descriptors[to].model->x = D_800AF880.components.descriptors[from].model->x;
    D_800AF880.components.descriptors[to].model->y = D_800AF880.components.descriptors[from].model->y;
    D_800AF880.components.descriptors[to].model->z = D_800AF880.components.descriptors[from].model->z;
    D_800AF880.components.descriptors[to].matrix.t[0] = D_800AF880.components.descriptors[from].matrix.t[0];
    D_800AF880.components.descriptors[to].matrix.t[1] = D_800AF880.components.descriptors[from].matrix.t[1];
    D_800AF880.components.descriptors[to].matrix.t[2] = D_800AF880.components.descriptors[from].matrix.t[2];
}

/* Clear the current actor's party bit in 800b219f. */
void func_8008D570(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_8005A444[i] == D_800AFD1C) {
            D_800B2078.party_bits &= ~(1 << i);
        }
    }
}

/* Event: set 800b21cd from byte 1: while it is nonzero the camera target's
 * goal height follows the actor (its y - 0x20) instead of the floor. */
void func_8008D5C8(void) {
    D_800B2078.camera_floor_fixed = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: clear (selector 0) or set (selector 1) the actor's layer bit 10. */
void func_8008D604(void) {
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        D_800B0078->layer_flags &= ~0x400;
        break;
    case 1:
        D_800B0078->layer_flags |= 0x400;
        break;
    }
    D_800B0078->pc += 2;
}

/* Event: set flag bit op1 (variable op1 >> 4, bit op1 & 15). */
void func_8008D684(void) {
    u16 reference = func_800ACDB8(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (func_800ACDB8(1) & 0xF);

    func_800A3074(variable & 0xFFFF, func_800A3018(variable) | bit);
    D_800B0078->pc += 3;
}

/* Event: clear flag bit op1 (variable op1 >> 4, bit op1 & 15). */
void func_8008D700(void) {
    u16 reference = func_800ACDB8(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (func_800ACDB8(1) & 0xF);

    func_800A3074(variable & 0xFFFF, func_800A3018(variable) & ~bit);
    D_800B0078->pc += 3;
}

/* Continue (pc + 5) when bit operand 1 & 15 of variable operand 1 >> 4 is
 * set, otherwise jump to operand 3. */
void func_8008D780(void) {
    u16 reference = func_800ACDB8(1);
    u32 variable = reference >> 4;
    s32 bit = 1 << (func_800ACDB8(1) & 0xF);

    if (func_800A3018(variable) & bit) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Write a 0x57 (arc move, mode 0x81) instruction with operands `a`, `b`,
 * `c` and 0x0c at the pc, followed by ff 57 8f 26 01 80 57 0f. */
void func_8008D808(s32 a, s32 b, s32 c) {
    EVENT_OPERAND_BYTE(0) = 0x57;
    EVENT_OPERAND_BYTE(1) = 0x81;
    func_8008D2E0(a, D_800B0078->pc + 2);
    func_8008D2E0(b, D_800B0078->pc + 4);
    func_8008D2E0(c, D_800B0078->pc + 6);
    func_8008D2E0(0xC, D_800B0078->pc + 8);
    EVENT_OPERAND_BYTE(0xA) = 0xFF;
    EVENT_OPERAND_BYTE(0xB) = 0x57;
    EVENT_OPERAND_BYTE(0xC) = 0x8F;
    EVENT_OPERAND_BYTE(0xD) = 0x26;
    EVENT_OPERAND_BYTE(0xE) = 1;
    EVENT_OPERAND_BYTE(0xF) = 0x80;
    EVENT_OPERAND_BYTE(0x10) = 0x57;
    EVENT_OPERAND_BYTE(0x11) = 0xF;
}

/* Write a 0x4b instruction with operands `a` and `b` into the event code at
 * pc+0xc (followed by ff ff 80) and advance the pc by 0xc. */
void func_8008DA04(s32 a, s32 b) {
    EVENT_OPERAND_BYTE(0xC) = 0x4B;
    func_8008D2E0(a, D_800B0078->pc + 0xD);
    func_8008D2E0(b, D_800B0078->pc + 0xF);
    EVENT_OPERAND_BYTE(0x11) = 0xFF;
    EVENT_OPERAND_BYTE(0x12) = 0xFF;
    EVENT_OPERAND_BYTE(0x13) = 0x80;
    D_800B0078->pc += 0xC;
}

/* Event: clear the actor's +75 (0xff). */
void func_8008DAFC(void) {
    D_800B0078->unk075 = 0xFF;
    D_800B0078->pc++;
}

/* Event: make the actor byte 1 selects the one the camera follows
 * (800b233e). */
void func_8008DB2C(void) {
    D_800B2078.unk233E = func_8009CDB4(1);
    D_800B0078->pc += 2;
}

/* Add to party member `member`'s points, capped at its maximum. Declared
 * int but returns nothing. */
s32 func_8008DB68(s32 member, s32 amount) {
    s32 character = func_8001ACF0(D_80062590[member]);

    if (character != 0xFF) {
        D_8005A39C->gears[character].hp += amount;
        if (D_8005A39C->gears[character].maxHp < D_8005A39C->gears[character].hp) {
            D_8005A39C->gears[character].hp = D_8005A39C->gears[character].maxHp;
        }
    }
}

/* Take from party member `member`'s points, leaving at least one. Declared
 * int but returns nothing. */
s32 func_8008DBF0(s32 member, s32 amount) {
    s32 character = func_8001ACF0(D_80062590[member]);
    s32 points;

    if (character != 0xFF) {
        points = D_8005A39C->gears[character].hp - amount;
        if (points <= 0) {
            points = 1;
        }
        D_8005A39C->gears[character].hp = points;
    }
}

/* Event: add operand 1 (immediate by flag 0x80 of byte 3) to the points of the
 * gears (character record byte 0) of the party members that mask table entry
 * byte 3 & 3 names (800aea2c: all, slot 0, 1, 2), capped at their maximum. */
void func_8008DC74(void) {
    s32 i;
    s32 amount = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 3]);
    s32 mask = D_800AEA2C[D_800ADC00[D_800B0078->pc + 3] & 3];

    for (i = 0; i < 3; i++) {
        if (D_80062590[i] != 0xFF && (mask & 1)) {
            func_8008DB68(i, amount);
        }
        mask >>= 1;
    }
    D_800B0078->pc += 4;
}

/* Event: take operand 1 (immediate by flag 0x80 of byte 3) from the points of
 * the gears (character record byte 0) of the party members that mask table
 * entry byte 3 & 3 names (800aea2c: all, slot 0, 1, 2), leaving at least 1. */
void func_8008DD6C(void) {
    s32 i;
    s32 amount = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 3]);
    s32 mask = D_800AEA2C[D_800ADC00[D_800B0078->pc + 3] & 3];

    for (i = 0; i < 3; i++) {
        if (D_80062590[i] != 0xFF && (mask & 1)) {
            func_8008DBF0(i, amount);
        }
        mask >>= 1;
    }
    D_800B0078->pc += 4;
}

/* Set the current actor's +75 to the actor selected by byte 1, if any. */
void func_8008DE64(void) {
    s32 actor = func_8009CDB4(1);

    if (actor != 0xFF) {
        D_800B0078->unk075 = actor;
    }
    D_800B0078->pc += 2;
}

/* Event fe 2c: store the flags (+000, low 16 bits) of the actor selected by
 * byte 1 in variable operand 1; the selector byte is the low byte of that same
 * operand. No actor: nothing is stored. */
void func_8008DEBC(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, actor->flags);
    }
    D_800B0078->pc += 3;
}

/* Event fe 2d: store flag halfword 1 (+002: flags bits 16-31) of the actor
 * selected by byte 1 in variable operand 1; the selector byte is the low byte
 * of that same operand. No actor: nothing is stored. */
void func_8008DF44(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, ACTOR_FLAG_HALF(actor, 1));
    }
    D_800B0078->pc += 3;
}

/* Event fe 2e: store the layer flags (+004, low 16 bits) of the actor selected
 * by byte 1 in variable operand 1; the selector byte is the low byte of that
 * same operand. No actor: nothing is stored. */
void func_8008DFCC(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, actor->layer_flags);
    }
    D_800B0078->pc += 3;
}

/* Event fe 2f: store flag halfword 3 (+006: layer flags bits 16-31) of the
 * actor selected by byte 1 in variable operand 1; the selector byte is the low
 * byte of that same operand. No actor: nothing is stored. */
void func_8008E054(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, ACTOR_FLAG_HALF(actor, 3));
    }
    D_800B0078->pc += 3;
}

/* Continue past a 5-byte instruction when `flags` has any bit of op1, else
 * jump to op4. */
void func_8008E0DC(s32 flags) {
    if (func_800ACDB8(1) & flags & 0xFFFF) {
        D_800B0078->pc += 6;
    } else {
        D_800B0078->pc = func_800ACDB8(4);
    }
}

/* Continue past a 4-byte instruction when `flags` has any bit of op1, else
 * jump to op3. */
void func_8008E148(s32 flags) {
    if (func_800ACDB8(1) & flags & 0xFFFF) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Event: store the planar distance between the actors selected by bytes 3
 * and 4 (0 when either is missing) in variable op1. */
void func_8008E1B4(void) {
    s32 distance = 0;
    s32 a = func_8009CDB4(3);
    s32 b = func_8009CDB4(4);
    s32 ax, az, bx, bz;

    if (a != 0xFF && b != 0xFF) {
        ax = D_800AF880.components.descriptors[a].actor->position[0] >> 16;
        bx = D_800AF880.components.descriptors[b].actor->position[0] >> 16;
        az = D_800AF880.components.descriptors[a].actor->position[2] >> 16;
        bz = D_800AF880.components.descriptors[b].actor->position[2] >> 16;
        distance = func_80099A4C(ax - bx, az - bz);
    }
    func_800A3074(func_800ACDB8(1) & 0xFFFF, distance);
    D_800B0078->pc += 5;
}

/* Event fe 34: continue (advance 6) when flag halfword 0 (+000: flags bits
 * 0-15) of the actor selected by byte 3 (not checked for none) has any bit of
 * raw operand 1, else jump to operand 4 (8008e0dc). */
void func_8008E298(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 0));
}

/* Event fe 35: continue (advance 6) when flag halfword 1 (+002: flags bits
 * 16-31) of the actor selected by byte 3 (not checked for none) has any bit of
 * raw operand 1, else jump to operand 4 (8008e0dc). */
void func_8008E2EC(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 1));
}

/* Event fe 36: continue (advance 6) when flag halfword 2 (+004: layer flags
 * bits 0-15) of the actor selected by byte 3 (not checked for none) has any bit
 * of raw operand 1, else jump to operand 4 (8008e0dc). */
void func_8008E340(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 2));
}

/* Event fe 37: continue (advance 6) when flag halfword 3 (+006: layer flags
 * bits 16-31) of the actor selected by byte 3 (not checked for none) has any
 * bit of raw operand 1, else jump to operand 4 (8008e0dc). */
void func_8008E394(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 3));
}

/* Event fe 30: continue (advance 5) when the current actor's flag halfword 0
 * (+000: flags bits 0-15) has any bit of raw operand 1, else jump to operand 3
 * (8008e148). */
void func_8008E3E8(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 0));
}

/* Event fe 31: continue (advance 5) when the current actor's flag halfword 1
 * (+002: flags bits 16-31) has any bit of raw operand 1, else jump to operand 3
 * (8008e148). */
void func_8008E414(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 1));
}

/* Event fe 32: continue (advance 5) when the current actor's flag halfword 2
 * (+004: layer flags bits 0-15) has any bit of raw operand 1, else jump to
 * operand 3 (8008e148). */
void func_8008E440(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 2));
}

/* Event fe 33: continue (advance 5) when the current actor's flag halfword 3
 * (+006: layer flags bits 16-31) has any bit of raw operand 1, else jump to
 * operand 3 (8008e148). */
void func_8008E46C(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 3));
}

/* Store `value` in variable op1. */
void func_8008E498(s32 value) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value & 0xFFFF);
    D_800B0078->pc += 3;
}

/* Event: store the current actor's flag halfword 0 in variable op1. */
void func_8008E4EC(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 0));
}

/* Event: store the current actor's flag halfword 1 in variable op1. */
void func_8008E518(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 1));
}

/* Event: store the current actor's flag halfword 2 in variable op1. */
void func_8008E544(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 2));
}

/* Event: store the current actor's flag halfword 3 in variable op1. */
void func_8008E570(void) {
    func_8008E498(ACTOR_FLAG_HALF(D_800B0078, 3));
}

/* Event: by selector byte 1, set (0-3) or clear (4-7) raw operand 2 in the
 * low or high half of the actor's flag word or layer flag word. */
void func_8008E59C(void) {
    u32 bits = func_800ACDB8(2) & 0xFFFF;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        D_800B0078->flags |= bits;
        break;
    case 1:
        D_800B0078->flags |= bits << 16;
        break;
    case 2:
        D_800B0078->layer_flags |= bits;
        break;
    case 3:
        D_800B0078->layer_flags |= bits << 16;
        break;
    case 4:
        D_800B0078->flags &= ~bits;
        break;
    case 5:
        D_800B0078->flags &= ~(bits << 16);
        break;
    case 6:
        D_800B0078->layer_flags &= ~bits;
        break;
    case 7:
        D_800B0078->layer_flags &= ~(bits << 16);
        break;
    }
    D_800B0078->pc += 4;
}

/* Draw 800b229c distinct random numbers 1..(800b2298 + 1) into 800b22a0
 * (none when the count is zero, which also clears the range). */
void func_8008E718(void) {
    s32 i;
    s32 j;
    s32 pick;

    D_800B2078.unk2294 = D_800B2078.unk2298;
    if (D_800B2078.unk229C == 0) {
        D_800B2078.unk2298 = 0;
        return;
    }
    for (i = 0; i < 32; i++) {
        D_800B2078.unk22A0[i] = 0xFFFF;
    }
    for (i = 0; i < D_800B2078.unk229C; i++) {
    retry:
        pick = (rand() * (D_800B2078.unk2298 + 1)) >> 15 & 0xFFFF;
        for (j = 0; j < 32; j++) {
            if (D_800B2078.unk22A0[j] == pick) {
                goto retry;
            }
        }
        D_800B2078.unk22A0[i] = pick;
    }
    for (i = 0; i < D_800B2078.unk229C; i++) {
        D_800B2078.unk22A0[i]++;
    }
}

/* Set the range 800b2298 (operand 1) and count 800b229c (operand 3, at most
 * 32), then draw that many distinct random numbers 1..range + 1 into
 * 800b22a0 (8008e718; a zero count only clears the range). */
void func_8008E85C(void) {
    D_800B2078.unk2298 = func_800ACDEC(1);
    D_800B2078.unk229C = func_800ACDEC(3);
    if (D_800B2078.unk229C > 0x20) {
        D_800B2078.unk229C = 0x20;
    }
    func_8008E718();
    D_800B0078->pc += 5;
}

/* Event: by selector byte, clear (0) or set (1, keeping the heading goal)
 * flag 0x8000, or set layer bit 19 (2); clearing also stops a model moving
 * under bit 19. */
void func_8008E8C8(void) {
    Sprite *model;

    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        if (D_800B0078->flags & 0x8000) {
            D_800B0078->flags &= ~0x8000;
        }
        if (D_800B0078->layer_flags & 0x80000) {
            model = D_800AF880.components.descriptors[D_800AFD1C].model;
            model->speed = 0;
            model->speed_z = 0;
            model->speed_x = 0;
            D_800B0078->layer_flags &= ~0x80000;
        }
        break;
    case 1:
        D_800B0078->flags |= 0x8000;
        D_800B0078->unk11C = D_800B0078->heading_goal;
        break;
    case 2:
        D_800B0078->layer_flags |= 0x80000;
        break;
    }
    D_800B0078->pc += 2;
}

/* Event 61: wait until the field movie player (800a7c58) has started
 * presenting frames (800adb7c), clearing the flag once seen; yield each
 * time. */
void func_8008E9F8(void) {
    if (D_800ADB7C == 0) {
        D_800B0078->pc--;
    } else {
        D_800ADB7C = 0;
        D_800B0078->pc++;
    }
    D_800B00C0 = 1;
}

/* Event 0xa0: while 800adbdc is clear (a battle request is ending the field)
 * pc-- back to fe and yield; otherwise request a movie: file op1, the
 * parameters at 800c3a2a/2c/2e from op3/op5/op7 and its sound bank op9
 * (selected operands, flags byte 11), with the default window and fade;
 * yields. */
void func_8008EA58(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    FIELD_MOVIE.file = func_8009CF78(1, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.unk2A = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.sound_start = func_8009D000(5, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.unk2E = func_8009D044(7, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.sound_bank = func_8009D088(9, EVENT_OPERAND_BYTE(0xB));
    FIELD_MOVIE.width = 0x140;
    D_800ADB80 = 0x40;
    FIELD_MOVIE.height = 0x100;
    FIELD_MOVIE.source_x = 0;
    FIELD_MOVIE.depth24 = 1;
    FIELD_MOVIE.y = 0;
    FIELD_MOVIE.x = 0;
    FIELD_MOVIE.source_y = 0x100;
    FIELD_MOVIE.unk3A = 0;
    FIELD_MOVIE.mode &= 0xF;
    D_800ADB74 = 0;
    D_800ADB70 = 1;
    D_800B00C0 = 1;
    D_800B0078->pc += 0xC;
}

/* Event 0x60: once sound is available, request movie op1 with parameters
 * op3/op5; op7's low nibble picks the display layout (0: half-width at
 * x 0x140, 1: full 16-bit, 2: full 24-bit) and its 0xc0 bits the fade. */
void func_8008EC30(void) {
    s32 mode;

    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    FIELD_MOVIE.file = func_800ACDEC(1);
    FIELD_MOVIE.unk2A = func_800ACDEC(3);
    FIELD_MOVIE.unk2E = func_800ACDEC(5);
    mode = FIELD_MOVIE.mode = func_800ACDEC(7);
    FIELD_MOVIE.width = 0x140;
    FIELD_MOVIE.height = 0x100;
    FIELD_MOVIE.mode &= 0xF;
    D_800ADB80 = mode & 0xC0;
    FIELD_MOVIE.sound_start = 1;
    switch (FIELD_MOVIE.mode) {
    case 0:
        FIELD_MOVIE.x = 0x140;
        FIELD_MOVIE.y = 0;
        FIELD_MOVIE.source_x = 0x140;
        FIELD_MOVIE.source_y = 0x100;
        D_800ADB74 = 1;
        FIELD_MOVIE.depth24 = 0;
        break;
    case 1:
        FIELD_MOVIE.source_x = 0;
        FIELD_MOVIE.y = 0;
        FIELD_MOVIE.x = 0;
        FIELD_MOVIE.source_y = 0x100;
        D_800ADB74 = 0;
        FIELD_MOVIE.depth24 = 0;
        break;
    case 2:
        FIELD_MOVIE.source_x = 0;
        FIELD_MOVIE.y = 0;
        FIELD_MOVIE.x = 0;
        FIELD_MOVIE.source_y = 0x100;
        D_800ADB74 = 0;
        FIELD_MOVIE.depth24 = 1;
        break;
    }
    FIELD_MOVIE.sound_bank = 0xFF;
    FIELD_MOVIE.unk3A = 0;
    D_800ADB70 = 1;
    D_800B00C0 = 1;
    D_800B0078->pc += 9;
}

/* Event 0x67: request movie op1 with parameters op3/op5/op7, mode op9 (0xfe
 * marks 800c3a3a, 0x40 the fade), window position op11/op13 and size
 * op15/op17, without a sound bank. */
void func_8008EE14(void) {
    s32 mode;
    u16 x;
    u16 y;

    FIELD_MOVIE.file = func_800ACDEC(1);
    FIELD_MOVIE.unk2A = func_800ACDEC(3);
    FIELD_MOVIE.sound_start = func_800ACDEC(5);
    FIELD_MOVIE.unk2E = func_800ACDEC(7);
    FIELD_MOVIE.mode = func_800ACDEC(9);
    mode = FIELD_MOVIE.mode;
    if (mode == 0xFE) {
        FIELD_MOVIE.unk3A = 1;
    } else {
        FIELD_MOVIE.unk3A = 0;
    }
    x = func_800ACDEC(0xB);
    FIELD_MOVIE.x = x;
    FIELD_MOVIE.source_x = x;
    y = func_800ACDEC(0xD);
    FIELD_MOVIE.y = y;
    FIELD_MOVIE.source_y = y;
    FIELD_MOVIE.width = func_800ACDEC(0xF);
    FIELD_MOVIE.height = func_800ACDEC(0x11);
    D_800ADB80 = mode & 0x40;
    FIELD_MOVIE.sound_bank = 0xFF;
    D_800ADB74 = 2;
    FIELD_MOVIE.depth24 = 0;
    FIELD_MOVIE.mode &= 0xF;
    D_800ADB70 = 1;
    D_800B0078->pc += 0x13;
}

/* Event: request screen transition 1 over operand 1 frames (800adb38/800adb3c):
 * 800a5924 resets the screen effect view (800a4748) and fades the screen pieces
 * out. */
void func_8008EF5C(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 1;
    D_800B0078->pc += 3;
}

/* Event: request screen transition 2 over operand 1 frames (800adb38/800adb3c):
 * 800a5924 fades the screen pieces out. */
void func_8008EFA0(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 2;
    D_800B0078->pc += 3;
}

/* Event: set both draw buffers' clip areas to x operand 1, y operand 3, width
 * operand 5 and height operand 7 (80071f64; the second buffer's lies 0x100
 * lines lower). */
void func_8008EFE4(void) {
    s32 x = func_800ACDEC(1);
    s32 y = func_800ACDEC(3);
    s32 w = func_800ACDEC(5);

    func_80071F64(x, y, w, func_800ACDEC(7));
    D_800B0078->pc += 9;
}

extern s32 D_800ADB38;
extern s32 D_800ADB3C;

/* Event: request screen transition 3 over operand 1 frames (800adb38/800adb3c):
 * 800a5924 fades the screen pieces in and holds them while the request stays
 * 3. */
void func_8008F070(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 3;
    D_800B0078->pc += 3;
}

/* Set the sprite tints of the actor byte 2 selects: byte 1 bit 0 sets its first
 * triple (+fc) and bit 1 its second (+ff) to operands 3, 5 and 7. */
void func_8008F0B4(void) {
    FieldActor *actor;
    s32 index;

    index = func_8009CDB4(2);
    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        if (EVENT_OPERAND_BYTE(1) & 1) {
            actor->color0[0] = func_800ACDEC(3);
            actor->color0[1] = func_800ACDEC(5);
            actor->color0[2] = func_800ACDEC(7);
        }
        if (EVENT_OPERAND_BYTE(1) & 2) {
            actor->color1[0] = func_800ACDEC(3);
            actor->color1[1] = func_800ACDEC(5);
            actor->color1[2] = func_800ACDEC(7);
        }
    }
    D_800B0078->pc += 9;
}

/* Event fe 5f: set the current actor's colour triple +fc (bit 0 of byte 1)
 * and/or +ff (bit 1) to operands 2, 4 and 6. */
void func_8008F1C8(void) {
    if (EVENT_OPERAND_BYTE(1) & 1) {
        D_800B0078->color0[0] = func_800ACDEC(2);
        D_800B0078->color0[1] = func_800ACDEC(4);
        D_800B0078->color0[2] = func_800ACDEC(6);
    }
    if (EVENT_OPERAND_BYTE(1) & 2) {
        D_800B0078->color1[0] = func_800ACDEC(2);
        D_800B0078->color1[1] = func_800ACDEC(4);
        D_800B0078->color1[2] = func_800ACDEC(6);
    }
    D_800B0078->pc += 8;
}


/* Event fe 5e: set the blend rate of the current actor's sprite to operand 1 &
 * 7 (80023290; 0 clears its blending flag). */
void func_8008F2D8(void) {
    s32 value = func_800ACDEC(1);

    func_80023290(D_800AF880.components.descriptors[D_800AFD1C].model, value);
    D_800B0078->pc += 3;
}

extern s32 D_800C3A5C;
extern s32 D_800C3A60;

/* Set the screen margins 800c3a5c (x) and 800c3a60 (y) from operands 1 and 3:
 * 800aaa74 widens the screen by them when testing whether a field model
 * instance is visible. */
void func_8008F348(void) {
    D_800C3A5C = func_800ACDEC(1);
    D_800C3A60 = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Set 800b233c from operand 1: bit i keeps sound effect 2 * i playing when
 * 800864f0 stops the field's sounds (it stops the other effects' two channels,
 * 8003a20c, and shifts the bits out). */
void func_8008F394(void) {
    D_800B2078.effects_kept = func_800ACDEC(1);
    D_800B0078->pc += 3;
}


/* Event: slide the volume of the two effect channels of sound 2 * operand 3 to
 * operand 1 over operand 5 frames (resident 8003a450). */
void func_8008F3D0(void) {
    s32 a;
    s32 b;

    a = func_800ACDEC(3) * 2;
    b = func_800ACDEC(1);
    func_8003A450(a, b, func_800ACDEC(5));
    D_800B0078->pc += 7;
}


/* Event fe 62: set the volume of the two effect channels of voice pair operand
 * 3 (channels (2 * op3) ^ 8 and the next, while they play) to operand 1
 * (8003a344). */
void func_8008F444(void) {
    s32 a;

    a = func_800ACDEC(3) * 2;
    func_8003A344(a, func_800ACDEC(1));
    D_800B0078->pc += 5;
}


/* Event fe 63: set the pan of the two effect channels of voice pair operand 3
 * (channels (2 * op3) ^ 8 and the next, while they play) to operand 1
 * (8003a55c). */
void func_8008F4A0(void) {
    s32 a;

    a = func_800ACDEC(3) * 2;
    func_8003A55C(a, func_800ACDEC(1));
    D_800B0078->pc += 5;
}

/* Event fe 65: play sound effect operand 1 on effect voice pair operand 3 at
 * full volume and centre pan, recording it in 800b2078 last_sound_effect;
 * effect 0 stops the pair instead (80085634). */
void func_8008F4FC(void) {
    s32 a;

    a = func_800ACDEC(1);
    func_80085634(a, func_800ACDEC(3));
    D_800B0078->pc += 5;
}

void func_800855C8(s32 a, s32 b, s32 c, s32 d);

/* Event fe 66: play sound effect operand 1 on effect voice pair operand 7 at
 * volume operand 5 and pan operand 3, stopping the pair first (800855c8). */
void func_8008F558(void) {
    s32 a;
    s32 b;
    s32 c;

    a = func_800ACDEC(1);
    b = func_800ACDEC(5);
    c = func_800ACDEC(3);
    func_800855C8(a, b, c, func_800ACDEC(7));
    D_800B0078->pc += 9;
}


/* Event fe 64: wait (pc-- to the fe) while any effect channel whose bit is set
 * in operand 1 << 8 is playing (8003a5d0(-1): the mask of active effect
 * channels), then advance. Yields. */
void func_8008F5E4(void) {
    s32 buttons;

    buttons = func_8003A5D0(-1);
    if (!(buttons & (func_800ACDEC(1) << 8))) {
        D_800B0078->pc += 3;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

/* Play sound effect operand 1 on voice pair 3 at full volume and centre pan
 * (80085634; 0 stops the pair). */
void func_8008F668(void) {
    func_80085634(func_800ACDEC(1), 3);
    D_800B0078->pc += 3;
}

/* Event fe 5d: play sound effect operand 1 on effect voice pair 3 at volume
 * operand 5 and pan operand 3, stopping the pair first (800855c8). */
void func_8008F6AC(void) {
    s32 a;
    s32 b;

    a = func_800ACDEC(1);
    b = func_800ACDEC(5);
    func_800855C8(a, b, func_800ACDEC(3), 3);
    D_800B0078->pc += 7;
}

extern s32 D_800ADBDC;
void func_8008F7B8(void);

/* Select field music track operand 1 (8008f7b8) with 8004f340 = 0 (its sequence
 * starts silent). With 800adb1c clear (scripts run at setup) the track is only
 * recorded, stopping the old music when it differs; otherwise it yields while
 * the disc stream or a wave load is busy or a change is pending, then stops the
 * old track and starts the new one when it differs (80085b20). Continues at +3;
 * yields while music is disabled (800adbdc clear). */
void func_8008F724(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        return;
    }
    D_8004F340 = 0;
    func_8008F7B8();
}

/* Select field music track operand 1 as 72 with 8004f340 = -1 (its sequence
 * starts at full volume); yields while music is disabled (800adbdc clear). */
void func_8008F76C(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        return;
    }
    D_8004F340 = -1;
    func_8008F7B8();
}

void func_80085EEC(void);
s32 func_8008A558(void);

/* Select the field music track (operand 1). Without D_800ADB1C the track is
 * only recorded; otherwise yield until the music system can take a change.
 * The two "busy" yields are separate branches that GCC merges into one tail
 * after the change branch, storing the comparison's constant 1. */
void func_8008F7B8(void) {
    s32 track;

    track = func_800ACDEC(1);
    if (D_800ADB1C == 0) {
        func_80085EEC();
        if (track != D_8004F324) {
            func_8001B66C();
            D_8004F308 = -1;
        }
        D_8004F324 = track;
        D_800B0078->pc += 3;
    } else if (func_8008A558() != 0 || D_800ADBDC == 0) {
        D_800B00C0 = 1;
    } else if (D_8004F354 == 1) {
        D_800B00C0 = 1;
    } else if (D_8004F308 != -1) {
        if (track != D_8004F324) {
            func_8001B66C();
            D_8004F324 = track;
            D_8004F308 = -1;
            func_80085B20(track, 0);
        }
        D_800B0078->pc += 3;
    } else {
        D_800B00C0 = 1;
    }
}

/* Start a camera shake toward amplitudes x operand 1, y operand 3 and z
 * operand 5 over operand-7 frames (at least 1); all zero ends the shake
 * (stop flag, two extra frames). */
void func_8008F90C(void) {
    s32 x;
    s32 y;
    s32 z;
    s32 frames;

    x = func_800ACDEC(1);
    y = func_800ACDEC(3);
    z = func_800ACDEC(5);
    frames = func_800ACDEC(7);
    if (frames == 0) {
        frames = 1;
    }
    D_800B0078->pc += 9;
    D_800AF880.shake = 1;
    D_800AF880.shake_time = frames;
    D_800AF880.shake_step[0] = ((x << 16) - D_800AF880.shake_amplitude[0]) / frames;
    D_800AF880.shake_step[1] = ((z << 16) - D_800AF880.shake_amplitude[1]) / frames;
    D_800AF880.shake_step[2] = ((y << 16) - D_800AF880.shake_amplitude[1]) / frames;
    if (x == 0 && z == 0 && y == 0) {
        D_800AF880.shake_time = frames + 2;
        D_800AF880.shake_stop = 1;
        return;
    }
    D_800AF880.shake_stop = 0;
}

/* Yield; advance once the camera moves operand 1 names (bit 1: target, bit
 * 0: eye) have no steps left. */
void func_8008FA38(void) {
    s32 mask;
    s32 wanted;

    wanted = func_800ACDEC(1);
    mask = 3;
    if (D_800AF880.target_steps == 0) {
        mask = 2;
    }
    if (D_800AF880.eye_steps == 0) {
        mask &= 1;
    }
    D_800B00C0 = 1;
    if (!(mask & wanted)) {
        D_800B0078->pc += 3;
    }
}

/* Event fe 6e: set the camera heading (heading_angles.vy and heading) from
 * selected operand 1 (immediate by flag 0x80 of byte 3). */
void func_8008FABC(void) {
    s16 heading;

    heading = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    D_800AF880.heading_angles.vy = heading;
    D_800AF880.heading = heading;
    D_800B0078->pc += 4;
}

/* Copy the working elevation/heading/zoom into the scripted camera. */
void func_8008FB28(void) {
    D_800AF880.scripted_scale = 0x1000;
    D_800AF880.scripted_heading = D_800AF880.heading_angles.vy;
    D_800AF880.scripted_elevation = D_800AF880.elevation;
    D_800AF880.scripted_zoom = (D_800AF880.projection * D_800AF880.distance) >> 12;
    D_800B0078->pc += 1;
}

/* Switch to the scripted camera now, from the working parameters. */
void func_8008FB98(void) {
    D_800AF880.mode = 1;
    D_800AFC7C += 4;
    D_800B0078->pc += 1;
    D_800AF880.scripted_scale = 0x1000;
    D_800AF880.target_a = 12;
    D_800AF880.target_b = 12;
    D_800AF880.flags |= 0x8000;
    D_800AF880.scripted_heading = D_800AF880.heading_angles.vy;
    D_800AF880.scripted_elevation = D_800AF880.elevation;
    D_800AF880.scripted_zoom = (D_800AF880.projection * D_800AF880.distance) >> 12;
}


/* Leave the scripted camera by its mode: 0 clears the hold flag; 1 with
 * operand 1 zero returns to mode 0 at once (hold flag cleared, camera
 * counter 2) and also skips the next 3 bytes (pc + 6), otherwise blends
 * back over operand-1 frames (mode 2). In mode 2 it neither advances nor
 * yields until the blend ends. */
void func_8008FC4C(void) {
    s32 frames;

    switch (D_800AF880.mode) {
    case 0:
        D_800AF880.flags &= 0x7FFF;
        D_800B0078->pc += 3;
        break;
    case 1:
        frames = func_800ACDEC(1);
        if (frames == 0) {
            D_800AF880.mode = 0;
            D_800AF880.flags &= 0x7FFF;
            D_800B0078->pc += 3;
            D_800B2078.camera_counter = 2;
        } else {
            D_800AF880.mode = 2;
            D_800AF880.target_a = frames;
            D_800AF880.target_b = frames;
        }
        D_800B0078->pc += 3;
        break;
    case 2:
        break;
    }
}

/* Set the scripted camera's two blend frame counts (800af880 target_a and
 * target_b) to operands 1 and 3, at least 1; raises the batch limit. */
void func_8008FD40(void) {
    s32 frames;

    D_800AF880.target_a = func_800ACDEC(1);
    frames = func_800ACDEC(3);
    D_800AF880.target_b = frames;
    if (D_800AF880.target_a == 0) {
        D_800AF880.target_a = 1;
    }
    if (frames == 0) {
        D_800AF880.target_b = 1;
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 5;
}

/* Save the target goal. */
void func_8008FDD0(void) {
    D_800AF880.saved_target.vx = D_800AF880.target_goal.vx;
    D_800AF880.saved_target.vy = D_800AF880.target_goal.vy;
    D_800AF880.saved_target.vz = D_800AF880.target_goal.vz;
    D_800AFC7C += 1;
    D_800B0078->pc += 1;
}

/* Set the saved camera target to selected operands 1/3/5 as x/z/y (flags byte
 * 7, bits 0x80/0x40/0x20; whole units). */
void func_8008FE2C(void) {
    D_800AF880.saved_target.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_target.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_target.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Set camera point A to the position of the actor of selector byte 1 (the party
 * leader when none, 8009cd7c). */
void func_8008FF04(void) {
    FieldActor *actor;

    actor = D_800AF880.components.descriptors[func_8009CD7C(1)].actor;
    D_800AF880.point_actor_a.vx = actor->position[0];
    D_800AF880.point_actor_a.vy = actor->position[1];
    D_800AF880.point_actor_a.vz = actor->position[2];
    D_800AFC7C += 1;
    D_800B0078->pc += 2;
}

/* Set camera point A to selected operands 1/3/5 as x/z/y (flags byte 7, bits
 * 0x80/0x40/0x20; whole units). */
void func_8008FF90(void) {
    D_800AF880.point_actor_a.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_a.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_a.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Save the eye goal. */
void func_80090068(void) {
    D_800AF880.saved_eye.vx = D_800AF880.eye_goal.vx;
    D_800AF880.saved_eye.vy = D_800AF880.eye_goal.vy;
    D_800AF880.saved_eye.vz = D_800AF880.eye_goal.vz;
    D_800AFC7C += 1;
    D_800B0078->pc += 1;
}

/* Set the saved camera eye to selected operands 1/3/5 as x/z/y (flags byte 7,
 * bits 0x80/0x40/0x20; whole units). */
void func_800900C4(void) {
    D_800AF880.saved_eye.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_eye.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_eye.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Set camera point B to the position of the actor of selector byte 1 (the party
 * leader when none, 8009cd7c). */
void func_8009019C(void) {
    FieldActor *actor;

    actor = D_800AF880.components.descriptors[func_8009CD7C(1)].actor;
    D_800AF880.point_actor_b.vx = actor->position[0];
    D_800AF880.point_actor_b.vy = actor->position[1];
    D_800AF880.point_actor_b.vz = actor->position[2];
    D_800AFC7C += 1;
    D_800B0078->pc += 2;
}

/* Set point B to selected operands 1, 3, 5 as x, z, y in whole units
 * (flags byte 7: 0x80, 0x40, 0x20); raises the batch limit. */
void func_80090228(void) {
    D_800AF880.point_actor_b.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_b.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.point_actor_b.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Reset the saved/point targets to the target goal and eyes to the eye goal. */
void func_80090300(void) {
    D_800AF880.point_actor_a.vx = D_800AF880.saved_target.vx = D_800AF880.target_goal.vx;
    D_800AF880.point_actor_a.vy = D_800AF880.saved_target.vy = D_800AF880.target_goal.vy;
    D_800AF880.point_actor_a.vz = D_800AF880.saved_target.vz = D_800AF880.target_goal.vz;
    D_800AF880.saved_eye.vx = D_800AF880.eye_goal.vx;
    D_800AF880.saved_eye.vy = D_800AF880.eye_goal.vy;
    D_800AF880.saved_eye.vz = D_800AF880.eye_goal.vz;
    D_800AF880.point_actor_b.vx = D_800AF880.eye_goal.vx;
    D_800AF880.point_actor_b.vy = D_800AF880.eye_goal.vy;
    D_800AF880.point_actor_b.vz = D_800AF880.eye_goal.vz;
    D_800AFC7C += 1;
    D_800B0078->pc += 1;
}

/* Event 0xac: start a scripted camera move. Mode 0 (1) moves the target
 * (eye) from its saved point to point A (B) over op2 steps; mode 2 (3) moves
 * it along the same line at op2 units per step. Byte-1 bit 0x80 also snaps
 * the live target (eye) to the start. */
void func_800903BC(void) {
    s32 mode;
    VECTOR delta;
    VECTOR direction;
    s32 distance;
    s32 speed;

    mode = EVENT_OPERAND_BYTE(1) & 0xF;
    switch (mode) {
    case 0:
        D_800AF880.target_steps = func_800ACDEC(2);
        if (D_800AF880.target_steps == 0) {
            D_800AF880.target_steps++;
            D_800AF880.target_a = 1;
        }
        D_800AF880.target_step.vx = (D_800AF880.point_actor_a.vx - D_800AF880.saved_target.vx) / D_800AF880.target_steps;
        D_800AF880.target_step.vy = (D_800AF880.point_actor_a.vy - D_800AF880.saved_target.vy) / D_800AF880.target_steps;
        D_800AF880.target_step.vz = (D_800AF880.point_actor_a.vz - D_800AF880.saved_target.vz) / D_800AF880.target_steps;
        D_800AF880.scripted_target.vx = D_800AF880.saved_target.vx;
        D_800AF880.scripted_target.vy = D_800AF880.saved_target.vy;
        D_800AF880.scripted_target.vz = D_800AF880.saved_target.vz;
        D_800AF880.scripted |= 1;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.target.vx = D_800AF880.saved_target.vx;
            D_800AF880.target.vy = D_800AF880.saved_target.vy;
            D_800AF880.target.vz = D_800AF880.saved_target.vz;
        }
        break;
    case 2:
        delta.vx = (D_800AF880.saved_target.vx - D_800AF880.point_actor_a.vx) >> 16;
        delta.vy = (D_800AF880.saved_target.vy - D_800AF880.point_actor_a.vy) >> 16;
        delta.vz = (D_800AF880.saved_target.vz - D_800AF880.point_actor_a.vz) >> 16;
        VectorNormal(&delta, &direction);
        distance = func_80099A04((D_800AF880.saved_target.vx - D_800AF880.point_actor_a.vx) >> 16,
                                 (D_800AF880.saved_target.vy - D_800AF880.point_actor_a.vy) >> 16,
                                 (D_800AF880.saved_target.vz - D_800AF880.point_actor_a.vz) >> 16);
        speed = func_800ACDEC(2);
        D_800AF880.target_step.vx = -(direction.vx * speed) * 16;
        D_800AF880.target_step.vy = -(direction.vy * speed) * 16;
        D_800AF880.target_step.vz = -(direction.vz * speed) * 16;
        D_800AF880.scripted_target.vx = D_800AF880.saved_target.vx;
        D_800AF880.scripted_target.vy = D_800AF880.saved_target.vy;
        D_800AF880.scripted_target.vz = D_800AF880.saved_target.vz;
        D_800AF880.scripted |= 1;
        D_800AF880.target_steps = distance / speed;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.target.vx = D_800AF880.saved_target.vx;
            D_800AF880.target.vy = D_800AF880.saved_target.vy;
            D_800AF880.target.vz = D_800AF880.saved_target.vz;
        }
        break;
    case 3:
        delta.vx = (D_800AF880.saved_eye.vx - D_800AF880.point_actor_b.vx) >> 16;
        delta.vy = (D_800AF880.saved_eye.vy - D_800AF880.point_actor_b.vy) >> 16;
        delta.vz = (D_800AF880.saved_eye.vz - D_800AF880.point_actor_b.vz) >> 16;
        VectorNormal(&delta, &direction);
        distance = func_80099A04((D_800AF880.saved_eye.vx - D_800AF880.point_actor_b.vx) >> 16,
                                 (D_800AF880.saved_eye.vy - D_800AF880.point_actor_b.vy) >> 16,
                                 (D_800AF880.saved_eye.vz - D_800AF880.point_actor_b.vz) >> 16);
        speed = func_800ACDEC(2);
        D_800AF880.eye_step[0] = -(direction.vx * speed) * 16;
        D_800AF880.eye_step[1] = -(direction.vy * speed) * 16;
        D_800AF880.eye_step[2] = -(direction.vz * speed) * 16;
        D_800AF880.scripted_eye[0] = D_800AF880.saved_eye.vx;
        D_800AF880.scripted_eye[1] = D_800AF880.saved_eye.vy;
        D_800AF880.scripted_eye[2] = D_800AF880.saved_eye.vz;
        D_800AF880.scripted |= 2;
        D_800AF880.eye_steps = distance / speed;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.eye.vx = D_800AF880.saved_eye.vx;
            D_800AF880.eye.vy = D_800AF880.saved_eye.vy;
            D_800AF880.eye.vz = D_800AF880.saved_eye.vz;
        }
        break;
    case 1:
        D_800AF880.eye_steps = func_800ACDEC(2);
        if (D_800AF880.eye_steps == 0) {
            D_800AF880.eye_steps++;
            D_800AF880.target_b = 1;
        }
        D_800AF880.eye_step[0] = (D_800AF880.point_actor_b.vx - D_800AF880.saved_eye.vx) / D_800AF880.eye_steps;
        D_800AF880.eye_step[1] = (D_800AF880.point_actor_b.vy - D_800AF880.saved_eye.vy) / D_800AF880.eye_steps;
        D_800AF880.eye_step[2] = (D_800AF880.point_actor_b.vz - D_800AF880.saved_eye.vz) / D_800AF880.eye_steps;
        D_800AF880.scripted_eye[0] = D_800AF880.saved_eye.vx;
        D_800AF880.scripted_eye[1] = D_800AF880.saved_eye.vy;
        D_800AF880.scripted_eye[2] = D_800AF880.saved_eye.vz;
        D_800AF880.scripted |= 2;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.eye.vx = D_800AF880.saved_eye.vx;
            D_800AF880.eye.vy = D_800AF880.saved_eye.vy;
            D_800AF880.eye.vz = D_800AF880.saved_eye.vz;
        }
        break;
    }
    D_800B0078->pc += 4;
}

/* Store the camera target's whole x, z, y in three variables. */
void func_80090A10(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, WHOLE(D_800AF880.target.vx));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, WHOLE(D_800AF880.target.vz));
    func_800A3074(func_800ACDB8(5) & 0xFFFF, WHOLE(D_800AF880.target.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Store the camera eye's whole x, z, y in three variables. */
void func_80090A94(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, WHOLE(D_800AF880.eye.vx));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, WHOLE(D_800AF880.eye.vz));
    func_800A3074(func_800ACDB8(5) & 0xFFFF, WHOLE(D_800AF880.eye.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Store the camera target goal's whole x, z, y in variables operands 1, 3
 * and 5; raises the batch limit. */
void func_80090B18(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, WHOLE(D_800AF880.target_goal.vx));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, WHOLE(D_800AF880.target_goal.vz));
    func_800A3074(func_800ACDB8(5) & 0xFFFF, WHOLE(D_800AF880.target_goal.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Store the camera eye goal's whole x, z, y in variables operands 1, 3 and
 * 5; raises the batch limit. */
void func_80090B9C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, WHOLE(D_800AF880.eye_goal.vx));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, WHOLE(D_800AF880.eye_goal.vz));
    func_800A3074(func_800ACDB8(5) & 0xFFFF, WHOLE(D_800AF880.eye_goal.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* With byte 3 zero store the scripted camera heading in variable operand 1,
 * otherwise set it to raw operand 1; raises the batch limit. */
void func_80090C20(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_heading);
    } else {
        D_800AF880.scripted_heading = func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* With byte 3 zero store the scripted camera elevation in variable operand
 * 1, otherwise set it to raw operand 1; raises the batch limit. */
void func_80090CB8(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_elevation);
    } else {
        D_800AF880.scripted_elevation = func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* With byte 3 zero store the scripted camera zoom in variable operand 1,
 * otherwise set it to raw operand 1; raises the batch limit. */
void func_80090D50(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_zoom);
    } else {
        D_800AF880.scripted_zoom = (u16)func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Store the scripted camera heading, elevation and zoom in variables
 * operands 1, 3 and 5. */
void func_80090DEC(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_heading);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_800AF880.scripted_elevation);
    func_800A3074(func_800ACDB8(5) & 0xFFFF, D_800AF880.scripted_zoom);
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}


/* Derive the scripted heading, pitch and zoom (half the distance) from
 * point A looking at point B, reset the scripted scale and store them in
 * variables operands 1, 3 and 5. */
void func_80090E70(void) {
    VECTOR a;
    VECTOR b;
    s32 heading;
    s32 pitch;
    s32 zoom;

    a.vx = D_800AF880.point_actor_a.vx;
    a.vy = D_800AF880.point_actor_a.vy;
    a.vz = D_800AF880.point_actor_a.vz;
    b.vx = D_800AF880.point_actor_b.vx;
    b.vy = D_800AF880.point_actor_b.vy;
    b.vz = D_800AF880.point_actor_b.vz;
    zoom = func_80099A04((b.vx - a.vx) >> 16, (b.vy - a.vy) >> 16,
                              (b.vz - a.vz) >> 16) / 2;
    D_800AF880.scripted_scale = 0x1000;
    heading = ((-ratan2(a.vz - b.vz, a.vx - b.vx) & 0xFFFF) - 0x400) & 0xFFF;
    pitch = ((-ratan2(func_80099A4C((b.vx - a.vx) >> 16, (b.vz - a.vz) >> 16),
                             (a.vy - b.vy) >> 16) * 360) >> 12) + 91;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, heading);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, pitch);
    func_800A3074(func_800ACDB8(5) & 0xFFFF, zoom);
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Rotate `point` about `center` in the XZ plane by `angle` (result mirrored
 * through the center, as the original subtracts center - point). */
void func_80091008(VECTOR *point, VECTOR *center, s32 angle) {
    MATRIX m;
    VECTOR offset;
    VECTOR rotated;
    SVECTOR angles;

    angles.vx = 0;
    angles.vy = angle;
    angles.vz = 0;
    PushMatrix();
    func_8003F738(&angles, &m);
    offset.vx = center->vx - point->vx;
    offset.vy = center->vy - point->vy;
    offset.vz = center->vz - point->vz;
    ApplyMatrixLV(&m, &offset, &rotated);
    point->vx = rotated.vx + center->vx;
    point->vz = rotated.vz + center->vz;
    PopMatrix();
}

void func_80091008(VECTOR *point, VECTOR *center, s32 angle);

/* Place a point around the centre (selected operands 1, 3, 5: x, z, y) at
 * heading 7, elevation 9 and distance 11 (selected, flags byte 13) and store
 * its whole x, z, y in variables operands 14, 16 and 18. */
void func_800910C0(void) {
    VECTOR center;
    VECTOR point;
    s32 heading;
    s32 elevation;
    s32 angle;
    s32 distance;

    center.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(13)) << 16;
    center.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(13)) << 16;
    center.vy = func_8009D000(5, EVENT_OPERAND_BYTE(13)) << 16;
    heading = func_8009D044(7, EVENT_OPERAND_BYTE(13));
    elevation = func_8009D088(9, EVENT_OPERAND_BYTE(13));
    distance = func_8009D0CC(11, EVENT_OPERAND_BYTE(13));
    angle = ((elevation * 0xB60) >> 8) + 0xC00;
    point.vy = ((-((func_8003F8CC(angle) * distance) << 5)) >> 16) * D_800AF880.scripted_scale * 16 + center.vy;
    point.vz = (((func_8003F8B0(angle) * distance) << 5) >> 16) * D_800AF880.scripted_scale * 16 + center.vz;
    point.vx = center.vx;
    func_80091008(&point, &center, heading);
    func_800A3074(func_800ACDB8(14) & 0xFFFF, WHOLE(point.vx));
    func_800A3074(func_800ACDB8(16) & 0xFFFF, WHOLE(point.vz));
    func_800A3074(func_800ACDB8(18) & 0xFFFF, WHOLE(point.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 20;
}

/* As eb around camera point byte 1 (0 saved target, 1 point A, 2 saved eye,
 * 3 point B): heading, elevation and distance from selected operands 2, 4,
 * 6 (flags byte 8); x, z, y go to variables operands 9, 11 and 13. */
void func_80091318(void) {
    VECTOR center;
    VECTOR point;
    s32 heading;
    s32 elevation;
    s32 angle;
    s32 distance;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        center.vx = D_800AF880.saved_target.vx;
        center.vy = D_800AF880.saved_target.vy;
        center.vz = D_800AF880.saved_target.vz;
        break;
    case 1:
        center.vx = D_800AF880.point_actor_a.vx;
        center.vy = D_800AF880.point_actor_a.vy;
        center.vz = D_800AF880.point_actor_a.vz;
        break;
    case 2:
        center.vx = D_800AF880.saved_eye.vx;
        center.vy = D_800AF880.saved_eye.vy;
        center.vz = D_800AF880.saved_eye.vz;
        break;
    case 3:
        center.vx = D_800AF880.point_actor_b.vx;
        center.vy = D_800AF880.point_actor_b.vy;
        center.vz = D_800AF880.point_actor_b.vz;
        break;
    }
    heading = func_8009CF78(2, EVENT_OPERAND_BYTE(8));
    elevation = func_8009CFBC(4, EVENT_OPERAND_BYTE(8));
    distance = func_8009D000(6, EVENT_OPERAND_BYTE(8));
    angle = ((elevation * 0xB60) >> 8) + 0xC00;
    point.vy = ((-((func_8003F8CC(angle) * distance) << 5)) >> 16) * D_800AF880.scripted_scale * 16 + center.vy;
    point.vz = (((func_8003F8B0(angle) * distance) << 5) >> 16) * D_800AF880.scripted_scale * 16 + center.vz;
    point.vx = center.vx;
    func_80091008(&point, &center, heading);
    func_800A3074(func_800ACDB8(9) & 0xFFFF, WHOLE(point.vx));
    func_800A3074(func_800ACDB8(11) & 0xFFFF, WHOLE(point.vz));
    func_800A3074(func_800ACDB8(13) & 0xFFFF, WHOLE(point.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 15;
}

/* Store camera point byte 1 (0 saved target, 1 point A, 2 saved eye, 3
 * point B) as whole x, z, y in variables operands 2, 4 and 6. */
void func_800915C4(void) {
    VECTOR point;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        point.vx = D_800AF880.saved_target.vx;
        point.vy = D_800AF880.saved_target.vy;
        point.vz = D_800AF880.saved_target.vz;
        break;
    case 1:
        point.vx = D_800AF880.point_actor_a.vx;
        point.vy = D_800AF880.point_actor_a.vy;
        point.vz = D_800AF880.point_actor_a.vz;
        break;
    case 2:
        point.vx = D_800AF880.saved_eye.vx;
        point.vy = D_800AF880.saved_eye.vy;
        point.vz = D_800AF880.saved_eye.vz;
        break;
    case 3:
        point.vx = D_800AF880.point_actor_b.vx;
        point.vy = D_800AF880.point_actor_b.vy;
        point.vz = D_800AF880.point_actor_b.vz;
        break;
    }
    func_800A3074(func_800ACDB8(2) & 0xFFFF, WHOLE(point.vx));
    func_800A3074(func_800ACDB8(4) & 0xFFFF, WHOLE(point.vz));
    func_800A3074(func_800ACDB8(6) & 0xFFFF, WHOLE(point.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Copy camera point operand 1 into camera point operand 2. */
void func_80091720(void) {
    VECTOR point;

    switch (EVENT_OPERAND_BYTE(1)) {
    case 0:
        point.vx = D_800AF880.saved_target.vx;
        point.vy = D_800AF880.saved_target.vy;
        point.vz = D_800AF880.saved_target.vz;
        break;
    case 1:
        point.vx = D_800AF880.point_actor_a.vx;
        point.vy = D_800AF880.point_actor_a.vy;
        point.vz = D_800AF880.point_actor_a.vz;
        break;
    case 2:
        point.vx = D_800AF880.saved_eye.vx;
        point.vy = D_800AF880.saved_eye.vy;
        point.vz = D_800AF880.saved_eye.vz;
        break;
    case 3:
        point.vx = D_800AF880.point_actor_b.vx;
        point.vy = D_800AF880.point_actor_b.vy;
        point.vz = D_800AF880.point_actor_b.vz;
        break;
    }
    switch (EVENT_OPERAND_BYTE(2)) {
    case 0:
        D_800AF880.saved_target.vx = point.vx;
        D_800AF880.saved_target.vy = point.vy;
        D_800AF880.saved_target.vz = point.vz;
        break;
    case 1:
        D_800AF880.point_actor_a.vx = point.vx;
        D_800AF880.point_actor_a.vy = point.vy;
        D_800AF880.point_actor_a.vz = point.vz;
        break;
    case 2:
        D_800AF880.saved_eye.vx = point.vx;
        D_800AF880.saved_eye.vy = point.vy;
        D_800AF880.saved_eye.vz = point.vz;
        break;
    case 3:
        D_800AF880.point_actor_b.vx = point.vx;
        D_800AF880.point_actor_b.vy = point.vy;
        D_800AF880.point_actor_b.vz = point.vz;
        break;
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 3;
}

void func_80073E38(void);

/* Set the fog colour (operands 1, 3, 5), far colour (7, 9, 11) and fog
 * range (13, 15), set 800b218e (sprite gate) and reselect every shown
 * model's drawing mode (80073e38). */
void func_80091944(void) {
    D_800B2078.fog_color[0] = func_800ACDEC(1);
    D_800B2078.fog_color[1] = func_800ACDEC(3);
    D_800B2078.fog_color[2] = func_800ACDEC(5);
    D_800B2078.far_color[0] = func_800ACDEC(7);
    D_800B2078.far_color[1] = func_800ACDEC(9);
    D_800B2078.far_color[2] = func_800ACDEC(11);
    D_800B2078.fog_range[0] = func_800ACDEC(13);
    D_800B2078.fog_range[1] = func_800ACDEC(15);
    D_800B2078.sprite_gate = 1;
    func_80073E38();
    D_800B0078->pc += 17;
}

/* Set the four camera bounds to signed operands 1, 3, 5 and minus operand
 * 7. */
void func_80091A08(void) {
    D_800AF880.bounds[0] = func_800ACD7C(1);
    D_800AF880.bounds[1] = func_800ACD7C(3);
    D_800AF880.bounds[2] = func_800ACD7C(5);
    D_800AF880.bounds[3] = -func_800ACD7C(7);
    D_800B0078->pc += 9;
}


/* Set the clear colour (800b219c) from operands 1, 3 and 5. */
void func_80091A78(void) {
    D_800B2078.clear_color[0] = func_800ACDEC(1);
    D_800B2078.clear_color[1] = func_800ACDEC(3);
    D_800B2078.clear_color[2] = func_800ACDEC(5);
    D_800B0078->pc += 7;
}

/* Event e4: empty. It neither advances nor yields, so the interpreter runs
 * it again until the pass's batch limit (or the 0x400 loop error); the
 * script stays on it. */
void func_80091AD4(void) {
}

/* The unit's own uninitialized variables, which only 80091adc reads: the
 * first of the field BSS (800af5e8-800af76c), the resident clearing it from
 * 800af5e4 with a pre-increment loop. */
static RECT D_800AF5E8[32];
static s16 D_800AF6E8[32];
static s16 D_800AF728[32];
static u16 D_800AF768;

/* Record a VRAM rectangle in the 32-slot ring at 800af5e8 and move it to
 * (dx, dy), or clear it to black when `clear` is set. */
void func_80091ADC(s32 x, s32 y, s32 w, s32 h, s32 dx, s32 dy, s32 clear) {
    s32 slot;

    slot = D_800AF768 & 0x1F;
    D_800AF5E8[slot].y = y;
    D_800AF5E8[slot].x = x;
    D_800AF5E8[slot].w = w;
    D_800AF5E8[slot].h = h;
    D_800AF6E8[slot] = dx;
    D_800AF728[slot] = dy;
    if (clear == 0) {
        MoveImage(&D_800AF5E8[slot], D_800AF6E8[slot], (s16)dy);
    } else {
        ClearImage(&D_800AF5E8[slot], 0, 0, 0);
    }
    D_800AF768++;
}

/* Event 0xe1: with op1 and op3 both zero, clear the VRAM rectangle
 * (op5, op7, op9, op11) to black; otherwise move the rectangle at (op1, op3)
 * of size op5 x op7 to (op9, op11). Selected operands, flags byte 13. */
void func_80091BBC(void) {
    s32 x;
    s32 y;
    RECT unused; /* the original frame holds an unused 8-byte local */

    x = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    y = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    if (x == 0 && y == 0) {
        func_80091ADC(func_8009D000(5, EVENT_OPERAND_BYTE(0xD)), func_8009D044(7, EVENT_OPERAND_BYTE(0xD)),
                      func_8009D088(9, EVENT_OPERAND_BYTE(0xD)), func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD)), 0, 0, 1);
    } else {
        func_80091ADC(x, y, func_8009D000(5, EVENT_OPERAND_BYTE(0xD)), func_8009D044(7, EVENT_OPERAND_BYTE(0xD)),
                      func_8009D088(9, EVENT_OPERAND_BYTE(0xD)), func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD)), 0);
    }
    D_800B0078->pc += 0xE;
}

/* Set the current actor's sprite draw mode (+134 bits 5-6) to selected
 * operand 1 & 3 and +ee to selected operand 3 (flags byte 5: 0x80, 0x40);
 * bit 5 draws the sprite with colour 0 through 8001e2f8 and bit 6 with
 * colour 1 through 8001e368, both at height +ee. */
void func_80091E00(void) {
    D_800B0078->unk134 = (D_800B0078->unk134 & ~0x60) | ((func_8009CF78(1, EVENT_OPERAND_BYTE(5)) & 3) << 5);
    D_800B0078->unkEE = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    D_800B0078->pc += 6;
}

/* As dd for the actor selected by byte 1: sprite draw mode (+134 bits 5-6)
 * = selected operand 2 & 3, +ee = selected operand 4 (flags byte 6). */
void func_80091E98(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->unk134 = (actor->unk134 & ~0x60) | ((func_8009CF78(2, EVENT_OPERAND_BYTE(6)) & 3) << 5);
        actor->unkEE = func_8009CFBC(4, EVENT_OPERAND_BYTE(6));
    }
    D_800B0078->pc += 7;
}

/* When the current descriptor is animated (flag 0x2000), set entry operand
 * 1 of the actor's channel word list (+118, which its model's animation
 * channels read through 80080a18) to operand 3, at most 0xfff. */
void func_80091F84(void) {
    s32 index;
    s32 value;

    index = func_800ACDEC(1);
    value = func_800ACDEC(3);
    if (value >= 0x1000) {
        value = 0xFFF;
    }
    if (D_800AF880.components.descriptors[D_800AFD1C].flags & 0x2000) {
        D_800B0078->list[index] = value;
    }
    D_800B0078->pc += 5;
}

/* Swap variables operand 1 and operand 3. */
void func_80092044(void) {
    s32 first;
    s32 second;

    first = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    second = func_800A3018(func_800ACDB8(3) & 0xFFFF);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, first);
    func_800A3074(func_800ACDB8(1) & 0xFFFF, second);
    D_800B0078->pc += 5;
}

/* Pass every handle in the list at 800afea8 to resident 80027EAC. */
void func_800920D8(void) {
    s32 i;

    for (i = 0; i < D_800AFEA8.count; i++) {
        func_80027EAC(D_800AFEA8.scrolls[i]);
    }
}

/* Event fe 40: store the low byte of raw operand 5 at index operand 3 of the
 * buffer of window-list entry operand 1 (800afea8, created by opcode da), when
 * the index is below the entry's length. */
void func_80092148(void) {
    s32 table;
    s32 index;
    s32 value;

    table = func_800ACDEC(1);
    index = func_800ACDEC(3);
    value = func_800ACDB8(5) & 0xFFFF;
    if (index < D_800AFEA8.lengths[table]) {
        D_800AFEA8.buffers[table][index] = value;
    }
    D_800B0078->pc += 7;
}

extern s32 D_800ADB8C;
void func_80027D64(TextureScroll *scroll, s16 x, s16 y, s16 width, s16 height, s16 length, s16 a, s16 b,
                   u8 *buffer);

/* Add a texture scroll (80027d64) to the list at 800afea8 (at most 32, not
 * while 800adb8c is set): area operands 1, 3, 5, 7 (x, y, w, h), operand 9
 * bands, source x operand 11 and y operand 13, every band's speed byte set
 * to operand 15. */
void func_800921E8(void) {
    s32 length;
    s32 fill;
    s32 i;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
    s32 a;

    if (D_800ADB8C == 0 && D_800AFEA8.count < 32) {
        length = func_800ACDB8(9) & 0xFFFF;
        D_800AFEA8.lengths[D_800AFEA8.count] = length;
        D_800AFEA8.buffers[D_800AFEA8.count] = func_80031BDC(length + 1, 0);
        D_800AFEA8.scrolls[D_800AFEA8.count] = func_80031BDC(0x18, 0);
        fill = func_800ACDB8(15) & 0xFFFF;
        for (i = 0; i < length; i++) {
            D_800AFEA8.buffers[D_800AFEA8.count][i] = fill;
        }
        x = (s16)func_800ACDB8(1);
        y = (s16)func_800ACDB8(3);
        width = (s16)func_800ACDB8(5);
        height = (s16)func_800ACDB8(7);
        a = (s16)func_800ACDB8(11);
        func_80027D64(D_800AFEA8.scrolls[D_800AFEA8.count], x, y, width, height, length, a,
                      func_800ACDB8(13), D_800AFEA8.buffers[D_800AFEA8.count]);
        D_800AFEA8.count++;
    }
    D_800B0078->pc += 17;
}

/* No operation. */
void func_800923E4(void) {
    D_800B0078->pc += 1;
}

/* No operation. */
void func_80092404(void) {
    D_800B0078->pc += 1;
}


/* Return byte `which` of collision attribute `index`. */
s32 func_80092424(s32 index, s32 which) {
    switch (which) {
    case 0:
        return D_800AF880.components.collision_attributes[index].bytes[0];
    case 1:
        return D_800AF880.components.collision_attributes[index].bytes[1];
    case 2:
        return D_800AF880.components.collision_attributes[index].bytes[2];
    case 3:
        return D_800AF880.components.collision_attributes[index].bytes[3];
    }
    return 0;
}

/* Replace byte `which` of collision attribute `index`. */
void func_800924D4(s32 index, s32 which, s32 value) {
    switch (which) {
    case 0:
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & ~0xFF) | value;
        break;
    case 1:
        value <<= 8;
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & 0xFFFF00FF) | value;
        break;
    case 2:
        value <<= 16;
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & 0xFF00FFFF) | value;
        break;
    case 3:
        value <<= 24;
        D_800AF880.components.collision_attributes[index].word = (D_800AF880.components.collision_attributes[index].word & 0x00FFFFFF) | value;
        break;
    }
}


/* Set 800b217c to operand 1 and the text speed to 8, 6 or 4 for values 0,
 * 1 and 2 (others keep it). */
void func_800925A0(void) {
    s32 mode;

    mode = func_800ACDEC(1);
    D_800B2078.unk217C = mode;
    switch (mode) {
    case 0:
        D_800B2078.text_speed = 8;
        break;
    case 1:
        D_800B2078.text_speed = 6;
        break;
    case 2:
        D_800B2078.text_speed = 4;
        break;
    }
    D_800B0078->pc += 3;
}


/* Set the field input mask (800b217a; ANDed into the held, pressed and
 * repeated buttons each frame) to raw operand 1. */
void func_80092628(void) {
    D_800B2078.input_mask = func_800ACDB8(1);
    D_800B0078->pc += 3;
}

/* Replace byte (byte 2, 0-3) of collision attribute record (byte 1) with
 * operand 3 (800924d4). */
void func_80092664(void) {
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), func_800ACDEC(3));
    D_800B0078->pc += 5;
}

/* OR operand 3 into byte (byte 2) of collision attribute record (byte 1). */
void func_800926C8(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value |= func_800ACDEC(3);
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    D_800B0078->pc += 5;
}

/* AND operand 3 into byte (byte 2) of collision attribute record (byte 1). */
void func_80092768(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value &= func_800ACDEC(3);
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    D_800B0078->pc += 5;
}

extern FieldDescriptor *D_800B06B8;

/* Offset the current actor's interaction point (+60/+64, added to its position
 * by the talk and touch triggers and the 80092894 walk goal) 36 units along the
 * published descriptor's facing (rotation y) and set its layer flag 0x800. */
void func_80092808(void) {
    FieldActor *actor;
    s32 step;

    D_800B0078->unk60 = (func_8003F8CC(D_800B06B8->rotation.vy) * 36) >> 12;
    step = -(func_8003F8B0(D_800B06B8->rotation.vy) * 36) >> 12;
    actor = D_800B0078;
    actor->unk64 = step;
    actor->layer_flags |= 0x800;
    actor->pc++;
}

/* Walk the controlled actor one step toward (x, z); with `mode` 0 the goal
 * is 40 units along direction `angle` from the running actor (publishing
 * the field id first when 800adbec asks). Returns -1 while walking (mode 1
 * retries the instruction) and 0 once it has arrived or is stuck, when it
 * stops, turns and the instruction continues. */
s32 func_80092894(s32 angle, s32 mode, s32 x, s32 z) {
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

    D_800B2078.encounter_inhibition = -1;
    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    model = D_800AF880.components.descriptors[D_800B2078.controlled].model;
    player->layer_flags |= 0x38;
    model->speed = 0x80000;
    reach = func_80099A8C(8) * 2;
    from_x = WHOLE(player->position[0]);
    from_z = WHOLE(player->position[2]);
    if (mode == 0) {
        if (D_800ADBDC == 0 || D_800ADBE4 == 0) {
            D_800B00C0 = 1;
        }
        if (D_800ADBEC != 0) {
            value = func_800ACDEC(4);
            field = func_800ACDEC(2);
            func_80092F44();
            D_800ADBEC = 0;
            func_800A3074(2, value);
            D_8004F34C = field;
        }
        direction = D_800B06B8->rotation.vy + angle - 0x400;
        goal_x = D_800B0078->unk60 + WHOLE(D_800B0078->position[0]) + ((func_8003F8CC(direction) * 40) >> 12);
        goal_z = D_800B0078->unk64 + WHOLE(D_800B0078->position[2]) + (-(func_8003F8B0(direction) * 40) >> 12);
    } else {
        goal_x = x;
        goal_z = z;
    }
    dx = goal_x - from_x;
    dz = goal_z - from_z;
    delta.vx = dx;
    delta.vy = 0;
    delta.vz = dz;
    if (reach < func_80099A4C(dx, dz)) {
        goto walk;
    }
arrive:
    player->heading_goal = player->heading = player->heading_goal | 0x8000;
    model->speed = 0;
    player->unkE8 = 0;
    func_800821F4(model, 0, &D_800AF880.components.descriptors[D_800B2078.controlled]);
    D_800B00C0 = 1;
    player->slots[player->slot].value = 0xFFFF;
    player->slots[player->slot].move_mode = 0;
    player->flags &= ~0x200000;
    if (mode == 1) {
        player->layer_flags &= ~0x38;
    }
    player->stuck = 0;
    D_800B0078->pc += 6;
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
    player->heading_goal = player->heading = func_8007B694(&delta);
    D_800B00C0 = 1;
    if (mode != 0) {
        D_800B0078->pc -= 1;
    }
    return -1;
}

/* Event fe 68: once field control allows it (800adbdc and 800adbe4 set,
 * 800adb2c and 800adb90 clear, 8004f308 not -1; else wait: pc-- to the fe),
 * walk the controlled actor one step toward x, z = selected operands 1 and 3
 * (flags 0x80/0x40 of byte 5; 800a0c4c sets its flag 0x80, 80092894 mode 1
 * waits with pc-- while walking), saving its flags in 800b2350 first; on
 * arrival advance 6 and clear its flag 0x80 again unless the saved flags had
 * it. */
void func_80092C20(void) {
    s32 x;
    s32 z;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
        D_800B0078->pc--;
        return;
    }
    x = func_8009CF78(1, EVENT_OPERAND_BYTE(5));
    z = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    if (D_800B2078.unk2350 == 0) {
        D_800B2078.unk2350 = D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags;
    }
    func_800A0C4C();
    if (func_80092894(0, 1, x, z) == 0) {
        if (!(D_800B2078.unk2350 & 0x80)) {
            D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags &= ~0x80;
        }
        D_800B2078.unk2350 = 0;
    }
}

extern s32 D_800ADBE4;
extern s32 D_800ADB2C;
extern s32 D_800ADB90;
s32 func_80092894(s32 a, s32 b, s32 c, s32 d);

/* Once field control allows it (yielding until then), set flag 0x80 on the
 * controlled actor and walk it a step per frame toward the point 40 units
 * from the running actor (plus its +60/+64 step) along its descriptor's
 * facing (80092894 mode 0, angle 0); on arrival or after 0x40 stuck steps
 * continue (pc + 6). With 800adbec set it first records the departure as
 * 98 does (field operand 2, entry operand 4). */
void func_80092DFC(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800A0C4C();
        func_80092894(0, 0, 0, 0);
    }
}

/* Once field control allows it (yield otherwise), set the controlled actor's
 * flag 0x80 (800a0c4c) and walk it (80092894 mode 0) toward the point 40 units
 * from this actor (plus its +60/+64) along this descriptor's facing - 0x20,
 * first requesting the change to map operand 2, entry operand 4 while none is
 * pending (800adbec); yields while walking and continues at +6 on arrival or
 * once stuck for more than 64 frames. Byte 1 is not read. */
void func_80092EA0(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800A0C4C();
        func_80092894(0x3E0, 0, 0, 0);
    }
}

s32 func_8009744C(void);
s32 func_8009A514(void);

/* Publish the current field id and two values in variables 4, 6 and 8 and
 * count variable 0x12 up. */
void func_80092F44(void) {
    func_800A3074(4, D_8004F34C & 0x3FFF);
    func_800A3074(6, func_8009744C() & 0xFFFF);
    func_800A3074(8, func_8009A514() & 0xFFFF);
    func_800A3074(0x12, (s16)(func_800A3018(0x12) + 1));
}

extern s32 D_800ADBD8;
extern s32 D_800B0064;

/* Event: when 800adbd8 is set, inhibit encounters, clear it and set 800b0064 to
 * operand 1: the field loop then ends with kind 3, which starts game mode
 * operand 1 & 0x7f (bit 7 first runs 8001bb50; 8007954c). Advances 3 either
 * way. */
void func_80092FB4(void) {
    if (D_800ADBD8 != 0) {
        D_800B2078.encounter_inhibition = -1;
        D_800ADBD8 = 0;
        D_800B0064 = func_800ACDEC(1);
    }
    D_800B0078->pc += 3;
}

extern u8 D_800B02C8;

/* Leave the field once field control allows it (yield otherwise): a last
 * play-record update (800a31e8), encounters inhibited, and the game state's
 * destination set from selected operands 1 (map, +231a), 3 (+231e), 5 (heading
 * +231c, plus 0x800; 0xffff keeps the camera's) and 7 (entry, +2320) with flags
 * byte 9; then clear 800adbe4 (the field loop exits with code 1), stop the play
 * record (800b02c8 = 1), yield and continue at +10. */
void func_80093014(void) {
    s32 heading;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
        return;
    }
    func_800A31E8();
    D_800B2078.encounter_inhibition = -1;
    D_800ADBE4 = 0;
    D_8005A39C->map = func_8009CF78(1, EVENT_OPERAND_BYTE(9));
    D_8005A39C->entry[1] = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    heading = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    if (((heading & 0xFFFF) == 0xFFFF) | (heading == -1)) {
        D_8005A39C->entry[0] = (D_800AF880.heading_angles.vy + 0x800) & 0xFFF;
    } else {
        D_8005A39C->entry[0] = (heading + 0x800) & 0xFFF;
    }
    D_8005A39C->entry[2] = func_8009D044(7, EVENT_OPERAND_BYTE(9));
    func_800931F8();
    D_800B02C8 = 1;
    D_800B00C0 = 1;
    D_800B0078->pc += 10;
}

void func_800931F8(void) {
}

extern s32 D_800B0048;
extern s32 D_800AFD14;
void func_800932D0(void);

/* Once field control allows it (yield otherwise), request a map change as 98
 * (800932d0: map operand 1, entry operand 3, advancing 5), then set the
 * transition kind 800b0048 from operand 5 and its frames 800afd14 from operand
 * 7 and continue at +9. With a movie requested (800adb70) 800932d0 neither
 * requests nor advances, so these reads come from +0/+2 and the PC advances by
 * 4 only. */
void func_80093200(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800932D0();
        func_800931F8();
        D_800B0048 = func_800ACDEC(0);
        D_800AFD14 = func_800ACDEC(2);
        D_800B0078->pc += 4;
    }
}

extern s32 D_800ADB70;
extern s32 D_800ADBEC;
void func_80092F44(void);

/* Request a change to field operand 1 at entry operand 3: once field
 * control allows it and no movie is requested, inhibit encounters and,
 * when 800adbec is set, record the departure (variables 4, 6, 8: field id,
 * the controlled actor's and the camera's octants; variable 0x12 counted
 * up), store the entry in variable 2 and the field in 8004f34c; then yield
 * and advance. Yields without advancing until allowed. */
void func_800932D0(void) {
    s32 entry;
    s32 field;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0 ||
        D_800ADB70 != 0) {
        D_800B00C0 = 1;
    } else {
        D_800B2078.encounter_inhibition = -1;
        if (D_800ADBEC != 0) {
            entry = func_800ACDEC(3);
            field = func_800ACDEC(1);
            func_80092F44();
            D_800ADBEC = 0;
            func_800A3074(2, entry);
            D_8004F34C = field;
            func_800931F8();
        }
        D_800B00C0 = 1;
        D_800B0078->pc += 5;
    }
}

extern s32 D_800ADBE0;
extern s32 D_800ADB88;
extern s32 D_800ADB18;

/* Event: once the field allows it (800adbdc, 800adbe4 and 800adbec set,
 * 800adb2c and 800adb90 clear, music result 8004f308 not -1; else pc-- back to
 * fe and yield), request a battle as opcode 71 does: 8005954c = 800b2356,
 * encounter kind 80059508 = operand 1, 800594f8, 800adbdc and 800adbe0 cleared,
 * 800adb88 set. Unless operand 5 is 0x7fff, also set the field to enter:
 * publish the current one (80092f44), variable 2 = entry operand 7, field id
 * 8004f34c = operand 5, 800adb18 = 1. Yields; bytes 3-4 are not read. */
void func_800933F8(void) {
    s32 field;
    s32 entry;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0 || D_800ADB2C != 0 || D_8004F308 == -1 ||
        D_800ADB90 != 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    D_8005954C = D_800B2078.unk2356;
    D_80059508 = func_800ACDEC(1);
    D_800594F8 = 0;
    D_800ADBDC = 0;
    D_800ADBE0 = 0;
    D_800ADB88 = 1;
    field = func_800ACDEC(5);
    if (field != 0x7FFF) {
        entry = func_800ACDEC(7);
        func_80092F44();
        func_800A3074(2, entry);
        D_8004F34C = field;
        D_800ADB18 = 1;
    }
    D_800B00C0 = 1;
    D_800B0078->pc += 9;
}

/* Request a battle once field control allows it (yield otherwise): battle
 * selector 80059508 = operand 1, its sound programs 8005954c (8001bbac) =
 * 800b2356, 800594f8 = 0; clear 800adbdc/800adbe0 and set 800adb88 (which ext
 * 7f waits on), then yield and continue at +3. */
void func_80093568(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0 || D_800ADB2C != 0 || D_8004F308 == -1 ||
        D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        D_8005954C = D_800B2078.unk2356;
        D_80059508 = func_800ACDEC(1);
        D_800594F8 = 0;
        D_800ADBDC = 0;
        D_800ADBE0 = 0;
        D_800ADB88 = 1;
        D_800B00C0 = 1;
        D_800B0078->pc += 3;
    }
}

/* Store byte (byte 2) of collision attribute record (byte 1) in variable
 * operand 3. */
void func_80093664(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, value);
    D_800B0078->pc += 5;
}


/* Event: wait (pc-- back to fe), yielding, until 8004f350 is zero: the menus
 * requested by ext 55-5a, cf and da (800adb64) have been run by 800799d4, which
 * clears it. */
void func_800936E4(void) {
    if (D_8004F350 == 0) {
        D_800B0078->pc += 1;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

extern s32 D_800ADB64;

/* Event fe 55: request menu kind 0 (800adb64, run by 800799d4) with the menu
 * parameter 800b236c (ext 99) in 80059171; count 8004f350 up and yield. */
void func_80093740(void) {
    D_800B00C0 = 1;
    D_800ADB64 = 0;
    D_80059171 = D_800B236C;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Request menu kind 6 (800adb64, run by 800799d4) with parameter 1 (80059171),
 * count 8004f350 up and yield. */
void func_80093790(void) {
    D_80059171 = 1;
    D_800ADB64 = 6;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Event fe 57: request menu kind 2 (800adb64); count 8004f350 up and yield. */
void func_800937E0(void) {
    D_800ADB64 = 2;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Event fe 58: request menu kind 3 (800adb64) with parameter operand 1
 * (80059171); count 8004f350 up and yield. */
void func_80093824(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 3;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Request a field change through menu kind 1 (800adb64 = 1, run by 800799d4):
 * inhibit encounters, publish the current field (80092f44), variable 2 = entry
 * operand 3, field id 8004f34c = operand 1; counts 8004f350 up and yields. */
void func_80093888(void) {
    s32 entry;
    s32 field;

    D_800B2078.encounter_inhibition = -1;
    entry = func_800ACDEC(3);
    field = func_800ACDEC(1);
    func_80092F44();
    func_800A3074(2, entry);
    D_8004F34C = field;
    func_800931F8();
    D_800ADB64 = 1;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 5;
}

/* Event fe 56: request menu kind 1 (800adb64) with entry operand 1, also
 * stored in the game's +2320 and vars[1] and in variable 2; count 8004f350
 * up and yield. */
void func_80093930(void) {
    s16 entry;

    entry = func_800ACDEC(1);
    D_800ADB64 = 1;
    D_800B00C0 = 1;
    D_8005A39C->vars[1] = entry;
    D_8005A39C->entry[2] = entry;
    D_800C3A68[1] = entry;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Event fe 59: request menu kind 4 (800adb64) with parameter operand 1
 * (80059171); count 8004f350 up and yield. */
void func_800939A0(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 4;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Event fe 5a: request menu kind 5 (800adb64) with parameter operand 1
 * (80059171); count 8004f350 up and yield. */
void func_80093A04(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 5;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Clear the camera hold flag (0x8000). */
void func_80093A68(void) {
    D_800AF880.flags &= 0x7FFF;
    D_800B0078->pc += 1;
}

/* Set the camera hold flag (0x8000). */
void func_80093A98(void) {
    D_800AF880.flags |= 0x8000;
    D_800B0078->pc += 1;
}


/* Release script control: clear the encounter inhibition, both control
 * bytes and the camera hold flags. */
void func_80093AC8(void) {
    D_800B2078.encounter_inhibition = 0;
    D_800B2078.script_control[0] = 0;
    D_800B2078.script_control[1] = 0;
    D_800AF880.flags &= 0x3FFF;
    D_800B0078->pc += 1;
}

/* Take script control (both control bytes, camera hold flags); re-runs
 * while the field is not ready. */
void func_80093B10(void) {
    D_800B2078.encounter_inhibition = -1;
    D_800B2078.script_control[0] = 1;
    D_800B2078.script_control[1] = 1;
    D_800AF880.flags |= 0xC000;
    if (D_800ADBDC == 0 || D_800ADBE4 == 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    D_800B0078->pc += 1;
}

/* Clear script control byte 0. */
void func_80093BB0(void) {
    D_800B2078.script_control[0] = 0;
    D_800B0078->pc += 1;
}

/* Set script control byte 0. */
void func_80093BD4(void) {
    D_800B2078.script_control[0] = 1;
    D_800B0078->pc += 1;
}

/* Clear script control byte 1. */
void func_80093BFC(void) {
    D_800B2078.script_control[1] = 0;
    D_800B0078->pc += 1;
}

/* Set script control byte 1. */
void func_80093C20(void) {
    D_800B2078.script_control[1] = 1;
    D_800B0078->pc += 1;
}

/* Clear the encounter inhibition. */
void func_80093C48(void) {
    D_800B2078.encounter_inhibition = 0;
    D_800B0078->pc += 1;
}

/* Inhibit encounters once the field is ready; yield until then. */
void func_80093C6C(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0) {
        D_800B00C0 = 1;
    } else {
        D_800B2078.encounter_inhibition = -1;
        D_800B0078->pc += 1;
    }
}

/* Set variable operand 3 to the bytecode byte at raw offset operand 1 plus
 * index operand 5 (a data table inside the script). */
void func_80093CD0(void) {
    u16 offset;

    offset = func_800ACDB8(1);
    offset += func_800ACDEC(5);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_800ADC00[offset]);
    D_800B0078->pc += 7;
}

/* Set variable operand 3 to the bytecode halfword at raw offset operand 1 plus
 * index operand 5, read unsigned when byte 7 is zero, else signed. */
void func_80093D48(void) {
    u16 offset;

    offset = func_800ACDB8(1);
    offset += func_800ACDEC(5);
    if (EVENT_OPERAND_BYTE(7) == 0) {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, D_800ADC00[offset] | (D_800ADC00[offset + 1] << 8));
    } else {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, (s16)(D_800ADC00[offset] + (D_800ADC00[offset + 1] << 8)));
    }
    D_800B0078->pc += 8;
}

/* Door-style swing while flag 0x100000 is clear: the first run plays sound
 * effect 8 on channel 3; each later run turns the current descriptor's y
 * rotation by +0x20 (byte 1 zero) or -0x20, and after 30 turns it sets the
 * flag and advances. It does not yield, so the interpreter repeats it
 * within a pass. With the flag set it just advances. */
void func_80093E30(void) {
    FieldActor *actor;

    if (!(D_800B0078->flags & 0x100000)) {
        if (!(D_800B0078->state.word & 0x20)) {
            D_800B0078->state.word |= 0x20;
            D_800B0078->unkE2 = 0;
            func_80085634(8, 3);
        } else {
            D_800B0078->unkE2++;
            actor = D_800B0078;
            if (actor->unkE2 < 31) {
                if (D_800ADC00[actor->pc + 1] == 0) {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy += 0x20;
                } else {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy -= 0x20;
                }
            } else {
                actor->unkE2 = 0;
                actor->flags |= 0x100000;
                actor->state.word &= ~0x20;
                D_800B0078->pc += 2;
            }
        }
    } else {
        D_800B0078->pc += 2;
    }
    func_80072254(D_800AFD1C);
}

/* The reverse swing while flag 0x100000 is set: the first run plays sound
 * effect 8 on channel 3; each later run turns the current descriptor's y
 * rotation by -0x20 (byte 1 zero) or +0x20, and after 30 turns it clears
 * the flag and advances. It does not yield. With the flag clear it just
 * advances. */
void func_80093FC0(void) {
    FieldActor *actor;

    if (D_800B0078->flags & 0x100000) {
        if (!(D_800B0078->state.word & 0x20)) {
            D_800B0078->state.word |= 0x20;
            D_800B0078->unkE2 = 0;
            func_80085634(8, 3);
        } else {
            D_800B0078->unkE2++;
            actor = D_800B0078;
            if (actor->unkE2 < 31) {
                if (D_800ADC00[actor->pc + 1] == 0) {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy -= 0x20;
                } else {
                    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy += 0x20;
                }
            } else {
                actor->unkE2 = 0;
                actor->flags &= ~0x100000;
                actor->state.word &= ~0x20;
                D_800B0078->pc += 2;
            }
        }
    } else {
        D_800B0078->pc += 2;
    }
    func_80072254(D_800AFD1C);
}

/* Event 0xe8 while flag 0x100000 is clear: the first run plays sound effect
 * 8 on channel 3 and copies the position to the target; each later run
 * moves the target (op5 0x1000/0x1001: target y minus/plus op1 * 16, the
 * original copying target z into the matrix y; otherwise op1 along the
 * descriptor's y rotation + op5 - 0x400) until op3 runs have passed, then
 * sets the flag and advances. It does not yield. With the flag set it just
 * advances. */
void func_80094158(void) {
    s32 angle;
    s32 sine;
    s32 cosine;
    FieldActor *actor;

    if (!(D_800B0078->flags & 0x100000)) {
        if (!(D_800B0078->state.word & 0x20)) {
            D_800B0078->state.word |= 0x20;
            D_800B0078->unkE2 = 0;
            func_80085634(8, 3);
            D_800B0078->target[0] = D_800B0078->position[0];
            D_800B0078->target[1] = D_800B0078->position[1];
            D_800B0078->target[2] = D_800B0078->position[2];
        } else {
            D_800B0078->unkE2++;
            if (D_800B0078->unkE2 < func_800ACDEC(3)) {
                switch (func_800ACDEC(5)) {
                case 0x1000:
                    D_800B0078->target[1] -= func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                case 0x1001:
                    D_800B0078->target[1] += func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                default:
                    angle = D_800B06B8->rotation.vy + func_800ACDEC(5) - 0x400;
                    sine = func_8003F8CC(angle);
                    D_800B0078->target[0] += sine * func_800ACDEC(1);
                    cosine = func_8003F8B0(angle);
                    D_800B0078->target[2] -= cosine * func_800ACDEC(1);
                    D_800B06B8->matrix.t[0] = WHOLE(D_800B0078->target[0]);
                    D_800B06B8->matrix.t[2] = WHOLE(D_800B0078->target[2]);
                    break;
                }
            } else {
                actor = D_800B0078;
                actor->unkE2 = 0;
                actor->flags |= 0x100000;
                actor->state.word &= ~0x20;
                D_800B0078->pc += 7;
            }
        }
    } else {
        D_800B0078->pc += 7;
    }
    func_80072254(D_800AFD1C);
}

/* Event 0xe9, the reverse of 0xe8 while flag 0x100000 is set: the first run
 * plays sound effect 8 on channel 3 (the target is not reset); each later
 * run moves the target as 0xe8 does with the direction offsets negated (the
 * 0x1000/0x1001 y steps are the same) until op3 runs have passed, then
 * clears the flag and advances. It does not yield. With the flag clear it
 * just advances. */
void func_800943AC(void) {
    FieldActor *actor;
    FieldActor *done;
    s32 angle;
    s32 sine;
    s32 cosine;

    actor = D_800B0078;
    if (actor->flags & 0x100000) {
        if (!(actor->state.word & 0x20)) {
            actor->state.word |= 0x20;
            actor->unkE2 = 0;
            func_80085634(8, 3);
        } else {
            actor->unkE2++;
            if (D_800B0078->unkE2 < func_800ACDEC(3)) {
                switch (func_800ACDEC(5)) {
                case 0x1000:
                    D_800B0078->target[1] -= func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                case 0x1001:
                    D_800B0078->target[1] += func_800ACDEC(1) * 16;
                    D_800B06B8->matrix.t[1] = WHOLE(D_800B0078->target[2]);
                    break;
                default:
                    angle = D_800B06B8->rotation.vy + func_800ACDEC(5) - 0x400;
                    sine = func_8003F8CC(angle);
                    D_800B0078->target[0] -= sine * func_800ACDEC(1);
                    cosine = func_8003F8B0(angle);
                    D_800B0078->target[2] += cosine * func_800ACDEC(1);
                    D_800B06B8->matrix.t[0] = WHOLE(D_800B0078->target[0]);
                    D_800B06B8->matrix.t[2] = WHOLE(D_800B0078->target[2]);
                    break;
                }
            } else {
                done = D_800B0078;
                done->unkE2 = 0;
                done->flags &= ~0x100000;
                done->state.word &= ~0x20;
                D_800B0078->pc += 7;
            }
        }
    } else {
        actor->pc += 7;
    }
    func_80072254(D_800AFD1C);
}


/* Set the play clock in variable 0x0a to operand 1 minutes : operand 3
 * seconds (low bytes), restart its frame count (8004f318) and stop it
 * (8004f328 = 0xff; 800a31e8 counts down with bit 2 and stops on bit 7). */
void func_800945D4(void) {
    s32 high;

    D_8004F318 = 0;
    D_8004F328 = 0xFF;
    high = func_800ACDEC(1);
    func_800A3074(10, ((high << 8) & 0xFF00) | (func_800ACDEC(3) & 0xFF));
    D_800B0078->pc += 5;
}

/* Set the play clock mode 8004f328 to byte 1 (800a31e8: bit 2 counts the
 * clock in variable 0x0a down, bit 7 stops it). */
void func_80094650(void) {
    D_8004F328 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Restart the play clock's frame count (8004f318) and stop the clock
 * (8004f328 = 0xff). */
void func_8009468C(void) {
    D_8004F318 = 0;
    D_8004F328 = 0xFF;
    D_800B0078->pc += 1;
}

/* Turn the current actor's model by angle operand 1 about its x axis (state
 * mode 1, +70), applied on top of its matrix when drawn and in collision
 * tests. */
void func_800946BC(void) {
    D_800B0078->state.word = (D_800B0078->state.word & ~3) | 1;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Turn the current actor's model by angle operand 1 about its y axis (state
 * mode 2, +70), applied on top of its matrix when drawn and in collision
 * tests. */
void func_80094710(void) {
    D_800B0078->state.word = (D_800B0078->state.word & ~3) | 2;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Turn the current actor's model by angle operand 1 about its z axis (state
 * mode 3, +70), applied on top of its matrix when drawn and in collision
 * tests. */
void func_80094764(void) {
    D_800B0078->state.word |= 3;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Turn the descriptor of the actor selected by byte 2 by operand 3 about the
 * axis byte 1 picks (0/1: x+/-, 2/3: y+/-, 4/5: z+/-) and rebuild its
 * matrix. */
void func_800947B0(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(2) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(2)];
        switch (EVENT_OPERAND_BYTE(1)) {
        case 0:
            descriptor->rotation.vx += func_800ACDEC(3);
            break;
        case 1:
            descriptor->rotation.vx -= func_800ACDEC(3);
            break;
        case 2:
            descriptor->rotation.vy += func_800ACDEC(3);
            break;
        case 3:
            descriptor->rotation.vy -= func_800ACDEC(3);
            break;
        case 4:
            descriptor->rotation.vz += func_800ACDEC(3);
            break;
        case 5:
            descriptor->rotation.vz -= func_800ACDEC(3);
            break;
        }
        func_80072254(func_8009CDB4(2));
    }
    D_800B0078->pc += 5;
}

/* Set rotation axis byte 3 (0 x, 1 y, 2 z) of the current descriptor to operand
 * 1 and rebuild its matrix (80072254). */
void func_80094918(void) {
    switch (EVENT_OPERAND_BYTE(3)) {
    case 0:
        D_800AF880.components.descriptors[D_800AFD1C].rotation.vx = func_800ACDEC(1);
        break;
    case 1:
        D_800AF880.components.descriptors[D_800AFD1C].rotation.vy = func_800ACDEC(1);
        break;
    case 2:
        D_800AF880.components.descriptors[D_800AFD1C].rotation.vz = func_800ACDEC(1);
        break;
    }
    D_800B0078->pc += 4;
    func_80072254(D_800AFD1C);
}

/* Add operand 1 to the current descriptor's x rotation and rebuild its
 * matrix (80072254). */
void func_80094A5C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vx += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Subtract operand 1 from the current descriptor's x rotation and rebuild
 * its matrix (80072254). */
void func_80094ACC(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vx -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Add operand 1 to the current descriptor's y rotation and rebuild its
 * matrix (80072254). */
void func_80094B3C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Subtract operand 1 from the current descriptor's y rotation and rebuild
 * its matrix (80072254). */
void func_80094BAC(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Add operand 1 to the current descriptor's z rotation and rebuild its
 * matrix (80072254). */
void func_80094C1C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vz += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Subtract operand 1 from the current descriptor's z rotation and rebuild
 * its matrix (80072254). */
void func_80094C8C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vz -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* First free slot of inventory list 0, or -1. */
s32 func_80094CFC(void) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->itemCounts[i] == 0 || D_8005A39C->itemIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 1, or -1. */
s32 func_80094D4C(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->weaponCounts[i] == 0 || D_8005A39C->weaponIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 2, or -1. */
s32 func_80094D9C(void) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (D_8005A39C->accessoryCounts[i] == 0 || D_8005A39C->accessoryIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 3, or -1. */
s32 func_80094DEC(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->gearPartCounts[i] == 0 || D_8005A39C->gearPartIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 4, or -1. */
s32 func_80094E3C(void) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->gearAccessoryCounts[i] == 0 || D_8005A39C->gearAccessoryIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 0, or -1. */
s32 func_80094E8C(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->itemIds[i] == id && D_8005A39C->itemCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 2, or -1. */
s32 func_80094EDC(s32 id) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (D_8005A39C->accessoryIds[i] == id && D_8005A39C->accessoryCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 1, or -1. */
s32 func_80094F2C(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->weaponIds[i] == id && D_8005A39C->weaponCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 3, or -1. */
s32 func_80094F7C(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->gearPartIds[i] == id && D_8005A39C->gearPartCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 4, or -1. */
s32 func_80094FCC(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->gearAccessoryIds[i] == id && D_8005A39C->gearAccessoryCounts[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Id array of the inventory list selected by item >> 8. */
u8 *func_8009501C(s32 item) {
    switch (item >> 8) {
    case 0:
        return D_8005A39C->itemIds;
    case 1:
        return D_8005A39C->weaponIds;
    case 2:
        return D_8005A39C->accessoryIds;
    case 3:
        return D_8005A39C->gearPartIds;
    case 4:
        return D_8005A39C->gearAccessoryIds;
    }
    return NULL;
}

/* Count array of the inventory list selected by item >> 8. */
u8 *func_800950A0(s32 item) {
    switch (item >> 8) {
    case 0:
        return D_8005A39C->itemCounts;
    case 1:
        return D_8005A39C->weaponCounts;
    case 2:
        return D_8005A39C->accessoryCounts;
    case 3:
        return D_8005A39C->gearPartCounts;
    case 4:
        return D_8005A39C->gearAccessoryCounts;
    }
    return NULL;
}

/* Slot holding `item` (list in the high byte), or -1; 0 for no list. */
s32 func_80095124(s32 item) {
    switch (item >> 8) {
    case 0:
        return func_80094E8C(item);
    case 1:
        return func_80094F2C(item - 0x100);
    case 2:
        return func_80094EDC(item - 0x200);
    case 3:
        return func_80094F7C(item - 0x300);
    case 4:
        return func_80094FCC(item - 0x400);
    }
    return 0;
}

/* First free slot of the list selected by item >> 8, or -1; 0 for no list. */
s32 func_800951B8(s32 item) {
    switch (item >> 8) {
    case 0:
        return func_80094CFC();
    case 1:
        return func_80094D4C();
    case 2:
        return func_80094D9C();
    case 3:
        return func_80094DEC();
    case 4:
        return func_80094E3C();
    }
    return 0;
}

void func_80095284(void);

/* Stop the current actor (func_80095284) and advance. */
void func_8009524C(void) {
    func_80095284();
    D_800B0078->pc += 1;
}

/* Stop the current actor: clear its motion (+30, +40 and its model's velocity
 * and speed), mark its heading turned (0x8000) and yield without advancing, so
 * the instruction holds the actor every frame. */
void func_80095284(void) {
    Sprite *model;
    u16 state;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800B00C0 = 1;
    D_800B0078->unk030[0] = 0;
    D_800B0078->unk030[1] = 0;
    D_800B0078->unk030[2] = 0;
    D_800B0078->unk40[0] = 0;
    D_800B0078->unk40[1] = 0;
    D_800B0078->unk40[2] = 0;
    state = D_800B0078->heading | 0x8000;
    D_800B0078->heading_goal = state;
    D_800B0078->heading = state;
    model->speed_x = 0;
    model->speed_z = 0;
    model->speed = 0;
}

/* Set the terrain angle from operand 1. */
void func_80095300(void) {
    D_800B2078.terrain_angle = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Trigger zone operand 1 and one of its corners packed as (z << 16) + x,
 * the point format of func_8004A70C.  The zone is addressed inside each
 * access (not through a Zone pointer): GCC then forms the address as
 * (scaled index + table), which ties the zone byte's register differently
 * from `zone = &D_800ADBF4[i]' (base + scaled index). */
#define EVENT_ZONE (D_800ADBF4[EVENT_OPERAND_BYTE(1)])
#define EVENT_ZONE_CORNER(k) ((EVENT_ZONE.corner[k].z << 16) + EVENT_ZONE.corner[k].x)
/* The actor the player controls. */
#define CONTROLLED_ACTOR (D_800AF880.components.descriptors[D_800B2078.controlled].actor)
/* The trigger zone named by the operand byte code[1]. */
#define CODE_ZONE(code) (D_800ADBF4[(code)[1]])
#define CODE_ZONE_CORNER(code, k) ((CODE_ZONE(code).corner[k].z << 16) + CODE_ZONE(code).corner[k].x)

/* Call (operand 2) when the controlled actor stands inside trigger zone
 * operand 1 and the call stack has room; otherwise skip. */
void func_8009533C(void) {
    FieldActor *player;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    a = EVENT_ZONE_CORNER(0);
    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    b = EVENT_ZONE_CORNER(1);
    c = EVENT_ZONE_CORNER(2);
    d = EVENT_ZONE_CORNER(3);
    if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
        func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0 &&
        (D_800B0078->state.word & 0x1C0) != 0x100) {
        D_800B0078->call_stack[(D_800B0078->state.word >> 6) & 7] = D_800B0078->pc + 4;
        D_800B0078->pc = func_800ACDB8(2);
        D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | (((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6);
        return;
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Call the script at operand 2 (pushing pc + 4) when the controlled actor
 * stands inside trigger zone byte 1, the zone's corner-0 height lies within
 * its body (y - height .. y) and the call stack has room; otherwise continue
 * (pc + 4).
 * The operand address is formed from pc and then rebased on the bytecode
 * (set twice, so sched keeps it where the original has it). */
void func_80095520(void) {
    u8 *code;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    code = (u8 *)D_800B0078->pc;
    code += (s32)D_800ADC00;
    if (WHOLE(CONTROLLED_ACTOR->position[1]) > CODE_ZONE(code).corner[0].y &&
        WHOLE(CONTROLLED_ACTOR->position[1]) - (u16)CONTROLLED_ACTOR->height < CODE_ZONE(code).corner[0].y) {
        a = CODE_ZONE_CORNER(code, 0);
        b = CODE_ZONE_CORNER(code, 1);
        point = (WHOLE(CONTROLLED_ACTOR->position[2]) << 16) + WHOLE(CONTROLLED_ACTOR->position[0]);
        c = CODE_ZONE_CORNER(code, 2);
        d = CODE_ZONE_CORNER(code, 3);
        if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
            func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0 &&
            (D_800B0078->state.word & 0x1C0) != 0x100) {
            D_800B0078->call_stack[(D_800B0078->state.word >> 6) & 7] = D_800B0078->pc + 4;
            D_800B0078->pc = func_800ACDB8(2);
            D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | (((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6);
            return;
        }
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Continue when the controlled actor is inside trigger zone operand 1,
 * else jump to operand 2. */
void func_80095734(void) {
    FieldActor *player;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    a = EVENT_ZONE_CORNER(0);
    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    b = EVENT_ZONE_CORNER(1);
    c = EVENT_ZONE_CORNER(2);
    d = EVENT_ZONE_CORNER(3);
    if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
        func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0) {
        D_800B0078->pc += 4;
        return;
    }
    D_800B0078->pc = func_800ACDB8(2);
    D_800AFC7C += 1;
}

/* Continue when the controlled actor is inside trigger zone operand 1 and
 * the zone's height lies within the actor's body, else jump to operand 2.
 * The operand address is formed from pc and then rebased on the bytecode
 * (set twice, so sched keeps it where the original has it). */
void func_800958C0(void) {
    u8 *code;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    code = (u8 *)D_800B0078->pc;
    code += (s32)D_800ADC00;
    if (WHOLE(CONTROLLED_ACTOR->position[1]) > CODE_ZONE(code).corner[0].y &&
        WHOLE(CONTROLLED_ACTOR->position[1]) - (u16)CONTROLLED_ACTOR->height < CODE_ZONE(code).corner[0].y) {
        a = CODE_ZONE_CORNER(code, 0);
        b = CODE_ZONE_CORNER(code, 1);
        point = (WHOLE(CONTROLLED_ACTOR->position[2]) << 16) + WHOLE(CONTROLLED_ACTOR->position[0]);
        c = CODE_ZONE_CORNER(code, 2);
        d = CODE_ZONE_CORNER(code, 3);
        if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
            func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0) {
            D_800B0078->pc += 4;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(2);
    D_800AFC7C += 1;
}

/* Project a selected actor's origin to the screen. */
void func_80095A7C(s32 *x, s32 *y) {
    SVECTOR origin;
    MATRIX m;
    union {
        s32 word;
        DVECTOR xy;
    } screen;
    s32 depth;
    s32 flag;

    CompMatrix(&D_800AF880.scaled_world, &D_800AF880.components.descriptors[func_8009CD7C(1)].transform, &m);
    origin.vx = 0;
    origin.vy = 0;
    origin.vz = 0;
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    RotTransPers(&origin, &screen.word, &depth, &flag);
    *y = screen.xy.vy;
    *x = screen.xy.vx;
}

extern s32 D_800ADC18;

/* Event fe 02: with 800adc18 set, advance at once. Otherwise yield and continue
 * when the origin of the actor selected by byte 1 (8009cd7c: the party leader
 * when none) projects inside the screen with a 32-pixel margin (x 33..287, y
 * 33..191), else jump to operand 2. */
void func_80095B3C(void) {
    s32 x;
    s32 y;

    func_80095A7C(&x, &y);
    if (D_800ADC18 != 0) {
        D_800B0078->pc += 4;
        return;
    }
    if (y > 32 && y < 192 && x > 32 && x < 288) {
        D_800B0078->pc += 4;
    } else {
        D_800B0078->pc = func_800ACDB8(2);
    }
    D_800B00C0 = 1;
}

/* Continue (pc + 4) when the origin of the actor selected by byte 1 (the
 * party leader by default) projects inside the 320x224 screen, otherwise
 * jump to operand 2; yields. While 800adc18 (the field start countdown)
 * runs it continues at once without yielding. */
void func_80095C00(void) {
    s32 x;
    s32 y;

    func_80095A7C(&x, &y);
    if (D_800ADC18 != 0) {
        D_800B0078->pc += 4;
        return;
    }
    if (y > 0 && y < 224 && x > 0 && x < 320) {
        D_800B0078->pc += 4;
    } else {
        D_800B0078->pc = func_800ACDB8(2);
    }
    D_800B00C0 = 1;
}

/* Event fe 05: continue (advance 6) when the actor selected by byte 1 is on
 * collision layer operand 2, else (also with no actor) jump to operand 4. */
void func_80095CC4(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        if (func_800ACDEC(2) == actor->layer) {
            D_800B0078->pc += 6;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(4);
}

/* Event fe 06: continue (advance 6) when the collision triangle under the actor
 * selected by byte 1 has attribute operand 2, else (also with no actor) jump to
 * operand 4. */
void func_80095D6C(void) {
    FieldActor *actor;
    u8 attribute;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        attribute = D_800AF880.components.collision_triangles[actor->layer][actor->triangle[actor->layer]].attribute;
        if (func_800ACDEC(2) == attribute) {
            D_800B0078->pc += 6;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(4);
}

/* Continue (pc + 6) when the actor selected by byte 1 is nearer than
 * operand 2 (3D distance) to the running actor; otherwise, or with no such
 * actor, jump to operand 4. */
void func_80095E48(void) {
    FieldActor *other;
    FieldDescriptor *descriptor;
    s32 distance;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        other = descriptor->actor;
        distance = func_80099A04(WHOLE(D_800B06B8->actor->position[0]) - WHOLE(other->position[0]),
                                 WHOLE(D_800B06B8->actor->position[1]) - WHOLE(other->position[1]),
                                 WHOLE(D_800B06B8->actor->position[2]) - WHOLE(other->position[2]));
        if (distance < func_800ACDEC(2)) {
            D_800B0078->pc += 6;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(4);
}

/* Continue when the party's gold is at least the 32-bit operand 1,
 * otherwise jump to operand 5. */
void func_80095F24(void) {
    u8 *operand;

    operand = &D_800ADC00[D_800B0078->pc];
    if (D_8005A39C->gold >=
        operand[1] + (operand[2] << 8) + (operand[3] << 16) + (operand[4] << 24)) {
        D_800B0078->pc += 7;
        return;
    }
    D_800B0078->pc = func_800ACDB8(5);
}

/* Add operand 1 to the party's gold, capped at 9999999. */
void func_80095FB8(void) {
    s32 gold;

    gold = D_8005A39C->gold + func_800ACDEC(1);
    if (gold > 9999999) {
        gold = 9999999;
    }
    D_8005A39C->gold = gold;
    D_800B0078->pc += 3;
}

/* Take operand 1 from the party's gold, not below zero. */
void func_8009601C(void) {
    s32 amount;
    s32 gold;

    amount = func_800ACDEC(1);
    gold = D_8005A39C->gold;
    gold -= amount;
    if (gold < 0) {
        gold = 0;
    }
    D_8005A39C->gold = gold;
    D_800B0078->pc += 3;
}

/* Continue when raw operand 1 shares a bit with `bits`, otherwise jump to
 * operand 3. */
void func_80096078(s32 bits) {
    if (func_800ACDB8(1) & bits & 0xFFFF) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue when raw operand 1 equals `value`, otherwise jump to operand 3. */
void func_800960E4(s32 value) {
    if ((func_800ACDB8(1) & 0xFFFF) == (value & 0xFFFF)) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

extern u16 D_800AFE9C;
extern u16 D_800AFC6C;
void func_80096078(s32 bits);
void func_800960E4(s32 value);

/* Continue (pc + 5) when the held buttons (800afe9c) equal raw operand 1,
 * otherwise jump to operand 3. */
void func_80096150(void) {
    func_800960E4(D_800AFE9C);
}

/* Continue (pc + 5) when the buttons held since the last record (800afc6c,
 * gathered by 800a31e8, cleared by 33) equal raw operand 1, otherwise jump
 * to operand 3. */
void func_80096178(void) {
    func_800960E4(D_800AFC6C);
}

/* Continue at +5 when the held buttons (800afe9c) share a bit with raw operand
 * 1, otherwise jump to operand 3 (80096078). */
void func_800961A0(void) {
    func_80096078(D_800AFE9C);
}

/* Continue at +5 when the buttons seen held since 33 last cleared them
 * (800afc6c, gathered each frame by 800a31e8) share a bit with raw operand 1,
 * otherwise jump to operand 3 (80096078). */
void func_800961C8(void) {
    func_80096078(D_800AFC6C);
}

/* Forget the buttons seen held (800afc6c = 0), which 32 and e3 test. */
void func_800961F0(void) {
    D_800AFC6C = 0;
    D_800B0078->pc += 1;
}

s32 func_80095124(s32 item);
u8 *func_800950A0(s32 item);
u8 *func_8009501C(s32 item);

/* Store the carried count of item operand 1 (its list in the high byte) in
 * variable operand 3 (0 when not carried). */
void func_80096214(void) {
    s32 item;
    s32 slot;
    u8 *counts;

    item = func_800ACDEC(1);
    slot = func_80095124(item);
    counts = func_800950A0(item);
    func_8009501C(item);
    if (slot != -1) {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, counts[slot]);
    } else {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, 0);
    }
    D_800B0078->pc += 5;
}

/* Continue when item operand 1 is carried, otherwise jump to operand 3. */
void func_800962C0(void) {
    if (func_80095124(func_800ACDEC(1)) != -1) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

s32 func_8009635C(s32 item);

/* Give one of item operand 1. */
void func_8009631C(void) {
    func_8009635C(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

s32 func_800951B8(s32 item);


/* Add one of `item` (list in the high byte) up to 99, or take a free slot.
 * Declared int without a return value, as the original's unfilled branch
 * delay slot shows (v0 stays live to the exit). */
s32 func_8009635C(s32 item) {
    s32 slot;
    u8 *counts;
    u8 *ids;

    slot = func_80095124(item);
    counts = func_800950A0(item);
    ids = func_8009501C(item);
    if (slot != -1) {
        if (counts[slot] < 99) {
            counts[slot]++;
        }
    } else {
        slot = func_800951B8(item);
        if (slot != -1) {
            ids[slot] = item;
            counts[slot] = 1;
        }
    }
}

/* Take one of item operand 1; an emptied slot's id becomes 0xFF. */
void func_8009640C(void) {
    s32 item;
    s32 slot;
    u8 *ids;
    u8 *counts;

    item = func_800ACDEC(1);
    slot = func_80095124(item);
    if (slot != -1) {
        ids = func_8009501C(item);
        counts = func_800950A0(item);
        if (--counts[slot] == 0) {
            ids[slot] = 0xFF;
        }
    }
    D_800B0078->pc += 3;
}

/* Continue when character operand 1 is in the party, otherwise jump to
 * operand 2. */
void func_800964B0(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (EVENT_OPERAND_BYTE(1) == D_80062590[i]) {
            D_800B0078->pc += 4;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(2);
}

/* Continue when game flag bit operand 1 (of unk1D30) is set, otherwise
 * jump to operand 2. */
void func_80096534(void) {
    if ((D_8005A39C->joined >> EVENT_OPERAND_BYTE(1)) & 1) {
        D_800B0078->pc += 4;
        return;
    }
    D_800B0078->pc = func_800ACDB8(2);
}

/* Set game flag bit operand 1 of unk1D30. */
void func_800965A8(void) {
    D_8005A39C->joined |= 1 << EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Clear game flag bit operand 1 of unk1D30. */
void func_800965F4(void) {
    D_8005A39C->joined &= ~(1 << EVENT_OPERAND_BYTE(1));
    D_800B0078->pc += 2;
}

/* Continue (pc + 5) when variable 0 is below operand 1, otherwise jump to
 * operand 3. */
void func_80096644(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (func_800A3018(0) < value) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue (pc + 5) when variable 0 is above operand 1, otherwise jump to
 * operand 3. */
void func_800966B4(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (value < func_800A3018(0)) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue (pc + 5) when variable 0 equals operand 1, otherwise jump to
 * operand 3. */
void func_80096724(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (func_800A3018(0) == value) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Set variable 0 to operand 1, raising the batch limit by 32. */
void func_80096790(void) {
    D_800AFC7C += 32;
    func_800A3074(0, func_800ACDEC(1));
    D_800B0078->pc += 3;
}

/* Copy variable 0 into variable operand 1. */
void func_800967E8(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, func_800A3018(0));
    D_800B0078->pc += 3;
}

/* Restore HP of party slot `slot`, up to its maximum. */
void func_80096844(s32 slot, s32 amount) {
    D_8005A39C->characters[D_80062590[slot]].hp += amount;
    if (D_8005A39C->characters[D_80062590[slot]].maxHp < D_8005A39C->characters[D_80062590[slot]].hp) {
        D_8005A39C->characters[D_80062590[slot]].hp = D_8005A39C->characters[D_80062590[slot]].maxHp;
    }
}

/* Reduce HP of party slot `slot`, leaving at least 1. */
void func_800968CC(s32 slot, s32 amount) {
    s32 hp;

    hp = D_8005A39C->characters[D_80062590[slot]].hp - amount;
    if (hp <= 0) {
        hp = 1;
    }
    D_8005A39C->characters[D_80062590[slot]].hp = hp;
}

/* Restore EP of party slot `slot`, up to its maximum. */
void func_80096920(s32 slot, s32 amount) {
    D_8005A39C->characters[D_80062590[slot]].ep += amount;
    if (D_8005A39C->characters[D_80062590[slot]].maxEp < D_8005A39C->characters[D_80062590[slot]].ep) {
        D_8005A39C->characters[D_80062590[slot]].ep = D_8005A39C->characters[D_80062590[slot]].maxEp;
    }
}

/* Reduce EP of party slot `slot`, leaving at least 1. */
void func_800969A8(s32 slot, s32 amount) {
    s32 ep;

    ep = D_8005A39C->characters[D_80062590[slot]].ep - amount;
    if (ep <= 0) {
        ep = 1;
    }
    D_8005A39C->characters[D_80062590[slot]].ep = ep;
}

extern s16 D_800AEA2C[4];

/* Reduce the HP of the party members masked by entry byte 3 & 3 of 800aea2c (7
 * all, then slot 0, 1, 2) by selected operand 1 (bit 0x80 of byte 3), leaving
 * at least 1 (800968cc). */
void func_800969FC(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_800968CC(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Set player control's jump mode (800b2344), animation mode and jump repeat
 * delay (800b2340) from operands 1, 3 and 5 and clear the repeat countdown
 * (800b2342). */
void func_80096AF4(void) {
    D_800B2078.jump_mode = func_800ACDEC(1);
    D_800B2078.animation_mode = func_800ACDEC(3);
    D_800B2078.repeat_delay = func_800ACDEC(5);
    D_800B2078.repeat_remaining = 0;
    D_800B0078->pc += 7;
}

/* Store the HP of the character in party slot byte 3 in variable operand 1
 * (nothing when the slot is empty). */
void func_80096B58(void) {
    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(3)]].hp);
    }
    D_800B0078->pc += 4;
}

/* Store the EP of the character in party slot byte 3 in variable operand 1
 * (nothing when the slot is empty). */
void func_80096C40(void) {
    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(3)]].ep);
    }
    D_800B0078->pc += 4;
}

/* Set the HP of the character in party slot byte 1 to operand 2, capped at its
 * maximum, when party slot byte 3 is occupied; byte 3 is also the high byte of
 * operand 2. */
void func_80096D28(void) {
    s32 hp;

    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        hp = func_800ACDEC(2);
        if (D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].maxHp < hp) {
            hp = D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].maxHp;
        }
        D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].hp = hp;
    }
    D_800B0078->pc += 4;
}

/* Set the EP of the character in party slot byte 1 to operand 2, capped at its
 * maximum, when party slot byte 3 is occupied; byte 3 is also the high byte of
 * operand 2. */
void func_80096E20(void) {
    s32 ep;

    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        ep = func_800ACDEC(2);
        if (D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].maxEp < ep) {
            ep = D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].maxEp;
        }
        D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].ep = ep;
    }
    D_800B0078->pc += 4;
}

/* Restore the EP of the party members masked by entry byte 3 & 3 of 800aea2c by
 * selected operand 1 (bit 0x80 of byte 3), up to their maximum (80096920); the
 * same as 7e (the HP restore 80096844 has no caller). */
void func_80096F18(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_80096920(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Reduce the EP of the party members masked by entry byte 3 & 3 of 800aea2c by
 * selected operand 1 (bit 0x80 of byte 3), leaving at least 1 (800969a8). */
void func_80097010(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_800969A8(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Restore the EP of the party members masked by entry byte 3 & 3 of 800aea2c by
 * selected operand 1 (bit 0x80 of byte 3), up to their maximum (80096920). */
void func_80097108(void) {
    s32 slot;
    s32 amount;
    s32 mask;

    amount = func_8009CF78(1, EVENT_OPERAND_BYTE(3));
    slot = 0;
    mask = D_800AEA2C[EVENT_OPERAND_BYTE(3) & 3];
    do {
        if (D_80062590[slot] != 0xFF && (mask & 1)) {
            func_80096920(slot, amount);
        }
        mask >>= 1;
        slot++;
    } while (slot < 3);
    D_800B0078->pc += 4;
}

/* Fully restore HP and EP of character operand 1. */
void func_80097200(void) {
    s32 id;

    id = func_800ACDEC(1);
    D_8005A39C->characters[id].hp = D_8005A39C->characters[id].maxHp;
    D_8005A39C->characters[id].ep = D_8005A39C->characters[id].maxEp;
    D_800B0078->pc += 3;
}

/* Fully restore every character's HP. */
void func_80097264(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        D_8005A39C->characters[i].hp = D_8005A39C->characters[i].maxHp;
    }
    D_800B0078->pc += 1;
}

/* Fully restore every character's EP. */
void func_800972AC(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        D_8005A39C->characters[i].ep = D_8005A39C->characters[i].maxEp;
    }
    D_800B0078->pc += 1;
}

/* Yield once. */
void func_800972F4(void) {
    D_800B00C0 = 1;
    D_800B0078->pc += 1;
}

void func_8007D93C(s32 a);
void func_80071E58(s32 a);

/* Reset fade channel 0 (8007d93c) and fade the screen back in from full over
 * operand-1 frames (80071e58: only while 800adc08 marks a fade-out, and
 * only in fade mode 2). */
void func_8009731C(void) {
    func_8007D93C(0);
    func_80071E58(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

void func_80071DCC(s32 a);

/* Fade the screen out on channel 0 over operand-1 frames (80071dcc: levels
 * rise from 0 to full with blend 2, unless 800adc08 already marks a
 * fade-out, and only in fade mode 2). */
void func_80097364(void) {
    func_80071DCC(func_800ACDEC(1));
    D_800B0078->pc += 3;
}


/* Start reading file 0xb8 + operand 2 of the current directory ahead as map
 * data (resident 8001b484, slot byte 1); retried without yielding until that
 * read is under way or done, then continue at +4. */
void func_800973A4(void) {
    if (func_8001B484(func_800ACDB8(2) & 0xFFFF, EVENT_OPERAND_BYTE(1)) == 0) {
        D_800B0078->pc += 4;
    }
}

/* Jump table: continue at three-byte entry operand 1 after this instruction
 * (pc + 3 + 3 * operand 1). */
void func_80097410(void) {
    D_800B0078->pc += func_800ACDEC(1) * 3 + 3;
}

/* Facing octant (0..7) of the controlled actor. */
s32 func_8009744C(void) {
    return (((D_800AF880.components.descriptors[D_800B2078.controlled].actor->heading_goal + 0x100) >> 9) + 2) & 7;
}

s32 func_80097A50(s32 speed);

/* Walk the current actor as 54 toward descriptor byte 1 at height selected
 * operand 2 (flags byte 4) for at most operand-5 steps (latched in the slot);
 * continues at +7 once within reach or out of steps, else yields. */
void func_8009749C(void) {
    FieldActor *other;

    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    other = D_800AF880.components.descriptors[EVENT_OPERAND_BYTE(1)].actor;
    D_800B0078->target[0] = WHOLE(other->position[0]);
    D_800B0078->target[2] = WHOLE(other->position[2]);
    D_800B0078->target[1] = WHOLE(other->position[1]);
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(5);
    }
    if (func_80097A50(func_800ACDEC(5)) == 0) {
        D_800B0078->pc += 7;
    }
}

/* Walk the current actor (80097a50 move mode 2) toward the position of
 * descriptor byte 1 (a raw index here; the reach check reads it as an actor
 * selector and widens the reach by both actors' +1e radii) at height selected
 * operand 2 (flags byte 4, bit 0x80) without a step limit; continues at +5 once
 * within reach or when no actor is selected, else yields. */
void func_800975C0(void) {
    FieldActor *other;

    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    other = D_800AF880.components.descriptors[EVENT_OPERAND_BYTE(1)].actor;
    D_800B0078->target[0] = WHOLE(other->position[0]);
    D_800B0078->target[2] = WHOLE(other->position[2]);
    D_800B0078->target[1] = WHOLE(other->position[1]);
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80097A50(0xFFFF) == 0) {
        D_800B0078->pc += 5;
    }
}

/* Walk the current actor (80097a50 move mode 1) toward its start position
 * offset by selected x/z/y operands 1/3/6 (flags byte 5) for at most operand-8
 * steps (latched in the slot); continues at +10 once within reach or out of
 * steps, else yields. */
void func_800976A8(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(8);
    }
    if (func_80097A50(func_800ACDEC(8)) == 0) {
        D_800B0078->pc += 10;
    }
}

/* Walk the current actor (80097a50 move mode 1) toward its start position
 * offset by selected x/z/y operands 1/3/6 (flags byte 5) without a step limit;
 * continues at +8 once within reach, else yields. */
void func_800977A4(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80097A50(0xFFFF) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Walk the current actor (80097a50 move mode 3; the start position is kept as
 * the target) toward a point along angle operand 1 (x as (start x + (cos << 5))
 * >> 12 and z as start z - (sin << 5) >> 12, as 80097a50 computes them) at
 * height offset selected operand 3 (flags byte 7, bit 0x80), for at most
 * operand-5 steps (latched in the slot); continues at +8 once within reach or
 * out of steps, else yields. */
void func_80097864(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 3;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(5);
    }
    if (func_80097A50(func_800ACDEC(5)) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Walk the current actor as 4c toward selected x/z/y operands 1/3/6 (flags byte
 * 5) for at most operand-8 steps (latched in the slot); continues at +10 once
 * within reach or out of steps, else yields. */
void func_80097954(void) {
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(8);
    }
    if (func_80097A50(func_800ACDEC(8)) == 0) {
        D_800B0078->pc += 10;
    }
}

/* Walk the current actor (80097a50 in the slot's move mode, 0 after a finished
 * move) toward selected x/z/y operands 1/3/6 (flags byte 5, bits
 * 0x80/0x40/0x20) without a step limit; continues at +8 once within reach, else
 * yields. */
void func_800979F0(void) {
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80097A50(0xFFFF) == 0) {
        D_800B0078->pc += 8;
    }
}

#include "field_motion.h"

/* Walk the current actor toward its move target (move modes 0-3: operand
 * position, offset from the target, another actor's reach, or a point at an
 * angle): set the step from its speed, face along it and return 0 once within
 * reach or out of steps, else -1. */
s32 func_80097A50(s32 speed) {
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
    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    x = 0;
    if (D_800AF880.components.descriptors[D_800AFD1C].actor->layer_flags & 0x2000) {
        model->speed = 0x8000000 / (u16)D_800B0078->unk76;
    } else {
        model->speed = 0x4000000 / (u16)D_800B0078->unk76;
    }
    reach = func_80099A8C(model->speed >> 15) + 1;
    extra = 0;
    switch (D_800B0078->slots[D_800B0078->slot].move_mode) {
    case 0:
        x = func_8009CF78(1, EVENT_OPERAND_BYTE(5));
        z = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
        y = func_8009D000(6, EVENT_OPERAND_BYTE(5));
        break;
    case 1:
        x = func_8009CF78(1, EVENT_OPERAND_BYTE(5)) + D_800B0078->target[0];
        z = func_8009CFBC(3, EVENT_OPERAND_BYTE(5)) + D_800B0078->target[2];
        y = D_800B0078->target[1] + func_8009D000(6, EVENT_OPERAND_BYTE(5));
        break;
    case 2:
        if (func_8009CDB4(1) == 0xFF) {
            return 0;
        }
        extra = func_80099A8C((u16)D_800AF880.components.descriptors[func_8009CDB4(1)].actor->gravity.s.whole +
                              (u16)D_800B0078->gravity.s.whole);
        x = D_800B0078->target[0];
        z = D_800B0078->target[2];
        y = func_8009CF78(2, EVENT_OPERAND_BYTE(4));
        break;
    case 3:
        angle = func_800ACDEC(1) & 0xFFF;
        x = (D_800B0078->target[0] + (func_8003F8CC(angle) << 5)) >> 12;
        z = D_800B0078->target[2] + (-(func_8003F8B0(angle) << 5) >> 12);
        y = D_800B0078->target[1] + func_8009CF78(3, EVENT_OPERAND_BYTE(7));
        break;
    }
    from_x = WHOLE(D_800B0078->position[0]);
    from_z = WHOLE(D_800B0078->position[2]);
    from_y = WHOLE(D_800B0078->position[1]);
    delta.vx = from_x - x;
    delta.vy = from_y - y;
    delta.vz = from_z - z;
    VectorNormal(&delta, &direction);
    scale = model->speed >> 8;
    step.vx = -((direction.vx * scale) >> 4);
    step.vy = -((direction.vy * scale) >> 4);
    step.vz = -((direction.vz * scale) >> 4);
    D_800B0078->unk40[0] = step.vx;
    D_800B0078->unk40[1] = step.vy;
    D_800B0078->unk40[2] = step.vz;
    D_800B0078->unk40[1] = 0;
    distance = func_80099A04(x - from_x, y - from_y, z - from_z);
    if (WHOLE(D_800B0078->unk40[0]) == 0 && WHOLE(D_800B0078->unk40[2]) == 0) {
        turning = 0;
    }
    D_800B0078->flags |= 0x400000;
    if (D_800B0078->slots[D_800B0078->slot].value == 0 || reach + extra >= distance) {
        if (turning == -1) {
            if (speed != 0) {
                if (!(D_800B0078->flags & 0x8000)) {
                    D_800B0078->heading_goal = D_800B0078->heading = (u16)D_800B0078->heading_goal | 0x8000;
                } else {
                    D_800B0078->heading_goal = D_800B0078->heading = D_800B0078->unk11C | 0x8000;
                }
            } else {
                D_800B0078->heading_goal = D_800B0078->heading = func_8007B694(&step) | 0x8000;
            }
        }
        D_800B0078->unkEC = (D_800B0078->position[1] + step.vy) >> 16;
        D_800B0078->slots[D_800B0078->slot].move_mode = 0;
        D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
        return 0;
    }
    if (turning == -1) {
        D_800B0078->heading_goal = D_800B0078->heading = func_8007B694(&step) | 0x8000;
    }
    D_800B0078->unkEC = (D_800B0078->position[1] + step.vy) >> 16;
    D_800B0078->flags |= 0x40000;
    D_800B0078->slots[D_800B0078->slot].value--;
    D_800B00C0 = 1;
    return -1;
}

s32 func_80099AC0(s32 speed);

/* Turn-move the current actor (80099ac0 move mode 2) toward the actor of
 * selector byte 1 for at most operand-2 steps (latched in the slot); continues
 * at +4 once within reach, out of steps or when no actor is selected, else
 * yields. */
void func_80098038(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(2);
    }
    if (func_80099AC0(func_800ACDEC(2)) == 0) {
        D_800B0078->pc += 4;
    }
}

/* Turn-move the current actor (80099ac0 move mode 2) toward the actor of
 * selector byte 1 (reach widened by both actors' +1e radii) without a step
 * limit; continues at +2 once within reach or when no actor is selected, else
 * yields. */
void func_800980FC(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80099AC0(0xFFFF) == 0) {
        D_800B0078->pc += 2;
    }
}

/* Turn-move the current actor (80099ac0 move mode 3; the start position is kept
 * as the target) toward the point 4096 units along angle operand 1 from its
 * start, for at most operand-3 steps (latched in the slot); continues at +5
 * once within reach or out of steps, else yields. */
void func_80098184(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 3;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(3);
    }
    if (func_80099AC0(func_800ACDEC(3)) == 0) {
        D_800B0078->pc += 5;
    }
}

/* Turn-move the current actor (80099ac0 move mode 1) toward its start position
 * offset by selected x/z operands 1/3 (flags byte 5) for at most operand-6
 * steps (latched in the slot); continues at +8 once within reach or out of
 * steps, else yields. */
void func_80098274(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(6);
    }
    if (func_80099AC0(func_800ACDEC(6)) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Turn-move the current actor (80099ac0 move mode 1) toward its start position
 * offset by selected x/z operands 1/3 (flags byte 5) without a step limit;
 * continues at +6 once within reach, else yields. */
void func_80098370(void) {
    if (D_800B0078->slots[D_800B0078->slot].move_mode == 0) {
        D_800B0078->slots[D_800B0078->slot].move_mode = 1;
        D_800B0078->target[0] = WHOLE(D_800B0078->position[0]);
        D_800B0078->target[1] = WHOLE(D_800B0078->position[1]);
        D_800B0078->target[2] = WHOLE(D_800B0078->position[2]);
    }
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80099AC0(0xFFFF) == 0) {
        D_800B0078->pc += 6;
    }
}

/* Turn-move the current actor (80099ac0 move mode 0) toward selected x/z
 * operands 1/3 (flags byte 5) for at most operand-6 steps (latched in the
 * slot); continues at +8 once within reach or out of steps, else yields. */
void func_80098430(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 0;
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(6);
    }
    if (func_80099AC0(func_800ACDEC(6)) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Event fe 1d: set the piece drift vector (800b2078 piece_drift) to selected
 * operands 1, 3, 5 (flags 0x80/0x40/0x20 of byte 7) and enable it
 * (piece_drift_mode bit 0x80). */
void func_800984EC(void) {
    D_800B2078.piece_drift[0] = func_8009CF78(1, EVENT_OPERAND_BYTE(7));
    D_800B2078.piece_drift[1] = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    D_800B2078.piece_drift[2] = func_8009D000(5, EVENT_OPERAND_BYTE(7));
    D_800B2078.piece_drift_mode |= 0x80;
    D_800B0078->pc += 8;
}

/* Event 0x74 (debug): print variable op1 unless 800c268c is set. */
void func_800985BC(void) {
    s32 value;

    if (D_800C268C == 0) {
        value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
        func_800379C8("DEB=%xh %d \n", value, value);
    }
    D_800B0078->pc += 3;
}

/* Event fe 73: store the planar length (80099a4c) of (op7 - op3, op9 - op5) in
 * variable operand 1; x1, z1, x2, z2 are selected operands by flags
 * 0x40/0x20/0x10/0x08 of byte 11. */
void func_8009861C(void) {
    s32 x1;
    s32 z1;
    s32 x2;
    s32 z2;

    x1 = func_8009CFBC(3, EVENT_OPERAND_BYTE(11));
    z1 = func_8009D000(5, EVENT_OPERAND_BYTE(11));
    x2 = func_8009D044(7, EVENT_OPERAND_BYTE(11));
    z2 = func_8009D088(9, EVENT_OPERAND_BYTE(11));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, func_80099A4C(x2 - x1, z2 - z1));
    D_800B0078->pc += 12;
}

/* Event fe 76: store the distance (80099a04) between the points (op3, op5, op7)
 * and (op9, op11, op13) in variable operand 1; selected operands by flags 0x40,
 * 0x20, 0x20, 0x10, 0x08 and 0x08 of byte 15, the pairs as the code reads them.
 */
void func_80098738(void) {
    s32 x1;
    s32 y1;
    s32 z1;
    s32 x2;
    s32 y2;
    s32 z2;

    x1 = func_8009CFBC(3, EVENT_OPERAND_BYTE(15));
    y1 = func_8009D000(5, EVENT_OPERAND_BYTE(15));
    z1 = func_8009D000(7, EVENT_OPERAND_BYTE(15));
    x2 = func_8009D044(9, EVENT_OPERAND_BYTE(15));
    y2 = func_8009D088(11, EVENT_OPERAND_BYTE(15));
    z2 = func_8009D088(13, EVENT_OPERAND_BYTE(15));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, func_80099A04(x2 - x1, z2 - z1, y2 - y1));
    D_800B0078->pc += 16;
}

s32 func_80073930(s32 a, s32 b, s32 c);

/* Event fe 72: store 80073930(op3, op5, op7) in variable operand 1: angle op3
 * turned toward op5 by step op7 the short way round, stopping at the goal
 * (12-bit); selected operands by flags 0x40/0x20/0x10 of byte 9. */
void func_800988B8(void) {
    s32 a;
    s32 b;
    s32 value;

    a = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    b = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    value = func_80073930(a, b, func_8009D044(7, EVENT_OPERAND_BYTE(9)));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 10;
}

/* Event fe 71: store the current actor's facing goal (+106, 12 bits) in
 * variable operand 1. */
void func_8009899C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B0078->heading_goal & 0xFFF);
    D_800B0078->pc += 3;
}

/* Event fe 75: store the facing goal (12 bits) of the actor selected by byte 1
 * in variable operand 2 (no actor: nothing is stored). */
void func_800989F0(void) {
    s32 index;
    FieldActor *actor;

    index = func_8009CDB4(1);
    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(2) & 0xFFFF, actor->heading_goal & 0xFFF);
    }
    D_800B0078->pc += 4;
}

/* Event fe 1c: place the current actor at x, z, y = selected operands 1, 3, 5
 * (whole units; flags 0x80/0x40/0x20 of byte 7), set its flag 0x10000 and layer
 * flag 0x200000, and mirror the position into its descriptor and model. */
void func_80098A7C(void) {
    Sprite *model;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800B0078->flags |= 0x10000;
    D_800B0078->layer_flags |= 0x200000;
    D_800B0078->position[0] = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800B0078->position[2] = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800B0078->position[1] = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = WHOLE(D_800B0078->position[0]);
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = WHOLE(D_800B0078->position[1]);
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = WHOLE(D_800B0078->position[2]);
    model->x = D_800B0078->position[0];
    model->y = D_800B0078->position[1];
    model->z = D_800B0078->position[2];
    D_800B0078->pc += 8;
}

void func_80098CAC(s32 mode);

/* Walk the current actor in a straight line without a step limit (80098cac mode
 * 0; the slot value is 0xffff). Byte 1 zero (9 bytes): set up a walk to
 * selected x/z/y operands 2/4/6 (flags byte 8, bits 0x80/0x40/0x20) over
 * distance / speed steps (speed (0x4000000 / +76) >> 16, doubled with layer
 * flag 0x2000) and continue at +9, the step instruction. Byte 1 non-zero (2
 * bytes): step the walk set up 9 bytes earlier (re-reading its operands),
 * yielding each frame; on arrival snap to the target, play the arrival
 * animation (+e6) and continue at +2. */
void func_80098C00(void) {
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    func_80098CAC(0);
}

/* Walk as 10 with a step limit (80098cac mode 1): the setup form (byte 1 zero,
 * 9 bytes) latches operand 11 in the slot, which is operand 2 of the following
 * step form (byte 1 non-zero, 4 bytes). A walk stopped by the limit does not
 * snap to the target; the step form continues at +4. */
void func_80098C3C(void) {
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(11);
    }
    func_80098CAC(1);
}

/* Walk the current actor to an operand position over a step count derived
 * from its speed (first call sets the step, later calls advance it); at the
 * end snap to the target (when the slot asks) and continue with the next
 * instruction, then update the model matrix and its animation. */
void func_80098CAC(s32 mode) {
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

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    if (D_800AF880.components.descriptors[D_800AFD1C].actor->layer_flags & 0x2000) {
        speed = (0x8000000 / (u16)D_800B0078->unk76) >> 16;
    } else {
        speed = (0x4000000 / (u16)D_800B0078->unk76) >> 16;
    }
    if (speed == 0) {
        speed = 1;
    }
    D_800B0078->flags |= 0x10000;
    pc = D_800B0078->pc;
    code = pc + D_800ADC00;
    animation = 1;
    if (code[1] == 0) {
        x = func_8009CF78(2, code[8]) << 16;
        z = func_8009CFBC(4, EVENT_OPERAND_BYTE(8)) << 16;
        y = func_8009D000(6, EVENT_OPERAND_BYTE(8)) << 16;
        steps = func_80099A04((x - D_800B0078->position[0]) >> 16, (y - D_800B0078->position[1]) >> 16,
                              (z - D_800B0078->position[2]) >> 16) / speed;
        D_800B0078->unk102 = steps;
        if ((s16)steps == 0) {
            D_800B0078->unk102 = steps + 1;
        }
        D_800B0078->target[0] = (x - D_800B0078->position[0]) / (s16)D_800B0078->unk102;
        D_800B0078->target[1] = (y - D_800B0078->position[1]) / (s16)D_800B0078->unk102;
        D_800B0078->target[2] = (z - D_800B0078->position[2]) / (s16)D_800B0078->unk102;
        if (x >> 16 != WHOLE(D_800B0078->position[0]) || z >> 16 != WHOLE(D_800B0078->position[2])) {
            D_800B0078->heading_goal = D_800B0078->heading = -ratan2(D_800B0078->target[2] >> 16, WHOLE(D_800B0078->target[0]));
        }
        D_800B0078->pc += 9;
    } else {
        if ((s16)D_800B0078->unk102 <= 0 || D_800B0078->slots[D_800B0078->slot].value == 0) {
            D_800B0078->pc = pc - 9;
            if (D_800B0078->slots[D_800B0078->slot].value != 0) {
                from.vx = D_800B0078->position[0];
                from.vy = D_800B0078->position[1];
                from.vz = D_800B0078->position[2];
                D_800B0078->position[0] = func_8009CF78(2, EVENT_OPERAND_BYTE(8)) << 16;
                D_800B0078->position[2] = func_8009CFBC(4, EVENT_OPERAND_BYTE(8)) << 16;
                D_800B0078->position[1] = func_8009D000(6, EVENT_OPERAND_BYTE(8)) << 16;
                D_800B0078->unk030[0] = D_800B0078->position[0] - from.vx;
                D_800B0078->unk030[1] = D_800B0078->position[1] - from.vy;
                D_800B0078->unk030[2] = D_800B0078->position[2] - from.vz;
            }
            animation = D_800B0078->unkE6;
            if (mode == 0) {
                D_800B0078->pc += 11;
            } else {
                D_800B0078->pc += 13;
            }
            D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
        } else {
            D_800B0078->position[0] += D_800B0078->target[0];
            D_800B0078->position[2] += D_800B0078->target[2];
            D_800B0078->position[1] += D_800B0078->target[1];
            D_800B0078->unk030[0] = D_800B0078->target[0];
            D_800B0078->unk030[1] = D_800B0078->target[1];
            D_800B0078->unk030[2] = D_800B0078->target[2];
            D_800B0078->slots[D_800B0078->slot].value--;
            D_800B00C0 = animation;
        }
        D_800B0078->unk102--;
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = WHOLE(D_800B0078->position[0]);
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = WHOLE(D_800B0078->position[1]);
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = WHOLE(D_800B0078->position[2]);
        model->x = D_800B0078->position[0];
        model->y = D_800B0078->position[1];
        model->z = D_800B0078->position[2];
    }
    if (D_800B0078->unk0EA != 0xFF) {
        animation = D_800B0078->unk0EA;
    }
    if (D_800B0078->unkE8 != animation && !(D_800B0078->flags & 0x2000000)) {
        D_800B0078->unkE8 = animation;
        func_800821F4(model, animation, D_800B06B8);
    }
    ((void (*)(void *, s32, FieldDescriptor *))func_80081F80)(model, D_800B0078->heading, D_800B06B8);
}

/* Arc jump of the current actor. Byte 1 & 3 = 0-2 sets one up (11 bytes):
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
void func_80099214(void) {
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

    pc = D_800B0078->pc;
    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    code = pc + D_800ADC00;
    D_800B0078->flags |= 0x10000;
    mode = code[1];
    switch (mode & 3) {
    case 0:
        steps = func_8009D044(8, code[10]);
    setup:
        if (steps == 0) {
            steps = 1;
        }
        x = func_8009CF78(2, EVENT_OPERAND_BYTE(10));
        z = func_8009CFBC(4, EVENT_OPERAND_BYTE(10));
        if (!(EVENT_OPERAND_BYTE(1) & 0x80)) {
            y = func_8009D000(6, EVENT_OPERAND_BYTE(10));
        } else {
            layer = func_8009D000(6, EVENT_OPERAND_BYTE(10));
            func_8007B1C4(x, z, layer, &points[layer], &normals[layer]);
            y = points[layer].vy;
            D_800B0078->layer = layer;
        }
        model->speed_y = -(model->gravity * steps / 2);
        model->speed_y += ((y << 16) - D_800B0078->position[1]) / steps;
        D_800B0078->target[1] = 0;
        ACTOR_ARC_STEPS(D_800B0078) = steps;
        D_800B0078->unk102 = 0;
        D_800B0078->pc += 11;
        D_800B0078->target[0] = ((x << 16) - D_800B0078->position[0]) / (steps + 1);
        D_800B0078->target[2] = ((z << 16) - D_800B0078->position[2]) / (steps + 1);
        break;
    case 1:
        x = func_8009CF78(2, code[10]);
        z = func_8009CFBC(4, EVENT_OPERAND_BYTE(10));
        x = (x << 16) - D_800B0078->position[0];
        z = (z << 16) - D_800B0078->position[2];
        steps = func_8009D044(8, EVENT_OPERAND_BYTE(10));
        steps = func_80099A4C(x >> 16, z >> 16) / steps;
        goto setup;
    case 2:
        func_8009CF78(2, code[10]);
        func_8009CFBC(4, EVENT_OPERAND_BYTE(10));
        y = func_8009D000(6, EVENT_OPERAND_BYTE(10));
        peak = -func_8009D044(8, EVENT_OPERAND_BYTE(10));
        y = (y << 16) - D_800B0078->position[1];
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
            for (layer = 0; layer < D_800AF880.components.layer_count - 1; layer++) {
                D_800B0078->triangle[layer] = func_8007B1C4(WHOLE(D_800B0078->position[0]), WHOLE(D_800B0078->position[2]),
                                                            layer, &points[layer], &normals[layer]);
            }
            D_800B0078->flags &= ~0x10000;
            D_800B0078->layer_flags &= ~0x200000;
            D_800B0078->pc += 2;
            break;
        }
        if ((s16)D_800B0078->unk102 < ACTOR_ARC_STEPS(D_800B0078)) {
            D_800B0078->position[0] += D_800B0078->target[0];
            D_800B0078->position[2] += D_800B0078->target[2];
            D_800B0078->position[1] += model->speed_y;
            model->speed_y += model->gravity;
            if ((D_800B0078->target[0] != 0 || D_800B0078->target[2] != 0) && !(D_800B0078->flags & 0x8000)) {
                value = func_8007B694((VECTOR *)D_800B0078->target) | 0x8000;
                D_800B0078->heading = value;
                D_800B0078->heading_goal = value;
            }
        } else {
            D_800B0078->pc = pc - 11;
            x = func_8009CF78(2, EVENT_OPERAND_BYTE(10));
            z = func_8009CFBC(4, EVENT_OPERAND_BYTE(10));
            if (EVENT_OPERAND_BYTE(1) & 0x80) {
                layer = func_8009D000(6, EVENT_OPERAND_BYTE(10));
                D_800B0078->triangle[layer] = func_8007B1C4(x, z, layer, &points[layer], &normals[layer]);
                y = points[layer].vy;
            } else {
                y = func_8009D000(6, EVENT_OPERAND_BYTE(10));
            }
            model->speed_y = 0;
            D_800B0078->position[0] = x << 16;
            D_800B0078->position[1] = y << 16;
            D_800B0078->position[2] = z << 16;
            D_800B0078->flags &= ~0x10000;
            D_800B0078->layer_flags &= ~0x200000;
            D_800B0078->pc += 13;
        }
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = WHOLE(D_800B0078->position[0]);
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = WHOLE(D_800B0078->position[1]);
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = WHOLE(D_800B0078->position[2]);
        model->x = D_800B0078->position[0];
        model->y = D_800B0078->position[1];
        model->z = D_800B0078->position[2];
        D_800B0078->unk102++;
        break;
    }
    D_800B00C0 = 1;
}

/* Turn-move the current actor (80099ac0 move mode 0) toward selected x/z
 * operands 1/3 (flags byte 5) without a step limit; continues at +6 once within
 * reach, else yields. */
void func_80099980(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 0;
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80099AC0(0xFFFF) == 0) {
        D_800B0078->pc += 6;
    }
}

/* Length of (dx, dy, dz). */
s32 func_80099A04(s32 dx, s32 dy, s32 dz) {
    VECTOR v;
    VECTOR squares;

    v.vx = dx;
    v.vy = dy;
    v.vz = dz;
    func_8004A414(&v, &squares);
    return SquareRoot0(squares.vx + squares.vy + squares.vz);
}

/* Length of (dx, dz). */
s32 func_80099A4C(s32 dx, s32 dz) {
    VECTOR v;
    VECTOR squares;

    v.vx = dx;
    v.vy = dz;
    v.vz = 0;
    func_8004A414(&v, &squares);
    return SquareRoot0(squares.vx + squares.vy);
}

/* Absolute value through the GTE square and square root. */
s32 func_80099A8C(s32 x) {
    VECTOR v;
    VECTOR squares;

    v.vx = x;
    func_8004A414(&v, &squares);
    return SquareRoot0(squares.vx);
}

/* Compiled-out debug trace of the actor a move targets. */
#define MOVE_TRACE_TARGET(actor) do { } while (0)

/* Turn-move toward the slot's target (mode 1: operand position plus the
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
s32 func_80099AC0(s32 speed) {
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
    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    x = 0;
    if (D_800AF880.components.descriptors[D_800AFD1C].actor->layer_flags & 0x2000) {
        model->speed = 0x8000000 / (u16)D_800B0078->unk76;
    } else if (model->speed == 0) {
        model->speed = 0x4000000 / (u16)D_800B0078->unk76;
    }
    reach = func_80099A8C(model->speed >> 15) + 1;
    from_x = WHOLE(D_800B0078->position[0]);
    from_z = WHOLE(D_800B0078->position[2]);
    switch (D_800B0078->slots[D_800B0078->slot].move_mode) {
    case 1:
        x = func_8009CF78(1, EVENT_OPERAND_BYTE(5)) + D_800B0078->target[0];
        z = func_8009CFBC(3, EVENT_OPERAND_BYTE(5)) + D_800B0078->target[2];
        break;
    case 2:
        if (func_8009CDB4(1) == 0xFF) {
            return 0;
        }
        index = func_8009CDB4(1);
        value = (s32)D_800B0078;
        model = (Sprite *)D_800AF880.components.descriptors;
        other = ((FieldDescriptor *)model)[index].actor;
        MOVE_TRACE_TARGET(other);
        extra = func_80099A8C((u16)other->gravity.s.whole + (u16)((FieldActor *)value)->gravity.s.whole);
        x = WHOLE(other->position[0]);
        z = WHOLE(other->position[2]);
        if (EVENT_OPERAND_BYTE(1) == D_800B2078.controlled) {
            D_800B0078->flags |= 0x200000;
        }
        break;
    case 3:
        angle = func_800ACDEC(1) & 0xFFF;
        x = D_800B0078->target[0] + ((func_8003F8CC(angle) << 12) >> 12);
        z = D_800B0078->target[2] + (-(func_8003F8B0(angle) << 12) >> 12);
        break;
    case 0:
    case 4:
        x = func_8009CF78(1, EVENT_OPERAND_BYTE(5));
        z = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
        break;
    }
    delta.vx = x - from_x;
    delta.vy = 0;
    delta.vz = z - from_z;
    distance = func_80099A4C(delta.vx, delta.vz);
    D_800B0078->flags |= 0x400000;
    value = D_800B0078->slots[D_800B0078->slot].value;
    if (value == 0 || reach + extra >= distance) {
        if (speed != 0) {
            if (!(D_800B0078->flags & 0x8000)) {
                D_800B0078->heading_goal = D_800B0078->heading = (u16)D_800B0078->heading_goal | 0x8000;
            } else {
                D_800B0078->heading_goal = D_800B0078->heading = D_800B0078->unk11C | 0x8000;
            }
        } else {
            D_800B0078->heading_goal = D_800B0078->heading = func_8007B694(&delta);
        }
        D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
        D_800B0078->slots[D_800B0078->slot].move_mode = 0;
        D_800B0078->flags &= 0xFDDFF7FF;
        return 0;
    }
    D_800B0078->slots[D_800B0078->slot].value = value - 1;
    value = func_8007B694(&delta);
    D_800B00C0 = 1;
    D_800B0078->heading_goal = D_800B0078->heading = value;
    return -1;
}

/* Store the current actor's party character (+e4, set by 16) in variable
 * operand 1. */
void func_80099EF8(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B0078->unkE4);
    D_800B0078->pc += 3;
}

/* Store the controlled actor's party character (+e4, set by 16) in variable
 * operand 1. */
void func_80099F48(void) {
    FieldActor *player;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, player->unkE4);
    D_800B0078->pc += 3;
}

/* Store the current actor's facing octant in variable operand 1. */
void func_80099FC4(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, (((D_800B0078->heading_goal + 0x100) >> 9) + 2) & 7);
    D_800B0078->pc += 3;
}

/* Store the translation x, z, y of the descriptor of selector byte 1 in
 * variables operand 2, 4 and 6 (nothing when no actor is selected). */
void func_8009A024(void) {
    s32 index;

    index = func_8009CDB4(1);
    if (index != 0xFF) {
        func_800A3074(func_800ACDB8(2) & 0xFFFF, D_800AF880.components.descriptors[index].transform.t[0]);
        func_800A3074(func_800ACDB8(4) & 0xFFFF, D_800AF880.components.descriptors[index].transform.t[2]);
        func_800A3074(func_800ACDB8(6) & 0xFFFF, D_800AF880.components.descriptors[index].transform.t[1]);
    }
    D_800B0078->pc += 8;
}

/* Event fe 45: set the current actor's idle animation (+e6, which a walk
 * (80098cac) ends with) from byte 1. */
void func_8009A0FC(void) {
    D_800B0078->unkE6 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Set the current actor's requested animation (+ea; the frame update and walks
 * play it while it is not 0xff) to byte 1 and clear its layer flag 0x1000000.
 */
void func_8009A130(void) {
    D_800B0078->layer_flags &= ~0x1000000;
    D_800B0078->unk0EA = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Set the requested animation (+ea) from byte 1 as 2c and clear layer flag
 * 0x10000, which the sprite's completion callback (80076a74) sets and 5e waits
 * for. */
void func_8009A174(void) {
    func_8009A130();
    D_800B0078->layer_flags &= ~0x10000;
}

/* Wait, retried without yielding, until the current actor's sprite completion
 * callback (80076a74) has set layer flag 0x10000, then clear the requested
 * animation (+ea = 0xff) and continue at +1. */
void func_8009A1AC(void) {
    if (D_800B0078->layer_flags & 0x10000) {
        D_800B0078->unk0EA = 0xFF;
        D_800B0078->pc += 1;
    }
}

/* Face the actor of party slot operand 1. */
void func_8009A1E4(void) {
    s32 index;
    FieldActor *other;
    s16 facing;

    index = D_8005A444[EVENT_OPERAND_BYTE(1)];
    if (index != 0xFF) {
        other = D_800AF880.components.descriptors[index].actor;
        facing = -ratan2(other->position[2] - D_800B0078->position[2],
                                other->position[0] - D_800B0078->position[0]) | 0x8000;
        D_800B0078->heading = facing;
        D_800B0078->heading_goal = facing;
    }
    D_800B0078->pc += 2;
}

/* Face a selected actor. */
void func_8009A2A8(void) {
    s32 index;
    FieldActor *other;
    s16 facing;

    index = func_8009CDB4(1);
    if (index != 0xFF) {
        other = D_800AF880.components.descriptors[index].actor;
        facing = -ratan2(other->position[2] - D_800B0078->position[2],
                                other->position[0] - D_800B0078->position[0]) | 0x8000;
        D_800B0078->heading = facing;
        D_800B0078->heading_goal = facing;
    }
    D_800B0078->pc += 2;
}

/* Blend the camera distance toward operand 1 over byte-3 frames (0: one
 * frame, with the camera counter raised by 2). */
void func_8009A34C(void) {
    s32 step;

    D_800AF880.steps = EVENT_OPERAND_BYTE(3);
    if (D_800AF880.steps == 0) {
        D_800AF880.steps++;
        D_800B2078.camera_counter += 2;
    }
    step = -((D_800AF880.distance - func_800ACDEC(1)) << 16) / D_800AF880.steps;
    D_800AF880.start = D_800AF880.distance << 16;
    D_800AF880.flags |= 1;
    D_800AF880.step = step;
    D_800B0078->pc += 4;
}

/* Blend the camera elevation toward `target` over `steps` frames. */
void func_8009A420(s32 target, s32 steps) {
    if (steps == 0) {
        D_800B2078.camera_counter = 2;
        steps = 1;
    }
    D_800AF880.elevation_steps = steps;
    D_800AF880.elevation_value = (s16)D_800AF880.elevation << 16;
    D_800AF880.flags |= 8;
    D_800AF880.elevation_step = -(((s16)D_800AF880.elevation - target) << 16) / steps;
}

/* Blend the camera elevation (selected operand 1, steps operand 3 & 0x7F). */
void func_8009A490(void) {
    func_8009A420(func_8009CF78(1, EVENT_OPERAND_BYTE(3)), EVENT_OPERAND_BYTE(3) & 0x7F);
    D_800B0078->pc += 4;
}

/* Camera angle octant (0..7). */
s32 func_8009A514(void) {
    return (7 - ((D_800AF880.angle - 0x100) >> 9)) & 7;
}

/* Store the camera heading octant (8009a514) in variable operand 1. */
void func_8009A534(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, func_8009A514() & 0xFFFF);
    D_800B0078->pc += 3;
}

/* Yield while any camera flag in operand byte 1 is set. */
void func_8009A58C(void) {
    if (!(D_800AF880.flags & EVENT_OPERAND_BYTE(1))) {
        D_800B0078->pc += 2;
        return;
    }
    D_800B00C0 = 1;
}

/* Yield while any camera flag in operand byte 1 is set (second opcode). */
void func_8009A5E0(void) {
    if (!(D_800AF880.flags & EVENT_OPERAND_BYTE(1))) {
        D_800B0078->pc += 2;
        return;
    }
    D_800B00C0 = 1;
}

/* Set camera heading mask 0 (800af880 +174) to operand 1: while the camera
 * faces an octant whose bit is set it turns on by one octant; 0xff in
 * either mask stops these and the shoulder-button turns (800726e8). */
void func_8009A634(void) {
    D_800AF880.heading_blocks[0] = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Set camera heading mask 1 (800af880 +175) to operand 1: the camera turns
 * away from octants whose bit is set and shoulder-button turns skip them;
 * 0xff in either mask stops these turns (800726e8). */
void func_8009A670(void) {
    D_800AF880.heading_blocks[1] = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Store (sin(angle) * length) >> 12 in variable operand 1: angle selected
 * operand 3 (bit 0x40 of byte 7), length selected operand 5 (bit 0x20). */
void func_8009A6AC(void) {
    s32 reference;
    s32 angle;
    s32 length;

    reference = func_800ACDB8(1) & 0xFFFF;
    angle = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    length = func_8009D000(5, EVENT_OPERAND_BYTE(7));
    func_800A3074(reference & 0xFFFF, (func_8003F8B0(angle) * length) >> 12);
    D_800B0078->pc += 8;
}

/* Store (cos(angle) * length) >> 12 in variable operand 1: angle selected
 * operand 3 (bit 0x40 of byte 7), length selected operand 5 (bit 0x20). */
void func_8009A768(void) {
    s32 reference;
    s32 angle;
    s32 length;

    reference = func_800ACDB8(1) & 0xFFFF;
    angle = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    length = func_8009D000(5, EVENT_OPERAND_BYTE(7));
    func_800A3074(reference & 0xFFFF, (func_8003F8CC(angle) * length) >> 12);
    D_800B0078->pc += 8;
}

/* Store atan2(selected operand 3, selected operand 5) in variable operand 1
 * (flags byte 7: 0x40, 0x20). */
void func_8009A824(void) {
    s32 reference;
    s32 y;

    reference = func_800ACDB8(1) & 0xFFFF;
    y = func_8009CFBC(3, EVENT_OPERAND_BYTE(7));
    func_800A3074(reference & 0xFFFF, (s16)ratan2(y, func_8009D000(5, EVENT_OPERAND_BYTE(7))));
    D_800B0078->pc += 8;
}

/* The current actor's facing octant (0..7). */
s32 func_8009A8DC(void) {
    return (((D_800B0078->heading_goal + 0x100) >> 9) + 2) & 7;
}

/* Face `angle`; outside D_800ADB1C also sets the rest facing. */
void func_8009A904(u16 angle) {
    if (D_800ADB1C == 0) {
        D_800B0078->heading = angle | 0x8000;
        D_800B0078->heading_goal = angle | 0x8000;
        D_800B0078->unk108 = angle | 0x8000;
    }
    D_800B0078->heading = angle | 0x8000;
    D_800B0078->heading_goal = angle | 0x8000;
    D_800B0078->pc += 3;
}

/* A selected actor faces `angle`. */
void func_8009A958(u16 angle) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        if (D_800ADB1C == 0) {
            actor->heading = angle | 0x8000;
            actor->heading_goal = angle | 0x8000;
            actor->unk108 = angle | 0x8000;
        }
        actor->heading = angle | 0x8000;
        actor->heading_goal = angle | 0x8000;
    }
    D_800B0078->pc += 4;
}

/* Selected actor operand 1 faces selected actor operand 2. */
void func_8009AA00(void) {
    FieldActor *actor;
    FieldActor *other;
    s16 facing;

    if (func_8009CDB4(1) != 0xFF && func_8009CDB4(2) != 0xFF) {
        other = D_800AF880.components.descriptors[func_8009CDB4(2)].actor;
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        facing = -ratan2(other->position[2] - actor->position[2],
                                other->position[0] - actor->position[0]) | 0x8000;
        if (D_800ADB1C == 0) {
            actor->heading = facing;
            actor->heading_goal = facing;
            actor->unk108 = facing;
        }
        actor->heading = facing;
        actor->heading_goal = facing;
    }
    D_800B0078->pc += 3;
}

/* Face `angle` relative to the camera. */
void func_8009AB08(u16 angle) {
    s16 facing;

    facing = ((angle - D_800AF880.angle) & 0xFFF) | 0x8000;
    D_800B0078->heading = facing;
    D_800B0078->heading_goal = facing;
    if (D_800ADB1C == 0) {
        D_800B0078->unk108 = facing;
    }
    D_800B0078->pc += 3;
}

extern s16 D_800AEA34[8];

/* Turn clockwise by operand-1 octants. */
void func_8009AB5C(void) {
    s32 turn;

    turn = func_800ACDEC(1);
    func_8009A904(D_800AEA34[(turn + func_8009A8DC()) & 7]);
}

/* Turn counter-clockwise by operand-1 octants. */
void func_8009ABAC(void) {
    s32 turn;

    turn = func_800ACDEC(1);
    func_8009A904(D_800AEA34[(func_8009A8DC() - turn) & 7]);
}

void func_8009A958(u16 angle);

/* A selected actor faces direction table entry operand 2. */
void func_8009ABFC(void) {
    func_8009A958(D_800AEA34[func_800ACDEC(2)]);
}

/* A selected actor faces direction entry operand 2, camera-relative. */
void func_8009AC34(void) {
    func_8009A958((D_800AEA34[func_800ACDEC(2)] - D_800AF880.angle) & 0xFFF);
}

/* Face direction table entry operand 1. */
void func_8009AC7C(void) {
    func_8009A904(D_800AEA34[func_800ACDEC(1)]);
}

void func_8009AB08(u16 angle);

/* Face direction table entry operand 1, camera-relative. */
void func_8009ACB4(void) {
    func_8009AB08(D_800AEA34[func_800ACDEC(1)]);
}

extern s16 D_800AEA44[8];

/* Face second-table direction operand byte 1, camera-relative. */
void func_8009ACEC(void) {
    s16 facing;

    facing = ((D_800AEA44[EVENT_OPERAND_BYTE(1)] - D_800AF880.angle) & 0xFFF) | 0x8000;
    D_800B0078->heading = facing;
    D_800B0078->heading_goal = facing;
    if (D_800ADB1C == 0) {
        D_800B0078->unk108 = facing;
    }
    D_800B0078->pc += 2;
}

extern s16 D_800AEA54[8];

/* Face third-table direction operand byte 1. */
void func_8009AD6C(void) {
    s16 facing;

    facing = D_800AEA54[EVENT_OPERAND_BYTE(1)] | 0x8000;
    D_800B0078->heading = facing;
    D_800B0078->heading_goal = facing;
    if (D_800ADB1C == 0) {
        D_800B0078->unk108 = facing;
    }
    D_800B0078->pc += 2;
}

/* Set camera flag 0x4000. */
void func_8009ADDC(void) {
    D_800AF880.flags |= 0x4000;
    D_800B0078->pc += 1;
}

/* Clear camera flag 0x4000 (and the upper half). */
void func_8009AE0C(void) {
    D_800AF880.flags &= 0xBFFF;
    D_800B0078->pc += 1;
}

/* Blend the camera projection toward `target` over `steps` frames (at
 * once when zero). */
void func_8009AE3C(s32 target, s32 steps) {
    if (steps != 0) {
        D_800AF880.flags |= 0x10;
        D_800AF880.projection_steps = steps;
        D_800AF880.projection_value = D_800AF880.projection << 16;
        D_800AF880.projection_step = -((D_800AF880.projection - target) << 16) / steps;
    } else {
        D_800AF880.projection = target;
        D_800AF880.projection_steps = 0;
        D_800B2078.camera_counter += 2;
    }
    D_800AF880.flags &= 0xDFFF;
}

/* Walk party slot `slot`'s member one gather step toward (x, z). Returns 0
 * when the member is absent, disabled or has arrived (it is then placed at
 * (x, z) facing `facing`, or its own heading for 0xff) and -1 while it is
 * still walking; a member stuck for 0x40 steps or an override warps. */
s32 func_8009AEE0(s32 slot, s32 x, s32 z, s32 facing) {
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

    member = D_8005A444[slot];
    if (member == 0xFF) {
        return 0;
    }
    if (D_800AF880.components.descriptors[member].flags & 0x20) {
        return 0;
    }
    model = D_800AF880.components.descriptors[member].model;
    actor = D_800AF880.components.descriptors[member].actor;
    if (model->speed == 0) {
        model->speed = 0x4000000 / (u16)actor->unk76;
    }
    step = func_80099A8C(model->speed >> 15) + 1;
    dx = x - WHOLE(actor->position[0]);
    dz = z - WHOLE(actor->position[2]);
    delta.vx = dx;
    delta.vy = 0;
    delta.vz = dz;
    distance = func_80099A4C(dx, dz);
    actor->flags |= 0x400000;
    if (step >= distance) {
    arrive:
        if (!(actor->flags & 0x8000)) {
            if (facing == 0xFF) {
                actor->heading_goal = actor->heading = actor->heading_goal | 0x8000;
            } else {
                actor->heading_goal = actor->heading = D_800AEA34[facing] | 0x8000;
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
    actor->heading_goal = actor->heading = func_8007B694(&delta);
    if ((s16)actor->stuck > 0x40 || (s16)D_800B2078.unk2348 != 0) {
        saved = D_800B0078;
        saved_index = D_800AFD1C;
        D_800B0078 = actor;
        D_800AFD1C = D_8005A444[slot];
        func_8009E574(x, z);
        D_800B0078 = saved;
        D_800AFD1C = saved_index;
        goto arrive;
    }
    return -1;
}

/* Force the party position. */
void func_8009B15C(void) {
    D_800B2078.forced_position = 1;
    D_800B0078->pc += 1;
}

void func_80081C54(s32 index);

/* Event fe 44: release forced positioning and party processing, clear the three
 * movement-history indices and preserve_nonplayer_motion, and record the
 * controlled actor's state in its movement history 32 times (80081c54). */
void func_8009B184(void) {
    s32 i;

    i = 0;
    D_800B2078.forced_position = 0;
    D_800B2078.party_processing_mode = 0;
    D_800B2360[2] = 0;
    D_800B2360[1] = 0;
    D_800B2360[0] = 0;
    D_800B2078.preserve_nonplayer_motion = 0;
    do {
        i++;
        func_80081C54(D_800B2078.controlled);
    } while (i < 32);
    D_800B0078->pc += 1;
}

s32 func_8009AEE0(s32 member, s32 x, s32 z, s32 range);
void func_8009B338(void);

/* Event fe 24: walk each party member one gather step (8009aee0) toward the
 * leader's position, keeping its heading, and yield. Once all three have
 * arrived, clear party processing mode and 800b2348, release the motion
 * overrides and refill the controlled actor's movement history (8009b338) and
 * advance; else set party processing mode 1 and wait (pc-- to the fe). */
void func_8009B210(void) {
    FieldActor *leader;
    s32 x;
    s32 z;
    s32 near;

    leader = D_800AF880.components.descriptors[D_8005A444[0]].actor;
    x = WHOLE(leader->position[0]);
    z = WHOLE(leader->position[2]);
    near = func_8009AEE0(0, x, z, 0xFF) == 0;
    if (func_8009AEE0(1, x, z, 0xFF) == 0) {
        near |= 2;
    }
    if (func_8009AEE0(2, x, z, 0xFF) == 0) {
        near |= 4;
    }
    D_800B00C0 = 1;
    if (near == 7) {
        D_800B0078->pc++;
        D_800B2078.party_processing_mode = 0;
        D_800B2078.unk2348 = 0;
        func_8009B338();
    } else {
        D_800B2078.party_processing_mode = 1;
        D_800B0078->pc--;
    }
}

/* Release the party motion overrides and resettle the controlled actor. */
void func_8009B338(void) {
    s32 i;

    i = 0;
    D_800B2360[2] = 0;
    D_800B2360[1] = 0;
    D_800B2360[0] = 0;
    D_800B2078.preserve_nonplayer_motion = 0;
    do {
        i++;
        func_80081C54(D_800B2078.controlled);
    } while (i < 32);
}

/* Event fe 23: gather the party: walk the members of party slots 0, 1 and 2 one
 * step each (8009aee0) toward (op1, op3), (op5, op7) and (op9, op11) (selected
 * operands, flags 0x80..0x04 of byte 13), each facing direction-table entry
 * op14, op16 or op18 on arrival (0xff: its own heading). Until all three have
 * arrived set party processing mode 1, yield and wait (pc-- to the fe); then
 * clear 800b2348 and the mode, yield and advance 20. Op1 = 0x7fff instead marks
 * every member's heading as turned (bit 15) and advances 20 at once. Both set
 * preserve_nonplayer_motion. */
void func_8009B398(void) {
    FieldActor *actor;
    s32 i;
    s32 near;

    if (func_8009CF78(1, EVENT_OPERAND_BYTE(0xD)) == 0x7FFF) {
        D_800B0078->pc += 0x14;
        D_800B2078.preserve_nonplayer_motion = 1;
        i = 0;
        do {
            if (D_8005A444[i] != 0xFF) {
                actor = D_800AF880.components.descriptors[D_8005A444[i]].actor;
                actor->heading_goal = actor->heading = actor->heading_goal | 0x8000;
            }
            i++;
        } while (i < 3);
        return;
    }
    near = func_8009AEE0(0, func_8009CF78(1, EVENT_OPERAND_BYTE(0xD)), func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD)),
                         func_800ACDEC(0xE)) == 0;
    if (func_8009AEE0(1, func_8009D000(5, EVENT_OPERAND_BYTE(0xD)), func_8009D044(7, EVENT_OPERAND_BYTE(0xD)),
                      func_800ACDEC(0x10)) == 0) {
        near |= 2;
    }
    if (func_8009AEE0(2, func_8009D088(9, EVENT_OPERAND_BYTE(0xD)), func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD)),
                      func_800ACDEC(0x12)) == 0) {
        near |= 4;
    }
    D_800B00C0 = 1;
    if (near == 7) {
        D_800B2078.unk2348 = 0;
        D_800B0078->pc += 0x14;
        D_800B2078.party_processing_mode = 0;
    } else {
        D_800B2078.party_processing_mode = 1;
        D_800B0078->pc -= 1;
    }
    D_800B2078.preserve_nonplayer_motion = 1;
}

/* Event fe 22: store the camera projection in variable operand 1. */
void func_8009B664(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.projection);
    D_800B0078->pc += 3;
}

void func_8009AE3C(s32 target, s32 steps);

/* Blend the camera projection toward operand 1 over operand-3 frames
 * (8009ae3c; at once when zero). */
void func_8009B6AC(void) {
    s32 target;

    target = func_800ACDEC(1);
    func_8009AE3C(target, func_800ACDEC(3));
    D_800B0078->pc += 5;
}

extern s16 D_800AEA64[64]; /* octant turn table [from * 8 + to] */

/* Turn the camera to octant `octant` over `steps` frames. */
void func_8009B708(s32 octant, s32 steps) {
    s32 current;
    s32 velocity;

    current = func_8009A514() & 0xFFFF;
    if (steps == 0) {
        steps = 1;
        D_800B2078.camera_counter += 2;
    }
    velocity = (D_800AEA64[current * 8 + octant] << 25) / steps;
    D_800AF880.heading_steps = steps;
    D_800AF880.heading = ((octant + 4) & 7) << 9;
    D_800AF880.heading_velocity = velocity;
}

/* Turn the camera one octant (direction 0: positive) over `steps` frames. */
void func_8009B7A8(s32 direction, s32 steps) {
    s32 velocity;
    s32 heading;

    if (steps == 0) {
        steps = 1;
        D_800B2078.camera_counter += 2;
    }
    if (direction == 0) {
        D_800AF880.heading_velocity = 0x2000000 / steps;
        D_800AF880.heading += 0x200;
    } else {
        D_800AF880.heading_velocity = (s32)0xFE000000 / steps;
        D_800AF880.heading -= 0x200;
    }
    D_800AF880.heading_steps = steps;
}

/* Once the camera heading is idle (yielding until then), turn it one octant
 * in the positive direction over operand-1 frames (8009b7a8). */
void func_8009B824(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B7A8(0, func_800ACDEC(1));
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

/* Same as c7 (the original also passes direction 0): once the camera
 * heading is idle, turn it one octant positive over operand-1 frames. */
void func_8009B884(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B7A8(0, func_800ACDEC(1));
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

void func_8009B708(s32 octant, s32 steps);

/* Turn the camera to octant operand 1 over operand-3 frames (8009b708) once
 * its heading is idle, waiting (yielding) until then; with 800adb1c clear
 * (the immediate runs at load) the heading is set at once. Yields. */
void func_8009B8E4(void) {
    s32 octant;
    s32 steps;
    s16 heading;

    octant = func_800ACDEC(1);
    steps = func_800ACDEC(3);
    if (D_800ADB1C == 0) {
        heading = (octant + 4) & 7;
        D_800AF880.heading_angles.vy = heading << 9;
        D_800AF880.heading = heading << 9;
        D_800B0078->pc += 5;
    } else if (D_800AF880.heading_steps == 0) {
        func_8009B708(octant, steps);
        D_800B0078->pc += 5;
    }
    D_800B00C0 = 1;
}

/* Once the camera is idle, save its octant, projection and elevation. */
void func_8009B9A0(void) {
    if (D_800AF880.heading_steps == 0) {
        D_800AF880.saved_view.octant = func_8009A514();
        D_800AF880.saved_view.projection = D_800AF880.projection;
        D_800AF880.saved_view.elevation = D_800AF880.elevation;
        D_800B0078->pc += 1;
    }
}

/* Once the camera is idle, blend back to the saved view over 32 frames. */
void func_8009BA0C(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B708(D_800AF880.saved_view.octant, 32);
        func_8009AE3C(D_800AF880.saved_view.projection, 32);
        func_8009A420(D_800AF880.saved_view.elevation, 32);
        D_800B0078->pc += 1;
    }
}



/* Set the camera at once: heading octant operand 1, elevation operand 3 and
 * projection operand 5 (SetGeomScreen). */
void func_8009BA7C(void) {
    s16 heading;
    s16 angle;

    D_800AF880.elevation = func_800ACDEC(3);
    heading = (func_800ACDEC(1) + 4) & 7;
    angle = heading << 9;
    D_800AF880.heading = angle;
    D_800AF880.heading_angles.vy = heading << 9;
    D_800AF880.heading_high = angle << 16;
    D_800AF880.projection = func_800ACDEC(5);
    SetGeomScreen(D_800AF880.projection);
    D_800B0078->pc += 7;
}

/* Wait for this actor's dialogue window to close. While it has an open one
 * yield without advancing; once that window's speaker has layer flag 0x200
 * and bit 0 of the actor's window style (+84) is clear, close the window
 * and end the current script slot (unless its priority is 7). With none
 * open, store +81 (the line chosen, see a9) in variable 0x14 and
 * continue. */
void func_8009BB0C(void) {
    s32 window;
    u32 value;
    s32 bits;

    if (func_8009CD18(&window) == -1) {
        D_800AFC7C += 8;
        func_800A3074(0x14, D_800B0078->unk081);
        D_800B0078->pc++;
        return;
    }
    if (D_800AF880.components.descriptors[D_800C2698[window].unk418].actor->layer_flags & 0x200) {
        value = D_800B0078->unk84;
        if (value >> 16) {
            bits = (value >> 16) & 0xFFFF;
        } else {
            bits = value & 0xFFFF;
        }
        if (!(bits & 1)) {
            if (D_800B0078->slots[D_800B0078->slot].priority != 7) {
                func_800A1B70();
            }
            D_800C2698[window].cleared = 0;
        }
    }
    D_800B00C0 = 1;
}

/* Event 0xa9: with an open dialogue window of its own, wait (yielding) until
 * its text box is ready (80033cd0 returns 1, or text box +84 and +6c are
 * set), then offer lines byte1 >> 4 .. byte1 & 0xf as a choice (+81 = 0xff
 * until one is taken) and continue; with none, just continue. Yields. */
void func_8009BC98(void) {
    s32 window;
    u32 first;

    if (func_8009CD18(&window) == 0) {
        D_800AFC7C += 8;
        if (func_80033CD0(&D_800C2698[window].text) == 1 ||
            ((&D_800C2698[window].text)->unk84 != 0 && (&D_800C2698[window].text)->unk6C != 0)) {
            D_800C2698[window].choice.status = 0;
            D_800B0078->unk081 = 0xFF;
            first = EVENT_OPERAND_BYTE(1) >> 4;
            D_800C2698[window].choice.first = first;
            D_800C2698[window].choice.count = (EVENT_OPERAND_BYTE(1) & 0xF) - first + 1;
            D_800C2698[window].choice.index = 0;
            func_80034800(&D_800C2698[window].text, 0xEF, 0x1E, 0xF0);
            D_800B0078->pc += 2;
        }
    } else {
        D_800B0078->pc += 2;
    }
    D_800B00C0 = 1;
}

/* Whether the current actor's octant (state bits 9-11) is within four
 * octants past the camera's. */
s32 func_8009BE58(void) {
    return ((((s32)(D_800B0078->state.word >> 9) & 7) - (func_8009A514() & 0xFFFF)) & 7) < 5;
}

s32 func_8009CD18(s32 *window);


/* With byte 1 zero close this actor's open dialogue window (if any);
 * otherwise clear its window overrides (+82, +83, +84, +88, +8a). Advances
 * and yields. */
void func_8009BE9C(void) {
    FieldActor *actor;
    s32 window;

    actor = D_800B0078;
    if (D_800ADC00[actor->pc + 1] == 0) {
        if (func_8009CD18(&window) == 0) {
            D_800C2698[window].cleared = 0;
            D_800B0078->pc += 2;
        } else {
            D_800B0078->pc += 2;
        }
    } else {
        actor->unk82 = 0;
        actor->unk88 = 0;
        actor->unk8A = 0;
        D_800B0078->unk83 = 0;
        D_800B0078->unk84 = 0;
        D_800B0078->pc += 2;
    }
    D_800B00C0 = 1;
}

void func_8009C01C(void);

/* With an actor selected by byte 1, copy its character (+80, the portrait)
 * to the current actor and open the message as d4 does (message operand 2,
 * style byte 4; pc + 5 once open); with none, skip 6 bytes. */
void func_8009BF8C(void) {
    if (func_8009CDB4(1) != 0xFF) {
        D_800B0078->character = D_800AF880.components.descriptors[func_8009CDB4(1)].actor->character;
        func_8009C01C();
        return;
    }
    D_800B0078->pc += 6;
}

s32 func_8009C5A8(s32 index, s32 mode);

/* Open this actor's dialogue window for message operand 2 by the speaker
 * the actor selector byte 1 picks (8009c5a8 mode 0), byte 4 overriding the
 * style; retried (pc back on this opcode) until it opens (pc + 5). With no
 * such actor (an empty party slot) it skips 6 bytes. */
void func_8009C01C(void) {
    if (func_8009CDB4(1) != 0xFF) {
        s32 index = func_8009CDB4(1);

        D_800B0078->pc += 1;
        if (func_8009C5A8(index, 0) == -1) {
            D_800B0078->pc -= 1;
        }
    } else {
        D_800B0078->pc += 6;
    }
}

/* Open this actor's dialogue window for message operand 1 by the speaker
 * (the current actor; 8009c5a8 mode 0), byte 3 overriding the style; an
 * open window of its own is closed first, and it is retried (yielding)
 * until the window opens (pc + 4). */
void func_8009C0B4(void) {
    func_8009C5A8(D_800AFD1C, 0);
}

/* Open this actor's dialogue window for message operand 1 in the fixed
 * full-width box (8009c5a8 mode 1), byte 3 overriding the style; an open
 * window of its own is closed first, and it is retried (yielding) until the
 * window opens (pc + 4). */
void func_8009C0DC(void) {
    func_8009C5A8(D_800AFD1C, 1);
}

/* Open the current actor's dialogue window for message operand 1 in mode 2
 * (8009c5a8: the fixed full-width box; byte 3, when non-zero, overrides the
 * style) and continue at +4; retried, yielding, until the window opens. */
void func_8009C104(void) {
    func_8009C5A8(D_800AFD1C, 2);
}

/* Open this actor's dialogue window for message operand 1 centred (8009c5a8
 * mode 3), byte 3 overriding the style; an open window of its own is closed
 * first, and it is retried (yielding) until the window opens (pc + 4). */
void func_8009C12C(void) {
    func_8009C5A8(D_800AFD1C, 3);
}

/* Show the dialogue portrait of `character`: finish a pending slot first
 * (upload loaded images, or release shown ones) and return -1; a slot
 * already holding it is selected (bits 2-4 of the actor state) and 0
 * returned; otherwise the next free slot starts loading its image files and
 * -1 is returned. The placement (800aeae4) and file (800ae1e0) tables are
 * indexed as flat arrays, which keeps their bases in registers as the
 * original does. One `file` variable serves both images: set in two blocks
 * it is allocated globally, and the second file's address, which dies where
 * it is loaded into `file`, takes the argument register `file` prefers. */
#define PLACE(i, k) (((s16 *)D_800AEAE4)[(i) * 8 + (k)])
#define FILES(c, k) (((u8 *)D_800AE1E0)[(c) * 2 + (k)])
s32 func_8009C154(s32 character) {
    s32 i;
    s32 found;
    s32 file;

    for (i = 0; i < 3; i++) {
        if (D_800B06A4[i].b == 1) {
            if (func_80028A60(1) == 0) {
                D_800B06A4[i].b = 2;
                func_80070340(D_800ADB10, PLACE(i, 0), PLACE(i, 1), PLACE(i, 2),
                              PLACE(i, 3), 0x100, 1);
                if (D_800B06A4[i].c == 0) {
                    func_80070340(D_800ADB10, PLACE(i, 4), PLACE(i, 5), PLACE(i, 6),
                                  PLACE(i, 7), 0x100, 1);
                } else {
                    func_80070340(D_800ADB14, PLACE(i, 4), PLACE(i, 5), PLACE(i, 6),
                                  PLACE(i, 7), 0x100, 1);
                }
            }
            return -1;
        }
        if (D_800B06A4[i].b == 2) {
            D_800B06A4[i].b = 0;
            func_800320E8(D_800ADB10);
            if (D_800B06A4[i].c == 1) {
                func_800320E8(D_800ADB14);
            }
            return -1;
        }
    }
    for (i = 0; i < 3; i++) {
        if (D_800B06A4[i].a == character) {
            D_800B0078->state.bits.unk2 = i;
            return 0;
        }
    }
    found = 0;
    for (i = 0; i < 3; i++) {
        D_800ADB0C++;
        if (D_800ADB0C >= 3) {
            D_800ADB0C = 0;
        }
        if (func_8009C538(D_800B06A4[D_800ADB0C].a) == 0) {
            found++;
            break;
        }
    }
    if (found == 0) {
        return -1;
    }
    D_800B0078->state.bits.unk2 = D_800ADB0C;
    func_80028470(4, 0);
    D_800B06A4[D_800ADB0C].a = character;
    D_800B06A4[D_800ADB0C].b = 1;
    D_800B06A4[D_800ADB0C].c = 0;
    i = 0;
    file = FILES(character, 0);
    D_800B00C8[i].file = file + 0x46;
    D_800B00C8[i].destination = D_800ADB10 = func_80031BDC(func_800288EC(file + 0x46), 0);
    i++;
    if (FILES(character, 1) != FILES(character, 0)) {
        D_800B06A4[D_800ADB0C].c = 1;
        file = FILES(character, 1);
        D_800B00C8[i].file = file + 0x46;
        D_800B00C8[i].destination = D_800ADB14 = func_80031BDC(func_800288EC(file + 0x46), 0);
        i++;
    }
    D_800B00C8[i].file = 0;
    D_800B00C8[i].destination = NULL;
    func_80029AFC(D_800B00C8, 0, 0);
    return -1;
}

/* -1 when an idle window shows message kind 1 for `id`, else 0. */
s32 func_8009C538(s32 id) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800C2698[i].busy == 0 && D_800C2698[i].unk494 == 1 && D_800C2698[i].unk495 == id) {
            return -1;
        }
    }
    return 0;
}

/* The window opener (8007f8dc), as this caller passes the message id. */
s32 func_8007F8DC(s16 x, s16 y, s32 message, s32 window, s32 columns, s32 rows, s32 owner, s32 speaker,
                  s32 mode, s32 turned, s32 flags);

/* Open this actor's dialogue window for message op1 above/below speaker
 * `speaker` (mode 0 follows the speaker, mode 3 is centred, others use the
 * fixed full-width box); op3 overrides the style byte. Returns -1 while the
 * window cannot open yet (the instruction is retried) and 0 once opened.
 * The "above" placement copies y into its own variable first, which keeps
 * the load ahead of rows * 14 as the original schedules it. */
s32 func_8009C5A8(s32 speaker, s32 mode) {
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

    D_800AFC7C += 0x20;
    if (D_800ADB2C != 0 || D_800AFD04 != 0 || D_800C4268 != 0 || D_800ADB64 != 0xFF ||
        (D_800ADB70 == 0 && func_8008A558() != 0)) {
        D_800B00C0 = 1;
        return -1;
    }
    if (D_800B0078->character != 0xFF && func_8009C154(D_800B0078->character) == -1) {
        D_800B00C0 = 1;
        return -1;
    }
    D_800C4268++;
    if (func_8009CD18(&owned) == -1) {
        D_800AFC7C += 8;
        message = func_800ACDB8(1);
        if (func_80080720() != 0) {
            window = func_80080760();
            if (window != 0xFFFF) {
                D_800C2698[window].cleared = 0;
                D_800B00C0 = 1;
                return -1;
            }
        } else {
            window = func_800807B4();
        }
        for (i = 0, idle = 0, combined = 0; i < 4; i++) {
            if (D_800C2698[i].busy == 0) {
                idle++;
                combined |= (s16)D_800C2698[i].style;
            }
        }
        columns = func_8003373C(D_800ADBF0, message);
        rows = func_80033760(D_800ADBF0, message);
        if (mode == 0 || mode == 3) {
            if (D_800B0078->unk82 != 0) {
                columns = D_800B0078->unk82;
            }
            if (D_800B0078->unk83 != 0) {
                rows = D_800B0078->unk83;
            }
        }
        progress = D_800B0078->unk84;
        low = progress & 0xFFFF;
        D_800B0078->unk84 = low;
        style = low;
        if (EVENT_OPERAND_BYTE(3) != 0) {
            style = (progress & 0xFF00) | EVENT_OPERAND_BYTE(3);
            D_800B0078->unk84 = low | (style << 16);
        }
        top = 0x10;
        switch ((style >> 4) & 3) {
        case 0:
            /* Automatic: below unless the camera faces the speaker's side
             * and a free window is above. */
            if (((D_800B0078->state.bits.octant - (u16)func_8009A514()) & 7) >= 5) {
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
            D_800C2698[window].style = 1;
            if (mode == 0 || mode == 3) {
                func_8007F814(speaker, &x, &y, -0x40);
                if (mode == 0) {
                    anchor = y;
                    top = anchor - rows * 14 - 0x24;
                } else {
                    top = 0x14;
                    x = 0xA0;
                }
                if (D_800B0078->character != 0xFF && !(style & 2)) {
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
            D_800C2698[window].style = 0x81;
            if (mode == 0 || mode == 3) {
                func_8007F814(speaker, &x, &y, -0x40);
                if (mode == 0) {
                    top = y + 0x30;
                } else {
                    top = 0x94;
                    x = 0xA0;
                }
                if (D_800B0078->character != 0xFF && !(style & 2)) {
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
            if (D_800B0078->unk88 != 0) {
                left = D_800B0078->unk88;
            }
            if (D_800B0078->unk8A != 0) {
                top = D_800B0078->unk8A;
            }
            if (D_800B0078->unk82 != 0) {
                columns = D_800B0078->unk82;
            }
            if (D_800B0078->unk83 != 0) {
                rows = D_800B0078->unk83;
            }
            if (D_800B0078->character != 0xFF && !(style & 2)) {
                rows = 4;
            }
        }
        if (style & 0x40) {
            D_800C2698[window].style |= 0x40;
        }
        flags = 0;
        if (!(style & 0xC)) {
            flags = ((((((s16)D_800AF880.components.descriptors[speaker].actor->heading_goal >> 9) -
                        (u16)func_8009A514()) + 1) & 7) >= 4) << 10;
        } else if (style & 4) {
            flags = 0x400;
        }
        func_8007F8DC(left, top, message, window, columns, rows, D_800AFD1C, speaker, mode, flags, style);
        func_8009CCF8(window);
        D_800B0078->heading |= 0x8000;
        D_800B0078->pc += 4;
        return 0;
    }
    D_800B00C0 = 1;
    D_800C2698[owned].cleared = 0;
    return -1;
}

/* Set talk-inhibit bit `bit`. */
void func_8009CCF8(s32 bit) {
    D_800B2078.open_windows |= 1 << bit;
}

/* Find the idle window owned by the current actor. */
s32 func_8009CD18(s32 *window) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800C2698[i].owner == D_800AFD1C && D_800C2698[i].busy == 0) {
            *window = i;
            return 0;
        }
    }
    return -1;
}

/* Actor selector at `offset`, defaulting to the party leader. */
s32 func_8009CD7C(s32 offset) {
    s32 index;

    index = func_8009CDB4(offset);
    if (index == 0xFF) {
        return D_8005A444[0];
    }
    return index;
}

/* Actor selector at `offset`: 0xFF/0xFE/0xFD pick party slots, 0xFB the
 * current actor. */
s32 func_8009CDB4(s32 offset) {
    s32 index;

    index = D_800ADC00[D_800B0078->pc + offset];
    if (index == 0xFF) {
        index = D_8005A444[0];
    } else if (index == 0xFE) {
        index = D_8005A444[1];
    } else if (index == 0xFD) {
        index = D_8005A444[2];
    } else if (index == 0xFB) {
        index = D_800AFD1C;
    }
    return index;
}

/* Set the current actor's dialogue window overrides (modes 0 and 3 of
 * 8009c5a8; 0 keeps the computed value): left = byte 1 * 2, top = byte 2,
 * columns = byte 3 * 3, rows = byte 4. */
void func_8009CE48(void) {
    D_800B0078->unk88 = EVENT_OPERAND_BYTE(1) * 2;
    D_800B0078->unk8A = EVENT_OPERAND_BYTE(2);
    D_800B0078->unk82 = EVENT_OPERAND_BYTE(3) * 3;
    D_800B0078->unk83 = EVENT_OPERAND_BYTE(4);
    D_800B0078->pc += 5;
}

/* Set the current actor's dialogue window overrides as cf from operands:
 * left = operand 1, top = operand 3, columns = operand 5 * 3, rows =
 * operand 7, and its window style word (+84) = operand 9. */
void func_8009CEE0(void) {
    D_800B0078->unk88 = func_800ACDEC(1);
    D_800B0078->unk8A = func_800ACDEC(3);
    D_800B0078->unk82 = func_800ACDEC(5) * 3;
    D_800B0078->unk83 = func_800ACDEC(7);
    D_800B0078->unk84 = func_800ACDEC(9);
    D_800B0078->pc += 11;
}

/* Event d1: empty. It neither advances nor yields, so the interpreter runs
 * it again until the pass's batch limit (or the 0x400 loop error); the
 * script stays on it. */
void func_8009CF70(void) {
}

/* Selected operand, immediate when flags bit 0x80 is set. */
s32 func_8009CF78(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x80) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x40 is set. */
s32 func_8009CFBC(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x40) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x20 is set. */
s32 func_8009D000(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x20) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x10 is set. */
s32 func_8009D044(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x10) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x08 is set. */
s32 func_8009D088(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x08) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x04 is set. */
s32 func_8009D0CC(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x04) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x02 is set. */
s32 func_8009D110(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x02) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}

/* Selected operand, immediate when flags bit 0x01 is set. */
s32 func_8009D154(s32 offset, s32 flags) {
    s32 value;

    if (flags & 0x01) {
        value = (s16)func_800ACD7C(offset);
    } else {
        value = func_800A3018(func_800ACDB8(offset) & 0xFFFF);
    }
    return value;
}


/* Store a random number (rand) in variable operand 1. */
void func_8009D198(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, rand());
    D_800B0078->pc += 3;
}

/* Store a random number 0..operand 3 ((rand() * (operand 3 + 1)) >> 15) in
 * variable operand 1. */
void func_8009D1F0(void) {
    s32 value;

    value = (rand() * (func_800ACDEC(3) + 1)) >> 15;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 5;
}

/* Shift variable operand 1 right by operand 3. */
void func_8009D260(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value = value >> func_800ACDEC(3);
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 5;
}

/* Shift variable operand 1 left by operand 3. */
void func_8009D2D0(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value = value << func_800ACDEC(3);
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 5;
}

/* Increment variable operand 1. */
void func_8009D340(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF) + 1;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 3;
}

/* Decrement variable operand 1. */
void func_8009D3A4(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF) - 1;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 3;
}

/* Clear bit (selected operand 3, immediate when byte 5 has bit 0x40) of
 * variable operand 1. */
void func_8009D408(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value &= ~(1 << func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* XOR variable operand 1 with selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void func_8009D4A0(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value ^= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* OR variable operand 1 with selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void func_8009D52C(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value |= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* AND variable operand 1 with selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void func_8009D5B8(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value &= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Set bit (selected operand 3, immediate when byte 5 has bit 0x40) of variable
 * operand 1. */
void func_8009D644(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value |= 1 << func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Multiply a variable by a selected operand. */
void func_8009D6D8(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value *= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Divide a variable by a selected operand (zero treated as one). */
void func_8009D768(void) {
    s32 value;
    s32 divisor;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    divisor = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    if (divisor == 0) {
        divisor = 1;
    }
    value /= divisor;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Subtract selected operand 3 (immediate when byte 5 has bit 0x40) from
 * variable operand 1. */
void func_8009D804(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value -= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Add selected operand 3 (immediate when byte 5 has bit 0x40) to variable
 * operand 1. */
void func_8009D890(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value += func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Set variable operand 1 to zero. */
void func_8009D91C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, 0);
    D_800B0078->pc += 3;
}

/* Set variable operand 1 to one. */
void func_8009D960(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, 1);
    D_800B0078->pc += 3;
}

/* Set variable operand 1 to selected operand 3 (immediate when byte 5 has bit
 * 0x40). */
void func_8009D9A4(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    D_800B0078->pc += 6;
}

/* Set the current actor's flag 0x20000, which keeps the player's talk and touch
 * triggers (8008399c) from starting its events 2 and 3. */
void func_8009DA1C(void) {
    FieldActor *actor = D_800B0078;

    actor->flags |= 0x20000;
    actor->pc++;
}

/* Clear the current actor's flag 0x20000 (see 2a): talk and touch can start its
 * events again. */
void func_8009DA44(void) {
    FieldActor *actor = D_800B0078;

    actor->flags &= ~0x20000;
    actor->pc++;
}

/* Set the current actor's flag 0x800000, which keeps the player's touch
 * trigger (8008399c) from starting its event 3. */
void func_8009DA70(void) {
    FieldActor *actor = D_800B0078;

    actor->flags |= 0x800000;
    actor->pc++;
}

/* Clear the current actor's flag 0x800000 (see cd). */
void func_8009DA98(void) {
    FieldActor *actor = D_800B0078;

    actor->flags &= ~0x800000;
    actor->pc++;
}

/* Hide the actor of selector byte 1 (flag 1), remove it from the event schedule
 * (layer flag 0x100000), set its descriptor flag 0x20 and release the current
 * actor's idle dialogue window. */
void func_8009DAC4(void) {
    FieldActor *actor;
    FieldDescriptor *descriptor;
    s32 window;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->flags |= 1;
        actor->layer_flags |= 0x100000;
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        descriptor->flags |= 0x20;
        if (func_8009CD18(&window) == 0) {
            D_800C2698[window].cleared = 0;
        }
    }
    D_800B0078->pc += 2;
}

/* Show the actor of selector byte 1 again: clear its flag 1 (see 27). */
void func_8009DBC8(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->flags &= ~1;
    }
    D_800B0078->pc += 2;
}

/* Stop the actor of selector byte 1 (clear its +30/+40 motion, mark its heading
 * turned) and hide it (flag 1: not drawn, its scripts not run), then release
 * the current actor's idle dialogue window. */
void func_8009DC4C(void) {
    FieldActor *actor;
    u16 state;
    s32 window;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
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
        if (func_8009CD18(&window) == 0) {
            D_800C2698[window].cleared = 0;
        }
    }
    D_800B0078->pc += 2;
}

/* Wait operand-1 frames (counted in the slot), yielding each frame. */
void func_8009DD34(void) {
    if (D_800B0078->slots[D_800B0078->slot].countdown == 0) {
        D_800B0078->slots[D_800B0078->slot].countdown = func_800ACDEC(1);
    } else {
        D_800B0078->slots[D_800B0078->slot].countdown--;
    }
    if (D_800B0078->slots[D_800B0078->slot].countdown == 0) {
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

/* Clear the descriptor flag 0x20 and layer flag 0x2000000 of the actor of
 * selector byte 1, unless it is removed from the schedule (layer flag
 * 0x100000). */
void func_8009DDEC(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        if (!(descriptor->actor->layer_flags & 0x100000)) {
            descriptor->flags &= 0xFFDF;
            descriptor->actor->layer_flags &= ~0x2000000;
        }
    }
    D_800B0078->pc += 2;
}

/* Set the descriptor flag 0x20 of the actor of selector byte 1 (disabling it).
 */
void func_8009DE94(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        descriptor->flags |= 0x20;
    }
    D_800B0078->pc += 2;
}

/* Clear the current descriptor's flag 0x20 (re-enabling it), reset the actor's
 * current animation (+e8 = 0xff) and clear its layer flag 0x2000000. */
void func_8009DF10(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags &= 0xFFDF;
    actor = D_800B0078;
    actor->unkE8 = 0xFF;
    actor->layer_flags &= ~0x2000000;
    actor->pc++;
}

/* Set layer flags 0x2000000 and 0x800 on a selected actor. */
void func_8009DF78(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        descriptor->actor->layer_flags |= 0x2000000;
        descriptor->actor->layer_flags |= 0x800;
    }
    D_800B0078->pc += 2;
}

/* Set layer flags 0x2000000 and 0x800 on the current actor. */
void func_8009E014(void) {
    FieldActor *actor = D_800B0078;

    actor->layer_flags |= 0x2000800;
    actor->pc++;
}

/* Disable the current descriptor (flag 0x20). */
void func_8009E040(void) {
    FieldDescriptor *descriptor;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags |= 0x20;
    D_800B0078->pc += 1;
}

void func_80021BCC(Sprite *model, u16 value);

/* Set the current actor's motion divisor (+76: walks and moves advance
 * 0x4000000 / it per step, 16.16) from operand 1 and pass it to its sprite as
 * the gravity divisor (80021bcc). */
void func_8009E094(void) {
    s16 value;

    value = func_800ACDEC(1);
    D_800B0078->unk76 = value;
    func_80021BCC(D_800AF880.components.descriptors[D_800AFD1C].model, value);
    D_800B0078->pc += 3;
}

/* Map operand bits onto actor flags (1->0x80, 4->0x20, 8->0x10, 0x10->8,
 * 0x20->4, 0x40->0x8000000). */
void func_8009E10C(void) {
    s32 bits;
    s32 flags;

    bits = func_800ACDEC(1);
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
    D_800B0078->flags = (D_800B0078->flags & 0xF7FFFF43) | flags;
    D_800B0078->pc += 3;
}

#include "field_actor_events.h"

/* Set the actor layer bits 0-2 from operand bits 0-2 and 3-5 from operand bits 4-6. */
void func_8009E1A0(void) {
    FieldActor *actor = D_800B0078;
    u8 *code = D_800ADC00;
    s32 bits;

    bits = code[actor->pc + 1] & 7;
    actor->layer_flags = (actor->layer_flags & ~7) | bits;
    bits = (code[actor->pc + 1] >> 1) & 0x38;
    actor->layer_flags = (actor->layer_flags & ~0x38) | bits;
    actor->pc += 2;
}

/* Enter mode 0x400000 (clearing 0x40000) from the current height. */
void func_8009E208(void) {
    FieldActor *actor = D_800B0078;
    s32 y;

    y = WHOLE(actor->position[1]);
    actor->unkEC = 0;
    actor->flags = (actor->flags & ~0x40000) | 0x400000;
    actor->pc++;
    actor->unk72 = y;
}

void func_8009E574(s32 x, s32 z);
void func_8009E810(s32 y);

/* Place the current actor at x/z signed operands 1/3 on its layer's floor
 * (8009e574, which clears its motion), then set its height to signed operand 5
 * (8009e810) and its flag 0x40000. */
void func_8009E248(void) {
    s32 a;

    a = (s16)func_800ACD7C(1);
    func_8009E574(a, (s16)func_800ACD7C(3));
    func_8009E810((s16)func_800ACD7C(5));
    D_800B0078->flags |= 0x40000;
    D_800B0078->pc += 7;
}

/* Set the current actor's height to selected operand 1 (flags byte 3, bit 0x80;
 * 8009e810 also sets +ec and +72) and set its flag 0x40000. */
void func_8009E2C8(void) {
    func_8009E810(func_8009CF78(1, EVENT_OPERAND_BYTE(3)));
    D_800B0078->flags |= 0x40000;
    D_800B0078->pc += 4;
}

/* Signed halfword of the bytecode at `offset`. */
s16 func_8009E330(s32 offset) {
    return D_800ADC00[offset] + (D_800ADC00[offset + 1] << 8);
}

/* Place the current actor on collision layer byte 5 at selected x/z operands
 * 1/3 (flags byte 6) on that layer's floor (8009e574, which clears its motion)
 * and clear its layer flag 0x200000 and flag 0x10000. */
void func_8009E35C(void) {
    s32 x;

    D_800B0078->layer = EVENT_OPERAND_BYTE(5);
    x = func_8009CF78(1, EVENT_OPERAND_BYTE(6));
    func_8009E574(x, func_8009CFBC(3, EVENT_OPERAND_BYTE(6)));
    D_800B0078->layer_flags &= ~0x200000;
    D_800B0078->flags &= ~0x10000;
    D_800B0078->pc += 7;
}

/* Move the current actor to layer operand 1 at its own x/z. */
void func_8009E428(void) {
    FieldActor *actor;

    D_800B0078->layer = EVENT_OPERAND_BYTE(1);
    actor = D_800AF880.components.descriptors[D_800AFD1C].actor;
    func_8009E574(WHOLE(actor->position[0]), WHOLE(actor->position[2]));
    D_800B0078->pc += 2;
}

/* Place the current actor at selected x/z operands 1/3 (flags byte 5) on the
 * floor of its collision layer (8009e574, which clears its motion) and clear
 * its layer flag 0x200000 and flag 0x10000. */
void func_8009E4BC(void) {
    s32 x;

    x = func_8009CF78(1, EVENT_OPERAND_BYTE(5));
    func_8009E574(x, func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    D_800B0078->layer_flags &= ~0x200000;
    D_800B0078->flags &= ~0x10000;
    D_800B0078->pc += 6;
}

/* Place the current actor at integer (x, z) on the floor of its layer:
 * locate the floor triangle of every layer, take its terrain, normal and
 * height, move the descriptor and model there and clear the motion. */
void func_8009E574(s32 x, s32 z) {
    VECTOR normals[4];
    SVECTOR points[4];
    Sprite *model;
    s32 layer;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    for (layer = 0; layer < D_800AF880.components.layer_count - 1; layer++) {
        D_800B0078->triangle[layer] = func_8007B1C4(x, z, layer, &points[layer], &normals[layer]);
    }
    D_800B0078->unk014 = func_80080968(D_800B0078);
    D_800B0078->unk50[0] = (normals + D_800B0078->layer)->vx;
    D_800B0078->unk50[1] = (normals + D_800B0078->layer)->vy;
    D_800B0078->unk50[2] = (normals + D_800B0078->layer)->vz;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[0] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = x;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[1] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = points[D_800B0078->layer].vy;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[2] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = z;
    model->ground = points[D_800B0078->layer].vy;
    D_800B0078->position[0] = x << 16;
    D_800B0078->position[1] = points[D_800B0078->layer].vy << 16;
    D_800B0078->position[2] = z << 16;
    D_800B0078->unk72 = points[D_800B0078->layer].vy;
    model->x = D_800B0078->position[0];
    model->y = D_800B0078->position[1];
    model->z = D_800B0078->position[2];
    D_800B0078->unk40[0] = 0;
    D_800B0078->unk40[1] = 0;
    D_800B0078->unk40[2] = 0;
    D_800B0078->unk030[0] = 0;
    D_800B0078->unk030[1] = 0;
    D_800B0078->unk030[2] = 0;
    D_800B0078->target[0] = 0;
    D_800B0078->target[1] = 0;
    D_800B0078->target[2] = 0;
    D_800B0078->unk62 = 0;
    D_800B0078->unk60 = 0;
    D_800B0078->unk64 = 0;
    model->speed_x = 0;
    model->speed_y = 0;
    model->speed_z = 0;
    D_800B0078->unkF0 = 0;
    D_800B0078->unkEC = 0;
    D_800B0078->unk72 = D_800B0078->position[1] >> 16;
    D_800B0078->flags = (D_800B0078->flags & ~0x40000) | 0x400000;
}

/* Set the current actor's height `y` (whole units). Defined K&R: callers
 * pass the operand unconverted and only its low halfword is used. */
void func_8009E810(y)
    s16 y;
{
    SVECTOR unused[3]; /* unused in the original; reserves 0x18 bytes */

    D_800B0078->position[1] = y << 16;
    D_800B0078->unkEC = y;
    D_800B0078->unk72 = y;
}

/* Set the current actor's dimensions from bytes 1-4, each doubled and only when
 * non-zero: +18, +1c, +1a (height) and +1e (the radius the talk, touch and move
 * reach checks add). */
void func_8009E83C(void) {
    if (EVENT_OPERAND_BYTE(1) != 0) {
        D_800B0078->unk18 = EVENT_OPERAND_BYTE(1) * 2;
    }
    if (EVENT_OPERAND_BYTE(2) != 0) {
        D_800B0078->gravity.s.fraction = EVENT_OPERAND_BYTE(2) * 2;
    }
    if (EVENT_OPERAND_BYTE(3) != 0) {
        D_800B0078->height = EVENT_OPERAND_BYTE(3) * 2;
    }
    if (EVENT_OPERAND_BYTE(4) != 0) {
        D_800B0078->gravity.s.whole = EVENT_OPERAND_BYTE(4) * 2;
    }
    D_800B0078->pc += 5;
}

/* Give the current actor a boundary quadrilateral (+114, allocated once and
 * marked by state bit 12): corner x/z pairs from selected operands 1-15 (flags
 * byte 17, bits 0x80 down to 0x01). */
void func_8009E91C(void) {
    if (!(D_800B0078->state.word & 0x1000)) {
        D_800B0078->unk114 = func_80031BDC(0x10, 0);
    }
    D_800B0078->state.word |= 0x1000;
    ((ActorBoundary *)D_800B0078->unk114)->corners[0].x = func_8009CF78(1, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[0].z = func_8009CFBC(3, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[1].x = func_8009D000(5, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[1].z = func_8009D044(7, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[2].x = func_8009D088(9, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[2].z = func_8009D0CC(11, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[3].x = func_8009D110(13, EVENT_OPERAND_BYTE(17));
    ((ActorBoundary *)D_800B0078->unk114)->corners[3].z = func_8009D154(15, EVENT_OPERAND_BYTE(17));
    D_800B0078->pc += 18;
}

/* -1 when one of the actor's slots carries event tag `tag`, else 0. */
s32 func_8009EB48(FieldActor *actor, s32 tag) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (tag == actor->slots[i].tag) {
            return -1;
        }
    }
    return 0;
}

/* Request event (low five bits of byte 2) at priority (its high three bits) on
 * the actor of selector byte 1, in its first free script slot (priority 15, not
 * linked), and continue at +3. Retried without yielding while no slot is free;
 * skipped when one of its slots already carries that event or no actor is
 * selected. A target removed from the schedule (layer flag 0x100000) instead
 * drops the request link (this slot's bits 16-17, the target's slot +cf bit
 * 22). */
void func_8009EB78(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (func_8009CDB4(1) != 0xFF) {
        index = func_8009CDB4(1);
        other = D_800AF880.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            D_800B0078->slots[D_800B0078->slot].unk16 = 0;
            other->slots[D_800B0078->unk0CF].unk22 = 0;
        } else if (func_8009EB48(other, EVENT_OPERAND_BYTE(2) & 0x1F) != -1) {
            for (i = 0; i < 8; i++) {
                if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                    other->slots[i].resume_pc = func_800A3090(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                    other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                    other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                    goto done;
                }
            }
            return;
        }
    }
done:
    D_800B0078->pc += 3;
}

/* Request event byte 2 on the actor of selector byte 1 as 07 and wait until it
 * has started: phase 0 (this slot's bits 16-17) requests it, linking the target
 * slot through +cf (retried without yielding while no slot is free), and phase
 * 1 yields until the target runs that slot or it has ended, then continues at
 * +3. Skipped when the event is already requested or no actor is selected; a
 * target removed from the schedule (layer flag 0x100000) drops the link and
 * continues. */
void func_8009ED68(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (func_8009CDB4(1) != 0xFF) {
        index = func_8009CDB4(1);
        other = D_800AF880.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            D_800B0078->slots[D_800B0078->slot].unk16 = 0;
            other->slots[D_800B0078->unk0CF].unk22 = 0;
        } else {
            switch (D_800B0078->slots[D_800B0078->slot].unk16) {
            case 0:
                if (func_8009EB48(other, EVENT_OPERAND_BYTE(2) & 0x1F) == -1) {
                    break;
                }
                for (i = 0; i < 8; i++) {
                    if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                        other->slots[i].resume_pc = func_800A3090(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                        other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                        other->slots[D_800B0078->unk0CF].unk22 = 1;
                        other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                        D_800B0078->unk0CF = i;
                        D_800B0078->slots[D_800B0078->slot].unk16 = 1;
                        return;
                    }
                }
                return;
            case 1:
                if (other->slot == D_800B0078->unk0CF || other->slots[D_800B0078->unk0CF].priority == 0xF) {
                    D_800B0078->pc += 3;
                    D_800B0078->slots[D_800B0078->slot].unk16 = 0;
                    other->slots[D_800B0078->unk0CF].unk22 = 0;
                    return;
                }
                D_800B00C0 = 1;
                return;
            default:
                return;
            }
        }
    }
    D_800B0078->pc += 3;
}

/* Request event byte 2 on the actor of selector byte 1 as 08 and wait until it
 * has finished: phase 1 yields until it has started, phase 2 until the linked
 * slot has ended (priority 15), then continues at +3. Skipped as 08 (event
 * already requested, no actor, or a target removed from the schedule, which
 * drops the link). */
void func_8009F0A0(void) {
    s32 index;
    FieldActor *other;
    s32 i;

    if (func_8009CDB4(1) != 0xFF) {
        index = func_8009CDB4(1);
        other = D_800AF880.components.descriptors[index].actor;
        if (other->layer_flags & 0x100000) {
            D_800B0078->slots[D_800B0078->slot].unk16 = 0;
            other->slots[D_800B0078->unk0CF].unk22 = 0;
        } else {
            switch (D_800B0078->slots[D_800B0078->slot].unk16) {
            case 0:
                if (func_8009EB48(other, EVENT_OPERAND_BYTE(2) & 0x1F) == -1) {
                    break;
                }
                for (i = 0; i < 8; i++) {
                    if (other->slots[i].priority == 0xF && other->slots[i].unk22 == 0) {
                        other->slots[i].resume_pc = func_800A3090(index, EVENT_OPERAND_BYTE(2) & 0x1F);
                        other->slots[i].priority = EVENT_OPERAND_BYTE(2) >> 5;
                        other->slots[D_800B0078->unk0CF].unk22 = 1;
                        D_800B0078->unk0CF = i;
                        D_800B0078->slots[D_800B0078->slot].unk16 = 1;
                        other->slots[i].tag = EVENT_OPERAND_BYTE(2) & 0x1F;
                        return;
                    }
                }
                return;
            case 1:
                if (other->slot == D_800B0078->unk0CF || other->slots[D_800B0078->unk0CF].priority == 0xF) {
                    D_800B0078->slots[D_800B0078->slot].unk16 = 2;
                    return;
                }
                D_800B00C0 = 1;
                return;
            case 2:
                if (other->slots[D_800B0078->unk0CF].priority == 0xF) {
                    D_800B0078->slots[D_800B0078->slot].unk16 = 0;
                    other->slots[D_800B0078->unk0CF].unk22 = 0;
                    D_800B0078->pc += 3;
                    return;
                }
                D_800B00C0 = 1;
                return;
            default:
                return;
            }
        }
    }
    D_800B0078->pc += 3;
}

/* Event fe 01: wander for one frame: every 16th run (counted in +102) point the
 * heading an octant (0x200) left or right of the facing goal at random,
 * otherwise along the goal; yields and advances. */
void func_8009F424(void) {
    s32 facing;

    facing = D_800B0078->heading_goal;
    if ((++D_800B0078->unk102 & 0xF) == 0) {
        if (!(rand() & 1)) {
            facing = (D_800B0078->heading_goal + 0x200) & 0xFFF;
        } else {
            facing = (D_800B0078->heading_goal - 0x200) & 0xFFF;
        }
    }
    D_800B00C0 = 1;
    D_800B0078->heading = facing;
    D_800B0078->pc += 1;
}

/* Wander with pauses: every 16th pass (+102 counts) either hold the facing
 * (rand & 0x30: mark the heading goal turned) or turn one octant either way at
 * random; set the heading, yield and continue at +1. */
void func_8009F4CC(void) {
    s32 facing;
    s32 random;

    facing = D_800B0078->heading_goal;
    if ((++D_800B0078->unk102 & 0xF) == 0) {
        random = rand();
        if (random & 0x30) {
            facing = D_800B0078->heading_goal |= 0x8000;
        } else if (!(random & 1)) {
            facing = (D_800B0078->heading_goal + 0x200) & 0xFFF;
        } else {
            facing = (D_800B0078->heading_goal - 0x200) & 0xFFF;
        }
    }
    D_800B00C0 = 1;
    D_800B0078->heading = facing;
    D_800B0078->pc += 1;
}

void func_8009F5F4(void);

/* Run player control for this frame and repeat this opcode. */
void func_8009F5A8(void) {
    u16 pc;

    pc = D_800B0078->pc;
    func_8009F5F4();
    D_800B00C0 = 1;
    D_800B0078->pc = pc;
}

/* Event a7: player control. With dialogue closed and encounters allowed,
 * poll the pad, count frames stuck against terrain, start a jump (0x800)
 * on the jump button or after 32 stuck frames, and face the d-pad
 * direction relative to the camera (0x8000 when none). Non-player actors
 * are marked 0x1000000 instead. */
void func_8009F5F4(void) {
    u8 unused[0x48]; /* the original frame holds 0x48 unused bytes */
    s32 i;
    s32 idle;
    s32 direction = 0;

    if (D_800B0078->flags & 0x4000) {
        for (i = 0; i < 4; i++) {
            if (D_800C2698[i].choice.status == 0) {
                break;
            }
        }
        idle = (i == 4) ? -1 : 0;
        if (idle == -1 && D_800B2078.encounter_inhibition == 0) {
            if (D_800AFE9C >> 12) {
                func_80079288();
            }
            D_800ADB68 = 1;
            if (D_800B0078->unk014 & 0x400000) {
                if (ACTOR_CACHED_POSITION(D_800B0078)[0] == WHOLE(D_800B0078->position[0])
                    && ACTOR_CACHED_POSITION(D_800B0078)[1] == WHOLE(D_800B0078->position[1])
                    && ACTOR_CACHED_POSITION(D_800B0078)[2] == WHOLE(D_800B0078->position[2])) {
                    D_800ADB02++;
                }
            } else {
                D_800ADB02 = 0;
            }
            if (D_800ADB02 > 32 && (D_800ADB02 = 32, D_800AFE9C & 0x80) && !(D_800B0078->flags & 0x1800) && D_800ADB64 == 0xFF) {
                goto jump;
            }
            if (D_800B2078.jump_mode == 0) {
                if ((D_800C2694 & 0x80) && !(D_800B0078->flags & 0x1800) && !(D_800B0078->unk014 & 0x400000) && D_800ADB64 == 0xFF) {
                jump:
                    if (func_80081F5C(D_800B0078) == 0) {
                        D_800B0078->flags |= 0x800;
                        D_800ADB28 = D_800B2360[0];
                    }
                }
            } else {
                if (D_800C2694 & 0x80) {
                    if (D_800B2078.repeat_remaining != 0) {
                        goto count;
                    }
                    if (D_800ADB64 == 0xFF && func_80081F5C(D_800B0078) == 0) {
                        D_800B0078->flags |= 0x800;
                        D_800ADB28 = D_800B2360[0];
                        D_800B0078->unkE8 = 0xFF;
                        D_800B2078.repeat_remaining = D_800B2078.repeat_delay;
                    }
                }
                if (D_800B2078.repeat_remaining != 0) {
                count:
                    D_800B2078.repeat_remaining--;
                }
            }
            if (D_800B2078.unk2354 == 0) {
                direction = D_800ADF68[(D_800AFE9C >> 12) ^ 0xF];
            } else {
                direction = D_800ADF88[(D_800AFE9C >> 12) ^ 0xF];
            }
            if (!(direction & 0x8000)) {
                direction = (direction - D_800AF880.angle) & 0xFFF;
            }
            D_800B0078->heading = direction;
        } else {
            D_800B0078->heading = direction | 0x8000;
        }
    } else if (D_800B2078.preserve_nonplayer_motion == 0) {
        D_800B0078->flags |= 0x1000000;
    }
    D_800B0078->pc += 1;
}

/* Party slot of character `id`, or -1. */
s32 func_8009FA00(s32 id) {
    s32 i;

    if (id == 0xFF) {
        return -1;
    }
    for (i = 0; i < 3; i++) {
        if (D_80062590[i] == 0xFF) {
            return -1;
        }
        if (D_80062590[i] == id) {
            return i;
        }
    }
    return -1;
}

s16 func_8009E330(s32 offset);


/* Place the current actor at entry point `entry` of the bytecode's entry
 * table (when present): layer, x/z, camera octant and facing (0xFF: from
 * variables 8 and 6). One angle variable holds the camera heading and then
 * the facing; because the facing sets bit 15, the heading's sign extension
 * for the 32-bit copy stays in the code. */
s32 func_8009FA54(s32 entry) {
    s32 marker;
    s32 record;
    s32 x;
    s32 angle;

    marker = D_800ADC00[0];
    if (marker != 0xFF) {
        return 0;
    }
    record = entry * 7;
    D_800B0078->layer = D_800ADC00[record + 5];
    x = func_8009E330(record + 1);
    func_8009E574(x, func_8009E330(record + 3));
    angle = ((D_800ADC00[record + 6] + 4) & 7) << 9;
    if (D_800ADC00[record + 6] == marker) {
        angle = ((func_800A3018(8) + 4) & 7) << 9;
    }
    D_800AF880.heading_angles.vy = angle;
    D_800AF880.heading = (s16)angle;
    D_800AF880.heading_high = angle << 16;
    angle = (((D_800ADC00[record + 7] - 2) & 7) << 9) | 0x8000;
    if (D_800ADC00[record + 7] == marker) {
        angle = (((func_800A3018(6) - 2) & 7) << 9) | 0x8000;
    }
    D_800B0078->unk108 = D_800B0078->heading_goal = D_800B0078->heading = angle;
    return 0;
}


/* Event fe 1e: switch the party files to gears: set 0xc000 in the field id
 * (8004f34c), wait for the disc and its pending read (8001ad1c), make the party
 * files match (8001b044: the gear files with 0xc000 set) and unpack them into
 * the party sprite blocks (8001b3a8); record byte 1 as the alternate sprite set
 * (800b2268) that opcode 16 uses. */
void func_8009FB98(void) {
    D_8004F34C |= 0xC000;
    func_8001AD1C();
    func_8001B044();
    func_8001B3A8();
    D_800B2078.unk2268 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}


/* Party slot whose field actor is `index`, or 0xFF. */
s32 func_8009FC10(s32 index) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (D_8006F990[i] == index) {
            return i;
        }
    }
    return 0xFF;
}

void func_8009FD10(s32 slot);

/* Event fe 41: set the stand-in flag (+22b1) of party slot operand 1 (clamped
 * to 2), then record the field id in the slot's variable triple (2a/30/36) and
 * clear its other two variables (8009fd10). */
void func_8009FC48(void) {
    s32 slot;

    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    D_8005A39C->inGear[slot] = 1;
    func_8009FD10(slot);
    D_800B0078->pc += 3;
}

/* Event fe 42: clear the stand-in flag (+22b1) of party slot operand 1 (clamped
 * to 2), then record the field id in the slot's variable triple (2a/30/36) and
 * clear its other two variables (8009fd10). */
void func_8009FCAC(void) {
    s32 slot;

    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    D_8005A39C->inGear[slot] = 0;
    func_8009FD10(slot);
    D_800B0078->pc += 3;
}

/* Record the field id in the slot's variable triple and clear the rest. */
void func_8009FD10(s32 slot) {
    switch (slot) {
    case 0:
        func_800A3074(0x2A, D_8004F34C & 0xFFF);
        func_800A3074(0x2C, 0);
        func_800A3074(0x2E, 0);
        break;
    case 1:
        func_800A3074(0x30, D_8004F34C & 0xFFF);
        func_800A3074(0x32, 0);
        func_800A3074(0x34, 0);
        break;
    case 2:
        func_800A3074(0x36, D_8004F34C & 0xFFF);
        func_800A3074(0x38, 0);
        func_800A3074(0x3A, 0);
        break;
    }
}

void func_800AD4D4(s32 slot);

/* Event fe 1f: when the current actor is a party slot's field actor (8009fc10:
 * 8006f990) and that slot's stand-in flag (+22b1) is clear, stand it in for the
 * slot's member (800ad4d4: swap their models, set the flag, restart both
 * animations). */
void func_8009FDD4(void) {
    s32 slot;

    slot = func_8009FC10(D_800AFD1C);
    if (slot != 0xFF && D_8005A39C->inGear[slot] == 0) {
        func_800AD4D4(slot);
    }
    D_800B0078->pc += 1;
}

void func_800ACFD0(s32 slot);

/* Event fe 20: when party slot byte 1 is occupied (80062590) and its stand-in
 * flag (+22b1) is set, return the slot to its member (800acfd0: swap the models
 * back, hand the heading over, restart both animations, clear the flag). */
void func_8009FE4C(void) {
    u8 slot;

    slot = EVENT_OPERAND_BYTE(1);
    if (D_80062590[slot] != 0xFF && D_8005A39C->inGear[slot] != 0) {
        func_800ACFD0(slot);
    }
    D_800B0078->pc += 2;
}

/* Record party slot `slot`'s map (its layer in bits 14 up) and integer x/z
 * in event variables 2a/2c/2e, 30/32/34 or 36/38/3a. Declared int without
 * a return value: the original keeps $v0 live on exit. */
s32 func_8009FEE4(s32 slot) {
    s32 layer;

    if (D_8005A444[slot] != 0xFF) {
        layer = D_800AF880.components.descriptors[D_8005A444[slot]].actor->layer << 14;
        switch (slot) {
        case 0:
            func_800A3074(0x2A, (D_8004F34C & 0xFFF) | layer);
            func_800A3074(0x2C, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[0]));
            func_800A3074(0x2E, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[2]));
            break;
        case 1:
            func_800A3074(0x30, (D_8004F34C & 0xFFF) | layer);
            func_800A3074(0x32, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[0]));
            func_800A3074(0x34, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[2]));
            break;
        case 2:
            func_800A3074(0x36, (D_8004F34C & 0xFFF) | layer);
            func_800A3074(0x38, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[0]));
            func_800A3074(0x3A, WHOLE(D_800AF880.components.descriptors[D_8005A444[slot]].actor->position[2]));
            break;
        }
    }
}

/* Read party slot `slot`'s variable triple (see func_8009FD10). */
void func_800A0158(s32 slot, s32 *a, s32 *b, s32 *c) {
    switch (slot) {
    case 0:
        *a = func_800A3018(0x2A);
        *b = func_800A3018(0x2C);
        *c = func_800A3018(0x2E);
        break;
    case 1:
        *a = func_800A3018(0x30);
        *b = func_800A3018(0x32);
        *c = func_800A3018(0x34);
        break;
    case 2:
        *a = func_800A3018(0x36);
        *b = func_800A3018(0x38);
        *c = func_800A3018(0x3A);
        break;
    }
}

/* Event 5c: the current actor becomes party slot operand 1 (at most 2):
 * take its member's sprite and shown it at the slot's recorded map
 * position (variables 2a..3a) when that is this map; with no member, show
 * the field's first sprite instead. Members away from this map stay hidden
 * (descriptor flag 0x20). */
void func_800A0228(void) {
    FieldDescriptor *descriptor;
    s32 slot;
    s32 shown;
    s32 map;
    s32 x;
    s32 z;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    shown = 1;
    D_8006F990[slot] = D_800AFD1C;
    if (D_80062590[slot] != 0xFF && func_8001ACF0(D_80062590[slot]) != 0xFF) {
        func_800A0158(slot, &map, &x, &z);
        D_800B0078->layer = (map >> 14) & 3;
        if ((D_8004F34C & 0xFFF) != (map & 0x3FFF)) {
            shown = 0;
            x = 0;
            z = 0;
            D_800B0078->layer = 0;
        }
        if (D_8005A39C->inGear[slot] != 0) {
            shown = 0;
        } else if (D_80062590[slot] == 7) {
            shown = 0;
        }
        descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
        func_80076AC0(D_800AFD1C, slot, D_8005A414[slot], 1, 0, slot, 1);
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
        if ((D_8004F34C & 0xFFF) != (map & 0x3FFF)) {
            D_800B0078->layer = 0;
        }
        func_8009E574(x, z);
        func_800A0C94();
        D_800B0078->flags = (D_800B0078->flags | 0x400) & ~0x300;
        if (shown == 0) {
            D_800AF880.components.descriptors[D_800AFD1C].flags |= 0x20;
        }
    } else {
        func_800A0D3C();
        D_800B0078->pc += 2;
        D_800B0078->layer_flags |= 0x800;
        return;
    }
    if (D_800AF880.components.layer_count - 1 < D_800B0078->layer) {
        D_800B0078->layer = 0;
    }
    D_800B0078->flags |= 0x20000;
    D_800B0078->layer_flags |= 0xC00;
    D_800B0078->pc += 3;
}

/* Copy actor `from`'s collision state, height, +50 words, position and
 * matrix to actor `to` and move `to`'s model to it. */
void func_800A0524(s32 to, s32 from) {
    FieldActor *target;
    FieldActor *source;
    s32 i;

    target = D_800AF880.components.descriptors[to].actor;
    source = D_800AF880.components.descriptors[from].actor;
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
    func_8007409C(&D_800AF880.components.descriptors[to].matrix, &D_800AF880.components.descriptors[from].matrix);
    func_80074078(&D_800AF880.components.descriptors[to].matrix, &D_800AF880.components.descriptors[from].matrix);
    D_800AF880.components.descriptors[to].model->x = D_800AF880.components.descriptors[from].actor->position[0];
    D_800AF880.components.descriptors[to].model->y = D_800AF880.components.descriptors[from].actor->position[1];
    D_800AF880.components.descriptors[to].model->z = D_800AF880.components.descriptors[from].actor->position[2];
}

extern s32 D_800AFFEC;
s32 func_8009FA00(s32 character);
extern s16 D_800AFD20;

/* Give the current actor the sprite of party member operand 1, or hide it
 * (flag 1, layer flag 0x100000) and end its script when absent. */
void func_800A06E8(void) {
    FieldDescriptor *descriptor;
    s32 slot;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    slot = func_8009FA00(func_8008CF3C(func_800ACDEC(1)));
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    if (slot != -1) {
        func_80076AC0(D_800AFD1C, slot, D_8005A414[slot], 2, 0, slot, 1);
        D_800AFD20 = -0xC0;
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
        func_800A0C94();
        D_800B0078->flags = (D_800B0078->flags | 0x100) & ~0x80;
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
    } else {
        func_80076AC0(D_800AFD1C, 0, D_8005A414[0], 1, 0, 0, 1);
        D_800B0078->flags |= 1;
        D_800AFFEC = 1;
        D_800B00C0 = 1;
        D_800B0078->layer_flags |= 0x100000;
    }
    D_800B0078->pc += 3;
}

/* Event 16: the current actor becomes party character operand 1 (ff, fe, fd: party slots 2, 1, 0). A party member takes its slot (slot 0 becomes the controlled actor), its sprite (or sprite 800ae294[character] of the alternate set 800b2268) and map entry variable 2; others hide and end their script.
 * The alternate sprite's offset entry is addressed before the call. */
void func_800A08B8(void) {
    FieldDescriptor *descriptor;
    s32 character;
    s32 slot;
    Sprite *model;
    s32 *sprites;
    s32 *entry;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    character = func_8008CF3C(func_800ACDEC(1));
    slot = func_8009FA00(character);
    D_800B0078->unkE4 = character;
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    if (slot != -1) {
        if (slot == 0) {
            D_800B2078.controlled = D_800AFD1C;
            D_800B2078.unk233E = D_800AFD1C;
            D_800B0078->flags = (D_800B0078->flags | 0x4400) & ~0x80;
        }
        D_8005A444[slot] = D_800AFD1C;
        if (D_800B2078.unk2268 != 0) {
            sprites = D_800AF880.components.sprites;
            entry = &sprites[D_800AE294[character] + 1] + D_800B2078.unk2268;
            func_80076AC0(D_800AFD1C, D_800AE294[character] + D_800B2078.unk2268, (u8 *)(*entry + (s32)sprites),
                          0, 0, (D_800AE294[character] + D_800B2078.unk2268) | 0x80, 1);
            D_800B0078->flags = (D_800B0078->flags | 0x400) & ~0x300;
            if (D_8005A39C->inGear[slot] != 0) {
                model = D_800AF880.components.descriptors[D_800AFD1C].model;
                D_800AF880.components.descriptors[D_800AFD1C].model = D_800AF880.components.descriptors[D_8006F990[slot]].model;
                D_800AF880.components.descriptors[D_8006F990[slot]].model = model;
                D_800B0078->flags = (D_800B0078->flags | 0x200) & ~0x500;
            }
        } else {
            func_80076AC0(D_800AFD1C, slot, D_8005A414[slot], 1, 0, slot, 1);
            D_800B0078->flags = (D_800B0078->flags | 0x400) & ~0x300;
        }
        D_800AFD20 = -0xC0;
        D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
        func_8009FA54(func_800A3018(2));
        func_800A0C94();
        D_800B0078->layer_flags &= ~0x800;
    } else {
        func_80076AC0(D_800AFD1C, 0, D_8005A414[0], 1, 0, 0, 1);
        D_800B0078->flags |= 1;
        D_800AFFEC = 1;
        D_800B00C0 = 1;
        D_800B0078->layer_flags |= 0x100000;
    }
    D_800B0078->flags |= 0x20000;
    D_800B0078->layer_flags |= 0x400;
    D_800B0078->pc += 3;
}

/* Set flag 0x80 on the controlled actor. */
void func_800A0C4C(void) {
    FieldActor *player;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    player->flags |= 0x80;
}

/* Mirror the current actor's position into its descriptor and model. */
void func_800A0C94(void) {
    Sprite *model;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[0] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] =
        WHOLE(D_800B0078->position[0]);
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[1] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] =
        WHOLE(D_800B0078->position[1]);
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[2] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] =
        WHOLE(D_800B0078->position[2]);
    model->x = D_800B0078->position[0];
    model->y = D_800B0078->position[1];
    model->z = D_800B0078->position[2];
    model->speed_y = 0;
    D_800B0078->unk72 = model->ground = WHOLE(D_800B0078->position[1]);
}


/* Give the current actor the field's first sprite and show it. */
void func_800A0D3C(void) {
    FieldActor *actor;
    s32 *sprites;

    sprites = D_800AF880.components.sprites;
    func_80076AC0(D_800AFD1C, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 1);
    func_800A0C94();
    actor = D_800B0078;
    actor->flags |= 0x100;
    actor->layer_flags |= 0x800;
    actor->pc++;
}

/* Set 800b234a from operand 1: the slot count the 801e module's tween pool is
 * reset with (801e738c) when the layers are rebuilt (80077ab4). */
void func_800A0DC0(void) {
    D_800B2078.unk234A = func_800ACDEC(1);
    D_800B0078->pc += 3;
}


/* Store the disc number (80028530: directory table word 0x3c) in variable
 * operand 1. */
void func_800A0DFC(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, func_80028530());
    D_800B0078->pc += 3;
}


/* Wait (pc-- back to fe), yielding, until the movie mode 800adb74 is clear
 * (800a7c58 clears it when the movie ends). */
void func_800A0E54(void) {
    if (D_800ADB74 == 0) {
        D_800B0078->pc += 1;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

extern s32 D_800ADB84;

/* Count 800adb84 up and yield: a nonzero 800adb84 ends a window movie (movie
 * mode 2, ext 67) in 800a7c58. */
void func_800A0EB0(void) {
    D_800B00C0 = 1;
    D_800ADB84 += 1;
    D_800B0078->pc += 1;
}


/* Close the current actor's 801e layer (+12c bits 13-15), clearing its layer
 * flag 0x2000: byte 1 0 deactivates the layer object, 1 releases the layer's
 * actor (801e8030) and counts 800b2264 down. Yields. Other byte values leave
 * the pc on the extended byte, which then runs as primary opcode ca. */
void func_800A0EE8(void) {
    FieldActor *actor;
    s32 layer;

    actor = D_800B0078;
    layer = actor->state.bits.layer;
    actor->layer_flags &= ~0x2000;
    switch (D_800ADC00[actor->pc + 1]) {
    case 0:
        D_801E8670[layer]->active = 0;
        D_800B0078->pc += 2;
        break;
    case 1:
        func_801E8030(actor->state.bits.layer);
        D_800B2078.unk2264--;
        D_800B0078->pc += 2;
        break;
    }
    D_800B00C0 = 1;
}

/* Event fe 5c: 801e layer model of the current actor's layer (+12c bits 13-15)
 * by byte 1; waits (pc-- to the fe) while a music-wave read or the stream is
 * busy (800adb2c, 8008a558). 0 deactivates the layer's object; 1 releases the
 * layer's actor (801e8030) and starts reading its two files 0x6ba/0x6bb + 2 *
 * operand 5, which is the operand of the following fe 5c 02; 2 waits for that
 * read, then creates the layer's actor from the files (801e742c) at the actor's
 * position and scale, sets layer flag 0x2000 and advances 4 (operand 2 is read
 * but unused). 0 and 1 advance 2. Other values do not advance: the pc stays on
 * the extended byte, which runs as primary 5c when the slot resumes. Yields. */
void func_800A0FD8(void) {
    s32 layer;

    if (D_800ADB2C != 0 || func_8008A558() != 0) {
        D_800B00C0 = 1;
        D_800B0078->pc--;
        return;
    }
    layer = D_800B0078->state.bits.layer;
    D_800B0078->layer_flags &= ~0x2000;
    func_80028470(4, 0);
    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        D_801E8670[layer]->active = 0;
        D_800B0078->pc += 2;
        break;
    case 1:
        func_801E8030(D_800B0078->state.bits.layer);
        D_800B2078.unk21DC[layer] = func_800ACDEC(5) * 2;
        D_800B2394[0].file = D_800B2078.unk21DC[layer] + 0x6BA;
        D_800B2394[0].destination = D_8005A420[layer] = func_80031BDC(func_800288EC(D_800B2078.unk21DC[layer] + 0x6BA), 0);
        D_800B2394[1].file = D_800B2078.unk21DC[layer] + 0x6BB;
        D_800B2394[1].destination = D_8005A450[layer] = func_80031BDC(func_800288EC(D_800B2078.unk21DC[layer] + 0x6BB), 1);
        D_800B2394[2].file = 0;
        D_800B2394[2].destination = 0;
        func_80029AFC(D_800B2394, 0, 0);
        D_800B0078->pc += 2;
        break;
    case 2:
        if (func_80028A60(1) == 0) {
            func_800ACDEC(2);
            func_801E742C(layer, 0, D_8005A420[layer], D_8005A450[layer],
                          (s16)(0x240 - (layer + D_800B2078.unk225F[layer]) * 64), 0x100, 0,
                          (s16)(layer + 0xFC), &D_800B2078.layer_angles[layer]);
            D_800B2078.layer_depths[layer] = D_801E8670[layer]->scale;
            func_800320E8(D_8005A450[layer]);
            D_800B0078->pc += 4;
            D_800B0078->layer_flags |= 0x2000;
            D_801E8670[layer]->scale = (D_800B0078->scale[0] * 5) >> 6;
            D_801E8670[layer]->y = D_800B0078->position[1] >> 16;
            D_801E8670[layer]->model->x = WHOLE(D_800B0078->position[0]);
            D_801E8670[layer]->model->z = WHOLE(D_800B0078->position[2]);
        } else {
            D_800B0078->pc--;
        }
        break;
    }
    D_800B00C0 = 1;
}

/* Make the current actor the next 801e layer (800b2264, counted up): give
 * it the field's first sprite, mirror its position, set flag 0x100 and
 * layer flag 0x2000 (clearing 0x800), record operand 1 doubled as the
 * layer's resource pair (files 0x6ba/0x6bb + it, which fe 5c loads) and
 * clear its 800b225f entry. */
void func_800A1364(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 *sprites;
    s32 value;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    value = func_800ACDEC(1);
    sprites = D_800AF880.components.sprites;
    func_80076AC0(D_800AFD1C, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 1);
    func_800A0C94();
    actor = D_800B0078;
    actor->pc += 3;
    actor->flags |= 0x100;
    D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
    actor->layer_flags = (actor->layer_flags | 0x2000) & ~0x800;
    D_800B2078.unk21DC[D_800B2078.unk2264] = value * 2;
    D_800B2078.unk225F[D_800B2078.unk2264] = 0;
    D_800B0078->state.bits.layer = D_800B2078.unk2264;
    D_800B2078.unk2264++;
}

/* Event fe 15: give the current actor field sprite operand 1 with bank operand
 * 3 (80076ac0), mirror its position into its descriptor and model (800a0c94)
 * and enable it: descriptor bit 0x200 set and 0x20 cleared, flag 0x100 set and
 * 0x80 cleared, layer bit 11 cleared. */
void func_800A14F0(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 sprite;
    s32 *sprites;
    u8 *data;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    sprite = func_800ACDEC(1);
    sprites = D_800AF880.components.sprites;
    data = (u8 *)(sprites[sprite + 1] + (s32)sprites);
    func_80076AC0(D_800AFD1C, sprite, data, 0, func_800ACDEC(3), sprite | 0x80, 1);
    func_800A0C94();
    actor = D_800B0078;
    actor->pc += 5;
    actor->flags = (actor->flags | 0x100) & ~0x80;
    actor->layer_flags &= ~0x800;
    D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
}

/* Give the current actor a sprite built from field sprite sheet operand 1
 * (80076ac0: kind 0, bank 0 and flag 0, which updates the sprite once), mirror
 * its position into its descriptor and model (800a0c94), set its flag 0x100
 * (clearing 0x80), clear its layer flag 0x800 and its descriptor flag 0x20; its
 * descriptor flags 0x0f80 first become 0x200 (scheduled by 800a2030). */
void func_800A1624(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 sprite;
    s32 *sprites;

    descriptor = &D_800AF880.components.descriptors[D_800AFD1C];
    descriptor->flags = (descriptor->flags & 0xF07F) | 0x200;
    sprite = func_800ACDEC(1);
    sprites = D_800AF880.components.sprites;
    func_80076AC0(D_800AFD1C, sprite, (u8 *)(sprites[sprite + 1] + (s32)sprites), 0, 0, sprite | 0x80, 0);
    func_800A0C94();
    actor = D_800B0078;
    actor->pc += 3;
    actor->flags = (actor->flags | 0x100) & ~0x80;
    actor->layer_flags &= ~0x800;
    D_800AF880.components.descriptors[D_800AFD1C].flags &= 0xFFDF;
}


/* Call the script at operand 1, pushing the return PC (after the 5-byte
 * instruction); with the four-entry call stack full, report and yield. */
void func_800A1730(void) {
    FieldActor *actor;

    actor = D_800B0078;
    if ((actor->state.word & 0x1C0) != 0x100) {
        actor->call_stack[(actor->state.word >> 6) & 7] = actor->pc + 5;
        D_800B0078->pc = func_800ACDB8(1);
        D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | ((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6;
    } else {
        if (D_800C268C == 0) {
            func_800379C8("STACKERR ACT=%d\n", D_800AFD1C);
        }
        D_800B00C0 = 1;
    }
}

/* Call the script at operand 1, pushing the return PC (after this 3-byte
 * instruction); with the four-entry call stack full, report and yield without
 * advancing. */
void func_800A17F4(void) {
    FieldActor *actor;

    actor = D_800B0078;
    if ((actor->state.word & 0x1C0) != 0x100) {
        actor->call_stack[(actor->state.word >> 6) & 7] = actor->pc + 3;
        D_800B0078->pc = func_800ACDB8(1);
        D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | ((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6;
    } else {
        if (D_800C268C == 0) {
            func_800379C8("STACKERR ACT=%d\n", D_800AFD1C);
        }
        D_800B00C0 = 1;
    }
}

extern s32 D_800AFFEC;

/* Return from a script call; with the call stack empty, report, end the
 * current script slot (priority 15, tag 0xff) and yield. */
void func_800A18B8(void) {
    FieldActor *actor;

    actor = D_800B0078;
    if ((actor->state.word & 0x1C0) == 0) {
        if (D_800C268C == 0) {
            func_800379C8("STACKERR ACT=%d\n", D_800AFD1C);
        }
        D_800B0078->slots[D_800B0078->slot].priority = 15;
        D_800B0078->slots[D_800B0078->slot].tag = 0xFF;
        D_800AFFEC = 1;
        D_800B00C0 = 1;
    } else {
        actor->state.word = (actor->state.word & ~0x1C0) | ((((actor->state.word >> 6) & 7) - 1) & 7) << 6;
        actor->pc = actor->call_stack[(actor->state.word >> 6) & 7];
    }
}

/* Reset the current actor's eight script slots (priority 15, tag ff, resume
 * pc ffff), its call depth and +84, select slot 0 and yield without
 * advancing: all its scripts end, so the scheduler next starts event 1. */
void func_800A19B0(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        D_800B0078->slots[i].countdown = 0;
        D_800B0078->slots[i].unk16 = 0;
        D_800B0078->slots[i].priority = 15;
        D_800B0078->slots[i].resume_pc = 0xFFFF;
        D_800B0078->slots[i].unk22 = 0;
        D_800B0078->slots[i].tag = 0xFF;
        D_800B0078->slots[i].value = 0xFFFF;
        D_800B0078->slots[i].move_mode = 0;
    }
    D_800B0078->slot = 0;
    D_800B0078->unk0CF = 0;
    D_800B00C0 = 1;
    D_800B0078->unk84 = 0;
    D_800B0078->state.bits.depth = 0;
}

s32 func_800A3090(s32 actor, s32 event);

/* Point every priority-7 script slot at the actor's script 1, end the
 * current slot and yield. */
void func_800A1A8C(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800B0078->slots[i].priority == 7) {
            D_800B0078->slots[i].resume_pc = func_800A3090(D_800AFD1C, 1);
        }
    }
    D_800B0078->slots[D_800B0078->slot].priority = 15;
    D_800B0078->slots[D_800B0078->slot].tag = 0xFF;
    D_800B00C0 = 1;
}

extern s32 D_800AFFEC;

/* End the current script slot and yield. */
void func_800A1B70(void) {
    D_800B0078->slots[D_800B0078->slot].priority = 15;
    D_800B0078->slots[D_800B0078->slot].tag = 0xFF;
    D_800AFFEC = 1;
    D_800B00C0 = 1;
}

/* Event 02: compare two halfword operands (bits 7/6 of operand byte 5
 * select an event variable or a signed immediate; variables compare
 * unsigned when flagged so) by condition bits 0-3 of byte 5, and jump to
 * operand 6 unless it holds. */
void func_800A1BD0(void) {
    s32 left;
    s32 right;
    s32 result;

    right = 0;
    left = 0;
    switch (EVENT_OPERAND_BYTE(5) & 0xF0) {
    case 0x00:
        left = func_800A3018(func_800ACDB8(1) & 0xFFFF);
        right = func_800A3018(func_800ACDB8(3) & 0xFFFF);
        if (func_800A2FE0(func_800ACDB8(1) & 0xFFFF) != 0) {
            right &= 0xFFFF;
        } else {
            right = (s16)right;
        }
        break;
    case 0x40:
        left = func_800A3018(func_800ACDB8(1) & 0xFFFF);
        right = (s16)func_800ACD7C(3);
        if (func_800A2FE0(func_800ACDB8(1) & 0xFFFF) != 0) {
            right &= 0xFFFF;
        }
        break;
    case 0x80:
        left = (s16)func_800ACD7C(1);
        right = func_800A3018(func_800ACDB8(3) & 0xFFFF);
        if (func_800A2FE0(func_800ACDB8(3) & 0xFFFF) != 0) {
            left &= 0xFFFF;
        }
        break;
    case 0xC0:
        left = (s16)func_800ACD7C(1);
        right = (s16)func_800ACD7C(3);
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
        D_800B0078->pc += 8;
    } else {
        D_800B0078->pc = func_800ACDB8(6);
    }
}

/* Jump to operand 1. */
void func_800A1E74(void) {
    D_800B0078->pc = func_800ACDB8(1);
}

/* Advance, raising the batch limit by 32. */
void func_800A1E9C(void) {
    D_800AFC7C += 32;
    D_800B0078->pc++;
}

extern void (*D_800AE2A0[])(void); /* event instructions */
extern s32 D_800ADBE0;
extern s32 D_800ADBEC;
extern s32 D_800AFFEC;

/* Run the current actor's event instructions until one yields, its script
 * slot ends, the field starts a transition or `limit` (raised by some
 * instructions) runs out; 1024 is an error. Declared int without a
 * value, as the original keeps $v0 live (its loop delay slot stays empty). */
s32 func_800A1EC8(s32 limit) {
    s32 count;

    D_800B00C0 = 0;
    D_800AFC7C = limit;
    for (count = 0; count < D_800AFC7C; count++) {
        if (count > 0x400) {
            if (D_800C268C == 0) {
                func_800379C8("EVENTLOOP ERROR ACT=%d\n", D_800AFD1C);
            }
            return;
        }
        D_800AE2A0[D_800ADC00[D_800B0078->pc]]();
        if (D_800AFFEC == 0) {
            D_800AFC7C = 0xFFFF;
        }
        if (D_800ADB1C != 0 && (D_800ADBE0 == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0)) {
            return;
        }
        if (D_800B00C0 == 1 && D_800AFFEC == D_800B00C0) {
            return;
        }
    }
}

/* Run every active actor's event script for this frame (only the first while
 * D_800ADB74 is 1): pick its highest-priority slot (or start event 1), run it
 * and keep its resume PC; stop once the field starts a transition. Declared
 * int without a value like func_800A1EC8. The running descriptor is
 * published before its actor is read (the actor pointer is loaded again). */
s32 func_800A2030(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 count;
    s32 index;
    s32 i;
    s32 priority;

    if (D_800ADB74 == 1) {
        count = 1;
    } else {
        count = D_800ADBFC;
    }
    D_800ADB68 = 0;
    D_800C4268 = 0;
    for (index = 0; index < count; index++) {
        if (!(D_800AF880.components.descriptors[index].flags & 0xF00)
            || (D_800AF880.components.descriptors[index].actor->layer_flags & 0x100000)) {
            continue;
        }
        if (D_800ADB1C != 0 && (D_800ADBE0 == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0)) {
            return;
        }
        descriptor = &D_800AF880.components.descriptors[index];
        D_800B06B8 = descriptor;
        actor = descriptor->actor;
        actor->flags &= ~0x1000000;
        D_800AFD1C = index;
        D_800B0078 = actor;
        priority = 0xF;
        if (D_800B2078.party_processing_mode != 0) {
            for (i = 0; i < 3; i++) {
                if (D_8005A444[i] != 0xFF && D_8005A444[i] == index) {
                    goto next;
                }
            }
        }
        for (i = 0; i < 8; i++) {
            if (priority >= D_800B0078->slots[i].priority) {
                priority = D_800B0078->slots[i].priority;
                D_800B0078->slot = i;
            }
        }
        if (priority == 0xF) {
            D_800B0078->slots[0].resume_pc = func_800A3090(index, 1);
            D_800B0078->slots[0].priority = 7;
            D_800B0078->slot = 0;
        }
        D_800B0078->pc = D_800B0078->slots[D_800B0078->slot].resume_pc;
        D_800AFFEC = 1;
        if (!(D_800B0078->flags & 1)) {
            func_800A1EC8(8);
        }
        D_800B0078->slots[D_800B0078->slot].resume_pc = D_800B0078->pc;
    next:;
    }
}

s32 func_800A1EC8(s32 limit);
extern s32 D_800AFFEC;

/* Run event `event` of actor 0 immediately with fresh script slots, then
 * restore the actor's record. */
void func_800A22AC(s32 event) {
    FieldActor *saved;
    s32 i;

    D_800B0078 = (D_800B06B8 = D_800AF880.components.descriptors)->actor;
    saved = func_80031BDC(sizeof(FieldActor), 1);
    *saved = *D_800B06B8->actor;
    for (i = 0; i < 8; i++) {
        D_800B0078->slots[i].countdown = 0;
        D_800B0078->slots[i].unk16 = 0;
        D_800B0078->slots[i].priority = 15;
        D_800B0078->slots[i].resume_pc = 0xFFFF;
        D_800B0078->slots[i].unk22 = 0;
        D_800B0078->slots[i].tag = 0xFF;
        D_800B0078->slots[i].value = 0xFFFF;
        D_800B0078->slots[i].move_mode = 0;
    }
    D_800AFD1C = 0;
    D_800ADB1C = 0;
    D_800AFFEC = 0;
    D_800B0078->pc = func_800A3090(0, event);
    func_800A1EC8(0xFFFF);
    D_800ADB1C = 1;
    *D_800B06B8->actor = *saved;
    func_800320E8(saved);
}

void func_800A22AC(s32 mode);

/* Rebuild the party (mode 3) with 800adb8c set. */
void func_800A2488(void) {
    D_800ADB8C = 1;
    func_800A22AC(3);
    func_800ACE24();
    D_800ADB8C = 0;
}

/* After a return to the field: run actor 0's event 2, show reassigned
 * party members, restart each 801e layer's animation, lift the controlled
 * actor 8 units unless its +74 is 0xff, and move the pieces by their
 * accumulated drift (mode 0: non-event pieces, mode 1: all but moving
 * event actors). `i` also holds the +74 byte, as in the original. */
void func_800A24C4(void) {
    u8 unused[0x10]; /* the original frame holds 0x10 unused bytes */
    s32 i;

    if (D_8004F30C != 0) {
        func_800A22AC(2);
        func_800AD898();
        for (i = 0; i < D_800B2078.unk2264; i++) {
            func_801E8330((u16)i, 0, D_800B2078.unk21E4[i]);
        }
        i = D_800AF880.components.descriptors[D_800B2078.controlled].actor->unk074;
        if (i != 0xFF) {
            D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[1] -= 8;
        }
        for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
            if (i < D_800ADBFC) {
                if (D_800AF880.components.descriptors[i].actor->state.word & 3) {
                    continue;
                }
            } else if ((D_800B2078.piece_drift_mode & 0x7F) == 0) {
                D_800AF880.components.descriptors[i].matrix.t[0] += PIECE_DRIFT_TOTAL[0];
                D_800AF880.components.descriptors[i].matrix.t[1] += PIECE_DRIFT_TOTAL[1];
                D_800AF880.components.descriptors[i].matrix.t[2] += PIECE_DRIFT_TOTAL[2];
            }
            if ((D_800B2078.piece_drift_mode & 0x7F) == 1) {
                D_800AF880.components.descriptors[i].matrix.t[0] += PIECE_DRIFT_TOTAL[0];
                D_800AF880.components.descriptors[i].matrix.t[1] += PIECE_DRIFT_TOTAL[1];
                D_800AF880.components.descriptors[i].matrix.t[2] += PIECE_DRIFT_TOTAL[2];
            }
        }
    }
}

void func_800A3C8C(void);

/* Reload the actors' extra blocks (file +124 into +120) and hand them to
 * their models, then refresh the field state. */
void func_800A2714(void) {
    FieldActor *actor;
    s32 i;

    if (D_8004F30C != 0) {
        for (i = 0; i < D_800ADBFC; i++) {
            func_80028470(4, 0);
            actor = D_800AF880.components.descriptors[i].actor;
            if (actor->unk124 != -1) {
                D_800B0078 = actor;
                D_800B0078->unk120 = func_80031BDC(func_800288EC(actor->unk124) + 8, 0);
                func_800295D8(D_800B0078->unk124, D_800B0078->unk120, 0, 0x80);
                func_80028A60(0);
            }
        }
        for (i = 0; i < D_800ADBFC; i++) {
            if (D_800AF880.components.descriptors[i].actor->unk124 != -1) {
                func_80021BF0(D_800AF880.components.descriptors[i].model,
                              (s32)D_800AF880.components.descriptors[i].actor->unk120);
            }
        }
        func_800A3C8C();
        if (D_800B2078.unk2078 != 0) {
            func_800A484C(1);
        }
        func_800A3074(0x10, 0);
        func_800A30B4();
        for (i = 0; i < D_800ADBFC; i++) {
            func_80072254(i);
        }
    }
}

/* When D_8004F30C is set: rebuild every actor's sprite and animation state,
 * swap in the party models and rerun the actors' setup scripts. Each sprite
 * table pointer is a block-local variable. */
void func_800A28D4(void) {
    s32 i;
    Sprite *model;
    FieldActor *actor;

    if (D_8004F30C != 0) {
        func_800A3474();
        for (i = 0; i < D_800ADBFC; i++) {
            actor = D_800AF880.components.descriptors[i].actor;
            if (!(actor->unk126 & 0x80)) {
                func_80076AC0(i, actor->unk127, D_8005A414[actor->unk126], actor->sprite_kind & 3,
                              actor->unk134 & 0xF, D_800AF880.components.descriptors[i].actor->unk126,
                              (D_800AF880.components.descriptors[i].actor->unk134 >> 4) & 1);
            } else {
                s32 *sprites = D_800AF880.components.sprites;

                func_80076AC0(i, actor->unk127, (u8 *)(sprites[(actor->unk126 & 0x7F) + 1] + (s32)sprites),
                              actor->sprite_kind & 3, actor->unk134 & 0xF,
                              D_800AF880.components.descriptors[i].actor->unk126,
                              (D_800AF880.components.descriptors[i].actor->unk134 >> 4) & 1);
                switch (D_800AF880.components.descriptors[i].actor->state.bits.unk16) {
                case 1:
                    func_8002303C(D_800AF880.components.descriptors[i].model, 2, 0);
                    SPRITE_SEQUENCER(D_800AF880.components.descriptors[i].model)->buffer[2] = D_800AF880.components.descriptors[i].actor->state.bits.unk18;
                    SPRITE_SEQUENCER(D_800AF880.components.descriptors[i].model)->buffer[3] = D_800AF880.components.descriptors[i].actor->unk130;
                    break;
                case 2:
                    func_8002303C(D_800AF880.components.descriptors[i].model, 3, 0);
                    SPRITE_SEQUENCER(D_800AF880.components.descriptors[i].model)->buffer[2] = D_800AF880.components.descriptors[i].actor->state.bits.unk18;
                    SPRITE_SEQUENCER(D_800AF880.components.descriptors[i].model)->buffer[3] = D_800AF880.components.descriptors[i].actor->unk130;
                    SPRITE_SEQUENCER(D_800AF880.components.descriptors[i].model)->buffer[4] = D_800AF880.components.descriptors[i].actor->unk130_9;
                    SPRITE_SEQUENCER(D_800AF880.components.descriptors[i].model)->buffer[5] = D_800AF880.components.descriptors[i].actor->unk130_19;
                    break;
                }
            }
        }
        if (D_800B2078.unk2268 != 0) {
            for (i = 0; i < 3; i++) {
                if (D_8005A444[i] != 0xFF) {
                    if (D_8005A39C->inGear[i] != 0) {
                        model = D_800AF880.components.descriptors[D_8005A444[i]].model;
                        D_800AF880.components.descriptors[D_8005A444[i]].model = D_800AF880.components.descriptors[D_8006F990[i]].model;
                        D_800AF880.components.descriptors[D_8006F990[i]].model = model;
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags |= 0x200;
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags &= ~0x500;
                        D_800AF880.components.descriptors[D_8006F990[i]].flags |= 0x20;
                    } else {
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags |= 0x400;
                        D_800AF880.components.descriptors[D_8006F990[i]].actor->flags &= ~0x300;
                    }
                }
            }
        }
    } else {
        func_800A3074(0x10, 0);
        func_800A30B4();
        for (i = 0; i < D_800ADBFC; i++) {
            D_800AFD1C = i;
            D_800B06B8 = &D_800AF880.components.descriptors[i];
            D_800B0078 = D_800B06B8->actor;
            D_800B0078->pc = func_800A3090(i, 2);
            if (D_800ADC00[D_800B0078->pc] == 0) {
                D_800B0078->layer_flags |= 0x4000000;
            }
            D_800AFD1C = i;
            D_800B06B8 = &D_800AF880.components.descriptors[i];
            D_800B0078 = D_800B06B8->actor;
            D_800B0078->pc = func_800A3090(i, 0);
        }
        for (i = 0; i < D_800ADBFC; i++) {
            D_800AFD1C = i;
            D_800AFC74 = 0;
            D_800AFFEC = 0;
            D_800B06B8 = &D_800AF880.components.descriptors[i];
            D_800B0078 = D_800B06B8->actor;
            func_800A1EC8(0xFFFF);
            if (D_800AFC74 == 0) {
                s32 *sprites = D_800AF880.components.sprites;

                func_80076AC0(i, 0, (u8 *)(sprites[1] + (s32)sprites), 0, 0, 0x80, 0);
                D_800B0078->layer_flags |= 0x800;
            }
        }
    }
}

/* Advance one byte. */
void func_800A2FC0(void) {
    D_800B0078->pc++;
}

/* -1 when event variable `reference` is read unsigned, else 0. */
s32 func_800A2FE0(s32 reference) {
    if (D_800ADBF8->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        return -1;
    }
    return 0;
}

/* Read event variable `reference` (a byte offset into the bank). */
s32 func_800A3018(s32 reference) {
    s32 value;

    if (D_800ADBF8->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        value = (u16)D_800C3A68[reference >> 1];
    } else {
        value = D_800C3A68[reference >> 1];
    }
    return value;
}

/* Write event variable `reference` (both bank cases store the same halfword). */
void func_800A3074(s32 reference, s32 value) {
    if (D_800ADBF8->unsigned_bits[reference >> 6] & (1 << ((reference >> 1) & 0x1F))) {
        D_800C3A68[reference >> 1] = (u16)value;
    } else {
        D_800C3A68[reference >> 1] = value;
    }
}

/* Entry PC of event `event` of actor `actor`. */
s32 func_800A3090(s32 actor, s32 event) {
    u16 *entries = D_800ADBF8->entries;

    return entries[actor * 32 + event];
}

/* Store the three party members in variables 3e, 40, 42. */
void func_800A30B4(void) {
    func_800A3074(0x3E, D_80062590[0]);
    func_800A3074(0x40, D_80062590[1]);
    func_800A3074(0x42, D_80062590[2]);
}

s32 func_8009744C(void);
s32 func_8009A514(void);

/* Record the current map and camera in the game state and variables and
 * save the event variable bank. */
void func_800A30FC(void) {
    s32 i;

    D_8005A39C->map = D_8004F34C;
    D_8005A39C->flagWords[0] = D_8004F324;
    D_8005A39C->entry[2] = D_8005A39C->vars[1];
    D_8005A39C->entry[0] = D_8005A39C->vars[4] << 9;
    func_800A3074(0x44, D_8005941C);
    func_800A3074(0x46, D_800594D0);
    func_800A3074(6, func_8009744C() & 0xFFFF);
    func_800A3074(8, func_8009A514() & 0xFFFF);
    func_800A3074(0x24, (s16)D_800AF880.elevation);
    func_800A3074(0x3C, D_8004F34C);
    func_800A30B4();
    for (i = 0; i < 0x200; i++) {
        D_8005A39C->vars[i] = D_800C3A68[i];
    }
}

/* Update the play record once per frame (not while 800b02c8 is 1): the
 * held buttons seen, the party, the departure data, the party slots'
 * positions, the play clock in variable 10 (minutes:seconds stepped
 * every 31 frames; counting down with 8004f328 bit 2, stopped by bit 7)
 * and the controlled actor's position in variables 1e-22. */
void func_800A31E8(void) {
    s32 i;
    s32 value;
    s32 seconds;
    s32 minutes;

    if (D_800B02C8 == 1) {
        return;
    }
    D_800AFC6C |= D_800AFE9C;
    for (i = 0; i < 3; i++) {
        D_8005A39C->party[i] = D_80062590[i];
    }
    func_800A30FC();
    D_8004F2F4 = 0;
    D_8004F318++;
    for (i = 0; i < 3; i++) {
        if (D_8005A39C->inGear[i] == 1) {
            func_8009FEE4(i);
        }
    }
    if (D_8004F318 > 30) {
        D_8004F318 = 0;
        if (!(D_8004F328 & 0x80)) {
            value = func_800A3018(0xA);
            seconds = value & 0xFF;
            minutes = (value >> 8) & 0xFF;
            if (!(D_8004F328 & 4)) {
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
            func_800A3074(0xA, (minutes << 8) | (seconds & 0xFF));
        }
    }
    func_800A3074(0xC, D_80059418 | (D_80059420 << 8));
    func_800A3074(0xE, D_80059484);
    func_800A3074(0x1E, WHOLE(D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[0]));
    func_800A3074(0x20, WHOLE(D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[2]));
    func_800A3074(0x22, WHOLE(D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[1]));
}

/* Read the field state block at D_8005A4E4 back (the inverse of
 * func_800A3F4C): descriptor count, view, collision attributes, D_800B2078
 * and the per-actor records, keeping each actor's list pointer and
 * allocating its link and unk114 blocks as the record says. */
void func_800A3474(void) {
    FieldDescriptor *descriptor;
    s32 i;
    s32 flags;
    s32 *list;

    D_800AFC50 = D_8005A4E4;
    D_800AF880.components.descriptor_count = *D_800AFC50;
    D_800AFC50 += 4;
    COPY_BLOCK(&D_800B007C, D_800AFC50, 0x38);
    D_800AFC50 += 0x38;
    COPY_BLOCK(&D_800AF880.world_angles, D_800AFC50, 0x74);
    D_800AFC50 += 0x74;
    COPY_BLOCK(D_800AF880.components.collision_attributes, D_800AFC50, 0x400);
    D_800AFC50 += 0x400;
    COPY_BLOCK(&D_800B2078, D_800AFC50, sizeof(FieldWork));
    D_800AFC50 += sizeof(FieldWork);
    COPY_BLOCK(&D_800AF880, D_800AFC50, 0x1C8);
    D_800AFC50 += 0x1C8;
    for (i = 0; i < D_800ADBFC; i++) {
        descriptor = &D_800AF880.components.descriptors[i];
        COPY_BLOCK(&descriptor->rotation, D_800AFC50, 8);
        D_800AFC50 += 8;
        COPY_BLOCK(&flags, D_800AFC50, 4);
        D_800AF880.components.descriptors[i].flags = flags;
        D_800AFC50 += 4;
        D_800AFC50 += 0x30;
        list = D_800AF880.components.descriptors[i].actor->list;
        COPY_BLOCK(D_800AF880.components.descriptors[i].actor, D_800AFC50, 0x138);
        D_800AF880.components.descriptors[i].actor->list = list;
        D_800AFC50 += 0x138;
        if (D_800AF880.components.descriptors[i].actor->unk134 & 0x80) {
            D_800AF880.components.descriptors[i].actor->link = func_80031BDC(0xC, 0);
            COPY_BLOCK(D_800AF880.components.descriptors[i].actor->link, D_800AFC50, 0xC);
            D_800AFC50 += 0xC;
        }
        if (D_800AF880.components.descriptors[i].actor->state.word & 0x1000) {
            D_800AF880.components.descriptors[i].actor->unk114 = func_80031BDC(0x10, 0);
            COPY_BLOCK(D_800AF880.components.descriptors[i].actor->unk114, D_800AFC50, 0x10);
            D_800AFC50 += 0x10;
        }
    }
    COPY_BLOCK(D_800C3A68, D_800AFC50, 0x800);
    D_800AFC50 += 0x800;
}

/* Read the descriptor count, view block and per-actor records from the
 * block at D_8005A4E4, passing each model its record (func_80021D50). */
void func_800A3C8C(void) {
    s32 changed;
    s32 i;
    u8 *record;

    D_800AFC50 = D_8005A4E4;
    D_800AF880.components.descriptor_count = *D_800AFC50;
    D_800AFC50 += 0x3C;
    *(ViewSnapshot *)&D_800AF880.world_angles = *(ViewSnapshot *)D_800AFC50;
    D_800AFC50 += 0x920;
    changed = 0;
    for (i = 0; i < 3; i++) {
        if (D_8005A408[i] != D_8005A39C->inGear[i]) {
            changed++;
        }
    }
    for (i = 0; i < D_800ADBFC; i++) {
        D_800AFC50 += 0xC;
        record = D_800AFC50;
        if (D_800AF880.components.descriptors[i].actor->unk124 != -1 && D_800AF880.components.descriptors[i].actor->unk0EA != 0xFF) {
            *(s16 *)(record + 0x14) = D_800AF880.components.descriptors[i].actor->unk0EA;
        }
        if (!(D_800AF880.components.descriptors[i].actor->layer_flags & 0x1000000)) {
            if (D_800B2078.unk2268 == 0 || !(D_800AF880.components.descriptors[i].actor->flags & 0x600)) {
                func_80021D50(D_800AF880.components.descriptors[i].model, (SpriteState *)D_800AFC50);
            } else if (changed == 0) {
                func_80021D50(D_800AF880.components.descriptors[i].model, (SpriteState *)D_800AFC50);
            }
        }
        record = D_800AFC50;
        D_800AFC50 = record + 0x168;
        if (D_800AF880.components.descriptors[i].actor->unk134 & 0x80) {
            D_800AFC50 = record + 0x174;
        }
        if (D_800AF880.components.descriptors[i].actor->state.word & 0x1000) {
            D_800AFC50 += 0x10;
        }
    }
}

/* Write the field state block at D_8005A4E4 (descriptor count, view,
 * collision attributes, D_800B2078, per-actor records and D_800C3A68) and
 * print its size. The counter variable is reused for the block address. */
void func_800A3F4C(void) {
    s32 i;
    s32 flags;
    s32 size;
    FieldDescriptor *descriptor;

    D_800AFC50 = D_8005A4E4;
    *D_800AFC50 = D_800AF880.components.descriptor_count;
    D_800AFC50 += 4;
    COPY_BLOCK(D_800AFC50, &D_800B007C, 0x38);
    D_800AFC50 += 0x38;
    COPY_BLOCK(D_800AFC50, &D_800AF880.world_angles, 0x74);
    D_800AFC50 += 0x74;
    COPY_BLOCK(D_800AFC50, D_800AF880.components.collision_attributes, 0x400);
    D_800AFC50 += 0x400;
    COPY_BLOCK(D_800AFC50, &D_800B2078, sizeof(FieldWork));
    D_800AFC50 += sizeof(FieldWork);
    COPY_BLOCK(D_800AFC50, &D_800AF880, 0x1C8);
    D_800AFC50 += 0x1C8;
    for (i = 0; i < D_800ADBFC; i++) {
        descriptor = &D_800AF880.components.descriptors[i];
        COPY_BLOCK(D_800AFC50, &descriptor->rotation, 8);
        D_800AFC50 += 8;
        flags = D_800AF880.components.descriptors[i].flags;
        COPY_BLOCK(D_800AFC50, &flags, 4);
        D_800AFC50 += 4;
        func_80021EBC(D_800AF880.components.descriptors[i].model, (SpriteState *)D_800AFC50);
        D_800AFC50 += 0x30;
        COPY_BLOCK(D_800AFC50, D_800AF880.components.descriptors[i].actor, 0x138);
        D_800AFC50 += 0x138;
        if (D_800AF880.components.descriptors[i].actor->unk134 & 0x80) {
            COPY_BLOCK(D_800AFC50, D_800AF880.components.descriptors[i].actor->link, 0xC);
            D_800AFC50 += 0xC;
        }
        if (D_800AF880.components.descriptors[i].actor->state.word & 0x1000) {
            COPY_BLOCK(D_800AFC50, D_800AF880.components.descriptors[i].actor->unk114, 0x10);
            D_800AFC50 += 0x10;
        }
    }
    COPY_BLOCK(D_800AFC50, D_800C3A68, 0x800);
    D_800AFC50 += 0x800;
    for (i = 0; i < 3; i++) {
        D_8005A408[i] = D_8005A39C->inGear[i];
    }
    size = (s32)D_800AFC50;
    i = (s32)D_8005A4E4;
    if (D_800C268C == 0) {
        size -= i;
        func_800379C8("SAVESIZE=%d %x\n", size, size);
    }
}

/* The event instructions by opcode (run by 800a1ec8), and the extended
 * instructions that opcode fe (800869b8) runs by the following byte. */
void (*D_800AE2A0[256])(void) = {
    /* 00 */ func_800A1B70, func_800A1E74, func_800A1BD0, func_8009C104,
    /* 04 */ func_800A1A8C, func_800A17F4, func_800A1730, func_8009EB78,
    /* 08 */ func_8009ED68, func_8009F0A0, func_8009533C, func_800A1624,
    /* 0C */ func_8009F5A8, func_800A18B8, func_80092404, func_800923E4,
    /* 10 */ func_80098C00, func_80098C3C, func_80093200, func_800A2FC0,
    /* 14 */ func_80093C48, func_80093C6C, func_800A08B8, func_8009E91C,
    /* 18 */ func_8009E83C, func_8009E4BC, func_8009E428, func_8009E35C,
    /* 1C */ func_8009E2C8, func_8009E248, func_8009E208, func_8009E1A0,
    /* 20 */ func_8009E10C, func_8009E094, func_8009DF10, func_8009E040,
    /* 24 */ func_8009DDEC, func_8009DE94, func_8009DD34, func_8009DC4C,
    /* 28 */ func_8009DBC8, func_8009DAC4, func_8009DA1C, func_8009DA44,
    /* 2C */ func_8009A130, func_8009A024, func_80099FC4, func_80099EF8,
    /* 30 */ func_80099F48, func_800961A0, func_800961C8, func_800961F0,
    /* 34 */ func_80096214, func_8009D9A4, func_8009D960, func_8009D91C,
    /* 38 */ func_8009D890, func_8009D804, func_8009D644, func_8009D408,
    /* 3C */ func_8009D340, func_8009D3A4, func_8009D5B8, func_8009D52C,
    /* 40 */ func_8009D4A0, func_8009D2D0, func_8009D260, func_8009D198,
    /* 44 */ func_80098184, func_80097864, func_80092808, func_80092EA0,
    /* 48 */ func_80093CD0, func_80093D48, func_80099980, func_80098430,
    /* 4C */ func_800979F0, func_80097954, func_80098370, func_80098274,
    /* 50 */ func_800977A4, func_800976A8, func_800980FC, func_80098038,
    /* 54 */ func_800975C0, func_8009749C, func_80093014, func_80099214,
    /* 58 */ func_80094918, func_8009F4CC, func_8009524C, func_80095284,
    /* 5C */ func_800A0228, func_8009A174, func_8009A1AC, func_8009AD6C,
    /* 60 */ func_8008FDD0, func_8008FE2C, func_8008FF04, func_8008FF90,
    /* 64 */ func_80090068, func_800900C4, func_8009019C, func_8009ABFC,
    /* 68 */ func_8009AC34, func_8009AC7C, func_8009ACB4, func_8009AB5C,
    /* 6C */ func_8009ABAC, func_8009A6AC, func_8009A768, func_8009A2A8,
    /* 70 */ func_8009A1E4, func_80093568, func_8008F724, func_80086C34,
    /* 74 */ func_8008F668, func_8008F76C, func_80093A68, func_80093A98,
    /* 78 */ func_800973A4, func_80097264, func_800972AC, func_800969FC,
    /* 7C */ func_80096F18, func_80097010, func_80097108, func_80095300,
    /* 80 */ func_80092664, func_800926C8, func_80093664, func_80092768,
    /* 84 */ func_80096644, func_800966B4, func_80096724, func_80096790,
    /* 88 */ func_800967E8, func_80095E48, func_80095C00, func_800962C0,
    /* 8C */ func_8009631C, func_8009640C, func_80095F24, func_80095FB8,
    /* 90 */ func_8009601C, func_800964B0, func_800A19B0, func_800A1364,
    /* 94 */ func_800945D4, func_80094650, func_8009468C, func_8009A634,
    /* 98 */ func_800932D0, func_8008FB98, func_8008FC4C, func_8008FD40,
    /* 9C */ func_8009BB0C, func_8009A34C, func_8009B9A0, func_8009BA0C,
    /* A0 */ func_8009BA7C, func_8009A670, func_8009A58C, func_80090228,
    /* A4 */ func_8009A490, func_8009A534, func_80097410, func_8009F5F4,
    /* A8 */ func_8009D1F0, func_8009BC98, func_8009ACEC, func_80090300,
    /* AC */ func_800903BC, func_80090B18, func_80090B9C, func_80090C20,
    /* B0 */ func_80090CB8, func_80090D50, func_8009A5E0, func_8009731C,
    /* B4 */ func_80097364, func_8009B8E4, func_8009B6AC, func_8009ADDC,
    /* B8 */ func_8009AE0C, func_80096534, func_800965A8, func_800965F4,
    /* BC */ func_800A0D3C, func_80094A5C, func_80094ACC, func_80094B3C,
    /* C0 */ func_80094BAC, func_80094C1C, func_80094C8C, func_800972F4,
    /* C4 */ func_80093E30, func_80093FC0, func_800A1E9C, func_8009B824,
    /* C8 */ func_8009B884, func_80095734, func_8009A824, func_800958C0,
    /* CC */ func_80095520, func_8009DA70, func_8009DA98, func_8009CE48,
    /* D0 */ func_8009CEE0, func_8009CF70, func_8009C0B4, func_8009C0DC,
    /* D4 */ func_8009C01C, func_80092628, func_800925A0, func_800946BC,
    /* D8 */ func_80094710, func_80094764, func_800921E8, func_80091F84,
    /* DC */ func_80092044, func_80091E00, func_8009D6D8, func_8009D768,
    /* E0 */ func_80091E98, func_80091BBC, func_80096150, func_80096178,
    /* E4 */ func_80091AD4, func_80091944, func_80091A08, func_80091A78,
    /* E8 */ func_80094158, func_800943AC, func_80092DFC, func_800910C0,
    /* EC */ func_80091318, func_800915C4, func_80091720, func_8008FA38,
    /* F0 */ func_80090DEC, func_8008B248, func_8008F90C, func_80090E70,
    /* F4 */ func_8009BE9C, func_8009C12C, func_8008E8C8, func_8008E85C,
    /* F8 */ func_8008E59C, func_8008DE64, func_800947B0, func_8008D780,
    /* FC */ func_8009BF8C, func_800A2FC0, func_800869B8, func_800A2FC0,
};
void (*D_800AE6A0[227])(void) = {
    /* 00 */ func_8008D2D8, func_8009F424, func_80095B3C, func_8008D0F4,
    /* 04 */ func_8008D26C, func_80095CC4, func_80095D6C, func_8008D604,
    /* 08 */ func_8008D180, func_8008D078, func_8008D684, func_8008D700,
    /* 0C */ func_8008CFEC, func_8008CF9C, func_8008C84C, func_8008C938,
    /* 10 */ func_8008CA60, func_8008CB4C, func_8008CC74, func_8008CD48,
    /* 14 */ func_8008CDD4, func_800A14F0, func_8008C7D8, func_8009AA00,
    /* 18 */ func_8008BDD8, func_8008C334, func_8008B894, func_8008B5D4,
    /* 1C */ func_80098A7C, func_800984EC, func_8009FB98, func_8009FDD4,
    /* 20 */ func_8009FE4C, func_800A06E8, func_8009B664, func_8009B398,
    /* 24 */ func_8009B210, func_8008D5C8, func_8008B2F0, func_8008B328,
    /* 28 */ func_8008E4EC, func_8008E518, func_8008E544, func_8008E570,
    /* 2C */ func_8008DEBC, func_8008DF44, func_8008DFCC, func_8008E054,
    /* 30 */ func_8008E3E8, func_8008E414, func_8008E440, func_8008E46C,
    /* 34 */ func_8008E298, func_8008E2EC, func_8008E340, func_8008E394,
    /* 38 */ func_8008E1B4, func_8008D230, func_8008CED0, func_8008CE64,
    /* 3C */ func_8008B180, func_8008AEC8, func_8008AFD8, func_8008B0E8,
    /* 40 */ func_80092148, func_8009FC48, func_8009FCAC, func_8009B15C,
    /* 44 */ func_8009B184, func_8009A0FC, func_8008AE5C, func_8008B144,
    /* 48 */ func_8008B518, func_8008DAFC, func_8008ACE8, func_8008A9AC,
    /* 4C */ func_8008A974, func_8008A93C, func_8008AA60, func_80093BB0,
    /* 50 */ func_80093BD4, func_80093BFC, func_80093C20, func_80093AC8,
    /* 54 */ func_80093B10, func_80093740, func_80093930, func_800937E0,
    /* 58 */ func_80093824, func_800939A0, func_80093A04, func_8008B210,
    /* 5C */ func_800A0FD8, func_8008F6AC, func_8008F2D8, func_8008F1C8,
    /* 60 */ func_8008EC30, func_8008E9F8, func_8008F444, func_8008F4A0,
    /* 64 */ func_8008F5E4, func_8008F4FC, func_8008F558, func_8008EE14,
    /* 68 */ func_80092C20, func_8008A6E0, func_8008A604, func_8008A640,
    /* 6C */ func_8008A5A0, func_8008FB28, func_8008FABC, func_8008B45C,
    /* 70 */ func_80089F54, func_8009899C, func_800988B8, func_8009861C,
    /* 74 */ func_800985BC, func_800989F0, func_80098738, func_8008A2E8,
    /* 78 */ func_8008A4F0, func_8008A4E8, func_8008A4E0, func_8008A518,
    /* 7C */ func_8008A500, func_8008A508, func_8008A510, func_8008A244,
    /* 80 */ func_80089FD0, func_8008A08C, func_8008A148, func_80092FB4,
    /* 84 */ func_800933F8, func_8008A2A0, func_80089F94, func_800936E4,
    /* 88 */ func_80089BF0, func_80089DCC, func_80089F18, func_80089B54,
    /* 8C */ func_8008F3D0, func_8008F394, func_8008F348, func_80088790,
    /* 90 */ func_80089004, func_80089174, func_80089374, func_80089574,
    /* 94 */ func_800896D4, func_80089880, func_80089A80, func_80089AE4,
    /* 98 */ func_800884CC, func_8008848C, func_8008F0B4, func_8008EF5C,
    /* 9C */ func_8008EFA0, func_8008F070, func_8008EFE4, func_800883D4,
    /* A0 */ func_8008EA58, func_80088360, func_8008825C, func_800881E8,
    /* A4 */ func_80088198, func_80088C1C, func_800888A4, func_800889BC,
    /* A8 */ func_80090A10, func_80090A94, func_8008DB2C, func_8008DC74,
    /* AC */ func_8008DD6C, func_80096B58, func_80096AF4, func_8008800C,
    /* B0 */ func_8008AACC, func_80087FD4, func_80096D28, func_80096E20,
    /* B4 */ func_80096C40, func_80087FA4, func_80087E98, func_80087E5C,
    /* B8 */ func_80087DE0, func_80087B5C, func_80087C34, func_80087D30,
    /* BC */ func_80087D80, func_80088B68, func_80087C0C, func_80087848,
    /* C0 */ func_80087800, func_80088508, func_80088674, func_8009E014,
    /* C4 */ func_8009DF78, func_80086F7C, func_8008BC80, func_800882B8,
    /* C8 */ func_80088CF8, func_80088D18, func_800A0EE8, func_800A0EB0,
    /* CC */ func_800A0E54, func_800A0DFC, func_800A0DC0, func_80093888,
    /* D0 */ func_8008764C, func_8008754C, func_8008752C, func_80087420,
    /* D4 */ func_80086FD0, func_80087960, func_800879D0, func_80087AB8,
    /* D8 */ func_80087A40, func_80087A7C, func_80093790, func_80097200,
    /* DC */ func_800873C4, func_800871B0, func_80087148, func_80086E1C,
    /* E0 */ func_80086DE0, func_80087580, func_80086D4C,
};

/* Party slot masks by event operand (& 3). */
s16 D_800AEA2C[4] = {7, 1, 2, 4};

/* Facings (0x8000 | angle) per direction: the direction table (turns and
 * faces), the second (camera-relative faces) and the third. */
s16 D_800AEA34[8] = {0x8C00, 0x8E00, 0x8000, 0x8200, 0x8400, 0x8600, 0x8800, 0x8A00};
s16 D_800AEA44[8] = {0x8C00, 0x8E00, 0x8000, 0x8200, 0x8400, 0x8600, 0x8800, 0x8A00};
s16 D_800AEA54[8] = {0x8C00, 0x8400, 0x8800, 0x8000, 0x8A00, 0x8E00, 0x8600, 0x8200};

/* Octant turn table [from * 8 + to]: the signed octants to turn. */
s16 D_800AEA64[64] = {
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
PortraitPlace D_800AEAE4[4][2] = {
    {{0x2C0, 0x100, 0, 0xE1}, {0x2E0, 0x100, 0, 0xE0}},
    {{0x2C0, 0x140, 0, 0xE3}, {0x2E0, 0x140, 0, 0xE2}},
    {{0x2C0, 0x180, 0, 0xE5}, {0x2E0, 0x180, 0, 0xE4}},
    {{0x2C0, 0x1C0, 0, 0xE7}, {0x2E0, 0x1C0, 0, 0xE6}},
};
