# Sine of a 12-bit angle (4096 = one turn), 4096 = 1.0: the low halfword of
# pair a0 & 0xFFF in the sine/cosine table rcossin_tbl, sign extended.
# Clobbers a0, t0 and at. Handwritten, like its cosine twin gpu_get_cos:
# the scaled index goes to t0, where the unit's GCC compiles the plain C
# lookup with the index scaled in place in a0.
glabel gpu_get_sin
    andi    $a0, $a0, 0xFFF
    sll     $t0, $a0, 2
    lui     $at, %hi(rcossin_tbl)
    addu    $at, $at, $t0
    lh      $v0, %lo(rcossin_tbl)($at)
    jr      $ra
     nop
endlabel gpu_get_sin
