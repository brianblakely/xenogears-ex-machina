/* PS-X EXE header of the resident program (both discs): the first 2048-byte
 * sector of SLUS_006.64/69, written by the PsyQ tools from the link. The BIOS
 * loads t_size bytes from file offset 0x800 to t_addr, sets $sp and $fp to
 * s_addr and jumps to pc0. */
#include "common.h"
#include "psyq/kernel.h"
#include "resident/mode.h"

/* Names the link defines (the target's linker scripts). */
extern char main_VRAM[];      /* load address: the start of the program image */
extern char __exe_text_size[]; /* loaded size, a whole number of sectors (link.ld) */

/* The header fills its sector; the rest is zero. */
typedef struct {
    struct XF_HDR xf;
    char unused[0x800 - sizeof(struct XF_HDR)];
} ExeFileHeader;

ExeFileHeader exe_header = {
    {
        "PS-X EXE",
        0,
        0,
        {
            (unsigned long)func_80019524,
            0, /* $gp is set by the entry code (0x80059170) */
            (unsigned long)main_VRAM,
            (unsigned long)__exe_text_size,
            0,
            0,
            0, /* the entry code clears its own BSS */
            0,
            0x801FFFF0, /* initial stack below the top of the 2 MiB RAM */
            0,
        },
        "Sony Computer Entertainment Inc. for North America area",
    },
};
