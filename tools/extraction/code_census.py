#!/usr/bin/env python3
"""Check that the matching targets hold all the code on both discs.

    code_census.py [--jobs N]

Code is any aligned `jr $ra` word (0x03E00008), a leaf's return as well as a
framed one. The targets are the ORIGINAL_SHA256 images of decomp/targets/*/*.mk;
each must occur on a disc, and on each disc:

- Every indexed file except movie streams, and the ISO 9660 boot program, is
  read with its whole sectors. Its forms are its raw bytes and every packed
  block that the original decoder (tools/analysis/packed.py) completes from any
  byte offset; a form that holds code must be a target image. A raw word is
  data instead where it lies in the token stream of a packed block of at least
  256 decoded bytes (the block is checked itself) or among the samples of a
  `wds ` wave bank, which the resident transfers to SPU memory (80037FD8). A
  block of literal tokens only repeats its file's bytes: its words count as the
  raw words they copy.
- Every movie stream (its first sector opens with the STR frame header
  0x80010160) spans size / 2336 sectors, each a video frame, XA audio or empty.
- No sector outside these files (the system area, the ISO 9660 descriptors,
  path tables and root directory, the disc index and directory table, the
  postgap) holds code.

It exits 1 on any other code, a missing target or a malformed stream. Inputs
are the local raw tracks and manifests (tools/extraction/disc_files.py); output
names original files only by slot and offset.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
from collections import Counter
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.analysis.packed import PackedError, PackedTruncated, decode_block  # noqa: E402

SECTOR = 2352
USER = 24
JR_RA = (0x03E00008).to_bytes(4, "little")
STR_FRAME = 0x80010160
STREAM_SECTOR = 2336  # a stream's size counts each sector's subheader and Form 2 data
OUTPUT_LIMIT = 0x200000  # the 2 MiB of RAM
LARGE_BLOCK = 0x100
# The start of a block that can hold a word: an output size of 8 (one group of
# literals) to 0x20ffff (decode_block rejects more than OUTPUT_LIMIT), then a
# flag whose first token is a literal (a copy has no output to copy from).
EVEN = b"".join(b"\\x%02x" % value for value in range(0, 256, 2))
HEADER = re.compile(
    rb"(?=(?:(?:[\x08-\xff].|.[\x01-\xff])[\x00-\x20]|..[\x01-\x20])\x00[%s])" % EVEN, re.DOTALL
)


def returns(data: bytes) -> list[int]:
    """Offsets of the aligned jr $ra words in data."""
    found = []
    at = data.find(JR_RA)
    while at >= 0:
        if at % 4 == 0:
            found.append(at)
        at = data.find(JR_RA, at + 1)
    return found


def blocks(data: bytes):
    """(offset, token bytes, output) of every packed block decodable from data."""
    view = memoryview(data)
    for match in HEADER.finditer(data):
        start = match.start()
        try:
            block = decode_block(view[start:], output_limit=OUTPUT_LIMIT)
        except PackedTruncated as error:
            # Complete but for the read of a further flag past the sectors.
            if len(error.output) == int.from_bytes(view[start : start + 4], "little"):
                yield start, len(data) - start, error.output
        except PackedError:
            pass
        else:
            yield start, block.token_bytes, block.data


def literal_source(start: int, tokens: int, output: bytes, offset: int) -> int | None:
    """File offset of output byte `offset` of a block of literal tokens only
    (each group a flag byte and eight literals), else None."""
    if len(output) % 8 or tokens != 4 + len(output) // 8 * 9:
        return None
    return start + 5 + offset // 8 * 9 + offset % 8


def wave_samples(data: bytes) -> range | None:
    """The samples of a `wds ` wave bank: SoundSequence (resident sound.h) keeps
    their size at +0x14 and file offset at +0x18."""
    if data[:4] != b"wds ":
        return None
    size, offset = struct.unpack_from("<II", data, 0x14)
    return range(offset, offset + size)


def census_file(job: tuple) -> tuple[list[str], set[str], int, int]:
    """Lines on the file's code, the targets it holds, its unexplained code
    and its number of packed blocks."""
    where, size, data, targets = job
    lines, found, unknown, token_streams = [], set(), 0, []
    raw_name = targets.get(hashlib.sha256(data[:size]).hexdigest())
    if raw_name:
        lines.append(f"{where} raw {size:#x} -> {raw_name}")
        found.add(raw_name)
    count = 0
    for start, tokens, output in blocks(data):
        count += 1
        if len(output) >= LARGE_BLOCK:
            token_streams.append((start, start + tokens))
        words = returns(output)
        if not words:
            continue
        name = targets.get(hashlib.sha256(output).hexdigest())
        if name:
            lines.append(f"{where} packed +{start:#x} {len(output):#x} -> {name}")
            found.add(name)
            continue
        sources = [literal_source(start, tokens, output, word) for word in words]
        if all(source is not None and source % 4 == 0 for source in sources):
            continue
        lines.append(f"{where} packed +{start:#x} {len(output):#x} -> UNMATCHED")
        unknown += 1
    samples = wave_samples(data)
    for word in returns(data):
        if raw_name and word < size:
            continue
        stream = next((s for s in token_streams if s[0] <= word < s[1]), None)
        if stream:
            reason = f"data of the packed block at +{stream[0]:#x}"
        elif samples and word in samples:
            reason = "wave bank samples"
        else:
            reason = "UNMATCHED"
            unknown += 1
        lines.append(f"{where} raw +{word:#x} jr $ra -> {reason}")
    return lines, found, unknown, count


def user_data(sector: bytes) -> bytes:
    """A Mode 2 sector's user data: Form 2 (submode bit 5) holds 2324 bytes."""
    return sector[USER : USER + (2324 if sector[18] & 0x20 else 2048)]


def stream_sector(sector: bytes) -> str:
    data = user_data(sector)
    if int.from_bytes(data[:4], "little") == STR_FRAME:
        return "video"
    if sector[18] & 0x04:
        return "audio"
    return "other" if any(data) else "empty"


def target_images() -> dict[str, str]:
    targets = {}
    for config in sorted((ROOT / "decomp/targets").glob("*/*.mk")):
        match = re.search(r"^ORIGINAL_SHA256 := ([0-9a-f]{64})$", config.read_text(), re.M)
        if not match:
            raise SystemExit(f"{config}: no ORIGINAL_SHA256")
        targets[match.group(1)] = config.stem
    return targets


def census_disc(disc: int, targets: dict[str, str], pool: ProcessPoolExecutor) -> tuple[set[str], int]:
    raw = (ROOT / f".local/discs/disc{disc}.bin").read_bytes()
    manifest = json.loads((ROOT / f".local/extract/disc{disc}/manifest.json").read_text())
    total = len(raw) // SECTOR
    owned = bytearray(total)
    jobs, movies, unknown = [], [], 0
    entries = [("boot", manifest["boot"])] + [(f"slot {entry['slot']}", entry) for entry in manifest["files"]]
    for name, entry in entries:
        if entry["size"] <= 0:
            continue
        lba = entry["lba"]
        if int.from_bytes(raw[lba * SECTOR + USER : lba * SECTOR + USER + 4], "little") == STR_FRAME:
            movies.append(entry)
            continue
        count = (entry["size"] + 2047) // 2048
        sectors = [raw[(lba + n) * SECTOR : (lba + n + 1) * SECTOR] for n in range(min(count, total - lba))]
        if len(sectors) < count or any(sector[18] & 0x20 for sector in sectors):
            print(f"disc{disc} {name}: sectors past the track or in Form 2")
            unknown += 1
            continue
        owned[lba : lba + count] = b"\1" * count
        jobs.append((f"disc{disc} {name}", entry["size"], b"".join(s[USER : USER + 2048] for s in sectors), targets))

    found, packed = set(), 0
    for lines, file_found, file_unknown, count in pool.map(census_file, jobs, chunksize=4):
        for line in lines:
            print(line)
        found |= file_found
        unknown += file_unknown
        packed += count
    file_bytes = sum(len(job[2]) for job in jobs)

    kinds = Counter()
    for entry in movies:
        count, rest = divmod(entry["size"], STREAM_SECTOR)
        stream = Counter(
            "file" if owned[lba] else stream_sector(raw[lba * SECTOR : (lba + 1) * SECTOR])
            for lba in range(entry["lba"], entry["lba"] + count)
        )
        kinds += stream
        if rest or stream["other"]:
            print(f"disc{disc} slot {entry['slot']} stream: {rest} bytes past whole sectors, {stream['other']} other sectors")
            unknown += 1
        owned[entry["lba"] : entry["lba"] + count] = b"\2" * count

    outside = nonzero = 0
    for lba in range(total):
        if owned[lba]:
            continue
        outside += 1
        data = user_data(raw[lba * SECTOR : (lba + 1) * SECTOR])
        nonzero += any(data)
        for word in returns(data):
            print(f"disc{disc} sector {lba} +{word:#x} jr $ra -> UNMATCHED")
            unknown += 1
    print(
        f"disc{disc}: {len(jobs)} files with the boot program ({file_bytes} bytes), {packed} packed blocks; "
        f"{len(movies)} movie streams ({kinds['video']} video, {kinds['audio']} audio, "
        f"{kinds['empty']} empty sectors); {outside} sectors outside them ({nonzero} not zero)"
    )
    return found, unknown


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    args = parser.parse_args()
    targets = target_images()
    found, unknown = set(), 0
    with ProcessPoolExecutor(args.jobs) as pool:
        for disc in (1, 2):
            disc_found, disc_unknown = census_disc(disc, targets, pool)
            found |= disc_found
            unknown += disc_unknown
    missing = sorted(set(targets.values()) - found)
    print(f"targets found: {len(found)} of {len(targets)}" + (f" (missing: {', '.join(missing)})" if missing else ""))
    print(f"unmatched code: {unknown}")
    return 1 if unknown or missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
