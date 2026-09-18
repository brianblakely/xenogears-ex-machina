# Compiled field collision queries and sweeps

`field_collision.hpp/.cpp` recover the ordinary and special triangle queries
(`8007bef4`, `8007c694`), movement sweeps (`8007bac0`, `8007b814`), edge projection
(`8007b6c4`), inline slope projection (`8007bd04..8007bdb8`), resident atan
(`8004b32c..8004b4ac`) and square root (`80048c4c..80048cd0`), and planar length
(`80099a4c`). These are the original source operations described by EVID-REF-024,
for profile `na-slus-00664-39c547a9afc6`, field overlay SHA-256
`38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc`.

The queries call the shared C++ height and normal implementation from EVID-REF-019
and actor initialization. NCLIP is now shared as `field_packed_area`; the existing
`field_edge_area` uses that implementation too. Edge and slope projection reuse
`normalize_field_vector`, and sweeps reuse the compiled original trigonometry
reader. Tables and collision component bytes are caller-owned original resources.
No original geometry, tables or code bytes are redistributed here.

The query retains the original ADDU coordinate packing, signed halfword reloads,
adjacency order, terrain masks, special-surface height check and 32-step limit.
A selected neighbor of `-1` reads its attribute identifier two bytes before the
triangle table before testing that sentinel. Its effective terrain is not
invented as zero. Initial triangle `-1` returns before parsing geometry and leaves
all outputs untouched. Other triangle and vertex accesses are checked against
their particular layer, including when a bad index would fit elsewhere in the
component. Invalid source accesses fail explicitly.

Ordinary sweeps probe left, right, then center; special sweeps probe center,
left, then right. Probe Y is not read. Failed probes can replace the caller's
edge and project a tangent before the floor query. A failed floor or slope
requery retains the original actor and velocity, with prior edge writes intact.
Successful calls store the selected floor and displacement, with the ordinary
slope flag set at its original stage. The byte-window entry point preserves
all unrelated actor bytes and both edge padding halfwords. It applies the
ordinary/special selection from the original motion caller.

The optional trace containers report computed intermediates; they supply no game
behavior and are not required by runtime calls. Original division BREAK paths,
negative square-root inputs, and normalization overflow remain explicit errors.
This does not implement GTE register side effects, timing, scratchpad copy lineage,
or the later position, layer, ceiling, rollback and contact operations.

The private comparison adapter replaces only candidate functions in the existing
EVID-REF-024 verifier. Python dataclasses carry native results; original payloads,
source qualification, nested call/stack checks and controls remain unchanged.
The compiled code matches all 729 recorded sweeps and 3,230 queries on the two
preserved routes, including 5,594 height calls, 160 edge projections, 634 slope
normalizations, 3,573 lengths and 9,245 square roots. A separate direct byte-window
comparison matches all actor, velocity and edge bytes on every sweep. The six
recordings also match all 119 artifacts from their separately built
uninstrumented controls. Shared actor initialization still matches 25 original
defaults, 50 floor queries and 686,400 compared payload bytes.

Authored C++ cases exercise missing-neighbor attributes, malformed per-layer
bounds, double-edge decisions, iteration limits, terrain masks and transitions,
signed arithmetic and inclusive edge gates, probe ordering, forced floors and
failed slope preservation. These tests cover source branches that the original
routes did not observe; they do not expand the observation claim.

Validation uses `nix develop path:./nix`, the CMake sanitizer preset and
`field-collision-reconstruction`. Exact private capture paths, source and binary
hashes, reproduction commands and unchanged-comparer reports are retained in
`.local/analysis/phase1-collision-cpp-20260918/`. The retained Ghidra query bodies
omit switch arms and the atan body omits shared tails, so review uses the complete
qualified instruction windows and dispatch tables as well as decompiler output.

Authored with AI assistance from this repository's original-disc source exports,
existing independently captured execution and prior local analysis. No other
Xenogears implementation was consulted. New C++ and authored fixtures are
project-owned MIT source. This source chunk does not close a broad Phase 1
requirement or establish a playable native route.
