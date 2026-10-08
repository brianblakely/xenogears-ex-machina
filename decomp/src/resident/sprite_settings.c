/* Initialized small globals between the first sprite unit's .sdata and the
 * second's. Every sprite unit addresses them absolutely while it addresses
 * small data of its own through $gp, and GCC writes a unit's data ahead of
 * its code, so they are no sprite unit's own; a data-only unit (GP 8 in the
 * target fragment), linked between the two, defines them. */
#include "common.h"
#include "psyq/libgpu.h"
#include "mode.h"
#include "sprite.h"

s32 D_80059198 = 0; /* extra frames per update */
SpriteVoice *D_8005919C = NULL;
s32 D_800591A0 = 0; /* unreferenced */
s32 D_800591A4 = 0; /* unreferenced */
s32 D_800591A8 = 0x2000;
u8 D_800591AC = 0; /* new main-list tasks count as active */
u8 D_800591AD = 0;
u8 D_800591AE = 0;
u8 D_800591AF = 0; /* allocation mode for sprite tasks */
u8 D_800591B0 = 1;
u8 D_800591B1 = 1; /* unreferenced */
u8 D_800591B2 = 0; /* unreferenced */
u8 D_800591B3 = 0;
s32 D_800591B4 = 0; /* unreferenced */
