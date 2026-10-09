/* Names the world map source uses for an address inside a C object, from
 * that object's linked address (worldmap_common.c). Two loaders pass the
 * shared read list from its first destination member (WORLD_READ_LIST,
 * worldmap.h). */
D_8009D3FC = D_8009D3F8 + 0x4; /* [0].dest */

/* The resident's mode table (8001808c, mode 3) clears the word after
 * 8009bbb0 through the word at 8009d80c for this overlay (80019560); nothing
 * in the image addresses the end of its last variable, so the link checks
 * it. */
ASSERT(worldmap_BSS_START == 0x8009BBB4 && worldmap_BSS_END == 0x8009D810,
       "the world map .bss differs from the resident mode table's bounds")
