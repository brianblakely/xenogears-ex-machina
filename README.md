# Xenogears: Ex Machina

**Stand tall, and shake the heavens.**

Xenogears: Ex Machina (XEM) is an independent, open-source project to preserve the
PlayStation role-playing game Xenogears and bring it to modern computers, mobile
devices and VR headsets.
The ambition is to keep the game's identity — its world, artwork and gameplay —
while opening the door to modern presentation, more ways to play, and tools for
creating new adventures.

## Project status

| Milestone | Status | Scope |
| --- | --- | --- |
| Original-game decompilation | **Complete** | Both [supported North American discs](analysis/reference-profiles.json); all 26 program components rebuild byte for byte. |
| Modern platform foundation | **Next** | Shared native and browser runtime. |
| First playable section | Planned | Exploration, dialogue, battle, menus, saving and media playback. |
| Complete game | Planned | Both-disc story, optional content and minigames. |
| Enhanced play and creation tools | Planned | Modern visuals, VR, conveniences, mods and desktop editors. |
| Playable release | **Not yet available** | Tested native downloads and browser play. |

[Completion record](https://github.com/brianblakely/xenogears-ex-machina/commit/e21f38248e8da2988f85e4f9b238aca4d2732a98)
· [Build and verification guide](docs/matching.md) · [Full roadmap](plan.md)

## Why recover the original code?

Decompilation means working backward from the game on disc to reconstruct source
code that developers can read, study and change. It produces reconstructed source,
not the studio's original development files. A byte-for-byte match provides a
precise reference for preserving the original game while adapting it to new
hardware.

XEM's goal is to run that recovered logic directly on modern hardware, not wrap a
PlayStation emulator in a new interface. The browser version will share the same
game logic.

## Planned player experience

### Play across more devices

The target platforms are Linux, Windows, macOS, Android and Meta Horizon OS, plus
web browsers on desktop, mobile and headsets. Development is led on Arch Linux.
Browser-based VR through WebXR is also planned for compatible browsers and
devices. Full editing tools are desktop-focused; mobile, headset and browser
versions are intended for playing the game and compatible prebuilt mods.

### Keep the original art. Choose a new look.

Modern display options will support higher resolutions, wider screens and
smoother motion while keeping the original gameplay timing intact. Players will
be able to choose modern presentation, an optional HD-2D-inspired look, or a
PS1-style presentation with selectable original visual quirks.

The HD-2D-inspired mode will bring scene lighting, shadows, atmosphere and
adjustable depth of field to the existing game art. It is an effects treatment,
not an art remake: the original sprites, textures, models and animations remain,
with no replacement art pack required. Graphics style and flat-screen or VR
viewing will be separate choices.

### See the world from a new perspective

Seated stereo and VR modes will offer a virtual screen and, later, a scalable
diorama: a tabletop-like view of the game's world that players can enlarge or
shrink. A stationary first-person inspection mode, on both ordinary screens and
in VR, will let players look around without turning Xenogears into a
first-person action game.

Optional hand and gaze controls are planned for devices that support them, with
conventional alternatives. VR is an additional way to experience the game, not
a requirement to play.

### Make a long adventure easier to live with

Planned conveniences include save states, rewind, fast-forward, supported
original-save import and export, and modern cutscene skipping. Gameplay options
will include disabling random battles in identified platforming-heavy areas.

Clearer movie playback and headphone surround derived from the game's original
Wide sound mode are later goals. The audio work will be grounded in verification
of the original signal and listening intent, with original playback options
retained.

### Create new Xenogears experiences

Mod support and desktop creation tools will open the way to custom levels,
cutscenes, battles and minigames. The plan includes mod management, gameplay
extensions and graphical editors built around the same game that players use.

AI-assisted authoring and automated playtesting are part of that vision.
Software agents will be able to play and inspect the game directly, repeat
scenarios and test changes, with or without a visible game window. Human creators
and agents will work through the same editing, debugging and game services rather
than a separate approximation of Xenogears.

## Built around your own copy

XEM provides source code and tools, not the original game discs. Original disc
images, artwork, music and other extracted game assets are not distributed with
the project. Matching builds use data from the user's own supported discs; the
planned native and browser versions will also import game data locally rather
than bundle it or upload users' disc images and saves.

The project's authored source is available under the [MIT License](LICENSE).

## Follow the project or get involved

Follow this repository for milestones and see the [full roadmap](plan.md) for the
planned progression from preservation to play to creation.

Developers can start with the [development guide](docs/development.md),
[porting handbook](docs/later-phases.md) and
[contribution guidelines](CONTRIBUTING.md).
