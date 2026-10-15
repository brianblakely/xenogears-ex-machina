#!/usr/bin/env python3
"""Analyse the listening test's answers (tools/listening/README.md).

    python3 tools/listening/analyse.py [--stimuli DIR] [--results DIR]
        [--models FILE] [--json FILE] [--bootstrap N] [--seed N]

Reads every results file the page saved (`.local/listening/results` by
default) with the manifests that planned them (`localization.json`,
`coherence.json` in `.local/listening`), prints a report and writes it as
JSON (`analysis.json` beside the results by default). Python's standard
library only.

Localization, per condition (the renderer, the KEMAR set's raw responses,
the Stereo pan control), over the static test trials (training and catch
trials apart), in the interaural-polar coordinates of the model evaluation
(tools/binaural_models/evaluate.m): the lateral RMS error and bias (bias
positive when answers lie further out than targets at least 10 degrees to a
side); for targets within 30 degrees of the median plane, the quadrant error
rate (polar error over 90 degrees) and the local polar RMS error (over the
rest); front/back confusions (targets at least 30 degrees from the frontal
plane answered on its other side); up/down confusions (targets at least 30
degrees from the horizontal plane answered on its other side); the elevation
gain (the slope of answered on target elevation). Each with a 95% bootstrap
interval over trials (percentiles; seeded). Moving trials: start and end
errors, and whether the answered movement went the right way. Catch trials:
the share answered on the side that sounded. The calibrated model
listeners' predictions (`build/binaural/models.json`, from evaluate.m) are
set beside them: the pool's median and range and the KEMAR head; they are
for every measured direction, the person's for the trials' directions.

Coherence (MUSHRA-style): every rating by attribute and condition, mean and
95% bootstrap interval, by excerpt too; the post-screening of ITU-R
BS.1534-3 (the hidden reference rated 90 or more in at least 85% of the
trials of attributes the reference tops); how far each repeat's rating moved;
ratings of identical conditions; and how envelopment ratings follow the
interaural cross-correlation the generator measured (a proxy, not a model).

One person's answers: a result about that person with those headphones,
not about listeners at large.
"""

from __future__ import annotations

import argparse
import json
import math
import random
import statistics
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CONDITIONS = ("rendered", "kemar", "stereo")
MODEL_SETS = {"rendered": "rendered_measured", "kemar": "kemar_raw", "stereo": "stereo_measured"}
LABELS = {"rendered": "Binaural renderer", "kemar": "raw KEMAR", "stereo": "Stereo pan (control)"}
# Directions are given to a degree: limits hold within this (30 degrees to
# the side is 30.000000000000004 after the sine and arcsine).
EPS = 1e-6
# Bands of the interaural cross-correlation the envelopment proxy reads:
# 500 Hz to 4 kHz (measure::OCTAVES' indices 2 to 5).
PROXY_BANDS = slice(2, 6)


# ---------------------------------------------------------------- geometry


def interaural(azimuth: float, elevation: float) -> tuple[float, float]:
    """A direction's lateral angle (positive to the right) and polar angle
    (0 ahead, 90 above, 180 behind; -90 to 270), degrees, from azimuth
    (clockwise from ahead) and elevation."""
    a, e = math.radians(azimuth), math.radians(elevation)
    x, y, z = math.cos(e) * math.cos(a), math.cos(e) * math.sin(a), math.sin(e)
    lateral = math.degrees(math.asin(max(-1.0, min(1.0, y))))
    polar = math.degrees(math.atan2(z, x))
    if polar < -90:
        polar += 360
    return lateral, polar


def wrap(degrees: float) -> float:
    """An angle difference in (-180, 180]."""
    d = (degrees + 180.0) % 360.0 - 180.0
    return 180.0 if d == -180.0 else d


def rms(values: list[float]) -> float | None:
    return math.sqrt(sum(v * v for v in values) / len(values)) if values else None


def mean(values: list[float]) -> float | None:
    return sum(values) / len(values) if values else None


# A pair is (target, response), each (azimuth, elevation).


def lateral_errors(pairs) -> dict:
    errors, outward = [], []
    for target, response in pairs:
        lt, rl = interaural(*target)[0], interaural(*response)[0]
        errors.append(rl - lt)
        if abs(lt) >= 10 - EPS:
            outward.append(math.copysign(1.0, lt) * (rl - lt))
    return {"rms": rms(errors), "bias": mean(outward), "n": len(errors)}


def sagittal(pairs) -> dict:
    """Quadrant error rate (percent) and local polar RMS error (degrees),
    for targets within 30 degrees of the median plane."""
    errors = []
    for target, response in pairs:
        lt, pt = interaural(*target)
        if abs(lt) <= 30 + EPS:
            errors.append(wrap(interaural(*response)[1] - pt))
    local = [e for e in errors if abs(e) <= 90]
    return {
        "quadrant_error": 100.0 * (len(errors) - len(local)) / len(errors) if errors else None,
        "local_polar_rms": rms(local),
        "n": len(errors),
    }


def front_back(pairs) -> dict:
    """Of targets within 30 degrees of the median plane and at least 30
    from the frontal plane (polar up to 60 or from 120), the share answered
    on its other side (evaluate.m's front_back, for answers)."""
    confused = []
    for target, response in pairs:
        lt, pt = interaural(*target)
        if abs(lt) > 30 + EPS:
            continue
        pr = interaural(*response)[1]
        if pt <= 60 + EPS:
            confused.append(pr > 90)
        elif pt >= 120 - EPS:
            confused.append(pr < 90)
    return {"rate": 100.0 * sum(confused) / len(confused) if confused else None, "n": len(confused)}


def up_down(pairs) -> dict:
    """Of targets at least 30 degrees above or below the horizontal plane,
    the share answered on its other side."""
    confused = []
    for target, response in pairs:
        if target[1] >= 30 - EPS:
            confused.append(response[1] < 0)
        elif target[1] <= -30 + EPS:
            confused.append(response[1] > 0)
    return {"rate": 100.0 * sum(confused) / len(confused) if confused else None, "n": len(confused)}


def elevation_gain(pairs) -> dict:
    """The least-squares line of answered on target elevation."""
    xs = [t[1] for t, _ in pairs]
    ys = [r[1] for _, r in pairs]
    if len(xs) < 3 or len(set(xs)) < 2:
        return {"gain": None, "bias": None, "r2": None, "n": len(xs)}
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys, strict=True))
    syy = sum((y - my) ** 2 for y in ys)
    gain = sxy / sxx
    return {
        "gain": gain,
        "bias": mean([y - x for x, y in zip(xs, ys, strict=True)]),
        "r2": (sxy * sxy) / (sxx * syy) if syy > 0 else None,
        "n": len(xs),
    }


METRICS = {
    "lateral_rms": lambda p: lateral_errors(p)["rms"],
    "lateral_bias": lambda p: lateral_errors(p)["bias"],
    "quadrant_error": lambda p: sagittal(p)["quadrant_error"],
    "local_polar_rms": lambda p: sagittal(p)["local_polar_rms"],
    "front_back": lambda p: front_back(p)["rate"],
    "up_down": lambda p: up_down(p)["rate"],
    "elevation_gain": lambda p: elevation_gain(p)["gain"],
}


def bootstrap(items: list, statistic, draws: int, rng: random.Random) -> list[float] | None:
    """The 2.5th and 97.5th percentiles of `statistic` over resamples of
    `items` (with replacement); None when it has no value."""
    if not items or draws <= 0:
        return None
    values = []
    for _ in range(draws):
        value = statistic([items[rng.randrange(len(items))] for _ in items])
        if value is not None:
            values.append(value)
    if len(values) < draws / 2:
        return None
    values.sort()
    pick = lambda q: values[min(len(values) - 1, max(0, int(round(q * (len(values) - 1)))))]  # noqa: E731
    return [pick(0.025), pick(0.975)]


def summarize(pairs, draws: int, rng: random.Random) -> dict:
    """Every static metric of `pairs`, each with its interval."""
    out = {"n": len(pairs)}
    for name, statistic in METRICS.items():
        out[name] = statistic(pairs)
        out[name + "_ci"] = bootstrap(pairs, statistic, draws, rng)
    out["counts"] = {
        "sagittal": sagittal(pairs)["n"],
        "front_back": front_back(pairs)["n"],
        "up_down": up_down(pairs)["n"],
    }
    return out


# ---------------------------------------------------------------- inputs


def load_results(directory: Path) -> list[dict]:
    results = []
    for path in sorted(directory.glob("*.json")):
        try:
            data = json.loads(path.read_text())
        except ValueError as error:
            print(f"warning: {path.name}: not JSON ({error})", file=sys.stderr)
            continue
        if isinstance(data, dict) and data.get("kind") == "xem-listening-results":
            data["_path"] = path.name
            results.append(data)
    return results


def load_json(path: Path) -> dict | None:
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return None


def check_manifest(result: dict, manifest: dict | None, notes: list[str]) -> bool:
    if manifest is None:
        notes.append(f"{result['_path']}: its manifest is missing; left out")
        return False
    if result.get("manifest", {}).get("generated") != manifest.get("generated"):
        notes.append(
            f"{result['_path']}: answered against other stimuli than the current manifest's"
            " (another commit or seed); left out"
        )
        return False
    return True


# ---------------------------------------------------------------- localization


def localization(
    results: list[dict], manifest: dict, draws: int, seed: int, notes: list[str]
) -> dict:
    stimuli = {s["id"]: s for s in manifest["stimuli"]}
    rng = random.Random(seed)
    static = {c: [] for c in CONDITIONS}
    by_session: dict[str, dict[str, list]] = {}
    moving = {"rendered": [], "stereo": []}
    catch, training = [], []
    for result in results:
        for trial in result.get("trials", []):
            stimulus = stimuli.get(trial["stimulus"])
            if stimulus is None:
                notes.append(f"{result['_path']}: unknown stimulus {trial['stimulus']}")
                continue
            response = trial["response"]
            if trial.get("training"):
                training.append(
                    (
                        (stimulus["start"][0], stimulus["start"][1]),
                        (response["azimuth"], response["elevation"]),
                    )
                )
            elif stimulus["condition"] == "catch":
                catch.append((stimulus["side"], response))
            elif stimulus["motion"] == "moving":
                moving[stimulus["condition"]].append((stimulus, response))
            else:
                pair = (
                    (stimulus["start"][0], stimulus["start"][1]),
                    (response["azimuth"], response["elevation"]),
                )
                static[stimulus["condition"]].append(pair)
                by_session.setdefault(result["session"], {c: [] for c in CONDITIONS})[
                    stimulus["condition"]
                ].append(pair)
    out = {
        "static": {c: summarize(static[c], draws, rng) for c in CONDITIONS},
        "sessions": {
            name: {
                c: {k: summarize(p, 0, rng)[k] for k in ("n", *METRICS)}
                for c, p in conditions.items()
            }
            for name, conditions in sorted(by_session.items())
        },
        "moving": {},
        "catch": None,
        "training": {
            "n": len(training),
            **({k: f(training) for k, f in METRICS.items()} if training else {}),
        },
    }
    for condition, items in moving.items():
        starts = [
            ((s["start"][0], s["start"][1]), (r["start"]["azimuth"], r["start"]["elevation"]))
            for s, r in items
        ]
        ends = [
            ((s["end"][0], s["end"][1]), (r["end"]["azimuth"], r["end"]["elevation"]))
            for s, r in items
        ]

        def right_way(chunk):
            if not chunk:
                return None
            good = 0
            for s, r in chunk:
                if s["path"].startswith("h"):
                    wanted = wrap(s["end"][0] - s["start"][0])
                    got = wrap(r["end"]["azimuth"] - r["start"]["azimuth"])
                else:
                    wanted = s["end"][1] - s["start"][1]
                    got = r["end"]["elevation"] - r["start"]["elevation"]
                good += got != 0 and math.copysign(1, got) == math.copysign(1, wanted)
            return 100.0 * good / len(chunk)

        out["moving"][condition] = {
            "n": len(items),
            "start": {k: lateral_errors(starts)[k] for k in ("rms", "bias")}
            | {"polar": sagittal(starts)},
            "end": {k: lateral_errors(ends)[k] for k in ("rms", "bias")}
            | {"polar": sagittal(ends)},
            "right_way": right_way(items),
            "right_way_ci": bootstrap(items, right_way, draws, rng),
        }

    def catch_rate(chunk):
        if not chunk:
            return None
        good = 0
        for side, response in chunk:
            lateral = interaural(response["azimuth"], response["elevation"])[0]
            good += (lateral >= 30 - EPS) if side == "right" else (lateral <= -30 + EPS)
        return 100.0 * good / len(chunk)

    out["catch"] = {
        "n": len(catch),
        "correct": catch_rate(catch),
        "correct_ci": bootstrap(catch, catch_rate, draws, rng),
    }
    if catch and catch_rate(catch) < 90:
        notes.append(
            f"catch trials: {catch_rate(catch):.0f}% answered on the side that sounded"
            " (under 90%: attention or headphones)"
        )
    return out


def spread(value):
    """A models.json number or {median, min, max, ...}."""
    if isinstance(value, dict):
        return {k: value.get(k) for k in ("median", "min", "max", "q25", "q75")}
    return {"median": value}


def compare_models(static: dict, models: dict | None) -> list[dict]:
    """The person's static metrics beside the model listeners'."""
    if not models:
        return []
    summary = models.get("summary", {})
    rows = []
    for condition in CONDITIONS:
        name = MODEL_SETS[condition]
        person = static[condition]
        sag = summary.get("sagittal", {}).get(name, {})
        lat = summary.get("lateral", {}).get(name, {})
        for metric, model, kemar in (
            ("quadrant_error", sag.get("mean_qe"), sag.get("mean_qe_kemar")),
            ("local_polar_rms", sag.get("mean_pe"), sag.get("mean_pe_kemar")),
            ("front_back", sag.get("mean_fb"), sag.get("mean_fb_kemar")),
            ("lateral_rms", lat.get("rms"), lat.get("rms_kemar")),
            ("lateral_bias", lat.get("bias"), lat.get("bias_kemar")),
        ):
            if model is None:
                continue
            pool = spread(model)
            value = person.get(metric)
            inside = None
            if value is not None and pool.get("min") is not None and pool.get("max") is not None:
                inside = pool["min"] <= value <= pool["max"]
            rows.append(
                {
                    "condition": condition,
                    "metric": metric,
                    "person": value,
                    "person_ci": person.get(metric + "_ci"),
                    "pool": pool,
                    "kemar_listener": kemar,
                    "within_pool_range": inside,
                }
            )
    return rows


# ---------------------------------------------------------------- coherence


def pearson(xs: list[float], ys: list[float]) -> float | None:
    if len(xs) < 3:
        return None
    mx, my = statistics.fmean(xs), statistics.fmean(ys)
    sx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    sy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if sx == 0 or sy == 0:
        return None
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys, strict=True)) / (sx * sy)


def ranks(values: list[float]) -> list[float]:
    order = sorted(range(len(values)), key=lambda i: values[i])
    out = [0.0] * len(values)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        for k in range(i, j + 1):
            out[order[k]] = (i + j) / 2.0
        i = j + 1
    return out


def coherence(results: list[dict], manifest: dict, draws: int, seed: int, notes: list[str]) -> dict:
    rng = random.Random(seed + 1)
    excerpts = {e["id"]: e for e in manifest["excerpts"]}
    attributes = {a["id"]: a for a in manifest["attributes"]}
    conditions = list(manifest["conditions"])
    ratings = [r for result in results for r in result.get("ratings", [])]
    out = {
        "n": len(ratings),
        "attributes": {},
        "excerpts": {},
        "post_screening": None,
        "repeats": None,
        "identical": None,
        "proxy": None,
    }
    for attribute in attributes:
        rows = [r for r in ratings if r["attribute"] == attribute]
        out["attributes"][attribute] = {}
        for condition in conditions:
            values = [r["ratings"][condition] for r in rows if condition in r["ratings"]]
            if not values:
                continue
            out["attributes"][attribute][condition] = {
                "mean": mean(values),
                "median": statistics.median(values),
                "n": len(values),
                "ci": bootstrap(values, mean, draws, rng),
                "minus_reference": mean(
                    [
                        r["ratings"][condition] - r["ratings"]["reference"]
                        for r in rows
                        if condition in r["ratings"]
                    ]
                ),
            }
    for excerpt in excerpts:
        rows = [r for r in ratings if r["excerpt"] == excerpt]
        if rows:
            out["excerpts"][excerpt] = {
                attribute: {
                    c: mean(
                        [
                            r["ratings"][c]
                            for r in rows
                            if r["attribute"] == attribute and c in r["ratings"]
                        ]
                    )
                    for c in conditions
                }
                for attribute in attributes
                if any(r["attribute"] == attribute for r in rows)
            }
    # Post-screening (BS.1534-3): the hidden reference at 90 or more in at
    # least 85% of the trials whose attribute the reference tops.
    topped = [r for r in ratings if attributes.get(r["attribute"], {}).get("reference_top")]
    if topped:
        high = sum(r["ratings"]["reference"] >= 90 for r in topped)
        share = 100.0 * high / len(topped)
        anchor_below = sum(
            r["ratings"]["anchor"] < r["ratings"]["reference"]
            for r in topped
            if "anchor" in r["ratings"]
        )
        out["post_screening"] = {
            "trials": len(topped),
            "reference_at_least_90": high,
            "share": share,
            "passed": share >= 85.0,
            "anchor_below_reference": anchor_below,
            "rule": "ITU-R BS.1534-3: the hidden reference rated 90 or more in at least 85% of"
            " the trials (attributes the reference tops: "
            + ", ".join(a for a in attributes if attributes[a].get("reference_top"))
            + ")",
        }
        if share < 85.0:
            notes.append(
                f"post-screening: the hidden reference was rated 90 or more in {share:.0f}%"
                " of the trials it tops (under 85%)"
            )
    # Repeats: the same excerpt, attribute and condition in two sessions.
    groups: dict[tuple, list[float]] = {}
    for r in ratings:
        for condition, value in r["ratings"].items():
            groups.setdefault((r["excerpt"], r["attribute"], condition), []).append(value)
    twice = [v for v in groups.values() if len(v) >= 2]
    if twice:
        out["repeats"] = {
            "pairs": len(twice),
            "mean_absolute_change": mean([abs(v[1] - v[0]) for v in twice]),
            "correlation": pearson([v[0] for v in twice], [v[1] for v in twice]),
        }
    # Identical conditions (the generator found the same samples) rated in
    # one trial: their ratings should agree.
    same = []
    for r in ratings:
        for group in excerpts.get(r["excerpt"], {}).get("identical", []):
            values = [r["ratings"][c] for c in group if c in r["ratings"]]
            if len(values) >= 2:
                same.append(max(values) - min(values))
    if same:
        out["identical"] = {
            "trials": len(same),
            "mean_spread": mean(same),
            "largest_spread": max(same),
        }
    # The envelopment proxy: ratings against how little the ears correlate
    # from 500 Hz to 4 kHz (Spearman).
    xs, ys = [], []
    for r in ratings:
        if r["attribute"] != "envelopment":
            continue
        for condition, value in r["ratings"].items():
            iacc = (
                excerpts.get(r["excerpt"], {}).get("conditions", {}).get(condition, {}).get("iacc")
            )
            if iacc:
                xs.append(1.0 - mean(iacc[PROXY_BANDS]))
                ys.append(value)
    if xs:
        out["proxy"] = {
            "what": "Spearman correlation of envelopment ratings with 1 - IACC (500 Hz to 4 kHz)",
            "n": len(xs),
            "spearman": pearson(ranks(xs), ranks(ys)),
        }
    return out


# ---------------------------------------------------------------- report


def number(value, digits=1, unit=""):
    if value is None:
        return "-"
    # No "-0.0" for a value that rounds to zero.
    return f"{(0.0 if abs(value) < 0.5 * 10**-digits else value):.{digits}f}{unit}"


def interval(ci, digits=1):
    return "" if not ci else f" [{number(ci[0], digits)}, {number(ci[1], digits)}]"


def stat(summary: dict, name: str, digits=1) -> str:
    """A metric and its interval, as printed."""
    return number(summary.get(name), digits) + interval(summary.get(name + "_ci"), digits)


def report(analysis: dict) -> str:
    lines = [
        "Listening test analysis (one listener: about that person and those headphones,"
        " not listeners at large)",
        "",
    ]
    for note in analysis["notes"]:
        lines.append(f"note: {note}")
    if analysis["notes"]:
        lines.append("")
    for c in analysis["calibration"]:
        sides = "right" if c["left_right_ok"] else "MISTAKEN"
        polarity = "right" if c["polarity_ok"] else "INVERTED?"
        lines.append(
            f"{c['file']}: level set {c['level_set']}, left/right {sides}, polarity {polarity}"
        )
    loc = analysis.get("localization")
    if loc:
        lines += ["", "Localization, static test trials (95% bootstrap intervals):"]
        for condition in CONDITIONS:
            s = loc["static"][condition]
            if not s["n"]:
                continue
            counts = s["counts"]
            lines += [
                f"  {LABELS[condition]} ({s['n']} trials):",
                f"    lateral error {stat(s, 'lateral_rms')} deg RMS,"
                f" bias {stat(s, 'lateral_bias')}",
                f"    quadrant errors {stat(s, 'quadrant_error')}%,"
                f" local polar error {stat(s, 'local_polar_rms')} deg RMS"
                f" ({counts['sagittal']} targets near the median plane)",
                f"    front/back confusions {stat(s, 'front_back')}% of {counts['front_back']},"
                f" up/down {stat(s, 'up_down')}% of {counts['up_down']}",
                f"    elevation gain {stat(s, 'elevation_gain', 2)}",
            ]
        for condition, m in loc["moving"].items():
            if m["n"]:
                lines.append(
                    f"  moving, {LABELS[condition]} ({m['n']}):"
                    f" the right way {stat(m, 'right_way')}%,"
                    f" end lateral error {number(m['end']['rms'])} deg RMS,"
                    f" start {number(m['start']['rms'])}"
                )
        if loc["catch"]["n"]:
            lines.append(
                f"  catch trials ({loc['catch']['n']}): {stat(loc['catch'], 'correct')}%"
                " on the side that sounded"
            )
    if analysis.get("models"):
        lines += [
            "",
            "Beside the calibrated model listeners (models.json; the models' numbers are over"
            " every measured direction):",
        ]
        for row in analysis["models"]:
            pool = row["pool"]
            inside = row["within_pool_range"]
            where = "" if inside is None else " within" if inside else " outside"
            where += " the pool's range" if where else ""
            lines.append(
                f"  {row['condition']:8s} {row['metric']:16s}"
                f" person {number(row['person'])}{interval(row['person_ci'])};"
                f" pool median {number(pool.get('median'))}"
                f" ({number(pool.get('min'))} to {number(pool.get('max'))}),"
                f" KEMAR {number(row['kemar_listener'])}{where}"
            )
    coh = analysis.get("coherence")
    if coh and coh["n"]:
        lines += ["", f"Musical coherence ({coh['n']} ratings; mean and 95% bootstrap interval):"]
        for attribute, by_condition in coh["attributes"].items():
            cells = ", ".join(
                f"{c} {number(v['mean'], 0)}{interval(v['ci'], 0)}" for c, v in by_condition.items()
            )
            lines.append(f"  {attribute}: {cells}")
        if coh["post_screening"]:
            p = coh["post_screening"]
            verdict = "passed" if p["passed"] else "NOT passed"
            lines.append(
                f"  post-screening: hidden reference at 90 or more in {p['reference_at_least_90']}"
                f" of {p['trials']} ({number(p['share'], 0)}%): {verdict}"
            )
        if coh["repeats"]:
            r = coh["repeats"]
            lines.append(
                f"  repeats: {r['pairs']} pairs, mean change {number(r['mean_absolute_change'])},"
                f" correlation {number(r['correlation'], 2)}"
            )
        if coh["identical"]:
            i = coh["identical"]
            lines.append(
                f"  identical conditions: mean spread {number(i['mean_spread'])}"
                f" over {i['trials']} trials"
            )
        if coh["proxy"]:
            lines.append(
                "  envelopment against 1 - IACC (500 Hz-4 kHz, a proxy):"
                f" Spearman {number(coh['proxy']['spearman'], 2)} over {coh['proxy']['n']}"
            )
    return "\n".join(lines)


def analyse(
    stimuli: Path, results_dir: Path, models_path: Path | None, draws: int, seed: int
) -> dict:
    notes: list[str] = []
    results = load_results(results_dir)
    localization_manifest = load_json(stimuli / "localization.json")
    coherence_manifest = load_json(stimuli / "coherence.json")
    models = load_json(models_path) if models_path else None
    loc_results = [
        r
        for r in results
        if r.get("part") == "localization" and check_manifest(r, localization_manifest, notes)
    ]
    coh_results = [
        r
        for r in results
        if r.get("part") == "coherence" and check_manifest(r, coherence_manifest, notes)
    ]
    for r in loc_results + coh_results:
        if not r.get("complete"):
            notes.append(f"{r['_path']}: session not finished; its answers so far are included")
    calibration = []
    for r in loc_results + coh_results:
        for c in r.get("calibrations", []):
            calibration.append(
                {
                    "file": r["_path"],
                    "level_set": c.get("level_set"),
                    "left_right_ok": c.get("left_right_ok"),
                    "polarity_ok": c.get("polarity_ok"),
                }
            )
            if c.get("polarity_ok") is False:
                notes.append(f"{r['_path']}: the polarity check picked the out-of-phase sound")
    analysis = {
        "kind": "xem-listening-analysis",
        "version": 1,
        "inputs": {
            "results": [r["_path"] for r in results],
            "stimuli": str(stimuli),
            "models": str(models_path) if models else None,
            "bootstrap": draws,
            "seed": seed,
        },
        "listeners": sorted({r.get("listener") for r in results if r.get("listener")}),
        "headphones": sorted({r.get("headphones") for r in results if r.get("headphones")}),
        "calibration": calibration,
        "notes": notes,
    }
    if loc_results:
        analysis["localization"] = localization(
            loc_results, localization_manifest, draws, seed, notes
        )
        analysis["models"] = compare_models(analysis["localization"]["static"], models)
    if coh_results:
        analysis["coherence"] = coherence(coh_results, coherence_manifest, draws, seed, notes)
    return analysis


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--stimuli", type=Path, default=ROOT / ".local/listening")
    parser.add_argument("--results", type=Path, help="default: STIMULI/results")
    parser.add_argument("--models", type=Path, default=ROOT / "build/binaural/models.json")
    parser.add_argument("--json", type=Path, help="default: STIMULI/analysis.json")
    parser.add_argument("--bootstrap", type=int, default=2000)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    results = args.results or args.stimuli / "results"
    if not results.is_dir():
        parser.error(f"no results in {results}")
    analysis = analyse(
        args.stimuli,
        results,
        args.models if args.models.is_file() else None,
        args.bootstrap,
        args.seed,
    )
    print(report(analysis))
    out = args.json or args.stimuli / "analysis.json"
    out.write_text(json.dumps(analysis, indent=1) + "\n")
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
