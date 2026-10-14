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
#   with stereo disparity, not reconstructed 3D);
# - first-person inspection in field 30: the underhand grab turns it on and
#   off, each eye then looks from the character's eye with the head (their
#   pictures cover the view, differ between the eyes and follow the head
#   while the player's Pause holds the game), the screen keeps its size,
#   and the game's RAM is xem-headless's.
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
# Hand gestures, scripted (Monado's simulated headset tracks no hands): an
# overhand grab pauses and another resumes, prayer hands open and close
# Settings, a pinch with the gaze on the panel's Close button closes it,
# and in field 30 a pinch with the gaze on the floor walks there.
echo '{"commands": [[0, {"cmd": "enter_field", "map": 30}]]}' >"$out/field30.json"
cp "$out/config/xenogears-ex-machina/settings.json" "$out/settings.saved"
echo '{"graphics": {"profile": "Hd2d"}, "assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/field30.json" --exit-after-frames 330 \
    --xr-hands "60:pause,120:pause,140:prayer,170:prayer,180:prayer,190:pinch@panel:410/30,230:pinch@120/150" >"$out/gestures.log" 2>"$out/gestures.err"
cp "$out/settings.saved" "$out/config/xenogears-ex-machina/settings.json"
# Field 12 (a room, a dialogue window and a portrait) under the HD-2D profile.
echo '{"commands": [[0, {"cmd": "enter_field", "map": 12}]]}' >"$out/field.json"
"$desktop" --xr --disc "$disc" --script "$out/field.json" --exit-at-frame 750 --xr-capture "$out/field" >"$out/field.log" 2>"$out/field.err"
printf '{"cmd":"frames","count":%d}\n{"cmd":"memory_hash"}\n' "$frames" |
    "$headless" --disc "$disc" --control >"$out/headless.log" 2>"$out/headless.err"
# First-person inspection in field 30, Fei walked into the room by the log
# (whose pads stand in for the player's): the underhand grab at headset
# frame 150 and again at 270, the Pause grab at 180 and 240, both eyes
# captured twice while paused; then the same log in xem-headless.
inspect='[[0, {"cmd": "enter_field", "map": 30}], [500, {"cmd": "pad", "buttons": ["down"]}], [560, {"cmd": "pad", "buttons": []}]]'
echo "{\"commands\": $inspect}" >"$out/inspect.json"
"$desktop" --xr --disc "$disc" --script "$out/inspect.json" --exit-at-frame 1200 \
    --xr-hands "150:inspect,180:pause,240:pause,270:inspect" --xr-capture "$out/inspect" --xr-capture-at 200,225 >"$out/inspect.log" 2>"$out/inspect.err"
printf '{"cmd":"schedule","commands":%s}\n{"cmd":"run_until","until":{"frames":1200},"max_frames":2000}\n{"cmd":"memory_hash"}\n' "$inspect" |
    "$headless" --disc "$disc" --control >"$out/inspect-headless.log" 2>"$out/inspect-headless.err"

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

rows = records("gestures.log")
by = {int(r["n"]): r for r in rows}
gestures = [(int(r["n"]), r["gestures"]) for r in rows if r["gestures"] != "[]"]
print(f"gestures: {gestures}")
paused = [r for r in rows if r["player_pause"] == "1"]
check(len(paused) >= 40, f"the Pause grab held the game for only {len(paused)} frames")
check(len({r["game_frames"] for r in paused}) == 1, "the game ran while the player paused it")
check(all(by[n]["player_pause"] == "0" for n in range(130, 140)), "the second grab did not resume")
panel = [r for r in rows if r["layers"] == "2" and int(r["n"]) < 175]
check(20 <= len(panel) <= 40, f"prayer hands showed the panel for {len(panel)} frames")
check(len({r["game_frames"] for r in panel}) == 1, "the game ran while Settings were open")
again = [int(r["n"]) for r in rows if r["layers"] == "2" and int(r["n"]) >= 175]
check(again and again[0] < 190 and max(again) < 200, f"a pinch on Close closed the panel: shown at {again[:3]}..{again[-3:]}")
walked = [r["assist"] for r in rows if int(r["n"]) > 230]
check("moving" in walked and walked[-1] == "arrived", f"the pinch did not walk the player there: {walked[:5]}..{walked[-3:]}")

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

# First-person inspection.
rows = records("inspect.log")
by = {int(r["n"]): r for r in rows}
on = [int(r["n"]) for r in rows if r["fp"] == "1"]
check(bool(on) and 150 <= on[0] <= 160 and 270 <= on[-1] <= 280 and len(on) == on[-1] - on[0] + 1,
      f"the underhand grabs turned inspection on and off: on {on[:1]}..{on[-1:]}")
check(all(r["fp"] == "0" for r in rows if int(r["n"]) > 285), "inspection stayed on after the second grab")
within = [r for r in rows if r["within"] == "1"]
check(len(within) >= len(on) - 4 and all(r["fp"] == "1" for r in within),
      f"the eyes looked from within the scene for {len(within)} of {len(on)} frames")
paused = [r for r in rows if r["player_pause"] == "1"]
check(len(paused) >= 40 and len({r["game_frames"] for r in paused}) == 1, f"the Pause grab held the game for {len(paused)} frames")
check(all(r["within"] == "1" and r["render"] == "1" for r in paused), "while paused the eyes stopped looking from within the scene")
check(len({r["head"] for r in paused}) > len(paused) // 2, "head tracking stopped while paused")
check(all(by[n]["player_pause"] == "1" for n in (200, 225)), "the eyes were not captured while paused")
check(all(r["scale"] == "1.0000" for r in rows), "the screen's size changed")
inspected = [json.loads(l) for l in open(f"{out}/inspect-headless.log") if l.startswith("{") or l.startswith('"')]
inspected = next(v for v in inspected if isinstance(v, str))
check(final_hash("inspect.log") == inspected, f"inspect.log: RAM at game frame 1200 {final_hash('inspect.log')} != headless {inspected}")
back = left[0][0:3]
def covered(rows_):
    # Pixels unlike the room around the screen (the running eyes' corner).
    return sum(1 for row in rows_ for x in range(0, len(row), 16)
               if sum(abs(row[x + k] - back[k]) for k in range(3)) > 30) / (len(rows_) * len(rows_[0]) // 16)
eyes = {f"{n} {side}": png(f"{out}/inspect/{n}/{side}.png")[2] for n in (200, 225) for side in ("left", "right")}
check(all(covered(e) > 0.9 for e in eyes.values()), "the eyes' pictures do not cover the view: "
      + ", ".join(f"{k} {covered(e):.2f}" for k, e in eyes.items()))
check(eyes["200 left"] != eyes["200 right"], "both eyes see the same picture from within the scene")
check(eyes["200 left"] != eyes["225 left"], "the paused view did not follow the head")
print(f"inspect: on {on[0]}..{on[-1]}, {len(within)} frames from within the scene, {len(paused)} paused at game frame "
      f"{paused[0]['game_frames']}; eyes cover " + ", ".join(f"{k} {covered(e):.0%}" for k, e in eyes.items()))
# Pacing and the presentation's time on its thread, from within the scene and as the screen.
def pace(select):
    pairs = [(a, b) for a, b in zip(rows, rows[1:]) if select(a) and select(b)]
    gaps = sorted(int(b["wall_us"]) - int(a["wall_us"]) for a, b in pairs)
    present = sorted(int(b["present_us"]) for _, b in pairs)
    late = sum(1 for g in gaps if g > 1.5 * period / 1000)
    return f"interval p50 {gaps[len(gaps) // 2] / 1000:.2f} ms, p99 {gaps[int(len(gaps) * 0.99)] / 1000:.2f} ms, {late} of {len(gaps)} late; presenting p50 {present[len(present) // 2] / 1000:.2f} ms, p95 {present[int(len(present) * 0.95)] / 1000:.2f} ms"
print(f"inspect pacing, the screen: {pace(lambda r: r['within'] == '0' and int(r['n']) > 20)}")
print(f"inspect pacing, from within: {pace(lambda r: r['within'] == '1')}")

if problems:
    print("xr-game-smoke: FAILED\n  " + "\n  ".join(problems))
    sys.exit(1)
print("xr-game-smoke: passed")
EOF
