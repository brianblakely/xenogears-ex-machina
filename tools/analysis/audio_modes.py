"""Compare matched two-channel captures of the Stereo, Wide and Mono sound modes.

The three captures must come from the same input program except for the mode
selection, so that their samples align. For each named frame window this
reports per-channel levels, left/right correlation, sum and difference energy,
the exact per-sample relations between the modes (identity or polarity
inversion, with the residual distribution) and the inter-channel lag of the
Wide/Stereo right-channel relation. Where Wide changes the left channel, the
difference (Stereo - Wide) / 2 is the part whose sign Wide flips; its delay
after the unchanged part (Stereo + Wide) / 2 is measured by cross-correlation.

Only derived numbers leave this tool; the recordings stay local.
"""

from __future__ import annotations

import argparse
import collections
import json
import math
import operator
import sys
import wave
from array import array
from dataclasses import dataclass
from pathlib import Path


class AudioModeError(ValueError):
    pass


@dataclass(frozen=True)
class Recording:
    rate: int
    frames: bytes  # interleaved little-endian s16 left/right

    def channels(self, start: int, end: int) -> tuple[array, array]:
        samples = array("h")
        samples.frombytes(self.frames[start * 4 : end * 4])
        if sys.byteorder != "little":
            samples.byteswap()
        return samples[0::2], samples[1::2]

    def __len__(self) -> int:
        return len(self.frames) // 4


def read_wav(path: Path) -> Recording:
    with wave.open(str(path), "rb") as source:
        if source.getnchannels() != 2 or source.getsampwidth() != 2:
            raise AudioModeError(f"{path} is not two-channel 16-bit PCM")
        return Recording(source.getframerate(), source.readframes(source.getnframes()))


def first_difference(a: Recording, b: Recording) -> int | None:
    """First sample index where the two recordings differ, None when equal."""
    n = min(len(a), len(b))
    x, y = memoryview(a.frames)[: n * 4], memoryview(b.frames)[: n * 4]
    if x == y:
        return None if len(a) == len(b) else n
    lo, hi = 0, n  # prefix [0, lo) equal, [0, hi) differs
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if x[: mid * 4] == y[: mid * 4]:
            lo = mid
        else:
            hi = mid
    return lo


def levels(left: array, right: array) -> dict:
    n = len(left)
    if n == 0:
        raise AudioModeError("empty window")
    ll, rr, lr = (math.sumprod(p, q) for p, q in ((left, left), (right, right), (left, right)))
    total = (ll + rr + 2 * lr) / 4  # ((L + R) / 2)^2 summed
    difference = (ll + rr - 2 * lr) / 4
    return {
        "rms_left": round(math.sqrt(ll / n), 2),
        "rms_right": round(math.sqrt(rr / n), 2),
        "correlation": round(lr / math.sqrt(ll * rr), 4) if ll and rr else None,
        "rms_sum_half": round(math.sqrt(total / n), 2),
        "rms_difference_half": round(math.sqrt(difference / n), 2),
        "sum_over_difference_db": (
            round(10 * math.log10(total / difference), 2) if total and difference else None
        ),
    }


def relation(a: array, b: array, sign: int) -> dict:
    """Residual a - sign * b: sign 1 tests identity, -1 polarity inversion."""
    combine = operator.sub if sign == 1 else operator.add
    residual = collections.Counter(map(combine, a, b))
    n = len(a)
    energy = sum(value * value * count for value, count in residual.items())
    return {
        "max_abs_residual": max(abs(value) for value in residual),
        "rms_residual": round(math.sqrt(energy / n), 3),
        "exact_fraction": round(residual.get(0, 0) / n, 6),
        "common_residuals": [[v, c] for v, c in residual.most_common(4)],
    }


def lagged_correlation(a: array, b: array, lag: int) -> float:
    """Normalized correlation of a[t] with b[t + lag] over the overlap."""
    if lag >= 0:
        x, y = a[: len(a) - lag], b[lag:]
    else:
        x, y = a[-lag:], b[: len(b) + lag]
    xx, yy = math.sumprod(x, x), math.sumprod(y, y)
    return math.sumprod(x, y) / math.sqrt(xx * yy) if xx and yy else 0.0


def delay_after(source: list, delayed: list, max_lag: int) -> dict:
    """Lag (samples, 0..max_lag) at which `delayed` best correlates with `source`."""
    scores = {lag: lagged_correlation(source, delayed, lag) for lag in range(max_lag + 1)}
    best = max(scores, key=scores.get)
    return {
        "lag_samples": best,
        "correlation": round(scores[best], 4),
        "correlation_at_zero": round(scores[0], 4),
    }


def compare_window(
    recordings: dict[str, Recording],
    start: int,
    end: int,
    delay_samples: int,
    max_lag: int,
) -> dict:
    channels = {mode: r.channels(start, end) for mode, r in recordings.items()}
    result = {"samples": end - start, "modes": {m: levels(*c) for m, c in channels.items()}}
    (sl, sr), (wl, wr) = channels["stereo"], channels["wide"]
    result["wide_vs_stereo"] = {
        "left_identity": relation(wl, sl, 1),
        "left_inversion": relation(wl, sl, -1),
        "right_identity": relation(wr, sr, 1),
        "right_inversion": relation(wr, sr, -1),
        "right_lag_correlation": {
            str(lag): round(lagged_correlation(sr, wr, lag), 5) for lag in range(-3, 4)
        },
    }
    if "mono" in channels:
        ml, mr = channels["mono"]
        result["mono_vs_stereo"] = {
            "left_identity": relation(ml, sl, 1),
            "right_identity": relation(mr, sr, 1),
        }
    if any(map(operator.ne, wl, sl)):
        count = min(delay_samples, len(sl))
        kept = [(s + w) / 2 for s, w in zip(sl[:count], wl[:count], strict=True)]
        flipped = [(s - w) / 2 for s, w in zip(sl[:count], wl[:count], strict=True)]
        result["wide_left_flipped_part"] = {
            "rms_kept": round(math.sqrt(math.sumprod(kept, kept) / count), 2),
            "rms_flipped": round(math.sqrt(math.sumprod(flipped, flipped) / count), 2),
            "delay_after_kept": delay_after(kept, flipped, max_lag),
        }
    return result


def parse_window(text: str) -> tuple[str, int, int]:
    name, start, end = text.split(":")
    if int(end) <= int(start):
        raise AudioModeError(f"window {text} is empty")
    return name, int(start), int(end)


def analyse(
    paths: dict[str, Path],
    windows: list[tuple[str, int, int]],
    fps: float,
    delay_seconds: float,
    max_lag_ms: float,
) -> dict:
    recordings = {mode: read_wav(path) for mode, path in paths.items()}
    rates = {r.rate for r in recordings.values()}
    if len(rates) != 1:
        raise AudioModeError(f"recordings have different sample rates {sorted(rates)}")
    rate = rates.pop()
    per_frame = rate / fps
    shortest = min(len(r) for r in recordings.values())
    report: dict = {"sample_rate": rate, "fps": fps, "samples": shortest, "first_difference": {}}
    modes = list(recordings)
    for i, a in enumerate(modes):
        for b in modes[i + 1 :]:
            at = first_difference(recordings[a], recordings[b])
            report["first_difference"][f"{a}/{b}"] = (
                None if at is None else {"sample": at, "frame": round(at / per_frame, 2)}
            )
    report["windows"] = {}
    for name, first, last in windows:
        start, end = round(first * per_frame), min(round(last * per_frame), shortest)
        if end <= start:
            raise AudioModeError(f"window {name} lies beyond the recordings")
        entry = compare_window(
            recordings,
            start,
            end,
            round(delay_seconds * rate),
            round(max_lag_ms * rate / 1000),
        )
        report["windows"][name] = {"frames": [first, last], **entry}
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stereo", type=Path, required=True)
    parser.add_argument("--wide", type=Path, required=True)
    parser.add_argument("--mono", type=Path)
    parser.add_argument(
        "--window",
        action="append",
        default=[],
        metavar="NAME:START:END",
        help="frame window (frontend frames)",
    )
    parser.add_argument("--fps", type=float, default=60.0)
    parser.add_argument(
        "--delay-seconds",
        type=float,
        default=10.0,
        help="length of the segment used for the delay search",
    )
    parser.add_argument("--max-lag-ms", type=float, default=150.0)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    paths = {"stereo": args.stereo, "wide": args.wide}
    if args.mono:
        paths["mono"] = args.mono
    report = analyse(
        paths, [parse_window(w) for w in args.window], args.fps, args.delay_seconds, args.max_lag_ms
    )
    text = json.dumps(report, indent=1) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
