#!/usr/bin/env bash
# The test suite, from anywhere in the repository:
#
#   src/runtime/scripts/suite.sh [desktop|web|android|xr|full] [--force] [--base REF] [--threads N]
#
# Skip before starting Nix when no runtime inputs differ from the merge base
# with origin/master, including staged, unstaged and untracked files. --base
# selects another comparison ref; --force bypasses the check and runs every
# unit again. See AGENTS.md.
#
# While developing, run only the platform whose host you are changing
# (desktop for the native and shared code), and full once at the end, before
# review or a merge, when those inputs changed (AGENTS.md):
#
#   desktop  (the default) the game module, the port's unit tests (with the
#            platforming table's check on the user's extracted discs), the
#            Rust workspace's tests (the game's order from within the scene
#            on wgpu's GL backend too) and the validation run
#   web      the game module, the web build and the browser suites, the
#            browser tests that measure time (WebXR's among them) last
#   android  the game module, the APK and the emulator's smoke test, its
#            game again with the APK's arm64-v8a library (which the
#            emulator runs through its ARM translation), and the game's
#            arm64 build under qemu (arm64-smoke.sh)
#   xr       the game module, the native build and the OpenXR smoke test
#   full     all of them, and what CI runs (.github/workflows/ci.yml) as on
#            a clone without the user's discs: tools/repository/check.py,
#            which builds and tests the workspace (here without a game
#            module), and the matching toolchain's smoke test
#
# A run is made of units (suite.py): a build, one Rust test binary, one
# browser test file, a device's smoke test.
#
# A unit that passed is not run again while what it reads is what it read
# then: the binaries it runs, the files under the paths it names (tracked or
# not, as the working tree has them), its command, the pinned tools and the
# private inputs' sizes and times. The summary calls it `same` and names the
# pass it stands on; its log is that pass's. So a run costs what changed: a
# change to one crate builds and runs the test binaries that link it, and
# full after the platforms' tiers runs what they did not. A unit that failed
# always runs again, and --force runs everything (after changing what a
# private input holds, or the machine's drivers by hand).
#
# The run keeps no more processor threads busy than --threads allows (a
# thread for each core unless given: it is pinned to that many processors,
# which leaves each core's second thread, worth about a quarter of the first
# here, to whatever else the machine does) and no more memory than was
# available when it began; it makes way for what else the machine does,
# starting a unit beside others only while the machine still has that unit's
# memory to give and nothing else keeps its threads busy. Builds follow one
# another, each in a cargo target directory of its own
# (src/runtime/target-*, none of them play.sh's), while what is already
# built is tested beside them. Each Rust test binary runs its tests on a
# share of the threads in proportion to its work, so that they end together.
# What measures time runs alone, one after another, once the rest is done
# (their emulator and headset render on the processor: beside the browser
# suites the Android smoke's frame rate fell from 56 to 10 a second, the XR
# smoke's headset frames came late, and their checks failed): the Android
# smoke (with the APK's x86_64 library, then its game with the arm64-v8a
# one), the XR smoke, the browser tests that measure time (the headset's
# frame rate, rewind frame times, fast-forward's music, the stalled page, a
# resume read within the rewind's one-second interval, the Binaural mode's
# cost) and the Rust tests that do (a frame's latency, the desktop's window,
# the settings panel).
#
# The Rust tests draw on the machine's graphics card where scripts/
# xem-gpu-host finds one (the heaviest of them draw most, in a quarter of
# the processor time), but for those that compare pictures to the last bit,
# which ask for the software rasterizer (xem-render's Gpu::reference: Mesa's
# lavapipe, the same on every machine); without a card they all draw on it.
#
# Rust builds use the `suite` profile (Cargo.toml: release's optimization
# without link-time optimization or debug information); for a failing test's
# backtrace, build its target again with line tables (`--profile
# suite-debug`) and run it with RUST_BACKTRACE=1. The game module is built
# again only when its sources or the matched links changed.
#
# Logs: build/suite/<unit>.log, with the units' results and times in
# build/suite/summary.txt. The user's disc 1 (discs/"Xenogears disc 1.chd")
# enables what needs it. Exits non-zero when any unit failed or was left
# because one it needs did.
set -uo pipefail

root=$(cd "$(dirname "$0")/../../.." && pwd)
runtime=$root/src/runtime
tier=desktop
tier_given=false
force=false
comparison=origin/master
threads=()
usage() { echo "usage: $0 [desktop|web|android|xr|full] [--force] [--base REF] [--threads N]"; }
while (($#)); do
    case $1 in
    desktop | web | android | xr | full)
        if $tier_given; then usage >&2; exit 2; fi
        tier=$1
        tier_given=true
        shift ;;
    --force) force=true; shift ;;
    --base)
        if (($# < 2)) || [[ $2 == -* ]]; then usage >&2; exit 2; fi
        comparison=$2
        shift 2 ;;
    --threads)
        if (($# < 2)) || [[ ! $2 =~ ^[1-9][0-9]*$ ]]; then usage >&2; exit 2; fi
        threads=(--threads "$2")
        shift 2 ;;
    -h | --help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
    esac
done

# This is a conservative runtime boundary, not a second test/dependency
# registry. Any runtime change enables the chosen tier; standalone tooling
# (such as tools/release/) still gets its own focused tests.
if ! $force; then
    base=$(git -C "$root" merge-base HEAD "$comparison") || {
        echo "Cannot resolve suite base '$comparison'; fetch it, choose --base REF, or use --force." >&2
        exit 2
    }
    inputs=(src/decomp/ src/port/ src/runtime/ nix/runtime/ nix/ghidra/
        tools/game_module.py tools/game_schema.py tools/game_coverage.py tools/reference/
        tools/analysis/platforming.py 'tests/test_port_*.py' 'tests/test_game_*.py'
        tests/test_dispatch_tables.py tests/test_platforming.py tests/fixtures/
        ':(glob,exclude)**/*.md')
    changed=false
    git -C "$root" diff --quiet "$base" -- "${inputs[@]}" || {
        status=$?
        if ((status != 1)); then exit "$status"; fi
        changed=true
    }
    untracked=$(git -C "$root" ls-files --others --exclude-standard -- "${inputs[@]}") || exit 2
    if ! $changed && [[ -z $untracked ]]; then
        echo "Skipping $tier suite: no runtime inputs changed since $comparison's merge base ($base)."
        echo "Use --force to run anyway; use focused tests for changes outside the runtime."
        exit 0
    fi
fi

# Everything runs in the runtime shell (the Android stage enters its own).
nix=(nix --extra-experimental-features 'nix-command flakes' develop)
if [[ -z ${XEM_SUITE_SHELL:-} ]]; then
    arguments=("$tier" --base "$comparison")
    if $force; then arguments+=(--force); fi
    exec "${nix[@]}" "path:$root/nix/runtime" -c env XEM_SUITE_SHELL=1 "$0" "${arguments[@]}" "${threads[@]}"
fi

if $force; then threads+=(--force); fi
exec python3 "$runtime/scripts/suite.py" "$tier" "${threads[@]}"
