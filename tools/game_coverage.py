"""Optional block coverage of the optimized game IR, before Asyncify.

Each block stores 1 in its own byte; there are no hash collisions, counters
to overflow or host calls in the game loop. This measures blocks, not all
paths or all possible game states. The map includes unvisited/unlinked
functions; it never claims that an unvisited block is unreachable.
"""

import concurrent.futures
import hashlib
import json
import os
import re
import subprocess

# Immediately after the port's reserved 16 MiB (snapshot::REGIONS), outside
# every game's address range. Keeping telemetry out of the linker allocation
# preserves port/stack pointers the game sometimes leaves in original RAM.
BASE = 0x11000000
HEADER = 32
LABEL = re.compile(r"^([-\w.$]+):(?:\s*;.*)?$")
FUNCTION = re.compile(r'^define .*?@("[^"\n]+"|[-\w.$]+)\(')

# Sparse, exact field-VM observations. Epochs clear the table in O(1); a
# queue lets the runner read only new entries, not megabytes of empty bits.
# These addresses, like the block probes, allocate no linker/stack storage.
SCRIPT = {
    # Last eight bytes of the reserved port region, included in snapshots.
    # A pending transition changes mode_field_map_id before old scripts stop.
    # Unlike hit telemetry, the loaded identity must rewind with game state.
    "identity": BASE - 8,
    "header": BASE + 0x10000,  # epoch, count, overflow (u32 each)
    "table": BASE + 0x20000,
    "slots": 131072,
    "queue": BASE + 0x220000,
    "capacity": 65536,
    "metric": "field dispatch (disc, map, PC) and completed handler (PC, next PC)",
}


def field_probe(out):
    """Compile a coverage-only observer using the build's recovered layout.

    The interpreter calls it around the actual opcode dispatch. Polling actor
    PCs once per frame would miss whole scripts and both sides of branches.
    It observes the same actor before/after, not a possibly changed global.
    A table collision is resolved by comparing full keys; overflow is fatal
    to the measurement, never silently treated as full coverage.
    """
    schema = json.loads((out / "schema.json").read_text())
    globals_, types = schema["globals"], schema["types"]

    def unwrap(index):
        value = (next(t for t in types if t.get("name") == index.removeprefix("record ")
                      and t["kind"] == "struct") if isinstance(index, str) else types[index])
        while value["kind"] in ("typedef", "const", "volatile"):
            value = types[value["type"]]
        return value

    actor_global = globals_["field_current_event_actor"]
    actor = unwrap(unwrap(actor_global["type"])["to"])
    pc = next(member for member in actor["members"] if member["name"] == "pc")
    if unwrap(pc["type"])["size"] != 2:
        raise ValueError("field PC is no longer a halfword")
    directory = globals_["cd_directory_table"]["address"]
    map_id = globals_["mode_field_map_id"]["address"]
    code = f"""
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned short u16;
typedef struct {{ u64 key; u32 epoch; u32 pad; }} Slot;
#define HEADER ((volatile u32 *){SCRIPT['header']}u)
#define TABLE ((volatile Slot *){SCRIPT['table']}u)
#define QUEUE ((volatile u64 *){SCRIPT['queue']}u)
#define IDENTITY ((volatile u32 *){SCRIPT['identity']}u)
static u32 ram(u32 address) {{ return 0x80000000u | (address & 0x1fffffu); }}
static void record(u64 key) {{
    u32 epoch = HEADER[0];
    if (!epoch) HEADER[0] = epoch = 1;
    u32 slot = (u32)((key ^ (key >> 29)) * 0x9e3779b97f4a7c15ull >> 32);
    for (u32 probe = 0; probe < 256; ++probe, ++slot) {{
        volatile Slot *entry = TABLE + (slot & ({SCRIPT['slots']}u - 1));
        if (entry->epoch == epoch) {{
            if (entry->key == key) return;
        }} else {{
            u32 count = HEADER[1];
            if (count >= {SCRIPT['capacity']}u) {{ HEADER[2] = 1; return; }}
            entry->key = key;
            entry->epoch = epoch;
            QUEUE[count] = key;
            HEADER[1] = count + 1;
            return;
        }}
    }}
    HEADER[2] = 2;
}}
void xem_coverage_field_load(void) {{
    u32 directory = *(volatile u32 *){directory}u;
    u32 disc = *(volatile u16 *)ram(directory + 0x78);
    u32 map = *(volatile u32 *){map_id}u & 0xfff;
    IDENTITY[0] = disc;
    IDENTITY[1] = map;
}}
u64 xem_coverage_field_begin(u32 actor) {{
    u32 disc = IDENTITY[0], map = IDENTITY[1];
    u32 pc = *(volatile u16 *)ram(actor + {pc['offset']});
    if (disc < 1 || disc > 2) {{ HEADER[2] = 3; return 0; }}
    u64 key = ((u64)disc << 45) | ((u64)map << 33) | ((u64)pc << 17);
    record(key | 0x10000u); /* dispatch observed, even if its handler traps */
    return key;
}}
void xem_coverage_field_end(u64 key, u32 actor) {{
    if (key) record(key | *(volatile u16 *)ram(actor + {pc['offset']}));
}}
"""
    source, ir = out / "ir/field-coverage.c", out / "ir/field-coverage.ll"
    source.write_text(code)
    subprocess.run(
        [os.environ["XEM_CLANG"], "--target=wasm32-unknown-unknown", "-O2",
         "-S", "-emit-llvm", str(source), "-o", str(ir)], check=True,
    )
    return ir, actor_global["address"]


def instrument_field(text, actor_address):
    """Bracket the one real primary dispatch; fail if the target changes."""
    lines, active, found = [], False, 0
    for line in text.splitlines():
        if FUNCTION.match(line):
            active = FUNCTION.match(line)[1] == "field_event_run_instructions"
        if active and re.search(r"call void @xem\.icall\.void_", line):
            found += 1
            lines += [
                f"  %xem.cov.actor = load i32, ptr inttoptr (i32 {actor_address} to ptr), align 4",
                "  %xem.cov.script = call i64 @xem_coverage_field_begin(i32 %xem.cov.actor)",
                line.replace("tail call", "call"),
                "  call void @xem_coverage_field_end(i64 %xem.cov.script, i32 %xem.cov.actor)",
            ]
        else:
            lines.append(line)
    if found != 1:
        raise ValueError(f"expected one field instruction dispatch, found {found}")
    lines += ["declare i64 @xem_coverage_field_begin(i32)",
              "declare void @xem_coverage_field_end(i64, i32)"]
    return "\n".join(lines) + "\n"


def instrument_field_load(text):
    """Remember the bundle's identity before its initialization scripts run."""
    lines, pending, found = [], False, 0
    for line in text.splitlines():
        head = FUNCTION.match(line)
        if head:
            pending = head[1] == "field_load_from_bundle"
        elif pending and line.strip() and not line.lstrip().startswith(";") and not LABEL.match(line):
            if not re.match(r"\s*%[^=]+ = phi\b", line):
                lines.append("  call void @xem_coverage_field_load()")
                pending, found = False, found + 1
        lines.append(line)
    if found != 1:
        raise ValueError(f"expected one field bundle loader, found {found}")
    lines.append("declare void @xem_coverage_field_load()")
    return "\n".join(lines) + "\n"


def instrument(text, source, image, blocks):
    """Place a store after each block's PHIs (including the implicit entry).

    Optimized functions may be marked memory(none/read), speculatable or
    readonly. Remove these now-false promises from definitions AND call
    sites/attribute groups before the next compiler pass.
    """
    result, function, pending, label = [], None, False, None
    for line in text.splitlines():
        if line.startswith(("define ", "declare ", "attributes ")) or re.match(
            r"\s*(?:%[-\w.]+ = )?(?:tail |musttail |notail )?call\b", line
        ):
            # Attributes are whitespace-delimited tokens. A symbol named
            # @readonly, or those words in a string constant, is game data.
            line = re.sub(r"(?<!\S)memory\([^)]*\)", "", line)
            line = re.sub(r"(?<!\S)(readnone|readonly|writeonly|speculatable|nosync)(?!\S)", "", line)
        head = FUNCTION.match(line)
        if head:
            function = head[1].strip('"')
            pending, label = True, "entry"
        elif function is not None:
            match = LABEL.match(line)
            if match:
                pending, label = True, match[1]
            elif pending and line.strip() and not line.lstrip().startswith(";"):
                if re.match(r"\s*%[^=]+ = phi\b", line):
                    result.append(line)
                    continue
                if line == "}":
                    raise ValueError(f"empty block in {source}: {function}:{label}")
                index = len(blocks)
                blocks.append(
                    {"source": source, "image": image, "function": function, "block": label}
                )
                result.append(
                    f"  store volatile i8 1, ptr inttoptr "
                    f"(i32 {BASE + HEADER + index} to ptr), align 1"
                )
                pending = False
            if line == "}":
                function = None
        result.append(line)
    return "\n".join(result) + "\n"


def prepare(ll_files, sources, out, jobs):
    """Optimize first, then instrument; keep the normal build untouched."""

    def optimize(path):
        dest = path.with_suffix(".opt.ll")
        subprocess.run(
            [
                os.environ["XEM_CLANG"],
                "--target=wasm32-unknown-unknown",
                "-O2",
                "-S",
                "-emit-llvm",
                str(path),
                "-o",
                str(dest),
                "-w",
            ],
            check=True,
        )
        return dest

    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        optimized = list(pool.map(optimize, ll_files))
    blocks, paths = [], []
    digest = hashlib.sha256()
    field_ir, actor_address = field_probe(out)
    field_hooks = 0
    for path, (source, image) in zip(optimized, sources, strict=True):
        text = path.read_text()
        digest.update(text.encode())
        rewritten = instrument(text, source, image, blocks)
        if source == "src/decomp/src/field/field_event.c":
            rewritten = instrument_field(rewritten, actor_address)
            field_hooks += 1
        elif source == "src/decomp/src/field/field.c":
            rewritten = instrument_field_load(rewritten)
            field_hooks += 1
        digest.update(rewritten.encode())
        dest = path.with_suffix(".cov.ll")
        dest.write_text(rewritten)
        paths.append(dest)
    if field_hooks != 2:
        raise ValueError("field coverage requires both the loader and interpreter")
    if BASE + HEADER + len(blocks) > SCRIPT["header"]:
        raise ValueError("block probes overlap script telemetry")
    paths.append(field_ir)
    digest.update(field_ir.read_bytes())
    digest.update(json.dumps(SCRIPT, sort_keys=True).encode())
    # The header verifies that the runner's map describes this exact build.
    digest.update(json.dumps(blocks, sort_keys=True).encode())
    signature = digest.digest()
    return paths, {
        "format": 1,
        "metric": "optimized IR blocks before Asyncify",
        "signature": signature.hex(),
        "address": BASE,
        "blocks": blocks,
        "field_scripts": SCRIPT,
    }


def leb(value):
    """Unsigned LEB128 (BASE also fits the positive signed i32 encoding)."""
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def read_leb(data, at):
    result, shift = 0, 0
    while True:
        value = data[at]
        result |= (value & 127) << shift
        at += 1
        if value < 128:
            return result, at
        shift += 7


def initialize(raw, manifest):
    """Add an active data segment for the signature AFTER linking.

    No extra linker allocation changes any existing data or stack address.
    wasm-validate checks the rewritten module before Asyncify sees it.
    """
    data = raw.read_bytes()
    if data[:8] != b"\0asm\x01\0\0\0":
        raise ValueError("not a wasm module")
    segment = b"\x00\x41" + leb(BASE) + b"\x0b" + leb(HEADER)
    segment += bytes.fromhex(manifest["signature"])
    result, at, found, insert_at = bytearray(data[:8]), 8, False, 8
    while at < len(data):
        section = data[at]
        size, start = read_leb(data, at + 1)
        payload = data[start : start + size]
        at = start + size
        if section in (11, 12):  # data section, optional data-count section
            count, offset = read_leb(payload, 0)
            payload = leb(count + 1) + payload[offset:]
            if section == 11:
                payload += segment
                found = True
        result += bytes([section]) + leb(len(payload)) + payload
        if section != 0:
            insert_at = len(result)
    if not found:
        payload = leb(1) + segment
        result[insert_at:insert_at] = b"\x0b" + leb(len(payload)) + payload
    raw.write_bytes(result)
    subprocess.run(["wasm-validate", str(raw)], check=True)


def finish(out, manifest):
    """Bind the coverage map to the actual final module."""
    wasm = out / "game.wasm"
    manifest["module_sha256"] = hashlib.sha256(wasm.read_bytes()).hexdigest()
    path = out / "coverage.json"
    path.write_text(json.dumps(manifest, separators=(",", ":")) + "\n")
    print(f"{len(manifest['blocks'])} coverage blocks: {path}")
