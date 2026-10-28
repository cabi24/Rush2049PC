# Changes

What changed for players, newest first. The details and the tests behind each item are in the documents linked.

## Beta candidate (2026-10-28)

The first builds were played on a Windows laptop on 2026-10-27: they booted and raced, but the sound was bad, the
near road flickered, a skull showed in every race and the stick didn't steer. All of that is fixed, along with
everything a night of testing on the retail cartridge found.

### Fixed

- **Steering.** The release build ignored the stick's left/right (and the keys'): an optimizing compiler dropped
  the store of the stick's x because the game's C declared that table one element long. Twenty more declarations
  of that shape are fixed, and CI checks the class ([OPTIMIZED.md](docs/port/OPTIMIZED.md) 2.7).
- **Sound.**
  - Every sound effect played effect 0, and each start left a voice looping; effects' pitch never changed.
  - The engines were silent: their level table was read in the cartridge's byte order, and the listener's position
    was written at the N64's offsets ([AUDIO.md](docs/port/AUDIO.md) 9).
  - Crackle (the output ran dry), old sound replayed, buffers the game's own renderer skipped, Windows resampling
    on top of ours, and clipping in the conversion are gone; the output opens at the device's own rate
    ([AUDIO.md](docs/port/AUDIO.md) 3, 4, 7).
  - The synthesis matches the game's audio microcode bit for bit.
- **The road flickered** near the camera: triangles crossing the eye plane were culled in some frames. They are
  clipped as the N64's RSP does ([GFX.md](docs/port/GFX.md)).
- **Distant textures shimmered**: the game's mip levels are now used as the N64 picks them.
- **A skull showed in every race**, and a 5-minute idle on a menu crashed; the pause screen's pages didn't show or
  hide their items ([BRINGUP.md](docs/port/BRINGUP.md) 15, [LAYOUT.md](docs/port/LAYOUT.md) 9).
- **Crashes**: quitting a battle, or quitting a race and starting another; a stunt round that never ended; a rare
  hang at the start prompt ([QA.md](docs/port/QA.md)).
- **A crash mid-race** when the audio manager's message pool ran out under a burst of retraces (a loaded
  machine): the game no longer writes through the empty pool, and late retraces reach it one at a time.
- **Sound dropouts**: audio tasks no longer wait behind graphics tasks (the RSP yields, as the game expects),
  and the game's audio renderer no longer builds over a buffer set before it plays.
- **Cheats** could not be entered and the cheat screen could not be opened.
- **Stutter at a high frame rate with vsync on** (60, 120 or `display` with `video.vsync`, its default): most frames
  between were skipped. The frames are now paced by the display's refresh, one for each, drawn for the moment it
  appears ([FRAMERATE.md](docs/port/FRAMERATE.md) 2).

### New

- **Native resolution** by default, at any window size (2880x1620 included), Windows display scaling handled;
  borderless or exclusive fullscreen, resolution presets ([GFX.md](docs/port/GFX.md) "Resolution and scale").
- **Widescreen** by default, with the race HUD at the screen's edges (`video.hud`).
- **Upscaled textures** (F1 > Video > Textures): a small neural network trained on public-domain textures sharpens
  the game's own at load time; texture packs and a texture dump for making them
  ([TEXTURES.md](docs/port/TEXTURES.md), [UPSCALE.md](docs/port/UPSCALE.md)).
- **High frame rate** (F1 > Video > Frame rate): 60, 120 or the display's rate, drawn between the game's own 30
  ([FRAMERATE.md](docs/port/FRAMERATE.md)).
- **The F1 menu** takes the mouse, and has Help (every control's bindings) and About (version, graphics, sound,
  controllers, files; copy for a bug report) pages.
- **Controllers**: Xbox pads through XInput on Windows, a generic mapping for pads SDL doesn't know, sensitivity
  and dead zone, rumble; the log names each pad and its player ([FRONTEND.md](docs/port/FRONTEND.md)).
- **The Expansion Pak** is on: the sixth race track, the tracks' animated objects and the front end's song.
- **Windows**: no console window; an icon and version information; errors in a message box
  ([WINDOWS.md](docs/port/WINDOWS.md) 10).
- **Settings**: those left at their default stay commented out in `rush2049pc.ini`, so new defaults reach you.
- **Choosing the ROM**: with no usable ROM, an Open ROM... dialog picks one, says what is wrong with a wrong file,
  and remembers a good one; byte-swapped (.v64) and little-endian (.n64) dumps play
  ([RELEASE.md](docs/port/RELEASE.md) "Choosing the ROM").
- **Saves are kept safe**: the controller pak is backed up when it changes (three copies), written atomically, and
  a damaged one is set aside, not formatted over ([RELEASE.md](docs/port/RELEASE.md) "Saves").
- **An accessory for every player** (F1 > Controllers, `game.pak2`...): players 2-4 get a rumble pak when their
  pad can rumble, each player's rumble going to their own pad, or a controller pak of their own
  (`rush2049pc-p2.mpk`...), kept as player 1's; a change while playing is a swap the game sees, as on the console
  ([ULTRA.md](docs/port/ULTRA.md) "Accessories").

- **Faster rendering**: GL state is sent only when it changes, vertices stream through one buffer and each frame
  is copied once; a race uses roughly 40% less CPU ([GFX.md](docs/port/GFX.md) "What it costs").

### Known limits

- Tested on real hardware on one Windows laptop so far; the rest by scripted headless runs.
- In loud scenes the mix clips, as the game's own mixer saturates; whether the N64 sounds the same is unchecked.
- Split screen keeps its HUD centred.
- A ghost is written to the controller pak only after the game's save prompt.

## First test builds (2026-10-27)

The retail cartridge boots and plays every mode, on Windows x86-64 and Linux (x86-64, aarch64), from the release
workflow ([RELEASE.md](docs/port/RELEASE.md)).
