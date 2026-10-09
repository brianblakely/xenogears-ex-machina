/* Symbols of the resident link that no object defines. */

/* Header field (decomp/src/resident/header.c): the loader reads whole
 * 2048-byte sectors, so the loaded size is the program image (.main without
 * its NOLOAD BSS) rounded up to 2048 bytes. The file ends there: objcopy
 * pads it to __exe_file_size (PAD_TO_SYMBOL), the header's sector and the
 * loaded size. */
__exe_text_size = ALIGN(SIZEOF(.main), 0x800);
__exe_file_size = LOADADDR(.main) + __exe_text_size;

/* The last word of the BSS: the entry point (func_80019524.s) and the mode
 * table's resident entries (main.c) clear the words after D_800592B8, the
 * last of the small data, through it. The BSS ends below the overlay area
 * at 8006faf0, so the link checks the bounds the units' variables reach. */
D_8006FAEC = main_BSS_END - 4;
ASSERT(main_BSS_START == 0x800592BC && main_BSS_END == 0x8006FAF0,
       "resident: the linked .bss is not the span the entry point clears")

/* Names the resident code uses for parts of uninitialized objects, each from
 * that object's linked address, where addressing the object compiles
 * differently: main_8001B6C4's window colour bytes cannot be an array there
 * (main_8001B6C4.c), 8003885c stores the CD mix bytes as scalars, and
 * 8001b970 reads the name slots' second bytes from a base of their own. */
D_800594D5 = D_800594D4 + 0x1;  /* window colour green */
D_800594D6 = D_800594D4 + 0x2;  /* window colour blue */
D_80059531 = D_80059530 + 0x1;  /* CdlATV .val1 (main2_800366E0.c) */
D_80059532 = D_80059530 + 0x2;  /* .val2 */
D_80059533 = D_80059530 + 0x3;  /* .val3 */
D_8006D635 = D_8006D634 + 0x1;  /* .names[0][1] (main_8001B6C4.c) */
