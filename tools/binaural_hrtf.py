"""Repack the MIT KEMAR diffuse-field equalized HRTFs for the binaural renderer.

The renderer (src/runtime/crates/xem-binaural) reads the 368 measured
directions of the MIT Media Lab's KEMAR set, diffuse-field equalized, at
44.1 kHz (the SPU's rate), from one file: this script copies the samples of
the archive's WAV files into it unchanged, in a fixed order, and checks the
archive against the SHA-256 recorded here. The data's copyright, permission
and citation are in src/runtime/crates/xem-binaural/hrtf/README.md.

    curl -O https://sound.media.mit.edu/resources/KEMAR/diffuse.zip
    python3 tools/binaural_hrtf.py diffuse.zip src/runtime/crates/xem-binaural/hrtf/kemar-diffuse.hrir

Layout (little-endian): "KEMR", u16 version 1, u16 entry count; then each
entry: i16 elevation and i16 azimuth (degrees: azimuth 0 ahead, 90 to the
right), and 128 frames of interleaved left/right i16 samples. Entries go by
elevation (-40 to 90), then azimuth (0 to 180), as the archive names them
(elevN/HNeAAAa.wav).
"""

from __future__ import annotations

import hashlib
import io
import re
import struct
import sys
import wave
import zipfile

ARCHIVE_SHA256 = "a48e73137390626ef1e45d1de6a4d98530c6aa570e71b40f7c4442241ed5fa22"
FRAMES = 128
NAME = re.compile(r"(?:diffuse/)?elev(-?\d+)/H(-?\d+)e(\d{3})a\.wav$")


def main(archive: str, output: str) -> None:
    data = open(archive, "rb").read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != ARCHIVE_SHA256:
        raise SystemExit(f"{archive}: SHA-256 {digest}, expected {ARCHIVE_SHA256}")
    entries = []
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        for name in zf.namelist():
            match = NAME.search(name)
            if not match:
                continue
            elevation, file_elevation, azimuth = int(match[1]), int(match[2]), int(match[3])
            if elevation != file_elevation or not -40 <= elevation <= 90 or not 0 <= azimuth <= 180:
                raise SystemExit(f"{name}: unexpected direction")
            with wave.open(io.BytesIO(zf.read(name))) as w:
                shape = (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes(), w.getcomptype())
                if shape != (2, 2, 44100, FRAMES, "NONE"):
                    raise SystemExit(f"{name}: {shape}, expected stereo 16-bit 44.1 kHz with {FRAMES} frames")
                entries.append((elevation, azimuth, w.readframes(FRAMES)))
    entries.sort(key=lambda entry: (entry[0], entry[1]))
    if len(entries) != 368 or len({(e, a) for e, a, _ in entries}) != len(entries):
        raise SystemExit(f"expected 368 distinct directions, found {len(entries)}")
    out = bytearray(b"KEMR" + struct.pack("<HH", 1, len(entries)))
    for elevation, azimuth, frames in entries:
        out += struct.pack("<hh", elevation, azimuth) + frames
    with open(output, "wb") as f:
        f.write(out)
    print(f"{output}: {len(entries)} directions, {len(out)} bytes, SHA-256 {hashlib.sha256(out).hexdigest()}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    main(sys.argv[1], sys.argv[2])
