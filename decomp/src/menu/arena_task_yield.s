# Yield the current task: save its live register bank, including the
# return address after this yield, then return through the suspended
# caller's save area. zero and at have no slots written by the switch.
# Handwritten: stores the register file through at, including k0/k1/sp,
# and loads sp from memory, which compiled code never does.
glabel arena_task_yield
    lui     $at, %hi(arena_current_task)
    lw      $at, %lo(arena_current_task)($at)
    nop                         # R3000 load delay before using the task
    .irp reg,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
        sw  $\reg, 4*\reg($at)
    .endr
    lui     $sp, %hi(arena_task_caller_stack)
    lw      $sp, %lo(arena_task_caller_stack)($sp)
    sw      $ra, 4*31($at)       # also separates sp's load from its use

    lw      $s0, -4($sp)
    lw      $s1, -8($sp)
    .irp reg,18,19,20,21,22,23
        lw  $\reg, -4*(\reg-14)($sp)
    .endr
    lw      $ra, -60($sp)
    .irp reg,26,27,28
        lw  $\reg, -4*(\reg-16)($sp)
    .endr
    jr      $ra
     lw     $fp, -56($sp)
endlabel arena_task_yield
