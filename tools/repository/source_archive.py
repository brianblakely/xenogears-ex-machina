"""Audit and package only an explicit allowlist of independently authored sources."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import tarfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
FORBIDDEN_PARTS = {
    "discs",
    ".local",
    ".git",
    "build",
    "dist",
    "saves",
    "captures",
    "imported",
    "cache",
}
ALLOWED_SUFFIXES = {
    ".md",
    ".txt",
    ".json",
    ".py",
    ".toml",
    ".nix",
    ".lock",
    ".cmake",
    ".cpp",
    ".java",
    ".h",
    ".hpp",
    ".in",
    ".yml",
    ".yaml",
}
ALLOWED_BASENAMES = {"LICENSE", ".gitignore", ".clang-format", ".editorconfig"}
MAX_TEXT_SIZE = 4 * 1024 * 1024


def validate_path(root: Path, name: str) -> Path:
    relative = PurePosixPath(name)
    if not name or "\\" in name or relative.is_absolute() or ".." in relative.parts:
        raise ValueError(f"Unsafe archive path: {name!r}")
    if relative.as_posix() != name or any(part in FORBIDDEN_PARTS for part in relative.parts):
        raise ValueError(f"Private or noncanonical archive path: {name}")
    path = root / name
    for parent in (path, *path.parents):
        if parent == root:
            break
        if parent.is_symlink():
            raise ValueError(f"Symlink cannot enter source archive: {name}")
    if not path.resolve().is_relative_to(root.resolve()) or not path.is_file():
        raise ValueError(f"Missing or escaping archive source: {name}")
    if path.name not in ALLOWED_BASENAMES and path.suffix not in ALLOWED_SUFFIXES:
        raise ValueError(f"Unreviewed source type: {name}")
    if path.stat().st_size > MAX_TEXT_SIZE:
        raise ValueError(f"Unreviewed large source: {name}")
    payload = path.read_bytes()
    if b"\x00" in payload:
        raise ValueError(f"Binary payload is not an authored source file: {name}")
    payload.decode("utf-8", errors="strict")
    return path


def source_files(root: Path = ROOT) -> list[Path]:
    manifest = root / "packaging/source-files.txt"
    names = [
        line for line in manifest.read_text().splitlines() if line and not line.startswith("#")
    ]
    if not names or names != sorted(set(names)):
        raise ValueError("Source allowlist must be nonempty, sorted and unique")
    return [validate_path(root, name) for name in names]


def archive_bytes(root: Path = ROOT) -> bytes:
    payload = io.BytesIO()
    with tarfile.open(fileobj=payload, mode="w", format=tarfile.PAX_FORMAT) as archive:
        for path in source_files(root):
            data = path.read_bytes()
            entry = tarfile.TarInfo("xenogears-ex-machina/" + path.relative_to(root).as_posix())
            entry.size = len(data)
            entry.mode = 0o644
            entry.mtime = 0
            entry.uid = entry.gid = 0
            entry.uname = entry.gname = ""
            archive.addfile(entry, io.BytesIO(data))
    # mtime and filename are explicit, so a repeat build produces the same gzip bytes.
    result = io.BytesIO()
    with gzip.GzipFile(fileobj=result, mode="wb", filename="", mtime=0) as compressed:
        compressed.write(payload.getvalue())
    return result.getvalue()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / "dist/xem-source-baseline.tar.gz")
    args = parser.parse_args()
    files = source_files()
    if args.check:
        print(f"Source boundary audit passed: {len(files)} explicit text sources")
        return
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / "dist").resolve()):
        parser.error("Archive output must be under ignored dist/")
    output.parent.mkdir(parents=True, exist_ok=True)
    data = archive_bytes()
    output.write_bytes(data)
    print(f"{hashlib.sha256(data).hexdigest()}  {output.relative_to(ROOT)} ({len(files)} sources)")


if __name__ == "__main__":
    main()
