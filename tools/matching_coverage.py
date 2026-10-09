#!/usr/bin/env python3
"""Report source coverage of one linked target, separately from binary matching.

Every byte of every .text input section of the linked ELF counts in one class.
Each function symbol counts once, with its bytes and its static MIPS
instructions (the four-byte words of its range, nops and branch delay slots
included):

* ``c``            cc1 emitted it (``.ent NAME``) in its decomp/src unit
* ``nonmatching``  assembly a C unit includes, named as the INCLUDE_ASM fallback
                   of a reviewed ``#ifdef NON_MATCHING`` candidate (found by the
                   only source scan; does not satisfy the exit)
* ``sdk``          other assembly inside a range classified as PsyQ SDK code
* ``handwritten``  other assembly inside a range classified as original
                   hand-written asm; the report fails on a handwritten byte
                   outside such a range, also of an authored .s unit under
                   decomp/src
* ``asm``          other assembly (remaining work)

The .text bytes outside every function (alignment padding and data words of an
included file) count as bytes, not instructions, under their owner's class,
attributed like data (below): an INCLUDE_ASM'd file's under its function's
class, an INCLUDE_RODATA'd file's as ``included``, a generated assembly unit's
as ``asm``, an authored one's as ``handwritten``; a classified range takes
precedence. The bytes cc1 put there are data objects (placed by a section
attribute, or emitted after an INCLUDE_RODATA left the assembler in .text) and
never count as ``c``: machine words in such an array could replace an
INCLUDE_ASM'd function and still match. They count as ``text_data`` (bytes, no
instructions) only where objects the classification names tile them, each from
its cc1 label to the next symbol; any other such byte fails the report, as does
a data directive cc1 emits among a function's code (jump tables under
``-membedded-pic``). The report fails unless every function lies inside its
input section and overlaps no other, so the classes add up to the .text input
sections exactly (``text_bytes``).

The classification file lists ``START END CLASS NOTE...`` lines (hex VRAM,
END exclusive; nonempty, disjoint ranges): ``sdk``, ``handwritten`` and
``asset`` ranges; for each data object a C unit places in .text a
``START END text_data NAME REASON...`` line, whose reason is the evidence that
the original keeps it there; and ``START END included NAME REASON...`` lines,
the reviewed reasons of included objects (below). Each text_data and included
line must be used. ``START END unrelocated REASON...`` lines are the reviewed
words of the relocation scan (``--relocations``, below); they may lie in
another line's range. This tool never reads or asserts binary agreement; run
the exact comparison separately.

Every loaded data byte counts in one class: each .rodata/.data/.sdata input
section of the GNU ld map, and the .bss/.sbss that lies in a loaded (PROGBITS)
output section, where an image holds its uninitialized variables as zeros.
The uninitialized data past the loaded image (NOLOAD .bss, up to ``--bss-end``
where the target declares the end its loader clears) counts apart, in
``bss_noload_classes``: a C unit's .bss/.sbss as ``bss``, and as
``bss_placeholder`` (remaining work) a generated assembly unit's .bss and
every byte no input section holds, a variable placed only by a linker-script
name (``remaining_bss_placeholder_bytes``).

* ``c``            emitted by cc1 in a data section of its decomp/src unit
* ``bss``          the same in .bss/.sbss (uninitialized, zero in the file)
* ``included``     original bytes a C unit links with INCLUDE_RODATA (a .s file)
                   or INCLUDE_ORIGINAL/INCLUDE_ASSET (``.incbin`` of the image)
* ``nonmatching``/``asm``/``sdk``/``handwritten``  the other bytes an
                   INCLUDE_ASM'd file puts in a C unit (splat moves rodata only
                   one function uses into its file), under that function's class
* ``handwritten``  an authored assembly unit under decomp/src
* ``sdk``/``asset``/``handwritten``  inside a classified range, which takes
                   precedence (``asset``: user-supplied game data/bytecode)
* ``placeholder``  generated data or loaded .bss from the original image
                   (remaining work)

Each included object (one INCLUDE_RODATA/INCLUDE_ORIGINAL/INCLUDE_ASSET
statement's bytes in one section, from its label; GAS may put alignment fill
ahead of it) that a classified range does not cover needs a reason, or the
report fails. Either its bytes are one text string whose terminator is
followed by 1-3 bytes of alignment padding to a word boundary, one of them
non-zero: a stray byte the original assembler left, which C cannot emit. Or
an ``included`` line of the classification names it (its range and label,
``-`` for none) with the reviewed reason, as for strings that several SDK
library functions share or a data object whose padding holds stray bytes.
Each such line must name an included object.

``--list CLASS`` prints the class's ranges instead of the report: its
functions, its .text bytes outside every function and its data ranges, each
data range with its section and object (an included one with its label);
``bss_placeholder`` ranges are split at each symbol, so each placeholder
variable shows with its extent.

``--script-symbols warn|strict`` checks the link instead (decomp/Makefile runs
it in ``verify``): a symbol a linker script given with ``--script`` assigns
(splat's undefined_syms_auto.txt as PROVIDE, where the link used it, a
``.data.ld`` or any other fragment) must not lie inside the target's own
loaded image or uninitialized data, where an address copied from the original
stands in for an object the link should place. A script named with
``--views`` may define names there only as views, expressions of symbols
the link places (``D_800C3EB4 = D_800C3EB0 + 0x4``), and must define one.
``warn`` reports and passes; ``strict`` fails.

``--relocations`` (also run by ``verify``) fails on each address of the
target's own image or uninitialized data that the link did not produce: an
aligned word holding one without an R_MIPS_32 relocation in its input object,
in a loaded data input section or in .text outside every function, and in
.text a lui whose immediate is the %hi of one without R_MIPS_HI16 and any
j/jal without R_MIPS_26. Words in a range the ``--classification`` classes
asset, included or handwritten are exempt; any other only through an
``unrelocated`` line giving the reason, which must cover a reported word.

A C unit's bytes are attributed by where GAS put them, not by source
spellings. ``make coverage`` compiles each C unit again with a label line at
both ends of the text of every asm statement (``--mark-asm`` on the
preprocessed unit, so macros, token pasting and included files are already
expanded) and assembles it with each label recording its position in each of
.text, .rodata, .data, .sdata, .sbss and .bss and the number of GAS macro
expansions so far (``--mark-gas``). The report fails unless, without the
labels, the unit's cc1 output, its GAS input (maspsx numbers its division-check
labels by input line) and its sections and relocations are exactly the built
object's, and the recorded positions tile every section. The bytes between a
statement's two labels, in every section, are that statement's, whatever its
text includes, incbins, expands or redefines; every other byte was assembled
from cc1's own lines, which may expand no GAS macro. A statement is classified
by its exact text in the cc1 output, which must be one of include_asm.h's:

* INCLUDE_ASM: the ``.include`` inside ``__maspsx_include_asm_hack_NAME``; its
  functions are assembly, its other bytes take the class of the function NAME
  (``nonmatching`` or ``asm`` where the file defines none, like the resident's
  SDK data tag in .text, which its range makes ``sdk``)
* INCLUDE_RODATA: its functions are assembly, its other bytes ``included``
* INCLUDE_ASSET/INCLUDE_ORIGINAL: ``included`` (or a range's class) outside
  .text
* the macro.inc include, which emits nothing

or, written inside a compiled function, one of the original-style macros the
recovered C uses (``ORIGINAL_ASM``: the PsyQ GTE macros of psyq/inline_c.h and
the local sets, the debugger break and pollhost, the stack switches, GET_RA,
addPrimLen9), whose text is exactly the macro's template with a register for
each operand. Such a statement must expand no GAS macro and emit only .text, in
that function, and its code counts with the function. Any other asm statement
fails the report, as does every byte the report cannot count: a function in a
C unit that cc1 did not emit and no INCLUDE_ASM/INCLUDE_RODATA file defines, a
compiled function holding a statement's other bytes, original bytes in .text,
a function in a data section, an input section other than .text and the data
sections. Bytes outside every input section (alignment gaps, a packer's zero
tail) are not counted.

``--mark-asm`` also fails the coverage build on each string cc1 would copy into
its output as it is, unless the string is a plain name: a declaration's asm name
(a register variable's register, as in ``register s32 v asm("$14")``, or a
symbol's assembler name), a section or alias attribute, and a line marker's file
name; any other preprocessor line fails it too. So the source cannot put an
assembler line among cc1's own.

Paths in the map are relative to the working directory, the repository root.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from bisect import bisect_left, bisect_right
from dataclasses import dataclass
from itertools import pairwise
from pathlib import Path

# Reviewed NON_MATCHING candidates (shared with nonmatching_score.py): each
# INCLUDE_ASM in the fallback of an `#ifdef NON_MATCHING ... #else ... #endif`.
INCLUDE_ASM = re.compile(r"INCLUDE_ASM\(\s*\"[^\"]*\"\s*,\s*(\w+)\s*\)")
NON_MATCHING = re.compile(r"#ifdef\s+NON_MATCHING(.*?)#else(.*?)#endif", re.DOTALL)
COMMENT_OR_LITERAL = re.compile(
    r"\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'|/\*.*?\*/|//[^\n]*", re.S
)

# The sections a C unit's bytes are attributed in, with each one's key in the
# position records.
TRACKED = {
    ".text": "text", ".rodata": "rodata", ".data": "data",
    ".sdata": "sdata", ".sbss": "sbss", ".bss": "bss",
}
DATA_SECTION = re.compile(r"\.(rodata|data|sdata|sbss|bss)\b")
BSS_SECTION = re.compile(r"\.s?bss\b")

# The coverage build's label lines. maspsx passes them through and its
# lookahead skips them; in an include-asm hack function it keeps only lines
# marked `# maspsx-keep`.
MARK = re.compile(r"^\s*Lcov([be])_(\d+):(?: # maspsx-keep)?\s*$")
# maspsx names its division-check labels after the input line number.
MASPSX_LINE_LABEL = re.compile(r"\.L_(NOT_DIV_BY_ZERO|DIV_BY_POSITIVE_SIGN)_(\d+)")
# cc1's own section switches, data directives and labels. cc1 puts jump tables
# and constants in .rdata; only -membedded-pic puts them in .text.
CC1_SECTION = re.compile(r"\s*(?:\.section\s+([^\s,]+)|(\.(?:text|data|rdata|sdata|sbss|bss))\s*$)")
CC1_DATA = re.compile(
    r"\s*\.(?:word|half|hword|short|byte|int|long|dword|gpword|quad|[248]byte|ascii|asciiz"
    r"|string|space|skip|zero|fill|float|single|double)\b"
)
CC1_LABEL = re.compile(r"([A-Za-z_.$][\w.$]*):\s*$")
C_TOKEN = re.compile(
    r"""(?P<space>^[ \t]*\#[^\n]*|\s+)
      | (?P<string>L?"(?:\\.|[^"\\\n])*")
      | (?P<char>L?'(?:\\.|[^'\\\n])*')
      | (?P<name>[A-Za-z_$][\w$]*)
      | (?P<punct>\.?\d(?:[eEpP][+-]|[\w.])*|->|\+\+|--|<<=|>>=|\.\.\.
                  |[-+*/%&|^<>=!]=|<<|>>|&&|\|\||.)""",
    re.M | re.S | re.X,
)
ASM_KEYWORDS = {"asm", "__asm", "__asm__"}
# An asm keyword starts a statement after one of these tokens, after the `)` of
# a condition, or when qualifiers (`volatile`, `const`) follow it; anywhere else
# it gives a declarator's assembler name (`register int v asm("$14")`).
STATEMENT_AFTER = {"", ";", "{", "}", ":", "else", "do", "__extension__"}
CONDITIONS = {"if", "while", "for", "switch"}
# The exact text of include_asm.h's statements in the cc1 output (each line
# stripped).
TEMPLATES = {
    "asm": re.compile(
        r"\.text # maspsx-keep\n\.align\s2 # maspsx-keep\n\.set noreorder # maspsx-keep\n"
        r"\.set noat # maspsx-keep\n\.include \"(?P<path>[^\"\n]+)\" # maspsx-keep\n"
        r"\.set reorder # maspsx-keep\n\.set at # maspsx-keep\n"
    ),
    "rodata": re.compile(r"\.section \.rodata\n\.include \"[^\"\n]+\"\n\.section \.text"),
    "asset": re.compile(
        r"\.section \S+\n\.align 2\n\.globl (?P<name>\w+)\n(?P=name):\n"
        r"\.incbin \"[^\"\n]+\", \w+ - \w+, (?P<size>\w+)\n\.size (?P=name), (?P=size)\n"
        r"\.previous"
    ),
    "macros": re.compile(r"\.include \"macro\.inc\"\n"),
}
# The other asm statements a compiled function may contain: the original-style
# macros the recovered C uses (docs/matching.md, What counts as recovered
# source), each by its template, the asm string as cc1 receives it. A statement's
# text in the cc1 output must be one of them exactly (each line stripped), with
# a register for each operand %N.
ORIGINAL_ASM = (
    # PsyQ GTE macros in the inline_c.h/gtemac.h form (psyq/inline_c.h and the
    # local sets of battle, field, menu, ovl2143, resident and worldmap).
    # Control registers: gte_SetRotMatrix, gte_SetLightMatrix,
    # gte_SetColorMatrix, gte_SetTransMatrix, gte_SetBackColor, gte_ldopv1.
    "lw $12, 0(%0);lw $13, 4(%0);ctc2 $12, $0;ctc2 $13, $1;lw $12, 8(%0);lw $13, 12(%0);"
    "lw $14, 16(%0);ctc2 $12, $2;ctc2 $13, $3;ctc2 $14, $4",
    "lw $12, 0(%0);lw $13, 4(%0);ctc2 $12, $8;ctc2 $13, $9;lw $12, 8(%0);lw $13, 12(%0);"
    "lw $14, 16(%0);ctc2 $12, $10;ctc2 $13, $11;ctc2 $14, $12",
    "lw $12, 0(%0);lw $13, 4(%0);ctc2 $12, $16;ctc2 $13, $17;lw $12, 8(%0);lw $13, 12(%0);"
    "lw $14, 16(%0);ctc2 $12, $18;ctc2 $13, $19;ctc2 $14, $20",
    "lw $12, 20(%0);lw $13, 24(%0);ctc2 $12, $5;lw $14, 28(%0);ctc2 $13, $6;ctc2 $14, $7",
    "sll $12, %0, 4;sll $13, %1, 4;sll $14, %2, 4;ctc2 $12, $13;ctc2 $13, $14;ctc2 $14, $15",
    "lw $12, 0(%0);lw $13, 4(%0);ctc2 $12, $0;lw $14, 8(%0);ctc2 $13, $2;ctc2 $14, $4",
    # Data register loads: gte_ldv0, gte_ldv1, gte_ldv2, gte_ldv01, gte_ldv3,
    # gte_ldv3c, gte_ldlv0, gte_ldlvl, gte_ldopv2, gte_ldclmv, gte_ldsv,
    # gte_ldrgb, gte_lddp, gte_ldsxy3, gte_ldsz4.
    "lwc2 $0, 0(%0);lwc2 $1, 4(%0)",
    "lwc2 $2, 0(%0);lwc2 $3, 4(%0)",
    "lwc2 $4, 0(%0);lwc2 $5, 4(%0)",
    "lwc2 $0, 0(%0);lwc2 $1, 4(%0);lwc2 $2, 0(%1);lwc2 $3, 4(%1)",
    "lwc2 $0, 0(%0);lwc2 $1, 4(%0);lwc2 $2, 0(%1);lwc2 $3, 4(%1);lwc2 $4, 0(%2);lwc2 $5, 4(%2)",
    "lwc2 $0, 0(%0);lwc2 $1, 4(%0);lwc2 $2, 8(%0);lwc2 $3, 12(%0);lwc2 $4, 16(%0);"
    "lwc2 $5, 20(%0)",
    "lhu $13, 4(%0);lhu $12, 0(%0);sll $13, $13, 16;or $12, $12, $13;mtc2 $12, $0;"
    "lwc2 $1, 8(%0)",
    "lwc2 $9, 0(%0);lwc2 $10, 4(%0);lwc2 $11, 8(%0)",
    "lwc2 $11, 8(%0);lwc2 $9, 0(%0);lwc2 $10, 4(%0)",
    "lhu $12, 0(%0);lhu $13, 6(%0);lhu $14, 12(%0);mtc2 $12, $9;mtc2 $13, $10;mtc2 $14, $11",
    "lhu $12, 0(%0);lhu $13, 2(%0);lhu $14, 4(%0);mtc2 $12, $9;mtc2 $13, $10;mtc2 $14, $11",
    "lwc2 $6, 0(%0)",
    "mtc2 %0, $8",
    "mtc2 %0, $12;mtc2 %2, $14;mtc2 %1, $13",
    "mtc2 %0, $16;mtc2 %1, $17;mtc2 %2, $18;mtc2 %3, $19",
    # Stores and reads: gte_stsxy (gte_stsxy2), gte_stsxy0, gte_stsxy1,
    # gte_stsxy01, gte_stsxy3, gte_stsxy3_ft4, gte_stsz1, gte_stsz2, gte_stsz
    # (menu's gte_stsz3), gte_stsz3 (gte_stsz3v), gte_stsz4c, gte_stotz,
    # gte_stdp, gte_strgb, gte_stopz, gte_stlvl, gte_stlvnl, gte_stclmv,
    # gte_stsv, gte_stszotz, gte_stflg, gte_getsxy2, gte_getsxy3.
    "swc2 $14, 0(%0)",
    "swc2 $12, 0(%0)",
    "swc2 $13, 0(%0)",
    "swc2 $12, 0(%0);swc2 $13, 0(%1)",
    "swc2 $12, 0(%0);swc2 $13, 0(%1);swc2 $14, 0(%2)",
    "swc2 $12, 8(%0);swc2 $13, 16(%0);swc2 $14, 24(%0)",
    "swc2 $17, 0(%0)",
    "swc2 $18, 0(%0)",
    "swc2 $19, 0(%0)",
    "swc2 $17, 0(%0);swc2 $18, 0(%1);swc2 $19, 0(%2)",
    "swc2 $16, 0(%0);swc2 $17, 4(%0);swc2 $18, 8(%0);swc2 $19, 12(%0)",
    "swc2 $7, 0(%0)",
    "swc2 $8, 0(%0)",
    "swc2 $22, 0(%0)",
    "swc2 $24, 0(%0)",
    "swc2 $9, 0(%0);swc2 $10, 4(%0);swc2 $11, 8(%0)",
    "swc2 $25, 0(%0);swc2 $26, 4(%0);swc2 $27, 8(%0)",
    "mfc2 $12, $9;mfc2 $13, $10;mfc2 $14, $11;sh $12, 0(%0);sh $13, 6(%0);sh $14, 12(%0)",
    "mfc2 $12, $9;mfc2 $13, $10;mfc2 $14, $11;sh $12, 0(%0);sh $13, 2(%0);sh $14, 4(%0)",
    "mfc2 $12, $19;nop;sra $12, $12, 2;sw $12, 0(%0)",
    "cfc2 $12, $31;nop;sw $12, 0(%0)",
    "mfc2 %0, $14; nop",
    "mfc2 %0, $12;mfc2 %1, $13;mfc2 %2, $14;nop",
    # Commands, each after the two nops: gte_rtps, gte_rtpt, gte_rt (ovl2143's
    # gte_rtv0tr), gte_rtv0, gte_rtir, gte_dpcs, gte_sqr0, gte_nccs, gte_nclip,
    # gte_avsz3, gte_avsz4, gte_op0, gte_op12, gte_gpf0, gte_gpf12, and battle's
    # gte_rtv0tr.
    "nop;nop;.word 0x4A180001",
    "nop;nop;.word 0x4A280030",
    "nop;nop;.word 0x4A480012",
    "nop;nop;.word 0x4A486012",
    "nop;nop;.word 0x4A49E012",
    "nop;nop;.word 0x4A780010",
    "nop;nop;.word 0x4AA00428",
    "nop;nop;.word 0x4B08041B",
    "nop;nop;.word 0x4B400006",
    "nop;nop;.word 0x4B58002D",
    "nop;nop;.word 0x4B68002E",
    "nop;nop;.word 0x4B70000C",
    "nop;nop;.word 0x4B78000C",
    "nop;nop;.word 0x4B90003D",
    "nop;nop;.word 0x4B98003D",
    "nop;nop;cop2 0x0480012",
    # The debugger break (ASPSX `break 1`): libsn.h's pollhost and the debug
    # stops (`break 1024`), slot39's (`break 0x400`), battle's word.
    "break 1024",
    "break 0x400",
    ".word 0x0001000D",
    # The stack switches (STACK_ENTER/SPAD_STACK_ENTER, STACK_LEAVE/
    # SPAD_STACK_LEAVE), the heap's GET_RA and worldmap's addPrimLen9.
    "move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8",
    "addiu $29, $29, 4\n\tlw $29, 0($29)",
    "move $15, %0\n\tsw $31, 0($15)",
    "lw $12, 0(%0);lui $13, 0x0900;or $12, $12, $13;lui $13, 0x00FF;ori $13, $13, 0xFFFF;"
    "and $13, %1, $13;sw $13, 0(%0);sw $12, 0(%1)",
)
# Strings cc1 copies into its output as they are: a declaration's asm name and
# a section or alias attribute, which must be plain names, and a line marker's
# file name (any other preprocessor line is refused).
PLAIN_NAME = re.compile(r"[A-Za-z0-9_.$]+")
VERBATIM_ATTRIBUTES = {"section", "__section__", "alias", "__alias__"}
DIRECTIVE = re.compile(r"^[ \t]*#.*$", re.M)
LINE_MARKER = re.compile(r'[ \t]*#[ \t]*\d+(?:[ \t]+"[^"\\\x00-\x1f]*"(?:[ \t]+\d+)*)?[ \t]*')

SHT_PROGBITS, SHT_SYMTAB, SHT_NOBITS, SHT_REL = 1, 2, 8, 9
SHF_ALLOC = 2
STT_FUNC, STT_SECTION, STT_FILE = 2, 3, 4
# The relocations of an address the link produces: a data word, a j/jal
# target, a lui's %hi.
R_MIPS_32, R_MIPS_26, R_MIPS_HI16 = 2, 4, 5


def mark_asm(text: str) -> str:
    """Preprocessed C with a label line at both ends of each asm statement's
    text. Fails on a preprocessor line other than a line marker with a plain
    file name, and on a declaration's asm name or a section or alias attribute
    that is not a plain name: cc1 copies them into its output as they are."""
    for line in DIRECTIVE.findall(text):
        if not LINE_MARKER.fullmatch(line):
            raise SystemExit(f"{line.strip()!r}: not a line marker with a plain file name")
    tokens = [match for match in C_TOKEN.finditer(text) if match.lastgroup != "space"]
    opener, closer, stack = {}, {}, []  # each `)` token's `(` and back
    for index, token in enumerate(tokens):
        if token.group() == "(":
            stack.append(index)
        elif token.group() == ")" and stack:
            opener[index] = stack.pop()
            closer[opener[index]] = index

    def plain(first: int, end: int, what: str) -> None:
        for token in tokens[first:end]:
            if token.lastgroup == "string" and not PLAIN_NAME.fullmatch(token.group()[1:-1]):
                raise SystemExit(f"{what} {token.group()}: not a plain name")

    for index, token in enumerate(tokens):
        if token.group() in ("__attribute__", "__attribute") and index + 1 in closer:
            for inner in range(index + 2, closer[index + 1]):
                if tokens[inner].group() in VERBATIM_ATTRIBUTES and inner + 1 in closer:
                    plain(inner + 2, closer[inner + 1], f"{tokens[inner].group()} attribute")
    out, last, number, i = [], 0, 0, 0
    while i < len(tokens):
        if tokens[i].lastgroup != "name" or tokens[i].group() not in ASM_KEYWORDS:
            i += 1
            continue
        j = i + 1
        while j < len(tokens) and tokens[j].lastgroup == "name":
            j += 1
        k = j + 1
        while k < len(tokens) and tokens[k].lastgroup == "string":
            k += 1
        if j == len(tokens) or tokens[j].group() != "(" or k == j + 1:
            i += 1
            continue
        before = tokens[i - 1].group() if i else ""
        if before == ")" and opener.get(i - 1):
            statement = tokens[opener[i - 1] - 1].group() in CONDITIONS
        else:
            statement = before in STATEMENT_AFTER
        if statement or j > i + 1:
            first, final = tokens[j + 1], tokens[k - 1]
            keep = " # maspsx-keep" if "# maspsx-keep" in text[first.start():final.end()] else ""
            out += [text[last:first.start()], f'"Lcovb_{number}:{keep}\\n\\t" ',
                    text[first.start():final.end()], f' "\\nLcove_{number}:{keep}"']
            last, number = final.end(), number + 1
        else:
            plain(j + 1, k, "asm name")
        i = k
    return "".join(out) + text[last:]


def mark_gas(text: str) -> str:
    """GAS input with each label line replaced by a record of every tracked
    section's position and the number of GAS macro expansions so far, as
    symbols no other line may define (`.equiv`)."""
    lines = ["\t.macro __cov_mark name"]
    for section, key in TRACKED.items():
        lines += [f"\t.pushsection {section}", f"\t.equiv \\name\\()_{key}, .", "\t.popsection"]
    lines += ["\t.equiv \\name\\()_macros, \\@", "\t.endm", "\t__cov_mark __cov_start"]
    seen: dict[str, int] = {}
    for line in text.splitlines():
        match = MARK.match(line)
        if match:
            tag = f"{match.group(1)}_{match.group(2)}"
            seen[tag] = seen.get(tag, -1) + 1
            line = f"\t__cov_mark __cov_{tag}_{seen[tag]}"
        lines.append(line)
    return "\n".join(lines + ["\t__cov_mark __cov_end", ""])


@dataclass
class Section:
    name: str
    type: int
    flags: int
    address: int
    size: int
    data: bytes
    info: int


@dataclass
class Symbol:
    name: str
    value: int
    size: int
    kind: int
    section: int


def read_elf(path: Path) -> tuple[list[Section], list[Symbol]]:
    """The sections and the symbol table of a 32-bit little-endian ELF file."""
    data = path.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        raise SystemExit(f"{path}: not a 32-bit little-endian ELF file")
    offset = struct.unpack_from("<I", data, 0x20)[0]
    size, count, names = struct.unpack_from("<HHH", data, 0x2E)
    headers = [struct.unpack_from("<10I", data, offset + i * size) for i in range(count)]

    def string(table: int, start: int) -> str:
        start += headers[table][4]
        return data[start:data.index(b"\0", start)].decode("latin-1")

    sections = [
        Section(string(names, h[0]), h[1], h[2], h[3], h[5],
                b"" if h[1] == SHT_NOBITS else data[h[4]:h[4] + h[5]], h[7])
        for h in headers
    ]
    symbols = [
        Symbol(string(h[6], name), value, size, info & 0xF, index)
        for h in headers if h[1] == SHT_SYMTAB
        for name, value, size, info, _other, index
        in struct.iter_unpack("<IIIBBH", data[h[4]:h[4] + h[5]])
    ]
    return sections, symbols


def relocations(
    sections: list[Section], symbols: list[Symbol], target: int
) -> list[tuple[int, int, str]]:
    """(offset, type, symbol) of each relocation of section index `target`; a
    section symbol by its section's name."""
    result = []
    for section in sections:
        if section.type == SHT_REL and section.info == target:
            for offset, info in struct.iter_unpack("<II", section.data):
                symbol = symbols[info >> 8]
                name = sections[symbol.section].name if symbol.kind == STT_SECTION else symbol.name
                result.append((offset, info & 0xFF, name))
    return result


def classification(path: Path | None) -> list[tuple[int, int, str, str]]:
    """The (START, END, CLASS, NOTE) lines of a classification file in address
    order; their ranges are nonempty and disjoint, the `unrelocated` lines
    (reviewed words of the relocation scan, which may lie in a class's range)
    among themselves."""
    if path is None:
        return []
    ranges = []
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            start, end, kind, *note = line.split()
            if kind not in ("sdk", "handwritten", "asset", "text_data", "included",
                            "unrelocated"):
                raise SystemExit(f"unknown classification {kind!r}")
            if kind == "text_data" and len(note) < 2:
                raise SystemExit(f"{line}: a text_data line names its object and the evidence"
                                 " that the original keeps it in .text")
            if kind == "included" and len(note) < 2:
                raise SystemExit(f"{line}: an included line names its object and the reason"
                                 " it stays original")
            if kind == "unrelocated" and not note:
                raise SystemExit(f"{line}: an unrelocated line gives the reason its words hold"
                                 " no address the link should produce")
            if int(end, 16) <= int(start, 16):
                raise SystemExit(f"{line}: an empty range")
            ranges.append((int(start, 16), int(end, 16), kind, " ".join(note)))
    ranges.sort()
    for reviewed in (False, True):
        lines = [entry for entry in ranges if (entry[2] == "unrelocated") == reviewed]
        for (_, end, kind, _), (start, _, other, _) in pairwise(lines):
            if start < end:
                raise SystemExit(
                    f"classification: {other} at {start:08x} overlaps {kind} up to {end:08x}"
                )
    return ranges


TEXT = set(range(0x20, 0x7F)) | {0x09, 0x0A, 0x0D}


def stray_padding(data: bytes) -> bool:
    """Whether data is one text string, its terminator and 1-3 bytes of
    alignment padding, one of them non-zero: a stray byte C cannot emit."""
    end = data.find(b"\0")
    padding = data[end + 1:]
    return end > 0 and set(data[:end]) <= TEXT and 0 < len(padding) < 4 and any(padding)


def map_sections(path: Path) -> list[tuple[str, int, int, str]]:
    """(section, address, size, object) of every nonempty input section the
    link placed (the discarded ones listed first are skipped)."""
    text = path.read_text()
    text = text.split("\nLinker script and memory map\n", 1)[-1]
    result, pending = [], None
    for line in text.splitlines():
        parts = line.split()
        if line.startswith(" .") and len(parts) == 1:
            pending = parts[0]  # GNU ld puts a long section name on a line of its own
            continue
        if pending and len(parts) == 3:
            parts = [pending] + parts
        elif not line.startswith(" ."):
            parts = []
        pending = None
        if len(parts) != 4:
            continue
        name, address, size, obj = parts
        if address.startswith("0x") and size.startswith("0x") and int(size, 16):
            result.append((name, int(address, 16), int(size, 16), obj))
    return result


def extent(sections: list[Section], bss_end: int | None) -> tuple[int, int, int]:
    """The target's own address range: the start and end of its loaded image
    and the end of its uninitialized data (the no-load sections the link
    places and the declared bss_end). Fails where a no-load section lies
    inside the image or past bss_end."""
    loaded = [(s.address, s.address + s.size) for s in sections
              if s.type == SHT_PROGBITS and s.flags & SHF_ALLOC and s.size]
    noload = [(s.address, s.address + s.size, s.name) for s in sections
              if s.type == SHT_NOBITS and s.flags & SHF_ALLOC and s.size]
    if not loaded:
        raise SystemExit("no loaded section")
    lo, hi = min(start for start, _ in loaded), max(end for _, end in loaded)
    end = max([hi] + [stop for _, stop, _ in noload])
    for start, stop, name in noload:
        if start < hi:
            raise SystemExit(f"no-load section {name} ({start:08x}-{stop:08x}) inside the image")
    if bss_end is not None:
        if bss_end < end:
            raise SystemExit(f"the link places the target up to {end:08x}, past its declared"
                             f" end {bss_end:08x} (BSS_END)")
        end = bss_end
    return lo, hi, end


# A linker script's symbol assignments, `NAME = EXPR;` and `PROVIDE(NAME = EXPR);`.
SCRIPT_ASSIGNMENT = re.compile(
    r"^[ \t]*(PROVIDE(?:_HIDDEN)?[ \t]*\([ \t]*)?([A-Za-z_.$][\w.$]*)[ \t]*=(?!=)[ \t]*"
    r"([^;]*?)[ \t]*;", re.M)
SCRIPT_COMMENT = re.compile(r"/\*.*?\*/", re.S)
# A PROVIDE the link used (its value) or not (`[!provide]`), in the map.
MAP_PROVIDE = re.compile(
    r"^\s+(0x[0-9a-fA-F]+|\[!provide\])\s+PROVIDE(?:_HIDDEN)? \(([A-Za-z_.$][\w.$]*) = ", re.M)
EXPRESSION_TOKEN = re.compile(r"0[xX][0-9a-fA-F]+|\d+|[A-Za-z_.$][\w.$]*")
LD_FUNCTIONS = {
    "ABSOLUTE", "ADDR", "ALIGN", "ALIGNOF", "BLOCK", "CONSTANT", "DEFINED", "LENGTH",
    "LOADADDR", "LOG2CEIL", "MAX", "MIN", "NEXT", "ORIGIN", "SEGMENT_START", "SIZEOF",
    "SIZEOF_HEADERS",
}


def script_assignments(path: Path) -> list[tuple[str, str, bool]]:
    """(name, expression, provide) of each symbol assignment of a linker
    script, in order."""
    text = SCRIPT_COMMENT.sub(" ", path.read_text())
    result = []
    for match in SCRIPT_ASSIGNMENT.finditer(text):
        provide, expression = bool(match.group(1)), match.group(3)
        if provide and expression.endswith(")"):
            expression = expression[:-1].rstrip()  # the PROVIDE's own parenthesis
        result.append((match.group(2), expression, provide))
    return result


def script_names(map_path: Path, scripts: list[Path]) -> dict[str, tuple[Path, str]]:
    """Each name the scripts define in the link, with its script and
    expression: the last plain assignment wins (also over an object's
    definition); a PROVIDE counts only where the link used it (its map)."""
    used = {match.group(2) for match in MAP_PROVIDE.finditer(map_path.read_text())
            if match.group(1) != "[!provide]"}
    defined: dict[str, tuple[Path, str]] = {}
    provided: dict[str, tuple[Path, str]] = {}
    for path in scripts:
        for name, expression, provide in script_assignments(path):
            if not provide:
                defined[name] = (path, expression)
            elif name in used:
                provided.setdefault(name, (path, expression))
    return {**{n: p for n, p in provided.items() if n not in defined}, **defined}


def check_script_symbols(elf: Path, map_path: Path, scripts: list[Path], views: list[Path],
                         bss_end: int | None, strict: bool) -> int:
    """Report each symbol the scripts define inside the target's own image or
    uninitialized data; a views script may define views there. Returns the
    exit status: 1 for a strict failure."""
    sections, symbols = read_elf(elf)
    lo, hi, end = extent(sections, bss_end)
    value = {s.name: s.value for s in symbols if s.name and s.section and s.kind != STT_SECTION}
    known = {str(path) for path in scripts}
    for path in views:
        if str(path) not in known:
            raise SystemExit(f"{path}: a views script that is not among the link's scripts")
    defined = script_names(map_path, scripts)
    view_scripts = {str(path) for path in views}

    def view(name: str, seen: frozenset[str] = frozenset()) -> bool:
        """Whether the name is a view: a views script's expression of symbols
        the link places, objects' or other views."""
        path, expression = defined[name]
        names = [token for token in EXPRESSION_TOKEN.findall(expression)
                 if not token[0].isdigit() and not token.startswith(".")
                 and token not in LD_FUNCTIONS]
        inner = seen | {name}
        return str(path) in view_scripts and bool(names) and all(
            other in value and (other not in defined
                                or (other not in inner and view(other, inner)))
            for other in names)

    found: dict[tuple[str, str], list[tuple[int, str]]] = {}
    allowed: set[str] = set()
    for name, (path, _expression) in defined.items():
        address = value.get(name)
        if address is None or not lo <= address < end:
            continue
        if view(name):
            allowed.add(str(path))
            continue
        region = "image" if address < hi else "uninitialized data"
        found.setdefault((str(path), region), []).append((address, name))
    level = "error" if strict else "warning"
    for (path, region), names in sorted(found.items()):
        names.sort()
        shown = ", ".join(name for _, name in names[:4]) + (", ..." if len(names) > 4 else "")
        span = f"{lo:08x}-{hi:08x}" if region == "image" else f"{hi:08x}-{end:08x}"
        print(f"{level}: {elf}: {len(names)} name(s) that {path} assigns lie inside the"
              f" target's own {region} ({span}): {shown}", file=sys.stderr)
    for path in sorted(view_scripts - allowed):
        print(f"{level}: {elf}: views script {path} defines no view inside the target's own"
              " image or uninitialized data", file=sys.stderr)
    return 1 if strict and (found or view_scripts - allowed) else 0


def check_relocations(elf: Path, map_path: Path, ranges: list[tuple[int, int, str, str]],
                      bss_end: int | None) -> int:
    """Report each address of the target's own image or uninitialized data
    that the link did not produce, a number copied from the original that
    would not follow its object: an aligned word whose value lies there
    without an R_MIPS_32 relocation in a loaded data input section or in
    .text outside every function, a lui in .text whose immediate is the %hi
    of such an address without R_MIPS_HI16, and any j/jal in .text without
    R_MIPS_26. Words in a range classified asset,
    included or handwritten are exempt, and so are those an `unrelocated`
    line of the classification gives its reason for; each such line must
    cover a reported word. Returns the exit status."""
    sections, symbols = read_elf(elf)
    lo, _hi, end = extent(sections, bss_end)
    loaded = [s for s in sections if s.type == SHT_PROGBITS and s.flags & SHF_ALLOC and s.size]

    def word(address: int) -> int | None:
        for s in loaded:
            if s.address <= address and address + 4 <= s.address + s.size:
                return struct.unpack_from("<I", s.data, address - s.address)[0]
        return None

    functions = sorted((s.value, s.value + s.size) for s in symbols
                       if s.kind == STT_FUNC and s.section and s.size)
    starts = [start for start, _ in functions]

    def in_function(address: int) -> bool:
        index = bisect_right(starts, address) - 1
        return index >= 0 and address < functions[index][1]

    first_hi, last_hi = (lo + 0x8000) >> 16, (end - 1 + 0x8000) >> 16
    exempt = [(s, e) for s, e, kind, _ in ranges if kind in ("asset", "included", "handwritten")]
    reviewed = [(s, e) for s, e, kind, _ in ranges if kind == "unrelocated"]
    used: set[tuple[int, int]] = set()
    objects: dict[str, list[Section]] = {}
    found = []
    for name, address, size, obj in map_sections(map_path):
        if not any(s.address <= address < s.address + s.size for s in loaded):
            continue  # uninitialized data the file does not hold
        if obj not in objects:
            objects[obj] = read_elf(Path(obj))[0]
        indices = [i for i, s in enumerate(objects[obj]) if s.name == name]
        if len(indices) != 1:
            raise SystemExit(f"{obj}: {len(indices)} input sections {name}")
        types: dict[int, set[int]] = {}
        for section in objects[obj]:
            if section.type == SHT_REL and section.info == indices[0]:
                for offset, info in struct.iter_unpack("<II", section.data):
                    types.setdefault(offset, set()).add(info & 0xFF)
        for a in range((address + 3) & ~3, address + size - 3, 4):
            value, kinds, problem = word(a), types.get(a - address, set()), None
            opcode = value >> 26
            if name == ".text" and opcode == 0x0F and first_hi <= value & 0xFFFF <= last_hi:
                if R_MIPS_HI16 not in kinds:
                    problem = (f"lui {value:08x}: the %hi of an address in {lo:08x}-{end:08x}"
                               " without R_MIPS_HI16")
            elif name == ".text" and opcode in (2, 3):
                if R_MIPS_26 not in kinds:
                    problem = f"{'j' if opcode == 2 else 'jal'} {value:08x} without R_MIPS_26"
            elif (lo <= value < end and R_MIPS_32 not in kinds
                  and not (name == ".text" and in_function(a))):
                problem = f"word {value:08x}, an address in {lo:08x}-{end:08x}, without R_MIPS_32"
            if problem is None or any(s <= a < e for s, e in exempt):
                continue
            line = next(((s, e) for s, e in reviewed if s <= a < e), None)
            if line is not None:
                used.add(line)
                continue
            found.append(f"{a:08x} ({name} of {obj}): {problem}")
    for entry in found:
        print(f"error: {elf}: {entry}", file=sys.stderr)
    for s, e in sorted(set(reviewed) - used):
        print(f"error: {elf}: unrelocated line {s:08x}-{e:08x}: no word there that the"
              " relocation scan reports", file=sys.stderr)
    return 1 if found or set(reviewed) - used else 0


def strip_comments(text: str) -> str:
    """C text without its comments (each becomes a space or its line breaks)."""

    def blank(match: re.Match[str]) -> str:
        found = match.group(0)
        return found if found[0] in "\"'" else "\n" * found.count("\n") or " "

    return COMMENT_OR_LITERAL.sub(blank, text)


def nonmatching_names(roots: list[Path]) -> set[str]:
    """The functions linked as the fallback of a reviewed NON_MATCHING candidate
    in the C units and headers under roots. Only `nonmatching` rather than `asm`
    depends on this scan."""
    names: set[str] = set()
    for root in roots:
        for source in sorted(root.rglob("*.[ch]")):
            for _block, fallback in NON_MATCHING.findall(strip_comments(source.read_text())):
                names.update(INCLUDE_ASM.findall(fallback))
    return names


@dataclass
class Asm:
    """One occurrence of an asm statement's text in a C unit's cc1 output."""

    lines: list[str]
    function: str | None  # the compiled function it is written in
    kind: str = "other"  # or a TEMPLATES key, or "original" (an ORIGINAL_ASM macro)
    name: str = ""  # INCLUDE_ASM's function
    macros: int = 0  # GAS macro expansions inside it


@dataclass
class Unit:
    """A C unit's attribution: per tracked section its (start, end, owner)
    spans, owner None for cc1's own lines, the functions cc1 emitted, the
    labels cc1 defined outside them (its data objects) and the object's .text
    symbols by offset."""

    spans: dict[str, list[tuple[int, int, Asm | None]]]
    compiled: set[str]
    objects: set[str]
    labels: dict[int, set[str]]


def renumber(lines: list[str]) -> list[str]:
    """GAS input lines with maspsx's line-numbered labels numbered in order."""
    numbers: dict[str, str] = {}

    def label(match: re.Match[str]) -> str:
        number = numbers.setdefault(match.group(2), str(len(numbers)))
        return f".L_{match.group(1)}_#{number}"

    return [MASPSX_LINE_LABEL.sub(label, line) for line in lines]


def asm_statements(
    path: str, text: str
) -> tuple[list[Asm], set[str], set[str], list[tuple[str, int]]]:
    """The asm statement texts of a marked cc1 output in order, the functions cc1
    emitted outside them, the labels cc1 defined outside its functions, and the
    statement labels in order. Fails on a data directive cc1 emits where it
    places a function's code, in .text (a jump table under -membedded-pic)."""
    statements, compiled, objects, labels = [], set(), set(), []
    current, function, opened, in_text = None, None, False, True
    for line in text.splitlines():
        match = MARK.match(line)
        if opened and not (match and match.group(1) == "b"):
            raise SystemExit(f"{path}: asm text without labels follows #APP")
        opened = line.strip() == "#APP"
        if match:
            kind, number = match.group(1), int(match.group(2))
            labels.append((kind, number))
            if kind == "b" and current is None:
                current = (number, [])
            elif kind == "e" and current is not None and current[0] == number:
                statements.append(Asm(current[1], function))
                current = None
            else:
                raise SystemExit(f"{path}: unbalanced asm statement labels at Lcov{kind}_{number}")
        elif current is not None:
            current[1].append(line)
        elif entry := re.match(r"\s*\.ent\s+(\S+)", line):
            function = entry.group(1)
            compiled.add(function)
        elif re.match(r"\s*\.end\s", line):
            function = None
        elif switch := CC1_SECTION.match(line):
            in_text = (switch.group(1) or switch.group(2)) == ".text"
        elif function is not None and in_text and CC1_DATA.match(line):
            raise SystemExit(f"{path}: cc1 emitted data in the .text of {function}"
                             f" ({line.strip()}): a jump table or constant among its code")
        elif function is None and (label := CC1_LABEL.match(line)):
            objects.add(label.group(1))
    if current is not None:
        raise SystemExit(f"{path}: unbalanced asm statement labels at the end")
    return statements, compiled, objects, labels


def original_pattern(template: str) -> re.Pattern[str]:
    """An ORIGINAL_ASM template's text in the cc1 output, each line stripped:
    a register for each operand %N, the same one at every use (cc1 names $29
    and $30 `$sp` and `$fp`)."""
    parts, seen = [], set()
    for piece in re.split(r"(%\d)", "\n".join(line.strip() for line in template.split("\n"))):
        if re.fullmatch(r"%\d", piece):
            n = piece[1]
            parts.append(f"(?P=o{n})" if piece in seen else rf"(?P<o{n}>\$(?:\d+|sp|fp))")
            seen.add(piece)
        else:
            parts.append(re.escape(piece))
    return re.compile("".join(parts))


ORIGINAL_PATTERNS = [original_pattern(template) for template in ORIGINAL_ASM]


def classify(statement: Asm) -> None:
    text = "\n".join(line.strip() for line in statement.lines)
    for kind, template in TEMPLATES.items():
        match = template.fullmatch(text)
        if match:
            if kind == "asm":
                path = Path(match.group("path"))
                hack = f"__maspsx_include_asm_hack_{path.stem}"
                if path.suffix != ".s" or statement.function != hack:
                    return
                statement.name = path.stem
            statement.kind = kind
            return
    if statement.function and any(pattern.fullmatch(text) for pattern in ORIGINAL_PATTERNS):
        statement.kind = "original"


def coverage_build(obj: str) -> Unit:
    """Attribute a C unit's object by its coverage build (decomp/Makefile)."""
    side = obj[:-2] + ".cov.o"
    try:
        cc1, gas, marked_cc1, marked_gas = (
            Path(path).read_text(encoding="latin-1")
            for path in (obj + ".cc1.s", obj + ".s", side + ".cc1.s", side + ".s")
        )
    except FileNotFoundError as error:
        raise SystemExit(
            f"{obj}: no coverage build ({error.filename}); `make coverage` builds it"
        ) from None
    if [line for line in marked_cc1.splitlines() if not MARK.match(line)] != cc1.splitlines():
        raise SystemExit(f"{side}.cc1.s: without its labels, not the unit's cc1 output {obj}.cc1.s")
    unmarked = [line for line in marked_gas.splitlines() if not MARK.match(line)]
    if renumber(unmarked) != renumber(gas.splitlines()):
        raise SystemExit(f"{side}.s: without its labels, not the unit's GAS input {obj}.s")
    statements, compiled, objects, labels = asm_statements(side + ".cc1.s", marked_cc1)
    matches = [MARK.match(line) for line in marked_gas.splitlines()]
    if labels != [(match.group(1), int(match.group(2))) for match in matches if match]:
        raise SystemExit(f"{side}.s: its labels are not those of {side}.cc1.s")

    built = read_elf(Path(obj))
    sections, symbols = read_elf(Path(side))
    sizes = {}
    for name in TRACKED:
        found = []
        for table, table_symbols in (built, (sections, symbols)):
            index = next((i for i, s in enumerate(table) if s.name == name), None)
            found.append((0, b"", []) if index is None else (
                table[index].size, table[index].data, relocations(table, table_symbols, index)
            ))
        if found[0] != found[1]:
            raise SystemExit(f"{side}: its {name} differs from the built object's")
        sizes[name] = found[0][0]

    seen: dict[str, int] = {}
    marks = ["__cov_start"]
    for kind, number in labels:
        tag = f"{kind}_{number}"
        seen[tag] = seen.get(tag, -1) + 1
        marks.append(f"__cov_{tag}_{seen[tag]}")
    marks.append("__cov_end")
    values = {symbol.name: symbol for symbol in symbols if symbol.name.startswith("__cov_")}

    def record(mark: str, key: str, section: str | None) -> int:
        symbol = values.get(f"{mark}_{key}")
        if symbol is None or section is not None and sections[symbol.section].name != section:
            raise SystemExit(f"{side}: no position record {mark}_{key}")
        return symbol.value

    macros = [record(mark, "macros", None) for mark in marks]
    spans: dict[str, list[tuple[int, int, Asm | None]]] = {}
    for section, key in TRACKED.items():
        positions = [record(mark, key, section) for mark in marks]
        if positions[0] or positions[-1] != sizes[section] or positions != sorted(positions):
            raise SystemExit(f"{side}: the position records do not tile {section}")
        spans[section] = [
            (start, end, statements[(i - 1) // 2] if i % 2 else None)
            for i, (start, end) in enumerate(pairwise(positions)) if end > start
        ]
    for i, (before, after) in enumerate(pairwise(macros)):
        if i % 2:
            statements[(i - 1) // 2].macros = after - before - 1
        elif after - before - 1:
            raise SystemExit(f"{obj}: cc1's lines expanded a GAS macro (before {marks[i + 1]})")

    sections_of: dict[int, set[str]] = {}
    for name, rows in spans.items():
        for _start, _end, owner in rows:
            if owner is not None:
                sections_of.setdefault(id(owner), set()).add(name)
    for statement in statements:
        classify(statement)
        emitted = sections_of.get(id(statement), set())
        text = " / ".join(statement.lines)[:120]
        where = f"{obj}: asm statement in {statement.function or 'file scope'} ({text})"
        if statement.kind == "macros" and emitted:
            raise SystemExit(f"{where}: the macro.inc include emits bytes")
        if statement.kind == "asset" and ".text" in emitted:
            raise SystemExit(
                f"{where}: INCLUDE_ASSET/INCLUDE_ORIGINAL links original bytes into .text"
            )
        if statement.kind == "other":
            raise SystemExit(f"{where}: neither one of include_asm.h's statements nor an"
                             " original-style macro (ORIGINAL_ASM) in a compiled function")
        if statement.kind == "original" and (statement.macros or emitted - {".text"}):
            raise SystemExit(f"{where}: an original-style macro expands a GAS macro or emits"
                             f" {', '.join(sorted(emitted))} bytes")
    text_index = next((i for i, section in enumerate(built[0]) if section.name == ".text"), None)
    offsets: dict[int, set[str]] = {}
    for symbol in built[1]:
        if symbol.section == text_index and symbol.kind != STT_SECTION and symbol.name:
            offsets.setdefault(symbol.value, set()).add(symbol.name)
    return Unit(spans, compiled, objects, offsets)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("elf", type=Path, nargs="?")
    parser.add_argument("--map", type=Path, help="GNU ld map of the same link")
    parser.add_argument("--src", type=Path, action="append", default=[])
    parser.add_argument("--classification", type=Path)
    parser.add_argument("--list", choices=[
        "c", "nonmatching", "sdk", "handwritten", "asm", "included", "asset", "text_data",
        "bss", "placeholder", "bss_placeholder",
    ], help="list the class's functions, its .text bytes outside every function and its"
            " data ranges (each with its section and object)")
    parser.add_argument("--bss-end", type=lambda text: int(text, 0), help=(
        "the end of the target's uninitialized data past its image (the bound its loader"
        " clears), where the link does not place all of it"))
    parser.add_argument("--script-symbols", choices=["warn", "strict"], help=(
        "check the link instead: symbols the --script files assign inside the target's own"
        " image or uninitialized data"))
    parser.add_argument("--script", type=Path, action="append", default=[],
                        help="a linker script of the link (repeatable)")
    parser.add_argument("--views", type=Path, action="append", default=[], help=(
        "a --script whose names inside the target are views of linked symbols (repeatable)"))
    parser.add_argument("--relocations", action="store_true", help=(
        "check the link instead (with --script-symbols, too): every address of the target's"
        " own image or uninitialized data in a loaded word carries its relocation"))
    parser.add_argument("--mark-asm", action="store_true", help=(
        "coverage build: label each asm statement of preprocessed C (stdin to stdout)"))
    parser.add_argument("--mark-gas", action="store_true", help=(
        "coverage build: turn the labels of GAS input into position records (stdin to stdout)"))
    args = parser.parse_args()
    if args.mark_asm or args.mark_gas:
        text = sys.stdin.buffer.read().decode("latin-1")
        marked = mark_asm(text) if args.mark_asm else mark_gas(text)
        sys.stdout.buffer.write(marked.encode("latin-1"))
        return
    if args.elf is None or args.map is None:
        parser.error("the ELF and --map are required")
    if args.script_symbols or args.relocations:
        status = 0
        if args.script_symbols:
            status |= check_script_symbols(args.elf, args.map, args.script, args.views,
                                           args.bss_end, args.script_symbols == "strict")
        if args.relocations:
            status |= check_relocations(args.elf, args.map, classification(args.classification),
                                        args.bss_end)
        sys.exit(status)

    nonmatching = nonmatching_names(args.src)
    ranges = classification(args.classification)
    # The data objects C units may place in .text: START: (END, NAME).
    allowed = {s: (e, note.split()[0]) for s, e, kind, note in ranges if kind == "text_data"}
    # The reviewed reasons of included objects: START: (END, NAME).
    reasons = {s: (e, note.split()[0]) for s, e, kind, note in ranges if kind == "included"}
    ranges = [entry for entry in ranges
              if entry[2] not in ("text_data", "included", "unrelocated")]

    def ranged(address: int) -> str | None:
        return next((k for s, e, k, _ in ranges if s <= address < e), None)

    def covered(lo: int, hi: int, kind: str) -> bool:
        """Whether ranges classified kind cover [lo, hi)."""
        for s, e, k, _note in sorted(ranges):
            if k == kind and s <= lo < e:
                lo = e
        return lo >= hi

    elf_sections, elf_symbols = read_elf(args.elf)
    _image_start, image_end, bss_end = extent(elf_sections, args.bss_end)
    inputs = map_sections(args.map)
    units: dict[str, Unit] = {}
    # The defined symbols by address (an absolute one too: a linker script
    # assignment may shadow an object's label).
    defined = sorted((s.value, s.name) for s in elf_symbols
                     if s.name and s.kind not in (STT_SECTION, STT_FILE) and s.section)

    def labels(lo: int, hi: int) -> list[tuple[int, str]]:
        return defined[bisect_left(defined, (lo, "")):bisect_left(defined, (hi, ""))]

    def image(lo: int, hi: int) -> bytes:
        """The loaded bytes [lo, hi), of one section."""
        for s in elf_sections:
            if s.type == SHT_PROGBITS and s.flags & SHF_ALLOC and s.address <= lo <= hi <= (
                    s.address + s.size):
                return s.data[lo - s.address:hi - s.address]
        raise SystemExit(f"{lo:08x}-{hi:08x}: not the loaded bytes of one section")

    def unit_of(obj: str) -> tuple[str, Unit | None]:
        """("c", attribution) for a C unit; ("handwritten", None) for an authored
        assembly unit under decomp/src; ("generated", None) otherwise."""
        if "/decomp/src/" not in obj:
            return "generated", None
        source = Path("decomp/src/" + obj.split("/decomp/src/", 1)[1]).with_suffix(".c")
        if not source.exists():
            return "handwritten", None
        if obj not in units:
            units[obj] = coverage_build(obj)
        return "c", units[obj]

    def split(lo: int, hi: int, cls: str) -> list[tuple[int, int, str]]:
        """[lo, hi) as (start, end, class) pieces: cls outside the classified
        ranges, which take precedence."""
        pieces = []
        for s, e, kind, _note in sorted(ranges):
            cut_lo, cut_hi = max(lo, s), min(hi, e)
            if cut_lo < cut_hi:
                pieces += [(lo, cut_lo, cls), (cut_lo, cut_hi, kind)]
                lo = cut_hi
        return [piece for piece in pieces + [(lo, hi, cls)] if piece[1] > piece[0]]

    texts = [(address, address + size, obj) for name, address, size, obj in inputs
             if name == ".text"]
    totals: dict[str, list[int]] = {}  # class: functions, bytes, bytes in functions
    listing: list[tuple[int, int, str]] = []

    def handwritten(cls: str, lo: int, hi: int, what: str) -> None:
        if cls == "handwritten" and not covered(lo, hi, "handwritten"):
            raise SystemExit(f"{what} ({lo:08x}-{hi:08x}): handwritten bytes outside every"
                             " handwritten range of the classification")

    def count(cls: str, address: int, size: int, function: str | None,
              label: str = "(outside every function)") -> None:
        handwritten(cls, address, address + size, function or "text outside every function")
        entry = totals.setdefault(cls, [0, 0, 0])
        entry[1] += size
        if function is not None:
            entry[0] += 1
            entry[2] += size
        if cls == args.list:
            listing.append((address, size, function or label))

    functions: dict[int, list[tuple[int, int, str]]] = {}  # per .text input section
    owned: dict[int, dict[str, str]] = {}  # INCLUDE_ASM statement: its functions' classes
    for symbol in sorted(elf_symbols, key=lambda s: (s.value, s.name)):
        if symbol.kind != STT_FUNC or symbol.section == 0:
            continue
        text = next(((lo, hi, obj) for lo, hi, obj in texts if lo <= symbol.value < hi), None)
        if text is None:
            section = next((f"{kind} of {obj}" for kind, lo, length, obj in inputs
                            if lo <= symbol.value < lo + length), None)
            if section:
                raise SystemExit(f"{symbol.name} ({symbol.value:08x}): a function in {section}")
            continue
        address, size, name = symbol.value, symbol.size, symbol.name
        if address % 4 or size % 4:
            raise SystemExit(f"unaligned MIPS function range: {name} at {address:08x}, size {size}")
        if address + size > text[1]:
            raise SystemExit(f"{name} ({address:08x}, size {size}) runs past its input section"
                             f" .text of {text[2]}")
        functions.setdefault(text[0], []).append((address, address + size, name))
        kind, unit = unit_of(text[2])
        if unit is None:
            cls = ranged(address) or ("handwritten" if kind == "handwritten" else "asm")
        else:
            lo = address - text[0]
            hi = max(lo + size, lo + 1)
            owners = [owner for start, end, owner in unit.spans[".text"] if start < hi and end > lo]
            if len(owners) == 1 and owners[0] is not None and owners[0].kind in ("asm", "rodata"):
                cls = "nonmatching" if name in nonmatching else ranged(address) or "asm"
                owned.setdefault(id(owners[0]), {})[name] = cls
            elif name in unit.compiled and all(
                owner is None or owner.kind == "original" and owner.function == name
                for owner in owners
            ):
                cls = "c"
            else:
                raise SystemExit(
                    f"{name} ({address:08x}, {text[2]}): a function that cc1 did not emit and no"
                    " INCLUDE_ASM/INCLUDE_RODATA file defines, or one holding included bytes"
                )
        count(cls, address, size, name)

    def statement_class(statement: Asm) -> str:
        if statement.kind != "asm":
            return "included"
        fallback = "nonmatching" if statement.name in nonmatching else "asm"
        return owned.get(id(statement), {}).get(statement.name, fallback)

    # Each included object: its bytes in one section, its unit.
    included: list[tuple[int, int, str]] = []
    used: set[int] = set()

    def text_data(obj: str, unit: Unit, base: int, a: int, b: int) -> None:
        """Count [a, b), bytes cc1 put in the .text of a C unit (at base)
        outside every function, as the allowed data objects that tile it, each
        from its cc1 label to the next symbol."""
        while a < b:
            if a not in allowed or allowed[a][0] > b:
                raise SystemExit(
                    f"{obj}: .text bytes {a:08x}-{b:08x} that cc1 emitted outside every function"
                    " are a data object in .text, which counts only as a text_data line of the"
                    " classification naming it with the evidence that the original keeps it there"
                )
            end, name = allowed[a]
            if (name not in unit.objects or name not in unit.labels.get(a - base, ())
                    or any(a - base < offset < end - base for offset in unit.labels)):
                raise SystemExit(
                    f"{obj}: text_data {name} ({a:08x}-{end:08x}) is not one data object cc1"
                    " defined there"
                )
            count("text_data", a, end - a, None, name)
            used.add(a)
            a = end

    # The .text bytes outside every function count as their owner's: an
    # INCLUDE_ASM/INCLUDE_RODATA file's (padding, data words) as its
    # statement's, an assembly unit's as its own; cc1's are data objects in
    # .text, never C code, counted only as allowed text_data.
    for lo, hi, obj in texts:
        holes, cursor, before = [], lo, ""
        for start, end, name in sorted(functions.get(lo, [])):
            if end == start:
                continue
            if start < cursor:
                raise SystemExit(f"{name} ({start:08x}) overlaps {before} ({obj})")
            holes += [(cursor, start)] if start > cursor else []
            cursor, before = end, name
        holes += [(cursor, hi)] if hi > cursor else []
        kind, unit = unit_of(obj)
        if unit is None:
            own = "handwritten" if kind == "handwritten" else "asm"
            pieces = [(a, b, own) for a, b in holes]
        else:
            if hi - lo != sum(end - start for start, end, _owner in unit.spans[".text"]):
                raise SystemExit(f"{obj}: input section .text is not the attributed one")
            pieces, objects = [], []
            for start, end, owner in unit.spans[".text"]:
                for a, b in holes:
                    a, b = max(a, lo + start), min(b, lo + end)
                    if a >= b:
                        continue
                    if owner is None:
                        if objects and objects[-1][1] == a:
                            a = objects.pop()[0]
                        objects.append((a, b))
                    elif owner.kind in ("asm", "rodata"):
                        pieces.append((a, b, statement_class(owner)))
                        if pieces[-1][2] == "included":
                            included.append((a, b, obj))
                    else:
                        raise SystemExit(f"{obj}: .text bytes {a:08x}-{b:08x} of an asm statement"
                                         " lie outside every function")
            for a, b in objects:
                text_data(obj, unit, lo, a, b)
        for a, b, own in pieces:
            for start, end, cls in split(a, b, own):
                count(cls, start, end - start, None)
    unused = sorted(set(allowed) - used)
    if unused:
        raise SystemExit(f"text_data {allowed[unused[0]][1]} ({unused[0]:08x}): no data object"
                         " cc1 placed in .text there")

    data: dict[str, int] = {}
    loaded = [(s.address, s.address + s.size) for s in elf_sections
              if s.type == SHT_PROGBITS and s.flags & SHF_ALLOC]

    def tally(cls: str, lo: int, hi: int, section: str, obj: str) -> None:
        handwritten(cls, lo, hi, f"{section} of {obj}")
        data[cls] = data.get(cls, 0) + hi - lo
        if cls == args.list:
            label = f"({section} {Path(obj).name})"
            named = labels(lo, hi) if cls == "included" else []
            label = f"{named[0][1]} {label}" if named else label
            if listing and listing[-1][0] + listing[-1][1] == lo and listing[-1][2] == label:
                listing[-1] = (listing[-1][0], hi - listing[-1][0], label)
            else:
                listing.append((lo, hi - lo, label))

    # The uninitialized data past the image, apart: each input section by its
    # object, then every byte no input section holds (bss_placeholder: a
    # variable only a linker-script name places), each listed range split at
    # its symbols.
    noload: dict[str, int] = {}
    held: list[tuple[int, int]] = []

    def tally_noload(cls: str, lo: int, hi: int, section: str | None, obj: str | None) -> None:
        handwritten(cls, lo, hi, f"{section} of {obj}" if obj else "uninitialized data")
        noload[cls] = noload.get(cls, 0) + hi - lo
        if cls != args.list:
            return
        label = f"({section} {Path(obj).name})" if obj else "(no object)"
        if cls != "bss_placeholder":
            if listing and listing[-1][0] + listing[-1][1] == lo and listing[-1][2] == label:
                listing[-1] = (listing[-1][0], hi - listing[-1][0], label)
            else:
                listing.append((lo, hi - lo, label))
            return
        cuts = sorted({lo} | {value for value, _ in labels(lo, hi)}) + [hi]
        names = dict(reversed(labels(lo, hi)))  # the first name at each address
        for start, stop in pairwise(cuts):
            listing.append((start, stop - start,
                            f"{names[start]} {label}" if start in names else label))

    for name, address, size, obj in inputs:
        if name == ".text":
            continue
        if not DATA_SECTION.match(name):
            raise SystemExit(f"{obj}: input section {name} is not counted")
        unloaded = bool(BSS_SECTION.match(name)) and not any(
            lo <= address and address + size <= hi for lo, hi in loaded)
        if unloaded:
            if not image_end <= address <= address + size <= bss_end:
                raise SystemExit(f"{obj}: no-load {name} ({address:08x}-{address + size:08x})"
                                 " outside the target's uninitialized data")
            held.append((address, address + size))
        kind, unit = unit_of(obj)
        if unit is None:
            own = ("handwritten" if kind == "handwritten" else
                   "bss_placeholder" if unloaded else "placeholder")
            pieces = split(address, address + size, own)
        else:
            rows = unit.spans.get(name)
            if rows is None or size != sum(end - start for start, end, _owner in rows):
                raise SystemExit(f"{obj}: input section {name} is not the attributed one")
            pieces = []
            for start, end, owner in rows:
                own = (statement_class(owner) if owner is not None
                       else "bss" if BSS_SECTION.match(name) else "c")
                if own == "included":
                    included.append((address + start, address + end, obj))
                pieces += split(address + start, address + end, own)
        for start, end, cls in pieces:
            (tally_noload if unloaded else tally)(cls, start, end, name, obj)
    # The bytes no input section holds: outside every no-load output section
    # (the link places nothing there) or named by a symbol, a variable that
    # only a linker-script name places; alignment fill between input
    # sections, as in the image, is not counted.
    sections_noload = [(s.address, s.address + s.size) for s in elf_sections
                       if s.type == SHT_NOBITS and s.flags & SHF_ALLOC and s.size]
    cuts = sorted({image_end, bss_end} | {x for span in held + sections_noload for x in span})
    for start, end in pairwise(cuts):
        if start < image_end or end > bss_end or any(a <= start < b for a, b in held):
            continue
        if any(a <= start and end <= b for a, b in sections_noload) and not labels(start, end):
            continue
        tally_noload("bss_placeholder", start, end, None, None)

    # Every included object a classified range does not cover needs a reason:
    # a stray byte in its string's padding or a reviewed classification line.
    reasoned: set[int] = set()
    for lo, hi, obj in included:
        named = labels(lo, hi)
        start, name = named[0] if named else (lo, "")
        if any(image(lo, start)):
            start = lo
        # The zero alignment fill an INCLUDE_* statement emits ahead of its label
        # (`.align 2`) needs no reason of its own; the object starts at the label.
        if not any(cls == "included" for _, _, cls in split(start, hi, "included")):
            continue
        names = {label for value, label in named if value == start} or {"-"}
        if start in reasons and reasons[start][0] == hi and reasons[start][1] in names:
            reasoned.add(start)
        elif not (start % 4 == 0 and hi % 4 == 0 and stray_padding(image(start, hi))):
            raise SystemExit(
                f"{obj}: included object {name or '(unnamed)'} ({start:08x}-{hi:08x}) has no"
                " stray byte in a string's alignment padding and no `included` line of the"
                " classification giving the reason it stays original"
            )
    for start in sorted(set(reasons) - reasoned):
        raise SystemExit(f"included line {reasons[start][1]} ({start:08x}-{reasons[start][0]:08x}):"
                         " no included object there")

    if args.list:
        print("\n".join(f"{address:08x} {size:6d} {name}" for address, size, name
                        in sorted(listing)))
        return
    text_bytes = sum(v[1] for v in totals.values())
    if text_bytes != sum(hi - lo for lo, hi, _obj in texts):
        raise SystemExit("the text classes do not add up to the .text input sections")
    remaining = [sum(totals.get(cls, [0, 0, 0])[i] for cls in ("asm", "nonmatching"))
                 for i in range(3)]
    report = {
        "claim": "source_coverage_only",
        "binary_agreement": "not_measured",
        "text_bytes": text_bytes,
        "text_instructions": sum(v[2] for v in totals.values()) // 4,
        "classes": {
            k: {"functions": v[0], "bytes": v[1], "instructions": v[2] // 4}
            for k, v in sorted(totals.items())
        },
        "remaining_asm_functions": remaining[0],
        "remaining_asm_bytes": remaining[1],
        "remaining_asm_instructions": remaining[2] // 4,
        "data_bytes": sum(data.values()),
        "data_classes": dict(sorted(data.items())),
        "remaining_data_placeholder_bytes": data.get("placeholder", 0),
        # Data that unrecovered or nonmatching assembly carries: the compiler
        # emits it once the function is C.
        "remaining_data_asm_bytes": data.get("asm", 0) + data.get("nonmatching", 0),
        # The uninitialized data past the image, which the file does not hold.
        "bss_noload_bytes": sum(noload.values()),
        "bss_noload_classes": dict(sorted(noload.items())),
        "remaining_bss_placeholder_bytes": noload.get("bss_placeholder", 0),
    }
    print(json.dumps(report, sort_keys=True))


if __name__ == "__main__":
    main()
