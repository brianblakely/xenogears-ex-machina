#ifndef FIELD_FIELD_EVENT_H
#define FIELD_FIELD_EVENT_H

/* The event interpreter (field_800854D0.c): the event package and bytecode,
 * the event variable bank, the current actor, the operand readers and the
 * instruction helpers, the state some instructions keep, and the parameters
 * events set for the panorama. */

#include "common.h"
#include "psyq/libgte.h"
#include "field.h"

/* The event package (field component 5, *800adbf8). */
typedef struct {
    u32 unsigned_bits[32]; /* 00: bit per variable read unsigned */
    s32 count;             /* 80: actors */
    u16 entries[32];       /* 84: 32 event entry PCs per actor */
} EventPackage;

extern EventPackage *D_800ADBF8;
extern s32 D_800ADBFC;         /* event actor count */
extern u8 *D_800ADC00;         /* event bytecode */
extern void (*D_800AE6A0[])(void); /* extended event instructions */
extern s16 D_800C3A68[0x400];  /* event variable bank */
extern FieldActor *D_800B0078; /* current event actor */
extern s32 D_800AFD1C;         /* current actor index */
extern FieldDescriptor *D_800B06B8; /* descriptor of the running actor */
extern s32 D_800AFFEC;
extern s32 D_800B00C0;         /* yield */
extern s32 D_800AFC7C;         /* batch limit */

s32 func_800A2030(void);       /* run every active actor's script for this frame */
s32 func_800A1EC8(s32 limit);  /* run the current actor's instructions */
s32 func_800A3090(s32 actor, s32 event); /* entry PC of an actor's event */
s32 func_8009EB48(FieldActor *actor, s32 tag); /* -1 when a slot carries `tag` */

/* Variables: a reference is a byte offset into the bank. */
s32 func_800A3018(s32 reference);          /* read */
void func_800A3074(s32 reference, s32 value); /* write */
s32 func_800A2FE0(s32 reference);          /* -1 when read unsigned */

/* Operand readers; each takes the byte offset from the working PC. */
#define EVENT_OPERAND_BYTE(offset) (D_800ADC00[D_800B0078->pc + (offset)])
s32 func_800ACD7C(s32 offset);  /* signed halfword */
s32 func_800ACDB8(s32 offset);  /* raw halfword */
s32 func_800ACDEC(s32 offset);  /* bit 15 immediate, else a variable */
s32 func_8009CD7C(s32 offset);  /* actor selector */
s32 func_8009CDB4(s32 offset);  /* actor selector; 0xff when none */
s32 func_8008CF3C(s32 id);      /* resolve a character id (fd-ff: party members) */
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

/* Instruction helpers. */
void func_8008E0DC(s32 flags); /* continue past 5 bytes when `flags` has a bit of op1, else jump */
void func_8008E148(s32 flags); /* the same past 4 bytes */
void func_8008E498(s32 value); /* store `value` in variable op1 */
void func_8008E718(void);      /* draw distinct random numbers into the work block */
void func_8008A93C(void);      /* event fe 4d: the current actor's animation override */
void func_80092F44(void);      /* publish the field id in variables 4, 6 and 8 */
void func_800A0D3C(void);      /* give the current actor the field's first sprite */
extern u32 *D_800B1F74;        /* TIM image held by instruction 0x77 */

/* An actor's boundary quadrilateral (+114 while state bit 12 is set). */
typedef struct {
    s16 x;
    s16 z;
} BoundaryCorner;

typedef struct {
    BoundaryCorner corners[4];
} ActorBoundary;

/* An actor's integer position cached at +68 (x, y, z). */
#define ACTOR_CACHED_POSITION(actor) ((s16 *)((u8 *)(actor) + 0x68))

/* The pieces' accumulated drift (x, y, z) at 800b21bc. */
#define PIECE_DRIFT_TOTAL ((s32 *)D_800B2078.unk21BC)

/* The actor list (+118) the morph channels read through 80080a18: whose
 * list and where. */
extern s32 D_800ADB58;         /* descriptor whose list is read */
extern s32 D_800ADB5C;         /* list position */

/* Parameters events set at 800b0080, one object: the panorama's (resident
 * 8002709c), built by the field load when `enabled`. */
typedef struct FieldEventParams {
    s16 unk80[8]; /* 800b0080 */
    s32 unk90;    /* 800b0090 */
    s32 unk94;    /* 800b0094 */
    s32 unk98;    /* 800b0098 */
    u8 unk9C[4];
    u8 unkA0[3];  /* 800b00a0 */
    u8 unkA3;
    u8 unkA4[3];  /* 800b00a4 */
    u8 unkA7;
    u8 unkA8[3];  /* 800b00a8 */
    u8 unkAB;
    s16 unkAC;    /* 800b00ac */
    s16 unkAE;    /* 800b00ae */
    s16 unkB0;    /* 800b00b0 */
    s16 enabled;  /* 800b00b2 */
} FieldEventParams;

extern FieldEventParams D_800B0080;

#endif
