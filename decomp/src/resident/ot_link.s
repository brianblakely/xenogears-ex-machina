# Shared body of the resident's ordering-table link helpers: one helper per
# PsyQ primitive type, in libgpu's struct order, each fixing the packet's
# word count (the length byte that setlen() would store).
#   a0 = ordering-table entry, a1 = packet.
# Prepend the packet to the entry: the entry receives the packet's 24-bit
# address, and the packet's tag receives the entry's previous contents ORed
# with words << 24. The previous contents are not masked; an ordering-table
# entry normally holds a link with a zero length byte. The tag is written
# through the masked address (the KUSEG mirror of the packet).
# Clobbers a1, t0 and at; no frame. Handwritten: the two 32-bit immediates
# are the assembler's and/or macro expansions through $at.
.ifndef RESIDENT_OT_LINK_MACROS
.set RESIDENT_OT_LINK_MACROS, 1

.macro ot_link name, words
glabel \name
    lui     $at, 0x00FF
    ori     $at, $at, 0xFFFF
    and     $a1, $a1, $at       # packet address without its segment byte
    lw      $t0, 0($a0)         # previous head of the entry
    sw      $a1, 0($a0)
    lui     $at, \words*0x100   # packet word count in the tag's top byte
    or      $t0, $t0, $at
    jr      $ra
     sw     $t0, 0($a1)
endlabel \name
.endm

.endif
