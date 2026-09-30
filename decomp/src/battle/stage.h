#ifndef BATTLE_STAGE_H
#define BATTLE_STAGE_H

/* The battle stage: its hierarchy, geometry and image animations, drawn each
 * frame (800A4654-800A7948). */

#include "common.h"
#include "model.h"
#include "scene.h"
#include "effect.h"

/* Run the calls between the two on a stack at the top of the scratchpad. */
#define SPAD_STACK_ENTER()                                                                         \
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"            \
                     :                                                                             \
                     : "r"(0x1F8003FC)                                                             \
                     : "$8", "memory")
#define SPAD_STACK_LEAVE() __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory")

/* The stage geometry block (D_800C3EA0; fields as far as recovered). */
typedef struct {
    u8 pad0[0x16DC];
    POLY_F4 flats[4]; /* 0x16DC */
    POLY_G4 quads[4]; /* 0x173C */
} StageGeometry;

/* The stage backdrop (the first resident handle of D_800C3D50; fields as far
 * as recovered). */
typedef struct {
    u8 pad0[0x280];
    POLY_F4 flats[4]; /* 0x280 */
    POLY_G4 quads[2]; /* 0x2E0 */
} StageBackdrop;

/* The stage's lit colours in their saved order (0x88 bytes): per side the
 * geometry quad's four corners, its flat and the backdrop's flat, then the
 * backdrop quads' corners and the two colours of D_800D2D40/D_800D2D48. */
typedef struct {
    struct {
        CVector quad[4];
        CVector flat;
        CVector backdropFlat;
    } sides[4];
    CVector backdropQuads[2][4];
    CVector extra[2];
} StageColors;

extern ModelList *D_800C3E48; /* the stage's models (hierarchy D_800C3E38) */
extern s32 D_800CCC5C;        /* frame steps */
extern ImageAnim D_800D3600;  /* the stage's image animation */
extern StageColors *D_800C3AC4; /* the stage's colours as loaded */
extern StageColors *D_800C3AC8; /* their working copy */
extern s16 D_800D2D2C;          /* stage image width */
extern s16 D_800D2D30;          /* stage image x */
extern s16 D_800D2D34;          /* stage image y */
extern s16 D_800C3EA8;          /* stage image height */

/* Resident services. */
void func_80027EAC(ResidentRecord18 *record);
void func_80025D4C(s32 size, u16 *out, u16 *a, u16 *b, s32 r, s32 g, s32 bl, s32 mode, s32 level);
void func_800273C4(void *handle, s32 arg1, s32 arg2, Matrix *view, u32 *ot, s32 buffer);

void func_800A48EC(ModelList *models, ModelPart *root, Matrix *view, s32 arg3, s32 arg4, u32 *ot, s32 buffer,
                   s32 depth);
void func_800A4DB8(void *geometry, s32 arg1, s32 arg2, Matrix *view, u32 *ot, s32 buffer);
void func_800A64E4(void);
void func_800A6884(u8 *out, s32 index, u8 *color);
void func_800A6AE8(void);

#endif
