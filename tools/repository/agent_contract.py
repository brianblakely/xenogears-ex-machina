"""Check Phase 0 specification integrity, never emulate a native game or server.

The schema evaluator implements only the declared vocabulary used by the authored
contract. Unknown keywords fail its schema audit; this is not a general-purpose
JSON Schema implementation. External validators can also consume the schema files.
Runtime registration, roles, state invariants and execution need native gate tests.
"""

from __future__ import annotations

import hashlib
import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KEYWORDS = {
    "$schema",
    "$defs",
    "$ref",
    "title",
    "type",
    "properties",
    "required",
    "additionalProperties",
    "maxProperties",
    "items",
    "maxItems",
    "minItems",
    "minLength",
    "maxLength",
    "minimum",
    "maximum",
    "pattern",
    "enum",
    "const",
    "oneOf",
    "anyOf",
    "format",
}
SCHEMAS = {"protocol.schema.json", "state.schema.json", "scenario.schema.json"}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(f"Agent contract: {message}")


def read(root: Path, path: str) -> dict:
    return json.loads((root / path).read_text())


def same(left: object, right: object) -> bool:
    # JSON booleans must never match numeric enum/const values through Python ==.
    if isinstance(left, bool) != isinstance(right, bool):
        return False
    return left == right


class ContractSchemas:
    def __init__(self, root: Path = ROOT):
        self.documents = {name: read(root, "docs/agent/" + name) for name in SCHEMAS}

    def resolve(self, reference: str, document: str) -> tuple[dict, str]:
        filename, separator, fragment = reference.partition("#")
        name = filename or document
        require(name in self.documents and separator == "#", "unknown/nonlocal schema reference")
        require(fragment.startswith("/$defs/"), "unsupported schema fragment")
        value = self.documents[name]
        for part in fragment[1:].split("/"):
            require(part in value, f"missing schema definition: {reference}")
            value = value[part]
        return value, name

    def audit(self) -> None:
        def walk(node: dict, document: str) -> None:
            require(isinstance(node, dict), "schema must be an object")
            require(not (node.keys() - KEYWORDS), "unsupported schema keyword")
            if "$ref" in node:
                self.resolve(node["$ref"], document)
            if "pattern" in node:
                re.compile(node["pattern"])
            if "format" in node:
                require(node["format"] in {"uint64", "positive-uint64"}, "unknown format")
            if "properties" in node:
                require(
                    set(node.get("required", [])) <= node["properties"].keys(),
                    "required property missing from schema",
                )
            for name in ("$defs", "properties"):
                for child in node.get(name, {}).values():
                    walk(child, document)
            for name in ("oneOf", "anyOf"):
                for child in node.get(name, []):
                    walk(child, document)
            for name in ("items", "additionalProperties"):
                if isinstance(node.get(name), dict):
                    walk(node[name], document)

        for name, document in self.documents.items():
            walk(document, name)

    def validate(self, value: object, reference: str) -> None:
        node, document = self.resolve(reference, "protocol.schema.json")
        self._check(value, node, document, 0)

    def _check(self, value: object, node: dict, document: str, depth: int) -> None:
        require(depth <= 128, "schema/value recursion limit")
        if "$ref" in node:
            target, name = self.resolve(node["$ref"], document)
            self._check(value, target, name, depth + 1)
        for variant in ("oneOf", "anyOf"):
            if variant not in node:
                continue
            matched = 0
            for child in node[variant]:
                try:
                    self._check(value, child, document, depth + 1)
                    matched += 1
                except ValueError:
                    pass
            require(
                matched == 1 if variant == "oneOf" else matched > 0,
                f"value does not match {variant}",
            )
        if "const" in node:
            require(same(value, node["const"]), "const mismatch")
        if "enum" in node:
            require(any(same(value, item) for item in node["enum"]), "enum mismatch")
        if "type" in node:
            types = {
                "object": isinstance(value, dict),
                "array": isinstance(value, list),
                "string": isinstance(value, str),
                "integer": type(value) is int,
                "number": type(value) in (int, float),
                "boolean": type(value) is bool,
                "null": value is None,
            }
            require(node["type"] in types and types[node["type"]], "wrong JSON type")
        if isinstance(value, dict):
            require(set(node.get("required", [])) <= value.keys(), "required property absent")
            require(len(value) <= node.get("maxProperties", len(value)), "object too large")
            properties = node.get("properties", {})
            extra = node.get("additionalProperties", True)
            for key, child in value.items():
                if key in properties:
                    self._check(child, properties[key], document, depth + 1)
                elif isinstance(extra, dict):
                    self._check(child, extra, document, depth + 1)
                else:
                    require(extra is True, "unknown object property")
        if isinstance(value, list):
            require(
                node.get("minItems", 0) <= len(value) <= node.get("maxItems", len(value)),
                "array bounds",
            )
            if "items" in node:
                for child in value:
                    self._check(child, node["items"], document, depth + 1)
        if isinstance(value, str):
            require(
                node.get("minLength", 0) <= len(value) <= node.get("maxLength", len(value)),
                "string bounds",
            )
            if "pattern" in node:
                require(re.search(node["pattern"], value) is not None, "string pattern")
            if "format" in node:
                require(value.isascii() and value.isdecimal(), "invalid unsigned counter")
                minimum = 1 if node["format"] == "positive-uint64" else 0
                require(minimum <= int(value) <= 2**64 - 1, "unsigned counter bounds")
        if type(value) in (int, float):
            require(type(value) is int or math.isfinite(value), "nonfinite number")
            require(
                node.get("minimum", value) <= value <= node.get("maximum", value), "numeric bounds"
            )


def decode_line(data: bytes) -> dict:
    """Specification fixture reader; this is not the future runtime transport."""
    require(
        len(data) <= 1048576 and data.endswith(b"\n") and b"\n" not in data[:-1],
        "one bounded LF-terminated message required",
    )
    require(not data.startswith(b"\xef\xbb\xbf"), "BOM is forbidden")

    def pairs(items: list) -> dict:
        result = {}
        for key, value in items:
            require(key not in result, "duplicate JSON key")
            result[key] = value
        return result

    def bad_constant(value: str) -> None:
        raise ValueError(f"Agent contract: nonfinite JSON constant {value}")

    try:
        value = json.loads(
            data.decode("utf-8"), object_pairs_hook=pairs, parse_constant=bad_constant
        )
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise ValueError("Agent contract: malformed JSON message") from error

    def depth(item: object, level: int = 0) -> None:
        require(level <= 32, "message nesting limit")
        if isinstance(item, dict):
            for child in item.values():
                depth(child, level + 1)
        elif isinstance(item, list):
            for child in item:
                depth(child, level + 1)
        elif isinstance(item, float):
            require(math.isfinite(item), "nonfinite JSON number")

    depth(value)
    require(isinstance(value, dict), "message must be an object")
    return value


def validate_semantics(packet: dict) -> None:
    """Cross-field specification checks, independent of unrecovered game rules."""

    def walk(value: object) -> None:
        if isinstance(value, list):
            for child in value:
                walk(child)
        if not isinstance(value, dict):
            return
        if {"boundary", "continuation_id"} <= value.keys():
            require(
                (value["boundary"] == "instruction") == (value["continuation_id"] is not None),
                "continuation must identify an instruction boundary only",
            )
        if {"complete", "next_cursor"} <= value.keys():
            require(
                value["complete"] == (value["next_cursor"] is None),
                "pagination completion/cursor contradiction",
            )
        if "actions" in value:
            targets = []
            for action in value["actions"]:
                target = (action["kind"], action.get("action", action.get("space")))
                if action["kind"] in {"button", "axes"}:
                    require(target not in targets, "conflicting same-tick action")
                    targets.append(target)
        for child in value.values():
            walk(child)

    walk(packet)
    if "params" not in packet:
        return
    params = packet["params"]
    if "guard" in params:
        require(params["guard"]["session_id"] == packet["session_id"], "guard session mismatch")
    if packet["method"] == "command.submit":
        require(int(params["at_tick"]) > int(params["guard"]["tick"]), "action targets past tick")
    if packet["method"] == "control.cycle":
        require(params["advance"]["guard"] == params["guard"], "cycle uses inconsistent guards")
        require(
            params["query"]["at"] == "latest" and not params["query"].get("cursor"),
            "cycle must query its resulting snapshot",
        )


def validate_parity(root: Path, parity: dict, methods: set[str], gates: set[str]) -> None:
    require(
        parity.get("schema_version") == 1 and parity.get("parity_version"), "parity version missing"
    )
    bound = {row["path"]: row["sha256"] for row in parity["baseline_sources"]}
    require(len(bound) == len(parity["baseline_sources"]), "duplicate parity source")
    require(
        {
            "analysis/scenarios/README.md",
            "analysis/scenarios/schema.json",
            "tools/reference/scenario.py",
        }
        <= bound.keys(),
        "missing required parity baseline",
    )
    for name, expected in bound.items():
        require(not Path(name).is_absolute() and ".." not in Path(name).parts, "unsafe parity path")
        require(
            hashlib.sha256((root / name).read_bytes()).hexdigest() == expected,
            f"parity source drift needs review/version update: {name}",
        )
    ids = set()
    for row in parity["rows"]:
        require(row["id"] not in ids, "duplicate parity row")
        ids.add(row["id"])
        emulator, native = row["emulator"], row["native"]
        require(emulator["status"] in parity["status_semantics"], "unknown emulator status")
        require(
            bool(emulator["scope"]) and bool(emulator["sources"]) and bool(emulator["tests"]),
            "unqualified emulator inventory",
        )
        require(
            set(emulator["sources"] + emulator["tests"]) <= bound.keys(),
            "unbound emulator source or test",
        )
        for key in emulator["original_findings"]:
            finding = read(root, f"analysis/findings/{key}.json")
            require(finding["validation"]["result"] == "passed", "unpassed original finding")
        if emulator["status"] == "observed_original":
            require(
                bool(emulator["original_findings"]), "observed capability lacks original evidence"
            )
        if emulator["status"] in {"unsupported", "not_present"}:
            require(native["relationship"] == "extension", "backlog cannot claim verified parity")
        require(
            native["methods"]
            and set(native["methods"]) <= methods
            and native["acceptance_gate"] in gates,
            "unmapped native parity",
        )
        require(
            native["status"] == "unimplemented" and not native["evidence"],
            "Phase 0 parity is not native execution evidence",
        )


def validate(root: Path, matrix: dict) -> dict:
    schemas = ContractSchemas(root)
    schemas.audit()
    catalog = read(root, "docs/agent/methods.json")
    methods = {item["id"]: item for item in catalog["methods"]}
    require(len(methods) == len(catalog["methods"]), "duplicate native method")
    wire = schemas.documents["protocol.schema.json"]["$defs"]
    for variant, field, expected_key in (
        ("request", "params", "request_schema"),
        ("completed", "result", "result_schema"),
    ):
        branches = wire[variant]["oneOf"]
        references = {
            branch["properties"]["method"]["const"]: branch["properties"][field]["$ref"]
            for branch in branches
        }
        require(
            len(references) == len(branches)
            and references == {key: method[expected_key] for key, method in methods.items()},
            "wire envelope and method catalog disagree",
        )
    tasks = matrix["source_snapshot"]["tasks"]
    for method in methods.values():
        require(method["minimum_role"] in catalog["role_order"], "invalid method role")
        require(method["status"] == "unimplemented", "specified method is not implemented")
        require(method["tasks"] and set(method["tasks"]) <= tasks.keys(), "unmapped method")
        schemas.resolve(method["request_schema"], "protocol.schema.json")
        schemas.resolve(method["result_schema"], "protocol.schema.json")
    for example in read(root, "docs/agent/examples.json")["examples"]:
        require(example["valid"] is True, "negative cases belong in independent tests")
        value = decode_line((json.dumps(example["value"]) + "\n").encode())
        schemas.validate(value, example["schema"])
        validate_semantics(value)
    acceptance = read(root, "docs/agent/acceptance.json")
    gates = {gate["id"]: gate for gate in acceptance["gates"]}
    require(len(gates) == len(acceptance["gates"]), "duplicate acceptance gate")
    for gate in gates.values():
        require(gate["tasks"] and set(gate["tasks"]) <= tasks.keys(), "unmapped native gate")
        require(
            all(gate.get(key) for key in ("preconditions", "stimulus", "acceptance", "artifacts")),
            "gate lacks an executable acceptance obligation",
        )
        require(
            gate["status"] == "defined" and not gate["evidence"],
            "Phase 0 cannot pass native acceptance",
        )
    for key in ("phase2_required", "phase3_required", "subsystem_required", "later_feature_gates"):
        require(
            acceptance[key] and set(acceptance[key]) <= gates.keys(), "missing architecture gate"
        )
    require(acceptance["phase3_required"] == ["GATE-FIRST-SLICE"], "first-slice gate omitted")
    for phase, group in ((2, "phase2_required"), (3, "phase3_required")):
        exit_gate = next(g for g in matrix["crosscutting"]["phase_exits"] if g["phase"] == phase)
        require(
            exit_gate.get("requires_agent_gates") == acceptance[group],
            f"Phase {phase} exit omits its native architecture gates",
        )
        if exit_gate["status"] == "passed":
            require(
                all(gates[key]["status"] == "passed" for key in acceptance[group]),
                "phase exit cannot bypass native architecture acceptance",
            )
    domains = read(root, "docs/agent/state-domains.json")
    for domain in domains["authoritative"]:
        require(
            domain["status"] == "unimplemented" and domain["concrete_schema"] is None,
            "Phase 0 must not invent recovered state layouts",
        )
        require(
            domain["required_fields"] and domain["promotion_gate"] in gates,
            "state domain lacks coverage obligation",
        )
    validate_parity(root, read(root, "docs/agent/emulator-parity.json"), set(methods), set(gates))
    history = read(root, "docs/requirements-migration.json")
    current = read(root, "tests/validation-results.json")["results"]
    for key, result in history["preserved_results"].items():
        require(current.get(key) == result, f"historical result reassigned or lost: {key}")
    return {
        "specified_methods": len(methods),
        "native_acceptance_gates_defined": len(gates),
        "authoritative_domain_obligations": len(domains["authoritative"]),
    }
