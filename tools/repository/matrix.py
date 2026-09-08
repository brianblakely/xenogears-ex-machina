"""Generate one traceable requirement/test row per reviewed plan facet."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def plan_sources(text: str) -> dict:
    sources = {"tasks": {}, "exits": {}, "boundaries": [], "tables": {}, "execution_rules": []}
    phase, task, table = -1, 0, None
    for line in text.splitlines():
        if match := re.match(r"## Phase (\d+) —", line):
            phase, task, table = int(match[1]), 0, None
        if match := re.match(r"- \[[ x]\] (.+)", line):
            task += 1
            sources["tasks"][f"P{phase:02}-T{task:02}"] = match[1]
        if line.startswith("**Exit criterion:** "):
            sources["exits"][f"P{phase:02}"] = line.removeprefix("**Exit criterion:** ")
        if phase < 0 and line.startswith("- **"):
            sources["boundaries"].append(line[2:])
        if line.startswith(("**Execution rule:**", "**Parallelization:**")):
            sources["execution_rules"].append(line)
        if line.startswith("| Action |"):
            table = "keyboard_defaults"
        elif line.startswith("| Checkpoint |"):
            table = "release_checkpoints"
        elif line.startswith("| Requirement |"):
            table = "feature_summary"
        elif table and line.startswith("|") and not line.startswith("|---"):
            sources["tables"].setdefault(table, []).append(
                [cell.strip() for cell in line.strip("|").split("|")]
            )
        elif line and not line.startswith("|"):
            table = None
    return sources


def facet_specs(text: str) -> dict:
    specs = {}
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        parts = [part.strip() for part in line.split("|")]
        if len(parts) != 3 or not re.fullmatch(r"P\d{2}-T\d{2}", parts[0]):
            raise ValueError(f"Malformed facet specification at line {number}")
        key, facets, procedure = parts
        names = [name.strip() for name in facets.split(";")]
        if key in specs or not all(names) or len(names) != len(set(names)) or not procedure:
            raise ValueError(f"Duplicate/empty facet or task at line {number}")
        specs[key] = {"facets": names, "procedure": procedure}
    return specs


def build_matrix(root: Path = ROOT) -> dict:
    sources = plan_sources((root / "plan.md").read_text())
    specs = facet_specs((root / "docs/requirement-facets.txt").read_text())
    if specs.keys() != sources["tasks"].keys():
        missing = sources["tasks"].keys() - specs.keys()
        extra = specs.keys() - sources["tasks"].keys()
        raise ValueError(f"Unmapped plan tasks: {sorted(missing)}; stale specs: {sorted(extra)}")
    results = json.loads((root / "tests/validation-results.json").read_text())
    rows = []
    for source_id, spec in specs.items():
        phase = int(source_id[1:3])
        for index, facet in enumerate(spec["facets"], 1):
            facet_id = f"{source_id}-F{index:02}"
            result = results["results"].get(facet_id, {})
            rows.append(
                {
                    "id": facet_id,
                    "source_id": source_id,
                    "phase": phase,
                    "requirement": facet,
                    "test_id": f"TEST-{facet_id}",
                    "procedure_and_acceptance": spec["procedure"],
                    "targets": ["repository"] if phase == 0 else target_scope(source_id),
                    "status": result.get("status", "defined"),
                    "evidence": result.get("evidence", []),
                }
            )
    row_ids = {row["id"] for row in rows}
    if extra := results["results"].keys() - row_ids:
        raise ValueError(f"Results refer to nonexistent facets: {sorted(extra)}")
    return {
        "schema_version": 1,
        "source": "plan.md",
        "prompt": "prompt.md is empty at the Phase 0 baseline; no extra requirements inferred.",
        "status_semantics": {
            "defined": "Test is specified, with no claim of execution or implementation.",
            "passed": "Linked evidence proves this facet in the explicitly recorded scope.",
            "failed": "Linked execution evidence contradicts the required result.",
            "blocked": "Specific recorded missing evidence or dependency prevents validation.",
        },
        "source_snapshot": sources,
        "crosscutting": json.loads((root / "docs/traceability.json").read_text()),
        "requirements": rows,
        "phase1_slice": json.loads((root / "analysis/slices/forest23.json").read_text()),
    }


def target_scope(source_id: str) -> list[str]:
    if source_id.startswith("P01-"):
        return ["original-reference-analysis"]
    if source_id in {"P02-T04", "P03-T01", "P13-T01"}:
        return ["arch-vulkan"]
    if source_id in {"P06-T03", "P13-T02"}:
        return ["windows-d3d12"]
    if source_id == "P13-T03":
        return ["macos-metal"]
    return ["arch-vulkan", "windows-d3d12", "macos-metal"]


def markdown(data: dict) -> str:
    rows = data["requirements"]
    lines = [
        "# Requirement-to-test matrix",
        "",
        "Generated by `python3 tools/repository/matrix.py`; edit the reviewed facet specifications",
        "in `docs/requirement-facets.txt`; results live in `tests/validation-results.json`.",
        "A **defined** test has not passed. A **blocked** test has an explicit evidence gap.",
        "Each facet has its own test ID and result; grouped procedures must exercise every facet.",
        "All target cells require individual platform evidence before a cross-platform pass.",
        "Original-game oracles require local original observations, never synthetic fixtures.",
        "The JSON companion preserves the exact source requirements, tables, boundaries and gates.",
        "",
        f"{len(data['source_snapshot']['tasks'])} source tasks; {len(rows)} explicit test facets.",
        "",
    ]
    for phase in range(14):
        lines += [f"## Phase {phase}", ""]
        for task_id, source in data["source_snapshot"]["tasks"].items():
            if not task_id.startswith(f"P{phase:02}-"):
                continue
            subset = [row for row in rows if row["source_id"] == task_id]
            lines += [f"### {task_id}", "", source, "", subset[0]["procedure_and_acceptance"], ""]
            lines += ["| Test ID | Required facet | Targets | Result |", "|---|---|---|---|"]
            for row in subset:
                evidence = ", ".join(row["evidence"])
                status = row["status"] + (f" ({evidence})" if evidence else "")
                lines.append(
                    f"| {row['test_id']} | {row['requirement']} | "
                    f"{', '.join(row['targets'])} | {status} |"
                )
            lines.append("")
        lines += ["Exit gate: " + data["source_snapshot"]["exits"][f"P{phase:02}"], ""]
    lines += [
        "## Crosscutting sources and default policy",
        "",
        "`docs/traceability.json` maps all seven project boundaries, all required-feature summary",
        "rows, all nine keyboard-default rows, execution rules and release checkpoints to tests.",
        "`docs/defaults.json` records prescribed defaults and leaves other choices undecided.",
        "`docs/platforms.json` separates declared target plans from executed platform coverage.",
        "",
    ]
    return "\n".join(lines)


def generated_files(root: Path = ROOT) -> dict[Path, str]:
    data = build_matrix(root)
    return {
        root / "docs/requirements.json": json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        root / "docs/requirements.md": markdown(data),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for path, expected in generated_files().items():
        if args.check:
            if not path.exists() or path.read_text() != expected:
                raise SystemExit(f"Stale generated matrix: {path.relative_to(ROOT)}")
        else:
            path.write_text(expected)
    print("Requirement matrix is current" if args.check else "Requirement matrix generated")


if __name__ == "__main__":
    main()
