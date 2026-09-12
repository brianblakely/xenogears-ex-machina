"""Strict Phase 1 first-slice gate; broad facets and later native gates remain intact."""

from __future__ import annotations

import hashlib
import re
from pathlib import Path

MANIFEST = "analysis/slices/forest23.json"
DOMAINS = (
    "source-loading",
    "field-representation",
    "script-execution",
    "field-behavior",
    "encounter-return",
    "menu-persistence",
    "required-media",
    "time-services",
    "reproducibility-remainder",
)
PROOFS = {f"P01-SLICE-{name.upper()}": name for name in DOMAINS}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(f"Phase 1 slice: {message}")


def indexed(rows: list[dict], key: str, label: str) -> dict:
    result = {row[key]: row for row in rows}
    require(len(result) == len(rows), f"duplicate {label}")
    return result


def validate_structure(matrix: dict) -> None:
    """Check scope and status even before any original evidence has passed."""
    manifest = matrix["phase1_slice"]
    rows = {row["id"]: row for row in matrix["requirements"]}
    facets = {key: row for key, row in rows.items() if row["phase"] == 1}
    gate = next(g for g in matrix["crosscutting"]["phase_exits"] if g["phase"] == 1)
    require(manifest.get("schema_version") == 1, "unsupported manifest schema")
    require(manifest.get("id") == "SLICE-FOREST-23", "unreviewed slice identity")
    require(manifest["status"] in {"candidate", "reviewed"}, "invalid slice status")
    require(
        gate.get("requires_all_phase_facets") is False
        and gate.get("requires_reviewed_slice") == MANIFEST,
        "gate must name the reviewed slice contract",
    )
    proofs = indexed(manifest["proofs"], "id", "proof IDs")
    require(
        {key: p["domain"] for key, p in proofs.items()} == PROOFS,
        "missing or changed required proof domain",
    )
    dispositions = indexed(manifest["facet_dispositions"], "facet", "facet dispositions")
    require(dispositions.keys() == facets.keys(), "dropped or unknown broad facet")
    backlog = indexed(manifest["backlog"], "id", "backlog IDs")
    assigned = set()
    for key, disposition in dispositions.items():
        require(
            facets[key]["targets"] == ["original-reference-analysis"], "analysis target changed"
        )
        if disposition["disposition"] == "required":
            references = disposition.get("proofs", [])
            require(
                bool(references) and set(references) <= proofs.keys(), "unmapped required facet"
            )
            assigned.update(references)
        else:
            require(disposition["disposition"] == "deferred", "unknown facet disposition")
            require(facets[key]["status"] != "passed", "deferred facet cannot be passed")
            references = disposition.get("backlog", [])
            require(
                bool(disposition.get("reason"))
                and bool(references)
                and set(references) <= backlog.keys(),
                "deferred facet lacks tracked rationale",
            )
            for reference in references:
                item = backlog[reference]
                require(
                    item["kind"] == "outside_slice"
                    and item["status"] == "unresolved"
                    and bool(item["later_facets"])
                    and all(
                        key in rows and rows[key]["phase"] not in (0, 1)
                        for key in item["later_facets"]
                    ),
                    "deferred work must remain unresolved and linked to later facets",
                )
    require(assigned == proofs.keys(), "required proof omitted from facet mapping")
    for item in backlog.values():
        require(
            bool(item.get("requirement")) and bool(item.get("next_experiment")), "empty backlog"
        )
        require(item["status"] in {"unresolved", "resolved"}, "invalid backlog status")
        if item["kind"] == "required_slice_gap":
            require(item.get("proof") in proofs, "required backlog gap lacks its proof")
            require(
                (item["status"] == "resolved") == (proofs[item["proof"]]["status"] == "passed"),
                "required backlog status contradicts proof",
            )
        else:
            require(item["kind"] == "outside_slice", "unknown backlog kind")
    for proof in proofs.values():
        require(
            proof["status"] in {"defined", "blocked", "failed", "passed"}, "invalid proof status"
        )
        require(bool(proof["source_profiles"]) and bool(proof["requirement"]), "unqualified proof")
        if proof["status"] == "passed":
            require(
                bool(proof["evidence"])
                and bool(proof["reconstruction"])
                and not proof["open_work"],
                "proof pass requires evidence, reconstruction and no required open work",
            )
        else:
            require(bool(proof["open_work"]), "incomplete proof lacks its remaining work")
    if gate["status"] == "passed" or manifest["status"] == "reviewed":
        require(
            all(p["status"] == "passed" for p in proofs.values()),
            "exit cannot bypass incomplete required proofs",
        )
        require(
            manifest["status"] == "reviewed"
            and bool(manifest["selection"].get("frozen_route"))
            and bool(manifest["review"].get("exit_reviewers")),
            "exit requires the frozen route and independent final review",
        )


def checked_source(root: Path, record: dict, allowlist: set[str]) -> None:
    name = record["path"]
    path = Path(name)
    require(
        not path.is_absolute() and ".." not in path.parts and name in allowlist, "unlisted source"
    )
    require(
        isinstance(record.get("sha256"), str)
        and re.fullmatch(r"[a-f0-9]{64}", record["sha256"]) is not None,
        "missing source digest",
    )
    require(
        hashlib.sha256((root / path).read_bytes()).hexdigest() == record["sha256"],
        f"stale reviewed source {name}",
    )


def original_evidence(evidence: dict, proof_id: str) -> None:
    require(proof_id in evidence.get("covers", []), "evidence does not cover this proof")
    require(
        "original-reference-analysis" in evidence.get("targets", [])
        and evidence.get("confidence") == "confirmed"
        and evidence.get("validation", {}).get("result") == "passed"
        and bool(evidence.get("locations"))
        and bool(evidence.get("source_profiles"))
        and bool(evidence.get("validation", {}).get("artifacts")),
        "proof requires confirmed original source/execution evidence",
    )
    require(
        any(r != evidence.get("author") for r in evidence.get("reviewers", [])),
        "proof requires independent original-evidence review",
    )


def validate_evidence(
    root: Path,
    matrix: dict,
    findings: dict,
    registry: dict,
    profiles: set[str],
    allowlist: set[str],
) -> None:
    """Verify original evidence and exact reviewed source hashes, never synthetic oracles."""
    manifest = matrix["phase1_slice"]
    require(manifest["source_profile"] in profiles, "unknown primary source")
    for proof in manifest["proofs"]:
        require(set(proof["source_profiles"]) <= profiles, "unknown proof source")
        if proof["status"] != "passed":
            continue
        evidence_ids = proof["evidence"]
        require(set(evidence_ids) <= findings.keys(), "proof must cite original findings")
        covered_sources = set()
        for key in evidence_ids:
            original_evidence(findings[key], proof["id"])
            covered_sources.update(findings[key]["source_profiles"])
        require(
            set(proof["source_profiles"]) <= covered_sources, "unproved required source profile"
        )
        for record in proof["reconstruction"]:
            checked_source(root, record, allowlist)
    gate = next(g for g in matrix["crosscutting"]["phase_exits"] if g["phase"] == 1)
    if gate["status"] != "passed" and manifest["status"] != "reviewed":
        return
    route = manifest["selection"]["frozen_route"]
    checked_source(root, route, allowlist)
    require(
        route["source_profile"] == manifest["source_profile"]
        and bool(route["original_evidence"])
        and set(route["original_evidence"]) <= findings.keys(),
        "frozen route lacks its original source evidence",
    )
    require(
        bool(gate["evidence"]) and set(gate["evidence"]) <= registry.keys(), "missing exit review"
    )
    for key in gate["evidence"]:
        evidence = registry[key]
        require(
            gate["id"] in evidence.get("covers", [])
            and evidence.get("validation", {}).get("result") == "passed"
            and evidence.get("evidence_kind") == "original_slice_review"
            and set(manifest["review"]["exit_reviewers"]) <= set(evidence.get("reviewers", []))
            and evidence.get("author") not in manifest["review"]["exit_reviewers"],
            "exit requires independent completed slice review",
        )
        checked_source(root, evidence["slice_manifest"], allowlist)
        require(
            evidence["slice_manifest"]["path"] == MANIFEST, "exit reviewed a different manifest"
        )
