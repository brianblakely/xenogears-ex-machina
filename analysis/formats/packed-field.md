# Packed blocks and the field bundle

This reconstruction comes from the two supplied, hash-defined original discs.
[EVID-REF-014](../findings/EVID-REF-014.json) records source coordinates and
measurements. It is analysis source, with independent review still outstanding.

The resident function at `0x80032eb4` takes source memory in `a0` and destination
memory in `a1`, returning the initial destination in `v0`. A little-endian u32
at source offset zero declares the output length. Each flag byte selects eight
tokens, least significant bit first. A zero bit copies one literal. A one bit
reads a two-byte back reference: the first byte and the low four bits of the
second form a 12-bit backward distance; the second byte's high four bits give
the copy length minus three. Copying advances one byte at a time, permitting
overlap with bytes written by that same copy.

The function tests output equality only between flag groups. It prefetches the
next flag even when returning. The reconstruction distinguishes token bytes from
the extra input byte read at exit. A bad distance, truncated source, oversized
output or group crossing the output bound raises an error. The original routine
does not provide those safety checks; the analysis code does not reproduce
unbounded memory access or an infinite decode.

## Three separate boundaries

The field loader uses nine logical sizes at `+0x10c` and nine packed offsets at
`+0x130`. Its allocation requests add 16 bytes to each logical size. The packed
stream has its own output length, which can exceed the logical component size.
Compressed streams can overlap the following stream's header while producing
their final padding. The wrapper at field-overlay `0x8007008c` ignores its first
size argument and forwards source and destination to the resident decoder.

The field file's measured source-slot length, physical CD-sector span and
in-memory surroundings are different bounds. Field 23's final component starts
at file offset 109,992. Its logical output is 72 bytes, packed output is 78 bytes,
and total source reads are 65 bytes. These reads end five bytes beyond the
110,052-byte file. At the captured original return, four adjacent RAM bytes
become output padding at offsets 74–77; the fifth byte is the exit prefetch.
They differ from the zero padding in the physical disc sector. Disc 2 field 15
also has a five-byte read beyond its file and a differing padded output.

The validator records both results. It hashes the entire original loaded file,
captures its actual boundary bytes, reconstructs the decoder using those exact
inputs, and compares every output byte by hash plus the original input/end/return
pointers. It separately checks that the logical component agrees with the
disc-derived interpretation. It does not mask output bytes or append invented
zeros. The identity and lifetime of the adjacent allocation remain analysis
work; this observation does not establish a general allocator model.

## Recovered structures

The event component contains a 128-byte variable-type bitmap, an actor count at
`+0x80`, and 32 little-endian u16 entry PCs per actor beginning at `+0x84`.
Bytecode follows those rows; PCs are relative to that bytecode. The parser checks
every entry and preserves the bitmap. Event scheduling and instruction meaning
need separate original evidence.

The collision component holds an active layer count, four triangle-byte counts,
an attribute offset and pairs of triangle/vertex offsets. Triangles occupy 14
bytes: three signed vertex indices, three opaque u16 adjacency fields and an
opaque u16 attribute field. Vertices have four signed halfwords. The original
loader reads all four triangle counts even when fewer layers are active.
Traversal meaning, material semantics and the unused vertex halfword remain
outside this structural finding.

## Reproduce locally

Verify the supplied CHDs with the pinned `#analysis` shell and recheck raw-track
hashes before a new experiment. The following example uses the public painting
room scenario. Use new output names on each run.

```sh
nix --extra-experimental-features 'nix-command flakes' develop path:./nix#observation-trace
python3 tools/analysis/verify_decoder.py prepare \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 --map 14 \
  --output .local/decoder-example/spec.json
python3 tools/reference/scenario.py painting-room \
  --content 'discs/Xenogears disc 1.chd' \
  --trace-instructions .local/decoder-example/spec.json \
  --output .local/decoder-example/run
python3 tools/analysis/verify_decoder.py compare \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 --map 14 \
  --capture .local/decoder-example/run/capture \
  --output .local/decoder-example/comparison.json
python3 tools/analysis/verify_field.py \
  --raw .local/references/source-1/disc.bin \
  --profile na-slus-00664-39c547a9afc6 --map 14 \
  --ram .local/decoder-example/run/capture/final.ram \
  --output .local/decoder-example/structures.json
```

The three verified cases cover 27 component calls and 374,216 decoded bytes.
They establish neither all original compression variants nor the complete
field format. Public tests contain authored synthetic fixtures only. Original
data, source-memory context, screenshots, audio and traces stay in `.local/`.
