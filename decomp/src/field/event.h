#ifndef FIELD_EVENT_H
#define FIELD_EVENT_H

#include "common.h"

/* Field event interpreter state (analysis/formats/field-lifecycle.md,
 * src/reconstruction/field_script.cpp). Opcode handlers read operands at byte
 * offsets from the current actor's working PC and advance it themselves. */

/* One 0x138-byte event actor record. */
typedef struct FieldActor {
    u32 flags;          /* 000 */
    u32 layer_flags;    /* 004 */
    u8 unk008[0x20 - 0x008];
    s32 position[3];    /* 020: 16.16 x, y, z */
    u8 unk02C[0xCC - 0x02C];
    u16 pc;             /* 0CC: working PC, relative to the bytecode */
    u8 slot;            /* 0CE: selected script slot */
    u8 unk0CF[0xFC - 0x0CF];
    u8 color0[3];       /* 0FC */
    u8 color1[3];       /* 0FF */
    u8 unk102[0x138 - 0x102];
} FieldActor;

/* One 0x5C-byte event descriptor; one per event actor. */
typedef struct FieldDescriptor {
    u8 unk00[0x04];
    s32 unk04;          /* 04 */
    u8 unk08[0x4C - 0x08];
    FieldActor *actor;  /* 4C */
    u8 unk50[0x5C - 0x50];
} FieldDescriptor;

extern u8 *D_800ADC00;              /* event bytecode */
extern FieldDescriptor *D_800AFB10; /* descriptor table */
extern s32 D_800AFD1C;              /* current actor index */
extern FieldActor *D_800B0078;      /* current actor */
extern s32 D_800B00C0;              /* yield: stop this actor's batch */

/* Operand readers; each takes the byte offset from the working PC. */
s32 func_800ACD7C(s32 offset);  /* signed halfword */
s32 func_800ACDB8(s32 offset);  /* raw halfword */
s32 func_800ACDEC(s32 offset);  /* bit 15: 15-bit immediate, else variable */
s32 func_8009CD7C(s32 offset);  /* actor selector */
s32 func_8009CDB4(s32 offset);  /* actor selector; 0xFF when none */
s32 func_800A3018(u16 reference); /* read a variable */

/* Selected operands: when the given bit of `flags` is set the operand is a
 * signed immediate halfword, otherwise a variable reference. */
s32 func_8009CF78(s32 offset, s32 flags); /* bit 0x80 */
s32 func_8009CFBC(s32 offset, s32 flags); /* bit 0x40 */
s32 func_8009D000(s32 offset, s32 flags); /* bit 0x20 */
s32 func_8009D044(s32 offset, s32 flags); /* bit 0x10 */

extern s32 D_800AFC7C;              /* batch limit */

#define EVENT_OPERAND_BYTE(offset) (D_800ADC00[D_800B0078->pc + (offset)])

#endif
