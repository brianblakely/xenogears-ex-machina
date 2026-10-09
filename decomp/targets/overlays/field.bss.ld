/* The resident's mode table entry for the field (8001809c: 800af5e4,
 * 800c426c) clears the field BSS from the word after the first address
 * through the second (80019560), so the units' variables must link exactly
 * there. */
ASSERT(field_BSS_START == 0x800AF5E8 && field_BSS_END == 0x800C4270,
       "field: the linked .bss is not the BSS the resident's mode table clears")
