# Arena scene scripts

- **Interpreter:** `arena_scene_run_script` (`decomp/src/menu/menu2.c`) in the `menu` overlay
  (Disc 1 file 35, Disc 2 file 30), run each frame by `arena_scene_update` (scenes) and
  `arena_scene_update_bout_end` (bout end).
- **Dispatch:** a `switch` on the command byte, each case commented with its operands
  and effect. Cases 1-34 take one to three bytes (unsigned byte operands). 0 and the
  bytes without a case (35-255) return without advancing; 16 and 17 never advance or
  return. There are no jumps.
- **Scripts:** `arena_scene_start_script` starts `arena_scene_scripts[0..9]` by scene
  (`arena_scene_enter`), the opening `arena_scene_opening_script` (`arena_scene_start_tutorial`) and the setup script
  `arena_scene_bout_end_script` (`arena_scene_start_bout_end`). `arena_scene_unreferenced_script` has the same form, but nothing
  starts it. All of them lie in `menu2.c`'s data and stay user-supplied: the unit
  links them from the user's image (`INCLUDE_ASSET`; `asset` lines in
  `decomp/targets/overlays/menu.classification.txt`), and only the pointer table
  `arena_scene_scripts` is C.
- **Coverage:** 35 cases defined (0-34), 31 used; 4, 9, 16 and 17 are unused. Each
  disc decodes 12 scripts, 217 instructions, none undecodable.
- **Tool:** `python3 -m tools.analysis.overlay_scripts --sweep` decodes both discs'
  own packed `menu` containers and prints aggregates;
  `--list arena [--disc N]` prints the local listing.
