/* The menu overlay's common (uninitialized global) variables. The menu's
 * assembler put those of up to eight bytes in .sbss and the larger ones in
 * .bss (menu.mk), and the original linker allocated each group's commons
 * after every unit's own, in an order of its own: the small ones at the end
 * of the file (zero there), the larger ones past it up to the end of the BSS
 * the resident's mode table clears (menu.bss.ld). This unit, linked last,
 * defines them in that order, each in a slot of whole words (decomp/Makefile,
 * uninitialized variables). GCC emits tentative definitions in the order of
 * their first declaration, so they are defined ahead of the headers that
 * declare them, structures by their tags; the headers then complete and
 * check the types. The SDK headers ahead of them declare none of them. */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/window.h"

s32 D_8009284C; /* horizontal distance between the actors */
s32 D_80092850;
POLY_FT3 *D_80092854[2]; /* map triangle pool per draw buffer */
s16 D_8009285C; /* display width */
u8 D_80092860; /* left bar texel row */
u8 D_80092864; /* right bar texel row */
struct DisplayBuffer *D_80092868;
s16 D_8009286C; /* display height */
struct DisplayBuffer *D_80092870;
struct MoveList *D_80092874; /* per model id */
s32 D_80092878; /* unreferenced */
u8 D_8009287C;
s32 D_80092880;
u8 D_80092884;
s32 D_80092888;
struct Environment *D_8009288C; /* current stage colours */
s32 D_80092890;
struct Actor *D_80092894; /* actor the scene script drives */
s16 D_80092898;
s32 D_8009289C;
u8 D_800928A0; /* buffer being built */
s32 D_800928A4[2]; /* unreferenced */
s32 D_800928AC;
s32 D_800928B0; /* selects the look-at marker (func_80082300 or func_80082178) */
u8 D_800928B4; /* stage */
s32 D_800928B8[2]; /* unreferenced */
u8 D_800928C0;
u8 D_800928C4; /* enables the retreat rule */
s32 D_800928C8; /* menu mode */
s32 D_800928CC;
u16 D_800928D0; /* debug display switches */
u8 D_800928D4;
u8 *D_800928D8; /* the 49 portraits, 0x1000 bytes each */
struct GroundSquare *D_800928DC;
s32 D_800928E0; /* unreferenced */
u32 *D_800928E4; /* ordering table primitives are added to */
s32 D_800928E8; /* owner of the segments started now */
struct ListEntry **D_800928EC;
u8 D_800928F0;
u8 D_800928F4;
s32 D_800928F8; /* recorded path points */
u8 D_800928FC; /* pad port driving the menus */
s32 D_80092900; /* bout-end sequence step */
s32 D_80092904; /* camera view */
s32 D_80092908;
s32 D_8009290C;
s32 D_80092910;
s32 D_80092914; /* colour changed this frame */
s32 D_80092918;
s32 D_8009291C;
u8 D_80092920;
s32 D_80092924;
s32 D_80092928; /* unreferenced */
s32 D_8009292C;
void (*D_80092930)(void *block);
s32 D_80092934;
u32 *D_80092938; /* ordering table of the buffer being built */
u8 D_8009293C;
s32 D_80092940;
s32 D_80092944;
s32 D_80092948;
s32 D_8009294C;
s32 D_80092950;

VECTOR D_80096FA8; /* last eye position: the scene origin */
struct SideHits D_80096FB8[2];
MATRIX D_80096FE0; /* screen scale */
VECTOR D_80097000; /* look-at work: third axis */
struct Actor D_80097010; /* scene actor */
VECTOR D_8009867C; /* camera eye */
Window D_8009868C; /* message window */
VECTOR D_8009871C; /* camera look-at point */
struct Actor D_8009872C; /* scene actor */
struct Settings D_80099D98; /* current option settings */
struct PolyFT4Words D_80099DA8[2][10];
VECTOR D_8009A0C8; /* look-at work: forward */
struct DisplayBuffer D_8009A0D8[2]; /* the display buffers */
VECTOR D_8009A2C8; /* mesh light direction */
MATRIX D_8009A2D8;
struct OverlayBuffer D_8009A2F8[2];
VECTOR D_8009A918; /* look-at work: up */
struct PathMarker D_8009A928[30]; /* recorded path points */

#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"
