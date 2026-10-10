# Model renderers that sort by one vertex depth instead of the average:
# model_draw_gt3_far/model_draw_gt4_far use the farthest vertex (largest SZ),
# model_draw_gt3_near/model_draw_gt4_near the nearest (smallest SZ). Each pair shares
# its body. `keep` is the branch, after slt of candidate < current, that
# keeps the current depth: bnez keeps the largest, beqz the smallest. The
# comparisons are signed. With InitGeom's ZSF3/ZSF4 (0x155, 0x100) an
# AVSZ OTZ is the average SZ / 4, so one SZ is shifted by model_ot_depth_shift + 2.
# Packets keep their prepared colours.
# See model_draw.s for the records, packet slots, pipelining, register use
# and bounds test, and model_draw_gt3_avg.s, whose projection and culling
# these routines repeat. a0 = records, a1 = count; the same globals are
# read and written. Handwritten for the same reasons: no frame, four entry
# points per body (selecting the packet format) and the exit in another
# routine.
.ifndef RESIDENT_MODEL_DEPTH_MACROS
.set RESIDENT_MODEL_DEPTH_MACROS, 1

# Triangles (POLY_GT3, G3, F3 and FT3 entries). As with AVSZ3 sorting,
# the three SXY words are written once the face is front-facing (the
# first also for a back face), s2 counts it, and a zero depth leaves it
# unlinked.
.macro model_depth_triangles gt3, g3, f3, ft3, keep
glabel \gt3
    model_packet 12, 9          # POLY_GT3
    j       .L\gt3\()_setup
     nop
alabel \g3
    model_packet 8, 6           # POLY_G3
    j       .L\gt3\()_setup
     nop
alabel \f3
    model_packet 4, 4           # POLY_F3
    j       .L\gt3\()_setup
     nop
alabel \ft3
    model_packet 8, 7           # POLY_FT3
.L\gt3\()_setup:
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addi    $t7, $t7, 2
    model_vertex0 $t0
    lwc2    $0, 0($t0)
    lwc2    $1, 4($t0)
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    subu    $s3, $s3, $a3
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.L\gt3\()_next:
    rtpt
    beqz    $a1, .Lmodel_draw_exit
     addiu  $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addu    $s3, $s3, $a3
    model_vertex0 $t0
    lwc2    $0, 0($t0)
    lwc2    $1, 4($t0)
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t1, $12
    bltz    $t0, .L\gt3\()_next
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .L\gt3\()_next, .L\gt3\()_y
.L\gt3\()_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .L\gt3\()_next, .L\gt3\()_x
.L\gt3\()_x:
     mfc2   $t4, $24            # NCLIP result
    mfc2    $t5, $18            # SZ1..SZ3: the three vertices
    blez    $t4, .L\gt3\()_next
     sw     $t1, 8($s3)
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    addu    $t0, $t0, $t9
    sw      $t3, 8($t0)
    mfc2    $t0, $17
    mfc2    $t2, $19
    slt     $t3, $t5, $t0
    \keep   $t3, .L\gt3\()_depth1
     and    $s3, $s3, $s6
    addu    $t0, $t5, $zero
.L\gt3\()_depth1:
    slt     $t3, $t2, $t0
    \keep   $t3, .L\gt3\()_depth2
     addiu  $s2, $s2, 1
    addu    $t0, $t2, $zero
.L\gt3\()_depth2:
    beqz    $t0, .L\gt3\()_next
     srav   $t0, $t0, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .L\gt3\()_next
     sw     $t1, 0($s3)
endlabel \gt3
.endm

# Quads (POLY_GT4, G4, F4 and FT4 entries). After the bounds test the
# SXY words are written, but a zero SZ at any of the four points rejects
# the quad before it is counted or linked (the fourth SXY is written only
# once SZ0 is nonzero); the final zero depth test can no longer fail.
# A nop opens each depth step: the first covers the move delay of SZ2,
# read in a branch delay slot; the other two only repeat the pattern.
.macro model_depth_quads gt4, g4, f4, ft4, keep
glabel \gt4
    model_packet 12, 12         # POLY_GT4
    j       .L\gt4\()_setup
     nop
alabel \g4
    model_packet 8, 8           # POLY_G4
    j       .L\gt4\()_setup
     nop
alabel \f4
    model_packet 4, 5           # POLY_F4
    j       .L\gt4\()_setup
     nop
alabel \ft4
    model_packet 8, 9           # POLY_FT4
.L\gt4\()_setup:
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addi    $t7, $t7, 2
    model_vertex0 $t6           # V0 is loaded at the loop top
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    subu    $s3, $s3, $a3
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.L\gt4\()_next:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    beqz    $a1, .Lmodel_draw_exit
     rtpt
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addu    $s3, $s3, $a3
    model_vertex0 $t6
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t1, $12
    bltz    $t0, .L\gt4\()_next
     mfc2   $t2, $13
    mfc2    $t3, $14
    nclip
    lhu     $t5, -2($a0)        # index 3
    mfc2    $t4, $24            # NCLIP result
    sll     $t0, $t5, 3
    blez    $t4, .L\gt4\()_next
     addu   $t0, $t0, $s0
    lwc2    $0, 0($t0)          # the fourth point
    lwc2    $1, 4($t0)
    rtps
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t4, $14
    bltz    $t0, .L\gt4\()_next
     sltu   $t0, $t1, $v0
    model_y_test 4, .L\gt4\()_next, .L\gt4\()_y
.L\gt4\()_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 4, .L\gt4\()_next, .L\gt4\()_x
.L\gt4\()_x:
     sw     $t1, 8($s3)
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    addu    $t0, $t0, $t9
    mfc2    $t1, $16            # SZ0..SZ3: the four points
    sw      $t3, 8($t0)
    beqz    $t1, .L\gt4\()_next
     addu   $t0, $t0, $t9
    mfc2    $t2, $17
    sw      $t4, 8($t0)
    beqz    $t2, .L\gt4\()_next
     slt    $t0, $t2, $t1
    \keep   $t0, .L\gt4\()_depth1
     mfc2   $t3, $18
    addu    $t1, $t2, $zero
.L\gt4\()_depth1:
    nop
    beqz    $t3, .L\gt4\()_next
     mfc2   $t4, $19
    slt     $t0, $t3, $t1
    \keep   $t0, .L\gt4\()_depth2
     and    $s3, $s3, $s6
    addu    $t1, $t3, $zero
.L\gt4\()_depth2:
    nop
    beqz    $t4, .L\gt4\()_next
     slt    $t2, $t4, $t1
    \keep   $t2, .L\gt4\()_depth3
     addiu  $s2, $s2, 1
    addu    $t1, $t4, $zero
.L\gt4\()_depth3:
    nop
    beqz    $t1, .L\gt4\()_next
     srav   $t0, $t1, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .L\gt4\()_next
     sw     $t1, 0($s3)
endlabel \gt4
.endm

.endif
