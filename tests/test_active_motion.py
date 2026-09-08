"""Authored movement states test composition, source ordering and failure paths."""

import struct
import unittest
from dataclasses import replace

from tests.sprite_fixtures import ADDRESS, RESOURCE, forbidden_read, invented_sprite, reader
from tests.test_collision_query import fixture
from tools.analysis.active_motion import (
    MotionControls,
    active_motion,
    idle_predicate,
    motion_bounds,
)
from tools.analysis.movement_sweep import SweepTables
from tools.analysis.sprite_state import put, u16, u32


def invented_motion():
    actor, descriptor = bytearray(312), bytearray(92)
    sprite, resource, trig = invented_sprite()
    put(descriptor, 4, ADDRESS)
    put(descriptor, 0x58, 0x40)
    put(actor, 0x74, 255, 1)
    put(actor, 0xEA, 255, 2)
    put(actor, 0x104, 0x8000, 2)
    put(actor, 0xEC, 7, 2)
    struct.pack_into("<3i", actor, 0x20, 4 << 16, 7 << 16, 4 << 16)
    controls = MotionControls(0, 1, 0, 1, 3, (255, 255), {}, 0, 3, 0, 0, 0)
    tables = SweepTables((4096,) * 192, (4096,) * 192, tuple(i // 2 for i in range(1025)), trig)
    return actor, descriptor, sprite, resource, controls, tables


class ActiveMotionTests(unittest.TestCase):
    def setUp(self):
        self.actor, self.descriptor, self.sprite, self.resource, self.controls, self.tables = (
            invented_motion()
        )
        self.locals = bytes(range(40))

    def run_motion(self, **changes):
        arguments = dict(
            actor_index=3,
            descriptor=self.descriptor,
            actor=self.actor,
            sprite=self.sprite,
            locals_before=self.locals,
            controls=self.controls,
            component=fixture(),
            tables=self.tables,
            read_resource=reader(RESOURCE, self.resource),
            read_memory=forbidden_read,
        )
        arguments.update(changes)
        return active_motion(**arguments)

    def test_inhibited_call_only_reports_the_actor_index_store(self):
        put(self.actor, 0, 0x1000000)
        put(self.actor, 0xE3, 255, 1)
        result = self.run_motion(
            actor_index=0xFFFFFFFF, component=b"", read_resource=forbidden_read
        )
        self.assertTrue(result.inhibited)
        self.assertEqual(
            (result.actor, result.sprite, result.locals), (self.actor, self.sprite, self.locals)
        )
        self.assertEqual(result.actor_index, 0xFFFFFFFF)
        self.assertEqual([s.name for s in result.stages], ["motion-after"])

    def test_idle_clears_horizontal_sprite_motion_and_keeps_vertical_speed(self):
        struct.pack_into("<4i", self.sprite, 0x0C, 17, -29, 41, 55)
        put(self.actor, 4, 0x1000)
        put(self.actor, 0x106, 123, 2)
        result = self.run_motion()
        self.assertEqual((result.stop_reason, result.animation, result.sweep), ("idle", None, None))
        self.assertEqual(struct.unpack_from("<4i", result.sprite, 0x0C), (0, -29, 0, 55))
        self.assertEqual(result.actor[0x30:0x3C], bytes(12))
        self.assertEqual((u32(result.actor, 0xF0), u16(result.actor, 0x106)), (65536, 0x807B))
        self.assertEqual(u32(result.actor, 4), 0)
        self.assertEqual(result.locals[12:], self.locals[12:])

    def test_countdown_stops_decrementing_at_eight(self):
        for before, after in ((0, 0), (8, 8), (9, 8), (255, 254)):
            with self.subTest(before=before):
                put(self.actor, 0xE3, before, 1)
                self.assertEqual(self.run_motion().actor[0xE3], after)

    def test_prior_motion_prevents_the_idle_shortcut(self):
        put(self.actor, 0x30, 1)
        result = self.run_motion()
        self.assertIsNotNone(result.sweep)
        self.assertIsNone(result.stop_reason)
        self.assertIn("before-bounds", [s.name for s in result.stages])

    def test_source_velocity_and_sweep_are_computed_before_commit(self):
        put(self.actor, 0x104, 0, 2)
        put(self.actor, 0xEA, 0, 2)
        put(self.sprite, 0x18, 65536)
        put(self.actor, 0x44, 77)
        result = self.run_motion()
        candidate = next(s for s in result.stages if s.name == "before-bounds")
        self.assertEqual(struct.unpack_from("<3i", candidate.locals), (65536, 77, 0))
        self.assertEqual(struct.unpack_from("<3i", result.actor, 0x30), (65536, 0, 0))
        self.assertEqual(result.actor[0x40:0x4C], bytes(12))
        self.assertEqual(u32(result.actor) & 0x4000000, 0x4000000)
        self.assertEqual(result.actor[0x20:0x2C], self.actor[0x20:0x2C])

    def test_missing_triangle_stops_without_a_sweep_and_preserves_sprite_y(self):
        put(self.actor, 8, 0xFFFF, 2)
        put(self.actor, 4, 8)  # Original terrain helper skips this disabled layer.
        put(self.actor, 0x40, 123)
        put(self.sprite, 0x10, -77)
        result = self.run_motion()
        self.assertEqual((result.stop_reason, result.sweep), ("missing-triangle", None))
        self.assertEqual(result.actor[0x30:0x3C], bytes(12))
        self.assertEqual(u32(result.sprite, 0x10), (-77) & 0xFFFFFFFF)

    def test_temporary_party_flag_union_restores_only_its_two_bits(self):
        put(self.actor, 0, 0x200)
        put(self.actor, 0x40, 65536)
        controls = replace(
            self.controls, party_indices=(5, 7), party_flags={5: 0x400, 7: 0xFFFFFFFF}
        )
        result = self.run_motion(controls=controls)
        before = next(s for s in result.stages if s.name.startswith("sweep-"))
        self.assertEqual(u32(before.actor), 0x600)
        self.assertEqual(u32(result.actor), 0x4000200)
        self.assertEqual(controls.party_flags, {5: 0x400, 7: 0xFFFFFFFF})

    def test_nonowner_skips_party_reads_and_owner_rejects_missing_flags(self):
        put(self.actor, 0x40, 65536)
        controls = replace(self.controls, party_indices=(7, 255))
        with self.assertRaisesRegex(ValueError, "party collision"):
            self.run_motion(controls=controls)
        self.assertIsNotNone(self.run_motion(actor_index=5, controls=controls).sweep)

    def test_rejected_sweep_stops_after_preserving_source_output_writes(self):
        put(self.actor, 0x40, 65536)
        result = self.run_motion(component=fixture(attributes=(0x800000,)))
        self.assertEqual((result.sweep.value, result.stop_reason), (-1, "sweep"))
        self.assertEqual(result.actor[0x30:0x3C], bytes(12))
        self.assertEqual(u32(result.actor, 0xF0), 65536)
        self.assertEqual(result.locals[16:32], self.locals[16:32])

    def test_halving_is_signed_and_occurs_after_the_sweep(self):
        put(self.actor, 0, 0x40000)
        put(self.actor, 0x14, 0x100)
        struct.pack_into("<3i", self.actor, 0x40, -3, 17, 3)
        result = self.run_motion()
        self.assertEqual(result.sweep.velocity, (-3, 0, 3))
        self.assertEqual(struct.unpack_from("<3i", result.actor, 0x30), (-2, 0, 1))

    def test_animation_override_runs_the_source_animation_model(self):
        put(self.actor, 0xE8, 1, 2)
        result = self.run_motion()
        self.assertEqual(result.animation, 0)
        self.assertEqual(u16(result.actor, 0xE8), 0)
        self.assertEqual(u32(result.sprite, 0x40) & 0x100000, 0x100000)
        self.assertEqual(u16(result.sprite, 0x9E), 1)
        self.assertEqual(
            [s.name for s in result.stages][-3:],
            ["animation-call", "animation-after", "motion-after"],
        )

    def test_animation_inhibition_preserves_old_mode_after_selection(self):
        put(self.actor, 0, 0x2000000)
        put(self.actor, 0xE8, 1, 2)
        result = self.run_motion(read_resource=forbidden_read)
        self.assertIsNone(result.animation)
        self.assertEqual(u16(result.actor, 0xE8), 1)

    def test_special_terrain_selects_mode_six_and_sets_idle_flag_only_for_old_six(self):
        put(self.actor, 0, 0x2000000)
        for mode, flags in ((0, 0), (6, 0x1000)):
            put(self.actor, 0xE8, mode, 2)
            result = self.run_motion(component=fixture(attributes=(0x200000,)))
            self.assertEqual(result.stages[-1].mode, 6)
            self.assertEqual(u32(result.actor, 4) & 0x1000, flags)

    def test_jump_speed_uses_retained_mode_then_global_animation(self):
        put(self.actor, 0, 0x800 | 0x2000000)
        put(self.actor, 0xE8, 2, 2)
        put(self.sprite, 6, 1, 2)
        result = self.run_motion()
        self.assertEqual((u32(result.sprite, 0x18), result.stages[-1].mode), (4096 * 96, 3))
        put(self.sprite, 6, u16(self.sprite, 0x84), 2)
        self.assertEqual(u32(self.run_motion().sprite, 0x18), 0)
        put(self.sprite, 0x18, 1234)
        self.assertEqual(
            u32(self.run_motion(controls=replace(self.controls, jump_gate=1)).sprite, 0x18), 1234
        )

    def test_original_inputs_are_not_mutated_on_an_unreconstructed_path(self):
        put(self.actor, 0x104, 0, 2)
        put(self.descriptor, 0x58, 0)
        before = bytes(self.actor), bytes(self.sprite), bytes(self.descriptor)
        with self.assertRaisesRegex(ValueError, "ratio path"):
            self.run_motion()
        self.assertEqual((self.actor, self.sprite, self.descriptor), before)


class MotionPredicateAndBoundsTests(unittest.TestCase):
    def test_idle_predicate_source_gates_and_signed_layers(self):
        actor, *_ = invented_motion()
        self.assertEqual(idle_predicate(actor, 0, 1), 0)
        for offset, value, size in (
            (0, 0x800, 4),
            (0, 0x400000, 4),
            (0x14, 0x20000, 4),
            (0x14, 0x400000, 4),
            (0x30, 1, 4),
            (0x34, -1, 4),
            (0x38, 1, 4),
            (0x74, 0, 1),
        ):
            changed = bytearray(actor)
            put(changed, offset, value, size)
            self.assertEqual(idle_predicate(changed, 0, 1), -1)
        self.assertEqual(idle_predicate(actor, 1, 1), -1)
        self.assertEqual(idle_predicate(actor, 0, 0), -1)
        put(actor, 4, 7)
        for layer, expected in ((-1, 0), (0, -1), (1, -1), (2, -1), (3, 0), (32767, 0)):
            put(actor, 0x10, layer, 2)
            self.assertEqual(idle_predicate(actor, 0, 1), expected)

    def test_disabled_bounds_do_not_dereference_the_quad_pointer(self):
        actor, *_ = invented_motion()
        put(actor, 0x114, 0xFFFFFFFF)
        self.assertEqual(motion_bounds(actor, (1, 2, 3), forbidden_read), 0)

    def test_pointed_quad_accepts_edges_and_rejects_each_outside_side(self):
        actor, *_ = invented_motion()
        put(actor, 0x12C, 0x1000)
        put(actor, 0x114, 0x80020000)
        quad = struct.pack("<8h", 0, 0, 0, 16, 16, 16, 16, 0)
        read = reader(0x80020000, quad)
        for x, z, value in (
            (0, 8, 0),
            (16, 8, 0),
            (8, 0, 0),
            (8, 16, 0),
            (8, 8, 0),
            (-1, 8, -1),
            (17, 8, -1),
            (8, -1, -1),
            (8, 17, -1),
        ):
            struct.pack_into("<3i", actor, 0x20, x << 16, 0, z << 16)
            self.assertEqual(motion_bounds(actor, (0, 999, 0), read), value)
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            motion_bounds(actor, (0, 0, 0), lambda pointer, size: b"")

    def test_negative_z_packing_preserves_the_original_x_borrow(self):
        actor, *_ = invented_motion()
        put(actor, 0x12C, 0x1000)
        put(actor, 0x114, 0x80020000)
        struct.pack_into("<3i", actor, 0x20, 0, 0, -65536)
        quad = struct.pack("<8h", 0, -2, 0, 2, 2, 2, 2, -2)
        self.assertEqual(motion_bounds(actor, (0, 0, 0), reader(0x80020000, quad)), -1)

    def test_rejected_quad_stops_before_sweep_and_clears_additive_motion(self):
        actor, descriptor, sprite, resource, controls, tables = invented_motion()
        put(actor, 0x12C, 0x1000)
        put(actor, 0x114, 0x80020000)
        put(actor, 0x40, 100 << 16)
        result = active_motion(
            3,
            descriptor,
            actor,
            sprite,
            bytes(40),
            controls,
            fixture(),
            tables,
            reader(RESOURCE, resource),
            reader(0x80020000, struct.pack("<8h", 0, 0, 0, 16, 16, 16, 16, 0)),
        )
        self.assertEqual((result.stop_reason, result.sweep), ("bounds", None))
        self.assertEqual(result.actor[0x40:0x4C], bytes(12))


if __name__ == "__main__":
    unittest.main()
