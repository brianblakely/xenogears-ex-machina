"""Native host builds of port C for unit tests (docs/runtime.md, The game module).

The port's reimplementations of the handwritten routines address game memory
by PS1 address (port/include/xem/memory.h). Natively, this module maps RAM at
0x80000000 (2 MB) and the scratchpad page at 0x1F800000 into the test process
at those same addresses, builds the port sources with the software GTE into a
shared library, and places each game global the C names at an address in the
mapped RAM (`--defsym`), so the C runs as in the game module.

Run inside `nix develop path:./nix/runtime` ($XEM_CLANG and ld.lld). The
inputs the tests build are invented: they describe no original content.
"""

import ctypes
import os
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLANG = os.environ.get("XEM_CLANG") or shutil.which("clang")

RAM = 0x80000000
RAM_BYTES = 0x200000
SCRATCHPAD = 0x1F800000
SCRATCHPAD_BYTES = 0x1000

COMMON_FLAGS = [
    "-O2",
    "-fPIC",
    "-ffreestanding",
    "-fno-builtin",
    "-nostdinc",
    "-std=gnu89",
    "-funsigned-char",
    "-fwrapv",
    "-fno-strict-aliasing",
    "-Wall",
    "-Werror",
    "-Wno-unused-function",
    "-Iport/include",
    "-Idecomp/include",
]

_LIBC = ctypes.CDLL(None, use_errno=True)
_LIBC.mmap.restype = ctypes.c_void_p
_LIBC.mmap.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_long]
_MAPPED = False


def map_game_memory():
    """Map RAM and the scratchpad at their PS1 addresses (once per process)."""
    global _MAPPED
    if _MAPPED:
        return
    prot = 3  # PROT_READ | PROT_WRITE
    flags = 0x02 | 0x20 | 0x100000  # MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE
    for address, size in ((RAM, RAM_BYTES), (SCRATCHPAD, SCRATCHPAD_BYTES)):
        got = _LIBC.mmap(address, size, prot, flags, -1, 0)
        if got != address:
            raise OSError(ctypes.get_errno(), f"cannot map game memory at {address:#x}")
    _MAPPED = True


def build(out, sources, symbols=None, defines=()):
    """Compile `sources` (paths from the repository root) with the software GTE
    into a shared library in `out`, the names in `symbols` placed at their
    addresses, and load it."""
    if not CLANG:
        raise RuntimeError("no clang (run inside nix develop path:./nix/runtime)")
    map_game_memory()
    objects = []
    for source in ["port/gte.c", *sources]:
        obj = os.path.join(out, source.replace("/", "_") + ".o")
        subprocess.run([CLANG, *COMMON_FLAGS, *(f"-D{d}" for d in defines), "-c", source, "-o", obj],
                       cwd=ROOT, check=True)
        objects.append(obj)
    library = os.path.join(out, "libport.so")
    defsyms = [f"-Wl,--defsym={name}={address:#x}" for name, address in (symbols or {}).items()]
    subprocess.run([CLANG, "-shared", "-nostdlib", "-fuse-ld=lld", *defsyms, *objects, "-o", library],
                   cwd=ROOT, check=True)
    lib = ctypes.CDLL(library)
    lib.xem_gte_write_data.argtypes = [ctypes.c_int, ctypes.c_uint]
    lib.xem_gte_write_control.argtypes = [ctypes.c_int, ctypes.c_uint]
    lib.xem_gte_read_data.restype = ctypes.c_uint
    lib.xem_gte_read_data.argtypes = [ctypes.c_int]
    lib.xem_gte_read_control.restype = ctypes.c_uint
    lib.xem_gte_read_control.argtypes = [ctypes.c_int]
    lib.xem_gte_execute.argtypes = [ctypes.c_uint]
    lib.xem_gte_state.restype = ctypes.c_void_p
    lib.xem_gte_state_size.restype = ctypes.c_uint
    return lib


def _check(address, size):
    if not (RAM <= address and address + size <= RAM + RAM_BYTES) and not (
        SCRATCHPAD <= address and address + size <= SCRATCHPAD + SCRATCHPAD_BYTES
    ):
        raise ValueError(f"{address:#x}+{size:#x} is outside the mapped game memory")


def read(address, size):
    _check(address, size)
    return ctypes.string_at(address, size)


def write(address, data):
    data = bytes(data)
    _check(address, len(data))
    ctypes.memmove(address, data, len(data))


def fill(address, size, byte=0):
    write(address, bytes([byte]) * size)


def clear():
    """Zero all of the mapped game memory."""
    fill(RAM, RAM_BYTES)
    fill(SCRATCHPAD, SCRATCHPAD_BYTES)


def u32(address):
    return struct.unpack("<I", read(address, 4))[0]


def u16(address):
    return struct.unpack("<H", read(address, 2))[0]


def s16(address):
    return struct.unpack("<h", read(address, 2))[0]


def put32(address, *values):
    write(address, struct.pack(f"<{len(values)}I", *(v & 0xFFFFFFFF for v in values)))


def put16(address, *values):
    write(address, struct.pack(f"<{len(values)}H", *(v & 0xFFFF for v in values)))


def gte_snapshot(lib):
    """The whole software GTE state (32 data, then 32 control registers)."""
    return list(struct.unpack("<64I", ctypes.string_at(lib.xem_gte_state(), lib.xem_gte_state_size())))


def gte_restore(lib, registers):
    ctypes.memmove(lib.xem_gte_state(), struct.pack("<64I", *registers), 256)
