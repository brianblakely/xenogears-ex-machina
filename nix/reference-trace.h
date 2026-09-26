/* SPDX-License-Identifier: GPL-2.0-or-later
 * Optional host-side observation extension for the separately built emulator.
 * No game addresses, emulated writes, or cycle adjustments occur here.
 */
#include <stdint.h>
#include <string.h>

#include "plugins.h"

/* Arguments: hook, pc, code, cycle, subcycle, path, 34 CPU registers, RAM,
 * scratchpad, the 64 raw GTE words (32 data, then 32 control), and the
 * interpreter's load-delay state: selected slot, two target registers, two
 * values. A pending load is not yet visible in the CPU registers. */
typedef void (*xem_trace_callback)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                   const uint32_t *, const uint8_t *, const uint8_t *,
                                   const uint32_t *, const uint32_t *);

#define XEM_TRACE_HOOKS 256u
static uint32_t xem_trace_pcs[XEM_TRACE_HOOKS];
static uint32_t xem_trace_pc_count;
/* One bit per aligned word of the 8 MiB KUSEG/KSEG0/KSEG1 RAM window: the
 * words a hook watches. Hooked PCs are distinct aligned system-RAM
 * addresses, so the bit decides a fetch before the hook list is searched. */
#define XEM_TRACE_FILTER_WORDS 0x200000u
static uint8_t xem_trace_filter[XEM_TRACE_FILTER_WORDS / 8u];
static uint32_t xem_trace_budget;
static uint32_t xem_trace_seen;
static uint32_t xem_trace_active;
static xem_trace_callback xem_trace_sink;

/* Coverage of the 2 MiB system RAM, one entry per aligned word (any
 * KUSEG/KSEG0/KSEG1 mirror). Host layout: executed bitmap, changed bitmap (a
 * later dispatch at the word fetched a different instruction), then the first
 * and the last fetched instruction word of each executed word. */
#define XEM_COVERAGE_WORDS 0x80000u
#define XEM_COVERAGE_BITMAP (XEM_COVERAGE_WORDS / 8u)
#define XEM_COVERAGE_BYTES (2u * XEM_COVERAGE_BITMAP + 8u * XEM_COVERAGE_WORDS)
static uint8_t xem_coverage_bits[XEM_COVERAGE_BITMAP];
static uint8_t xem_coverage_changed[XEM_COVERAGE_BITMAP];
static uint32_t xem_coverage_first[XEM_COVERAGE_WORDS];
static uint32_t xem_coverage_last[XEM_COVERAGE_WORDS];
static uint32_t xem_coverage_active;

#define XEM_TRACE_EXPORT __attribute__((visibility("default")))

XEM_TRACE_EXPORT int retro_xem_trace_configure(const uint32_t *pcs, uint32_t count, uint32_t budget,
                                               xem_trace_callback callback) {
    uint32_t i, j;
    xem_trace_active = 0;
    xem_trace_sink = 0;
    xem_trace_seen = 0;
    xem_trace_pc_count = 0;
    memset(xem_trace_filter, 0, sizeof(xem_trace_filter));
    if (!pcs || !callback || !count || count > XEM_TRACE_HOOKS || !budget || budget > 1000000)
        return 0;
    for (i = 0; i < count; i++) {
        if (pcs[i] & 3)
            return 0;
        for (j = 0; j < i; j++)
            if (pcs[i] == pcs[j])
                return 0;
        xem_trace_pcs[i] = pcs[i];
    }
    for (i = 0; i < count; i++) {
        const uint32_t word = (pcs[i] & 0x7fffffu) >> 2;
        xem_trace_filter[word >> 3] |= (uint8_t)(1u << (word & 7u));
    }
    xem_trace_pc_count = count;
    xem_trace_budget = budget;
    xem_trace_sink = callback;
    return 1;
}

XEM_TRACE_EXPORT void retro_xem_trace_enable(uint32_t active) { xem_trace_active = active != 0; }

XEM_TRACE_EXPORT uint32_t retro_xem_trace_count(void) { return xem_trace_seen; }

XEM_TRACE_EXPORT void retro_xem_coverage_enable(uint32_t active) {
    xem_coverage_active = active != 0;
}

/* Copy the w x h VRAM rectangle at (x, y) to the host, row by row (16-bit
 * pixels, little-endian). The GPU plugin's save-state export (the same call a
 * save state makes) first completes buffered commands; nothing is written to
 * VRAM, RAM or CPU state. Returns 0 for an invalid rectangle or buffer. */
XEM_TRACE_EXPORT int retro_xem_vram_read(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                         uint8_t *out, uint32_t size) {
    GPUFreeze_t header;
    uint16_t *vram = 0;
    uint32_t row;
    if (!out || !w || !h || x >= 1024u || y >= 512u || w > 1024u - x || h > 512u - y ||
        size != w * h * 2u || !GPU_freeze)
        return 0;
    memset(&header, 0, sizeof(header));
    header.ulFreezeVersion = 1;
    if (GPU_freeze(1, &header, &vram) != 1 || !vram)
        return 0;
    for (row = 0; row < h; row++)
        memcpy(out + row * w * 2u, vram + (y + row) * 1024u + x, w * 2u);
    return 1;
}

/* Copy the coverage arrays to the host in the documented layout and clear them. */
XEM_TRACE_EXPORT int retro_xem_coverage_take(uint8_t *out, uint32_t size) {
    if (!out || size != XEM_COVERAGE_BYTES)
        return 0;
    memcpy(out, xem_coverage_bits, XEM_COVERAGE_BITMAP);
    memcpy(out + XEM_COVERAGE_BITMAP, xem_coverage_changed, XEM_COVERAGE_BITMAP);
    memcpy(out + 2u * XEM_COVERAGE_BITMAP, xem_coverage_first, sizeof(xem_coverage_first));
    memcpy(out + 2u * XEM_COVERAGE_BITMAP + sizeof(xem_coverage_first), xem_coverage_last,
           sizeof(xem_coverage_last));
    memset(xem_coverage_bits, 0, sizeof(xem_coverage_bits));
    memset(xem_coverage_changed, 0, sizeof(xem_coverage_changed));
    memset(xem_coverage_first, 0, sizeof(xem_coverage_first));
    memset(xem_coverage_last, 0, sizeof(xem_coverage_last));
    return 1;
}

static inline void xem_trace_instruction(const psxRegisters *regs, uint32_t pc, uint32_t code,
                                         uint32_t path) {
    uint32_t i;
    if (xem_coverage_active) {
        uint32_t address = pc & 0x1fffffffu;
        if (address < 0x800000u) {
            const uint32_t word = (address & 0x1fffffu) >> 2;
            const uint8_t bit = (uint8_t)(1u << (word & 7u));
            if (!(xem_coverage_bits[word >> 3] & bit)) {
                xem_coverage_bits[word >> 3] |= bit;
                xem_coverage_first[word] = code;
            } else if (xem_coverage_last[word] != code) {
                xem_coverage_changed[word >> 3] |= bit;
            }
            xem_coverage_last[word] = code;
        }
    }
    if (!xem_trace_active || !xem_trace_sink || xem_trace_seen >= xem_trace_budget)
        return;
    {
        const uint32_t word = (pc & 0x7fffffu) >> 2;
        if (!(xem_trace_filter[word >> 3] & (1u << (word & 7u))))
            return;
    }
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
