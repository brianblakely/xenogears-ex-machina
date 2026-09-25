/* SPDX-License-Identifier: GPL-2.0-or-later
 * Optional host-side observation extension for the separately built emulator.
 * No game addresses, emulated writes, or cycle adjustments occur here.
 */
#include <stdint.h>

/* Arguments: hook, pc, code, cycle, subcycle, path, 34 CPU registers, RAM,
 * scratchpad, the 64 raw GTE words (32 data, then 32 control), and the
 * interpreter's load-delay state: selected slot, two target registers, two
 * values. A pending load is not yet visible in the CPU registers. */
typedef void (*xem_trace_callback)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                   const uint32_t *, const uint8_t *, const uint8_t *,
                                   const uint32_t *, const uint32_t *);

static uint32_t xem_trace_pcs[64];
static uint32_t xem_trace_pc_count;
static uint32_t xem_trace_budget;
static uint32_t xem_trace_seen;
static uint32_t xem_trace_active;
static xem_trace_callback xem_trace_sink;

#define XEM_TRACE_EXPORT __attribute__((visibility("default")))

XEM_TRACE_EXPORT int retro_xem_trace_configure(const uint32_t *pcs, uint32_t count, uint32_t budget,
                                               xem_trace_callback callback) {
    uint32_t i, j;
    xem_trace_active = 0;
    xem_trace_sink = 0;
    xem_trace_seen = 0;
    xem_trace_pc_count = 0;
    if (!pcs || !callback || !count || count > 64 || !budget || budget > 1000000)
        return 0;
    for (i = 0; i < count; i++) {
        if (pcs[i] & 3)
            return 0;
        for (j = 0; j < i; j++)
            if (pcs[i] == pcs[j])
                return 0;
        xem_trace_pcs[i] = pcs[i];
    }
    xem_trace_pc_count = count;
    xem_trace_budget = budget;
    xem_trace_sink = callback;
    return 1;
}

XEM_TRACE_EXPORT void retro_xem_trace_enable(uint32_t active) { xem_trace_active = active != 0; }

XEM_TRACE_EXPORT uint32_t retro_xem_trace_count(void) { return xem_trace_seen; }

static inline void xem_trace_instruction(const psxRegisters *regs, uint32_t pc, uint32_t code,
                                         uint32_t path) {
    uint32_t i;
    if (!xem_trace_active || !xem_trace_sink || xem_trace_seen >= xem_trace_budget)
        return;
    for (i = 0; i < xem_trace_pc_count; i++) {
        if (pc == xem_trace_pcs[i]) {
            const uint32_t load_delay[5] = {regs->dloadSel, regs->dloadReg[0], regs->dloadReg[1],
                                            regs->dloadVal[0], regs->dloadVal[1]};
            xem_trace_seen++;
            xem_trace_sink(i, pc, code, regs->cycle, regs->subCycle, path, regs->GPR.r,
                           regs->ptrs.psxM, regs->ptrs.psxH, (const uint32_t *)&regs->CP2,
                           load_delay);
            return;
        }
    }
}
