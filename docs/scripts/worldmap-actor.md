# World map actor scripts

- **Interpreter:** `func_80076B34` (`decomp/src/worldmap/worldmap_80072238.c`), the
  update handler of an actor that world-map modes 17 and 18 register
  (`func_80082324`, `func_8008355C`); its start handler sets the script.
- **Dispatch:** `D_8009A3C0[12]`, handlers `func_80076BC4`-`func_80076D8C`, each
  commented with its operands and effect. The opcode is not range-checked.
- **Format:** signed halfwords. The interpreter reads the 32-bit word at the script
  position (opcode in the low half, first argument in the high half) and passes the
  next two halfwords as the other arguments. A handler returns the halfwords to
  advance (2 or 4); 0 yields until the next frame. There are no jumps; opcode 0 ends
  the world-map loop and never advances.
- **Scripts:** `D_8009A758` (217 words, `func_800827C8`) and `D_8009AC60` (51 words,
  `func_800838E8`), the only values stored to an actor's script pointer. They lie in
  `worldmap_800811C0.c`'s data and stay user-supplied: the unit links them from the
  user's image (`INCLUDE_ASSET`; `asset` lines in
  `decomp/targets/overlays/worldmap.classification.txt`).
- **Coverage:** 12 opcodes defined, all 12 used. Each disc decodes 2 scripts, 172
  instructions, none undecodable.
- **Tool:** `python3 -m tools.analysis.overlay_scripts --sweep` decodes both discs'
  own packed `worldmap` containers and prints aggregates;
  `--list worldmap [--disc N]` prints the local listing.
