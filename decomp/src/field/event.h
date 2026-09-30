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
    s16 triangle[4];     /* 008: located triangle per layer */
    s16 layer;           /* 010 */
    u8 unk012[0x01A - 0x012];
    s16 height;          /* 01A */
    u8 unk01C[0x020 - 0x01C];
    Fixed position[3];  /* 020: x, y, z */
    u8 unk02C[0x030 - 0x02C];
    s32 unk30[3];        /* 030 */
    u8 unk03C[0x040 - 0x03C];
    s32 unk40[3];        /* 040 */
    u8 unk04C[0x060 - 0x04C];
    s16 unk60;           /* 060 */
    u8 unk062[0x064 - 0x062];
    s16 unk64;           /* 064 */
    u8 unk066[0x070 - 0x066];
    s16 unk70;           /* 070 */
    u8 unk072[0x078 - 0x072];
    u16 call_stack[4];   /* 078: return PCs */
    u8 unk080[0x0CC - 0x080];
    u16 pc;             /* 0CC: working PC, relative to the bytecode */
    u8 slot;            /* 0CE: selected script slot */
    u8 unk0CF[0x0E2 - 0x0CF];
    u8 unkE2;            /* 0E2 */
    u8 unk0E3[0x0EE - 0x0E3];
    s16 unkEE;          /* 0EE */
    u8 unk0F0[0xFC - 0x0F0];
    u8 color0[3];       /* 0FC */
    u8 color1[3];       /* 0FF */
    u8 unk102[0x104 - 0x102];
    u16 unk104;          /* 104 */
    u16 unk106;          /* 106 */
    u8 unk108[0x118 - 0x108];
    s32 *words;         /* 118 */
    u8 unk11C[0x12C - 0x11C];
    u32 unk12C;          /* 12C */
    u8 unk130[0x134 - 0x130];
    u32 unk134;         /* 134 */
} FieldActor;

/* The object at descriptor offset 04. */
typedef struct FieldModel {
    u8 unk00[0x0C];
    s32 unk0C;          /* 0C */
    u8 unk10[0x14 - 0x10];
    s32 unk14;          /* 14 */
    s32 unk18;          /* 18 */
} FieldModel;

/* One 0x5C-byte event descriptor; one per event actor. */
typedef struct FieldDescriptor {
    u8 unk00[0x04];
    FieldModel *model;  /* 04 */
    u8 unk08[0x2C - 0x08];
    MATRIX transform;    /* 2C */
    FieldActor *actor;  /* 4C */
    SVECTOR rotation;   /* 50 */
    u16 flags;          /* 58 */
    u8 unk5A[0x5C - 0x5A];
} FieldDescriptor;

/* One 0xA4-byte character record (src/reconstruction/battle_levels.cpp). */
typedef struct Character {
    u8 unk00[0x4C];
    u16 hp;             /* 4C */
    u16 max_hp;         /* 4E */
    u16 ep;             /* 50 */
    u16 max_ep;         /* 52 */
    u8 unk54[0xA4 - 0x54];
} Character;

/* Resident persistent game state (*8005a39c). */
typedef struct GameState {
    u8 unk0000[0x026C - 0x0000];
    Character characters[11]; /* 026C: count unverified */
    u8 unk0978[0x1924 - 0x0978];
    s32 gold;            /* 1924 */
    u8 unk1928[0x1932 - 0x1928];
    s16 unk1932;         /* 1932 */
    u8 unk1934[0x1D30 - 0x1934];
    u16 unk1D30;         /* 1D30: flag bits */
    u8 unk1D32[0x1D38 - 0x1D32];
    u8 count1[100];      /* 1D38: inventory list 1 */
    u8 id1[100];         /* 1D9C */
    u8 count2[200];      /* 1E00: inventory list 2 */
    u8 id2[200];         /* 1EC8 */
    u8 count0[150];      /* 1F90: inventory list 0 */
    u8 id0[150];         /* 2026 */
    u8 count3[100];      /* 20BC: inventory list 3 */
    u8 id3[100];         /* 2120 */
    u8 count4[150];      /* 2184: inventory list 4 */
    u8 id4[150];         /* 221A */
    u8 unk22B0[0x2320 - 0x22B0];
    s16 unk2320;         /* 2320 */
    u8 unk2322[0x2324 - 0x2322];
} GameState;

extern GameState *D_8005A39C;
extern s32 D_80062590[3];           /* party character ids (0xFF: empty) */
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

/* Field mode block at 800b233c; addressed as one aggregate. */
typedef struct FieldModes {
    s16 effects_kept;           /* 33c */
    s16 unk33E;                 /* 33e */
    s16 repeat_delay;           /* 340 */
    s16 repeat_remaining;       /* 342 */
    s16 jump_mode;              /* 344 */
    s16 animation_mode;         /* 346 */
    s16 gather_override;        /* 348 */
    s16 unk34A;                 /* 34a */
    s16 unk34C;                 /* 34c */
    s16 followers_idle;         /* 34e */
} FieldModes;

extern FieldModes D_800B233C;

typedef union {
    u32 word;
    u8 bytes[4];
} Attribute;

/* A 14-byte collision triangle. */
typedef struct {
    u8 unk00[0x0C];
    u8 attribute;       /* 0C */
    u8 unk0D;
} Triangle;

/* Field scene block at 800afa64 (meanings from src/reconstruction/
 * original_layout.cpp and src/analysis/field_memory.cpp). Addressed as one
 * aggregate: the original derives member addresses from each other. */
typedef struct FieldScene {
    MATRIX scaled_world;            /* a64 */
    MATRIX unkA84;                  /* a84 */
    MATRIX world;                   /* aa4 */
    s32 scale;                      /* ac4 */
    u8 lights[0x3C];                /* ac8 */
    s16 back_color[3];              /* b04 */
    u8 unkB0A[0xB10 - 0xB0A];
    FieldDescriptor *descriptors;   /* b10 */
    s32 unkB14;                     /* b14 */
    void *collision;                /* b18 */
    s32 unkB1C;                     /* b1c */
    Attribute *attributes;          /* b20 */
    Triangle *triangles[4];         /* b24 */
    void *vertices[4];              /* b34 */
    s32 triangle_counts[4];         /* b44 */
    s16 layer_count;                /* b54 */
} FieldScene;

extern FieldScene D_800AFA64;

/* Trigger zone (field component 8): four x, y, z corners. */
typedef struct {
    s16 x;
    s16 y;
    s16 z;
} ZonePoint;

typedef struct {
    ZonePoint corner[4];
} Zone;

extern Zone *D_800ADBF4;            /* trigger zones */
extern s32 D_800B226C;              /* controlled actor index */
s32 func_8004A70C(s32 a, s32 b, s32 point); /* side of edge a-b */

extern u8 *D_800ADC00;              /* event bytecode */
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

void func_80072254(s32 index);    /* reapply a descriptor's rotation */
void func_80085634(s32 a, s32 b);

#define EVENT_OPERAND_BYTE(offset) (D_800ADC00[D_800B0078->pc + (offset)])

#endif
