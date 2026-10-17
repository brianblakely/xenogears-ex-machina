"""Publish only the complete, explicitly named artifact set for one commit."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def expected(tag):
    prefix = f"xenogears-ex-machina-{tag}-"
    artifacts = {prefix + suffix for suffix in [
        "linux-x86_64.AppImage", "linux-aarch64.AppImage", "windows-x86_64.zip",
        "macos-x86_64.zip", "macos-aarch64.zip", "android-horizon.apk", "web.zip", "source.tar.gz",
    ]} | {"THIRD_PARTY_NOTICES.txt"}
    artifacts.update(prefix + platform + "-runtime-sources.tar.gz" for platform in [
        "linux-x86_64", "linux-aarch64", "windows-x86_64", "macos-x86_64", "macos-aarch64",
    ])
    return artifacts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["tag", "commit", "repository", "artifacts"]:
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    root = Path(args.artifacts)
    files = []
    for name in sorted(expected(args.tag)):
        matches = [path for folder in root.glob("release-*") if folder.name != "release-inputs"
                   for path in folder.glob(name) if path.is_file() and not path.is_symlink()]
        if len(matches) != 1:
            raise SystemExit(f"expected one {name}, found {len(matches)}")
        files += matches
    hashes = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in files)
    checksums = root / "SHA256SUMS"
    checksums.write_text(hashes)
    files.append(checksums)
    view = subprocess.run(["gh", "release", "view", args.tag, "--repo", args.repository, "--json", "isDraft"],
                          capture_output=True, text=True)
    if view.returncode == 0 and not json.loads(view.stdout)["isDraft"]:
        # An already public version is immutable. A retry must match it exactly.
        previous = root / "previous"
        previous.mkdir(exist_ok=True)
        subprocess.run(["gh", "release", "download", args.tag, "--repo", args.repository, "--pattern", "SHA256SUMS",
                        "--dir", str(previous), "--clobber"], check=True)
        if (previous / "SHA256SUMS").read_text() != hashes:
            raise SystemExit("published release differs; refusing to replace it")
        return
    notes = root / "notes.md"
    notes.write_text(
        f"Portable Xenogears Ex Machina {args.tag}, built from `{args.commit}`.\n\n"
        "Place the desktop executable beside your own disc images. First launch organizes them into `discs/`; "
        "memory cards and save states use `cards/` and `saves/`. Android and Horizon OS share the signed APK.\n\n"
        "[Browser play](https://brianblakely.github.io/xenogears-ex-machina/) · "
        f"[Installation, supported platforms and qualification limits](https://github.com/{args.repository}/blob/{args.tag}/docs/releases.md)\n\n"
        "`SHA256SUMS` covers all downloads. No original game images, BIOS, extracted assets or personal saves are included.\n"
    )
    if view.returncode != 0:
        subprocess.run(["gh", "release", "create", args.tag, "--repo", args.repository, "--verify-tag", "--draft",
                        "--title", args.tag, "--notes-file", str(notes)], check=True)
    subprocess.run(["gh", "release", "upload", args.tag, "--repo", args.repository, "--clobber", *map(str, files)], check=True)
    master = json.loads(subprocess.check_output(["gh", "api", f"repos/{args.repository}/git/ref/heads/master"], text=True))
    latest = "true" if master["object"]["sha"] == args.commit else "false"
    subprocess.run(["gh", "release", "edit", args.tag, "--repo", args.repository, "--draft=false", f"--latest={latest}",
                    "--notes-file", str(notes)], check=True)


if __name__ == "__main__":
    main()
