# Resume Task *a0 until it yields through func_8008BC04. The caller's
# preserved registers live below its unchanged sp while the task runs on
# its own stack. The save layout skips sp-12 and does not save t8/t9.
# k0/k1 are preserved too, as required by this original context interface.
glabel func_8008BB3C
    sw      $s0, -4($sp)
    sw      $s1, -8($sp)
    # s2..s7, followed by k0/k1/gp/sp/fp/ra in the caller save area.
    .irp reg,18,19,20,21,22,23
        sw  $\reg, -4*(\reg-14)($sp)
    .endr
    .irp reg,26,27,28,29,30,31
        sw  $\reg, -4*(\reg-16)($sp)
    .endr

    lui     $at, %hi(D_80096D88)
    sw      $sp, %lo(D_80096D88)($at)
    lui     $at, %hi(D_80096D8C)
    sw      $a0, %lo(D_80096D8C)($at)
    addu    $at, $a0, $zero

    # Task.regs is indexed by hardware register number. Keep at pointing
    # at the task while loading v0..gp, including the argument registers.
    .irp reg,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28
        lw  $\reg, 4*\reg($at)
    .endr
    lw      $ra, 4*31($at)
    lw      $sp, 4*29($at)
    jr      $ra
     lw     $fp, 4*30($at)
endlabel func_8008BB3C
