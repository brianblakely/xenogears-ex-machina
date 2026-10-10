# Cue timelines

Readers that step through (time, value) entries embedded in an overlay's data
and act on each entry in turn, without dispatching on it. The tables are
user-supplied: the units link them from the user's image (`INCLUDE_ASSET`;
`asset` lines in the targets' classification files), and
`python3 -m tools.analysis.overlay_scripts` decodes them from each disc's own
packed overlay.

## Field movie sound timelines

- **Reader:** `field_movie_play_due_sounds` in the `field` overlay (Disc 1 file 36,
  Disc 2 file 31). The field movie player `field_movie_play` seeks the timeline with
  `field_movie_load_sound_bank` before the movie starts and runs
  `field_movie_play_due_sounds` after each movie step (`field_movie_run_frames`); at
  the end `field_movie_release_sound_bank` releases the bank.
- **Table:** `field_movie_sound_timelines`, 96 u16 (frame, sound) pairs: a leading
  end, then one run per movie sound-effect bank, each ended by an entry whose frame
  is 0xFFFF (sound 0). The asset line is in
  `decomp/targets/overlays/field.classification.txt`.
- **Bank:** event `fe a0` (`field_event_play_movie_sound`) requests a movie with its
  sound bank in operand 9; 0xFF requests none, as events 60 and 67 always do, and
  then `field_movie_play_due_sounds` plays nothing. `field_movie_load_sound_bank`
  loads file 0x115 + bank of directory (0x1C, 0), adds it to the open effect banks
  (`sound_add_effect_bank`) and leaves the position
  `field_movie_sound_timeline_index` past bank + 1 ends.
- **Timing:** the movie's frame callback (`field_movie_frame_callback`) stores the frame in
  `field_movie_frame`. `field_movie_play_due_sounds` plays, in table order, every entry whose frame
  plus the movie's sound start (`FIELD_MOVIE.sound_start`, event `fe a0`'s operand
  5) the movie frame has reached, several in one call when they are due together:
  the loaded bank's effect in the low byte of the sound, on the voice pair in bits
  8-10 (`sound_play_effect_on_channel` gets pair * 2). Neither routine tests the end itself:
  frame 0xFFFF lies past every movie.
- **Coverage:** both discs hold the same table: 10 runs, 85 entries, each run's
  frames in order and no sound with bits 11-15 set. Banks 0-9 are files
  0x115-0x11E (Disc 1 slots 392-401, Disc 2 slots 387-396), each a "seds" bank
  that `sound_check_file` accepts, and each run plays every effect of its bank
  except effect 0 exactly once. The field event scripts request each of banks
  0-9 with `fe a0` (0-3 once, 4-9 three times), no bank 11 times on Disc 1 and 10
  on Disc 2, and once a bank held in a variable.
- **Tool:** `--sweep` prints these aggregates; `--list movie-sounds [--disc N]`
  prints each bank's run. `tests/test_overlay_scripts.py` checks the decoder against
  `field_movie_load_sound_bank`, `field_movie_play_due_sounds` and
  `field_event_play_movie_sound`, and that the table stays an asset.

## World map terrain texture animations

- **Readers:** `worldmap_texture_anim_advance` and `worldmap_texture_anim2_advance`
  in the `worldmap` overlay (Disc 1 file 37, Disc 2 file 32), run once per frame of
  the world-map loop (`worldmap_run_frame_loop`), an update below.
  `worldmap_texture_anim_create` and `worldmap_texture_anim2_create` create the
  animations of the area file's two animation sections (+0x20 and +0x24: a count,
  then each animation's image offset): animation i gets slot i of
  `worldmap_texture_anim_slots` (two slots) or `worldmap_texture_anim2_slots`
  (three), frame 0 and timer 1.
- **Tables:** a slot is a `TexAnimSlot` {RECT rect; s32; frames}, the VRAM rect the
  images go to and the frame sequence; the slots are C. The sequences
  `worldmap_texture_anim_slot0_frames`, `worldmap_texture_anim_slot1_frames` (the
  slots of `worldmap_texture_anim_slots`) and `worldmap_texture_anim2_slot0_frames`,
  `worldmap_texture_anim2_slot1_frames`, `worldmap_texture_anim2_slot2_frames`
  (those of `worldmap_texture_anim2_slots`) are `TexAnimFrame` {s16 image; s16
  duration} runs ended by a negative duration, assets in
  `decomp/targets/overlays/worldmap.classification.txt`.
- **Timing:** each update counts a slot's timer down; at 0 the stepper moves to the
  next frame and takes its duration, restarts at frame 0 with that frame's
  duration when the new one is negative, and uploads the frame's image into the
  slot's rect (16 bytes per image in the first set, w * h * 2 in the second).
  Update 1 uploads frame 1; frame 0 follows the end.
- **Coverage:** both discs hold the same five runs, 31 frames: eight images of 5
  updates each in the first set and five of 8 in the second, a cycle of 40 updates
  per slot. Open: how many animations each area starts comes from its area file,
  which this decoder does not read; a count past two (or three) would take slots
  beyond `worldmap_texture_anim_slots` (`worldmap_texture_anim2_slots`).
- **Tool:** `--sweep`; `--list worldmap-textures [--disc N]` prints each slot's rect
  and frames. `tests/test_overlay_scripts.py` checks the decoder against the slot
  tables, the creators and the steppers, and that the runs stay assets.
