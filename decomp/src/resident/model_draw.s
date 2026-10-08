# Shared operations of the resident model primitive renderers: the draw
# routines that the primitive type table D_8004FE50 lists per type and
# draw mode, called by func_8002C700 as draw(records, count).
# These macros emit no code until a renderer uses them.
#
# Every renderer walks `count` eight-byte face records (u16 vertex indices
# 0..2, and index 3 for quads) and owns one packet slot per face, in order,
# in the packet buffer at D_80059424 that func_8002C8CC prepared through
# the types' prepare routines. It writes the screen coordinates (and some
# colours or UVs) of the accepted faces and links them into the ordering
# table; other packet words stay as prepared. Rotation, translation,
# projection, OT/depth-cue scales and lights are already in the GTE.
# Each loop starts projecting the face it is about to test, then loads the
# next record and its vertices while the GTE works: the record after the
# last face is read and projected too, and a count of 0 still projects
# the first face. Faces whose RTPT/RTPS FLAG has the error bit (31) set,
# where some result overflowed or saturated, are culled.
#
# Register use common to the renderers (variants are noted per routine):
#   a0 = face record, a1 = faces left, a3 = packet size in bytes,
#   s0 = vertex array (D_8005953C, eight-byte SVECTORs),
#   s2 = primitives drawn (D_80059578), s3 = packet slot,
#   s4 = ordering table (D_80059568), s6 = 0x00FFFFFF DMA address mask,
#   v0 = packed y limit (D_800500FC), v1 = x limit (D_800500F8),
#   t1..t4 = packed SXY of the face, t4/t5 = record words being decoded,
#   t7 = OT depth shift (D_80050100), t8 = DMA tag length (words << 24),
#   t9 = byte step between the packet's XY words.
# On the culling path s3 is reduced to its 24-bit DMA address (main RAM is
# mirrored at 0) and stays so. A linked packet's tag is the old OT word
# ORed with the tag length, without masking, and the OT word becomes the
# packet's address. All renderers leave through the exit in
# func_8002E010.s with a3 = packet size: it stores s2 and the slot pointer
# advanced past the last face to D_80059578 and D_80059424 and restores
# the s registers. Other registers, at and the GTE state are clobbered.
.ifndef RESIDENT_MODEL_DRAW_MACROS
.set RESIDENT_MODEL_DRAW_MACROS, 1

# Packet format of a multi-format entry point: XY word step, tag length and
# packet size (the tag word plus `words` words).
.macro model_packet xy_step, words
    ori     $t9, $zero, \xy_step
    lui     $t8, \words << 8
    ori     $a3, $zero, (\words + 1) * 4
.endm

# The renderers keep no frame. They save all eight s registers below the
# caller's unchanged stack pointer, leaving sp-12 unused; the shared exit
# in func_8002E010.s restores them.
.macro model_draw_save
    sw      $s0, -4($sp)
    sw      $s1, -8($sp)
    sw      $s2, -16($sp)
    sw      $s3, -20($sp)
    sw      $s4, -24($sp)
    sw      $s5, -28($sp)
    sw      $s6, -32($sp)
    sw      $s7, -36($sp)
.endm

# Load the drawing state; `shift` receives the OT depth shift. With
# normals=1, a3 also receives the vertex normal array (D_8005952C).
.macro model_draw_state shift, normals=0
    lui     $s0, %hi(D_8005953C)
    lw      $s0, %lo(D_8005953C)($s0)
.if \normals
    lui     $a3, %hi(D_8005952C)
    lw      $a3, %lo(D_8005952C)($a3)
.endif
    lui     $s2, %hi(D_80059578)
    lw      $s2, %lo(D_80059578)($s2)
    lui     $s3, %hi(D_80059424)
    lw      $s3, %lo(D_80059424)($s3)
    lui     $s4, %hi(D_80059568)
    lw      $s4, %lo(D_80059568)($s4)
    lui     $v0, %hi(D_800500FC)
    lw      $v0, %lo(D_800500FC)($v0)
    lui     $v1, %hi(D_800500F8)
    lw      $v1, %lo(D_800500F8)($v1)
    lui     \shift, %hi(D_80050100)
    lw      \shift, %lo(D_80050100)(\shift)
.endm

# Vertex addresses of the record whose first word is in t4 (indices 0 and
# 1) and third halfword in t5 (index 2). Index 1's byte offset is taken
# as (word >> 13) & 0xFFF8, keeping only its low thirteen bits.
.macro model_vertex0 reg
    andi    \reg, $t4, 0xFFFF
    sll     \reg, \reg, 3
    addu    \reg, \reg, $s0
.endm

.macro model_vertex1 reg
    srl     \reg, $t4, 13
    andi    \reg, \reg, 0xFFF8
    addu    \reg, \reg, $s0
.endm

.macro model_vertex2 reg
    sll     \reg, $t5, 3
    addu    \reg, \reg, $s0
.endm

# Screen bounds: a face is kept when some vertex passes the y test and
# some vertex, not necessarily the same one, passes the x test. The y test
# compares the packed SXY word unsigned with v0 = limit << 16, accepting
# 0 <= y < limit whatever x is; the x test compares its low halfword
# unsigned with v1. Negative coordinates fail both.
#
# The caller computes the first vertex's y test into `first` (normally
# sltu $t0, $t1, $v0) and places the accept label on the instruction that
# follows the macro: it is the delay slot of the final reject branch too.
.macro model_y_test count, reject, accept, first=$t0
    bnez    \first, \accept
     sltu   $t0, $t2, $v0
    bnez    $t0, \accept
     sltu   $t0, $t3, $v0
.if \count == 4
    bnez    $t0, \accept
     sltu   $t0, $t4, $v0
.endif
    beqz    $t0, \reject
.endm

# The caller masks SXY0 first (andi $t0, $t1, 0xFFFF), normally as the
# instruction at the y test's accept label.
.macro model_x_test count, reject, accept
    sltu    $t0, $t0, $v1
    bnez    $t0, \accept
     andi   $t0, $t2, 0xFFFF
    sltu    $t0, $t0, $v1
    bnez    $t0, \accept
     andi   $t0, $t3, 0xFFFF
.if \count == 4
    sltu    $t0, $t0, $v1
    bnez    $t0, \accept
     andi   $t0, $t4, 0xFFFF
.endif
    sltu    $t0, $t0, $v1
    beqz    $t0, \reject
.endm

.endif
