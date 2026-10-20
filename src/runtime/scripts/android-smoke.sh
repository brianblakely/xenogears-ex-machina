#!/usr/bin/env bash
# Android smoke test on the emulator (no device needed):
#
#   nix develop path:./nix/runtime#android -c src/runtime/scripts/android-smoke.sh
#
# Builds the x86_64 APK (XEM_SKIP_BUILD=1 reuses it) while it boots a headless
# API-34 AVD (swiftshader_indirect: guest Vulkan on SwiftShader; XEM_SMOKE_BOOT=1
# only boots it, for a later run to use) kept under
# .local/android, installs the app fresh and checks from logcat and
# screenshots (build/android/smoke/*.png): first frame, bundled asset,
# Vulkan device, a settings change by touch, disc import through the system
# document picker (a synthetic MODE2 image; the user's disc 1 CHD too when
# XEM_DISC1 or discs/"Xenogears disc 1.chd" exists), background and return
# (surface dropped and recreated, simulation time not advanced), low memory,
# rotation, activity recreation (teardown and a new SDL_main run) and restart
# (settings and the persisted document grant survive). With the user's disc
# 1 the game boots from it, taking the Kanji font of a BIOS image made here
# where Import BIOS keeps one (files/bios.bin), and plays for ~25 s
# (screenshots), with its frame rate measured and its opening movie shown
# rebuilt (a frame logged with movie=1), a pad press, a touch on the control
# deck, and a background and return during which the
# game's frame count does not move, plus repeated Home/return at 20x FF; then
# Settings by touch: Stop fast-forward, Hold to rewind held (the game's
# vblank goes back), Save state and, after the game played on, Load state
# (the vblank and memory hash the save left), and Start fast-forward. With
# xem-headless (XEM_HEADLESS, else the suite's build) a state it made in
# field 30 loads on the device through Load state's key (F8) and plays on
# 600 frames to xem-headless's memory hash, and the device's Save state
# (F5) there, pulled, is xem-headless's state at that frame byte for byte,
# loads natively to that RAM and goes on to where xem-headless's own goes.
# XEM_SMOKE_GAME_ONLY=1 runs only the
# start and the game. XEM_SMOKE_ABI=arm64-v8a builds and installs the APK's
# arm64-v8a library instead, which the AVD's x86_64 image runs through its
# ARM translation (libndk_translation; the start checks the app's process
# runs it). XEM_SMOKE_SLICE=1 then plays the Phase 3 slice's session
# logs (tests/reference-inputs/slice-*.json) to their native memory hashes:
# the forest route with a background and return on the way, the save to card
# 2, and the load of that card in a fresh process. Touch coordinates are those of the AVD's pixel_6
# profile in portrait.
set -euo pipefail

root=$(cd "$(dirname "$0")/../../.." && pwd)
out=$root/build/android/smoke
mkdir -p "$out"
export ANDROID_USER_HOME=$root/.local/android/home
export ANDROID_AVD_HOME=$ANDROID_USER_HOME/avd
export ANDROID_EMULATOR_HOME=$ANDROID_USER_HOME
port=5584
serial=emulator-$port
adb=(adb -s "$serial")
apk=$root/build/android/gradle/app/outputs/apk/debug/xem-debug.apk
package=dev.xem.app
activity=$package/.XemActivity
disc1=${XEM_DISC1:-$root/discs/Xenogears disc 1.chd}
abi=${XEM_SMOKE_ABI:-x86_64}

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

# The emulator boots while the APK and the synthetic disc build; a watcher
# notes when its boot completes. XEM_SMOKE_BOOT=1 only boots it and leaves it
# running: a later run finds it booted (its boot noted) and uses it.
if [ ! -f "$ANDROID_AVD_HOME/xem-smoke.avd/config.ini" ]; then
    mkdir -p "$ANDROID_AVD_HOME"
    echo no | avdmanager create avd -n xem-smoke -d pixel_6 \
        -k "system-images;android-34;google_apis;x86_64" >/dev/null
fi
booted=$out/booted-at
emulator_pid=
watcher_pid=
if [ -f "$booted" ] && [ "$("${adb[@]}" get-state 2>/dev/null)" = device ]; then
    echo "using the emulator booted before"
else
    rm -f "$booted"
    emulator -avd xem-smoke -port "$port" -no-window -no-audio -no-boot-anim -no-snapshot \
        -gpu "${XEM_EMULATOR_GPU:-swiftshader_indirect}" >"$out/emulator.log" 2>&1 &
    emulator_pid=$!
    (
        "${adb[@]}" wait-for-device
        for _ in $(seq 150); do
            [ "$("${adb[@]}" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && date +%s >"$booted" && exit 0
            sleep 2
        done
    ) &
    watcher_pid=$!
fi
logcat_pid=
cleanup() {
    [ -z "$watcher_pid" ] || kill "$watcher_pid" 2>/dev/null || true
    [ -n "$logcat_pid" ] && kill "$logcat_pid" 2>/dev/null
    # The emulator stops, whichever run booted it, and is gone before this
    # run ends: a run right after boots its own (it took the one shutting
    # down for booted, and found it offline).
    rm -f "$booted"
    "${adb[@]}" emu kill >/dev/null 2>&1 || { [ -n "$emulator_pid" ] && kill "$emulator_pid" 2>/dev/null; } || true
    [ -z "$emulator_pid" ] || wait "$emulator_pid" 2>/dev/null || true
    for _ in $(seq 60); do
        pgrep -f -- "-avd xem-smoke -port $port" >/dev/null || break
        sleep 1
    done
}
if [ -n "${XEM_SMOKE_BOOT:-}" ]; then
    [ -z "$watcher_pid" ] || wait "$watcher_pid" || true
    [ -f "$booted" ] || { cleanup; fail "emulator did not boot"; }
    echo "emulator booted; left running for the smoke test"
    exit 0
fi
trap cleanup EXIT

if [ -z "${XEM_SKIP_BUILD:-}" ]; then
    XEM_ANDROID_ABIS=$abi "$root/src/runtime/scripts/android-build.sh"
fi
[ -f "$apk" ] || fail "no APK at $apk"

# The synthetic disc: an unknown PlayStation disc whose boot program's digest we know.
synthetic=$out/xem-synthetic.bin
synthetic_sha=$(cd "$root/src/runtime" && cargo run -q -p xem-disc --example synthetic-disc -- "$synthetic")

[ -z "$watcher_pid" ] || wait "$watcher_pid" || true
[ -f "$booted" ] || fail "emulator did not boot"
echo "emulator booted: $("${adb[@]}" shell getprop ro.build.fingerprint | tr -d '\r')"
# Right after boot the system still changes app configurations (asset
# paths), which recreates activities; let that settle for 20 s from the boot
# (the build may have taken them already).
settle=$(( $(cat "$booted") + 20 - $(date +%s) ))
[ "$settle" -gt 0 ] && sleep "$settle"
# No "viewing full screen" hint over the app; portrait to start.
"${adb[@]}" shell settings put secure immersive_mode_confirmations confirmed
"${adb[@]}" shell settings put system accelerometer_rotation 0
"${adb[@]}" shell settings put system user_rotation 0
# The AVD keeps its data between runs: undo what a previous run may have left.
"${adb[@]}" shell settings put global always_finish_activities 0
# No "isn't responding" dialog over the app when the system UI is slow on a
# loaded host (it took the taps); the app's own stalls still fail the checks.
"${adb[@]}" shell settings put global hide_error_dialogs 1
"${adb[@]}" shell wm density reset
for _ in $(seq 15); do
    [[ $("${adb[@]}" shell dumpsys window displays) == *"mRotation=0 "* ]] && break
    sleep 1
done
[[ $("${adb[@]}" shell dumpsys window displays) == *"mRotation=0 "* ]] || fail "display not in portrait"

"${adb[@]}" uninstall "$package" >/dev/null 2>&1 || true
"${adb[@]}" install --abi "$abi" "$apk" >/dev/null
"${adb[@]}" push "$synthetic" /sdcard/Download/xem-synthetic.bin >/dev/null

log=$out/logcat.txt
"${adb[@]}" logcat -c
"${adb[@]}" logcat -v time >"$log" &
logcat_pid=$!

mark=1
mark() { mark=$(($(wc -l <"$log") + 1)); }
# wait_for REGEX [SECONDS]: the first matching line logged since the last mark.
wait_for() {
    local line
    for _ in $(seq "${2:-30}"); do
        line=$(RE=$1 awk -v start="$mark" 'NR >= start && $0 ~ ENVIRON["RE"] { print; exit }' "$log")
        if [ -n "$line" ]; then
            echo "  $line"
            return 0
        fi
        sleep 1
    done
    fail "no log line matching: $1"
}
shot() {
    "${adb[@]}" exec-out screencap -p >"$out/$1.png"
    echo "  screenshot $out/$1.png"
}
tap() { "${adb[@]}" shell input tap "$1" "$2"; }
# pick_document LABEL REGEX: chooses the document whose name matches REGEX in
# the system document picker (DocumentsUI), via Downloads, until it closes.
pick_document() {
    local xy tapped=
    for _ in $(seq 30); do
        "${adb[@]}" shell uiautomator dump /sdcard/ui.xml >/dev/null 2>&1 || true
        "${adb[@]}" exec-out cat /sdcard/ui.xml >"$out/ui.xml"
        if [[ $(<"$out/ui.xml") != *documentsui* ]]; then
            [ -n "$tapped" ] && return 0
        elif xy=$(python3 "$root/src/runtime/scripts/android-ui.py" "$out/ui.xml" "$2"); then
            [ -n "$tapped" ] || shot "picker-$1"
            tap $xy
            tapped=1
        elif xy=$(python3 "$root/src/runtime/scripts/android-ui.py" "$out/ui.xml" "Downloads"); then
            tap $xy
        elif xy=$(python3 "$root/src/runtime/scripts/android-ui.py" "$out/ui.xml" "Show roots"); then
            tap $xy
        fi
        sleep 2
    done
    fail "document $2 not chosen in the picker"
}
sim_of() { sed -E 's/.*sim=([0-9.]+)s.*/\1/'; }

echo "== start"
mark
"${adb[@]}" shell am start -n "$activity" >/dev/null
wait_for "xem: bundled asset xem-asset-check.txt: 130 bytes"
wait_for "xem: gpu .*\((Vulkan|Gl)," 60
wait_for "xem: settings .*revision 0"
wait_for "xem: frame rendered frame=1 " 60
wait_for "xem: frame rendered frame=120 " 60
shot 01-started
# The library the app's process runs: the package's ABI, and the ARM
# translation's linker for arm64 libraries (with its arm64 system libraries)
# mapped in the process only when that is arm64-v8a.
pid=$("${adb[@]}" shell pidof "$package" | tr -d '\r')
"${adb[@]}" exec-out run-as "$package" cat "/proc/$pid/maps" >"$out/maps.txt"
installed=$("${adb[@]}" shell dumpsys package "$package" | sed -n -E 's/.*primaryCpuAbi=([^ ]+).*/\1/p' | head -1 | tr -d '\r')
translated=$(awk '$NF ~ "^/system/lib64/arm64/" { print $NF }' "$out/maps.txt" | sort -u | wc -l)
[ "$installed" = "$abi" ] || fail "the app was installed with its $installed library, not $abi"
if [ "$abi" = x86_64 ]; then
    ! grep -q '/system/bin/arm64/linker64' "$out/maps.txt" || fail "the x86_64 app's process runs arm64 code"
    echo "  the app runs its x86_64 library"
else
    grep -q '/system/bin/arm64/linker64' "$out/maps.txt" || fail "the app's process runs no arm64 code through the ARM translation"
    [ "$translated" -gt 0 ] || fail "the app's process maps the ARM translation's linker but none of its arm64 system libraries"
    echo "  the app runs its $installed library through the ARM translation ($translated of its arm64 system libraries mapped)"
fi

if [ -z "${XEM_SMOKE_GAME_ONLY:-}" ]; then
# The panel (1064x1470 at 2.625, at 8,465 on the 1080x2400 screen) shows
# its nine sections as a strip of tabs: Graphics at 210,722, Saves at
# 659,722. xem-ui's panel test checks both taps at this size and scale.
echo "== settings change by touch (Show FPS)"
mark
tap 210 722
sleep 1
shot 01-graphics
# The compact Graphics card has no heading/help rows; its Show FPS switch
# is centred here in 01-graphics.png (the old position hit Resolution).
tap 930 1350
# Check the operation itself. During first-boot log traffic the setting was
# saved and survived restart, but logcat missed its one-line acknowledgement.
fps_saved=
for _ in $(seq 30); do
    if "${adb[@]}" exec-out run-as "$package" cat files/settings.json |
        python3 -c 'import json, sys; sys.exit(0 if json.load(sys.stdin)["presentation"]["show_fps"] is True else 1)'; then
        fps_saved=1
        break
    fi
    sleep 1
done
[ -n "$fps_saved" ] || fail "Show FPS was not enabled and saved by touch"
echo "  Show FPS enabled and saved by touch"

echo "== disc import through the document picker (Import disc... button)"
mark
tap 659 722
sleep 1
shot 01-saves
# Import disc is now the first row directly below the tabs.
tap 858 915
wait_for "xem: disc picker opened"
pick_document synthetic "xem-synthetic\.bin"
wait_for "I/xem.*persisted read permission for content://"
wait_for "xem: disc identified source=content://.*boot=SLUS_999\.99;1 sha256=$synthetic_sha disc=not a Xenogears disc" 60
wait_for "xem: disc import remembered content://"
wait_for "xem: frame rendered"
sleep 2
shot 02-imported

echo "== Back hides the settings panel (scene only), Back shows it again"
"${adb[@]}" shell input keyevent KEYCODE_BACK
sleep 2
shot 03-panel-hidden
"${adb[@]}" shell input keyevent KEYCODE_BACK

echo "== background and return"
mark
"${adb[@]}" shell input keyevent KEYCODE_HOME
background=$(wait_for "xem: lifecycle WILL_ENTER_BACKGROUND")
echo "$background"
wait_for "xem: surface dropped"
wait_for "xem: lifecycle DID_ENTER_BACKGROUND"
sleep 8
shot 04-home
mark
"${adb[@]}" shell am start -n "$activity" >/dev/null
wait_for "xem: lifecycle WILL_ENTER_FOREGROUND"
wait_for "xem: lifecycle DID_ENTER_FOREGROUND"
wait_for "xem: surface recreated"
resumed=$(wait_for "xem: frame rendered")
echo "$resumed"
before=$(echo "$background" | sim_of)
after=$(echo "$resumed" | sim_of)
python3 -c "import sys; d = $after - $before; print(f'  simulation time across ~10 s in the background: +{d:.3f} s'); sys.exit(0 if 0 <= d < 0.5 else 1)" ||
    fail "simulation time jumped from $before to $after"
sleep 2
shot 05-resumed

echo "== low memory"
mark
"${adb[@]}" shell am send-trim-memory "$package" RUNNING_CRITICAL
wait_for "xem: lifecycle LOW_MEMORY"

echo "== rotation"
mark
# Rotation locked (set again: the settings just after boot may not take), then turned.
"${adb[@]}" shell settings put system accelerometer_rotation 0
"${adb[@]}" shell settings put system user_rotation 1
wait_for "xem: resized 2400x1080"
sleep 3
shot 06-landscape
mark
"${adb[@]}" shell settings put system user_rotation 0
wait_for "xem: resized 1080x2400"

echo "== activity recreation (display density change): SDL_main runs again in the process"
# A density change recreates the activity (the launcher comes to the front
# as it restarts too); returning to the app runs SDL_main again in the same
# process, which reopens the imported disc from its persisted grant.
for density in 480 reset; do
    mark
    "${adb[@]}" shell wm density "$density"
    wait_for "xem: lifecycle TERMINATING"
    wait_for "xem: exit"
    # The system recreates activities more than once; let it settle.
    sleep 8
    "${adb[@]}" shell am start -n "$activity" >/dev/null
    wait_for "xem: start \(run [2-9] in this process\)"
    wait_for "xem: reopening imported disc content://"
    wait_for "xem: frame rendered frame=120 " 90
done

echo "== restart: settings and the imported document persist"
mark
"${adb[@]}" shell am force-stop "$package"
"${adb[@]}" shell am start -n "$activity" >/dev/null
wait_for "xem: settings .*: .*show_fps: true"
wait_for "xem: reopening imported disc content://"
wait_for "xem: disc identified source=content://.*sha256=$synthetic_sha" 60
wait_for "xem: frame rendered frame=1 " 60
fi

vblank_of() { sed -E 's/.*game_vblank=([0-9]+).*/\1/'; }
if [ -f "$disc1" ]; then
    echo "== the user's disc 1 (local only; removed from the emulator afterwards): the game"
    "${adb[@]}" push "$disc1" /sdcard/Download/xem-disc1.chd >/dev/null
    mark
    game_start=$mark
    "${adb[@]}" shell am force-stop "$package"
    # Configure only this disposable smoke installation before its restart.
    if "${adb[@]}" shell run-as "$package" test -f files/settings.json; then
        "${adb[@]}" exec-out run-as "$package" cat files/settings.json >"$out/ff-settings.json"
    else
        echo '{}' >"$out/ff-settings.json"
    fi
    python3 - "$out/ff-settings.json" <<'PY'
import json
import sys
from pathlib import Path
path = Path(sys.argv[1])
settings = json.loads(path.read_text())
settings["fast_forward_rate"] = {"Fixed": 20}
path.write_text(json.dumps(settings))
PY
    "${adb[@]}" push "$out/ff-settings.json" /data/local/tmp/xem-ff-settings.json >/dev/null
    "${adb[@]}" shell run-as "$package" cp /data/local/tmp/xem-ff-settings.json files/settings.json
    "${adb[@]}" shell rm /data/local/tmp/xem-ff-settings.json
    # A BIOS image where Import BIOS keeps it, whose font the boot takes: a
    # plausible font made here (glyphs 147-208, the full-width digits and
    # Latin letters, each different), no original content.
    python3 - "$out/bios.bin" <<'PY'
import sys
rom = bytearray(0x80000)
for glyph in range(147, 209):
    rom[0x66000 + glyph * 30 + 2] = glyph - 146
open(sys.argv[1], "wb").write(rom)
PY
    "${adb[@]}" push "$out/bios.bin" /data/local/tmp/xem-bios.bin >/dev/null
    "${adb[@]}" shell run-as "$package" cp /data/local/tmp/xem-bios.bin files/bios.bin
    "${adb[@]}" shell rm /data/local/tmp/xem-bios.bin
    "${adb[@]}" shell am start -n "$activity" --esa xem.args --pick-disc >/dev/null
    wait_for "xem: settings .*fast_forward_rate: Fixed\(20\)" 60
    wait_for "xem: disc picker opened" 60
    pick_document disc1 "xem-disc1\.chd"
    wait_for "xem: disc identified source=content://.*boot=SLUS_006\.64;1 sha256=dc0b2dd7[0-9a-f]* disc=Xenogears disc 1 \(SLUS-00664\)" 180
    game_booted=$(wait_for "xem: game booted source=content://.*boot=SLUS_006\.64;1" 60)
    echo "$game_booted"
    [[ $game_booted == *"audio=stream opened"* ]] || fail "no audio stream for the game"
    [[ $game_booted == *" font=bios "* ]] || fail "the boot did not take the stored BIOS image's font"
    # Later boots (the slice's) without it, as natively.
    "${adb[@]}" shell run-as "$package" rm files/bios.bin
    wait_for "xem: frame rendered .*game_vblank=" 60
    for at in 03 08 15 25; do
        sleep $((10#$at - ${previous:-0}))
        previous=$((10#$at))
        shot "07-game-${at}s"
    done
    grep -E "xem: (fps|frame rendered .*game_vblank)" "$log" | tail -12 | sed 's/^/  /'
    # The opening movie (game frames 401-1325) shown rebuilt from what the
    # MDEC decoded (the Enhanced movies setting): while the shots ran, or
    # soon after on a slow emulator (before Start skips it below).
    wait_for "xem: frame rendered .*game_vblank=.* movie=1" 60
    # Game memory is one 2 GiB linear-memory mapping, committed as touched.
    pid=$("${adb[@]}" shell pidof "$package" | tr -d '\r')
    echo "  memory: $("${adb[@]}" shell grep -E "'^(VmSize|VmRSS)'" "/proc/$pid/status" | tr -s ' \t\r\n' ' ')" \
        "$("${adb[@]}" shell dumpsys meminfo "$package" | grep -E 'TOTAL PSS' | tr -s ' ' | tr -d '\r')"

    echo "== a pad press (Enter is Start)"
    mark
    "${adb[@]}" shell input keyevent --longpress KEYCODE_ENTER
    wait_for "xem: pad buttons 0x0008 "
    wait_for "xem: pad buttons 0x0000 "
    sleep 3
    shot 08-game-start-pressed

    echo "== the touch deck: a finger held on Confirm holds Cross"
    # Logged once the game shows (before this step's mark): the latest place.
    deck=$(grep "xem: touch deck Confirm " "$log" | tail -1)
    [ -n "$deck" ] || fail "no touch deck logged"
    read -r x y w h <<<"$(sed -E 's/.*touch deck Confirm ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+).*/\1 \2 \3 \4/' <<<"$deck")"
    mark
    "${adb[@]}" shell input swipe $((x + w / 2)) $((y + h / 2)) $((x + w / 2)) $((y + h / 2)) 800
    wait_for "xem: pad buttons 0x4000 "
    wait_for "xem: pad buttons 0x0000 "
    shot 08-game-touch-deck

    echo "== background and return: the game's clock stops"
    mark
    "${adb[@]}" shell input keyevent KEYCODE_HOME
    background=$(wait_for "xem: lifecycle WILL_ENTER_BACKGROUND .*game_vblank=")
    echo "$background"
    wait_for "xem: game paused"
    wait_for "xem: lifecycle DID_ENTER_BACKGROUND"
    sleep 10
    mark
    "${adb[@]}" shell am start -n "$activity" >/dev/null
    foreground=$(wait_for "xem: lifecycle DID_ENTER_FOREGROUND .*game_vblank=")
    echo "$foreground"
    wait_for "xem: game resumed"
    resumed=$(wait_for "xem: frame rendered .*game_vblank=")
    echo "$resumed"
    before=$(echo "$background" | vblank_of)
    during=$(echo "$foreground" | vblank_of)
    after=$(echo "$resumed" | vblank_of)
    echo "  game vblank: $before at background, $during at foreground, $after at the first frame back"
    [ "$during" = "$before" ] || fail "the game ran in the background ($before -> $during)"
    [ $((after - during)) -le 2 ] || fail "a burst of $((after - during)) game frames on return"
    sleep 5
    shot 09-game-resumed
    wait_for "xem: fps rendered=.* game=" 15

    echo "== repeated Home during 20x fast-forward"
    # The game boot hides Settings; press the default keyboard FF binding.
    "${adb[@]}" shell input keyevent --longpress KEYCODE_GRAVE
    wait_for "xem: frame rendered .*fast_forward=true" 30
    for attempt in $(seq 6); do
        sleep 0.2
        mark
        "${adb[@]}" shell input keyevent KEYCODE_HOME
        background=$(wait_for "xem: lifecycle WILL_ENTER_BACKGROUND .*game_vblank=.*fast_forward=true")
        wait_for "xem: game paused"
        wait_for "xem: surface dropped"
        wait_for "xem: lifecycle DID_ENTER_BACKGROUND"
        sleep 1
        mark
        "${adb[@]}" shell am start -n "$activity" >/dev/null
        foreground=$(wait_for "xem: lifecycle DID_ENTER_FOREGROUND .*game_vblank=")
        wait_for "xem: game resumed"
        wait_for "xem: surface recreated"
        wait_for "xem: frame rendered .*game_vblank=.*fast_forward=true"
        before=$(echo "$background" | vblank_of)
        during=$(echo "$foreground" | vblank_of)
        [ "$before" = "$during" ] || fail "FF game advanced in the background ($before -> $during)"
        echo "  FF lifecycle round $attempt handled at vblank $before; rendering resumed"
    done
    if tail -n "+$game_start" "$log" | grep -E "lifecycle event .* while rendering ignored"; then
        fail "Android lifecycle event was pumped while the live host state was borrowed"
    fi
    shot 09-fast-forward-resumed

    echo "== Settings' time controls and save states by touch"
    # Back shows the panel (at 8,465 as above): Time's tab at 572,722 and its
    # Stop fast-forward at 829,1644 (fast-forward is on at 20x), Hold to
    # rewind at 850,1389 held 2 s, Saves at 659,722 with Save state and Load
    # state at 306,1095 and 764,1095, then Time's Start fast-forward
    # (xem-ui's panel test checks each tap at this size). Each operation
    # logs the game's vblank and memory hash just after it. The emulator's
    # software Vulkan draws the panel at about two frames a second (and the
    # game runs at most two frames a drawn one), so the checks read those
    # lines, and Back hides the panel while the game is to play on.
    op_of() { sed -E 's/.*game_vblank=([0-9]+) memory_hash ([0-9a-f]+) .*/\1 \2/'; }
    "${adb[@]}" shell input keyevent KEYCODE_BACK
    sleep 2
    tap 572 722
    sleep 2
    mark
    tap 829 1644
    wait_for "xem: app Action\(ToggleFastForward\) game_vblank=.* ok: Fast-forward off\." 60
    shot 10-time
    mark
    "${adb[@]}" shell input swipe 850 1389 850 1389 2000
    held=$(wait_for "xem: app UiRewind\(true\) game_vblank=.* ok: Rewinding while held\." 60)
    released=$(wait_for "xem: app UiRewind\(false\) game_vblank=.* ok: Rewind released\." 60)
    before=$(echo "$held" | vblank_of)
    after=$(echo "$released" | vblank_of)
    tap 659 722
    sleep 2
    mark
    tap 306 1095
    saved=$(wait_for "xem: app Action\(SaveState\) game_vblank=.* ok: State written: .*/files/saves/state\.xemstate" 60)
    echo "  Hold to rewind: game vblank $before when pressed, $after when released, $(echo "$saved" | vblank_of) at Save state"
    [ "$after" -lt "$before" ] || fail "Hold to rewind did not go back ($before -> $after)"
    [ "$(echo "$saved" | vblank_of)" -gt "$after" ] || fail "the game did not play on after the rewind"
    "${adb[@]}" shell input keyevent KEYCODE_BACK
    sleep 6
    mark
    moved=$(wait_for "xem: frame rendered .*game_vblank=" 90)
    [ "$(echo "$moved" | vblank_of)" -gt "$(echo "$saved" | vblank_of)" ] || fail "the game did not play on after the save"
    "${adb[@]}" shell input keyevent KEYCODE_BACK
    sleep 2
    mark
    tap 764 1095
    loaded=$(wait_for "xem: app Action\(LoadState\) game_vblank=.* ok: State restored: .*/files/saves/state\.xemstate" 60)
    echo "  Save state left vblank and memory hash $(op_of <<<"$saved"), the game played on to vblank" \
        "$(echo "$moved" | vblank_of), Load state left $(op_of <<<"$loaded")"
    [ "$(op_of <<<"$saved")" = "$(op_of <<<"$loaded")" ] || fail "Load state did not give back the saved game"
    echo "  $("${adb[@]}" shell run-as "$package" ls -l files/saves/state.xemstate | tr -d '\r')"
    shot 10-saves
    tap 572 722
    sleep 2
    mark
    tap 829 1644
    wait_for "xem: app Action\(ToggleFastForward\) game_vblank=.* ok: Fast-forward on\." 60
    "${adb[@]}" shell input keyevent KEYCODE_BACK
    wait_for "xem: frame rendered .*fast_forward=true" 90

    if tail -n "+$game_start" "$log" |
        grep -E "xem: (panic|game stopped|error)|FATAL EXCEPTION|SIGSEGV|SIGABRT|Fatal signal" | sed 's/^/  /'; then
        fail "crash or game trap in the log"
    fi
    # A native state through the app's own load path: xem-headless's field
    # 30 state (at frame F) where Load state reads it (files/saves/state.xemstate),
    # a session log with no commands that ends at F+600 (files/script.json:
    # the game plays as the original, no player's input or host controls),
    # Load state by its key (F8) once the game booted: the log's memory hash
    # at F+600 is xem-headless's. Save state (F5) there, the state pulled:
    # xem-headless's own state at F+600, and played on natively, the RAM
    # xem-headless's own reaches.
    headless=${XEM_HEADLESS:-$root/src/runtime/target-suite/suite/xem-headless}
    if [ -x "$headless" ]; then
        echo "== a native state loaded on the device, played on, saved there and played on natively"
        native() { "$headless" --disc "$disc1" --control 2>/dev/null; }
        last_string() { python3 -c 'import json,sys; print([json.loads(l) for l in sys.stdin if l.startswith(chr(34))][-1])'; }
        # 1 600 frames in: the game the boot starts plays toward the log's
        # end until the load, which must come first (about 11 s here).
        printf '{"cmd":"enter_field","map":30}\n{"cmd":"frames","count":1600}\n{"cmd":"state_save","path":"%s"}\n' "$out/native.xemstate" | native >/dev/null
        at=$(printf '{"cmd":"state_load","path":"%s"}\n' "$out/native.xemstate" | native | python3 -c 'import json,sys; print(json.loads(sys.stdin.readline())["frames"])')
        start_hash=$(printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"memory_hash"}\n' "$out/native.xemstate" | native | last_string)
        played=$(printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"frames","count":600}\n{"cmd":"memory_hash"}\n' "$out/native.xemstate" | native | last_string)
        twice=$(printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"frames","count":1200}\n{"cmd":"memory_hash"}\n' "$out/native.xemstate" | native | last_string)
        echo "{\"commands\": [], \"frames\": $((at + 600))}" >"$out/state-script.json"
        "${adb[@]}" push "$out/native.xemstate" /data/local/tmp/xem-state.xemstate >/dev/null
        "${adb[@]}" push "$out/state-script.json" /data/local/tmp/xem-script.json >/dev/null
        mark
        "${adb[@]}" shell am force-stop "$package"
        "${adb[@]}" shell run-as "$package" cp /data/local/tmp/xem-state.xemstate files/saves/state.xemstate
        "${adb[@]}" shell run-as "$package" cp /data/local/tmp/xem-script.json files/script.json
        "${adb[@]}" shell am start -n "$activity" --esa xem.args --pick-disc >/dev/null
        wait_for "xem: disc picker opened" 60
        pick_document disc1 "xem-disc1\.chd"
        wait_for "xem: script .* to frame Some\($((at + 600))\)" 120
        wait_for "xem: game booted" 60
        "${adb[@]}" shell input keyevent --longpress KEYCODE_F8
        loaded=$(wait_for "xem: app Action\(LoadState\) game_vblank=.* ok: State restored: .*/files/saves/state\.xemstate" 60)
        got=$(wait_for "xem: script frame $((at + 600)) memory_hash " 120 | sed -E 's/.*memory_hash ([0-9a-f]+).*/\1/')
        echo "  xem-headless's state at frame $at (RAM $start_hash) loaded on the device: $(op_of <<<"$loaded");" \
            "frame $((at + 600)) memory_hash $got (natively $played)"
        [ "$(op_of <<<"$loaded" | cut -d' ' -f2)" = "$start_hash" ] || fail "Load state did not give the native state's RAM"
        [ "$got" = "$played" ] || fail "the native state went on elsewhere on the device"
        mark
        "${adb[@]}" shell input keyevent --longpress KEYCODE_F5
        saved=$(wait_for "xem: app Action\(SaveState\) game_vblank=.* ok: State written: .*/files/saves/state\.xemstate" 60)
        "${adb[@]}" exec-out run-as "$package" cat files/saves/state.xemstate >"$out/device.xemstate"
        continued=$(printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"memory_hash"}\n' "$out/device.xemstate" | native | last_string)
        onward=$(printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"frames","count":600}\n{"cmd":"memory_hash"}\n' "$out/device.xemstate" | native | last_string)
        echo "  Save state on the device: $(op_of <<<"$saved"), $(wc -c <"$out/device.xemstate") bytes; natively RAM $continued," \
            "600 frames on $onward (the native state's own $twice)"
        [ "$(op_of <<<"$saved" | cut -d' ' -f2)" = "$played" ] || fail "Save state on the device did not hold the RAM it played to"
        [ "$continued" = "$played" ] || fail "the device's state loaded natively elsewhere"
        [ "$onward" = "$twice" ] || fail "the device's state went on natively elsewhere"
        # The default graphics ask for no reveals (no wider framing,
        # pull-back or inspection): the whole state is xem-headless's.
        printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"frames","count":600}\n{"cmd":"state_save","path":"%s"}\n' \
            "$out/native.xemstate" "$out/native-played.xemstate" | native >/dev/null
        cmp -s "$out/device.xemstate" "$out/native-played.xemstate" || fail "the device's state is not xem-headless's at that frame, byte for byte"
        echo "  the device's state is xem-headless's at frame $((at + 600)), byte for byte"
        "${adb[@]}" shell run-as "$package" rm -f files/script.json files/saves/state.xemstate
        "${adb[@]}" shell rm -f /data/local/tmp/xem-state.xemstate /data/local/tmp/xem-script.json
    else
        echo "== no xem-headless at $headless (XEM_HEADLESS): skipped the state interchange step"
    fi
    if [ -n "${XEM_SMOKE_SLICE:-}" ]; then
        # play_log NAME [lifecycle]: the session log tests/reference-inputs/slice-NAME.json
        # as files/script.json, from a fresh start through the picker, to the
        # memory hash its native recording ended at.
        play_log() {
            local name=$1 file="$root/tests/reference-inputs/slice-$1.json" want frames got
            want=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['memory_hash'])" "$file")
            frames=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['frames'])" "$file")
            "${adb[@]}" push "$file" /data/local/tmp/xem-script.json >/dev/null
            "${adb[@]}" shell run-as "$package" cp /data/local/tmp/xem-script.json files/script.json
            mark
            "${adb[@]}" shell am force-stop "$package"
            "${adb[@]}" shell am start -n "$activity" --esa xem.args --pick-disc >/dev/null
            wait_for "xem: disc picker opened" 60
            pick_document disc1 "xem-disc1\.chd"
            wait_for "xem: script .* to frame Some\($frames\)" 120
            if [ "${2:-}" = lifecycle ]; then
                sleep 60
                "${adb[@]}" shell input keyevent KEYCODE_HOME
                wait_for "xem: game paused" 30
                sleep 10
                "${adb[@]}" shell am start -n "$activity" >/dev/null
                wait_for "xem: game resumed" 30
                echo "  backgrounded for 10 s at about frame 3600 and returned"
            fi
            got=$(wait_for "xem: script frame $frames memory_hash " $((frames / 60 + 240)) | sed -E 's/.*memory_hash ([0-9a-f]+).*/\1/')
            echo "  $name: frame $frames memory_hash $got (native $want)"
            [ "$got" = "$want" ] || fail "the $name slice ended elsewhere than natively"
        }
        echo "== the Phase 3 slice from session logs (XEM_SMOKE_SLICE)"
        "${adb[@]}" shell run-as "$package" rm -rf files/cards
        "${adb[@]}" shell run-as "$package" mkdir files/cards
        play_log forest lifecycle
        # The save starts from an unformatted card 2; the load from a fresh
        # process reads the card the save left in files/cards.
        head -c 131072 /dev/zero >"$out/card2.mcd"
        "${adb[@]}" push "$out/card2.mcd" /data/local/tmp/xem-card2.mcd >/dev/null
        "${adb[@]}" shell run-as "$package" rm -rf files/cards
        "${adb[@]}" shell run-as "$package" mkdir files/cards
        "${adb[@]}" shell run-as "$package" cp /data/local/tmp/xem-card2.mcd files/cards/card2.mcd
        play_log save
        "${adb[@]}" shell am force-stop "$package"
        play_log load-card2
        "${adb[@]}" shell run-as "$package" rm -f files/script.json
        "${adb[@]}" shell rm -f /data/local/tmp/xem-script.json /data/local/tmp/xem-card2.mcd
    fi
    "${adb[@]}" shell rm -f /sdcard/Download/xem-disc1.chd
else
    echo "== no local disc 1 image: skipped the real-disc import"
fi

"${adb[@]}" shell am force-stop "$package"
echo "PASS (log $log)"
