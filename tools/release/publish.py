"""Publish only the complete, explicitly named artifact set for one commit."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from tools.release.version import native_changed, version


def published_releases(repository):
    pages = json.loads(subprocess.check_output([
        "gh", "api", f"repos/{repository}/releases", "--paginate", "--slurp",
    ], text=True))
    releases = []
    for page in pages:
        for release in page:
            number = version(release["tag_name"])
            if not release["draft"] and not release["prerelease"] and number:
                releases.append((number, release["tag_name"]))
    return releases


def previous_release(repository, tag, commit):
    candidates = [(number, name) for number, name in published_releases(repository) if number < version(tag)]
    for _, previous in sorted(candidates, reverse=True):
        ancestor = subprocess.run(["git", "merge-base", "--is-ancestor", previous, commit], check=False)
        if ancestor.returncode == 0:
            return previous
        if ancestor.returncode != 1:
            ancestor.check_returncode()
    return None


def current_native_release(repository, tag, commit):
    master = json.loads(subprocess.check_output(["gh", "api", f"repos/{repository}/git/ref/heads/master"], text=True))
    head = master["object"]["sha"]
    if head != commit:
        # A docs-only push can arrive while this app builds. The checkout may
        # predate it; fetch the exact head we are checking, without moving HEAD.
        subprocess.run(["git", "fetch", "--no-tags", "origin", head], check=True)
        ancestor = subprocess.run(["git", "merge-base", "--is-ancestor", commit, head], check=False)
        if ancestor.returncode not in (0, 1):
            ancestor.check_returncode()
        if ancestor.returncode == 1 or native_changed(commit, head):
            return False
    # An older build with identical inputs must not displace a newer release.
    return not any(number > version(tag) for number, _ in published_releases(repository))


def commits_since(previous, commit):
    """The commits since the previous release, newest first: (hash, subject)."""
    log = subprocess.check_output(["git", "log", "--no-merges", "--format=%H%x00%s", f"{previous}..{commit}"],
                                  text=True)
    return [tuple(line.split("\0", 1)) for line in log.splitlines() if line]


# A release body holds at most 125 000 characters; the comparison lists the rest.
MAX_COMMITS = 500


def render_commits(repository, previous, commits):
    shown = commits[:MAX_COMMITS]
    escape = {"&": "&amp;", "<": "&lt;", ">": "&gt;"}
    lines = [f"- {''.join(escape.get(c, c) for c in subject)} "
             f"([`{sha[:7]}`](https://github.com/{repository}/commit/{sha}))" for sha, subject in shown]
    if len(commits) > len(shown):
        lines.append(f"- …and {len(commits) - len(shown)} more in the full comparison.")
    count = f"{len(commits)} commit{'' if len(commits) == 1 else 's'}"
    return f"<details>\n<summary>{count} since {previous}</summary>\n\n" + "\n".join(lines) + "\n\n</details>"


def render_notes(repository, tag, previous, changes, commits=()):
    release_url = f"https://github.com/{repository}/releases"
    if previous:
        changelog = f"Here's what's changed since [{previous}]({release_url}/tag/{previous}):\n\n"
    else:
        changelog = "Welcome to our first release!\n\n"
    changelog += "\n\n".join(changes) if changes else "No player-facing changes were noted for this update."
    if previous:
        changelog += f"\n\n[See all changes](https://github.com/{repository}/compare/{previous}...{tag})."
        if commits:
            changelog += "\n\n" + render_commits(repository, previous, commits)
    owner, project = repository.split("/")
    return Path(__file__).with_name("notes.md").read_text().format(
        tag=tag,
        downloads=f"{release_url}/download/{tag}/xenogears-ex-machina-{tag}-",
        guide=f"https://github.com/{repository}/blob/{tag}/docs/releases.md",
        manual=f"https://github.com/{repository}/blob/{tag}/MANUAL.md",
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
    commits = commits_since(previous, commit) if previous else []
    return render_notes(repository, tag, previous, changes, commits)


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
    latest = str(current_native_release(args.repository, args.tag, args.commit)).lower()
    subprocess.run(["gh", "release", "edit", args.tag, "--repo", args.repository, "--draft=false", f"--latest={latest}",
                    "--notes-file", str(notes)], check=True)


if __name__ == "__main__":
    main()
