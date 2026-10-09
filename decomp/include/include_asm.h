#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

/*
 * Link a MIPS function into this translation unit. Files generated under
 * .local/ are private recovery scaffolding; authored handwritten routines
 * belong under decomp/src/. tools/matching_coverage.py counts both as assembly
 * under their explicit classification, never as recovered compiled C.
 * The maspsx_hack form keeps GCC from reordering file-scope asm blocks.
 */
#define INCLUDE_ASM(FOLDER, NAME)                                              \
    void __maspsx_include_asm_hack_##NAME() {                                  \
        __asm__(".text # maspsx-keep\n"                                        \
                "\t.align\t2 # maspsx-keep\n"                                  \
                "\t.set noreorder # maspsx-keep\n"                             \
                "\t.set noat # maspsx-keep\n"                                  \
                ".include \"" FOLDER "/" #NAME ".s\" # maspsx-keep\n"          \
                "\t.set reorder # maspsx-keep\n"                               \
                "\t.set at # maspsx-keep\n");                                  \
    }
#define INCLUDE_RODATA(FOLDER, NAME)                                           \
    __asm__(".section .rodata\n"                                               \
            "\t.include \"" FOLDER "/" #NAME ".s\"\n"                          \
            ".section .text")

/*
 * Link SIZE bytes of the user's original image at VRAM as NAME: media or
 * bytecode embedded in a unit's data (a packed image, font or sound bank),
 * which stays user-supplied and out of the source (an `asset` range in the
 * target's classification). The target defines ORIGINAL_IMAGE (its pristine
 * input) and ORIGINAL_BASE (the VRAM of file offset 0).
 */
#define INCLUDE_ASSET_STR(X) #X
#define INCLUDE_ASSET_XSTR(X) INCLUDE_ASSET_STR(X)
#define INCLUDE_ASSET(SECTION, NAME, VRAM, SIZE)                               \
    __asm__(".section " SECTION "\n"                                          \
            "\t.align 2\n"                                                    \
            "\t.globl " #NAME "\n"                                            \
            #NAME ":\n"                                                        \
            "\t.incbin \"" ORIGINAL_IMAGE "\", " #VRAM " - "                  \
            INCLUDE_ASSET_XSTR(ORIGINAL_BASE) ", " #SIZE "\n"                 \
            "\t.size " #NAME ", " #SIZE "\n"                                  \
            "\t.previous")

/*
 * The .data analogue of INCLUDE_RODATA: an object whose alignment padding
 * holds stray bytes the original assembler left there and nothing reads.
 * C cannot emit them, so the object and its padding (SIZE bytes at VRAM)
 * stay original, linked from the pristine input as INCLUDE_ASSET does, at
 * the object's place among its unit's definitions.
 * tools/matching_coverage.py counts NAME's bytes as `included`, not as C.
 */
#define INCLUDE_ORIGINAL(SECTION, NAME, VRAM, SIZE)                            \
    INCLUDE_ASSET(SECTION, NAME, VRAM, SIZE)

/*
 * INCLUDE_ORIGINAL for an object GCC aligns to a byte (a u8 flag ahead of
 * its padding), which follows the object before it directly: no .align.
 */
#define INCLUDE_ORIGINAL_UNALIGNED(SECTION, NAME, VRAM, SIZE)                  \
    __asm__(".section " SECTION "\n"                                          \
            "\t.globl " #NAME "\n"                                            \
            #NAME ":\n"                                                        \
            "\t.incbin \"" ORIGINAL_IMAGE "\", " #VRAM " - "                  \
            INCLUDE_ASSET_XSTR(ORIGINAL_BASE) ", " #SIZE "\n"                 \
            "\t.size " #NAME ", " #SIZE "\n"                                  \
            "\t.previous")

__asm__(".include \"macro.inc\"\n");

#endif
