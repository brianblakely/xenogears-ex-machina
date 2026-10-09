/* The menu overlay's common (uninitialized global) variables of up to eight
 * bytes. The original linker allocated them after every unit's small
 * variables, in an order of its own, at the end of the file (zero there), so
 * this unit, linked last, defines them, each in a slot of whole words
 * (decomp/Makefile, uninitialized variables). GCC emits tentative
 * definitions in the order of their first declaration, so they are defined
 * ahead of the headers that declare them; the headers then check their
 * types. */
#include "common.h"

s32 D_8009284C; /* horizontal distance between the actors */
s32 D_80092850;
struct PolyFT3 *D_80092854[2]; /* map triangle pool per draw buffer */
s16 D_8009285C; /* display width */
u8 D_80092860; /* left bar texel row */
u8 D_80092864; /* right bar texel row */
struct Window *D_80092868;
s16 D_8009286C; /* display height */
struct Window *D_80092870;
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

#include "menu.h"
#include "sparkle.h"
#include "scene.h"
#include "spark.h"
#include "sound.h"
#include "brain.h"
#include "window.h"
#include "gte.h"
