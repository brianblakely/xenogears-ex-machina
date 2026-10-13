# HRTF data

`kemar-diffuse.hrir` holds the head-related impulse responses the binaural
renderer places sounds with: the MIT Media Lab's measurements of a KEMAR
dummy head (normal pinna), in their diffuse-field equalized form, 128 samples
at 44.1 kHz for each of 368 directions (elevations -40 to 90 degrees, azimuths
0 to 180; the other side is the mirror image).

- Source: <https://sound.media.mit.edu/resources/KEMAR/diffuse.zip>,
  SHA-256 `a48e73137390626ef1e45d1de6a4d98530c6aa570e71b40f7c4442241ed5fa22`.
- `tools/binaural_hrtf.py` copies the archive's samples into this file
  unchanged (its docstring gives the layout); the file's SHA-256 is
  `4a7f56b11edf0ce857ac94aa13941a798a037fe21681f125ed0f11cfc551ab40`.
- Terms, from the archive's README: "This data is Copyright 1994 by the MIT
  Media Laboratory. It is provided free with no restrictions on use,
  provided the authors are cited when the data is used in any research or
  commercial application." The measurements' documentation (hrtfdoc.txt)
  says the same: "It is provided without any usage restrictions. We request
  that you cite the authors when using this data for research or commercial
  applications."
- Citation: Bill Gardner and Keith Martin, "HRTF Measurements of a KEMAR
  Dummy-Head Microphone", MIT Media Lab Perceptual Computing Technical
  Report #280, May 1994; W. G. Gardner and K. D. Martin, "HRTF measurements
  of a KEMAR", J. Acoust. Soc. Am. 97(6), June 1995.

The renderer derives what it uses from these samples when it starts (onset
alignment, interaural delays, level normalization: `src/hrtf.rs`); nothing
else is precomputed.
