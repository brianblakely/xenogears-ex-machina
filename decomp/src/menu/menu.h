#ifndef MENU_H
#define MENU_H

#include "common.h"

/* libgte-layout vector: three 32-bit components and padding. */
typedef struct {
    s32 vx;
    s32 vy;
    s32 vz;
    s32 pad;
} Vector;

/* Menu camera: eye position (D_8009867C) and look-at point (D_8009871C). */
extern Vector D_8009867C;
extern Vector D_8009871C;
extern s32 D_800925F4; /* vertical camera lift of the current view */
extern Vector D_8009872C;
extern Vector D_80097010;
extern s32 D_80097064;
extern Vector D_80099078;
extern s32 D_80098780;
extern u8 D_80092954[];

s32 func_8003F8B0(s32 angle); /* sine, 4096 = 1.0 */
s32 func_8003F8CC(s32 angle); /* cosine, 4096 = 1.0 */
void func_800346D4(void *arg);
void func_80083C0C(s32 arg);
void func_80083738(Vector *dst, Vector *src);
void func_800828F8(Vector *position, Vector *step, s32 limit);
s32 func_80082488(Vector *position, s32 arg);

#endif
