"""Authored adjacency and terrain fixtures; no original geometry or lookup data."""

import struct
import unittest
from dataclasses import replace

from tools.analysis.collision_query import QueryActor, collision_query, packed_area, packed_xz


def fixture(*, second=False, attributes=(0,), triangle_attributes=(0, 0), self_neighbor=False):
    vertices = [(0, 7, 0), (0, 7, 16), (16, 7, 0)]
    triangles = [((0, 1, 2), (0 if self_neighbor else -1, 1 if second else -1, -1))]
    if second:
        vertices += [(16, 11, 16), (16, 11, 0), (0, 11, 16)]
        triangles.append(((3, 4, 5), (-1, 0, -1)))
    triangle_base = 0x30
    vertex_base = triangle_base + len(triangles) * 14
    attribute_base = vertex_base + len(vertices) * 8
    data = bytearray(attribute_base + len(attributes) * 4)
    struct.pack_into("<II", data, 0, 1, len(triangles) * 14)
    struct.pack_into("<III", data, 0x14, attribute_base, triangle_base, vertex_base)
    for index, (indices, neighbors) in enumerate(triangles):
        struct.pack_into(
            "<3h4H",
            data,
            triangle_base + 14 * index,
            *indices,
            *(n & 0xFFFF for n in neighbors),
            triangle_attributes[index],
        )
    for index, point in enumerate(vertices):
        struct.pack_into("<4h", data, vertex_base + index * 8, *point, 0)
    for index, value in enumerate(attributes):
        struct.pack_into("<I", data, attribute_base + 4 * index, value)
    return bytes(data)


class CollisionQueryTests(unittest.TestCase):
    table = (4096,) * 192
    actor = QueryActor(0, 0, 0, 0, (4 << 16, 7 << 16, 4 << 16))

    def query(self, component, *, actor=None, target=(4, 4), ordinary=True, mode=-1, control=0):
        actor = actor or self.actor
        candidate = (
            (target[0] << 16) - actor.position[0],
            None,
            (target[1] << 16) - actor.position[2],
        )
        return collision_query(
            component,
            self.table,
            actor,
            candidate,
            ordinary=ordinary,
            mode=mode,
            attribute_control=control,
        )

    def test_inside_probe_writes_zero_y_and_preserves_edge_and_actor_triangle(self):
        result = self.query(fixture())
        self.assertEqual((result.value, result.point, result.attribute), (0, (4, 0, 4), 0))
        self.assertIsNone(result.edge)
        self.assertEqual(result.heights, ())
        self.assertEqual((result.counter, result.mask, self.actor.triangle), (256, 0, 0))

    def test_final_height_and_exact_edge_points_count_as_inside(self):
        result = self.query(fixture(), target=(0, 8), mode=0)
        self.assertEqual((result.value, result.point), (0, (0, 7, 8)))
        self.assertEqual(len(result.heights), 1)
        self.assertEqual(result.areas[0].value, 0)

    def test_crossing_uses_neighbor_order_and_keeps_actor_triangle_unchanged(self):
        result = self.query(fixture(second=True), target=(12, 12), mode=0)
        self.assertEqual(
            (result.value, result.point, result.terminal_triangle), (0, (12, 11, 12), 1)
        )
        self.assertEqual([step.triangle for step in result.steps], [0, 1])
        self.assertEqual([step.mask for step in result.steps], [2, 0])
        self.assertEqual(self.actor.triangle, 0)

    def test_missing_initial_triangle_does_not_write_any_output(self):
        result = self.query(fixture(), actor=replace(self.actor, triangle=-1))
        self.assertEqual(result.value, -1)
        self.assertEqual((result.point, result.edge, result.attribute), (None, None, None))
        self.assertEqual((result.areas, result.attribute_reads), ((), ()))

    def test_missing_neighbor_reads_actual_preceding_attribute_byte(self):
        data = bytearray(fixture(attributes=(0, 0x20)))
        data[0x2E] = 1  # triangle(-1)+12, within the authored preceding header.
        result = self.query(bytes(data), target=(-2, 4))
        self.assertEqual(
            (result.value, result.reason, result.attribute), (-1, "missing-neighbor", 0x20)
        )
        self.assertEqual(result.attribute_reads[-1].triangle, -1)
        self.assertEqual(result.attribute_reads[-1].index, 1)
        self.assertEqual(result.edge, ((0, 7, 0), (0, 7, 16)))

    def test_double_edge_decisions_use_original_position(self):
        cases = [
            ((4, 2), (-2, 20), 3, 1),
            ((4, 2), (-2, -2), 5, 4),
            ((10, 5), (20, -2), 6, 2),
        ]
        for origin, target, mask, edge_mask in cases:
            with self.subTest(mask=mask):
                actor = replace(self.actor, position=(origin[0] << 16, 7 << 16, origin[1] << 16))
                result = self.query(fixture(), actor=actor, target=target)
                self.assertEqual((result.steps[0].mask, result.mask), (mask, edge_mask))
                self.assertEqual(len(result.areas), 4)
                self.assertEqual(result.value, -1)

    def test_iteration_limit_rejects_self_adjacency_without_changing_index(self):
        result = self.query(fixture(self_neighbor=True), target=(-2, 4))
        self.assertEqual((result.value, result.reason), (-1, "iteration-limit"))
        self.assertEqual((len(result.steps), result.counter, result.terminal_triangle), (32, 32, 0))
        self.assertEqual(len(result.attribute_reads), 33)
        self.assertEqual(result.edge, ((0, 7, 0), (0, 7, 16)))

    def test_low_attribute_byte_and_low_control_byte_are_significant(self):
        data = fixture(attributes=(0, 0x800000), triangle_attributes=(0xAA01, 0))
        self.assertEqual(self.query(data).reason, "layer-zero-terrain")
        self.assertEqual(self.query(data, control=0x100).value, -1)
        self.assertEqual(self.query(data, control=1).value, 0)
        self.assertEqual(self.query(data, actor=replace(self.actor, layer_flags=8)).attribute, 0)

    def test_special_query_has_an_extra_actor_mask_and_no_attribute_output(self):
        data = fixture(attributes=(0, 8), triangle_attributes=(1, 0))
        actor = replace(self.actor, flags=0x200)
        self.assertEqual(self.query(data, actor=actor).value, 0)
        special = self.query(data, actor=actor, ordinary=False)
        self.assertEqual((special.value, special.reason), (-1, "actor-terrain-flags"))
        self.assertIsNone(special.attribute)
        self.assertIsNone(special.edge)

    def test_probe_can_compute_height_when_entering_a_special_surface(self):
        data = fixture(second=True, attributes=(0, 0x400000), triangle_attributes=(0, 1))
        actor = replace(self.actor, position=(4 << 16, 20 << 16, 4 << 16))
        result = self.query(data, actor=actor, target=(12, 12))
        self.assertEqual(
            (result.value, result.reason, result.point), (-1, "terrain-floor-height", (12, 11, 12))
        )
        self.assertEqual([height.site for height in result.heights], ["terrain"])
        self.assertEqual(result.edge, ((0, 7, 16), (16, 7, 0)))

    def test_mode_80_and_initial_special_surface_suppress_transition_height_gate(self):
        actor = replace(self.actor, position=(4 << 16, 20 << 16, 4 << 16))
        data = fixture(second=True, attributes=(0, 0x400000), triangle_attributes=(0, 1))
        mode80 = self.query(data, actor=actor, target=(12, 12), mode=0x80)
        self.assertEqual(
            (mode80.value, mode80.point, mode80.initial_special), (0, (12, 11, 12), True)
        )
        initial = fixture(second=True, attributes=(0, 0x400000), triangle_attributes=(1, 1))
        result = self.query(initial, actor=actor, target=(12, 12))
        self.assertEqual((result.value, result.point, result.heights), (0, (12, 0, 12), ()))

    def test_packed_negative_z_borrows_and_invalid_source_access_fails(self):
        self.assertEqual(packed_xz(4, -1), 0x0003FFFF)
        self.assertEqual(packed_area((packed_xz(0, 0), packed_xz(0, 16), packed_xz(2, 3))), 32)
        with self.assertRaises(ValueError):
            self.query(fixture(), actor=replace(self.actor, layer=1))
        with self.assertRaises(ValueError):
            self.query(fixture()[:0x20])


if __name__ == "__main__":
    unittest.main()
