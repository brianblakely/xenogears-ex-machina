#!/usr/bin/env bash
# The test suite, from anywhere in the repository:
#
#   src/runtime/scripts/suite.sh [desktop|web|android|xr|full] [--force] [--base REF]
#
# Skip before starting Nix when no runtime inputs differ from the merge base
# with origin/master, including staged, unstaged and untracked files. --base
# selects another comparison ref; --force bypasses the check. See AGENTS.md.
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
# In full, the builds run side by side, each in a cargo target directory of
# its own (src/runtime/target-*, none of them play.sh's): the native one,
# then the Rust tests and the validation run at once; CI's checks; the web
# build, then the browser suites (the slice's, the longest, from the start
# beside the others two files at a time; the slice's tests two at a time
# within it: at most four browsers); the Android APK,
# the emulator boots (its boot measures nothing), and the arm64 build plays
# the slice's logs under qemu (measuring nothing either). Then what measures
# frame rates runs alone, one after another (their emulator, headset and
# browsers render on the processor: beside the browser suites the Android
# smoke's frame rate fell from 56 to 10 a second, the XR smoke's headset
# frames came late, and their checks failed): the Android smoke (with the
# APK's x86_64 library, then its game with the arm64-v8a one), the XR
# smoke, and the browser tests that measure time (the headset's frame rate,
# rewind frame times, fast-forward's music, the stalled page, a resume read
# within the rewind's one-second interval, the Binaural mode's cost).
#
# Rust builds use the `suite` profile (Cargo.toml: release's optimization
# without link-time optimization or debug information); a failing test
# target is built again with line tables (`suite-debug`) and run with a
# backtrace into rust-debug.log. The game module keeps its file when a build
# changes nothing, so nothing downstream rebuilds.
#
# Logs: build/suite/<stage>.log, with the stages' results and times in
# build/suite/summary.txt. The user's disc 1 (discs/"Xenogears disc 1.chd")
# enables what needs it. Exits non-zero when any stage failed.
set -uo pipefail

root=$(cd "$(dirname "$0")/../../.." && pwd)
runtime=$root/src/runtime
tier=desktop
tier_given=false
force=false
comparison=origin/master
usage() { echo "usage: $0 [desktop|web|android|xr|full] [--force] [--base REF]"; }
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
    exec "${nix[@]}" "path:$root/nix/runtime" -c env XEM_SUITE_SHELL=1 "$0" "${arguments[@]}"
fi

logs=$root/build/suite
rm -rf "$logs"
mkdir -p "$logs"
# The Rust disc tests read the raw image (.local/discs/disc1.bin); every
# other stage finds discs/"Xenogears disc 1.chd" itself.
unset XEM_DISC1
disc="$root/discs/Xenogears disc 1.chd"
# The native builds' own directory; the stages beside them set theirs.
export CARGO_TARGET_DIR=$runtime/target-suite
headless=$CARGO_TARGET_DIR/suite/xem-headless
# Cargo builds side by side would each start a compiler per core: the web
# build (the browser suites wait for it) takes half the cores, the others a
# quarter.
jobs=$(nproc)
web_jobs=$((jobs > 1 ? jobs / 2 : 1))
side_jobs=$((jobs > 3 ? jobs / 4 : 1))
began=$(date +%s)

# stage NAME COMMAND...: run COMMAND into NAME.log and note its result.
stage() {
    local name=$1 start=$(date +%s) result=pass
    shift
    "$@" >"$logs/$name.log" 2>&1 || result=FAIL
    echo "$result $(($(date +%s) - start))" >"$logs/$name.result"
    [[ $result == pass ]]
}

module() {
    cd "$root" && python3 tools/game_module.py --stubs
}

port() {
    cd "$root" && python3 -m unittest tests.test_port_model_draw tests.test_port_gte tests.test_port_gpu \
        tests.test_game_module_pause tests.test_port_original_traces tests.test_port_resident tests.test_port_arena \
        tests.test_port_worldmap tests.test_port_presentation tests.test_port_controls tests.test_port_disc tests.test_dispatch_tables \
        tests.test_platforming tests.test_port_encounters tests.test_port_text_speed tests.test_port_movie_skip \
        tests.test_game_coverage tests.test_game_module_memory || return
    # The platforming table's proof on the user's discs: xem-core's
    # PLATFORMING_AREAS is the classifier's (about 33 s); it reads both discs
    # extracted and their images under .local, and is skipped without them.
    local number
    for number in 1 2; do
        if [[ ! -f $root/.local/extract/disc$number/manifest.json || ! -f $root/.local/discs/disc$number.bin ]]; then
            echo "skipping the platforming table's check: no disc $number (.local/extract/disc$number, .local/discs/disc$number.bin)"
            return 0
        fi
    done
    python3 -m tools.analysis.platforming --check
}

# What the native stages run: the tests, the validation example and the
# hosts (the browser suites compare with xem-headless, and with the
# first_person example's first-person listener).
native() {
    cd "$runtime" && cargo test --profile suite --workspace --no-run &&
        cargo build --profile suite -p xem-headless -p xem-desktop --bins --example validation --example first_person
}

# The workspace's tests; each failing target again with line tables. Then
# the game's order from within the scene again on wgpu's GL backend (Mesa's
# llvmpipe through EGL), whose clip space WebGL2 and GLES draw through.
rust() {
    cd "$runtime" || return 1
    local status=0
    VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD cargo test --profile suite --workspace --no-fail-fast || status=1
    if ((status)); then
        grep -o 'to rerun pass `[^`]*`' "$logs/rust.log" | sed 's/.*`\(.*\)`/\1/' | sort -u | while read -r target; do
            echo "== $target"
            # shellcheck disable=SC2086 # the target's own arguments
            VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD RUST_BACKTRACE=1 cargo test --profile suite-debug $target
        done >"$logs/rust-debug.log" 2>&1
    fi
    WGPU_BACKEND=gl __EGL_VENDOR_LIBRARY_FILENAMES=$XEM_MESA_EGL LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        cargo test --profile suite --workspace --test ps1 --test first_person -- \
        from_within_a_face_drawn_over_another from_within_the_games_order || status=1
    return $status
}

validation() {
    cd "$runtime" && "$CARGO_TARGET_DIR/suite/examples/validation"
}

# CI's two steps, without a game module (none at that path) as on a clone
# without the user's discs.
ci() {
    cd "$root" && CARGO_TARGET_DIR=$runtime/target-public CARGO_BUILD_JOBS=$side_jobs \
        XEM_GAME_WASM=/nonexistent/game.wasm python3 tools/repository/check.py &&
        "${nix[@]}" "path:$root/nix/ghidra#matching" -c make -C src/decomp smoke
}

web() {
    cd "$root" && CARGO_TARGET_DIR=$runtime/target-web CARGO_BUILD_JOBS=$web_jobs python3 src/runtime/web/build.py
}

# The browser suites but the timed tests, once the native build has made
# the host they compare with: the slice's (the longest) from the start, the
# others two files at a time (node runs them in name order).
timed='rewind frame p99|keep music on host time|rewind uses elapsed seconds during FF|the configured history length expires game saves by elapsed play time|drops missed time|Start pause freezes rewind history|phone-class processor'
browser() {
    until [[ -f $logs/native.result ]]; do sleep 1; done
    grep -q '^pass' "$logs/native.result" || return 1
    cd "$runtime/web" || return 1
    local status=0
    XEM_HEADLESS=$headless node --test tests/slice.test.mjs >"$logs/browser-slice.log" 2>&1 &
    local slice=$!
    XEM_HEADLESS=$headless node --test --test-concurrency=2 --test-skip-pattern="$timed" \
        tests/binaural.test.mjs tests/disc.test.mjs tests/first-person.test.mjs tests/listening.test.mjs tests/page.test.mjs \
        tests/persistence.test.mjs tests/render.test.mjs tests/sound.test.mjs tests/xr-binaural.test.mjs tests/xr-hands.test.mjs || status=1
    wait "$slice" || status=1
    return $status
}

# The timed tests, one at a time.
timing() {
    cd "$runtime/web" || return 1
    local status=0 pattern
    while read -r file pattern; do
        echo "== $file: $pattern"
        XEM_HEADLESS=$headless node --test --test-name-pattern="$pattern" "tests/$file" || status=1
    done <<'TESTS'
persistence.test.mjs rewind frame p99
persistence.test.mjs keep music on host time
persistence.test.mjs rewind uses elapsed seconds during FF
persistence.test.mjs the configured history length expires game saves by elapsed play time
persistence.test.mjs Start pause freezes rewind history
page.test.mjs drops missed time
binaural.test.mjs phone-class processor
xr-game.test.mjs .
xr-diorama.test.mjs .
TESTS
    return $status
}

xr() {
    cd "$root" && XEM_CARGO_PROFILE=suite XEM_SKIP_BUILD=1 src/runtime/scripts/xr-game-smoke.sh
}

# The Android stages, in the Android shell and their own target directory.
in_android() {
    cd "$root" && "${nix[@]}" "path:$root/nix/runtime#android" -c env CARGO_TARGET_DIR="$runtime/target-android" \
        CARGO_BUILD_JOBS="$side_jobs" "$@"
}
android_build() {
    in_android env XEM_ANDROID_ABIS="x86_64 arm64-v8a" src/runtime/scripts/android-build.sh
}
android_boot() {
    in_android env XEM_SMOKE_BOOT=1 src/runtime/scripts/android-smoke.sh
}
android() {
    # The smoke compares states with xem-headless (the native stage's build,
    # or one of its own in the android tier).
    (cd "$runtime" && cargo build --profile suite -p xem-headless) &&
        in_android env XEM_SKIP_BUILD=1 XEM_HEADLESS="$headless" src/runtime/scripts/android-smoke.sh
}
# The smoke's game (its state interchange among it) with the APK's
# arm64-v8a library, which the emulator's x86_64 image runs through its ARM
# translation.
android_arm64() {
    (cd "$runtime" && cargo build --profile suite -p xem-headless) &&
        in_android env XEM_SKIP_BUILD=1 XEM_SMOKE_ABI=arm64-v8a XEM_SMOKE_GAME_ONLY=1 XEM_HEADLESS="$headless" \
            src/runtime/scripts/android-smoke.sh
}
# The arm64 build under qemu compares with xem-headless too, and builds in
# a target directory of its own.
arm64() {
    (cd "$runtime" && cargo build --profile suite -p xem-headless) &&
        in_android env CARGO_TARGET_DIR="$runtime/target-arm64" XEM_HEADLESS="$headless" src/runtime/scripts/arm64-smoke.sh
}

summary() {
    {
        for result in "$logs"/*.result; do
            name=$(basename "$result" .result)
            read -r outcome seconds <"$result"
            printf '%-10s %-4s %4d s\n' "$name" "$outcome" "$seconds"
        done
        echo "$tier suite: $(($(date +%s) - began)) s"
    } | tee "$logs/summary.txt"
    ! grep -q '^FAIL' "$logs"/*.result
}

passed() { grep -q '^pass' "$logs/$1.result" 2>/dev/null; }
# covers PLATFORM...: whether this tier runs one of their stages.
covers() { [[ $tier == full || " $* " == *" $tier "* ]]; }

stage module module || { summary; exit 1; }
covers desktop && stage port port &
if covers desktop web xr; then
    {
        stage native native || exit
        if covers desktop; then
            stage rust rust &
            [[ ! -f $disc ]] || stage validation validation &
            wait
        fi
    } &
fi
[[ $tier != full ]] || stage ci ci &
covers web && { stage web web && stage browser browser; } &
if covers android; then
    stage android-build android_build &
    stage android-boot android_boot &
    stage arm64 arm64 &
fi
wait
if covers android; then
    if passed android-build; then
        stage android android
        stage android-arm64 android_arm64
    elif passed android-boot; then
        # No APK to test: stop the emulator booted for it (android-smoke.sh's port).
        in_android adb -s emulator-5584 emu kill >/dev/null 2>&1
    fi
fi
! covers xr || ! passed native || [[ ! -f $disc ]] || stage xr xr
# The timed tests need the web build the browser suites ran on.
! covers web || ! passed web || ! passed native || stage timing timing
summary
