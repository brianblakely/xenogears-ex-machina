# Sine of a 12-bit angle (4096 = one turn), 4096 = 1.0: the low halfword of
# pair a0 & 0xFFF in the sine/cosine table D_800523F0, sign extended.
# Clobbers a0, t0 and at. Handwritten, like its cosine twin func_8003F8CC:
# the scaled index goes to t0, which compiled code does not allocate here.
glabel func_8003F8B0
    andi    $a0, $a0, 0xFFF
    sll     $t0, $a0, 2
    lui     $at, %hi(D_800523F0)
    addu    $at, $at, $t0
    lh      $v0, %lo(D_800523F0)($at)
    jr      $ra
     nop
endlabel func_8003F8B0
