# Arena scene scripts

- **Interpreter:** `func_8007107C` (`decomp/src/menu/menu2.c`) in the `menu` overlay
  (Disc 1 file 35, Disc 2 file 30), run each frame by `func_80071AD0` (scenes) and
  `func_80072170` (bout end).
- **Dispatch:** a `switch` on the command byte, each case commented with its operands
  and effect. Cases 1-34 take one to three bytes (unsigned byte operands). 0 and the
  bytes without a case (35-255) return without advancing; 16 and 17 never advance or
  return. There are no jumps.
- **Scripts:** `func_80070F80` starts `D_8009105C[0..9]` by scene
  (`func_8007191C`), the opening `D_80090F38` (`func_800719F0`) and the setup script
  `D_800910C4` (`func_800720D4`). `D_80091050` has the same form, but nothing
  starts it.
- **Coverage:** 35 cases defined (0-34), 31 used; 4, 9, 16 and 17 are unused. Each
  disc decodes 12 scripts, 217 instructions, none undecodable.
- **Tool:** `python3 -m tools.analysis.overlay_scripts --sweep` decodes both discs'
  own packed `menu` containers and prints aggregates;
  `--list arena [--disc N]` prints the local listing.
