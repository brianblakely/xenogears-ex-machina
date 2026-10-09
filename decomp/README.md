# Original-target source

This directory owns new PS1-target recovery. Group source and small shared headers
by resident/overlay subsystem. Keep the existing host reconstruction separate as
reference/portability code; do not duplicate its Program abstraction here.

Read ../docs/matching.md for the qualified compiler configuration and the
per-function loop. `targets/` holds one splat configuration and make fragment
per executable image (both residents and every decoded overlay); `src/<target>/`
holds its C. Functions not yet recovered are linked from locally generated
assembly with `INCLUDE_ASM`; that assembly, the original images and all build
output stay in ignored `.local/`.

`make all-verify` compares every target exactly and fails on a name a linker script
defines inside a target's own image or uninitialized data, other than the views a
target allows (`LINK_VIEWS`). `make all-coverage` reports C, nonmatching, SDK,
hand-written and remaining assembly, data and uninitialized data per target by where
the assembler put each byte (a second, labelled build of each C unit; see
../docs/matching.md); binary agreement and source coverage are separate claims.
