#ifndef PSYQ_ABS_H
#define PSYQ_ABS_H

/* PsyQ abs.h: the absolute value macro (abs() itself is in psyq/libc.h). Its
 * x >= 0 test comes first; ovl3381's battle_module_tiles_start needs that order (with
 * x < 0 first its branches come out reversed), while the battle and the world
 * map build the same either way. */
#define ABS(x) (((x)>=0)?(x):(-(x)))

#endif
