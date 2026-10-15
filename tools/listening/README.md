# The Binaural listening test

A test you take yourself, over headphones, of the Binaural sound mode
(docs/runtime.md, Binaural sound mode; plan.md, Phase 8): where you hear
sounds the renderer places, against the KEMAR set's own responses and the
original Stereo pan law, and how coherent the arranged music sounds against
the original mix. It runs in a local page; every answer is saved on this
machine. The stimuli are made from your disc and stay in `.local/listening`
(ignored by git): never commit or publish them, and share results only as
you choose. Nothing here judges you while you listen; `analyse.py` reads
the answers afterwards, beside the calibrated model listeners' predictions
(tools/binaural_models/evaluate.m).

One person's answers describe that person with those headphones, not
listeners at large. The test is optional: the plan's verification uses
the model listeners and objective measures, not people.

## What you need

- Headphones (the KEMAR set is diffuse-field equalized, so open or
  diffuse-field-tuned headphones suit it best; note the model on the first
  screen) and a quiet room.
- The runtime shell, which has cargo, Python and a browser. From the
  repository's root:

      nix --extra-experimental-features 'nix-command flakes' develop path:./nix/runtime

  and run the commands below in it, from the repository's root.
- For the music: disc 1 at `discs/Xenogears disc 1.chd` (or `XEM_DISC1`)
  and the game module, built once with

      python3 tools/game_module.py --stubs

  which needs the matched PS1 links (`make -C src/decomp all-verify`;
  docs/runtime.md, The game module).
- Time: three localization sessions of about 30-40 minutes and two
  coherence sessions of about 45-60 minutes; take them on different days if
  you like. A session can be left and resumed.

## Steps

1. Make the localization stimuli (a few seconds once built; no disc
   needed):

       (cd src/runtime && cargo run --release -p xem-headless --example listening -- localization)

   This writes `.local/listening/localization.json` and
   `.local/listening/localization/` (about 110 MB).

2. Make the music excerpts (2 to 3 minutes):

       (cd src/runtime && cargo run --release -p xem-headless --example listening -- music)

   This writes `.local/listening/coherence.json`, `coherence/` (about 100
   MB) and `states/`, the saved state each excerpt starts from. To hear a
   scene of your own instead of a built-in one (a boss you reached, say),
   save a state in the desktop app (a `.xemstate` file of the same game
   module) and pass it: with `--state lahan-gear-battle=PATH` that excerpt
   starts there, labelled as your state, and the others are made as
   before. A disc 2 state is taken too while disc 2 lies beside disc 1 in
   `discs/`. `--only NAME,...` makes a test of those excerpts alone (its
   manifest holds only them). A name that is not an excerpt's (the table
   below) is refused. Both commands take `--out DIR` and `--seed N` (2026
   by default; the same seed and commit make the same files).

3. Serve the page and open it:

       python3 tools/listening/serve.py

   then open http://127.0.0.1:8642/ in a browser (Chromium or Firefox). It
   listens on this machine only. `--port`, `--stimuli DIR` and `--results
   DIR` change where it serves and saves (`.local/listening/results` by
   default).

4. Enter a name for yourself and your headphones, and take the sessions:
   L1, L2 and L3 (localization; L1 starts with longer training), C1 and C2
   (coherence). Each session first checks the level (set your volume to the
   pink noise: comfortable, about a conversation's loudness, then leave
   it), which ear is which, and the headphones' polarity. Each answer is
   saved at once to `results/NAME-SESSION.json` (the checks, every trial's
   stimulus, level rove and answer, every rating and how often each
   version was played); a session taken again goes to `NAME-SESSION-r2.json`.

5. Analyse:

       python3 tools/listening/analyse.py

   It prints a report and writes `.local/listening/analysis.json`; with
   `build/binaural/models.json` present (the model evaluation) the model
   listeners' predictions sit beside yours. It analyses one listener: when
   the results hold answers under several names, it lists them and stops,
   and `--listener NAME` (the name as the results' file names show it)
   chooses one. `--results DIR`, `--json FILE`, `--bootstrap N` (2000) and
   `--seed N` change its inputs and intervals.

## Localization

Half-second bursts of seeded Gaussian noise (20 ms ramps) from 48
directions: elevations -30, 0, 30 and 60 degrees, azimuths every 30, all
measured directions of the KEMAR set. Three conditions:

- **rendered**: placed by the Binaural renderer as the game's voices are (a
  voice at the Stereo pan law's centre routed to one source: the KEMAR set
  interpolated, levelled, its bass shelf, the interaural delay as phase);
- **kemar**: the KEMAR set's raw responses at that direction, convolved;
- **stereo**: the original Stereo pan law over headphones (the direction's
  lateral position as the pan), the control.

Each condition is scaled so that its bursts' median loudness is -23 LUFS
(ITU-R BS.1770-4); each keeps the level differences its directions have,
and every trial's level is roved by up to 3 dB either way. A session holds
each direction twice per condition (96 trials a condition, 288 over three
sessions), 32 moving sounds (rendered and stereo: 60 degrees in 1.5 s
across the front, the sides and the back, and from 30 below to 30 above at
four azimuths, each way; the raw set has no directions between its measured
ones, so it is not moved) and 8 catch trials (noise in one ear only),
shuffled into four blocks. L1 starts with 24 training trials, L2 and L3
with 12: after each, the dial shows where the sound was.

You answer on a dial seen from above (ahead at the top; click or drag, or
the arrow keys) and an elevation slider (90 above to 90 below). Every
answer needs its height set: move or press the slider, or press Level for
a sound at ear height (the slider starts level, and a height left there
untouched is not taken for an answer). For a moving sound you mark where
it started and where it ended. Each test sound plays once. Keep your head
still, facing the screen.

## Musical coherence

Eight excerpts, each reached from boot by a fixed recipe and saved as a
state, then presented from it at Spatial Music immersion 0, 35, 70 and 100
(Positional Effects off, so effects keep their original mix):

| Excerpt | Kind | Recipe |
| --- | --- | --- |
| lahan | town | field 1, Lahan village |
| kislev | town | field 213 |
| kislev-199 | field | field 199 |
| solaris | field | field 466 |
| world-map | world map | boot into the world map |
| battle | battle | the debug battle selector's battle: the ordinary battle theme |
| lahan-gear-battle | event battle, in place of a boss | field 2 (Lahan in flames), Cross at every dialogue until Weltall's battle |
| title-movie | CD estimate | the title movie's music |

An event battle stands in for a boss. The routes checked reach two story
battles, Weltall's in burning Lahan (from boot) and one on disc 2's
`soul-vessel` route (from the test saves `tools/memcard_saves.py` makes out
of your memory card: Fei, Elly and a third member on foot against four
small creatures and a tall one). Both keep the music their scene was
already playing, and whether either counts as a boss is not established.
The generator reaches its scenes from boot, without your card, so
Weltall's battle is used; `--state lahan-gear-battle=PATH` takes a state
of a boss you reached (or of the soul vessel's battle) instead.

The title movie's music is the only premixed (CD) music the routes reach,
and while a movie plays the runtime keeps its mix original: its immersions
are the capture mixed again through the premixed-music estimate, so they
show the estimate on real game music, not what the title presents.

Each screen has the labelled reference (the original Stereo output) and six
versions to rate from 0 to 100: the reference again (hidden), a
disintegrated low anchor (every musical part at a random place; for the
title, the CD pair's frequency bands at random places), and the four
immersions. The versions play in step and loop; switch freely, play every
one and move every slider before going on. Five questions, each on its own
screen: coherence (do the parts hold together as one ensemble), lead and
foundation (do the melody, bass and rhythm stay put), envelopment, tone
colour (against the reference) and preference. Every condition is brought
to -23 LUFS. C1 and C2 each rate every excerpt once per question, so each
rating is given twice.

## What the analysis computes

Over the static test trials of each condition, in the interaural-polar
coordinates of the model evaluation: the lateral RMS error and bias (bias
positive when answers lie further out than targets at least 10 degrees to a
side); for targets within 30 degrees of the median plane, the quadrant error
rate (polar error over 90 degrees) and the local polar RMS error (the rest);
front/back confusions (targets at least 30 degrees from the frontal plane
answered on its other side); up/down confusions (targets at least 30
degrees above or below the horizontal plane answered on its other side);
the elevation gain (the slope of answered on target elevation). Each comes
with a 95% bootstrap interval over trials. Moving trials: start and end
errors and whether the answered movement went the right way. Catch trials:
the share answered on the side that sounded (under 90% is noted). Beside
them, the models' pool median and range and the KEMAR listener (theirs are
over every measured direction).

Coherence: each question's ratings by condition (mean, median, interval;
by excerpt too), each condition against the hidden reference; the
post-screening of ITU-R BS.1534-3 (the hidden reference at 90 or more in at
least 85% of the trials of questions the reference tops: coherence, lead and
foundation, tone colour); how far ratings moved between the two sessions;
and how envelopment ratings follow the interaural cross-correlation the
generator measured from 500 Hz to 4 kHz (a proxy, not a model). No
validated model of musical coherence exists.

## Checking the page

    (cd src/runtime/web && node --test tests/listening.test.mjs)

runs the page in headless Chromium on a small synthetic set: a script
answers every planned session through the page's controls (no answer is
taken before its height is set; a session is left mid-block and resumed;
a save the server refuses stops the session with a message, and starting
it again asks that trial again), and the analysis must read its answers
back. With `XEM_LISTENING_STIMULI=DIR` (an absolute path) it runs every
session of the stimuli in DIR instead (about half an hour), answers saved
to a temporary directory. Nobody listens there.

An unfinished session resumes only on the stimuli it was answering: each
manifest's `generated` block holds a SHA-256 of every file the generator
wrote and of the rest of the manifest, so stimuli made again in any other
way (another commit or seed, `--only`, `--state`, a changed tree) start
the session afresh in a file of its own, and the analysis leaves the old
answers out. Making them again the same way (the same commit, seed,
options and tree) keeps the answers.
