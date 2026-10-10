#!/usr/bin/env bash
# Runs xem-xr-demo against Monado without a display or headset and checks the
# session lifecycle, frame scheduling, stereo views, layers, swapchain protocol
# and head tracking with the simulation running and paused.
#
#   nix develop path:./nix/runtime -c runtime/scripts/xr-smoke.sh
#
# Monado runs as its own service (monado-service) with the null compositor (no
# window; it accepts and retires frames), the simulated HMD turning continuously
# (SIMULATED_ROTATE) and two simulated simple controllers. Each run captures the
# two eye layers and the panel image from the swapchains before release.
#
# XEM_XR_COMPOSITED=1 adds a run under Monado's real compositor, presenting to a
# headless weston, and screenshots weston: the runtime's own composition of the
# projection and panel quad layers.
#
# Monado and the demo use lavapipe unless VK_ICD_FILENAMES is set (e.g. by
# runtime/scripts/xem-gpu-host). Logs and images go to runtime/target/xr-smoke
# (XEM_XR_OUT).
set -euo pipefail

runtime=$(cd "$(dirname "$0")/.." && pwd)
out=${XEM_XR_OUT:-$runtime/target/xr-smoke}
frames=${XEM_XR_FRAMES:-90}
: "${XEM_MONADO_RUNTIME:?run inside nix develop path:./nix/runtime}"
export VK_ICD_FILENAMES=${VK_ICD_FILENAMES:-$XEM_LAVAPIPE_ICD}
export XR_RUNTIME_JSON=$XEM_MONADO_RUNTIME

(cd "$runtime" && cargo build -p xem-xr --example xem-xr-demo)
demo=$runtime/target/debug/examples/xem-xr-demo

rm -rf "$out"
mkdir -p "$out"
# A private runtime directory keeps these services apart from any other Monado
# or Wayland session; it must be short, as the IPC socket path has to fit in a
# sockaddr_un.
rundir=$(mktemp -d "${XDG_RUNTIME_DIR:-/tmp}/xem-xr.XXXXXX")
export XDG_RUNTIME_DIR=$rundir
services=()
stop_services() {
    # Newest first: Monado before the Wayland server it presents to.
    # Monado's Wayland compositor may not return from SIGTERM: kill it after 5 s.
    local i
    for ((i = ${#services[@]} - 1; i >= 0; i--)); do
        kill "${services[i]}" 2>/dev/null || true
        timeout 5 tail --pid="${services[i]}" -f /dev/null || kill -9 "${services[i]}" 2>/dev/null || true
        wait "${services[i]}" 2>/dev/null || true
    done
    services=()
}
trap 'stop_services; rm -rf "$rundir"' EXIT

wait_for_socket() {
    local socket=$1 log=$2
    for _ in $(seq 100); do
        [[ -S $socket ]] && return
        kill -0 "${services[-1]}" 2>/dev/null || break
        sleep 0.1
    done
    echo "xr-smoke: no $socket; see $log" >&2
    tail -20 "$log" >&2
    exit 1
}

start_monado() {
    local log=$1
    shift
    # A stopped service leaves its socket file behind.
    rm -f "$rundir/monado_comp_ipc"
    env XRT_NO_STDIN=1 SIMULATED_ENABLE=1 SIMULATED_ROTATE=1 \
        SIMULATED_LEFT=simple SIMULATED_RIGHT=simple "$@" monado-service >"$log" 2>&1 &
    services+=($!)
    wait_for_socket "$rundir/monado_comp_ipc" "$log"
}

start_monado "$out/monado.log" XRT_COMPOSITOR_NULL=1
"$demo" --exit-after-frames "$frames" --capture "$out/running" >"$out/running.log" 2>"$out/running.err"
"$demo" --exit-after-frames "$frames" --paused --capture "$out/paused" >"$out/paused.log" 2>"$out/paused.err"
stop_services

if [[ ${XEM_XR_COMPOSITED:-0} == 1 ]]; then
    weston --backend=headless --renderer=pixman --width=1600 --height=900 \
        --socket=wl-xem --debug --idle-time=0 >"$out/weston.log" 2>&1 &
    services+=($!)
    wait_for_socket "$rundir/wl-xem" "$out/weston.log"
    start_monado "$out/monado-composited.log" WAYLAND_DISPLAY=wl-xem XRT_COMPOSITOR_FORCE_WAYLAND=1
    "$demo" --exit-after-frames 150 >"$out/composited.log" 2>"$out/composited.err" &
    run=$!
    sleep 6
    mkdir -p "$out/composited"
    (cd "$out/composited" && WAYLAND_DISPLAY=wl-xem weston-screenshooter)
    wait "$run"
    stop_services
fi

python3 -I - "$out" "$frames" <<'EOF'
import sys
from pathlib import Path

out, expected = Path(sys.argv[1]), int(sys.argv[2])
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def parse(name):
    frames, states = [], []
    for line in (out / f"{name}.log").read_text().splitlines():
        kind, _, rest = line.partition(" ")
        fields = dict(f.split("=", 1) for f in rest.split(" ") if "=" in f)
        if kind == "frame":
            frames.append(fields)
        elif kind == "event" and "state" in fields:
            states.append(fields["state"])
    return frames, states


runs = ["running", "paused"] + (["composited"] if (out / "composited.log").exists() else [])
for name in runs:
    frames, states = parse(name)
    label = f"{name}:"
    count = expected if name != "composited" else 150
    check(len(frames) == count, f"{label} {len(frames)} frames, expected {count}")
    wanted = ["READY", "SYNCHRONIZED", "VISIBLE", "FOCUSED", "STOPPING", "EXITING"]
    seen = iter(states)
    check(all(s in seen for s in wanted), f"{label} states {states} miss the order {wanted}")
    times = [int(f["pdt_ns"]) for f in frames]
    check(all(b > a for a, b in zip(times, times[1:])), f"{label} predicted display times not increasing")
    check(all(int(f["period_ns"]) > 0 for f in frames), f"{label} zero display period")
    rendered = [f for f in frames if f["render"] == "1"]
    check(len(rendered) >= count - 2, f"{label} only {len(rendered)} frames rendered")
    panel_updates = 0
    for f in frames:
        n = f["n"]
        ops = [op.rstrip("0123456789") for op in f["ops"].split(",")]
        eyes = [op for op in ops if op.startswith("eyes.")]
        panel = [op for op in ops if op.startswith("panel.")]
        protocol = ["acquire", "wait", "submit", "release"]
        check(eyes == ([f"eyes.{op}" for op in protocol] if f["render"] == "1" else []),
              f"{label} frame {n}: eye swapchain order {eyes}")
        check(panel in ([], [f"panel.{op}" for op in protocol]),
              f"{label} frame {n}: panel swapchain order {panel}")
        panel_updates += bool(panel)
    for f in rendered:
        n = f["n"]
        check(f["views_valid"] == "1", f"{label} frame {n}: views not tracked")
        check(f["eye_l"] != f["eye_r"] and float(f["ipd"]) > 0.03, f"{label} frame {n}: eyes not distinct")
        check(f["layers"] == "projection,quad", f"{label} frame {n}: layers {f['layers']}")
    check(panel_updates >= 1, f"{label} the panel image was never updated")
    heads = {f["head"] for f in frames}
    check(len(heads) > len(frames) // 2, f"{label} head pose changed only {len(heads)} times")
    sims = [float(f["sim_time"]) for f in frames]
    if name == "paused":
        check(len(set(sims)) == 1, f"{label} simulation time moved while paused: {sorted(set(sims))[:4]}")
    else:
        check(all(b > a for a, b in zip(sims, sims[1:])), f"{label} simulation time not advancing")
    if name == "composited":
        check(any((out / name).glob("*.png")), f"{label} no weston screenshot")
    for image in ["left.png", "right.png", "panel.png"] if name != "composited" else []:
        check((out / name / image).is_file(), f"{label} no {image}")
    periods = sorted({int(f["period_ns"]) for f in frames})
    print(f"{name}: {len(frames)} frames, {len(rendered)} rendered, {panel_updates} panel updates, "
          f"states {' '.join(states)}, {len(heads)} distinct head poses, "
          f"sim_time {sims[0]:.4f}..{sims[-1]:.4f}, ipd {rendered[-1]['ipd'] if rendered else '-'}, "
          f"periods_ns {periods[0]}..{periods[-1]}")

for failure in failures:
    print("FAIL", failure)
sys.exit(1 if failures else 0)
EOF
echo "xr-smoke: passed ($out)"
