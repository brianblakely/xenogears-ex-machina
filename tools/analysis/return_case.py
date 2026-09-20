"""Prepare a connected field-return case from qualified private original evidence.

The reconstruction receives entry state, original resource bytes and explicit
allocation service results. Captured internal calls and their outputs go only
into the separate comparison file. No private Python source is executed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from .animation_trace import OriginalResources
from .field import field_components
from .original_trace import capture_records, ordered_rows, require
from .party_sprites import LOADER_WINDOWS, compare_loader_records, sprite_sources
from .sprite_state import u16, u32
from .verify_collision_math import source_bytes
from .verify_field import ROOT, file_sha, private_output

PROFILE = "na-slus-00664-39c547a9afc6"
EXECUTABLE = "dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119"
OVERLAY = "38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc"
LIFECYCLE = Path(".local/analysis/phase1-field-lifecycle-20260909")
CREATION = Path(".local/analysis/phase1-field-creation-20260909")
CONSTRUCTOR = Path(".local/analysis/phase1-field-creation-20260912")
FACTORY = Path(".local/analysis/phase1-field-factory-20260918")
SPRITE_RETURN = Path(".local/analysis/phase1-sprite-return-20260909")
CONTROL = Path(".local/scenarios/phase1-field23-battle-20260908-v6-control/capture")
GLOBAL_NAMES = (
    "globals-007c",
    "globals-fa54",
    "collision-attributes",
    "globals-2078",
    "globals-f880",
)


def recorded_artifact(evidence: str, path: Path) -> dict:
    """Require the immutable artifact recorded by an existing public finding."""
    finding = json.loads((ROOT / "analysis/findings" / (evidence + ".json")).read_text())
    records = [item for item in finding["validation"]["artifacts"] if item["path"] == str(path)]
    require(len(records) == 1, "Artifact is not recorded by " + evidence + ": " + str(path))
    require(file_sha(path) == records[0]["sha256"], "Historical evidence changed: " + str(path))
    return json.loads(path.read_text())


def validate_report_inputs(report: dict) -> None:
    for name, expected in report["hashes"].items():
        require(file_sha(Path(name)) == expected, "Historical qualification input changed: " + name)


def original_capture(path: Path, spec_path: Path, sources: dict, windows=()) -> tuple[list, dict]:
    spec = json.loads(spec_path.read_text())
    for hook in spec["hooks"]:
        begin = 0x80000000 + hook["guard"]["offset"]
        expected = bytes.fromhex(hook["guard"]["expected"])
        require(
            source_bytes(sources, begin, begin + len(expected)) == expected,
            "Original trace guard differs from supplied source",
        )
    _, rows, observation = capture_records(path, sources, spec, tuple(windows))
    control = json.loads((CONTROL / "observation.json").read_text())
    require(
        observation["inputs"] == control["inputs"]
        and observation["scenario"]["program"] == control["scenario"]["program"],
        "Original capture route differs from independent control",
    )
    names = sorted(
        p.name
        for p in CONTROL.iterdir()
        if p.suffix in {".png", ".bin"} or p.name in {"audio.wav", "final.ram", "final.state"}
    )
    require(len(names) == 124, "Original control artifact set differs")
    for name in names:
        require(
            file_sha(path / name) == file_sha(CONTROL / name), "Original control differs: " + name
        )
    return list(ordered_rows(rows, spec)), {
        "capture": str(path),
        "trace_sha256": file_sha(path / "instruction-trace.jsonl"),
        "observation_sha256": file_sha(path / "observation.json"),
        "control_artifacts": len(names),
    }


def source_exports(path: Path, sources: dict, selected: set[str]) -> None:
    index = json.loads((path / "index.json").read_text())
    found = set()
    for function in index["functions"]:
        entry = function["entry"].split("::")[-1]
        if entry not in selected:
            continue
        found.add(entry)
        require(function["complete"] and not function["error"], "Incomplete original source export")
        body = b"".join(
            source_bytes(
                sources,
                int(item["begin"].split("::")[-1], 16),
                int(item["end"].split("::")[-1], 16) + 1,
            )
            for item in function["body"]
        )
        require(
            hashlib.sha256(body).hexdigest() == function["body_sha256"], "Original body differs"
        )
        require(
            file_sha(path / "c" / function["c"]) == function["c_sha256"],
            "Original C export differs",
        )
        for instruction in function["instructions"]:
            at = int(instruction["address"].split("::")[-1], 16)
            require(
                source_bytes(sources, at, at + 4) == bytes.fromhex(instruction["bytes"]),
                "Original instruction differs",
            )
    require(found == selected, "Missing required original caller/callee export")


def named(rows: list, hook: str) -> list:
    return [(row, payload) for row, payload in rows if row["hook"] == hook]


def single(rows: list, hook: str) -> dict:
    matches = named(rows, hook)
    require(len(matches) == 1, "Expected one original " + hook)
    return matches[0][1]


def chunks(payload: dict, prefix: str, offsets: tuple[int, ...]) -> bytes:
    return b"".join(payload[prefix + str(offset)] for offset in offsets)


def factory_environment(payload: dict) -> list[int]:
    sprite, heap, tasks = (
        payload[name] for name in ("sprite-controls", "heap-controls", "task-controls")
    )
    return [
        u32(sprite, 8),
        sprite[0x1D],
        u32(sprite, 0x28),
        sprite[0x20],
        u32(sprite),
        u32(payload["return-mode"]),
        u16(payload["sprite-gate"]),
        u32(payload["initialization-count"]),
        u16(heap),
        u32(payload["heap-class-eight"]),
        u32(heap, 20),
        u32(tasks),
        u16(tasks, 0x6C),
        u32(tasks, 0x98),
        u32(tasks, 0x164),
        u32(tasks, 0x168),
    ]


def encountered_command(rows: list, actor_index: int, opcode: int) -> tuple[int, dict]:
    """Project the original VM entry after earlier commands have committed."""
    active = None
    for row, payload in rows:
        if row["hook"] == "factory-entry":
            active = row["gpr_u32"][4]
        elif active == actor_index and row["hook"] == "vm-step" and payload["command"][0] == opcode:
            command_pc = row["gpr_u32"][2]
            require(u32(payload["sprite"], 0x64) == command_pc, "Original sprite PC differs")
            return command_pc, {
                "index": actor_index,
                "sprite": {
                    "address": row["gpr_u32"][17],
                    "bytes": payload["sprite"].hex(),
                },
            }
    raise ValueError("Missing original encountered sprite command")


def qualified_resource_spans(resources: OriginalResources) -> list[dict]:
    """Keep every differing live/source byte unsupported, without replacing it."""
    result = []
    for address, data in resources.resources.items():
        live = resources.ram[address - 0x80000000 : address - 0x80000000 + len(data)]
        require(len(data) == len(live), "Original resource extends beyond qualified RAM")
        start = None
        for offset in range(len(data) + 1):
            equal = offset < len(data) and data[offset] == live[offset]
            if equal and start is None:
                start = offset
            elif not equal and start is not None:
                result.append({"address": address + start, "bytes": data[start:offset].hex()})
                start = None
    return result


def verify_return_join(defaults: list, restore: list, factories: list) -> tuple[list, list]:
    """Validate correlation before any captured internal output becomes an oracle."""
    before, after = named(restore, "restore-actor-before"), named(restore, "restore-actor-after")
    factory_before = named(factories, "factory-actor-before")
    initialized = named(defaults, "defaults-after")
    initial_table = chunks(named(defaults, "factory-after")[-1][1], "descriptors-", (0, 1, 2))
    require(
        len(before) == len(after) == len(factory_before) == len(initialized) == 25,
        "Incomplete actor chain",
    )
    targets, expected = [], []
    for index, ((row, old), (_, restored), (_, factory), (_, initialized_actor)) in enumerate(
        zip(before, after, factory_before, initialized, strict=True)
    ):
        require(row["gpr_u32"][17] == index, "Original restore actor order differs")
        descriptor = initial_table[index * 92 : (index + 1) * 92]
        stored_descriptor = chunks(old, "descriptors-", (0, 1024, 2048))[
            index * 92 : (index + 1) * 92
        ]
        # The trace hooks actor-copy entry after the unconditional descriptor
        # writes. Take earlier descriptor input and assert every other byte.
        require(
            descriptor[:0x50] == stored_descriptor[:0x50]
            and descriptor[0x5A:] == stored_descriptor[0x5A:],
            "Unexplained descriptor mutation before return restore",
        )
        require(
            old["actor"] == initialized_actor["actor"],
            "Initialized actor does not reach return restore",
        )
        require(
            restored["actor"] == factory["actor"], "Restored actor does not reach sprite factory"
        )
        require(
            stored_descriptor == factory["descriptor"],
            "Restored descriptor does not reach sprite factory",
        )
        require(
            u32(descriptor, 0x4C) == row["gpr_u32"][7], "Return actor allocation identity differs"
        )
        targets.append({"actor": old["actor"].hex(), "descriptor": descriptor.hex()})
        expected.append({"actor": restored["actor"].hex(), "descriptor": stored_descriptor.hex()})
    return targets, expected


def prepare_case(raw: Path) -> tuple[dict, dict]:
    entry = "field_return"
    source = sprite_sources(raw, PROFILE, 23)
    factory_report = recorded_artifact("EVID-REF-036", FACTORY / "factory-qualification-v2.json")
    overlap_report = recorded_artifact("EVID-REF-036", FACTORY / "constructor-overlap-v2.json")
    for report in (factory_report, overlap_report):
        require(report["result"] == "passed", "Incomplete original factory qualification")
        validate_report_inputs(report)
    restore_report = recorded_artifact("EVID-REF-028", LIFECYCLE / "replay-input-v1.json")
    actor_report = recorded_artifact("EVID-REF-030", CONSTRUCTOR / "comparison-v3.json")
    require(
        restore_report["raw_sha256"] == actor_report["raw_sha256"] == source["raw_sha256"],
        "Original disc identity differs",
    )
    source_exports(CREATION / "ghidra-v2", source, {"800a28d4", "80076ac0", "80080a74", "80080f44"})
    source_exports(LIFECYCLE / "ghidra/lifecycle-v2", source, {"800a3474"})
    captures = []

    def capture(name, spec, windows=()):
        rows, provenance = original_capture(
            Path(".local/scenarios") / name / "capture", spec, source, windows
        )
        captures.append(provenance)
        return rows

    restore = capture(
        "phase1-lifecycle-restore-20260909-v1",
        LIFECYCLE / "restore-trace-spec-v1.json",
        ((0x800A3474, 0x800A3C8C),),
    )
    require(
        captures[-1]["trace_sha256"] == restore_report["captures"][1]["trace_sha256"],
        "Restore trace differs from reviewed evidence",
    )
    defaults = capture(
        "phase1-field-actor-defaults-20260909-v1",
        CREATION / "actor-trace-spec-v1.json",
        ((0x80080A74, 0x80080F44), (0x8003FA38, 0x8003FA68)),
    )
    require(
        captures[-1]["trace_sha256"] == actor_report["trace_sha256"],
        "Defaults trace differs from reviewed evidence",
    )
    factories = capture("phase1-field-factory-20260918-v2", FACTORY / "factory-spec-v2.json")
    constructors = capture(
        "phase1-sprite-construction-20260912-v2", CONSTRUCTOR / "constructor-trace-spec-v2.json"
    )
    loader = capture(
        "phase1-sprite-return-loader-20260909-v1",
        SPRITE_RETURN / "loader-trace-spec-v1.json",
        LOADER_WINDOWS,
    )
    policy = capture("phase1-sprite-return-20260909-v1", SPRITE_RETURN / "trace-spec-v1.json")
    anchor_path = Path(".local/scenarios/phase1-sprite-return-20260909-v1/capture/ram-010762.bin")
    anchor = anchor_path.read_bytes()
    require(
        file_sha(anchor_path) == file_sha(CONTROL / anchor_path.name),
        "Return resource RAM anchor differs",
    )
    loader_spec = json.loads((SPRITE_RETURN / "loader-trace-spec-v1.json").read_text())
    loader_result, party = compare_loader_records(
        [row for row, _ in loader], source, loader_spec, anchor
    )
    resources = OriginalResources(source, anchor, party)
    for row, _ in policy:
        for digest in row.get("digests", []):
            require("unavailable" not in digest, "Missing original resource digest")
            start = digest["pointer_value"] - 0x80000000
            data = anchor[start : start + digest["size"]]
            require(
                len(data) == digest["size"]
                and hashlib.sha256(data).hexdigest() == digest["sha256"],
                "Original live resource digest differs",
            )

    targets, restored = verify_return_join(defaults, restore, factories)
    head, tail = single(restore, "restore-before-head"), single(restore, "restore-before-tail")
    require(head["snapshot-count"] == tail["snapshot-count"], "Original return header changed")
    snapshot = (
        head["snapshot-count"]
        + chunks(head, "snapshot-", tuple(range(0, 0x1C00, 1024)))
        + chunks(tail, "snapshot-tail-", tuple(range(0, 0x1C00, 1024)))
    )
    require(u32(head["event-resources"], 12) == 25, "Original event actor count differs")
    entries, finals = named(factories, "factory-entry"), named(factories, "factory-after")
    actor_finals = named(factories, "factory-actor-after")
    old_entries = named(constructors, "field-create-before")
    first = entries[0][1]
    party_pointers = list(struct.unpack("<3I", old_entries[0][1]["party-resources"]))
    field_base = u32(old_entries[0][1]["field-sprite-component"])
    require(
        field_base == u32(head["resource-pointers"], 20), "Return field resource ownership differs"
    )
    field_component = field_components(source["field_source"])[3].logical_data
    field_header_size = (u32(field_component) + 1) * 4
    field_header = field_component[:field_header_size]
    require(
        anchor[field_base - 0x80000000 : field_base - 0x80000000 + len(field_header)]
        == field_header,
        "Return field resource directory differs",
    )
    qualified = qualified_resource_spans(resources)
    qualified.extend(
        (
            {"address": field_base, "bytes": field_header.hex()},
            {"address": 0x800B1F78, "bytes": first["resource-coordinates"].hex()},
        )
    )

    allocations_by_actor = {}
    current = None
    requested = None
    for row, payload in constructors:
        name, gpr = row["hook"], row["gpr_u32"]
        if name == "field-create-before":
            current = gpr[4]
            allocations_by_actor[current] = []
        elif name == "allocate-sprite-after":
            require(
                current is not None and len(payload["sprite"]) == 356,
                "Unqualified incoming sprite allocation",
            )
            allocations_by_actor[current].append(
                {"address": gpr[2], "bytes": payload["sprite"].hex(), "mode": 0}
            )
        elif name == "allocate-parts-before":
            requested = gpr[4]
        elif name == "allocate-parts-after":
            require(
                current is not None
                and requested is not None
                and requested <= len(payload["parts-window"]),
                "Uncaptured incoming part allocation",
            )
            allocations_by_actor[current].append(
                {"address": gpr[2], "bytes": payload["parts-window"][:requested].hex(), "mode": 0}
            )
    current = None
    releases_by_actor = {}
    for row, payload in factories:
        if row["hook"] == "factory-entry":
            current = row["gpr_u32"][4]
            releases_by_actor[current] = []
        elif row["hook"] == "resize-allocation":
            allocations_by_actor[current].append(
                {
                    "address": row["gpr_u32"][2],
                    "bytes": payload["incoming-allocation"].hex(),
                    "mode": 0,
                }
            )
        elif row["hook"] == "resize-release":
            releases_by_actor[current].append(row["gpr_u32"][4])

    expected_factories = []
    for index, ((row, payload), (_, final), (_, actor_final)) in enumerate(
        zip(entries, finals, actor_finals, strict=True)
    ):
        actor = bytes.fromhex(restored[index]["actor"])
        tag = actor[0x126]
        resource = (
            party_pointers[tag]
            if tag < 0x80
            else field_base + u32(field_header, 4 + (tag & 0x7F) * 4)
        )
        computed = [
            index,
            actor[0x127],
            resource,
            u32(actor, 0x130) >> 28 & 3,
            u32(actor, 0x134) & 15,
            tag,
            u32(actor, 0x134) >> 4 & 1,
        ]
        original = [
            *row["gpr_u32"][4:8],
            u32(payload["arguments"], 16),
            u32(payload["arguments"], 20),
            u32(payload["arguments"], 24),
        ]
        require(computed == original, "Restored actor does not produce original factory arguments")
        require(
            row["gpr_u32"] == old_entries[index][0]["gpr_u32"],
            "Constructor allocation call lineage differs",
        )
        if index < 19:
            if index:
                require(
                    factory_environment(finals[index - 1][1]) == factory_environment(payload),
                    "Factory shared environment has an unexplained gap",
                )
            expected_factories.append(
                {
                    "actor_index": index,
                    "arguments": computed,
                    "actor_bytes": actor_final["actor"].hex(),
                    "descriptor": final["descriptor"].hex(),
                    "sprite": {
                        "address": u32(final["descriptor"], 4),
                        "bytes": final["sprite"].hex(),
                    },
                    "parts": {
                        k: v for k, v in allocations_by_actor[index][-1].items() if k != "mode"
                    },
                    "environment": factory_environment(final),
                    "requested": [
                        len(bytes.fromhex(block["bytes"])) for block in allocations_by_actor[index]
                    ],
                    "released": releases_by_actor[index],
                }
            )
    require(
        factory_environment(finals[18][1]) == factory_environment(entries[19][1]),
        "Shared state differs at the encountered dependency",
    )
    restore_final = single(restore, "restore-after")
    require(
        u16(restore_final["globals-2078"], 0x116) == u16(first["sprite-gate"]),
        "Restored field state does not produce the original sprite gate",
    )
    input_state = {
        "snapshot": snapshot.hex(),
        "event_actor_count": 25,
        "actors": targets,
        "environment": factory_environment(first),
        "field_sprite_base": field_base,
        "party_resources": party_pointers,
        "allocations": [block for index in range(20) for block in allocations_by_actor[index]],
        "resources": qualified,
        "trig": source_bytes(source, 0x800523F0, 0x800563F0).hex(),
        "widths": source_bytes(source, 0x8004FC40, 0x8004FD40).hex(),
    }
    before_defaults, after_defaults = (
        named(defaults, "defaults-before"),
        named(defaults, "defaults-after"),
    )
    require(
        len(before_defaults) == len(after_defaults) == 25, "Incomplete default initialization chain"
    )
    for index in range(24):
        require(
            after_defaults[index][1]["rng"] == before_defaults[index + 1][1]["rng"],
            "Unrepresented RNG consumer between actors",
        )
    for index, ((_, _old), (_, final)) in enumerate(
        zip(before_defaults, after_defaults, strict=True)
    ):
        after_descriptor = chunks(final, "descriptors-", (0, 1, 2))[index * 92 : (index + 1) * 92]
        initialized_descriptor = bytes.fromhex(targets[index]["descriptor"])
        require(
            after_descriptor[:8] == initialized_descriptor[:8]
            and after_descriptor[12:] == initialized_descriptor[12:],
            "Unexplained descriptor effect after defaults",
        )
    source_identity = {
        "profile": PROFILE,
        "executable": EXECUTABLE,
        "overlay": OVERLAY,
        "field": 23,
        "entry_address": "0x800a28d4",
    }
    provenance = {
        "raw_sha256": source["raw_sha256"],
        "captures": captures,
        "loader": loader_result,
        "joined_restored_actors": 25,
        "contiguous_factory_returns": 19,
        "qualified_initialization_rng_calls": 25,
        "limitations": [
            "One original encounter-return route; six captures are correlated observations, "
            "not independent scenarios.",
            "Original allocation results are external inputs; game heap, shadow initialization "
            "and full loader remain unrecovered.",
            "Part bytes are checked against their qualified incoming allocation only on "
            "completed factory paths whose original source leaves them unchanged.",
            "Field resource spans exclude differing source/live bytes; encountering a gap is "
            "unsupported input, not a successful comparison.",
            "Resident audio-event words 80059504/80059540 are outside factory-owned state; "
            "callback cadence is not claimed.",
            "Initialized actors are qualified starting inputs; initialization and its RNG calls "
            "do not execute in this connected return entry.",
        ],
    }
    case = {
        "schema_version": 1,
        "entry": entry,
        "source": source_identity,
        "input": input_state,
        "provenance": provenance,
    }
    checkpoints = [
        {
            "operation": "restore_field_data",
            "actor": None,
            "actors": restored,
            "globals": {name: restore_final[name].hex() for name in GLOBAL_NAMES},
            "variables": chunks(restore_final, "variables-", (0, 1024)).hex(),
            "descriptor_count": u32(restore_final["resource-pointers"], 4),
            "bytes_used": u32(restore_final["cursor"]) - 0x8005A4E4,
        }
    ]
    checkpoints.extend(
        {
            "operation": "create_field_sprite",
            "actor": item["actor_index"],
            **{key: value for key, value in item.items() if key != "actor_index"},
        }
        for item in expected_factories
    )
    expected = {
        "schema_version": 1,
        "entry": entry,
        "source": source_identity,
        "checkpoints": checkpoints,
        "stop": {"actor_index": 19, "operation": "ordinary_sprite_command", "opcode": 0x96},
        "provenance": provenance,
    }
    command_pc, partial = encountered_command(factories, 19, 0x96)
    expected["stop"]["sprite_bytecode_pc"] = command_pc
    expected["partial_actor"] = partial
    expected["case_sha256"] = hashlib.sha256(
        json.dumps(case, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()
    return case, expected


def main() -> None:
    parser = argparse.ArgumentParser(__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    export = sub.add_parser(
        "export", help="Qualify original evidence and write separate input/oracle files"
    )
    export.add_argument("--raw", type=Path, default=Path(".local/references/source-1/disc.bin"))
    export.add_argument("--case", type=Path, required=True)
    export.add_argument("--expected", type=Path, required=True)
    args = parser.parse_args()
    case_path, expected_path = private_output(args.case), private_output(args.expected)
    require(case_path != expected_path, "Execution inputs and expectations require separate files")
    case, expected = prepare_case(args.raw)
    for path, value in ((case_path, case), (expected_path, expected)):
        with path.open("x") as stream:
            json.dump(value, stream, indent=2)
            stream.write("\n")
    print(
        json.dumps(
            {
                "case": str(case_path),
                "expected": str(expected_path),
                "entry": case["entry"],
                "actors": 25,
                "factory_returns": 19,
            }
        )
    )


if __name__ == "__main__":
    main()
