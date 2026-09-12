"""Validate Phase 0 authoring contracts and reviews, never claim native execution."""

from __future__ import annotations

import base64
import hashlib
import json
import re
from pathlib import Path

STAGES = [
    "resolve",
    "parameters",
    "typecheck",
    "transpile",
    "execute",
    "normalize",
    "validate",
    "export",
    "verify_package",
    "publish",
]
EARLY_GATES = {
    "AUTHOR-BUILD",
    "AUTHOR-ISOLATION",
    "AUTHOR-PACKAGE",
    "AUTHOR-GEOMETRY",
    "AUTHOR-MINIMAL",
    "AUTHOR-TWO-ROOM",
    "AUTHOR-MIXED-ASSETS",
    "AUTHOR-PLAY",
    "AUTHOR-REPAIR",
    "AUTHOR-REVISION",
    "AUTHOR-EVENT-STATE",
    "AUTHOR-DELIVERY",
}
EXPANDED_GATES = {"AUTHOR-TOWN", "AUTHOR-EDITORS"}
REVIEWED_LICENSES = {
    "0BSD",
    "MIT",
    "ISC",
    "Apache-2.0",
    "LGPL-3.0-or-later",
    "Apache-2.0 AND LGPL-3.0-or-later",
    "Apache-2.0 AND LGPL-3.0-or-later AND MIT",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError("Authoring contract: " + message)


def read(root: Path, path: str) -> dict:
    return json.loads((root / path).read_text())


def file_hash(root: Path, path: str) -> str:
    return hashlib.sha256((root / path).read_bytes()).hexdigest()


def validate_policy(contract: dict) -> None:
    require(contract["schema_version"] == 1, "unknown contract schema")
    require(contract["contract_version"] == "0.1.0", "unreviewed contract version")
    require(
        contract["implementation_status"] == "specified_not_implemented",
        "specification must not claim an implemented authoring bridge",
    )
    require([stage["id"] for stage in contract["stages"]] == STAGES, "build stage boundary lost")
    require(
        all(
            stage.get(key) for stage in contract["stages"] for key in ("input", "output", "failure")
        ),
        "stage lacks explicit inputs, outputs or failures",
    )
    require(
        set(contract["authoritative_sources"])
        == {"typescript", "explicit_json_parameters", "lua_modules", "source_assets"},
        "authoritative sources drifted",
    )
    require(
        not set(contract["authoritative_sources"]) & set(contract["derived_outputs"]),
        "derived output became a competing authoritative source",
    )
    package = contract["package"]
    require(
        {
            "node",
            "typescript_execution",
            "javascript_gameplay",
            "browser",
            "qt_qml",
            "authoring_tools",
            "geometry_kernel",
            "build_callbacks",
            "runtime_prompt_interpretation",
        }
        <= set(package["runtime_forbidden"]),
        "authoring dependencies leaked across native runtime boundary",
    )
    require(
        package["headless_gpu_allocation"] is False
        and package["glTF_is_gameplay_contract"] is False
        and package["headless_uses_same_collision_and_events"] is True
        and package["failure_retains_previous_package"] is True,
        "native loader/headless/publication boundary lost",
    )
    require(
        contract["parameters"]["unknown_fields"] == "reject"
        and contract["parameters"]["coercion"] is False
        and contract["parameters"]["unrepresentable_schema"] == "reject"
        and contract["parameters"]["native_validator_parity_required"] is True,
        "serializable parameter and native parity policy weakened",
    )
    require(
        contract["behavior"]["typescript_callbacks_retained"] is False
        and contract["behavior"]["introspection_and_snapshots_required"] is True,
        "behavior must use explicit inspectable native progress",
    )
    require(
        contract["service"]["test_package_is_immutable"] is True
        and contract["service"]["protected_acceptance_is_writable"] is False,
        "test revision or protected harness boundary lost",
    )
    isolation = contract["isolation"]
    require(
        isolation["status"] == "required_not_implemented"
        and isolation["boundary"] == "operating_system_or_disposable_vm"
        and isolation["unavailable_action"] == "reject_untrusted_build",
        "untrusted builds require a qualified OS boundary and refusal when unavailable",
    )
    for key in (
        "plain_node_fallback",
        "node_vm_is_security_boundary",
        "build_network",
        "loopback_network",
        "content_can_raise_limits",
        "untrusted_generator_debug_authority",
        "untrusted_lua_debug_authority",
    ):
        require(isolation[key] is False, "unsafe untrusted-build policy: " + key)
    require(
        isolation["dependency_acquisition_separate"] is True
        and isolation["lock_and_integrity_required"] is True
        and isolation["lifecycle_scripts"] == "disabled_unless_separate_reviewed_recipe",
        "dependency acquisition/install boundary lost",
    )
    require(
        set(isolation["writable_mounts"]) == {"private_output", "private_temp"}
        and isolation["source_mount"] == "immutable_read_only_snapshot"
        and isolation["dependency_mount"] == "immutable_read_only_reviewed_closure",
        "worker filesystem policy weakened",
    )
    require(
        {
            "host_home",
            "credentials",
            "ambient_environment",
            "inherited_fds",
            "ssh_agent",
            "runtime_control_socket",
            "container_daemon",
            "display",
            "gpu",
            "undeclared_assets",
            "original_disc_tree",
            "shared_tool_writes",
            "protected_harness_writes",
        }
        <= set(isolation["denied"]),
        "worker host authority exclusion lost",
    )
    limits = isolation["supervisor_limits"]
    require(
        set(limits)
        == {
            "wall_ms",
            "cpu_seconds",
            "memory_bytes",
            "output_bytes",
            "temp_bytes",
            "log_bytes",
            "processes",
            "open_files",
        }
        and all(type(value) is int and value > 0 for value in limits.values()),
        "supervisor requires explicit positive independent resource limits",
    )
    verification = contract["verification"]
    require(
        all(
            verification[key] is True
            for key in (
                "legal_actions_required",
                "initial_debug_setup_separate",
                "corrective_debug_disqualifies_play",
                "acceptance_revision_protected",
            )
        ),
        "ordinary-play or protected acceptance policy weakened",
    )
    require(
        set(verification["early_gate_ids"]) == EARLY_GATES
        and set(verification["expanded_gate_ids"]) == EXPANDED_GATES,
        "source-to-playable acceptance gate omitted",
    )


def validate_dependencies(root: Path, review: dict, manifest: dict, lock: dict) -> int:
    require(review["schema_version"] == 1, "unknown dependency-review schema")
    for label in ("manifest", "lockfile", "toolchain_lock", "toolchain_recipe"):
        require(
            file_hash(root, review[label]) == review[label + "_sha256"],
            "dependency review drift: " + label,
        )
    require(
        manifest.get("private") is True and not manifest.get("dependencies"),
        "authoring package must remain private and build-only",
    )
    require(not manifest.get("scripts"), "unreviewed root package lifecycle/scripts")
    require(lock["lockfileVersion"] == 3, "unknown lockfile format")
    require(
        lock["packages"][""]["devDependencies"] == manifest["devDependencies"],
        "manifest and lock direct dependencies disagree",
    )
    require(
        manifest["engines"]["node"] == review["node_version"]
        and lock["packages"][""]["engines"] == manifest["engines"],
        "Node pin mismatch",
    )
    for name, version in manifest["devDependencies"].items():
        require(
            re.fullmatch(r"\d+\.\d+\.\d+", version) is not None,
            "direct dependency is not exactly pinned",
        )
        require(
            lock["packages"]["node_modules/" + name]["version"] == version,
            "direct locked version mismatch",
        )
    rows = {row["path"]: row for row in review["packages"]}
    require(len(rows) == len(review["packages"]), "duplicate reviewed package path")
    require(set(rows) == set(lock["packages"]) - {""}, "unreviewed or stale transitive package")
    for path, row in rows.items():
        item = lock["packages"][path]
        require(
            path.startswith("node_modules/") and ".." not in path.split("/"),
            "unsafe locked package path",
        )
        require(
            not item.get("link") and item.get("dev") is True,
            "dependency is linked or not build-only",
        )
        require(row["scope"] == "authoring_build_only", "transitive dependency promoted to runtime")
        require(
            all(item.get(key) == row[key] for key in ("version", "resolved", "integrity")),
            "locked package differs from reviewed package: " + path,
        )
        require(
            row["license_expression"] == item.get("license")
            and row["license_expression"] in REVIEWED_LICENSES,
            "package license lacks review: " + path,
        )
        require(
            item["resolved"].startswith("https://registry.npmjs.org/"), "unreviewed registry/source"
        )
        try:
            algorithm, digest = item["integrity"].split("-", 1)
            valid_integrity = (
                algorithm == "sha512" and len(base64.b64decode(digest, validate=True)) == 64
            )
        except (ValueError, TypeError):
            valid_integrity = False
        require(valid_integrity, "invalid package integrity")
        require(
            row["install_policy"] == "all_lifecycle_scripts_disabled"
            and row["review_decision"]
            == (
                "reviewed_for_locked_local_build_tool_acquisition_"
                "not_runtime_or_tool_redistribution"
            ),
            "unreviewed package execution or distribution approval",
        )
        require(
            all(
                row.get(key)
                for key in (
                    "metadata_url",
                    "metadata_sha256",
                    "payload_class",
                    "license_evidence_scope",
                    "redistribution_policy",
                )
            ),
            "incomplete transitive review",
        )
        require(
            re.fullmatch(r"[0-9a-f]{64}", row["metadata_sha256"]) is not None,
            "invalid metadata identity",
        )
    require(
        review["installation"]["lifecycle_scripts_executed"] is False
        and not review["installation"]["reviewed_install_scripts_approved"],
        "installation hook execution lacks separate approval",
    )
    qualification = review["qualification"]
    require(
        all(
            file_hash(root, path) == digest
            for path, digest in qualification["input_hashes"].items()
        ),
        "qualified dependency fixture or toolchain source drifted",
    )
    require(
        qualification["native_bridge_tested"] is False
        and qualification["isolation_tested"] is False,
        "dependency smoke test was promoted to native/isolation evidence",
    )
    require(
        review["license_review_limits"] and review["embedded_payload_reviews"],
        "bundled code obligations omitted",
    )
    return len(rows)


def validate_gates(contract: dict, acceptance: dict, matrix: dict) -> int:
    require(acceptance["schema_version"] == 1, "unknown acceptance schema")
    require(
        acceptance["contract_version"] == contract["contract_version"],
        "gate contract version mismatch",
    )
    gates = {gate["id"]: gate for gate in acceptance["gates"]}
    require(len(gates) == len(acceptance["gates"]), "duplicate authoring gate")
    require(
        set(gates) == EARLY_GATES | EXPANDED_GATES, "source-to-playable acceptance gate omitted"
    )
    tasks = matrix["source_snapshot"]["tasks"]
    for gate in gates.values():
        require(
            set(gate["tasks"]) <= tasks.keys() and gate["tasks"], "gate refers to missing tasks"
        )
        require(
            gate["status"] == "defined" and gate["evidence"] == [],
            "unimplemented native authoring gate promoted",
        )
        require(
            gate["milestone"] == ("early" if gate["id"] in EARLY_GATES else "expanded"),
            "gate milestone mismatch",
        )
        require(
            all(gate.get(key) for key in ("preconditions", "stimulus", "acceptance", "artifacts")),
            "gate missing executable acceptance obligations",
        )
    policy = acceptance["evidence_policy"]
    require(
        policy["phase0_specification_tests_are_runtime_evidence"] is False
        and policy["unrecorded_corrective_debug_is_pass"] is False
        and policy["immutable_package_and_protected_harness_hashes_required"] is True,
        "specification/ordinary-play evidence boundary weakened",
    )
    cross = matrix["crosscutting"]
    exits = {item["id"]: item for item in cross["phase_exits"] + cross["early_exits"]}
    bindings = {**exits, **{item["id"]: item for item in cross["release_checkpoints"]}}
    for phase, gate in bindings.items():
        references = set(gate.get("requires_authoring_gates", []))
        require(
            references <= gates.keys(), "phase exit references unknown authoring gate: " + phase
        )
        if gate["status"] == "passed":
            require(
                all(gates[identity]["status"] == "passed" for identity in references),
                "phase exit passed without executed authoring gates: " + phase,
            )
    required = {
        "P02A-EARLY-EXIT": EARLY_GATES,
        "P03-EXIT": EARLY_GATES,
        "P07-EXIT": EARLY_GATES | {"AUTHOR-TOWN"},
        "P11-EXIT": {"AUTHOR-EDITORS"},
        "P12-EXIT": {"AUTHOR-EDITORS"},
        "P13-EXIT": EARLY_GATES | EXPANDED_GATES,
    }
    for phase, prerequisites in required.items():
        require(
            prerequisites <= set(exits[phase].get("requires_authoring_gates", [])),
            "phase exit omits authoring prerequisite: " + phase,
        )
    return len(gates)


def validate(root: Path, matrix: dict) -> dict:
    contract = read(root, "docs/authoring/contract.json")
    validate_policy(contract)
    dependency_count = validate_dependencies(
        root,
        read(root, "docs/authoring/dependencies.json"),
        read(root, "tools/authoring/package.json"),
        read(root, "tools/authoring/package-lock.json"),
    )
    central = read(root, "docs/dependencies.json")
    require(
        central["authoring_dependency_review"] == "docs/authoring/dependencies.json",
        "central dependency record omits the separate authoring review",
    )
    direct = read(root, "tools/authoring/package.json")["devDependencies"]
    records = {item["name"]: item for item in central["dependencies"]}
    require(
        all(
            records[name]["version"] == version
            and records[name]["scope"] == "authoring_build_tool_only"
            for name, version in direct.items()
        ),
        "authoring dependencies lack separate central scope/pin review",
    )
    require(
        records["cgltf"]["scope"] == "proposed_runtime_dependency_not_linked",
        "native importer was adopted without its implementation gates",
    )
    gate_count = validate_gates(contract, read(root, "docs/authoring/acceptance.json"), matrix)
    return {"authoring_gates_defined": gate_count, "reviewed_authoring_packages": dependency_count}
