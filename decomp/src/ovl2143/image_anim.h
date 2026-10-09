#ifndef OVL2143_IMAGE_ANIM_H
#define OVL2143_IMAGE_ANIM_H

/* Image animations: VRAM rectangles whose pixels are rebuilt (by the
 * resident's image decoders or by colour fades) each time a frame curve
 * selects another frame. */

#include "common.h"
#include "psyq/libgpu.h"

/* A curve mapping time to a frame: func_801E0850/08d4/0938/0988 (called
 * without a prototype: time, divisor, base). */
typedef s16 (*FrameCurve)();

/* A row of three colours. */
typedef struct {
    u16 c[3];
} ColorRow;

/* An image animation (0x30 bytes, an actor's records30): a VRAM rectangle
 * whose pixels are rebuilt each time the curve selects another frame. */
typedef struct ImageAnim {
    struct ImageAnim *target; /* +0: image the frames are copied into */
    u16 *pixels;            /* +4 */
    u16 *pixels2;           /* +8 */
    u16 *work;              /* +c */
    u8 mode;                /* +10: 0/1 resident decoders, 4/5 fades */
    u8 dirty;               /* +11 */
    s16 h12;                /* +12 */
    u16 time;               /* +14 */
    u16 speed;              /* +16 */
    u16 frame;              /* +18 */
    u16 active;             /* +1a */
    ColorRow *colors;       /* +1c */
    s16 divisor;            /* +20 */
    s16 base;               /* +22 */
    FrameCurve curve;       /* +24 */
    RECT rect;              /* +28 */
} ImageAnim;

s16 func_801E08D4(s16 value, s16 divisor, s16 base);
ImageAnim *func_801E0A00(ImageAnim *anim, ImageAnim *target, u16 mode, u16 flags, ColorRow *colors,
                         s16 x, s16 y, s16 z, s16 x2, s16 y2, s16 z2, s16 x3, s16 y3, s16 w, s16 h,
                         s16 speed, s16 divisor, s16 base, FrameCurve curve);
s16 func_801E1258(ImageAnim *anim, s32 ticks);
void func_801E165C(ImageAnim *anim);
void func_801E1708(ImageAnim *anim, s16 level);
void func_801E17B8(ImageAnim *anim, s16 level);
FrameCurve func_801E34BC(s32 type);

#endif
