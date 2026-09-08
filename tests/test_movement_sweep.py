"""Authored sweep scenarios with artificial geometry and coefficient tables."""

import struct
import unittest
from dataclasses import replace

from tests.test_collision_query import fixture
from tools.analysis.collision_query import QueryActor
from tools.analysis.movement_sweep import (
    SweepActor,
    SweepTables,
    movement_sweep,
    uses_ordinary_sweep,
)


class MovementSweepTests(unittest.TestCase):
    actor = SweepActor(QueryActor(0, 0, 0, 0, (4 << 16, 7 << 16, 4 << 16)), 0, 255, 7, 5)
    tables = SweepTables(
        (4096,) * 192, (4096,) * 192, tuple(i // 2 for i in range(1025)), bytes(16384)
    )
    edge = ((0, 7, 0), (16, 7, 0))

    def run_sweep(
        self,
        *,
        actor=None,
        tables=None,
        component=None,
        velocity=(65536, 123, 0),
        ordinary=True,
        mode=0,
        direction=0,
    ):
        return movement_sweep(
            fixture() if component is None else component,
            tables or self.tables,
            actor or self.actor,
            velocity,
            self.edge,
            direction,
            ordinary=ordinary,
            collision_mode=mode,
            attribute_control=0,
        )

    def test_ordinary_probe_order_slope_requery_and_floor_commit(self):
        result = self.run_sweep()
        self.assertEqual(
            [q.stage for q in result.queries],
            ["probe-left", "probe-right", "probe-center", "floor", "slope-floor"],
        )
        self.assertTrue(all(q.candidate[1] is None for q in result.queries[:3]))
        self.assertEqual((result.value, result.velocity, result.actor.floor), (0, (65536, 0, 0), 7))
        self.assertEqual(result.actor.query.flags, 0x04000000)
        self.assertEqual(result.actor.query.position, self.actor.query.position)

    def test_special_probe_order_does_not_add_the_ordinary_floor_flag(self):
        actor = replace(self.actor, query=replace(self.actor.query, flags=0x800))
        result = self.run_sweep(actor=actor, ordinary=False)
        self.assertEqual(
            [q.stage for q in result.queries],
            ["probe-center", "probe-left", "probe-right", "floor"],
        )
        self.assertEqual(
            (result.value, result.velocity, result.actor.query.flags), (0, (65536, 0, 0), 0x800)
        )
        self.assertIsNone(result.slope)

    def test_special_floor_rejection_preserves_velocity_and_prior_floor(self):
        actor = replace(
            self.actor,
            query=replace(self.actor.query, position=(4 << 16, 10 << 16, 4 << 16)),
            floor=99,
        )
        result = self.run_sweep(actor=actor, ordinary=False)
        self.assertEqual(
            (result.value, result.velocity, result.actor), (-1, (65536, 123, 0), actor)
        )
        allowed = self.run_sweep(actor=actor, ordinary=False, mode=1)
        self.assertEqual(
            (allowed.value, allowed.velocity[1], allowed.actor.floor), (0, -3 << 16, 7)
        )

    def test_special_forced_floor_replaces_the_queried_height(self):
        actor = replace(self.actor, query=replace(self.actor.query, flags=0x40000))
        result = self.run_sweep(actor=actor, ordinary=False)
        self.assertEqual(result.queries[-1].result.point[1], 7)
        self.assertEqual((result.value, result.actor.floor, result.velocity[1]), (0, 5, -2 << 16))

    def test_failed_floor_query_keeps_input_velocity_after_edge_projection(self):
        component = fixture(attributes=(0x800000,))
        result = self.run_sweep(component=component)
        self.assertEqual([q.stage for q in result.queries], ["probe-left", "floor"])
        self.assertIsNotNone(result.projection)
        self.assertEqual(
            (result.value, result.velocity, result.actor), (-1, (65536, 123, 0), self.actor)
        )
        self.assertEqual(result.edge, self.edge)

    def test_obstructed_probe_can_stop_then_commit_a_valid_stationary_floor(self):
        tables = replace(self.tables, trigonometry=struct.pack("<hh", 0, 4096) * 4096)
        actor = replace(
            self.actor, query=replace(self.actor.query, position=(15 << 16, 7 << 16, 0))
        )
        result = self.run_sweep(actor=actor, tables=tables, velocity=(0, 123, 0), direction=0xE00)
        self.assertEqual(result.projection.branch, "stop")
        self.assertEqual([q.stage for q in result.queries], ["probe-left", "floor", "slope-floor"])
        self.assertEqual((result.value, result.velocity, result.actor.floor), (0, (0, 0, 0), 7))

    def test_failed_slope_requery_keeps_flags_and_velocity_but_preserves_edge_write(self):
        tables = replace(self.tables, reciprocal=(8192,) * 192, square_root=(8192,) * 192)
        actor = replace(
            self.actor, query=replace(self.actor.query, position=(10 << 16, 7 << 16, 4 << 16))
        )
        result = self.run_sweep(actor=actor, tables=tables)
        self.assertEqual(result.queries[-1].stage, "slope-floor")
        self.assertEqual(
            (result.value, result.actor, result.velocity), (-1, actor, (65536, 123, 0))
        )
        self.assertEqual(result.edge, ((0, 7, 16), (16, 7, 0)))

    def test_main_caller_selects_special_for_flags_link_or_global_mode(self):
        self.assertTrue(uses_ordinary_sweep(self.actor, 0))
        for flags in (0x800, 0x1000, 0x40000):
            with self.subTest(flags=flags):
                actor = replace(self.actor, query=replace(self.actor.query, flags=flags))
                self.assertFalse(uses_ordinary_sweep(actor, 0))
        self.assertFalse(uses_ordinary_sweep(replace(self.actor, linked_actor=5), 0))
        self.assertFalse(uses_ordinary_sweep(self.actor, 1))


if __name__ == "__main__":
    unittest.main()
