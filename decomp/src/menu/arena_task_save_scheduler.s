# The scheduler's two words, which only these routines address: the
# suspended caller's sp while a task runs (arena_task_caller_stack) and the running
# TaskContext * (arena_current_task). Handwritten storage in an explicit .bss, not a
# compiler .lcomm, so the menu assembler's small-data rule (menu.mk)
# left these 8 bytes in .bss, where they open menu7's larger variables
# ahead of its compiled ones.
    .section .bss
dlabel arena_task_caller_stack  # 80096D88
    .space 4
enddlabel arena_task_caller_stack
dlabel arena_current_task  # 80096D8C
    .space 4
enddlabel arena_current_task
    .text

# Save the scheduler's current caller stack and task so a nested scheduler
# can restore them. a0 points to two words: caller sp, then TaskContext *.
# This is authored assembly for the original handwritten context interface.
# Handwritten: t0 and t1 hold the two words; a plain-C probe under GCC 2.7.2
# and 2.6.3 reuses v0 for both.
glabel arena_task_save_scheduler
    lui     $t0, %hi(arena_task_caller_stack)
    lw      $t0, %lo(arena_task_caller_stack)($t0)
    lui     $t1, %hi(arena_current_task)
    lw      $t1, %lo(arena_current_task)($t1)
    sw      $t0, 0($a0)
    jr      $ra
     sw     $t1, 4($a0)
endlabel arena_task_save_scheduler
