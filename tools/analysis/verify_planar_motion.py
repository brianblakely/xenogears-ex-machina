"""Qualify original sources and compare bounded planar-motion trace stages.

Complete field collision and opaque sprite handlers remain outside this model.
Shared captured bytes, original pointer storage, source commands, call order and
committed stores are checked independently of the arithmetic reconstruction.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if not __package__:
    sys.path.insert(0, str(ROOT))
    from tools.analysis.field import field_components
    from tools.analysis.original_trace import capture_records, exact_payload, ordered_rows, require
    from tools.analysis.party_sprites import (
        LOADER_WINDOWS,
        compare_loader,
        digest,
        sprite_sources,
        u32,
    )
    from tools.analysis.planar_motion import (
        animation_speed,
        command_pc_store,
        field_party_velocity,
        motion_mode,
        sprite_bundle_offsets,
        sprite_velocity,
        trig_pair,
    )
    from tools.analysis.verify_collision_math import source_bytes
    from tools.analysis.verify_field import file_sha, private_output, verify
    from tools.reference.instruction_trace import validate_instruction_trace
else:
    from ..reference.instruction_trace import validate_instruction_trace
    from .field import field_components
    from .original_trace import capture_records, exact_payload, ordered_rows, require
    from .party_sprites import LOADER_WINDOWS, compare_loader, digest, sprite_sources, u32
    from .planar_motion import (
        animation_speed,
        command_pc_store,
        field_party_velocity,
        motion_mode,
        sprite_bundle_offsets,
        sprite_velocity,
        trig_pair,
    )
    from .verify_collision_math import source_bytes
    from .verify_field import file_sha, private_output, verify


PLANAR_WINDOWS = (
    (0x800183D8, 0x800185A4),
    (0x8001FBE4, 0x8001FC34),
    (0x80021958, 0x800219AC),
    (0x80021FE0, 0x80022000),
    (0x80022974, 0x80022A00),
    (0x8002490C, 0x80024F20),
    (0x8003F8B0, 0x8003F8E8),
    (0x8004FC40, 0x8004FD40),
    (0x800523F0, 0x800563F0),
    (0x800712AC, 0x800712E4),
    (0x80081F80, 0x800821F4),
    (0x80082BB8, 0x80082C8C),
    (0x8008314C, 0x80083178),
    (0x800A09B4, 0x800A0A04),
)
COMMON = (
    "field",
    "descriptor-table",
    "player-descriptor",
    "player-actor",
    "player-sprite",
    "movement-controls",
    "held-inputs",
    "sprite-globals",
    "motion-actor-index",
)


def s16(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def planar_specification(sources: dict, ram: bytes) -> dict:
    descriptor = u32(ram, 0xAFB10) - 0x80000000 + 3 * 92
    require(0 <= descriptor <= 0x200000 - 92, "original player descriptor outside RAM")

    def direct(name, offset, size):
        return {"name": name, "offset": offset, "size": size}

    def reg(name, index, size, offset=0):
        return {"name": name, "register": index, "relative_offset": offset, "size": size}

    common = [
        direct("field", 0x4F34C, 4),
        direct("descriptor-table", 0xAFB10, 4),
        direct("player-descriptor", descriptor, 92),
        {
            "name": "player-actor",
            "pointer_offset": descriptor + 0x4C,
            "relative_offset": 0,
            "size": 312,
        },
        {
            "name": "player-sprite",
            "pointer_offset": descriptor + 4,
            "relative_offset": 0,
            "size": 180,
        },
        direct("movement-controls", 0xADB64, 12),
        direct("held-inputs", 0xAFE98, 8),
        direct("sprite-globals", 0x59190, 32),
    ]
    actor_index = direct("motion-actor-index", 0x65B08, 4)
    hooks = []

    def hook(name, pc, ranges):
        hooks.append(
            {
                "name": name,
                "pc": pc,
                "guard": {
                    "offset": pc - 16 - 0x80000000,
                    "expected": source_bytes(sources, pc - 16, pc + 16).hex(),
                },
                "ranges": ranges,
            }
        )

    definitions = [
        ("field-velocity-before", 0x80081F80, [reg("sprite", 4, 180), reg("descriptor", 6, 92)]),
        (
            "field-actor",
            0x8008200C,
            [reg("actor", 4, 312), reg("sprite", 18, 180), reg("descriptor", 19, 92)],
        ),
        ("field-velocity-after", 0x800821D4, [reg("sprite", 18, 180), reg("descriptor", 19, 92)]),
        ("sprite-velocity-before", 0x80022974, [reg("sprite", 4, 180)]),
        ("sprite-velocity-after", 0x800229E8, [reg("sprite", 17, 180)]),
        ("speed-before", 0x80021958, [reg("sprite", 19, 180), reg("operand", 17, 8, -1)]),
        ("speed-after", 0x800219A4, [reg("sprite", 19, 180), reg("operand", 17, 8, -1)]),
        ("sine-result", 0x8003F8C8, [reg("lookup-pair", 1, 4, 0x23F0)]),
        ("cosine-result", 0x8003F8E4, [reg("lookup-pair", 1, 4, 0x23F0)]),
    ]
    for name, pc in [
        ("motion-ready", 0x80082BF8),
        ("motion-mode", 0x80082C8C),
        ("motion-after", 0x8008314C),
    ]:
        definitions.append(
            (name, pc, [reg("actor", 16, 312), reg("sprite", 19, 180), reg("descriptor", 22, 92)])
        )
    for name, pc, ranges in definitions:
        hook(name, pc, common + ranges + [actor_index])
    for name, pc in [("sprite-pc-advance", 0x80024EFC), ("sprite-pc-loop", 0x8002490C)]:
        hook(
            name,
            pc,
            common + [actor_index, reg("sprite", 17, 180), direct("command-widths", 0x4FC40, 256)],
        )
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": "field23-planar-motion-v3",
            "source_profile": sources["profile"]["id"],
            "start_frame": 4300,
            "end_frame": 5241,
            "max_callbacks": 45000,
            "hooks": hooks,
        }
    )


def selected(payload: dict, names: tuple[str, ...]) -> dict:
    """Explicit shared capture set; never silently intersect differing range sets."""
    require(all(name in payload for name in names), "missing shared original state range")
    return {name: payload[name] for name in names}


def changed(payload: dict, pointer: int, stores: list[tuple[int, int, int]]) -> dict:
    result = {k: bytearray(v) for k, v in payload.items()}
    for offset, value, size in stores:
        data = (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")
        result["sprite"][offset : offset + size] = data
        if pointer == u32(payload["player-descriptor"], 4):
            result["player-sprite"][offset : offset + size] = data
    return {k: bytes(v) for k, v in result.items()}


def call(row: dict, before: dict, delta: int, return_address: int | None = None) -> None:
    require(
        row["frontend_run"] == before["frontend_run"]
        and row["gpr_u32"][29] == before["gpr_u32"][29] + delta
        and (return_address is None or row["gpr_u32"][31] == return_address),
        "original planar call stack, return or run differs",
    )


def source_command(resources: dict, payload: dict, pointer: int, size: int, ram: bytes) -> bytes:
    base = u32(payload["sprite"], 0x44)
    require(
        base in resources and u32(payload["sprite"], 0x48) == base,
        "original sprite does not select a qualified party or field resource",
    )
    offset = pointer - base
    require(0 <= offset <= len(resources[base]) - size, "original sprite command outside source")
    data = resources[base][offset : offset + size]
    require(
        ram[pointer - 0x80000000 : pointer - 0x80000000 + size] == data,
        "original sprite command differs between source and final RAM",
    )
    return data


def compare_planar_records(rows, sources: dict, spec: dict, ram: bytes, resources: dict) -> dict:
    table = source_bytes(sources, 0x800523F0, 0x800563F0)
    widths = source_bytes(sources, 0x8004FC40, 0x8004FD40)
    require(
        ram[0x523F0:0x563F0] == table and ram[0x4FC40:0x4FD40] == widths,
        "original trigonometric or command-width table differs",
    )
    for pointer, data in resources.items():
        require(
            ram[pointer - 0x80000000 : pointer - 0x80000000 + len(data)] == data,
            "party source differs between loader and planar captures",
        )
    # Field component3 contains additional sprite resources. Its inner data has
    # runtime changes; qualify the exact index and each used command separately.
    # Report all differences instead of claiming whole-component equality.
    component = field_components(sources["field_source"])[3]
    offsets = sprite_bundle_offsets(component.logical_data)
    field_base = u32(ram, 0xAFB1C)
    field_offset = field_base - 0x80000000
    require(
        0 <= field_offset <= len(ram) - component.logical_size,
        "original field sprite component outside RAM",
    )
    field_ram = ram[field_offset : field_offset + component.logical_size]
    header_size = 4 * (len(offsets) + 1)
    require(
        field_ram[:header_size] == component.logical_data[:header_size],
        "original field sprite index differs from source",
    )
    command_resources = dict(resources)
    for begin, end in zip(offsets, offsets[1:] + (component.logical_size,), strict=True):
        require(field_base + begin not in command_resources, "overlapping original sprite sources")
        command_resources[field_base + begin] = component.logical_data[begin:end]
    field_component = {
        "source": sources["field_record"],
        "component": 3,
        "source_offset": component.source_offset,
        "logical_bytes": component.logical_size,
        "logical_sha256": digest(component.logical_data),
        "pointer_global": "0x800afb1c",
        "base": f"0x{field_base:08x}",
        "source_index_offsets": list(offsets),
        "index_bytes_equal": header_size,
        "runtime_sha256": digest(field_ram),
        "runtime_differing_byte_offsets_unreconstructed": [
            i
            for i, (a, b) in enumerate(zip(field_ram, component.logical_data, strict=True))
            if a != b
        ],
    }
    player_descriptor = next(
        r["offset"] for r in spec["hooks"][0]["ranges"] if r["name"] == "player-descriptor"
    )
    counts, outcomes, modes, opcodes, inputs = Counter(), Counter(), Counter(), Counter(), Counter()
    pending, loops, speeds = {}, {}, []
    for row, payload in ordered_rows(rows, spec):
        name, gpr = row["hook"], row["gpr_u32"]
        counts[name] += 1
        require(
            u32(payload["field"]) == sources["map"]
            and u32(payload["descriptor-table"]) + 3 * 92 == player_descriptor + 0x80000000,
            "original field or player descriptor identity differs",
        )
        if "pc" in pending:
            require(name == "sprite-pc-loop", "original PC store is not followed by its loop")
        if "speed_pc" in pending:
            require(name == "sprite-pc-advance", "original speed return did not reach PC store")
        if name.startswith("motion-"):
            require(
                gpr[22] == u32(payload["descriptor-table"]) + 92 * gpr[21]
                and gpr[16] == u32(payload["descriptor"], 0x4C)
                and gpr[19] == u32(payload["descriptor"], 4),
                "original motion actor index or descriptor relationship differs",
            )
        if name == "motion-ready":
            require(not pending, "nested original motion call")
            mode = motion_mode(
                u32(payload["actor"]),
                u32(payload["held-inputs"], 4),
                u32(payload["movement-controls"], 4),
                s16(payload["actor"], 0xE8),
            )
            pending["motion"] = [row, payload, mode, False]
        elif name == "motion-mode":
            require(set(pending) == {"motion"}, "orphan original motion-mode prefix")
            before, old, mode, seen = pending["motion"]
            require(
                mode is not None and not seen and gpr[20] == mode,
                "original motion mode, inhibition or prefix order differs",
            )
            call(row, before, 0, before["gpr_u32"][31])
            expected = dict(old)
            expected["motion-actor-index"] = before["gpr_u32"][21].to_bytes(4, "little")
            exact_payload(payload, expected, "motion mode prefix")
            pending["motion"][3] = True
            modes[mode] += 1
        elif name == "motion-after":
            require(set(pending) == {"motion"}, "incomplete original motion call")
            before, old, mode, seen = pending.pop("motion")
            call(row, before, 0)
            require(
                all(gpr[i] == before["gpr_u32"][i] for i in (16, 19, 21, 22))
                and u32(payload["motion-actor-index"]) == gpr[21]
                and seen == (mode is not None),
                "original motion argument or branch differs",
            )
            if mode is None:
                require(gpr[31] == before["gpr_u32"][31], "inhibited motion changed its return")
                expected = dict(old)
                expected["motion-actor-index"] = gpr[21].to_bytes(4, "little")
                exact_payload(payload, expected, "inhibited motion")
                outcomes["inhibited_motion"] += 1
            else:
                outcomes["active_motion_body_opaque"] += 1
        elif name == "field-velocity-before":
            require(not (set(pending) - {"motion"}), "nested original field velocity call")
            require(
                gpr[4] == u32(payload["descriptor"], 4)
                and gpr[6] >= u32(payload["descriptor-table"])
                and (gpr[6] - u32(payload["descriptor-table"])) % 92 == 0
                and u32(payload["descriptor"], 0x58) & 0x40
                and not gpr[5] & 0x8000,
                "unobserved field velocity branch or invalid descriptor",
            )
            if "motion" in pending:
                require(pending["motion"][3], "field velocity preceded its motion-mode prefix")
            pending["field"] = [row, payload, None, False]
        elif name == "field-actor":
            require(
                "field" in pending and not (set(pending) - {"field", "motion"}),
                "orphan original field actor selection",
            )
            before, old, effect, completed = pending["field"]
            require(
                effect is None
                and gpr[4] == u32(payload["descriptor"], 0x4C)
                and gpr[18] == before["gpr_u32"][4]
                and gpr[19] == before["gpr_u32"][6]
                and gpr[3] == u32(payload["actor"], 4),
                "original field actor arguments differ",
            )
            call(row, before, -40, before["gpr_u32"][31])
            exact_payload(
                selected(payload, COMMON + ("sprite", "descriptor")), old, "field actor selection"
            )
            effect = field_party_velocity(
                before["gpr_u32"][5],
                s16(old["sprite"], 0x32),
                u32(old["descriptor"], 0x58),
                u32(payload["actor"], 4),
                u32(old["sprite"], 0x18),
                u32(old["sprite"], 0xAC),
                table,
            )
            pending["field"][2] = effect
            inputs[("field_actor_flags", u32(payload["actor"], 4))] += 1
        elif name == "sprite-velocity-before":
            require(
                "sprite" not in pending and (("field" in pending) != ("speed" in pending)),
                "orphan or nested original sprite velocity",
            )
            if "field" in pending:
                before, old, effect, completed = pending["field"]
                require(
                    effect is not None and not completed and gpr[4] == before["gpr_u32"][4],
                    "missing field actor selection or repeated sprite velocity",
                )
                expected = changed(
                    selected(old, COMMON + ("sprite",)), gpr[4], [(0x32, effect.angle, 2)]
                )
                call(row, before, -64, 0x80021FF0)
            else:
                before, old, speed, completed = pending["speed"]
                require(not completed and gpr[4] == before["gpr_u32"][19], "repeated speed vector")
                expected = changed(selected(old, COMMON + ("sprite",)), gpr[4], [(0x18, speed, 4)])
                call(row, before, 0, 0x800219A4)
            exact_payload(payload, expected, "sprite velocity caller stores")
            sine, cosine = trig_pair(table, s16(payload["sprite"], 0x32))
            vector = sprite_velocity(
                u32(payload["sprite"], 0x18), u32(payload["sprite"], 0xAC), sine, cosine
            )
            pending["sprite"] = [row, payload, vector, []]
            inputs[("sprite_speed", u32(payload["sprite"], 0x18))] += 1
            inputs[("sprite_angle", s16(payload["sprite"], 0x32))] += 1
            inputs[("sprite_divisor", (u32(payload["sprite"], 0xAC) >> 7) & 0xFFF)] += 1
        elif name in ("sine-result", "cosine-result"):
            require(
                0 <= gpr[4] < 4096 and gpr[8] == gpr[4] * 4 and gpr[1] == 0x80050000 + gpr[8],
                "original trigonometric index differs",
            )
            pair = table[gpr[8] : gpr[8] + 4]
            value = s16(pair, 0 if name == "sine-result" else 2)
            require(
                payload["lookup-pair"] == pair and gpr[2] == value & 0xFFFFFFFF,
                "original trigonometric source pair or signed result differs",
            )
            if "sprite" in pending:
                before, old, vector, calls = pending["sprite"]
                require(
                    len(calls) < 2
                    and name == ("cosine-result" if not calls else "sine-result")
                    and gpr[4] == s16(old["sprite"], 0x32) & 0xFFF
                    and gpr[17] == before["gpr_u32"][4]
                    and gpr[16] == vector.scalar & 0xFFFFFFFF,
                    "original sprite lookup order, angle, scalar or pointer differs",
                )
                call(row, before, -32, 0x800229B4 if not calls else 0x800229D0)
                expected = changed(old, gpr[17], [(0x0C, vector.x, 4)] if calls else [])
                exact_payload(
                    selected(payload, COMMON),
                    selected(expected, COMMON),
                    "sprite lookup shared state",
                )
                calls.append(name)
            outcomes[name] += 1
        elif name == "sprite-velocity-after":
            require("sprite" in pending, "orphan original sprite velocity result")
            before, old, vector, calls = pending.pop("sprite")
            require(
                len(calls) == 2
                and gpr[17] == before["gpr_u32"][4]
                and gpr[16] == vector.scalar & 0xFFFFFFFF,
                "missing sprite lookup or changed scalar",
            )
            call(row, before, -32, 0x800229D0)
            exact_payload(
                payload,
                changed(old, gpr[17], [(0x0C, vector.x, 4), (0x14, vector.z, 4)]),
                "sprite planar vector",
            )
            if "field" in pending:
                require(
                    vector == pending["field"][2].sprite, "field and sprite vector inputs differ"
                )
                pending["field"][3] = True
            else:
                require("speed" in pending, "missing original sprite velocity caller")
                pending["speed"][3] = True
                pending["speed"][1] = changed(
                    pending["speed"][1],
                    gpr[17],
                    [(0x18, pending["speed"][2], 4), (0x0C, vector.x, 4), (0x14, vector.z, 4)],
                )
            outcomes["sprite_velocity"] += 1
        elif name == "field-velocity-after":
            require("field" in pending and "sprite" not in pending, "orphan field velocity return")
            before, old, effect, completed = pending.pop("field")
            require(
                completed and gpr[18] == before["gpr_u32"][4] and gpr[19] == before["gpr_u32"][6],
                "incomplete field vector or changed arguments",
            )
            call(row, before, -40, 0x80082034)
            exact_payload(
                payload,
                changed(
                    old,
                    gpr[18],
                    [(0x32, effect.angle, 2), (0x0C, effect.x, 4), (0x14, effect.z, 4)],
                ),
                "field planar quantization",
            )
            outcomes["field_velocity"] += 1
        elif name == "speed-before":
            require(
                not pending and gpr[19] in loops and gpr[5] == 0xA0 and gpr[31] == 0x80024EDC,
                "original speed command did not originate in the observed sprite VM",
            )
            loop, old = loops[gpr[19]]
            opcode = source_command(resources, old, u32(old["sprite"], 0x64), 1, ram)[0]
            require(
                opcode == 0xA0
                and gpr[17] == u32(old["sprite"], 0x64) + 1
                and payload["operand"] == source_command(resources, payload, gpr[17] - 1, 8, ram),
                "original A0 operand pointer or source bytes differ",
            )
            call(row, loop, -136, 0x80024EDC)
            exact_payload(
                selected(payload, COMMON + ("sprite",)),
                selected(old, COMMON + ("sprite",)),
                "sprite VM to A0 dispatch",
            )
            speed = animation_speed(
                payload["operand"][1],
                s16(payload["sprite"], 0x82),
                u32(payload["sprite-globals"], 8),
            )
            pending["speed"] = [row, payload, speed, False]
            speeds.append(
                {
                    "frontend_run": row["frontend_run"],
                    "sprite_pointer": f"0x{gpr[19]:08x}",
                    "operand": payload["operand"][1],
                    "speed": speed,
                    "scale": s16(payload["sprite"], 0x82),
                    "rate_control": u32(payload["sprite-globals"], 8),
                }
            )
        elif name == "speed-after":
            require(set(pending) == {"speed"}, "orphan original speed return")
            before, expected, speed, completed = pending.pop("speed")
            require(
                completed and gpr[19] == before["gpr_u32"][19] and gpr[17] == before["gpr_u32"][17],
                "missing original speed vector or changed operand pointer",
            )
            call(row, before, 0, 0x800219A4)
            exact_payload(payload, expected, "speed command")
            pending["speed_pc"] = (row, payload)
            outcomes["speed_commands"] += 1
        elif name == "sprite-pc-advance":
            require(
                not (set(pending) - {"speed_pc"}) and gpr[17] in loops,
                "original sprite PC store has no corresponding loop",
            )
            loop, old = loops[gpr[17]]
            opcode = source_command(command_resources, old, u32(old["sprite"], 0x64), 1, ram)[0]
            require(
                payload["command-widths"] == widths
                and gpr[18] == opcode
                and gpr[16] == u32(old["sprite"], 0x64) + 1
                and gpr[3] == widths[opcode],
                "original sprite opcode, operand pointer or width differs",
            )
            call(row, loop, 0)
            value = command_pc_store(u32(payload["sprite"], 0x64), opcode, widths)
            require(gpr[2] == value, "original sprite post-handler PC addition differs")
            if "speed_pc" in pending:
                before, speed_payload = pending.pop("speed_pc")
                call(row, before, 136)
                require(opcode == 0xA0, "speed command reached another opcode's PC store")
                exact_payload(
                    selected(payload, COMMON + ("sprite",)),
                    selected(speed_payload, COMMON + ("sprite",)),
                    "A0 return to PC store",
                )
            pending["pc"] = (row, payload, value)
            opcodes[opcode] += 1
            inputs[("command_source_base", u32(old["sprite"], 0x44))] += 1
        elif name == "sprite-pc-loop":
            require(
                not (set(pending) - {"pc"}) and payload["command-widths"] == widths,
                "unexpected original sprite loop or changed width table",
            )
            if "pc" in pending:
                before, old, value = pending.pop("pc")
                require(gpr[17] == before["gpr_u32"][17], "original PC store changed sprite")
                call(row, before, 0, before["gpr_u32"][31])
                exact_payload(
                    payload, changed(old, gpr[17], [(0x64, value, 4)]), "committed sprite PC"
                )
                outcomes["committed_pc_stores"] += 1
            else:
                outcomes["other_sprite_loop_entries_opaque"] += 1
            # The loop first tests sprite+9e; an opaque return need not read a
            # command at all. Qualify its source only when a compared path does.
            loops[gpr[17]] = (row, payload)
        else:
            raise ValueError("unimplemented original planar observation")
    require(
        not pending
        and all(
            outcomes[k]
            for k in (
                "inhibited_motion",
                "active_motion_body_opaque",
                "field_velocity",
                "sprite_velocity",
                "speed_commands",
                "committed_pc_stores",
            )
        )
        and set(modes) == {1, 2},
        "incomplete original planar comparison",
    )
    return {
        "result": "passed",
        "records": sum(counts.values()),
        "counts": dict(counts),
        "outcomes": dict(outcomes),
        "motion_modes": dict(modes),
        "pc_opcodes": [
            {"opcode": f"0x{opcode:02x}", "width": widths[opcode], "count": count}
            for opcode, count in sorted(opcodes.items())
        ],
        "speed_commands": speeds,
        "observed_inputs": [
            {"field": name, "value": value, "count": count}
            for (name, value), count in sorted(inputs.items())
        ],
        "trig_table_sha256": digest(table),
        "command_widths_sha256": digest(widths),
        "field_sprite_component": field_component,
    }


def compare(sources: dict, loader_capture: Path, planar_capture: Path) -> dict:
    loader, resources = compare_loader(sources, loader_capture)
    spec = planar_specification(sources, (planar_capture / "final.ram").read_bytes())
    ram, rows, observation = capture_records(planar_capture, sources, spec, PLANAR_WINDOWS)
    require(
        u32(ram, 0x183D8 + (0xA0 - 0x8A) * 4) == 0x80021958, "original sprite A0 dispatch differs"
    )
    planar = compare_planar_records(rows, sources, spec, ram, resources)
    paths = (
        "tools/analysis/planar_motion.py",
        "tools/analysis/verify_planar_motion.py",
        "tools/analysis/jump_physics.py",
        "tools/analysis/original_trace.py",
        "tools/analysis/party_sprites.py",
        "tools/analysis/arithmetic.py",
        "tools/analysis/field.py",
        "tools/analysis/packed.py",
        "tools/analysis/verify_collision_math.py",
        "tools/analysis/verify_field.py",
    )
    return {
        "schema_version": 1,
        "kind": "original_planar_motion_comparison",
        "result": "passed",
        "source_profile": sources["profile"]["id"],
        "map": sources["map"],
        "raw_track_sha256": sources["raw_sha256"],
        "loader": loader,
        "planar": planar,
        "trace": {
            "path": str(planar_capture / "instruction-trace.jsonl"),
            "sha256": observation["instruction_trace"]["trace_sha256"],
        },
        "observation": {
            "path": str(planar_capture / "observation.json"),
            "sha256": file_sha(planar_capture / "observation.json"),
        },
        "tool_sources": {path: file_sha(ROOT / path) for path in paths},
        "code_windows": [
            {
                "begin": f"0x{begin:08x}",
                "end_exclusive": f"0x{end:08x}",
                "sha256": digest(source_bytes(sources, begin, end)),
            }
            for begin, end in LOADER_WINDOWS + PLANAR_WINDOWS
        ],
        "scope": (
            "Motion-mode prefix/inhibited return, A0 speed and nested sprite vectors, ordinary "
            "field velocity, source lookups and committed PC stores. Full active motion, other "
            "VM loop paths and other handler effects remain opaque."
        ),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "compare"))
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--map", type=int, required=True)
    parser.add_argument("--ram", type=Path)
    parser.add_argument("--loader-capture", type=Path)
    parser.add_argument("--planar-capture", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = private_output(args.output)
        sources = sprite_sources(args.raw, args.profile, args.map)
        if args.mode == "prepare":
            require(args.ram is not None, "planar preparation needs qualified original RAM")
            verify(args.raw, args.ram, args.profile, args.map)
            result = planar_specification(sources, args.ram.read_bytes())
        else:
            require(
                args.loader_capture is not None and args.planar_capture is not None,
                "planar comparison needs original loader and movement captures",
            )
            result = compare(sources, args.loader_capture, args.planar_capture)
            result["source_check"] = verify(
                args.raw, args.planar_capture / "final.ram", args.profile, args.map
            )
        with output.open("x") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
        print(f"{args.mode}: {output}")
    except (ValueError, KeyError, IndexError, OSError, struct.error) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
