"""Synthetic broker protocol tests; these do not assert host credential isolation."""

from __future__ import annotations

import copy
import io
import json
import socket
import struct
import unittest
from contextlib import redirect_stderr
from types import SimpleNamespace
from unittest.mock import Mock, patch

from tools.analysis import jev_broker as broker


def request():
    return {
        "request": {
            "model": broker.MODEL,
            "state": {
                "question": "Which source clarifies ownership?",
                "blocker": {},
                "candidate": {"id": "synthetic", "excerpt": "list ownership", "private": False},
            },
            "questions": {"relevance": broker.QUESTION},
        },
        "allow_private_upload": False,
    }


def response():
    return {
        "model": broker.MODEL,
        "answers": {"relevance": {"type": "noul", "noul": 0.75}},
        "usage": {"input_tokens": 4, "output_tokens": 1},
    }


class BrokerTests(unittest.TestCase):
    def test_only_fixed_ranking_operations_and_model_are_accepted(self):
        original = request()
        self.assertEqual(broker.checked_request(original), original["request"])
        for field, value in (
            ("url", "file:///etc/shadow"),
            ("headers", {"Authorization": "x"}),
            ("command", "cat /etc/shadow"),
            ("path", "/etc/shadow"),
        ):
            for container in ("envelope", "request", "state", "candidate"):
                mutated = copy.deepcopy(original)
                target = mutated if container == "envelope" else mutated["request"]
                if container in ("state", "candidate"):
                    target = target["state"]
                if container == "candidate":
                    target = target["candidate"]
                target[field] = value
                # A candidate path is inert evidence metadata; it is never opened.
                if field == "path" and container == "candidate":
                    continue
                with self.assertRaises(ValueError):
                    broker.checked_request(mutated)
        for field, value in (("model", "jev-latest"), ("questions", {"other": broker.QUESTION})):
            mutated = copy.deepcopy(original)
            mutated["request"][field] = value
            with self.assertRaises(ValueError):
                broker.checked_request(mutated)

    def test_private_upload_and_proposal_permission_are_required(self):
        envelope = request()
        envelope["request"]["state"]["candidate"]["private"] = True
        with self.assertRaisesRegex(ValueError, "Upload not authorized"):
            broker.checked_request(envelope)
        envelope["allow_private_upload"] = True
        broker.checked_request(envelope)
        envelope = request()
        envelope["request"]["questions"] = {"relevance": broker.CHOICE_QUESTION}
        with self.assertRaisesRegex(ValueError, "Proposal upload"):
            broker.checked_request(envelope)

    def test_request_and_input_framing_limits(self):
        envelope = request()
        envelope["request"]["state"]["blocker"]["text"] = "x" * broker.REQUEST_BYTES
        with self.assertRaisesRegex(ValueError, "Oversized"):
            broker.checked_request(envelope)
        connection = Mock()
        connection.recv.return_value = b"x" * 11
        with self.assertRaisesRegex(ValueError, "Oversized"):
            broker.receive(connection, 10)
        connection.recv.return_value = b"{}\n{}\n"
        with self.assertRaisesRegex(ValueError, "framing"):
            broker.receive(connection, 10)
        connection.recv.return_value = b""
        with self.assertRaisesRegex(ValueError, "Truncated"):
            broker.receive(connection, 10)

    def test_kernel_peer_uid_is_checked(self):
        server = object.__new__(broker.Server)
        server.allowed_uid = 1000
        connection = Mock()
        for uid, permitted in ((1000, True), (1001, False), (0, False)):
            connection.getsockopt.return_value = struct.pack("3i", 42, uid, 1000)
            self.assertEqual(server.verify_request(connection, None), permitted)
        connection.getsockopt.assert_called_with(socket.SOL_SOCKET, socket.SO_PEERCRED, 12)
        self.assertEqual(server.max_children, 4)
        self.assertEqual(server.request_queue_size, 4)

    def exercise_handler(self, service):
        connection = Mock()
        connection.recv.return_value = broker.canonical(request()) + b"\n"
        with (
            patch.object(broker, "post", side_effect=service) as post,
            patch.object(broker.signal, "signal"),
            patch.object(broker.signal, "alarm") as alarm,
        ):
            broker.Handler(connection, None, SimpleNamespace(api_key="synthetic-secret"))
        alarm.assert_any_call(broker.DEADLINE)
        alarm.assert_called_with(0)
        post.assert_called_once_with(request()["request"], "synthetic-secret")
        connection.settimeout.assert_called_with(2)
        return json.loads(connection.sendall.call_args.args[0])

    def test_authorized_mock_ranking_returns_only_validated_response(self):
        result = self.exercise_handler(lambda *args: {**response(), "debug": "synthetic-secret"})
        self.assertEqual(result, response())
        self.assertNotIn("synthetic-secret", json.dumps(result))

    def test_failures_never_return_secret_or_upstream_exception(self):
        for error in (
            OSError("upstream echoed synthetic-secret"),
            ValueError("synthetic-secret"),
            TimeoutError("synthetic-secret"),
        ):

            def service(*args, error=error):
                raise error

            self.assertEqual(
                self.exercise_handler(service), {"error": "ranking_unavailable_or_rejected"}
            )
        result = self.exercise_handler(lambda *args: {"error": "synthetic-secret"})
        self.assertNotIn("synthetic-secret", json.dumps(result))

    def test_client_sends_only_envelope_to_fixed_socket(self):
        connection = Mock()
        connection.__enter__ = Mock(return_value=connection)
        connection.__exit__ = Mock(return_value=False)
        connection.recv.return_value = broker.canonical(response()) + b"\n"
        with patch.object(broker.socket, "socket", return_value=connection) as create:
            result = broker.broker_post(request()["request"], allow_private=False)
        create.assert_called_once_with(socket.AF_UNIX, socket.SOCK_STREAM)
        connection.connect.assert_called_once_with(str(broker.SOCKET))
        connection.settimeout.assert_called_once_with(broker.DEADLINE + 2)
        self.assertEqual(json.loads(connection.sendall.call_args.args[0]), request())
        self.assertEqual(result, response())

    def test_same_uid_service_refuses_before_loading_credential(self):
        with (
            patch("sys.argv", ["jev_broker", "--allowed-uid", "1000"]),
            patch.object(broker.os, "getuid", return_value=1000),
            patch.object(broker.Path, "open") as open_file,
            redirect_stderr(io.StringIO()),
            self.assertRaises(SystemExit),
        ):
            broker.main()
        open_file.assert_not_called()


if __name__ == "__main__":
    unittest.main()
