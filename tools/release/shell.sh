#!/usr/bin/env bash
# Enter the locked release shell, including nix develop's own Bash lookup.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
shell_name=$1
shift
set -- "path:$root/nix/runtime#$shell_name" -c "$@"
if [[ $(uname -s) = Darwin && $(uname -m) = x86_64 ]]; then
    # Nix selects bashInteractive from the input named nixpkgs even when the
    # devShell uses another package set. Use the same Intel pin for both.
    pin=$(python3 - "$root/nix/runtime/flake.lock" <<'PY'
import json, sys
locked = json.load(open(sys.argv[1]))["nodes"]["nixpkgs-intel-macos"]["locked"]
print(f'github:{locked["owner"]}/{locked["repo"]}/{locked["rev"]}')
PY
    )
    set -- --override-input nixpkgs "$pin" --no-write-lock-file "$@"
fi
exec nix --extra-experimental-features 'nix-command flakes' develop "$@"
