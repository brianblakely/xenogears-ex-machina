# Jev-assisted Phase 1 research

`tools.analysis.jev` is an optional, read-only assistant to the
[executable reconstruction loop](executable-reconstruction.md). It assembles a
small evidence packet for an encountered dependency and optionally asks TypeSafe
Jev to rank it. It can also rank explicitly authored experiment or Ghidra
investigation proposals. It never implements game behavior, executes a proposal,
changes Ghidra, edits recovery status, or grants a proof pass.

## Start with the execution report

Run these commands from the repository root in the existing pinned Nix shell.
Use the full saved `tools.analysis.execution run` report, not its stdout summary.
Correct behavioral divergence before investigating a later missing dependency.

```sh
python3 -m tools.analysis.jev evidence \
  --report .local/execution/return-run-001.json \
  --output .local/jev/evidence-local-001.json
```

This is entirely local, including when `TYPESAFE_API_KEY` happens to be set.
The current recovery entry supplies the handler, next investigation, existing
C++ implementation and linked findings. Keyword retrieval selects at most twelve
line-exact excerpts from the explicit source allowlist. Each excerpt retains its
path, line range and full-file SHA256. The packet includes the original report,
inventory and tool hashes and preserves the inventory's status labels.

For the current sprite `0x96` dependency, the existing next investigation leads
to task-list initialization/removal rather than asking a model to pick a new
subsystem. The tool uses the inventory, not hardcoded opcode-specific advice.

To use Jev, set `TYPESAFE_API_KEY` in the agent's environment and add `--online`:

```sh
python3 -m tools.analysis.jev evidence \
  --report .local/execution/return-run-001.json \
  --question 'What explains task-list initialization, removal and ownership?' \
  --online --output .local/jev/evidence-ranked-001.json
```

Read `candidates` in packet order; use the paths and line ranges to inspect the
complete sources. `decisions` records Jev's relevance probabilities, model and
token usage. A high relevance probability is not confidence in recovered game
behavior. The local score is keyword overlap, not a probability.

All output paths must be new and under ignored `.local/`. Exit 0 means a research
packet was written, including an explicitly labeled local fallback; it does not
mean Jev succeeded or Phase 1 advanced. Invalid input exits 2. The stdout summary
and packet both report the selected method.

## Source boundaries and private exports

The report must match the current executable/profile/field-overlay identity in
`analysis/recovery.json`. The existing inventory validator checks namespaces,
statuses and dispatch fingerprints. A stale report table target is rejected.
Findings for other profiles or conflicting recorded code identities are excluded.
Public notes and authored code remain **repository context**, not automatically
qualified original evidence. Plain text has no authoritative source metadata.

The tool does not crawl `.local/`, `discs/`, arbitrary directories, or paths found
inside findings. In particular, private capture paths in a public finding are
not followed. To include specific private text exports, create a manifest under
`.local/` with this structure:

```json
{
  "artifacts": [
    {
      "path": ".local/ghidra-work/task-list.c",
      "sha256": "FULL_FILE_SHA256",
      "source": {
        "profile": "COPY_FROM_REPORT_SOURCE",
        "executable": "COPY_FROM_REPORT_SOURCE",
        "overlay": "COPY_FROM_REPORT_SOURCE"
      }
    }
  ]
}
```

Replace the illustrative values with the exact digest and source binding of the
already-qualified export. Identity comparison happens before reading its bytes;
a matching identity still requires an exact full-file digest. The manifest is an
operator assertion, **not independent source qualification**. The original
[evidence workflow](evidence-workflow.md) still applies.

Add `--manifest .local/jev/exports.json` to the evidence command. Private excerpts
can be used locally without any upload. Combining them with `--online` also
requires the operator's explicit `--allow-private-upload` permission, even for
cached rankings. Do not give an unattended agent permission to set that flag
without approval. This authorizes sending the selected text to TypeSafe; it is
not permission to publish the original export.

Files are limited to 512 KiB, excerpts to 3,500 characters and 48 lines, and
private manifests to 64 entries. Public unavailable/oversized files and identity
exclusions are reported. Unsafe paths, symlinks and private digest mismatches
fail rather than being silently accepted. Unknown metadata does not become an
original-behavior claim.

## Rank experiments or Ghidra investigations

Codex supplies hypotheses and bounded candidate investigations. For example,
create a local proposal file from the current report's source identity:

```python
import json
from pathlib import Path

report = json.loads(Path('.local/execution/return-run-001.json').read_text())
proposals = {
    'source': report['source'],
    'question': 'Which investigation best distinguishes an uninitialized task list '
                'from incorrect list removal? Prefer existing evidence before a fresh capture.',
    'candidates': [
        {'id': 'inspect-existing',
         'description': 'Inspect existing qualified initialization and removal exports '
                        'with their callers, list head and ownership stores.'},
        {'id': 'targeted-observation',
         'description': 'If existing captures lack initialization, prepare source-guarded '
                        'observations before and after initialization and removal on the '
                        'same original route. Confirm hook availability first.'}
    ]
}
with Path('.local/jev/proposals.json').open('x') as stream:
    json.dump(proposals, stream, indent=2)
```

```sh
python3 -m tools.analysis.jev choose \
  --report .local/execution/return-run-001.json \
  --choices .local/jev/proposals.json \
  --online --allow-private-upload \
  --output .local/jev/proposals-ranked-001.json
```

There may be 1–12 proposals, each with a unique ID and description. Without
`--online`, the tool preserves their authored order. Proposals remain private
research input and require upload permission. Their source identity must match
the blocker. No command strings, memory writes or Ghidra changes are executed.

Use the same command for a shared-type, signature or calling-context question:
state the concrete ambiguity in `question`, then describe the candidate
investigations. Include their prerequisites, available observations and expected
discriminating observations. Jev ranks investigations; source review determines
which explanation is correct. Any useful discovered route must still become a
fixed, independently replayed original-game scenario.

## Network, caching and service review

The optional external analysis service is **TypeSafe Jev `jev-1.13.0`**, pinned in
`tools/analysis/jev.py`; no moving model alias is used. Its fixed endpoint is
`POST https://api.typesafe.ai/v1/systemone`. Official sources consulted on
2026-09-20: [HTTP contract](https://docs.typesafe.ai/api),
[model identity](https://docs.typesafe.ai/models), and
[reranking pattern](https://docs.typesafe.ai/cookbooks/rerank_typesafe).

This independently authored adapter uses existing Python standard-library HTTP,
JSON and concurrency support. No SDK, package, native library, model weights or
new Nix dependency is installed or redistributed. The alternative is the local
retrieval path; adding an SDK would enlarge the dependency closure for one small
HTTP operation. The service is optional analysis infrastructure, never a runtime,
build, configure-time or CI dependency. Remote service use is subject to the
operator's TypeSafe account terms; no model redistribution license is asserted.

Requests contain one bounded candidate, the research question and a projected
blocker. Full reports, checkpoints, partial game state, expected output files and
API keys are not included in request bodies. Authentication uses only the Bearer
header. HTTPS redirects are rejected. At most four requests are in flight, with
an eight-second timeout and one bounded retry for HTTP 429/529; longer server
retry delays cause local fallback instead of an unbounded wait.

Successful responses are validated and cached under `.local/jev/cache/`, keyed by
the exact endpoint, pinned model, question, source context and candidate contents.
Packets and cache entries are written with mode 0600. Invalid caches are
ignored. Missing credentials, service errors or malformed responses preserve the
entire local ranking; partial model scores are never mixed with keyword scores.
Service error bodies and credentials are not written to packets. A complete
cache can be reused without a key under `--online`. Default local mode ignores it
so the keyword baseline remains reproducible. Cache contents are local research
artifacts, not independent evidence or an authenticated service transcript.

## Validation and integration

```sh
python3 -m unittest discover -s tests -p 'test_jev.py'
python3 tools/repository/check.py
```

The new tests use authored synthetic metadata/text and mocked HTTP responses.
They require no key, network, original discs, captures or C++ runner. Existing
CTest `repository-contracts` discovers them without new build configuration.
Compare local and Jev packets on real blockers before claiming a productivity
improvement. Historical evaluations must exclude later solution material.

This contribution was authored with ChatGPT assistance using this repository's
execution/recovery interfaces and the official TypeSafe documentation above.
No other Xenogears project's source or reverse-engineering work was consulted.
The new source, documentation and invented fixtures follow the repository's MIT
license. No original-game reconstruction or Phase 1 completion marks are changed.
