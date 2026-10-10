# Build the rotation matrix M = Rx * Ry * Rz (Z is applied first) from the
# three 12-bit angles at a0 (SVECTOR, word aligned: vx/vy are read as one
# word) into the MATRIX at a1, and return a1. Each angle indexes the
# sine/cosine pair table rcossin_tbl (low halfword sine, high halfword
# cosine, 4096 = 1.0) after masking with 0xFFF. With s/c the sine/cosine of
# x, y, z, every product is a 32-bit multiply shifted right by 12:
#   a = (cz * -sy) >> 12               b = (sz * -sy) >> 12
#   m[0][0] = (cz*cy) >> 12            m[0][1] = -(sz*cy) >> 12
#   m[0][2] = sy
#   m[1][0] = (sz*cx >> 12) - (a*sx >> 12)
#   m[1][1] = (cz*cx >> 12) + (b*sx >> 12)
#   m[1][2] = -(cy*sx) >> 12
#   m[2][0] = (a*cx >> 12) + (sz*sx >> 12)
#   m[2][1] = (cz*sx >> 12) - (b*cx >> 12)
#   m[2][2] = (cy*cx) >> 12
# The negations precede their shifts. Only the nine rotation halfwords are
# stored; the matrix padding and translation are untouched.
# Leaf code in t registers: clobbers t0..t9, at, HI and LO.
# Handwritten: trapping neg/add/sub. The nops keep each mflo two
# instructions ahead of the next mult (the R3000 HI/LO hazard).
glabel gpu_build_rotation_matrix
    lw      $t1, 0($a0)         # vx | vy << 16
    lh      $t3, 4($a0)         # vz
    srl     $t2, $t1, 16
    andi    $t1, $t1, 0xFFF     # x index
    andi    $t2, $t2, 0xFFF     # y index
    sll     $t0, $t2, 2
    lui     $at, %hi(rcossin_tbl)
    addu    $at, $at, $t0
    lw      $t5, %lo(rcossin_tbl)($at) # y pair
    andi    $t3, $t3, 0xFFF     # z index
    sll     $t8, $t5, 16
    sll     $t0, $t3, 2
    lui     $at, %hi(rcossin_tbl)
    addu    $at, $at, $t0
    lw      $t6, %lo(rcossin_tbl)($at) # z pair
    sra     $t5, $t5, 16        # cy
    sll     $t9, $t6, 16
    sra     $t9, $t9, 16        # sz
    sra     $t6, $t6, 16        # cz
    mult    $t6, $t5
    sll     $t2, $t1, 2
    lui     $at, %hi(rcossin_tbl)
    addu    $at, $at, $t2
    lw      $t4, %lo(rcossin_tbl)($at) # x pair
    mflo    $t0                 # cz*cy
    sll     $t7, $t4, 16
    sra     $t4, $t4, 16        # cx
    mult    $t9, $t4
    sra     $t8, $t8, 16        # sy
    neg     $t8, $t8            # -sy
    mflo    $t1                 # sz*cx
    sra     $t0, $t0, 12
    sh      $t0, 0($a1)         # m[0][0]
    mult    $t6, $t8
    sra     $t7, $t7, 16        # sx
    mflo    $t0
    sra     $t1, $t1, 12
    sra     $t0, $t0, 12        # a
    mult    $t0, $t7
    mflo    $t2                 # a*sx
    nop
    nop
    mult    $t9, $t7
    sra     $t2, $t2, 12
    mflo    $t3                 # sz*sx
    sub     $t1, $t1, $t2
    sh      $t1, 6($a1)         # m[1][0]
    mult    $t0, $t4
    mflo    $t2                 # a*cx
    sra     $t3, $t3, 12
    nop
    mult    $t9, $t5
    sra     $t2, $t2, 12
    mflo    $t0                 # sz*cy
    add     $t2, $t2, $t3
    sh      $t2, 12($a1)        # m[2][0]
    mult    $t6, $t4
    neg     $t0, $t0
    mflo    $t2                 # cz*cx
    sra     $t0, $t0, 12
    sh      $t0, 2($a1)         # m[0][1]
    mult    $t9, $t8
    mflo    $t0
    sra     $t2, $t2, 12
    sra     $t0, $t0, 12        # b
    mult    $t0, $t7
    mflo    $t3                 # b*sx
    nop
    nop
    mult    $t6, $t7
    sra     $t3, $t3, 12
    mflo    $t1                 # cz*sx
    add     $t2, $t2, $t3
    sh      $t2, 8($a1)         # m[1][1]
    mult    $t0, $t4
    mflo    $t2                 # b*cx
    sra     $t1, $t1, 12
    nop
    mult    $t5, $t7
    sra     $t2, $t2, 12
    sub     $t1, $t1, $t2
    sh      $t1, 14($a1)        # m[2][1]
    mflo    $t0                 # cy*sx
    neg     $t3, $t8            # sy
    sh      $t3, 4($a1)         # m[0][2]
    mult    $t5, $t4
    neg     $t0, $t0
    mflo    $t1                 # cy*cx
    sra     $t0, $t0, 12
    sh      $t0, 10($a1)        # m[1][2]
    sra     $t1, $t1, 12
    sh      $t1, 16($a1)        # m[2][2]
    jr      $ra
     addu   $v0, $a1, $zero
endlabel gpu_build_rotation_matrix
