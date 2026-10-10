# Battle event-script VM (ovl3087)

A battle's event script runs when its formation has flag 0x20, which sets
800C3D48 ([formations.md](formations.md)). The battle overlay loads ovl3087
(80070E2C) and calls `battle_event_script_run` through 80070EB0, at the battle's start and
between turns.

- Interpreter: ovl3087 `battle_event_script_run`
  (`decomp/src/ovl3087/battle_event_script_vm.c`, matching):
  `switch (code[pc])`, 76 cases 00-4B. Each case calls one handler that returns
  the instruction length, or 0 while it waits. Operands come from 801E57F8, from
  byte 1 on. In the masked form, a bit of byte 5 marks an operand as an
  immediate; in the signed form, bit 15 does. Any other operand is a variable
  offset. There is no default case: opcodes 4C-FF reapply the previous length.
- Execution: there are up to 16 threads, each with eight priority levels and
  an eight-entry table (0 start, 1 idle, others requested by 03-05). A pass
  gives each thread one battle frame (800716D8), then up to four instructions.
  Passes repeat until opcode 22 makes one the last. Jump targets are absolute
  bytecode offsets.
- Effect scripts: opcodes 23 and 38 start an object's effect script (800AA384,
  800AA320). With 23, the effect's 02/03 commands report back through 80080C6C
  (thread byte 0x34).
- Per-opcode semantics are in the module's `OPCODES` table (each entry names
  its handler) and in the handler comments.

Data: directory 20/0 (ovl3087 is file 1). File 2 is the script archive: a
relocatable table of packed blocks (80032E88), with a script and its message
data per set. The formation's byte 3 (8006F9DF, `scriptSet`) picks the set; opcode
24 names the next battle's formation of the same encounter set. File 3 is the
model archive (opcode 35); file 4 is the sound bank. A script holds 0x40
bytes, the thread count (u32), 16 bytes of entries per thread, then the
bytecode.

`python3 -m tools.analysis.battle_event_vm --sweep` decodes every set on both
discs, which hold identical archives: 96 sets (48 per disc), 468 threads and
9200 instructions. 50 of 76 opcodes are used; 0 are unknown or undecodable,
and no request (03-05) names an entry outside the thread tables. No thread
reaches 18 runs, which decode as 26 instructions, kept out of the use counts.
70 sets end in 112 nonzero bytes, at most 7 per set, after their last
instruction. No unreached byte fails to decode.
`--list FILE [--set N]` prints the disassembly. Keep listings under `.local/`.
`tests/test_battle_event_vm.py` checks the table against 801E879C's cases and
the lengths that its handlers return.

Open: 26 opcodes (04 07-0C 0F-17 19 2C-2E 30 32-34 3D 47) have no use in the
data. They are decoded from their handlers only.
