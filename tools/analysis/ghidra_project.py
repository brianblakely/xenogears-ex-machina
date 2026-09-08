"""Import hash-qualified original code into the pinned Ghidra PSX environment.

All extracted bytes, projects, instruction listings and automatic pseudocode stay
private. An automatic analysis report is a review aid, not a completed finding.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

from .packed import decode_block
from .verify_field import ROOT, file_sha, load_sources, sha

OVERLAY_BASE = 0x8006FAF0
OVERLAY_SHA = "38a1ce829a6f094c505f67143d6ace2d328418c65425a7383991179467e1fdfc"
SCRIPTS = ("PrepareOriginalProgram.java", "ExportOriginalAnalysis.java")


def prepare(raw: Path, profile: str, output: Path) -> dict:
    """Reproduce resident and field-overlay bytes directly from an original disc."""
    sources = load_sources(raw, profile, 23)
    exe = sources["exe"]
    overlay = decode_block(sources["overlay_packed"]).data
    if exe[:8] != b"PS-X EXE" or sha(overlay) != OVERLAY_SHA:
        raise ValueError("Unsupported executable/decoded-overlay identity")
    entry, gp, address, size = struct.unpack_from("<4I", exe, 0x10)
    if address != 0x80010000 or size != len(exe) - 0x800:
        raise ValueError("Unreviewed resident executable layout")
    exe_path, overlay_path = output / "resident.exe", output / "field-overlay.bin"
    exe_path.write_bytes(exe)
    overlay_path.write_bytes(overlay)
    manifest = {
        "schema_version": 1,
        "profile": profile,
        "raw_track_sha256": sources["raw_sha256"],
        "exe_path": str(exe_path),
        "exe_sha256": sha(exe),
        "entry": entry,
        "header_gp": gp,
        "text_address": address,
        "text_size": size,
        "overlay_path": str(overlay_path),
        "overlay_sha256": sha(overlay),
        "overlay_base": OVERLAY_BASE,
        "overlay_record": sources["overlay_record"],
        "scope": "Original resident and decoded field overlay; other RAM is uninitialized.",
    }
    (output / "sources.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def run(raw: Path, profile: str, output: Path, timeout: int) -> dict:
    if not os.environ.get("IN_NIX_SHELL") or not os.environ.get("XEM_PSX_LOADER_DIR"):
        raise ValueError("Enter the pinned nix/ghidra shell first")
    headless = shutil.which("ghidra-analyzeHeadless")
    if headless is None:
        raise ValueError("Pinned Ghidra headless executable unavailable")
    output = output.resolve()
    if not output.is_relative_to((ROOT / ".local").resolve()) or output.exists():
        raise ValueError("Choose a new private directory under .local")
    output.mkdir(parents=True, mode=0o700)
    manifest = prepare(raw, profile, output)
    settings, cache, temporary = (output / name for name in ("settings", "cache", "tmp"))
    for path in (settings, cache, temporary):
        path.mkdir(mode=0o700)
    environment = dict(os.environ)
    environment["JAVA_TOOL_OPTIONS"] = " ".join(
        f"-Dapplication.{name}={path}"
        for name, path in (("settingsdir", settings), ("cachedir", cache), ("tempdir", temporary))
    )
    # Ghidra forbids project path components beginning with a dot. Work in a
    # private temporary project and archive the closed project under .local.
    with tempfile.TemporaryDirectory(prefix="xem-ghidra-", dir="/tmp") as work:
        project = Path(work) / "projects"
        project.mkdir(mode=0o700)
        command = [
            headless,
            str(project),
            "original",
            "-import",
            manifest["exe_path"],
            "-loader",
            "PsxLoader",
            "-processor",
            "PSX:LE:32:default",
            "-scriptPath",
            str(ROOT / "tools/analysis/ghidra"),
            "-preScript",
            SCRIPTS[0],
            str(output / "sources.json"),
            "-postScript",
            SCRIPTS[1],
            str(output / "analysis.json"),
            "-analysisTimeoutPerFile",
            str(timeout),
            "-max-cpu",
            "4",
            "-log",
            str(output / "analysis.log"),
            "-scriptlog",
            str(output / "scripts.log"),
        ]
        (output / "invocation.json").write_text(
            json.dumps(
                {
                    "command": command,
                    "java_tool_options": environment["JAVA_TOOL_OPTIONS"],
                },
                indent=2,
            )
            + "\n"
        )
        with (output / "console.log").open("w") as log:
            completed = subprocess.run(
                command,
                cwd=ROOT,
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=timeout * 3 + 900,
                check=False,
            )
        shutil.copytree(project, output / "closed-project")
    if completed.returncode:
        raise ValueError(f"Ghidra failed ({completed.returncode}); inspect private console.log")
    report = json.loads((output / "analysis.json").read_text())
    if (
        report["profile"] != profile
        or report["language"] != "PSX:LE:32:default"
        or report["loader"] != "PSX Executables Loader"
        or report["overlay_sha256"] != manifest["overlay_sha256"]
    ):
        raise ValueError("Ghidra source qualification did not complete")
    if any(not row.get("complete") or row.get("error") for row in report["decompilation"]):
        raise ValueError("Selected Ghidra function failed; inspect private analysis.json")
    log = (output / "analysis.log").read_text()
    if (
        "Analysis succeeded" not in log
        or "Save succeeded" not in log
        or "MIPS UnAlligned Instruction Fix" not in log
        or "analysis timed out" in log.lower()
        or "Ignoring class 'ghidra.app.plugin.core.analysis.PsxMipsPreAnalyzer'" in log
    ):
        raise ValueError("Expected analysis, PSX analyzer or project-save completion is absent")
    sources = [
        "nix/ghidra/flake.nix",
        "nix/ghidra/flake.lock",
        "tools/analysis/ghidra_project.py",
        *[f"tools/analysis/ghidra/{name}" for name in SCRIPTS],
        "tools/analysis/verify_field.py",
        "tools/analysis/packed.py",
        "tools/reference/inspect_disc.py",
        "analysis/reference-profiles.json",
        "analysis/coverage/source-fingerprints.json",
    ]
    artifacts = [path for path in output.rglob("*") if path.is_file()]
    verification = {
        "schema_version": 1,
        "status": "imported_for_review",
        "profile": profile,
        "functions_identified_automatically": report["functions"],
        "selected_decompilations": len(report["decompilation"]),
        "source_manifest_sha256": file_sha(output / "sources.json"),
        "authored_sources": [{"path": name, "sha256": file_sha(ROOT / name)} for name in sources],
        "tool_environment": {
            "headless": headless,
            "ghidra_home": os.environ["XEM_GHIDRA_INSTALL_DIR"],
            "psx_loader": os.environ["XEM_PSX_LOADER_DIR"],
        },
        "artifacts": [
            {"path": str(path.relative_to(output)), "sha256": file_sha(path)}
            for path in sorted(artifacts)
        ],
        "limits": (
            "Automatic boundaries, names, types and pseudocode require original-source "
            "and execution review. Uninitialized RAM is not an observed state. "
            "No gameplay completion is asserted."
        ),
    }
    (output / "verification.json").write_text(json.dumps(verification, indent=2) + "\n")
    return verification


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--analysis-timeout", type=int, default=180)
    args = parser.parse_args()
    if not 30 <= args.analysis_timeout <= 1800:
        parser.error("Analysis timeout must be between 30 and 1800 seconds")
    report = run(args.raw, args.profile, args.output, args.analysis_timeout)
    print(
        f"Imported {report['profile']}: {report['selected_decompilations']} private review exports"
    )


if __name__ == "__main__":
    main()
