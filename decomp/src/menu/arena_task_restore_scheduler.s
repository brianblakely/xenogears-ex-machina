# Restore the two-word caller context saved by arena_task_save_scheduler.
# a0 stays intact; t0 and at are scratch registers in this interface.
# Handwritten: t0 carries both words; a plain-C probe under GCC 2.7.2 and
# 2.6.3 loads them into v0 and v1.
glabel arena_task_restore_scheduler
    lw      $t0, 0($a0)
    lui     $at, %hi(arena_task_caller_stack)
    sw      $t0, %lo(arena_task_caller_stack)($at)
    lw      $t0, 4($a0)
    lui     $at, %hi(arena_current_task)
    sw      $t0, %lo(arena_current_task)($at)
    jr      $ra
     nop
endlabel arena_task_restore_scheduler
