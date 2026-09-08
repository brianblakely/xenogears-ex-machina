"""Apply exact, game-independent observation hooks to the pinned interpreter."""

from pathlib import Path

path = Path("libpcsxcore/psxinterpreter.c")
source = path.read_text()
replacements = [
    (
        '#include "../include/compiler_features.h"\n',
        '#include "../include/compiler_features.h"\n#include "xem_reference_trace.h"\n',
        1,
    ),
    (
        "\tregs->code = fetch(regs, memRLUT, pc);\n\tpsxBSC[regs->code >> 26]",
        "\tregs->code = fetch(regs, memRLUT, pc);\n"
        "\txem_trace_instruction(regs, pc, regs->code, 0);\n\tpsxBSC[regs->code >> 26]",
        2,
    ),
    (
        "\t\tif (likely(!isBranch(code))) {\n\t\t\tdloadStep(regs);\n",
        "\t\tif (likely(!isBranch(code))) {\n\t\t\tdloadStep(regs);\n"
        "\t\t\txem_trace_instruction(regs, tar1, code, 2);\n",
        1,
    ),
    (
        "\t\ttar1 = psxBranchNoDelay(regs, tar2, code, &taken);",
        "\t\txem_trace_instruction(regs, tar1, code, 3);\n"
        "\t\ttar1 = psxBranchNoDelay(regs, tar2, code, &taken);",
        1,
    ),
    (
        "\tif (unlikely(isBranch(code))) {\n\t\tregs->pc = pc;",
        "\tif (unlikely(isBranch(code))) {\n"
        "\t\txem_trace_instruction(regs, pc, code, 3);\n\t\tregs->pc = pc;",
        1,
    ),
    (
        "\tdloadStep(regs);\n\tpsxBSC[code >> 26](regs, code);",
        "\tdloadStep(regs);\n\txem_trace_instruction(regs, pc, code, 1);\n"
        "\tpsxBSC[code >> 26](regs, code);",
        1,
    ),
]
for before, after, count in replacements:
    if source.count(before) != count:
        raise SystemExit("Pinned interpreter hook context changed; independent review required")
    source = source.replace(before, after)
path.write_text(source)
