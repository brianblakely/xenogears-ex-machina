.include "decomp/src/resident/model_draw.s"

# Draw depth-cued flat textured quads (POLY_FT4) sorted by AVSZ4: draw
# mode 4 of primitive types 9 and 13. RGBC holds the model colour
# D_80059598. Quads are projected and culled as in func_8002E010; the
# four SXY words are written and the quad counted once it passes the
# bounds test. Unless its OTZ is 0, its colour word becomes DPCS of the
# model colour (by the IR0 of the fourth point) with the packet's code
# less bit 0, and it is linked.
# a0 = records, a1 = count; takes func_8002E010's shared exit directly.
# Handwritten: no frame, cross-routine exit, pipelined GTE work.
glabel func_8002FCFC
    model_packet 8, 9           # POLY_FT4
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t6
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    lui     $t0, %hi(D_80059598)
    lw      $t0, %lo(D_80059598)($t0)
    subu    $s3, $s3, $a3
    mtc2    $t0, $6             # RGBC
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.Ldepth_cue_ft4:
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
    mfc2    $t0, $31            # RTPT flags
    mfc2    $t1, $12
    bltz    $t0, .Ldepth_cue_ft4
     mfc2   $t2, $13
    mfc2    $t3, $14
    nclip
    lhu     $t5, -2($a0)        # index 3
    and     $s3, $s3, $s6
    mfc2    $t4, $24            # NCLIP result
    sll     $t0, $t5, 3
    blez    $t4, .Ldepth_cue_ft4
     addu   $t0, $t0, $s0
    lwc2    $0, 0($t0)          # the fourth point
    lwc2    $1, 4($t0)
    rtps
    mfc2    $t0, $31
    mfc2    $t4, $14
    bltz    $t0, .Ldepth_cue_ft4
     avsz4
    sltu    $t0, $t1, $v0
    model_y_test 4, .Ldepth_cue_ft4, .Ldepth_cue_ft4_y
.Ldepth_cue_ft4_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 4, .Ldepth_cue_ft4, .Ldepth_cue_ft4_x
.Ldepth_cue_ft4_x:
     sw     $t1, 8($s3)
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    addu    $t0, $t0, $t9
    sw      $t3, 8($t0)
    addu    $t0, $t0, $t9
    sw      $t4, 8($t0)
    mfc2    $t0, $7             # OTZ
    addiu   $s2, $s2, 1
    beqz    $t0, .Ldepth_cue_ft4
     dpcs
    srav    $t0, $t0, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lbu     $t1, 7($s3)         # the packet's code
    mfc2    $t2, $22            # depth-cued RGB2
    sll     $t1, $t1, 24
    lui     $at, 0xFE00         # code without bit 0 (raw texture)
    and     $t1, $t1, $at
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    sw      $t1, 4($s3)
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .Ldepth_cue_ft4
     sw     $t1, 0($s3)
endlabel func_8002FCFC
