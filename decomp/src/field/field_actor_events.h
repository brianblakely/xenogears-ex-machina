#ifndef FIELD_FIELD_ACTOR_EVENTS_H
#define FIELD_FIELD_ACTOR_EVENTS_H

#include "field.h"

/* Event-instruction helpers of the actor script interpreter (8009e1a0-800a5924). */
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

/* Player control (event a7). */
extern u16 D_800AFE9C;         /* held pad buttons */
extern u16 D_800C2694;         /* newly pressed pad buttons */
extern s16 D_800ADB02;         /* frames stuck against terrain */
extern s32 D_800ADB28;         /* latched jump setting */
extern s32 D_800ADB64;         /* jump contact, 0xff none */
extern s32 D_800ADB68;         /* pad input polled this pass */
extern u16 D_800ADF68[16];     /* d-pad direction per button state */
extern u16 D_800ADF88[16];     /* alternate d-pad directions */
extern void func_80079288(void);

extern s32 D_8006F990[3];    /* descriptor of each party slot's actor */
void func_8009E574(s32 x, s32 z);
void func_800A0158(s32 slot, s32 *a, s32 *b, s32 *c);
void func_800A0D3C(void);

extern u8 D_800AE294[];        /* sprite of each character */

s32 func_800A2FE0(s32 reference);  /* -1 when a variable is unsigned */
extern s32 D_800C4268;
/* The pieces' accumulated drift (x, y, z) at 800b21bc. */
#define PIECE_DRIFT_TOTAL ((s32 *)D_800B2078.unk21BC)
void func_800AD898(void);
extern s32 D_8004F30C;          /* returning to the field */

/* The play record (800a31e8). */
extern u8 D_800B02C8;
extern u16 D_800AFC6C;         /* buttons held since the last record */
extern s32 D_8004F2F4;
extern s32 D_8004F318;         /* frames since the play clock stepped */
extern s32 D_8004F328;
extern u8 D_80059418;
extern u8 D_80059420;
extern u8 D_80059484;
void func_800A30FC(void);
s32 func_8009FEE4(s32 slot);

/* Floor triangle of `layer` under (x, z), with its point and normal. */
s16 func_8007B1C4(s32 x, s32 z, s32 layer, SVECTOR *point, VECTOR *normal);

/* The field snapshot (8005a4e4) and its read cursor. */
extern u8 D_8005A4E4[];
extern u8 *D_800AFC50;
extern s32 D_8005A408[3];      /* party modes when saved */
void func_80021D50(FieldModel *model, u8 *checkpoint);
/* The view's world block (800afa54, 0x74 bytes) as copied byte-wise. */
typedef struct {
    u8 bytes[0x74];
} ViewSnapshot;

/* The screen distortion (800a484c). */
extern s32 D_800ADB24;         /* distortion buffers allocated */
extern RECT D_800AEB24[15];    /* saved strip sources */

/* The 801e effect layers (800a0fd8). */
extern u8 *D_801E8670[];
extern void func_801E8030(s32 layer);
/* The resident file-list entry of 80029afc (resident/cd.h FileRequest). */
typedef struct {
    u16 file;
    void *destination;
} LayerFileRequest;
typedef struct {
    u8 unk00[0x5C];
    s32 x;       /* 5C */
    u8 unk60[4];
    s32 z;       /* 64 */
} LayerModel;
typedef struct {
    u8 unk00[4];
    LayerModel *model; /* 04 */
    u8 unk08[0x1C - 0x08];
    s16 scale;         /* 1C */
    u8 unk1E[0x34 - 0x1E];
    u8 active;         /* 34 */
    u8 unk35[0x60 - 0x35];
    s16 y;             /* 60 */
} EffectLayer;
typedef struct {
    s32 motion[4][2];  /* 800b21ec */
    s32 scale[4];      /* 800b220c */
} LayerState;
#define LAYER_STATE (*(LayerState *)&D_800B2078.unk21E4[4])
extern LayerFileRequest D_800B2394[3];
extern void *D_8005A420[4];
extern void *D_8005A450[4];
s32 func_80029AFC(void *list, s32 mode, s32 a2);
void func_801E742C(s32 layer, s32 a1, void *data0, void *data1, s16 x, s32 a5, s32 a6, s16 a7, void *state);


void func_80021EBC(FieldModel *model, u8 *checkpoint);

/* Copy `size` bytes as one unaligned block (a byte-struct assignment). */
#define COPY_BLOCK(destination, source, size)                    \
    {                                                            \
        typedef struct {                                         \
            u8 bytes[size];                                      \
        } Block;                                                 \
        *(Block *)(destination) = *(Block *)(source);            \
    }

extern s32 D_800AFC74;         /* set when an initialization made a sprite */
void func_8002303C(FieldModel *model, s32, s32);
void func_800A3474(void);

s32 func_8009EB48(FieldActor *actor, s32 tag); /* -1 when a slot has `tag` */
s32 func_800A3090(s32 actor, s32 event);       /* entry PC of an actor's event */

#endif
