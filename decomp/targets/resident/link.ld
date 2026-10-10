/* Symbols of the resident link that no object defines. */

/* Header field (decomp/src/resident/header.c): the loader reads whole
 * 2048-byte sectors, so the loaded size is the program image (.main without
 * its NOLOAD BSS) rounded up to 2048 bytes. The file ends there: objcopy
 * pads it to __exe_file_size (PAD_TO_SYMBOL), the header's sector and the
 * loaded size. */
__exe_text_size = ALIGN(SIZEOF(.main), 0x800);
__exe_file_size = LOADADDR(.main) + __exe_text_size;

/* $gp: the -G8 units and the PsyQ libraries address the small data
 * relative to _gp, which the entry point loads (boot_reset_stack_and_gp.s). The
 * original's, 80059170, is the first byte of the small data, so the link
 * derives it from there. */
_gp = main_SDATA_START;

/* The last word of the BSS: the entry point (boot_entry_point.s) and the mode
 * table's resident entries (main.c) clear the words after heap_report_output, the
 * last of the small data, through it. The BSS ends below the overlay area
 * at 8006faf0, so the link checks the bounds the units' variables reach. */
boot_bss_last_word = main_BSS_END - 4; /* 8006FAEC */

/* The overlay area the modes load at, right after the BSS (main.c, the
 * unpack buffer of model_renderer.c): each mode overlay's first unit defines
 * its first word, the overlay's number, under a name of its own, so the
 * resident takes the address from here, as splat's list gave it before. */
mode_overlay_area = 0x8006FAF0;
ASSERT(main_BSS_START == 0x800592BC && main_BSS_END == 0x8006FAF0,
       "resident: the linked .bss is not the span the entry point clears")

/* Names the resident code uses for parts of uninitialized objects, each from
 * that object's linked address, where addressing the object compiles
 * differently: mode_battle_and_menu's window colour bytes cannot be an array there
 * (mode_battle_and_menu.c), 8003885c stores the CD mix bytes as scalars, and
 * 8001b970 reads the name slots' second bytes from a base of their own. */
window_color_green = window_color + 0x1;  /* 800594D5: window colour green */
window_color_blue = window_color + 0x2;  /* 800594D6: window colour blue */
sound_cd_mix_left_to_right = sound_cd_mix + 0x1;  /* 80059531: CdlATV .val1 (console_and_sound_driver.c) */
sound_cd_mix_right_to_right = sound_cd_mix + 0x2;  /* 80059532: .val2 */
sound_cd_mix_right_to_left = sound_cd_mix + 0x3;  /* 80059533: .val3 */
game_data_name_code_high_bytes = game_data + 0x1;  /* 8006D635: .names[0][1] (mode_battle_and_menu.c) */
