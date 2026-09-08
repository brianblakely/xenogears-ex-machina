"""Qualify the two observed party sprite resources and their original decoder calls.

This is an analysis of the selected original route, not a complete character
asset importer. Decoder inputs beyond catalog length come from captured RAM.
"""

from __future__ import annotations

import hashlib
import json
import struct
from collections import Counter
from pathlib import Path

from ..reference.inspect_disc import RawCd
from ..reference.instruction_trace import validate_instruction_trace
from .original_trace import capture_records, exact_payload, ordered_rows, require
from .packed import decode_block
from .verify_collision_math import math_sources, source_bytes
from .verify_field import file_sha

ROOT = Path(__file__).resolve().parents[2]
LOADER_WINDOWS = (
    (0x8001AD4C, 0x8001AEB8),
    (0x8001B3A8, 0x8001B480),
    (0x80028470, 0x800284B4),
    (0x80028738, 0x80028808),
    (0x80032EB4, 0x80032F54),
)


def u32(data: bytes, offset: int = 0) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sprite_sources(raw: Path, profile: str, map_id: int) -> dict:
    sources = math_sources(raw, profile, map_id)
    inventory = next(
        p
        for p in json.loads((ROOT / "analysis/coverage/source-fingerprints.json").read_text())[
            "profiles"
        ]
        if p["source_profile"] == profile
    )
    records = {
        r[0]: dict(zip(inventory["records_columns"], r, strict=True)) for r in inventory["records"]
    }
    with raw.open("rb") as stream:
        cd = RawCd(stream, raw.stat().st_size)
        sources["directory"] = cd.read_extent(40, 2048)
        sources["catalog"] = cd.read_extent(24, 15 * 2048)
        require(
            digest(sources["catalog"]) == inventory["source_index_table_sha256"],
            "original source index fingerprint differs",
        )
        base = struct.unpack_from("<H", sources["directory"], 8)[0] - 1
        sources["party_directory_base"] = base
        sources["party_resources"] = {}
        for character in (0, 2):
            record = records[base + character + 5 - 1]
            require(
                record["projection_unit"] == 2048 and 55060 <= record["projected_bytes"] < 56684,
                "sprite probe does not cover this original resource length",
            )
            packed = cd.read_extent(record["source_lba"], record["source_sector_count"] * 2048)
            require(
                digest(packed[: record["projected_bytes"]]) == record["projection_sha256"],
                "original party resource projection differs",
            )
            sources["party_resources"][character] = {"record": record, "physical": packed}
    return sources


def loader_specification(sources: dict) -> dict:
    def direct(name, offset, size):
        return {"name": name, "offset": offset, "size": size}

    def reg(name, index, offset, size):
        return {"name": name, "register": index, "relative_offset": offset, "size": size}

    common = [
        direct("field", 0x4F34C, 4),
        direct("source-tables", 0x4FDF0, 48),
        direct("party-ids", 0x62590, 12),
        direct("input-pointers", 0x65AFC, 12),
        direct("output-pointers", 0x5A414, 12),
    ]
    definitions = [
        (
            "party-before",
            0x8001B430,
            [
                reg("packed-header", 4, 0, 64),
                reg("packed-tail-a", 4, 55060, 1024),
                reg("packed-tail-b", 4, 56084, 600),
            ],
            [{"name": "packed-prefix", "register": 4, "size": 55060, "max_bytes": 55060}],
        ),
        (
            "party-after",
            0x8001B434,
            [reg("decoded-header", 14, 0, 64), reg("source-end", 4, -16, 32)],
            [{"name": "decoded", "register": 14, "end_register": 5, "max_bytes": 100000}],
        ),
        ("directory-before", 0x80028470, [], None),
        ("directory-after", 0x800284AC, [], None),
        ("file-size-row", 0x800287C0, [reg("catalog-row", 3, 0, 7)], None),
    ]
    hooks = []
    for name, pc, ranges, digests in definitions:
        hook = {
            "name": name,
            "pc": pc,
            "guard": {
                "offset": pc - 16 - 0x80000000,
                "expected": source_bytes(sources, pc - 16, pc + 16).hex(),
            },
            "ranges": common + ranges,
        }
        if digests:
            hook["digests"] = digests
        hooks.append(hook)
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": "field23-party-loader-v2",
            "source_profile": sources["profile"]["id"],
            "start_frame": 0,
            "end_frame": 4317,
            "max_callbacks": 600,
            "hooks": hooks,
        }
    )


def compare_loader_records(rows, sources: dict, spec: dict, ram: bytes) -> tuple[dict, dict]:
    pending = {}
    counts = Counter()
    resources, outputs = {}, []
    for row, payload in ordered_rows(rows, spec):
        name, gpr = row["hook"], row["gpr_u32"]
        counts[name] += 1
        base = u32(payload["source-tables"], 0x24)
        if name == "directory-before":
            require(not pending, "nested original loader directory call")
            index = (gpr[4] + gpr[5]) & 0xFFFFFFFF
            require(
                index * 2 + 2 <= len(sources["directory"]), "unresolved original directory access"
            )
            entry = struct.unpack_from("<H", sources["directory"], index * 2)[0]
            pending["directory"] = (row, payload, index, entry)
        elif name == "directory-after":
            require(set(pending) == {"directory"}, "unpaired original directory return")
            before, old, index, entry = pending.pop("directory")
            expected = dict(old)
            table = bytearray(old["source-tables"])
            struct.pack_into("<I", table, 0x24, max(entry - 1, 0))
            expected["source-tables"] = bytes(table)
            exact_payload(payload, expected, "directory return")
            require(
                gpr[2] == ((entry - 1) & 0xFFFFFFFF)
                and gpr[4] == (u32(old["source-tables"], 4) + 2 * index) & 0xFFFFFFFF
                and gpr[29] == before["gpr_u32"][29]
                and gpr[31] == before["gpr_u32"][31]
                and row["frontend_run"] == before["frontend_run"],
                "original directory result differs",
            )
        elif name == "file-size-row":
            require(not pending, "catalog observation inside an unexpected loader call")
            slot = (gpr[18] + base - 1) & 0xFFFFFFFF
            offset = 7 * slot
            require(
                offset + 7 <= len(sources["catalog"])
                and gpr[2] == slot
                and gpr[3] == (u32(payload["source-tables"]) + offset) & 0xFFFFFFFF
                and payload["catalog-row"] == sources["catalog"][offset : offset + 7],
                "original catalog index, pointer or bytes differ",
            )
        elif name == "party-before":
            require(not pending, "nested original party decoder")
            index = gpr[19]
            require(
                0 <= index < 3
                and gpr[16] == 0x80065AFC + 4 * index
                and gpr[17] == 4 * index
                and gpr[18] == 0x8005A414 + 4 * index
                and gpr[20] == 0x80062590 + 4 * index
                and gpr[31] == 0x8001B434,
                "original party decoder call relationship differs",
            )
            character = u32(payload["party-ids"], 4 * index)
            require(
                character in sources["party_resources"] and base == sources["party_directory_base"],
                "unqualified original party resource selection",
            )
            item = sources["party_resources"][character]
            packed, record = item["physical"], item["record"]
            require(
                gpr[4] == u32(payload["input-pointers"], 4 * index)
                and gpr[5] == u32(payload["output-pointers"], 4 * index),
                "original decoder arguments differ from party pointers",
            )
            require(
                payload["packed-header"] == packed[:64]
                and row.get("digests")
                == [
                    {
                        "name": "packed-prefix",
                        "pointer_value": gpr[4],
                        "size": 55060,
                        "resolved_offset": gpr[4] - 0x80000000,
                        "sha256": digest(packed[:55060]),
                    }
                ],
                "original packed sprite input differs from source",
            )
            measured = packed[:55060] + payload["packed-tail-a"] + payload["packed-tail-b"]
            size = record["projected_bytes"]
            require(
                measured[:size] == packed[:size],
                "original packed tail differs inside catalog length",
            )
            decoded = decode_block(measured)
            pending["party"] = (row, payload, character, record, measured, decoded)
        elif name == "party-after":
            require(set(pending) == {"party"}, "unpaired original party decoder return")
            before, old, character, record, measured, decoded = pending.pop("party")
            entry = before["gpr_u32"]
            source_end, output_end = (
                entry[4] + decoded.source_bytes_read,
                entry[5] + len(decoded.data),
            )
            require(
                gpr[4] == source_end
                and gpr[5] == gpr[15] == output_end
                and gpr[2] == gpr[14] == entry[5]
                and gpr[29] == entry[29]
                and gpr[31] == entry[31]
                and gpr[16:22] == entry[16:22],
                "original decoder end, output or call registers differ",
            )
            common = ("field", "source-tables", "party-ids", "input-pointers", "output-pointers")
            exact_payload(
                {k: payload[k] for k in common}, {k: old[k] for k in common}, "party decoder return"
            )
            end = decoded.source_bytes_read
            require(
                payload["source-end"] == measured[end - 16 : end + 16]
                and payload["decoded-header"] == decoded.data[:64]
                and row.get("digests")
                == [
                    {
                        "name": "decoded",
                        "pointer_value": entry[5],
                        "end_pointer_value": output_end,
                        "size": len(decoded.data),
                        "resolved_offset": entry[5] - 0x80000000,
                        "sha256": digest(decoded.data),
                    }
                ],
                "original complete decoder output or source boundary differs",
            )
            require(
                entry[5] not in resources
                and ram[entry[5] - 0x80000000 : output_end - 0x80000000] == decoded.data,
                "party resource changed between decoder return and final RAM",
            )
            resources[entry[5]] = decoded.data
            outputs.append(
                {
                    "character_id": character,
                    "party_index": entry[19],
                    "source": record,
                    "input_pointer": f"0x{entry[4]:08x}",
                    "output_pointer": f"0x{entry[5]:08x}",
                    "frontend_entry": before["frontend_run"],
                    "frontend_return": row["frontend_run"],
                    "decoded_bytes": len(decoded.data),
                    "decoded_sha256": digest(decoded.data),
                    "token_bytes": decoded.token_bytes,
                    "source_bytes_read": decoded.source_bytes_read,
                    "token_bytes_beyond_catalog": max(
                        0, decoded.token_bytes - record["projected_bytes"]
                    ),
                    "reads_beyond_catalog": max(
                        0, decoded.source_bytes_read - record["projected_bytes"]
                    ),
                    "adjacent_input_hex": measured[
                        record["projected_bytes"] : decoded.source_bytes_read
                    ].hex(),
                }
            )
        else:
            raise ValueError("unimplemented original party loader observation")
    require(not pending and len(resources) == 2, "incomplete two-party-resource observation")
    return {
        "result": "passed",
        "records": sum(counts.values()),
        "counts": dict(counts),
        "resources": outputs,
    }, resources


def compare_loader(sources: dict, capture: Path) -> tuple[dict, dict]:
    spec = loader_specification(sources)
    ram, rows, _ = capture_records(capture, sources, spec, LOADER_WINDOWS)
    result, resources = compare_loader_records(rows, sources, spec, ram)
    return {
        **result,
        "trace": {
            "path": str(capture / "instruction-trace.jsonl"),
            "sha256": file_sha(capture / "instruction-trace.jsonl"),
        },
        "observation": {
            "path": str(capture / "observation.json"),
            "sha256": file_sha(capture / "observation.json"),
        },
        "directory_sha256": digest(sources["directory"]),
        "source_catalog_sha256": digest(sources["catalog"]),
    }, resources
