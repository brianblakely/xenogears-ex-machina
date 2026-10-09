.include "decomp/src/resident/model_draw.s"

# Average-depth model renderers that keep the packets' prepared colours:
# draw mode 0 of every triangle and quad type of D_8004FE50, and its other
# modes where a type has no lit or depth-cued variant. Also the exit that
# every model renderer shares. See model_draw.s for the face records,
# packet slots, pipelining and register use.
#
# Triangles: func_8002E010 (POLY_GT3), func_8002E024 (POLY_G3),
# func_8002E038 (POLY_F3) and func_8002E04C (POLY_FT3) select the packet
# format in t9/t8/a3. A face is kept when the error test after RTPT (which
# never fails, model_draw.s) and the screen bounds test pass and NCLIP is
# positive. Its three SXY words are written and s2 counts it; it is then
# linked at OT[OTZ >> D_80050100] with OTZ = AVSZ3, unless OTZ is 0.
# Quads: func_8002E22C (POLY_GT4), func_8002E240 (POLY_G4),
# func_8002E254 (POLY_F4) and func_8002E268 (POLY_FT4). RTPT projects the
# first three points and NCLIP tests their winding only; RTPS then
# projects the fourth, followed by the error test again. OTZ = AVSZ4. A quad
# that passes the bounds test is counted, and only then skipped when its
# OTZ is 0; otherwise its tag and four SXY words are written.
#
# a0 = records, a1 = count. Reads D_8005953C, D_80059424, D_80059568,
# D_80059578, D_800500F8/FC and D_80050100. Writes D_80059424 (advanced by
# count slots), D_80059578 (+ faces counted), the packets and the OT.
# Clobbers a0-a3, v0, v1, t0-t9, at and the GTE registers.
#
# Handwritten: no frame (s registers saved below sp), four entry points
# per body, other renderers branch into this function's exit, and the
# GTE work is pipelined across loop iterations.
glabel func_8002E010
    model_packet 12, 9          # POLY_GT3
    j       .Lavsz_triangle_setup
     nop
alabel func_8002E024
    model_packet 8, 6           # POLY_G3
    j       .Lavsz_triangle_setup
     nop
alabel func_8002E038
    model_packet 4, 4           # POLY_F3
    j       .Lavsz_triangle_setup
     nop
alabel func_8002E04C
    model_packet 8, 7           # POLY_FT3
.Lavsz_triangle_setup:
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)         # first face
    lhu     $t5, 4($a0)
    model_vertex0 $t0
    lwc2    $0, 0($t0)          # V0
    lwc2    $1, 4($t0)
    model_vertex1 $t0
    lwc2    $2, 0($t0)          # V1
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)          # V2
    lwc2    $5, 4($t0)
    subu    $s3, $s3, $a3       # one slot before the first face's
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.Lavsz_triangle:
    rtpt
    beqz    $a1, .Lmodel_draw_exit
     addiu  $a1, $a1, -1
    addiu   $a0, $a0, 8         # load the next face during RTPT
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addu    $s3, $s3, $a3       # slot of the face being drawn
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
    mfc2    $t1, $12            # SXY0
    bltz    $t0, .Lavsz_triangle  # error bit set
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13            # SXY1
    mfc2    $t3, $14            # SXY2
    nclip
    model_y_test 3, .Lavsz_triangle, .Lavsz_triangle_y
.Lavsz_triangle_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .Lavsz_triangle, .Lavsz_triangle_x
.Lavsz_triangle_x:
     mfc2   $t4, $24            # NCLIP result (MAC0)
    avsz3
    blez    $t4, .Lavsz_triangle  # back-facing or degenerate
     and    $s3, $s3, $s6
    sw      $t1, 8($s3)         # XY0..XY2, t9 bytes apart
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    addu    $t0, $t0, $t9
    sw      $t3, 8($t0)
    mfc2    $t5, $7             # OTZ
    addiu   $s2, $s2, 1
    beqz    $t5, .Lavsz_triangle  # counted, but not linked
     srav   $t5, $t5, $t7
    sll     $t5, $t5, 2
    addu    $t5, $t5, $s4
    lw      $t1, 0($t5)
    sw      $s3, 0($t5)
    or      $t1, $t1, $t8
    j       .Lavsz_triangle
     sw     $t1, 0($s3)

# Shared exit of the model renderers (8002E1F4); a3 = their packet size.
# Step past the last face's slot and store the slot pointer and counter.
.Lmodel_draw_exit:
    addu    $s3, $s3, $a3
    lui     $at, %hi(D_80059578)
    sw      $s2, %lo(D_80059578)($at)
    lui     $at, %hi(D_80059424)
    sw      $s3, %lo(D_80059424)($at)
    lw      $s0, -4($sp)
    lw      $s1, -8($sp)
    lw      $s2, -16($sp)
    lw      $s3, -20($sp)
    lw      $s4, -24($sp)
    lw      $s5, -28($sp)
    lw      $s6, -32($sp)
    jr      $ra
     lw     $s7, -36($sp)

# Quads.
alabel func_8002E22C
    model_packet 12, 12         # POLY_GT4
    j       .Lavsz_quad_setup
     nop
alabel func_8002E240
    model_packet 8, 8           # POLY_G4
    j       .Lavsz_quad_setup
     nop
alabel func_8002E254
    model_packet 4, 5           # POLY_F4
    j       .Lavsz_quad_setup
     nop
alabel func_8002E268
    model_packet 8, 9           # POLY_FT4
.Lavsz_quad_setup:
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    # V0 is loaded at the loop top, as the fourth point is loaded into V0.
    model_vertex0 $t6
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    subu    $s3, $s3, $a3
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.Lavsz_quad:
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
    nclip
    bltz    $t0, .Lavsz_quad
     and    $s3, $s3, $s6
    lhu     $t0, -2($a0)        # index 3, from the record before a0
    mfc2    $t1, $12
    mfc2    $t4, $24            # NCLIP result
    sll     $t0, $t0, 3
    blez    $t4, .Lavsz_quad
     addu   $t0, $t0, $s0
    mfc2    $t2, $13
    lwc2    $0, 0($t0)          # V0 = the fourth point
    lwc2    $1, 4($t0)
    mfc2    $t3, $14
    rtps
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t4, $14            # its SXY, pushed onto the FIFO
    bltz    $t0, .Lavsz_quad
     avsz4
    sltu    $t0, $t1, $v0
    model_y_test 4, .Lavsz_quad, .Lavsz_quad_y
.Lavsz_quad_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 4, .Lavsz_quad, .Lavsz_quad_x
.Lavsz_quad_x:
     mfc2   $t0, $7             # OTZ
    addiu   $s2, $s2, 1
    beqz    $t0, .Lavsz_quad    # counted, but neither linked nor written
     srav   $t0, $t0, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lw      $t5, 0($t0)
    sw      $s3, 0($t0)
    or      $t0, $t5, $t8
    sw      $t0, 0($s3)
    sw      $t1, 8($s3)
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    addu    $t0, $t0, $t9
    sw      $t3, 8($t0)
    addu    $t0, $t0, $t9
    j       .Lavsz_quad
     sw     $t4, 8($t0)
endlabel func_8002E010
