#ifndef FIELD_FIELD_DRAW_H
#define FIELD_FIELD_DRAW_H

/* The field frame's drawing (field.c): the draw blocks' switch, the model
 * and sprite passes, the compass and pointer markers, the screen fades, the
 * overlay sprites and the sprite block of field_800A9274.c. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "field/monitor.h"
#include "field.h"

void func_80073F50(void);      /* switch draw blocks, clear the overlay table */
void func_80073FE0(void);      /* swap the draw buffer, clear its tables */
void func_80075910(void);      /* finish the frame */

/* The model pass (800748e8): instances are culled by their bounding square
 * against the screen widened by the margins (the instances' refresh,
 * func_80073E38, is in field/monitor.h). */
extern s32 D_800C3A5C;         /* screen margin x */
extern s32 D_800C3A60;         /* screen margin y */
void func_800748E8(void);      /* draw the models */
s32 func_8007469C(void);       /* whether a shown descriptor has flag 0x8000 */
void func_800AA9DC(FieldInstance *instance); /* bounds from the mesh */
s32 func_800AAA74(FieldInstance *instance);  /* 0 when on screen, else -1 */

/* The sprite pass and the sprite factory (80076ac0). */
extern s32 D_800AFC74;         /* sprites created */
void func_80076AC0(s32 index, s32 slot, void *data, s32 kind, s32 bank, s32 unk, s32 flag);

/* A pointer marker: its quad's corners and primitive per buffer. */
typedef struct FieldMarker {
    SVECTOR v[4];
    POLY_FT4 poly[2];
} FieldMarker;

void func_8007AA44(FieldMarker *marker); /* set a marker up */
void func_8007AB6C(u_long *ot, FieldMarker *marker, MATRIX *m, s32 buffer); /* project and link */
void func_8007AC58(u_long *ot, FieldMarker *marker, MATRIX *m, s32 buffer); /* as a standing sprite */

/* The compass (80074108). */
extern FieldMarker D_800B06BC[25]; /* ring, letters, needle and pointer quads */
extern u16 D_800ADC24[8];      /* heading octant bit per palette row */
extern DVECTOR D_800ADC34[4];  /* letter x, z offsets */
extern s16 D_800ADB48;         /* needle heading */
extern s16 D_800ADB4A;         /* needle goal */
extern u16 D_800AFC08[16];     /* compass colours read back from VRAM */
extern u16 D_800AFD24[128];    /* compass palette */
extern RECT D_800B004C;        /* compass colour strip */
void func_8007A5C4(void);      /* build the four letters */
void func_8007A7F4(FieldMarker *record, s32 column, s32 row, s32 style); /* build a grid quad */

/* Screen fades: two channels in the work block (FadeChannel); a fade starts
 * with func_80071D08 (field/monitor.h). */
extern s32 D_800ADC04;         /* fade mode; fades start only in mode 2 */
extern s16 D_800ADC08;         /* fade started */
extern DR_MODE D_800AFE24[2];  /* fade draw mode per buffer */
extern RECT D_800AFE3C[2];     /* fade texture windows */
extern RECT D_800AFE4C;        /* fade copy source */
extern TILE D_800AFE54[2];     /* fade tile per buffer */
void func_80071A64(void);          /* set both channels' primitives up */
void func_8007D93C(s32 channel);   /* prepare a channel */
void func_80071DCC(s32 steps);     /* fade channel 0 out to white, once */
void func_80071E58(s32 steps);     /* fade it back in, once */
void func_80071CB4(void *ot, s32 buffer); /* step and draw both channels */
void func_8007DA44(u_long *ot, s32 buffer); /* link the active channels */

/* The five overlay sprites (800abd18), per sprite and draw buffer. */
typedef struct OverlaySprites {
    DR_MODE modes[5][2];
    SPRT sprites[5][2];
} OverlaySprites;

extern OverlaySprites D_800B0188;
void func_800ABD18(void);      /* set them up */
void func_800ABEC8(void);      /* link the current buffer's */

/* The sprite block (800aac08): 33 sprites, each with a draw mode, per draw
 * buffer. */
typedef struct FieldSprites {
    DR_MODE modes[33][2];
    SPRT sprites[33][2];
} FieldSprites;

extern FieldSprites *D_800AFC68;
void func_800AAC08(void);      /* allocate it */
void func_800AABD8(void);      /* release it */
void func_800AADC8(s32, s32, s32, s32); /* set a sprite's colour */
void func_800AAE4C(s32 index, s32 x, s32 y, s32 anchor); /* place and link a sprite */

#endif
