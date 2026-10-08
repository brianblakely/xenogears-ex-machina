.include "decomp/src/resident/model_draw.s"

# Draw flat triangles (POLY_F3) lit from the lit-colour cache: draw mode 1
# of primitive type 0. func_8002CDCC recorded twelve bytes per face at
# D_80059498 when the packets were built: the colour word, then the face
# normal (SVECTOR). This consumes one record per face, culled or not, and
# stores the advanced cache pointer at the end.
# Faces are projected and culled as in func_8002E010 (flag error bit,
# bounds test, NCLIP); the three SXY words are written before the NCLIP
# test. A front-facing face is counted, lit with NCCS (RGBC = the cached
# colour, V0 = the cached normal) and always linked at its AVSZ3 OTZ,
# without a zero test. Its colour word takes the lit RGB and the cached
# colour's code byte.
# a0 = records, a1 = count; a2 = cache pointer. V0 carries the normal, so
# the next face's V0 is loaded at the loop top from t6. Exits through
# func_8002E010's shared exit with a3 = 20, the packet size. Handwritten:
# no frame, cross-routine exit, pipelined GTE work.
glabel func_8002ED20
    lui     $t8, 0x0400         # POLY_F3 tag length
    model_draw_save
    model_draw_state $t7
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t6
    lui     $a2, %hi(D_80059498)
    lw      $a2, %lo(D_80059498)($a2)
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    addiu   $s3, $s3, -20

.Llit_f3:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    rtpt
    beqz    $a1, .Llit_f3_done
     addiu  $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addiu   $s3, $s3, 20
    model_vertex0 $t6
    addiu   $a2, $a2, 12        # this face's cache record ends here
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t1
    lwc2    $4, 0($t1)
    lwc2    $5, 4($t1)
    mfc2    $t0, $31            # FLAG
    mfc2    $t1, $12
    bltz    $t0, .Llit_f3
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .Llit_f3, .Llit_f3_y
.Llit_f3_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .Llit_f3, .Llit_f3_x
.Llit_f3_x:
     mfc2   $t0, $24            # NCLIP result
    sw      $t1, 8($s3)
    avsz3
    sw      $t2, 12($s3)
    blez    $t0, .Llit_f3
     sw     $t3, 16($s3)
    and     $s3, $s3, $s6
    mfc2    $t0, $7             # OTZ
    addiu   $s2, $s2, 1
    lw      $t1, -12($a2)       # cached colour
    lwc2    $0, -8($a2)         # cached normal
    lwc2    $1, -4($a2)
    mtc2    $t1, $6             # RGBC
    nccs
    srav    $t0, $t0, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    mfc2    $t2, $22            # lit RGB2
    lui     $at, 0xFF00         # the cached colour's code byte
    and     $t1, $t1, $at
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    sw      $t1, 4($s3)
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .Llit_f3
     sw     $t1, 0($s3)

.Llit_f3_done:
    lui     $at, %hi(D_80059498)
    sw      $a2, %lo(D_80059498)($at)
    ori     $a3, $zero, 20
    j       .Lmodel_draw_exit
     nop
endlabel func_8002ED20
