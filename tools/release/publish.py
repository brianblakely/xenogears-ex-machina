"""Publish only the complete, explicitly named artifact set for one commit."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from tools.release.version import version


def previous_release(repository, tag, commit):
    pages = json.loads(subprocess.check_output([
        "gh", "api", f"repos/{repository}/releases", "--paginate", "--slurp",
    ], text=True))
    candidates = []
    for page in pages:
        for release in page:
            number = version(release["tag_name"])
            if not release["draft"] and not release["prerelease"] and number and number < version(tag):
                candidates.append((number, release["tag_name"]))
    for _, previous in sorted(candidates, reverse=True):
        ancestor = subprocess.run(["git", "merge-base", "--is-ancestor", previous, commit], check=False)
        if ancestor.returncode == 0:
            return previous
        if ancestor.returncode != 1:
            ancestor.check_returncode()
    return None


def render_notes(repository, tag, previous, changes):
    release_url = f"https://github.com/{repository}/releases"
    if previous:
        changelog = f"Here's what's changed since [{previous}]({release_url}/tag/{previous}):\n\n"
    else:
        changelog = "Welcome to our first release!\n\n"
    changelog += "\n\n".join(changes) if changes else "No player-facing changes were noted for this update."
    if previous:
        changelog += f"\n\n[See all changes](https://github.com/{repository}/compare/{previous}...{tag})."
    owner, project = repository.split("/")
    return Path(__file__).with_name("notes.md").read_text().format(
        tag=tag,
        downloads=f"{release_url}/download/{tag}/xenogears-ex-machina-{tag}-",
        guide=f"https://github.com/{repository}/blob/{tag}/docs/releases.md",
        browser=f"https://{owner}.github.io/{project}/",
        changelog=changelog,
    )


def release_notes(repository, tag, commit):
    previous = previous_release(repository, tag, commit)
    directory = "docs/changes/"
    if previous:
        command = ["git", "diff", "--name-only", "--diff-filter=A", "-z", previous, commit, "--", directory]
    else:
        command = ["git", "ls-tree", "-r", "--name-only", "-z", commit, "--", directory]
    paths = subprocess.check_output(command, text=True).split("\0")
    changes = []
    for path in sorted(path for path in paths if path.endswith(".md")):
        change = subprocess.check_output(["git", "show", f"{commit}:{path}"], text=True).strip()
        if not change:
            raise ValueError(f"empty release note: {path}")
        changes.append(change)
    return render_notes(repository, tag, previous, changes)


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
    notes.write_text(release_notes(args.repository, args.tag, args.commit))
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
