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
# - the title movie's frames (from boot) are rebuilt from what the MDEC
#   decoded (movie=1), as many pixels across as the 3 m screen 2.5 m ahead
#   spans in an eye (from its field of view), under the Enhanced movies
#   setting, and with wider framing on the screen has no sides for them
#   (the renderer's own frames before the movie have them); under Original
#   they are the device's own 320x216 pictures (movie=0), which changes
#   nothing in the game either;
# - first-person inspection in field 30: the underhand grab turns it on and
#   off, each eye then looks from the character's eye with the head (their
#   pictures cover the view, show the room, differ between the eyes and
#   follow the head while the player's Pause holds the game, and the eyes
#   show them in the renderer's own colours, under HD-2D and under the
#   default Modern profile), the grab toggles it while the game is paused,
#   the screen keeps its size, and the game's RAM is xem-headless's;
# - the seated diorama (the headset view setting) in field 30: the eyes see
#   the room on the table in 3D (their disparity grows from the far rows to
#   the near ones, where the screen's is the same in every row, and each
#   eye shows its diorama picture in the renderer's colours), the app's
#   Recenter puts the seat at the head, Monado's recentring of LOCAL
#   (`monado-ctl -c`) reaches the session and puts the seat back at LOCAL's
#   origin, the game's RAM is xem-headless's, prayer hands open the
#   settings panel above the table and a pinch on its Close closes it, and
#   a pinch with the gaze through the table on a floor point walks Fei
#   there; in field 12 both eyes show the pane above the table with the
#   game's dialogue on it, in the pane's colours;
# - save states and time controls in field 30 (a state xem-headless saved,
#   loaded through an agent's --control command): the panel's Save state
#   and Load state by pinches (the HUD's notices, the state file, which
#   xem-headless restores to the RAM the save left, and the load's RAM the
#   save's), Time's Start fast-forward (the game near the rate the session
#   paces it at) and the controllers' chords from scripted buttons: Back+RT
#   turns it off (60 frames a second again), Back+LT held rewinds;
# - held rewinds go on through Settings' pause under the HUD's icon (Back+LT,
#   and the panel's Hold to rewind under a scripted controller ray's
#   trigger), Settings hold the game again once a rewind is let go, and
#   Settings closed with the trigger still held let go of Hold to rewind
#   (the game plays on behind the closed panel).
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

# The boot run with wider framing on (the title movie's frames rebuilt
# without its sides).
cp "$out/config/xenogears-ex-machina/settings.json" "$out/settings.saved"
echo '{"graphics": {"profile": "Hd2d", "widescreen": true}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --exit-at-frame "$frames" --xr-capture "$out/eyes" >"$out/running.log" 2>"$out/running.err"
cp "$out/settings.saved" "$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --exit-at-frame "$frames" --xr-pause 60,120 >"$out/paused.log" 2>"$out/paused.err"
# The title movie under the Original movies setting.
cp "$out/config/xenogears-ex-machina/settings.json" "$out/settings.saved"
echo '{"graphics": {"profile": "Hd2d", "filtering": {"movies": "Original"}}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --exit-at-frame "$frames" >"$out/original.log" 2>"$out/original.err"
cp "$out/settings.saved" "$out/config/xenogears-ex-machina/settings.json"
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
# The same under the default profile (Modern: no effects, so each eye's
# picture is the frame itself, whose alpha is the PS1's mask bit): on at
# 150, the Pause grab at 180, the underhand grab at 195 and 210 while the
# game is paused (off, then on), both eyes captured at 230.
echo '{"graphics": {"profile": "Modern"}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/inspect.json" --exit-after-frames 240 \
    --xr-hands "150:inspect,180:pause,195:inspect,210:inspect" --xr-capture "$out/modern" --xr-capture-at 230 >"$out/modern.log" 2>"$out/modern.err"
# The diorama, with the same log as the inspection run (its RAM checked
# against the same headless run): both eyes captured at headset frames 150
# and 250, the app's Recenter at 60, Monado's recentring once the session
# has drawn 100 frames, the app's Recenter again at 220. Field 12's dialogue
# on the pane (from about headset frame 90), captured at 120. Then prayer hands
# at 60 open the settings panel above the table, a pinch on its Close at 90
# closes it, and a pinch through the table at a floor point of field 30 (a
# world point: the gaze aims where the table shows it) walks there.
echo '{"graphics": {"profile": "Hd2d", "xr": {"view": "Diorama"}}}' >"$out/config/xenogears-ex-machina/settings.json"
(
    for _ in $(seq 600); do grep -q '^xrframe n=100 ' "$out/diorama.log" 2>/dev/null && break; sleep 0.1; done
    monado-ctl -c >"$out/monado-ctl.log" 2>&1 || echo "monado-ctl exit $?" >>"$out/monado-ctl.log"
) &
recentring=$!
"$desktop" --xr --disc "$disc" --script "$out/inspect.json" --exit-at-frame 1200 --xr-recenter-at 60,220 \
    --xr-capture "$out/diorama" --xr-capture-at 150,250 >"$out/diorama.log" 2>"$out/diorama.err"
wait "$recentring"
"$desktop" --xr --disc "$disc" --script "$out/field.json" --exit-at-frame 1000 --xr-capture "$out/field-diorama" --xr-capture-at 120 \
    >"$out/field-diorama.log" 2>"$out/field-diorama.err"
echo '{"graphics": {"xr": {"view": "Diorama"}}, "assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/field30.json" --exit-after-frames 330 \
    --xr-hands "60:prayer,90:pinch@panel:410/30,150:pinch@world:88.67/0/-55.33" >"$out/diorama-walk.log" 2>"$out/diorama-walk.err"
# Save states and time controls, the player's own play (no session log)
# from field 30, which an agent's command loads at the first headset frame.
# Prayer hands open Settings at headset frame 20 (the game held by their
# pause), pinches on the 440x560 panel choose Saves (298,98) and its Save
# state (122,170); prayer hands close it at 80, open it at 160, and Load
# state (314,170) restores it; Time (256,98), its Start fast-forward
# (347,395), and Settings closed at 240. The scripted controller buttons
# hold Back+RT at 350 (fast-forward off) and Back+LT from 460 to 500 (a
# rewind). Then the state file the panel wrote, restored by xem-headless.
printf '{"cmd":"enter_field","map":30}\n{"cmd":"frames","count":400}\n{"cmd":"state_save","path":"%s"}\n' "$out/field30.xemstate" |
    "$headless" --disc "$disc" --control >"$out/saves-prep.log" 2>"$out/saves-prep.err"
echo '{"assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
printf '{"cmd":"state_load","path":"%s"}\n' "$out/field30.xemstate" |
    "$desktop" --xr --control --disc "$disc" --exit-after-frames 560 \
    --xr-hands "20:prayer,40:pinch@panel:298/98,60:pinch@panel:122/170,80:prayer,160:prayer,180:pinch@panel:314/170,200:pinch@panel:256/98,220:pinch@panel:347/395,240:prayer" \
    --xr-pad "350:Back+RT,352:none,460:Back+LT,500:none" >"$out/saves.log" 2>"$out/saves.err"
printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"memory_hash"}\n' "$out/config/xenogears-ex-machina/state.xemstate" |
    "$headless" --disc "$disc" --control >"$out/saves-headless.log" 2>"$out/saves-headless.err"
# Held rewinds through Settings' pause, from the same state: Back+Start
# opens Settings at headset frame 200 and Back+LT is held from 205 to 211;
# then the right controller's ray (scripted: Monado's controllers point
# where no test chooses) clicks Time (256,98) at 220 and holds its
# Hold to rewind (355,325) with the trigger (R1) from 230, Back+Start
# closes Settings at 245 with the trigger still held, and it lets go at 270.
printf '{"cmd":"state_load","path":"%s"}\n' "$out/field30.xemstate" |
    "$desktop" --xr --control --disc "$disc" --exit-after-frames 300 \
    --xr-pad "200:Back+Start,202:none,205:Back+LT,211:none,220:R1@panel:256/98,222:none@panel:256/98,230:R1@panel:355/325,245:R1+Back+Start@panel:355/325,247:R1@panel:355/325,270:none" \
    >"$out/held.log" 2>"$out/held.err"

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
    # The RAM the run ended with: its last memory hash (each app
    # operation's line before it carries the RAM just after that operation).
    hashes = [m.group(1) for line in open(f"{out}/{name}") if (m := re.search(r"memory_hash ([0-9a-f]{16})", line))]
    if not hashes:
        sys.exit(f"xr-game-smoke: no memory hash in {name}")
    return hashes[-1]

problems = []
def check(ok, message):
    if not ok:
        problems.append(message)

headless = [json.loads(l) for l in open(f"{out}/headless.log") if l.startswith("{") or l.startswith('"')]
reference = next(v for v in headless if isinstance(v, str))
for name in ["running.log", "paused.log", "original.log"]:
    check(final_hash(name) == reference, f"{name}: RAM at game frame {frames} {final_hash(name)} != headless {reference}")

# The title movie: its pictures from game frame 401 (the display shows
# black 24-bit frames from 371), its frame 2 there.
def movie_rows(name):
    return [r for r in records(name) if int(r["game_frames"]) >= 405]
rebuilt, original = movie_rows("running.log"), movie_rows("original.log")
check(len(rebuilt) >= 30 and all(r["movie"] == "1" for r in rebuilt),
      f"Enhanced: {sum(r['movie'] == '1' for r in rebuilt)} of {len(rebuilt)} movie frames rebuilt")
# As many pixels across as the default screen (3 m wide, 2.5 m ahead)
# spans in the 320-pixel eye, whose field of view's tangents the log gives,
# the display's 320x216 shape kept.
left_, right_ = (float(t) for t in rebuilt[0]["fov"].split(",")[:2]) if rebuilt else (-1.0, 1.0)
across = min(320, round(3.0 / 2.5 * 320 / (right_ - left_)))
spanned = f"{across}x{round(across * 216 / 320)}"
sizes = {r["picture"] for r in rebuilt}
check(sizes == {spanned}, f"Enhanced: rebuilt movie frames are made at the screen's span in an eye, {spanned}, not {sizes}")
# Wider framing is on in this run: the renderer's own frames before the
# movie (view=1) have its sides (display pixels each side of a 16:9
# screen), a rebuilt movie frame and the device's own pictures (view=0)
# none.
booting = records("running.log")
sides = {r["side"] for r in booting if r["view"] == "1" and r["movie"] == "0"}
check(bool(sides) and all(int(s) > 0 for s in sides), f"wider framing: the renderer's frames before the movie have sides, not {sides}")
check(all(r["side"] == "0" for r in rebuilt), f"wider framing: rebuilt movie frames have no sides: {sorted({r['side'] for r in rebuilt})}")
check(all(r["side"] == "0" for r in booting if r["view"] == "0"), "wider framing: the device's own pictures have no sides")
check(len(original) >= 30 and all(r["movie"] == "0" and r["picture"] == "320x216" for r in original),
      f"Original: movie frames are the device's own pictures: {[(r['movie'], r['picture']) for r in original][:3]}")
print(f"movies: {len(rebuilt)} headset frames rebuilt at {', '.join(sizes)} (the screen's span: {spanned}), sides {sorted({r['side'] for r in rebuilt})} "
      f"(the renderer's frames before: {sorted(sides)}); under Original {len(original)} at 320x216")

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
def means(image, n=(16, 12)):
    # The mean colours of a grid of blocks over the whole image.
    w, h, rows_ = image
    blocks = []
    for j in range(n[1]):
        for i in range(n[0]):
            xs, ys = range(i * w // n[0], (i + 1) * w // n[0]), range(j * h // n[1], (j + 1) * h // n[1])
            blocks.append([sum(rows_[y][4 * x + k] for y in ys for x in xs) / (len(xs) * len(ys)) for k in range(3)])
    return blocks
def off(eye, picture):
    # How far an eye's view is from the picture the renderer drew for it
    # (over the whole view): the median of the grid's blocks' mean colour
    # differences, levels (the interface's pane covers a few blocks).
    d = sorted(sum(abs(a - b) for a, b in zip(p, q)) / 3 for p, q in zip(means(eye), means(picture)))
    return d[len(d) // 2]
def colours(image):
    return len({row[x:x + 3] for row in image[2] for x in range(0, len(row), 4)})
# Each eye as captured and the picture the renderer drew for it, under
# HD-2D twice while paused and under the default profile once.
eyes = {f"{n} {side}": (png(f"{out}/inspect/{n}/{side}.png"), png(f"{out}/inspect/{n}/{side}-picture.png"))
        for n in (200, 225) for side in ("left", "right")}
eyes.update({f"Modern {side}": (png(f"{out}/modern/230/{side}.png"), png(f"{out}/modern/230/{side}-picture.png"))
             for side in ("left", "right")})
for name, (eye, picture) in eyes.items():
    check(covered(eye[2]) > 0.9, f"{name}: the eye's picture does not cover the view ({covered(eye[2]):.2f})")
    check(colours(picture) > 100, f"{name}: the eye's picture holds {colours(picture)} colours, not the room")
    check(off(eye, picture) <= 4, f"{name}: the eye shows its picture {off(eye, picture):.1f} levels off its colours")
check(eyes["200 left"][1] != eyes["200 right"][1], "both eyes see the same picture from within the scene")
check(eyes["200 left"][1] != eyes["225 left"][1], "the paused view did not follow the head")
print(f"inspect: on {on[0]}..{on[-1]}, {len(within)} frames from within the scene, {len(paused)} paused at game frame "
      f"{paused[0]['game_frames']}; eyes " + ", ".join(f"{k}: cover {covered(e[2]):.0%}, {colours(p)} colours, "
      f"{off(e, p):.1f} levels off" for k, (e, p) in eyes.items()))
# Pacing and the presentation's time on its thread, from within the scene and as the screen.
def pace(select):
    pairs = [(a, b) for a, b in zip(rows, rows[1:]) if select(a) and select(b)]
    gaps = sorted(int(b["wall_us"]) - int(a["wall_us"]) for a, b in pairs)
    present = sorted(int(b["present_us"]) for _, b in pairs)
    late = sum(1 for g in gaps if g > 1.5 * period / 1000)
    return f"interval p50 {gaps[len(gaps) // 2] / 1000:.2f} ms, p99 {gaps[int(len(gaps) * 0.99)] / 1000:.2f} ms, {late} of {len(gaps)} late; presenting p50 {present[len(present) // 2] / 1000:.2f} ms, p95 {present[int(len(present) * 0.95)] / 1000:.2f} ms"
print(f"inspect pacing, the screen: {pace(lambda r: r['within'] == '0' and int(r['n']) > 20)}")
print(f"inspect pacing, from within: {pace(lambda r: r['within'] == '1')}")
# Under the default profile the underhand grab turns the view off and on
# again while the player's Pause holds the game.
rows = records("modern.log")
by = {int(r["n"]): r for r in rows}
paused = [r for r in rows if r["player_pause"] == "1"]
check(len(paused) >= 40 and len({r["game_frames"] for r in paused}) == 1, f"Modern: the Pause grab held the game for {len(paused)} frames")
gone = [int(r["n"]) for r in paused if r["fp"] == "0"]
check(bool(gone) and 195 <= gone[0] <= 208 and 209 <= gone[-1] <= 222 and len(gone) == gone[-1] - gone[0] + 1,
      f"Modern: the grabs while paused turned the view off and on again: off {gone[:1]}..{gone[-1:]}")
check(all(by[n]["fp"] == "1" and by[n]["within"] == "1" for n in range(225, 235)), "Modern: the view was not back from within the scene while paused")
print(f"modern: paused from frame {paused[0]['n'] if paused else '-'}, off while paused {gone[:1]}..{gone[-1:]}")

# The diorama.
def luma(image):
    w, h, rows_ = image
    return w, h, [[(rows_[y][4 * x] * 3 + rows_[y][4 * x + 1] * 6 + rows_[y][4 * x + 2]) / 10 for x in range(w)] for y in range(h)]
def disparities(left, right, bands=8, reach=40):
    # Per band of rows with texture: the horizontal shift that maps the
    # right eye's picture on the left's best (right[x] ~ left[x + s]; a
    # nearer point shifts more), where the best is a clear one.
    w, h, l = luma(left)
    _, _, r = luma(right)
    shifts = []
    for b in range(bands):
        ys, xs = range(b * h // bands, (b + 1) * h // bands), range(reach, w - reach)
        values = [l[y][x] for y in ys for x in xs]
        mean = sum(values) / len(values)
        spread = (sum((v - mean) ** 2 for v in values) / len(values)) ** 0.5
        sad = [sum(abs(r[y][x] - l[y][x + s]) for y in ys for x in xs) / (len(ys) * len(xs)) for s in range(-reach, reach + 1)]
        best = min(range(len(sad)), key=lambda i: sad[i])
        if spread > 5 and sorted(sad)[0] < 0.6 * sorted(sad)[len(sad) // 2]:
            shifts.append((b, best - reach))
    return shifts
rows = records("diorama.log")
by = {int(r["n"]): r for r in rows}
shown = [r for r in rows if r["diorama"] == "1"]
check(len(shown) >= len(rows) // 2, f"the diorama showed in only {len(shown)} of {len(rows)} headset frames")
check(all(r["xr_view"] == "diorama" for r in rows), "the headset view was not the diorama")
check(final_hash("diorama.log") == inspected, f"diorama.log: RAM at game frame 1200 {final_hash('diorama.log')} != headless {inspected}")
def covered_off(eye, picture, n=(16, 12)):
    # Over the blocks the eye's diorama picture covers whole (alpha 255):
    # how many, and the median of the eye's mean colour difference from it.
    w, h, e = eye
    _, _, p = picture
    diffs = []
    for j in range(n[1]):
        for i in range(n[0]):
            xs, ys = range(i * w // n[0], (i + 1) * w // n[0]), range(j * h // n[1], (j + 1) * h // n[1])
            if all(p[y][4 * x + 3] == 255 for y in ys for x in xs):
                mean = lambda rows_, k: sum(rows_[y][4 * x + k] for y in ys for x in xs) / (len(xs) * len(ys))
                diffs.append(sum(abs(mean(e, k) - mean(p, k)) for k in range(3)) / 3)
    diffs.sort()
    return len(diffs), diffs[len(diffs) // 2] if diffs else 255
for n in (150, 250):
    check(by[n]["diorama"] == "1" and by[n]["share"] == "0.000", f"frame {n}: the diorama alone was not shown ({by[n]['diorama']}, {by[n]['share']})")
    d = disparities(png(f"{out}/diorama/{n}/left.png"), png(f"{out}/diorama/{n}/right.png"))
    shifts = [s for _, s in d]
    check(len(d) >= 3 and max(shifts) - min(shifts) >= 5 and d[-1][1] > d[0][1],
          f"frame {n}: the eyes' disparity does not grow with nearness: {d}")
    # Each eye shows its diorama picture in the renderer's colours.
    for side in ("left", "right"):
        blocks, levels = covered_off(png(f"{out}/diorama/{n}/{side}.png"), png(f"{out}/diorama/{n}/{side}-diorama.png"))
        check(blocks >= 20 and levels <= 4, f"frame {n} {side}: the eye shows its diorama {levels:.1f} levels off over {blocks} covered blocks")
    print(f"diorama {n}: disparity by band of rows (top first) {d}; the eyes {covered_off(png(f'{out}/diorama/{n}/left.png'), png(f'{out}/diorama/{n}/left-diorama.png'))} (covered blocks, levels off)")
flat = disparities(png(f"{out}/field/left.png"), png(f"{out}/field/right.png"))
check(len(flat) >= 3 and max(s for _, s in flat) - min(s for _, s in flat) <= 1, f"the screen's disparity is not one in every row: {flat}")
print(f"screen (field 12): disparity by band of rows {flat}")
# The app's Recenter at 60 puts the seat at the head; Monado's recentring
# reaches the session and puts the seat, moved until then, back at LOCAL's
# origin on the frame it does; the second Recenter at 220 puts it at the
# head again.
def place(r):
    return [float(v) for v in r["seat"].split(";")[0].split(",")], float(r["seat"].split(";")[1])
def recentred_at(count):
    r = next((r for r in rows if r["recenters"] == str(count)), None)
    if r is None:
        return None, f"no Recenter number {count}"
    head = [float(v) for v in r["head"].split(";")[0].split(",")]
    seat, _ = place(r)
    return r, f"Recenter {count} at frame {r['n']}: seat {seat} for the head {head}" if any(abs(a - b) >= 0.001 for a, b in zip(head, seat)) else None
events = [l for l in open(f"{out}/diorama.log") if l.startswith("event reference_space_change")]
check(len(events) >= 1, f"Monado's recentring did not reach the session ({open(f'{out}/monado-ctl.log').read().strip()!r})")
first, problem = recentred_at(1)
check(problem is None and first is not None and int(first["n"]) == 60, problem or f"the first Recenter came at {first['n']}, not 60")
reset = next((r for r in rows if r["resets"] != "0"), None)
before = by.get(int(reset["n"]) - 1) if reset else None
if reset and before:
    (moved, yaw), (back, back_yaw) = place(before), place(reset)
    check(int(before["recenters"]) == 1 and (max(abs(v) for v in moved) > 0.001 or abs(yaw) > 0.001),
          f"the seat had not moved before LOCAL's recentring: {before['seat']} at frame {before['n']}")
    check(max(abs(v) for v in back) < 1e-4 and abs(back_yaw) < 1e-4 and int(reset["n"]) < 220,
          f"the seat after the reference changed: {reset['seat']} at frame {reset['n']}")
check(reset is not None and before is not None, "no frame shows the reference's change")
second, problem = recentred_at(2)
check(problem is None and second is not None and int(second["n"]) == 220, problem or f"the second Recenter came at {second['n']}, not 220")
print(f"diorama: shown in {len(shown)} of {len(rows)} headset frames; Recenter at {first and first['n']} (seat {first and first['seat']}), "
      f"{len(events)} recentring of LOCAL at frame {reset and reset['n']} (seat {before and before['seat']} before, {reset and reset['seat']} after), "
      f"Recenter at {second and second['n']} (seat {second and second['seat']}); RAM {final_hash('diorama.log')}")
print(f"diorama pacing: {pace(lambda r: r['diorama'] == '1')}")
# Field 12's dialogue on the pane above the table: each eye shows the
# pane's picture where the pane stands. Each eye pixel's centre is taken
# through the eye's projection and view onto the pane's plane; where it
# meets the pane on an opaque texel (the window's frame, text and the
# portrait: its body is translucent), clear of the texels' edges, the eye
# shows that texel's colour (the pane is drawn unfiltered over the
# diorama, at the diorama's share).
rows = records("field-diorama.log")
by = {int(r["n"]): r for r in rows}
line = next((l for l in open(f"{out}/field-diorama.log") if l.startswith("capture pane ")), None)
check(line is not None, "no capture of the diorama's pane in field 12")
if line:
    fields = dict(f.split("=", 1) for f in line.split()[2:])
    n = int(fields["n"])
    check(by[n]["diorama"] == "1" and by[n]["share"] == "0.000" and '"dialogue"' in by[n]["context"],
          f"field 12 at frame {n}: no dialogue on the diorama alone ({by[n]['diorama']}, {by[n]['share']}, {by[n]['context']})")
    numbers = lambda text: [float(v) for v in text.split(",")]
    def apply(m, p):
        # A column-major matrix times a point.
        return [sum(m[4 * c + r] * p[c] for c in range(4)) for r in range(4)]
    pose, (pw, ph) = numbers(fields["pose"]), numbers(fields["size"])
    paw, pah, pane = png(f"{out}/field-diorama/{n}/pane.png")
    opaque = sum(1 for row in pane for x in range(3, len(row), 4) if row[x] == 255) / (paw * pah)
    check(opaque > 0.05, f"the pane holds no interface ({opaque:.1%} of it opaque)")
    origin, across, up = apply(pose, [0, 0, 0, 1]), apply(pose, [1, 0, 0, 0]), apply(pose, [0, 1, 0, 0])
    for side in ("left", "right"):
        w, h, eye = png(f"{out}/field-diorama/{n}/{side}.png")
        vp = numbers(fields[side])
        c0, cx, cy = apply(vp, origin), apply(vp, across), apply(vp, up)
        corners = [apply(vp, [origin[k] + (u - 0.5) * pw * across[k] + (0.5 - v) * ph * up[k] for k in range(3)] + [1.0])
                   for u, v in ((0, 0), (1, 0), (0, 1), (1, 1))]
        check(all(c[3] > 0 for c in corners), f"field 12 {side}: the pane is behind the eye")
        xs = [(c[0] / c[3] + 1) / 2 * w for c in corners if c[3] > 0] or [0]
        ys = [(1 - c[1] / c[3]) / 2 * h for c in corners if c[3] > 0] or [0]
        diffs = []
        for py in range(max(0, int(min(ys))), min(h, int(max(ys)) + 1)):
            for px in range(max(0, int(min(xs))), min(w, int(max(xs)) + 1)):
                # Where the pixel's centre meets the pane: a point a across and
                # b up from its middle (metres), its clip position the pixel's.
                nx, ny = (px + 0.5) / w * 2 - 1, 1 - (py + 0.5) / h * 2
                a11, a12, a21, a22 = cx[0] - nx * cx[3], cy[0] - nx * cy[3], cx[1] - ny * cx[3], cy[1] - ny * cy[3]
                b1, b2 = nx * c0[3] - c0[0], ny * c0[3] - c0[1]
                det = a11 * a22 - a12 * a21
                if abs(det) < 1e-12:
                    continue
                a, b = (b1 * a22 - a12 * b2) / det, (a11 * b2 - b1 * a21) / det
                tu, tv = (a / pw + 0.5) * paw, (0.5 - b / ph) * pah
                if not (0 <= tu < paw and 0 <= tv < pah) or min(tu % 1, 1 - tu % 1, tv % 1, 1 - tv % 1) < 0.05:
                    continue
                texel = pane[int(tv)][4 * int(tu):4 * int(tu) + 4]
                if texel[3] == 255:
                    diffs.append(max(abs(eye[py][4 * px + k] - texel[k]) for k in range(3)))
        within = sum(1 for d in diffs if d <= 2) / max(1, len(diffs))
        check(len(diffs) >= 100 and within >= 0.98, f"field 12 {side}: {within:.1%} of {len(diffs)} eye pixels on the pane's opaque texels show them within 2 levels")
        print(f"field 12 diorama, {side} eye: the pane ({opaque:.0%} of it opaque) spans {min(xs):.0f}-{max(xs):.0f} x {min(ys):.0f}-{max(ys):.0f}; "
              f"{within:.1%} of {len(diffs)} pixels on its opaque texels within 2 levels (worst {max(diffs, default=255)})")
# Prayer hands open the settings panel above the table (its quad layer) and
# a pinch on its Close closes it, the game held meanwhile; then a pinch
# through the table walks Fei to the floor point gazed at.
rows = records("diorama-walk.log")
panel = [r for r in rows if r["layers"] == "2"]
opened = [int(r["n"]) for r in panel]
check(bool(opened) and 60 <= opened[0] <= 80 and opened[-1] < 100 and len(opened) == opened[-1] - opened[0] + 1,
      f"prayer hands showed the panel above the table and a pinch on Close closed it: shown {opened[:1]}..{opened[-1:]}")
check(len({r["game_frames"] for r in panel}) == 1, "the game ran while the panel was open")
check(all(r["diorama"] == "1" for r in panel), "the diorama gave way while the panel was open")
walked = [r["assist"] for r in rows if int(r["n"]) >= 150]
pinch = [int(r["n"]) for r in rows if "Pinch" in r["gestures"] and int(r["n"]) >= 150]
check(bool(pinch) and all(r["diorama"] == "1" for r in rows if int(r["n"]) in pinch), f"the pinch did not come while the diorama showed: {pinch}")
check("moving" in walked and walked[-1] == "arrived", f"the pinch through the diorama did not walk Fei there: {walked[:5]}..{walked[-3:]}")
print(f"diorama walk: the panel above the table shown {opened[:1]}..{opened[-1:]}; pinch at {pinch}, {walked[:2]}..{walked[-1:]}")

# Save states and time controls: each app operation's line carries the
# game's vblank and memory hash just after it.
lines = open(f"{out}/saves.log").read().splitlines()
replies = [json.loads(l) for l in lines if l.startswith("{")]
prep = [json.loads(l) for l in open(f"{out}/saves-prep.log") if l.startswith("{")]
at = next((p["frames"] for p in reversed(prep) if "frames" in p), None)
check(len(replies) == 1 and "error" not in replies[0] and replies[0].get("frames") == at,
      f"the agent's state_load in the headset (the state at frame {at}): {str(replies)[:200]}")
def operations(action):
    found = []
    for line in lines:
        m = re.match(rf"app {re.escape(action)} game_vblank=(\d+) memory_hash ([0-9a-f]{{16}}) (ok|error): (.*)", line)
        if m:
            found.append((int(m.group(1)), m.group(2), m.group(3), m.group(4)))
    return found
saved, loaded = operations("Action(SaveState)"), operations("Action(LoadState)")
check(len(saved) == 1 and saved[0][2] == "ok", f"the panel's Save state: {saved}")
check(len(loaded) == 1 and loaded[0][2] == "ok", f"the panel's Load state: {loaded}")
saved = saved[0] if saved else (0, "-", "", "")
loaded = loaded[0] if loaded else (0, "-", "", "")
check(loaded[:2] == saved[:2], f"Load state left vblank {loaded[0]}, RAM {loaded[1]}, not the save's {saved[0]}, {saved[1]}")
notices = [l for l in lines if l.startswith("hud notice=")]
check(notices == ['hud notice="State saved"', 'hud notice="State loaded"'], f"the HUD's results: {notices}")
rows = records("saves.log")
by = {int(r["n"]): r for r in rows}
# Each result shows for four seconds of headset frames.
shown = [int(r["n"]) for r in rows if "notice" in r["hud"].split(",")]
check(abs(len(shown) - 8e9 / period) <= 0.2 * 8e9 / period and all(60 <= n <= 300 for n in shown),
      f"the HUD showed the results for {len(shown)} frames: {shown[:1]}..{shown[-1:]}")
held = [r for r in rows if 25 <= int(r["n"]) <= 75]
check(all(r["layers"] == "2" and r["paused"] == "1" for r in held) and len({r["game_frames"] for r in held}) == 1,
      "Settings did not hold the game while the state was saved")
written = [json.loads(l) for l in open(f"{out}/saves-headless.log") if l.startswith("{") or l.startswith('"')]
restored = next((v for v in written if isinstance(v, str)), None)
check(restored == saved[1], f"the state file the panel wrote restores to RAM {restored}, not the save's {saved[1]}")
toggles = operations("Action(ToggleFastForward)")
check(len(toggles) == 2 and toggles[0][3].startswith("Fast-forward on") and toggles[1][3] == "Fast-forward off.",
      f"Start fast-forward, then the Back+RT chord: {toggles}")
# The rate while fast-forward runs (Settings closed, until the chord),
# against the factor the session paces it at; then 1x again.
running = [r for r in rows if r["ff"] not in ("0", "uncapped") and r["layers"] == "1"]
check(len(running) > 60, f"fast-forward ran {len(running)} headset frames with Settings closed")
if len(running) > 60:
    a, b = running[10], running[-2]
    rate = (int(b["game_frames"]) - int(a["game_frames"])) / ((int(b["pdt_ns"]) - int(a["pdt_ns"])) / 1e9)
    factors = [int(r["ff"]) for r in running[10:-1]]
    factor = sum(factors) / len(factors)
    check(factor >= 2 and 0.8 <= rate / (60 * factor) <= 1.1,
          f"fast-forward ran the game {rate:.0f} frames a second at a mean factor of {factor:.2f} (paced {min(factors)}-{max(factors)})")
    print(f"fast-forward: {rate:.0f} game frames a second at a mean factor of {factor:.2f} (paced {min(factors)}-{max(factors)} of the 10x setting), "
          f"{rate / (60 * factor):.2f} of 60 times it")
off = [r for r in rows if 360 <= int(r["n"]) <= 455]
check(all(r["ff"] == "0" for r in off), "fast-forward stayed on after the chord")
rate = (int(off[-1]["game_frames"]) - int(off[0]["game_frames"])) / ((int(off[-1]["pdt_ns"]) - int(off[0]["pdt_ns"])) / 1e9)
check(55 <= rate <= 65, f"after the chord the game ran {rate:.1f} frames a second")
rewound = [r for r in rows if 461 <= int(r["n"]) < 500]
check(any("rewind" in r["hud"].split(",") for r in rewound), "the HUD showed no rewind while Back+LT was held")
before, after = int(by[459]["game_frames"]), int(by[501]["game_frames"])
check(after < before - 120, f"Back+LT held for 2 s rewound from game frame {before} to {after}")
check(int(by[555]["game_frames"]) > after + 60, "the game did not play on after the rewind")
check([o[2] for o in operations("Rewind(true)") + operations("Rewind(false)")] == ["ok", "ok"], "the rewind chord's requests")
print(f"saves: saved and loaded at vblank {saved[0]} (RAM {saved[1]}, the state file's too), results shown {len(shown)} frames; "
      f"1x {rate:.1f} frames a second after the chord; rewound from game frame {before} to {after}")

# Held rewinds through Settings' pause: each rewind request with the
# headset frame it ran in (its app line comes before that frame's line).
rows = records("held.log")
by = {int(r["n"]): r for r in rows}
def gf(n):
    return int(by[n]["game_frames"])
rewinds, last = [], -1
for line in open(f"{out}/held.log"):
    if line.startswith("xrframe "):
        last = int(line.split()[1].split("=", 1)[1])
    m = re.match(r"app (\w*Rewind)\((true|false)\) game_vblank=\d+ memory_hash [0-9a-f]{16} (ok|error)", line)
    if m:
        rewinds.append((last + 1, m.group(1), m.group(2), m.group(3)))
check([r[1:] for r in rewinds] == [("Rewind", "true", "ok"), ("Rewind", "false", "ok"), ("UiRewind", "true", "ok"), ("UiRewind", "false", "ok")],
      f"the held run's rewinds: {rewinds}")
if len(rewinds) == 4:
    (chord_on, *_), (chord_off, *_), (held_on, *_), (held_off, *_) = rewinds
    # Back+LT with Settings shown rewinds through their pause under the
    # HUD's icon; let go, they hold the game again.
    during = [by[n] for n in range(chord_on, chord_off)]
    check((chord_on, chord_off) == (205, 211) and all(r["layers"] == "2" and r["paused"] == "0" for r in during)
          and any("rewind" in r["hud"].split(",") for r in during) and gf(chord_off - 1) < gf(chord_on - 1),
          f"Back+LT with Settings shown, frames {chord_on}-{chord_off}: game frame {gf(chord_on - 1)} to {gf(chord_off - 1)}, "
          f"layers/paused/hud {[(r['layers'], r['paused'], r['hud']) for r in during]}")
    held = [by[n] for n in range(chord_off, 220)]
    check(all(r["paused"] == "1" for r in held) and len({r["game_frames"] for r in held}) == 1,
          "Settings did not hold the game again once Back+LT was let go")
    # The panel's Hold to rewind under the ray's trigger, the same.
    during = [by[n] for n in range(held_on, 245)]
    check(230 <= held_on < 240 and all(r["layers"] == "2" and r["paused"] == "0" for r in during)
          and any("rewind" in r["hud"].split(",") for r in during) and gf(244) < gf(held_on - 1),
          f"Hold to rewind from headset frame {held_on}: game frame {gf(held_on - 1)} to {gf(244)}, "
          f"layers/paused/hud {[(r['layers'], r['paused'], r['hud']) for r in during]}")
    # Settings closed with the trigger still held let go of it then: the
    # game plays on, not rewinding behind the closed panel.
    hidden = range(245, 270)
    check(held_off == 245 and all(by[n]["layers"] == "1" for n in hidden)
          and all(gf(n + 1) >= gf(n) for n in hidden) and gf(269) > gf(245) + 30,
          f"Settings closed at 245 let go of Hold to rewind at headset frame {held_off}; then game frames {[gf(n) for n in hidden]}")
    print(f"held: Back+LT with Settings shown rewound game frame {gf(chord_on - 1)} to {gf(chord_off - 1)}; Hold to rewind "
          f"from headset frame {held_on} rewound {gf(held_on - 1)} to {gf(244)}; Settings closed at 245 let it go, the game on "
          f"to {gf(269)} by 269 with the trigger still held")

if problems:
    print("xr-game-smoke: FAILED\n  " + "\n  ".join(problems))
    sys.exit(1)
print("xr-game-smoke: passed")
EOF
