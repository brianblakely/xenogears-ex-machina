"""Check CI status and native release inputs, and reserve a version; retries reuse the same tag."""
import argparse
import json
import os
import re
import subprocess
import tomllib
from pathlib import Path

NATIVE_PATHS = [
    "src/decomp/", "src/port/", "src/runtime/crates/",
    "src/runtime/Cargo.toml", "src/runtime/Cargo.lock", "src/runtime/.cargo/",
    "src/runtime/android/", "src/runtime/scripts/android-build.sh",
    "nix/runtime/", "tools/game_module.py", "tools/game_schema.py",
    "packaging/linux.nix", "packaging/recomplauncher.json", "tools/release/package.py", "tools/release/shell.sh",
    ":(exclude)src/runtime/crates/xem-web/",
    ":(exclude)src/runtime/crates/xem-webxr/",
    ":(exclude)src/runtime/crates/xem-host/src/game_absent.rs",
    ":(glob,exclude)**/tests/**", ":(glob,exclude)**/test/**",
    ":(glob,exclude)**/androidTest/**", ":(glob,exclude)**/examples/**",
    ":(glob,exclude)**/benches/**", ":(glob,exclude)**/*.md",
]

CFG_TEST_RE = re.compile(r"cfg\s*\(\s*(?:test|all\s*\([^()]*\btest\b[^()]*\))\s*\)")
ITEM_QUALIFIERS = {"default", "unsafe", "async", "auto"}
ITEM_KEYWORDS = {
    "fn", "mod", "struct", "enum", "union", "trait", "impl",
    "type", "static", "const", "use", "macro_rules",
}


def _is_ident_start(ch):
    return ch.isalpha() or ch == "_"


def _is_ident_continue(ch):
    return ch.isalnum() or ch == "_"


def _skip_non_code(text, i):
    """If text[i:] starts a Rust comment, string, raw string or char literal, return its end index."""
    n = len(text)
    ch = text[i]
    if ch == "/" and i + 1 < n:
        nxt = text[i + 1]
        if nxt == "/":
            end = text.find("\n", i + 2)
            return n if end == -1 else end
        if nxt == "*":
            depth = 1
            j = i + 2
            while j < n and depth > 0:
                if text[j:j + 2] == "/*":
                    depth += 1
                    j += 2
                elif text[j:j + 2] == "*/":
                    depth -= 1
                    j += 2
                else:
                    j += 1
            return j
        return None
    if ch == '"':
        j = i + 1
        while j < n:
            if text[j] == "\\":
                j += 2
            elif text[j] == '"':
                return j + 1
            else:
                j += 1
        return n
    if ch == "'":
        if i + 1 < n and text[i + 1] == "\\":
            j = i + 2
            while j < n and text[j] != "\n":
                if text[j] == "\\":
                    j += 2
                elif text[j] == "'":
                    return j + 1
                else:
                    j += 1
            return None
        if i + 2 < n and text[i + 2] == "'" and text[i + 1] != "\n":
            return i + 3
        return None
    # Raw string literal: r"...", r#"..."#, br#"..."#, cr#"..."#.
    # Raw identifiers like r#type have no '"' immediately after the hashes.
    if ch in ("r", "b", "c") and (i == 0 or not _is_ident_continue(text[i - 1])):
        if ch == "r":
            j = i + 1
        elif i + 1 < n and text[i + 1] == "r":
            j = i + 2
        else:
            return None
        hashes = 0
        while j < n and text[j] == "#":
            hashes += 1
            j += 1
        if j < n and text[j] == '"':
            closing = '"' + "#" * hashes
            end = text.find(closing, j + 1)
            return n if end == -1 else end + len(closing)
    return None


def _skip_ws_and_comments(text, i):
    n = len(text)
    while i < n:
        if text[i].isspace():
            i += 1
        elif text[i] == "/" and (end := _skip_non_code(text, i)) is not None:
            i = end
        else:
            break
    return i


def _parse_outer_attribute(text, i):
    """Parse #[...] starting at i, returning (end_index, inner_text) or None."""
    n = len(text)
    if i >= n or text[i] != "#":
        return None
    j = _skip_ws_and_comments(text, i + 1)
    if j >= n or text[j] != "[":
        return None
    inner_start = j + 1
    j += 1
    depth = 1
    while j < n and depth > 0:
        end = _skip_non_code(text, j)
        if end is not None:
            j = end
            continue
        if text[j] == "[":
            depth += 1
        elif text[j] == "]":
            depth -= 1
        j += 1
    if depth != 0:
        return None
    return j, text[inner_start:j - 1]


def _read_ident(text, i):
    n = len(text)
    if i >= n or not _is_ident_start(text[i]):
        return None, i
    j = i + 1
    while j < n and _is_ident_continue(text[j]):
        j += 1
    return text[i:j], j


def _skip_balanced(text, i, open_ch, close_ch):
    n = len(text)
    if i >= n or text[i] != open_ch:
        return None
    j = i + 1
    depth = 1
    while j < n and depth > 0:
        end = _skip_non_code(text, j)
        if end is not None:
            j = end
            continue
        if text[j] == open_ch:
            depth += 1
        elif text[j] == close_ch:
            depth -= 1
        j += 1
    return j if depth == 0 else None


def _starts_rust_item(text, i):
    """Return True if text[i:] begins a standalone Rust item (not a struct field or expression)."""
    j = _skip_ws_and_comments(text, i)
    word, after = _read_ident(text, j)
    if word == "pub":
        j = _skip_ws_and_comments(text, after)
        if j < len(text) and text[j] == "(":
            j = _skip_balanced(text, j, "(", ")")
            if j is None:
                return False
            j = _skip_ws_and_comments(text, j)
        word, after = _read_ident(text, j)
    while word in ITEM_QUALIFIERS or word == "extern":
        j = _skip_ws_and_comments(text, after)
        if word == "extern" and j < len(text) and text[j] == '"':
            end = _skip_non_code(text, j)
            if end is None:
                return False
            j = _skip_ws_and_comments(text, end)
        word, after = _read_ident(text, j)
    return word in ITEM_KEYWORDS


def _scan_rust_item_end(text, i):
    """Return index immediately after the Rust item starting at i, or None to fail safe."""
    n = len(text)
    j = i
    depth = 0
    saw_equals = False
    while j < n:
        end = _skip_non_code(text, j)
        if end is not None:
            j = end
            continue
        ch = text[j]
        if ch in "([":
            depth += 1
            j += 1
        elif ch in ")]":
            if depth == 0:
                return None
            depth -= 1
            j += 1
        elif ch == "{":
            if depth == 0:
                end = _skip_balanced(text, j, "{", "}")
                if end is None:
                    return None
                j = end
                if not saw_equals:
                    return j
            else:
                depth += 1
                j += 1
        elif ch == "}":
            if depth == 0:
                return None
            depth -= 1
            j += 1
        elif ch == ";" and depth == 0:
            return j + 1
        elif ch == "," and depth == 0:
            return None
        elif ch == "=" and depth == 0:
            prev = text[j - 1] if j > 0 else ""
            nxt = text[j + 1] if j + 1 < n else ""
            if prev not in ("=", "!", "<", ">") and nxt not in ("=", ">"):
                saw_equals = True
            j += 1
        else:
            j += 1
    return None


def strip_rust_cfg_test(source):
    """Remove standalone #[cfg(test)] items so test-only edits compare equal.

    Fails safe by leaving any unrecognised or malformed construct intact.
    """
    n = len(source)
    out = []
    i = 0
    while i < n:
        end = _skip_non_code(source, i)
        if end is not None:
            out.append(source[i:end])
            i = end
            continue
        if source[i] == "#":
            parsed = _parse_outer_attribute(source, i)
            if parsed is not None:
                group_start = i
                has_cfg_test = False
                j = i
                while True:
                    attr = _parse_outer_attribute(source, j)
                    if attr is None:
                        break
                    attr_end, inner = attr
                    if CFG_TEST_RE.fullmatch(inner.strip()):
                        has_cfg_test = True
                    j = _skip_ws_and_comments(source, attr_end)
                    if j >= n or source[j] != "#":
                        break
                if has_cfg_test and _starts_rust_item(source, j):
                    item_end = _scan_rust_item_end(source, j)
                    if item_end is not None:
                        # Trim blank lines/indentation directly preceding the removed item.
                        saw_newline = False
                        while out:
                            tail = out[-1]
                            trimmed = tail.rstrip(" \t\r\n")
                            if "\n" in tail[len(trimmed):]:
                                saw_newline = True
                            if trimmed:
                                out[-1] = trimmed
                                break
                            out.pop()
                        if out and saw_newline:
                            out.append("\n")
                        while item_end < n and source[item_end] in " \t\r":
                            item_end += 1
                        if item_end < n and source[item_end] == "\n":
                            item_end += 1
                        i = item_end
                        continue
                out.append(source[group_start:j])
                i = j
                continue
        out.append(source[i])
        i += 1
    return "".join(out)


def _strip_suite_profiles(source):
    try:
        parsed = tomllib.loads(source)
    except tomllib.TOMLDecodeError:
        return None
    profiles = parsed.get("profile")
    if isinstance(profiles, dict):
        profiles.pop("suite", None)
        profiles.pop("suite-debug", None)
        if not profiles:
            parsed.pop("profile", None)
    return parsed


def _git_blob(rev, path):
    result = subprocess.run(["git", "cat-file", "-p", f"{rev}:{path}"],
                            check=False, capture_output=True)
    if result.returncode != 0:
        return None
    try:
        return result.stdout.decode("utf-8")
    except UnicodeDecodeError:
        return None


def native_changed(base, commit):
    """Compare native game, host and build inputs between two commits.

    Test suites (including inline #[cfg(test)] Rust items, suite Cargo profiles
    and public-CI-only stubs), documentation, browser-only code and publication
    machinery do not change the native app. Keep this separate from suite.sh's
    broader test-selection boundary.
    """
    if base == "0" * 40:
        base = subprocess.check_output(["git", "hash-object", "-t", "tree", "--stdin"],
                                       input="", text=True).strip()
    result = subprocess.run(
        ["git", "diff", "--name-only", "-z", base, commit, "--", *NATIVE_PATHS],
        check=False, capture_output=True, text=True,
    )
    if result.returncode != 0:
        result.check_returncode()
    paths = [path for path in result.stdout.split("\0") if path]
    for path in paths:
        if path.endswith(".rs"):
            old = _git_blob(base, path)
            new = _git_blob(commit, path)
            if old is None or new is None or strip_rust_cfg_test(old) != strip_rust_cfg_test(new):
                return True
        elif path == "src/runtime/Cargo.toml":
            old = _git_blob(base, path)
            new = _git_blob(commit, path)
            if old is None or new is None:
                return True
            old_toml = _strip_suite_profiles(old)
            new_toml = _strip_suite_profiles(new)
            if old_toml is None or new_toml is None or old_toml != new_toml:
                return True
        else:
            return True
    return False


def version(tag):
    match = re.fullmatch(r"v(\d+)\.(\d+)(?:\.(\d+))?", tag)
    return tuple(int(n or 0) for n in match.groups()) if match else None


# Each release takes the highest tag's next minor version (v1.5, v1.6, ...),
# beginning here: the releases before v1.5 advanced the patch version.
FIRST = (1, 5)


def next_tag(tags):
    major, minor, _ = max((number for tag in tags if (number := version(tag)) is not None), default=(0, 0, 0))
    if (major, minor) < FIRST:
        major, minor = FIRST[0], FIRST[1] - 1
    return f"v{major}.{minor + 1}"


def android_code(tag):
    """An APK's versionCode, which rises with the version: a thousand minor
    versions a major one (every published code is below v1.5's: v1.0.12's is
    1000012)."""
    major, minor, patch = version(tag)
    if major > 2000 or minor > 999 or patch > 999:
        raise ValueError("version exceeds Android versionCode allocation")
    return major * 1_000_000 + minor * 1_000 + patch


def gh(*arguments, data=None):
    command = ["gh", "api", *arguments]
    if data is not None:
        command += ["--input", "-"]
    return subprocess.run(command, input=json.dumps(data) if data is not None else None,
                          text=True, capture_output=True)


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


def latest_published_ancestor(repository, commit, before_tag=None):
    releases = published_releases(repository)
    if before_tag is not None:
        limit = version(before_tag)
        releases = [(number, name) for number, name in releases if number < limit]
    for _, previous in sorted(releases, reverse=True):
        ancestor = subprocess.run(["git", "merge-base", "--is-ancestor", previous, commit], check=False)
        if ancestor.returncode == 0:
            return previous
        if ancestor.returncode != 1:
            ancestor.check_returncode()
    return None


def suite_passed_in_run(repository, run_id):
    pages = json.loads(subprocess.check_output([
        "gh", "api", f"repos/{repository}/actions/runs/{run_id}/jobs?per_page=100", "--paginate", "--slurp",
    ], text=True))
    jobs = [job for page in pages for job in page.get("jobs", [])]
    suite_jobs = [job for job in jobs if job["name"] == "suite" or job["name"].startswith("suite (")]
    return bool(suite_jobs) and all(job.get("conclusion") == "success" for job in suite_jobs)


def _runtime_inputs_changed_since(base):
    suite_script = Path(__file__).resolve().parents[2] / "src/runtime/scripts/suite.sh"
    result = subprocess.run([str(suite_script), "--changed", "--base", base], check=False)
    if result.returncode == 0:
        return True
    if result.returncode == 1:
        return False
    result.check_returncode()


def require_ci_passed(repository, commit, ci_run_id=None):
    """Verify CI passed on commit, and that no runtime inputs lack a passing full suite."""
    if ci_run_id is not None:
        run = json.loads(subprocess.check_output([
            "gh", "api", f"repos/{repository}/actions/runs/{ci_run_id}",
        ], text=True))
        if run.get("conclusion") != "success" or run.get("head_sha") != commit:
            raise RuntimeError(f"CI run {ci_run_id} did not pass for {commit}; refusing to publish")
        current_run_id = run["id"]
    else:
        payload = json.loads(subprocess.check_output([
            "gh", "api", f"repos/{repository}/actions/workflows/ci.yml/runs?head_sha={commit}&status=completed&per_page=20",
        ], text=True))
        passing = [run for run in payload.get("workflow_runs", []) if run.get("conclusion") == "success"]
        if not passing:
            raise RuntimeError(f"CI has not passed for {commit}; refusing to publish")
        current_run_id = passing[0]["id"]

    if suite_passed_in_run(repository, current_run_id):
        return

    # CI skipped the platform suite on this commit (no runtime inputs changed
    # against its push base). Confirm no runtime inputs changed since the most
    # recent passing full-suite CI run on master's ancestry.
    payload = json.loads(subprocess.check_output([
        "gh", "api", f"repos/{repository}/actions/workflows/ci.yml/runs?branch=master&status=completed&per_page=100",
    ], text=True))
    for run in payload.get("workflow_runs", []):
        if run.get("conclusion") != "success" or run.get("id") == current_run_id:
            continue
        sha = run.get("head_sha")
        if not sha:
            continue
        ancestor = subprocess.run(["git", "merge-base", "--is-ancestor", sha, commit],
                                  check=False, capture_output=True)
        if ancestor.returncode != 0:
            continue
        if suite_passed_in_run(repository, run["id"]):
            if _runtime_inputs_changed_since(sha):
                raise RuntimeError(
                    f"runtime inputs changed since the last passing full CI suite ({sha[:12]}); refusing to publish"
                )
            return
    raise RuntimeError("no passing full CI suite run found on master ancestry; refusing to publish")


def reserve(repository, commit, preview=False):
    for _ in range(10):
        response = gh(f"repos/{repository}/git/matching-refs/tags/v")
        response.check_returncode()
        refs = json.loads(response.stdout)
        tags = {item["ref"].removeprefix("refs/tags/"): item["object"] for item in refs}
        for tag, obj in tags.items():
            if version(tag) is not None and obj["type"] == "commit" and obj["sha"] == commit:
                return tag
        tag = next_tag(tags)
        if preview:
            return tag
        response = gh(f"repos/{repository}/git/refs", "--method", "POST",
                      data={"ref": f"refs/tags/{tag}", "sha": commit})
        if response.returncode == 0:
            return tag
        if '"Reference already exists"' not in response.stdout:
            raise RuntimeError(response.stderr + response.stdout)
    raise RuntimeError("concurrent releases repeatedly reserved the next version")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--preview", action="store_true", help="build a qualification branch without creating a tag")
    parser.add_argument("--check-native-since", metavar="BASE", help="only check inputs; never reserve a tag")
    parser.add_argument("--check-native-since-release", action="store_true",
                        help="check inputs against the latest published release on this commit's ancestry")
    parser.add_argument("--require-ci", action="store_true",
                        help="refuse to proceed unless CI passed for this commit and its runtime inputs")
    parser.add_argument("--ci-run-id", help="triggering CI workflow run ID to verify")
    args = parser.parse_args()
    if args.check_native_since and args.check_native_since_release:
        parser.error("choose one of --check-native-since and --check-native-since-release")
    if (args.check_native_since_release or args.require_ci) and not args.repository:
        parser.error("--repository is required for release/CI checks")

    if args.check_native_since or args.check_native_since_release:
        if args.check_native_since_release:
            base = latest_published_ancestor(args.repository, args.commit) or ("0" * 40)
        else:
            base = args.check_native_since
        changed = native_changed(base, args.commit)
        if changed and args.require_ci:
            require_ci_passed(args.repository, args.commit, args.ci_run_id)
        values = f"changed={str(changed).lower()}\n"
    elif args.require_ci:
        require_ci_passed(args.repository, args.commit, args.ci_run_id)
        values = ""
    else:
        if not args.repository:
            parser.error("--repository is required to reserve a version")
        tag = reserve(args.repository, args.commit, args.preview)
        values = f"tag={tag}\nandroid_code={android_code(tag)}\n"
    print(values, end="")
    if values and (path := os.environ.get("GITHUB_OUTPUT")):
        with open(path, "a") as output:
            output.write(values)


if __name__ == "__main__":
    main()
