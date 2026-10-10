#ifndef OVL2143_IMAGE_ANIM_H
#define OVL2143_IMAGE_ANIM_H

/* Image animations (battle/effect.h's ImageAnim): VRAM rectangles whose
 * pixels are rebuilt (by the resident's image decoders or by colour fades)
 * each time a frame curve selects another frame. */

#include "common.h"
#include "battle/effect.h"

s16 gear_model_frame_curve_rising(s16 value, s16 divisor, s16 base);
ImageAnim *gear_model_start_image_anim(ImageAnim *anim, ImageAnim *target, u16 mode, u16 flags, ColorRow *colors,
                         s16 x, s16 y, s16 z, s16 x2, s16 y2, s16 z2, s16 x3, s16 y3, s16 w, s16 h,
                         s16 speed, s16 divisor, s16 base, FrameCurve curve);
s16 gear_model_step_image_anim(ImageAnim *anim, s32 ticks);
void gear_model_stop_image_anim(ImageAnim *anim);
void gear_model_fade_image_anim(ImageAnim *anim, s16 level);
void gear_model_blend_image_anim(ImageAnim *anim, s16 level);
FrameCurve gear_model_get_frame_curve(s32 mode);

#endif
