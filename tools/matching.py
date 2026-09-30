"""Fingerprint the pristine input and compare complete rebuilt bytes, without masks."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def compare(original: Path, rebuilt: Path, expected_sha256: str) -> dict:
    if not re.fullmatch(r"[0-9a-fA-F]{64}", expected_sha256):
        raise ValueError("Expected original SHA256 must contain exactly 64 hexadecimal digits")
    if original.samefile(rebuilt):
        raise ValueError("Original and rebuilt must be distinct files")
    expected = original.read_bytes()
    actual = rebuilt.read_bytes()
    if not expected:
        raise ValueError("The original executable image must not be empty")
    original_hash = hashlib.sha256(expected).hexdigest()
    if original_hash != expected_sha256.lower():
        raise ValueError(
            "Pristine original fingerprint mismatch; do not update expectations to pass"
        )
    first = next(
        (i for i, (a, b) in enumerate(zip(expected, actual, strict=False)) if a != b), None
    )
    if first is None and len(expected) != len(actual):
        first = min(len(expected), len(actual))
    return {
        "matched": expected == actual,
        "original_sha256": original_hash,
        "rebuilt_sha256": hashlib.sha256(actual).hexdigest(),
        "original_bytes": len(expected),
        "rebuilt_bytes": len(actual),
        "first_difference": first,
        "claim": "binary_agreement_only",
        "source_coverage": "not_measured",
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("rebuilt", type=Path)
    parser.add_argument("--sha256", required=True, help="qualified pristine original digest")
    args = parser.parse_args(argv)
    try:
        result = compare(args.original, args.rebuilt, args.sha256)
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2
    print(json.dumps(result, sort_keys=True))
    return 0 if result["matched"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
