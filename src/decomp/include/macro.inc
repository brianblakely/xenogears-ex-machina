# Labels used by splat/spimdisasm assembly: global functions and data, local
# jump-table targets, and the size/extent markers of each original symbol.
.macro glabel label
    .global \label
    .type \label, @function
    \label:
.endm

.macro dlabel label
    .global \label
    \label:
.endm

.macro jlabel label
    \label:
.endm

.macro endlabel label
    .size \label, . - \label
.endm

.macro enddlabel label
    .size \label, . - \label
.endm

.macro nonmatching label, size=1
.endm

# Alternative entry label inside a function (a second symbol at an address
# another function jumps or calls into).
.macro alabel label
    .global \label
    \label:
.endm

# Geometry Transformation Engine (coprocessor 2) commands, as printed by
# rabbitizer. Each is COP2 with its command word; sf/mx/v/cv/lm are fields.
.macro gte_cmd word
    .word 0x4A000000 | (\word)
.endm
.macro rtps
    gte_cmd 0x0180001
.endm
.macro rtpt
    gte_cmd 0x0280030
.endm
.macro nclip
    gte_cmd 0x1400006
.endm
.macro avsz3
    gte_cmd 0x158002D
.endm
.macro avsz4
    gte_cmd 0x168002E
.endm
.macro dpcs
    gte_cmd 0x0780010
.endm
.macro dpct
    gte_cmd 0x0F8002A
.endm
.macro dpcl
    gte_cmd 0x0680029
.endm
.macro intpl
    gte_cmd 0x0980011
.endm
.macro ncs
    gte_cmd 0x0C8041E
.endm
.macro nct
    gte_cmd 0x0D80420
.endm
.macro ncds
    gte_cmd 0x0E80413
.endm
.macro ncdt
    gte_cmd 0x0F80416
.endm
.macro nccs
    gte_cmd 0x108041B
.endm
.macro ncct
    gte_cmd 0x118043F
.endm
.macro cdp
    gte_cmd 0x1280414
.endm
.macro cc
    gte_cmd 0x138041C
.endm
.macro sqr sf
    gte_cmd 0x0A00428 | (\sf << 19)
.endm
.macro op sf
    gte_cmd 0x170000C | (\sf << 19)
.endm
.macro gpf sf
    gte_cmd 0x190003D | (\sf << 19)
.endm
.macro gpl sf
    gte_cmd 0x1A0003E | (\sf << 19)
.endm
.macro mvmva sf, mx, v, cv, lm
    gte_cmd 0x0400012 | (\sf << 19) | (\mx << 17) | (\v << 15) | (\cv << 13) | (\lm << 10)
.endm
