# Phase 1 checkpoint

The goal is the complete both-disc matching decomp in plan.md, whose checklist
records the exit items. The qualified compilers, every resident and overlay target
and their settings live under decomp/ (docs/matching.md). The build owns the status
rather than this file: `make -C decomp all-verify` compares every rebuilt image with
its original byte for byte, `all-coverage` reports source coverage by class
(remaining assembly, placeholders, SDK and handwritten code) and `all-container`
the packed containers. The public MIPS fixture (`make -C decomp smoke`) checks
tooling only.

Existing host-reference recovery and original comparisons remain available in
analysis/findings/ and src/reconstruction/. EVID-REF-047 is the bounded menu/ownership
checkpoint at the reviewed starting commit 6ec589387649af12c719458daa4b3530c57ba3b4.
Its failures and limitations are not closed by the new plan. Earlier facet/phase
references describe the historical workflow, not the current decomp exit.
