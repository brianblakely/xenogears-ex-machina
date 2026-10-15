"""The listening test's analysis (tools/listening/analyse.py) on answers
written here: the geometry, every localization metric on answers with known
errors, the bootstrap, the model comparison and the coherence summary. No
person's answers are involved."""

import json
import random
import tempfile
import unittest
from pathlib import Path

from tools.listening import analyse

# The test's directions: elevations -30, 0, 30 and 60, azimuths every 30.
GRID = [(a * 30.0, e) for e in (-30.0, 0.0, 30.0, 60.0) for a in range(12)]


def mirrored(azimuth: float, elevation: float) -> tuple[float, float]:
    """The front/back mirror image: the same lateral angle, the polar angle
    reflected about the frontal plane."""
    return ((180.0 - azimuth) % 360.0, elevation)


class Geometry(unittest.TestCase):
    def test_interaural_coordinates_follow_the_renderers_convention(self):
        cases = {
            (0, 0): (0, 0),
            (180, 0): (0, 180),
            (0, 90): (0, 90),
            (0, -40): (0, -40),
            (180, 30): (0, 150),
        }
        for (azimuth, elevation), (lateral, polar) in cases.items():
            got = analyse.interaural(azimuth, elevation)
            self.assertAlmostEqual(got[0], lateral, places=6, msg=(azimuth, elevation))
            self.assertAlmostEqual(got[1], polar, places=6, msg=(azimuth, elevation))
        # On the interaural axis only the lateral angle means anything.
        self.assertAlmostEqual(analyse.interaural(90, 0)[0], 90, places=6)
        self.assertAlmostEqual(analyse.interaural(270, 0)[0], -90, places=6)
        # 60 to the right at 60 up is 25.7 degrees from the median plane,
        # 30 to the right at 60 up 14.5.
        self.assertAlmostEqual(analyse.interaural(60, 60)[0], 25.6589, places=3)
        self.assertAlmostEqual(analyse.interaural(30, 60)[0], 14.4775, places=3)

    def test_angle_differences_wrap_to_a_half_turn(self):
        self.assertEqual(analyse.wrap(350 - 10), -20)
        self.assertEqual(analyse.wrap(10 - 350), 20)
        self.assertEqual(analyse.wrap(-180), 180)
        self.assertEqual(analyse.wrap(540), 180)


class Localization(unittest.TestCase):
    def test_exact_answers_have_no_error_and_unit_elevation_gain(self):
        pairs = [(t, t) for t in GRID]
        s = analyse.summarize(pairs, 0, random.Random(1))
        self.assertEqual(s["n"], 48)
        self.assertAlmostEqual(s["lateral_rms"], 0.0, places=9)
        self.assertEqual(s["quadrant_error"], 0.0)
        self.assertAlmostEqual(s["local_polar_rms"], 0.0, places=9)
        self.assertEqual(s["front_back"], 0.0)
        self.assertEqual(s["up_down"], 0.0)
        self.assertAlmostEqual(s["elevation_gain"], 1.0, places=9)
        # Within 30 degrees of the median plane: 6 a ring at -30, 0 and 30,
        # all 12 at 60 (cos 60 halves the lateral angle).
        self.assertEqual(s["counts"]["sagittal"], 30)

    def test_front_back_mirror_images_are_all_confusions_and_keep_the_lateral_angle(self):
        pairs = [(t, mirrored(*t)) for t in GRID]
        s = analyse.summarize(pairs, 0, random.Random(1))
        self.assertAlmostEqual(s["lateral_rms"], 0.0, places=6)
        self.assertEqual(s["front_back"], 100.0)
        # Independently: a mirror image's polar error is 180 - 2 * polar,
        # a quadrant error when that exceeds 90.
        errors = []
        for t, _ in pairs:
            lateral, polar = analyse.interaural(*t)
            if abs(lateral) <= 30 + 1e-6:
                errors.append(analyse.wrap(180 - 2 * polar))
        quadrant = 100 * sum(abs(e) > 90 for e in errors) / len(errors)
        self.assertAlmostEqual(s["quadrant_error"], quadrant)
        self.assertGreater(quadrant, 0)
        self.assertEqual(s["up_down"], 0.0)

    def test_negated_elevations_are_up_down_confusions_with_gain_minus_one(self):
        pairs = [(t, (t[0], -t[1])) for t in GRID]
        s = analyse.summarize(pairs, 0, random.Random(1))
        self.assertEqual(s["up_down"], 100.0)
        self.assertEqual(s["counts"]["up_down"], 36)
        self.assertAlmostEqual(s["elevation_gain"], -1.0, places=9)

    def test_halved_elevations_give_gain_one_half(self):
        pairs = [(t, (t[0], t[1] / 2)) for t in GRID]
        self.assertAlmostEqual(analyse.elevation_gain(pairs)["gain"], 0.5, places=9)

    def test_answers_further_out_have_a_positive_lateral_bias(self):
        pairs = [((90.0 + d, 0.0), (90.0 + d / 2, 0.0)) for d in (-60.0, -30.0, 30.0, 60.0)]
        result = analyse.lateral_errors(pairs)
        self.assertGreater(result["bias"], 0)
        inward = analyse.lateral_errors([((60.0, 0.0), (30.0, 0.0))])
        self.assertLess(inward["bias"], 0)

    def test_the_bootstrap_is_seeded_and_covers_the_estimate(self):
        values = [float(v) for v in range(100)]
        a = analyse.bootstrap(values, analyse.mean, 500, random.Random(7))
        b = analyse.bootstrap(values, analyse.mean, 500, random.Random(7))
        self.assertEqual(a, b)
        self.assertLess(a[0], 49.5)
        self.assertGreater(a[1], 49.5)
        self.assertIsNone(analyse.bootstrap([], analyse.mean, 500, random.Random(7)))
        self.assertIsNone(analyse.bootstrap(values, analyse.mean, 0, random.Random(7)))


def manifest_and_results(directory: Path) -> None:
    """A small localization and coherence manifest, results answering them
    with known errors, and model predictions."""
    generated = {"tool": "test", "seed": 1}
    stimuli = []
    for condition in ("rendered", "kemar", "stereo"):
        for azimuth, elevation in GRID:
            stimuli.append(
                {
                    "id": f"{condition}-{azimuth:.0f}-{elevation:.0f}",
                    "condition": condition,
                    "motion": "static",
                    "start": [azimuth, elevation],
                    "end": [azimuth, elevation],
                    "file": "x.wav",
                }
            )
    stimuli.append(
        {
            "id": "move-h",
            "condition": "rendered",
            "motion": "moving",
            "path": "h000cw",
            "start": [330.0, 0.0],
            "end": [30.0, 0.0],
            "file": "x.wav",
        }
    )
    stimuli.append(
        {
            "id": "move-v",
            "condition": "stereo",
            "motion": "moving",
            "path": "v090up",
            "start": [90.0, -30.0],
            "end": [90.0, 30.0],
            "file": "x.wav",
        }
    )
    stimuli.append(
        {
            "id": "catch-left",
            "condition": "catch",
            "motion": "static",
            "side": "left",
            "start": [270.0, 0.0],
            "end": [270.0, 0.0],
            "file": "x.wav",
        }
    )
    stimuli.append(
        {
            "id": "train",
            "condition": "rendered",
            "motion": "static",
            "training": True,
            "start": [0.0, 0.0],
            "end": [0.0, 0.0],
            "file": "x.wav",
        }
    )
    (directory / "localization.json").write_text(
        json.dumps(
            {
                "kind": "xem-listening-localization",
                "version": 1,
                "generated": generated,
                "stimuli": stimuli,
                "sessions": [],
            }
        )
    )
    trials = []
    for s in stimuli:
        if s["motion"] == "moving":
            # The vertical one is answered the wrong way.
            end, start = s["end"], s["start"]
            if s["path"].startswith("v"):
                start, end = end, start
            response = {
                "start": {"azimuth": start[0], "elevation": start[1]},
                "end": {"azimuth": end[0], "elevation": end[1]},
            }
        elif s["condition"] == "stereo":
            # The control: everything heard ahead.
            response = {"azimuth": 0.0, "elevation": 0.0}
        elif s["condition"] == "kemar":
            # The raw set: the mirror image every time.
            a, e = mirrored(*s["start"])
            response = {"azimuth": a, "elevation": e}
        else:
            response = {"azimuth": s["start"][0], "elevation": s["start"][1]}
        trials.append(
            {
                "trial": s["id"],
                "stimulus": s["id"],
                "training": bool(s.get("training")),
                "response": response,
            }
        )
    results = directory / "results"
    results.mkdir()
    loc = {
        "kind": "xem-listening-results",
        "part": "localization",
        "session": "L1",
        "listener": "test",
        "file": "test-L1.json",
        "complete": True,
        "manifest": {"generated": generated},
        "calibrations": [{"level_set": True, "left_right_ok": True, "polarity_ok": True}],
        "trials": trials,
    }
    (results / "test-L1.json").write_text(json.dumps(loc))
    # Coherence: two sessions rating one excerpt; immersion-35 and -70
    # identical samples.
    coh_generated = {"tool": "test", "seed": 2}
    conditions = [
        "reference",
        "anchor",
        "immersion-0",
        "immersion-35",
        "immersion-70",
        "immersion-100",
    ]
    iacc = {
        "reference": 0.9,
        "anchor": 0.2,
        "immersion-0": 0.7,
        "immersion-35": 0.5,
        "immersion-70": 0.5,
        "immersion-100": 0.3,
    }
    coh = {
        "kind": "xem-listening-coherence",
        "version": 1,
        "generated": coh_generated,
        "attributes": [
            {"id": "coherence", "reference_top": True},
            {"id": "envelopment", "reference_top": False},
        ],
        "conditions": {c: c for c in conditions},
        "excerpts": [
            {
                "id": "song",
                "identical": [["immersion-35", "immersion-70"]],
                "conditions": {c: {"iacc": [iacc[c]] * 7} for c in conditions},
            }
        ],
        "sessions": [],
    }
    (directory / "coherence.json").write_text(json.dumps(coh))
    scores = {
        "coherence": {
            "reference": 100,
            "anchor": 10,
            "immersion-0": 90,
            "immersion-35": 80,
            "immersion-70": 80,
            "immersion-100": 60,
        },
        "envelopment": {
            "reference": 10,
            "anchor": 90,
            "immersion-0": 30,
            "immersion-35": 50,
            "immersion-70": 50,
            "immersion-100": 70,
        },
    }
    for session, shift in (("C1", 0), ("C2", 4)):
        ratings = [
            {
                "trial": f"{session}-{a}",
                "excerpt": "song",
                "attribute": a,
                "ratings": {c: max(0, v - shift) for c, v in scores[a].items()},
            }
            for a in scores
        ]
        (results / f"test-{session}.json").write_text(
            json.dumps(
                {
                    "kind": "xem-listening-results",
                    "part": "coherence",
                    "session": session,
                    "listener": "test",
                    "file": f"test-{session}.json",
                    "complete": True,
                    "manifest": {"generated": coh_generated},
                    "calibrations": [],
                    "ratings": ratings,
                }
            )
        )
    models = {
        "summary": {
            "sagittal": {
                name: {
                    "mean_qe": {"median": 30.0, "min": 20.0, "max": 40.0},
                    "mean_qe_kemar": 26.0,
                    "mean_pe": {"median": 40.0, "min": 35.0, "max": 45.0},
                    "mean_pe_kemar": 36.0,
                    "mean_fb": {"median": 40.0, "min": 30.0, "max": 50.0},
                    "mean_fb_kemar": 33.0,
                }
                for name in ("rendered_measured", "kemar_raw", "stereo_measured")
            },
            "lateral": {
                name: {
                    "rms": {"median": 8.0, "min": 5.0, "max": 13.0},
                    "rms_kemar": 3.0,
                    "bias": {"median": -3.0, "min": -9.0, "max": 7.0},
                    "bias_kemar": 2.0,
                }
                for name in ("rendered_measured", "kemar_raw", "stereo_measured")
            },
        }
    }
    (directory / "models.json").write_text(json.dumps(models))


class Analysis(unittest.TestCase):
    def test_the_whole_analysis_reads_known_answers_back(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            manifest_and_results(directory)
            result = analyse.analyse(
                directory, directory / "results", directory / "models.json", 200, 1
            )
        self.assertEqual(result["notes"], [])
        static = result["localization"]["static"]
        self.assertEqual(static["rendered"]["n"], 48)
        self.assertAlmostEqual(static["rendered"]["lateral_rms"], 0.0, places=6)
        self.assertEqual(static["rendered"]["front_back"], 0.0)
        self.assertEqual(static["kemar"]["front_back"], 100.0)
        self.assertAlmostEqual(static["kemar"]["lateral_rms"], 0.0, places=6)
        # The control answered ahead and level: no up/down confusion, gain 0.
        self.assertEqual(static["stereo"]["up_down"], 0.0)
        self.assertAlmostEqual(static["stereo"]["elevation_gain"], 0.0, places=9)
        moving = result["localization"]["moving"]
        self.assertEqual(moving["rendered"]["right_way"], 100.0)
        self.assertEqual(moving["stereo"]["right_way"], 0.0)
        self.assertEqual(result["localization"]["catch"]["correct"], 100.0)
        self.assertEqual(result["localization"]["training"]["n"], 1)
        rows = {(r["condition"], r["metric"]): r for r in result["models"]}
        self.assertEqual(rows[("rendered", "quadrant_error")]["pool"]["median"], 30.0)
        self.assertFalse(rows[("rendered", "quadrant_error")]["within_pool_range"])
        self.assertTrue(rows[("kemar", "front_back")]["within_pool_range"] is False)
        coherence = result["coherence"]
        self.assertEqual(coherence["n"], 4)
        self.assertEqual(coherence["attributes"]["coherence"]["reference"]["mean"], 98.0)
        self.assertEqual(coherence["attributes"]["coherence"]["anchor"]["minus_reference"], -90.0)
        self.assertTrue(coherence["post_screening"]["passed"])
        self.assertEqual(coherence["post_screening"]["trials"], 2)
        self.assertEqual(coherence["repeats"]["pairs"], 12)
        self.assertAlmostEqual(coherence["repeats"]["mean_absolute_change"], 4.0)
        self.assertEqual(coherence["identical"]["largest_spread"], 0)
        # The two sessions' ratings tie in no place the correlations do.
        self.assertGreater(coherence["proxy"]["spearman"], 0.95)
        text = analyse.report(result)
        self.assertIn("one listener", text)
        self.assertIn("post-screening", text)

    def test_answers_to_other_stimuli_are_left_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            manifest_and_results(directory)
            path = directory / "results" / "test-L1.json"
            data = json.loads(path.read_text())
            data["manifest"]["generated"]["seed"] = 99
            path.write_text(json.dumps(data))
            result = analyse.analyse(directory, directory / "results", None, 0, 1)
        self.assertNotIn("localization", result)
        self.assertTrue(any("other stimuli" in note for note in result["notes"]))


if __name__ == "__main__":
    unittest.main()
