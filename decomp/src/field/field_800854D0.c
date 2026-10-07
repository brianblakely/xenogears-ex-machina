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
#include "field_anim.h"
#include "field_gte.h"
#include "field_motion.h"


/* One music-wave stream step: pass arrivals to the chunk callback; -1 once
 * the stream finished and its ring is released. */
s32 func_800854D0(void) {
    s32 arrived = func_80028B14();

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

    if (D_800C3A38 == 0xFF) {
        return;
    }
    times = &D_800AE060[0][0];
    sounds = &D_800AE060[0][1];
    for (;;) {
        if (D_800B06A0 < times[D_800C3A64 * 2] + D_800C3A2C) {
            return;
        }
        sound = sounds[D_800C3A64 * 2];
        func_80039EC4((sound & 0xFF) | (D_800B235C->id << 16), ((sound >> 8) & 7) * 2);
        D_800C3A64++;
    }
}

/* Release a movie's sound-effect bank, when one is loaded. */
void func_80085738(void) {
    if (D_800C3A38 != 0xFF) {
        func_80039FF8();
        func_8003852C(D_800B235C);
        func_800320E8(D_800B235C);
    }
}

#ifdef NON_MATCHING
/* Load a movie's sound-effect bank (file 0x115 + bank) and seek the movie
 * sound timeline past the bank's 0xffff-terminated runs.
 * NON_MATCHING: the original copies the loaded bank from s0 into s1 (s0
 * then holds the file) and recomputes bank + 1 in the loop test; this
 * keeps the bank in s1 directly and hoists bank + 1 into s0. */
void func_80085788(void) {
    u16 *times;
    s32 bank;
    s32 file;
    s32 pos;
    s32 i;

    bank = D_800C3A38;
    if (bank != 0xFF) {
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
        times = &D_800AE060[0][0];
        for (i = 0; i < bank + 1; i++) {
            while (1) {
                if (times[pos * 2] == 0xFFFF) {
                    break;
                }
                pos++;
            }
            pos++;
            D_800C3A64 = pos;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_80085788);
#endif

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

/* The music-wave chunk count (D_800B2078.wave_chunks), which the chunk
 * callback addresses as a scalar of its own. */
extern s32 D_800B2370;

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
        func_8002945C(chunk);
        if (D_800B2370 == 4) {
            D_8006258C = func_800380D0(D_800C3A1C, 0x2000, 0);
        }
        break;
    case 4:
        func_8003BDFC(0x10);
        *(WaveChunk *)D_800C3A1C = *chunk;
        func_8003827C(D_800C3A1C, 0x800);
        func_8002945C(chunk);
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
            D_800B2078.wave_chunks = 0;
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
            sequence = func_80039850(D_80062648);
            D_80062528 = sequence;
            if (D_8004F340 == -1) {
                func_80039A80(sequence, 0x7F, 0);
            } else {
                func_80039A80(D_80062528, 0, 0);
                func_8003A89C(D_80062528, 0, 0);
            }
        } else {
            D_80062528 = D_8004F2FC;
            func_80039B68(D_8004F2FC, 0x7F, 0xF0);
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
        func_80039C4C(D_8004F2FC);
        func_800399D4(D_8004F2FC);
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
    bank = func_80037FD8(D_800B00E0, 0);
    D_8006251C = bank;
    D_80059560 = bank;
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
        func_80038310(D_8006251C);
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

/* Event opcode fe: run the extended instruction named by the next byte. */
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

/* Event: selector byte 1: 0 skips; 1 starts emitter record 0 on the current
 * actor with operand 4 (0x27 selects mode 0x20, else 0x22). */
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

/* Event opcode e2: call resident 80019cd0, yield and step over the opcode. */
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

/* Event opcode e0: set 800b2358 from its byte operand. */
void func_80086DE0(void) {
    D_800B2078.unk2358 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: switch to the 640-wide display (op1 0), or set (1) / clear (2)
 * 800adb54. */
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

/* Event: set the current actor's +128 to (op1 << 12) | op3. */
void func_80086F7C(void) {
    s32 high = func_800ACDEC(1);

    D_800B0078->unk128 = (high << 12) | func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: effect control by selector byte: stop (0), start with four
 * operands (1), pause (2) or restart with four operands (3). */
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

    D_8005A39C->records[record].flags |= bits;
    D_800B0078->pc += 5;
}

/* Event op dd: save a 256-wide screen band (0), process rows of it (1),
 * release its buffers (2) or do nothing (3). */
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

/* Event: store op3 in byte op1 of the table at 800b225f. */
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
    D_8005A39C->unk22B6 |= 0x4000;
    D_800B0078->pc++;
}

/* Event: copy character op1 over character op3. */
void func_80087580(void) {
    s32 from = func_800ACDEC(1);

    D_8005A39C->gears[func_800ACDEC(3)] = D_8005A39C->gears[from];
    D_800B0078->pc += 5;
}

/* Event: copy character slot and record op1 over op3; slots 9 and 10 set
 * game flags 0x2000 / 0x1000. */
void func_8008764C(void) {
    s32 from = func_800ACDEC(1);
    s32 to = func_800ACDEC(3);

    D_8005A39C->characters[to] = D_8005A39C->characters[from];
    D_8005A39C->records[to] = D_8005A39C->records[from];
    {
        GameState *state = D_8005A39C;

        if (to == 9) {
            state->unk22B6 |= 0x2000;
        }
    }
    if (to == 10) {
        D_8005A39C->unk22B6 |= 0x1000;
    }
    D_800B0078->pc += 5;
}

/* Event: store 80050622 in variable op1. */
void func_80087800(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_80050622);
    D_800B0078->pc += 3;
}

/* Event: once sound is idle, stop it and set the six sound bytes at
 * 8005061c from operands; wait otherwise. */
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

/* Event: store the game's +1844 and +1846 in variables op1 and op3. */
void func_80087960(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk1844);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->unk1846);
    D_800B0078->pc += 5;
}

/* Event: store the game's +184e and +1852 in variables op1 and op3. */
void func_800879D0(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk184E);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->unk1852);
    D_800B0078->pc += 5;
}

/* Event: set 800b2357 from its byte operand. */
void func_80087A40(void) {
    D_800B2078.unk2357 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: set 800b2354 from its byte operand. */
void func_80087A7C(void) {
    D_800B2078.unk2354 = D_800ADC00[D_800B0078->pc + 1];
    D_800B0078->pc += 2;
}

/* Event: set the game's +184e and +1852 from operands 1 and 3 (immediate by
 * flags 0x80/0x40 of byte 9) and reset +1850/+1854, setting +1856. */
void func_80087AB8(void) {
    D_8005A39C->unk184E = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk1852 = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk1854 = 0;
    D_8005A39C->unk1850 = 0;
    D_8005A39C->unk1856 = 1;
    D_800B0078->pc += 6;
}

/* Event: store the game's four halfwords at +182c in variables op1..op7. */
void func_80087B5C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk182C[0]);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->unk182C[1]);
    func_800A3074(func_800ACDB8(5) & 0xFFFF, D_8005A39C->unk182C[2]);
    func_800A3074(func_800ACDB8(7) & 0xFFFF, D_8005A39C->unk182C[3]);
    D_800B0078->pc += 9;
}

/* Event: set 8004f300. */
void func_80087C0C(void) {
    D_8004F300 = 1;
    D_800B0078->pc++;
}

/* Event: set the game's four halfwords at +182c from operands 1..7
 * (immediate by flags 0x80/0x40/0x20/0x10 of byte 9). */
void func_80087C34(void) {
    D_8005A39C->unk182C[0] = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk182C[1] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk182C[2] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_8005A39C->unk182C[3] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event: store the game's +1834 in variable op1. */
void func_80087D30(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->unk1834);
    D_800B0078->pc += 3;
}

/* Event: set the game's +1834 from operand 1 (immediate when flag 0x80 of
 * byte 3 is set). */
void func_80087D80(void) {
    D_8005A39C->unk1834 = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 3]);
    D_800B0078->pc += 4;
}

/* Event: set 800b2355 (selector byte 0) or 800b2356 from operand 2. */
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

/* Event: make the selected actor the controlled one (clearing every actor's
 * control flags first). */
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

/* Event: count 800b2348 up. */
void func_80087FA4(void) {
    D_800B2078.unk2348++;
    D_800B0078->pc++;
}

/* Event: run 800a8ba4. */
void func_80087FD4(void) {
    func_800A8BA4();
    D_800B0078->pc++;
}

extern void func_801E72CC(MATRIX *m, MATRIX *work, s32 a, s32 b);
/* Event: rotate the vector operands 5/7/9 by the rotation built (801e72cc)
 * from operands 1 and 3 and store the result in variables 0xc, 0xe, 0x10. */
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

/* Event: restore every character's two gauges to their maxima. */
void func_80088198(void) {
    s32 i;
    GameState *state = D_8005A39C;

    for (i = 0; i < 20; i++) {
        state->gears[i].points = state->gears[i].points_max;
        state->gears[i].gauge = state->gears[i].gauge_max;
    }
    D_800B0078->pc++;
}

/* Event: pause (selector 0) or resume the particles' VRAM. */
void func_800881E8(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        func_800A915C();
    } else {
        func_800A91F0();
    }
    D_800B0078->pc += 2;
}

/* Event: wait until the pending sound is resolved, yielding each time. */
void func_8008825C(void) {
    if (D_8004F308 == -1) {
        D_800B0078->pc--;
    } else {
        D_800B0078->pc++;
    }
    D_800B00C0 = 1;
}

/* Event: store character op1's slot byte +2c (0xff for none) in variable
 * op3. */
void func_800882B8(void) {
    s32 character = func_8008CF3C(func_800ACDEC(1));

    if (character != 0xFF) {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, D_8005A39C->characters[character].unkA0);
    } else {
        func_800A3074(func_800ACDB8(3) & 0xFFFF, 0xFF);
    }
    D_800B0078->pc += 5;
}

/* Event: set byte +4 of party slot op1 to op3. */
void func_80088360(void) {
    s32 slot = func_800ACDEC(1);

    D_8005A39C->characters[slot].unkA0 = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Event: set (selector 0) or clear character op2's bit of the game's +2318. */
void func_800883D4(void) {
    s32 character = func_8008CF3C(func_800ACDEC(2));

    if (character != 0xFF) {
        if (D_800ADC00[D_800B0078->pc + 1] == 0) {
            D_8005A39C->unk2318 |= 1 << character;
        } else {
            D_8005A39C->unk2318 &= ~(1 << character);
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
    FieldModel *model;

    member = D_8005A444[func_800ACDEC(5)];
    D_800AFC7C += 4;
    if (member != 0xFF) {
        model = D_800AF880.components.descriptors[member].model;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, model->animation->unk0C);
        func_800A3074(func_800ACDB8(3) & 0xFFFF, member);
        if (model->animation->unk0C != 1) {
            model->animation->unk0C = 0;
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
        D_800B02CC[D_800B2078.unk2384].unk30[i][0] = D_800B02CC[D_800B2078.unk2384].unk30[i][1] = 0;
    }
}

/* Event: start effect op3 (0..3 map to 0, 0x10, 0x20, 0x30) with op5 and op7
 * for actor op1, using four batch steps. */
void func_80088674(void) {
    s32 actor = func_800ACDEC(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    D_800B2078.unk2374 = func_800ACDEC(1);
    D_800B2078.unk2378 = func_800ACDEC(3);
    D_800B2078.unk237C = func_800ACDEC(5);
    D_800B2078.unk2380 = func_800ACDEC(7);
    D_800B0078->pc += 9;
    func_800A94A4(actor);
    switch (D_800B2078.unk2378) {
    case 0:
        D_800B2078.unk2378 = 0;
        break;
    case 1:
        D_800B2078.unk2378 = 0x10;
        break;
    case 2:
        D_800B2078.unk2378 = 0x20;
        break;
    case 3:
        D_800B2078.unk2378 = 0x30;
        break;
    }
    D_800AFC7C += 4;
}

/* Event: start effect op2 (0..3 map to 0, 0x10, 0x20, 0x30) with op4 and op6
 * on the selected actor, using four batch steps. */
void func_80088790(void) {
    s32 actor = func_8009CDB4(1);

    if (actor == 0xFF) {
        actor = 0;
    }
    D_800B2078.unk2374 = actor;
    D_800B2078.unk2378 = func_800ACDEC(2);
    D_800B2078.unk237C = func_800ACDEC(4);
    D_800B2078.unk2380 = func_800ACDEC(6);
    D_800B0078->pc += 8;
    func_800A94A4(actor);
    switch (D_800B2078.unk2378) {
    case 0:
        D_800B2078.unk2378 = 0;
        break;
    case 1:
        D_800B2078.unk2378 = 0x10;
        break;
    case 2:
        D_800B2078.unk2378 = 0x20;
        break;
    case 3:
        D_800B2078.unk2378 = 0x30;
        break;
    }
    D_800AFC7C += 4;
}

extern void func_8002303C(FieldModel *model, s32, s32);
/* Event: set the current actor's model frame from operands 1 and 3. */
void func_800888A4(void) {
    FieldModel *model;
    u16 low;
    s32 frame;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    low = func_800ACDEC(1) & 0xF;
    frame = ((func_800ACDEC(1) >> 4) << 8) + func_800ACDEC(3);
    func_8002303C(model, 2, 0);
    model->animation->unk18[2] = low << 6;
    D_800B0078->state.bits.unk18 = low << 6;
    model->animation->unk18[3] = frame;
    D_800B0078->unk130 = frame;
    D_800B0078->state.bits.unk16 = 1;
    D_800B0078->pc += 5;
}

/* Event: set the current actor's two model frames from operands 1/3 and
 * 5/7. */
void func_800889BC(void) {
    FieldModel *model;
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
    model->animation->unk18[2] = low0 << 6;
    D_800B0078->state.bits.unk18 = low0 << 6;
    model->animation->unk18[3] = frame0;
    D_800B0078->unk130 = frame0;
    model->animation->unk18[4] = low1 << 6;
    D_800B0078->unk130_9 = low1 << 6;
    model->animation->unk18[5] = frame1;
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
    D_800B02CC[D_800B2078.unk2384].flags |= bits;
    D_800AFC7C += 4;
    D_800B0078->pc += 7;
}

/* Event: set the current record's +24, high flag byte and +76 from operands
 * 1, 3 and 5, using four batch steps. */
void func_80088C1C(void) {
    D_800B02CC[D_800B2078.unk2384].unk24 = func_800ACDEC(1);
    D_800B02CC[D_800B2078.unk2384].flags |= func_800ACDEC(3) << 8;
    D_800B02CC[D_800B2078.unk2384].unk76 = func_800ACDEC(5);
    D_800AFC7C += 4;
    D_800B0078->pc += 7;
}

/* 80088d38 with 0. */
void func_80088CF8(void) {
    func_80088D38(0);
}

/* 80088d38 with 4. */
void func_80088D18(void) {
    func_80088D38(4);
}

/* Set the current emitter record's four +30 pairs from index first on to the
 * selected operands 1..15 (flags byte 0x11); four batch steps. */
void func_80088D38(s32 first) {
    D_800B02CC[D_800B2078.unk2384].unk30[first][0] = func_8009CF78(1, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first][1] = func_8009CFBC(3, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 1][0] = func_8009D000(5, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 1][1] = func_8009D044(7, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 2][0] = func_8009D088(9, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 2][1] = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 3][0] = func_8009D110(0xD, EVENT_OPERAND_BYTE(0x11));
    D_800B02CC[D_800B2078.unk2384].unk30[first + 3][1] = func_8009D154(0xF, EVENT_OPERAND_BYTE(0x11));
    D_800AFC7C += 4;
    D_800B0078->pc += 0x12;
}

extern s32 D_800ADB40;
/* Event: select emitter record operand 1 and start it with the effect
 * actor, count operand 3, +02 operand 5 and +04 operand 7; four batch
 * steps. */
void func_80089004(void) {
    s32 index;

    D_800B2078.unk2384 = index = func_800ACDEC(1);
    D_800B02CC[index].unk24 = 1;
    D_800B02CC[D_800B2078.unk2384].unk52 = D_800B2078.unk2374;
    D_800ADB40 = D_800B02CC[D_800B2078.unk2384].unk52;
    D_800B02CC[D_800B2078.unk2384].unk00 = 0;
    D_800B02CC[D_800B2078.unk2384].unk76 = 0;
    D_800B02CC[D_800B2078.unk2384].count = func_800ACDEC(3);
    D_800B02CC[D_800B2078.unk2384].unk02 = func_800ACDEC(5);
    D_800B02CC[D_800B2078.unk2384].unk04 = func_800ACDEC(7);
    func_8008861C();
    D_800AFC7C += 4;
    D_800B0078->pc += 9;
}

/* Event: set the current emitter record's +0c and +14 vectors from the
 * selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089174(void) {
    D_800B02CC[D_800B2078.unk2384].unk0C.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk0C.vy = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk0C.vz = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk14.vx = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk14.vy = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk14.vz = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: set the current emitter record's +08, +1c vector, +26 and +28 from
 * the selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089374(void) {
    D_800B02CC[D_800B2078.unk2384].unk08 = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk1C.vx = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk1C.vy = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk1C.vz = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk26 = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk28 = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: set the current record's +56, +58 and +54 from operands 1..5, its
 * flags from op7, op9 and the effect kind, and +72/+74 from the effect
 * parameters, using four batch steps. */
void func_80089574(void) {
    s16 flags;

    D_800B02CC[D_800B2078.unk2384].unk56 = func_800ACDEC(1);
    D_800B02CC[D_800B2078.unk2384].unk58 = func_800ACDEC(3);
    D_800B02CC[D_800B2078.unk2384].unk54 = func_800ACDEC(5);
    flags = func_800ACDEC(7);
    D_800B02CC[D_800B2078.unk2384].flags = flags | (func_800ACDEC(9) * 2) | D_800B2078.unk2378;
    D_800B02CC[D_800B2078.unk2384].unk72 = D_800B2078.unk237C;
    D_800B02CC[D_800B2078.unk2384].unk74 = D_800B2078.unk2380;
    D_800AFC7C += 4;
    D_800B0078->pc += 11;
}

/* Event: set the current emitter record's +5a and +62 vectors from the
 * selected operands 1/3 and 5/7 (flags byte 9); four batch steps. */
void func_800896D4(void) {
    D_800B02CC[D_800B2078.unk2384].unk5A.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk5A.vy = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk5A.vz = 0;
    D_800B02CC[D_800B2078.unk2384].unk62.vx = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk62.vy = func_8009D044(7, EVENT_OPERAND_BYTE(9));
    D_800B02CC[D_800B2078.unk2384].unk62.vz = 0;
    D_800AFC7C += 4;
    D_800B0078->pc += 10;
}

/* Event: set the current emitter record's bytes +6a..+6c and +6e..+70 from
 * the selected operands 1..11 (flags byte 13); four batch steps. */
void func_80089880(void) {
    D_800B02CC[D_800B2078.unk2384].unk6A = func_8009CF78(1, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6B = func_8009CFBC(3, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6C = func_8009D000(5, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6E = func_8009D044(7, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk6F = func_8009D088(9, EVENT_OPERAND_BYTE(0xD));
    D_800B02CC[D_800B2078.unk2384].unk70 = func_8009D0CC(0xB, EVENT_OPERAND_BYTE(0xD));
    D_800AFC7C += 4;
    D_800B0078->pc += 0xE;
}

/* Event: use four batch steps and, unless 800adb8c is set, 800a99a8 for the
 * current actor. */
void func_80089A80(void) {
    D_800AFC7C += 4;
    if (D_800ADB8C == 0) {
        func_800A99A8(D_800AFD1C);
    }
    D_800B0078->pc++;
}

/* Event: use four batch steps and run 800a98e8 for the current actor with
 * its byte operand. */
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

/* Event: set 800b22e0 from operand 1. */
void func_80089F18(void) {
    D_800B2078.unk22E0 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set 800b21d2 to operand 1 less 0x80. */
void func_80089F54(void) {
    D_800B2078.piece_drift_mode = func_800ACDEC(1) - 0x80;
    D_800B0078->pc += 3;
}

/* Event: store operand byte 1 in 800afe84. */
void func_80089F94(void) {
    FieldActor *actor;

    actor = D_800B0078;
    D_800AFE84 = D_800ADC00[actor->pc + 1];
    actor->pc += 2;
}

/* Event: set the eight halfwords at 800b0080 from raw operands (+88
 * cleared; +84 at least 1). */
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

/* Event: set 800b0090, 800b0098 and 800b0094 from operands 1, 3 and 5
 * (immediate by flags 0x80/0x40/0x20 of byte 7). */
void func_8008A08C(void) {
    D_800B0080.unk90 = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0080.unk98 = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0080.unk94 = func_8009D000(5, D_800ADC00[D_800B0078->pc + 7]);
    D_800B0078->pc += 8;
}

/* Event: set the object parameters at 800b00a0 from twelve operands and
 * enable it (800b00b2). */
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

/* Event: store 800b06a0 in variable op1. */
void func_8008A2A0(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B06A0);
    D_800B0078->pc += 3;
}

/* Event 0x77: once the display is idle, load TIM file 0x7fb + op5 (mode 0),
 * upload it to VRAM at op4/op6 with CLUT row op8 + 0xe8 (mode 1) or free
 * it (other modes). */
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

void func_8008A4E0(void) {
}

void func_8008A4E8(void) {
}

void func_8008A4F0(void) {
}

void func_8008A4F8(void) {
}

void func_8008A500(void) {
}

void func_8008A508(void) {
}

void func_8008A510(void) {
}

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

/* Event: call 8003633c(0) when its byte operand is zero. */
void func_8008A5A0(void) {
    if (D_800ADC00[D_800B0078->pc + 1] == 0) {
        func_8003633C(0);
    }
    D_800B0078->pc++;
}

/* Event: set 800b21d4 from operand 1. */
void func_8008A604(void) {
    D_800B2078.unk21D4 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set character op3's slot byte +4 to op1 less its byte +3 (at
 * least 0). */
void func_8008A640(void) {
    s32 character = func_8008CF3C(func_800ACDEC(3));
    s32 value;

    if (character != 0xFF) {
        value = func_800ACDEC(1) - D_8005A39C->characters[character].unk77;
        if (value < 0) {
            value = 0;
        }
        D_8005A39C->characters[character].unk78 = value;
    }
    D_800B0078->pc += 5;
}

/* Event: store character op3's slot bytes +3 and +4 summed (0 for none) in
 * variable op1. */
void func_8008A6E0(void) {
    s32 character = func_8008CF3C(func_800ACDEC(3));
    s32 sum;

    if (character != 0xFF) {
        sum = D_8005A39C->characters[character].unk77 + D_8005A39C->characters[character].unk78;
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

/* Event: set actor field EA to the complement of operand byte 1. */
void func_8008A93C(void) {
    FieldActor *actor;

    actor = D_800B0078;
    actor->unk0EA = ~D_800ADC00[actor->pc + 1];
    actor->pc += 2;
}

/* Event: as 8008a93c, and clear the actor's layer bit 16. */
void func_8008A974(void) {
    func_8008A93C();
    D_800B0078->layer_flags &= ~0x10000;
}

/* Event: once the stream is stopped, hand the actor's block to its model
 * (80021bf0) and continue; otherwise wait. Yields either way. */
void func_8008A9AC(void) {
    if (func_8008A558() == 0) {
        D_800ADB90 = 0;
        func_80021BF0(D_800AF880.components.descriptors[D_800AFD1C].model, D_800B0078->unk120);
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

/* Event 0xb0: load wave-bank file op4 into slot op2 (releasing the old bank)
 * or, with mode byte 1, register the loaded bank; waits while the display
 * is busy. */
void func_8008AACC(void) {
    s32 file;
    s32 slot;
    void *data;

    if (func_8008A558() == 0) {
        if (EVENT_OPERAND_BYTE(1) == 1) {
            D_80062518[D_800AFD18] = func_80037FD8(D_800AFD08, 0);
            func_8003BDFC(0x10);
            func_800320E8(D_800AFD08);
            if (D_800AFD18 == 3) {
                D_800595AC = D_80062524;
            }
            D_800B00C0 = 1;
            D_800B0078->pc += 2;
        } else {
            slot = func_800ACDEC(2);
            D_800AFD18 = slot;
            func_80038310(D_80062518[slot]);
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

/* Event: set entry op1 of the 800b221c triples from operands 3, 5 and 7
 * (immediate by flags of byte 9). */
void func_8008AEC8(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);

    D_800B2078.unk221C[index][0] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk221C[index][1] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk221C[index][2] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event: set column op1 of the 800b223c table from operands 3, 5 and 7
 * (immediate by flags of byte 9). */
void func_8008AFD8(void) {
    s32 index = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 9]);

    D_800B2078.unk223C[0][index] = func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk223C[1][index] = func_8009D000(5, D_800ADC00[D_800B0078->pc + 9]);
    D_800B2078.unk223C[2][index] = func_8009D044(7, D_800ADC00[D_800B0078->pc + 9]);
    D_800B0078->pc += 10;
}

/* Event: set the three bytes at 800b225c from operands 1, 3 and 5. */
void func_8008B0E8(void) {
    D_800B2078.unk225C[0] = func_800ACDEC(1);
    D_800B2078.unk225C[1] = func_800ACDEC(3);
    D_800B2078.unk225C[2] = func_800ACDEC(5);
    D_800B0078->pc += 7;
}

/* Event: set 800b21b4 from operand 1. */
void func_8008B144(void) {
    D_800B2078.unk21B4 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: when the 801e module is loaded, pass (op1, op3) to 801e8330 and
 * keep op3 in the table at 800b21e4. */
void func_8008B180(void) {
    s32 index;

    if (D_800ADB1C != 0) {
        index = func_800ACDEC(1) & 0xFFFF;
        func_801E8330(index, 0, func_800ACDEC(3));
        D_800B2078.unk21E4[func_800ACDEC(1)] = func_800ACDEC(3);
    }
    D_800B0078->pc += 5;
}

/* Event: set the actor's +11e from operand 1. */
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

/* Event: run 800a484c(0) and skip fourteen operand bytes. */
void func_8008B2F0(void) {
    func_800A484C(0);
    D_800B0078->pc += 15;
}

/* Event: 801e effect control by selector byte: start with operand 2 (0),
 * wait for 800b2078 to clear (1), clear it (2) or stop (3); yields. */
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

#ifdef NON_MATCHING
/* Event 0x1b: scroll the current actor's textured polygons by (op1, op3)
 * texels in both draw buffers. */
void func_8008B5D4(void) {
    FieldInstance *instance;
    POLY_FT3 *ft3;
    POLY_FT3 *ft3_other;
    POLY_FT4 *ft4;
    POLY_FT4 *ft4_other;
    u8 *prims;
    u8 *other;
    FieldMesh *mesh;
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
    group = mesh->groups;
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
                    group += 2;
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
                    group += 2;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_8008B5D4);
#endif

/* Event: once the disc is idle, stop the stream, decode the pending party
 * sprite into its block, release the buffer and apply it; wait otherwise. */
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

    D_8005A39C->unk1D30 |= 1 << D_800ADBC8;
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
                D_8005A39C->unk1D30 |= 1 << D_800ADBC8;
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

/* Event: once idle, add character op1 to the party (a free slot starts its
 * sprite load) or mark it waiting (+1d30); yields while busy. */
void func_8008BC80(void) {
    s32 pending = D_800ADBC4;
    s32 member;
    s32 slot;

    if (pending == 0xFF && D_800ADB2C == 0 && func_8008A558() == 0) {
        func_80028A60(0);
        member = func_800ACDEC(1);
        if (member != pending) {
            if (func_8008A790(member, &slot) == 0) {
                D_8005A39C->unk22B1[slot] = 0;
                D_80062590[slot] = member;
                func_8008A7DC(member, slot);
                D_800B0078->pc += 3;
                return;
            }
            D_8005A39C->unk1D30 |= 1 << member;
            D_800B0078->pc += 5;
            return;
        }
        D_800B0078->pc += 5;
        return;
    }
    D_800B00C0 = 1;
    D_800B0078->pc--;
}

/* Event: once idle, add the character in its byte operand to the party (a
 * free slot starts its sprite load) or mark it waiting; yields while busy. */
void func_8008BDD8(void) {
    s32 slot;
    s32 member;

    if (D_800ADBC4 == 0xFF && D_800ADB2C == 0 && func_8008A558() == 0) {
        func_80028A60(0);
        if (func_8008A790(D_800ADC00[D_800B0078->pc + 1], &slot) == 0) {
            D_8005A39C->unk22B1[slot] = 0;
            member = D_800ADC00[D_800B0078->pc + 1];
            D_80062590[slot] = member;
            func_8008A7DC(member, slot);
            D_800B0078->pc += 2;
            return;
        }
        D_8005A39C->unk1D30 |= 1 << D_800ADC00[D_800B0078->pc + 1];
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

#ifdef NON_MATCHING
/* Take party slot `slot`'s member out of the party: its actor gets the
 * lead's sprite and is hidden, and the slot's ids are cleared. */
void func_8008C180(s32 slot) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    FieldActor *member;
    s32 current;
    u16 pc;
    s32 index;

    if (D_8005A444[slot] != 0xFF) {
        descriptor = D_800B06B8;
        actor = D_800B0078;
        current = D_800AFD1C;
        pc = actor->pc;
        D_800B06B8 = &D_800AF880.components.descriptors[D_8005A444[slot]];
        D_800B0078 = D_800B06B8->actor;
        func_80080A74(D_8005A444[slot]);
        index = D_8005A444[slot];
        D_800AFD1C = index;
        D_800AF880.components.descriptors[index].flags = (D_800AF880.components.descriptors[index].flags & 0xF07F) | 0x200;
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
    }
    D_80062590[slot] = 0xFF;
    D_8006FABC[slot] = 0xFF;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_8008C180);
#endif

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
                    D_8005A39C->unk22B1[0] = 0;
                } else {
                    *(PartySprite *)D_8005A414[0] = *(PartySprite *)D_8005A414[1];
                    D_80062590[0] = D_80062590[1];
                    D_8006FABC[0] = D_8006FABC[1];
                    D_80062590[1] = 0xFF;
                    D_8006FABC[1] = 0xFF;
                    D_8005A39C->unk22B1[0] = D_8005A39C->unk22B1[1];
                    if (D_80062590[2] != 0xFF) {
                        *(PartySprite *)D_8005A414[1] = *(PartySprite *)D_8005A414[2];
                        D_80062590[1] = D_80062590[2];
                        D_8006FABC[1] = D_8006FABC[2];
                        D_80062590[2] = 0xFF;
                        D_8006FABC[2] = 0xFF;
                        D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                    }
                }
                break;
            case 1:
                if (D_80062590[2] == 0xFF) {
                    D_80062590[1] = 0xFF;
                    D_8006FABC[1] = 0xFF;
                    D_8005A39C->unk22B1[1] = 0;
                } else {
                    *(PartySprite *)D_8005A414[1] = *(PartySprite *)D_8005A414[2];
                    D_80062590[1] = D_80062590[2];
                    D_8006FABC[1] = D_8006FABC[2];
                    D_80062590[2] = 0xFF;
                    D_8006FABC[2] = 0xFF;
                    D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                    D_8005A39C->unk22B1[2] = 0;
                }
                break;
            case 2:
                D_80062590[2] = 0xFF;
                D_8006FABC[2] = 0xFF;
                D_8005A39C->unk22B1[2] = 0;
                break;
            }
        } else {
            switch (slot) {
            case 0:
                D_8005A39C->unk22B1[0] = D_8005A39C->unk22B1[1];
                D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                D_8005A39C->unk22B1[2] = 0;
                func_8008C180(0);
                func_8008BF38(0);
                func_8008BF38(1);
                break;
            case 1:
                D_8005A39C->unk22B1[1] = D_8005A39C->unk22B1[2];
                D_8005A39C->unk22B1[2] = 0;
                func_8008C180(1);
                func_8008BF38(1);
                break;
            case 2:
                D_8005A39C->unk22B1[2] = 0;
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

/* Event: release the actor's block at +114 when flag 0x1000 of +12c says it
 * holds one. */
void func_8008C7D8(void) {
    if (D_800B0078->state.word & 0x1000) {
        func_800320E8(D_800B0078->unk114);
        D_800B0078->state.word &= ~0x1000;
    }
    D_800B0078->pc++;
}

/* Event: with the sequence playing, pass op1/op3 to 8003a89c; wait while a
 * sound is still loading into the 801e module. */
void func_8008C84C(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_800ACDEC(1);
        func_8003A89C(D_80062528, a, func_800ACDEC(3));
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

/* Event: as 8008c84c through 8003a948 with selected operands 1 and 3. */
void func_8008C938(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 5]);
        func_8003A948(D_80062528, a, func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 5]));
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

/* Event: as 8008c84c through 8003a838. */
void func_8008CA60(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_800ACDEC(1);
        func_8003A838(D_80062528, a, func_800ACDEC(3));
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

/* Event: as 8008c938 through 8003a9bc. */
void func_8008CB4C(void) {
    s32 a;

    if (D_8004F36C != 0) {
        a = func_8009CF78(1, D_800ADC00[D_800B0078->pc + 5]);
        func_8003A9BC(D_80062528, a, func_8009CFBC(3, D_800ADC00[D_800B0078->pc + 5]));
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

/* Event: as 8008c84c through 8003aac4 with one operand. */
void func_8008CC74(void) {
    if (D_8004F36C != 0) {
        func_8003AAC4(D_80062528, func_800ACDEC(1));
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

/* Event: set the actor's sound (op1, op3) with mode 0, stopping its current
 * one; a zero sound turns the mode off. */
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

/* Event: as 8008cd48 with mode 0x80. */
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

/* Event: clear party member op1's bit of the game's +1d32. */
void func_8008CE64(void) {
    s32 member = func_8008CF3C(func_800ACDEC(1));

    if (member != 0xFF) {
        D_8005A39C->unk1D32 &= ~(1 << member);
    }
    D_800B0078->pc += 3;
}

/* Event: set party member op1's bit of the game's +1d32. */
void func_8008CED0(void) {
    s32 member = func_8008CF3C(func_800ACDEC(1));

    if (member != 0xFF) {
        D_8005A39C->unk1D32 |= 1 << member;
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

/* Event: set the actor's character (+80) from operand 1. */
void func_8008CF9C(void) {
    D_800B0078->character = func_8008CF3C(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

/* Event: set the six halfwords at 800b21a0 from raw operands. */
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

    D_800AF880.components.descriptors[D_800AFD1C].model->unk2C = (u32)(scale * 3) >> 2;
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

    D_800AF880.components.descriptors[D_800AFD1C].model->unk2C = 0xC00;
    D_800B0078->scale[0] = x;
    D_800B0078->scale[1] = y;
    D_800B0078->scale[2] = z;
    func_80072254(D_800AFD1C);
    D_800B0078->pc += 7;
}

/* Event: set 800b218c from operand 1. */
void func_8008D230(void) {
    D_800B2078.scale = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Event: set the current model's +82 to twice operand 1. */
void func_8008D26C(void) {
    D_800AF880.components.descriptors[D_800AFD1C].model->unk82 = func_800ACDEC(1) * 2;
    D_800B0078->pc += 3;
}

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
    D_800AF880.components.descriptors[to].model->unk84 = D_800AF880.components.descriptors[from].model->unk84;
    D_800AF880.components.descriptors[to].model->position[0] = D_800AF880.components.descriptors[from].model->position[0];
    D_800AF880.components.descriptors[to].model->position[1] = D_800AF880.components.descriptors[from].model->position[1];
    D_800AF880.components.descriptors[to].model->position[2] = D_800AF880.components.descriptors[from].model->position[2];
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

/* Event: set 800b21cd from its byte operand. */
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

/* Event: continue when flag bit op1 is set, else jump to op3. */
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

/* Event: set 800b233e to the selected actor. */
void func_8008DB2C(void) {
    D_800B2078.unk233E = func_8009CDB4(1);
    D_800B0078->pc += 2;
}

/* Add to party member `member`'s points, capped at its maximum. Declared
 * int but returns nothing. */
s32 func_8008DB68(s32 member, s32 amount) {
    s32 character = func_8001ACF0(D_80062590[member]);

    if (character != 0xFF) {
        D_8005A39C->gears[character].points += amount;
        if (D_8005A39C->gears[character].points_max < D_8005A39C->gears[character].points) {
            D_8005A39C->gears[character].points = D_8005A39C->gears[character].points_max;
        }
    }
}

/* Take from party member `member`'s points, leaving at least one. Declared
 * int but returns nothing. */
s32 func_8008DBF0(s32 member, s32 amount) {
    s32 character = func_8001ACF0(D_80062590[member]);
    s32 points;

    if (character != 0xFF) {
        points = D_8005A39C->gears[character].points - amount;
        if (points <= 0) {
            points = 1;
        }
        D_8005A39C->gears[character].points = points;
    }
}

/* Event: add operand 1 to the points of the party members the byte-3 mask
 * names. */
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

/* Event: take operand 1 from the points of the party members the byte-3
 * mask names. */
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

/* Event: set the actor's +75 to the selected actor, if any. */
void func_8008DE64(void) {
    s32 actor = func_8009CDB4(1);

    if (actor != 0xFF) {
        D_800B0078->unk075 = actor;
    }
    D_800B0078->pc += 2;
}

/* Event: store the selected actor's flags in variable op1. */
void func_8008DEBC(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, actor->flags);
    }
    D_800B0078->pc += 3;
}

/* Event: store the selected actor's flag halfword 1 in variable op1. */
void func_8008DF44(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, ACTOR_FLAG_HALF(actor, 1));
    }
    D_800B0078->pc += 3;
}

/* Event: store the selected actor's layer flags in variable op1. */
void func_8008DFCC(void) {
    s32 index = func_8009CDB4(1);
    FieldActor *actor;

    if (index != 0xFF) {
        actor = D_800AF880.components.descriptors[index].actor;
        func_800A3074(func_800ACDB8(1) & 0xFFFF, actor->layer_flags);
    }
    D_800B0078->pc += 3;
}

/* Event: store the selected actor's flag halfword 3 in variable op1. */
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

/* Event: test the selected actor's flag halfword 0 (8008e0dc). */
void func_8008E298(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 0));
}

/* Event: test the selected actor's flag halfword 1 (8008e0dc). */
void func_8008E2EC(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 1));
}

/* Event: test the selected actor's flag halfword 2 (8008e0dc). */
void func_8008E340(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 2));
}

/* Event: test the selected actor's flag halfword 3 (8008e0dc). */
void func_8008E394(void) {
    func_8008E0DC(ACTOR_FLAG_HALF(D_800AF880.components.descriptors[func_8009CDB4(3)].actor, 3));
}

/* Event: test the current actor's flag halfword 0 (8008e148). */
void func_8008E3E8(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 0));
}

/* Event: test the current actor's flag halfword 1 (8008e148). */
void func_8008E414(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 1));
}

/* Event: test the current actor's flag halfword 2 (8008e148). */
void func_8008E440(void) {
    func_8008E148(ACTOR_FLAG_HALF(D_800B0078, 2));
}

/* Event: test the current actor's flag halfword 3 (8008e148). */
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

/* Event: set 800b2298 and the 800b229c count (at most 32) from operands 1
 * and 3, then apply them (8008e718). */
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
    FieldModel *model;

    switch (D_800ADC00[D_800B0078->pc + 1]) {
    case 0:
        if (D_800B0078->flags & 0x8000) {
            D_800B0078->flags &= ~0x8000;
        }
        if (D_800B0078->layer_flags & 0x80000) {
            model = D_800AF880.components.descriptors[D_800AFD1C].model;
            model->unk18 = 0;
            model->velocity[2] = 0;
            model->velocity[0] = 0;
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

/* Event: wait for 800adb7c, clearing it once seen; yield each time. */
void func_8008E9F8(void) {
    if (D_800ADB7C == 0) {
        D_800B0078->pc--;
    } else {
        D_800ADB7C = 0;
        D_800B0078->pc++;
    }
    D_800B00C0 = 1;
}

/* Event 0xa0: once sound is available, request a movie: file op1, the
 * parameters at 800c3a2a/2c/2e from op3/op5/op7 and its sound bank op9
 * (selected operands, flags byte 11), with the default window and fade. */
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

#ifdef NON_MATCHING
/* Event 0x60: once sound is available, request movie op1 with parameters
 * op3/op5; op7's low nibble picks the display layout (0: half-width at
 * x 0x140, 1: full 16-bit, 2: full 24-bit) and its 0xc0 bits the fade. */
void func_8008EC30(void) {
    s32 mode;
    s32 layout;

    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        D_800B0078->pc -= 1;
        return;
    }
    FIELD_MOVIE.file = func_800ACDEC(1);
    FIELD_MOVIE.unk2A = func_800ACDEC(3);
    FIELD_MOVIE.unk2E = func_800ACDEC(5);
    mode = func_800ACDEC(7);
    FIELD_MOVIE.mode = mode;
    D_800ADB80 = mode & 0xC0;
    FIELD_MOVIE.width = 0x140;
    FIELD_MOVIE.height = 0x100;
    FIELD_MOVIE.mode &= 0xF;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_8008EC30);
#endif

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

/* Event: request transition 1 with operand 1 (800adb38/800adb3c). */
void func_8008EF5C(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 1;
    D_800B0078->pc += 3;
}

/* Event: request transition 2 with operand 1 (800adb38/800adb3c). */
void func_8008EFA0(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 2;
    D_800B0078->pc += 3;
}

/* Event: set both draw buffers' clip areas from four operands. */
void func_8008EFE4(void) {
    s32 x = func_800ACDEC(1);
    s32 y = func_800ACDEC(3);
    s32 w = func_800ACDEC(5);

    func_80071F64(x, y, w, func_800ACDEC(7));
    D_800B0078->pc += 9;
}

extern s32 D_800ADB38;
extern s32 D_800ADB3C;

/* Store an operand in D_800ADB3C and set D_800ADB38 to 3. */
void func_8008F070(void) {
    D_800ADB3C = func_800ACDEC(1);
    D_800ADB38 = 3;
    D_800B0078->pc += 3;
}

/* Set a selected actor's colour triples; mode bits 1/2 select each triple. */
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

/* Set the current actor's colour triples; mode bits 1/2 select each triple. */
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

void func_80023290(FieldModel *model, s32 value);

/* Pass an operand to resident 80023290 with the current descriptor's word 04. */
void func_8008F2D8(void) {
    s32 value = func_800ACDEC(1);

    func_80023290(D_800AF880.components.descriptors[D_800AFD1C].model, value);
    D_800B0078->pc += 3;
}

extern s32 D_800C3A5C;
extern s32 D_800C3A60;

/* Store two operands in D_800C3A5C/D_800C3A60. */
void func_8008F348(void) {
    D_800C3A5C = func_800ACDEC(1);
    D_800C3A60 = func_800ACDEC(3);
    D_800B0078->pc += 5;
}

/* Set which field effects are kept across a reload. */
void func_8008F394(void) {
    D_800B2078.effects_kept = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

void func_8003A450(s32 a, s32 b, s32 c);

/* Resident 8003A450(2 * op3, op1, op5). */
void func_8008F3D0(void) {
    s32 a;
    s32 b;

    a = func_800ACDEC(3) * 2;
    b = func_800ACDEC(1);
    func_8003A450(a, b, func_800ACDEC(5));
    D_800B0078->pc += 7;
}

void func_8003A344(s32 a, s32 b);

/* Resident 8003A344(2 * op3, op1). */
void func_8008F444(void) {
    s32 a;

    a = func_800ACDEC(3) * 2;
    func_8003A344(a, func_800ACDEC(1));
    D_800B0078->pc += 5;
}

void func_8003A55C(s32 a, s32 b);

/* Resident 8003A55C(2 * op3, op1). */
void func_8008F4A0(void) {
    s32 a;

    a = func_800ACDEC(3) * 2;
    func_8003A55C(a, func_800ACDEC(1));
    D_800B0078->pc += 5;
}

/* Field 80085634(op1, op3). */
void func_8008F4FC(void) {
    s32 a;

    a = func_800ACDEC(1);
    func_80085634(a, func_800ACDEC(3));
    D_800B0078->pc += 5;
}

void func_800855C8(s32 a, s32 b, s32 c, s32 d);

/* Field 800855C8(op1, op5, op3, op7). */
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

s32 func_8003A5D0(s32 mask);

/* Re-run this opcode each frame while any operand button (<< 8) is held. */
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

/* Field 80085634(op1, 3). */
void func_8008F668(void) {
    func_80085634(func_800ACDEC(1), 3);
    D_800B0078->pc += 3;
}

/* Field 800855C8(op1, op5, op3, 3). */
void func_8008F6AC(void) {
    s32 a;
    s32 b;

    a = func_800ACDEC(1);
    b = func_800ACDEC(5);
    func_800855C8(a, b, func_800ACDEC(3), 3);
    D_800B0078->pc += 7;
}

extern s32 D_800ADBDC;
extern s32 D_8004F340;
void func_8008F7B8(void);

/* Request field music with D_8004F340 = 0, or yield while music is disabled. */
void func_8008F724(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        return;
    }
    D_8004F340 = 0;
    func_8008F7B8();
}

/* Request field music with D_8004F340 = -1, or yield while music is disabled. */
void func_8008F76C(void) {
    if (D_800ADBDC == 0) {
        D_800B00C0 = 1;
        return;
    }
    D_8004F340 = -1;
    func_8008F7B8();
}

extern s32 D_8004F308;
extern s32 D_8004F324;
extern s32 D_8004F354;
void func_80085EEC(void);
void func_8001B66C(void);
s32 func_8008A558(void);

#ifdef NON_MATCHING
/* Select the field music track (operand 1). Without D_800ADB1C the track is
 * only recorded; otherwise yield until the music system can take a change.
 * NON_MATCHING: the original places the second yield after the change branch,
 * storing the comparison's constant 1. */
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
    } else if (D_8004F354 == 1 || D_8004F308 == -1) {
        D_800B00C0 = 1;
    } else {
        if (track != D_8004F324) {
            func_8001B66C();
            D_8004F324 = track;
            D_8004F308 = -1;
            func_80085B20(track, 0);
        }
        D_800B0078->pc += 3;
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_8008F7B8);
#endif

/* Start a camera shake toward three amplitudes over a frame count. */
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

/* Yield; advance once the requested camera moves (bit 1: target, bit 0: eye)
 * have no steps left. */
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

/* Set the camera heading from a selected operand. */
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


/* Leave the scripted camera: mode 0 just clears the hold flag; mode 1 either
 * ends at once (operand 0, also skipping the next opcode) or blends back over
 * the operand's frame count (mode 2). */
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

/* Set the scripted camera's two blend frame counts (at least 1). */
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

/* Set the saved target from three selected operands (whole units). */
void func_8008FE2C(void) {
    D_800AF880.saved_target.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_target.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_target.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Point A follows a selected actor's position. */
void func_8008FF04(void) {
    FieldActor *actor;

    actor = D_800AF880.components.descriptors[func_8009CD7C(1)].actor;
    D_800AF880.point_actor_a.vx = actor->position[0];
    D_800AF880.point_actor_a.vy = actor->position[1];
    D_800AF880.point_actor_a.vz = actor->position[2];
    D_800AFC7C += 1;
    D_800B0078->pc += 2;
}

/* Set point A from three selected operands (whole units). */
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

/* Set the saved eye from three selected operands (whole units). */
void func_800900C4(void) {
    D_800AF880.saved_eye.vx = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_eye.vz = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.saved_eye.vy = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AFC7C += 1;
    D_800B0078->pc += 8;
}

/* Point B follows a selected actor's position. */
void func_8009019C(void) {
    FieldActor *actor;

    actor = D_800AF880.components.descriptors[func_8009CD7C(1)].actor;
    D_800AF880.point_actor_b.vx = actor->position[0];
    D_800AF880.point_actor_b.vy = actor->position[1];
    D_800AF880.point_actor_b.vz = actor->position[2];
    D_800AFC7C += 1;
    D_800B0078->pc += 2;
}

/* Set point B from three selected operands (whole units). */
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

#ifdef NON_MATCHING
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
        D_800AF880.scripted_target.vx = D_800AF880.saved_target.vx;
        D_800AF880.scripted_target.vy = D_800AF880.saved_target.vy;
        D_800AF880.scripted_target.vz = D_800AF880.saved_target.vz;
        D_800AF880.scripted |= 1;
        D_800AF880.target_step.vx = (D_800AF880.point_actor_a.vx - D_800AF880.saved_target.vx) / D_800AF880.target_steps;
        D_800AF880.target_step.vy = (D_800AF880.point_actor_a.vy - D_800AF880.saved_target.vy) / D_800AF880.target_steps;
        D_800AF880.target_step.vz = (D_800AF880.point_actor_a.vz - D_800AF880.saved_target.vz) / D_800AF880.target_steps;
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
        D_800AF880.scripted_target.vx = D_800AF880.saved_target.vx;
        D_800AF880.scripted_target.vy = D_800AF880.saved_target.vy;
        D_800AF880.scripted_target.vz = D_800AF880.saved_target.vz;
        D_800AF880.scripted |= 1;
        D_800AF880.target_step.vx = -(direction.vx * speed) * 16;
        D_800AF880.target_step.vy = -(direction.vy * speed) * 16;
        D_800AF880.target_step.vz = -(direction.vz * speed) * 16;
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
        D_800AF880.scripted_eye[0] = D_800AF880.saved_eye.vx;
        D_800AF880.scripted_eye[1] = D_800AF880.saved_eye.vy;
        D_800AF880.scripted_eye[2] = D_800AF880.saved_eye.vz;
        D_800AF880.scripted |= 2;
        D_800AF880.eye_step[0] = -(direction.vx * speed) * 16;
        D_800AF880.eye_step[1] = -(direction.vy * speed) * 16;
        D_800AF880.eye_step[2] = -(direction.vz * speed) * 16;
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
        D_800AF880.scripted_eye[0] = D_800AF880.saved_eye.vx;
        D_800AF880.scripted_eye[1] = D_800AF880.saved_eye.vy;
        D_800AF880.scripted_eye[2] = D_800AF880.saved_eye.vz;
        D_800AF880.scripted |= 2;
        D_800AF880.eye_step[0] = (D_800AF880.point_actor_b.vx - D_800AF880.saved_eye.vx) / D_800AF880.eye_steps;
        D_800AF880.eye_step[1] = (D_800AF880.point_actor_b.vy - D_800AF880.saved_eye.vy) / D_800AF880.eye_steps;
        D_800AF880.eye_step[2] = (D_800AF880.point_actor_b.vz - D_800AF880.saved_eye.vz) / D_800AF880.eye_steps;
        if (EVENT_OPERAND_BYTE(1) & 0x80) {
            D_800AF880.eye.vx = D_800AF880.saved_eye.vx;
            D_800AF880.eye.vy = D_800AF880.saved_eye.vy;
            D_800AF880.eye.vz = D_800AF880.saved_eye.vz;
        }
        break;
    }
    D_800B0078->pc += 4;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_800903BC);
#endif

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

/* Store the target goal's whole x, z, y in three variables. */
void func_80090B18(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, WHOLE(D_800AF880.target_goal.vx));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, WHOLE(D_800AF880.target_goal.vz));
    func_800A3074(func_800ACDB8(5) & 0xFFFF, WHOLE(D_800AF880.target_goal.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Store the eye goal's whole x, z, y in three variables. */
void func_80090B9C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, WHOLE(D_800AF880.eye_goal.vx));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, WHOLE(D_800AF880.eye_goal.vz));
    func_800A3074(func_800ACDB8(5) & 0xFFFF, WHOLE(D_800AF880.eye_goal.vy));
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}

/* Mode 0: store the scripted heading in a variable; otherwise set it from
 * the raw operand. */
void func_80090C20(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_heading);
    } else {
        D_800AF880.scripted_heading = func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Mode 0: store the scripted elevation in a variable; otherwise set it from
 * the raw operand. */
void func_80090CB8(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_elevation);
    } else {
        D_800AF880.scripted_elevation = func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Mode 0: store the scripted zoom in a variable; otherwise set it from the
 * raw operand. */
void func_80090D50(void) {
    if (EVENT_OPERAND_BYTE(3) == 0) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_zoom);
    } else {
        D_800AF880.scripted_zoom = (u16)func_800ACDB8(1);
    }
    D_800AFC7C += 1;
    D_800B0078->pc += 4;
}

/* Store the scripted heading, elevation and zoom in three variables. */
void func_80090DEC(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.scripted_heading);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_800AF880.scripted_elevation);
    func_800A3074(func_800ACDB8(5) & 0xFFFF, D_800AF880.scripted_zoom);
    D_800AFC7C += 1;
    D_800B0078->pc += 7;
}


/* Derive a scripted heading, pitch and zoom from point A looking at point B
 * and store them in three variables. */
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

/* Place a point at a heading/elevation/distance from a centre given by
 * selected operands and store its whole x, z, y in three variables. */
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

/* As func_800910C0, around camera point operand 1 (saved target, point A,
 * saved eye, point B). */
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

/* Store camera point operand 1's whole x, z, y in three variables. */
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

/* Set two colour triples and two ranges from operands, then apply them. */
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

/* Set the four camera bounds from signed operands (the last negated). */
void func_80091A08(void) {
    D_800AF880.bounds[0] = func_800ACD7C(1);
    D_800AF880.bounds[1] = func_800ACD7C(3);
    D_800AF880.bounds[2] = func_800ACD7C(5);
    D_800AF880.bounds[3] = -func_800ACD7C(7);
    D_800B0078->pc += 9;
}


/* Set a colour triple from three operands. */
void func_80091A78(void) {
    D_800B2078.clear_color[0] = func_800ACDEC(1);
    D_800B2078.clear_color[1] = func_800ACDEC(3);
    D_800B2078.clear_color[2] = func_800ACDEC(5);
    D_800B0078->pc += 7;
}

void func_80091AD4(void) {
}

extern RECT D_800AF5E8[32];
extern s16 D_800AF6E8[32];
extern s16 D_800AF728[32];
extern u16 D_800AF768;

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

/* Set the current actor's two-bit mode (flags bits 5-6) and value EE from
 * selected operands. */
void func_80091E00(void) {
    D_800B0078->unk134 = (D_800B0078->unk134 & ~0x60) | ((func_8009CF78(1, EVENT_OPERAND_BYTE(5)) & 3) << 5);
    D_800B0078->unkEE = func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    D_800B0078->pc += 6;
}

/* As func_80091E00, for a selected actor. */
void func_80091E98(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->unk134 = (actor->unk134 & ~0x60) | ((func_8009CF78(2, EVENT_OPERAND_BYTE(6)) & 3) << 5);
        actor->unkEE = func_8009CFBC(4, EVENT_OPERAND_BYTE(6));
    }
    D_800B0078->pc += 7;
}

/* Store an operand (clamped to 0xFFF) in slot operand 1 of the current
 * actor's word table when its descriptor has flag 0x2000. */
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

/* Swap two variables. */
void func_80092044(void) {
    s32 first;
    s32 second;

    first = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    second = func_800A3018(func_800ACDB8(3) & 0xFFFF);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, first);
    func_800A3074(func_800ACDB8(1) & 0xFFFF, second);
    D_800B0078->pc += 5;
}

void func_80027EAC(s32 handle);

/* Pass every handle in the list at 800afea8 to resident 80027EAC. */
void func_800920D8(void) {
    s32 i;

    for (i = 0; i < D_800AFEA8.count; i++) {
        func_80027EAC(D_800AFEA8.handles[i]);
    }
}

typedef struct {
    u8 *data[32];
    s16 size[32];
} ByteTables;
extern ByteTables D_800AFF2C;

/* Store a raw operand byte at index operand 3 of table operand 1, within its
 * size. */
void func_80092148(void) {
    s32 table;
    s32 index;
    s32 value;

    table = func_800ACDEC(1);
    index = func_800ACDEC(3);
    value = func_800ACDB8(5) & 0xFFFF;
    if (index < D_800AFF2C.size[table]) {
        D_800AFF2C.data[table][index] = value;
    }
    D_800B0078->pc += 7;
}

extern s32 D_800ADB8C;
void *func_80031BDC(s32 size, s32 flags);
void func_80027D64(s32 handle, s16 x, s16 y, s16 width, s16 height, s16 length, s16 a, s16 b, u8 *buffer);

/* Open a new entry in the list at 800afea8: allocate its handle and a
 * filled buffer, then create it through resident 80027D64. */
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
        D_800AFEA8.handles[D_800AFEA8.count] = (s32)func_80031BDC(0x18, 0);
        fill = func_800ACDB8(15) & 0xFFFF;
        for (i = 0; i < length; i++) {
            D_800AFEA8.buffers[D_800AFEA8.count][i] = fill;
        }
        x = (s16)func_800ACDB8(1);
        y = (s16)func_800ACDB8(3);
        width = (s16)func_800ACDB8(5);
        height = (s16)func_800ACDB8(7);
        a = (s16)func_800ACDB8(11);
        func_80027D64(D_800AFEA8.handles[D_800AFEA8.count], x, y, width, height, length, a,
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


/* Select mode 0..2 (unk17C) and set the text speed to 8, 6 or 4. */
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


/* Set the input mask from a raw operand. */
void func_80092628(void) {
    D_800B2078.input_mask = func_800ACDB8(1);
    D_800B0078->pc += 3;
}

/* Set a collision attribute byte (operands: index, byte, value). */
void func_80092664(void) {
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), func_800ACDEC(3));
    D_800B0078->pc += 5;
}

/* OR a value into a collision attribute byte. */
void func_800926C8(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value |= func_800ACDEC(3);
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    D_800B0078->pc += 5;
}

/* AND a value into a collision attribute byte. */
void func_80092768(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    value &= func_800ACDEC(3);
    func_800924D4(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2), value);
    D_800B0078->pc += 5;
}

extern FieldDescriptor *D_800B06B8;

/* Give the current actor a step along the published descriptor's facing
 * and set its layer flag 0x800. */
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
    FieldModel *model;
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
    model->unk18 = 0x80000;
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
    model->unk18 = 0;
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

/* Walk the player to the selected x/z (800a0c4c/80092894) when the field
 * is idle, restoring its flag 0x80 on arrival; otherwise retry. */
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

/* Start transition 80092894(0, ...) once field control allows it; yield
 * until then. */
void func_80092DFC(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800A0C4C();
        func_80092894(0, 0, 0, 0);
    }
}

/* As func_80092DFC with first argument 0x3E0. */
void func_80092EA0(void) {
    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
    } else {
        func_800A0C4C();
        func_80092894(0x3E0, 0, 0, 0);
    }
}

extern s32 D_8004F34C;
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

/* When D_800ADBD8 is set, consume it and store an operand in D_800B0064. */
void func_80092FB4(void) {
    if (D_800ADBD8 != 0) {
        D_800B2078.encounter_inhibition = -1;
        D_800ADBD8 = 0;
        D_800B0064 = func_800ACDEC(1);
    }
    D_800B0078->pc += 3;
}

extern u8 D_800B02C8;

/* Request a map change (map, entry, heading or keep the camera's, and
 * operand 7) when the field is idle. Yields. */
void func_80093014(void) {
    s32 heading;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADB2C != 0 || D_8004F308 == -1 || D_800ADB90 != 0) {
        D_800B00C0 = 1;
        return;
    }
    func_800A31E8();
    D_800B2078.encounter_inhibition = -1;
    D_800ADBE4 = 0;
    D_8005A39C->unk231A = func_8009CF78(1, EVENT_OPERAND_BYTE(9));
    D_8005A39C->unk231E = func_8009CFBC(3, EVENT_OPERAND_BYTE(9));
    heading = func_8009D000(5, EVENT_OPERAND_BYTE(9));
    if (((heading & 0xFFFF) == 0xFFFF) | (heading == -1)) {
        D_8005A39C->unk231C = (D_800AF880.heading_angles.vy + 0x800) & 0xFFF;
    } else {
        D_8005A39C->unk231C = (heading + 0x800) & 0xFFF;
    }
    D_8005A39C->unk2320 = func_8009D044(7, EVENT_OPERAND_BYTE(9));
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

/* Once field control allows it, run func_800932D0 and store two operands. */
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

/* Request a field change (operands: field id, entry) once field control
 * allows it, then yield. */
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

extern u8 D_8005954C;
extern u8 D_80059508;
extern u8 D_800594F8;
extern s32 D_800ADBE0;
extern s32 D_800ADB88;
extern s32 D_800ADB18;

/* Leave the field for another module (operand 1), optionally requesting a
 * field change (operands 5, 7; 0x7FFF: none). Re-runs until allowed. */
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

/* Leave the field for another module (operand 1) once allowed. */
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

/* Store a collision attribute byte in a variable. */
void func_80093664(void) {
    s32 value;

    value = func_80092424(EVENT_OPERAND_BYTE(1), EVENT_OPERAND_BYTE(2));
    func_800A3074(func_800ACDB8(3) & 0xFFFF, value);
    D_800B0078->pc += 5;
}

extern s32 D_8004F350;

/* Yield; advance only once D_8004F350 is zero. */
void func_800936E4(void) {
    if (D_8004F350 == 0) {
        D_800B0078->pc += 1;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

extern u8 D_80059171;
extern s32 D_800ADB64;

/* Request field action 0 with parameter D_800B2078.unk236C. */
void func_80093740(void) {
    D_800B00C0 = 1;
    D_800ADB64 = 0;
    D_80059171 = D_800B2078.unk236C;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Request field action 6 with parameter 1. */
void func_80093790(void) {
    D_80059171 = 1;
    D_800ADB64 = 6;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Request field action 2. */
void func_800937E0(void) {
    D_800ADB64 = 2;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 1;
}

/* Request field action 3 with an operand parameter. */
void func_80093824(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 3;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Request a field change (operands: field id, entry) as field action 1. */
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

/* Field action 1 with an entry operand also stored in game state and
 * variable 2. */
void func_80093930(void) {
    s16 entry;

    entry = func_800ACDEC(1);
    D_800ADB64 = 1;
    D_800B00C0 = 1;
    D_8005A39C->vars[1] = entry;
    D_8005A39C->unk2320 = entry;
    D_800C3A68[1] = entry;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Request field action 4 with an operand parameter. */
void func_800939A0(void) {
    D_80059171 = func_800ACDEC(1);
    D_800ADB64 = 4;
    D_800B00C0 = 1;
    D_8004F350 += 1;
    D_800B0078->pc += 3;
}

/* Request field action 5 with an operand parameter. */
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

/* Load a bytecode table byte (table offset + index) into a variable. */
void func_80093CD0(void) {
    u16 offset;

    offset = func_800ACDB8(1);
    offset += func_800ACDEC(5);
    func_800A3074(func_800ACDB8(3) & 0xFFFF, D_800ADC00[offset]);
    D_800B0078->pc += 7;
}

/* Load a bytecode table halfword (unsigned when operand 7 is zero, else
 * signed) into a variable. */
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

/* Door-style swing: while flag 0x100000 is clear, turn the current
 * descriptor by 0x20 per frame (direction operand 1) for 31 frames, then set
 * the flag and advance. */
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

/* The reverse swing: while flag 0x100000 is set, turn back over 31 frames,
 * then clear it and advance. */
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

/* Event 0xe8: shake the actor for op3 frames (0x100000 set once done): op5
 * 0x1000/0x1001 moves the target down/up by op1 * 16, other values move it
 * op1 along direction op5 relative to the actor's heading. */
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

/* Event 0xe9: the reverse shake of 0xe8, run while flag 0x100000 is set
 * (cleared once op3 frames have passed); the direction offsets are negated
 * and the target is not reset first. */
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

extern s32 D_8004F318;
extern s32 D_8004F328;

/* Reset D_8004F318/D_8004F328 and store (op1 << 8 | op3) in variable 10. */
void func_800945D4(void) {
    s32 high;

    D_8004F318 = 0;
    D_8004F328 = 0xFF;
    high = func_800ACDEC(1);
    func_800A3074(10, ((high << 8) & 0xFF00) | (func_800ACDEC(3) & 0xFF));
    D_800B0078->pc += 5;
}

/* Set D_8004F328 from an operand byte. */
void func_80094650(void) {
    D_8004F328 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Reset D_8004F318 and D_8004F328. */
void func_8009468C(void) {
    D_8004F318 = 0;
    D_8004F328 = 0xFF;
    D_800B0078->pc += 1;
}

/* Set the current actor's two-bit mode (state) to 1 with value unk70. */
void func_800946BC(void) {
    D_800B0078->state.word = (D_800B0078->state.word & ~3) | 1;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Set the current actor's two-bit mode (state) to 2 with value unk70. */
void func_80094710(void) {
    D_800B0078->state.word = (D_800B0078->state.word & ~3) | 2;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Set the current actor's two-bit mode (state) to 3 with value unk70. */
void func_80094764(void) {
    D_800B0078->state.word |= 3;
    D_800B0078->unk70 = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Turn a selected actor's descriptor: operand 1 picks axis and sign
 * (0/1: x+/-, 2/3: y+/-, 4/5: z+/-). */
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

/* Set one rotation axis (operand 3) of the current descriptor. */
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

/* Turn the current descriptor about x by an operand and reapply it. */
void func_80094A5C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vx += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about x by minus an operand. */
void func_80094ACC(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vx -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about y by an operand. */
void func_80094B3C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about y by minus an operand. */
void func_80094BAC(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vy -= delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about z by an operand. */
void func_80094C1C(void) {
    s32 delta;

    delta = func_800ACDEC(1);
    D_800AF880.components.descriptors[D_800AFD1C].rotation.vz += delta;
    D_800B0078->pc += 3;
    func_80072254(D_800AFD1C);
}

/* Turn the current descriptor about z by minus an operand. */
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
        if (D_8005A39C->count0[i] == 0 || D_8005A39C->id0[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 1, or -1. */
s32 func_80094D4C(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->count1[i] == 0 || D_8005A39C->id1[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 2, or -1. */
s32 func_80094D9C(void) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (D_8005A39C->count2[i] == 0 || D_8005A39C->id2[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 3, or -1. */
s32 func_80094DEC(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->count3[i] == 0 || D_8005A39C->id3[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* First free slot of inventory list 4, or -1. */
s32 func_80094E3C(void) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->count4[i] == 0 || D_8005A39C->id4[i] == 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 0, or -1. */
s32 func_80094E8C(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->id0[i] == id && D_8005A39C->count0[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 2, or -1. */
s32 func_80094EDC(s32 id) {
    s32 i;

    for (i = 0; i < 200; i++) {
        if (D_8005A39C->id2[i] == id && D_8005A39C->count2[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 1, or -1. */
s32 func_80094F2C(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->id1[i] == id && D_8005A39C->count1[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 3, or -1. */
s32 func_80094F7C(s32 id) {
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_8005A39C->id3[i] == id && D_8005A39C->count3[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Slot of item `id` in inventory list 4, or -1. */
s32 func_80094FCC(s32 id) {
    s32 i;

    for (i = 0; i < 150; i++) {
        if (D_8005A39C->id4[i] == id && D_8005A39C->count4[i] != 0) {
            return i;
        }
    }
    return -1;
}

/* Id array of the inventory list selected by item >> 8. */
u8 *func_8009501C(s32 item) {
    switch (item >> 8) {
    case 0:
        return D_8005A39C->id0;
    case 1:
        return D_8005A39C->id1;
    case 2:
        return D_8005A39C->id2;
    case 3:
        return D_8005A39C->id3;
    case 4:
        return D_8005A39C->id4;
    }
    return NULL;
}

/* Count array of the inventory list selected by item >> 8. */
u8 *func_800950A0(s32 item) {
    switch (item >> 8) {
    case 0:
        return D_8005A39C->count0;
    case 1:
        return D_8005A39C->count1;
    case 2:
        return D_8005A39C->count2;
    case 3:
        return D_8005A39C->count3;
    case 4:
        return D_8005A39C->count4;
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

/* Stop the current actor: clear its motion words and its model's, mark
 * 0x8000 in unk104/unk106, and yield. */
void func_80095284(void) {
    FieldModel *model;
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
    model->velocity[0] = 0;
    model->velocity[2] = 0;
    model->unk18 = 0;
}

/* Set the terrain angle from an operand. */
void func_80095300(void) {
    D_800B2078.terrain_angle = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

#ifdef NON_MATCHING
/* Call (operand 2) when the controlled actor stands inside trigger zone
 * operand 1 and the call stack has room; otherwise skip.
 * NON_MATCHING: the original loads the zone number after computing the
 * actor's point (instruction scheduling). */
void func_8009533C(void) {
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    zone = &D_800ADBF4[EVENT_OPERAND_BYTE(1)];
    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    a = (zone->corner[0].z << 16) + zone->corner[0].x;
    b = (zone->corner[1].z << 16) + zone->corner[1].x;
    c = (zone->corner[2].z << 16) + zone->corner[2].x;
    d = (zone->corner[3].z << 16) + zone->corner[3].x;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_8009533C);
#endif

#ifdef NON_MATCHING
/* As func_8009533C, also requiring the zone's height within the
 * controlled actor's vertical extent.
 * NON_MATCHING: the original loads the zone number after the actor lookup
 * (instruction scheduling), as in 8009533c. */
void func_80095520(void) {
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    zone = &D_800ADBF4[EVENT_OPERAND_BYTE(1)];
    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    if (zone->corner[0].y < WHOLE(player->position[1]) &&
        WHOLE(player->position[1]) - (u16)player->height < zone->corner[0].y) {
        a = (zone->corner[0].z << 16) + zone->corner[0].x;
        b = (zone->corner[1].z << 16) + zone->corner[1].x;
        point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
        c = (zone->corner[2].z << 16) + zone->corner[2].x;
        d = (zone->corner[3].z << 16) + zone->corner[3].x;
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_80095520);
#endif

#ifdef NON_MATCHING
/* Continue when the controlled actor is inside trigger zone operand 1,
 * else jump to operand 2.
 * NON_MATCHING: the original loads the zone number after computing the
 * actor's point (instruction scheduling), as in 8009533c. */
void func_80095734(void) {
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    zone = &D_800ADBF4[D_800ADC00[D_800B0078->pc + 1]];
    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
    a = (zone->corner[0].z << 16) + zone->corner[0].x;
    b = (zone->corner[1].z << 16) + zone->corner[1].x;
    c = (zone->corner[2].z << 16) + zone->corner[2].x;
    d = (zone->corner[3].z << 16) + zone->corner[3].x;
    if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
        func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0) {
        D_800B0078->pc += 4;
        return;
    }
    D_800B0078->pc = func_800ACDB8(2);
    D_800AFC7C += 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_80095734);
#endif

#ifdef NON_MATCHING
/* Continue when the controlled actor is inside trigger zone operand 1 and
 * the zone's height lies within the actor's body, else jump to operand 2.
 * NON_MATCHING: the original loads the zone number after the actor lookup
 * (instruction scheduling), as in 8009533c. */
void func_800958C0(void) {
    FieldActor *player;
    Zone *zone;
    s32 point;
    s32 a;
    s32 b;
    s32 c;
    s32 d;

    zone = &D_800ADBF4[EVENT_OPERAND_BYTE(1)];
    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    if (zone->corner[0].y < WHOLE(player->position[1]) &&
        WHOLE(player->position[1]) - (u16)player->height < zone->corner[0].y) {
        a = (zone->corner[0].z << 16) + zone->corner[0].x;
        b = (zone->corner[1].z << 16) + zone->corner[1].x;
        point = (WHOLE(player->position[2]) << 16) + WHOLE(player->position[0]);
        c = (zone->corner[2].z << 16) + zone->corner[2].x;
        d = (zone->corner[3].z << 16) + zone->corner[3].x;
        if (func_8004A70C(a, b, point) >= 0 && func_8004A70C(b, c, point) >= 0 &&
            func_8004A70C(c, d, point) >= 0 && func_8004A70C(d, a, point) >= 0) {
            D_800B0078->pc += 4;
            return;
        }
    }
    D_800B0078->pc = func_800ACDB8(2);
    D_800AFC7C += 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_800958C0);
#endif

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

/* Yield; continue while a selected actor is well inside the screen,
 * otherwise jump to operand 2. */
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

/* Yield; continue while a selected actor is on screen, otherwise jump to
 * operand 2. */
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

/* Continue when a selected actor is on collision layer operand 2,
 * otherwise jump to operand 4. */
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

/* Continue when the triangle a selected actor stands on has attribute
 * operand 2, otherwise jump to operand 4. */
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

/* Continue when a selected actor is nearer than operand 2 to the published
 * actor, otherwise jump to operand 4. */
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
    if ((u32)D_8005A39C->gold >=
        operand[1] + (operand[2] << 8) + (operand[3] << 16) + (operand[4] << 24)) {
        D_800B0078->pc += 7;
        return;
    }
    D_800B0078->pc = func_800ACDB8(5);
}

/* Add gold, capped at 9999999. */
void func_80095FB8(void) {
    s32 gold;

    gold = D_8005A39C->gold + func_800ACDEC(1);
    if (gold > 9999999) {
        gold = 9999999;
    }
    D_8005A39C->gold = gold;
    D_800B0078->pc += 3;
}

/* Remove gold, not below zero. */
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

/* Branch unless the held buttons equal raw operand 1. */
void func_80096150(void) {
    func_800960E4(D_800AFE9C);
}

/* Branch unless D_800AFC6C equals raw operand 1. */
void func_80096178(void) {
    func_800960E4(D_800AFC6C);
}

/* Branch unless a held button is among raw operand 1. */
void func_800961A0(void) {
    func_80096078(D_800AFE9C);
}

/* Branch unless D_800AFC6C shares a bit with raw operand 1. */
void func_800961C8(void) {
    func_80096078(D_800AFC6C);
}

/* Clear D_800AFC6C. */
void func_800961F0(void) {
    D_800AFC6C = 0;
    D_800B0078->pc += 1;
}

s32 func_80095124(s32 item);
u8 *func_800950A0(s32 item);
u8 *func_8009501C(s32 item);

/* Store the carried count of item operand 1 in a variable. */
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
    if ((D_8005A39C->unk1D30 >> EVENT_OPERAND_BYTE(1)) & 1) {
        D_800B0078->pc += 4;
        return;
    }
    D_800B0078->pc = func_800ACDB8(2);
}

/* Set game flag bit operand 1 of unk1D30. */
void func_800965A8(void) {
    D_8005A39C->unk1D30 |= 1 << EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Clear game flag bit operand 1 of unk1D30. */
void func_800965F4(void) {
    D_8005A39C->unk1D30 &= ~(1 << EVENT_OPERAND_BYTE(1));
    D_800B0078->pc += 2;
}

/* Continue when variable 0 is below operand 1, otherwise jump. */
void func_80096644(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (func_800A3018(0) < value) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue when variable 0 is above operand 1, otherwise jump. */
void func_800966B4(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (value < func_800A3018(0)) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Continue when variable 0 equals operand 1, otherwise jump. */
void func_80096724(void) {
    s32 value;

    value = func_800ACDEC(1);
    if (func_800A3018(0) == value) {
        D_800B0078->pc += 5;
    } else {
        D_800B0078->pc = func_800ACDB8(3);
    }
}

/* Store an operand in variable 0 (extending the batch limit). */
void func_80096790(void) {
    D_800AFC7C += 32;
    func_800A3074(0, func_800ACDEC(1));
    D_800B0078->pc += 3;
}

/* Copy variable 0 into a variable. */
void func_800967E8(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, func_800A3018(0));
    D_800B0078->pc += 3;
}

/* Restore HP of party slot `slot`, up to its maximum. */
void func_80096844(s32 slot, s32 amount) {
    D_8005A39C->characters[D_80062590[slot]].hp += amount;
    if (D_8005A39C->characters[D_80062590[slot]].max_hp < D_8005A39C->characters[D_80062590[slot]].hp) {
        D_8005A39C->characters[D_80062590[slot]].hp = D_8005A39C->characters[D_80062590[slot]].max_hp;
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
    if (D_8005A39C->characters[D_80062590[slot]].max_ep < D_8005A39C->characters[D_80062590[slot]].ep) {
        D_8005A39C->characters[D_80062590[slot]].ep = D_8005A39C->characters[D_80062590[slot]].max_ep;
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

/* Reduce the HP of the party members selected by mask table entry
 * (operand 3 & 3) by a selected operand. */
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

/* Set the jump mode, animation mode and repeat delay. */
void func_80096AF4(void) {
    D_800B2078.jump_mode = func_800ACDEC(1);
    D_800B2078.animation_mode = func_800ACDEC(3);
    D_800B2078.repeat_delay = func_800ACDEC(5);
    D_800B2078.repeat_remaining = 0;
    D_800B0078->pc += 7;
}

/* Store the HP of party slot operand 3 in a variable. */
void func_80096B58(void) {
    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(3)]].hp);
    }
    D_800B0078->pc += 4;
}

/* Store the EP of party slot operand 3 in a variable. */
void func_80096C40(void) {
    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        func_800A3074(func_800ACDB8(1) & 0xFFFF, D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(3)]].ep);
    }
    D_800B0078->pc += 4;
}

/* Set the HP of party slot operand 1 (capped at its maximum); operand 3
 * selects the slot that must be occupied. */
void func_80096D28(void) {
    s32 hp;

    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        hp = func_800ACDEC(2);
        if (D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_hp < hp) {
            hp = D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_hp;
        }
        D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].hp = hp;
    }
    D_800B0078->pc += 4;
}

/* Set the EP of party slot operand 1 (capped at its maximum). */
void func_80096E20(void) {
    s32 ep;

    if (D_80062590[EVENT_OPERAND_BYTE(3)] != 0xFF) {
        ep = func_800ACDEC(2);
        if (D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_ep < ep) {
            ep = D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].max_ep;
        }
        D_8005A39C->characters[D_80062590[EVENT_OPERAND_BYTE(1)]].ep = ep;
    }
    D_800B0078->pc += 4;
}

/* Restore the EP of the masked party members by a selected operand (as
 * func_80097108). */
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

/* Reduce the EP of the masked party members by a selected operand. */
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

/* Restore the EP of the masked party members by a selected operand. */
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
    D_8005A39C->characters[id].hp = D_8005A39C->characters[id].max_hp;
    D_8005A39C->characters[id].ep = D_8005A39C->characters[id].max_ep;
    D_800B0078->pc += 3;
}

/* Fully restore every character's HP. */
void func_80097264(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        D_8005A39C->characters[i].hp = D_8005A39C->characters[i].max_hp;
    }
    D_800B0078->pc += 1;
}

/* Fully restore every character's EP. */
void func_800972AC(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        D_8005A39C->characters[i].ep = D_8005A39C->characters[i].max_ep;
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

/* Field 8007D93C(0), then 80071E58(operand 1). */
void func_8009731C(void) {
    func_8007D93C(0);
    func_80071E58(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

void func_80071DCC(s32 a);

/* Field 80071DCC(operand 1). */
void func_80097364(void) {
    func_80071DCC(func_800ACDEC(1));
    D_800B0078->pc += 3;
}

s32 func_8001B484(s32 a, s32 b);

/* Re-run until resident 8001B484(raw operand 2, operand byte 1) returns 0. */
void func_800973A4(void) {
    if (func_8001B484(func_800ACDB8(2) & 0xFFFF, EVENT_OPERAND_BYTE(1)) == 0) {
        D_800B0078->pc += 4;
    }
}

/* Skip operand-1 three-byte entries (and this opcode). */
void func_80097410(void) {
    D_800B0078->pc += func_800ACDEC(1) * 3 + 3;
}

/* Facing octant (0..7) of the controlled actor. */
s32 func_8009744C(void) {
    return (((D_800AF880.components.descriptors[D_800B2078.controlled].actor->heading_goal + 0x100) >> 9) + 2) & 7;
}

s32 func_80097A50(s32 speed);

/* Move the current actor toward actor operand 1 (speed operand 5, latched
 * in the slot); advances once arrived. */
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

/* Move toward actor operand 1 at the default speed. */
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

/* Start a relative move (mode 1) from the current position, speed
 * operand 8; advances once arrived. */
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

/* Relative move (mode 1) at the default speed. */
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

/* Move in mode 3 from the current position, speed operand 5. */
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

/* Continue a move at speed operand 8. */
void func_80097954(void) {
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(8);
    }
    if (func_80097A50(func_800ACDEC(8)) == 0) {
        D_800B0078->pc += 10;
    }
}

/* Continue a move at the default speed. */
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
    FieldModel *model;
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
        model->unk18 = 0x8000000 / (u16)D_800B0078->unk76;
    } else {
        model->unk18 = 0x4000000 / (u16)D_800B0078->unk76;
    }
    reach = func_80099A8C(model->unk18 >> 15) + 1;
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
    scale = model->unk18 >> 8;
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

/* Turn-move (mode 2) with speed operand 2 latched in the slot. */
void func_80098038(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(2);
    }
    if (func_80099AC0(func_800ACDEC(2)) == 0) {
        D_800B0078->pc += 4;
    }
}

/* Turn-move (mode 2) at the default speed. */
void func_800980FC(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 2;
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    if (func_80099AC0(0xFFFF) == 0) {
        D_800B0078->pc += 2;
    }
}

/* Turn-move in mode 3 from the current position, speed operand 3. */
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

/* Turn-move in mode 1 from the current position, speed operand 6. */
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

/* Turn-move in mode 1 at the default speed. */
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

/* Turn-move in mode 0, speed operand 6. */
void func_80098430(void) {
    D_800B0078->slots[D_800B0078->slot].move_mode = 0;
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(6);
    }
    if (func_80099AC0(func_800ACDEC(6)) == 0) {
        D_800B0078->pc += 8;
    }
}

/* Set the piece drift vector from selected operands and enable it. */
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

/* Store the planar length of (x2 - x1, z2 - z1) from selected operands in
 * a variable. */
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

/* Store the distance between two points from selected operands in a
 * variable. */
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

/* Store field 80073930(three selected operands) in a variable. */
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

/* Store the current actor's facing (12 bits) in a variable. */
void func_8009899C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B0078->heading_goal & 0xFFF);
    D_800B0078->pc += 3;
}

/* Store a selected actor's facing (12 bits) in a variable. */
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

/* Place the current actor at a selected position (whole units), marking
 * flags 0x10000 / layer 0x200000, and mirror it to its descriptor and
 * model. */
void func_80098A7C(void) {
    FieldModel *model;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800B0078->flags |= 0x10000;
    D_800B0078->layer_flags |= 0x200000;
    D_800B0078->position[0] = func_8009CF78(1, EVENT_OPERAND_BYTE(7)) << 16;
    D_800B0078->position[2] = func_8009CFBC(3, EVENT_OPERAND_BYTE(7)) << 16;
    D_800B0078->position[1] = func_8009D000(5, EVENT_OPERAND_BYTE(7)) << 16;
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = WHOLE(D_800B0078->position[0]);
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = WHOLE(D_800B0078->position[1]);
    D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = WHOLE(D_800B0078->position[2]);
    model->position[0] = D_800B0078->position[0];
    model->position[1] = D_800B0078->position[1];
    model->position[2] = D_800B0078->position[2];
    D_800B0078->pc += 8;
}

void func_80098CAC(s32 mode);

/* Walk mode 0 at the default speed. */
void func_80098C00(void) {
    D_800B0078->slots[D_800B0078->slot].value = 0xFFFF;
    func_80098CAC(0);
}

/* Walk mode 1 with speed operand 11 latched in the slot. */
void func_80098C3C(void) {
    if (D_800B0078->slots[D_800B0078->slot].value == 0xFFFF) {
        D_800B0078->slots[D_800B0078->slot].value = func_800ACDEC(11);
    }
    func_80098CAC(1);
}

#ifdef NON_MATCHING
/* Walk the current actor to an operand position over a step count derived
 * from its speed (first call sets the step, later calls advance it); at the
 * end snap to the target (when the slot asks) and continue with the next
 * instruction, then update the model matrix and its animation.
 * NON_MATCHING: the original keeps D_800B0078 in t0 throughout and loads all
 * six position/target words of the step branch before its first store; GCC
 * here uses a2 and interleaves the loads and stores. */
void func_80098CAC(s32 mode) {
    VECTOR from;
    FieldModel *model;
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
            D_800B0078->unk030[1] = D_800B0078->target[1];
            D_800B0078->unk030[2] = D_800B0078->target[2];
            D_800B0078->position[1] += D_800B0078->target[1];
            D_800B0078->unk030[0] = D_800B0078->target[0];
            D_800B0078->slots[D_800B0078->slot].value--;
            D_800B00C0 = animation;
        }
        D_800B0078->unk102--;
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] = WHOLE(D_800B0078->position[0]);
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] = WHOLE(D_800B0078->position[1]);
        D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] = WHOLE(D_800B0078->position[2]);
        model->position[0] = D_800B0078->position[0];
        model->position[1] = D_800B0078->position[1];
        model->position[2] = D_800B0078->position[2];
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_80098CAC);
#endif

#ifdef NON_MATCHING
/* Event arc jump: set up a parabolic move to an operand position (modes
 * 0-2) or step it (3; 0xf re-reads the floor triangles).
 * NON_MATCHING: case 2's abs(peak - (y >> 16)) keeps the difference in s0
 * (`subu s0,s0,v0; bgez s0; move a0,s0; negu a0,a0`) where the original
 * keeps it in v0 and negates it into a0 (`negu a0,v0`); the folded
 * ternary, if-statement and temporary forms do not reproduce it. */
void func_80099214(void) {
    VECTOR normals[4];
    SVECTOR points[4];
    SVECTOR unused[4]; /* unused in the original; reserves 0x20 bytes */
    FieldModel *model;
    u8 *code;
    u16 pc;
    u8 mode;
    s32 steps;
    s32 x;
    s32 z;
    s32 y;
    s32 layer;
    s32 peak;
    s32 distance;

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
        model->velocity[1] = -(model->gravity.value * steps / 2);
        model->velocity[1] += ((y << 16) - D_800B0078->position[1]) / steps;
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
        model->velocity[1] = -(SquareRoot0(model->gravity.s.whole * (peak << 1)) << 16);
        SquareRoot0(peak);
        steps = SquareRoot0(abs(peak - (y >> 16)));
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
            D_800B0078->position[1] += model->velocity[1];
            model->velocity[1] += model->gravity.value;
            if ((D_800B0078->target[0] != 0 || D_800B0078->target[2] != 0) && !(D_800B0078->flags & 0x8000)) {
                D_800B0078->heading_goal = D_800B0078->heading = func_8007B694((VECTOR *)D_800B0078->target) | 0x8000;
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
            model->velocity[1] = 0;
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
        model->position[0] = D_800B0078->position[0];
        model->position[1] = D_800B0078->position[1];
        model->position[2] = D_800B0078->position[2];
        D_800B0078->unk102++;
        break;
    }
    D_800B00C0 = 1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_80099214);
#endif

/* Turn-move in mode 0 at the default speed. */
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

#ifdef NON_MATCHING
s32 func_80099AC0(s32 speed) {
    VECTOR delta;
    FieldModel *model;
    s32 reach;
    s32 extra;
    s32 from_x;
    s32 from_z;
    s32 x;
    s32 z;
    s32 angle;
    s32 distance;
    FieldActor *other;
    u16 count;
    s32 facing;

    extra = 0;
    z = 0;
    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    x = 0;
    if (D_800AF880.components.descriptors[D_800AFD1C].actor->layer_flags & 0x2000) {
        model->unk18 = 0x8000000 / (u16)D_800B0078->unk76;
    } else if (model->unk18 == 0) {
        model->unk18 = 0x4000000 / (u16)D_800B0078->unk76;
    }
    reach = func_80099A8C(model->unk18 >> 15) + 1;
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
        other = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        extra = func_80099A8C((u16)D_800B0078->gravity.s.whole + (u16)other->gravity.s.whole);
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
    count = D_800B0078->slots[D_800B0078->slot].value;
    if (count == 0 || reach + extra >= distance) {
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
    D_800B0078->slots[D_800B0078->slot].value = count - 1;
    facing = func_8007B694(&delta);
    D_800B00C0 = 1;
    D_800B0078->heading_goal = D_800B0078->heading = facing;
    return -1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_80099AC0);
#endif

/* Store the current actor's unkE4 in a variable. */
void func_80099EF8(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800B0078->unkE4);
    D_800B0078->pc += 3;
}

/* Store the controlled actor's unkE4 in a variable. */
void func_80099F48(void) {
    FieldActor *player;

    player = D_800AF880.components.descriptors[D_800B2078.controlled].actor;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, player->unkE4);
    D_800B0078->pc += 3;
}

/* Store the current actor's facing octant in a variable. */
void func_80099FC4(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, (((D_800B0078->heading_goal + 0x100) >> 9) + 2) & 7);
    D_800B0078->pc += 3;
}

/* Store a selected descriptor's translation x, z, y in three variables. */
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

/* Set the current actor's unkE6 from an operand byte. */
void func_8009A0FC(void) {
    D_800B0078->unkE6 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* Clear layer flag 0x1000000 and set unkEA from an operand byte. */
void func_8009A130(void) {
    D_800B0078->layer_flags &= ~0x1000000;
    D_800B0078->unk0EA = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

/* As func_8009A130, also clearing layer flag 0x10000. */
void func_8009A174(void) {
    func_8009A130();
    D_800B0078->layer_flags &= ~0x10000;
}

/* Once layer flag 0x10000 is set, set unkEA to 0xFF and advance. */
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

/* Blend the camera distance toward operand 1 over operand-3 frames. */
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

/* Store the camera angle octant in a variable. */
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

/* Set camera heading block 0. */
void func_8009A634(void) {
    D_800AF880.heading_blocks[0] = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Set camera heading block 1. */
void func_8009A670(void) {
    D_800AF880.heading_blocks[1] = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

/* Store (sin(selected angle) * selected length) >> 12 in a variable. */
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

/* Store (cos(selected angle) * selected length) >> 12 in a variable. */
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

/* Store atan2(selected y, selected x) in a variable. */
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
    FieldModel *model;
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
    if (model->unk18 == 0) {
        model->unk18 = 0x4000000 / (u16)actor->unk76;
    }
    step = func_80099A8C(model->unk18 >> 15) + 1;
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

/* Release forced positioning and resettle the controlled actor. */
void func_8009B184(void) {
    s32 i;

    i = 0;
    D_800B2078.forced_position = 0;
    D_800B2078.party_processing_mode = 0;
    D_800B2078.history[2] = 0;
    D_800B2078.history[1] = 0;
    D_800B2078.history[0] = 0;
    D_800B2078.preserve_nonplayer_motion = 0;
    do {
        i++;
        func_80081C54(D_800B2078.controlled);
    } while (i < 32);
    D_800B0078->pc += 1;
}

s32 func_8009AEE0(s32 member, s32 x, s32 z, s32 range);
void func_8009B338(void);

/* Yield until all three party members are near the leader, then release
 * party processing and continue. */
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
    D_800B2078.history[2] = 0;
    D_800B2078.history[1] = 0;
    D_800B2078.history[0] = 0;
    D_800B2078.preserve_nonplayer_motion = 0;
    do {
        i++;
        func_80081C54(D_800B2078.controlled);
    } while (i < 32);
}

/* Event 0x23: gather the party at (op1, op3), (op5, op7) and (op9, op11)
 * facing op15/op17/op19, retrying until all three have arrived; op1 0x7fff
 * instead marks every member's heading as turned and continues. */
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

/* Store the camera projection in a variable. */
void func_8009B664(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, D_800AF880.projection);
    D_800B0078->pc += 3;
}

void func_8009AE3C(s32 target, s32 steps);

/* Blend the camera projection (operands: target, frames). */
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

/* Once the camera is idle, turn one octant over operand-1 frames. */
void func_8009B824(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B7A8(0, func_800ACDEC(1));
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

/* As func_8009B824 (the original also passes direction 0). */
void func_8009B884(void) {
    if (D_800AF880.heading_steps == 0) {
        func_8009B7A8(0, func_800ACDEC(1));
        D_800B0078->pc += 3;
    }
    D_800B00C0 = 1;
}

void func_8009B708(s32 octant, s32 steps);

/* Turn the camera to octant operand 1 over operand-3 frames (at once
 * outside D_800ADB1C). */
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



/* Set the camera elevation, octant and projection at once. */
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

/* Wait on the actor's dialogue window: with none, store the actor's +81
 * byte in variable 14 and continue; otherwise once its speaker has layer
 * flag 0x200 and the actor's low wait bit is clear, end the waiting script
 * slot and release the window. Yields. */
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

/* Event 0xa9: once this actor's dialogue window has its answer (or is
 * still being typed), highlight lines op1 >> 4 .. op1 & 0xf as a choice. */
void func_8009BC98(void) {
    s32 window;
    u32 first;

    if (func_8009CD18(&window) == 0) {
        D_800AFC7C += 8;
        if (func_80033CD0(&D_800C2698[window].text) == 1 ||
            ((&D_800C2698[window].text)->unk84 != 0 && (&D_800C2698[window].text)->unk6C != 0)) {
            D_800C2698[window].status = 0;
            D_800B0078->unk081 = 0xFF;
            first = EVENT_OPERAND_BYTE(1) >> 4;
            D_800C2698[window].unk37E = first;
            D_800C2698[window].unk380 = (EVENT_OPERAND_BYTE(1) & 0xF) - first + 1;
            D_800C2698[window].unk382 = 0;
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


/* Close this actor's dialogue window (operand 1 zero) or reset its
 * speech state; yields. */
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

/* Copy a selected actor's unk80 and open its message (func_8009C01C). */
void func_8009BF8C(void) {
    if (func_8009CDB4(1) != 0xFF) {
        D_800B0078->character = D_800AF880.components.descriptors[func_8009CDB4(1)].actor->character;
        func_8009C01C();
        return;
    }
    D_800B0078->pc += 6;
}

s32 func_8009C5A8(s32 index, s32 mode);

/* Show a message for a selected actor; re-runs until a window is free. */
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

/* Show a message for the current actor (mode 0). */
void func_8009C0B4(void) {
    func_8009C5A8(D_800AFD1C, 0);
}

/* Show a message for the current actor (mode 1). */
void func_8009C0DC(void) {
    func_8009C5A8(D_800AFD1C, 1);
}

/* Show a message for the current actor (mode 2). */
void func_8009C104(void) {
    func_8009C5A8(D_800AFD1C, 2);
}

/* Show a message for the current actor (mode 3). */
void func_8009C12C(void) {
    func_8009C5A8(D_800AFD1C, 3);
}

#ifdef NON_MATCHING
/* Show the dialogue portrait of `character`: finish a pending slot first
 * (upload loaded images, or release shown ones) and return -1; a slot
 * already holding it is selected (bits 2-4 of the actor state) and 0
 * returned; otherwise the next free slot starts loading its image files and
 * -1 is returned. The placement (800aeae4) and file (800ae1e0) tables are
 * indexed as flat arrays, which keeps their bases in registers as the
 * original does.
 * NON_MATCHING: only the second file's address differs: the original
 * computes it into a0, GCC here into s1 (c * 2 + &D_800AE1E1). */
#define PLACE(i, k) (((s16 *)D_800AEAE4)[(i) * 8 + (k)])
#define FILES(c, k) (((u8 *)D_800AE1E0)[(c) * 2 + (k)])
s32 func_8009C154(s32 character) {
    s32 i;
    s32 found;

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
    D_800B00C8[i].file = FILES(character, 0) + 0x46;
    D_800B00C8[i].destination = D_800ADB10 = func_80031BDC(func_800288EC(D_800B00C8[i].file), 0);
    i++;
    if (FILES(character, 1) != FILES(character, 0)) {
        D_800B06A4[D_800ADB0C].c = 1;
        D_800B00C8[i].file = FILES(character, 1) + 0x46;
        D_800B00C8[i].destination = D_800ADB14 = func_80031BDC(func_800288EC(D_800B00C8[i].file), 0);
        i++;
    }
    D_800B00C8[i].file = 0;
    D_800B00C8[i].destination = NULL;
    func_80029AFC(D_800B00C8, 0, 0);
    return -1;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_8009C154);
#endif

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

/* Set the current actor's speech parameters from operand bytes. */
void func_8009CE48(void) {
    D_800B0078->unk88 = EVENT_OPERAND_BYTE(1) * 2;
    D_800B0078->unk8A = EVENT_OPERAND_BYTE(2);
    D_800B0078->unk82 = EVENT_OPERAND_BYTE(3) * 3;
    D_800B0078->unk83 = EVENT_OPERAND_BYTE(4);
    D_800B0078->pc += 5;
}

/* Set the current actor's speech parameters from operands. */
void func_8009CEE0(void) {
    D_800B0078->unk88 = func_800ACDEC(1);
    D_800B0078->unk8A = func_800ACDEC(3);
    D_800B0078->unk82 = func_800ACDEC(5) * 3;
    D_800B0078->unk83 = func_800ACDEC(7);
    D_800B0078->unk84 = func_800ACDEC(9);
    D_800B0078->pc += 11;
}

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


/* Store a random number in a variable. */
void func_8009D198(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, rand());
    D_800B0078->pc += 3;
}

/* Store a random number in 0..operand in a variable. */
void func_8009D1F0(void) {
    s32 value;

    value = (rand() * (func_800ACDEC(3) + 1)) >> 15;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 5;
}

/* Shift a variable right by an operand. */
void func_8009D260(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value = value >> func_800ACDEC(3);
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 5;
}

/* Shift a variable left by an operand. */
void func_8009D2D0(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value = value << func_800ACDEC(3);
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 5;
}

/* Increment a variable. */
void func_8009D340(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF) + 1;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 3;
}

/* Decrement a variable. */
void func_8009D3A4(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF) - 1;
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 3;
}

/* Clear bit (selected operand) of a variable. */
void func_8009D408(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value &= ~(1 << func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* XOR a variable with a selected operand. */
void func_8009D4A0(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value ^= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* OR a variable with a selected operand. */
void func_8009D52C(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value |= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* AND a variable with a selected operand. */
void func_8009D5B8(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value &= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Set bit (selected operand) of a variable. */
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

/* Subtract a selected operand from a variable. */
void func_8009D804(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value -= func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Add a selected operand to a variable. */
void func_8009D890(void) {
    s32 value;

    value = func_800A3018(func_800ACDB8(1) & 0xFFFF);
    value += func_8009CFBC(3, EVENT_OPERAND_BYTE(5));
    func_800A3074(func_800ACDB8(1) & 0xFFFF, value);
    D_800B0078->pc += 6;
}

/* Clear a variable. */
void func_8009D91C(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, 0);
    D_800B0078->pc += 3;
}

/* Set a variable to one. */
void func_8009D960(void) {
    func_800A3074(func_800ACDB8(1) & 0xFFFF, 1);
    D_800B0078->pc += 3;
}

/* Set a variable from a selected operand. */
void func_8009D9A4(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, func_8009CFBC(3, EVENT_OPERAND_BYTE(5)));
    D_800B0078->pc += 6;
}

/* Set actor flag 0x20000. */
void func_8009DA1C(void) {
    FieldActor *actor = D_800B0078;

    actor->flags |= 0x20000;
    actor->pc++;
}

/* Clear actor flag 0x20000. */
void func_8009DA44(void) {
    FieldActor *actor = D_800B0078;

    actor->flags &= ~0x20000;
    actor->pc++;
}

/* Set actor flag 0x800000. */
void func_8009DA70(void) {
    FieldActor *actor = D_800B0078;

    actor->flags |= 0x800000;
    actor->pc++;
}

/* Clear actor flag 0x800000. */
void func_8009DA98(void) {
    FieldActor *actor = D_800B0078;

    actor->flags &= ~0x800000;
    actor->pc++;
}

/* Hide a selected actor, disable its descriptor and release the current
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

/* Show a selected actor again. */
void func_8009DBC8(void) {
    FieldActor *actor;

    if (func_8009CDB4(1) != 0xFF) {
        actor = D_800AF880.components.descriptors[func_8009CDB4(1)].actor;
        actor->flags &= ~1;
    }
    D_800B0078->pc += 2;
}

/* Stop and hide a selected actor and release the current actor's idle
 * dialogue window. */
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

/* Re-enable a selected, still visible actor's descriptor. */
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

/* Disable a selected actor's descriptor (flag 0x20). */
void func_8009DE94(void) {
    FieldDescriptor *descriptor;

    if (func_8009CDB4(1) != 0xFF) {
        descriptor = &D_800AF880.components.descriptors[func_8009CDB4(1)];
        descriptor->flags |= 0x20;
    }
    D_800B0078->pc += 2;
}

/* Re-enable the current descriptor. */
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

void func_80021BCC(FieldModel *model, u16 value);

/* Set the current actor's unk76 and pass it to its model. */
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

/* Start a jump (func_8009E574 / func_8009E810 from signed operands). */
void func_8009E248(void) {
    s32 a;

    a = (s16)func_800ACD7C(1);
    func_8009E574(a, (s16)func_800ACD7C(3));
    func_8009E810((s16)func_800ACD7C(5));
    D_800B0078->flags |= 0x40000;
    D_800B0078->pc += 7;
}

/* Start func_8009E810 from a selected operand. */
void func_8009E2C8(void) {
    func_8009E810(func_8009CF78(1, EVENT_OPERAND_BYTE(3)));
    D_800B0078->flags |= 0x40000;
    D_800B0078->pc += 4;
}

/* Signed halfword of the bytecode at `offset`. */
s16 func_8009E330(s32 offset) {
    return D_800ADC00[offset] + (D_800ADC00[offset + 1] << 8);
}

/* Place the current actor on layer operand 5 at a selected x/z. */
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

/* Place the current actor at a selected x/z. */
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
    FieldModel *model;
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
    model->unk84 = points[D_800B0078->layer].vy;
    D_800B0078->position[0] = x << 16;
    D_800B0078->position[1] = points[D_800B0078->layer].vy << 16;
    D_800B0078->position[2] = z << 16;
    D_800B0078->unk72 = points[D_800B0078->layer].vy;
    model->position[0] = D_800B0078->position[0];
    model->position[1] = D_800B0078->position[1];
    model->position[2] = D_800B0078->position[2];
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
    model->velocity[0] = 0;
    model->velocity[1] = 0;
    model->velocity[2] = 0;
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

/* Set the current actor's extents from non-zero operand bytes (doubled). */
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

/* Event: give the current actor a boundary quadrilateral (+114, allocated
 * once and marked by state bit 12) of four selected x/z corners. */
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

/* Event: request event (tag low five bits of operand 2, priority its high
 * three) on a selected actor, in its first free slot; retried while none is
 * free. An actor in a request handshake (+04 bit 20) instead releases both
 * linked slots. */
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

/* Event: request event operand 2 on a selected actor and wait until it has
 * started: phase 0 (the current slot's bits 16-17) requests it, linking the
 * two slots through +cf, and phase 1 waits for the target to run it. */
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

/* Event: request event operand 2 on a selected actor and wait until it has
 * started (phase 1 of the current slot's bits 16-17) and then finished
 * (phase 2, its slot free again). */
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

/* Wander: every 16 frames turn the facing target by +/- an octant. */
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

/* Wander with pauses: as func_8009F424, sometimes holding the facing. */
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
            if (D_800C2698[i].status == 0) {
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
                        D_800ADB28 = D_800B2078.history[0];
                    }
                }
            } else {
                if (D_800C2694 & 0x80) {
                    if (D_800B2078.repeat_remaining != 0) {
                        goto count;
                    }
                    if (D_800ADB64 == 0xFF && func_80081F5C(D_800B0078) == 0) {
                        D_800B0078->flags |= 0x800;
                        D_800ADB28 = D_800B2078.history[0];
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


#ifdef NON_MATCHING
/* Place the current actor at entry point `entry` of the bytecode's entry
 * table (when present): layer, x/z, camera octant and facing (0xFF: from
 * variables 8 and 6).
 * NON_MATCHING: the original sets the return 0 in the first branch's delay
 * slot, keeps heading/facing in a1, stores D_800AF880.heading as the
 * sign-extended (heading << 16) >> 16 shared with heading_high, and copies
 * facing to v1 before its three stores; GCC here folds the extension away
 * (heading's range is known) and stores facing straight from a0. */
s32 func_8009FA54(s32 entry) {
    s32 marker;
    s32 record;
    s32 x;
    s16 heading;
    s32 facing;

    marker = D_800ADC00[0];
    if (marker == 0xFF) {
        record = entry * 7;
        D_800B0078->layer = D_800ADC00[record + 5];
        x = func_8009E330(record + 1);
        func_8009E574(x, func_8009E330(record + 3));
        heading = ((D_800ADC00[record + 6] + 4) & 7) << 9;
        if (D_800ADC00[record + 6] == marker) {
            heading = ((func_800A3018(8) + 4) & 7) << 9;
        }
        D_800AF880.heading_angles.vy = heading;
        D_800AF880.heading = heading;
        D_800AF880.heading_high = heading << 16;
        facing = (((D_800ADC00[record + 7] - 2) & 7) << 9) | 0x8000;
        if (D_800ADC00[record + 7] == marker) {
            facing = (((func_800A3018(6) - 2) & 7) << 9) | 0x8000;
        }
        D_800B0078->heading = facing;
        D_800B0078->heading_goal = facing;
        D_800B0078->unk108 = facing;
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_8009FA54);
#endif

extern s32 D_8004F34C;
void func_8001AD1C(void);
void func_8001B044(void);
void func_8001B3A8(void);

/* Mark the field id (0xC000) and reset the resident services; record
 * operand byte 1 in D_800B2268. */
void func_8009FB98(void) {
    D_8004F34C |= 0xC000;
    func_8001AD1C();
    func_8001B044();
    func_8001B3A8();
    D_800B2078.unk2268 = EVENT_OPERAND_BYTE(1);
    D_800B0078->pc += 2;
}

extern s32 D_8006F990[3];

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

/* Set party slot flag (unk22B1) for slot operand 1 (max 2). */
void func_8009FC48(void) {
    s32 slot;

    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    D_8005A39C->unk22B1[slot] = 1;
    func_8009FD10(slot);
    D_800B0078->pc += 3;
}

/* Clear party slot flag (unk22B1) for slot operand 1 (max 2). */
void func_8009FCAC(void) {
    s32 slot;

    slot = func_800ACDEC(1);
    if (slot >= 3) {
        slot = 2;
    }
    D_8005A39C->unk22B1[slot] = 0;
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

/* Run func_800AD4D4 for the current actor's party slot unless flagged. */
void func_8009FDD4(void) {
    s32 slot;

    slot = func_8009FC10(D_800AFD1C);
    if (slot != 0xFF && D_8005A39C->unk22B1[slot] == 0) {
        func_800AD4D4(slot);
    }
    D_800B0078->pc += 1;
}

void func_800ACFD0(s32 slot);

/* Run func_800ACFD0 for party slot operand 1 when occupied and flagged. */
void func_8009FE4C(void) {
    u8 slot;

    slot = EVENT_OPERAND_BYTE(1);
    if (D_80062590[slot] != 0xFF && D_8005A39C->unk22B1[slot] != 0) {
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
        if (D_8005A39C->unk22B1[slot] != 0) {
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
    D_800AF880.components.descriptors[to].model->position[0] = D_800AF880.components.descriptors[from].actor->position[0];
    D_800AF880.components.descriptors[to].model->position[1] = D_800AF880.components.descriptors[from].actor->position[1];
    D_800AF880.components.descriptors[to].model->position[2] = D_800AF880.components.descriptors[from].actor->position[2];
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
    FieldModel *model;
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
            if (D_8005A39C->unk22B1[slot] != 0) {
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
    FieldModel *model;

    model = D_800AF880.components.descriptors[D_800AFD1C].model;
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[0] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[0] =
        WHOLE(D_800B0078->position[0]);
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[1] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[1] =
        WHOLE(D_800B0078->position[1]);
    D_800AF880.components.descriptors[D_800AFD1C].transform.t[2] = D_800AF880.components.descriptors[D_800AFD1C].matrix.t[2] =
        WHOLE(D_800B0078->position[2]);
    model->position[0] = D_800B0078->position[0];
    model->position[1] = D_800B0078->position[1];
    model->position[2] = D_800B0078->position[2];
    model->velocity[1] = 0;
    D_800B0078->unk72 = model->unk84 = WHOLE(D_800B0078->position[1]);
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

/* Set D_800B2078.unk234A (mode block unk34A) from an operand. */
void func_800A0DC0(void) {
    D_800B2078.unk234A = func_800ACDEC(1);
    D_800B0078->pc += 3;
}

s32 func_80028530(void);

/* Store resident 80028530() in a variable. */
void func_800A0DFC(void) {
    s32 reference;

    reference = func_800ACDB8(1) & 0xFFFF;
    func_800A3074(reference & 0xFFFF, func_80028530());
    D_800B0078->pc += 3;
}


/* Yield; advance once D_800ADB74 is zero. */
void func_800A0E54(void) {
    if (D_800ADB74 == 0) {
        D_800B0078->pc += 1;
    } else {
        D_800B0078->pc -= 1;
    }
    D_800B00C0 = 1;
}

extern s32 D_800ADB84;

/* Count D_800ADB84 up and yield. */
void func_800A0EB0(void) {
    D_800B00C0 = 1;
    D_800ADB84 += 1;
    D_800B0078->pc += 1;
}


/* Close the current actor's 801e layer: mode 0 clears its flag, mode 1
 * releases it. Yields. */
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

/* Layer model command for the current actor's layer (yields while
 * D_800ADB2C or func_8008A558 is busy): operand 1 = 0 deactivates the
 * layer's model, 1 starts loading its two resources (files 0x6ba/0x6bb +
 * 2 * operand 5), 2 waits for the load and then builds the model at the
 * actor's position. */
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

/* Give the current actor the field's first sprite on the next free 801e
 * layer (operand 1: layer parameter) and show it. */
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

/* Give the current actor sprite operand 1 (parameter operand 3), mirror its
 * position and show it. */
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

/* As func_800A14F0 with parameter 0. */
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

extern char D_8006FD44[]; /* "STACKERR ACT=%d\n" */
extern void func_800379C8(char *format, ...);

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
            func_800379C8(D_8006FD44, D_800AFD1C);
        }
        D_800B00C0 = 1;
    }
}

/* As func_800A1730 for a 3-byte instruction. */
void func_800A17F4(void) {
    FieldActor *actor;

    actor = D_800B0078;
    if ((actor->state.word & 0x1C0) != 0x100) {
        actor->call_stack[(actor->state.word >> 6) & 7] = actor->pc + 3;
        D_800B0078->pc = func_800ACDB8(1);
        D_800B0078->state.word = (D_800B0078->state.word & ~0x1C0) | ((((D_800B0078->state.word >> 6) & 7) + 1) & 7) << 6;
    } else {
        if (D_800C268C == 0) {
            func_800379C8(D_8006FD44, D_800AFD1C);
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
            func_800379C8(D_8006FD44, D_800AFD1C);
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

/* Reset the current actor's eight script slots and call stack; yields. */
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

INCLUDE_RODATA(".local/decomp/field/asm/nonmatchings/field_800854D0", D_8006FD44);

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

extern s32 D_8004F30C;
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
                              D_800AF880.components.descriptors[i].actor->unk120);
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
    FieldModel *model;
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
                    D_800AF880.components.descriptors[i].model->animation->unk18[2] = D_800AF880.components.descriptors[i].actor->state.bits.unk18;
                    D_800AF880.components.descriptors[i].model->animation->unk18[3] = D_800AF880.components.descriptors[i].actor->unk130;
                    break;
                case 2:
                    func_8002303C(D_800AF880.components.descriptors[i].model, 3, 0);
                    D_800AF880.components.descriptors[i].model->animation->unk18[2] = D_800AF880.components.descriptors[i].actor->state.bits.unk18;
                    D_800AF880.components.descriptors[i].model->animation->unk18[3] = D_800AF880.components.descriptors[i].actor->unk130;
                    D_800AF880.components.descriptors[i].model->animation->unk18[4] = D_800AF880.components.descriptors[i].actor->unk130_9;
                    D_800AF880.components.descriptors[i].model->animation->unk18[5] = D_800AF880.components.descriptors[i].actor->unk130_19;
                    break;
                }
            }
        }
        if (D_800B2078.unk2268 != 0) {
            for (i = 0; i < 3; i++) {
                if (D_8005A444[i] != 0xFF) {
                    if (D_8005A39C->unk22B1[i] != 0) {
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

extern u16 D_8005941C;
extern u8 D_800594D0;
s32 func_8009744C(void);
s32 func_8009A514(void);

/* Record the current map and camera in the game state and variables and
 * save the event variable bank. */
void func_800A30FC(void) {
    s32 i;

    D_8005A39C->unk231A = D_8004F34C;
    D_8005A39C->unk2322 = D_8004F324;
    D_8005A39C->unk2320 = D_8005A39C->vars[1];
    D_8005A39C->unk231C = D_8005A39C->vars[4] << 9;
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
        D_8005A39C->unk1D34[i] = D_80062590[i];
    }
    func_800A30FC();
    D_8004F2F4 = 0;
    D_8004F318++;
    for (i = 0; i < 3; i++) {
        if (D_8005A39C->unk22B1[i] == 1) {
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
    COPY_BLOCK(&D_800B2078, D_800AFC50, 0x2E4);
    D_800AFC50 += 0x2E4;
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

#ifdef NON_MATCHING
/* Read the descriptor count, view block and per-actor records from the
 * block at D_8005A4E4, passing each model its record (func_80021D50).
 * NON_MATCHING: the original forms the first descriptor pointer offset
 * first (`addu a0,s1,v0`) and keeps it in a0 with the actor in a1; the
 * pointer local here adds base first and swaps a0/a1 through both blocks. */
void func_800A3C8C(void) {
    s32 changed;
    s32 i;
    u8 *record;
    FieldDescriptor *descriptor;
    FieldActor *actor;

    D_800AFC50 = D_8005A4E4;
    D_800AF880.components.descriptor_count = *D_800AFC50;
    D_800AFC50 += 0x3C;
    *(ViewSnapshot *)&D_800AF880.world_angles = *(ViewSnapshot *)D_800AFC50;
    D_800AFC50 += 0x920;
    changed = 0;
    for (i = 0; i < 3; i++) {
        if (D_8005A408[i] != D_8005A39C->unk22B1[i]) {
            changed++;
        }
    }
    for (i = 0; i < D_800ADBFC; i++) {
        record = D_800AFC50;
        D_800AFC50 += 0xC;
        if (D_800AF880.components.descriptors[i].actor->unk124 != -1 && D_800AF880.components.descriptors[i].actor->unk0EA != 0xFF) {
            *(s16 *)(record + 0x20) = D_800AF880.components.descriptors[i].actor->unk0EA;
        }
        descriptor = &D_800AF880.components.descriptors[i];
        if (!(descriptor->actor->layer_flags & 0x1000000)
            && (D_800B2078.unk2268 == 0 || !(descriptor->actor->flags & 0x600) || changed == 0)) {
            func_80021D50(descriptor->model, D_800AFC50);
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
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_800A3C8C);
#endif

#ifdef NON_MATCHING
/* Write the field state block at D_8005A4E4 (descriptor count, view,
 * collision attributes, D_800B2078, per-actor records and D_800C3A68) and
 * print its size.
 * NON_MATCHING: the original adds the descriptor base before the index for
 * the rotation copy (a descriptor pointer for that copy alone reproduces
 * it) and holds &D_8005A4E4 in s1, loaded before D_800C268C, for the size
 * computed in the branch delay slot. */
void func_800A3F4C(void) {
    s32 i;
    s32 flags;
    s32 size;
    u8 *snapshot;

    D_800AFC50 = D_8005A4E4;
    *D_800AFC50 = D_800AF880.components.descriptor_count;
    D_800AFC50 += 4;
    COPY_BLOCK(D_800AFC50, &D_800B007C, 0x38);
    D_800AFC50 += 0x38;
    COPY_BLOCK(D_800AFC50, &D_800AF880.world_angles, 0x74);
    D_800AFC50 += 0x74;
    COPY_BLOCK(D_800AFC50, D_800AF880.components.collision_attributes, 0x400);
    D_800AFC50 += 0x400;
    COPY_BLOCK(D_800AFC50, &D_800B2078, 0x2E4);
    D_800AFC50 += 0x2E4;
    COPY_BLOCK(D_800AFC50, &D_800AF880, 0x1C8);
    D_800AFC50 += 0x1C8;
    for (i = 0; i < D_800ADBFC; i++) {
        COPY_BLOCK(D_800AFC50, &D_800AF880.components.descriptors[i].rotation, 8);
        D_800AFC50 += 8;
        flags = D_800AF880.components.descriptors[i].flags;
        COPY_BLOCK(D_800AFC50, &flags, 4);
        D_800AFC50 += 4;
        func_80021EBC(D_800AF880.components.descriptors[i].model, D_800AFC50);
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
        D_8005A408[i] = D_8005A39C->unk22B1[i];
    }
    snapshot = D_8005A4E4;
    size = D_800AFC50 - snapshot;
    if (D_800C268C == 0) {
        func_800379C8("SAVESIZE=%d %x\n", size, size);
    }
}
#else
INCLUDE_ASM(".local/decomp/field/asm/nonmatchings/field_800854D0", func_800A3F4C);
#endif
