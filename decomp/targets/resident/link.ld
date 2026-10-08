/* Symbols of the resident link that no object defines. */

/* Header field (decomp/src/resident/header.c): the loader reads whole
 * 2048-byte sectors, so the loaded size is the program image (.main without
 * its NOLOAD BSS) rounded up to 2048 bytes. objcopy --pad-to fills the file
 * to the same boundary. */
__exe_text_size = ALIGN(SIZEOF(.main), 0x800);
