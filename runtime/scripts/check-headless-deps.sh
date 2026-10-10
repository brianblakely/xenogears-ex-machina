#!/usr/bin/env bash
# The headless runtime must build without the graphical stack (plan.md Phase 2):
# no wgpu, SDL, Slint, OpenXR or browser bindings in xem-headless's normal
# dependency tree.
set -euo pipefail
cd "$(dirname "$0")/.."
forbidden='^(wgpu|wgpu-hal|wgpu-core|naga|sdl3|sdl3-sys|slint|i-slint-[a-z-]+|openxr|openxr-sys|ash|web-sys|wasm-bindgen|winit|glow|png) '
tree=$(cargo tree -p xem-headless -e normal --prefix none --no-dedupe | sort -u)
if grep -E "$forbidden" <<<"$tree"; then
    echo "check-headless-deps: graphical dependencies above reach xem-headless" >&2
    exit 1
fi
echo "check-headless-deps: $(wc -l <<<"$tree") crates, none graphical"
