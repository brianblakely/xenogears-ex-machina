# Install the initial stack and small-data base, then return to ra:
# sp = fp = 0x80200000 (the top of the 2 MiB of main RAM), gp = _gp.
# The entry point falls through into this routine with ra = boot_main.
# The mode dispatcher mode_dispatch calls it to discard the whole stack
# before running the next mode, abandoning its own frame (it never returns).
# Handwritten: it rewrites sp/fp/gp, which compiled code cannot express.
glabel boot_reset_stack_and_gp
    lui     $sp, 0x8020
    addu    $fp, $sp, $zero
    lui     $gp, %hi(_gp)
    addiu   $gp, $gp, %lo(_gp)
    jr      $ra
     nop
endlabel boot_reset_stack_and_gp
