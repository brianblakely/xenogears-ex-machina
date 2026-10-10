# Cosine of a 12-bit angle (4096 = one turn), 4096 = 1.0: the high halfword
# of pair a0 & 0xFFF in the sine/cosine table rcossin_tbl, sign extended.
# Clobbers a0, t0 and at. Handwritten, like gpu_get_sin.
glabel gpu_get_cos
    andi    $a0, $a0, 0xFFF
    sll     $t0, $a0, 2
    lui     $at, %hi(rcossin_tbl+2)
    addu    $at, $at, $t0
    lh      $v0, %lo(rcossin_tbl+2)($at)
    jr      $ra
     nop
endlabel gpu_get_cos
