#ifndef RESIDENT_MODEL_H
#define RESIDENT_MODEL_H

#include "common.h"
#include "heap.h"

/* Resident model renderer. Field names follow their observed use; unknown
 * bytes keep their offsets. */

/* A model's list of paired data offsets: entries 0..last (none when last
 * is -1). */
typedef struct {
    s32 unk0;
    u8 *first;
    u8 *second;
} ModelListEntry;

typedef struct {
    s32 last;
    ModelListEntry entries[1];
} ModelList;

/* One model of a group. The tables are stored as offsets from the group
 * and relocated to addresses once. */
typedef struct {
    u8 *table0;
    u8 *table4;
    u8 *table8;
    u8 *primitives;
    u8 unk10[4];
    ModelList *list; /* optional */
    u8 unk18[0x20];
} Model;

/* A loaded model group: its heap block ends at the first model's primitives
 * once trimmed. */
typedef struct {
    s32 count;
    s32 flags;       /* bit 0: relocated; bit 1: trimmed */
    u8 unk8[0x10];
    Model models[1];
} ModelGroup;

/* A model's primitive buffer. */
typedef struct {
    u16 flags;       /* bit 0: owns `buffer`; bit 6: trimmed */
    u8 unk2[0x12];
    u8 *end;
    u8 *buffer;
    u8 unk1C[0x18];
    s32 size;
} ModelBuffer;

/* Renderer output packet header. */
typedef struct {
    u8 unk0[3];
    u8 code;
    s32 value;
} RenderPacket;

extern RenderPacket *D_80059424;

void func_8002DDE4(void *image, s32 mode, s32 x, s32 y, s32 a4, s32 a5, s32 a6); /* upload an image */
u8 *func_8002DFE0(void); /* the shared unpack buffer */

#endif
