"""Adversarial authoring specification/review tests; no native authoring is simulated."""

from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.authoring.qualify import installed_identities
from tools.repository.authoring_contract import (
    read,
    validate,
    validate_dependencies,
    validate_gates,
    validate_policy,
)
from tools.repository.matrix import ROOT, build_matrix


class AuthoringContractTests(unittest.TestCase):
    def setUp(self):
        self.contract = read(ROOT, "docs/authoring/contract.json")
        self.acceptance = read(ROOT, "docs/authoring/acceptance.json")
        self.review = read(ROOT, "docs/authoring/dependencies.json")
        self.manifest = read(ROOT, "tools/authoring/package.json")
        self.lock = read(ROOT, "tools/authoring/package-lock.json")

    def dependencies(self):
        return validate_dependencies(ROOT, self.review, self.manifest, self.lock)

    def test_contract_matches_current_plan_and_reviewed_complete_lock(self):
        result = validate(ROOT, build_matrix())
        self.assertEqual(result["authoring_gates_defined"], 14)
        self.assertEqual(result["reviewed_authoring_packages"], len(self.lock["packages"]) - 1)

    def test_untrusted_build_cannot_fall_back_to_plain_node_or_acquire_privilege(self):
        for key in (
            "plain_node_fallback",
            "node_vm_is_security_boundary",
            "build_network",
            "loopback_network",
            "content_can_raise_limits",
            "untrusted_generator_debug_authority",
            "untrusted_lua_debug_authority",
        ):
            with self.subTest(key=key):
                changed = copy.deepcopy(self.contract)
                changed["isolation"][key] = True
                with self.assertRaisesRegex(ValueError, "unsafe untrusted-build"):
                    validate_policy(changed)

    def test_untrusted_build_requires_independent_limits_and_protected_mounts(self):
        for key in self.contract["isolation"]["supervisor_limits"]:
            for value in (0, -1, True, "unlimited"):
                with self.subTest(key=key, value=value):
                    changed = copy.deepcopy(self.contract)
                    changed["isolation"]["supervisor_limits"][key] = value
                    with self.assertRaisesRegex(ValueError, "positive independent resource limits"):
                        validate_policy(changed)
        self.contract["isolation"]["writable_mounts"].append("host_home")
        with self.assertRaisesRegex(ValueError, "filesystem policy"):
            validate_policy(self.contract)

    def test_credentials_harness_and_runtime_socket_cannot_be_exposed(self):
        for key in ("credentials", "protected_harness_writes", "runtime_control_socket"):
            changed = copy.deepcopy(self.contract)
            changed["isolation"]["denied"].remove(key)
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "host authority"):
                validate_policy(changed)

    def test_runtime_cannot_require_node_or_silently_retain_build_callbacks(self):
        self.contract["package"]["runtime_forbidden"].remove("node")
        with self.assertRaisesRegex(ValueError, "native runtime boundary"):
            validate_policy(self.contract)
        self.contract = read(ROOT, "docs/authoring/contract.json")
        self.contract["behavior"]["typescript_callbacks_retained"] = True
        with self.assertRaisesRegex(ValueError, "inspectable native progress"):
            validate_policy(self.contract)

    def test_typechecking_and_native_validator_parity_cannot_be_dropped(self):
        self.contract["stages"] = [s for s in self.contract["stages"] if s["id"] != "typecheck"]
        with self.assertRaisesRegex(ValueError, "build stage boundary"):
            validate_policy(self.contract)
        self.contract = read(ROOT, "docs/authoring/contract.json")
        self.contract["parameters"]["native_validator_parity_required"] = False
        with self.assertRaisesRegex(ValueError, "native parity policy"):
            validate_policy(self.contract)

    def test_changed_dependency_pins_require_renewed_review(self):
        self.review["lockfile_sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "dependency review drift"):
            self.dependencies()
        self.review = read(ROOT, "docs/authoring/dependencies.json")
        self.review["packages"].pop()
        with self.assertRaisesRegex(ValueError, "unreviewed or stale transitive"):
            self.dependencies()

    def test_optional_platform_native_packages_are_also_reviewed(self):
        path = next(k for k, v in self.lock["packages"].items() if v.get("optional"))
        self.lock["packages"][path]["version"] = "999.0.0"
        with self.assertRaisesRegex(ValueError, "locked package differs"):
            self.dependencies()

    def test_dependency_scripts_and_production_scope_cannot_sneak_in(self):
        self.review["installation"]["lifecycle_scripts_executed"] = True
        with self.assertRaisesRegex(ValueError, "installation hook execution"):
            self.dependencies()
        self.review = read(ROOT, "docs/authoring/dependencies.json")
        self.review["packages"][0]["scope"] = "shipping_runtime"
        with self.assertRaisesRegex(ValueError, "promoted to runtime"):
            self.dependencies()

    def test_infrastructure_smoke_cannot_promote_unimplemented_native_gates(self):
        self.review["qualification"]["native_bridge_tested"] = True
        with self.assertRaisesRegex(ValueError, "promoted to native"):
            self.dependencies()
        self.acceptance["gates"][0]["status"] = "passed"
        with self.assertRaisesRegex(ValueError, "unimplemented native authoring gate promoted"):
            validate_gates(self.contract, self.acceptance, build_matrix())

    def test_early_and_release_prerequisites_cannot_be_removed(self):
        matrix = build_matrix()
        for collection, identity in (
            ("early_exits", "P02A-EARLY-EXIT"),
            ("phase_exits", "P03-EXIT"),
            ("phase_exits", "P13-EXIT"),
        ):
            changed = copy.deepcopy(matrix)
            gate = next(g for g in changed["crosscutting"][collection] if g["id"] == identity)
            gate["requires_authoring_gates"].clear()
            with (
                self.subTest(identity=identity),
                self.assertRaisesRegex(ValueError, "omits authoring prerequisite"),
            ):
                validate_gates(self.contract, self.acceptance, changed)

    def test_dependency_qualification_rejects_stale_installed_manifests(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            authoring = root / "tools/authoring"
            package = authoring / "node_modules/example"
            package.mkdir(parents=True)
            record = {
                "version": "1.0.0",
                "resolved": "https://registry.npmjs.org/example/-/example-1.0.0.tgz",
                "integrity": "sha512-reviewed",
            }
            lock = {"packages": {"node_modules/example": record}}
            (authoring / "package-lock.json").write_text(json.dumps(lock))
            (authoring / "node_modules/.package-lock.json").write_text(json.dumps(lock))
            manifest = package / "package.json"
            manifest.write_text(json.dumps({"name": "example", "version": "1.0.0"}))
            with patch("tools.authoring.qualify.ROOT", root):
                self.assertEqual(installed_identities()["node_modules/example"]["version"], "1.0.0")
                manifest.write_text(json.dumps({"name": "example", "version": "2.0.0"}))
                with self.assertRaisesRegex(ValueError, "manifest differs"):
                    installed_identities()
                (authoring / "node_modules/.package-lock.json").write_text(
                    json.dumps({"packages": {}})
                )
                with self.assertRaisesRegex(
                    ValueError, "Required reviewed package is not installed"
                ):
                    installed_identities()

    def test_completed_narrow_facets_cannot_bypass_unexecuted_authoring_gates(self):
        for collection, identity in (
            ("early_exits", "P02A-EARLY-EXIT"),
            ("phase_exits", "P03-EXIT"),
        ):
            matrix = build_matrix()
            gate = next(g for g in matrix["crosscutting"][collection] if g["id"] == identity)
            gate["status"] = "passed"
            for row in matrix["requirements"]:
                if row["source_id"] in {f"P02A-T{index:02}" for index in range(35, 40)}:
                    row["status"] = "passed"
            with (
                self.subTest(identity=identity),
                self.assertRaisesRegex(ValueError, "passed without executed authoring gates"),
            ):
                validate_gates(self.contract, self.acceptance, matrix)

    def test_unknown_authoring_prerequisites_are_not_ignored(self):
        matrix = build_matrix()
        matrix["crosscutting"]["early_exits"][0]["requires_authoring_gates"].append(
            "AUTHOR-INVENTED"
        )
        with self.assertRaisesRegex(ValueError, "unknown authoring gate"):
            validate_gates(self.contract, self.acceptance, matrix)

    def test_direct_release_authoring_dependencies_validate_ids_and_execution(self):
        for identity, status, expected_error in (
            ("AUTHOR-INVENTED", "defined", "unknown authoring gate"),
            ("AUTHOR-PLAY", "passed", "passed without executed authoring gates"),
        ):
            matrix = build_matrix()
            release = matrix["crosscutting"]["release_checkpoints"][0]
            release["requires_authoring_gates"] = [identity]
            release["status"] = status
            with (
                self.subTest(identity=identity),
                self.assertRaisesRegex(ValueError, expected_error),
            ):
                validate_gates(self.contract, self.acceptance, matrix)


if __name__ == "__main__":
    unittest.main()
