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
field return. The escape route calls no action commit, AI run or result application;
the escape decision itself is not reconstructed, and defeat is not yet observed.
The turn scheduler, menu input, result screens and full return readiness remain
required work. The post-battle module is
directory `10` file 4 (sha256 `f474fd48...`), byte-identical to RAM after
victory.
