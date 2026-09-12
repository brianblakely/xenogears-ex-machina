"""Invented geometry and state boundaries; no original data or lookup tables."""

import struct
import unittest
from dataclasses import replace

from tests.test_collision_query import fixture
from tools.analysis import position as model
from tools.analysis import position_query as layer
from tools.analysis.arithmetic import signed32
from tools.analysis.sprite_state import put, u16, u32


def flat_layers(heights=(7, 17), attributes=(0,), indices=(0, 0)):
    count = len(heights)
    cursor = 0x18 + 8 * count
    chunks = []
    for i, height in enumerate(heights):
        triangle = struct.pack("<3h4H", 0, 1, 2, 65535, 65535, 65535, indices[i])
        vertices = b"".join(
            struct.pack("<4h", x, height, z, 0) for x, z in ((0, 0), (0, 16), (16, 0))
        )
        chunks.append((cursor, cursor + 14, triangle + vertices))
        cursor += len(triangle + vertices)
    data = bytearray(cursor + 4 * len(attributes))
    put(data, 0, count)
    put(data, 0x14, cursor)
    for i, (tb, vb, chunk) in enumerate(chunks):
        put(data, 4 + 4 * i, 14)
        put(data, 0x18 + 8 * i, tb)
        put(data, 0x1C + 8 * i, vb)
        data[tb : tb + len(chunk)] = chunk
    for i, value in enumerate(attributes):
        put(data, cursor + 4 * i, value)
    return bytes(data)


class PositionTests(unittest.TestCase):
    def setUp(self):
        self.actor = bytearray(312)
        self.sprite = bytearray(512)
        self.descriptor = bytearray(92)
        struct.pack_into("<3i", self.actor, 0x20, 4 << 16, 7 << 16, 4 << 16)
        put(self.actor, 0x74, 255, 1)
        put(self.actor, 0x72, 7, 2)
        put(self.actor, 0x1A, 48, 2)
        put(self.sprite, 0x84, 7, 2)
        put(self.sprite, 0x1C, 4096)
        self.frame = bytes(i & 255 for i in range(256))
        self.ring = bytes((i * 13) & 255 for i in range(2304))
        self.controls = model.PositionControls(3, 1, 0, 0, 1, 3, 1)
        self.component = flat_layers()

    def run_position(self, **changes):
        args = dict(
            actor_index=3,
            linked_floor=0,
            link_status=0,
            actor=self.actor,
            sprite=self.sprite,
            descriptor=self.descriptor,
            frame=self.frame,
            sp=0x800F0000,
            gpr=tuple(range(34)),
            controls=self.controls,
            component=self.component,
            reciprocal=(4096,) * 192,
            ring=self.ring,
            history_index=0,
            history_reset=19,
            position_result=123,
        )
        args.update(changes)
        return model.position(**args)

    def at(self, result, name):
        return next(s for s in result.stages if s.name == name)

    def test_idle_sets_only_controlled_result_and_preserves_locals(self):
        result = self.run_position(controls=replace(self.controls, forced=0), component=b"")
        out = result.stages[-1]
        self.assertEqual((result.value, result.reason, out.position_result), (-1, "idle", 65535))
        self.assertEqual(
            (out.actor, out.sprite, out.descriptor, out.ring),
            (self.actor, self.sprite, self.descriptor, self.ring),
        )
        self.assertEqual(out.frame[0x18:0xD8], self.frame[0x18:0xD8])

    def test_each_inhibition_precedes_forced_update_and_geometry_access(self):
        for offset, flag, reason in (
            (0, 0x1000000, "actor-inhibited"),
            (4, 0x200000, "layer-inhibited"),
            (0, 0x10000, "position-inhibited"),
        ):
            with self.subTest(flag=flag):
                actor = bytearray(self.actor)
                put(actor, offset, flag)
                result = self.run_position(actor=actor, component=b"", actor_index=5)
                self.assertEqual(
                    (result.value, result.reason, result.stages[-1].position_result),
                    (-1, reason, 123),
                )

    def test_stationary_commit_computes_floors_and_history_without_stale_locals(self):
        result = self.run_position()
        out = result.stages[-1]
        self.assertEqual(
            (result.value, result.reason, out.history_index, out.history_reset),
            (0, "committed", 31, 0),
        )
        self.assertEqual(
            struct.unpack_from("<4i", out.frame, 0x18), (7, 17, 0x7FFFFFFF, 0x7FFFFFFF)
        )
        self.assertEqual(struct.unpack_from("<3i", out.descriptor, 0x20), (4, 7, 4))
        self.assertEqual(out.frame[0x5C:0x60], self.frame[0x5C:0x60])
        self.assertEqual(out.frame[0x6C:0x70], self.frame[0x6C:0x70])
        self.assertEqual(out.frame[0xA8:0xD8], self.frame[0xA8:0xD8])

    def test_missing_later_query_preserves_horizontal_position_and_triangles(self):
        put(self.actor, 10, -1, 2)
        put(self.actor, 0x30, 65536)
        put(self.actor, 0xF0, 999)
        result = self.run_position()
        ground = self.at(result, "position-ground")
        out = result.stages[-1]
        self.assertEqual(result.queries[1].reason, "missing-initial-triangle")
        self.assertEqual((u32(ground.actor, 0x20), u32(ground.actor, 0xF0)), (4 << 16, 0))
        self.assertEqual(out.actor[8:16], self.actor[8:16])
        self.assertEqual(result.value, 0)

    def test_sort_moves_floor_upper_and_layer_together_with_equal_stability(self):
        result = self.run_position(component=flat_layers((17, 7), indices=(0x100, 0x200)))
        stage = self.at(result, "position-sorted")
        self.assertEqual(struct.unpack_from("<3i", stage.frame, 0x18), (7, 17, 0x7FFFFFFF))
        self.assertEqual(struct.unpack_from("<3i", stage.frame, 0x28), (15, 21, 0x7FFFFFFF))
        self.assertEqual(struct.unpack_from("<3i", stage.frame, 0x38), (1, 0, 2))
        self.assertEqual(u16(self.at(result, "position-selected-layer").actor, 0x10), 1)
        same = self.at(self.run_position(component=flat_layers((7, 7))), "position-sorted")
        self.assertEqual(struct.unpack_from("<3i", same.frame, 0x38), (0, 1, 2))

    def test_disabled_layer_is_masked_after_query_outputs_are_written(self):
        put(self.actor, 4, 1)
        result = self.run_position()
        raw = self.at(result, "position-layers")
        sorted_ = self.at(result, "position-sorted")
        self.assertEqual(u32(raw.frame, 0x18), 7)
        self.assertEqual(
            struct.unpack_from("<3i", sorted_.frame, 0x18), (17, 0x7FFFFFFF, 0x7FFFFFFF)
        )
        self.assertEqual(u16(self.at(result, "position-selected-layer").actor, 0x10), 1)

    def test_step_back_attribute_selects_previous_sorted_layer(self):
        put(self.actor, 0x24, 12 << 16)
        result = self.run_position(
            component=flat_layers((7, 17), attributes=(0, 4), indices=(0, 1))
        )
        self.assertEqual(u16(self.at(result, "position-selected-layer").actor, 0x10), 0)

    def test_linked_floor_updates_floor_y_and_detaches_low_current_surface(self):
        put(self.actor, 0x74, 5, 1)
        result = self.run_position(linked_floor=-3, link_status=1)
        stage = self.at(result, "position-vertical-before")
        self.assertEqual(
            (u16(stage.sprite, 0x84), signed32(u32(stage.actor, 0x24)), stage.actor[0x74]),
            (65533, -3 << 16, 5),
        )
        stage = self.at(
            self.run_position(linked_floor=7, link_status=1), "position-vertical-before"
        )
        self.assertEqual(stage.actor[0x74], 255)

    def test_nonzero_collision_mode_uses_unsigned_link_status_cutoff(self):
        for status, floor in ((0, 3), (1, 3), (2, 7), (0xFFFFFFFF, 7)):
            with self.subTest(status=status):
                stage = self.at(
                    self.run_position(
                        linked_floor=3,
                        link_status=status,
                        controls=replace(self.controls, collision_mode=1),
                    ),
                    "position-vertical-before",
                )
                self.assertEqual(
                    (u16(stage.sprite, 0x84), u32(stage.actor, 0x24)), (floor, 7 << 16)
                )

    def test_forced_height_applies_before_vertical_integration(self):
        put(self.actor, 0, 0x40000)
        put(self.actor, 0xEC, -4, 2)
        put(self.sprite, 0x10, 1234)
        stage = self.at(self.run_position(), "position-vertical-before")
        self.assertEqual((signed32(u32(stage.actor, 0x24)), u32(stage.sprite, 0x10)), (-4 << 16, 0))

    def test_airborne_integration_preserves_fraction_and_adds_gravity(self):
        put(self.actor, 0x24, (4 << 16) + 123)
        put(self.sprite, 0x10, -8192)
        result = self.run_position()
        stage = self.at(result, "position-vertical-after")
        self.assertEqual(
            (
                u32(stage.actor, 0x24),
                signed32(u32(stage.sprite, 0x10)),
                signed32(u32(stage.actor, 0xF0)),
            ),
            ((4 << 16) + 123 - 8192, -4096, -4096),
        )
        self.assertTrue(u32(stage.actor) & 0x1000)

    def test_terrain_rejection_rolls_back_xz_but_integrates_y_and_writes_history(self):
        put(self.actor, 0, 0x100)
        put(self.actor, 0x30, 65536)
        put(self.sprite, 0x10, -8192)
        result = self.run_position(component=flat_layers(attributes=(0x20,)))
        out = result.stages[-1]
        self.assertEqual((result.reason, out.position_result), ("terrain-rejected", 0xFFF))
        self.assertEqual(
            (u32(out.actor, 0x20), u32(out.actor, 0x24), signed32(u32(out.sprite, 0x10))),
            (4 << 16, (7 << 16) - 8192, -4096),
        )
        self.assertEqual(out.descriptor[0x20:0x24], self.descriptor[0x20:0x24])
        self.assertEqual(out.history_index, 31)

    def test_unreconstructed_diagnostic_call_is_explicit(self):
        put(self.actor, 0, 0x100)
        for control in (None, 0):
            with self.subTest(control=control), self.assertRaisesRegex(ValueError, "diagnostic"):
                self.run_position(
                    component=flat_layers(attributes=(0x20,)),
                    controls=replace(self.controls, debug_control=control),
                )

    def test_negative_headroom_rolls_back_horizontal_and_upward_motion(self):
        put(self.actor, 0x30, 65536)
        for velocity, y in ((-8192, (7 << 16) - 8192), (-12288, 7 << 16)):
            with self.subTest(velocity=velocity):
                put(self.sprite, 0x10, velocity)
                result = self.run_position(component=flat_layers(indices=(0xFF00, 0)))
                out = result.stages[-1]
                self.assertEqual(
                    (
                        result.reason,
                        u32(out.actor, 0x20),
                        u32(out.actor, 0x24),
                        u32(out.sprite, 0x10),
                    ),
                    ("volume-rejected", 4 << 16, y, 0),
                )
                self.assertEqual(u32(out.actor, 0xF0), 0)

    def test_foreign_actor_does_not_write_controlled_history_or_result(self):
        result = self.run_position(actor_index=5)
        out = result.stages[-1]
        self.assertEqual(
            (out.ring, out.history_index, out.history_reset, out.position_result),
            (self.ring, 0, 19, 123),
        )


class LayerAndHistoryTests(unittest.TestCase):
    setUp = PositionTests.setUp

    def test_layer_query_rejects_triangle_indices_outside_its_table(self):
        # These offsets can still lie inside the component's vertex/other-layer data.
        for index in (-2, 1, 3, 32767):
            with self.subTest(index=index):
                put(self.actor, 8, index, 2)
                with self.assertRaisesRegex(ValueError, "layer 0 triangle.*outside"):
                    layer.layer_floor(self.component, (4096,) * 192, self.actor, 0, 0)

    def test_layer_query_rejects_out_of_table_neighbors_after_crossing(self):
        put(self.actor, 0x20, -2 << 16)
        for index in (-2, 1, 3, 32767):
            with self.subTest(index=index):
                component = bytearray(self.component)
                put(component, u32(component, 0x18) + 6, index, 2)
                with self.assertRaisesRegex(ValueError, "layer 0 triangle.*outside"):
                    layer.layer_floor(component, (4096,) * 192, self.actor, 0, 0)

    def test_layer_query_rejects_vertices_outside_the_layer_vertex_table(self):
        for index in (-1, 3, 32767):
            with self.subTest(index=index):
                component = bytearray(self.component)
                put(component, u32(component, 0x18), index, 2)
                with self.assertRaisesRegex(ValueError, "layer 0 triangle 0: invalid vertex index"):
                    layer.layer_floor(component, (4096,) * 192, self.actor, 0, 0)

    def test_layer_query_follows_signed_neighbor_and_writes_terminal_index(self):
        put(self.actor, 0x20, 12 << 16)
        put(self.actor, 0x28, 12 << 16)
        result = layer.layer_floor(fixture(second=True), (4096,) * 192, self.actor, 0, 0)
        self.assertEqual((result.value, result.floor, result.triangle), (0, 11, 1))
        self.assertEqual(result.steps, ((0, 0, 2), (1, 1, 0)))

    def test_terrain_mask_preserves_normal_and_triangle_outputs(self):
        component = fixture(attributes=(0x800000,), triangle_attributes=(0x7F00, 0))
        result = layer.layer_floor(component, (4096,) * 192, self.actor, 0, 0)
        self.assertEqual((result.floor, result.upper, result.triangle), (0x7FFFFFFF, 0x7FFFFFFF, 0))
        self.assertIsNotNone(result.normal)
        for control, flags in ((1, 0), (0, 8)):
            put(self.actor, 4, flags)
            result = layer.layer_floor(component, (4096,) * 192, self.actor, 0, control)
            self.assertEqual((result.floor, result.upper), (7, 515))

    def test_layer_extent_is_signed_and_motion_can_reuse_prior_floor(self):
        put(self.actor, 0x34, 1)
        put(self.actor, 0x72, -9, 2)
        result = layer.layer_floor(
            fixture(triangle_attributes=(0xFF00, 0)), (4096,) * 192, self.actor, 0, 0
        )
        self.assertEqual((result.floor, result.upper), (-9, -9))

    def test_missing_neighbor_and_iteration_limit_write_no_outputs(self):
        put(self.actor, 0x20, -2 << 16)
        for component, reason, steps in (
            (fixture(), "missing-neighbor", 1),
            (fixture(self_neighbor=True), "iteration-limit", 32),
        ):
            result = layer.layer_floor(component, (4096,) * 192, self.actor, 0, 0)
            self.assertEqual((result.value, result.reason, len(result.steps)), (-1, reason, steps))
            self.assertEqual(
                (result.floor, result.upper, result.normal, result.triangle),
                (None, None, None, None),
            )

    def test_history_writes_low_direction_and_preserves_sparse_record_padding(self):
        put(self.actor, 0x106, 0xF123, 2)
        put(self.actor, 0xE8, 0x8765, 2)
        put(self.sprite, 0x10, -12345)
        ring, index, reset = layer.history_store(3, 3, 0, 1, 99, self.ring, self.actor, self.sprite)
        self.assertEqual((index, reset), (0, 0))
        self.assertEqual(struct.unpack_from("<3H", ring, 72 + 0x10), (7, 0x8765, 0x123))
        self.assertEqual(struct.unpack_from("<i", ring, 72 + 0x24)[0], -12345)
        for a, b in (
            (0, 72),
            (72 + 14, 72 + 16),
            (72 + 30, 72 + 32),
            (72 + 44, 72 + 48),
            (72 + 60, 72 + 64),
            (72 + 69, 2304),
        ):
            self.assertEqual(ring[a:b], self.ring[a:b])


if __name__ == "__main__":
    unittest.main()
