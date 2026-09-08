"""Prove clean source-only builds and deterministic Release artifacts without discs."""

from __future__ import annotations

import hashlib
import io
import json
import os
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

from source_archive import ROOT, archive_bytes


def run(command: list[str], cwd: Path, log: Path) -> None:
    with log.open("a") as stream:
        stream.write("COMMAND " + " ".join(command) + "\n")
        stream.flush()
        result = subprocess.run(command, cwd=cwd, stdout=stream, stderr=stream, check=False)
    if result.returncode:
        raise RuntimeError(f"Clean build failed; inspect {log}")


def main() -> None:
    if not os.environ.get("IN_NIX_SHELL"):
        raise SystemExit("Enter the pinned Nix development shell first")
    private = ROOT / ".local/verification"
    private.mkdir(parents=True, exist_ok=True)
    data = archive_bytes()
    if data != archive_bytes():
        raise SystemExit("Repeated source archives differ")
    proof = Path(tempfile.mkdtemp(prefix="proof-", dir=private))
    source_archive = proof / "source.tar.gz"
    source_archive.write_bytes(data)
    results = []
    with tempfile.TemporaryDirectory(prefix="source-only-", dir=private) as temporary:
        work = Path(temporary)
        for label in ("a", "b"):
            checkout = work / label
            checkout.mkdir()
            with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
                archive.extractall(checkout, filter="data")
            source = checkout / "xenogears-ex-machina"
            log = proof / f"source-only-{label}.log"
            log.write_text("")
            run([sys.executable, "tools/repository/validate.py"], source, log)
            run(["cmake", "--preset", "release"], source, log)
            run(["cmake", "--build", "--preset", "release"], source, log)
            run(["ctest", "--preset", "release"], source, log)
            run(
                ["cmake", "--install", "build/release", "--prefix", str(checkout / "install")],
                source,
                log,
            )
            binary = source / "build/release/xem-baseline"
            installed = sorted(
                path.relative_to(checkout / "install").as_posix()
                for path in (checkout / "install").rglob("*")
                if path.is_file()
            )
            expected = ["bin/xem-baseline", "share/licenses/xenogears-ex-machina/LICENSE"]
            if installed != expected:
                raise RuntimeError(f"Unexpected installed files: {installed}")
            results.append(
                {
                    "build": label,
                    "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                    "installed_files": installed,
                    "log": log.relative_to(ROOT).as_posix(),
                    "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest(),
                    "original_data_present": False,
                }
            )
    if results[0]["binary_sha256"] != results[1]["binary_sha256"]:
        raise SystemExit("Independent clean Release binaries are not byte-identical")
    record = {
        "schema_version": 1,
        "source_archive": source_archive.relative_to(ROOT).as_posix(),
        "source_archive_sha256": hashlib.sha256(data).hexdigest(),
        "source_archive_reproducible": True,
        "release_binary_reproducible": True,
        "builds": results,
        "scope": "Same pinned x86_64 Linux Nix environment, two isolated source-only directories; "
        "not cross-platform binary identity or gameplay validation.",
    }
    output = private / "reproducibility.json"
    (proof / "reproducibility.json").write_text(json.dumps(record, indent=2) + "\n")
    output.write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
