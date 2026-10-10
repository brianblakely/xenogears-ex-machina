# Store a0 in the resident word mode_arena_task, by which the menu overlay picks
# its task (the boot routine and the field set 0). Clobbers t0; no frame.
# Handwritten: the full address is formed in t0 and the store goes through
# it, where compiled code folds %lo into the store's offset.
glabel mode_set_arena_task
    lui     $t0, %hi(mode_arena_task)
    addiu   $t0, $t0, %lo(mode_arena_task)
    sw      $a0, 0($t0)
    jr      $ra
     nop
endlabel mode_set_arena_task
