# Original-source reverse engineering

Ghidra is the primary environment for Phase 1. The separate
`nix/ghidra/flake.lock` pins Ghidra 12.1.2, Java 21 and the
[lab313ru PlayStation loader](https://github.com/lab313ru/ghidra_psx_ldr).
It also supplies an optional pinned [m2c](https://github.com/matt-kempster/m2c)
environment. The existing observation and build locks remain unchanged so that
historical execution evidence remains reproducible. This tool configuration is
currently exercised on x86_64 Linux only.

The objective is coherent, readable source and format/state documentation that
Phase 2 can incorporate. A broad provisional decompilation is a useful milestone;
it does not by itself satisfy a Phase 1 completion requirement.

## Qualified imports

Resume an existing qualified project when it contains the required source. If a
finding fingerprints the closed project or its exports, preserve those artifacts
and work on a copy. Qualify newly required overlays by their original bytes, load
addresses and loading context. Reimport existing code only when needed to repair
or extend its qualification.

For a new import, from the repository root with the captured original raw tracks:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra
python -m tools.analysis.ghidra_project \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 \
  --output .local/ghidra/new-disc1-analysis
python -m tools.analysis.ghidra_project \
  --raw .local/references/source-2/disc.bin \
  --profile na-slus-00669-5eab85c683d4 \
  --output .local/ghidra/new-disc2-analysis
```

Each output must be new. The importer verifies the complete original track,
resident executable, measured packed-overlay slot and decoded-overlay identity.
It imports the resident through `PsxLoader` with `PSX:LE:32:default`, verifies
every resident byte in Ghidra, and creates a separate overlay address space named
from its SHA256. Both reviewed disc profiles currently contain the same field
overlay; the resident and disc identities remain separate. Other overlays need
their own recovered load address and source identity before import.

The private output contains source manifests, original bytes, the invocation,
logs, an automatic function inventory, selected instruction/P-code listings,
decompiler output and a closed Ghidra project. `verification.json` fingerprints
the authored import sources and generated artifacts. The wrapper checks script,
analyzer, decompiler and project-save completion rather than accepting the
headless process exit code alone.

Ghidra rejects project paths containing a component beginning with a dot. The
wrapper therefore uses a temporary private project, closes it, and copies it to
`closed-project/` under the requested `.local` result. To resume interactively,
copy that directory into a private working directory with no hidden path
components, start `ghidra` inside the same pinned shell, and open `original.gpr`.
Keep all projects, RAM, original bytes and automatic exports outside the public
source allowlist. The environment flake contains only tool configuration; do not
add private data or links to it.

The pinned extension needs two explicit compatibility adjustments. Its checked-in
compiled SLEIGH file predates Ghidra 12.1.2, so the recipe rebuilds it from the
pinned language source. Its `MipsPreAnalyzer` shares a class name with Ghidra's
built-in analyzer, which only accepts the `MIPS` processor. Renaming the extension
class makes its `PSX` unaligned-load analysis discoverable. These patches change
tool packaging, not original program bytes.

## Reconstructing a subsystem

Begin with related Ghidra C exports across the qualified resident and required
overlays. Use their calls, shared globals, structures, initialization and resource
dependencies to select a connected boundary. Preserve original addresses and
source identities. Correct supported shared types, signatures and register
context in Ghidra, then re-decompile dependent functions before translating them.

Use C-like output as the starting point for authored, readable C/C++. Retain
existing Python models as extraction/comparison references. Inspect assembly or
P-code selectively for ambiguous calling context, overlay references, arithmetic,
indirect dispatch and hardware operations. m2c remains optional for a concrete
function-level advantage. The [field lifecycle handoff](../analysis/formats/field-lifecycle.md)
and `analysis/recovery.json` distinguish automatic surveys, inferred annotations,
source-reviewed modules and execution coverage.

Reuse existing captures and purposeful connected tests. Collect new original
execution when it resolves material uncertainty or a required subsystem boundary;
batch repository/source audits at meaningful milestones. A small helper does not
require its own elaborate proof pipeline.

### Repeatable working loop

1. Select the next blocker from the dependency map in
   [the recovery inventory](../analysis/recovery.json) and the
   [slice contract](phase1-slice-contract.md). Prioritize connected loaders, state
   initialization, events, field behavior, encounter/return, menus, persistence
   and required media. Allow provisional understanding outside that boundary.
2. Export the relevant functions with callers, callees, dispatch sites,
   initialization and cleanup. Retain source profile, executable/overlay digest,
   addresses, body ranges, signatures, direct/computed references, warnings and
   annotation provenance. Keep missing functions and failed exports visible.
3. Read the C as a connected program. Trace data from loading through ownership,
   initialization, updates and release/return. Consult existing source models,
   format notes and captures before reconstructing established behavior.
4. Identify corrections that clarify several functions: a shared structure,
   forwarded argument, return type, pointer base or register context. Apply only
   supported corrections in the qualified working project and record their basis
   and limits. Retain original identities and addresses when naming functions.
5. Re-decompile affected callers, callees and other users of the shared data.
   Compare with the previous export and the original source. Check whether the
   correction resolves the ambiguity or merely hides a warning. Inspect specific
   instructions or P-code where needed, feed the result back into Ghidra, and
   repeat the dependent review.
6. Produce authored modules and format/state notes as understanding develops.
   Specify inputs, state changes, ownership and unresolved services. Validate the
   connected boundary with existing evidence first, then record the remaining
   blockers. Complete analysis of every adjacent helper is not a prerequisite
   for a useful, explicitly scoped module.

### Types, signatures and context

Build partial structures from supported offsets, access widths, strides and
allocation sizes. Preserve unknown members and bits. Distinguish an address,
pointer, relative offset, array index and resource selector before assigning
types. A cleaner decompilation is not sufficient evidence for a type change.

Review callers and callees together when correcting parameters and returns.
Registers may be forwarded unchanged: a missing automatic parameter does not
prove that the argument is unused. Record implicit register inputs, stack
arguments, return extension and shared-global context where they affect behavior.
Scope `gp` and other register assumptions to the supported startup/call path or
captured context. Do not apply one observed value to arbitrary restore paths.

Resolve numeric references in the correct overlay address space and loaded
context. Identical addresses in different overlays are different source
identities; a static target alone does not establish the overlay active at runtime.
Retain the original integer contract when translating C: signed loads, truncation,
sign/zero extension, wraparound, shifts and fixed-point rounding. Host C/C++
undefined behavior must not replace a defined original operation.

For each correction, retain the supporting source/capture, the prior inference
and unresolved alternatives in a comment or linked review record. An annotation
is not an execution claim, even when it clarifies many functions.

### Function boundaries and indirect dispatch

A table value pointing into executable memory is a candidate code reference,
not automatically a function entry. An evidence window or trace-hook address can
also lie inside a function. Do not create functions at all such addresses merely
to obtain separate C output.

Review the table's consumer. Call dispatch and internal switch jumps have
different control-flow and register-context implications. Check incoming
calls/jumps, surrounding flow, stack setup, saved-register use and shared returns.
A prologue is supporting evidence, not a requirement for every leaf function.
Internal switch targets may depend on locals established before the jump and
belong in the containing function.

When indirect flow is missing, verify the original table, index calculation and
possible targets before adding flow references in the correct address space.
Remove demonstrably incorrect splits, restore the containing flow, and
re-decompile the containing function and affected callers. Preserve the earlier
export and record the correction. Do not invent an index guard the original does
not enforce; keep unresolved and non-code table values visible.

Warnings about unreachable code, unexplained `unaff_*` values, implausible
low-memory globals, missing arguments or pointer scaling call for review of
boundaries, signatures and context. They do not establish that the original code
has no effect. A warning-free export still needs semantic review.

## Selective function review

Start with a concrete question the C does not answer reliably: calling convention,
forwarded registers, overlay reference, pointer arithmetic, signedness, overflow,
fixed-point rounding, indirect dispatch, GTE operation or hardware interaction.
Inspect enough surrounding instructions and delay slots to answer that question.
Expand the review when needed; routinely rebuilding every function from assembly
is unnecessary.

Use the Ghidra listing, delay slots, references, memory map and P-code alongside
the decompiler. Record disc/revision, resident or overlay digest, address, complete
function boundary and callers before assigning a semantic name. Library signature
names and automatically inferred boundaries remain suggestions. The importer
clears the loader's fallback PsyQ version; it does not assume a version when the
loader cannot detect one. A detected SDK version and inferred GP value still need
review. Uninitialized memory and hardware blocks are not captured runtime state.

Resolve signed loads, pointer widths, state ownership, fixed-point shifts, 32-bit
wraparound, GTE operations and shared return paths explicitly. Reconstruct the
behavior in independently authored source, compare it with qualified original
execution and unchanged uninstrumented controls, and test meaningful boundaries
with authored fixtures. Reject altered source, trace inputs and expected effects.
Record unobserved branches and unresolved calls in `analysis/recovery.json`.
Automatic pseudocode and a successful import do not close a gameplay finding or
a Phase 1 todo.

Capstone remains a secondary byte-level decoder in `nix#disassembly` and the
Ghidra shell. The original-observation core and read-only instruction traces
remain in `nix#observation` and `nix#observation-trace`. Consult
[the evidence workflow](evidence-workflow.md) before promoting a result.

## Authored source, validation and handoff

Prefer reviewed C/C++ for stable game logic Phase 2 can incorporate. Use Python
for extraction, exploration and comparisons, retaining verified models unless a
new implementation has a concrete integration benefit. Keep PS1 correlation
layouts separate from native semantic interfaces. Document required services and
resource lifetimes; unknown operations must fail explicitly or remain visibly
unsupported, never become guessed behavior or successful no-ops.

Raw decompiler output, original code/data, projects and traces stay private.
Renaming or formatting an automatic export does not make it reviewed authored
source. Distributable modules need an understood implementation, source-qualified
provenance, explicit uncertainty and intentional source-allowlist entries.
Preserve historical evidence artifacts when changing current sources.

Record these distinct claims in the existing findings and recovery inventory:

| Claim | What it establishes |
| --- | --- |
| Automatic output | Ghidra discovered/exported code under recorded settings; boundaries and semantics may be wrong |
| Inferred annotation | A proposed name, type, signature or context has a stated basis and limits |
| Source-reviewed reconstruction | Authored behavior has been reviewed against qualified original source; unresolved dependencies remain recorded |
| Execution-validated behavior | The reconstruction agrees with independent original captures on specified paths and inputs |

These are separate claims, not automatic whole-subsystem promotions. Retain the
existing confidence vocabulary and maturity gates. Execution coverage of one path
does not validate every source-reviewed branch or an unknown call it surrounds.

Start validation with existing qualified captures, models and tests. Use authored
fixtures for meaningful arithmetic, malformed-input and control-flow boundaries;
use original execution for material uncertainties and important transitions.
Expected original behavior must come from independent source or execution
evidence, not the new implementation's output. Retain the existing source/trace
qualification and control-comparison requirements for original-execution claims.

Group checks around connected behavior and shared state. A fresh capture or
separate elaborate proof pipeline is not required for each small helper. Run
affected checks during development; batch comprehensive repository, provenance
and distribution audits at meaningful milestones. Repeat or broaden passing
checks when a new change, failure or unresolved concern warrants it. Keep
unexercised behavior explicit.

Report the reusable modules, recovered format/state contracts, resolved subsystem
relationships, source identities, validation scope and remaining Phase 2 blockers.
Mark `plan.md` todos complete only when their actual requirements and required
evidence are met. A large export or passing synthetic suite does not complete a
broad Phase 1 todo or the slice proof.

## Optional matching reconstruction

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
m2c --target mipsel-gcc-c --no-cache --stop-on-error \
  --context .local/analysis/function-context.c \
  .local/analysis/function.s > .local/analysis/function-m2c.c
```

Select m2c when a bounded function benefits from another control-flow/type
reconstruction or matching-oriented C work. Derive the input assembly from the
qualified original instructions and record any symbol or type annotations. The
target option configures the tool; it does not establish the original compiler.
Check its output against Ghidra and original execution. Binary matching is not
the project's completion criterion, and generated C stays private until reviewed.

The m2c build runs its upstream suite. Its pinned Python Graphviz 0.21 dependency
includes Python 3.14 compatibility fixes and explicitly overrides m2c's upstream
`~=0.20.1` requirement. The verified build passed all 431 upstream cases. This is
tool validation, separate from game-behavior evidence.

Tool dependency and license records are in
[dependencies.json](dependencies.json). The PSX extension and signature repository
lack a repository-wide license in their pinned trees; no redistribution permission
is inferred. They are external local analysis tools and are not included in the
source archive or native runtime.
