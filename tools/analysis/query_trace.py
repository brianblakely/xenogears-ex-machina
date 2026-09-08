"""Compare original query order, signed areas, heights and caller memory effects."""

from __future__ import annotations

import struct
from collections import Counter

from .arithmetic import signed32
from .collision_math import height_and_normal
from .collision_query import collision_query, packed_area, packed_xz
from .original_trace import exact_payload, require
from .sweep_trace import (
    actor_state,
    call,
    correlate,
    ordered_rows,
    read_captured,
    selected,
    u32,
    vector,
    write_edge,
)
from .verify_collision_math import address_region


def area_pairs(rows, sources: dict, spec: dict, ram: bytes) -> tuple[list, dict]:
    pending, areas, pairs, cases = None, [], [], Counter()
    for row, payload in ordered_rows(rows, spec, sources, ram):
        name, gpr = row["hook"], row["gpr_u32"]
        if name.endswith("query-before"):
            require(pending is None, "nested original area query")
            expected = collision_query(
                sources["component"],
                sources["table"],
                actor_state(payload["actor"]).query,
                vector(payload["candidate"]),
                ordinary=name.startswith("ordinary"),
                mode=signed32(u32(payload["query-caller"], 20)),
                attribute_control=payload["query-controls"][4],
            )
            pending, areas = (row, payload, expected), []
        elif "-area-" in name:
            require(pending is not None, "orphan original signed area")
            before, old, expected = pending
            require(
                name.split("-")[0] == before["hook"].split("-")[0],
                "original area belongs to a different query",
            )
            call(row, before, 0x80 if name.startswith("special") else 0x90, row["pc"])
            points = tuple(gpr[4:7])
            value = packed_area(points)
            require(gpr[2] == value & 0xFFFFFFFF, "original signed-area result differs")
            exact_payload(payload, selected(old), "signed-area shared state")
            actual = int(name.rsplit("-", 1)[1]), points, value
            require(len(areas) < len(expected.areas), "additional original signed-area call")
            source_area = expected.areas[len(areas)]
            require(
                actual == (source_area.site, source_area.points, source_area.value),
                "original signed-area source inputs or call sequence differs",
            )
            areas.append(actual)
            cases[(name, "negative" if value < 0 else "zero" if value == 0 else "positive")] += 1
        elif name.endswith("query-after"):
            require(
                pending is not None and name.split("-")[0] == pending[0]["hook"].split("-")[0],
                "orphan original area-query return",
            )
            call(row, pending[0], 0x80 if name.startswith("special") else 0x90)
            require(
                len(areas) == len(pending[2].areas) and gpr[2] == pending[2].value & 0xFFFFFFFF,
                "original signed-area sequence incomplete or query return differs",
            )
            pairs.append({"before": pending[0], "after": row, "areas": tuple(areas)})
            pending = None
        else:
            raise ValueError("unexpected original area-trace stage")
    require(pending is None and pairs, "incomplete original signed-area trace")
    return pairs, {
        "queries": len(pairs),
        "signed_areas": sum(cases.values()),
        "cases": [list(key) + [value] for key, value in sorted(cases.items())],
    }


def caller_effect(before: dict, payload: dict, result) -> bytes:
    gpr, caller = before["gpr_u32"], bytearray(payload["query-caller"])

    def store(pointer, data):
        offset = pointer - gpr[29]
        require(
            0 <= offset <= len(caller) - len(data), "original query output outside caller capture"
        )
        caller[offset : offset + len(data)] = data

    if result.point is not None:
        store(u32(caller, 16), struct.pack("<3h", *result.point))
    if result.attribute is not None:
        store(u32(caller, 24), struct.pack("<I", result.attribute))
    return bytes(caller)


def compare_queries(rows, sources: dict, spec: dict, ram: bytes, pairs: list) -> dict:
    triangles = {
        tuple(layer.vertices[v][:3] for v in triangle.vertices)
        for layer in sources["layers"]
        for triangle in layer.triangles
    }
    component_base = u32(ram, 0xAFB18) - 0x80000000
    pending, height_pending = None, None
    index, height_pairs, nested_heights, areas_count = 0, 0, 0, 0
    cases, masks, copies = Counter(), Counter(), Counter()
    ignored = {
        "ordinary-sweep-before",
        "ordinary-sweep-after",
        "special-sweep-before",
        "special-sweep-after",
        "edge-projection-before",
        "edge-projection-after",
    }
    for row, payload in ordered_rows(rows, spec, sources, ram):
        name, gpr = row["hook"], row["gpr_u32"]
        if name.endswith("query-before"):
            require(
                pending is None and height_pending is None and index < len(pairs),
                "nested or additional original query",
            )
            correlate(row, pairs[index]["before"])
            require(
                gpr[5] == gpr[6] + 0x20 and payload["position"] == payload["actor"][0x20:0x2C],
                "original query position does not belong to its actor",
            )
            ordinary = name.startswith("ordinary")
            expected = collision_query(
                sources["component"],
                sources["table"],
                actor_state(payload["actor"]).query,
                vector(payload["candidate"]),
                ordinary=ordinary,
                mode=signed32(u32(payload["query-caller"], 20)),
                attribute_control=payload["query-controls"][4],
            )
            require(
                tuple((area.site, area.points, area.value) for area in expected.areas)
                == pairs[index]["areas"],
                "source-derived query area sequence differs",
            )
            areas_count += len(expected.areas)
            pending = {
                "before": row,
                "old": payload,
                "expected": expected,
                "ordinary": ordinary,
                "loop": 0,
                "decision": 0,
                "height": 0,
            }
        elif name.endswith("triangle-loop"):
            require(
                pending is not None
                and height_pending is None
                and pending["loop"] == pending["decision"]
                and pending["loop"] < len(pending["expected"].steps),
                "original triangle loop order differs",
            )
            ordinary = pending["ordinary"]
            require(
                name.startswith("special") != ordinary, "original loop uses a different query kind"
            )
            call(row, pending["before"], 0x90 if ordinary else 0x80)
            step = pending["expected"].steps[pending["loop"]]
            at = 0x48 if ordinary else 0x40
            require(
                gpr[16] == step.triangle & 0xFFFFFFFF
                and gpr[30] == step.target
                and u32(payload["query-frame"], at) == step.origin
                and u32(payload["query-frame"], at + 8) == step.counter
                and u32(payload["query-frame"], at - 8) == pending["expected"].attribute_mask,
                "original query triangle, coordinates, counter or attribute mask differs",
            )
            exact_payload(selected(payload), selected(pending["old"]), "query loop shared state")
            pending["loop"] += 1
        elif name.endswith("triangle-decision"):
            require(
                pending is not None
                and height_pending is None
                and pending["decision"] + 1 == pending["loop"],
                "original triangle decision order differs",
            )
            ordinary = pending["ordinary"]
            require(
                name.startswith("special") != ordinary,
                "original decision uses a different query kind",
            )
            call(row, pending["before"], 0x90 if ordinary else 0x80)
            step = pending["expected"].steps[pending["decision"]]
            registers = (20, 19, 17) if ordinary else (20, 19, 18)
            require(
                gpr[18 if ordinary else 17] == step.mask
                and tuple(gpr[k] for k in registers)
                == tuple(packed_xz(v[0], v[2]) for v in step.vertices),
                "original query edge mask or packed source vertices differs",
            )
            exact_payload(
                selected(payload), selected(pending["old"]), "query decision shared state"
            )
            masks[step.mask] += 1
            pending["decision"] += 1
        elif name == "height-before":
            require(height_pending is None, "nested original height query")
            vertices = tuple(struct.unpack_from("<3h", payload[k]) for k in "abc")
            require(
                vertices in triangles, "original height vertices absent from source collision mesh"
            )
            for vertex_index, key in enumerate("abc"):
                space, offset = address_region(gpr[vertex_index + 4])
                if space == "ram" and component_base <= offset < component_base + len(
                    sources["component"]
                ):
                    require(
                        payload[key] == ram[offset : offset + 8],
                        "original height source vertex differs",
                    )
                else:
                    copies[space] += 1
            point = struct.unpack("<3h", payload["point"])
            height, normal = height_and_normal(vertices, point[0], point[2], sources["table"])
            if pending is not None:
                require(
                    pending["height"] < len(pending["expected"].heights),
                    "additional original nested height",
                )
                expected = pending["expected"].heights[pending["height"]]
                caller = (
                    (0x8007C3D8 if pending["ordinary"] else 0x8007CAA4)
                    if expected.site == "final"
                    else 0x8007C318
                )
                call(row, pending["before"], 0x90 if pending["ordinary"] else 0x80, caller)
                require(
                    vertices == expected.vertices
                    and (point[0], point[2]) == (expected.point[0], expected.point[2])
                    and gpr[7] == u32(pending["old"]["query-caller"], 16)
                    and height == expected.point[1]
                    and normal == expected.normal,
                    "original nested query height inputs or result differs",
                )
                pending["height"] += 1
                nested_heights += 1
            height_pending = (row, payload, height, normal)
        elif name == "height-after":
            require(height_pending is not None, "orphan original height return")
            before, old, height, normal = height_pending
            call(row, before, 0x58)
            require(
                gpr[19] == before["gpr_u32"][7]
                and gpr[20] == u32(old["normal-pointer"])
                and payload["point"]
                == old["point"][:2] + struct.pack("<h", height) + old["point"][4:]
                and payload["normal"] == struct.pack("<3i", *normal),
                "original height/normal outputs differ",
            )
            exact_payload(selected(payload), selected(old), "height shared state")
            height_pairs += 1
            height_pending = None
        elif name.endswith("query-after"):
            require(
                pending is not None and height_pending is None,
                "orphan or premature original query return",
            )
            before, old, expected = pending["before"], pending["old"], pending["expected"]
            size = 0x90 if pending["ordinary"] else 0x80
            call(row, before, size)
            require(
                name.split("-")[0] == before["hook"].split("-")[0]
                and gpr[23] == before["gpr_u32"][7]
                and u32(payload["query-frame"], size - 4) == before["gpr_u32"][31]
                and gpr[2] == expected.value & 0xFFFFFFFF
                and gpr[16] == expected.terminal_triangle & 0xFFFFFFFF,
                "original query return, terminal triangle or saved caller differs",
            )
            require(
                pending["loop"] == pending["decision"] == len(expected.steps)
                and pending["height"] == len(expected.heights),
                "original query stages omitted",
            )
            require(
                read_captured(row, before["gpr_u32"][29], 176)
                == caller_effect(before, old, expected)
                and payload["edge"] == write_edge(old["edge"], expected.edge),
                "original query caller/edge writes or preserved bytes differ",
            )
            exact_payload(selected(payload), selected(old), "query return shared state")
            correlate(row, pairs[index]["after"])
            cases[
                (
                    name,
                    expected.reason,
                    len(expected.steps),
                    expected.attribute_mask,
                    expected.terminal_triangle,
                )
            ] += 1
            index += 1
            pending = None
        elif name not in ignored:
            raise ValueError("unexpected original query-trace stage")
        else:
            require(
                pending is None and height_pending is None,
                "original sweep event interrupts a query",
            )
    require(
        pending is None and height_pending is None and index == len(pairs),
        "incomplete original query trace",
    )
    return {
        "queries": index,
        "signed_areas": areas_count,
        "height_dependencies": nested_heights,
        "all_height_pairs": height_pairs,
        "geometry_copy_vertices": dict(copies),
        "edge_masks": dict(sorted(masks.items())),
        "cases": [list(key) + [value] for key, value in sorted(cases.items())],
    }
