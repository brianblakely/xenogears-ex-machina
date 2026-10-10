#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

/*
 * The port build's view of decomp/include/include_asm.h. Nothing is linked from
 * assembly or from the original image: a function a unit includes as assembly
 * is defined in decomp/port (the SDK, the BIOS and the handwritten routines as
 * portable C), and data included from the image lives at its original address
 * in game memory, which the images the game loads fill.
 */
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)
#define INCLUDE_ASSET(SECTION, NAME, VRAM, SIZE)
#define INCLUDE_ORIGINAL(SECTION, NAME, VRAM, SIZE)
#define INCLUDE_ORIGINAL_UNALIGNED(SECTION, NAME, VRAM, SIZE)

#include "xem/port.h"

#endif
