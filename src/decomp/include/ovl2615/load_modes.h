#ifndef OVL2615_LOAD_MODES_H
#define OVL2615_LOAD_MODES_H

#include "common.h"

/* The battle setup module's (ovl2615, 801E4000) setup phases and load modes,
 * as they are defined. The battle runs a load mode as its intro (its modes
 * 1-4) and the phases from its own intro (800B7870); the load modes run the
 * phases between their frames. The phase's prototype keeps the callers' u8
 * conversion. */
void battle_setup_run_phase(u8 phase);                  /* run one setup phase (battle_setup_phases.c) */
void battle_setup_run_shatter_load_mode(void);          /* the shatter transition (load_modes.c) */
void battle_setup_run_shatter_in_place_load_mode(void);
void battle_setup_run_burst_load_mode(void);            /* the burst transition (burst_modes.c) */
void battle_setup_run_burst_variant1_load_mode(void);

#endif
