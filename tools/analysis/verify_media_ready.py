"""Compare original music wait, polling policy and selected resource loading.

The qualified route uses music selectors 12 and 6. This does not decode their
sequencing/sample payloads or establish Mono/Stereo/Wide behavior. All reports
and captured original resource bytes remain private.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if not __package__:
    sys.path.insert(0, str(ROOT))
    from tools.analysis import event_media, event_state, events, music_loading
    from tools.analysis.field import event_package, field_components
    from tools.analysis.packed import decode_block
    from tools.analysis.verify_event_state import record_ranges
    from tools.analysis.verify_field import file_sha, load_sources, private_output, verify
    from tools.reference.inspect_disc import RawCd
    from tools.reference.instruction_trace import validate_instruction_trace
    from tools.reference.memory_sampler import ram_pointer_offset
else:
    from ..reference.inspect_disc import RawCd
    from ..reference.instruction_trace import validate_instruction_trace
    from ..reference.memory_sampler import ram_pointer_offset
    from . import event_media, event_state, events, music_loading
    from .field import event_package, field_components
    from .packed import decode_block
    from .verify_event_state import record_ranges
    from .verify_field import file_sha, load_sources, private_output, verify

BASE = 0x8006FAF0
GROUPS = ("events", "poll", "resources")


def media_sources(raw: Path, profile: str, map_id: int) -> dict:
    if map_id != 23:
        raise ValueError("This media route comparison is qualified for field 23 only")
    sources = load_sources(raw, profile, map_id)
    overlay = decode_block(sources["overlay_packed"]).data
    catalog = json.loads((ROOT / "analysis/coverage/source-fingerprints.json").read_text())
    table = next(p for p in catalog["profiles"] if p["source_profile"] == profile)
    rows = {r[0]: dict(zip(table["records_columns"], r, strict=True)) for r in table["records"]}
    selections, assets = {}, {}
    with raw.open("rb") as stream:
        cd = RawCd(stream, raw.stat().st_size)
        directory = cd.read_sector(40)[:128]
        directory_base = int.from_bytes(directory[56:58], "little") - 1
        for selector in (6, 12):
            offset = 0x800ADFCC - BASE + 2 * selector
            bank, shared = overlay[offset : offset + 2]
            selections[selector] = music_loading.MusicSelection(selector, bank, shared)
            numbers = [0x14 + 2 * selector] + ([] if bank == 255 else [0x13 + 2 * bank])
            for number in numbers:
                slot = directory_base + number - 1
                row = rows[slot]
                data = cd.read_extent(row["source_lba"], row["projected_bytes"])
                if hashlib.sha256(data).hexdigest() != row["projection_sha256"]:
                    raise ValueError("Original media source fingerprint mismatch")
                assets[slot] = {"record": row, "data": data, "file_number": number}
    return {
        **sources,
        "overlay": overlay,
        "selections": selections,
        "audio_directory_base": directory_base,
        "assets": assets,
    }


def specification(sources: dict, group: str) -> dict:
    common = [
        {"name": "field", "offset": 0x4F34C, "size": 4},
        {"name": "actor-index", "offset": 0xAFD1C, "size": 4},
        {"name": "budget-mode", "offset": 0xAFFEC, "size": 4},
        {"name": "break-requested", "offset": 0xB00C0, "size": 4},
        {"name": "media", "offset": 0x4F2E0, "size": 176},
        {"name": "deferred-read", "offset": 0xAFC54, "size": 4},
    ]
    hooks = []

    def hook(name, pc, begin, finish, ranges, resident=False, digests=None):
        code = (
            sources["exe"][0x800 + begin - 0x80010000 : 0x800 + finish - 0x80010000]
            if resident
            else sources["overlay"][begin - BASE : finish - BASE]
        )
        row = {
            "name": name,
            "pc": pc,
            "guard": {"offset": begin - 0x80000000, "expected": code.hex()},
            "ranges": ranges,
        }
        if digests:
            row["digests"] = digests
        hooks.append(row)

    if group == "events":
        ranges = [
            {"name": "actor", "pointer_offset": 0xB0078, "relative_offset": 0, "size": 256}
        ] + common
        for name, pc in [
            ("extended-before", 0x800869B8),
            ("extended-dispatch", 0x800869E8),
            ("extended-after", 0x80086A0C),
        ]:
            hook(name, pc, 0x800869B8, 0x80086A1C, ranges)
        for name, pc in [("wait-before", 0x8008825C), ("wait-after", 0x800882B0)]:
            hook(name, pc, 0x8008825C, 0x800882B8, ranges)
    elif group == "poll":
        for name, pc, begin, end in [
            ("poll-before", 0x80085C90, 0x80085C90, 0x80085D08),
            ("wave-result", 0x80085CBC, 0x80085CB4, 0x80085D08),
            ("shared-result", 0x80085D58, 0x80085D48, 0x80085D64),
            ("disc-result", 0x80085DDC, 0x80085DD4, 0x80085E14),
            ("poll-after", 0x80085ED4, 0x80085E9C, 0x80085EEC),
            ("store-result", 0x80078B98, 0x80078B6C, 0x80078BC8),
        ]:
            hook(name, pc, begin, end, common)
    elif group == "resources":
        ranges = [{"name": "directory-base", "offset": 0x4FE14, "size": 4}] + common
        hook("source-read", 0x800295D8, 0x800295D8, 0x80029650, ranges, True)
        # File identity/length determines which captured bytes are the SMDS.
        # Extra bytes in this 6144-byte resident buffer are not file payload.
        inputs = [
            {"name": f"input-{i}", "register": 4, "relative_offset": 1024 * i, "size": 1024}
            for i in range(6)
        ]
        hook("music-before", 0x80039850, 0x80039850, 0x80039910, ranges + inputs, True)
        hook(
            "music-after",
            0x800398F8,
            0x80039850,
            0x80039910,
            ranges + [{"name": "sequence", "register": 2, "relative_offset": 0, "size": 256}],
            True,
        )
        for name, pc, register in [
            ("start-before", 0x80039A80, 4),
            ("start-after", 0x80039B4C, 16),
        ]:
            hook(
                name,
                pc,
                0x80039A80,
                0x80039B68,
                ranges
                + [{"name": "sequence", "register": register, "relative_offset": 0, "size": 256}],
                True,
            )
        hook(
            "wave-stream-create",
            0x800380D0,
            0x800380D0,
            0x80038150,
            ranges,
            True,
            [{"name": "wave-prefix", "register": 4, "size": 8192, "max_bytes": 8192}],
        )
    else:
        raise ValueError("Unknown original media trace group")
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": f"field23-music-{group}",
            "source_profile": sources["profile"]["id"],
            "start_frame": 450,
            "end_frame": 5000,
            "max_callbacks": 512 if group == "resources" else 20000,
            "hooks": hooks,
        }
    )


def qualified_ranges(record: dict, hook: dict) -> dict:
    normalized = []
    for actual, expected in zip(record["ranges"], hook["ranges"], strict=True):
        if "register" in expected:
            value = record["gpr_u32"][expected["register"]]
            if (
                actual["register"] != expected["register"]
                or actual["register_value"] != value
                or actual["relative_offset"] != expected["relative_offset"]
            ):
                raise ValueError("Original media register reference differs from specification")
            offset = ram_pointer_offset(value)
            if offset is None:
                raise ValueError("Original media pointer outside system RAM")
            expected = {
                "name": expected["name"],
                "size": expected["size"],
                "offset": offset + expected["relative_offset"],
            }
        normalized.append(expected)
    return record_ranges(record, {**hook, "ranges": normalized})


def compare_events(records, sources: dict) -> dict:
    package = event_package(field_components(sources["field_source"])[5].logical_data)
    pending, waiting, counts, cases = None, None, Counter(), Counter()
    for record, ranges in records:
        stage = record["hook"]
        actor = event_state.read_actor_scripts(ranges["actor"])
        index = int.from_bytes(ranges["actor-index"], "little")
        package.entry(index, 0)
        if stage == "extended-before":
            if pending is not None or package.bytecode[actor.pc] != 0xFE:
                raise ValueError("Nested extended dispatch or different original prefix")
            expected = dict(ranges)
            expected["actor"] = event_state.correlate_actor_scripts(
                ranges["actor"], event_media.enter_extended(actor)
            )
            pending = {"original": ranges, "dispatch": expected, "stage": "dispatch"}
        elif stage == "extended-dispatch":
            if pending is None or pending["stage"] != "dispatch" or ranges != pending["dispatch"]:
                raise ValueError("Original extended-prefix PC increment or opaque state differs")
            pending["stage"] = "handler"
            pending["opcode"] = package.bytecode[actor.pc]
            counts["dispatch_increments"] += 1
        elif stage == "wait-before":
            if pending is None or pending["opcode"] != 0xA2 or waiting is not None:
                raise ValueError("Unexpected original extended music wait")
            if ranges != pending["dispatch"]:
                raise ValueError("Original extended dispatch changes state before a2")
            original_actor = event_state.read_actor_scripts(pending["original"]["actor"])
            instruction = events.decode_instruction(package.bytecode, original_actor.pc)
            control = event_state.InterpreterControl(
                int.from_bytes(ranges["budget-mode"], "little"),
                int.from_bytes(ranges["break-requested"], "little"),
            )
            status = int.from_bytes(ranges["media"][0x28:0x2C], "little")
            effect = event_media.execute_music_wait(instruction, original_actor, control, status)
            waiting = dict(ranges)
            waiting["actor"] = event_state.correlate_actor_scripts(ranges["actor"], effect.actor)
            waiting["break-requested"] = effect.control.break_requested.to_bytes(4, "little")
            cases[(status, control.budget_mode)] += 1
        elif stage == "wait-after":
            if waiting is None or ranges != waiting:
                raise ValueError(
                    "Original music wait differs from reconstructed full captured state"
                )
            pending["wait_result"] = waiting
            waiting = None
            counts["music_waits"] += 1
        elif stage == "extended-after":
            if pending is None or pending["stage"] != "handler" or waiting is not None:
                raise ValueError("Original extended handler has no matching return")
            if pending["opcode"] == 0xA2 and ranges != pending.get("wait_result"):
                raise ValueError("Original a2 return differs from its wrapper return")
            pending = None
        else:
            raise ValueError("Unexpected original music-event hook")
    if pending is not None or waiting is not None or not counts["music_waits"]:
        raise ValueError("Incomplete or empty original music-wait trace")
    return {
        "counts": dict(counts),
        "wait_cases": [
            {"load_result_u32": status, "budget_mode": mode, "count": count}
            for (status, mode), count in sorted(cases.items())
        ],
        "scope": "Exact prefix increment for observed extended dispatches; full captured "
        "state for a2 only. Other extended handler effects remain opaque and unvalidated.",
    }


def compare_poll(records, sources: dict) -> dict:
    pending, commit, cases = None, None, []
    counts = Counter()
    for record, ranges in records:
        stage = record["hook"]
        if stage == "poll-before":
            if pending is not None or commit is not None:
                raise ValueError("Nested or uncommitted original music poll")
            selector = record["gpr_u32"][4]
            if selector not in sources["selections"]:
                raise ValueError("Original poll uses an unqualified music selector")
            pending = {
                "ranges": ranges,
                "selection": sources["selections"][selector],
                "inputs": {},
                "frame": record["frontend_run"],
            }
        elif stage in ("wave-result", "shared-result", "disc-result"):
            if pending is None:
                raise ValueError("Original music service result outside a poll")
            key = {
                "wave-result": "wave_result",
                "shared-result": "shared_result",
                "disc-result": "disc_busy",
            }[stage]
            if key in pending["inputs"]:
                raise ValueError("Repeated original music service result")
            pending["inputs"][key] = record["gpr_u32"][2]
        elif stage == "poll-after":
            if pending is None:
                raise ValueError("Original poll return has no entry")
            before = pending["ranges"]
            state = music_loading.read_music_state(before["media"], before["deferred-read"])
            effect = music_loading.poll_music(
                state, pending["selection"], music_loading.MusicPollInputs(**pending["inputs"])
            )
            expected = dict(before)
            expected["media"], expected["deferred-read"] = music_loading.correlate_music_state(
                before["media"], effect.state
            )
            if ranges != expected or record["gpr_u32"][2] != effect.result:
                differences = {
                    name: [
                        i
                        for i, (a, b) in enumerate(zip(expected[name], ranges[name], strict=True))
                        if a != b
                    ]
                    for name in expected
                    if expected[name] != ranges[name]
                }
                raise ValueError(f"Original music poll differs: {differences}")
            commit = (dict(ranges), effect.result)
            cases.append(
                {
                    "selector": pending["selection"].sequence,
                    "frame": pending["frame"],
                    "result": effect.result,
                    "inputs": pending["inputs"],
                    "operations": effect.operations,
                }
            )
            pending = None
            counts["polls"] += 1
        elif stage == "store-result":
            if commit is not None:
                expected, value = commit
                media = bytearray(expected["media"])
                media[0x28:0x2C] = value.to_bytes(4, "little")
                expected["media"] = bytes(media)
                if ranges != expected:
                    raise ValueError("Original caller does not commit the exact poll result")
                counts["commits"] += 1
                commit = None
        else:
            raise ValueError("Unexpected original music-poll hook")
    if pending is not None or commit is not None or not counts["polls"]:
        raise ValueError("Incomplete or empty original music-poll capture")
    return {
        "counts": dict(counts),
        "poll_cases": cases,
        "scope": "Exact original poll decisions, return, captured state and caller commit. "
        "Service results are original inputs; audio sequencing and wave transfer are not decoded.",
    }


def compare_resources(records, sources: dict) -> dict:
    """Match complete selected SMDS files and a bounded initial WDS block."""
    assets = sources["assets"]
    reads, sequences, waves = [], [], []
    pending = None
    for record, ranges in records:
        stage, registers = record["hook"], record["gpr_u32"]
        frame = record["frontend_run"]
        if stage == "source-read":
            directory = int.from_bytes(ranges["directory-base"], "little")
            slot = directory + registers[4] - 1
            if slot not in assets:
                if slot != sources["field_record"]["slot"]:
                    raise ValueError("Original source read outside qualified route assets")
                continue
            if directory != sources["audio_directory_base"]:
                raise ValueError("Original music read uses a different directory base")
            reads.append(
                {
                    "slot": slot,
                    "file_number": registers[4],
                    "destination": registers[5],
                    "mode": registers[7],
                    "frame": frame,
                }
            )
        elif stage == "music-before":
            if pending is not None:
                raise ValueError("Nested or unstarted original music sequence")
            selector = int.from_bytes(ranges["media"][0x44:0x48], "little")
            if selector not in sources["selections"]:
                raise ValueError("Original music consumer uses an unqualified selector")
            slot = sources["audio_directory_base"] + 0x14 + 2 * selector - 1
            asset = assets[slot]
            captured = b"".join(ranges[f"input-{i}"] for i in range(6))
            if len(asset["data"]) > len(captured) or not asset["data"].startswith(b"smds"):
                raise ValueError("Qualified music sequence does not fit the captured buffer")
            if captured[: len(asset["data"])] != asset["data"]:
                raise ValueError("Original music consumer input differs from complete source file")
            if not reads or reads[-1]["slot"] != slot or reads[-1]["destination"] != registers[4]:
                raise ValueError("Original music consumer input has no matching source read")
            pending = {
                "slot": slot,
                "selector": selector,
                "source_pointer": registers[4],
                "bytes": len(asset["data"]),
                "sha256": hashlib.sha256(asset["data"]).hexdigest(),
                "create_frame": frame,
                "stage": "create-return",
            }
        elif stage == "music-after":
            if pending is None or pending["stage"] != "create-return":
                raise ValueError("Original music consumer has no matching entry")
            if int.from_bytes(ranges["sequence"][8:12], "little") != pending["source_pointer"]:
                raise ValueError("Original sequence object does not retain its source pointer")
            pending.update(sequence_pointer=registers[2], stage="start")
        elif stage == "start-before":
            if pending is None or pending["stage"] != "start":
                raise ValueError("Original sequence start has no matching creation")
            if (
                registers[4] != pending["sequence_pointer"]
                or int.from_bytes(ranges["sequence"][8:12], "little") != pending["source_pointer"]
            ):
                raise ValueError("Original sequence start uses a different object or source")
            pending.update(start_arguments=registers[5:7], start_frame=frame, stage="start-return")
        elif stage == "start-after":
            if pending is None or pending["stage"] != "start-return":
                raise ValueError("Original sequence start has no matching return")
            if (
                registers[16] != pending["sequence_pointer"]
                or int.from_bytes(ranges["sequence"][8:12], "little") != pending["source_pointer"]
                or not int.from_bytes(ranges["sequence"][16:18], "little") & 0x8000
            ):
                raise ValueError("Original start object, backlink or active flag differs")
            sequences.append({key: value for key, value in pending.items() if key != "stage"})
            pending = None
        elif stage == "wave-stream-create":
            candidates = [
                (slot, asset) for slot, asset in assets.items() if asset["data"].startswith(b"wds ")
            ]
            if len(candidates) != 1 or registers[5] != 8192:
                raise ValueError("Original wave stream does not match the qualified initial block")
            slot, asset = candidates[0]
            expected = {
                "name": "wave-prefix",
                "pointer_value": registers[4],
                "size": 8192,
                "resolved_offset": ram_pointer_offset(registers[4]),
                "sha256": hashlib.sha256(asset["data"][:8192]).hexdigest(),
            }
            if expected["resolved_offset"] is None or record.get("digests") != [expected]:
                raise ValueError(
                    "Original initial wave block digest or pointer differs from source"
                )
            if not reads or reads[-1]["slot"] != slot:
                raise ValueError("Original initial wave block has no matching source read")
            waves.append(
                {
                    "slot": slot,
                    "source_bytes": len(asset["data"]),
                    "matched_bytes": 8192,
                    "sha256": expected["sha256"],
                    "frame": frame,
                }
            )
        else:
            raise ValueError("Unexpected original music-resource hook")
    if pending is not None or len(sequences) != 2 or len(waves) != 1:
        raise ValueError("Incomplete or different original music-resource route")
    if {row["selector"] for row in sequences} != {6, 12}:
        raise ValueError("Original route does not cover both qualified music selectors")
    return {
        "counts": {
            "music_reads": len(reads),
            "sequences": len(sequences),
            "sequence_bytes": sum(row["bytes"] for row in sequences),
            "initial_wave_blocks": len(waves),
        },
        "reads": reads,
        "sequences": sequences,
        "wave_blocks": waves,
        "scope": "Two complete source SMDS files match original consumer input, with created "
        "object backlinks and start flags. Only the first 8192 source WDS bytes match the "
        "stream-creation input; remaining transfers and playback decoding are unvalidated.",
    }


def compare(sources: dict, capture: Path, group: str) -> dict:
    observation_path = capture / "observation.json"
    observation = json.loads(observation_path.read_text())
    spec = specification(sources, group)
    metadata = observation["instruction_trace"]
    if (
        observation["source_profile"] != sources["profile"]["id"]
        or observation["content_sha256"]
        != sources["profile"]["measurement"]["source"]["chd"]["sha256"]
        or not observation["scenario"]["complete"]
        or metadata["spec"] != spec
        or metadata["failed"]
        or metadata["budget_reached"]
        or metadata["unavailable_ranges"]
        or any(metadata["guard_mismatches_by_hook"].values())
    ):
        raise ValueError("Incomplete, unqualified or failed original music capture")
    spec_path = capture / "instruction-trace-spec.json"
    trace = capture / "instruction-trace.jsonl"
    if (
        file_sha(trace) != metadata["trace_sha256"]
        or file_sha(spec_path) != metadata["specification_sha256"]
        or json.loads(spec_path.read_text()) != spec
    ):
        raise ValueError("Original music trace or specification hash mismatch")
    ram = (capture / "final.ram").read_bytes()
    if len(ram) != 0x200000:
        raise ValueError("Original music comparison needs exactly 2 MiB RAM")
    for begin, finish in [
        (0x800869B8, 0x80086A1C),
        (0x8008825C, 0x800882B8),
        (0x80085C90, 0x80085EEC),
        (0x80078B5C, 0x80078BC8),
    ]:
        if (
            sources["overlay"][begin - BASE : finish - BASE]
            != ram[begin - 0x80000000 : finish - 0x80000000]
        ):
            raise ValueError("Original music instructions differ from overlay source")
    for table, opcode, target in [(0x800AE2A0, 0xFE, 0x800869B8), (0x800AE6A0, 0xA2, 0x8008825C)]:
        address = table + 4 * opcode
        source = sources["overlay"][address - BASE : address - BASE + 4]
        if (
            int.from_bytes(source, "little") != target
            or source != ram[address - 0x80000000 : address - 0x80000000 + 4]
        ):
            raise ValueError("Original extended dispatch table differs from qualified source")
    for hook in spec["hooks"]:
        guard = hook["guard"]
        code = bytes.fromhex(guard["expected"])
        if ram[guard["offset"] : guard["offset"] + len(code)] != code:
            raise ValueError("Original guarded instructions differ from final RAM")
    hooks = {hook["name"]: hook for hook in spec["hooks"]}
    total, last_frame = 0, -1

    def records(stream):
        nonlocal total, last_frame
        for line in stream:
            record = json.loads(line)
            if record["event"] != total or total >= spec["max_callbacks"]:
                raise ValueError("Noncontiguous or over-budget original music trace")
            frame = record["frontend_run"]
            if not spec["start_frame"] <= frame < spec["end_frame"] or frame < last_frame:
                raise ValueError("Original music trace has invalid frontend ordering")
            ranges = qualified_ranges(record, hooks[record["hook"]])
            if int.from_bytes(ranges["field"], "little") != 23:
                raise ValueError("Original music trace enters a different field")
            total, last_frame = total + 1, frame
            yield record, ranges

    with trace.open() as stream:
        result = {"events": compare_events, "poll": compare_poll, "resources": compare_resources}[
            group
        ](records(stream), sources)
    if total != metadata["records"] or total != metadata["candidate_callbacks"]:
        raise ValueError("Original music trace count differs from observation metadata")
    return {
        "schema_version": 1,
        "kind": "original_field_music_loading_comparison",
        "source_profile": sources["profile"]["id"],
        "map": 23,
        "group": group,
        "records": total,
        **result,
        "raw_track_sha256": sources["raw_sha256"],
        "trace": {"path": str(trace), "sha256": file_sha(trace)},
        "observation": {"path": str(observation_path), "sha256": file_sha(observation_path)},
        "tool_sources": {
            name: file_sha(ROOT / name)
            for name in (
                "tools/analysis/events.py",
                "tools/analysis/event_media.py",
                "tools/analysis/music_loading.py",
                "tools/analysis/verify_media_ready.py",
                "tools/analysis/verify_event_state.py",
                "tools/analysis/verify_field.py",
            )
        },
        "result": "passed",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["prepare", "compare"])
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--group", choices=GROUPS, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = media_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            result = specification(sources, args.group)
        else:
            if args.capture is None:
                raise ValueError("compare requires --capture")
            structure = verify(args.raw, args.capture / "final.ram", args.profile, args.map)
            result = compare(sources, args.capture, args.group)
            result["field_structure_validation"] = structure
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output.relative_to(ROOT)}")
    except (ValueError, KeyError, IndexError, StopIteration, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
