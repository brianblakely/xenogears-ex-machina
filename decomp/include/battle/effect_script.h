#ifndef BATTLE_EFFECT_SCRIPT_FILE_H
#define BATTLE_EFFECT_SCRIPT_FILE_H

#include "common.h"

/* An effect script file: a PlayStation TMD model. The id 0x41, flags (bit 0
 * FIXP: relocated), the object (entry) count, then per entry its vertex,
 * normal and primitive tables with their counts and a scale; its commands are
 * TMD primitives (not bytecode of the battle's effect VM, 800AAD54). The
 * battle draws them (800B168C-800B2AEC), the battle module ovl3384 breaks one
 * into pieces, and the resident's model_slot_ring_tmd (0x170 bytes, one entry) is
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
 * indices) and the GPU code (bits 2-4: 4 textured, 8 quad, 16 gouraud). Then,
 * by primitive:
 *   flat (F3/F4):      0x04 colour
 *   gouraud (G3/G4):   0x04, 0x08, 0x0C (, 0x10) colours
 *   textured (FT3/FT4, GT3/GT4): 0x04 uv0, 0x06 clut, 0x08 uv1, 0x0A tpage,
 *                      0x0C uv2 (, 0x10 uv3); lit: FT3 0x10, FT4 0x14 colour,
 *                      GT3 0x10.., GT4 0x14.. colours 4 bytes apart. */
typedef struct {
    u8 primWords;
    u8 words;
    u8 flags;
    u8 code;
} ScriptCommand;

/* An entry's vertex table as 800B1EA0 scales it before relocation: the
 * vertices at offset from the entry, their count, and the scale word's bit 15
 * once scaled. */
typedef struct {
    s32 offset; /* 0x00: from the list */
    s32 count;  /* 0x04 */
    u8 pad8[0x18 - 0x8];
    u32 flags;  /* 0x18 */
} VertexList;

/* Drawing (the battle, 800B15D8's unit). */
u8 *battle_tmd_get_object(u8 *table, s32 index);   /* entry index of a script file */
s32 battle_tmd_get_packet_size(ScriptEntry *entry);      /* the bytes of its primitives */
/* Defined old-style: callers pass the entry, its primitive buffer and the
 * blend and shade flags unconverted. */
void battle_tmd_build_packets();                       /* build the entry's primitives */
void battle_tmd_scale_vertices(VertexList *list, s32 shift);
void battle_tmd_draw_object();                       /* draw the entry (old-style) */

#endif
