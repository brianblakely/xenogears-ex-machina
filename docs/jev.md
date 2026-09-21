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

This is entirely local. Local mode never reads credentials or sends requests.
The current recovery entry supplies the handler, next investigation, existing
C++ implementation and linked findings. Keyword retrieval selects at most twelve
line-exact excerpts from the explicit source allowlist. Each excerpt retains its
path, line range and full-file SHA256. The packet includes the original report,
inventory and tool hashes and preserves the inventory's status labels.

The tool uses the current inventory and execution report, not hardcoded
opcode-specific advice or a historical research frontier.

To use Jev, provision the isolated host broker below and add `--online`:

```sh
python3 -m tools.analysis.jev evidence \
  --report .local/execution/return-run-001.json \
  --question 'What explains task-list initialization, removal and ownership?' \
  --online --output .local/jev/evidence-ranked-001.json
```

If the operator **explicitly authorizes using the existing agent environment
credential**, the narrowly scoped override is
`--online --credential-env typesafe_api_key`. This reads only that exact variable
internally for the authentication header; its value is never printed or stored.
It bypasses the broker, makes no broker fallback, and labels new decisions
`transport: environment_credential`. A missing variable produces
`credential_environment_unconfigured`. This route does **not** provide
credential isolation: the agent environment contains the key. It was added at
the operator's explicit request after the isolated broker was found unconfigured.
The default remains the isolated broker; no environment credential is used
automatically. Neither a `jev` method nor a transport label proves OS isolation.

The operator subsequently explicitly authorized the local credential file:

```sh
python3 -m tools.analysis.jev evidence \
  --report .local/execution/return-run-001.json \
  --online --credential-file jevapikey.txt \
  --output .local/jev/evidence-file-ranked-001.json
```

This is another explicit direct route, labeled `transport: file_credential`;
it cannot be combined with `--credential-env`. The file is read only for the
authentication header, at most 4,097 bytes to enforce a 4,096-byte file limit.
Empty, non-ASCII, malformed, oversized or unavailable files produce only
`credential_file_unavailable`, without file contents or filesystem exception
details. The private-upload gate runs before the credential read. Local mode
never opens it. No file credential or credential path is included in request
bodies, packets or caches. `jevapikey.txt` is excluded by an exact ignore entry
and kept mode 0600; neither that permission nor ignoring the file isolates it
from the same-user agent. This operator-authorized route does **not** complete
credential isolation. Keep it outside the source archive and any export manifest.

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
`tools/analysis/jev_broker.py`; no moving model alias is used. Its fixed endpoint is
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
header inside the broker, or the explicitly authorized environment/file route.
Environment-supplied proxies and HTTPS redirects are
rejected. At most four requests are in flight, with an eight-second HTTP timeout
and one bounded retry for HTTP 429/529; longer server retry delays cause local
fallback instead of an unbounded wait. A forked broker child has a 22-second
whole-request deadline, including DNS, TLS, body reads and retry. Socket input
has a two-second idle timeout, a 32,000-byte envelope limit and one request per
connection. The accept backlog and child count are each capped at four. No
caller can provide an endpoint, authentication header, command or file operation.
Evidence paths inside candidate text are inert metadata and are never opened by
the broker. The only accepted operations are the existing evidence/proposal
relevance rubrics with the pinned model; private requests still require the
explicit upload permission.

Successful responses are validated and cached under `.local/jev/cache/`, keyed by
the exact endpoint, pinned model, question, source context and candidate contents.
Packets and cache entries are written with mode 0600. Invalid caches are
ignored. An absent broker, service errors or malformed responses preserve the
entire local ranking; partial model scores are never mixed with keyword scores.
Service error bodies and credentials are not written to packets. A complete
cache can be reused without a broker under `--online`. Default local mode ignores it
so the keyword baseline remains reproducible. Cache contents are local research
artifacts, not independent evidence or an authenticated service transcript.

## Credential isolation and host setup

Isolated online use is **unconfigured until an administrator installs and verifies
this boundary**. The 2026-09-20 inspection found Codex CLI 0.155.1 and no socket at
`/run/xem-jev/broker.sock`. It inspected CLI help, executable location and UID,
not credential files, shell history or process environments. The official
[Cloud environment documentation](https://learn.chatgpt.com/docs/environments/cloud-environment)
limits Cloud secrets to setup scripts; the documented
[network permissions](https://learn.chatgpt.com/docs/permissions) control
destinations and Unix sockets. Neither establishes an available host-managed
TypeSafe credential injector on this machine. A configured API key alone does
not establish isolation. The operator subsequently authorized the explicit
environment and file routes above; these do not complete isolation. No real credential
value was displayed, searched for, or included in an artifact.

The small Linux broker uses a separate `xem-jev` service UID, a private systemd
credential mount, and a reviewed administrator-owned standalone copy of
`jev_broker.py`. Kernel `SO_PEERCRED` restricts the socket to one agent UID.
The service and agent must have different non-root UIDs. The agent must lack
administrative rights that could read the credential, ptrace the service, or
modify `/opt/xem-jev`, the service unit, interpreter, or credential source.
Root ownership and the separate UID are the isolation boundary; the socket or
a shell wrapper alone would not provide it. The mutable checkout is never
imported by the deployed broker. Core dumps and service stdout/stderr are
disabled; responses contain only validated model/probability/token counts or
one constant error. There is no automatic environment-key fallback.

An administrator must perform these steps **outside the agent session**, after
reviewing the two deployment files. Do not grant the agent sudo, mount access,
service management or broad filesystem/network permission to perform them.
The placeholders contain paths and numeric identities, never key values.

1. Obtain the pinned Python executable path inside the existing Nix shell with
   `readlink -f "$(command -v python3)"`; use it for `@PYTHON@` in a separately
   reviewed copy of `tools/analysis/xem-jev.service.in`. Replace `@AGENT_UID@` and
   `@AGENT_GROUP@` with the agent's numeric UID and existing primary group. Keep
   this Nix store executable alive with a host-managed GC root. No extra runtime
   package is installed into the game or the development shell.
2. Install the protected account, code and unit from that reviewed copy:

   ```sh
   sudo useradd --system --no-create-home --shell /usr/bin/nologin xem-jev
   sudo install -d -o root -g root -m 0755 /opt/xem-jev
   sudo install -o root -g root -m 0644 <REVIEWED_BROKER_PY> /opt/xem-jev/jev_broker.py
   sudo install -o root -g root -m 0644 <REVIEWED_SERVICE> /etc/systemd/system/xem-jev.service
   sudo install -d -o root -g root -m 0700 /etc/xem-jev
   ```

3. First verify with a dummy credential and mocked upstream, using the short
   host-only procedure below. After it passes, use the host's secret manager to
   create `/etc/xem-jev/typesafe.key`, owned by root with mode 0600. For a secret
   manager with a stdout export operation, the host-only pattern is:

   ```sh
   <HOST_SECRET_MANAGER_EXPORT_COMMAND> | sudo sh -c 'umask 077; cat > /etc/xem-jev/typesafe.key'
   ```

   The operator reported that the existing key is configured as an agent/shell
   environment variable. Its presence or value was not inspected. If importing
   that existing variable, the **administrator's own Bash session**, outside
   Codex, can instead run:

   ```sh
   set +x
   umask 077
   builtin printf '%s' "${typesafe_api_key:?Configure the host shell first}" |
     sudo sh -c 'umask 077; cat > /etc/xem-jev/typesafe.key'
   unset typesafe_api_key
   sudo systemctl daemon-reload
   sudo systemctl enable --now xem-jev.service
   ```

   Bash's builtin writes directly to a pipe; no external command receives the
   credential in its argv, and there is no `tee` or terminal output. Do not
   perform this import through agent tools. Remove the original environment
   injection from the agent's launcher/configuration and fully restart the
   agent and any parent process that supplied it. For a CLI launched from a
   clean shell, `env -u typesafe_api_key <CODEX_LAUNCH_COMMAND>` removes it for
   that launch; ensure no startup hook reinjects it. Unsetting a variable in one
   child shell cannot clean an existing agent process. A configured broker does
   not isolate a duplicate key left in the agent's environment.

4. Verify the deployed code, service, interpreter and credential source cannot
   be modified/read as appropriate by the agent UID. Check denied access using
   a **dummy credential first**, not by trying to print the real key. A proper
   deployment verification must also show an authorized mocked ranking request
   succeeds and an unrelated UID cannot use the socket. Repository mock tests
   do not establish these OS properties. If an existing Codex network profile
   blocks this Unix socket, the administrator may allow only
   `/run/xem-jev/broker.sock` for this project using the documented Unix-socket
   permission, then restart that session. Preserve all other restrictions.
   A forwarding proxy that changes peer UID must not be worked around by
   allowing a broad/root UID; keep online mode unconfigured instead.

For the dummy check, install the service and reviewed broker first, then run
these **administrator-only** commands before importing the real key:

```sh
builtin printf '%s' 'xem-dummy-only' |
  sudo sh -c 'umask 077; cat > /etc/xem-jev/typesafe.key'
sudo sh -c 'cat > /opt/xem-jev/dummy.py; chmod 0644 /opt/xem-jev/dummy.py' <<'PY'
import runpy
scope = runpy.run_path('/opt/xem-jev/jev_broker.py')
def synthetic(payload, credential):
    assert credential == 'xem-dummy-only'
    return {'model': scope['MODEL'],
            'answers': {'relevance': {'type': 'noul', 'noul': 0.75}},
            'usage': {'input_tokens': 1, 'output_tokens': 1}}
scope['main'].__globals__['post'] = synthetic
scope['main']()
PY
sudo install -d -o root -g root -m 0755 /etc/systemd/system/xem-jev.service.d
```

Create the following administrator-owned, mode 0644 temporary drop-in at
`/etc/systemd/system/xem-jev.service.d/dummy.conf`, substituting the same reviewed
Python path and agent UID as in the unit, then run `sudo systemctl daemon-reload`
and `sudo systemctl start xem-jev.service`:

```ini
[Service]
ExecStart=
ExecStart=<PINNED_PYTHON> -I /opt/xem-jev/dummy.py --allowed-uid <AGENT_UID>
```

As the ordinary **agent UID**, run this in the pinned Nix shell. All four
protected writes and the dummy credential read must be denied, while the mocked
ranking must return the synthetic probability. It never prints file contents:

```python
import os
from tools.analysis import jev_broker as broker
for path, mode in [('/etc/xem-jev/typesafe.key', os.O_RDONLY),
                   ('/opt/xem-jev/jev_broker.py', os.O_WRONLY),
                   ('/opt/xem-jev/dummy.py', os.O_WRONLY),
                   ('/etc/systemd/system/xem-jev.service', os.O_WRONLY),
                   ('/etc/systemd/system/xem-jev.service.d/dummy.conf', os.O_WRONLY)]:
    try:
        fd = os.open(path, mode)  # No read, write, create or truncate operation.
    except PermissionError:
        continue
    os.close(fd)
    raise SystemExit('Isolation check failed')
request = {'model': broker.MODEL, 'questions': {'relevance': broker.QUESTION},
           'state': {'blocker': {}, 'question': 'Synthetic ownership question',
                     'candidate': {'id': 'dummy', 'excerpt': 'Synthetic data', 'private': False}}}
result = broker.broker_post(request, allow_private=False)
assert result['answers']['relevance']['noul'] == 0.75
print('Dummy source/code/config denial and authorized ranking passed')
```

These commands are a deployment verification procedure, not an executed result.
If the sandbox blocks the socket, leave that check pending until the exact socket
permission is configured. Afterwards the administrator must stop the service,
remove `/opt/xem-jev/dummy.py` and the `dummy.conf` drop-in, reload systemd, then
import the real credential and start the original unit. Never use the synthetic
response as a live Jev result or retain the dummy drop-in for online research.

Only after that verification, run one public-only `evidence --online` packet
through the socket. Inspect decision methods: `jev` is a new response, `cache`
is replay, and `local` with `credential_broker_unconfigured` or
`service_unavailable_or_invalid_response` means no online result. If the host
steps require intervention, continue reconstruction with local evidence.
Administrator provisioning and OS isolation verification were not performed in
the development sandbox. A narrowly approved capability check found systemd
261 and root-owned installation directories, but `sudo -n true` required a
password. Consequently a genuine separate-UID dummy deployment could not be
created by this session. Same-user mocks are not a substitute. No isolated live
smoke test is claimed; any authorized environment-route result is recorded in
its own packet and does not verify deployment.

## Validation and integration

```sh
python3 -m unittest discover -s tests -p 'test_jev*.py'
python3 tools/repository/check.py
```

The tests use authored synthetic metadata/text, dummy credentials and mocked
HTTP/socket responses. They cover operation restrictions, kernel peer-UID
checks, private-upload gating, client framing, bounded retries and deadlines,
sanitized errors, cache replay and missing-broker fallback. These are unit tests,
not deployment isolation verification.
They require no key, network, original discs, captures or C++ runner. Existing
CTest `repository-contracts` discovers them without new build configuration.
Compare local and Jev packets on real blockers before claiming a productivity
improvement. Historical evaluations must exclude later solution material.

This contribution was authored with ChatGPT assistance using this repository's
execution/recovery interfaces and the official TypeSafe documentation above.
No other Xenogears project's source or reverse-engineering work was consulted.
The new source, documentation and invented fixtures follow the repository's MIT
license. No original-game reconstruction or Phase 1 completion marks are changed.
