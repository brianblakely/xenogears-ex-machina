#ifndef RESIDENT_MODEL_H
#define RESIDENT_MODEL_H

#include "common.h"

/* Resident model renderer. Field names follow their observed use; unknown
 * bytes keep their offsets. */

/* A loaded model group: its heap block ends at `primitives` once trimmed. */
typedef struct {
    u8 unk0[4];
    s32 flags;       /* bit 1: trimmed */
    u8 unk8[0x1C];
    u8 *primitives;
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

extern void *func_80031BDC(s32 size, s32 mode);
extern void *func_80031F70(void *data, s32 size);
extern s32 func_800320E8(void *data);
extern void func_800324B8(s16 kind);

#endif
