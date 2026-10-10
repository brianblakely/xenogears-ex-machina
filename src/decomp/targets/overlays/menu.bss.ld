/* The resident's mode table entry for the menu (800180cc: 800925d0,
 * 8009b554) clears the menu BSS from the word after the first address
 * through the second (80019560), so the units' variables must link exactly
 * there: the .sbss closing the file, the .bss from the program's end on. */
ASSERT(menu_SBSS_START == 0x800925D4 && menu_SBSS_END == menu_BSS_START &&
       menu_BSS_END == 0x8009B558,
       "menu: the linked .sbss and .bss are not the BSS the resident's mode table clears")

/* arena_brain_run_practice_command reads the opponent's command, arena_settings.command, by a name
 * of its own: as a member, GCC keeps the byte's address in a register for
 * its three reads, where the original loads it absolutely at each. */
arena_settings_command = arena_settings + 0xA; /* 80099DA2 */
