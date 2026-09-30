# Original-target source

This directory owns new PS1-target recovery. Group source and small shared headers
by resident/overlay subsystem. Keep the existing host reconstruction separate as
reference/portability code; do not duplicate its Program abstraction here.

Read ../docs/matching.md. No Xenogears target/compiler is claimed qualified yet.
Add a target make fragment with its exact source identity, object list, linker
script and original-compatible compile rules. Unconverted original assembly/data
remain in ignored .local/ and are explicitly unfinished source recovery.

`make smoke` runs the authored MIPS toolchain fixture. `make CONFIG=... verify`
links the declared objects and performs exact, fingerprinted binary comparison.
Neither command infers source coverage. Track remaining source/assembly ranges
from actual target inputs and linker maps, not from another handwritten dashboard.
