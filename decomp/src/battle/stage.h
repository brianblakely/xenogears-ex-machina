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

extern ModelList *D_800C3E48; /* the stage's models (hierarchy D_800C3E38) */
extern s32 D_800CCC5C;        /* frame steps */
extern ImageAnim D_800D3600;  /* the stage's image animation */
extern void *D_800C3AC4;      /* saved stage colours */
extern void *D_800C3AC8;

/* Resident services. */
void func_80027EAC(ResidentRecord18 *record);
void func_800273C4(void *handle, s32 arg1, s32 arg2, Matrix *view, u32 *ot, s32 buffer);

void func_800A48EC(ModelList *models, ModelPart *root, Matrix *view, s32 arg3, s32 arg4, u32 *ot, s32 buffer,
                   s32 depth);
void func_800A4DB8(void *geometry, s32 arg1, s32 arg2, Matrix *view, u32 *ot, s32 buffer);
void func_800A64E4(void);
void func_800A6884(u8 *out, s32 index, u8 *color);
void func_800A6AE8(void);

#endif
