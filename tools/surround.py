#!/usr/bin/env python3
"""Upmix stereo WAV windows to 5.1 with FreeSurround and report where they land.

    python3 tools/surround.py WAV START:END [START:END ...] [--out PREFIX]

For each window (seconds) the stereo is decoded by `freesurround` (the
nix/runtime shell's FreeSurround filter, a matrix-surround style decoder) and
the energy of each 5.1 channel is printed as a share of the total, together
with the input's L/R correlation. In-phase common content steers to the
centre, one-sided content to its front speaker and anti-phase content to the
rears. --out writes each window's 5.1 as PREFIX-START-END.wav (float32,
FL FC FR BL BR LFE) for listening.
"""

import array
import math
import subprocess
import sys
import wave

CHANNELS = ["FL", "FC", "FR", "BL", "BR", "LFE"]


def read_window(path, start, end):
    with wave.open(path) as wav:
        if wav.getnchannels() != 2 or wav.getsampwidth() != 2:
            raise ValueError(f"{path}: expected 16-bit stereo")
        rate = wav.getframerate()
        wav.setpos(int(start * rate))
        samples = array.array("h", wav.readframes(int((end - start) * rate)))
    if sys.byteorder != "little":
        samples.byteswap()
    return rate, array.array("f", (s / 32768 for s in samples))


def upmix(rate, stereo):
    result = subprocess.run(
        ["freesurround", str(rate)], input=stereo.tobytes(), capture_output=True, check=True
    )
    surround = array.array("f")
    surround.frombytes(result.stdout)
    return surround


def correlation(stereo):
    left, right = stereo[0::2], stereo[1::2]
    lr = sum(a * b for a, b in zip(left, right, strict=True))
    ll = sum(a * a for a in left)
    rr = sum(b * b for b in right)
    return lr / math.sqrt(ll * rr) if ll and rr else 0.0


def write_wav(path, rate, surround):
    # WAVE_FORMAT_IEEE_FLOAT, which the wave module cannot write.
    data = surround.tobytes()
    header = b"RIFF" + (36 + len(data)).to_bytes(4, "little") + b"WAVEfmt "
    header += (16).to_bytes(4, "little") + (3).to_bytes(2, "little") + (6).to_bytes(2, "little")
    header += rate.to_bytes(4, "little") + (rate * 24).to_bytes(4, "little")
    header += (24).to_bytes(2, "little") + (32).to_bytes(2, "little")
    with open(path, "wb") as out:
        out.write(header + b"data" + len(data).to_bytes(4, "little") + data)


def main(argv):
    out = None
    if "--out" in argv:
        at = argv.index("--out")
        out = argv[at + 1]
        argv = argv[:at] + argv[at + 2 :]
    if len(argv) < 2:
        sys.exit(__doc__)
    path, windows = argv[0], argv[1:]
    print(f"{'window':>11} {'L/R corr':>8} " + " ".join(f"{c:>5}" for c in CHANNELS) + "  rear")
    for window in windows:
        start, end = (float(t) for t in window.split(":"))
        rate, stereo = read_window(path, start, end)
        surround = upmix(rate, stereo)
        energy = [sum(s * s for s in surround[c::6]) for c in range(6)]
        total = sum(energy) or 1.0
        shares = [e / total for e in energy]
        print(
            f"{window:>11} {correlation(stereo):8.3f} "
            + " ".join(f"{s:5.3f}" for s in shares)
            + f" {shares[3] + shares[4]:5.3f}"
        )
        if out:
            write_wav(f"{out}-{window.replace(':', '-')}.wav", rate, surround)


if __name__ == "__main__":
    main(sys.argv[1:])
