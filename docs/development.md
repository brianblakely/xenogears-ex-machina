# Development

Phase 1 starts with [matching](matching.md), not a repository-wide document audit.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix/ghidra#matching
make -C decomp smoke
```

For Ghidra, enter `path:./nix/ghidra` instead. Reuse qualified private projects and
source identities; importer usage is in [reverse engineering](reverse-engineering.md).

For the retained host C++ reference and its public regressions:

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix
python3 tools/repository/build.py debug --target test-program --test connected-program
python3 tools/repository/check.py --preset debug
```

Use `--preset all` explicitly for debug/release/sanitizers. Formatting is an explicit
`python3 tools/repository/format.py --check` operation, not a reason to regenerate
requirements metadata. The source allowlist and JSON integrity are checked by
`tools/repository/validate.py`; historical evidence validators remain available to
the tests that exercise them. No generated requirements matrix is required.

Heavy reference captures/builds retain their host-wide concurrency locks. Do not
recapture a passing route per function or poll files in indefinite sleep loops.
Read only the current implementation, tests and evidence needed for the change.

Original images, extracted bytes and execution artifacts stay outside distributed
sources. Keep Nix path inputs restricted to their tool directories. Matching game
images needs the user's originals; public synthetic tests never claim game parity.
