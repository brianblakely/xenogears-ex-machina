#!/usr/bin/env python3
"""Serve the listening test on this machine (tools/listening/README.md).

    python3 tools/listening/serve.py [--stimuli DIR] [--results DIR] [--port N]

Serves the page (this directory) at http://127.0.0.1:PORT/, the stimuli
(`.local/listening` by default: what `cargo run --release -p xem-headless
--example listening -- localization` and `-- music` wrote) under
/stimuli/, and keeps the page's answers in the results directory
(`.local/listening/results` by default): GET /results/ lists them, GET and
PUT /results/NAME.json read and write one (each answer is saved at once, so
a session can be resumed). It listens on the loopback address only; the
stimuli are the game's and stay on this machine. Python's standard library
only.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[2]
PAGE = Path(__file__).resolve().parent
PAGE_FILES = {"/": "index.html", "/index.html": "index.html", "/listening.js": "listening.js"}
RESULT_NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*\.json$")
TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".json": "application/json",
    ".wav": "audio/wav",
}
LARGEST_RESULT = 16 * 1024 * 1024


def inside(base: Path, relative: str) -> Path | None:
    """The file `relative` names under `base`, or None outside it."""
    path = (base / relative).resolve()
    return path if path.is_relative_to(base.resolve()) else None


def summary(path: Path) -> dict | None:
    """What the session list shows of a results file."""
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict):
        return None
    answered = len(data.get("trials") or []) + len(data.get("ratings") or [])
    return {
        "file": path.name,
        "listener": data.get("listener"),
        "session": data.get("session"),
        "part": data.get("part"),
        "complete": bool(data.get("complete")),
        "answered": answered,
    }


def handler(stimuli: Path, results: Path):
    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, format, *args):  # noqa: A002 - the base class's name
            sys.stderr.write(f"{self.log_date_time_string()} {format % args}\n")

        def send_file(self, path: Path) -> None:
            if not path.is_file():
                self.send_error(HTTPStatus.NOT_FOUND)
                return
            data = path.read_bytes()
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", TYPES.get(path.suffix, "application/octet-stream"))
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def send_json(self, value, status=HTTPStatus.OK) -> None:
            data = json.dumps(value).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):  # noqa: N802 - http.server's name
            path = unquote(urlsplit(self.path).path)
            if path in PAGE_FILES:
                self.send_file(PAGE / PAGE_FILES[path])
            elif path.startswith("/stimuli/"):
                target = inside(stimuli, path[len("/stimuli/") :])
                self.send_file(target) if target else self.send_error(HTTPStatus.FORBIDDEN)
            elif path == "/results/":
                files = sorted(results.glob("*.json")) if results.is_dir() else []
                self.send_json([s for s in map(summary, files) if s])
            elif path.startswith("/results/"):
                name = path[len("/results/") :]
                if not RESULT_NAME.match(name):
                    self.send_error(HTTPStatus.FORBIDDEN)
                    return
                self.send_file(results / name)
            else:
                self.send_error(HTTPStatus.NOT_FOUND)

        def do_PUT(self):  # noqa: N802 - http.server's name
            path = unquote(urlsplit(self.path).path)
            name = path[len("/results/") :] if path.startswith("/results/") else ""
            if not RESULT_NAME.match(name):
                self.send_error(HTTPStatus.FORBIDDEN)
                return
            length = int(self.headers.get("Content-Length") or 0)
            if not 0 < length <= LARGEST_RESULT:
                self.send_error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE)
                return
            body = self.rfile.read(length)
            try:
                value = json.loads(body)
            except ValueError as error:
                self.send_json({"error": f"not JSON: {error}"}, HTTPStatus.BAD_REQUEST)
                return
            if (
                not isinstance(value, dict)
                or value.get("kind") != "xem-listening-results"
                or value.get("file") != name
            ):
                self.send_json({"error": "not this file's results"}, HTTPStatus.BAD_REQUEST)
                return
            results.mkdir(parents=True, exist_ok=True)
            # Written whole, then renamed over the old file.
            handle, temporary = tempfile.mkstemp(dir=results, prefix=".", suffix=".part")
            with os.fdopen(handle, "wb") as out:
                out.write(json.dumps(value, indent=1).encode() + b"\n")
            os.replace(temporary, results / name)
            self.send_json({"saved": name})

    return Handler


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--stimuli", type=Path, default=ROOT / ".local/listening")
    parser.add_argument("--results", type=Path, help="default: STIMULI/results")
    parser.add_argument("--port", type=int, default=8642)
    args = parser.parse_args()
    stimuli = args.stimuli.resolve()
    results = (args.results or stimuli / "results").resolve()
    if not (stimuli / "localization.json").is_file():
        print(
            f"note: {stimuli}/localization.json is missing (README: generating the stimuli)",
            file=sys.stderr,
        )
    server = ThreadingHTTPServer(("127.0.0.1", args.port), handler(stimuli, results))
    print(
        f"listening test: http://127.0.0.1:{server.server_address[1]}/"
        f" (stimuli {stimuli}, results {results})",
        flush=True,
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
