"""Validate the current recovery inventory and diagnose incomplete dependencies.

This is an analysis coverage check, not an instruction interpreter. Inventory
membership, an observed subset and an authored library never grant a proof pass.
The existing slice gate evaluates independently reviewed, scoped original proof.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GROUPS = {
    "unresolved_symbols": "symbol",
    "unknown_formats": "format",
    "unverified_behavior": "behavior",
}
ANALYSIS_STATUSES = {"unresolved", "source_reconstructed_unobserved", "observed_subset"}
NATIVE_STATUSES = {"unimplemented", "authored_cpp_library"}
REFERENCE = re.compile(
    r"(?:symbol|format|behavior):[a-z0-9][a-z0-9_-]*|instruction:[a-z][a-z0-9_-]*:0x[0-9a-f]{2}"
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(f"Recovery inventory: {message}")


def nonempty(value) -> bool:
    return isinstance(value, str) and bool(value.strip())


def evidence(row: dict, label: str, known: set[str] | None) -> list[str]:
    items = row.get("evidence", [])
    require(
        isinstance(items, list)
        and all(nonempty(item) for item in items)
        and len(items) == len(set(items)),
        f"{label} has invalid or duplicate evidence",
    )
    if known is not None:
        require(set(items) <= known, f"{label} references unknown evidence")
    return items


def entries(
    data: dict,
    *,
    known_evidence: set[str] | None = None,
    allowed_sources: set[str] | None = None,
) -> dict[str, dict]:
    """Index the explicitly incomplete inventory without upgrading its statuses."""
    require(isinstance(data, dict) and data.get("schema_version") == 1, "unsupported schema")
    require(type(data.get("coverage_complete")) is bool, "coverage_complete must be boolean")
    for key in ("scope", "reconstruction_status_rule", "unknown_instruction_policy"):
        require(nonempty(data.get(key)), f"missing {key}")
    result = {}
    for group, category in GROUPS.items():
        rows = data.get(group)
        require(isinstance(rows, list), f"missing {group}")
        for row in rows:
            require(isinstance(row, dict), f"invalid {group} entry")
            identifier = row.get("id")
            reference = f"{category}:{identifier}"
            require(
                isinstance(identifier, str) and REFERENCE.fullmatch(reference) is not None,
                f"invalid {group} ID",
            )
            require(reference not in result, f"duplicate {reference}")
            require(row.get("status", "unresolved") == "unresolved", f"{reference} hides a gap")
            require(nonempty(row.get("next_experiment")), f"{reference} lacks its next experiment")
            result[reference] = {
                "dependency": reference,
                "analysis_status": "unresolved",
                "detail": row["next_experiment"],
                "evidence": evidence(row, reference, known_evidence),
            }
    tables = data.get("event_dispatch_tables")
    require(isinstance(tables, list) and len(tables) == 2, "missing event dispatch tables")
    require(
        all(isinstance(table, dict) and isinstance(table.get("namespace"), str) for table in tables)
        and {table.get("namespace") for table in tables} == {"primary", "extended"},
        "event namespaces must remain distinct",
    )
    sprite = data.get("sprite_command_dispatch")
    require(
        isinstance(sprite, dict) and sprite.get("namespace") == "sprite", "missing sprite table"
    )
    for table in [*tables, sprite]:
        namespace = table["namespace"]
        # These are the recorded original table windows, including non-code
        # extended values. Bytes outside a window are unlisted, never no-ops.
        expected = range(0x8A, 0xFD) if namespace == "sprite" else range(256)
        rows = table.get("rows")
        require(
            isinstance(rows, list) and len(rows) == len(expected), f"{namespace} table has gaps"
        )
        values = []
        for opcode, row in zip(expected, rows, strict=True):
            reference = f"instruction:{namespace}:0x{opcode:02x}"
            require(
                isinstance(row, dict) and row.get("opcode") == f"0x{opcode:02x}",
                f"missing, duplicated or reordered {reference}",
            )
            target = row.get("handler")
            require(
                isinstance(target, str) and re.fullmatch(r"0x[0-9a-f]{8}", target) is not None,
                f"{reference} lacks its original table value",
            )
            values.append(int(target, 16))
            analysis, native = row.get("analysis_status"), row.get("native_status")
            require(
                isinstance(analysis, str) and analysis in ANALYSIS_STATUSES,
                f"{reference} has unreviewed analysis status",
            )
            require(
                isinstance(native, str) and native in NATIVE_STATUSES,
                f"{reference} has unreviewed native status",
            )
            references = evidence(row, reference, known_evidence)
            if analysis != "unresolved":
                require(bool(references), f"{reference} has unsupported partial recovery")
            if native == "authored_cpp_library":
                source = row.get("cpp_reconstruction")
                require(
                    analysis != "unresolved"
                    and isinstance(source, str)
                    and source.startswith("src/")
                    and ".." not in Path(source).parts,
                    f"{reference} lacks its authored source boundary",
                )
                if allowed_sources is not None:
                    require(source in allowed_sources, f"{reference} uses an unlisted source")
            result[reference] = {
                "dependency": reference,
                "analysis_status": analysis,
                "native_status": native,
                "table_value": target,
                "detail": "Full instruction behavior remains unverified; a shared table value "
                "does not establish aliases or no-ops. Consult the finding's bounded scope.",
                "evidence": references,
            }
        expected_hash = table.get("table_sha256" if namespace == "sprite" else "sha256")
        actual_hash = hashlib.sha256(struct.pack(f"<{len(values)}I", *values)).hexdigest()
        require(actual_hash == expected_hash, f"{namespace} original table fingerprint differs")
    require(not data["coverage_complete"], "incomplete entries cannot establish complete coverage")
    return result


def validate(data: dict, **kwargs) -> dict:
    indexed = entries(data, **kwargs)
    return {
        "inventory_valid": True,
        "coverage_complete": False,
        "incomplete_entries": len(indexed),
        "categories": {
            category: sum(key.startswith(category + ":") for key in indexed)
            for category in (*GROUPS.values(), "instruction")
        },
    }


def diagnose(data: dict, dependencies: list[str], coverage: str) -> dict:
    """Block a declared full-coverage request and identify each incomplete input.

    The caller declares the affected coverage; this tool does not infer reachability
    or make unrelated slice proofs fail because of an instruction outside their route.
    """
    require(nonempty(coverage), "a diagnostic needs the affected coverage")
    require(
        isinstance(dependencies, list)
        and bool(dependencies)
        and all(isinstance(key, str) and REFERENCE.fullmatch(key) for key in dependencies),
        "supply explicit symbol, format, behavior or namespaced instruction dependencies",
    )
    require(len(dependencies) == len(set(dependencies)), "duplicate diagnostic dependency")
    indexed = entries(data)
    blockers = []
    for key in dependencies:
        item = indexed.get(key)
        if item is None:
            item = {
                "dependency": key,
                "analysis_status": "unlisted",
                "detail": "No qualified inventory entry; add original-source analysis before use.",
                "evidence": [],
            }
        blockers.append({**item, "blocked_coverage": coverage})
    return {"status": "blocked", "coverage": coverage, "blockers": blockers}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, default=ROOT / "analysis/recovery.json")
    parser.add_argument("--coverage")
    parser.add_argument("--dependency", action="append")
    args = parser.parse_args()
    if bool(args.coverage) != bool(args.dependency):
        parser.error("--coverage and --dependency must be supplied together")
    try:
        data = json.loads(args.inventory.read_text())
        result = (
            diagnose(data, args.dependency, args.coverage) if args.dependency else validate(data)
        )
    except (OSError, ValueError) as error:
        parser.exit(2, f"{error}\n")
    print(json.dumps(result, indent=2))
    if result.get("status") == "blocked":
        parser.exit(1)


if __name__ == "__main__":
    main()
