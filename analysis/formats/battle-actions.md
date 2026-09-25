# Battle actions, knockouts, victory and rewards

`include/xem/reconstruction/battle.hpp` and `src/reconstruction/battle*.cpp`
reconstruct the battle-overlay path that the frozen field 23 encounter
exercises: party attacks from command commit to damage, knockout, victory and
the victory rewards written to persistent party state.
The source is project-authored from the qualified battle overlay (sha256
`1830b4ef1fe37129972fc310dfad534f8161d6c0b123e74254c3711334a3e291`, loaded at
`8006faf0`) and the resident executable. No external Xenogears implementation or
reverse-engineering source was consulted.

## Memory model

Battle mode owns the overlay image and its static data (`8006faf0..800d39f0`, the
range in resident mode-table row `800180ac`) plus the allocated heap blocks it
reaches: the three battle setup allocates (pointers `800c3ea4`, `800d2d28`,
`800c3eac`), the growth-table file (pointer `800d2c08`) and the block holding the
post-battle module at `801de000`, and the enemy data file (pointer `800c3dd0`,
set up by aux4 `801e4958`) that holds the enemy AI scripts. The resident game data (`8006d634`, the
`2358`-byte block resident `8001b9d8` initializes for a new game) moves into
battle memory for each call. `BattleMemory` holds these regions whole and the code reads and writes them by original
address, exactly as the original does. That includes the places where the
original addresses records absolutely from `800ccce8` rather than through the
record base pointer `800c34b0`. The resident `rand` state `8005a1fc` stays
resident-owned. Reads outside the owned regions fail explicitly.

Combatant records are 11 slots of `0x170` bytes (0-2 party, 3-10 enemies). The
per-action arrays follow the records: damage `+5f6c` (u32 per slot), result code
`+5fa0` (u8, `ff` untouched), target mask `+5fac`, attacker and command
`+5fc1/+5fc2`.

## Reconstructed functions

| Original | Source | Behavior |
| --- | --- | --- |
| `80085ccc` | `commit_action` | Record attacker, targets, animation, alive mask and command; resolve |
| `800941a4` | `resolve` | Attacker and descriptor selection, per-target formula dispatch through table `800c348c`, post-adjust, status effects, shown command, descriptor publication, usage counters |
| `80094ee4` | `physical_formula` | Formula type 0: immunity, hit outcome, attack/defense with status and element modifiers, `k1*A - k2*D`, power/20, variance, outcome table `80070370`, 9999 cap |
| `80095d4c` | `formula_type3` | Formula type 3: chance from attacker `+60` or descriptor `+1c` against a rand percent; amount from attacker or target `+4e`/`+52` times power/20 (kind 5: target HP - 1); the same amount written for attacker and target with paired result codes (2/0 or 3/1) |
| `80096ab8`, `80096fbc`, `8009b46c`, `80097610`, `80096494` | `battle_formula.cpp` | Hit/evade outcome, attack, wounded-party bonus, defense, element adjustment |
| `800946f4`, `800968c0`, `80096824`, `800958d8`, `80095a78`, `80095b44` (+ `80097964`, `800995a0`, `80099498`, `8009b684`) | `battle_steps.cpp` | Post-adjust, counterattack, ether check and status effects |
| `80097d08`, `8009ac48`, `8009ab38`, `80094c78`, `80099fb0` | `battle.cpp` | Result clear, command-table flags, party totals, descriptor publication |
| `800799c8` (+ `8007ef6c`, `8007f8c0`, `80079948`, `8001bd40`, `8007a628`) | `run_enemy_script` (`battle_ai.cpp`) | Enemy AI script: clear the action list `800d2e5c` and event types, run rules (conditions `80..9b` with the `99` OR chain, then actions `01..74`) from the enemy's script pointer (`800d3400 + enemy*40`) until `fd`/`ff`. Actions `01`, `0c`, `3e`, `41`, `50`, `52`, `62` (no handler) and `64` and conditions `82`, `83` are translated; the others fail naming their handler |
| `80085618` (+ `800883ac`, `80089c08`) | `apply_results` | Apply queued results (jump table `80070250`) to HP, EP, gear HP and gauge; knockouts and formation-group bookkeeping |
| `8007252c` (+ `80089c48`, `80089c9c`) | `update_alive` | Alive mask `800d39dc`, victory (`800c48ea = 1`) and defeat (`81`) |
| `8007171c`, `800718bc` (+ `80098af8`, `80094d24`) | `atb_tick`, `reload_turn_timer` | Per-frame ATB timers, haste/slow/stop statuses and readiness; turn-timer reload from speed with rand spread |
| `801e2280` to `801e23d4` | `total_rewards` | Experience pool `800d2c84`, defeated mask `800d2c9c` and gold `8006ef58` (cap 9999999) from counted knocked-out enemies |
| `801e2794` (+ `801e3a18`, `801e403c`, `801e41b4`, `801e2888`, `801e42c4` and helpers) | `grant_rewards` | Skills, tier and level flags, write-back of HP/EP/use counters and gear to persistent records, drop rolls |
| `801e2acc` (+ `801e2eb0`, `801e308c`, `801e335c`, `801e3500`, `801e3610`, `801e3700`, `801e38cc`) | `distribute_experience` | Experience split and weighted pools, two level tracks with growth-table requirements, rand stat growth |
| `801e1444` (+ `801e1370`) | `add_drops` | Add up to 8 drops to the five inventory lists (jump table `801de000`), stacking to 99 |
| `800723e0` to its call of `80071b94`, plus `80071b94` to `80071bd8`/`80072254` | `select_turn` (`battle_turn.cpp`) | Turn scheduler: the forced slot `800d2dc0`, else the first slot at or after the cursor `800d2dd7` in the turn order `800d2dd8` whose ready byte `800d2de4` is exactly 1 (turn state `+2d3` = slot + 1, cursor passes it); nobody ready leaves `+2d3` 0 |
| `80071bd8` to `80071c74` (+ `80085350`, `80071a08`, `80099890`) | `begin_turn`, `count_down_statuses` | Turn start: ATB hold (`800d3298 = 0`), `+2d3` becomes the slot, event and result counters reset, timed status counters (`+15d..+169`) count down and clear their bits at zero |
| `80071c74` to `80071c80`/`80072090`/`800720a0`/`800720f8` | `prepare_turn` | Party turn: reaction flags `800c3d1a` cleared, acting member's marker vertices written to the draw buffer `800ccb34` of the `800c3ea4` block |
| `80079778` to its first frame (+ `80085388`) | `begin_actions` | Event queue reset for the actor |
| `800793f0` (+ `80079674`, `80078998`, `80078b34`, `800877e0`, `80087edc`, `80085c88`, `80085454`, `800785d4`, `800787e0`) | `execute_actions`, `resume_actions` | Action list `800d2e5c` (32 entries of 8 bytes; after entry 31 until a type 0): type 1 commits (`80085ccc`), accumulates (`80085454`) and applies results and queues the animation event; type 2 queues an approach event `fd`, plans the route `800c48ec` from the formation data `*800d3364` and moves the actor into the target's formation group; type `e` queues event `f7`; the queue closes with `fe` |
| `80072160` to `800721dc` (+ `80072270`, `800841e0`, `80083ff4`, `80085310`) | `order_targets` | Held party timers (`800d2c9e`); each slot's default target (`+3c + 40*slot`): the lowest-HP valid target, from the same formation group when it has one |
| `800721ec` to `80072230` | `settle_turn` | Alive updates around the end-of-turn regeneration check (`8009ada0`) |
| `80072240` to `80072254` | `finish_turn` | Turn-timer reload, ATB enabled again |
| `80089ccc` | `decode_input` (`battle_menu.cpp`) | Menu input: dequeue resident pad entries (`80035cdc`); `800594a4` bits `2000/4000/8000/1000` give codes 0-3 (remembered in `800c3e29`/`800c3e28`), `8005948c` bits `20/40/80/10` give 4-7, `100` d, `800` e; 8 when nothing, ff once the battle ends; menu effects `4c/4d/4e` through `80039db8` |
| `80080160` dispatch (table `8006fe7c`), `80081504`, `8008115c`, `80082504` (+ `8009aa44`, `8009a9d0`, `8009be0c`) | `menu_step`, `defend`, `try_escape`, `write_back_party` | Command pages 1, 3 and 9 with the decoded code: face codes move between pages (items with a nonzero availability halfword beep `4f`; some need a second press of the same button), confirm on page 3 defends, on page 9 tries to escape |
| `800811b8` (+ `80087a38`, `80085d34`, `800879a8`, `800877e0`, `80084a7c`, `800841e0`, `80077698`) | `enter_attack_page`, `resume_attack_entry` (`battle_attack.cpp`) | Page 1's attack item: AP sprites, event reset, route, then (after the attack model `800b89fc`) the attack target (the default target when it is a candidate, else the first candidate), the four direction-arrow `POLY_G3` pairs, page 5 |
| `80081b58` (+ `8008189c`, `80084854` over `ratan2` `8004b32c`, `80085eb4`, `80080b64`, `8009413c`) | `attack_page`, `resume_attack_view`, `direction_target` | Page 5: arrow flags from the nearest candidate in each screen direction, codes 0-3 retarget, 4/6/7 cost 3/1/2 AP (blocked items beep), too little AP beeps, 5 cancels or closes; page `64` (`80080838`) sets `+2e2 = ff` and stops while `+2e1` is set |
| `800819a4`, `800861d0` (+ `8008ac00`), `80087af0` (+ `80087edc`, `80085388`, `80085ccc`, `80079840`, `80079ab0`, `80085c88`) | `resume_combo`, `confirm_attack`, `resume_attack_confirm`, `run_reaction_script` | Confirmation: the target fixed and approach event `fd`, the combo history `+2cc` and its pattern (`800c3160`) with the known-deathblow tests, three text blocks (tag 2 via `80032498`), the combo step (`800c34b3`), commit, the attacked enemy's memory (`800d3400 + e*40`), its reaction script, result application, the turn timer from table `800c31d4`, and the close (`fe`) when no AP remain |
| `8002675c` via `80076a10` | `draw_glyph` | A glyph-table sprite (`800d2f5c`) as `POLY_FT4` parts (`SetPolyFT4`, `GetTPage`, `GetClut`) in the draw buffer `800ccb34` |
| `801e1fb8` screens between frames (+ `8008fa60`) | `result_screen_step` (`battle_results.cpp`) | The summary, experience, level-up and gold/items screens' Cross waits and flags (UI `+a0`, `+a1`, `+ac`, `+b0..b2`, `+cf`), and window closes releasing each window's two blocks (`800d2e38`, `800d2d90`) |
| resident `8001b758` to `8001b82c` (+ `8001ac94`, `8001996c`) | `Program::finish_battle_mode` | After the battle: outcomes 1, `40`, `21` select mode 6 (`800d3338`), 2 (`8005947c`) or 1/3 by the map selector `8006f94e`; defeat (`81`) clears `8004f30c` and selects mode 1 with selector `1ea` and `8006f950..954` cleared; `800594f8 = 1` unless `8005947c` |

Explicit failures remain for paths the original leaves undefined (uninitialized
stack reads in `80096fbc`/`80097610`) and for code not yet reconstructed: the
gear resolver `8009c198`, formula types 1, 2 and 4-7, formula type 3 with descriptor `+18` of 2 or more (reads the caller's `s0`), and character 4's item bookkeeping
`8009afd8`. Unguarded divides reproduce the R3000A divide-by-zero results.

## Validation and limits

A route capture hooks every action commit, result application and alive-mask
update of the encounter. From each original entry image the C++ reproduces the
exact exit image and rand state: all four party attacks (15 damage each), all
four knockouts, all six alive-mask updates including the victory, all 2,080
per-frame ATB ticks and 4 turn-timer reloads, the
victory's reward totals and its reward call (experience split, one level-up with
rand stat growth, write-back to all 11 persistent character records). See
[EVID-REF-043](../findings/EVID-REF-043.json). The route's enemies carry no gold
and drop nothing, so gold addition and drop addition have source and synthetic
coverage only. An alternative input where the party defends lets the enemies
act: every enemy AI script run and every action commit, including formula type 3,
matches exactly ([EVID-REF-046](../findings/EVID-REF-046.json)). That encounter
uses a single enemy script. A third input chooses Escape with both party
members: every hooked call of that route matches, from the battle's ATB ticks,
alive update and teardown heap releases (one frees the block holding the sound
driver's effect bank, which the driver keeps only as a stale address) through the
field return. The escape route calls no action commit, AI run or result application.

## Turn procedure and command menu

`80070f40` runs the battle: every frame it calls `800723e0` (while `800ccc58`
is set) and the frame routine `800716d8`, whose logic tick `8008a274` runs the
ATB tick and the input decode `80089ccc`. `800723e0` selects the actor and calls
the turn procedure `80071b94`, which spans many frames (menu, animation, text,
camera). The reconstruction stops at its presentation calls and compares each
decision step between them: selection, turn start, party preparation, the
enemy script (`800799c8`), the event-queue reset, the action executor, target
ordering, the end-of-turn alive updates and the timer reload. Presentation
stays outside every compared step: the enemy name text (`80033728`, `80034eac`,
`800769e8`), frame waits (`80071a8c`, `80071a38`, `80071964`, `80071ae0`), the
animation wait on `+2db`, the camera (`800ba4e0`, `800bfe48`, `800bcd98`) and,
inside the executor, the camera framing `800bc404`: the executor step ends at
that call (`80078c64`) and a resumed step starts at its return (`80078c6c`),
continuing the same entry with the executor's registers; its comparison
excludes the 40-byte frame of `80078b34` that the entry lies inside
(`--frame-above 40`). The party menu `80080160` is compared per frame at its
page dispatch (`800807c8` to `80080930`) and per decode call.

The attack pages keep presentation out in two ways. The camera `800bc404`
and the target cursor camera `800bcd98` run inside a step and are bracketed
like interrupt code (`memory_case --presentation`): bytes only they change are
theirs, and every segment of original code between them must leave the GTE
rotation and translation unchanged, while the C++ keeps the entry values.
Calls whose effects later code depends on end the step and a resume entry
continues after them: the attack model `800b89fc` (it spans frames and runs
the logic tick; resume at `80087ac0`), the target text `80093b08` (it
allocates and releases heap blocks; resume at its return `80094134`), the
combo's camera `800bcd98(0)` in `800819a4` (it releases heap blocks; resume at
`800819e4`) and the frame inside `800861d0` (resume at `80086b34`, a loop head
whose repeats belong to the step: `--entry-repeats`). Glyph sprites are built
in C++ (their part count sets UI `+7b` and `+5d81`). The route selects every
attack with Cross, so the direction codes 0-3 run only inside the per-frame
arrow flags; a known deathblow's name (`80086028`), the model reset of a
cancel (`800b8da4`) and a reaction that queues actions fail explicitly. The escape decision is one rand draw: `rand() % 100 < 50` succeeds,
writes the party back to persistent data (`8009be0c`) and sets the outcome to
`40`; a failure only ends the member's menu. Defense sets the record's `+15a`
bit 1 and command index 0.

On the enemy-turns input every step matches its original exit image: 70
turn starts, party preparations (with the branch taken), target orderings,
settles and finishes, 24 action-queue resets and executor runs, 20 resumed
executor runs, and 1,377 of 1,378 selections; the remaining selection and two
of 2,012 input decodes contain interrupt code and match in a capture that
brackets it (32 selections, 270 decodes and ATB ticks). The escape input
matches all 618 decodes and 483 page steps, including both escape attempts (a
failed and a successful roll); the encounter input matches all 2,092 decodes.
Every page 1, 3, 9 and `64` step of the three inputs matches; on those
captures (no presentation brackets) the attack entries and page 5 steps stop
at their first presentation call. A shared encounter capture with the
brackets and resume hooks (frames 6570-8130) matches every attack step: all
1,100 page steps (4 up to the attack model, 504 up to the target text, 592
complete), 504 continuations after the target text (500 idle frames and 4
confirmations up to the combo camera), 4 combo continuations and 4
confirmations after the frame (commit, reaction script and results), and all
4 attack page entries after the model (one contains a CD interrupt and
matches in a short capture that brackets it).

The result screens are compared between frames (from the frame routine's
return to its next call, the result being the next call's return address):
1,545 of 1,644 steps match, all Cross waits (651 summary, 279 experience, 306
level-up, 302 gold/items frames), the flag steps and the window closes. The
99 others stop explicitly at screen contents: the damage count (sound
`80039e60`), level-up and item windows, learned skills, the summary steps and
the loading waits around the module.

A party defeat was observed with an analysis probe: two fingerprinted setup
writes lower the persistent HP of characters 0 and 2 to 1 before field entry
(the scenario becomes an `analysis_probe`; it is not progression). With the
enemy-turns input the second enemy attack knocks out the last member; the alive
update sets the outcome `81`. The resident battle mode (`8001b758`) then clears
`8004f30c`, selects mode 1 with the persistent map selector `8006f94e = 1ea`
(`8001996c`), and the game enters map 490, the memory-card load screen. That
epilogue (`Program::finish_battle_mode`) matches its original exit image on
the defeat probe and after the encounter's victory; the escape outcome `40`
takes the victory branch.

The post-battle module is
directory `10` file 4 (sha256 `f474fd48...`), byte-identical to RAM after
victory.
