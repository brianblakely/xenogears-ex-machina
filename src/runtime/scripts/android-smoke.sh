#!/usr/bin/env bash
# Android smoke test on the emulator (no device needed):
#
#   nix develop path:./nix/runtime#android -c src/runtime/scripts/android-smoke.sh
#
# Builds the x86_64 APK (XEM_SKIP_BUILD=1 reuses it), boots a headless API-34
# AVD (swiftshader_indirect: guest Vulkan on SwiftShader) kept under
# .local/android, installs the app fresh and checks from logcat and
# screenshots (build/android/smoke/*.png): first frame, bundled asset,
# Vulkan device, a settings change by touch, disc import through the system
# document picker (a synthetic MODE2 image; the user's disc 1 CHD too when
# XEM_DISC1 or discs/"Xenogears disc 1.chd" exists), background and return
# (surface dropped and recreated, simulation time not advanced), low memory,
# rotation, activity recreation (teardown and a new SDL_main run) and restart
# (settings and the persisted document grant survive). With the user's disc
# 1 the game boots from it and plays for ~25 s (screenshots), with its frame
# rate measured, a pad press, and a background and return during which the
# game's frame count does not move, plus repeated Home/return at 20x FF.
# XEM_SMOKE_GAME_ONLY=1 runs only the
# start and the game. XEM_SMOKE_SLICE=1 then plays the Phase 3 slice's session
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

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

if [ -z "${XEM_SKIP_BUILD:-}" ]; then
    XEM_ANDROID_ABIS=x86_64 "$root/src/runtime/scripts/android-build.sh"
fi
[ -f "$apk" ] || fail "no APK at $apk"

# The synthetic disc: an unknown PlayStation disc whose boot program's digest we know.
synthetic=$out/xem-synthetic.bin
synthetic_sha=$(cd "$root/src/runtime" && cargo run -q -p xem-disc --example synthetic-disc -- "$synthetic")

if [ ! -f "$ANDROID_AVD_HOME/xem-smoke.avd/config.ini" ]; then
    mkdir -p "$ANDROID_AVD_HOME"
    echo no | avdmanager create avd -n xem-smoke -d pixel_6 \
        -k "system-images;android-34;google_apis;x86_64" >/dev/null
fi
emulator -avd xem-smoke -port "$port" -no-window -no-audio -no-boot-anim -no-snapshot \
    -gpu "${XEM_EMULATOR_GPU:-swiftshader_indirect}" >"$out/emulator.log" 2>&1 &
emulator_pid=$!
logcat_pid=
cleanup() {
    [ -n "$logcat_pid" ] && kill "$logcat_pid" 2>/dev/null
    "${adb[@]}" emu kill >/dev/null 2>&1 || kill "$emulator_pid" 2>/dev/null || true
    wait "$emulator_pid" 2>/dev/null || true
}
trap cleanup EXIT

"${adb[@]}" wait-for-device
for _ in $(seq 150); do
    [ "$("${adb[@]}" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break
    sleep 2
done
[ "$("${adb[@]}" shell getprop sys.boot_completed | tr -d '\r')" = 1 ] || fail "emulator did not boot"
echo "emulator booted: $("${adb[@]}" shell getprop ro.build.fingerprint | tr -d '\r')"
# Right after boot the system still changes app configurations (asset
# paths), which recreates activities; let that settle.
sleep 20
# No "viewing full screen" hint over the app; portrait to start.
"${adb[@]}" shell settings put secure immersive_mode_confirmations confirmed
"${adb[@]}" shell settings put system accelerometer_rotation 0
"${adb[@]}" shell settings put system user_rotation 0
# The AVD keeps its data between runs: undo what a previous run may have left.
"${adb[@]}" shell settings put global always_finish_activities 0
"${adb[@]}" shell wm density reset
for _ in $(seq 15); do
    [[ $("${adb[@]}" shell dumpsys window displays) == *"mRotation=0 "* ]] && break
    sleep 1
done
[[ $("${adb[@]}" shell dumpsys window displays) == *"mRotation=0 "* ]] || fail "display not in portrait"

"${adb[@]}" uninstall "$package" >/dev/null 2>&1 || true
"${adb[@]}" install "$apk" >/dev/null
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

if [ -z "${XEM_SMOKE_GAME_ONLY:-}" ]; then
echo "== settings change by touch (Show FPS)"
mark
tap 540 670
sleep 1
tap 540 780
sleep 1
shot 01-graphics
tap 73 1082
wait_for "xem: settings applied ShowFps\(true\)"

echo "== disc import through the document picker (Import disc... button)"
mark
tap 540 670
sleep 1
tap 540 1095
sleep 1
shot 01-saves
tap 540 1572
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
    "${adb[@]}" shell am start -n "$activity" --esa xem.args --pick-disc >/dev/null
    wait_for "xem: settings .*fast_forward_rate: Fixed\(20\)" 60
    wait_for "xem: disc picker opened" 60
    pick_document disc1 "xem-disc1\.chd"
    wait_for "xem: disc identified source=content://.*boot=SLUS_006\.64;1 sha256=dc0b2dd7[0-9a-f]* disc=Xenogears disc 1 \(SLUS-00664\)" 180
    booted=$(wait_for "xem: game booted source=content://.*boot=SLUS_006\.64;1" 60)
    echo "$booted"
    [[ $booted == *"audio=stream opened"* ]] || fail "no audio stream for the game"
    wait_for "xem: frame rendered .*game_vblank=" 60
    for at in 03 08 15 25; do
        sleep $((10#$at - ${previous:-0}))
        previous=$((10#$at))
        shot "07-game-${at}s"
    done
    grep -E "xem: (fps|frame rendered .*game_vblank)" "$log" | tail -12 | sed 's/^/  /'
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

    if tail -n "+$game_start" "$log" |
        grep -E "xem: (panic|game stopped|error)|FATAL EXCEPTION|SIGSEGV|SIGABRT|Fatal signal" | sed 's/^/  /'; then
        fail "crash or game trap in the log"
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
