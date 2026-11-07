# Changes

What changed for players, newest first. The details and the tests behind each item are in the documents linked.

## Next (after the beta candidate)

### New

- **Online play at 60 frames a second, and a round of polish** (README "Playing online"): the host's game frame rate
  is the session's, now proved to give one game at 60 as at 30 (and checked on Windows against Linux in CI); F1 >
  Online shows it to both players. The page also has the session's rules as rows, locked while a session runs: the
  rejoin window (`net.rejoin`), desync recovery (`net.recover`) and a rumble pak (`net.rumble`, off: with it on, your
  controller holds a rumble pak in an online race instead of your controller pak, as on an N64, and rumbles; only
  yours). The netplay folder keeps the newest 20 sessions (`net.keep`; F1 > Online > Watch a replay's last row; kept
  replays are never removed). A friend's ghost imported into your saves gets the game's own names on an online
  session's pak, as it does offline. A guest started from F1 > Online that goes out of sync now rebuilds its game, as
  one started from the launcher does, instead of going back to the game ([NETPLAY.md](docs/port/NETPLAY.md) 17).
  Nothing changes without the online options.

- **Online play from the game itself** (F1 > Online, README "Playing online"): Host a race (a port, a join code), Join
  a race (type the host's address, or pick one of the last 8 hosts you joined), Watch a replay, and this PC's
  addresses with Tailscale's first, copied to the clipboard for your friend with one key. Choosing Host, Join or Watch
  restarts the program into it with your options (the launchers still work); when a session you started there ends,
  the game comes back without it. In a session the page shows the other PC and the link, and Leave the session. An
  address that isn't one is said at once, in a sentence. Nothing changes until you choose one of them
  ([NETPLAY.md](docs/port/NETPLAY.md) 13).

- **Replays of online sessions**: each session's input log in `netplay/` now replays on its own, `--replay-inputs
  FILE` with nothing else (it takes the session's settings and both players' paks, which it no longer writes to), or
  from F1 > Online > Watch a replay, in real time; Keep this replay copies one into `replays/`. A replay's state is the
  session's at every retrace ([NETPLAY.md](docs/port/NETPLAY.md) 13.3).

- **Your own view online** (`net.view`, F1 > Video > Online view; README "Playing online"): with `own`, each player's
  PC shows only that player's view of an online race, filling the window as a one-player race does, with that
  player's speedometer, place, laps and coins at the screen's edges and the map and race clock at the right; the
  other player's view, the side panel and the dividers aren't drawn. Each player chooses their own, and the two games
  stay one (it changes only what is drawn). `split`, the default, keeps the game's split screen
  ([NETPLAY.md](docs/port/NETPLAY.md) 14).
- **Online play survives a lost connection, a crash, and going out of sync** (README "Playing online"): a session
  waits up to 5 minutes for a player that went silent (`net.rejoin`, 300 s; was 15 s), saying how long; a connection
  that comes back goes on where it was. A player whose game is gone (a crash, the window closed, the PC restarted)
  starts "Host" or "Join online race" again: the game finds its session, gets the race so far from the other PC,
  replays it unpaced with nothing drawn (about 100 times real time on the synthetic races), every retrace checked
  against the other PC, and plays on; both screens show the progress. When the two games go out of sync the guest's
  game starts again and rebuilds the race from the host's the same way (`net.recover`, on); a difference that
  repeats there stops the session with its report, as before. Escape still leaves for good
  ([NETPLAY.md](docs/port/NETPLAY.md) 15, [NETWORK.md](docs/port/NETWORK.md) 12). Nothing changes without the
  online options.

- **Online play from the desktop** (README "Playing online"): `Host online race.bat` and `Join online race.bat`
  (`host-online.sh` and `join-online.sh` on Linux) start a session with no options to type: the join one asks for the
  host's address. The host's window opens at once with a waiting screen (the port, this PC's addresses, Tailscale's
  100.x one and its MagicDNS name first; Esc cancels) instead of waiting with no window, and the guest says whom it
  joined as soon as its window opens. Whatever stops a session before the race says why in a box (the address isn't
  found, nobody answers, another version, ROM or mods; nobody joined in 10 minutes). In a session F1 shows the other
  PC, the round trip, the input delay, the stalls and the last hash check (on its Online page since the entry above)
  ([NETPLAY.md](docs/port/NETPLAY.md) 12.10).

- **Online play, a preview** (`--net-host`, `--net-join HOST`; README "Playing online"): two PCs race in the game's
  two-player split screen over the internet (Tailscale, or a forwarded UDP port 24049), the host as player 1 and the
  guest as player 2, the guest taking the host's game settings for the session and each bringing a copy of their own
  controller pak. Both games check, at every retrace, that they are the same; a ROM, build or mods that differ are
  refused at the start, saying what differs. The game runs at its own speed on the paced clock, with sound from your
  own sound card; F1 and the pause key don't pause it ([NETPLAY.md](docs/port/NETPLAY.md) "Status: the session").
  Nothing changes without the options.

- **Online play's clock** (groundwork, not a setting): `--paced` runs the game on `--fast`'s virtual clock, which
  takes the same course on every PC from the same inputs, at the pace of real time: each move of the clock waits for
  its wall time. A paced run is `--fast`'s to the bit on every synthetic path, at 60.000 retraces a second; a host too
  slow for it slows the game in wall time and never changes its course. Vsync, frames between and the window behave
  as in real time; the sound card is fed from a copy of the game's sound, so its timing never reaches the game
  ([ULTRA.md](docs/port/ULTRA.md) 10, [AUDIO.md](docs/port/AUDIO.md) 11, [NETPLAY.md](docs/port/NETPLAY.md)).

- **Online play, the network part**: `port/net`, the library
  two PCs will race through, in lockstep over UDP, with an input delay that follows the connection, time sync, a check
  every retrace that both games are the same, and a join code. Tested on its own against a stand-in game, on
  simulated networks from a LAN to 150 ms with 10% loss, a latency that changes mid-race and a peer that stops for
  3 s: both sides the same at every tick ([NETWORK.md](docs/port/NETWORK.md), the plan [NETPLAY.md](docs/port/NETPLAY.md)).

- **Input recording and replay, and the state hash** (online play's groundwork, [LOCKSTEP.md](docs/port/LOCKSTEP.md)):
  `--record-inputs FILE` keeps each retrace's pads, `--replay-inputs FILE` plays them back (a replay of another
  build, cartridge or settings is refused, saying what differs), `--input-delay D` plays your input D retraces late,
  and `--log-hash N` prints a hash of the game's state. With `--fast` a replay is the recorded game, retrace for
  retrace. A new profile's id in its save no longer depends on where the program was loaded in memory. None of this
  changes a game run without the options.

- **Fill the window** (`video.fill`, F1 > Video > Fill the window): `on` makes the picture reach the window's top and
  bottom. The game shows 220 rows, and the port showed them in a TV's 240-line frame, with a band above and below
  (66 pixels each at 2880 × 1620). With widescreen the picture is then the window's shape, the 3D a little less wide
  than before (392 N64 pixels across at 16:9, where it was 426); with side art (the HD menu mod) the main menu fills
  the screen too. `off`, the default, is as before ([GFX.md](docs/port/GFX.md) "Fill the window").

- **The cheat screen in the main menu, and every cheat on it** (`game.cheats_menu`, `game.cheats_unlock`, F1 > Game >
  Cheats menu, Unlock all cheats): the game's own CHEATS option shows in the main menu without its button code, and
  on the cheat screen every cheat can be switched without its own code (the cartridge makes a cheat available only
  once its code is entered there, for the session). Both `off`, the default, are the N64's
  ([FRONTEND.md](docs/port/FRONTEND.md) "The cheat screen in the main menu").

- **Ghost save prompt for every profile** (`game.ghost_prompt`, F1 > Game > Ghost save prompt): after a finished
  ghost race the game offers to save the ghost with its own save prompt, which the cartridge shows only to its
  developer's profile name (any other profile's ghosts were kept only until the game closed, so `saves = files` made no
  ghost files from play). With `saves = files` the saved ghost is a `.r49ghost` in the saves folder. `off`, the
  default, is the N64's ([SAVES.md](docs/port/SAVES.md) 7.1).

- **PC SETUP in the main menu** (`game.pc_setup`, F1 > Game > PC SETUP in menu): `off`, the default, is the N64's
  main menu. `on` adds a sixth option below VIDEO, drawn as the game draws its own, that opens the settings overlay
  over the menu, as F1 does. And a fix for the overlay: the START or B that closed it no longer also reaches the game
  (on the main menu START started a race) ([FRONTEND.md](docs/port/FRONTEND.md) "PC SETUP in the main menu").

- **Side art: menus that fill a wide screen** (texture packs): a pack's replacement for a background tile can carry
  columns past the 4:3 frame (`<name>+l<N>.png`, `+r<M>`, `+l<N>r<M>`); in widescreen the tiles at the frame's sides
  then reach into the side bands instead of leaving them black, and the menu's own scrolling glow beside them goes
  on under them. Off widescreen, and without such files, nothing changes
  ([TEXTURES.md](docs/port/TEXTURES.md) "Side art").

- **Smooth keyboard steering** (`input.keyboard_steering`, `input.keyboard_steer_time`, F1 > Keyboard > Steering):
  `instant`, the default, is the keys' full lock the moment one goes down, as before; at speed half a second of it
  puts the car into the barrier. `smooth` moves the stick from the centre to full tilt over the steering time (250 ms
  by default, 0-1000) and back to the centre in a third of it when the key is released or the other way is pressed,
  so a tap steers a little and a held key a lot. For keys and controller buttons bound to the stick, both axes, the
  same at every frame rate; a controller's stick is not changed. At once
  ([FRONTEND.md](docs/port/FRONTEND.md) "The stick").

- **Presets** (F1 > Presets, `--preset enhanced`): the recommended PC settings in one step. **PC enhanced** sets the
  game at 60, draw distance far, detail high, far textures anisotropic, widescreen auto, 64 voices with every drone's
  engine heard, and 64 MiB of memory; **PC maximum** also draws the whole track without fog and upscales the world's
  textures; **N64 original** puts all of those back to the defaults. Choosing one is the same as changing each
  setting on its page: what applies at once does, the voices and the memory are marked for a restart, and Save writes
  them. The cars a race holds, the display's settings and the player's own (the HUD's place, the mods, the saves, the
  controls) are left as they are. `--preset NAME` does the same for one run, without writing the file
  ([FRONTEND.md](docs/port/FRONTEND.md) "Presets").

- **Far textures** (`video.texture_lod`, F1 > Video > Far textures): `original`, the default, picks each texture's
  smaller copies where the N64 does, for its 320 x 240 screen, so at a PC's resolution the road and the walls a few
  car lengths ahead were drawn from a copy a quarter or a sixteenth of the size. `sharp` picks them for the window's
  own pixels; `anisotropic` also keeps the far road, seen edge on, sharp along its length. From the next frame
  ([DETAIL.md](docs/port/DETAIL.md) 2.7).

- **Every engine heard in a race of 12** (`audio.voices`, `audio.engines`, F1 > Audio): `original`, the defaults, are
  the N64's sound exactly: 32 voices, of which the effects get 16, and a sound manager that plays the 12 loudest sounds
  at once, so in a race of 12 only the player's engine and the nearest four or five drones' are heard. `voices = 48, 64
  or 96` (from the next start) gives the effects all but the music's 16, plays three quarters of those at once and has
  room for 128 positional sounds: at 64 every drone's engine in earshot is heard with the effects. `engines = nearest2
  ... nearest8` (at once) keeps the N nearest drones' engines to each player's view, fading in and out, with the others
  silent; `all` plays every drone's engine whatever the effects (it wants 64 voices: at `original` it leaves effects
  unplayed). In a race of 12 at 64 the mixer takes 1% of one of a Raspberry Pi 5's cores (0.9% at `original`) and the
  heap 470 KB more. Fixed with it, at every setting: on a machine busy enough to hold the sound manager back a retrace,
  a sound could be started twice and its first voice play on to the end of the race (with more voices nothing took it
  back) ([AUDIO.md](docs/port/AUDIO.md) 10, [FRONTEND.md](docs/port/FRONTEND.md)).

- **Saves as files, and ghosts to share** (`game.saves = files`, `--saves files`, F1 > Saves): instead of one
  32 KB controller pak image per player, each profile and each ghost is a file of its own in the player's folder
  (`saves/p1` in the data folder), and the folder keeps any number of ghosts; the game still sees a controller pak,
  with the profiles, the ghosts it saved and the fastest of the rest that fit. The first start with files imports
  the player's `.mpk` (left as it is); F1 > Saves > Export to the .mpk goes back. A ghost exports to a
  `.r49ghost` file, and one dropped into the folder's `import/` (a friend's) is checked, put on the pak and raced
  like the player's own. Deleted saves go to the folder's `trash/`, never removed; each file is written atomically
  and keeps three backups. `pak`, the default, is the `.mpk` exactly as before ([SAVES.md](docs/port/SAVES.md),
  [FRONTEND.md](docs/port/FRONTEND.md); `tools/port/ghost.py` reads and converts all of it).

- **Mods** (a `mods/` folder in the data folder, F1 > Mods): a mod is a folder or a .zip with a mod.ini and files
  that stand in for the cartridge's (cars, tracks, fonts, the HUD's and the menus' models and textures, text, sounds),
  named by the game's file names (`cars/car01.bin`) or numbers (`wave_088.bin`), and optionally a texture pack and
  text overrides. Your ROM is never changed. Every file is checked at start; one the game can't use is refused with a
  line in the log and the cartridge's is used, and of two mods for one file the first in the order wins. Larger files
  get the memory they need (game.memory is raised, and the log says so). F1 > Mods turns mods on and off and orders
  them, from the next start; `--no-mods` or `game.mods = off` plays the cartridge's files only. Making mods:
  `tools/mods/rushmods.py` lists your ROM's files, unpacks them as mods use them, makes a mod's skeleton and checks a
  mod by the game's rules ([MODS.md](docs/port/MODS.md), [FRONTEND.md](docs/port/FRONTEND.md)).

- **The game at 60 frames a second** (`game.frame_rate`, F1 > Game > Game frame rate): `original`, the default, is
  the N64's 30. At 60 the game draws a frame every retrace, its own, not one drawn between two: the menus, the HUD,
  the cameras, the smoke and sparks, the battle weapons and every timer step 60 times a second at the N64's speed, and
  the car physics is the same as before, every retrace. A race the game doesn't steer once a frame (no drones or
  shots, its resurrections at the same moment) has the same physics to the bit at either setting; the drones'
  catch-up, a resurrection's place and moment (a car stuck 6 s) and the shots are decided once a frame, so at 60 they
  are decided twice as often. The race's loading screen counts the N64's frames, so the countdown starts within a
  retrace of original's (it was 5 sooner). A host that can't keep 60 in a race is told so in the log and on F1 >
  About (the game keeps its speed; its frames are then partly the N64's 30 Hz ones). With vsync on, a 60 Hz display
  no longer halves it: each frame's swap at its retrace could wait a whole refresh, and at some phases the game drew
  30 frames a second, a camera that looked jerky in a pan; its frames are now paced by the display's vblanks. On a
  144 Hz display `video.frame_rate = display` is the smooth choice. Applies from the next screen.
  `video.frame_rate` still draws frames between the game's on top ([FRAMERATE.md](docs/port/FRAMERATE.md) 7,
  [FRONTEND.md](docs/port/FRONTEND.md)).

- **Draw distance, detail and fog** (`video.draw_distance`, `video.detail`, `video.fog`, F1 > Video): at `original`,
  the defaults, the N64's picture exactly. `draw_distance = far` draws everything the game limits by distance twice
  as far: more of the track (the world's visibility tables reach 2,000 ft further), the cars and their wheels twice
  as far out, the scenery past 2,048 ft, the far plane at 4,000 ft; `max` draws the whole track and every
  model at any distance. `detail = high` keeps each model at its most detailed level wherever it is drawn, and gives
  split screen one player's models and tires. `fog = far` puts the fog options' fog twice as far, `off` removes it
  (with the sky in place of the fog's colour). The log's `scene:` line says what was drawn. On the retail tracks far
  draws about twice the triangles and max two to five times ([DETAIL.md](docs/port/DETAIL.md) 4,
  [FRONTEND.md](docs/port/FRONTEND.md)).

- **More memory for the game** (`game.memory`, F1 > Game > Memory, from the next start): `original`, the default,
  is the N64's 8 MB (4 without the Expansion Pak). 16 to 256 MB give the game's heap that much more room: the files
  it loads, a race's cars, and later, larger replacement files. The game runs the same with its data anywhere in it,
  pictures and sound included. The log's last lines say how much of the heap a run used at most, and a line says
  when the game found its heap short and fell back (sharing a car type, as the N64 game does); About shows it too
  ([MEMORY.md](docs/port/MEMORY.md), [FRONTEND.md](docs/port/FRONTEND.md)).

- **More cars a race, up to 12** (`game.cars`, F1 > Game > Cars in a race): `original`, the default, is the N64's
  6. Set it to 7-12, then raise the track select's Drones option, which goes up to one less than the setting (its
  default, 5, still races 6 cars). A single race takes them, for one or two players; the circuit stays 6 cars, and the
  attract demo races the setting's cars. The grid has a row for every car, and the HUD has two-digit places, the 7th
  to 12th places on the results, and a minimap dot and a radar blip for every car. Tested on every race track, to
  the finish ([MORECARS.md](docs/port/MORECARS.md) 10.4, [FRONTEND.md](docs/port/FRONTEND.md)).

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
- **The Linux aarch64 build faulted at startup** on kernels with 16 KiB pages, a Raspberry Pi 5's among them.
- **Windows' log** was written in 4 KiB bursts, and a crash's first line came before what led to it.
- **Tables only half converted** from the cartridge's byte order: the trackside objects' collision size and mass
  (read as about zero), the cars' wing positions (all but one car type had them at the centre), the race rumble's
  speed steps (rumble came out near zero) ([DATA.md](docs/port/DATA.md) 8).
- **Undefined behaviour** a sanitizer run found: a weapons table read past its end, a table declared one row
  long, a float conversion that differed on ARM, and three more ([SANITIZE.md](docs/port/SANITIZE.md)).
- **The same picture on any GPU** at native resolution: rectangles land on whole pixels and triangles are clipped
  before the GPU's own clipper, so a one-pixel seam can't depend on the driver ([GFX.md](docs/port/GFX.md)).
- **Stutter at a high frame rate with vsync on** (60, 120 or `display` with `video.vsync`, its default): most frames
  between were skipped. The frames are now paced by the display's refresh, one for each, drawn for the moment it
  appears ([FRAMERATE.md](docs/port/FRAMERATE.md) 2). The refresh is the display's own on Windows and X11 (59.94, not
  the 59 or 60 reported), so a driver that queues frames no longer holds one twice about once a second, and on an
  exact 60 Hz display the frames between no longer depend on where its refresh falls against the game's.
- **A stutter when the game saved** on a slow disk (a laptop's hard disk, a USB stick, a network folder, an
  antivirus scan): the saves are now written in the background, and every way of quitting waits for them
  ([RELEASE.md](docs/port/RELEASE.md) "Saves").

### New

- **Native resolution** by default, at any window size (2880x1620 included), Windows display scaling handled;
  borderless or exclusive fullscreen, resolution presets ([GFX.md](docs/port/GFX.md) "Resolution and scale").
- **Widescreen** by default, with the race HUD at the screen's edges (`video.hud`).
- **Upscaled textures** (F1 > Video > Textures): a small neural network trained on public-domain textures sharpens
  the 3D world's own as they first appear, in the background, and keeps them for the next run; texture packs and a
  texture dump for making them
  ([TEXTURES.md](docs/port/TEXTURES.md), [UPSCALE.md](docs/port/UPSCALE.md)).
- **High frame rate** (F1 > Video > Frame rate): 60, 120, 144 or the display's rate (any from 30 to 1000 in the
  file), drawn between the game's own 30
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
- With a high frame rate at a laptop panel's full resolution (2880x1620), an integrated GPU draws few frames
  between the game's own; Resolution 1080p in desktop fullscreen, or a whole render scale, gives it room
  ([QA.md](docs/port/QA.md) "Reported, not fixed here").
- A ghost is written to the controller pak only after the game's save prompt.

## First test builds (2026-10-27)

The retail cartridge boots and plays every mode, on Windows x86-64 and Linux (x86-64, aarch64), from the release
workflow ([RELEASE.md](docs/port/RELEASE.md)).
