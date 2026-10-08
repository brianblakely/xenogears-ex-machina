# Store a0 in the resident word D_80050618, by which the menu overlay picks
# its task (the boot routine and the field set 0). Clobbers t0; no frame.
# Handwritten: the full address is formed in t0 and the store goes through
# it, where compiled code folds %lo into the store's offset.
glabel func_800379B4
    lui     $t0, %hi(D_80050618)
    addiu   $t0, $t0, %lo(D_80050618)
    sw      $a0, 0($t0)
    jr      $ra
     nop
endlabel func_800379B4
