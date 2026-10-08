# Cosine of a 12-bit angle (4096 = one turn), 4096 = 1.0: the high halfword
# of pair a0 & 0xFFF in the sine/cosine table D_800523F0, sign extended.
# Clobbers a0, t0 and at. Handwritten, like func_8003F8B0.
glabel func_8003F8CC
    andi    $a0, $a0, 0xFFF
    sll     $t0, $a0, 2
    lui     $at, %hi(D_800523F0+2)
    addu    $at, $at, $t0
    lh      $v0, %lo(D_800523F0+2)($at)
    jr      $ra
     nop
endlabel func_8003F8CC
