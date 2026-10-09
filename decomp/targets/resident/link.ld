/* Symbols of the resident link that no object defines. */

/* Header field (decomp/src/resident/header.c): the loader reads whole
 * 2048-byte sectors, so the loaded size is the program image (.main without
 * its NOLOAD BSS) rounded up to 2048 bytes. The file ends there: objcopy
 * pads it to __exe_file_size (PAD_TO_SYMBOL), the header's sector and the
 * loaded size. */
__exe_text_size = ALIGN(SIZEOF(.main), 0x800);
__exe_file_size = LOADADDR(.main) + __exe_text_size;
