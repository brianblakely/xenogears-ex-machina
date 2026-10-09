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
 * that object's linked address. A byte or halfword view of its own would
 * share the word, which the slot rule gives the object alone
 * (decomp/Makefile); main_8001B6C4's window colour bytes cannot be an array
 * there (main_8001B6C4.c). */
D_80059409 = D_80059408 + 0x1;  /* reverb type */
D_8005940A = D_80059408 + 0x2;  /* reverb delay */
D_8005940B = D_80059408 + 0x3;  /* reverb feedback */
D_800594D5 = D_800594D4 + 0x1;  /* window colour green */
D_800594D6 = D_800594D4 + 0x2;  /* window colour blue */
D_80059531 = D_80059530 + 0x1;  /* CdlATV .val1 */
D_80059532 = D_80059530 + 0x2;  /* .val2 */
D_80059533 = D_80059530 + 0x3;  /* .val3 */
D_80059599 = D_80059598 + 0x1;  /* light colour .g */
D_8005959A = D_80059598 + 0x2;  /* .b */
D_8005A3C6 = D_8005A3C0 + 0x6;  /* .attr.mvol.right: 8001b970 clears 20 halfwords back from it */
D_8005A3EE = D_8005A3C0 + 0x2E; /* .cd_request */
D_80062524 = D_80062518 + 0xC;  /* wave bank slot 3 */
D_8006D635 = D_8006D634 + 0x1;  /* .names[0][1] */
D_8006EF58 = D_8006D634 + 0x1924; /* .gold */
D_8006F2E8 = D_8006D634 + 0x1CB4; /* .nameSlots */
D_8006F94E = D_8006D634 + 0x231A; /* .map */
D_8006F950 = D_8006D634 + 0x231C; /* .entry[0] */
D_8006F952 = D_8006D634 + 0x231E; /* .entry[1] */
D_8006F954 = D_8006D634 + 0x2320; /* .entry[2] */
D_8006F9DE = D_8006F9DC + 0x2;  /* the scene selector */
