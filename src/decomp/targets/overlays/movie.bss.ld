/* The resident's mode table entry for the movie (800180ec: 80076f38,
 * 80077454) clears the movie BSS from the word after the first address
 * through the second (80019560), so the unit's variables must link exactly
 * there. */
ASSERT(movie_BSS_START == 0x80076F3C && movie_BSS_END == 0x80077458,
       "movie: the linked .bss is not the BSS the resident's mode table clears")
