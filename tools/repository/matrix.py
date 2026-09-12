"""Generate one traceable requirement/test row per reviewed plan facet."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PHASE_HEADING = re.compile(r"## Phase (\d+[A-Z]?) —")


def phase_id(value: int | str) -> str:
    """Retain numeric phase IDs and distinguish lettered phases such as 2A."""
    match = re.fullmatch(r"(\d+)([A-Z]?)", str(value))
    if not match:
        raise ValueError(f"Invalid phase: {value}")
    return f"P{int(match[1]):02}{match[2]}"


def phase_value(source_id: str) -> int | str:
    value = source_id.split("-", 1)[0].removeprefix("P")
    match = re.fullmatch(r"(\d+)([A-Z]?)", value)
    if not match:
        raise ValueError(f"Invalid phase source: {source_id}")
    return f"{int(match[1])}{match[2]}" if match[2] else int(match[1])


def plan_sources(text: str) -> dict:
    sources = {
        "tasks": {},
        "exits": {},
        "early_exits": {},
        "phases": [],
        "boundaries": [],
        "tables": {},
        "execution_rules": [],
        "foundational_requirement": "",
        "foundational_authoring_requirement": "",
    }
    phase, task, table, section = None, 0, None, None
    for line in text.splitlines():
        if line.startswith("## "):
            section = line[3:]
        if match := PHASE_HEADING.match(line):
            phase, task, table = phase_id(match[1]), 0, None
            if phase in sources["phases"]:
                raise ValueError(f"Duplicate plan phase: {phase}")
            sources["phases"].append(phase)
        if match := re.match(r"- \[[ x]\] (.+)", line):
            if phase is None:
                raise ValueError("Plan task precedes its phase")
            task += 1
            sources["tasks"][f"{phase}-T{task:02}"] = match[1]
        if line.startswith("**Exit criterion:** "):
            sources["exits"][phase] = line.removeprefix("**Exit criterion:** ")
        if line.startswith("**Early exit criterion,"):
            sources["early_exits"][phase] = line
        if phase is None and line.startswith("- **"):
            sources["boundaries"].append(line[2:])
        if line.startswith("**Foundational requirement:**"):
            sources["foundational_requirement"] = line
        if line.startswith("**Foundational authoring requirement:**"):
            sources["foundational_authoring_requirement"] = line
        if line.startswith(("**Execution rule:**", "**Parallelization:**", "**Ordering:**")):
            sources["execution_rules"].append(line)
        if line.startswith("| Action |"):
            table = "keyboard_defaults"
        elif line.startswith("| Capability |"):
            table = (
                "authoring_contract"
                if section == "Foundational agent-authoring contract"
                else "agent_contract"
            )
        elif line.startswith("| Layer |"):
            table = "authoring_stack"
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
        if len(parts) != 3 or not re.fullmatch(r"P\d{2}[A-Z]?-T\d{2}", parts[0]):
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
    review = json.loads((root / "docs/requirement-review.json").read_text())
    if review.get("schema_version") != 1 or review["tasks"].keys() != specs.keys():
        raise ValueError("Requirement coverage review omits tasks or uses an unknown schema")
    for key, spec in specs.items():
        expected = {
            "source_sha256": hashlib.sha256(sources["tasks"][key].encode()).hexdigest(),
            "spec_sha256": hashlib.sha256(
                json.dumps(spec, sort_keys=True, ensure_ascii=False).encode()
            ).hexdigest(),
        }
        if review["tasks"][key] != expected:
            raise ValueError(f"Requirement needs renewed source/facet coverage review: {key}")
    results = json.loads((root / "tests/validation-results.json").read_text())
    rows = []
    for source_id, spec in specs.items():
        phase = phase_value(source_id)
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
    if source_id.startswith("P02A-") and 35 <= int(source_id.split("-T")[1]) <= 39:
        return ["arch-vulkan", "arch-headless"]
    if source_id in {"P02-T04", "P03-T01", "P03-T09", "P03-T10", "P03-T11", "P03-T12", "P13-T01"}:
        if source_id in {"P03-T09", "P03-T10", "P03-T11", "P03-T12"}:
            return ["arch-vulkan", "arch-headless"]
        return ["arch-vulkan"]
    if source_id in {"P06-T03", "P13-T02"}:
        return ["windows-d3d12"]
    if source_id == "P13-T03":
        return ["macos-metal"]
    targets = ["arch-vulkan", "windows-d3d12", "macos-metal"]
    if (
        source_id.startswith("P02-")
        and 14 <= int(source_id[5:]) <= 41
        or source_id.startswith("P02A-")
        or source_id.startswith("P07-")
        and int(source_id[5:]) >= 15
        or source_id.startswith("P11-")
        and int(source_id[5:]) >= 13
        or source_id == "P12-T11"
        or source_id.startswith("P13-")
        and int(source_id[5:]) >= 19
        or source_id
        in {
            "P02-T01",
            "P02-T08",
            "P02-T09",
            "P02-T10",
            "P02-T11",
            "P02-T12",
            "P02-T13",
            "P02-T42",
            "P02-T46",
            "P02-T47",
            "P04-T13",
            "P04-T14",
            "P06-T13",
            "P06-T14",
            "P07-T13",
            "P07-T14",
            "P08-T12",
            "P08-T13",
            "P08-T14",
            "P09-T18",
            "P10-T23",
            "P11-T12",
            "P12-T10",
            "P13-T06",
            "P13-T07",
            "P13-T08",
            "P13-T09",
        }
    ):
        targets += ["arch-headless", "windows-headless", "macos-headless"]
    return targets


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
    for phase in data["source_snapshot"]["phases"]:
        lines += [f"## Phase {phase_value(phase)}", ""]
        for task_id, source in data["source_snapshot"]["tasks"].items():
            if not task_id.startswith(f"{phase}-"):
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
        if phase in data["source_snapshot"]["exits"]:
            lines += ["Exit gate: " + data["source_snapshot"]["exits"][phase], ""]
        if phase in data["source_snapshot"]["early_exits"]:
            lines += [data["source_snapshot"]["early_exits"][phase], ""]
    lines += [
        "## Crosscutting sources and default policy",
        "",
        "`docs/traceability.json` maps every project boundary, foundational native-agent and",
        "agent-authoring contract row, selected authoring-stack responsibility, early gate,",
        "required-feature row, keyboard default, execution rule and release checkpoint to tests.",
        "`docs/requirement-review.json` binds reviewed source text to facet specifications;",
        "`docs/requirements-migration.json` preserves the earlier evidence and task mapping.",
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
