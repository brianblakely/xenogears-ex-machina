"""Compare source-reconstructed collision arithmetic with original execution.

The original uses both system RAM and scratchpad stacks. This comparator binds
each output to its captured input pointers and exact code/data revision. It is
analysis infrastructure, not a native collision controller or hardware test.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if not __package__:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.arithmetic import signed16, signed32
    from tools.analysis.collision_math import height_and_normal, locate, normalize
    from tools.analysis.field import collision_package, field_components
    from tools.analysis.packed import decode_block
    from tools.analysis.verify_field import file_sha, load_sources, private_output, verify
    from tools.reference.instruction_trace import (
        scratchpad_pointer_offset,
        validate_instruction_trace,
    )
    from tools.reference.memory_sampler import ram_pointer_offset
else:
    from ..reference.instruction_trace import scratchpad_pointer_offset, validate_instruction_trace
    from ..reference.memory_sampler import ram_pointer_offset
    from .arithmetic import signed16, signed32
    from .collision_math import height_and_normal, locate, normalize
    from .field import collision_package, field_components
    from .packed import decode_block
    from .verify_field import file_sha, load_sources, private_output, verify

BASE = 0x8006FAF0
WINDOWS = (
    (0x80048D7C, 0x80048E94),
    (0x8004A480, 0x8004A4D8),
    (0x8007B07C, 0x8007B478),
    (0x80084FAC, 0x80084FCC),
)


def source_bytes(sources: dict, begin: int, end: int) -> bytes:
    if begin >= BASE:
        return sources["overlay"][begin - BASE : end - BASE]
    return sources["exe"][0x800 + begin - 0x80010000 : 0x800 + end - 0x80010000]


def math_sources(raw: Path, profile: str, map_id: int) -> dict:
    sources = load_sources(raw, profile, map_id)
    sources["overlay"] = decode_block(sources["overlay_packed"]).data
    sources["map"] = map_id
    sources["table_bytes"] = source_bytes(sources, 0x80056B14, 0x80056D14)
    sources["table"] = struct.unpack("<192h", sources["table_bytes"][128:])
    component = field_components(sources["field_source"])[1].logical_data
    sources["component"] = component
    sources["layers"] = collision_package(component).layers
    return sources


def specification(sources: dict, start: int, end: int) -> dict:
    hooks = []

    def register(name, index, offset, size):
        return {"name": name, "register": index, "relative_offset": offset, "size": size}

    def direct(name, offset, size):
        return {"name": name, "offset": offset, "size": size}

    def hook(name, pc, ranges):
        begin, finish = pc - 16, pc + 16
        hooks.append(
            {
                "name": name,
                "pc": pc,
                "guard": {
                    "offset": begin - 0x80000000,
                    "expected": source_bytes(sources, begin, finish).hex(),
                },
                "ranges": ranges + [direct("field", 0x4F34C, 4)],
            }
        )

    for name, pc in (("normalize-before", 0x80048DD8), ("normalize-after", 0x80048E8C)):
        hook(name, pc, [direct("table", 0x56B14, 512)])
    hook(
        "height-before",
        0x8007B07C,
        [register(n, i, 0, 8) for n, i in (("a", 4), ("b", 5), ("c", 6), ("point", 7))]
        + [register("normal-pointer", 29, 16, 4)],
    )
    hook(
        "height-after",
        0x8007B1A0,
        [
            register("point", 19, 0, 8),
            register("normal", 20, 0, 12),
            register("saved-return", 29, 0x54, 4),
        ],
    )
    hook(
        "locate-before",
        0x8007B1C4,
        [
            register("point", 7, 0, 6),
            register("normal-pointer", 29, 16, 4),
            direct("mesh-globals", 0xAFB24, 52),
        ],
    )
    hook(
        "locate-after",
        0x8007B460,
        [register("point", 17, 0, 6), register("saved-return", 29, 0x98, 4)],
    )
    hook("actor-normal-after", 0x80084FC4, [register("normal", 17, 0x50, 12)])
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": f"field{sources['map']}-collision-math-{start}-{end}",
            "source_profile": sources["profile"]["id"],
            "start_frame": start,
            "end_frame": end,
            "max_callbacks": 95000,
            "hooks": hooks,
        }
    )


def address_region(pointer: int) -> tuple[str, int]:
    offset = ram_pointer_offset(pointer)
    if offset is not None:
        return "ram", offset
    offset = scratchpad_pointer_offset(pointer)
    if offset is not None:
        return "scratchpad", offset
    raise ValueError("Original collision pointer outside captured memory spaces")


def qualified_ranges(record: dict, hook: dict) -> dict[str, bytes]:
    if record["pc"] != hook["pc"] or len(record["gpr_u32"]) != 34:
        raise ValueError("Original collision PC or register count differs from specification")
    if any(type(v) is not int or not 0 <= v <= 0xFFFFFFFF for v in record["gpr_u32"]):
        raise ValueError("Invalid original u32 register value")
    guard = hook["guard"]
    code = bytes.fromhex(guard["expected"])
    index = record["pc"] - 0x80000000 - guard["offset"]
    if record["code"] != int.from_bytes(code[index : index + 4], "little"):
        raise ValueError("Original collision instruction differs from source guard")
    if len(record["ranges"]) != len(hook["ranges"]):
        raise ValueError("Incomplete original collision ranges")
    result = {}
    for actual, expected in zip(record["ranges"], hook["ranges"], strict=True):
        if "unavailable" in actual or any(actual.get(k) != expected[k] for k in ("name", "size")):
            raise ValueError("Unavailable or substituted original collision range")
        if "register" in expected:
            pointer = record["gpr_u32"][expected["register"]]
            space, offset = address_region(pointer)
            if (
                actual.get("register") != expected["register"]
                or actual.get("register_value") != pointer
                or actual.get("relative_offset") != expected["relative_offset"]
            ):
                raise ValueError("Original collision register reference differs")
            offset += expected["relative_offset"]
        else:
            space, offset = "ram", expected["offset"]
            if "register" in actual or "pointer_offset" in actual:
                raise ValueError("Original direct collision range was substituted")
        limit = 1024 if space == "scratchpad" else 0x200000
        if (
            not 0 <= offset <= limit - expected["size"]
            or actual.get("resolved_space", "ram") != space
            or actual.get("resolved_offset") != offset
        ):
            raise ValueError("Original collision range resolved to a different region")
        data = bytes.fromhex(actual["hex"])
        if len(data) != expected["size"]:
            raise ValueError("Original collision payload length differs")
        result[expected["name"]] = data
    return result


def compare_records(records, sources: dict, mesh_globals: bytes) -> dict:
    counts, cases, callers, spaces = Counter(), Counter(), Counter(), Counter()
    pending = {}
    last_normal = None
    triangles = {
        tuple(layer.vertices[v][:3] for v in triangle.vertices)
        for layer in sources["layers"]
        for triangle in layer.triangles
    }
    location_cases = set()
    for record, ranges in records:
        name, registers = record["hook"], record["gpr_u32"]
        counts[name] += 1
        spaces.update(r.get("resolved_space", "ram") for r in record["ranges"])
        if name == "normalize-before":
            if "normalize" in pending or ranges["table"] != sources["table_bytes"]:
                raise ValueError("Nested normalization or changed original table")
            values = tuple(registers[8:11])
            magnitude = sum(signed16(v) ** 2 for v in values)
            leading = (32 - magnitude.bit_length()) & ~1
            shift = (31 - leading) >> 1
            table_shift = leading - 24
            scaled = magnitude << table_shift if table_shift >= 0 else magnitude >> -table_shift
            index = scaled - 64
            multiplier = struct.unpack_from("<h", sources["table_bytes"], 128 + 2 * index)[0]
            pending["normalize"] = (
                normalize(values, sources["table"]),
                registers[31],
                multiplier,
                shift,
                index,
            )
            cases["zero_vector" if magnitude == 0 else "nonzero_vector"] += 1
            callers[f"0x{registers[7]:08x}"] += 1
        elif name == "normalize-after":
            if "normalize" not in pending:
                raise ValueError("Unpaired original normalization return")
            expected, return_to, multiplier, shift, index = pending.pop("normalize")
            if (
                ranges["table"] != sources["table_bytes"]
                or tuple(signed32(v) for v in registers[8:11]) != expected
                or registers[31] != return_to
                or signed32(registers[13]) != multiplier
                or signed32(registers[14]) != shift
                or signed32(registers[12]) != index * 2
            ):
                raise ValueError("Original normalization output or lookup differs")
            last_normal = (expected, registers[5], registers[7])
        elif name == "height-before":
            if "height" in pending:
                raise ValueError("Nested original height calculation")
            vertices = tuple(struct.unpack("<4h", ranges[n])[:3] for n in ("a", "b", "c"))
            if vertices not in triangles:
                raise ValueError("Original height vertices differ from the source collision mesh")
            point = struct.unpack("<4h", ranges["point"])
            height, normal = height_and_normal(vertices, point[0], point[2], sources["table"])
            pending["height"] = {
                "point": ranges["point"][:2] + struct.pack("<h", height) + ranges["point"][4:],
                "normal": normal,
                "point_pointer": registers[7],
                "normal_pointer": int.from_bytes(ranges["normal-pointer"], "little"),
                "return_to": registers[31],
            }
            if "locate" in pending:
                location = pending["locate"]
                if location["normal_pointer"] != pending["height"]["normal_pointer"]:
                    raise ValueError("Original location height writes a different normal")
                location["height_calls"] += 1
                if location["result"] is None or (height, normal) != (
                    location["result"][1][1],
                    location["result"][2],
                ):
                    raise ValueError("Original location invokes an inconsistent height calculation")
            cases["vertical_triangle" if normal[1] == 0 else "height_plane"] += 1
        elif name == "height-after":
            if "height" not in pending:
                raise ValueError("Unpaired original height return")
            before = pending.pop("height")
            if (
                ranges["point"] != before["point"]
                or struct.unpack("<3i", ranges["normal"]) != before["normal"]
                or registers[19] != before["point_pointer"]
                or registers[20] != before["normal_pointer"]
                or int.from_bytes(ranges["saved-return"], "little") != before["return_to"]
            ):
                raise ValueError("Original height point, normal or output identity differs")
        elif name == "locate-before":
            if "locate" in pending or ranges["mesh-globals"] != mesh_globals:
                raise ValueError("Nested location query or changed original mesh globals")
            x, z = (signed32(v) for v in registers[4:6])
            layer = registers[6]
            if not 0 <= layer < len(sources["layers"]):
                raise ValueError("Original location layer outside source package")
            result = locate(sources["layers"][layer], x, z, sources["table"])
            pending["locate"] = {
                "result": result,
                "return_to": registers[31],
                "point_pointer": registers[7],
                "normal_pointer": int.from_bytes(ranges["normal-pointer"], "little"),
                "height_calls": 0,
            }
            location_cases.add((layer, x, z, None if result is None else result[0]))
            cases[f"location_layer_{layer}"] += 1
            cases["location_miss" if result is None else "location_found"] += 1
        elif name == "locate-after":
            if "locate" not in pending or "height" in pending:
                raise ValueError("Unpaired or premature original location return")
            before = pending.pop("locate")
            result = before["result"]
            index, point = (0, (0, 0, 0)) if result is None else result[:2]
            if (
                registers[2] != index
                or registers[17] != before["point_pointer"]
                or struct.unpack("<3h", ranges["point"]) != point
                or int.from_bytes(ranges["saved-return"], "little") != before["return_to"]
                or before["height_calls"] != int(result is not None)
            ):
                raise ValueError("Original location result or output identity differs")
        elif name == "actor-normal-after":
            if last_normal is None:
                raise ValueError("Original actor normal has no captured normalization")
            expected, pointer, return_to = last_normal
            if (
                return_to != 0x80084FC4
                or pointer != registers[17] + 0x50
                or struct.unpack("<3i", ranges["normal"]) != expected
            ):
                raise ValueError("Original actor normal differs from the second normalization")
            last_normal = None
        else:
            raise ValueError("Unrecognized original collision hook")
    if pending or not counts:
        raise ValueError("Incomplete or empty original collision trace")
    return {
        "counts": dict(counts),
        "cases": dict(cases),
        "normalization_callers": dict(callers),
        "captured_memory_spaces": dict(spaces),
        "location_cases": [list(case) for case in sorted(location_cases)],
        "location_miss_normal_output": "not independently sampled",
    }


def compare(sources: dict, capture: Path, start: int, end: int) -> dict:
    observation_path = capture / "observation.json"
    observation = json.loads(observation_path.read_text())
    metadata = observation["instruction_trace"]
    spec = specification(sources, start, end)
    if (
        observation["source_profile"] != sources["profile"]["id"]
        or observation["content_sha256"]
        != sources["profile"]["measurement"]["source"]["chd"]["sha256"]
        or not observation["scenario"]["complete"]
        or metadata["core_extension_api_version"] != 2
        or metadata["spec"] != spec
        or metadata["failed"]
        or metadata["budget_reached"]
        or metadata["unavailable_ranges"]
        or any(metadata["guard_mismatches_by_hook"].values())
    ):
        raise ValueError("Incomplete, unqualified or failed original collision capture")
    trace, spec_path = capture / "instruction-trace.jsonl", capture / "instruction-trace-spec.json"
    if (
        file_sha(trace) != metadata["trace_sha256"]
        or file_sha(spec_path) != metadata["specification_sha256"]
        or json.loads(spec_path.read_text()) != spec
    ):
        raise ValueError("Original collision trace or specification fingerprint differs")
    ram_path = capture / "final.ram"
    ram = ram_path.read_bytes()
    final_capture = observation["captures"][-1]
    if (
        len(ram) != 0x200000
        or final_capture["frame"] != observation["frames"]
        or file_sha(ram_path) != final_capture["ram_sha256"]
    ):
        raise ValueError("Original collision final RAM is incomplete or altered")
    for begin, finish in WINDOWS:
        if ram[begin - 0x80000000 : finish - 0x80000000] != source_bytes(sources, begin, finish):
            raise ValueError("Original collision instructions differ from source")
    component_address = int.from_bytes(ram[0xAFB18:0xAFB1C], "little")
    space, offset = address_region(component_address)
    if space != "ram" or ram[offset : offset + len(sources["component"])] != sources["component"]:
        raise ValueError("Original collision component differs from source")
    mesh_globals = ram[0xAFB24:0xAFB58]
    for index, layer in enumerate(sources["layers"]):
        triangle, vertex = struct.unpack_from("<II", sources["component"], 0x18 + index * 8)
        actual = tuple(
            struct.unpack_from("<I", mesh_globals, index * 4 + n)[0] for n in (0, 16, 32)
        )
        if actual != (
            component_address + triangle,
            component_address + vertex,
            len(layer.triangles),
        ):
            raise ValueError("Original collision pointers/counts differ from source layout")
    hooks = {hook["name"]: hook for hook in spec["hooks"]}
    total, last_frame = 0, -1

    def records(stream):
        nonlocal total, last_frame
        for line in stream:
            record = json.loads(line)
            frame = record["frontend_run"]
            if record["event"] != total or total >= spec["max_callbacks"]:
                raise ValueError("Original collision trace is noncontiguous or over budget")
            if not start <= frame < end or frame < last_frame:
                raise ValueError("Original collision trace has invalid frontend ordering")
            ranges = qualified_ranges(record, hooks[record["hook"]])
            if int.from_bytes(ranges["field"], "little") != sources["map"]:
                raise ValueError("Original collision trace enters another field")
            total, last_frame = total + 1, frame
            yield record, ranges

    with trace.open() as stream:
        result = compare_records(records(stream), sources, mesh_globals)
    if total != metadata["records"] or total != metadata["candidate_callbacks"]:
        raise ValueError("Original collision record count differs from observation")
    return {
        "schema_version": 1,
        "kind": "original_collision_arithmetic_comparison",
        "source_profile": sources["profile"]["id"],
        "map": sources["map"],
        "frontend_window": [start, end],
        "records": total,
        **result,
        "raw_track_sha256": sources["raw_sha256"],
        "trace": {"path": str(trace), "sha256": file_sha(trace)},
        "observation": {"path": str(observation_path), "sha256": file_sha(observation_path)},
        "table_window_sha256": hashlib.sha256(sources["table_bytes"]).hexdigest(),
        "tool_sources": {
            name: file_sha(ROOT / name)
            for name in (
                "tools/analysis/collision_math.py",
                "tools/analysis/verify_collision_math.py",
                "tools/reference/instruction_trace.py",
            )
        },
        "result": "passed",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "compare"))
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--start", type=int, required=True)
    parser.add_argument("--end", type=int, required=True)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = math_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            result = specification(sources, args.start, args.end)
        else:
            if args.capture is None:
                raise ValueError("Comparison requires an original capture")
            verify(args.raw, args.capture / "final.ram", args.profile, args.map)
            result = compare(sources, args.capture, args.start, args.end)
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output}")
    except (ValueError, KeyError, OSError, struct.error) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
