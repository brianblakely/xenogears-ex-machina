/* 80032DCC: Heap report to a PC host file. This unit owns the report file handle (a
 * $gp-relative static in its .sbss) and, unlike the heap unit, addresses
 * other units' small globals absolutely: 80032E04 stores the report output
 * hook heap_report_output with lui/sw, while the heap unit's 80032BDC loads it
 * through $gp. */
#include "common.h"

#include "psyq/libc.h"
#include "psyq/libsn.h"
#include "resident/console.h"
#include "resident/heap.h"

static s32 heap_report_file; /* 80059348: host file of the heap report */

/* 80032DCC: Report output to the host file. */
void heap_write_report_line(char *line) {
    PCwrite(heap_report_file, line, strlen(line));
}

/* 80032E04: Write the full heap report to the host file `name`. */
void heap_write_report_file(char *name) {
    PCinit();
    heap_report_file = PCcreat(name, 0);
    heap_report_output = heap_write_report_line;
    heap_print_report(1, 0, 0, -1);
    heap_report_output = (void (*)(char *))console_report_printf;
    PCclose(heap_report_file);
}
