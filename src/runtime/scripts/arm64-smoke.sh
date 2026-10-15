#!/usr/bin/env bash
# The game on arm64 under qemu's user-mode emulator (no device needed):
#
#   nix develop path:./nix/runtime#android -c src/runtime/scripts/arm64-smoke.sh
#
# Builds xem-headless for aarch64-linux-android as the APK's arm64-v8a
# library is built (cargo-ndk, API 29: the NDK's clang compiles the game
# module's translation), but linked statically against bionic, so that the
# shell's qemu-aarch64 runs it without an Android system (XEM_SKIP_BUILD=1
# reuses the build). With the user's disc 1 (XEM_DISC1, else
# discs/"Xenogears disc 1.chd") it then plays the Phase 3 slice's session
# logs (tests/reference-inputs/slice-*.json: the forest route, the save to an
# unformatted card 2, the title screen's load of that card as card 1 and as
# card 2) under qemu-aarch64 and with xem-headless on this machine
# (XEM_HEADLESS, else the suite's build): each ends at the memory hash its
# recording ended at, in the same state on both byte for byte; the save
# writes the same card 2 on both; and the forest route's state in its
# battle (frame 7000), the same on both, goes on from each on the other to
# the route's end and its state. Lahan and the world map drawn as a camera
# pulled back and a wider framing draw them (xem-headless's reveal_frames:
# the game module's only floating point judges the faces the game culls,
# xem_present_face_visible Lahan's scenery and the world map's terrain,
# xem_present_points_visible the world map's cloud quads) reveal the same
# faces on both, to the same RAM and state. Inputs, replies and states:
# build/arm64/.
set -euo pipefail

root=$(cd "$(dirname "$0")/../../.." && pwd)
runtime=$root/src/runtime
out=$root/build/arm64
target=${CARGO_TARGET_DIR:-$runtime/target-arm64}
arm64=$target/aarch64-linux-android/suite/xem-headless
headless=${XEM_HEADLESS:-$runtime/target-suite/suite/xem-headless}
disc1=${XEM_DISC1:-$root/discs/Xenogears disc 1.chd}

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

command -v qemu-aarch64 >/dev/null || fail "no qemu-aarch64: run inside nix develop path:./nix/runtime#android"
if [ -z "${XEM_SKIP_BUILD:-}" ]; then
    : "${ANDROID_NDK_HOME:?run inside nix develop path:./nix/runtime#android}"
    # crt-static links bionic's libc.a: no Android linker or libraries at
    # run time (and no notice of the static link's unused -no-pie).
    (cd "$runtime" && CARGO_TARGET_DIR=$target \
        CARGO_TARGET_AARCH64_LINUX_ANDROID_RUSTFLAGS="-C target-feature=+crt-static -C link-arg=-Wno-unused-command-line-argument" \
        cargo ndk -t arm64-v8a -P 29 build -p xem-headless --profile suite)
fi
[ -x "$arm64" ] || fail "no arm64 build at $arm64"
if [ ! -f "$disc1" ]; then
    echo "== no local disc 1 image: built only"
    exit 0
fi
[ -x "$headless" ] || fail "no xem-headless at $headless (XEM_HEADLESS)"
rm -rf "$out"
mkdir -p "$out"
cd "$root"

# Each run's commands, one line each (xem-headless --control).
python3 - "$out" "$root/tests/reference-inputs" <<'EOF'
import json, sys
out, logs = sys.argv[1], sys.argv[2]
# In the forest route's battle (about frames 5950 to 8600).
MID = 7000
def log(name):
    return json.load(open(f"{logs}/slice-{name}.json"))
def write(name, commands):
    with open(f"{out}/{name}.in", "w") as f:
        f.writelines(json.dumps(command) + "\n" for command in commands)
forest = log("forest")
for arch, other in (("arm64", "native"), ("native", "arm64")):
    write(f"{arch}-forest", [
        {"cmd": "schedule", "commands": forest["commands"]},
        {"cmd": "frames", "count": MID},
        {"cmd": "state_save", "path": f"{out}/{arch}-forest-{MID}.xemstate"},
        {"cmd": "frames", "count": forest["frames"] - MID},
        {"cmd": "memory_hash"},
        {"cmd": "state_save", "path": f"{out}/{arch}-forest.xemstate"},
    ])
    # The other's state in battle, in a fresh session given the log's
    # commands from there on (a state holds no schedule).
    write(f"{arch}-onward", [
        {"cmd": "state_load", "path": f"{out}/{other}-forest-{MID}.xemstate"},
        {"cmd": "schedule", "commands": [c for c in forest["commands"] if c[0] >= MID]},
        {"cmd": "frames", "count": forest["frames"] - MID},
        {"cmd": "memory_hash"},
        {"cmd": "state_save", "path": f"{out}/{arch}-onward.xemstate"},
    ])
    for name in ("save", "load", "load-card2"):
        played = log(name)
        write(f"{arch}-{name}", [
            {"cmd": "schedule", "commands": played["commands"]},
            {"cmd": "frames", "count": played["frames"]},
            {"cmd": "memory_hash"},
            {"cmd": "state_save", "path": f"{out}/{arch}-{name}.xemstate"},
        ])
    # Lahan (field 1) and the world map, each drawn as a 16:9 framing's sides
    # (53 pixels) and their margin with the camera pulled back by 3000 depth
    # units, ask the port for the faces the game culls: it judges each on the
    # pulled-back camera's screen in floating point (Lahan: about 460
    # revealed a frame here; the world map: about 1 850, its clouds' quads
    # among them, the only ones xem_present_points_visible judges).
    for name, entry, count in (
        ("lahan", [{"cmd": "enter_field", "map": 1}, {"cmd": "frames", "count": 300}], 600),
        ("worldmap", [{"cmd": "boot_mode", "mode": 3}, {"cmd": "frames", "count": 900}], 300),
    ):
        write(f"{arch}-reveals-{name}", entry + [
            {"cmd": "reveal_frames", "count": count, "reach": [85, 32], "pull_back": 3000},
            {"cmd": "memory_hash"},
            {"cmd": "state_save", "path": f"{out}/{arch}-reveals-{name}.xemstate"},
        ])
EOF

# run ARCH NAME...: each NAME in turn, xem-headless (ARCH arm64: its arm64
# build under qemu-aarch64) on NAME's commands with the cards in
# ARCH-cards-NAME; replies in ARCH-NAME.out, seconds in ARCH-NAME.seconds.
run() {
    local arch=$1 name start
    local program=("$headless")
    [ "$arch" = native ] || program=(qemu-aarch64 "$arm64")
    shift
    for name in "$@"; do
        start=$SECONDS
        "${program[@]}" --disc "$disc1" --control --cards "$out/$arch-cards-$name" \
            <"$out/$arch-$name.in" >"$out/$arch-$name.out" 2>"$out/$arch-$name.err" || return 1
        echo $((SECONDS - start)) >"$out/$arch-$name.seconds"
    done
}
# together NAME...: under qemu each NAME beside the others, natively one
# after another (seconds each), all at once.
together() {
    local pids=() pid failed=0 name
    for name in "$@"; do
        run arm64 "$name" &
        pids+=($!)
    done
    run native "$@" &
    pids+=($!)
    for pid in "${pids[@]}"; do
        wait "$pid" || failed=1
    done
    return $failed
}

for arch in arm64 native; do
    mkdir -p "$out/$arch-cards-forest" "$out/$arch-cards-save" "$out/$arch-cards-onward" \
        "$out/$arch-cards-load" "$out/$arch-cards-load-card2" \
        "$out/$arch-cards-reveals-lahan" "$out/$arch-cards-reveals-worldmap"
    # Card 2 starts unformatted, as on the original route.
    head -c 131072 /dev/zero >"$out/$arch-cards-save/card2.mcd"
done
echo "== the forest route, the save and the world map pulled back under qemu-aarch64 and natively"
together forest save reveals-worldmap || fail "a run stopped (build/arm64/*.err)"
for arch in arm64 native; do
    cp "$out/$arch-cards-save/card2.mcd" "$out/$arch-cards-load/card1.mcd"
    cp "$out/$arch-cards-save/card2.mcd" "$out/$arch-cards-load-card2/card2.mcd"
done
echo "== the loads of that card, the battle's state on the other, and Lahan pulled back"
together load load-card2 onward reveals-lahan || fail "a run stopped (build/arm64/*.err)"

python3 - "$out" "$root/tests/reference-inputs" <<'EOF'
import filecmp, json, sys
out, logs = sys.argv[1], sys.argv[2]
frames = {}
recorded = {}
for name in ("forest", "save", "load", "load-card2"):
    played = json.load(open(f"{logs}/slice-{name}.json"))
    frames[name], recorded[name] = played["frames"], played["memory_hash"]
frames["onward"], recorded["onward"] = frames["forest"] - 7000, recorded["forest"]
same = lambda a, b: filecmp.cmp(f"{out}/{a}", f"{out}/{b}", shallow=False)
failures = []
for name in ("forest", "save", "load", "load-card2", "onward"):
    line = []
    for arch in ("arm64", "native"):
        replies = [json.loads(text) for text in open(f"{out}/{arch}-{name}.out")]
        errors = [reply for reply in replies if isinstance(reply, dict) and "error" in reply]
        if errors:
            failures.append(f"{arch} {name}: {errors[0]}")
        got = next((reply for reply in replies if isinstance(reply, str)), None)
        seconds = int(open(f"{out}/{arch}-{name}.seconds").read())
        line.append(f"{arch} {got} in {seconds} s ({frames[name] // max(seconds, 1)} frames/s)")
        if got != recorded[name]:
            failures.append(f"{arch} {name} ended at {got}, its recording at {recorded[name]}")
    print(f"  {name}: {frames[name]} frames, recorded {recorded[name]}; " + ", ".join(line))
    if not same(f"arm64-{name}.xemstate", f"native-{name}.xemstate"):
        failures.append(f"{name}: the arm64 state at its end is not the native one")
if not same("arm64-forest-7000.xemstate", "native-forest-7000.xemstate"):
    failures.append("the forest route's arm64 state at frame 7000 is not the native one")
for arch in ("arm64", "native"):
    if not same(f"{arch}-onward.xemstate", f"{arch}-forest.xemstate"):
        failures.append(f"{arch}: the battle's state from the other went on to another state than the route's own")
if not same("arm64-cards-save/card2.mcd", "native-cards-save/card2.mcd"):
    failures.append("the save's card 2 under qemu is not the native one")
# Lahan and the world map pulled back: reveal_frames' answer (the frames,
# the primitives traced, the culled faces revealed, their digest) and the
# RAM after it, on each.
for name, count in (("lahan", 600), ("worldmap", 300)):
    reveals = {}
    for arch in ("arm64", "native"):
        replies = [json.loads(text) for text in open(f"{out}/{arch}-reveals-{name}.out")]
        errors = [reply for reply in replies if isinstance(reply, dict) and "error" in reply]
        if errors:
            failures.append(f"{arch} reveals-{name}: {errors[0]}")
        answer = next((reply for reply in replies if isinstance(reply, dict) and "digest" in reply), {})
        ram = next((reply for reply in replies if isinstance(reply, str)), None)
        seconds = int(open(f"{out}/{arch}-reveals-{name}.seconds").read())
        reveals[arch] = (answer, ram)
        print(f"  reveals-{name}: {arch} {answer.get('frames')} frames, {answer.get('drawn')} primitives,"
              f" {answer.get('revealed')} culled faces revealed (digest {answer.get('digest')}), RAM {ram} in {seconds} s")
    if reveals["native"][0].get("frames") != count or not reveals["native"][0].get("revealed"):
        failures.append(f"reveals-{name}: natively {reveals['native'][0]}, not {count} frames revealing culled faces")
    if reveals["arm64"] != reveals["native"]:
        failures.append(f"reveals-{name}: under qemu other faces were revealed, or the RAM went elsewhere, than natively")
    if not same(f"arm64-reveals-{name}.xemstate", f"native-reveals-{name}.xemstate"):
        failures.append(f"reveals-{name}: the arm64 state at its end is not the native one")
for failure in failures:
    print(f"FAIL: {failure}", file=sys.stderr)
if failures:
    sys.exit(1)
print("  every log's end state, the battle's state at frame 7000 and the save's card 2: arm64's the native ones, byte for byte;"
      " the battle's state from each went on to the route's end state on the other;"
      " Lahan and the world map pulled back revealed the same culled faces on both, to the same RAM and state")
EOF
echo "PASS (build/arm64)"
