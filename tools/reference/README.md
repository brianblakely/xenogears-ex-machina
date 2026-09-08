# Original-source reference tools

`capture.py` verifies a supplied CHD and measures standard disc/boot metadata.
`scenario.py` provides checkpoint-free original-game scene testing. Its bounded
executor and `observe.py` backend drive a separately supplied, pinned
PCSX-ReARMed libretro core. These tools do not implement a native game runtime.
The external core is analysis infrastructure and is not linked into or included
with the project's baseline executable or source archive.

Run from the repository root after the source has a measured profile:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix#observation
python3 tools/reference/scenario.py painting-room \
  --content 'discs/Xenogears disc 1.chd' \
  --output .local/scenarios/painting-room-example
```

Use the `disc2-field-15` preset with the other source and a new output directory.
See [scenario contracts, examples and adapter backlog](../../analysis/scenarios/README.md)
for direct numeric fields, declarative setup, readiness, ordered inputs, and
capture provenance. New emulator work uses this scenario system; low-level
adapter probes use `observe.py --program` with an explicit bounded analysis
program and cold boot.

Both launchers accept `--sample-memory <spec.json>` for bounded, read-only RAM
sampling. A specification contains `schema_version: 1`, an identifier `name`,
the exact `source_profile`, `start_frame`, `every_frames`, `max_samples`, and
`ranges`. Each range has a unique `name`, a `size` in bytes, and either a direct
RAM `offset` or a `pointer_offset` plus signed `relative_offset`. All offsets
and frame counts are JSON integers. For example, a synthetic range is
`{"name":"actor-state","pointer_offset":64,"relative_offset":12,"size":8}`.
Pointers are read as little-endian 32-bit values; physical RAM and its first
KSEG0/KSEG1 aliases are accepted. Null, unmapped, and out-of-bounds pointers
produce explicit unavailable records. An in-range pointer still needs separate
evidence for the object's identity and lifetime, especially when overlays change.

Samples occur after the preceding `retro_run` and before the next scenario tick
or its guarded writes, including boundary zero when requested. Each private
`memory-trace.jsonl` record contains the frame, active program step before that
tick, buttons used in the preceding run, resolved ranges, and their bytes in
hexadecimal. The exact specification, sampler and validation-helper hashes,
trace hash, unavailable counts and budget status accompany the observation.
The sampler stops recording at its budget without changing scenario execution.
It accepts at most 32 ranges, 1024 bytes per range, 8192 bytes per sample, 36001
samples, 64 MiB of requested payload and one million range records. This samples
frontend boundaries; many original instructions can run between samples.

For individual original script dispatches or other recovered code points, use
the optional `observation-trace` Nix shell and add
`--trace-instructions <spec.json>` to either cold-boot launcher. This shell builds
the same pinned external core with a small host-side interpreter extension from
`nix/reference-trace.h` and `nix/reference-trace-patch.py`. It adds no game
addresses and does not change emulated RAM, registers or cycle arithmetic. The
extension is GPL-2.0-or-later analysis infrastructure; it is not linked into the
native runtime. Qualify any new core revision against an uninstrumented original
run before promoting its observations.

An instruction-trace specification has `schema_version: 1`, `name`, exact
`source_profile`, inclusive `start_frame`, exclusive `end_frame`, `max_callbacks`
and 1..16 `hooks`. Each hook has a unique `name`, aligned original `pc`, a
`guard` containing RAM `offset` and exact lowercase `expected` code bytes, and
`ranges` using the RAM sampler's range forms. A range may alternatively contain
`register`, `relative_offset` and `size` to read a RAM address from a captured
register; this is useful when the original instruction has already calculated a
script operand address. The guard must cover the watched instruction and checks
both current RAM and the core's fetched opcode. Address reuse with a different
overlay produces counted guard rejections. No guessed or unmatched instruction
is silently interpreted.

Extension API 2 also exposes the first 1 KiB of scratchpad backing memory to
register-based ranges. The physical `1f800000` and pinned-core `9f800000`/`bf800000`
aliases are bounded independently from RAM. Such records include
`resolved_space: "scratchpad"` and an offset within that 1 KiB. Adjacent hardware
registers and ranges crossing the boundary are unavailable; no emulated bus
read occurs. Direct ranges, pointer-offset ranges and digests remain RAM-only.
The collector also accepts historical API 1 cores without scratchpad support.

The private `instruction-trace.jsonl` records matched addresses immediately
before instruction dispatch, with zero-based `frontend_run`, the core's raw
32-bit cycle/subcycle values, registers and sampled bytes. `gpr_u32` contains
registers 0..31 followed by LO and HI. `dispatch_path` is 0 for ordinary fetch,
1 for a normal branch delay slot, 2 for a nonbranch instruction after a branch in
a delay slot, or 3 for the core's special branch-in-delay handling. Register
values are those exposed at that dispatch point; unusual branch/load-delay
semantics require separate interpretation. Callback budgets include rejected
guards; reaching a budget leaves the remainder untraced and is explicit in the
report. Payload and range-record limits apply before execution. The exact spec,
core/tool/extension-input hashes, rejection counts and trace hash accompany the
capture. After-handler effects can be sampled by specifying the independently
recovered return address as another hook.

The source is identified
by its selected CHD hash, not by its filename. A different CHD container requires
independent measurement and reference-profile review, even if its decoded data
might match an accepted raw-track hash. Output directories are never overwritten.

Historical input intervals use zero-based frontend frame indices: `start` is inclusive and
`end` is exclusive. Overlapping intervals combine their held buttons. The example
selects New Game after the title presentation; it is independently recorded input,
not an original asset. Supported names are `up`, `down`, `left`, `right`, `cross`,
`circle`, `square`, `triangle`, `start`, `select`, `l1`, `r1`, `l2`, and `r2`.
Frame counts describe `retro_run` calls, not recovered Xenogears simulation ticks.

The default explicitly selects the core's HLE BIOS. An optional `--bios` argument
accepts a user-supplied 512 KiB BIOS, hashes it, and copies it into that private
capture's system directory. Automatic core selection is requested in that mode;
a reported HLE fallback causes failure. No BIOS is downloaded or distributed.
Both modes are external-emulator observations and need corroboration before
establishing hardware timing or uncertain original-game behavior.

Each completed capture records its source profile, CHD/core/tool/Nix-lock hashes,
input intervals, core options, notices, reported timing, images, audio, and RAM
hashes. Private RAM and emulator checkpoint files support later investigation.
`started.json` records setup; only `observation.json` denotes a completed capture.
Historical observations may include `--load-state <previous>/final.state`.
Restore checks the checkpoint hash and source, core,
BIOS, and option identities against the previous completed observation. These
checkpoints are specific to the external emulator; they are not native save
states and do not satisfy Phase 8.

The archived EVID-REF-006 route is
`tests/reference-inputs/opening-observation-route.json`. Its reproducible stage
procedure was: write each stage's `inputs` array to a local JSON file; call
`observe.py` with that stage's `frames`, `capture_every`, selected Disc 1 CHD and a
new local output directory; start `opening` from reset, and supply the immediately
preceding stage's `final.state` to every subsequent stage. After `room-square`,
run each branch independently with its own frames, capture interval and inputs,
always restoring `room-square/final.state`. Never restore the preceding branch.
The exact observer version is hashed in the finding and retained locally as
`.local/tools/observe-before-scenarios.py`. This describes how the existing
evidence was collected; the cold-boot painting-room preset now reaches the menu
directly for further investigations.

`tests/reference-inputs/kernel-mode-observation-route.json` records additional
cold-boot Kernel menu paths for EVID-REF-009. To reproduce them in the pinned
observation shell, create a new private route directory, write each case's
`scenario` object to its own JSON file, and pass that file to `scenario.py` with
the matching measured CHD and a new output directory. These cases assert Kernel
readiness and execute bounded input schedules; their `observed_output` records
the reviewed result. In particular, the original Bonus Battling label is itself
a selectable entry, and the early exploratory filenames do not reliably name
the selected branch. The Battle and Menu cases' flat/black output is recorded as
an unresolved setup path, without claiming successful gameplay initialization.

`tests/reference-inputs/menu-observation-route.json` uses the same reproduction
procedure for EVID-REF-011: write a case's `scenario` object to a private JSON file
and run it with the matching CHD and a fresh output directory. These cold-boot
paths reach the File menu from field 14 on Disc 1 and through the world-map
bridge transition on both discs. The reviewed outputs show card-slot grids and
Copy/Delete options. Their initialized RAM headers identify source coordinates
for further save research; ordinary card save/load has not been validated.

A measured HLE checkpoint experiment resumed the first source after frame 2400
and compared it with uninterrupted input through frame 2700. The final image
matched, but two emulated RAM bytes differed, at offsets `0x57844` and `0x595c4`.
Their meaning is unresolved. Do not mask them out or treat visual agreement as
state equivalence. Historical checkpoints preserve existing evidence and require
separate validation for any behavioral comparison that depends on them. New
emulator investigations use the cold-boot scenario system described above.

Captures, audio, executable memory, BIOS copies, and emulator checkpoints stay
under ignored `.local/`. Public tests use invented colors, inputs, and checkpoint
bytes. They check the capture frontend, not original-game correctness.

The frontend uses the public [libretro ABI](https://github.com/libretro/libretro-common/blob/master/include/libretro.h).
The core's [upstream documentation](https://docs.libretro.com/library/pcsx_rearmed/)
describes its GPLv2 license and BIOS modes. See the dependency review and
`docs/evidence-workflow.md` before promoting an observation into a finding.
