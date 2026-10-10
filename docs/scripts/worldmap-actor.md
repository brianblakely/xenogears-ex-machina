# World map actor scripts

- **Interpreter:** `worldmap_actor_script_run` (`decomp/src/worldmap/worldmap_open_map.c`), the
  update handler of an actor that world-map modes 17 and 18 register
  (`worldmap_scene17_start`, `worldmap_scene18_start`); its start handler sets the script.
- **Dispatch:** `worldmap_actor_script_handlers[12]`, handlers `worldmap_actor_script_exit_worldmap`-`worldmap_actor_script_set_fade`, each
  commented with its operands and effect. The opcode is not range-checked.
- **Format:** signed halfwords. The interpreter reads the 32-bit word at the script
  position (opcode in the low half, first argument in the high half) and passes the
  next two halfwords as the other arguments. A handler returns the halfwords to
  advance (2 or 4); 0 yields until the next frame. There are no jumps; opcode 0 ends
  the world-map loop and never advances.
- **Scripts:** `worldmap_scene17_actor_script` (217 words, `worldmap_scene17_script_start`) and `worldmap_scene18_actor_script` (51 words,
  `worldmap_scene18_script_start`), the only values stored to an actor's script pointer. They lie in
  `worldmap_scenes_16_18.c`'s data and stay user-supplied: the unit links them from the
  user's image (`INCLUDE_ASSET`; `asset` lines in
  `decomp/targets/overlays/worldmap.classification.txt`).
- **Coverage:** 12 opcodes defined, all 12 used. Each disc decodes 2 scripts, 172
  instructions, none undecodable.
- **Tool:** `python3 -m tools.analysis.overlay_scripts --sweep` decodes both discs'
  own packed `worldmap` containers and prints aggregates;
  `--list worldmap [--disc N]` prints the local listing.
