#ifndef FIELD_EVENT_H
#define FIELD_EVENT_H

#include "geometry.h"

/* Field event interpreter state (analysis/formats/field-lifecycle.md,
 * src/reconstruction/field_script.cpp). Opcode handlers read operands at byte
 * offsets from the current actor's working PC and advance it themselves. */

/* One 0x138-byte event actor record. */
typedef struct FieldActor {
    u32 flags;          /* 000 */
    u32 layer_flags;    /* 004 */
    u8 unk008[0x20 - 0x008];
    Fixed position[3];  /* 020: x, y, z */
    u8 unk02C[0x060 - 0x02C];
    s16 unk60;           /* 060 */
    u8 unk062[0x064 - 0x062];
    s16 unk64;           /* 064 */
    u8 unk066[0x0CC - 0x066];
    u16 pc;             /* 0CC: working PC, relative to the bytecode */
    u8 slot;            /* 0CE: selected script slot */
    u8 unk0CF[0xEE - 0x0CF];
    s16 unkEE;          /* 0EE */
    u8 unk0F0[0xFC - 0x0F0];
    u8 color0[3];       /* 0FC */
    u8 color1[3];       /* 0FF */
    u8 unk102[0x118 - 0x102];
    s32 *words;         /* 118 */
    u8 unk11C[0x134 - 0x11C];
    u32 unk134;         /* 134 */
} FieldActor;

/* One 0x5C-byte event descriptor; one per event actor. */
typedef struct FieldDescriptor {
    u8 unk00[0x04];
    s32 unk04;          /* 04 */
    u8 unk08[0x4C - 0x08];
    FieldActor *actor;  /* 4C */
    u8 unk50[0x52 - 0x50];
    s16 unk52;           /* 52 */
    u8 unk54[0x58 - 0x54];
    u16 flags;          /* 58 */
    u8 unk5A[0x5C - 0x5A];
} FieldDescriptor;

/* Resident persistent game state (*8005a39c). */
typedef struct GameState {
    u8 unk0000[0x1932];
    s16 unk1932;         /* 1932 */
    u8 unk1934[0x2320 - 0x1934];
    s16 unk2320;         /* 2320 */
    u8 unk2322[0x2324 - 0x2322];
} GameState;

extern GameState *D_8005A39C;
extern s16 D_800C3A68[];            /* event variable bank */

/* Field settings block at 800b2174 (meanings from src/reconstruction/
 * original_layout.cpp); addressed as one aggregate. */
typedef struct FieldSettings {
    s16 talk_inhibited;         /* 174 */
    s16 encounter_inhibition;   /* 176 */
    s16 terrain_angle;          /* 178 */
    s16 input_mask;             /* 17a */
    s32 unk17C;                 /* 17c */
    u8 unk180[0x18E - 0x180];
    s16 sprite_gate;            /* 18e */
    u8 fog_color[4];            /* 190 */
    u8 far_color[4];            /* 194 */
    s16 fog_range[2];           /* 198 */
    u8 clear_color[4];          /* 19c */
    u8 unk1A0[0x1D0 - 0x1A0];
    u8 script_control[2];       /* 1d0 */
    u8 unk1D2[0x1D6 - 0x1D2];
    s16 text_speed;             /* 1d6 */
    s32 camera_counter;         /* 1d8 */
} FieldSettings;

extern FieldSettings D_800B2174;

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
void func_800A3074(u16 reference, s32 value); /* write a variable */

/* Selected operands: when the given bit of `flags` is set the operand is a
 * signed immediate halfword, otherwise a variable reference. */
s32 func_8009CF78(s32 offset, s32 flags); /* bit 0x80 */
s32 func_8009CFBC(s32 offset, s32 flags); /* bit 0x40 */
s32 func_8009D000(s32 offset, s32 flags); /* bit 0x20 */
s32 func_8009D044(s32 offset, s32 flags); /* bit 0x10 */
s32 func_8009D088(s32 offset, s32 flags); /* bit 0x08 */
s32 func_8009D0CC(s32 offset, s32 flags); /* bit 0x04 */
s32 func_8009D110(s32 offset, s32 flags); /* bit 0x02 */
s32 func_8009D154(s32 offset, s32 flags); /* bit 0x01 */

extern s32 D_800AFC7C;              /* batch limit */

#define EVENT_OPERAND_BYTE(offset) (D_800ADC00[D_800B0078->pc + (offset)])

#endif
