"""Invented actor/geometry inputs; these tests do not establish original behavior."""

import struct
import unittest
from dataclasses import replace

from tests.test_position import flat_layers
from tools.analysis.actor_defaults import initialize_actor_defaults, random_step
from tools.analysis.field import collision_package
from tools.analysis.sprite_state import put, u16, u32


class ActorDefaultsTests(unittest.TestCase):
    def setUp(self):
        self.actor = bytes((i * 31 + 17) & 255 for i in range(0x138))
        self.descriptor = bytearray((i * 13 + 7) & 255 for i in range(0x5C))
        struct.pack_into("<3i", self.descriptor, 0x20, 4, 123, 4)
        put(self.descriptor, 0x58, 0, 2)
        self.scratch = bytes((i * 7 + 5) & 255 for i in range(0x60))
        self.mesh = collision_package(flat_layers(attributes=(0x12340020,)))

    def run_defaults(self, **changes):
        values = dict(
            actor=self.actor,
            descriptor=self.descriptor,
            seed=1,
            layer_count=3,
            triangle_counts=(1, 1, 0, 0),
            mesh=self.mesh,
            reciprocal=(4096,) * 192,
            scratch=self.scratch,
        )
        values.update(changes)
        return initialize_actor_defaults(**values)

    def test_rng_zero_one_and_high_seed_wrap(self):
        self.assertEqual((random_step(0).seed, random_step(0).value), (0x3039, 0))
        self.assertEqual((random_step(1).seed, random_step(1).value), (0x41C67EA6, 16838))
        self.assertEqual(
            (random_step(0xFFFFFFFF).seed, random_step(0xFFFFFFFF).value), (0xBE39E1CC, 15929)
        )
        self.assertEqual(random_step(0x100000001), random_step(1))
        self.assertEqual(random_step(-1), random_step(0xFFFFFFFF))

    def test_default_positions_use_layer_zero_and_two_queries(self):
        result = self.run_defaults()
        self.assertEqual(result.queried_layers, 2)
        self.assertEqual(struct.unpack_from("<3i", result.descriptor, 0x20), (4, 7, 4))
        self.assertEqual(struct.unpack_from("<3i", result.actor, 0x20), (4 << 16, 7 << 16, 4 << 16))
        self.assertEqual(u16(result.actor, 0x72), 7)
        self.assertEqual(u32(result.actor, 0x14), 0x12340020)
        self.assertEqual((u32(result.actor), u32(result.actor, 4)), (0xB0, 0x800))
        self.assertEqual(result.seed, 0x41C67EA6)
        self.assertEqual(u16(result.actor, 0x102), 16838)

    def test_y_preservation_flag_uses_descriptor_58_and_signed_low_coordinate(self):
        put(self.descriptor, 0x58, 0x80, 2)
        put(self.descriptor, 0x24, 0xABCDFFF9)
        result = self.run_defaults()
        self.assertEqual(u32(result.descriptor, 0x24), 0xABCDFFF9)
        self.assertEqual(u32(result.actor, 0x24), 0xFFF90000)
        self.assertEqual(u16(result.actor, 0x72), 0xFFF9)
        # Same bit in descriptor+0 is a different field and must not preserve Y.
        put(self.descriptor, 0x58, 0, 2)
        put(self.descriptor, 0, 0x80)
        self.assertEqual(u32(self.run_defaults().descriptor, 0x24), 7)

    def test_queries_use_low_signed_halfwords_and_position_shifts_wrap(self):
        struct.pack_into("<3I", self.descriptor, 0x20, 0xABCD0004, 123, 0xFFFF0004)
        result = self.run_defaults()
        self.assertEqual(struct.unpack_from("<3i", result.actor, 0x20), (4 << 16, 7 << 16, 4 << 16))
        self.assertEqual(u32(result.descriptor, 0x20), 0xABCD0004)
        self.assertEqual(u32(result.descriptor, 0x28), 0xFFFF0004)

    def test_event_slots_reset_priority_and_preserve_unowned_control_bits(self):
        actor = bytearray(self.actor)
        for slot in range(8):
            put(actor, 0x90 + slot * 8, 0xFFFFFFFF)
        result = self.run_defaults(actor=actor)
        for slot in range(8):
            offset = 0x8C + slot * 8
            self.assertEqual(result.actor[offset : offset + 4], b"\xff\xff\x00\xff")
            self.assertEqual(u32(result.actor, offset + 4), 0xFE3CFFFF)
        self.assertEqual(result.actor[0xCC:0xD0], bytes(4))

    def test_unwritten_actor_fields_and_descriptor_bytes_survive(self):
        result = self.run_defaults()
        for start, end in (
            (0x11, 0x14),
            (0x2C, 0x30),
            (0x3C, 0x40),
            (0x68, 0x6E),
            (0xDC, 0xE2),
            (0x110, 0x11E),
            (0x12A, 0x12C),
        ):
            with self.subTest(start=start):
                # actor+11 is overwritten as the upper byte of the layer halfword.
                if start == 0x11:
                    start = 0x12
                self.assertEqual(result.actor[start:end], self.actor[start:end])
        self.assertEqual(result.descriptor[:0x24], self.descriptor[:0x24])
        self.assertEqual(result.descriptor[0x28:], self.descriptor[0x28:])

    def test_ordered_packed_flag_clears_leave_high_unowned_fields(self):
        actor = bytearray(self.actor)
        for offset in (0x12C, 0x130, 0x134):
            put(actor, offset, 0xFFFFFFFF)
        result = self.run_defaults(actor=actor)
        self.assertEqual(u32(result.actor, 0x12C), 0xF000E000)
        self.assertEqual(u32(result.actor, 0x130), 0xF0000000)
        self.assertEqual(u32(result.actor, 0x134), 0xFFFFFF1F)

    def test_random_stage_precedes_result_store_and_uses_one_step(self):
        stages = []
        result = self.run_defaults(on_stage=stages.append)
        before, after = stages[:2]
        self.assertEqual((before.name, after.name), ("random-before", "random-after"))
        self.assertEqual(before.actor, after.actor)
        self.assertEqual(before.actor[0x102:0x104], self.actor[0x102:0x104])
        self.assertEqual((before.seed, after.seed, result.seed), (1, 0x41C67EA6, 0x41C67EA6))
        self.assertEqual(stages[2].name, "locate-before")
        self.assertEqual(u16(stages[2].actor, 0x102), 16838)

    def test_scratch_padding_and_unqueried_entries_remain_input(self):
        result = self.run_defaults()
        for start, end in ((12, 16), (28, 64), (70, 72), (78, 96)):
            self.assertEqual(result.scratch[start:end], self.scratch[start:end])
        self.assertEqual(struct.unpack_from("<3h", result.scratch, 0x40), (4, 7, 4))
        self.assertEqual(struct.unpack_from("<3h", result.scratch, 0x48), (4, 17, 4))

    def test_zero_query_path_reads_supplied_scratch_without_invented_defaults(self):
        scratch = bytearray(self.scratch)
        struct.pack_into("<3i", scratch, 0, 11, -12, 13)
        put(scratch, 0x42, -15, 2)
        for layer_count in (0, 1, 0xFFFF):
            with self.subTest(layer_count=layer_count):
                result = self.run_defaults(layer_count=layer_count, scratch=scratch)
                self.assertEqual((result.queried_layers, result.scratch), (0, scratch))
                self.assertEqual(struct.unpack_from("<3i", result.actor, 0x50), (11, -12, 13))
                self.assertEqual(struct.unpack_from("<i", result.descriptor, 0x24)[0], -15)

    def test_no_match_stores_zero_but_retains_triangle_zero_terrain_semantics(self):
        put(self.descriptor, 0x20, -1)
        stages = []
        result = self.run_defaults(on_stage=stages.append)
        queries = [stage for stage in stages if stage.name == "locate-after"]
        self.assertEqual(
            [(stage.query_result, stage.query_matched) for stage in queries],
            [(0, False), (0, False)],
        )
        self.assertEqual(result.actor[8:16], bytes(8))
        self.assertEqual(result.triangle_counts, (1, 1, 0, 0))
        self.assertEqual(u32(result.actor, 0x14), 0x12340020)
        self.assertEqual(struct.unpack_from("<3i", result.actor, 0x50), (0, 0, 0))
        self.assertEqual(u32(result.descriptor, 0x24), 0)

    def test_zero_active_count_preserves_physical_triangle_for_terrain_lookup(self):
        stages = []
        result = self.run_defaults(triangle_counts=(0, 1, 0, 0), on_stage=stages.append)
        self.assertEqual(result.triangle_counts, (0, 1, 0, 0))
        self.assertEqual(u32(result.actor, 0x14), 0x12340020)
        self.assertFalse(
            next(stage for stage in stages if stage.name == "locate-after").query_matched
        )
        self.assertEqual(u32(result.descriptor, 0x24), 0)

    def test_queried_layer_and_physical_triangle_bounds_fail_explicitly(self):
        for changes in (
            dict(layer_count=6),
            dict(layer_count=4),
            dict(triangle_counts=(1, 1)),
            dict(triangle_counts=(2, 1, 0, 0)),
        ):
            with self.subTest(changes=changes), self.assertRaisesRegex(ValueError, "count"):
                self.run_defaults(**changes)
        layer = self.mesh.layers[0]
        invalid = replace(layer.triangles[0], vertices=(-1, 1, 2))
        mesh = replace(
            self.mesh, layers=(replace(layer, triangles=(invalid,)), self.mesh.layers[1])
        )
        with self.assertRaisesRegex(ValueError, "vertex"):
            self.run_defaults(mesh=mesh)

    def test_incomplete_original_storage_is_rejected_without_mutating_inputs(self):
        for name, value in (
            ("actor", self.actor[:-1]),
            ("descriptor", self.descriptor[:-1]),
            ("scratch", self.scratch[:-1]),
        ):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "Incomplete"):
                self.run_defaults(**{name: value})
        original = bytes(self.descriptor)
        self.run_defaults()
        self.assertEqual(bytes(self.descriptor), original)


if __name__ == "__main__":
    unittest.main()
