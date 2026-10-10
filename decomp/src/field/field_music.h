#ifndef FIELD_FIELD_MUSIC_H
#define FIELD_FIELD_MUSIC_H

#include "common.h"

/* Per music, two bytes: the wave file to load (0xff none) and 1 when the
 * shared wave bank is released first. The table is static here, so each
 * unit that includes this header gets its own copy at the start of its data:
 * field_event.c reads its copy (800adfcc); field.c's (800ada68) is never
 * read. */
static u8 field_music_wave_table[73 * 2] = { /* 800ADFCC */
    0, 0, 1, 0, 2, 0, 3, 0, 4, 0, 5, 0, 6, 0, 7, 0,
    8, 0, 9, 0, 10, 1, 255, 0, 255, 0, 255, 0, 255, 0, 15, 0,
    16, 0, 17, 0, 17, 0, 8, 0, 20, 0, 16, 0, 22, 0, 16, 0,
    24, 0, 25, 0, 24, 0, 16, 0, 17, 0, 255, 0, 30, 1, 31, 1,
    32, 1, 33, 1, 34, 1, 35, 0, 35, 1, 37, 0, 38, 0, 39, 1,
    40, 0, 41, 0, 42, 1, 43, 0, 44, 0, 45, 0, 46, 0, 47, 0,
    48, 1, 49, 1, 50, 1, 51, 1, 52, 1, 53, 1, 54, 1, 55, 1,
    56, 1, 57, 1, 58, 0, 22, 0, 60, 0, 61, 1, 62, 1, 63, 0,
    40, 0, 65, 1, 66, 0, 6, 0, 68, 1, 69, 1, 70, 0, 71, 0,
    72, 0,
};

#endif
