"""Capture original-program observations with an external libretro emulator.

ABI reference: https://raw.githubusercontent.com/libretro/libretro-common/master/include/libretro.h
Run from the repository root inside the pinned Nix observation shell. Output stays
private. Emulated observations require corroboration; this tool is never a native
game runtime and does not establish hardware timing or recovered game semantics.
"""

from __future__ import annotations

import argparse
import ctypes as ct
import hashlib
import json
import os
import shutil
import struct
import sys
import time
import wave
import zlib
from datetime import UTC, datetime
from pathlib import Path

if __package__:
    from .instruction_trace import InstructionTrace, load_instruction_trace
    from .memory_sampler import load_sampling
    from .scenario_program import ScenarioProgram
else:
    from instruction_trace import InstructionTrace, load_instruction_trace
    from memory_sampler import load_sampling
    from scenario_program import ScenarioProgram

ROOT = Path(__file__).resolve().parents[2]
BUTTONS = {
    "cross": 0,
    "square": 1,
    "select": 2,
    "start": 3,
    "up": 4,
    "down": 5,
    "left": 6,
    "right": 7,
    "circle": 8,
    "triangle": 9,
    "l1": 10,
    "r1": 11,
    "l2": 12,
    "r2": 13,
}


class GameInfo(ct.Structure):
    _fields_ = [
        ("path", ct.c_char_p),
        ("data", ct.c_void_p),
        ("size", ct.c_size_t),
        ("meta", ct.c_char_p),
    ]


class SystemInfo(ct.Structure):
    _fields_ = [
        ("name", ct.c_char_p),
        ("version", ct.c_char_p),
        ("extensions", ct.c_char_p),
        ("fullpath", ct.c_bool),
        ("block_extract", ct.c_bool),
    ]


class Variable(ct.Structure):
    _fields_ = [("key", ct.c_char_p), ("value", ct.c_char_p)]


class Message(ct.Structure):
    _fields_ = [("text", ct.c_char_p), ("frames", ct.c_uint)]


class Geometry(ct.Structure):
    _fields_ = [
        ("width", ct.c_uint),
        ("height", ct.c_uint),
        ("max_width", ct.c_uint),
        ("max_height", ct.c_uint),
        ("aspect", ct.c_float),
    ]


class Timing(ct.Structure):
    _fields_ = [("fps", ct.c_double), ("sample_rate", ct.c_double)]


class AvInfo(ct.Structure):
    _fields_ = [("geometry", Geometry), ("timing", Timing)]


Environment = ct.CFUNCTYPE(ct.c_bool, ct.c_uint, ct.c_void_p)
Video = ct.CFUNCTYPE(None, ct.c_void_p, ct.c_uint, ct.c_uint, ct.c_size_t)
Audio = ct.CFUNCTYPE(None, ct.c_int16, ct.c_int16)
AudioBatch = ct.CFUNCTYPE(ct.c_size_t, ct.POINTER(ct.c_int16), ct.c_size_t)
InputPoll = ct.CFUNCTYPE(None)
InputState = ct.CFUNCTYPE(ct.c_int16, ct.c_uint, ct.c_uint, ct.c_uint, ct.c_uint)


def sha256_file(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def validate_inputs(schedule: object, frames: int) -> list[dict]:
    if not isinstance(schedule, list) or len(schedule) > 10000:
        raise ValueError("Input schedule must be a list of at most 10000 intervals")
    for interval in schedule:
        if not isinstance(interval, dict) or set(interval) != {"start", "end", "buttons"}:
            raise ValueError("Each interval requires only start, end, and buttons")
        if type(interval["start"]) is not int or type(interval["end"]) is not int:
            raise ValueError("Input interval bounds must be integer frame indices")
        if not 0 <= interval["start"] < interval["end"] <= frames:
            raise ValueError("Input interval lies outside the requested frame budget")
        if not isinstance(interval["buttons"], list) or any(
            not isinstance(button, str) or button not in BUTTONS for button in interval["buttons"]
        ):
            raise ValueError("Input interval has unknown button names")
        if len(set(interval["buttons"])) != len(interval["buttons"]):
            raise ValueError("Input interval repeats a button")
    return schedule


def validate_reference_state(path: Path, identity: dict) -> bytes:
    if path.name != "final.state":
        raise ValueError("Only final.state from a completed observation can be restored")
    manifest = json.loads((path.parent / "observation.json").read_text())
    for key in (
        "schema_version",
        "source_profile",
        "content_sha256",
        "core_sha256",
        "bios_sha256",
        "effective_options",
    ):
        if key not in manifest or manifest[key] != identity[key]:
            raise ValueError(f"Reference state has incompatible {key}")
    if not 0 < path.stat().st_size <= 128 * 1024 * 1024:
        raise ValueError("Reference state is empty or oversized")
    if manifest.get("final_state_sha256") != sha256_file(path):
        raise ValueError("Reference state checksum mismatch")
    return path.read_bytes()


def write_png(
    path: Path, raw: bytes, width: int, height: int, pitch: int, pixel_format: int
) -> None:
    if pixel_format not in (0, 1, 2) or not (1 <= width <= 2048 and 1 <= height <= 1024):
        raise ValueError("Unsupported pixel format or image dimensions")
    pixel_bytes = 4 if pixel_format == 1 else 2
    if not width * pixel_bytes <= pitch <= 16384 or len(raw) != pitch * height:
        raise ValueError("Truncated or inconsistent image pitch/buffer")
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            if pixel_format == 1:
                value = int.from_bytes(
                    raw[y * pitch + x * 4 : y * pitch + x * 4 + 4], sys.byteorder
                )
                channels = ((value >> 16) & 255, (value >> 8) & 255, value & 255)
            else:
                value = int.from_bytes(
                    raw[y * pitch + x * 2 : y * pitch + x * 2 + 2], sys.byteorder
                )
                if pixel_format == 2:
                    r, g, b = (value >> 11) & 31, (value >> 5) & 63, value & 31
                    channels = ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))
                else:
                    r, g, b = (value >> 10) & 31, (value >> 5) & 31, value & 31
                    channels = ((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2))
            rows.extend(channels)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        )

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=os.environ.get("XEM_REFERENCE_CORE"))
    parser.add_argument("--content", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=900)
    parser.add_argument("--capture-every", type=int, default=300)
    parser.add_argument("--inputs", type=Path)
    parser.add_argument("--load-state", type=Path)
    parser.add_argument("--program", type=Path, help="Compiled cold-boot scenario program")
    parser.add_argument("--sample-memory", type=Path, help="Bounded read-only RAM sampling JSON")
    parser.add_argument(
        "--trace-instructions", type=Path, help="Guarded instruction-address trace JSON"
    )
    parser.add_argument("--bios", type=Path, help="Optional user-supplied 512 KiB PS1 BIOS dump")
    args = parser.parse_args()
    if not 1 <= args.frames <= 36000 or args.capture_every < 1:
        parser.error("Frame budget must be 1..36000 and capture interval positive")
    if Path.cwd().resolve() != ROOT:
        parser.error("Run from the repository root")
    if (
        not args.core
        or not args.core.is_file()
        or not str(args.core.resolve()).startswith("/nix/store/")
    ):
        parser.error("A Nix-provided core is required; enter the observation shell")
    if not args.content.is_file() or args.content.suffix.lower() != ".chd":
        parser.error("Content must be a CHD matching a selected reference profile")
    content_hash = sha256_file(args.content)
    profiles = json.loads((ROOT / "analysis/reference-profiles.json").read_text())["profiles"]
    profile = next(
        (row for row in profiles if row["measurement"]["source"]["chd"]["sha256"] == content_hash),
        None,
    )
    if profile is None:
        parser.error("Unsupported CHD hash; select and verify the source before observation")
    if args.bios and (not args.bios.is_file() or args.bios.stat().st_size != 512 * 1024):
        parser.error("A supplied PS1 BIOS dump must be exactly 512 KiB")
    program = None
    if args.program:
        if args.inputs or args.load_state:
            parser.error("Scenario programs cold-boot with their own ordered inputs")
        try:
            program = ScenarioProgram(json.loads(args.program.read_text()))
        except (ValueError, OSError) as error:
            parser.error(str(error))
        if program.program["source_profile"] != profile["id"]:
            parser.error("Scenario program targets a different source profile")
    sampler, sampling_bytes = None, None
    if args.sample_memory:
        try:
            sampler, sampling_bytes = load_sampling(args.sample_memory)
        except (ValueError, OSError) as error:
            parser.error(str(error))
        if sampler.spec["source_profile"] != profile["id"]:
            parser.error("Memory sampling targets a different source profile")
    instruction_spec, instruction_bytes = None, None
    if args.trace_instructions:
        if not program:
            parser.error("Instruction tracing requires a cold-boot scenario program")
        try:
            instruction_spec, instruction_bytes = load_instruction_trace(args.trace_instructions)
        except (ValueError, OSError) as error:
            parser.error(str(error))
        if instruction_spec["source_profile"] != profile["id"]:
            parser.error("Instruction tracing targets a different source profile")
    try:
        schedule = validate_inputs(
            json.loads(args.inputs.read_text()) if args.inputs else [], args.frames
        )
    except (ValueError, OSError) as error:
        parser.error(str(error))
    out = args.output.resolve()
    if not out.is_relative_to(ROOT / ".local") or (ROOT / ".local").is_symlink():
        parser.error("All original-content capture must stay in ignored .local/")
    out.mkdir(parents=True, exist_ok=False)
    if sampler:
        (out / "memory-sampling.json").write_bytes(sampling_bytes)
    if instruction_spec:
        (out / "instruction-trace-spec.json").write_bytes(instruction_bytes)
    system = out / "system"
    saves = out / "saves"
    system.mkdir()
    saves.mkdir()
    if args.bios:
        shutil.copyfile(args.bios, system / "scph-user.bin")
    directories = {9: str(system).encode(), 31: str(saves).encode()}
    options = {
        b"pcsx_rearmed_bios": b"auto" if args.bios else b"HLE",
        b"pcsx_rearmed_gpu_thread_rendering": b"disabled",
    }
    if program:
        # Scenario instrumentation can modify original instructions. The
        # interpreter avoids stale generated code after a guarded RAM write.
        options[b"pcsx_rearmed_drc"] = b"disabled"
    defaults = {}
    available_options = {}
    environment_calls = set()
    notices = []
    errors = []
    frame = 0
    pixel_format = 0
    latest = None
    audio_frames = 0
    audio_digest = hashlib.sha256()
    wav = None
    shutdown = False

    @Environment
    def environment(command, data):
        nonlocal pixel_format, shutdown
        try:
            environment_calls.add(command)
            if command in directories:
                ct.cast(data, ct.POINTER(ct.c_char_p))[0] = directories[command]
                return True
            if command in (2, 3, 17):
                ct.cast(data, ct.POINTER(ct.c_bool))[0] = command == 3
                return True
            if command == 10:
                value = ct.cast(data, ct.POINTER(ct.c_int))[0]
                if value not in (0, 1, 2):
                    return False
                pixel_format = value
                return True
            if command == 15:
                value = ct.cast(data, ct.POINTER(Variable)).contents
                selected = options.get(value.key, defaults.get(value.key))
                value.value = selected
                return selected is not None
            if command == 16:
                values = ct.cast(data, ct.POINTER(Variable))
                for index in range(4096):
                    value = values[index]
                    if not value.key:
                        for key, selected in options.items():
                            if (
                                key not in available_options
                                or selected not in available_options[key]
                            ):
                                raise ValueError(f"Unsupported core option {key!r}={selected!r}")
                        return True
                    choices = value.value.split(b"; ", 1)[1].split(b"|")
                    available_options[value.key] = choices
                    defaults[value.key] = choices[0]
                raise ValueError("Unterminated core option table")
            if command in (39, 52, 57, 59):
                ct.cast(data, ct.POINTER(ct.c_uint))[0] = 0
                return True
            if command == (47 | 0x10000):
                if data:
                    ct.cast(data, ct.POINTER(ct.c_uint))[0] = 3
                return True
            if command == 6:
                notice = ct.cast(data, ct.POINTER(Message)).contents
                notices.append({"frame": frame, "message": notice.text.decode(errors="replace")})
                return True
            if command == 7:
                shutdown = True
                return True
            if command in (8, 11, 18, 32, 35, 37, 55):
                return True
            return False
        except Exception as error:
            errors.append(f"Environment {command}: {error}")
            return False

    @Video
    def video(data, width, height, pitch):
        nonlocal latest
        if not data:
            return
        if data == ct.c_void_p(-1).value or width > 2048 or height > 1024 or pitch > 16384:
            errors.append("Unsupported or oversized video buffer")
            return
        if width and height:
            latest = (ct.string_at(data, pitch * height), width, height, pitch, pixel_format)

    def accept_audio(data: bytes, count: int) -> None:
        nonlocal audio_frames
        audio_frames += count
        audio_digest.update(data)
        if wav:
            wav.writeframesraw(data)

    @Audio
    def audio(left, right):
        accept_audio(struct.pack("<hh", left, right), 1)

    @AudioBatch
    def audio_batch(data, count):
        if count > 44100 * 10:
            errors.append("Oversized audio callback")
            return 0
        accept_audio(ct.string_at(data, count * 4), count)
        return count

    @InputPoll
    def input_poll():
        pass

    def held_buttons(at_frame):
        held = {
            name
            for item in schedule
            if item["start"] <= at_frame < item["end"]
            for name in item["buttons"]
        }
        if program:
            held.update(program.buttons)
        return held

    @InputState
    def input_state(port, device, index, identifier):
        if port != 0 or device != 1 or index != 0:
            return 0
        held = {BUTTONS[name] for name in held_buttons(frame)}
        if identifier == 256:
            return sum(1 << value for value in held)
        return int(identifier in held)

    core = ct.CDLL(str(args.core.resolve()))
    prototypes = {
        "retro_api_version": (ct.c_uint, []),
        "retro_set_environment": (None, [Environment]),
        "retro_set_video_refresh": (None, [Video]),
        "retro_set_audio_sample": (None, [Audio]),
        "retro_set_audio_sample_batch": (None, [AudioBatch]),
        "retro_set_input_poll": (None, [InputPoll]),
        "retro_set_input_state": (None, [InputState]),
        "retro_get_system_info": (None, [ct.POINTER(SystemInfo)]),
        "retro_get_system_av_info": (None, [ct.POINTER(AvInfo)]),
        "retro_init": (None, []),
        "retro_load_game": (ct.c_bool, [ct.POINTER(GameInfo)]),
        "retro_set_controller_port_device": (None, [ct.c_uint, ct.c_uint]),
        "retro_run": (None, []),
        "retro_get_memory_data": (ct.c_void_p, [ct.c_uint]),
        "retro_get_memory_size": (ct.c_size_t, [ct.c_uint]),
        "retro_serialize_size": (ct.c_size_t, []),
        "retro_serialize": (ct.c_bool, [ct.c_void_p, ct.c_size_t]),
        "retro_unserialize": (ct.c_bool, [ct.c_void_p, ct.c_size_t]),
        "retro_unload_game": (None, []),
        "retro_deinit": (None, []),
    }
    for name, (restype, argtypes) in prototypes.items():
        function = getattr(core, name)
        function.restype = restype
        function.argtypes = argtypes
    if core.retro_api_version() != 1:
        raise RuntimeError("Unsupported libretro API version")
    core.retro_set_environment(environment)
    core.retro_set_video_refresh(video)
    core.retro_set_audio_sample(audio)
    core.retro_set_audio_sample_batch(audio_batch)
    core.retro_set_input_poll(input_poll)
    core.retro_set_input_state(input_state)
    info = SystemInfo()
    core.retro_get_system_info(ct.byref(info))
    if info.name != b"PCSX-ReARMed":
        raise RuntimeError("This observation frontend is qualified only for PCSX-ReARMed")
    if errors:
        raise RuntimeError(errors)
    core.retro_init()
    content = GameInfo(str(args.content.resolve()).encode(), None, 0, None)
    if not core.retro_load_game(ct.byref(content)):
        raise RuntimeError("External core could not load content")
    core.retro_set_controller_port_device(0, 1)
    av = AvInfo()
    core.retro_get_system_av_info(ct.byref(av))
    identity = {
        "schema_version": 1,
        "source_profile": profile["id"],
        "content_sha256": content_hash,
        "core_sha256": sha256_file(args.core),
        "bios_sha256": sha256_file(args.bios) if args.bios else None,
        "effective_options": {
            key.decode(): value.decode() for key, value in (defaults | options).items()
        },
    }
    if errors:
        raise RuntimeError(errors)
    if args.bios and any("hle" in notice["message"].lower() for notice in notices):
        raise RuntimeError("Core reported HLE despite a supplied BIOS; capture is not accepted")
    if args.load_state:
        state_data = validate_reference_state(args.load_state, identity)
        state_buffer = ct.create_string_buffer(state_data)
        if not core.retro_unserialize(state_buffer, len(state_data)):
            raise RuntimeError("External reference state rejected")
    wav = wave.open(str(out / "audio.wav"), "wb")
    wav.setnchannels(2)
    wav.setsampwidth(2)
    wav.setframerate(round(av.timing.sample_rate))
    report = {
        **identity,
        "kind": "provisional_external_emulator_observation",
        "bios": (
            "user-supplied, auto selection requested"
            if args.bios
            else "HLE; timing and compatibility require corroboration"
        ),
        "created_at_utc": datetime.now(UTC).isoformat(),
        "tool_sha256": sha256_file(Path(__file__)),
        "nix_lock_sha256": sha256_file(ROOT / "nix/flake.lock"),
        "argv": sys.argv,
        "working_directory": str(ROOT),
        "core_path": str(args.core.resolve()),
        "core_name": info.name.decode(),
        "core_version": info.version.decode(),
        "content_path": str(args.content.resolve()),
        "inputs": schedule,
        "fps_reported_by_core": av.timing.fps,
        "sample_rate_reported_by_core": av.timing.sample_rate,
        "initial_reference_state_sha256": sha256_file(args.load_state) if args.load_state else None,
        "limitations": [
            "External emulation is an observation aid, not proof of hardware timing.",
            "Frame numbers count frontend retro_run calls, not recovered simulation ticks.",
            "RAM/state files belong to the external emulator and are not native snapshots.",
            "Only the supplied input path was observed; no whole-game coverage is implied.",
        ],
        "captures": [],
    }
    if program:
        report["scenario"] = {
            "program": program.program,
            "program_sha256": sha256_file(args.program),
            "executor_sha256": sha256_file(Path(__file__).with_name("scenario_program.py")),
            "cold_boot": True,
            "instruction_execution": "external-core interpreter for instrumented testing",
        }
    trace, trace_digest = None, hashlib.sha256()
    instruction_tracer = None
    if instruction_spec:
        instruction_tracer = InstructionTrace(
            core, instruction_spec, out / "instruction-trace.jsonl", errors
        )
        report["instruction_trace"] = {
            "schema_version": 1,
            "kind": "guarded_external_interpreter_instruction_addresses",
            "specification": "instruction-trace-spec.json",
            "specification_sha256": hashlib.sha256(instruction_bytes).hexdigest(),
            "spec": instruction_spec,
            "trace": "instruction-trace.jsonl",
            "tool_sha256": sha256_file(Path(__file__).with_name("instruction_trace.py")),
            "memory_helpers_sha256": sha256_file(Path(__file__).with_name("memory_sampler.py")),
            "validation_helpers_sha256": sha256_file(
                Path(__file__).with_name("scenario_program.py")
            ),
            "core_extension_inputs": {
                name: sha256_file(ROOT / "nix" / name)
                for name in ("flake.nix", "reference-trace.h", "reference-trace-patch.py")
            },
            "core_extension_api_version": instruction_tracer.api_version,
            "point": "after_original_fetch_and_before_instruction_dispatch",
            "limitations": [
                "Only configured addresses, guarded by original code windows, are recorded.",
                "Registers describe core dispatch state; this is not hardware timing proof.",
                "frontend_run is the zero-based retro_run containing the observation.",
                "Budget counts candidate callbacks including rejected code guards.",
                "No CPU registers, RAM or emulated cycles are modified by the trace extension.",
            ],
        }
    if sampler:
        trace = (out / "memory-trace.jsonl").open("xb")
        report["memory_sampling"] = {
            "schema_version": 1,
            "kind": "read_only_frontend_boundary_samples",
            "boundary": "after_previous_retro_run_before_scenario_tick",
            "specification": "memory-sampling.json",
            "specification_sha256": hashlib.sha256(sampling_bytes).hexdigest(),
            "spec": sampler.spec,
            "sampler_sha256": sha256_file(Path(__file__).with_name("memory_sampler.py")),
            "validation_helpers_sha256": sha256_file(
                Path(__file__).with_name("scenario_program.py")
            ),
            "trace": "memory-trace.jsonl",
            "limitation": (
                "One sample per configured frontend boundary; not an instruction trace. "
                "In-range pointers do not establish the identity or lifetime of an object."
            ),
        }
    (out / "started.json").write_text(json.dumps(report, indent=2) + "\n")
    started = time.monotonic()

    def memory_snapshot():
        pointer = core.retro_get_memory_data(2)
        size = core.retro_get_memory_size(2)
        if not pointer or not 0 < size <= 16 * 1024 * 1024:
            raise RuntimeError("External core did not expose bounded system RAM")
        return pointer, ct.string_at(pointer, size)

    def finish_sampling():
        if trace and not trace.closed:
            trace.close()
            report["memory_sampling"].update(sampler.status())
            report["memory_sampling"]["trace_sha256"] = trace_digest.hexdigest()
        if instruction_tracer:
            report["instruction_trace"].update(instruction_tracer.finish())

    def capture_frame(completed_frames, labels, final=False):
        capture = {"frame": completed_frames, "audio_frames": audio_frames}
        if labels:
            capture["scenario_steps"] = labels
        if latest:
            path = out / f"frame-{completed_frames:06d}.png"
            write_png(path, *latest)
            capture.update(
                {
                    "image": path.name,
                    "sha256": sha256_file(path),
                    "width": latest[1],
                    "height": latest[2],
                }
            )
        _, memory = memory_snapshot()
        capture["ram_sha256"] = hashlib.sha256(memory).hexdigest()
        if final:
            (out / "final.ram").write_bytes(memory)
        if labels:
            (out / f"ram-{completed_frames:06d}.bin").write_bytes(memory)
        report["captures"].append(capture)
        print(
            f"Captured frame {completed_frames}; {time.monotonic() - started:.2f} wall seconds",
            flush=True,
        )

    completed_frames = 0
    try:
        for boundary in range(args.frames + 1):
            labels = []
            sampling_due = sampler is not None and sampler.due(boundary)
            if program or sampling_due:
                pointer, memory = memory_snapshot()
            if sampling_due:
                sample = sampler.sample(boundary, memory)
                sample["program_step_before_tick"] = (
                    program.program["steps"][program.index]["name"]
                    if program and not program.complete
                    else None
                )
                sample["buttons_previous_run"] = (
                    sorted(held_buttons(boundary - 1)) if boundary else []
                )
                data = (json.dumps(sample, separators=(",", ":")) + "\n").encode()
                trace.write(data)
                trace_digest.update(data)
            if program:
                memory_size = len(memory)

                def write_memory(offset, data, ram_pointer=pointer, ram_size=memory_size):
                    if offset + len(data) > ram_size:
                        raise ValueError("Scenario write exceeds actual emulator RAM")
                    ct.memmove(ram_pointer + offset, data, len(data))

                labels = program.tick(boundary, memory, write_memory)
            final = boundary == args.frames or (program is not None and program.complete)
            if boundary and (labels or boundary % args.capture_every == 0 or final):
                capture_frame(boundary, labels, final)
            if final:
                if program and not program.complete:
                    raise TimeoutError("Global scenario frame budget expired before readiness")
                break
            frame = boundary
            if instruction_tracer:
                instruction_tracer.start_run(frame)
            core.retro_run()
            completed_frames = boundary + 1
            if errors:
                raise RuntimeError(errors)
            if shutdown:
                raise RuntimeError("External core requested shutdown")
    except Exception as error:
        finish_sampling()
        if program:
            report["scenario"].update(
                {"complete": False, "events": program.events, "last_conditions": program.observed}
            )
        report.update({"error": str(error), "frames": completed_frames})
        (out / "failure.json").write_text(json.dumps(report, indent=2) + "\n")
        wav.close()
        wav = None
        core.retro_unload_game()
        core.retro_deinit()
        raise
    finish_sampling()
    wav.close()
    wav = None
    if latest is None:
        raise RuntimeError("No software video output was observed")
    size = core.retro_serialize_size()
    if 0 < size <= 128 * 1024 * 1024:
        state_buffer = ct.create_string_buffer(size)
        if core.retro_serialize(state_buffer, size):
            (out / "final.state").write_bytes(state_buffer.raw)
            report["final_state_sha256"] = sha256_file(out / "final.state")
    report.update(
        {
            "frames": completed_frames,
            "wall_seconds": time.monotonic() - started,
            "audio_frames": audio_frames,
            "audio_sha256": audio_digest.hexdigest(),
            "environment_calls": sorted(environment_calls),
            "messages": notices,
        }
    )
    if program:
        report["scenario"].update({"complete": True, "events": program.events})
    core.retro_unload_game()
    core.retro_deinit()
    (out / "observation.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
