#!/usr/bin/env bash
# Plays the game on a virtual screen in an OpenXR session against Monado
# (no display or headset; local only: it needs the user's disc 1 and the game
# module) and checks Phase 4's headset rules:
# - the game runs at its own 60 frames a second while the headset presents at
#   its own rate (Monado's simulated HMD predicts 20 Hz), once per headset
#   frame and never per eye;
# - pausing the game keeps head tracking and rendering going;
# - neither the headset nor the HD-2D profile nor pausing changes the game:
#   the RAM at game frame N equals xem-headless's at frame N;
# - both eyes show the same game picture from their own views (a flat screen
#   with stereo disparity, not reconstructed 3D).
#
#   nix develop path:./nix/runtime -c src/runtime/scripts/xr-game-smoke.sh
#
# Logs and eye captures go to src/runtime/target/xr-game-smoke (XEM_XR_OUT).
set -euo pipefail

runtime=$(cd "$(dirname "$0")/.." && pwd)
root=$(cd "$runtime/../.." && pwd)
out=${XEM_XR_OUT:-$runtime/target/xr-game-smoke}
disc=${XEM_DISC1:-$root/discs/Xenogears disc 1.chd}
frames=${XEM_XR_GAME_FRAMES:-600}
: "${XEM_MONADO_RUNTIME:?run inside nix develop path:./nix/runtime}"
export VK_ICD_FILENAMES=${VK_ICD_FILENAMES:-$XEM_LAVAPIPE_ICD}
export XR_RUNTIME_JSON=$XEM_MONADO_RUNTIME
export SDL_AUDIO_DRIVER=dummy

# XEM_CARGO_PROFILE (default release) is the cargo profile it builds and runs;
# XEM_SKIP_BUILD=1 runs that profile's existing builds.
profile=${XEM_CARGO_PROFILE:-release}
[[ -n ${XEM_SKIP_BUILD:-} ]] || (cd "$runtime" && cargo build --profile "$profile" -p xem-desktop -p xem-headless)
desktop=${CARGO_TARGET_DIR:-$runtime/target}/$profile/xem-desktop
headless=${CARGO_TARGET_DIR:-$runtime/target}/$profile/xem-headless

rm -rf "$out"
mkdir -p "$out/config/xenogears-ex-machina"
echo '{"graphics": {"profile": "Hd2d"}}' >"$out/config/xenogears-ex-machina/settings.json"
export XDG_CONFIG_HOME=$out/config
rundir=$(mktemp -d "${XDG_RUNTIME_DIR:-/tmp}/xem-xr.XXXXXX")
export XDG_RUNTIME_DIR=$rundir
monado=
stop() {
    if [[ -n $monado ]]; then
        kill "$monado" 2>/dev/null || true
        timeout 5 tail --pid="$monado" -f /dev/null || kill -9 "$monado" 2>/dev/null || true
    fi
    rm -rf "$rundir"
}
trap stop EXIT
env XRT_NO_STDIN=1 XRT_COMPOSITOR_NULL=1 \
    SIMULATED_ENABLE=1 SIMULATED_ROTATE=1 SIMULATED_LEFT=simple SIMULATED_RIGHT=simple \
    monado-service >"$out/monado.log" 2>&1 &
monado=$!
for _ in $(seq 100); do [[ -S $rundir/monado_comp_ipc ]] && break; sleep 0.1; done
[[ -S $rundir/monado_comp_ipc ]] || { echo "xr-game-smoke: Monado did not start" >&2; tail "$out/monado.log" >&2; exit 1; }

"$desktop" --xr --disc "$disc" --exit-at-frame "$frames" --xr-capture "$out/eyes" >"$out/running.log" 2>"$out/running.err"
"$desktop" --xr --disc "$disc" --exit-at-frame "$frames" --xr-pause 60,120 >"$out/paused.log" 2>"$out/paused.err"
# Field 12 (a room, a dialogue window and a portrait) under the HD-2D profile.
echo '{"commands": [[0, {"cmd": "enter_field", "map": 12}]]}' >"$out/field.json"
"$desktop" --xr --disc "$disc" --script "$out/field.json" --exit-at-frame 750 --xr-capture "$out/field" >"$out/field.log" 2>"$out/field.err"
printf '{"cmd":"frames","count":%d}\n{"cmd":"memory_hash"}\n' "$frames" |
    "$headless" --disc "$disc" --control >"$out/headless.log" 2>"$out/headless.err"

python3 - "$out" "$frames" <<'EOF'
import json, re, sys, zlib, struct
out, frames = sys.argv[1], int(sys.argv[2])

def records(name):
    rows = []
    for line in open(f"{out}/{name}"):
        if line.startswith("xrframe "):
            rows.append(dict(f.split("=", 1) for f in line.split()[1:]))
    return rows

def final_hash(name):
    for line in open(f"{out}/{name}"):
        m = re.search(r"memory_hash ([0-9a-f]{16})", line)
        if m:
            return m.group(1)
    sys.exit(f"xr-game-smoke: no memory hash in {name}")

problems = []
def check(ok, message):
    if not ok:
        problems.append(message)

headless = [json.loads(l) for l in open(f"{out}/headless.log") if l.startswith("{") or l.startswith('"')]
reference = next(v for v in headless if isinstance(v, str))
for name in ["running.log", "paused.log"]:
    check(final_hash(name) == reference, f"{name}: RAM at game frame {frames} {final_hash(name)} != headless {reference}")

rows = records("running.log")
period = sorted(int(r["period_ns"]) for r in rows)[len(rows) // 2]
check(abs(period - 1e9 / 60) > 2e6, f"headset period {period} ns is the game's own; pacing is not shown independent")
# Game frames per second of headset time over the run's middle half.
a, b = rows[len(rows) // 4], rows[3 * len(rows) // 4]
seconds = (int(b["pdt_ns"]) - int(a["pdt_ns"])) / 1e9
rate = (int(b["game_frames"]) - int(a["game_frames"])) / seconds
check(57 <= rate <= 63, f"game ran {rate:.1f} frames a second at a {1e9 / period:.0f} Hz headset")
check(max(int(r["game_ran"]) for r in rows) <= 4, "a headset frame ran a burst of game frames")
check(sum(1 for r in rows if r["picture"] != "-") > len(rows) // 2, "the game picture is missing from most frames")
print(f"running: {len(rows)} headset frames, period {period / 1e6:.2f} ms, game {rate:.1f} fps")
# Sustained pacing: the loop's wall-clock intervals (xrWaitFrame paces it).
walls = [int(r["wall_us"]) for r in rows[len(rows) // 10:]]
gaps = sorted(b - a for a, b in zip(walls, walls[1:]))
p50, p99 = gaps[len(gaps) // 2], gaps[int(len(gaps) * 0.99)]
missed = sum(1 for g in gaps if g > 1.5 * period / 1000)
print(f"pacing: interval p50 {p50 / 1000:.2f} ms, p99 {p99 / 1000:.2f} ms, {missed} of {len(gaps)} beyond 1.5 periods")
check(missed <= len(gaps) // 20, f"{missed} of {len(gaps)} headset frames late")

rows = records("paused.log")
paused = [r for r in rows if r["paused"] == "1"]
check(len(paused) >= 100, f"only {len(paused)} paused frames")
check(len({r["game_frames"] for r in paused}) == 1, "the game advanced while paused")
check(all(r["render"] == "1" for r in paused), "a paused frame was not rendered")
check(len({r["head"] for r in paused}) > len(paused) // 2, "head tracking stopped while paused")
print(f"paused: {len(paused)} frames at game frame {paused[0]['game_frames']}, {len({r['head'] for r in paused})} head poses")

def png(path):
    data = open(path, "rb").read()
    pos, chunks, w = 8, [], 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        if kind == b"IHDR":
            w, h = struct.unpack(">II", data[pos + 8:pos + 16])
        if kind == b"IDAT":
            chunks.append(data[pos + 8:pos + 8 + n])
        pos += 12 + n
    raw = zlib.decompress(b"".join(chunks))
    stride = w * 4
    rows_, prev = [], bytearray(stride)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a_ = line[x - 4] if x >= 4 else 0
            b_ = prev[x]
            c_ = prev[x - 4] if x >= 4 else 0
            if f == 1: line[x] = (line[x] + a_) & 255
            elif f == 2: line[x] = (line[x] + b_) & 255
            elif f == 3: line[x] = (line[x] + (a_ + b_) // 2) & 255
            elif f == 4:
                p_ = a_ + b_ - c_
                pa, pb, pc = abs(p_ - a_), abs(p_ - b_), abs(p_ - c_)
                line[x] = (line[x] + (a_ if pa <= pb and pa <= pc else b_ if pb <= pc else c_)) & 255
        rows_.append(bytes(line)); prev = line
    return w, h, rows_

lw, lh, left = png(f"{out}/eyes/left.png")
_, _, right = png(f"{out}/eyes/right.png")
def lit(rows_):
    # Pixels unlike the backdrop (the room around the screen).
    back = rows_[0][0:3]
    return sum(1 for row in rows_ for x in range(0, len(row), 16)
               if sum(abs(row[x + k] - back[k]) for k in range(3)) > 30) / (len(rows_) * len(rows_[0]) // 16)
check(lit(left) > 0.05 and lit(right) > 0.05, f"the screen is not visible ({lit(left):.3f}, {lit(right):.3f})")
check(left != right, "both eyes are identical (no stereo views)")
print(f"eyes: {lw}x{lh}, screen covers {lit(left):.2%} / {lit(right):.2%}")
_, _, field = png(f"{out}/field/left.png")
colours = {row[x:x + 3] for row in field for x in range(0, len(row), 4)}
check(len(colours) > 200, f"the field shows only {len(colours)} colours")
print(f"field: {len(colours)} colours on the left eye")

if problems:
    print("xr-game-smoke: FAILED\n  " + "\n  ".join(problems))
    sys.exit(1)
print("xr-game-smoke: passed")
EOF
