/* Heap report to a PC host file. This unit owns the report file handle (a
 * $gp-relative static in its .sbss) and, unlike the heap unit, addresses
 * other units' small globals absolutely: 80032E04 stores the report output
 * hook D_800592B8 with lui/sw, while the heap unit's 80032BDC loads it
 * through $gp. */
#include "common.h"

#include "psyq/libc.h"
#include "psyq/libsn.h"
#include "resident/console.h"
#include "resident/heap.h"

static s32 D_80059348; /* host file of the heap report */

/* Report output to the host file. */
void func_80032DCC(char *line) {
    func_8004C470(D_80059348, line, strlen(line));
}

/* Write the full heap report to the host file `name`. */
void func_80032E04(char *name) {
    func_8004C38C();
    D_80059348 = PCcreat(name, 0);
    D_800592B8 = func_80032DCC;
    func_8003278C(1, 0, 0, -1);
    D_800592B8 = func_800379C8;
    PCclose(D_80059348);
}
