"""Optional research ranking; never a behavioral oracle or a Phase 1 gate.

Local retrieval is the default. --online permits bounded TypeSafe requests;
private inputs additionally require --allow-private-upload. No game is executed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path, PurePosixPath

from tools.analysis.jev_broker import (
    CHOICE_QUESTION,
    ENDPOINT,
    MODEL,
    QUESTION,
    broker_post,
    canonical,
    checked_response,
    post,
    require,
)
from tools.repository.recovery import GROUPS, REFERENCE, entries

ROOT = Path(__file__).resolve().parents[2]
LIMIT = 12
EXCERPT_CHARS = 3500
FILE_BYTES = 512 * 1024
STOP_WORDS = set(
    "a an and are as at be by for from in is it of on or that the this to with".split()
)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_bytes(path: Path, limit: int = FILE_BYTES) -> bytes:
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    require(len(data) <= limit, f"Input exceeds {limit} bytes: {path.name}")
    return data


def safe_path(root: Path, name: str, *, private: bool = False) -> Path:
    """Do not follow links or let a manifest escape the repository."""
    require(isinstance(name, str) and bool(name), "Missing repository-relative path")
    parts = PurePosixPath(name).parts
    require(
        bool(parts)
        and not name.startswith("/")
        and "\\" not in name
        and all(p not in (".", "..") for p in parts)
        and PurePosixPath(name).as_posix() == name,
        "Path must be normalized and repository-relative",
    )
    require(not private or parts[0] == ".local", "Private artifacts must be under .local/")
    path = root.resolve()
    for part in parts:
        path /= part
        require(not path.is_symlink(), "Symlink paths are not accepted")
    return path


def identity(source: object) -> dict:
    require(isinstance(source, dict), "Missing original source identity")
    result = {key: source.get(key) for key in ("profile", "executable", "overlay")}
    require(
        isinstance(result["profile"], str)
        and re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,100}", result["profile"]) is not None,
        "Invalid source profile",
    )
    for key in ("executable", "overlay"):
        require(
            isinstance(result[key], str) and re.fullmatch(r"[0-9a-f]{64}", result[key]) is not None,
            f"Missing {key} SHA256",
        )
    return result


def blocker_from_report(report: dict, inventory: dict) -> dict:
    require(isinstance(report, dict), "Expected an execution report")
    require(
        report.get("status") in ("dependency_needs_recovery", "dependency_not_connected"),
        "Use an encountered dependency report; fix divergence or invalid input first",
    )
    comparison = report.get("comparison")
    require(
        comparison is None
        or (isinstance(comparison, dict) and comparison.get("status") == "matched"),
        "Fix the report comparison first",
    )
    dependency = report.get("dependency")
    require(
        isinstance(dependency, str) and REFERENCE.fullmatch(dependency) is not None,
        "Missing namespaced execution dependency",
    )
    source = identity(report.get("source"))
    sprite = inventory.get("sprite_command_dispatch", {})
    expected = identity(
        {
            "profile": sprite.get("source_profile"),
            "executable": sprite.get("executable_sha256"),
            "overlay": inventory.get("field_overlay", {}).get("decoded_sha256"),
        }
    )
    require(source == expected, "Report does not match the current reconstruction source")
    indexed = entries(inventory)
    row = dict(indexed.get(dependency, {"analysis_status": "unlisted"}))
    originals = {
        f"{category}:{item['id']}": item
        for group, category in GROUPS.items()
        for item in inventory[group]
    }
    for table in [*inventory["event_dispatch_tables"], sprite]:
        for item in table["rows"]:
            originals[f"instruction:{table['namespace']}:{item['opcode']}"] = item
    original = originals.get(dependency, {})
    for key in ("name", "next_experiment", "cpp_reconstruction", "scope"):
        if key in original:
            row[key] = original[key]
    recorded = report.get("recovery", {})
    require(isinstance(recorded, dict), "Invalid report recovery metadata")
    if recorded.get("table_value") is not None:
        require(recorded["table_value"] == row.get("table_value"), "Stale report table target")
    # Never send checkpoints, partial_state, case data, or arbitrary report fields.
    return {
        "dependency": dependency,
        "source": source,
        "status": report["status"],
        "inventory": row,
    }


def tokens(text: str) -> set[str]:
    text = re.sub(r"\b0x([0-9a-f]+)\b", r"\1", text.lower())
    return set(re.findall(r"[a-z0-9]+", text)) - STOP_WORDS


def overlap(text: str, terms: set[str]) -> int:
    return sum(8 if re.fullmatch(r"[0-9a-f]{8}", term) else 1 for term in tokens(text) & terms)


def excerpt(text: str, terms: set[str]) -> tuple[str, int, int, int]:
    """Return one exact line window, not a generated summary."""
    lines = text.splitlines(keepends=True)
    best = ("", 0, 0, -1)
    for start in range(0, len(lines), 16):
        window = ""
        end = start
        for line in lines[start : start + 48]:
            if len(window) + len(line) > EXCERPT_CHARS:
                break
            window += line
            end += 1
        score = overlap(window, terms)
        if window and score > best[3]:
            best = (window, start + 1, end, score)
    return best


def candidate(path: str, data: bytes, text: str, start: int, end: int, **metadata) -> dict:
    key = {"path": path, "sha256": sha(data), "start_line": start, "end_line": end}
    return {"id": sha(canonical(key))[:20], **key, "excerpt": text, **metadata}


def gather(root: Path, blocker: dict, question: str, manifest: str | None) -> tuple[list, list]:
    detail = blocker["inventory"]
    terms = tokens(
        question
        + " "
        + blocker["dependency"]
        + " "
        + json.dumps(
            {
                key: detail[key]
                for key in ("name", "next_experiment", "table_value", "cpp_reconstruction")
                if key in detail
            }
        )
    )
    if "next_experiment" not in detail and not blocker["dependency"].startswith("instruction:"):
        terms |= tokens(detail.get("detail", ""))
    allowed = read_bytes(safe_path(root, "packaging/source-files.txt")).decode().splitlines()
    linked = {f"analysis/findings/{item}.json" for item in blocker["inventory"].get("evidence", [])}
    linked.add(blocker["inventory"].get("cpp_reconstruction", ""))
    rows, excluded = [], []
    for name in sorted(set(allowed)):
        if not (
            name.startswith(
                (
                    "analysis/formats/",
                    "analysis/findings/",
                    "src/reconstruction/",
                    "include/xem/reconstruction/",
                    "tools/analysis/",
                )
            )
            or name
            in (
                "docs/executable-reconstruction.md",
                "docs/reverse-engineering.md",
                "docs/phase1-progress.md",
            )
        ) or name in ("tools/analysis/jev.py", "tools/analysis/jev_broker.py"):
            continue
        path = safe_path(root, name)
        try:
            data = read_bytes(path)
            text = data.decode("utf-8")
            if name.startswith("analysis/findings/"):
                finding = json.loads(text)
                if blocker["source"]["profile"] not in finding.get("source_profiles", []):
                    excluded.append({"path": name, "reason": "different_or_unspecified_profile"})
                    continue
                conflicting = False
                for location in finding.get("locations", []):
                    if (
                        not isinstance(location, dict)
                        or location.get("profile") != blocker["source"]["profile"]
                    ):
                        continue
                    overlay = location.get("overlay")
                    if isinstance(overlay, dict):
                        overlay = overlay.get("decoded_sha256")
                    if location.get("executable") not in (
                        None,
                        blocker["source"]["executable"],
                    ) or overlay not in (None, blocker["source"]["overlay"]):
                        conflicting = True
                if conflicting:
                    excluded.append({"path": name, "reason": "conflicting_code_identity"})
                    continue
        except (OSError, ValueError):
            excluded.append({"path": name, "reason": "unavailable_oversized_or_nontext"})
            continue
        text, start, end, score = excerpt(text, terms)
        score += overlap(name, terms) + (12 if name in linked else 0)
        if not text:
            excluded.append({"path": name, "reason": "no_bounded_text_window"})
        if text and score > 0:
            rows.append(
                candidate(
                    name,
                    data,
                    text,
                    start,
                    end,
                    local_score=score,
                    private=False,
                    scope="repository_context_not_qualified_evidence",
                )
            )
    if manifest:
        document = json.loads(read_bytes(safe_path(root, manifest, private=True)))
        require(isinstance(document, dict) and set(document) == {"artifacts"}, "Invalid manifest")
        artifacts = document["artifacts"]
        require(
            isinstance(artifacts, list) and len(artifacts) <= 64,
            "At most 64 private artifacts",
        )
        seen = set()
        for item in artifacts:
            require(
                isinstance(item, dict) and set(item) == {"path", "sha256", "source"},
                "Each artifact needs path, sha256 and source",
            )
            name = item["path"]
            path = safe_path(root, name, private=True)
            require(name not in seen, "Duplicate artifact path")
            seen.add(name)
            # Identity comparison precedes reading any private bytes.
            if identity(item["source"]) != blocker["source"]:
                excluded.append({"path": name, "reason": "source_identity_mismatch"})
                continue
            data = read_bytes(path)
            require(sha(data) == item["sha256"], "Private artifact digest mismatch")
            text, start, end, score = excerpt(data.decode("utf-8"), terms)
            require(bool(text), "Private artifact has no bounded text window")
            rows.append(
                candidate(
                    name,
                    data,
                    text,
                    start,
                    end,
                    local_score=score,
                    private=True,
                    scope="manifest_bound_not_independently_qualified",
                    source=blocker["source"],
                )
            )
    rows.sort(key=lambda row: (-row["local_score"], row["path"]))
    return rows[:LIMIT], excluded


def proposed_choices(root: Path, name: str, source: dict) -> tuple[str, list]:
    document = json.loads(read_bytes(safe_path(root, name, private=True)))
    require(
        isinstance(document, dict) and set(document) == {"source", "question", "candidates"},
        "Choices need source, question and candidates",
    )
    require(identity(document["source"]) == source, "Choice source identity mismatch")
    question = document["question"]
    require(isinstance(question, str) and 0 < len(question.strip()) <= 4000, "Invalid question")
    rows = document["candidates"]
    require(isinstance(rows, list) and 1 <= len(rows) <= LIMIT, "Supply 1..12 proposed choices")
    result, seen = [], set()
    for index, row in enumerate(rows):
        require(isinstance(row, dict) and set(row) == {"id", "description"}, "Invalid choice")
        key, text = row["id"], row["description"]
        require(
            isinstance(key, str)
            and re.fullmatch(r"[a-z0-9_-]{1,64}", key) is not None
            and key not in seen,
            "Invalid or duplicate choice ID",
        )
        require(
            isinstance(text, str) and 0 < len(text.strip()) <= EXCERPT_CHARS,
            "Invalid choice description",
        )
        seen.add(key)
        result.append(
            {
                "id": key,
                "excerpt": text,
                "private": True,
                "scope": "authored_proposal_not_evidence",
                "local_score": len(rows) - index,
            }
        )
    return question, result


def atomic_cache(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(dir=path.parent, prefix=".jev-")
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(canonical(value))
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def rank(
    root: Path,
    blocker: dict,
    question: str,
    rows: list,
    *,
    online: bool,
    allow_private: bool,
    choices: bool = False,
    credential_env: str | None = None,
    credential_file: Path | None = None,
) -> dict:
    require(
        not online or allow_private or not any(row["private"] for row in rows),
        "Private snippets/proposals require --allow-private-upload before any API request",
    )
    if not online or not rows:
        return {"method": "local", "candidates": rows, "decisions": []}
    require(credential_env in (None, "typesafe_api_key"), "Unsupported credential environment")
    require(not (credential_env and credential_file), "Choose one credential source")
    rubric = CHOICE_QUESTION if choices else QUESTION
    # Explicit operator override only. This route does not provide credential isolation.
    api_key = os.environ.get(credential_env, "") if credential_env else None
    if credential_file is not None:
        # Explicit operator override. Read only for authentication, never evidence.
        # Keep decoding and filesystem exception contents out of diagnostics.
        try:
            with credential_file.open("rb") as stream:
                secret = stream.read(4097)
            require(len(secret) <= 4096, "Invalid credential")
            secret = secret.strip()
            require(
                0 < len(secret) <= 4096 and all(33 <= b <= 126 for b in secret),
                "Invalid credential",
            )
            api_key = secret.decode("ascii")
        except Exception:
            api_key = ""
    direct_credential = credential_env is not None or credential_file is not None

    def evaluate(row: dict) -> dict:
        payload = {
            "model": MODEL,
            "state": {
                "blocker": blocker,
                "question": question,
                "candidate": {k: v for k, v in row.items() if k != "local_score"},
            },
            "questions": {"relevance": rubric},
        }
        request_hash = sha(canonical({"endpoint": ENDPOINT, "request": payload}))
        path = safe_path(root, f".local/jev/cache/{request_hash}.json", private=True)
        cache_warning = None
        try:
            if path.exists():
                cached = json.loads(read_bytes(path))
                require(cached.get("request_sha256") == request_hash, "Cache identity mismatch")
                response = checked_response(cached["response"])
                return {
                    "id": row["id"],
                    "request_sha256": request_hash,
                    "method": "cache",
                    **response,
                }
        except (OSError, ValueError, KeyError, TypeError, AttributeError):
            cache_warning = "invalid_cache_ignored"
        result = {"id": row["id"], "request_sha256": request_hash}
        if cache_warning:
            result["cache_warning"] = cache_warning
        if direct_credential and not api_key:
            return {
                **result,
                "method": "local",
                "reason": "credential_file_unavailable"
                if credential_file is not None
                else "credential_environment_unconfigured",
            }
        try:
            response = checked_response(
                post(payload, api_key)
                if direct_credential
                else broker_post(payload, allow_private=allow_private)
            )
        except FileNotFoundError:
            return {
                **result,
                "method": "local",
                "reason": "service_unavailable_or_invalid_response"
                if direct_credential
                else "credential_broker_unconfigured",
            }
        except Exception:
            # Service errors may contain request text or secrets; never copy them to reports.
            return {
                **result,
                "method": "local",
                "reason": "service_unavailable_or_invalid_response",
            }
        try:
            atomic_cache(path, {"request_sha256": request_hash, "response": response})
        except OSError:
            result["cache_warning"] = "cache_write_failed"
        return {
            **result,
            "method": "jev",
            **response,
            "transport": (
                "file_credential"
                if credential_file is not None
                else "environment_credential"
                if credential_env
                else "isolated_broker"
            ),
        }

    with ThreadPoolExecutor(max_workers=4) as pool:
        decisions = list(pool.map(evaluate, rows))
    # Never mix local keyword scores and model probabilities into a single scale.
    complete = all(item["method"] in ("cache", "jev") for item in decisions)
    ranked = list(rows)
    if complete:
        scores = {item["id"]: item["answers"]["relevance"]["noul"] for item in decisions}
        ranked.sort(key=lambda row: (-scores[row["id"]], -row["local_score"], row["id"]))
    return {
        "method": "jev" if complete else "local_fallback",
        "candidates": ranked,
        "decisions": decisions,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("evidence", "choose"))
    parser.add_argument("--report", required=True, help="Full tools.analysis.execution report")
    parser.add_argument("--output", required=True, help="New JSON packet under .local/")
    parser.add_argument("--manifest", help="Private artifact manifest under .local/")
    parser.add_argument("--choices", help="Authored experiment/Ghidra proposals under .local/")
    parser.add_argument("--question", help="Evidence query; choose reads its own question")
    parser.add_argument("--online", action="store_true", help="Permit TypeSafe API requests")
    parser.add_argument(
        "--credential-env",
        choices=("typesafe_api_key",),
        help="Explicit operator override: use this agent environment credential; "
        "does not provide credential isolation",
    )
    parser.add_argument(
        "--credential-file",
        type=Path,
        help="Explicit operator override: local key file; not credential isolation",
    )
    parser.add_argument("--allow-private-upload", action="store_true")
    args = parser.parse_args()
    try:
        require(
            (args.command == "choose") == bool(args.choices),
            "choose requires --choices; evidence does not accept it",
        )
        require(not args.credential_env or args.online, "--credential-env requires --online")
        require(not args.credential_file or args.online, "--credential-file requires --online")
        require(not args.manifest or args.command == "evidence", "--manifest is for evidence")
        require(
            args.command == "evidence" or args.question is None,
            "Put the choice question in --choices",
        )
        if args.question is not None:
            require(
                0 < len(args.question.strip()) <= 4000,
                "Question must contain 1..4000 characters",
            )
        output = safe_path(ROOT, args.output, private=True)
        require(not output.exists(), "Output must be new; preserve prior packets")
        report_data = read_bytes(safe_path(ROOT, args.report, private=True), 64 * 1024 * 1024)
        inventory_data = read_bytes(safe_path(ROOT, "analysis/recovery.json"), 2 * 1024 * 1024)
        blocker = blocker_from_report(json.loads(report_data), json.loads(inventory_data))
        if args.command == "choose":
            question, rows = proposed_choices(ROOT, args.choices, blocker["source"])
            excluded = []
        else:
            question = args.question or "What existing material resolves this dependency?"
            rows, excluded = gather(ROOT, blocker, question, args.manifest)
        result = rank(
            ROOT,
            blocker,
            question,
            rows,
            online=args.online,
            allow_private=args.allow_private_upload,
            choices=args.command == "choose",
            credential_env=args.credential_env,
            credential_file=args.credential_file,
        )
        packet = {
            "purpose": "advisory_research_only",
            "command": args.command,
            "report_sha256": sha(report_data),
            "inventory_sha256": sha(inventory_data),
            "tool_sha256": sha(Path(__file__).read_bytes()),
            "blocker": blocker,
            "question": question,
            "excluded": excluded,
            **result,
            "limits": {"shortlist": LIMIT, "excerpt_characters": EXCERPT_CHARS},
            "scope": (
                "Ranking is not source qualification, behavioral validation, or proof. "
                "Proposals are never executed. Local fallback preserves the original order."
            ),
        }
        output.parent.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "w") as stream:
            json.dump(packet, stream, indent=2, allow_nan=False)
            stream.write("\n")
        print(
            json.dumps(
                {
                    "packet": args.output,
                    "method": result["method"],
                    "candidates": [row["id"] for row in result["candidates"]],
                },
                indent=2,
            )
        )
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        parser.exit(2, f"Jev research input: {error}\n")


if __name__ == "__main__":
    main()
