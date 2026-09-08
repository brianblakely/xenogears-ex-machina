"""Source-generated, read-only observations for complete party movement calls."""

from ..reference.instruction_trace import validate_instruction_trace
from .animation_trace import WINDOWS as ANIMATION_WINDOWS
from .original_trace import require
from .sprite_state import u32
from .sweep_trace import WINDOWS as SWEEP_WINDOWS
from .verify_collision_math import source_bytes

WINDOWS = tuple(
    sorted(
        set(
            ANIMATION_WINDOWS
            + SWEEP_WINDOWS
            + (
                (0x80082494, 0x800825AC),
                (0x8008492C, 0x80084A40),
                (0x80080968, 0x800809D0),
                (0x80081F80, 0x800821F4),
                (0x80022974, 0x800229E8),
            )
        )
    )
)
SEGMENTS = {
    "control": (4300, 5241, 12000, "field23-complete-motion-v1"),
    "traversal-a": (4300, 4900, 19000, "field23-complete-motion-traversal-a-v3"),
    "traversal-b": (4900, 5500, 19000, "field23-complete-motion-traversal-b-v3"),
    "traversal-c": (5500, 6200, 19000, "field23-complete-motion-traversal-c-v3"),
}


def specification(sources, ram, segment):
    require(
        segment in SEGMENTS and sources["map"] == 23 and len(ram) == 0x200000,
        "Unqualified original movement segment, field or RAM",
    )
    table = u32(ram, 0xAFB10) - 0x80000000
    require(0 <= table <= len(ram) - 6 * 92, "Original movement descriptor table outside RAM")
    player, companion = table + 3 * 92, table + 5 * 92

    def direct(name, offset, size):
        return {"name": name, "offset": offset, "size": size}

    def pointer(name, offset, size):
        return {"name": name, "pointer_offset": offset, "relative_offset": 0, "size": size}

    def reg(name, register, size, relative=0):
        return {"name": name, "register": register, "relative_offset": relative, "size": size}

    common = [
        direct("field", 0x4F34C, 4),
        direct("descriptor-table", 0xAFB10, 4),
        direct("player-descriptor", player, 92),
        pointer("player-actor", player + 0x4C, 312),
        pointer("player-sprite", player + 4, 512),
        direct("held-inputs", 0xAFE98, 8),
        direct("motion-actor-index", 0x65B08, 4),
        direct("movement-controls", 0xADB64, 12),
        direct("collision-controls", 0xADB98, 120),
        direct("party-controls", 0x5A448, 8),
        direct("motion-globals", 0xB226C, 224),
        direct("companion-descriptor", companion, 92),
        pointer("companion-actor", companion + 0x4C, 312),
        pointer("companion-sprite", companion + 4, 512),
        direct("sprite-controls", 0x59190, 40),
        direct("query-controls", 0xB21C8, 8),
        direct("mesh-globals", 0xAFB18, 64),
    ]
    hooks = []

    def hook(name, pc, ranges):
        hooks.append(
            {
                "name": name,
                "pc": pc,
                "guard": {
                    "offset": pc - 16 - 0x80000000,
                    "expected": source_bytes(sources, pc - 16, pc + 16).hex(),
                },
                "ranges": common + ranges,
            }
        )

    hook(
        "motion-before",
        0x80082BB8,
        [reg("actor", 6, 312), reg("descriptor", 5, 92), reg("motion-locals", 29, 40, -0x48)],
    )
    motion = [
        reg("actor", 16, 312),
        reg("sprite", 19, 512),
        reg("descriptor", 22, 92),
        reg("motion-locals", 29, 40, 0x10),
    ]
    for name, pc in [
        ("motion-mode", 0x80082C8C),
        ("after-predicate", 0x80082CC0),
        ("after-vector", 0x80082D14),
        ("before-bounds", 0x80082D94),
        ("after-bounds", 0x80082DA0),
        ("after-sweep", 0x80082F30),
        ("animation-choice", 0x800830C0),
        ("animation-after", 0x800830F4),
        ("motion-after", 0x8008314C),
    ]:
        hook(
            name,
            pc,
            motion + ([reg("saved-registers", 29, 32, 0x38)] if name == "motion-after" else []),
        )
    for name, pc in [("predicate-before", 0x8008492C), ("predicate-after", 0x80084A38)]:
        hook(name, pc, [reg("actor", 4, 312)])
    for name, pc in [("sweep-ordinary-before", 0x8007BAC0), ("sweep-special-before", 0x8007B814)]:
        hook(name, pc, [reg("velocity", 4, 12), reg("actor", 5, 312), reg("sweep-output", 6, 16)])
    hook("animation-call", 0x800821F4, [reg("sprite", 4, 512), reg("descriptor", 6, 92)])
    hook("bounds-call", 0x80082494, [reg("velocity", 4, 12), reg("actor", 5, 312)])
    start, end, budget, name = SEGMENTS[segment]
    return validate_instruction_trace(
        {
            "schema_version": 1,
            "name": name,
            "source_profile": sources["profile"]["id"],
            "start_frame": start,
            "end_frame": end,
            "max_callbacks": budget,
            "hooks": hooks,
        }
    )
