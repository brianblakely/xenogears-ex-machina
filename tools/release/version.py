"""Reserve one version per master commit; retries reuse the same tag."""
import argparse
import json
import os
import re
import subprocess


def version(tag):
    match = re.fullmatch(r"v(\d+)\.(\d+)(?:\.(\d+))?", tag)
    return tuple(int(n or 0) for n in match.groups()) if match else None


def next_tag(tags):
    versions = [number for tag in tags if (number := version(tag)) is not None]
    if not versions:
        return "v0.5"
    major, minor, patch = max(max(versions), (0, 5, 0))
    return f"v{major}.{minor}.{patch + 1}"


def android_code(tag):
    major, minor, patch = version(tag)
    if major > 2000 or minor > 99 or patch > 9999:
        raise ValueError("version exceeds Android versionCode allocation")
    return major * 1_000_000 + minor * 10_000 + patch


def gh(*arguments, data=None):
    command = ["gh", "api", *arguments]
    if data is not None:
        command += ["--input", "-"]
    return subprocess.run(command, input=json.dumps(data) if data is not None else None,
                          text=True, capture_output=True)


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
    parser.add_argument("--repository", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--preview", action="store_true", help="build a qualification branch without creating a tag")
    args = parser.parse_args()
    tag = reserve(args.repository, args.commit, args.preview)
    values = f"tag={tag}\nandroid_code={android_code(tag)}\n"
    print(values, end="")
    if path := os.environ.get("GITHUB_OUTPUT"):
        with open(path, "a") as output:
            output.write(values)


if __name__ == "__main__":
    main()
