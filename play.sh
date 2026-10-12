#!/usr/bin/env bash
# Plays the game from this checkout in the desktop host.
#
#   ./play.sh [DISC] [xem-desktop options...]
#
# DISC defaults to "discs/Xenogears disc 1.chd" (CHD, cue or bin). Options go to
# xem-desktop, e.g. --cards DIR (memory cards; default: beside the settings
# file) or --bios FILE (the BIOS image's Kanji font). XEM_BIOS=FILE adds --bios.
#
# It builds what is missing or out of date, each step in its pinned Nix shell:
#   1. the matched PS1 builds (make -C src/decomp all-verify, a job per core;
#      needs the discs),
#   2. the game module and schema (tools/game_module.py),
#   3. xem-desktop (release).
# Then it runs on the host's NVIDIA driver when there is one
# (src/runtime/scripts/xem-gpu-host), else on the shell's software Vulkan.
#
# Keys (src/runtime/crates/xem-settings/src/game_keys.rs): WASD the D-pad;
# E or the left mouse button Cross (confirm), F or the right button Circle
# (cancel), Shift Circle held (run), Space Triangle (jump), Tab Square (menu);
# Left/Right L1/R1 (camera); 1/2 L2/R2; Enter Start; Backspace Select. Escape
# opens the settings panel. A gamepad works too (Back+Start: the panel).
set -euo pipefail

root=$(cd "$(dirname "$0")" && pwd)
cd "$root"
nix=(nix --extra-experimental-features 'nix-command flakes' develop)

disc="discs/Xenogears disc 1.chd"
if [[ $# -gt 0 && $1 != -* ]]; then
    disc=$1
    shift
fi
[[ -f $disc ]] || { echo "play.sh: no disc at $disc" >&2; exit 1; }
args=(--disc "$(realpath "$disc")" "$@")
[[ -n ${XEM_BIOS:-} ]] && args+=(--bios "$(realpath "$XEM_BIOS")")

# 1. The matched links give every object its original address.
if ! compgen -G ".local/decomp/build/*.elf" >/dev/null; then
    echo "play.sh: building the matched PS1 images (once; several minutes)"
    "${nix[@]}" path:./nix/ghidra#matching -c make -j"$(nproc)" -C src/decomp all-verify
fi

# 2. The game module, again whenever the decomp, the port or the tool changed
#    since its last build finished (its stamp; game.wasm keeps its time when
#    a build changes nothing).
module=build/game/game.wasm
stamp=build/game/stamp
if [[ ! -f $module || ! -f $stamp ]] || [[ -n $(find src/decomp/src src/decomp/include src/port tools/game_module.py \
    tools/game_schema.py -newer "$stamp" -type f -print -quit) ]]; then
    echo "play.sh: building the game module"
    "${nix[@]}" path:./nix/runtime -c python3 tools/game_module.py --stubs
fi

# 3. The desktop host (cargo rebuilds only what changed).
"${nix[@]}" path:./nix/runtime -c bash -c 'cd src/runtime && cargo build --release -p xem-desktop'

desktop=src/runtime/target/release/xem-desktop
if [[ -f ${XEM_HOST_ICD:-/usr/share/vulkan/icd.d/nvidia_icd.json} ]]; then
    exec "${nix[@]}" path:./nix/runtime -c src/runtime/scripts/xem-gpu-host "$desktop" "${args[@]}"
fi
echo "play.sh: no NVIDIA driver found; rendering on the software Vulkan driver"
exec "${nix[@]}" path:./nix/runtime -c "$desktop" "${args[@]}"
