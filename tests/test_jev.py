"""Synthetic research-tool tests. No original game data, network or API key required."""

from __future__ import annotations

import copy
import hashlib
import http.client
import io
import json
import os
import struct
import tempfile
import unittest
import urllib.error
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import Mock, patch

from tools.analysis import jev, jev_broker


def inventory_fixture():
    def table(namespace, opcodes):
        values = [0x80010000] * len(opcodes)
        return {
            "namespace": namespace,
            "table_sha256" if namespace == "sprite" else "sha256": hashlib.sha256(
                struct.pack(f"<{len(values)}I", *values)
            ).hexdigest(),
            "rows": [
                {
                    "opcode": f"0x{opcode:02x}",
                    "handler": "0x80010000",
                    "analysis_status": "unresolved",
                    "native_status": "unimplemented",
                }
                for opcode in opcodes
            ],
        }

    sprite = table("sprite", range(0x8A, 0xFD))
    sprite.update(source_profile="synthetic-profile", executable_sha256="a" * 64)
    sprite["rows"][0x96 - 0x8A]["next_experiment"] = "Review task list initialization and removal"
    return {
        "schema_version": 1,
        "coverage_complete": False,
        "scope": "Synthetic only",
        "reconstruction_status_rule": "No promotion",
        "unknown_instruction_policy": "Fail",
        "unresolved_symbols": [],
        "unknown_formats": [],
        "unverified_behavior": [],
        "field_overlay": {"decoded_sha256": "b" * 64},
        "event_dispatch_tables": [table("primary", range(256)), table("extended", range(256))],
        "sprite_command_dispatch": sprite,
    }


def response(score=0.8):
    return {
        "model": jev.MODEL,
        "answers": {"relevance": {"type": "noul", "noul": score}},
        "usage": {"input_tokens": 30, "output_tokens": 2},
    }


class JevTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.inventory = inventory_fixture()
        self.source = {"profile": "synthetic-profile", "executable": "a" * 64, "overlay": "b" * 64}
        self.report = {
            "status": "dependency_needs_recovery",
            "dependency": "instruction:sprite:0x96",
            "source": self.source,
            "partial_state": "NEVER_UPLOAD_THIS",
            "checkpoints": ["EXPECTED_BYTES"],
            "case": "PRIVATE_CASE",
        }
        self.blocker = jev.blocker_from_report(self.report, self.inventory)
        self.rows = [
            {"id": "one", "excerpt": "task list removal", "private": False, "local_score": 8},
            {"id": "two", "excerpt": "task list creation", "private": False, "local_score": 5},
        ]
        self.write("analysis/recovery.json", self.inventory)
        self.write(".local/run.json", self.report)
        self.write("packaging/source-files.txt", "analysis/formats/tasks.md\n")
        self.write(
            "analysis/formats/tasks.md",
            "# Task lists\nReview task list initialization and removal.\n",
        )

    def write(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value if isinstance(value, str) else json.dumps(value))
        return path

    def rank(self, **kwargs):
        return jev.rank(
            self.root,
            self.blocker,
            "Review task list ownership",
            self.rows,
            online=kwargs.pop("online", True),
            allow_private=kwargs.pop("allow_private", False),
            **kwargs,
        )

    def test_report_projection_does_not_include_game_state_or_promote_status(self):
        before = copy.deepcopy(self.report)
        text = json.dumps(self.blocker)
        for private in ("NEVER_UPLOAD_THIS", "EXPECTED_BYTES", "PRIVATE_CASE"):
            self.assertNotIn(private, text)
        self.assertEqual(self.report, before)
        self.assertEqual(self.blocker["inventory"]["analysis_status"], "unresolved")
        self.assertIn("initialization", self.blocker["inventory"]["next_experiment"])

    def test_inconsistent_comparison_cannot_skip_an_earlier_divergence(self):
        with self.assertRaisesRegex(ValueError, "comparison first"):
            jev.blocker_from_report(
                {**self.report, "comparison": {"status": "behavioral_divergence"}},
                self.inventory,
            )

    def test_conflicting_code_identity_excludes_finding_before_ranking(self):
        self.write(
            "analysis/findings/conflict.json",
            {
                "source_profiles": [self.source["profile"]],
                "locations": [{"profile": self.source["profile"], "executable": "c" * 64}],
                "observation": "task list removal",
            },
        )
        self.write("packaging/source-files.txt", "analysis/findings/conflict.json\n")
        rows, excluded = jev.gather(self.root, self.blocker, "task list", None)
        self.assertEqual(rows, [])
        self.assertEqual(excluded[0]["reason"], "conflicting_code_identity")

    def test_shortlist_and_excerpt_limits_are_enforced(self):
        paths = []
        for index in range(20):
            name = f"analysis/formats/tasks-{index:02d}.md"
            paths.append(name)
            self.write(name, "task list removal with bounded source context\n" * 200)
        self.write("packaging/source-files.txt", "\n".join(paths))
        rows, _ = jev.gather(self.root, self.blocker, "task list", None)
        self.assertEqual(len(rows), jev.LIMIT)
        self.assertTrue(all(len(row["excerpt"]) <= jev.EXCERPT_CHARS for row in rows))
        self.assertTrue(all(row["end_line"] - row["start_line"] < 48 for row in rows))

    def test_source_mismatches_and_missing_coordinates_reject(self):
        for key in self.source:
            report = copy.deepcopy(self.report)
            report["source"][key] = "other" if key == "profile" else "c" * 64
            with self.assertRaisesRegex(ValueError, "source"):
                jev.blocker_from_report(report, self.inventory)
            del report["source"][key]
            with self.assertRaises(ValueError):
                jev.blocker_from_report(report, self.inventory)

    def test_divergence_and_non_dependency_reports_are_not_reinterpreted(self):
        statuses = ("behavioral_divergence", "completed_boundary", "host_timeout", "invalid_input")
        for status in statuses:
            with self.assertRaisesRegex(ValueError, "fix divergence"):
                jev.blocker_from_report({**self.report, "status": status}, self.inventory)

    def test_stale_targets_and_corrupted_inventory_fail(self):
        with self.assertRaisesRegex(ValueError, "Stale"):
            jev.blocker_from_report(
                {**self.report, "recovery": {"table_value": "0x80020000"}}, self.inventory
            )
        self.inventory["sprite_command_dispatch"]["rows"][0]["handler"] = "0x80020000"
        with self.assertRaisesRegex(ValueError, "fingerprint"):
            jev.blocker_from_report(self.report, self.inventory)

    def test_namespaces_and_unlisted_dependencies_remain_distinct(self):
        primary = jev.blocker_from_report(
            {**self.report, "dependency": "instruction:primary:0x96"}, self.inventory
        )
        self.assertNotIn("next_experiment", primary["inventory"])
        unlisted = jev.blocker_from_report(
            {**self.report, "dependency": "symbol:unknown"}, self.inventory
        )
        self.assertEqual(unlisted["inventory"]["analysis_status"], "unlisted")

    def test_allowlist_and_line_exact_excerpts(self):
        self.write("analysis/formats/not-allowed.md", "task list " * 40)
        self.write(".local/secret.txt", "task list " * 40)
        rows, excluded = jev.gather(self.root, self.blocker, "task list", None)
        self.assertEqual(excluded, [])
        self.assertEqual([row["path"] for row in rows], ["analysis/formats/tasks.md"])
        row = rows[0]
        data = (self.root / row["path"]).read_bytes()
        self.assertEqual(row["sha256"], jev.sha(data))
        self.assertEqual(
            row["excerpt"],
            "".join(
                data.decode().splitlines(keepends=True)[row["start_line"] - 1 : row["end_line"]]
            ),
        )
        self.assertEqual(row["scope"], "repository_context_not_qualified_evidence")

    def test_finding_profile_filter_precedes_ranking(self):
        self.write(
            "analysis/findings/foreign.json", {"source_profiles": ["other"], "text": "task list"}
        )
        self.write("packaging/source-files.txt", "analysis/findings/foreign.json\n")
        rows, excluded = jev.gather(self.root, self.blocker, "task list", None)
        self.assertFalse(rows)
        self.assertEqual(excluded[0]["reason"], "different_or_unspecified_profile")

    def manifest(self, source=None, digest=None):
        data = b"// Synthetic C-like export: task list initialization and removal\n"
        self.write(".local/export.c", data.decode())
        self.write(
            ".local/manifest.json",
            {
                "artifacts": [
                    {
                        "path": ".local/export.c",
                        "source": source or self.source,
                        "sha256": digest or jev.sha(data),
                    }
                ]
            },
        )
        return ".local/manifest.json"

    def test_private_identity_is_checked_before_read_and_digest_is_required(self):
        manifest = self.manifest({**self.source, "overlay": "c" * 64})
        (self.root / ".local/export.c").unlink()
        _, excluded = jev.gather(self.root, self.blocker, "task list", manifest)
        self.assertEqual(excluded[-1]["reason"], "source_identity_mismatch")
        manifest = self.manifest(digest="c" * 64)
        with self.assertRaisesRegex(ValueError, "digest mismatch"):
            jev.gather(self.root, self.blocker, "task list", manifest)

    def test_private_excerpt_is_labeled_and_never_silently_uploaded(self):
        rows, _ = jev.gather(self.root, self.blocker, "task list", self.manifest())
        self.rows = rows
        self.assertTrue(any(row["private"] for row in rows))
        with patch.object(jev, "broker_post") as post:
            with self.assertRaisesRegex(ValueError, "allow-private-upload"):
                self.rank()
            post.assert_not_called()
            self.assertEqual(self.rank(online=False)["method"], "local")

    def test_traversal_absolute_hidden_and_symlink_paths_reject(self):
        names = (".", "../secret", "/secret", ".local/../secret", ".local//file", "./file", "a\\b")
        for name in names:
            with self.assertRaises(ValueError):
                jev.safe_path(self.root, name)
        with self.assertRaisesRegex(ValueError, "under .local"):
            jev.safe_path(self.root, "discs/source.bin", private=True)
        (self.root / ".local/link").symlink_to(self.root / "analysis")
        with self.assertRaisesRegex(ValueError, "Symlink"):
            jev.safe_path(self.root, ".local/link/file", private=True)

    def test_offline_and_missing_broker_ignore_agent_credentials(self):
        with (
            patch.dict(os.environ, {"TYPESAFE_API_KEY": "synthetic-must-not-be-read"}),
            patch.object(jev, "broker_post", side_effect=FileNotFoundError) as post,
        ):
            self.assertEqual(self.rank(online=False)["method"], "local")
            post.assert_not_called()
            result = self.rank()
            self.assertEqual(result["method"], "local_fallback")
            self.assertTrue(
                all(d["reason"] == "credential_broker_unconfigured" for d in result["decisions"])
            )

    def test_explicit_environment_override_is_used_without_broker_or_secret_output(self):
        with (
            patch.dict(os.environ, {"typesafe_api_key": "synthetic-explicit-key"}, clear=True),
            patch.object(jev, "post", return_value=response()) as direct,
            patch.object(jev, "broker_post") as broker,
        ):
            result = self.rank(credential_env="typesafe_api_key")
        self.assertEqual(direct.call_count, 2)
        self.assertTrue(
            all(call.args[1] == "synthetic-explicit-key" for call in direct.call_args_list)
        )
        broker.assert_not_called()
        self.assertTrue(
            all(d["transport"] == "environment_credential" for d in result["decisions"])
        )
        self.assertNotIn("synthetic-explicit-key", json.dumps(result))
        for path in (self.root / ".local/jev/cache").glob("*.json"):
            self.assertNotIn("synthetic-explicit-key", path.read_text())

    def test_environment_override_missing_or_failed_never_falls_back_to_broker(self):
        for env, reason in (
            ({}, "credential_environment_unconfigured"),
            ({"typesafe_api_key": "synthetic-key"}, "service_unavailable_or_invalid_response"),
        ):
            with (
                patch.dict(os.environ, env, clear=True),
                patch.object(jev, "post", side_effect=http.client.BadStatusLine("synthetic-key")),
                patch.object(jev, "broker_post") as broker,
            ):
                result = self.rank(credential_env="typesafe_api_key")
            broker.assert_not_called()
            self.assertEqual(result["method"], "local_fallback")
            self.assertTrue(all(d["reason"] == reason for d in result["decisions"]))
            self.assertNotIn("synthetic-key", json.dumps(result))

    def test_valid_broker_ranking_and_cache_replay_without_broker(self):
        def service(payload, *, allow_private):
            self.assertEqual(payload["model"], "jev-1.13.0")
            self.assertEqual(set(payload["questions"]), {"relevance"})
            self.assertNotIn("NEVER_UPLOAD_THIS", json.dumps(payload))
            return response(0.9 if payload["state"]["candidate"]["id"] == "two" else 0.2)

        with (
            patch.dict(os.environ, {"TYPESAFE_API_KEY": "test-secret"}),
            patch.object(jev, "broker_post", side_effect=service) as post,
        ):
            result = self.rank()
            self.assertEqual([row["id"] for row in result["candidates"]], ["two", "one"])
            self.assertEqual(post.call_count, 2)
        with patch.dict(os.environ, {}, clear=True), patch.object(jev, "broker_post") as post:
            cached = self.rank()
            self.assertEqual(cached["candidates"], result["candidates"])
            self.assertTrue(all(d["method"] == "cache" for d in cached["decisions"]))
            post.assert_not_called()
        for path in (self.root / ".local/jev/cache").glob("*.json"):
            self.assertNotIn("test-secret", path.read_text())
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_explicit_file_override_is_used_without_secret_or_path_in_artifacts(self):
        credential = self.write("synthetic-credential.txt", "synthetic-file-key\n")
        with (
            patch.object(jev, "post", return_value=response()) as direct,
            patch.object(jev, "broker_post") as broker,
        ):
            result = self.rank(credential_file=credential)
        broker.assert_not_called()
        self.assertEqual(direct.call_count, 2)
        self.assertTrue(all(call.args[1] == "synthetic-file-key" for call in direct.call_args_list))
        self.assertTrue(all(d["transport"] == "file_credential" for d in result["decisions"]))
        artifacts = [json.dumps(result)]
        artifacts += [p.read_text() for p in (self.root / ".local/jev/cache").glob("*.json")]
        for artifact in artifacts:
            self.assertNotIn("synthetic-file-key", artifact)
            self.assertNotIn(str(credential), artifact)

    def test_malformed_missing_and_oversized_credentials_fail_without_disclosure(self):
        credential = self.root / "synthetic-credential.txt"
        for contents in (
            b"synthetic-secret-\xff",
            b"s" * 4097,
            b"synthetic-secret" + b" " * 4097,
            b"",
            None,
        ):
            if contents is not None:
                credential.write_bytes(contents)
            else:
                credential.unlink()
            with (
                patch.object(jev, "post") as direct,
                patch.object(jev, "broker_post") as broker,
            ):
                result = self.rank(credential_file=credential)
            direct.assert_not_called()
            broker.assert_not_called()
            self.assertEqual(result["method"], "local_fallback")
            self.assertTrue(
                all(d["reason"] == "credential_file_unavailable" for d in result["decisions"])
            )
            self.assertNotIn("synthetic-secret", json.dumps(result))
            self.assertNotIn(str(credential), json.dumps(result))

    def test_private_upload_gate_and_local_mode_precede_credential_file_read(self):
        credential = Mock(spec=Path)
        self.rows[0]["private"] = True
        with self.assertRaisesRegex(ValueError, "allow-private-upload"):
            self.rank(credential_file=credential)
        self.assertEqual(self.rank(online=False, credential_file=credential)["method"], "local")
        credential.open.assert_not_called()

    def test_credential_file_errors_and_source_conflicts_do_not_leak(self):
        credential = Mock(spec=Path)
        credential.open.side_effect = OSError("synthetic-file-secret")
        result = self.rank(credential_file=credential)
        self.assertNotIn("synthetic-file-secret", json.dumps(result))
        with self.assertRaisesRegex(ValueError, "Choose one credential source"):
            self.rank(credential_file=credential, credential_env="typesafe_api_key")

    def test_changed_question_excerpt_or_model_cannot_reuse_cache(self):
        self.rows = self.rows[:1]
        with (
            patch.dict(os.environ, {"TYPESAFE_API_KEY": "test"}),
            patch.object(jev, "broker_post", side_effect=lambda *a, **kw: response()) as post,
        ):
            self.rank()
            self.rows[0]["excerpt"] += " changed"
            self.rank()
            jev.rank(
                self.root,
                self.blocker,
                "different question",
                self.rows,
                online=True,
                allow_private=False,
            )
            with patch.object(jev, "MODEL", "jev-test-pinned"):
                self.rank()
            self.assertEqual(post.call_count, 4)

    def test_corrupted_cache_is_ignored_not_trusted(self):
        self.rows = self.rows[:1]
        with (
            patch.dict(os.environ, {"TYPESAFE_API_KEY": "test"}),
            patch.object(jev, "broker_post", return_value=response()) as post,
        ):
            self.rank()
            path = next((self.root / ".local/jev/cache").glob("*.json"))
            path.write_text('{"request_sha256":"wrong", "response":{}}')
            result = self.rank()
            self.assertEqual(post.call_count, 2)
            self.assertEqual(result["decisions"][0]["cache_warning"], "invalid_cache_ignored")

    def test_partial_failure_preserves_whole_local_order_and_redacts_error(self):
        def service(payload, *, allow_private):
            if payload["state"]["candidate"]["id"] == "one":
                raise OSError("server echoed test-secret")
            return response(0.99)

        with (
            patch.dict(os.environ, {"TYPESAFE_API_KEY": "test-secret"}),
            patch.object(jev, "broker_post", side_effect=service),
        ):
            result = self.rank()
        self.assertEqual(result["method"], "local_fallback")
        self.assertEqual(result["candidates"], self.rows)
        self.assertNotIn("test-secret", json.dumps(result))

    def test_invalid_scores_answer_ids_models_and_usage_reject(self):
        for score in (True, -0.1, 1.1, float("nan"), float("inf"), "0.5", 10**400):
            with self.assertRaises(ValueError):
                jev.checked_response(response(score))
        mutations = (("model", "jev-latest"), ("answers", {}), ("usage", {"input_tokens": True}))
        for field, value in mutations:
            data = response()
            data[field] = value
            with self.assertRaises(ValueError):
                jev.checked_response(data)

    def test_proposals_are_bounded_bound_to_source_and_preserve_ids(self):
        doc = {
            "source": self.source,
            "question": "Which experiment distinguishes A and B?",
            "candidates": [
                {"id": "observe-init", "description": "Observe list initialization"},
                {"id": "repeat", "description": "Repeat an unchanged trace"},
            ],
        }
        self.write(".local/choices.json", doc)
        question, rows = jev.proposed_choices(self.root, ".local/choices.json", self.source)
        with patch.object(jev, "broker_post") as post:
            result = jev.rank(
                self.root,
                self.blocker,
                question,
                rows,
                online=False,
                allow_private=False,
                choices=True,
            )
            post.assert_not_called()
        self.assertEqual([r["id"] for r in result["candidates"]], ["observe-init", "repeat"])
        doc["candidates"][1]["id"] = "observe-init"
        self.write(".local/choices.json", doc)
        with self.assertRaisesRegex(ValueError, "duplicate"):
            jev.proposed_choices(self.root, ".local/choices.json", self.source)

    def test_cli_writes_private_immutable_advisory_packet_and_leaves_inputs_unchanged(self):
        before = (self.root / ".local/run.json").read_bytes()
        args = ["jev", "evidence", "--report", ".local/run.json", "--output", ".local/packet.json"]
        with (
            patch.object(jev, "ROOT", self.root),
            patch("sys.argv", args),
            redirect_stdout(io.StringIO()),
            patch.object(jev, "broker_post") as post,
            redirect_stderr(io.StringIO()),
        ):
            jev.main()
            with self.assertRaises(SystemExit) as stop:
                jev.main()
            self.assertEqual(stop.exception.code, 2)
            post.assert_not_called()
        packet = json.loads((self.root / ".local/packet.json").read_text())
        self.assertEqual(packet["purpose"], "advisory_research_only")
        self.assertEqual(packet["method"], "local")
        self.assertEqual(packet["report_sha256"], jev.sha(before))
        self.assertEqual((self.root / ".local/run.json").read_bytes(), before)
        self.assertEqual((self.root / ".local/packet.json").stat().st_mode & 0o777, 0o600)

    def test_http_wire_shape_timeout_and_redirect_rejection(self):
        class Opener:
            def open(inner, request, timeout):
                self.assertEqual(request.full_url, jev.ENDPOINT)
                self.assertEqual(timeout, jev_broker.TIMEOUT)
                self.assertEqual(request.get_header("Authorization"), "Bearer test")
                self.assertEqual(json.loads(request.data), {"test": 1})
                return io.BytesIO(json.dumps(response()).encode())

        with patch.object(jev_broker.urllib.request, "build_opener", return_value=Opener()):
            self.assertEqual(jev_broker.post({"test": 1}, "test"), response())
        self.assertIsNone(
            jev_broker.NoRedirect().redirect_request(
                None, None, 302, None, {}, "https://other.invalid"
            )
        )

    def test_http_overload_retries_once_and_respects_long_retry_after(self):
        for delay, attempts in (("0", 2), ("99", 1), ("unrecognized", 1)):

            class Opener:
                calls = 0

                def open(inner, *args, delay=delay, **kwargs):
                    inner.calls += 1
                    raise urllib.error.HTTPError(
                        jev.ENDPOINT, 429, "overloaded", {"Retry-After": delay}, None
                    )

            opener = Opener()
            with (
                patch.object(jev_broker.urllib.request, "build_opener", return_value=opener),
                patch.object(jev_broker.time, "sleep"),
            ):
                with self.assertRaisesRegex(ValueError, "HTTP 429"):
                    jev_broker.post({}, "test")
            self.assertEqual(opener.calls, attempts)

    def test_http_response_size_is_bounded(self):
        class Opener:
            def open(inner, *args, **kwargs):
                return io.BytesIO(b" " * (64 * 1024 + 1))

        with patch.object(jev_broker.urllib.request, "build_opener", return_value=Opener()):
            with self.assertRaisesRegex(ValueError, "Oversized"):
                jev_broker.post({}, "test")


if __name__ == "__main__":
    unittest.main()
