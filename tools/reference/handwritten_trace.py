"""Write an instruction-trace specification for comparing the port's
handwritten-routine replacements with the original routines
(tests/test_port_original_traces.py).

Every hook takes a RAM and scratchpad snapshot (the trace records the GTE
words and CPU registers anyway): the entry of each resident model renderer,
the renderers' shared return (the `jr $ra` of model_draw_gt3_avg.s's exit), and
the entry and return of sprite_darken_pixels, sprite_blend_pixels and
text_unpack_lzss. Guards are the original code bytes, read from the user's
matched resident link (.local/decomp/build/SLUS_006.64.elf); the
specification therefore stays private.

    python3 tools/reference/handwritten_trace.py START END BUDGET OUT.json [--skip NAME,...]
    nix develop path:./nix#observation-trace -c python3 tools/reference/scenario.py \\
        SCENARIO.json --content DISC.chd --trace-instructions OUT.json \\
        --output .local/scenarios/p2-handwritten-routines-NAME

The captures behind the test replayed the p1-forest-encounter-menu scenario
(field 23, its encounter and the menu) with windows 6600-6660 (budget 6000, before
the LZSS hooks existed: "battle") and 1300-12200 skipping model_draw_ft3_avg
and model_draw_ft4_avg (budget 8192: "wide"). Field 23 draws with the far
sorts, the battle with the AVSZ sorts; later windows met no other renderer.
"""

import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.game_module import elf_symbols, section_bytes  # noqa: E402

RESIDENT = ROOT / ".local/decomp/build/SLUS_006.64.elf"
PROFILE = "na-slus-00664-39c547a9afc6"
# The renderers' shared exit (model_draw_gt3_avg.s, 8002E1F4) and its jr $ra.
MODEL_DRAW_RETURN = 0x8002E1F4 + 0x30
JR_RA = 0x03E00008


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("start", type=int)
    parser.add_argument("end", type=int)
    parser.add_argument("budget", type=int)
    parser.add_argument("out", type=Path)
    parser.add_argument("--skip", default="", help="entry hooks to leave out, comma-separated")
    args = parser.parse_args()
    data, sections, symbols = elf_symbols(RESIDENT)
    names = {name: value for name, value, *_ in symbols}

    def word(address):
        return struct.unpack("<I", section_bytes(data, sections, address, 4))[0]

    def hook(name, pc):
        start = pc & ~7
        return {
            "name": name,
            "pc": pc,
            "guard": {"offset": start - 0x80000000, "expected": section_bytes(data, sections, start, 8).hex()},
            "ranges": [{"name": "a0", "register": 4, "relative_offset": 0, "size": 8}],
            "snapshot": True,
        }

    skip = set(filter(None, args.skip.split(",")))
    renderers = sorted(n for n in names if n.startswith("model_draw_") and n != "model_draw_sprite_model"
                       and not n.endswith("unreferenced") and n not in skip)
    hooks = [hook(name, names[name]) for name in renderers]
    assert word(MODEL_DRAW_RETURN) == JR_RA
    hooks.append(hook("model_draw_exit", MODEL_DRAW_RETURN))
    for name in ("sprite_darken_pixels", "sprite_blend_pixels", "text_unpack_lzss"):
        hooks.append(hook(name, names[name]))
        ret = next(names[name] + o for o in range(0, 0x800, 4) if word(names[name] + o) == JR_RA)
        hooks.append(hook(name + "_return", ret))
    spec = {
        "schema_version": 1,
        "name": "handwritten-routines",
        "source_profile": PROFILE,
        "start_frame": args.start,
        "end_frame": args.end,
        "max_callbacks": args.budget,
        "max_snapshots": args.budget,
        "hooks": hooks,
    }
    args.out.write_text(json.dumps(spec, indent=1) + "\n")


if __name__ == "__main__":
    main()
