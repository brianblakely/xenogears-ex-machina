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
GEOMETRY = Path(".local/analysis/phase1-factory-f5-20260921-v3")
SPRITE_RETURN = Path(".local/analysis/phase1-sprite-return-20260909")
CONTROL = Path(".local/scenarios/phase1-field23-battle-20260908-v6-control/capture")
# The immutable captures predate removal of the observer API version export.
# Read this exact commit's source blobs to qualify their recorded fingerprints;
# neither the old collector nor its emulator interface is restored or executed.
COLLECTOR_REVISION = "4eeb44d0665a3bc4862b7a37268a6c86b8305dec"
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


def original_capture(
    path: Path,
    spec_path: Path,
    sources: dict,
    windows=(),
    *,
    collector_revision: str | None = COLLECTOR_REVISION,
) -> tuple[list, dict]:
    spec = json.loads(spec_path.read_text())
    for hook in spec["hooks"]:
        begin = 0x80000000 + hook["guard"]["offset"]
        expected = bytes.fromhex(hook["guard"]["expected"])
        require(
            source_bytes(sources, begin, begin + len(expected)) == expected,
            "Original trace guard differs from supplied source",
        )
    _, rows, observation = capture_records(
        path, sources, spec, tuple(windows), collector_revision=collector_revision
    )
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
        "collector_revision": collector_revision,
        "control_artifacts": len(names),
    }


def source_exports(path: Path, sources: dict, selected: set[str], *, survey: bool = False) -> None:
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
        c_path = path / function["c_file"] if survey else path / "c" / function["c"]
        require(
            file_sha(c_path) == function["c_sha256"],
            "Original C export differs",
        )
        for instruction in () if survey else function["instructions"]:
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


def verify_required_join(factories: list, required: list) -> None:
    """Bind additional service observations to the same uninterrupted factories."""
    for name in ("factory-entry", "vm-step"):
        first, second = named(factories, name), named(required, name)
        require(len(first) == len(second), "Required capture join count differs")
        for (old_row, old), (row, payload) in zip(first, second, strict=True):
            require(
                all(row[key] == old_row[key] for key in ("pc", "frontend_run", "gpr_u32")),
                "Required capture call lineage differs",
            )
            for key in ("field", "return-mode", "heap-controls", "arguments", "sprite", "command"):
                if key in old and key in payload:
                    require(
                        old[key] == payload[key], "Required capture shared range differs: " + key
                    )
            require(
                old["sprite-controls"] == payload["sprite-controls"][16:]
                and old["task-controls"][0x164:0x16C] == payload["task-heads"][:8]
                and old["task-controls"][0x98:0x9C] == payload["task-current"]
                and (
                    "task-next" not in payload or payload["task-heads"][4:8] == payload["task-next"]
                )
                and (
                    "task-active-count" not in payload
                    or old["task-controls"][0x3C:0x40] == payload["task-active-count"]
                ),
                "Required capture shared controls differ",
            )


def required_upload_allocations(rows: list, actor_index: int) -> list[dict]:
    """Keep captured incoming scratch allocations as external heap results."""
    allocations = []
    actor = None
    active = False
    for row, payload in rows:
        name, gpr = row["hook"], row["gpr_u32"]
        if name == "factory-entry":
            actor = gpr[4]
        elif actor == actor_index:
            if name == "fc-entry":
                require(not active, "Nested original FC allocation")
                active = True
            elif name in ("fc-allocation", "helper-allocation"):
                require(active, "Orphan original FC allocation")
                require(
                    len(allocations) < 2
                    and name == ("fc-allocation", "helper-allocation")[len(allocations)],
                    "Original FC allocation order differs",
                )
                data = chunks(payload, "incoming-", tuple(range(8)))
                require(len(data) == 8192, "Incomplete original scratch allocation")
                require(gpr[31] == row["pc"], "Original scratch allocator return differs")
                allocations.append(
                    {
                        "address": gpr[2],
                        "bytes": data.hex(),
                        "mode": int(name == "helper-allocation"),
                    }
                )
            elif name == "fc-after":
                require(active and len(allocations) == 2, "Incomplete original FC allocations")
                active = False
    require(not active and len(allocations) == 2, "Expected one complete original FC")
    return allocations


def required_upload_requests(
    rows: list, resources: OriginalResources, actor_index: int
) -> list[dict]:
    """Project LoadImage arguments directly from original call observations."""
    requests = []
    actor = None
    active = False
    image = None
    for row, payload in rows:
        name, gpr = row["hook"], row["gpr_u32"]
        if name == "factory-entry":
            actor = gpr[4]
        elif actor == actor_index:
            if name == "upload-entry":
                require(not active, "Nested original image upload")
                active = True
            elif name == "image-entry":
                require(active and image is None, "Orphan original LoadImage")
                rectangle = list(struct.unpack("<4h", payload["rectangle"]))
                size = rectangle[2] * rectangle[3] * 2
                pixels = payload["pixel-prefix"] + payload["pixel-continuation"]
                require(
                    rectangle[2] > 0 and rectangle[3] > 0 and size <= len(pixels),
                    "Incomplete original image pixels",
                )
                require(
                    resources.read(gpr[5], size) == pixels[:size],
                    "Original image pixels differ from qualified source",
                )
                require(
                    gpr[31] == 0x8002DF7C and gpr[4] == gpr[29] + 16,
                    "Original LoadImage caller/rectangle lineage differs",
                )
                image = (gpr[29], gpr[5] + size, payload["rectangle"])
                requests.append(
                    {"rectangle": rectangle, "source_address": gpr[5], "bytes": pixels[:size].hex()}
                )
            elif name == "image-after":
                require(
                    image is not None and (gpr[29], gpr[16], payload["frame"][16:24]) == image,
                    "Original image return/cursor differs",
                )
                image = None
            elif name == "upload-after":
                require(
                    active and image is None and gpr[2] == 0, "Incomplete original image upload"
                )
                active = False
    require(not active and image is None and requests, "Missing original image requests")
    return requests


def child_controls(payload: dict) -> dict:
    controls = payload["sprite-controls"]
    return {
        "task_serial": u32(controls, 4),
        "task_primary_count": u32(controls, 8),
        "task_auxiliary_count": u32(controls, 12),
        "task_active_flags": u32(payload["task-active-count"]),
        "child_creation_flags": controls[0x2C],
        "child_allocation_mode": controls[0x2F],
        "texture_page": u16(payload["texture-page"]),
        "texture_mode": u32(payload["texture-mode"]),
    }


def model_controls(payload: dict) -> dict:
    controls, inputs = payload["model-controls"], payload["packet-inputs"]
    return {
        "material_page": u16(controls),
        "material_palette": u16(controls, 4),
        "palette_base": u32(controls, 12),
        "palette_mode": u32(payload["model-offset-mode"]),
        "primitive_count": u32(payload["packet-count"]),
        "output": u32(payload["packet-cursor"]),
        "shading": u32(inputs, 16),
        "geometry": u32(inputs),
        "normals": u32(inputs, 4),
        "vertices": u32(inputs, 20),
        "auxiliary": u32(payload["packet-auxiliary"]),
    }


def geometry_records(rows: list) -> dict[int, dict]:
    """Qualify only F5's six allocator calls and directly observed packet output."""
    actor = None
    vm = active = None
    results = {}
    for row, payload in rows:
        name, gpr = row["hook"], row["gpr_u32"]
        if name == "factory-entry":
            require(active is None, "Unclosed original F5 operation")
            actor = gpr[4]
        elif name == "vm-step":
            vm = row, payload
        elif name == "f5-entry":
            require(
                active is None
                and vm is not None
                and vm[1]["command"][0] == 0xF5
                and gpr[19] == vm[0]["gpr_u32"][17]
                and gpr[17] == vm[0]["gpr_u32"][2] + 1,
                "Original F5 VM ownership differs",
            )
            active = {
                "actor": actor,
                "sprite": gpr[19],
                "operand": gpr[17],
                "command": payload["operand"],
                "entry": row,
            }
        elif active is not None:
            if name == "resource-before":
                operand = active["command"]
                delta = operand[0] + operand[1] * 256 + struct.unpack("<b", operand[2:3])[0] * 65536
                require(
                    gpr[4] == active["operand"] + delta, "Original F5 resource argument differs"
                )
                active["resource_address"] = gpr[4]
                active["resource_before"] = chunks(payload, "resource-", (0, 1, 2, 3))
            elif name == "resource-after":
                require(
                    gpr[16] == active["resource_address"], "Original F5 resource return differs"
                )
                active["resource_after"] = chunks(payload, "resource-", (0, 1, 2, 3))
            elif name == "geometry-allocation-entry":
                require(
                    gpr[31] == 0x800211A8
                    and gpr[4] == active["resource_address"]
                    and payload["header"] == active["resource_after"][:64],
                    "Original F5 allocation caller differs",
                )
                active["allocation_frame"] = gpr[29] - 32
                active["half_size"] = u32(payload["header"], 0x34)
            elif name == "heap-entry":
                require(
                    gpr[31] == 0x8002CB8C
                    and gpr[29] == active["allocation_frame"]
                    and gpr[4] == 2 * active["half_size"]
                    and gpr[5] == 0,
                    "Original F5 heap size/mode/caller differs",
                )
                active["request"] = gpr[4]
            elif name == "geometry-allocation-after":
                data = chunks(payload, "incoming-", (0, 1))
                require(
                    0 < active["request"] <= len(data)
                    and gpr[31] == 0x8002CB8C
                    and gpr[29] == active["allocation_frame"],
                    "Original F5 allocator return differs",
                )
                active["allocation"] = {
                    "address": gpr[2],
                    "bytes": data[: active["request"]].hex(),
                    "mode": 0,
                }
            elif name == "pack-entry":
                require(
                    gpr[31] == 0x800211BC
                    and gpr[4] == active["resource_address"]
                    and gpr[5] == active["allocation"]["address"]
                    and gpr[6] == 0,
                    "Original F5 packing arguments differ",
                )
            elif name == "copy-before":
                size = active["half_size"]
                require(
                    gpr[4] == active["allocation"]["address"] + size
                    and gpr[5] == active["allocation"]["address"]
                    and gpr[6] == size
                    and size <= len(payload["packed"]),
                    "Original F5 copy arguments differ",
                )
                active["packed"] = payload["packed"][:size]
            elif name == "copy-after":
                size = active["half_size"]
                require(
                    gpr[2] == active["allocation"]["address"] + size
                    and payload["copied"][:size] == active["packed"],
                    "Original F5 second buffer differs",
                )
                require(actor not in results, "Repeated original F5 for one actor")
                results[actor] = {
                    "allocation": active["allocation"],
                    "resource_address": active["resource_address"],
                    "resource_before": active["resource_before"],
                    "resource_after": active["resource_after"],
                    "model_state": model_controls(payload),
                    "buffer": {
                        "address": active["allocation"]["address"],
                        "bytes": (active["packed"] + payload["copied"][:size]).hex(),
                    },
                }
                active = None
    require(
        active is None and set(results) == set(range(19, 25)), "Incomplete original F5 coverage"
    )
    return results


def qualified_field_component(base: int, data: bytes, anchor: bytes, geometries: dict) -> dict:
    """Qualify raw field bytes before this return's observed F5 relocations."""
    headers, previous = {}, {}
    for actor in range(19, 25):
        record = geometries[actor]
        address = record["resource_address"]
        begin = address - base
        require(0 <= begin <= len(data) - 64, "Original model header outside field component")
        before, after = record["resource_before"], record["resource_after"]
        expected = previous.get(address, data[begin : begin + 64])
        require(before[:64] == expected, "Original model first use/relocation lineage differs")
        updated = bytearray(before[:64])
        require(u32(updated, 0x1C) == 0, "Original model relocation table requires qualification")
        if (u16(updated) & 0x20) == 0:
            struct.pack_into("<H", updated, 0, u16(updated) | 0x20)
            for offset in (8, 12, 16, 20):
                struct.pack_into(
                    "<I", updated, offset, (u32(updated, offset) + address) & 0xFFFFFFFF
                )
        require(
            bytes(updated) == after[:64] and before[64:] == after[64:],
            "Original F5 relocation has unexplained effects",
        )
        headers.update({address + i: value for i, value in enumerate(data[begin : begin + 64])})
        previous[address] = after[:64]
    live = anchor[base - 0x80000000 : base - 0x80000000 + len(data)]
    require(
        len(live) == len(data)
        and all(
            original == observed or headers.get(base + i) == original
            for i, (original, observed) in enumerate(zip(data, live, strict=True))
        ),
        "Field component has unqualified source/live differences",
    )
    return {"address": base, "bytes": data.hex()}


def required_child(rows: list, actor_index: int) -> tuple[dict, dict]:
    """Separate one child's original incoming allocation and committed output."""
    actor = None
    request = allocation = final = None
    for row, payload in rows:
        name, gpr = row["hook"], row["gpr_u32"]
        if name == "factory-entry":
            actor = gpr[4]
        elif actor == actor_index:
            if name == "child-allocation-before":
                require(request is None, "Repeated original child allocation")
                request = gpr[4], gpr[5], gpr[29]
            elif name == "child-allocation-after":
                require(
                    request is not None
                    and request[0] <= len(payload["incoming"])
                    and request[2] == gpr[29]
                    and gpr[31] == row["pc"],
                    "Unqualified original child allocation",
                )
                allocation = {
                    "address": gpr[2],
                    "bytes": payload["incoming"][: request[0]].hex(),
                    "mode": request[1],
                }
            elif name == "child-after":
                require(
                    allocation is not None
                    and gpr[20] == allocation["address"]
                    and len(payload["node"]) == request[0],
                    "Original child allocation/output lineage differs",
                )
                final = {
                    **child_controls(payload),
                    "task_pending_head": u32(payload["task-heads"], 8),
                    "task_nodes": [{"address": gpr[20], "bytes": payload["node"].hex()}],
                }
    require(
        allocation is not None and final is not None, "Missing original child allocation/output"
    )
    return allocation, final


def first_child_command(rows: list, actor_index: int, opcode: int = 0x8D) -> tuple[int, dict]:
    """Qualify the first callback's timer prefix before its first VM command.

    Original 80022df4/80023210 and the 800248d4 entry do not store to the
    task headers. The timer decrements child+9e from one to zero; the direct
    VM capture must differ from task entry only in that independently reviewed
    store. Do not include the extra 92 captured bytes outside the allocation.
    """
    actor = None
    before = None
    prefix_qualified = False
    for row, payload in rows:
        name, gpr = row["hook"], row["gpr_u32"]
        if name == "factory-entry":
            actor = gpr[4]
        elif actor == actor_index:
            if name == "task-before":
                require(before is None, "Repeated task before first original child command")
                before = row, payload
            elif name == "vm-step" and before is not None:
                original_row, original = before
                node = original["node"]
                require(
                    len(node) == 320
                    and u32(node, 4) == original_row["gpr_u32"][4] + 56 == gpr[17]
                    and u32(payload["sprite"], 0x64) == gpr[2],
                    "Original first child VM ownership differs",
                )
                if not prefix_qualified:
                    child = bytearray(node[56:])
                    require(u16(child, 0x9E) == 1, "Original child timer prefix differs")
                    struct.pack_into("<H", child, 0x9E, 0)
                    require(
                        payload["command"][0] == 0x8D and bytes(child) == payload["sprite"][:264],
                        "Unexplained original child change before first command",
                    )
                    prefix_qualified = True
                if payload["command"][0] != opcode:
                    require(
                        payload["command"][0] in (0x8D, 0xF5), "Unqualified child prefix command"
                    )
                    continue
                return gpr[2], {
                    **child_controls(payload),
                    "task_pending_head": u32(payload["task-heads"], 8),
                    "task_nodes": [
                        {
                            "address": original_row["gpr_u32"][4],
                            "bytes": (node[:56] + payload["sprite"][:264]).hex(),
                        }
                    ],
                }
    raise ValueError("Missing original first child command")


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


def prepare_case(raw: Path, stop_opcode: int = 0xFC) -> tuple[dict, dict]:
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
    source_exports(
        Path(".local/ghidra/subsystem-survey-disc1-v4"),
        source,
        {
            "8001ce74",
            "8001fbe4",
            "8001fb30",
            "8002dde4",
            "80044894",
            "80022df4",
            "80023210",
            "800248d4",
            "8002cb54",
            "8002c59c",
            "8002c8cc",
            "8003f968",
            "80031bdc",
        },
        survey=True,
    )
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
    required = capture(
        "phase1-field-factory-required-20260918-v2", FACTORY / "required-spec-v2.json"
    )
    verify_required_join(factories, required)
    children = capture("phase1-field-factory-child-20260918-v1", FACTORY / "child-spec-v1.json")
    verify_required_join(factories, children)
    geometry_revision = json.loads((GEOMETRY / "invocation.json").read_text())["collector_revision"]
    geometry_rows, geometry_provenance = original_capture(
        Path(".local/scenarios/phase1-factory-f5-20260921-v3/capture"),
        GEOMETRY / "trace-spec.json",
        source,
        collector_revision=geometry_revision,
    )
    captures.append(geometry_provenance)
    verify_required_join(factories, geometry_rows)
    geometries = geometry_records(geometry_rows)
    initial_model = named(geometry_rows, "factory-entry")[0][1]
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
    child_allocation, child_final = required_child(children, 19)
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
    initial_tasks = named(required, "factory-entry")[0][1]["task-heads"]
    require(initial_tasks == bytes(12), "Initial task-list nodes require original qualification")
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
    field_resource = qualified_field_component(field_base, field_component, anchor, geometries)
    qualified = [
        span
        for span in qualified
        if not field_base <= span["address"] < field_base + len(field_component)
    ]
    task_callbacks = source_bytes(source, 0x8004FD40, 0x8004FD4C)
    require(
        anchor[0x4FD40:0x4FD4C] == task_callbacks,
        "Original sprite task callback table differs from resident source",
    )
    qualified.extend(
        (
            field_resource,
            {"address": 0x800B1F78, "bytes": first["resource-coordinates"].hex()},
            {"address": 0x8004FD40, "bytes": task_callbacks.hex()},
        )
    )
    for kind in (5, 13):
        address = 0x8004FE68 + kind * 40
        table_row = source_bytes(source, address, address + 40)
        require(
            anchor[address - 0x80000000 : address - 0x80000000 + 40] == table_row,
            "Original model dispatch table differs from resident source",
        )
        qualified.append({"address": address, "bytes": table_row.hex()})

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
        "allocations": [block for index in range(20) for block in allocations_by_actor[index]]
        + required_upload_allocations(required, 19)
        + [child_allocation, geometries[19]["allocation"]],
        "task_pending_head": u32(initial_tasks, 8),
        "task_nodes": [],
        **child_controls(named(children, "factory-entry")[0][1]),
        "texture_page": u32(initial_model["model-controls"], 8),
        "model_state": model_controls(initial_model),
        "heap_class_five_context": u32(initial_model["heap-class-five"]),
        "heap_tag": u16(initial_model["model-controls"], 16),
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
            "One original encounter-return route; nine captures are correlated observations, "
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
        "stop": {"actor_index": 19, "operation": "ordinary_sprite_command", "opcode": stop_opcode},
        "provenance": provenance,
    }
    command_pc, partial = encountered_command(factories, 19, stop_opcode)
    expected["stop"]["sprite_bytecode_pc"] = command_pc
    expected["partial_actor"] = partial
    expected["uploads"] = []
    if stop_opcode in (0xE0, 0x81, 0x8D, 0xF5, 0xA3):
        expected["uploads"] = required_upload_requests(required, resources, 19)
    if stop_opcode == 0x81:
        expected["partial_state"] = child_final
    if stop_opcode in (0x8D, 0xF5, 0xA3):
        command_pc, expected["partial_state"] = first_child_command(children, 19, stop_opcode)
        require(command_pc == expected["stop"]["sprite_bytecode_pc"], "Child command joins differ")
        del expected["partial_actor"]
    if stop_opcode == 0xA3:
        expected_resources = [dict(span) for span in qualified]
        for span in expected_resources:
            if span["address"] == field_base:
                updated = bytearray(field_component)
                begin = geometries[19]["resource_address"] - field_base
                updated[begin : begin + 64] = geometries[19]["resource_after"][:64]
                span["bytes"] = updated.hex()
        expected["partial_state"].update(
            model_state=geometries[19]["model_state"],
            model_buffers=[geometries[19]["buffer"]],
            resources=expected_resources,
        )
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
    export.add_argument("--stop-opcode", type=lambda value: int(value, 0), default=0xFC)
    args = parser.parse_args()
    case_path, expected_path = private_output(args.case), private_output(args.expected)
    require(case_path != expected_path, "Execution inputs and expectations require separate files")
    case, expected = prepare_case(args.raw, args.stop_opcode)
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
