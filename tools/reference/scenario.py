"""Cold-boot original-game scenarios through independently recovered loader adapters."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

if __package__:
    from .instruction_trace import load_instruction_trace
    from .memory_sampler import load_sampling
    from .observe import sha256_file
    from .scenario_program import integer, keys, validate_program
else:
    from instruction_trace import load_instruction_trace
    from memory_sampler import load_sampling
    from observe import sha256_file
    from scenario_program import integer, keys, validate_program

ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = ROOT / "analysis/scenarios"
PROFILE_IDS = (
    "na-slus-00664-39c547a9afc6",
    "na-slus-00669-5eab85c683d4",
)
PROFILE_SOURCES = {
    PROFILE_IDS[0]: {
        "raw_track_sha256": "39c547a9afc6da15d847ef81a2c6cea1a6516bdfa562cf13b0999b04e8598bda",
        "source_index_table_sha256": (
            "3ae8e513a0f759eed5f39ff23594fc67a49ab601810b850c9f75e34979ab3afc"
        ),
        "first_child_slot": 606,
    },
    PROFILE_IDS[1]: {
        "raw_track_sha256": "5eab85c683d4d7087d345b587472db9c44df29b35ce66553c2626d26018b947e",
        "source_index_table_sha256": (
            "60377123a0a598fe9686177fe27dde7cf79b6e6ef58d576d7ea529b10945b536"
        ),
        "first_child_slot": 601,
    },
}


def condition(offset: int, width: int, value: int) -> dict:
    return {"offset": offset, "width": width, "operator": "eq", "value": value}


def compile_scenario(scenario: object, availability: dict) -> tuple[dict, int]:
    value = keys(
        scenario,
        {"schema_version", "name", "source_profile", "entry"},
        {"description", "launch", "steps", "state_writes"},
        "Scenario",
    )
    if type(value["schema_version"]) is not int or value["schema_version"] != 1:
        raise ValueError("Unsupported scenario schema")
    if "description" in value and not isinstance(value["description"], str):
        raise ValueError("Scenario description must be text")
    profile = value["source_profile"]
    if profile not in PROFILE_IDS:
        raise ValueError("No recovered launcher adapter for this source profile")
    entry = keys(value["entry"], {"type"}, {"map"}, "Entry")
    if entry["type"] not in ("kernel", "field"):
        raise ValueError(
            "Recovered adapters support kernel and field entry; "
            "other entry types remain unrecovered"
        )
    if entry["type"] == "kernel" and "map" in entry:
        raise ValueError("Kernel entry does not accept a map")
    launch = keys(
        value.get("launch", {}), set(), {"ready", "timeout_frames", "settle_frames"}, "Launch"
    )
    if launch.get("ready", "map") != "map":
        raise ValueError(
            "Only map initialization readiness is recovered; "
            "player-control readiness remains unimplemented"
        )
    timeout = integer(launch.get("timeout_frames", 5000), "Launch timeout", 1, 30000)
    settle = integer(launch.get("settle_frames", 120), "Settle frames", 1, 3600)
    steps = [
        {
            "name": "guard-reset-dispatch",
            "timeout_frames": 1,
            "when": [condition(0x19930, 4, 0x34040006)],
            "writes": [
                {
                    "offset": 0x19930,
                    "expected": "06000434",
                    "value": "00000434",
                    "reason": "Choose the original Kernel MENU after normal reset initialization.",
                }
            ],
            "evidence": ["EVID-REF-007"],
        },
        {
            "name": "kernel-ready",
            "timeout_frames": timeout,
            "when": [condition(0x592D0, 4, 1)],
            "stable_frames": 2,
            "run_frames": settle if entry["type"] == "kernel" else 1,
            "capture": entry["type"] == "kernel",
        },
    ]
    if entry["type"] == "field":
        map_id = integer(entry.get("map"), "Numeric map ID", 0, 4095)
        catalog = next(
            (
                item
                for item in availability.get("profiles", [])
                if item["source_profile"] == profile
            ),
            None,
        )
        if catalog is None:
            raise ValueError("No measured source-pair catalog for this profile")
        if any(catalog.get(key) != expected for key, expected in PROFILE_SOURCES[profile].items()):
            raise ValueError("Source-pair catalog identity differs from the recovered profile")
        pair = next(
            (item for item in catalog["pairs"] if item["candidate_map_index"] == map_id), None
        )
        if pair is None:
            raise ValueError(f"Map {map_id} has no measured source pair")
        first_slot = catalog["first_child_slot"] + 2 * map_id
        if pair.get("source_slots") != [first_slot, first_slot + 1]:
            raise ValueError("Source pair disagrees with the recovered field-slot formula")
        if pair["availability"] != "non_dummy_source_pair" or any(pair["exact_cdmake_dummy"]):
            raise ValueError(
                f"Map {map_id} contains the exact original CDMAKE dummy marker on this source"
            )
        state_writes = value.get("state_writes", [])
        if not isinstance(state_writes, list):
            raise ValueError("state_writes must be a list of explicit fingerprinted writes")
        reserved = (
            set(range(0x19930, 0x19934))
            | set(range(0x6F94E, 0x6F950))
            | set(range(0x18088, 0x1808C))
            | set(range(0x592D0, 0x592D4))
        )
        for write in state_writes:
            keys(write, {"offset", "expected", "value", "reason"}, set(), "State write")
            if type(write["offset"]) is not int or not isinstance(write["value"], str):
                raise ValueError("Invalid explicit state write")
            if set(range(write["offset"], write["offset"] + len(write["value"]) // 2)) & reserved:
                raise ValueError("State writes cannot override the launcher controls")
        if state_writes:
            steps.append(
                {
                    "name": "unreviewed-state-setup",
                    "timeout_frames": 1,
                    "writes": state_writes,
                }
            )
        steps += [
            {
                "name": "request-field",
                "timeout_frames": 1,
                "writes": [
                    {
                        "offset": 0x6F94E,
                        "expected": "ea01",
                        "value": map_id.to_bytes(2, "little").hex(),
                        "reason": "Set the persistent selector consumed by original field entry.",
                    },
                    {
                        "offset": 0x18088,
                        "expected": "00000000",
                        "value": "01000000",
                        "reason": "Select original mode 1, labeled Field by the Kernel MENU.",
                    },
                    {
                        "offset": 0x592D0,
                        "expected": "01000000",
                        "value": "00000000",
                        "reason": "Finish Kernel MENU and invoke its normal dispatcher.",
                    },
                ],
                "evidence": ["EVID-REF-007"],
            },
            {
                "name": "map-initialized",
                "timeout_frames": timeout,
                "when": [
                    condition(0x4F34C, 4, map_id),
                    condition(0x4F2F8, 4, 1),
                    condition(0xADB04, 1, 1),
                ],
                "stable_frames": 2,
                "run_frames": settle,
                "capture": True,
            },
        ]
    elif value.get("state_writes"):
        raise ValueError("State setup is available only before field entry")
    runtime_steps = value.get("steps", [])
    if not isinstance(runtime_steps, list) or len(runtime_steps) > 100:
        raise ValueError("Runtime steps must be a list of at most 100 steps")
    for index, item in enumerate(runtime_steps):
        if not isinstance(item, dict):
            raise ValueError("Runtime steps must be objects")
        if "wait_frames" in item:
            keys(item, {"wait_frames"}, {"capture"}, "Wait step")
            steps.append(
                {
                    "name": f"step-{index:03d}-wait",
                    "timeout_frames": 1,
                    "run_frames": integer(item["wait_frames"], "Wait frames", 1, 30000),
                    "capture": item.get("capture", False),
                }
            )
        else:
            keys(item, {"buttons"}, {"hold_frames", "after_frames", "capture"}, "Input step")
            steps.append(
                {
                    "name": f"step-{index:03d}-press",
                    "timeout_frames": 1,
                    "run_frames": integer(item.get("hold_frames", 8), "Hold frames", 1, 3600),
                    "buttons": item["buttons"],
                }
            )
            steps.append(
                {
                    "name": f"step-{index:03d}-release",
                    "timeout_frames": 1,
                    "run_frames": integer(item.get("after_frames", 60), "After frames", 1, 30000),
                    "capture": item.get("capture", False),
                }
            )
    program = validate_program(
        {
            "schema_version": 1,
            "name": value["name"],
            "source_profile": profile,
            "kind": "analysis_probe" if value.get("state_writes") else "recovered_scenario",
            "steps": steps,
        }
    )
    budget = sum(step["timeout_frames"] + step.get("run_frames", 0) + 1 for step in steps)
    if budget > 36000:
        raise ValueError("Scenario exceeds the bounded 36000-frame execution budget")
    return program, budget


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", nargs="?", help="Checked-in scenario name or JSON path")
    parser.add_argument("--map", type=lambda value: int(value, 0), help="Direct numeric field ID")
    parser.add_argument(
        "--profile", choices=PROFILE_IDS, help="Explicit supported source-profile override"
    )
    parser.add_argument("--content", type=Path, help="User-supplied original CHD")
    parser.add_argument("--output", type=Path, help="New private directory under .local/")
    parser.add_argument("--bios", type=Path)
    parser.add_argument("--sample-memory", type=Path, help="Bounded read-only RAM sampling JSON")
    parser.add_argument(
        "--trace-instructions", type=Path, help="Guarded instruction-address trace JSON"
    )
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    compiler_hash = sha256_file(Path(__file__))
    if args.list:
        for path in sorted(SCENARIOS.glob("*.json")):
            if path.name != "schema.json":
                print(path.stem)
        return
    sampler, sampling_bytes = None, None
    instruction_spec, instruction_bytes = None, None
    try:
        if args.scenario:
            direct = Path(args.scenario)
            path = direct if direct.is_file() else SCENARIOS / f"{args.scenario}.json"
            scenario = json.loads(path.read_text())
        elif args.map is not None:
            scenario = {
                "schema_version": 1,
                "name": f"field-{args.map}",
                "source_profile": args.profile or PROFILE_IDS[0],
                "entry": {"type": "field", "map": args.map},
            }
        else:
            parser.error("Choose a named scenario or --map")
        if args.profile:
            scenario["source_profile"] = args.profile
        if args.map is not None:
            scenario["entry"] = {"type": "field", "map": args.map}
        catalog_path = ROOT / "analysis/coverage/field-pairs.json"
        catalog_bytes = catalog_path.read_bytes()
        catalog_hash = hashlib.sha256(catalog_bytes).hexdigest()
        availability = json.loads(catalog_bytes)
        program, budget = compile_scenario(scenario, availability)
        if args.sample_memory:
            sampler, sampling_bytes = load_sampling(args.sample_memory)
            if sampler.spec["source_profile"] != scenario["source_profile"]:
                raise ValueError("Memory sampling targets a different source profile")
        if args.trace_instructions:
            instruction_spec, instruction_bytes = load_instruction_trace(args.trace_instructions)
            if instruction_spec["source_profile"] != scenario["source_profile"]:
                raise ValueError("Instruction tracing targets a different source profile")
    except (ValueError, OSError, KeyError) as error:
        parser.error(str(error))
    if args.dry_run:
        print(
            json.dumps(
                {
                    "scenario": scenario,
                    "program": program,
                    "frame_budget": budget,
                    **({"memory_sampling": sampler.spec} if sampler else {}),
                    **({"instruction_trace": instruction_spec} if instruction_spec else {}),
                },
                indent=2,
            )
        )
        return
    if not args.content or not args.output:
        parser.error("Execution requires --content and --output")
    if Path.cwd().resolve() != ROOT:
        parser.error("Run from the repository root inside the pinned Nix observation shell")
    out = args.output.resolve()
    if not out.is_relative_to(ROOT / ".local") or (ROOT / ".local").is_symlink():
        parser.error("Scenario output must stay in ignored .local/")
    out.mkdir(parents=True, exist_ok=False)
    scenario_path = out / "scenario.json"
    program_path = out / "program.json"
    (out / "source-catalog.json").write_bytes(catalog_bytes)
    scenario_path.write_text(json.dumps(scenario, indent=2) + "\n")
    program_path.write_text(json.dumps(program, indent=2) + "\n")
    provenance = {
        "scenario_sha256": sha256_file(scenario_path),
        "program_sha256": sha256_file(program_path),
        "compiler_sha256": compiler_hash,
        "source_catalog_sha256": catalog_hash,
        "source_catalog": "source-catalog.json",
        "setup_review": (
            "Unreviewed state experiment; loader evidence does not cover custom writes."
            if program["kind"] == "analysis_probe"
            else "Recovered loader adapter with original reset defaults."
        ),
    }
    if sampler:
        (out / "memory-sampling.json").write_bytes(sampling_bytes)
        provenance["memory_sampling"] = {
            "specification": "memory-sampling.json",
            "specification_sha256": hashlib.sha256(sampling_bytes).hexdigest(),
        }
    if instruction_spec:
        (out / "instruction-trace-spec.json").write_bytes(instruction_bytes)
        provenance["instruction_trace"] = {
            "specification": "instruction-trace-spec.json",
            "specification_sha256": hashlib.sha256(instruction_bytes).hexdigest(),
        }
    (out / "started.json").write_text(json.dumps(provenance, indent=2) + "\n")
    command = [
        sys.executable,
        str(ROOT / "tools/reference/observe.py"),
        "--content",
        str(args.content),
        "--output",
        str(out / "capture"),
        "--program",
        str(program_path),
        "--frames",
        str(budget),
        "--capture-every",
        "600",
    ]
    if args.bios:
        command += ["--bios", str(args.bios)]
    if sampler:
        command += ["--sample-memory", str(out / "memory-sampling.json")]
    if instruction_spec:
        command += ["--trace-instructions", str(out / "instruction-trace-spec.json")]
    result = subprocess.run(command, cwd=ROOT, check=False)
    if result.returncode:
        raise SystemExit(result.returncode)
    observation = json.loads((out / "capture/observation.json").read_text())
    if not observation["scenario"]["complete"]:
        raise RuntimeError("Scenario capture finished without completing its program")
    report = {
        "schema_version": 1,
        "scenario": scenario,
        **provenance,
        "observation": "capture/observation.json",
        "observation_sha256": sha256_file(out / "capture/observation.json"),
        "cold_boot": True,
        "complete": True,
        "readiness_scope": (
            "Original map loop initialization; scripted entry events may still be running."
            if scenario["entry"]["type"] == "field"
            else "Kernel MENU readiness only; later input targets have no readiness assertion."
        ),
        "state_basis": (
            "Original reset defaults plus explicitly recorded setup; "
            "no gameplay checkpoint is imported."
        ),
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Completed scenario {scenario['name']}: {out / 'report.json'}")


if __name__ == "__main__":
    main()
