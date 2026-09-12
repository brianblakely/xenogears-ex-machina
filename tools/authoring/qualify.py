"""Qualify repository-owned dependency fixtures; never run arbitrary authoring input."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def installed_identities() -> dict:
    """Match npm's installed closure and actual manifests to the reviewed lock.

    This catches stale/wrong installations for trusted local qualification. It is
    not content authentication or a substitute for isolated untrusted execution.
    """
    lock = json.loads((ROOT / "tools/authoring/package-lock.json").read_text())["packages"]
    installed = json.loads((ROOT / "tools/authoring/node_modules/.package-lock.json").read_text())[
        "packages"
    ]
    identities = {}
    for path, record in installed.items():
        if path not in lock:
            raise ValueError("Installed package is absent from the reviewed lock: " + path)
        expected = lock[path]
        if any(
            record.get(key) != expected.get(key) for key in ("version", "resolved", "integrity")
        ):
            raise ValueError("Installed package metadata differs from the reviewed lock: " + path)
        package_file = ROOT / "tools/authoring" / path / "package.json"
        package_bytes = package_file.read_bytes()
        package = json.loads(package_bytes)
        name = path.rsplit("node_modules/", 1)[1]
        if package["name"] != name or package["version"] != expected["version"]:
            raise ValueError("Installed package manifest differs from the reviewed lock: " + path)
        identities[path] = {
            "version": package["version"],
            "integrity": expected["integrity"],
            "package_manifest_sha256": hashlib.sha256(package_bytes).hexdigest(),
        }
    for path, record in lock.items():
        if path and not record.get("optional") and path not in identities:
            raise ValueError("Required reviewed package is not installed: " + path)
    return identities


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="New report path under .local/authoring")
    args = parser.parse_args()
    if not os.environ.get("IN_NIX_SHELL"):
        raise SystemExit("Enter the separate pinned nix/authoring shell first")
    report_root = ROOT / ".local/authoring"
    report_root.mkdir(parents=True, exist_ok=True)
    target = (
        args.output
        or Path(tempfile.mkdtemp(prefix="qualification-", dir=report_root)) / "report.json"
    )
    target = target.resolve()
    if not target.is_relative_to(report_root.resolve()) or target.exists():
        parser.error("Qualification output must be a new path under .local/authoring")
    manifest = json.loads((ROOT / "tools/authoring/package.json").read_text())
    version = subprocess.check_output(["node", "--version"], text=True).strip()
    if version != "v" + manifest["engines"]["node"]:
        raise SystemExit("Node version does not match the reviewed authoring pin")
    npm_version = subprocess.check_output(["npm", "--version"], text=True).strip()
    review = json.loads((ROOT / "docs/authoring/dependencies.json").read_text())
    if npm_version != review["npm_version"]:
        raise SystemExit("npm version does not match the reviewed authoring pin")
    output = ROOT / "tools/authoring/node_modules/.cache/xem/qualification.mjs"
    output.parent.mkdir(parents=True, exist_ok=True)
    commands = [
        [
            "node",
            "tools/authoring/node_modules/typescript/bin/tsc",
            "--noEmit",
            "--project",
            "tools/authoring/tsconfig.json",
        ],
        [
            "node",
            "tools/authoring/node_modules/esbuild/bin/esbuild",
            "tools/authoring/qualification.ts",
            "--bundle",
            "--packages=external",
            "--platform=node",
            "--target=node24",
            "--format=esm",
            "--sourcemap",
            "--outfile=" + output.relative_to(ROOT).as_posix(),
        ],
        ["node", output.relative_to(ROOT).as_posix()],
    ]
    report = {
        "schema_version": 1,
        "scope": "trusted_repository_dependency_fixture_only",
        "node": version,
        "npm": npm_version,
        "status": "failed",
        "commands": [],
        "installed_packages": installed_identities(),
        "inputs": {
            path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
            for path in (
                "nix/authoring/flake.nix",
                "nix/authoring/flake.lock",
                "tools/authoring/package.json",
                "tools/authoring/package-lock.json",
                "tools/authoring/tsconfig.json",
                "tools/authoring/qualification.ts",
                "tools/authoring/qualify.py",
            )
        },
        "native_bridge_tested": False,
        "untrusted_isolation_tested": False,
    }
    target.parent.mkdir(parents=True, exist_ok=True)
    try:
        for command in commands:
            result = subprocess.run(
                command, cwd=ROOT, text=True, capture_output=True, timeout=120, check=False
            )
            report["commands"].append(
                {
                    "command": command,
                    "exit_code": result.returncode,
                    "stdout": result.stdout,
                    "stderr": result.stderr,
                }
            )
            if result.returncode:
                raise RuntimeError(result.stderr or result.stdout or "qualification command failed")
        report["result"] = json.loads(result.stdout)
        report["status"] = "passed"
    finally:
        with target.open("x") as stream:
            stream.write(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["result"], indent=2))
    print("Qualification report: " + target.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
