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
#   settings panel beside the diorama's pane (off it as seen from the seat)
#   and a pinch on its Close closes it, and a pinch with the gaze through
#   the table on a floor point walks Fei there; in field 12 both eyes show
#   the pane above the table with the game's dialogue on it, in the pane's
#   colours; an agent's settings_apply (--control) switches Graphics' headset
#   view to the screen in the session, shown at once, and back to the
#   diorama, eased in from the screen;
# - hands on the diorama, the player's own play from a field 30 state:
#   either hand's grabs as on the screen, the diorama showing through them;
#   the player's Pause holds the game while the head moves, the eyes
#   following it and seeing the table in depth; the underhand grab looks
#   from within the scene and gives the diorama back at once at the scale
#   and place it had; inspection stops a walk through the table under way
#   and none restarts; prayer hands open and close the settings panel
#   beside the pane, the game held; with the diorama as the view the title movie
#   (its rebuilt frames in their own shape) and the title show on the
#   screen; field 30's menu, the Mountain Path's random battle and its end,
#   and Lahan left for the world map and a field entered from it each go
#   from the diorama to the screen and back to a diorama (the battle's, the
#   world map's, the field's), and map 25's story movie shows on the screen
#   between its field's dioramas, the settings panel's place beside the
#   pane after a frame that showed the diorama and below the screen after
#   one that showed the screen; in every run with the diorama as the view,
#   the view eases between the diorama and the screen over 0.3 s;
# - a battle on foot (its log attacking every 30 frames) and the world map
#   (its vehicle driven on, at 25%) as dioramas: the diorama shows in every
#   headset frame from its first; the battle's table stands where its
#   start view looks during its opening, then on its combatants to the
#   end, whatever its camera does; the world map's stays on its camera's
#   target as the map moves under it; the eyes see 3D in the renderer's
#   colours and the RAM is xem-headless's;
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
#   (the game plays on behind the closed panel); with hands alone, a pinch
#   held on Hold to rewind holds it from the pinch's start to its end, the
#   gaze leaving the button meanwhile (the panel's pointer moves nowhere
#   while a pinch holds it), and an agent's `app` command recentres;
# - the gameplay options in the headset (an agent on --control, the game
#   running): the settings' defaults reach the session (random battles off
#   on xem-core's platforming maps, every other option the original's), and
#   settings_apply's RandomBattles Original is acknowledged and reaches it;
#   in field 12's dialogue a pinch aimed at AUTO beside the screen turns
#   auto-advance on through the settings (the app's toggle, the settings
#   and the session's dwell), pressing none of the game's buttons (the
#   prompt still waits, counted toward the dwell every vertical blank since
#   the toggle), and auto-advance then turns the page;
# - first-person inspection on the controllers in field 30 (that state, the
#   player's own play): Back released alone toggles it, held directions,
#   Jump and a camera turn leave the character where it stands while it
#   shows (it walks once it is off), the right stick turns the view 30
#   degrees a push, and an agent's settings_apply to the Original renderer
#   ends it;
# - first person by an agent's command in field 30 (the player's own play
#   from that state): `first_person` turns the view on looking 30 degrees
#   right from within the scene, a walk under way stops, `navigate` and a
#   walk by `act` are refused while it shows, `status` reports it, and it
#   turns the view off; Toggle first-person view's chord (the right stick's
#   click with the right trigger, scripted) turns it on and off;
# - state interchange: that state loaded in the headset (--load-state) and
#   played on 600 game frames under HD-2D has xem-headless's RAM at the
#   300th, where the headset writes its state (--save-state-at), and at
#   the 600th; the headset's state is xem-headless's at that frame byte for
#   byte (the screen without wider framing asks for no reveals), loads
#   natively to its RAM and goes on to the same end;
# - headset qualification: through the player's Pause and Settings' own the
#   head stays tracked and the eyes' pictures follow it while the game
#   stands still; either hand's overhand grab pauses and resumes, both
#   hands' together toggle once, and a sideways grab, a fist turned while
#   closed and a hand found already closed toggle nothing; two whole-hand
#   pinches scale the screen (to 2 and back, the runtime's pinch held
#   through one), owning both hands (no grab, pinch or walk) while the
#   game runs on and the settings panel moves down out of the screen's
#   way; a pinch after it walks; the underhand grab of the left hand turns
#   inspection on and off in the field and nothing in its menu;
# - a scene tour, both eyes captured: the title movie, the title (field
#   490), field 30 and its menu, field 12's dialogue, the world map and
#   battles on foot and in Gears are on the screen in front of the eyes;
#   the grab looks from within the scene only in the field and the
#   dialogue (and gives the screen back), and turns nothing on elsewhere;
#   the dialogue's and the Gears' RAM is xem-headless's;
# - on the screen view the app's Recenter puts the seat at the head and
#   Monado's recentring of LOCAL puts it back at LOCAL's origin, the screen
#   in front of the eyes after each; the world map's control deck keeps
#   as many keys, as wide and as far beside the screen at every scale;
# - two-hand scaling of the diorama (scripted whole-hand pinches spread,
#   then contract): the table's scale follows the hands live, the camera's
#   target stays at the table's anchor within a millimetre, each gesture's
#   scale is committed to the setting once when the hands let go, a key
#   beside the pane keeps its width, nothing else happens and the RAM is
#   xem-headless's; Monado's simulated HMD (opaque, no XR_FB_passthrough)
#   shows no room: Passthrough, asked for, changes nothing;
# - the Binaural mode in field 15 (the frame log's `binaural`: what the
#   presenter holds): its head is the head's turn from the seat every frame,
#   the game running or paused; in first-person inspection, with the game
#   paused, each effect holds still in the listener's view once that turn is
#   taken off (the head applied once, neither twice with the view nor not at
#   all) through the head's turns, the app's Recenter, Monado's recentring of
#   LOCAL and lost tracking (scripted: the last head kept, and what is heard
#   with it), the view being the screen's directions turned up by the
#   camera's pitch (level on its heading, the head doing the pitching), and
#   inspection ended in the pause takes the view away at once; the music
#   holds still; in the diorama the same while the simulated head
#   moves 10 cm and more (its position ignored) and whole-hand pinches
#   scale the table from 25% to the 400% limit and back to 25% (their own
#   gestures only, moving nobody), the listener's view held to the last bit
#   through the scales and both recentrings; the listener is the
#   controlled character throughout, no number is NaN or infinite, the
#   session's end leaves the presenter no head and no view (the game goes
#   back to a flat host), and the RAM at game frame 1200 is xem-headless's.
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
export XEM_DATA_DIR=$out/config/xenogears-ex-machina
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
# and in field 30 a pinch with the gaze on the floor walks there; both eyes
# captured twice in each pause (the player's, Settings').
echo '{"commands": [[0, {"cmd": "enter_field", "map": 30}]]}' >"$out/field30.json"
cp "$out/config/xenogears-ex-machina/settings.json" "$out/settings.saved"
echo '{"graphics": {"profile": "Hd2d"}, "assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/field30.json" --exit-after-frames 330 \
    --xr-hands "60:pause,120:pause,140:prayer,170:prayer,180:prayer,190:pinch@panel:410/30,230:pinch@120/150" \
    --xr-capture "$out/gestures" --xr-capture-at 80,110,150,165 >"$out/gestures.log" 2>"$out/gestures.err"
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
# at 60 open the settings panel beside the pane, a pinch on its Close at 90
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
# The title movie and the title with the diorama as the view, from boot:
# both on the screen.
echo '{"graphics": {"xr": {"view": "Diorama"}}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --exit-at-frame 1800 >"$out/diorama-title.log" 2>"$out/diorama-title.err"
# The diorama's fallbacks to the screen, each a session log with the
# diorama as the view: field 30's menu (opened at game frame 600, closed at
# 1100: the hands run's log, below); the Mountain Path's random battle and
# its end (Up held from game frame 120 to 700, the battle from 355; Cross
# every 30 frames from 760, the field again from 1600); Lahan left for the
# world map (tests/reference-inputs/world-map-lahan-exit.json) and a field
# entered from it at game frame 2860.
hands='[[0, {"cmd": "enter_field", "map": 30}], [600, {"cmd": "act", "actions": ["Menu"], "frames": 2}], [1100, {"cmd": "act", "actions": ["Cancel"], "frames": 2}]]'
echo "{\"commands\": $hands}" >"$out/hands.json"
battle15='[[0, {"cmd": "enter_field", "map": 15, "entry": 0}], [120, {"cmd": "pad", "buttons": ["up"]}], [700, {"cmd": "pad", "buttons": []}]'
for f in $(seq 760 30 2080); do
    battle15="$battle15, [$f, {\"cmd\": \"press\", \"buttons\": [\"cross\"], \"frames\": 2}]"
done
echo "{\"commands\": $battle15]}" >"$out/battle15.json"
python3 -c 'import json, sys; route = json.load(open(sys.argv[1])); print(json.dumps({"commands": route["commands"] + [[2860, {"cmd": "enter_field", "map": 1}]]}))' \
    "$root/tests/reference-inputs/world-map-lahan-exit.json" >"$out/lahan-worldmap.json"
"$desktop" --xr --disc "$disc" --script "$out/hands.json" --exit-at-frame 1500 >"$out/diorama-menu.log" 2>"$out/diorama-menu.err"
"$desktop" --xr --disc "$disc" --script "$out/battle15.json" --exit-at-frame 2400 >"$out/diorama-battle.log" 2>"$out/diorama-battle.err"
"$desktop" --xr --disc "$disc" --script "$out/lahan-worldmap.json" --exit-at-frame 3400 >"$out/diorama-worldmap.log" 2>"$out/diorama-worldmap.err"
# Map 25's story movie between its scene and the field after it, the
# player's own play from a state xem-headless saved there (the scene's
# dialogue auto-advanced to game frame 5080; the movie begins about 350
# frames on, its 4 280 frames on the screen, then map 25 again).
printf '{"cmd":"enter_field","map":25}\n{"cmd":"auto_advance","dwell":1}\n{"cmd":"frames","count":5080}\n{"cmd":"auto_advance","dwell":0}\n{"cmd":"state_save","path":"%s"}\n' "$out/movie25.xemstate" |
    "$headless" --disc "$disc" --control >"$out/movie25-prep.log" 2>"$out/movie25-prep.err"
at=$(printf '{"cmd":"state_load","path":"%s"}\n' "$out/movie25.xemstate" | "$headless" --disc "$disc" --control 2>/dev/null |
    python3 -c 'import json, sys; print(json.loads(sys.stdin.readline())["frames"])')
"$desktop" --xr --disc "$disc" --load-state "$out/movie25.xemstate" --exit-at-frame "$((at + 5300))" >"$out/diorama-movie.log" 2>"$out/diorama-movie.err"
# Save states and time controls, the player's own play (no session log)
# from field 30, which an agent's command loads at the first headset frame.
# Prayer hands open Settings at headset frame 20 (the game held by their
# pause), pinches on the 440x560 panel choose Saves (269,98) and its Save
# state (122,170); prayer hands close it at 80, open it at 160, and Load
# state (314,170) restores it; Time (232,98), its Start fast-forward
# (347,449), and Settings closed at 240. The scripted controller buttons
# hold Back+RT at 350 (fast-forward off) and Back+LT from 460 to 500 (a
# rewind). Then the state file the panel wrote, restored by xem-headless.
printf '{"cmd":"enter_field","map":30}\n{"cmd":"frames","count":400}\n{"cmd":"state_save","path":"%s"}\n' "$out/field30.xemstate" |
    "$headless" --disc "$disc" --control >"$out/saves-prep.log" 2>"$out/saves-prep.err"
echo '{"assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
printf '{"cmd":"state_load","path":"%s"}\n' "$out/field30.xemstate" |
    "$desktop" --xr --control --disc "$disc" --exit-after-frames 560 \
    --xr-hands "20:prayer,40:pinch@panel:269/98,60:pinch@panel:122/170,80:prayer,160:prayer,180:pinch@panel:314/170,200:pinch@panel:232/98,220:pinch@panel:347/449,240:prayer" \
    --xr-pad "350:Back+RT,352:none,460:Back+LT,500:none" >"$out/saves.log" 2>"$out/saves.err"
printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"memory_hash"}\n' "$out/config/xenogears-ex-machina/saves/state.xemstate" |
    "$headless" --disc "$disc" --control >"$out/saves-headless.log" 2>"$out/saves-headless.err"
# Hands on the diorama (docs/runtime.md, Headset qualification), the
# player's own play (no session log, whose inputs would stand in for the
# player's and leave inspection's stop to the log) from the same state, the
# gaze the eyes': the left hand's Pause grab at headset frame 100 and again
# at 130 (both eyes captured at 110 and 125 while it holds the game, the
# head moving), both hands' at 160, the right hand's at 190; a sideways
# grab at 220, a fist closed sideways and turned at 250, a hand found
# closed at 280 (its loss pauses; the right hand's Pause at 310 resumes);
# the left hand's underhand grab at 340 and 380 (from within the scene,
# then the diorama again); a pinch through the table at 420 walks, the
# underhand grab at 422 stops the walk and at 465 gives the diorama back
# with no walk; prayer hands at 520 and 550 open and close the settings
# panel beside the pane.
cp "$out/config/xenogears-ex-machina/settings.json" "$out/settings.saved"
echo '{"graphics": {"xr": {"view": "Diorama"}}, "assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --load-state "$out/field30.xemstate" --exit-after-frames 600 \
    --xr-hands "100:pause-left,130:pause-left,160:pause-both,190:pause,220:grab-sideways,250:fist-turn,280:fist-reacquire,310:pause,340:inspect-left,380:inspect-left,420:pinch@world:88.67/0/-55.33,422:inspect-left,465:inspect-left,520:prayer,550:prayer" \
    --xr-capture "$out/diorama-hands" --xr-capture-at 110,125 >"$out/diorama-hands.log" 2>"$out/diorama-hands.err"
# Graphics' headset view switched in the session by an agent, the player's
# own play from the same state with the diorama as the view: once the
# session has drawn 100 frames, `settings_apply` through --control sets it
# to the screen, and at 200 to the diorama again (the run's other graphics
# settings are the defaults, which the block's missing fields take).
echo '{"graphics": {"xr": {"view": "Diorama"}}}' >"$out/config/xenogears-ex-machina/settings.json"
for n in 100 200; do
    for _ in $(seq 600); do grep -q "^xrframe n=$n " "$out/diorama-switch.log" 2>/dev/null && break; sleep 0.1; done
    view=$([[ $n == 100 ]] && echo Screen || echo Diorama)
    printf '{"cmd":"settings_apply","change":{"Graphics":{"xr":{"view":"%s"}}}}\n' "$view"
done | "$desktop" --xr --control --disc "$disc" --load-state "$out/field30.xemstate" --exit-after-frames 320 \
    >"$out/diorama-switch.log" 2>"$out/diorama-switch.err"
cp "$out/settings.saved" "$out/config/xenogears-ex-machina/settings.json"
# Held rewinds through Settings' pause, from the same state: Back+Start
# opens Settings at headset frame 200 and Back+LT is held from 205 to 211;
# then the right controller's ray (scripted: Monado's controllers point
# where no test chooses) clicks Time (232,98) at 220 and holds its
# Hold to rewind (355,352) with the trigger (R1) from 230, Back+Start
# closes Settings at 245 with the trigger still held, and it lets go at 270.
# Before, while the game runs, the agent reads the session's status at
# headset frame 40 (the gameplay options the settings gave it), applies
# RandomBattles Original and reads the status again at 80.
drawn() { # LOG N: wait (at most a minute) until the headset run logging to LOG drew frame N
    for _ in $(seq 600); do grep -q "^xrframe n=$2 " "$1" 2>/dev/null && return; sleep 0.1; done
}
{
    printf '{"cmd":"state_load","path":"%s"}\n' "$out/field30.xemstate"
    drawn "$out/held.log" 40
    echo '{"cmd":"status"}'
    echo '{"cmd":"settings_apply","change":{"RandomBattles":"Original"}}'
    drawn "$out/held.log" 80
    echo '{"cmd":"status"}'
} | "$desktop" --xr --control --disc "$disc" --exit-after-frames 300 \
    --xr-pad "200:Back+Start,202:none,205:Back+LT,211:none,220:R1@panel:232/98,222:none@panel:232/98,230:R1@panel:355/352,245:R1+Back+Start@panel:355/352,247:R1@panel:355/352,270:none" \
    >"$out/held.log" 2>"$out/held.err"
# OpenXR's controller paths for first-person inspection, the player's own
# play (no session log, so the host gives the session the view's state)
# from the same state: the controllers' Back released alone (scripted)
# toggles it on at headset frame 40; Down, North (Jump) and L1 (a camera
# turn) are held from 60 to 120 while it shows; Back toggles it off at 130
# and Down held from 140 to 160 walks; Back toggles it on again at 170; the
# right stick (scripted: Monado's simulated controllers have none) pushed
# right from 190 and again from 205 turns the view 30 degrees each push;
# and an agent's settings_apply to the Original renderer once the session
# has drawn 230 frames ends it.
cp "$out/config/xenogears-ex-machina/settings.json" "$out/settings.saved"
(
    for _ in $(seq 900); do grep -q '^xrframe n=230 ' "$out/controllers.log" 2>/dev/null && break; sleep 0.1; done
    printf '%s\n' '{"cmd":"settings_apply","change":{"Graphics":{"renderer":"Original"}}}'
    for _ in $(seq 600); do grep -q '^xrframe n=270 ' "$out/controllers.log" 2>/dev/null && break; sleep 0.1; done
) | "$desktop" --xr --control --disc "$disc" --load-state "$out/field30.xemstate" --exit-after-frames 280 \
    --xr-pad "40:Back,42:none,60:Down+North+L1,121:none,130:Back,132:none,140:Down,161:none,170:Back,172:none" \
    --xr-stick "190:1/0,196:0/0,205:1/0,211:0/0" >"$out/controllers.log" 2>"$out/controllers.err"
cp "$out/settings.saved" "$out/config/xenogears-ex-machina/settings.json"
# First person by an agent's command (--control), the player's own play
# from the same state: a walk by `navigate` once headset frame 30 is drawn,
# the view on looking 30 degrees right once 36 is (the walk stops), at 80
# `navigate` and a walk by `act` refused, `status`, and the view off; then
# Toggle first-person view's chord (the right stick's click with the right
# trigger: Back+R1) at 102 turns it on and at 132 off.
{
    printf '{"cmd":"state_load","path":"%s"}\n' "$out/field30.xemstate"
    drawn "$out/agent-inspect.log" 30
    echo '{"cmd":"navigate","to":[-80,-90]}'
    drawn "$out/agent-inspect.log" 36
    echo '{"cmd":"first_person","on":true,"yaw":30}'
    drawn "$out/agent-inspect.log" 80
    printf '%s\n' '{"cmd":"navigate","to":[-80,-90]}' '{"cmd":"act","actions":["Up"],"frames":10}' '{"cmd":"status"}' '{"cmd":"first_person","on":false}'
    drawn "$out/agent-inspect.log" 150
} | "$desktop" --xr --control --disc "$disc" --exit-after-frames 160 \
    --xr-pad "100:Back,102:Back+R1,104:none,130:Back,132:Back+R1,134:none" >"$out/agent-inspect.log" 2>"$out/agent-inspect.err"
# A hands-only player's held rewind, from the same state, the gaze the
# eyes': an agent's app command recentres once the session has drawn 100
# frames; prayer hands open Settings at headset frame 200, a pinch picks
# Time (232,98) at 220, and a pinch held on its Hold to rewind (355,352)
# from 240 (the pinch itself from 242 to 281) rewinds through Settings'
# pause, the gaze moving down off the button meanwhile at 260 (the panel's
# pointer moves nowhere while the pinch holds it).
echo '{"assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
(
    printf '{"cmd":"state_load","path":"%s"}\n' "$out/field30.xemstate"
    for _ in $(seq 600); do grep -q '^xrframe n=100 ' "$out/held-hands.log" 2>/dev/null && break; sleep 0.1; done
    printf '{"cmd":"app","request":{"Action":"Recenter"}}\n'
) | "$desktop" --xr --control --disc "$disc" --exit-after-frames 320 \
    --xr-hands "200:prayer,220:pinch@panel:232/98,240:pinch-hold@panel:355/352,260:rest@panel:355/390" \
    >"$out/held-hands.log" 2>"$out/held-hands.err"
# The headset's AUTO key, the player's own play from field 12's dialogue
# (a state xem-headless saved with its prompt waiting), the gaze the
# eyes': a pinch at headset frame 40 aimed at AUTO beside the screen's
# right edge (screen pixel 336/108: the key's centre 0.15 m past the edge
# of the 3 m screen showing the field's 320x216 picture), the agent
# reading the session's status at 30, 55 and 110 and the settings at 55.
printf '{"cmd":"enter_field","map":12}\n{"cmd":"frames","count":450}\n{"cmd":"status"}\n{"cmd":"state_save","path":"%s"}\n' "$out/dialogue12.xemstate" |
    "$headless" --disc "$disc" --control >"$out/auto-prep.log" 2>"$out/auto-prep.err"
echo '{"assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
{
    printf '{"cmd":"state_load","path":"%s"}\n' "$out/dialogue12.xemstate"
    drawn "$out/auto.log" 30
    echo '{"cmd":"status"}'
    drawn "$out/auto.log" 55
    echo '{"cmd":"status"}'
    echo '{"cmd":"settings"}'
    drawn "$out/auto.log" 110
    echo '{"cmd":"status"}'
} | "$desktop" --xr --control --disc "$disc" --exit-after-frames 130 --xr-hands "40:pinch@336/108" >"$out/auto.log" 2>"$out/auto.err"
# State interchange: field 30's state from xem-headless (the saves run's)
# loaded in the headset (--load-state) and played on 600 game frames with
# no input under HD-2D, the state the headset writes at the 300th
# (--save-state-at) loaded by xem-headless and played on 300 frames, and
# the same frames in xem-headless from its own state. The screen without
# wider framing leaves the field's characters the original's view, and no
# focus pause waits on Monado's session focus, so the headset gives the
# game xem-headless's controls.
at=$(printf '{"cmd":"state_load","path":"%s"}\n' "$out/field30.xemstate" | "$headless" --disc "$disc" --control 2>/dev/null |
    python3 -c 'import json, sys; print(json.loads(sys.stdin.readline())["frames"])')
echo '{"graphics": {"profile": "Hd2d"}, "pause_on_focus_loss": false}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --load-state "$out/field30.xemstate" --save-state-at "$((at + 300)):$out/xr-saved.xemstate" \
    --exit-at-frame "$((at + 600))" >"$out/interchange.log" 2>"$out/interchange.err"
printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"frames","count":300}\n{"cmd":"memory_hash"}\n{"cmd":"state_save","path":"%s"}\n{"cmd":"frames","count":300}\n{"cmd":"memory_hash"}\n' \
    "$out/field30.xemstate" "$out/headless-saved.xemstate" | "$headless" --disc "$disc" --control >"$out/interchange-headless.log" 2>"$out/interchange-headless.err"
printf '{"cmd":"state_load","path":"%s"}\n{"cmd":"memory_hash"}\n{"cmd":"frames","count":300}\n{"cmd":"memory_hash"}\n' \
    "$out/xr-saved.xemstate" | "$headless" --disc "$disc" --control >"$out/interchange-native.log" 2>"$out/interchange-native.err"
# Headset qualification on the screen view (docs/runtime.md, Headset
# qualification): short runs, each from its own start, so that a headset
# frame lands in its scene whether the game runs 2.6 or 3.6 frames to one.
# Either hand's grabs and two-hand scaling in field 30 under the default
# profile, the gaze the eyes': the log (hands.json, above) opens the
# field's menu at game frame 600 and closes it at 1100, the left hand's
# underhand grab at 150 turning nothing on in it (both eyes captured at
# 160); the left hand's Pause grab
# at 400 and 430, both hands' at 460 (one toggle), the right hand's at 490;
# a sideways grab at 520, a fist closed sideways and turned palm down and
# up at 550, a hand found closed at 590 (its loss pauses; the right hand's
# Pause at 620 resumes); two whole-hand pinches spreading at 660,
# contracting at 690 and spreading at 720 with the runtime's pinch held and
# the gaze on walkable floor, then a pinch there at 760, which walks; the
# left hand's underhand grab at 850 and 890 (inspection on and off), both
# eyes captured at 840 and 875.
echo '{"graphics": {"profile": "Modern"}, "assisted": {"pointer": "Eye"}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/hands.json" --exit-after-frames 920 \
    --xr-hands "150:inspect-left,400:pause-left,430:pause-left,460:pause-both,490:pause,520:grab-sideways,550:fist-turn,590:fist-reacquire,620:pause,660:scale-out,690:scale-in,720:scale-pinch@120/150,760:pinch@120/150,850:inspect-left,890:inspect-left" \
    --xr-capture "$out/hands" --xr-capture-at 160,840,875 >"$out/hands.log" 2>"$out/hands.err"
# The scene tour under HD-2D, both eyes captured in each scene (pictures
# for people to read, and the checks): from boot the title movie (headset
# frame 290) and the title, field 490 (560; the underhand grab at 570 turns
# nothing on); field 12's dialogue (entered at game frame 0: the screen at
# 100, the grab at 110, the eyes from within at 135, the grab again at
# 150; then the same log in xem-headless). Field 30 and its menu are the
# hands run's; the world map and the battles follow.
echo '{"graphics": {"profile": "Hd2d"}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --exit-at-frame 2100 --xr-hands "570:inspect" --xr-capture "$out/title" --xr-capture-at 290,560 \
    >"$out/title.log" 2>"$out/title.err"
"$desktop" --xr --disc "$disc" --script "$out/field.json" --exit-at-frame 1000 --xr-hands "110:inspect,150:inspect" \
    --xr-capture "$out/dialogue" --xr-capture-at 100,135 >"$out/dialogue.log" 2>"$out/dialogue.err"
printf '{"cmd":"schedule","commands":[[0, {"cmd": "enter_field", "map": 12}]]}\n{"cmd":"run_until","until":{"frames":1000},"max_frames":2000}\n{"cmd":"memory_hash"}\n' |
    "$headless" --disc "$disc" --control >"$out/dialogue-headless.log" 2>"$out/dialogue-headless.err"
# The world map (boot mode 3) on the screen: captured at 280, the
# underhand grab at 300 turns nothing on; two whole-hand pinches spreading
# at 320 and contracting at 350 beside its control deck's keys; the app's
# Recenter at 380, then Monado's recentring of LOCAL once the session has
# drawn 400 frames, both eyes captured at 390 and 440.
echo '{"commands": [[0, {"cmd": "boot_mode", "mode": 3}]]}' >"$out/worldmap.json"
(
    for _ in $(seq 900); do grep -q '^xrframe n=400 ' "$out/worldmap.log" 2>/dev/null && break; sleep 0.1; done
    monado-ctl -c >"$out/monado-ctl-worldmap.log" 2>&1 || echo "monado-ctl exit $?" >>"$out/monado-ctl-worldmap.log"
) &
recentring=$!
"$desktop" --xr --disc "$disc" --script "$out/worldmap.json" --exit-after-frames 470 --xr-recenter-at 380 \
    --xr-hands "300:inspect,320:scale-out,350:scale-in" --xr-capture "$out/worldmap" --xr-capture-at 280,390,440 \
    >"$out/worldmap.log" 2>"$out/worldmap.err"
wait "$recentring"
# Battles from the original's debug battle selector (Start with the default
# scene and party; in Gears with each member's gear column set to Nml
# first): captured at 560 and 620, the underhand grab at 590 turns nothing
# on; then the Gears' log in xem-headless.
foot='[[60, {"cmd": "write", "path": "mode_battle_standalone", "value": 1}], [60, {"cmd": "boot_mode", "mode": 2}], [960, {"cmd": "press", "buttons": ["start"], "frames": 1}]]'
gear='[[60, {"cmd": "write", "path": "mode_battle_standalone", "value": 1}], [60, {"cmd": "boot_mode", "mode": 2}]'
at=960
for button in down down cross right cross right cross start; do
    gear="$gear, [$at, {\"cmd\": \"press\", \"buttons\": [\"$button\"], \"frames\": 1}]"
    at=$((at + 21))
done
gear="$gear]"
echo "{\"commands\": $foot}" >"$out/battle.json"
echo "{\"commands\": $gear}" >"$out/gear.json"
for battle in battle gear; do
    "$desktop" --xr --disc "$disc" --script "$out/$battle.json" --exit-at-frame 2300 \
        --xr-hands "590:inspect" --xr-capture "$out/$battle" --xr-capture-at 560,620 >"$out/$battle.log" 2>"$out/$battle.err"
done
printf '{"cmd":"schedule","commands":%s}\n{"cmd":"run_until","until":{"frames":2300},"max_frames":3000}\n{"cmd":"memory_hash"}\n' "$gear" |
    "$headless" --disc "$disc" --control >"$out/gear-headless.log" 2>"$out/gear-headless.err"
# The battle on foot as a diorama, its log pressing Cross every 30 frames
# from game frame 1500 (its attacks: the battle's camera moves), both eyes
# captured at headset frames 600 and 700; the world map as a diorama at
# 25%, the vehicle it begins in driven on by its throttle (Triangle) from
# game frame 800 to 1200, both eyes captured at 350 and 420; then each log
# in xem-headless.
fight="${foot%]}"
for f in $(seq 1500 30 2200); do
    fight="$fight, [$f, {\"cmd\": \"press\", \"buttons\": [\"cross\"], \"frames\": 2}]"
done
fight="$fight]"
echo "{\"commands\": $fight}" >"$out/fight.json"
echo '{"graphics": {"xr": {"view": "Diorama"}}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/fight.json" --exit-at-frame 2300 --xr-capture "$out/battle-diorama" --xr-capture-at 600,700 \
    >"$out/battle-diorama.log" 2>"$out/battle-diorama.err"
printf '{"cmd":"schedule","commands":%s}\n{"cmd":"run_until","until":{"frames":2300},"max_frames":3000}\n{"cmd":"memory_hash"}\n' "$fight" |
    "$headless" --disc "$disc" --control >"$out/battle-diorama-headless.log" 2>"$out/battle-diorama-headless.err"
drive='[[0, {"cmd": "boot_mode", "mode": 3}], [800, {"cmd": "press", "buttons": ["triangle"], "frames": 400}]]'
echo "{\"commands\": $drive}" >"$out/worlddrive.json"
echo '{"graphics": {"xr": {"view": "Diorama", "diorama_scale": 25}}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/worlddrive.json" --exit-at-frame 1500 --xr-capture "$out/worldmap-diorama" --xr-capture-at 350,420 \
    >"$out/worldmap-diorama.log" 2>"$out/worldmap-diorama.err"
printf '{"cmd":"schedule","commands":%s}\n{"cmd":"run_until","until":{"frames":1500},"max_frames":2500}\n{"cmd":"memory_hash"}\n' "$drive" |
    "$headless" --disc "$disc" --control >"$out/worldmap-diorama-headless.log" 2>"$out/worldmap-diorama-headless.err"
# Two-hand scaling of the diorama, with the inspection run's log (its RAM
# checked against the same headless run): two whole-hand pinches spread at
# headset frame 110 and contract at 150, once the log has walked Fei into
# the room (game frames 500 to 560, by about headset frame 80) and the
# camera holds still, both eyes captured at 140; and Passthrough asked for,
# which Monado's simulated HMD cannot show.
echo '{"graphics": {"profile": "Hd2d", "xr": {"view": "Diorama", "passthrough": true}}}' >"$out/config/xenogears-ex-machina/settings.json"
"$desktop" --xr --disc "$disc" --script "$out/inspect.json" --exit-at-frame 1200 --xr-hands "110:spread,150:contract" \
    --xr-capture "$out/diorama-scale" --xr-capture-at 140 >"$out/diorama-scale.log" 2>"$out/diorama-scale.err"
cp "$out/config/xenogears-ex-machina/settings.json" "$out/diorama-scale.settings.json"
# The Binaural mode in field 15 (three emitters the presenter places, and
# its music), under the default profile: the screen, then first-person
# inspection from headset frame 120 (the underhand grab) to 255, the
# player's Pause from 150 to 270 holding the game, and in it the app's
# Recenter at 190, Monado's recentring of LOCAL once the session has drawn
# 220 frames, the head lost for 15 frames from 240 and inspection's end;
# then the same log in xem-headless (its RAM and the controlled character at
# game frame 1200).
# The diorama with the same log, at 25% of its scale: the game held by the
# test's pause from headset frame 150 to 410, and in it whole-hand pinches
# spreading at 160, 180, 200 and 220 (2.52 times each: 63%, 159%, then the
# 400% limit twice), Monado's recentring once the session has drawn 240
# frames (a recentring ends a scaling under way, so none is), the app's
# Recenter at 265, and contracting at 280 to 380 every 20 frames (0.598
# times each: 239%, 143%, 85%, 51%, 30%, then the 25% limit).
echo '{"commands": [[0, {"cmd": "enter_field", "map": 15}]]}' >"$out/field15.json"
for run in binaural binaural-diorama; do
    case $run in
    binaural)
        echo '{"binaural": {"enabled": true}}' >"$out/config/xenogears-ex-machina/settings.json"
        at=220 options=(--xr-hands "120:inspect,150:pause,255:inspect,270:pause" --xr-recenter-at 190 --xr-lose-tracking 240,15) ;;
    binaural-diorama)
        echo '{"graphics": {"xr": {"view": "Diorama", "diorama_scale": 25}}, "binaural": {"enabled": true}}' >"$out/config/xenogears-ex-machina/settings.json"
        at=240 options=(--xr-pause 150,260 --xr-recenter-at 265
            --xr-hands "160:spread,180:spread,200:spread,220:spread,280:contract,300:contract,320:contract,340:contract,360:contract,380:contract") ;;
    esac
    (
        for _ in $(seq 900); do grep -q "^xrframe n=$at " "$out/$run.log" 2>/dev/null && break; sleep 0.1; done
        monado-ctl -c >"$out/monado-ctl-$run.log" 2>&1 || echo "monado-ctl exit $?" >>"$out/monado-ctl-$run.log"
    ) &
    recentring=$!
    "$desktop" --xr --disc "$disc" --script "$out/field15.json" --exit-at-frame 1200 "${options[@]}" >"$out/$run.log" 2>"$out/$run.err"
    wait "$recentring"
done
printf '{"cmd":"schedule","commands":[[0, {"cmd": "enter_field", "map": 15}]]}\n{"cmd":"run_until","until":{"frames":1200},"max_frames":2000}\n{"cmd":"memory_hash"}\n{"cmd":"field"}\n' |
    "$headless" --disc "$disc" --control >"$out/binaural-headless.log" 2>"$out/binaural-headless.err"

python3 - "$out" "$frames" "$root" <<'EOF'
import json, math, re, sys, zlib, struct
out, frames, root = sys.argv[1], int(sys.argv[2]), sys.argv[3]

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
# How far round to the right of the seat's way (seat=) a room point (x, z)
# stands, about the vertical through the seat (degrees).
def round_from(r, x, z):
    sx, _, sz, yaw = (float(v) for v in r["seat"].replace(";", ",").split(","))
    dx, dz = x - sx, z - sz
    return math.degrees(math.atan2(dx * math.cos(yaw) - dz * math.sin(yaw), -dx * math.sin(yaw) - dz * math.cos(yaw)))

# Where the settings panel's middle (panel=) stands from the diorama's pane
# (pane=, its middle; 0.6 m wide) as seen from the seat (seat=): degrees
# round to the right, about the vertical through the seat, from the pane's
# right edge and from its middle.
def beside_pane(r):
    yaw = float(r["seat"].split(";")[1])
    panel, pane = ([float(v) for v in r[k].split(",")] for k in ("panel", "pane"))
    edge = (pane[0] + 0.3 * math.cos(yaw), pane[2] - 0.3 * math.sin(yaw))
    at = round_from(r, panel[0], panel[2])
    return at - round_from(r, *edge), at - round_from(r, pane[0], pane[2])

# Where the settings panel stands (panel=, placed before the frame
# presents): beside the diorama's pane (its middle 20 degrees or more round
# to the right of the seat's way) or below the screen (straight ahead, half
# a metre or more below the seat).
def panel_place(r):
    x, y, z = (float(v) for v in r["panel"].split(","))
    up = y - float(r["seat"].split(",")[1])
    turn = round_from(r, x, z)
    return "beside" if turn >= 20 else "below" if abs(turn) < 0.5 and up < -0.5 else f"{turn:.1f} degrees round, {up:.2f} m up"

# Prayer hands open the settings panel beside the diorama's pane (its quad
# layer: its middle some 20 degrees or more round from the pane's right
# edge as seen from the seat, half the panel being 12, the keys beside the
# pane 6: off the pane, which stays in view) and a pinch on its Close
# closes it, the game held meanwhile; then a pinch through the table walks
# Fei to the floor point gazed at.
rows = records("diorama-walk.log")
panel = [r for r in rows if r["layers"] == "2"]
opened = [int(r["n"]) for r in panel]
check(bool(opened) and 60 <= opened[0] <= 80 and opened[-1] < 100 and len(opened) == opened[-1] - opened[0] + 1,
      f"prayer hands showed the panel beside the pane and a pinch on Close closed it: shown {opened[:1]}..{opened[-1:]}")
check(len({r["game_frames"] for r in panel}) == 1, "the game ran while the panel was open")
check(all(r["diorama"] == "1" for r in panel), "the diorama gave way while the panel was open")
beside = sorted({beside_pane(r) for r in panel})
check(bool(beside) and all(edge >= 20 and middle >= 30 for edge, middle in beside),
      f"the panel beside the pane (degrees round from its right edge and its middle): {beside[:3]}")
walked = [r["assist"] for r in rows if int(r["n"]) >= 150]
pinch = [int(r["n"]) for r in rows if "Pinch" in r["gestures"] and int(r["n"]) >= 150]
check(bool(pinch) and all(r["diorama"] == "1" for r in rows if int(r["n"]) in pinch), f"the pinch did not come while the diorama showed: {pinch}")
check("moving" in walked and walked[-1] == "arrived", f"the pinch through the diorama did not walk Fei there: {walked[:5]}..{walked[-3:]}")
print(f"diorama walk: the panel beside the pane shown {opened[:1]}..{opened[-1:]}, its middle at {panel[0]['panel'] if panel else '-'}, "
      f"{beside[0][0] if beside else 0:.1f} degrees round from the pane's right edge, {beside[0][1] if beside else 0:.1f} from its middle; "
      f"pinch at {pinch}, {walked[:2]}..{walked[-1:]}")

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

# A hands-only player's held rewind: the pinch held on Hold to rewind (242
# to 281) holds it from its start to its end, the gaze leaving the button
# at 260 changing nothing; meanwhile the game goes back through Settings'
# pause under the HUD's icon, and Settings hold it again once it ends. An
# agent's app command recentred the view once (its reply, the frames').
lines = open(f"{out}/held-hands.log").read().splitlines()
rows = records("held-hands.log")
by = {int(r["n"]): r for r in rows}
def gf(n):
    return int(by[n]["game_frames"])
rewinds, last = [], -1
for line in lines:
    if line.startswith("xrframe "):
        last = int(line.split()[1].split("=", 1)[1])
    m = re.match(r"app (\w*Rewind)\((true|false)\) game_vblank=\d+ memory_hash [0-9a-f]{16} (ok|error)", line)
    if m:
        rewinds.append((last + 1, m.group(1), m.group(2), m.group(3)))
check([r[1:] for r in rewinds] == [("UiRewind", "true", "ok"), ("UiRewind", "false", "ok")], f"the held pinch's rewinds: {rewinds}")
if len(rewinds) == 2:
    (held_on, *_), (held_off, *_) = rewinds
    during = [by[n] for n in range(held_on, held_off)]
    check(242 <= held_on < 252 and 282 <= held_off <= 284,
          f"the pinch held from headset frame 242 to 281 held Hold to rewind from {held_on} to {held_off}")
    check(all(r["layers"] == "2" and r["paused"] == "0" for r in during) and any("rewind" in r["hud"].split(",") for r in during)
          and gf(held_off - 1) < gf(held_on - 1) - 120,
          f"the held pinch rewound game frame {gf(held_on - 1)} to {gf(held_off - 1)}, "
          f"layers/paused/hud {sorted({(r['layers'], r['paused'], r['hud']) for r in during})}")
    after = [by[n] for n in range(held_off + 1, 320)]
    check(all(r["layers"] == "2" and r["paused"] == "1" for r in after) and len({r["game_frames"] for r in after}) == 1,
          "Settings did not hold the game again once the pinch let go")
    print(f"held hands: the pinch held Hold to rewind from headset frame {held_on} to {held_off}, game frame {gf(held_on - 1)} "
          f"to {gf(held_off - 1)}, the gaze off the button from 260")
replies = [json.loads(l) for l in lines if l.startswith("{")]
recentred = [l for l in lines if l.startswith("recenter seat=")]
counts = [(int(r["n"]), r["recenters"]) for r in rows]
first = next((n for n, c in counts if c == "1"), None)
check(len(replies) == 2 and replies[1] == {"ok": True, "message": "The view is in front of you again."} and len(recentred) == 1
      and first is not None and first >= 100 and all(c == ("1" if n >= first else "0") for n, c in counts),
      f"the agent's app Recenter: replies {str(replies)[-160:]}, {recentred}, recenters from frame {first}")
print(f"held hands: an agent's app Recenter at headset frame {first} ({recentred[:1]})")

# The gameplay options in the headset (the held run's agent): the settings'
# defaults reach the session (random battles off on xem-core's platforming
# maps, every other option the original's), and an agent's RandomBattles
# Original is acknowledged and reaches it in the next frames.
table = re.search(r"PLATFORMING_AREAS: [^=]*= &\[(.*?)\n\];", open(f"{root}/src/runtime/crates/xem-core/src/encounters.rs").read(), re.S)
platforming = sorted(int(n) for n in re.findall(r"\d+", re.sub(r'"[^"]*"', "", table.group(1)))) if table else None
# (The first reply, state_load's, is the status the state was loaded with.)
replies = [json.loads(l) for l in open(f"{out}/held.log") if l.startswith("{")][1:]
statuses = [r for r in replies if "random_battles_off" in r]
applied = [r for r in replies if "revision" in r]
options = lambda s: [s["half_encounters"], s["text_speed"], s["auto_advance"], s["movie_skip"]]
check(platforming and len(statuses) == 2 and len(applied) == 1, f"the held run's agent: {len(statuses)} statuses, {len(applied)} acknowledgements, table {platforming}")
if platforming and len(statuses) == 2 and len(applied) == 1:
    before, after = statuses
    check(before["random_battles_off"] == platforming and options(before) == [False, 0, 0, False],
          f"the defaults in the headset's session: random battles off on {before['random_battles_off']} (the table's {platforming}), "
          f"half encounters, text speed, auto-advance and movie skipping {options(before)}")
    check(isinstance(applied[0]["revision"], int) and applied[0]["settings"]["random_battles"] == "Original",
          f"settings_apply RandomBattles Original: {str(applied[0])[:200]}")
    check(after["random_battles_off"] == [] and after["frames"] > before["frames"] and options(after) == options(before),
          f"the next status after the change: random battles off on {after['random_battles_off']}, game frame {before['frames']} to {after['frames']}")
    print(f"gameplay options: the session had random battles off on {before['random_battles_off']} and {options(before)} at game frame "
          f"{before['frames']}; RandomBattles Original acknowledged (revision {applied[0]['revision']}), none off at {after['frames']}")

# The headset's AUTO key: aimed at beside the screen in field 12's
# dialogue, a pinch is the app's toggle through the settings, never a
# press of the game's: 45-ish game frames on the prompt still waits and
# has counted toward the dwell every vertical blank since the toggle (a
# press would have turned the page and restarted the count); auto-advance
# then turns it.
prep = [json.loads(l) for l in open(f"{out}/auto-prep.log") if l.startswith("{")]
check(any(p.get("dialogue", {}).get("kind") == "waiting" for p in prep), f"field 12's state was not saved at its prompt: {[p.get('dialogue') for p in prep]}")
lines = open(f"{out}/auto.log").read().splitlines()
replies = [json.loads(l) for l in lines if l.startswith("{")][1:]
statuses = [r for r in replies if "auto_advance_waited" in r]
saved = [r for r in replies if "app_bindings" in r]
toggles = [(int(m.group(1)), m.group(2)) for l in lines
           if (m := re.match(r"app Action\(ToggleAutoAdvance\) game_vblank=(\d+) memory_hash [0-9a-f]{16} (ok|error)", l))]
rows = records("auto.log")
pinched = [r for r in rows if "Pinch" in r["gestures"]]
check(len(toggles) == 1 and toggles[0][1] == "ok" and len(statuses) == 3 and len(saved) == 1,
      f"the pinch at AUTO: toggles {toggles}, {len(statuses)} statuses, {len(saved)} settings")
check(bool(pinched) and all(r["keys"].startswith("1:") and '"dialogue"' in r["context"] for r in pinched),
      f"the pinch came in the dialogue with AUTO beside the screen: {[(r['n'], r['keys'], r['context']) for r in pinched]}")
if len(toggles) == 1 and len(statuses) == 3 and len(saved) == 1:
    (toggled, _), (before, after, later) = toggles[0], statuses
    since = after["vblanks"] - toggled
    check(before["dialogue"]["kind"] == "waiting" and before["auto_advance"] == 0, f"before the pinch: {before['dialogue']}, auto-advance {before['auto_advance']}")
    check(after["auto_advance"] == 120 and saved[0]["auto_advance"] == {"enabled": True, "dwell": 120},
          f"auto-advance through the settings: the session's dwell {after['auto_advance']}, the settings' {saved[0]['auto_advance']}")
    check(after["dialogue"] == before["dialogue"] and 0 < since < 120 and abs(after["auto_advance_waited"] - since) <= 2,
          f"no game button: {since} vertical blanks after the toggle the prompt {after['dialogue']} has waited {after['auto_advance_waited']} toward the dwell")
    check(later["vblanks"] - toggled > 130 and later["auto_advance_waited"] < later["vblanks"] - toggled - 120,
          f"auto-advance did not turn the page: {later['vblanks'] - toggled} vertical blanks on, waited {later['auto_advance_waited']}, {later['dialogue']}")
    print(f"AUTO: a pinch at headset frame {pinched[0]['n'] if pinched else '-'} turned auto-advance on at game vblank {toggled}; "
          f"{since} vertical blanks on the prompt had waited {after['auto_advance_waited']}; {later['vblanks'] - toggled} on, "
          f"after auto-advance's press, {later['auto_advance_waited']} ({later['dialogue']})")
# First person by an agent's command: each reply with the headset frame it
# was answered in (before that frame's line); the view's frames.
rows = records("agent-inspect.log")
by = {int(r["n"]): r for r in rows}
replies, last = [], -1
for line in open(f"{out}/agent-inspect.log"):
    if line.startswith("xrframe "):
        last = int(line.split()[1].split("=", 1)[1])
    elif line.startswith("{"):
        replies.append((last + 1, json.loads(line)))
inspecting = "first-person inspection keeps the character"
check(len(replies) == 7 and len(rows) == 160, f"agent-inspect: {len(replies)} replies, {len(rows)} headset frames")
if len(replies) == 7 and len(rows) == 160:
    (_, loaded), (walk_at, walk), (on_at, on), (_, route), (_, act), (_, status), (off_at, off) = replies
    check("error" not in loaded and "error" not in walk and isinstance(walk.get("status", {}).get("assist"), dict),
          f"agent-inspect: the state loaded and a walk began: {loaded} {walk}")
    walking = [by[n]["assist"] for n in range(walk_at, on_at)]
    check(on == {"first_person": True, "look": {"yaw": 30.0, "pitch": 0.0}}, f"agent-inspect: first_person on answered {on}")
    shown = [by[n] for n in range(on_at, off_at)]
    check(all(r["fp"] == "1" and r["look"] == "0.5236,0.0000" for r in shown) and sum(r["within"] == "1" for r in shown) >= len(shown) - 4,
          f"agent-inspect: from {on_at} to {off_at} fp/look/within {[(r['fp'], r['look'], r['within']) for r in shown[:4]]}...")
    check(any(a not in ("-", "stopped") for a in walking) and all(r["assist"] == "stopped" for r in shown),
          f"agent-inspect: the walk under way {walking} stopped when the view came on: {[r['assist'] for r in shown[:4]]}")
    check(all(str(r.get("error", "")).startswith(inspecting) for r in (route, act)), f"agent-inspect: refused while it shows: {route} {act}")
    check(status.get("inspecting") is True, f"agent-inspect: status shows inspecting: {status.get('inspecting')}")
    check(off == {"first_person": False, "look": None} and all(by[n]["fp"] == "0" for n in range(off_at, 102)),
          f"agent-inspect: first_person off answered {off} at {off_at}")
    chord = [int(by[n]["fp"]) for n in range(100, 160)]
    check(chord == [0] * 2 + [1] * 30 + [0] * 28, f"agent-inspect: Back+R1 at 102 and 132 toggled fp {chord}")
    print(f"agent-inspect: the walk began at headset frame {walk_at} ({walking[:3]}...), first_person on at {on_at} looked 30 degrees "
          f"right and stopped it, navigate and act refused, off at {off_at}; Back+R1 toggled it on at 102 and off at 132")

# OpenXR's controller paths for first-person inspection (the controllers
# run): Back released alone toggles it in the field, held directions, Jump
# and a camera turn leave the character where it stands while it shows
# (and walk it once it is off), the right stick turns it a step a push, and
# the Original renderer an agent sets ends it.
def controllers():
    rows = records("controllers.log")
    by = {int(r["n"]): r for r in rows}
    fp = lambda a, b: "".join(by[n]["fp"] for n in range(a, b))
    check(fp(0, 40) == "0" * 40 and fp(45, 129) == "1" * 84 and fp(134, 169) == "0" * 35 and fp(175, 228) == "1" * 53,
          f"controllers: Back released alone at 42, 132 and 172 toggles inspection: {fp(0, 230)}")
    check(all(by[n]["within"] == "1" for n in range(50, 129)), "controllers: inspection looks from within the scene")
    stood = {by[n]["player"] for n in range(59, 125)}
    check(len(stood) == 1 and "-" not in stood, f"controllers: Down, North and L1 held while inspecting moved the character: {sorted(stood)}")
    walked = (by[139]["player"], by[165]["player"])
    check(walked[0] != walked[1] and "-" not in walked, f"controllers: Down held with inspection off walked nowhere: {walked}")
    yaw = lambda n: float(by[n]["look"].split(",")[0]) if by[n]["look"] != "-" else None
    turns = (yaw(188), yaw(200), yaw(215))
    check(None not in turns and abs(turns[0]) < 1e-3 and abs(turns[1] - math.pi / 6) < 1e-3 and abs(turns[2] - math.pi / 3) < 1e-3,
          f"controllers: the right stick turned the view 30 degrees a push: {turns}")
    # The agent's reply, after the headset frame it came in.
    last, replied = -1, None
    for line in open(f"{out}/controllers.log"):
        if line.startswith("xrframe "):
            last = int(line.split()[1].split("=", 1)[1])
        elif line.startswith("{") and replied is None:
            reply = json.loads(line)
            check(reply.get("settings", {}).get("graphics", {}).get("renderer") == "Original", f"controllers: the agent's settings_apply: {line[:200]}")
            replied = last + 1
    ended = next((n for n in sorted(by) if replied is not None and n >= replied and by[n]["fp"] == "0"), None)
    check(replied is not None and ended is not None and ended - replied <= 1 and fp(175, replied) == "1" * (replied - 175)
          and all(by[n]["fp"] == "0" and by[n]["view"] == "0" for n in range(ended, 278)),
          f"controllers: the Original renderer set at headset frame {replied} ended inspection at {ended}: {fp(170, 280)}")
    print(f"controllers: Back toggled inspection on at {fp(0, 60).find('1')}, off at {134 - fp(120, 134)[::-1].find('1')}, on at {fp(0, 200).rfind('0') + 1}; "
          f"the character at {sorted(stood)} through Down, North and L1 held while inspecting, walked {walked[0]} to {walked[1]} with it off; "
          f"the view's yaw {turns} through two right-stick pushes; the Original renderer set at {replied}, inspection off from {ended}")
controllers()

# State interchange: xem-headless's state played on in the headset reaches
# xem-headless's RAM at the frame the headset saves at and the frame it ends
# at, and the state the headset saved loads natively to its RAM and goes on
# to the same end.
def strings(name):
    return [json.loads(l) for l in open(f"{out}/{name}") if l.startswith('"')]
log = open(f"{out}/interchange.log").read()
saved = re.search(r"^state saved frame (\d+) memory_hash ([0-9a-f]{16}) path ", log, re.M)
ended = re.search(r"^xem-desktop: frame (\d+) memory_hash ([0-9a-f]{16})", log, re.M)
loaded = next((json.loads(l) for l in open(f"{out}/interchange-headless.log") if l.startswith("{")), {})
reference, native = strings("interchange-headless.log"), strings("interchange-native.log")
at = loaded.get("frames")
check(bool(saved) and bool(ended) and at is not None and len(reference) == 2 and len(native) == 2,
      f"the interchange runs: saved {bool(saved)}, ended {bool(ended)}, from frame {at}, hashes {reference} {native}")
if saved and ended and at is not None and len(reference) == 2 and len(native) == 2:
    check((int(saved.group(1)), int(ended.group(1))) == (at + 300, at + 600),
          f"the headset saved at frame {saved.group(1)} and ended at {ended.group(1)}, not {at + 300} and {at + 600}")
    check(saved.group(2) == reference[0], f"the headset's RAM at frame {at + 300} {saved.group(2)}, xem-headless's {reference[0]}")
    check(ended.group(2) == reference[1], f"the headset's RAM at frame {at + 600} {ended.group(2)}, xem-headless's {reference[1]}")
    check(native[0] == saved.group(2), f"the headset's state loads natively to RAM {native[0]}, not its {saved.group(2)}")
    check(native[1] == reference[1], f"the headset's state goes on natively to RAM {native[1]}, not {reference[1]}")
    # The screen without wider framing asks for no reveals: the whole state
    # is xem-headless's at that frame.
    check(open(f"{out}/xr-saved.xemstate", "rb").read() == open(f"{out}/headless-saved.xemstate", "rb").read(),
          "the headset's state is not xem-headless's at the same frame, byte for byte")
    print(f"interchange: xem-headless's field 30 state at frame {at} played on in the headset to {saved.group(2)} at {at + 300} "
          f"(saved, xem-headless's state byte for byte) and {ended.group(2)} at {at + 600}, xem-headless's; the headset's state "
          f"natively {native[0]}, then {native[1]}")

# Headset qualification. Paused head tracking (the gestures run): through
# the player's Pause and Settings' own the head is tracked (more than half
# the frames have a pose of their own), the eyes' pictures follow it (over
# 2% of their pixels change between the two captures), the game stands
# still.
def changed(a, b):
    (w, h, ra), (_, _, rb) = a, b
    return sum(1 for y in range(h) for x in range(0, 4 * w, 4) if ra[y][x:x + 3] != rb[y][x:x + 3]) / (w * h)
rows = records("gestures.log")
for name, held, (a, b) in [("the player's Pause", [r for r in rows if r["player_pause"] == "1"], (80, 110)),
                           ("Settings' pause", [r for r in rows if r["layers"] == "2" and int(r["n"]) < 175], (150, 165))]:
    check(len({r["head"] for r in held}) > len(held) // 2, f"{name}: head tracking stopped ({len({r['head'] for r in held})} poses in {len(held)} frames)")
    check(len({r["game_frames"] for r in held}) == 1, f"{name}: the game ran")
    moved = changed(png(f"{out}/gestures/{a}/left.png"), png(f"{out}/gestures/{b}/left.png"))
    check(moved > 0.02, f"{name}: the eyes did not follow the head ({moved:.1%} of the left eye changed from {a} to {b})")
    print(f"paused head tracking, {name}: {len({r['head'] for r in held})} head poses in {len(held)} frames, the game at "
          f"{sorted({r['game_frames'] for r in held})}, {moved:.1%} of the left eye changed from frame {a} to {b}")

def gestures_in(rows, first, last):
    return [g for r in rows if first <= int(r["n"]) < last for g in json.loads(r["gestures"])]

# Either hand's grabs and two-hand scaling (the hands run).
rows = records("hands.log")
by = {int(r["n"]): r for r in rows}
pause = lambda hand: {"gesture": "Pause", "hand": hand}
menu = [r for r in rows if 150 <= int(r["n"]) < 170]
check({"gesture": "Inspect", "hand": "Left"} in gestures_in(rows, 150, 170) and bool(menu) and all('"menu"' in r["context"] for r in menu)
      and all(r["fp"] == "0" for r in rows if int(r["n"]) < 850),
      f"in the field's menu the underhand grab turned nothing on: {gestures_in(rows, 150, 170)}, {menu[0]['context'][:40] if menu else '-'}")
for first, last, wanted, paused, what in [
    (400, 420, [pause("Left")], "1", "the left hand's Pause grab pauses"),
    (430, 450, [pause("Left")], "0", "and resumes"),
    (460, 480, None, "1", "both hands closing together toggle once"),
    (490, 510, [pause("Right")], "0", "the right hand's resumes"),
    (520, 585, [], "0", "a sideways grab and a fist turned while closed toggle nothing"),
    (590, 615, [{"gesture": "TrackingLost", "hand": "Right"}], "1", "a hand lost and found closed: its loss pauses, the fist toggles nothing"),
    (620, 640, [pause("Right")], "0", "the right hand's Pause resumes"),
]:
    seen = gestures_in(rows, first, last)
    ok = (len(seen) == 1 and seen[0]["gesture"] == "Pause") if wanted is None else seen == wanted
    check(ok and by[last - 5]["player_pause"] == paused and '"walk"' in by[first]["context"],
          f"{what}: {seen}, paused {by[last - 5]['player_pause']}, {by[first]['context'][:30]}")
scaling = [r for r in rows if 660 <= int(r["n"]) < 760]
kinds = [g["gesture"] for g in gestures_in(rows, 660, 760)]
scales = {n: float(by[n]["scale"]) for n in (685, 715, 755)}
check(set(kinds) <= {"ScaleBegin", "Scale", "ScaleEnd"} and kinds.count("ScaleBegin") == 3 and kinds.count("ScaleEnd") == 3,
      f"two whole-hand pinches gave only their scaling (the runtime's pinch held through the last): {sorted(set(kinds))}")
check(abs(scales[685] - 2.02) < 0.05 and abs(scales[715] - 1.0) < 0.03 and abs(scales[755] - 2.02) < 0.05, f"the screen's scale {scales}")
check(all(r["player_pause"] == "0" for r in scaling), "scaling paused the game")
check(int(by[759]["game_frames"]) - int(by[660]["game_frames"]) > 250, "the game did not run on while scaling")
check(all(r["assist"] == "-" for r in scaling), f"scaling walked: {sorted({r['assist'] for r in scaling})}")
# (Field 30's walk shows no keys; the world map run checks them as it scales.)
panel_y = {n: float(by[n]["panel"].split(",")[1]) for n in (655, 685)}
check(panel_y[685] < panel_y[655] - 0.1, f"the settings panel stays below the scaled screen: its middle at {panel_y}")
walked = [r["assist"] for r in rows if 760 <= int(r["n"]) < 840]
check("moving" in walked and walked[-1] == "arrived", f"the pinch after scaling did not walk: {walked[:3]}..{walked[-2:]}")
on = [int(r["n"]) for r in rows if r["fp"] == "1"]
check(bool(on) and 850 <= on[0] <= 856 and 890 <= on[-1] <= 896 and len(on) == on[-1] - on[0] + 1,
      f"the left hand's underhand grab turned inspection on and off in the field: on {on[:1]}..{on[-1:]}")
print(f"hands: nothing from the grab in the menu; left, both and right grabs, sideways, turned and reacquired fists as the contract says; "
      f"scale {scales}, the panel's middle {panel_y}; inspection on {on[:1]}..{on[-1:]}")

# Hands on the diorama (the diorama-hands run): either hand's grabs as on
# the screen, the diorama shown through them; the player's Pause holds the
# game while the head moves, the eyes following it and seeing the table in
# depth; the underhand grab looks from within the scene and gives the
# diorama back at once at the scale and place it had; inspection stops a
# walk under way and none restarts; prayer hands open the settings panel
# beside the pane, the game held.
def diorama_hands():
    rows = records("diorama-hands.log")
    by = {int(r["n"]): r for r in rows}
    first = next((int(r["n"]) for r in rows if r["diorama"] == "1"), None)
    check(first is not None and first < 100, f"diorama hands: the diorama shows only from headset frame {first}")
    for start, end, wanted, paused, what in [
        (100, 120, [pause("Left")], "1", "the left hand's Pause grab pauses"),
        (130, 150, [pause("Left")], "0", "and resumes"),
        (160, 180, None, "1", "both hands closing together toggle once"),
        (190, 210, [pause("Right")], "0", "the right hand's resumes"),
        (220, 275, [], "0", "a sideways grab and a fist turned while closed toggle nothing"),
        (280, 305, [{"gesture": "TrackingLost", "hand": "Right"}], "1", "a hand lost and found closed: its loss pauses, the fist toggles nothing"),
        (310, 330, [pause("Right")], "0", "the right hand's Pause resumes"),
    ]:
        seen = gestures_in(rows, start, end)
        ok = (len(seen) == 1 and seen[0]["gesture"] == "Pause") if wanted is None else seen == wanted
        shown = sum(by[n]["diorama"] == "1" for n in range(start, end))
        check(ok and by[end - 5]["player_pause"] == paused and shown == end - start,
              f"diorama hands, {what}: {seen}, paused {by[end - 5]['player_pause']}, the diorama in {shown} of {end - start} frames")
    held = [r for r in rows if 100 <= int(r["n"]) < 135 and r["player_pause"] == "1"]
    check(len(held) >= 20 and len({r["head"] for r in held}) > len(held) // 2,
          f"diorama hands: head tracking stopped while paused ({len({r['head'] for r in held})} poses in {len(held)} frames)")
    check(len({r["game_frames"] for r in held}) == 1, "diorama hands: the game ran while the player paused it")
    moved = changed(png(f"{out}/diorama-hands/110/left.png"), png(f"{out}/diorama-hands/125/left.png"))
    check(moved > 0.02, f"diorama hands: the eyes did not follow the head while paused ({moved:.1%} of the left eye changed)")
    depths = {}
    for n in (110, 125):
        d = depths[n] = disparities(png(f"{out}/diorama-hands/{n}/left.png"), png(f"{out}/diorama-hands/{n}/right.png"))
        shifts = [s for _, s in d]
        check(by[n]["diorama"] == "1" and by[n]["share"] == "0.000" and len(d) >= 3 and max(shifts) - min(shifts) >= 5 and d[-1][1] > d[0][1],
              f"diorama hands, paused at {n}: the eyes' disparity does not grow with nearness: {d}")
    # How the left eye moved from 110 to 125, band by band (the head's move
    # shifts the near rows more than the far: recorded, not checked).
    motion = disparities(png(f"{out}/diorama-hands/110/left.png"), png(f"{out}/diorama-hands/125/left.png"))
    on = [int(r["n"]) for r in rows if 340 <= int(r["n"]) < 420 and r["fp"] == "1"]
    check(bool(on) and on[0] <= 350 and 380 <= on[-1] <= 392 and len(on) == on[-1] - on[0] + 1
          and all(by[n]["within"] == "1" and by[n]["diorama"] == "0" for n in on),
          f"diorama hands: the underhand grab looks from within the scene from the diorama: on {on[:1]}..{on[-1:]}")
    if on:
        before, after = by[on[0] - 1], by[on[-1] + 1]
        same = lambda r: (r["world_scale"], r["diorama_scale"], r["table"])
        check(after["diorama"] == "1" and after["share"] == "0.000" and same(after) == same(before),
              f"diorama hands: inspection ended, the diorama back at once at its scale and place: {before['n']} {same(before)}, {after['n']} {after['diorama']} {after['share']} {same(after)}")
    looked = [int(r["n"]) for r in rows if 420 <= int(r["n"]) < 520 and r["fp"] == "1"]
    walked = [r["assist"] for r in rows if 420 <= int(r["n"]) < (looked[0] if looked else 520)]
    check(bool(looked) and "moving" in walked, f"diorama hands: the pinch through the table walked until the grab: {walked}, inspection {looked[:1]}..{looked[-1:]}")
    if looked:
        later = sorted({r["assist"] for r in rows if int(r["n"]) > looked[0] + 2})
        check(later == ["stopped"], f"diorama hands: inspection stopped the walk and none restarted: {later}")
        back = by[looked[-1] + 1]
        check(back["diorama"] == "1" and back["share"] == "0.000", f"diorama hands: the diorama back at once after the second inspection ({back['diorama']}, {back['share']})")
    panel = [r for r in rows if r["layers"] == "2" and int(r["n"]) >= 520]
    check(20 <= len(panel) <= 40 and len({r["game_frames"] for r in panel}) == 1 and all(r["diorama"] == "1" for r in panel),
          f"diorama hands: prayer hands showed the settings panel with the diorama for {len(panel)} frames, the game held")
    beside = sorted({beside_pane(r) for r in panel})
    check(bool(beside) and all(edge >= 20 and middle >= 30 for edge, middle in beside),
          f"diorama hands: the panel beside the pane (degrees round from its right edge and its middle): {beside[:3]}")
    print(f"diorama hands: the diorama from {first}; left, both and right grabs, sideways, turned and reacquired fists as on the screen; paused "
          f"{len(held)} frames, {len({r['head'] for r in held})} head poses, {moved:.1%} of the left eye changed from 110 to 125, the eyes' disparity "
          f"{depths}, the left eye's move {motion}; inspection on {on[:1]}..{on[-1:]}, the diorama back {same(after) if on else '-'}; the walk "
          f"{walked[:3]}..{walked[-1:]} stopped by inspection {looked[:1]}..{looked[-1:]}; the panel {len(panel)} frames, its middle {panel[0]['panel'] if panel else '-'}")
diorama_hands()

# Graphics' headset view switched by an agent in the session (the
# diorama-switch run): each settings_apply acknowledged with the settings
# it made; from the first headset frame after the switch to the screen, the
# screen alone (xr_view=screen, no diorama, share 1), at once; after the
# switch back, the diorama view (xr_view=diorama) eases in from the screen
# over frames between the two, each step within its time over the 0.3 s
# switch, to the diorama alone.
def diorama_switch():
    lines = open(f"{out}/diorama-switch.log").read().splitlines()
    replies, at, last = [], [], None
    for line in lines:
        if line.startswith("xrframe "):
            last = int(line.split()[1].split("=")[1])
        elif line.startswith("{"):
            replies.append(json.loads(line))
            at.append(last)
    views = [r.get("settings", {}).get("graphics", {}).get("xr", {}).get("view") for r in replies]
    check(views == ["Screen", "Diorama"] and all("revision" in r for r in replies) and None not in at,
          f"diorama switch: settings_apply answered {[r.get('error', v) for r, v in zip(replies, views)]} after headset frames {at}")
    if len(at) != 2 or None in at:
        return
    rows = records("diorama-switch.log")
    shown = lambda r: r["diorama"] == "1" and r["share"] == "0.000"
    before = [r for r in rows if at[0] - 10 < int(r["n"]) <= at[0]]
    screen = [r for r in rows if at[0] < int(r["n"]) <= at[1]]
    back = [r for r in rows if int(r["n"]) > at[1]]
    check(len(before) == 10 and all(r["xr_view"] == "diorama" and shown(r) for r in before), f"diorama switch: the diorama alone before the switch: {[(r['n'], r['diorama'], r['share']) for r in before]}")
    check(len(screen) >= 50 and all(r["xr_view"] == "screen" and r["diorama"] == "0" and r["share"] == "1.000" for r in screen),
          f"diorama switch: the screen alone from the first frame after the switch: {sorted({(r['xr_view'], r['diorama'], r['share']) for r in screen})}")
    jumps = [(b["n"], a["share"], b["share"]) for a, b in zip(back, back[1:])
             if abs(float(b["share"]) - float(a["share"])) > (int(b["presented_us"]) - int(a["presented_us"])) / 1e6 / 0.3 + 0.002]
    eased = [r["share"] for r in back if 0.0 < float(r["share"]) < 1.0]
    check(bool(back) and back[0]["xr_view"] == "diorama" and back[0]["share"] == "1.000" and eased and not jumps and len(back) >= 20 and all(shown(r) for r in back[-10:]),
          f"diorama switch: back to the diorama, eased in from the screen: first {back[0]['share'] if back else '-'}, eased over {eased}, jumps {jumps}")
    print(f"diorama switch: settings_apply after headset frames {at}; the screen alone from {screen[0]['n'] if screen else '-'} for {len(screen)} frames; "
          f"back to the diorama from the screen over {eased}, alone from {next((r['n'] for r in back if shown(r)), '-')}")
diorama_switch()

# The title movie and the title with the diorama as the view: on the screen,
# the movie's rebuilt frames at the screen's span in their own shape.
def diorama_title():
    rows = records("diorama-title.log")
    movie = [r for r in rows if r["movie"] == "1"]
    check(len(movie) >= 30 and all(r["xr_view"] == "diorama" and r["diorama"] == "0" and r["share"] == "1.000" and r["picture"] == spanned for r in movie),
          f"diorama title: the movie on the screen in its own shape: {sorted({(r['diorama'], r['share'], r['picture']) for r in movie})}")
    title = [r for r in rows if int(r["game_frames"]) >= 1450]
    check(len(title) >= 20 and all(r["diorama"] == "0" for r in title), "diorama title: the title (field 490) on the screen")
    print(f"diorama title: {len(movie)} rebuilt movie frames on the screen at {spanned}; the title on the screen in {len(title)} frames")
diorama_title()

# In every run with the diorama as the view, the view eases between the
# diorama and the screen: from a headset frame to the next the screen's
# share moves by no more than the time passed over the 0.3 s switch (but
# for the device's own pictures and first-person inspection, which take the
# view at once).
def diorama_easing():
    easing = {}
    for name in ("diorama.log", "field-diorama.log", "diorama-walk.log", "diorama-hands.log", "battle-diorama.log", "worldmap-diorama.log",
                 "diorama-scale.log", "binaural-diorama.log", "diorama-title.log", "diorama-menu.log", "diorama-battle.log", "diorama-worldmap.log",
                 "diorama-movie.log"):
        rows, jumps, eased = records(name), [], 0
        for a, b in zip(rows, rows[1:]):
            if "0" in (a["view"], b["view"]) or "1" in (a["fp"], b["fp"]):
                continue
            dt = (int(b["presented_us"]) - int(a["presented_us"])) / 1e6
            if abs(float(b["share"]) - float(a["share"])) > dt / 0.3 + 0.002:
                jumps.append((b["n"], a["share"], b["share"], round(dt, 3)))
            eased += 0.0 < float(b["share"]) < 1.0
        easing[name] = eased
        check(not jumps, f"{name}: the screen's share jumped: {jumps[:5]}")
    print(f"diorama easing, frames between the screen and the diorama: {easing}")
diorama_easing()

# The diorama's fallbacks: from the first frame the diorama shows alone,
# each run goes to the screen and back to a diorama, each way through frames
# between the two (D the diorama alone, E between, S the screen alone; the
# context the game reports in each). The settings panel stands beside the
# pane after a frame that showed the diorama (pane=), and below the screen
# after one where the screen stood in for it.
def fallback_panel(name):
    rows = records(name)
    wrong = [(b["n"], a["pane"], panel_place(b)) for a, b in zip(rows, rows[1:]) if panel_place(b) != ("beside" if a["pane"] != "-" else "below")]
    places = [panel_place(r) for r in rows[1:]]
    check(not wrong and "beside" in places and "below" in places, f"{name}: the settings panel beside the pane after the diorama and below the screen after the screen: {wrong[:5]}")
    return f"the panel beside the pane in {places.count('beside')} frames, below the screen in {places.count('below')}"
def diorama_fallbacks():
    for name, wanted, kinds, what in (
        ("diorama-menu.log", "DESED", ["walk", "menu", "walk"], "field 30's menu"),
        ("diorama-battle.log", "DESEDESED", ["walk", None, "battle", None, "walk"], "the Mountain Path's random battle"),
        ("diorama-worldmap.log", "DESEDESED", ["walk", None, "world_map", None, None], "Lahan, the world map and a field entered from it"),
    ):
        runs = []
        for r in records(name):
            share, shown = float(r["share"]), r["diorama"] == "1"
            phase = "D" if shown and share == 0.0 else "S" if share == 1.0 and not shown else "E"
            kind = json.loads(r["context"]).get("kind", "-")
            if runs and runs[-1][0] == phase:
                runs[-1][2] = int(r["n"])
                runs[-1][3].add(kind)
            else:
                runs.append([phase, int(r["n"]), int(r["n"]), {kind}])
        runs = runs[next((i for i, run in enumerate(runs) if run[0] == "D"), len(runs)):]
        told = ", ".join(f"{p} {a}-{b} {'/'.join(sorted(k))}" for p, a, b, k in runs)
        steady = [run for run in runs if run[0] != "E"]
        check("".join(run[0] for run in runs) == wanted and all(k is None or k in run[3] for k, run in zip(kinds, steady)),
              f"{name}: {what} from the diorama to the screen and back: {told}")
        print(f"diorama fallbacks, {what}: {told}; {fallback_panel(name)}")
    # Map 25's story movie: the diorama eases out over its first rebuilt
    # frames, the movie shows on the screen alone, and map 25's diorama
    # eases back in after it.
    rows = records("diorama-movie.log")
    phase = lambda r: "D" if r["diorama"] == "1" and float(r["share"]) == 0.0 else "S" if float(r["share"]) == 1.0 and r["diorama"] == "0" else "E"
    movie = [i for i, r in enumerate(rows) if r["movie"] == "1"]
    before = [i for i in range(movie[0]) if phase(rows[i]) == "D"] if movie else []
    out_ = [i for i in range(before[-1] + 1, movie[0] + 30) if phase(rows[i]) == "E"] if before else []
    alone = [i for i in movie if phase(rows[i]) == "S"]
    after = [i for i in range(movie[-1] + 1, len(rows)) if phase(rows[i]) == "D"] if movie else []
    back = [i for i in range(movie[-1] + 1, after[0]) if phase(rows[i]) == "E"] if after else []
    told = (f"the diorama alone to headset frame {rows[before[-1]]['n'] if before else '-'}, eased out over {len(out_)} frames, the movie's "
            f"{len(movie)} rebuilt frames ({len(alone)} on the screen alone), eased back in over {len(back)} to map 25's diorama from "
            f"{rows[after[0]]['n'] if after else '-'}")
    check(len(movie) >= 300 and before and out_ and len(alone) * 10 >= len(movie) * 9 and after and back, f"diorama-movie.log: map 25's movie between dioramas: {told}")
    print(f"diorama fallbacks, map 25's movie: {told}; {fallback_panel('diorama-movie.log')}")
diorama_fallbacks()

# The scene tour: each capture shows the screen in front of the eye, or the
# eye's picture from within the scene over its view; the grab looks from
# within only in a field and its dialogue, not on the title (field 490) or
# in the field's menu; the dialogue's RAM is xem-headless's.
def tour_screen(log, n, what, context=None):
    by = {int(r["n"]): r for r in records(log)}
    eyes = [png(f"{out}/{log[:-4]}/{n}/{side}.png") for side in ("left", "right")]
    check(all(lit(e[2]) > 0.05 for e in eyes) and by[n]["within"] == "0", f"tour, {what}: the screen is not in front ({[round(lit(e[2]), 3) for e in eyes]})")
    check(context is None or context in by[n]["context"], f"tour, {what}: the game shows {by[n]['context'][:60]}")
    return by[n]
def tour_within(log, n, what):
    # From within, the scene covers more than three quarters of each eye
    # (dark walls near the room's colour make up the rest); the screen in
    # front covers about a quarter.
    by = {int(r["n"]): r for r in records(log)}
    eyes = [png(f"{out}/{log[:-4]}/{n}/{side}.png") for side in ("left", "right")]
    check(by[n]["fp"] == "1" and by[n]["within"] == "1" and all(covered(e[2]) > 0.75 for e in eyes),
          f"tour, {what}: the eyes do not look from within the scene ({by[n]['fp']}, {by[n]['within']}, {[round(covered(e[2]), 3) for e in eyes]})")
def tour_grab(log, n, inspectable, what, hand="Right"):
    rows = records(log)
    by = {int(r["n"]): r for r in rows}
    grabbed = {"gesture": "Inspect", "hand": hand} in gestures_in(rows, n, n + 15)
    check(grabbed and by[n + 14]["fp"] == ("1" if inspectable else "0"), f"tour, {what}: the underhand grab ({grabbed}) left inspection {by[n + 14]['fp']}")
check(tour_screen("title.log", 290, "the title movie")["movie"] == "1", "tour: the title movie was not rebuilt")
tour_screen("title.log", 560, "the title", '"map":490')
tour_grab("title.log", 570, False, "the title")
tour_screen("hands.log", 160, "field 30's menu", '"menu"')
tour_screen("hands.log", 840, "field 30", '"walk"')
tour_within("hands.log", 875, "field 30")
tour_screen("dialogue.log", 100, "field 12's dialogue", '"dialogue"')
tour_grab("dialogue.log", 110, True, "the dialogue")
tour_within("dialogue.log", 135, "field 12's dialogue")
check(records("dialogue.log")[-1]["fp"] == "0", "tour: the grab again did not give the dialogue's screen back")
for log in ("title.log", "hands.log", "dialogue.log"):
    check(all(r["picture"] != "-" for r in records(log) if int(r["n"]) >= 5), f"{log}: a headset frame went without a picture")
reference = [json.loads(l) for l in open(f"{out}/dialogue-headless.log") if l.startswith("{") or l.startswith('"')]
reference = next(v for v in reference if isinstance(v, str))
check(final_hash("dialogue.log") == reference, f"dialogue.log: RAM at game frame 1000 {final_hash('dialogue.log')} != headless {reference}")
print(f"tour: the title movie, the title, field 30, its menu and field 12's dialogue on the screen, from within in the field and the "
      f"dialogue only; the dialogue's RAM {final_hash('dialogue.log')}")

# The world map: on the screen, the grab turning nothing on, scaling beside
# its control deck's keys (as many, as wide and as far beside the screen's
# edge at every scale); the app's Recenter puts the seat at the head and
# Monado's recentring of LOCAL puts it back at LOCAL's origin, the screen
# in front of the eyes after each.
rows = records("worldmap.log")
by = {int(r["n"]): r for r in rows}
check('"world_map"' in by[280]["context"] and lit(png(f"{out}/worldmap/280/left.png")[2]) > 0.05, f"the world map is not on the screen: {by[280]['context'][:40]}")
check({"gesture": "Inspect", "hand": "Right"} in gestures_in(rows, 300, 315) and all(r["fp"] == "0" for r in rows), "the world map's grab turned inspection on")
keys = {r["keys"] for r in rows if 315 <= int(r["n"]) < 380}
scales = [float(r["scale"]) for r in rows if 315 <= int(r["n"]) < 380]
check(len(keys) == 1 and next(iter(keys)) != "-" and max(scales) > 1.9 and abs(scales[-1] - 1.0) < 0.03,
      f"the deck's keys changed as the screen scaled ({sorted(keys)}, scale {min(scales):.2f}-{max(scales):.2f})")
count, widths, gaps = next(iter(keys)).split(":") if len(keys) == 1 and next(iter(keys)) != "-" else ("0", "0-0", "0-0")
check(widths == "0.2200-0.2200" and all(abs(float(g) - 0.04) < 0.001 for g in gaps.split("-")), f"the keys' widths {widths}, gaps {gaps}")
recentred = next((r for r in rows if r["recenters"] == "1"), None)
if recentred:
    head = [float(v) for v in recentred["head"].split(";")[0].split(",")]
    seat = [float(v) for v in recentred["seat"].split(";")[0].split(",")]
    check(int(recentred["n"]) == 380 and max(abs(a - b) for a, b in zip(head, seat)) < 0.001, f"Recenter at {recentred['n']}: seat {seat} for the head {head}")
check(recentred is not None, "no Recenter in the world map run")
events = [l for l in open(f"{out}/worldmap.log") if l.startswith("event reference_space_change")]
reset = next((r for r in rows if r["resets"] != "0"), None)
check(bool(events) and reset is not None and int(reset["n"]) > 380, f"Monado's recentring did not reach the session ({open(f'{out}/monado-ctl-worldmap.log').read().strip()!r})")
if reset:
    place, yaw = reset["seat"].split(";")
    check(max(abs(float(v)) for v in place.split(",")) < 1e-4 and abs(float(yaw)) < 1e-4, f"the seat after LOCAL's recentring: {reset['seat']}")
for n in (390, 440):
    eyes = [png(f"{out}/worldmap/{n}/{side}.png") for side in ("left", "right")]
    check(all(lit(e[2]) > 0.05 for e in eyes), f"world map at {n}: the screen is not in front after recentring")
print(f"world map: keys {sorted(keys)} at scales {min(scales):.2f}-{max(scales):.2f}; Recenter at {recentred and recentred['n']} "
      f"(seat {recentred and recentred['seat']}), LOCAL recentred at {reset and reset['n']} (seat {reset and reset['seat']})")

# Battles on foot and in Gears: on the screen, the grab turning nothing on;
# the Gears' RAM is xem-headless's.
for battle in ("battle", "gear"):
    rows = records(f"{battle}.log")
    by = {int(r["n"]): r for r in rows}
    for n in (560, 620):
        eyes = [png(f"{out}/{battle}/{n}/{side}.png") for side in ("left", "right")]
        check('"battle"' in by[n]["context"] and all(lit(e[2]) > 0.05 for e in eyes), f"{battle} at {n}: no battle on the screen ({by[n]['context'][:40]})")
    check({"gesture": "Inspect", "hand": "Right"} in gestures_in(rows, 590, 605) and all(r["fp"] == "0" for r in rows), f"{battle}: the grab turned inspection on")
    check(all(r["picture"] != "-" for r in rows if int(r["n"]) >= 5), f"{battle}: a headset frame went without a picture")
reference = [json.loads(l) for l in open(f"{out}/gear-headless.log") if l.startswith("{") or l.startswith('"')]
reference = next(v for v in reference if isinstance(v, str))
check(final_hash("gear.log") == reference, f"gear.log: RAM at game frame 2300 {final_hash('gear.log')} != headless {reference}")
print(f"battles: on foot and in Gears on the screen at headset frames 560 and 620, the grab turning nothing on; Gears' RAM {final_hash('gear.log')}")

# A battle and the world map as dioramas: from its first diorama frame on
# the diorama shows in every headset frame (through the battle's attacks
# and the world map's drive); the battle's table stands at two places
# only, the start view's look-at during its opening, then its combatants'
# middle to the end, while its camera's target moves; the world map's
# stays on its camera's target across (its frame's (0, height, 0): the map
# moves under it) at 25%, the target above its top within the headroom as
# the table settles towards the ground under the climbing vehicle; the
# eyes see 3D (their disparity grows with nearness) in the renderer's
# colours; the RAM is xem-headless's for the same log.
def diorama_run(name, frames, captures):
    rows = records(f"{name}-diorama.log")
    by = {int(r["n"]): r for r in rows}
    shown = [r for r in rows if r["diorama"] == "1"]
    first = int(shown[0]["n"]) if shown else None
    check(first is not None and all(r["diorama"] == "1" for r in rows if int(r["n"]) >= first), f"{name}: the diorama gave way to the screen after frame {first}")
    # The places the table's centre showed (world units), each from the
    # headset frame it first did.
    tables = []
    for r in shown:
        at = tuple(float(v) for v in r["table"].split(","))
        if not tables or max(abs(a - b) for a, b in zip(at, tables[-1][0])) > 1:
            tables.append((at, int(r["n"])))
    for n in captures:
        check(by[n]["diorama"] == "1" and by[n]["share"] == "0.000", f"{name} at {n}: the diorama alone was not shown ({by[n]['diorama']}, {by[n]['share']})")
        d = disparities(png(f"{out}/{name}-diorama/{n}/left.png"), png(f"{out}/{name}-diorama/{n}/right.png"))
        shifts = [s for _, s in d]
        check(len(d) >= 3 and max(shifts) - min(shifts) >= 5 and d[-1][1] > d[0][1], f"{name} at {n}: the eyes' disparity does not grow with nearness: {d}")
        for side in ("left", "right"):
            blocks, levels = covered_off(png(f"{out}/{name}-diorama/{n}/{side}.png"), png(f"{out}/{name}-diorama/{n}/{side}-diorama.png"))
            check(blocks >= 20 and levels <= 4, f"{name} at {n} {side}: the eye shows its diorama {levels:.1f} levels off over {blocks} covered blocks")
        print(f"{name} diorama {n}: disparity by band of rows (top first) {d}")
    reference = next(v for v in (json.loads(l) for l in open(f"{out}/{name}-diorama-headless.log") if l.startswith("{") or l.startswith('"')) if isinstance(v, str))
    check(final_hash(f"{name}-diorama.log") == reference, f"{name}-diorama.log: RAM at game frame {frames} {final_hash(f'{name}-diorama.log')} != headless {reference}")
    return rows, shown, tables
rows, shown, tables = diorama_run("battle", 2300, (600, 700))
moved = {r["target"] for r in shown}
check(len(tables) == 2 and len(moved) > 20, f"the battle's table stood at {len(tables)} places ({tables[:3]}..) while its camera's target took {len(moved)}")
print(f"battle diorama: shown from headset frame {shown[0]['n'] if shown else '-'}; the table at {tables}; the camera's target at {len(moved)} places; RAM {final_hash('battle-diorama.log')}")
rows, shown, tables = diorama_run("worldmap", 1500, (350, 420))
scales = {r["world_scale"] for r in shown}
centres = [tuple(float(v) for v in r["table"].split(",")) for r in shown]
targets = [tuple(float(v) for v in r["target"].split(",")) for r in shown]
heights = {t[1] for t in targets}
across = all(c[0] == 0 and c[2] == 0 and t[0] == 0 and t[2] == 0 for c, t in zip(centres, targets))
# How far above the table's top the target stands (y down; metres), within
# the headroom (eight tenths of the volume's 0.55 m above its top).
above = [round((c[1] - t[1]) * float(r["world_scale"]), 4) for c, t, r in zip(centres, targets, shown)]
check(across and len(heights) > 5 and above and min(above) >= -0.001 and max(above) <= 0.8 * 0.55 + 0.001 and above[-1] > 0.01 and scales == {"2.812500e-4"},
      f"the world map's table off its camera's target (across: {across}; the target {min(above, default='-')} to {max(above, default='-')} m above its top "
      f"over {len(heights)} heights), {sorted(scales)} m a unit")
print(f"world map diorama at 25%: shown from headset frame {shown[0]['n'] if shown else '-'}; the table on its target across, the target "
      f"{min(above, default='-')} to {max(above, default='-')} m above its top, at {min(heights, default='-')} to {max(heights, default='-')} (y down), "
      f"{sorted(scales)} m a unit; RAM {final_hash('worldmap-diorama.log')}")

# Two-hand scaling of the diorama: the spread enlarges it live and the
# contraction shrinks it, each committed to the setting once, when the hands
# let go, at the scale shown last; the camera's target stays at the table's
# anchor within a millimetre while the table's scale changes; a key beside
# the pane keeps its width; nothing else happens (no pause, press, command
# or walk) and the RAM at game frame 1200 is xem-headless's. Monado cannot
# show the room: Passthrough, asked for, changes nothing.
rows = records("diorama-scale.log")
by = {int(r["n"]): r for r in rows}
log = open(f"{out}/diorama-scale.log").read().splitlines()
commits = [int(l.split("diorama_scale=")[1].split()[0]) for l in log if l.startswith("scale commit ")]
check(final_hash("diorama-scale.log") == inspected, f"diorama-scale.log: RAM at game frame 1200 {final_hash('diorama-scale.log')} != headless {inspected}")
check(len(commits) == 2 and 240 <= commits[0] <= 260 and abs(commits[1] - commits[0] * 0.598) <= 2,
      f"the spread and the contraction were committed once each, about 2.5 and 0.6 times: {commits}")
check(not any("refused" in l for l in log if l.startswith("scale commit ")), "a scale was refused")
saved = json.load(open(f"{out}/diorama-scale.settings.json"))["graphics"]["xr"]
check(saved.get("diorama_scale") == (commits[-1] if commits else None), f"the setting keeps the last scale: {saved.get('diorama_scale')} for {commits}")
scales = [float(r["diorama_scale"]) for r in rows]
check(all(abs(s - 1.0) < 1e-4 for s in scales[:110]), f"the scale before the spread: {sorted(set(scales[:110]))}")
gestures = [json.loads(r["gestures"]) for r in rows]
named = {g["gesture"] for frame in gestures for g in frame}
check(named <= {"ScaleBegin", "Scale", "ScaleEnd"} and {"ScaleBegin", "ScaleEnd"} <= named, f"gestures other than scaling: {sorted(named)}")
check(all(r["player_pause"] == "0" and r["paused"] == "0" for r in rows), "scaling paused the game")
check(all(r["assist"] == rows[0]["assist"] for r in rows), f"scaling commanded the character: {sorted({r['assist'] for r in rows})}")
def scaled(begin):
    # The frames from a scaling's begin to its end, while the diorama shows.
    start = next((i for i, g in enumerate(gestures) if i >= begin and any(x["gesture"] == "ScaleBegin" for x in g)), None)
    end = next((i for i, g in enumerate(gestures) if start is not None and i > start and any(x["gesture"] == "ScaleEnd" for x in g)), None)
    return [] if start is None or end is None else [r for r in rows[start:end + 1] if r["diorama"] == "1"]
for name, begin, grows in (("spread", 110, True), ("contraction", 150, False)):
    frames_ = scaled(begin)
    check(len(frames_) >= 10, f"the {name} scaled over {len(frames_)} frames showing the diorama")
    if not frames_:
        continue
    world = [float(r["world_scale"]) for r in frames_]
    shown_ = [float(r["diorama_scale"]) for r in frames_]
    anchors = [[float(v) for v in r["anchor"].split(",")] for r in frames_]
    targets = [[float(v) for v in r["target"].split(",")] for r in frames_]
    moved = max(sum((a - b) ** 2 for a, b in zip(p, anchors[0])) ** 0.5 for p in anchors)
    drift = max(sum((a - b) ** 2 for a, b in zip(p, targets[0])) ** 0.5 for p in targets)
    ratio = world[-1] / world[0]
    check((ratio > 2.0) if grows else (ratio < 0.7), f"the {name}: the table's scale went {world[0]:.4e} to {world[-1]:.4e} ({ratio:.3f} times)")
    check(abs(ratio - shown_[-1] / shown_[0]) < 0.01, f"the {name}: the table scales by the diorama's scale ({ratio:.4f} against {shown_[-1] / shown_[0]:.4f})")
    check(drift < 1.0 and moved < 0.001, f"the {name}: the anchor moved {moved * 1000:.3f} mm (the camera's target {drift:.2f} units)")
    print(f"diorama {name}: headset frames {frames_[0]['n']}..{frames_[-1]['n']}, shown scale {shown_[0]:.4f} to {shown_[-1]:.4f}, "
          f"the table's {ratio:.4f} times, the anchor within {moved * 1000:.4f} mm (target within {drift:.3f} units)")
for commit in commits:
    # The commit is the scale the frame before showed.
    line = next(i for i, l in enumerate(log) if l.startswith(f"scale commit diorama_scale={commit} "))
    before = [l for l in log[:line] if l.startswith("xrframe ")][-1]
    shown_last = float(dict(f.split("=", 1) for f in before.split()[1:])["diorama_scale"])
    check(abs(shown_last * 100 - commit) <= 0.5, f"committed {commit}% after showing {shown_last:.4f}")
keys = {r["key"] for r in rows if r["diorama"] == "1"}
check(len(keys) == 1, f"a key beside the pane changed its width with the scale: {sorted(keys)}")
check(any(l.startswith("xr room: passthrough=unavailable blend_modes=OPAQUE") for l in log),
      f"Monado's simulated HMD offers the room: {[l for l in log if l.startswith('xr room')]}")
# No frame showed the room, and every rendered one submitted the projection
# layer alone (no passthrough layer; the panel never opens in this run).
check(all(r["passthrough"] == "0" for r in rows), "the room showed (passthrough) on an opaque display")
check(all(r["layers"] == "1" for r in rows if r["render"] == "1"), f"layers other than the projection: {sorted({r['layers'] for r in rows if r['render'] == '1'})}")
_, _, scaled_eye = png(f"{out}/diorama-scale/140/left.png")
check(scaled_eye[0][0:4] == left[0][0:3] + b"\xff", f"the room's backdrop is drawn, opaque, with Passthrough on: {tuple(scaled_eye[0][0:4])} against {tuple(left[0][0:3])}")
print(f"diorama scaling: committed {commits} (setting {saved.get('diorama_scale')}), a key {sorted(keys)} m wide; "
      f"{next((l for l in log if l.startswith('xr room')), 'no room line')}; RAM {final_hash('diorama-scale.log')}")

# The Binaural mode. Each frame's `binaural` field is what the presenter
# holds: its head and its listener's view ([yaw, pitch], degrees, yaw to
# the right), the listening position, and where each placed effect and
# music part heads ([id, azimuth, elevation], degrees). The head as the
# presenter should take it is the logged pose's turn from the seat, its
# axes (right, down, forward) as xem_core::binaural::head_axes makes them;
# taking them off a target (their transpose) gives the direction in the
# listener's view, which holds still while the scene does, however the head
# turns or moves, if the head is applied once (applied twice, or not at
# all, it would move with the head).
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)
def turned(q, v):
    x, y, z, w = q
    t = (2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0]))
    return (v[0] + w * t[0] + (y * t[2] - z * t[1]), v[1] + w * t[1] + (z * t[0] - x * t[2]), v[2] + w * t[2] + (x * t[1] - y * t[0]))
def heard_head(r):
    if r["head"] == "-":
        return None
    q = tuple(float(v) for v in r["head"].split(";")[1].split(","))
    half = float(r["seat"].split(";")[1]) / 2
    relative = qmul((0.0, -math.sin(half), 0.0, math.cos(half)), q)
    view = lambda v: (v[0], -v[1], -v[2])
    return [view(turned(relative, axis)) for axis in ((1, 0, 0), (0, -1, 0), (0, 0, -1))]
def facing(f):
    return math.degrees(math.atan2(f[0], f[2])), math.degrees(math.atan2(-f[1], math.hypot(f[0], f[2])))
def toward(azimuth, elevation):
    a, e = math.radians(azimuth), math.radians(elevation)
    return (math.sin(a) * math.cos(e), -math.sin(e), math.cos(a) * math.cos(e))
def taken_off(axes, d):
    return tuple(sum(axes[r][i] * d[r] for r in range(3)) for i in range(3))
def apart(a, b):
    chord = math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))
    return math.degrees(2 * math.asin(min(1.0, chord / 2)))
def presented(r):
    return json.loads(r["binaural"]) if r["binaural"] != "-" else None
def numbers(value):
    return [v for item in (value if isinstance(value, list) else [value]) for v in (numbers(item) if isinstance(item, list) else [item])]
reference = [json.loads(l) for l in open(f"{out}/binaural-headless.log") if l.startswith("{") or l.startswith('"')]
binaural_hash = next(v for v in reference if isinstance(v, str))
field15 = next(v for v in reference if isinstance(v, dict) and "actors" in v)
character = next(a["position"] for a in field15["actors"] if a["index"] == field15["player"])
def binaural(name, held):
    """The checks both runs share, `held` the frames the game stood still
    in: the presenter's head is the pose's turn from the seat, every frame
    (the game running or not) but the lost ones; the listening position is
    the controlled character's; nothing is NaN or infinite; and while the
    game stands still the listener's view and every music part hold still
    and each effect does once the head is taken off. Returns the frames,
    the held ones and each effect's direction in the view."""
    rows = records(f"{name}.log")
    worst, heads, odd, elsewhere = 0.0, 0, [], []
    for r in rows:
        b = presented(r)
        if b is None:
            continue
        values = numbers([b["head"] or [], b["view"] or [], b["at"] or [], b["effects"], b["music"]])
        if not all(isinstance(v, (int, float)) and math.isfinite(v) for v in values):
            odd.append(r["n"])
        axes = heard_head(r)
        if axes is not None and b["head"] is not None:
            yaw, pitch = facing(axes[2])
            worst = max(worst, abs((b["head"][0] - yaw + 180) % 360 - 180), abs(b["head"][1] - pitch))
            heads += 1
        if b["at"] is not None and b["at"] != [character["x"], character["y"], character["z"]]:
            elsewhere.append((r["n"], b["at"]))
    check(not odd, f"{name}: frames with numbers neither finite nor numbers: {odd[:5]}")
    check(not elsewhere, f"{name}: the listener away from the controlled character at {character}: {elsewhere[:3]}")
    check(heads > 300 and worst < 0.02, f"{name}: the presenter's head is the pose's turn from the seat within {worst:.4f} degrees over {heads} frames")
    still = [r for r in rows if held(r) and presented(r) is not None]
    views = {json.dumps(presented(r)["view"]) for r in still}
    check(len(still) > 100 and len(views) == 1 and "null" not in views, f"{name}: the listener's view held still while the game did: {sorted(views)[:3]}")
    parts = {}
    effects = {}
    yaws = []
    for r in still:
        b = presented(r)
        for part in b["music"]:
            parts.setdefault(part[0], set()).add(tuple(part[1:]))
        axes = heard_head(r)
        if axes is None:
            continue
        yaws.append(facing(axes[2])[0])
        for id_, azimuth, elevation in b["effects"]:
            effects.setdefault(id_, []).append(taken_off(axes, toward(azimuth, elevation)))
    check(len(parts) >= 4 and all(len(p) == 1 for p in parts.values()), f"{name}: music parts moved while the game stood still: { {k: len(v) for k, v in parts.items()} }")
    spread = max(yaws) - min(yaws) if yaws else 0.0
    off = {id_: max(apart(v[0], x) for x in v) for id_, v in effects.items()}
    check(len(effects) == 3 and all(len(v) > 80 for v in effects.values()) and max(off.values(), default=99) < 0.02 and spread > 3,
          f"{name}: with the head taken off, each effect holds still within {off} degrees as the head turns over {spread:.2f} degrees")
    return rows, still, {id_: v[0] for id_, v in effects.items()}, worst, spread, off

# The screen, then first-person inspection with the head's look.
rows, still, inspected, worst, spread, off = binaural("binaural", lambda r: r["player_pause"] == "1" and r["fp"] == "1")
by = {int(r["n"]): r for r in rows}
check(all(r["within"] == "1" for r in still) and min(int(r["n"]) for r in still) < 160, "binaural: the player's Pause came while inspection looked from within the scene")
# Inspection ended with the game held: the listener's view goes at once
# (the presentation's, applied paused or not, as the head is).
ended = [r for r in rows if r["player_pause"] == "1" and r["fp"] == "0" and int(r["n"]) > 200]
check(len(ended) >= 5 and all(presented(r)["view"] is None for r in ended[1:]),
      f"binaural: inspection ended with the game held, the listener's view went: {[(r['n'], presented(r)['view']) for r in ended[:3]]}")
check(final_hash("binaural.log") == binaural_hash, f"binaural.log: RAM at game frame 1200 {final_hash('binaural.log')} != headless {binaural_hash}")
# The head looks: the view inspection gives the listener is level on the
# camera's heading (the head does the pitching), so a direction is heard
# as the screen's (the game camera's) turned up by the camera's pitch, then
# by the head; the screen's directions themselves held still before
# inspection (the scene stands still).
view = presented(still[0])["view"]
screen = {}
for r in rows:
    b = presented(r)
    axes = heard_head(r)
    if int(r["n"]) < 118 and r["fp"] == "0" and b is not None and len(b["effects"]) == 3 and axes is not None:
        for id_, azimuth, elevation in b["effects"]:
            screen.setdefault(id_, []).append(taken_off(axes, toward(azimuth, elevation)))
pitch = math.radians(view[1])
level = [(1, 0, 0), (0, math.cos(pitch), math.sin(pitch)), (0, -math.sin(pitch), math.cos(pitch))]
held_screen = {id_: max(apart(v[0], x) for x in v) for id_, v in screen.items()}
looked = {id_: apart(tuple(sum(a * b for a, b in zip(row, screen[id_][-1])) for row in level), inspected[id_]) for id_ in inspected if id_ in screen}
check(abs(view[0]) < 0.01 and len(looked) == 3 and all(len(v) >= 30 for v in screen.values()) and max(held_screen.values()) < 0.02 and max(looked.values()) < 0.05,
      f"binaural: inspection hears the screen's directions turned by the camera's pitch ({view}) then the head: {looked} "
      f"(the screen's held within {held_screen} over {[len(v) for v in screen.values()]} frames)")
# The app's Recenter puts the seat at the head: the head is ahead (its
# pitch kept); Monado's recentring puts the seat back at LOCAL's origin.
recentred = next((r for r in rows if r["recenters"] == "1"), None)
check(recentred is not None and int(recentred["n"]) == 190 and abs(presented(recentred)["head"][0]) < 0.01,
      f"binaural: the Recenter turned the presenter's head ahead: {recentred and (recentred['n'], presented(recentred)['head'])}")
moved = next((r for r in rows if r["resets"] == "1"), None)
check(moved is not None and 220 <= int(moved["n"]) < 240 and moved["seat"].split(";")[1] in ("0.0000", "-0.0000") and moved["player_pause"] == "1",
      f"binaural: Monado's recentring reached the session in the pause: {moved and (moved['n'], moved['seat'])} ({open(f'{out}/monado-ctl-binaural.log').read().strip()!r})")
# Lost tracking keeps the last head, and what is heard with it.
lost = [int(r["n"]) for r in rows if r["head"] == "-" and int(r["n"]) > 100]
check(lost == list(range(240, 255)), f"binaural: the head was lost in frames {lost[:2]}..{lost[-1:]}")
if lost == list(range(240, 255)):
    before = presented(by[239])
    check(all(presented(by[n])["head"] == before["head"] and presented(by[n])["effects"] == before["effects"] for n in lost),
          "binaural: lost tracking kept the last head and the directions heard with it")
    check(presented(by[255])["head"] != before["head"], "binaural: found again, the head follows the pose")
print(f"binaural: the presenter's head is the pose's within {worst:.4f} degrees; inspecting, paused, the head turning over {spread:.2f} degrees "
      f"(the Recenter at 190, LOCAL recentred at {moved and moved['n']}, the head lost 240-254), each effect holds still in the view within "
      f"{max(off.values()):.4f} degrees; heard as the screen's turned by the camera's pitch {view[1]:.2f} within {max(looked.values()):.4f}; "
      f"RAM {final_hash('binaural.log')}")
# The diorama: the head's turn from the seat once, its position (Monado's
# simulated head moves) ignored, the listener at the character; two-hand
# scaling (no movement, no gesture but its own), Monado's recentring and
# the app's Recenter change nothing heard but the head's turn.
rows, still, _, worst, spread, off = binaural("binaural-diorama", lambda r: 150 <= int(r["n"]) < 410)
check(all(r["diorama"] == "1" and r["share"] == "0.000" and r["paused"] == "1" for r in still), "binaural-diorama: the diorama alone showed while the game was held")
check(len({r["game_frames"] for r in still}) == 1, "binaural-diorama: the game ran while it was held")
check(final_hash("binaural-diorama.log") == binaural_hash, f"binaural-diorama.log: RAM at game frame 1200 {final_hash('binaural-diorama.log')} != headless {binaural_hash}")
# From the setting's 25% to the 400% limit and back to the 25% limit, each
# gesture committed once.
scales = [float(r["diorama_scale"]) for r in still]
kinds = {g["gesture"] for r in still for g in json.loads(r["gestures"])}
committed = [int(m.group(1)) for l in open(f"{out}/binaural-diorama.log") if (m := re.match(r"scale commit diorama_scale=(\d+) revision=", l))]
check(scales[0] == 0.25 and max(scales) == 4.0 and scales[-1] == 0.25 and len(committed) == 10 and max(committed) == 400 and committed[-1] == 25
      and kinds <= {"ScaleBegin", "Scale", "ScaleEnd"} and all(r["assist"] == "-" for r in still),
      f"binaural-diorama: two-hand scaling from {scales[0]} to {max(scales)} and back to {scales[-1]} (committed {committed}) gave only its own gestures {sorted(kinds)}, moved nobody")
places = [tuple(float(v) for v in r["head"].split(";")[0].split(",")) for r in still if r["head"] != "-"]
reach = max(math.dist(p, q) for p in places for q in places[::10])
check(reach > 0.1, f"binaural-diorama: the head moved {reach:.3f} m while the game was held")
recentred = next((r for r in rows if r["recenters"] == "1"), None)
moved = next((r for r in rows if r["resets"] == "1"), None)
check(recentred is not None and int(recentred["n"]) == 265 and abs(presented(recentred)["head"][0]) < 0.01,
      f"binaural-diorama: the Recenter turned the presenter's head ahead: {recentred and (recentred['n'], presented(recentred)['head'])}")
check(moved is not None and 240 <= int(moved["n"]) < 265, f"binaural-diorama: Monado's recentring reached the session in the pause ({open(f'{out}/monado-ctl-binaural-diorama.log').read().strip()!r})")
print(f"binaural diorama: the presenter's head within {worst:.4f} degrees of the pose; the game held, the head turning over {spread:.2f} degrees and "
      f"moving {reach:.3f} m, scaled {scales[0]:.2f}-{max(scales):.2f}-{scales[-1]:.2f} (committed {committed}), LOCAL recentred at {moved and moved['n']} and "
      f"recentred at 265: the view held still and each effect in it within {max(off.values()):.4f} degrees; RAM {final_hash('binaural-diorama.log')}")
# The session's end gives the game back to a flat host (Android's window):
# the presenter keeps neither the head nor the listener's view it had.
for name in ("binaural", "binaural-diorama"):
    last = presented(records(f"{name}.log")[-1])
    ended = next((l for l in open(f"{out}/{name}.log") if l.startswith("exit frames=")), "")
    left = json.loads(ended.split(" binaural=", 1)[1]) if " binaural=" in ended else None
    check(last["head"] is not None and (name == "binaural" or last["view"] is not None) and left is not None and left["head"] is None and left["view"] is None,
          f"{name}: the session ended with the presenter's head {last['head']} and view {last['view']}, leaving {left}")

if problems:
    print("xr-game-smoke: FAILED\n  " + "\n  ".join(problems))
    sys.exit(1)
print("xr-game-smoke: passed")
EOF
