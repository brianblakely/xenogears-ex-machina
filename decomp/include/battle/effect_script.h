#ifndef BATTLE_EFFECT_SCRIPT_FILE_H
#define BATTLE_EFFECT_SCRIPT_FILE_H

#include "common.h"

/* An effect script file: a PlayStation TMD model. The id 0x41, flags (bit 0
 * FIXP: relocated), the object (entry) count, then per entry its vertex,
 * normal and primitive tables with their counts and a scale; its commands are
 * TMD primitives (not bytecode of the battle's effect VM, 800AAD54). The
 * battle draws them (800B1720-800B2AEC), the battle module ovl3384 breaks one
 * into pieces, and the resident's D_8001C76C (0x170 bytes, one entry) is
 * one. */

/* An entry (0x1C bytes); the table offsets are from the entry until
 * relocated. */
typedef struct ScriptEntry {
    u8 *vertices;    /* 0x00 */
    s32 vertexCount; /* 0x04 */
    u8 *normals;     /* 0x08 */
    s32 normalCount; /* 0x0C */
    u8 *commands;    /* 0x10: 4-byte aligned, each opening with a ScriptCommand */
    s32 count;       /* 0x14: commands */
    s32 scale;       /* 0x18 */
} ScriptEntry;

typedef struct {
    u8 pad0[4];
    u32 flags; /* 0x04: bit 0 relocated */
    u8 pad8[4];
    ScriptEntry entries[1]; /* 0x0C */
} ScriptFile;

/* A command's header: the words of the GPU primitive it builds and its own
 * words, each less one, flags (bit 0 lit; it selects the layout of the vertex
 * indices) and the GPU code (bits 2-4: 4 textured, 8 quad, 16 gouraud). */
typedef struct {
    u8 primWords;
    u8 words;
    u8 flags;
    u8 code;
} ScriptCommand;

#endif
