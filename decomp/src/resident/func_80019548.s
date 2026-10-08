# Install the initial stack and small-data base, then return to ra:
# sp = fp = 0x80200000 (the top of the 2 MiB of main RAM), gp = _gp.
# The entry point falls through into this routine with ra = func_80019578.
# The mode dispatcher func_80019ACC calls it to discard the whole stack
# before running the next mode, abandoning its own frame (it never returns).
# Handwritten: it rewrites sp/fp/gp, which compiled code cannot express.
glabel func_80019548
    lui     $sp, 0x8020
    addu    $fp, $sp, $zero
    lui     $gp, %hi(_gp)
    addiu   $gp, $gp, %lo(_gp)
    jr      $ra
     nop
endlabel func_80019548
