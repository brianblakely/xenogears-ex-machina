#ifndef OVL2143_IMAGE_ANIM_H
#define OVL2143_IMAGE_ANIM_H

/* Image animations (battle/effect.h's ImageAnim): VRAM rectangles whose
 * pixels are rebuilt (by the resident's image decoders or by colour fades)
 * each time a frame curve selects another frame. */

#include "common.h"
#include "battle/effect.h"

s16 func_801E08D4(s16 value, s16 divisor, s16 base);
ImageAnim *func_801E0A00(ImageAnim *anim, ImageAnim *target, u16 mode, u16 flags, ColorRow *colors,
                         s16 x, s16 y, s16 z, s16 x2, s16 y2, s16 z2, s16 x3, s16 y3, s16 w, s16 h,
                         s16 speed, s16 divisor, s16 base, FrameCurve curve);
s16 func_801E1258(ImageAnim *anim, s32 ticks);
void func_801E165C(ImageAnim *anim);
void func_801E1708(ImageAnim *anim, s16 level);
void func_801E17B8(ImageAnim *anim, s16 level);
FrameCurve func_801E34BC(s32 mode);

#endif
