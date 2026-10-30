# Xenogears Ex Machina — Player's Manual

Welcome to Xenogears Ex Machina. Play with the original look and sound, or
make yourself at home with sharper graphics, different controls, quick saves
and a few ways to make a long adventure easier to fit into your day.

This guide covers the downloadable app on computers, Android devices and
headsets. It explains the app's features without spoiling the story. For help
choosing a download or getting it to open, see the [installation guide](docs/releases.md).
The complete story and every optional activity are still being checked; the
release's current limits are listed there too.

- [Getting started](#getting-started)
- [Playing with a keyboard, mouse or controller](#playing-with-a-keyboard-mouse-or-controller)
- [Touch controls](#touch-controls)
- [Changing your controls](#changing-your-controls)
- [Saving, loading and backups](#saving-loading-and-backups)
- [Rewind, fast-forward and pause](#rewind-fast-forward-and-pause)
- [Gameplay options](#gameplay-options)
- [Making the picture your own](#making-the-picture-your-own)
- [Looking around in first person](#looking-around-in-first-person)
- [Sound](#sound)
- [Playing in a headset](#playing-in-a-headset)
- [The Debug menu](#the-debug-menu)
- [Updates and help](#updates-and-help)

## Getting started

You'll need your own North American Xenogears disc images: Disc 1
(`SLUS-00664`) and Disc 2 (`SLUS-00669`). Both the original and Greatest Hits
editions are supported. The game itself is not included in the download.
Your discs and saves stay on your device.

CHD and BIN/CUE images work. Keep a CUE file together with all of its track
files, and choose the CUE file when importing. Some files ending in `.iso`
also work, but they must be complete raw disc images. If the app rejects an
image, check that it is the supported edition and includes the disc's audio
and movies. Changing its filename won't fix an unsupported image.

You don't need a PlayStation BIOS to play.

### On a computer

1. Download the app for your computer from [Releases](https://github.com/brianblakely/xenogears-ex-machina/releases).
   On Windows or macOS, unzip the download. Keep the included files together.
2. Put the app in a folder where you can save files. Put both disc images
   beside `xem-desktop.exe`, the Linux AppImage, or the macOS app bundle.
3. Open the app. On Linux, first allow the AppImage to run as a program in
   its file properties. On first launch, the app moves recognized disc images
   into a folder called `discs` and creates folders for your saves.
4. Disc 1 starts automatically when it is found. If it isn't found, open
   **Settings → Saves → Import disc…** and choose it yourself.

Keep both discs together in `discs` so the app can find Disc 2 when the game
asks for it. If it needs help finding a disc, its message will tell you;
use **Import disc…** to supply the requested image.

On Steam Deck or another SteamOS device, do the setup in Desktop Mode. Add
the AppImage to Steam as a non-Steam game, then return to Gaming Mode and
launch it from your library. The release page has the full setup steps.

### On Android or a standalone headset

Install the Android / Meta Horizon OS APK. Use **Import disc…** to choose
your local image when you first open the app. Keep the image where it is
after importing: the app needs permission to read that file again next time.
If you move it or remove that permission, import it again.

Your cards and saves live in the app's storage. Export backups before
uninstalling the app or clearing its data.

### Finding Settings

Press **Esc**, or hold the controller's **Back/Select** button and press
**Start**. Touch players can tap **Settings**. Headset controls are explained
[below](#playing-in-a-headset).

Choose a section from the sidebar. On a small screen, the sections appear
as tabs along the top. Changes are saved as you make them; control bindings
have their own Save buttons. Close Settings to return to the game.

**Reset** at the top restarts the game, like pressing Reset on a console.
Save first. It does not mean “restore my settings.” **Quit** asks before
closing the app. **About** shows your version and the app's credits.

### Using Settings with a controller

On a computer, on Android and in a browser, the controller works Settings
once it is open (the buttons are named as in the
[controls table](#playing-with-a-keyboard-mouse-or-controller)):

| To | Press |
| --- | --- |
| Go to the next or previous setting | D-pad or left stick down/up |
| Move a slider, change a choice or a number | D-pad or left stick left/right |
| Press a button, flip a switch, open a list | Bottom button |
| Close a list or an editor, then Settings itself | Right button |
| Go to the previous or next section | L1 / R1 |

Your first press shows where you are: an outline marks the setting the
controller is on, and the page scrolls to keep it in view. Hold a direction
to repeat it. Left and right also move along a row of buttons. In an open
list, up and down choose and the bottom button closes it. **Hold to rewind**
rewinds for as long as you hold the bottom button. Back + Start still closes
Settings at any time. Headset controllers point at Settings instead
([below](#headset-controllers)).

## Playing with a keyboard, mouse or controller

These are the starting controls. You can change them in Settings.

Controller names below describe the button's position, so the table works
even if your controller uses different letters. **Bottom** is A on an Xbox
controller or Cross on a PlayStation controller; **right** is B/Circle,
**left** is X/Square, and **top** is Y/Triangle. **Back** means the small
Back, Select or View button. L1/R1 are the shoulder buttons; LT/RT are the
triggers. Clicking a stick means pressing it down like a button.

| What you want to do | Keyboard | Controller |
| --- | --- | --- |
| Move or choose a menu entry | W, A, S, D | Left stick or D-pad |
| Confirm, talk or examine | E or backslash (`\`) | Bottom button |
| Cancel or go back | F | Right button |
| Run while moving | Hold Shift | Hold the right button |
| Jump | Space | Top button |
| Open the game's menu | Tab | Left button |
| Turn the camera | Left/right arrow | L1/R1 or right stick left/right |
| Pause or resume | Enter | Start |
| Select | Backspace | Back |
| Original L2 / R2 actions | 1 / 2 | LT / RT |
| Open app Settings | Esc | Back + Start |

The game decides when jumping, pausing or opening a menu is available.
Follow its prompts during special activities. On the world map, pressing
L2 and R2 together boards or leaves a Gear where the game allows it.

When entering a battle attack sequence, the controls change:

| Attack | Keyboard | Controller |
| --- | --- | --- |
| Weak | W | Top button |
| Strong | A | Left button |
| Fierce | S | Bottom button |
| End the sequence | D | Right button |

### Using the mouse

Click the ground to walk there, or click a person to approach and talk.
Choose menu entries and battle targets by clicking them. Left-click also
confirms; right-click cancels. You still make your own jumps. Taking over
with a movement key or stick cancels an automatic walking route.

The mouse wheel pulls the camera back or brings it closer: scroll down to
zoom out and up to zoom in. This works with the Ex Machina picture and is
most useful while exploring. The world map gets a smaller zoom change.

In **Settings → Controls**, turn on **Turn the camera with the mouse** to
use mouse movement for looking instead. The app takes hold of the pointer
while you play; open Settings to use it normally again. **Mouse sensitivity**
changes how quickly it turns. **Mouse deadzone** lets you move the mouse a
little before the ordinary field camera starts turning.

**Hide the cursor when the mouse is idle** keeps the pointer out of the
picture after a few seconds. Move the mouse to bring it back.

### Handy app shortcuts

For a controller combination, hold Back and press the other button.

| Feature | Keyboard | Controller |
| --- | --- | --- |
| Save state | F5 | Back + bottom button |
| Load state | F8 | Back + top button |
| Rewind | Hold R | Hold Back + LT |
| Turn rewind history on/off | F7 | Back + right button |
| Turn fast-forward on/off | Backtick (`` ` ``) | Back + RT |
| Turn automatic dialogue advance on/off | T | Back + left button |
| Toggle first-person view | V | Back + R1 |

## Touch controls

Use the on-screen D-pad and labelled buttons just as you would a controller.
Keep a finger down to hold a button, and slide across the D-pad to change
direction. The camera buttons, Start and Settings sit near the top.

You can also tap the ground to walk, tap a person to approach and talk, or
tap an entry or target to choose it. Tap dialogue to advance it. **LOOK**
switches first-person view on and off; drag on the picture to look around.
**AUTO** switches automatic dialogue advance on and off during field scenes.

The controls change for the activity you're playing. Battling gets its own
attack, jump, dash, charge and special buttons. The world map adds **Board**.
Shops add **+**, **−**, **ALL** and **NONE** for quantities.

In **Settings → Controls**, **Touch controls** shows or hides the touch
buttons. **Stop at ledges** helps automatic walking stop before a jump;
you still press Jump yourself. **Reduce transparency** makes the added touch
and headset controls more solid, and **Reduce motion** tones down their
animation. These options affect the added controls, not the game's artwork.

## Changing your controls

Open **Settings → Controls → Game controls**, choose an action, and edit
its keyboard, controller or mouse entries. Choose **Save** when you're done.
**Clear** removes that action's bindings; **Restore defaults** restores the
game controls. The four attack actions have their own entries.

With a controller, move to the **Gamepad** box and press the bottom button.
The box says **Press buttons…**: press the button you want and let go (press
several together to make each of them an alternative), then move down to
**Save**. On a keyboard, type the input names into the boxes. For example:

| Type this | To use this input |
| --- | --- |
| `KeyJ` | The J key |
| `Space` | Space bar |
| `ShiftLeft` | Left Shift |
| `ArrowUp` | Up arrow |
| `South`, `East`, `West`, `North` | Bottom, right, left, top controller button |
| `L1`, `R1`, `LT`, `RT` | Shoulder buttons and triggers |
| `LStickUp` or `RStickLeft` | A stick direction |
| `MouseLeft`, `MouseRight`, `MouseMiddle` | A mouse button |

Separate alternatives with a space or comma: `KeyE Backslash` lets either
key confirm. If an input is already assigned to another action, clear that
assignment first. The app explains conflicts beneath Settings.

Open **Settings → Shortcuts** to change app shortcuts instead. Here a plus
sign means “hold together”: `ControlLeft+KeyS` means left Ctrl and S;
`Back+South` means Back and the bottom controller button. With a
controller, press the bottom button on the **Buttons** box, then hold the
buttons of the new shortcut together and let go; they fill the box (while it
waits, your buttons start no shortcut). Choose **Save shortcut** to keep the
change, **Clear** to remove it, or **Cancel** to leave the edit.

You can also assign shortcuts to features that start without one: backup
imports and exports, retrying card writes, changing rewind length or
fast-forward speed, showing the frame counter, pausing when you switch away,
Reset and headset recentering. Esc and Back + Start remain the way into
Settings.

**Run by default**, under Controls, swaps walking and running while you
move. Hold the usual Run button when you want to walk.

**Invert LT/RT chords** makes LT alone rewind and RT alone toggle
fast-forward. With this on, hold Back with a trigger to use the game's
ordinary L2 or R2 action.

## Saving, loading and backups

There are two ways to save. Use both if you like, but keep ordinary game
saves for progress you want to keep through updates.

### Ordinary game saves

Save through Xenogears' own save menu wherever the game allows it. The app
provides two memory cards. On a computer they live in the `cards` folder
beside the app. Load an ordinary save through the game's own loading screen.

### Save states

A save state remembers your exact moment, including a battle or conversation.
Press **F5** or choose **Settings → Saves → Save state**. Press **F8** or
choose **Load state** to return to it.

There is one quick-save-state slot. Saving again replaces it. Watch for
the success message before closing the app. To keep several moments, use
**Export state backup** and give each file a different name.

Loading a state replaces your current moment. It keeps your current app
settings and does not roll your memory-card saves back. Rewind follows the
same rule, so neither is a way to undo an overwritten memory-card save.

Save states can move between supported devices using a compatible version
of the app and the same supported game. Keep the needed disc available.
If a state comes from an incompatible version, the app explains why it
cannot load it. Use an ordinary memory-card save to continue after an update.

### Backing up or bringing an old save

Open **Settings → Saves → Backups**:

| Button | What it does |
| --- | --- |
| Export state backup | Saves your current moment to a file you choose. |
| Import state backup | Loads a saved moment from a file. Start the game first. |
| Export original card 1 / 2 | Copies that whole memory card to a standard `.mcd` file. |
| Import original card 1 / 2 | Replaces that card with the card or save file you choose. |
| Export settings | Saves your preferences and controls to a backup file. |
| Import settings | Replaces your preferences and controls with a backup. |
| Retry pending card writes | Tries again after a memory-card save could not be written. |
| Import BIOS (save titles) | Uses your own console BIOS's lettering for save titles. |

Export a card before replacing it. Importing a single save creates a card
containing only that save; it does not add it to the existing card. To use
an imported save, open the game's loading screen and choose the card you
imported into.

Supported imports include ordinary PlayStation memory-card files, DexDrive
(`.gme`), Connectix VGS (`.vgs`), PSP or PS Vita (`.vmp`), PS3 PlayStation
saves (`.psv`), single saves (`.mcs`, `.ps1`), and Action Replay, GameShark,
Xploder or Caetla saves (`.psx`, `.mcb`, `.mcx`, `.pda`). Raw single saves
also work when they retain their original game filename. Exports use `.mcd`
for cards and `.xemstate` for states.

The optional BIOS import is only needed for the original save-title font.
Without it, the app uses substitute letters and numbers; some characters
may be blank.

Backups are files you move yourself. There is no automatic cloud sync. You
can take compatible saves to another computer, Android device, headset or
the browser version. Leave immersive headset play to use file pickers.

## Rewind, fast-forward and pause

### Take back a mistake

Hold **R** or **Back + LT** to rewind. Let go when you reach the moment you
want, then carry on. Touch and headset players can hold **Settings → Time →
Hold to rewind**.

Rewind history is on by default. It remembers roughly the last 30 seconds
of play, taking a checkpoint every second. Under **Time**, change
**Interval** for smaller or larger steps and **History length** for how far
back you want to go. Longer history uses more memory; the status line shows
what is currently available. History has to build up as you play and is
temporary, so make a save before quitting.

The seconds mean time spent playing, even during fast-forward. Pausing does
not use up the history. **F7** switches history recording on or off.

### Speed up travel

Press **backtick** or **Back + RT** once to start fast-forward and again to
stop. You don't have to hold it. **Settings → Time** has the same Start/Stop
button.

Choose **1×, 5×, 10×, 15×, 20×** or **Uncapped** with the Speed slider. The
starting rate is 10×; Uncapped goes as fast as your device can manage. Choosing
a speed alone doesn't start fast-forward. The music keeps its normal tempo
and pitch. Some scenes still wait for their music, so they may not speed up
as much as travel does.

### Step away

Press **Enter/Start** to pause or resume where the game permits it. You can
also click or tap the paused game to resume.

**Settings → Time → Pause when the window loses focus** is on by default.
When you switch to another window, the app asks the game to pause. Some
scenes and transitions cannot pause immediately. Coming back leaves a paused
game paused until you resume it.

## Gameplay options

Find these in **Settings → Gameplay**. Each choice is saved for next time.

- **Random battles — Not while platforming:** the default. Turns off random
  encounters in the selected jumping areas of Blackmoon Forest, the
  Aveh–Kislev border mountains and Babel Tower. Choose **Original** for the
  game's usual behavior. Story battles still happen.
- **Encounter rate — Half:** halves random encounters in fields and doubles
  the EXP, G and deathblow progress battles award. Choose **Original** for
  the normal rate and rewards.
- **Text speed:** choose **Original**, **Fast** or **Instant**. Faster text
  still respects pauses and timing a scene needs.
- **Auto-advance dialogue:** turns field dialogue pages for you after they
  finish appearing. **Wait on each page** sets the reading time, from half
  a second to ten seconds. It starts at two seconds. Choices still wait for
  your answer. Use **T**, **Back + left button**, or the touch/headset
  **AUTO** button to toggle it while playing.
- **Movies can be skipped:** lets Confirm end a movie early; title movies
  also accept Start. It starts off. Small movies playing inside a scene
  may still follow that scene's timing. This skips movies, not entire story
  scenes.

To see the scene behind a dialogue box, **hold Select**: Backspace, the
controller's Back button, or touch **SELECT**. Release it to bring the text
back. Hiding the box does not pause the scene or stop automatic dialogue
advance.

## Making the picture your own

Open **Settings → Graphics**. You can change the look while playing.

### Picture and profiles

**Renderer** chooses how the picture is drawn:

- **Ex Machina** offers the new graphics choices below. It starts with the
  Modern profile.
- **Original** shows the console-style picture. Choose this when you want
  the original presentation; the extra graphics controls are hidden.

With Ex Machina selected, choose a **Profile**:

| Profile | What to expect |
| --- | --- |
| Modern | A sharper picture with steadier shapes and textures. |
| HD-2D | Lighting, shadows and soft atmospheric effects over the original art. |
| PS1 | Low resolution and the original visual quirks. |
| Custom | Your own mixture of effects and original quirks. |

These use the game's own artwork. You don't need to find a replacement
texture pack.

**Resolution** controls how much detail the app draws. Start with **Auto**,
or choose 1× through 8×. Higher values give a sharper picture but ask more
of your device. The PS1 profile uses the original resolution.

**HD-2D quality** changes how finely the effects are drawn. Try **Auto**
first; lower it if play feels slow. **Full-resolution sprites in large-scale
areas** uses the party's more detailed character artwork in areas where the
original game uses smaller sprites. It starts off.

### Size, motion and sharpness

| Setting | What it changes |
| --- | --- |
| Fullscreen | Fills your desktop display. Turn it off to play in a window. |
| Scale: Fit | Fits the picture while keeping its proportions. |
| Scale: Integer | Enlarges pixels in whole steps, which may leave larger borders. |
| Scale: Stretch | Fills the available space, even if it changes the proportions. |
| Filter: Nearest / Linear | Keeps the final picture crisp or softens its edges. |
| High Framerate | Smooths movement between the game's frames. It starts off and can add a little input delay. |
| Widescreen | Shows more scenery at the sides to suit a wider screen. |
| Zoom out | Pulls the exploring camera back, from 1× to 5×. The mouse wheel changes this too. |
| Show FPS | Shows a small counter of how often the picture updates. |

Under **Texture Filtering**, set **World**, **Sprites** and **Interface**
separately. **Nearest** keeps visible pixel edges; **Linear** blends them
for a softer look. For **Movies**, **Enhanced** smooths block edges and
sharpens the enlarged picture; **Original** keeps the original movie image.

### HD-2D and Custom effects

Move each slider to taste. Zero turns that effect down to nothing.

| Effect | What you see |
| --- | --- |
| Lighting | More light and shade across the scene. |
| Shadows | Added shadowing. |
| Ambient occlusion | Darker corners and places where surfaces meet. |
| Fog | Haze over distant scenery. |
| Light shafts | Streaks of light through the scene. |
| Bloom | A glow around bright areas. |
| Colour grading | A change to the scene's overall colours and tone. |
| Depth of field | Softens scenery away from the focus point. |
| Focus | Moves that focus nearer with negative values, farther with positive values. Zero keeps the usual focus. |

HD-2D and Custom remember their own effect levels. **Reset Defaults** beside
the profile's effects restores that profile's usual levels.

Custom also offers **Original limits**:

- **Affine textures:** brings back the original texture stretching and wobble.
- **Vertex snapping:** brings back the small steps in the edges of moving shapes.
- **15-bit colour:** uses the original, more limited range of colours.
- **Dithering:** uses little pixel patterns to blend shades.
- **Native resolution:** draws at the original low resolution.
- **Painter's ordering:** keeps the game's original way of drawing overlapping
  objects. Turning it off changes how their depth is handled and can change
  which surfaces appear in front.

## Looking around in first person

While in a field, press **V**, **Back + R1**, click the controller's right
stick, or tap **LOOK**. This lets you look from your character's position.
Use the arrow keys, right stick, touch drag, or mouse with mouse camera
enabled to look around. In a headset, simply look with your head.

Press the same toggle again to return. Your character stays put while you
inspect the view; turn it off before walking or jumping. A walking route
you had started is cancelled. Story scenes can still carry on.

While walking, Select can also toggle the view where the game doesn't use
it to show a map picture. Use V or LOOK if Select has another job there.
First-person viewing needs the **Ex Machina** renderer and isn't available
inside the game's menu. Older scenery was built for particular camera
angles, so some new viewpoints reveal flat sprites or missing surfaces.

## Sound

Open **Settings → Audio**. **Master volume** changes the whole game's volume.

| Sound mode | What it sounds like |
| --- | --- |
| Mono | The same mix from both sides. |
| Stereo | The original left-and-right sound. |
| Wide | The game's original wider sound setting. |
| Binaural | Sound arranged around you for headphones. |

In **Binaural**, **Spatial music** spreads the musical parts around you.
**Music immersion** adjusts how far that arrangement reaches around you;
lower it for a more forward-facing sound. **Positional effects** places
world sounds where they happen. You can turn music and effects placement
on or off separately.

In a headset, **VR: Stationary music** keeps the musical arrangement in
place as you turn your head. Leave it off if you prefer the music to turn
with you. Choosing Mono, Stereo or Wide returns to the game's original
sound modes. Changing the sound mode in the game's own menu also leaves
Binaural mode.

## Playing in a headset

You can play on a virtual screen, view the world as a tabletop miniature,
or look around from your character's position. Headset features depend on
what your device supports.

On a computer, native VR starts with the app's **`--xr` launch option**.
For a Steam shortcut, put `--xr` in its Launch Options. Your headset's
OpenXR software must be set up first. The Graphics screen chooses how an
active headset session looks; it does not start VR by itself. The standalone
APK can also run as a flat panel; immersive startup currently needs a
launcher that supplies that same option. See the [headset setup notes](docs/runtime.md#openxr-adapter)
for this extra setup.

### Screen or diorama

In **Settings → Graphics → Headset**, choose **Screen** for a large virtual
screen. Change its **Distance** and **Width** until it is comfortable to see.

Choose **Diorama** to put the 3D scene on a table in front of you:

- **Diorama scale** makes the miniature smaller or larger, from 25% to 400%.
- **Table height** places it higher or lower relative to your head.
- **Table distance** moves it closer or farther away.
- **Table tilt** tips the view toward you.
- **Table turns** chooses smooth turning, turning in steps, or a fixed direction.
- **Passthrough**, where offered, shows your room around the diorama.

The game uses the virtual screen for scenes that don't have a suitable
3D view. Its screen distance and width still matter in Diorama mode.
The diorama does not draw the HD-2D effects; the screen keeps them. Sprites
remain flat artwork even when you look at them in 3D.

**Recenter** places the view in front of where you're sitting and facing
now. **Reset diorama** restores the table's size, placement and turns.
Headset **Depth of field** is separate from the ordinary picture's setting
and starts off.

### Headset controllers

The left stick moves. A/B/X/Y follow the bottom/right/left/top buttons in
the [controller table](#playing-with-a-keyboard-mouse-or-controller). The
triggers act as L1/R1, and the grips as LT/RT. Clicking the left stick is
Start; clicking the right stick is Back.

Use the left menu button, or click both sticks together, for Settings.
Point a controller at a setting and squeeze its trigger to select it.

Hold the right stick down and press A to save a state, Y to load, the left
grip to rewind, or the right grip to toggle fast-forward. Hold that stick
down and squeeze the right trigger to toggle first-person view. In that
view, turn your head to look and use the right stick for stepped turns.

### Playing with your hands

Enable **Hand gestures** in **Settings → Controls**. Choose where the
**Pointer** comes from: your head, eye gaze, left hand or right hand. Head
pointing is the starting choice. Eye gaze and hand pointing need the
matching headset support; if your chosen pointer isn't available, choose
one your device provides.

Point and pinch to use the buttons beside the game or select a setting.
Where the headset provides pinch-only pointing controls, pinch the ground
to walk, a person to approach and talk, or a dialogue to continue it.
The buttons beside the view provide movement, Jump, Menu and Back too.

The app also recognizes these whole-hand gestures when hand tracking is
available:

| Gesture | Action |
| --- | --- |
| Close an open hand into a fist with the palm facing down | Pause or resume. |
| Close an open hand into a fist with the palm facing up | Toggle first-person view in a field. |
| Bring open palms together like prayer hands | Open or close Settings. |
| Gather all fingertips on both hands, then move the hands apart or together | Enlarge or shrink the diorama; on the virtual screen, resize it for this session. Release a hand to finish. |

Either hand can perform the one-hand gestures. Open it again before doing
another grab. Looking around still works while paused. Losing tracking
stops walking and pauses play; once tracking returns, use the pause gesture
to resume. Closing Settings keeps a pause you had already set.

Some headsets also recognize thumb taps and swipes. Where available:

- While walking, pinch to set a destination; tap over a person to talk or
  over the ground to stop. Swipe up to jump, left/right to turn the camera,
  and down to open the menu.
- In dialogue, tap to advance. Look at a choice and make a fresh tap to
  answer it.
- In menus, tap the entry you're pointing at to choose it. Up/down swipes
  step through a list; a right swipe goes back. On character screens, point
  away from the list to change characters instead.
- In a battle command ring, swipe toward a command to choose it; tap to
  change the ring's page. During an attack sequence, up is Weak, left is
  Strong, down is Fierce, and right ends it.
- For targets, point and tap to choose, or use the separate ally/enemy
  list when characters overlap.

Battling, world-map travel and special field activities have their own
control decks beside the view. Lit buttons show held actions. With thumb
controls, a pinch takes hold of the deck's movement stick; move your hand
to steer and pinch again to let go. This lets one hand steer while its
thumb works the action controls. Use the deck's labelled buttons for each
activity, including **Board** on the world map. Shops offer quantity buttons.

## The Debug menu

**Settings → Debug** lets curious players try the game's original test
setups. These can reveal places, characters and movies from later in the
story. **Starting a test restarts the game with that setup, so save first.**

- **Field:** enter a map and entrance number, then start the field test.
  **Go to developer room** opens the game's developer room.
- **Battle:** choose a formation and battle file, pick up to three party
  members and their Gear options, then start the battle test.
- **Worldmap:** starts the world-map test.
- **Battling:** starts the Battling activity.
- **Menu:** opens a selected game screen, such as party changes, loading,
  naming, shops or disc changing. The number beneath selects the character,
  Gear, shop or disc for that screen.
- **Movie:** choose a movie number and whether to play its picture, sound
  or both.

The numbers refer to the game's own test setups. Some need particular
content or a particular disc, and a test setup isn't the same as reaching
that place through the story. Load your saved game or state to return to
your adventure.

## Updates and help

On a computer, back up your game folder before updating. Replace the app
and its included files, but keep **`discs`**, **`cards`**, **`saves`** and
**`settings.json`**, plus your imported BIOS if you use one. If you play
through Steam, update its shortcut when the AppImage's filename changes.

On Android or Horizon OS, install the new version over the existing app.
Export your cards first. Keep ordinary game saves as well as states, since
a state may require the version that created it.

| If this happens… | Try this |
| --- | --- |
| The game asks for a disc | Keep both supported images available, or use Saves → Import disc… to select the requested disc. |
| A save state won't load | Read the message. Check the app version and the required disc, or load an ordinary memory-card save. |
| A card save could not be written | Check free space and that the game folder is writable, then choose Retry pending card writes. Keep the app open until the write succeeds. |
| The picture feels slow | Lower Resolution or HD-2D quality, or try the Modern profile. |
| The background is too blurry | Lower Depth of field or adjust Focus. |
| You can't move after looking around | Turn first-person view off with V, LOOK or its controller shortcut. |
| The mouse pointer has disappeared | Move it, or open Settings. Check the mouse camera and idle-cursor options. |
| Headset pointing does nothing | Choose a pointer your headset supports and check that tracking is available. |
| A backup button won't open a file picker in VR | Return to flat/windowed play to import or export files. |
| The app won't open | Check the platform notes in the installation guide, including Linux permissions and macOS opening restrictions. |

An error bar at the bottom of Settings explains a failed action. **Dismiss**
hides the message; it doesn't fix the underlying problem. If you need to
report an issue, include the version from **About**, your device, what you
were doing and the exact message. You can find help and report problems
on the [project's GitHub page](https://github.com/brianblakely/xenogears-ex-machina).
