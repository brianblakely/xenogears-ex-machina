"""Check the public sources without generated requirements paperwork.

Runs the repository validator, the Python tests and ruff's pyflakes and import
rules (`--select F,I`; the full configured rule set still reports pre-existing
findings, mostly line length, so it is not yet a gate). Inside
`nix develop path:./nix/runtime` (cargo on PATH) it also builds and tests the
Rust workspace in src/runtime and checks the headless dependency tree. The
matching build needs the user's originals and runs separately
(`make -C src/decomp all-split all-verify`, docs/matching.md).
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.reference.host_slots import slot  # noqa: E402

RUNTIME = ROOT / "src/runtime"


def run(command: list[str], cwd: Path = ROOT, env: dict | None = None) -> None:
    print("Running: " + " ".join(command), flush=True)
    if subprocess.run(command, cwd=cwd, env=env, check=False).returncode:
        raise SystemExit(1)


def main() -> None:
    run([sys.executable, "tools/repository/validate.py"])
    run([sys.executable, "-m", "unittest", "discover", "-s", "tests"])
    run(["ruff", "check", "--select", "F,I", "."])
    if shutil.which("cargo"):
        env = dict(os.environ)
        # rustc overflows its default stack compiling some dependencies (i-slint).
        env.setdefault("RUST_MIN_STACK", "67108864")
        if env.get("XEM_LAVAPIPE_ICD"):
            env.setdefault("VK_ICD_FILENAMES", env["XEM_LAVAPIPE_ICD"])
        with slot("build"):
            run(["cargo", "build", "--workspace"], RUNTIME, env)
            run(["cargo", "test", "--workspace"], RUNTIME, env)
        run(["scripts/check-headless-deps.sh"], RUNTIME)
    else:
        print("cargo is not on PATH: Rust checks skipped (use nix develop path:./nix/runtime)")
    print("Public checks passed; no original-game match is claimed.")


if __name__ == "__main__":
    main()
