#!/usr/bin/env bash
# Builds the WebXR test page's wasm package into runtime/web/xr/pkg.
# Run inside `nix develop path:./nix/runtime` (cargo, wasm-bindgen 0.2.127).
# XEM_XR_PROFILE=dev builds unoptimized.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
# cargo reads runtime/.cargo/config.toml (web_sys_unstable_apis) from the cwd.
cd "$here/../.."
profile=${XEM_XR_PROFILE:-release}
cargo build --target wasm32-unknown-unknown -p xem-webxr --profile "$profile"
dir=$profile
if [ "$profile" = dev ]; then dir=debug; fi
wasm-bindgen --target web --no-typescript --out-dir "$here/pkg" \
  "target/wasm32-unknown-unknown/$dir/xem_webxr.wasm"
