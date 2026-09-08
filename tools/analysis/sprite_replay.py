"""Original-source reconstruction of facing replay and frame scheduling.

Allocation and alternate frame formats remain explicit failures. Resource bytes,
runtime list memory, and any required incoming duration register are supplied.
"""

from .arithmetic import signed16, signed32
from .sprite_state import inside, put, read_exact, u16, u32


def clear_auxiliary(sprite, address):
    out = bytearray(sprite)
    renderer = inside(out, address, u32(out, 0x20), 64)
    auxiliary = inside(out, address, u32(out, renderer + 0x34), 64)
    out[auxiliary : auxiliary + 64] = bytes(64)
    return bytes(out)


def previous_frame(sprite, address, frame, binding, read, on_part=None):
    out = bytearray(sprite)
    binding_offset = inside(out, address, binding, 20)
    directory = u32(out, binding_offset)
    word = int.from_bytes(read_exact(read, directory, 2), "little")
    frame = signed32(frame)
    if frame >= (word & 0x1FF) + 1:
        return bytes(out)
    if word & 0x8000:
        raise ValueError("Alternate original sprite frame format remains unreconstructed")
    record = directory + int.from_bytes(read_exact(read, directory + frame * 2, 2), "little")
    flags = read_exact(read, record, 1)[0]
    count = flags & 63
    pointer = record + count * 4 + 6
    part = 0
    for _ in range(4096):
        if part == count:
            return bytes(out)
        if on_part:
            on_part(pointer, part, count, flags & 0x80, bytes(out))
        command = read_exact(read, pointer, 1)[0]
        pointer += 1
        if command & 0x80:
            if command & 0x40:
                renderer = inside(out, address, u32(out, 0x20), 64)
                target = u32(out, renderer + 0x34)
                if not target:
                    raise ValueError("Original frame auxiliary allocation remains unreconstructed")
                target = inside(out, address, target, 64) + (command & 7) * 8
                if command & 0x20:
                    operands = read_exact(read, pointer, 2)
                    pointer += 2
                    put(out, target, operands[0], 1)
                    put(out, target + 1, operands[1], 1)
                value = 0
                if command & 0x10:
                    value = read_exact(read, pointer, 1)[0] << 4
                    pointer += 1
                put(out, target + 6, value, 2)
            else:
                pointer += int(bool(command & 1)) + int(bool(command & 2))
        else:
            pointer += 4 if flags & 0x80 else 2
            part += 1
    raise ValueError("Original previous-frame parsing exceeded inspection bound")


def frame_change(
    sprite, address, frame, list_head, read, read_memory, on_list=None, on_previous=None
):
    out = bytearray(sprite)
    if u32(out, 0x3C) & 3 != 1:
        put(out, 0x34, 0, 2)
        return bytes(out), list_head
    renderer = inside(out, address, u32(out, 0x20), 64)
    if u32(out, 0x40) & 0x100000:
        put(out, 0x40, u32(out, 0x40) & ~0x100000)
        if u32(out, renderer + 0x34):
            out = bytearray(clear_auxiliary(out, address))
    if u32(out, 0x40) & 0x20000:
        node = list_head
        seen = set()
        while node:
            if node in seen:
                raise ValueError("Original frame list is cyclic")
            seen.add(node)
            if on_list:
                on_list(node, bytes(out))
            if node == address:
                binding = u32(out, 0x24)
                if binding not in (0x8005A474, 0x8006BE10) and not u32(out, 0x40) & 0x80000:
                    if on_previous:
                        on_previous(bytes(out), u16(out, 0x34), binding)
                    out = bytearray(previous_frame(out, address, u16(out, 0x34), binding, read))
                put(out, 0x34, frame, 2)
                return bytes(out), list_head
            other_renderer = int.from_bytes(read_exact(read_memory, node + 0x20, 4), "little")
            node = int.from_bytes(read_exact(read_memory, other_renderer + 0x38, 4), "little")
    put(out, 0x34, frame, 2)
    put(out, 0x40, u32(out, 0x40) | 0x20000)
    put(out, renderer + 0x38, list_head)
    return bytes(out), address


def lookup_frame(sprite, address, list_head, read, read_memory, on_frame=None):
    out = bytearray(sprite)
    pointer = u32(out, 0x54) + ((u32(out, 0xA8) >> 11) & 63) * 2
    value = int.from_bytes(read_exact(read, pointer, 2), "little")
    put(out, 0xAC, (u32(out, 0xAC) & ~8) | ((value >> 6) & 8))
    flags = u32(out, 0xAC)
    put(out, 0x3C, (u32(out, 0x3C) & ~8) | ((((flags >> 3) ^ (flags >> 2)) & 1) << 3))
    if on_frame:
        on_frame(bytes(out), value & 0x1FF)
    return frame_change(out, address, value & 0x1FF, list_head, read, read_memory)


def command_replay(
    sprite,
    address,
    target,
    target_step,
    list_head,
    widths,
    read,
    read_memory,
    on_step=None,
    initial_duration=None,
):
    if len(widths) != 256:
        raise ValueError("Original replay width table must contain 256 bytes")
    out = bytearray(sprite)
    duration = None if initial_duration is None else signed32(initial_duration)
    for _ in range(4096):
        pointer = u32(out, 0x64)
        counter = (u32(out, 0xA8) >> 22) & 63
        if on_step:
            on_step(bytes(out), list_head)
        if pointer == target and counter == target_step:
            return bytes(out), list_head
        opcode = read_exact(read, pointer, 1)[0]
        if opcode < 0x80:
            put(out, 0x64, pointer + 1)
            if opcode < 0x10:
                out, list_head = frame_change(
                    out, address, u16(out, 0x34) + 1, list_head, read, read_memory
                )
            elif opcode < 0x20:
                put(
                    out,
                    0xA8,
                    (u32(out, 0xA8) & 0xFFFE07FF) | ((((u32(out, 0xA8) >> 11) + 1) & 63) << 11),
                )
                out, list_head = lookup_frame(out, address, list_head, read, read_memory)
            elif opcode < 0x30:
                out, list_head = frame_change(
                    out, address, u16(out, 0x34) - 1, list_head, read, read_memory
                )
            if opcode < 0x40:
                duration = (opcode & 15) + 1
            if duration is None:
                raise ValueError("Original replay needs the incoming S3 duration register")
            out = bytearray(out)
            counter = (((u32(out, 0xA8) >> 22) + 1) & 63) or 63
            put(out, 0x9E, u16(out, 0x9E) + duration, 2)
            put(out, 0xA8, (u32(out, 0xA8) & 0xF03FFFFF) | (counter << 22))
            continue
        if opcode in (0x80, 0x81, 0x82):
            return bytes(out), list_head
        if opcode in (0x86, 0x87, 0x97) and pointer == target:
            return bytes(out), list_head
        if opcode == 0xB3:
            put(
                out,
                0xA8,
                (u32(out, 0xA8) & 0xFFFE07FF) | ((read_exact(read, pointer + 1, 1)[0] & 63) << 11),
            )
        elif opcode == 0xBE:
            value = int.from_bytes(read_exact(read, pointer + 1, 2), "little")
            put(out, 0xAC, (u32(out, 0xAC) & ~8) | ((value >> 6) & 8))
            flags = u32(out, 0xAC)
            put(out, 0x3C, (u32(out, 0x3C) & ~8) | ((((flags >> 3) ^ (flags >> 2)) & 1) << 3))
            if u16(out, 0x34) != value & 0x1FF:
                out, list_head = frame_change(
                    out, address, value & 0x1FF, list_head, read, read_memory
                )
                out = bytearray(out)
            put(out, 0x9E, u16(out, 0x9E) + 1 + ((value >> 11) & 15), 2)
        elif opcode == 0xE2:
            delta = signed16(int.from_bytes(read_exact(read, pointer + 1, 2), "little"))
            cursor = (out[0x8C] - 3) & 255
            put(out, 0x8C, cursor, 1)
            for i in range(3):
                # The original reloads this byte between stores. An underflowed
                # cursor can alias its own storage or the current command pointer.
                cursor = out[0x8C]
                cursor = cursor if cursor < 128 else cursor - 256
                put(out, 0x8E + cursor + i, (pointer + 3) >> (8 * i), 1)
            put(out, 0x64, u32(out, 0x64) + delta)
            continue
        width = widths[opcode]
        if not width:
            raise ValueError("Original replay has a zero-width instruction")
        put(out, 0x64, pointer + width)
    raise ValueError("Original command replay exceeded inspection bound")
