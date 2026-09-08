"""Original matrix reconstruction, reviewed in Ghidra against original MIPS.

Resident 8003f738 rotation; 8004974c row scaling and 80049dcc column scaling.
The last scale store writes 32 bits at matrix+16, including the padding halfword.
"""

import struct

from .arithmetic import signed16, signed32
from .sprite_state import inside, u16, u32


def fixed_product(a, b):
    return signed32(signed32(a) * signed32(b)) >> 12


def rotation(angles, table):
    if len(angles) != 3 or len(table) != 16384:
        raise ValueError("Unqualified original trig table")
    sx, cx = struct.unpack_from("<2h", table, (angles[0] & 0xFFF) * 4)
    sy, cy = struct.unpack_from("<2h", table, (angles[1] & 0xFFF) * 4)
    sz, cz = struct.unpack_from("<2h", table, (angles[2] & 0xFFF) * 4)
    yz = fixed_product(cz, -sy)
    yz2 = fixed_product(sz, -sy)
    # Keep negation before the arithmetic shift, and each original intermediate.
    values = (
        fixed_product(cz, cy),
        signed32(-signed32(sz * cy)) >> 12,
        sy,
        signed32(fixed_product(sz, cx) - fixed_product(yz, sx)),
        signed32(fixed_product(cz, cx) + fixed_product(yz2, sx)),
        signed32(-signed32(cy * sx)) >> 12,
        signed32(fixed_product(yz, cx) + fixed_product(sz, sx)),
        signed32(fixed_product(cz, sx) - fixed_product(yz2, cx)),
        fixed_product(cy, cx),
    )
    return tuple(signed16(x) for x in values)


def scale_matrix(matrix, scales, columns=False):
    if len(matrix) != 32 or len(scales) != 3:
        raise ValueError("Incomplete original matrix/vector")
    old = struct.unpack_from("<9h", matrix)
    out = bytearray(matrix)
    values = [fixed_product(v, scales[i % 3 if columns else i // 3]) for i, v in enumerate(old)]
    struct.pack_into("<8h", out, 0, *(signed16(v) for v in values[:8]))
    struct.pack_into("<i", out, 16, values[8])
    return bytes(out)


def sprite_matrix(sprite, address, table):
    renderer = u32(sprite, 0x20)
    relative = inside(sprite, address, renderer, 44)
    if u32(sprite, 0x40) & 1:
        raise ValueError("GTE matrix-product branch remains unreconstructed")
    angles = struct.unpack_from("<3h", sprite, relative)
    scales = struct.unpack_from("<3h", sprite, relative + 6)
    matrix = bytearray(sprite[relative + 12 : relative + 44])
    struct.pack_into("<9h", matrix, 0, *rotation(angles, table))
    matrix = scale_matrix(matrix, scales)
    extra = u16(sprite, 0x3A)
    if extra:
        matrix = scale_matrix(matrix, (extra >> 1,) * 3, columns=True)
    out = bytearray(sprite)
    out[relative + 12 : relative + 44] = matrix
    return bytes(out)
