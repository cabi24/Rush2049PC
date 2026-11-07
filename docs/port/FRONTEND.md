# The desktop frontend: input, window and settings (port/input, port/config)

Part of Phase 6 of [PORT_PLAN.md](../PORT_PLAN.md). Written 2026-10-26. It covers the keyboard and game
controllers, the window's keys (fullscreen, screenshots, pause, quit), rumble, and `rush2049pc.ini`. All of it was
checked on SDL's offscreen video driver and SDL virtual joysticks. No real display or controller was available.
The settings overlay (Phase 8, 2026-10-27) has [its own section](#the-settings-overlay).

| File | What |
|---|---|
| `port/input/input.c` | `pc_input_poll`: live input, then `--press` scripts over it, then rumble; `--log-input`; the overlay's turn at each event pump |
| `port/input/input_sdl.c` | SDL2 builds: keyboard, controllers, hot-plug, rumble, hotkeys, focus and pause; the overlay's keys, buttons and mouse, the cursor, About's clipboard and folder (no-ops without SDL2) |
| `port/input/menu.c` | the settings overlay: pages (Help and About too), live changes, Save, the mouse's hit test, its drawing |
| `port/input/stick.c` | dead zone, the N64 range, the octagonal gate |
| `port/config/config.c` | the INI reader, the settings table, the file's location, the first-run file, the layers, saving changes in place |
| `port/config/race_cars.c` | `game.cars` at run time: the setting, and the count the game latches at each race's start |
| `port/config/cheats.c` | `game.cheats_menu` and `game.cheats_unlock`: [the cheat screen](#the-cheat-screen-in-the-main-menu-gamecheats_menu-gamecheats_unlock) in the main menu, every cheat on it available |
| `port/config/presets.c` | the [presets](#presets-n64-original-pc-enhanced-pc-maximum): N64 original, PC enhanced, PC maximum |
| `port/input/tests/` | `frontend_test` (no SDL), `frontend_menu_test` (the overlay, headless) and `frontend_sdl_test` (SDL2 builds) |

## Controls

The defaults. Keys are named by their position on a US keyboard, so W/A/S/D stay in place on other layouts.
Controllers use SDL's GameController layout: XInput positions, so "A" is the south button on any pad, Cross on a
PlayStation pad and B on a Nintendo one.

| N64 | Keyboard (player 1) | Xbox | PlayStation |
|---|---|---|---|
| Stick | arrow keys | left stick | left stick |
| A (accelerate) | X | A (south), RT | Cross, R2 |
| B (brake) | C | X (west), LT | Square, L2 |
| Z | Z | LB | L1 |
| L | Q | Back (View) | Share (Create) |
| R | E | RB | R1 |
| START | Enter | Start (Menu) | Options |
| C-up / C-down | I / K | right stick up / down, or Y / B | right stick up / down, or Triangle / Circle |
| C-left / C-right | J / L | right stick left / right | right stick left / right |
| D-pad | W / S / A / D | D-pad | D-pad |

The triggers are buttons here: past `input.axis_threshold` they press A and B.

| Key | Does |
|---|---|
| F1 | open and close the [settings overlay](#the-settings-overlay), which holds the game; on a controller, both stick clicks together |
| Alt+Enter, F11 | toggle fullscreen, borderless or exclusive as `video.fullscreen_mode` says |
| F12 | save the shown frame as `rush2049pc-YYYYMMDD-HHMMSS.png` (in `video.screenshot_dir`; by default `screenshots/` in the data folder) |
| Pause | hold the game, and press again to go on |
| Escape | quit |

- **Where they come from.** The keyboard keys, the face buttons and the sticks are v5's
  (`Rush2049Recompiled/runtime/src/sdl_frontend.cpp`). v5 had the triggers on Z and R. Here the triggers are a
  second accelerate and brake, Z moves to LB, and L moves to Back. v5's exact layout is one edit away (the
  file's `[controller]` comment gives it).
- **Not checked against the ROM.** That A accelerates and B brakes is the game's usual layout, but it was not
  checked here. The game maps N64 buttons to its own through a ROM table (`D_8011FAD4`, read by
  `ControllerUpdate`), and the game has its own control setup screen.
- **Players.** The keyboard is player 1 (`input.keyboard_player`).
  - Controllers take players from `input.first_controller_player` up, in the order they are plugged in. They
    keep that player until unplugged. A pad with the generic mapping (below) takes one at its first button press.
  - A pad plugged in while every player has one waits, and takes the first player that frees up.
  - A player's keyboard and controller are merged: buttons are OR'ed, and the stronger stick wins on each axis.
  - With `first_controller_player = 2`, the keyboard has player 1 to itself (v5's `--keyboard-alone`).
- **Connected.** Controller 1 is always connected. Another port is connected while a controller is assigned to
  it or a press script uses it, so unplugging a pad unplugs that N64 controller.
- **The stick.**
  - The dead zone (`input.deadzone`, 0.15) is radial. The travel past it is rescaled from 0, so movement starts
    small instead of jumping.
  - `input.sensitivity` (1) multiplies the travel past the dead zone: at 2 full tilt comes at half the travel; at
    0.5 it is never reached. Keys and buttons on the stick still give full tilt.
  - Full tilt reaches `input.stick_range` (80) left, right, up and down. On the diagonals it reaches
    (`stick_diagonal`, `stick_diagonal`) = (70, 70): the octagonal gate of an N64 stick. Between those, it
    follows the octagon's edges.
  - Keys and buttons bound to `STICK_*` give full tilt; two of them give the diagonal corner.
  - **Smooth keyboard steering** (`input.keyboard_steering = smooth`; `instant`, the default, is full tilt at
    once). Each axis of the stick those keys and buttons give moves a step a retrace toward where they point
    (`pc_steer_step`, port/input/stick.c): out to full tilt in `input.keyboard_steer_time` (250 ms: 15 retraces, the
    N64 stick reading 5, 11, 16, 21, 27, 32 ... 75, 80), back to the centre three times as fast (5 retraces: 64, 48,
    32, 16, 0) when the key is released or both ways are held, and on a reversal back to the centre at that pace and
    out the other way at the ramp's, with what is left of the retrace past the centre. A ramp of `0` is instant,
    reversal included. The value goes through the gate as a stick's does, so Right and Up held reach the corner,
    (70, 70). The steps are the polls (`pc_input_live_poll`, once per retrace in `pc_input_poll`), 60 a second on
    the N64 clock whatever `game.frame_rate` and `video.frame_rate` are, and the same on the virtual clock (`--fast`);
    nothing in it knows the game's state, so a key held from the menus into a race keeps its ramp. While the game is
    held (the pause key, the overlay) there are no retraces and the ramp waits; keys still held when the overlay
    closes are ignored until released (above), so the stick goes back to the centre at the return's pace. Up and
    Down (the stick's y, `STICK_UP` and `STICK_DOWN`) ramp the same way, so where the game reads the stick against a
    threshold of its own (a menu moved by the stick) a key on it acts when the ramp passes that threshold rather
    than at the press; the D-pad's keys are not ramped. A controller's stick, and the press scripts' own `STICK*`
    presses into the pads, are not changed; `--press-via keyboard` presses keys, which ramp.
  - `stick_diagonal = 0` gives a round gate instead.
- **Triggers and stick directions as buttons.** They press their N64 button past `input.axis_threshold` (0.5 of
  the travel after the dead zone).
- **Rumble.** Each player's rumble pak (`game.pak`, `pak2`, `pak3`, `pak4` = rumble, or auto; `--rumble` for
  player 1) has the motor that `port/ultra/si.c` emulates, and it drives `SDL_GameControllerRumble` on that player's
  pad: player 2's motor rumbles player 2's controller.
  - Auto, the default for players 2-4, is a rumble pak while the player's controller can rumble
    (`SDL_GameControllerHasRumble`, SDL 2.0.18 and later; before that every pad is taken to rumble), else nothing.
    The log's controller line ends in "; no rumble motor" for a pad that can't. A pad that leaves takes its rumble
    pak with it, and the game sees it go; one that comes brings one (a hot swap: [ULTRA.md](ULTRA.md)
    "Accessories").
  - Both motors run at `input.rumble` × the level. The level is 1 while the motor is on. Otherwise it is the
    share of the last 4 retraces the motor was on, so a game that pulses the motor gets a steady, weaker rumble.
  - Each rumble lasts 1 s and is renewed every 0.5 s while it goes on, so it stops by itself if the game stops.
  - It stops at quit.
- **Scripts.** `--press` and `--press-file` (v5's format, [ULTRA.md](ULTRA.md)) still work. They are OR'ed over
  live input, and a scripted stick direction replaces that axis. `MENU` is this port's own: the overlay's key.
- **Focus.** Losing focus releases every held key, because the key-up events go to the other window.
  Controllers keep working in the background, as in v5.
- **Pause.** The pause key, and losing focus with `video.pause_on_focus_loss = on`, hold the main loop. While it
  is held:
  - there are no retraces, OS timers, RSP tasks or AI events, so the game's threads stop where they wait for
    them;
  - window events are still handled, the last frame is shown again when the window is exposed or resized, and
    the title says "paused";
  - the N64 clock is set back by the time held, so `osGetTime` and the timers don't jump.
- **Hotkeys** never reach the game. Binding a hotkey's key to a control does nothing for that key.
- **The mouse** never reaches the game: it is the [overlay's](#the-mouse) only. During play the cursor is hidden from
  the start, shows when the mouse moves, and hides again 2 s after it stops; while the overlay is open it shows.
- **Mappings.** A `gamecontrollerdb.txt` next to `rush2049pc.ini` is loaded at start (SDL's format), for pads
  that SDL doesn't know.
- **Pads SDL has no mapping for** (DirectInput pads, N64-style USB pads, wheels) used to be passed over without a
  word. They get a generic mapping: axes 1-2 the stick, axes 3-4 the right stick, hat 1 the D-pad, buttons 1-10 in
  XInput's order (a b x y, LB RB, back start, the stick clicks), then guide and the extras, so the overlay can
  bind every one; axes 5-6 are the triggers from their middle up, so an axis resting in the middle reads released.
  - It takes a player at its first button press, not when it is found, since a joystick may be something other
    than a pad (a phantom device would otherwise hold player 1).
  - Something with neither two axes nor a hat, or without buttons, is not used, and the log says so.
  - A line for it in `gamecontrollerdb.txt` replaces the generic mapping.
- **Windows.** Xbox-style pads go through XInput, which is polled. SDL's default for them, its raw input driver,
  takes the sticks and buttons from window messages to a hidden window and only the triggers and guide from XInput,
  so a pad whose raw input doesn't arrive has working triggers and dead sticks. `SDL_JOYSTICK_RAWINPUT=1` in the
  environment brings raw input back. PlayStation and Switch pads stay on HIDAPI, others on DirectInput. The window
  is raised at start, so the keys reach it rather than the console it was started from.
- **The log** (`rush2049pc.log`) names each controller as it is found, and its player:
  `controller 'Xbox Series X Controller' (Xbox One, SDL mapping, GUID 030000005e04...) -> player 1`. It also says
  when a controller leaves, waits for a free player, gets the generic mapping, isn't used, or when there is none
  at start. The overlay's Controllers page shows the same live (below).

## The window

- `port/gfx` owns the window and pumps its events in `pc_gfx_poll_events`. port/input gets every event through
  `pc_gfx_set_event_hook`, before port/gfx's own handling, so there is one event loop. Everything runs on the
  main thread, which holds the GL context.
- The main loop now pumps events first at each retrace, then reads the pads, so the pads hold that retrace's
  input rather than the previous one's.
- **Size and sharpness.** By default (`video.scale = native`) the frame is drawn at the window's own pixels, any
  fraction of the N64's (6.75× at 2880 × 1620), and shown pixel for pixel, with square N64 pixels and bars for the
  rest. `video.scale = 1` (or `n64`) is the N64's 320 × 240 shown in square pixels; 2-8 and `auto` (the largest whole
  multiple) as before ([GFX.md](GFX.md) "Resolution and scale"). The window is resizable, with a minimum of
  320 × 240. The default window (`video.width = auto`) is two thirds of the desktop at the picture's aspect:
  1920 × 1080 on a 2880 × 1620 screen.
- **Display scaling.** Sizes are pixels. On Windows the program is per-monitor DPI aware, so a 200% desktop gives it
  real pixels, not a half-size window that Windows blurs up; where the window system counts in points (macOS,
  Wayland), port/gfx converts by the drawable's size against the window's ([GFX.md](GFX.md) "Window and display").
- **Fullscreen.** Alt+Enter and F11 toggle it; `video.fullscreen_mode` is `desktop` (borderless over the screen,
  the default) or `exclusive` (the display switches to `video.resolution`, the closest mode it has).
  `video.resolution` in desktop fullscreen is the size the frame is drawn at, fitted to the screen (`1080p` on a
  1620p panel draws less and scales up).
- **Remembered.** With `video.remember_window` (on), the window's size and place out of fullscreen go into the file
  read at start when the program ends, if they changed (`video.width`, `height`, `position`); not with
  `--no-config`, nor headless. A place on a display that is gone opens centred.
- **Widescreen.** `video.widescreen` (`--widescreen`, `RUSH_GFX_WIDESCREEN`) widens the picture to a display
  aspect: `off`, `auto`, `16:9`, `16:10`, `21:9`, any `W:H` or a ratio from 4:3 to 4. The 3D view shows more at
  the sides at the same scale; menus and sprites stay 4:3 in the middle; full-width fills and backgrounds span
  the width. The renderer does it all ([GFX.md](GFX.md) "Widescreen"); the game code is unchanged.
  - `video.hud` (`RUSH_GFX_HUD`): `edges`, the default, puts the race HUD at the screen's edges: in a race with one
    view, the speedometer and radar keep their distance to the left edge, the place, laps, coins, map and timer to
    the right one, as on a 4:3 screen; centred banners, the pause menu, other screens and split screen stay in the
    4:3 frame. `centered` keeps the HUD in the 4:3 frame, as the N64 draws it ([GFX.md](GFX.md) "The HUD at the
    screen's edges"). Its one game C change places the coin icons beside their moved counts.
  - `auto`, the default, takes the window's aspect at each frame, so resizing the window or going fullscreen
    changes it. A 4:3 window is off.
  - `video.fill` (`RUSH_GFX_FILL`): `off`, the default, shows the game's 220 rows in a TV's 240-line frame, with a
    band above and below. `on` makes them reach the window's top and bottom: with widescreen the picture is the
    window's shape (at most an N64 pixel cut at each side), the 3D a little less wide than without it; in a 4:3
    window the bands stay ([GFX.md](GFX.md) "Fill the window").
  - With a fixed aspect and an auto window, the window opens at the picture's aspect; with 640 × 480 it opens at
    853 × 480 at 16:9. Another size set in the file or with `--size` is kept.
  - At 16:9 the frame is 426 × 220 (the game's 220 rows of the 240-row screen, with its bands above and below, as
    on a TV). Native draws it 2880 pixels across on a 2880 × 1620 screen; `auto` and whole scales at 6×, with a
    border.
- **Quit.** Closing the window, the quit key, Ctrl-C and SIGTERM all end the loop the same way. The controller
  pak is flushed, rumble stops, and audio and the renderer shut down.

## Settings: rush2049pc.ini

- **Location.** (What the program writes besides, the controller pak, screenshots and the log, follows the same
  rule: [RELEASE.md](RELEASE.md) "Files".)
  - `rush2049pc.ini` next to the executable, when there is one (a portable install).
  - Otherwise `$XDG_CONFIG_HOME/rush2049pc/rush2049pc.ini`. That is `~/.config/rush2049pc/` when
    `XDG_CONFIG_HOME` is unset, or relative, as the XDG spec says. On Windows, `%APPDATA%\rush2049pc\rush2049pc.ini`.
  - `--config PATH` reads another file, and `--no-config` reads none.
- **First run.** When the located file doesn't exist, it is written with every setting and a comment on each,
  and then read; an empty one next to the program (as the release archives ship it) is filled the same way. A
  setting at its default is written commented out (`; key = value`), so it follows the program's defaults when
  they change in a later version; the overlay's saves and a removed `;` keep a value. `--config`, `--no-config`,
  `--print-config` and `--selftest` never write it.
- **Written by the program** only in place, through `pc_ini_update` (the rest of the file byte for byte): the
  overlay's Save, the window's size and place at exit (`video.remember_window`), and `game.rom` when the player
  chooses the ROM in the first run's file dialog ([RELEASE.md](RELEASE.md) "Choosing the ROM").
- **Layers.** Each overrides the one before:
  1. the defaults;
  2. the file;
  3. the environment variables other parts already read: `RUSH2049_AUDIO`, `RUSH_GFX_SCALE`, `RUSH_GFX_API`,
     `RUSH_GFX_WIDESCREEN`, `RUSH_GFX_FRAME_RATE`, `RUSH_GFX_TEXTURES`, `RUSH_GFX_HUD`, `RUSH_GFX_DRAW_DISTANCE`,
     `RUSH_GFX_FOG`, `RUSH_GFX_TEXTURE_LOD`;
  4. a press file's `pak on|off`;
  5. command-line options, in order (`--preset NAME` is its settings at its place: [below](#presets-n64-original-pc-enhanced-pc-maximum)).
- **Bad values.** A bad line in the file, or a bad environment value, is reported with its line and skipped.
  The setting keeps its value from the layer before. A bad option is an error (exit 2).
- **`--print-config`** prints the settings in effect, as a settings file.
- **`--set SECTION.KEY=VALUE`** sets anything, for example `--set video.vsync=off` or `--set keyboard.A=Space`.
- **The format.** `[section]`, `key = value`, and `;` or `#` comments. An inline comment needs whitespace before
  its `;` or `#`. A value in double quotes keeps edge spaces and comment characters. Names are
  case-insensitive. Booleans are `on/off`, `true/false`, `yes/no` or `1/0`. A UTF-8 BOM and CRLF are fine.

| Setting | Default | Option | |
|---|---|---|---|
| `game.rom` | `baserom.us.z64` | `--rom`, a path alone | the ROM: a full path, or a name looked for next to the program, in the user data folder, then in the working directory ([RELEASE.md](RELEASE.md) "The ROM"). A ROM chosen in the first run's "Open ROM..." dialog is written here as a full path, in place (`pc_settings_save`), in the file that was read; not with `--no-config` |
| `game.rom_check` | on | `--no-rom-check` | refuse a ROM whose SHA-1 isn't the USA ROM's |
| `game.expansion_pak` | on | `--expansion-pak` | 8 MiB of RDRAM (off: 4 MiB) |
| `game.cars` | original | `--set game.cars=N` | the most cars a race holds: `original` (the N64's 6) or 6-12 (`PC_MAX_LINKS`); from the next race ([below](#the-cars-a-race-holds-gamecars), [MORECARS.md](MORECARS.md)) |
| `game.memory` | original | `--set game.memory=N` | the game's memory: `original` (the N64's 8 MiB, 4 with `expansion_pak` off) or 16-256 MiB, the RDRAM window, osMemSize and so its heap; from the next start. With a size the game uses its Expansion Pak features, so `expansion_pak = off` does nothing then, and the settings warn ([MEMORY.md](MEMORY.md) 1) |
| `game.frame_rate` | original | `--set game.frame_rate=60` | the game's own frame rate: `original` (a frame every two retraces at most, 30 a second: the N64's) or `60` (a frame every retrace: the menus, the HUD, the cameras and the effects step 60 times a second at the N64's speed; the car physics is the same). From the next screen ([below](#the-games-frame-rate-gameframe_rate), [FRAMERATE.md](FRAMERATE.md) 7) |
| `game.pc_setup` | off | `--set game.pc_setup=on` | PC SETUP in the main menu, below its options: it opens the settings overlay over the menu; at once ([below](#pc-setup-in-the-main-menu-gamepc_setup)) |
| `game.ghost_prompt` | off | `--set game.ghost_prompt=on` | the ghost save prompt after a finished ghost race for every profile, not only the developer's name; from the next race's end ([SAVES.md](SAVES.md) 7.1) |
| `game.cheats_menu` | off | `--set game.cheats_menu=on` | the main menu's own CHEATS option (the cheat screen) without its button code; at once ([below](#the-cheat-screen-in-the-main-menu-gamecheats_menu-gamecheats_unlock)) |
| `game.cheats_unlock` | off | `--set game.cheats_unlock=on` | every cheat on the cheat screen available, without its button code; at once ([below](#the-cheat-screen-in-the-main-menu-gamecheats_menu-gamecheats_unlock)) |
| `game.mods` | on | `--no-mods` (off) | use the mods folder's mods that are on: their files in place of the cartridge's; off: the cartridge's only; from the next start ([MODS.md](MODS.md)) |
| `game.mods_dir` | `mods` (in the data folder) | `--mods-dir PATH` | the mods folder: a folder or .zip per mod; a name alone is in the data folder, `--mods-dir` from the working directory |
| `game.mods_order` | (empty) | | mod folders, comma-separated, first first: of two mods' files for one wave the first's is used; the rest follow by their mod.ini priority (the overlay's Mods page writes it) |
| `[mods]` folder | on | | each mod's folder = on or off; a mod not listed is on (the Mods page writes these) |
| `game.pak` | memory | `--no-pak`, `--rumble`, `--pak` | controller 1's accessory: memory (the controller pak: the saves), rumble, none, or auto (rumble when the player's pad can rumble, else none) |
| `game.pak_file` | `rush2049pc.mpk` | `--pak PATH` | the controller pak image; a name alone is in the data folder (RELEASE.md "Files"), `--pak` from the working directory |
| `game.pak2`, `pak3`, `pak4` | auto | `--no-pak` (none) | controllers 2-4's accessories, as `game.pak`: auto gives multiplayer rumble where the pads have it, without a file; memory gives the player saves of their own |
| `game.pak2_file`, `pak3_file`, `pak4_file` | `rush2049pc-p2.mpk`... | | controllers 2-4's controller pak images, in the data folder as `pak_file`, each loaded, backed up and written as controller 1's (RELEASE.md "Saves") |
| `game.saves`, `saves2`, `saves3`, `saves4` | pak | `--saves pak\|files` (all four) | how each controller's pak is kept: `pak` (its `.mpk`, as above) or `files` (a folder: a file per profile and ghost, any number of ghosts, the ones that fit on the pak the game sees, ghosts shared as `.r49ghost`); the first start with files imports the `.mpk`, which is left as it is; from the next start ([below](#saves-as-files-gamesaves), [SAVES.md](SAVES.md)) |
| `game.saves_dir`, `saves2_dir`... | `saves/p1`, `saves/p2`... | | each controller's saves folder, in the data folder as `pak_file` |
| `video.width`, `video.height` | auto | `--size WxH` | window size in pixels; auto: two thirds of the desktop at the picture's aspect (headless: 320 × 240) |
| `video.position` | center | | where the window opens: `center` or `X,Y` |
| `video.remember_window` | on | | write the window's size and place at exit (above) |
| `video.fullscreen` | off | `--fullscreen` | start in fullscreen |
| `video.fullscreen_mode` | desktop | `--exclusive` | `desktop` (borderless) or `exclusive` (the display's mode changes) |
| `video.resolution` | desktop | `--resolution` | `desktop`, `720p`, `1080p`, `1440p`, `1620p` (2880 × 1620), `2160p` or `WxH`: exclusive fullscreen's display mode; the size desktop fullscreen draws at |
| `video.vsync` | on | | swap interval 1 or 0; on, no more frames are shown than the display refreshes, whatever `frame_rate` asks (logged) |
| `video.frame_rate` | original | `--frame-rate` | frames shown a second: `original` (the game's own: each frame as the VI swaps it in, 30 or fewer; 60 or fewer with `game.frame_rate = 60`), `display` (the display's refresh; with vsync), or 30-1000 (60, 120, 144). The frames between the game's are drawn from its last two; the game's timing is unchanged, and screenshots are the game's frames ([FRAMERATE.md](FRAMERATE.md); `RUSH_GFX_FRAME_RATE`) |
| `video.scale` | native | `--scale N` | internal resolution: `native` (the window's own pixels, any fraction), `auto` (the largest whole multiple the window holds), or 1-8 (`1` or `n64`: the N64's) (`RUSH_GFX_SCALE`) |
| `video.api` | auto | | `auto` (desktop GL first on Windows, whose drivers are made for it; OpenGL ES first elsewhere), `gl` or `gles`; each falls back to the other (`RUSH_GFX_API`, [GFX.md](GFX.md) "Backend, frame and settings") |
| `video.widescreen` | auto | `--widescreen` | auto (the window's), off, 16:9, 16:10, 21:9, W:H or a ratio up to 4 (`RUSH_GFX_WIDESCREEN`; above) |
| `video.fill` | off | | on: the picture's rows reach the window's top and bottom, no bands (`RUSH_GFX_FILL`; "Widescreen" above) |
| `video.pause_on_focus_loss` | off | | hold the game in the background |
| `video.screenshot_dir` | (`screenshots` in the data folder) | | where F12 saves |
| `video.textures` | original | | `original`, `upscaled` (the 3D world's textures 2x at internal scale 2-3, 4x from 4; the HUD and menus as they are) or `pack` (replacements from `video.texture_pack`) ([TEXTURES.md](TEXTURES.md); `RUSH_GFX_TEXTURES`) |
| `video.texture_pack` | `textures` (in the data folder) | | the texture pack's folder: PNG files named as the dump names them |
| `video.texture_dump` | (empty: off) | | a folder to write every texture drawn into, as PNG, for making a pack ([TEXTURES.md](TEXTURES.md) "Texture packs") |
| `video.hud` | edges | | the race HUD in widescreen: `edges` (at the screen's edges) or `centered` (in the 4:3 frame) (`RUSH_GFX_HUD`; "Widescreen" above) |
| `video.draw_distance` | original | | how far the game draws: `original` (the N64's: its 2,000-ft far plane, the world's visibility tables, each model's distance), `far` (each limit twice as far, more of the track: the cars, their wheels and the scenery further out), `max` (the whole track, every model at any distance) (`RUSH_GFX_DRAW_DISTANCE`; [below](#draw-distance-detail-and-fog-videodraw_distance-detail-fog), [DETAIL.md](DETAIL.md)) |
| `video.detail` | original | | `high`: each model's most detailed level wherever it is drawn, and in split screen the one-player models and objects (those from the next track) ([DETAIL.md](DETAIL.md) 2.5) |
| `video.fog` | original | | `original` (the game's: the track select's fog option), `far` (its start and end twice as far) or `off` (and with the fog option the sky dome the game leaves out in a fog, from the next track) (`RUSH_GFX_FOG`; [DETAIL.md](DETAIL.md) 3) |
| `video.texture_lod` | original | | which copy of a texture's mip chain a pixel of the world shows: `original` (where the N64 shows it, for its 320 x 240), `sharp` (where the window's own pixels need it: the road sharp further out) or `anisotropic` (sharp, and four samples along a surface seen edge on, the far road); from the next triangle (`RUSH_GFX_TEXTURE_LOD`; [DETAIL.md](DETAIL.md) 2.7) |
| `audio.backend` | auto | `--audio` | auto, sdl, alsa, null, wav (`RUSH2049_AUDIO`) |
| `audio.volume` | 100 | | master volume, 0-100 ([AUDIO.md](AUDIO.md): its square is the gain) |
| `audio.latency` | 0 | | the least sound kept queued for the device, 0-200 ms; 0 sizes it from the device's pulls, and it grows after each underrun ([AUDIO.md](AUDIO.md) §7) |
| `audio.voices` | original | | the sound library's voices: `original` (the N64's 32: 16 for the effects, 16 for the songs) or `48`, `64`, `96` (the songs keep 16, the effects get the rest, the sound manager plays three quarters of those at once, with 128 positional sources), so more cars' engines and effects are heard in a race of 12; from the next start ([AUDIO.md](AUDIO.md) 10.2) |
| `audio.engines` | original | | which drones' engines play: `original` (the game's: the loudest its budget holds), `nearest2`, `nearest3`, `nearest4`, `nearest6`, `nearest8` (the N nearest drones to each player's view, with hysteresis and a quarter-second fade; the others' silent) or `all` (every drone in earshot, outside the budget the effects keep; with 12 cars it needs `voices` 64 or more); at once ([AUDIO.md](AUDIO.md) 10.2) |
| `input.keyboard_player` | 1 | | 1-4, 0 for none |
| `input.first_controller_player` | 1 | | controllers fill players from here |
| `input.deadzone` | 0.15 | | 0-0.9 |
| `input.sensitivity` | 1.0 | | 0.5-2: the stick's travel past the dead zone times this |
| `input.axis_threshold` | 0.5 | | a trigger or stick direction bound to a button presses it past this |
| `input.stick_range` | 80 | | full tilt along an axis, 1-127 |
| `input.stick_diagonal` | 70 | | each axis at full diagonal tilt; 0 for round |
| `input.keyboard_steering` | instant | | keys and buttons bound to `STICK_*`: `instant` (full tilt at once) or `smooth` (a ramp to full tilt over `keyboard_steer_time`, back to the centre in a third of it; [above](#controls)); at once |
| `input.keyboard_steer_time` | 250 | | with `smooth`: ms from the centre to full tilt, 0-1000 (0 is instant), 60 steps a second at any frame rate |
| `input.rumble` | 1.0 | | strength 0-1 |
| `[keyboard]` `A` ... `STICK_RIGHT` | the table above | | SDL key names, comma-separated |
| `controller.analog` | left | | the stick that is the N64 stick: left, right, none |
| `controller.menu` | leftstick+rightstick | | what opens the settings overlay: a button or axis, or several held together joined by `+` (`guide`, `back+start`) |
| `[controller]` `A` ... `STICK_RIGHT` | the table above | | SDL GameController names |
| `hotkeys.fullscreen`, `screenshot`, `pause`, `quit`, `menu` | F11, F12, Pause, Escape, F1 | | empty for none; `menu` is the settings overlay |

- **N64 controls in bindings.** A B Z START, DUP DDOWN DLEFT DRIGHT, L R, CUP CDOWN CLEFT CRIGHT, and STICK_UP
  STICK_DOWN STICK_LEFT STICK_RIGHT.
- **Key names.** SDL's names: `X`, `Return`, `Space`, `Left Shift`, `Keypad 8`, `F1`, and so on. Punctuation
  goes by an alias, because a list or a comment can't hold the character: `Comma`, `Period`, `Slash`,
  `Semicolon`, `Apostrophe`, `Minus`, `Equals`, `LeftBracket`, `RightBracket`, `Backslash`, `Grave`, `Hash`.
- **Controller names.**
  - Buttons: `a b x y back guide start leftstick rightstick leftshoulder rightshoulder dpup dpdown dpleft dpright
    misc1 paddle1-4 touchpad`.
  - Axes: `lefttrigger`, `righttrigger`, and `+leftx -leftx +lefty -lefty` with the same for `rightx` and
    `righty`. +x is right and +y is down, as SDL has them.
- **Unknown names** are reported and skipped. The rest of the list still works.
- Every other environment variable the program reads is environment-only ([below](#environment-variables)), as
  are `--dump-audio` and the run-limit options.
- **How settings reach the parts.**
  - port/gfx's settings go through the variables it already reads: main sets `RUSH_GFX_SCALE`, `RUSH_GFX_API`,
    `RUSH_GFX_WIDESCREEN`, `RUSH_GFX_FRAME_RATE`, `RUSH_GFX_TEXTURES`, `RUSH_GFX_HUD`, `RUSH_GFX_DRAW_DISTANCE`,
    `RUSH_GFX_FOG` and `RUSH_GFX_TEXTURE_LOD` to the resolved values
    before `pc_gfx_init`, and `RUSH_GFX_TEXTURE_PACK`, `RUSH_GFX_TEXTURE_DUMP` and `RUSH_GFX_TEXTURE_CACHE` to the
    data folder's paths unless they are set already. The main loop asks port/gfx for the frame rate in effect
    (`pc_gfx_frame_rate`) at each pass, so the overlay's change applies at once.
  - The window size goes to `pc_gfx_init`; the fullscreen mode, the resolution and the place to
    `pc_gfx_set_fullscreen_mode`, `pc_gfx_set_resolution` and `pc_gfx_set_window_position` before it.
  - port/input sets vsync and fullscreen at start through port/gfx (`pc_gfx_set_vsync`, `pc_gfx_set_fullscreen`),
    and the title and the minimum size on the SDL window.
  - Old defaults: a file an older build wrote with every default (no `video.resolution` line; scale 1, 640 × 480,
    widescreen off) gets the new ones, native, auto and auto, in the file too, with a line in the log.
  - main sets the volume and the latency with `pc_audio_set_volume` and `pc_audio_set_latency` after
    `pc_audio_init`.
  - main sets `game.cars` with `pc_race_cars_set` before the game starts; the game takes it at each race's start
    ([below](#the-cars-a-race-holds-gamecars)).
  - main sets `video.draw_distance` and `video.detail` for the game with `pc_detail_set` (port/config/detail.c); the
    renderer takes `draw_distance`, `fog` and `texture_lod` from their three variables
    ([below](#draw-distance-detail-and-fog-videodraw_distance-detail-fog)).
  - main sets `game.frame_rate` with `pc_game_fps_set`; the game takes it at each change of its state
    ([below](#the-games-frame-rate-gameframe_rate)).

### The cars a race holds (game.cars)

2026-10-29, [MORECARS.md](MORECARS.md) 10.1. `original`, the default, is the N64's 6 exactly; 6 is the same. 7-12
let a single race hold that many: the players and the drones the track select's Drones option asks for.

- **Latched.** The game reads the count through `pc_race_cars()` (`PC_RACE_CARS`, `port/include/pc_game_types.h`)
  in loops all over a race, on its game and model threads, so that is a copy: `pc_race_cars_begin()` latches the
  setting into it where a race's roster is filled (CalcPolePositions, except on the results screen, where
  display_list_flush calls it for the cars that race on, and the attract demo's `func_800EC190`:
  [MORECARS.md](MORECARS.md) 3.1, item 3). A change in the overlay applies from the next race and never during one.
  Until the first `pc_race_cars_begin()` the count is 6. `pc_cars_setting()` is the setting itself, unlatched (6
  for `original`), for the front end. All three are in `port/config/race_cars.c`; `pc_race_cars_set` (main, the
  overlay) takes the setting there, `original` as 0, anything outside 6-12 to the nearest end.
- **The Drones option** (the track select's options list, item 17: `runtime_a_c93c_closure/group.c`) goes from 0 to
  `pc_cars_setting() - 1` and wraps there: 5, as on the N64, with `original`. Its reset value (the reset button)
  and its default (`func_800CCB40`) stay 5, so a player who never touches it races 6 cars at any setting. It shows
  the value as the other options do, a plain decimal number, so 10 and 11 are two digits in its box (seen on the
  retail cartridge).
- **Saves.** The option is a signed byte of the player's profile (`reverb_setup`'s `modeA`, the race tracks'), so
  0-11 fit. A profile saved with more than 5 drones keeps its value when the setting goes back to `original`: the
  option shows it, the race holds 6 cars (the roster's cap is the latched count, item 3), and LEFT or RIGHT wraps
  it to 0, as the N64's rule does for a value past its 5 (seen on the retail cartridge: 11 with `game.cars=12`, the
  setting put back to `original` in the overlay, then LEFT: 0); an N64 reading such a profile does the same.
- **Status (2026-10-29).** Races of up to 12 cars work: every item of [MORECARS.md](MORECARS.md) 10.2 is in, and
  the retail cartridge races 12 on each race track, for one and two players, to the finish and back to the menu
  ([MORECARS.md](MORECARS.md) 10.4). To race more than 6, set the cars (the overlay's Game page, or
  `--set game.cars=12`) and raise the track select's Drones option, whose default, 5, races 6 at any setting. The
  synthetic paths `race12`, `race2p12`, `demo12`, `pause12`, `raceend12` and `circuit12` raise it through the menu.

### Draw distance, detail and fog (video.draw_distance, detail, fog)

2026-11-03, [DETAIL.md](DETAIL.md). Three settings for what a PC's GPU can draw that an N64 couldn't; at `original`,
the default, each is the N64's game exactly.

- **`draw_distance`**: `far` moves every distance limit of the game twice as far (the world's visibility tables
  reach the cells within 2,000 ft of the eye; each model's last level of detail; the fog option's radius; the
  matrices' 2,048 ft; the far scenery's 1,600 ft) and the depth range's far plane to twice the game's; `max` lifts
  them all and moves the far plane to 32 times the game's. The game's fog and everything else it sees of the depth
  stay as they were.
- **`detail`**: `high` draws each model's first, most detailed level wherever it is drawn (no simpler car or wheels
  further out), the tires at their first level in split screen, and, from the next track, split screen's models and
  level objects as one player has them.
- **`fog`**: `far` gives a point the fog the game gives one half as far (the fog options and their regions); `off`
  none, and with a fog option the sky dome the game leaves out in a fog (from the next track).
- **Live.** The overlay's Video page has the three ("Draw distance", "Detail", "Fog"), and "Far textures" (`video.texture_lod`, DETAIL.md 2.7); a change applies from the game's
  next frame (`pc_detail_set`, `pc_gfx_set_draw_distance`, `pc_gfx_set_fog`), detail's split-screen models from the
  next track.
- **What it costs, what it looks like**: [DETAIL.md](DETAIL.md) 4 and 5. At a run's end the log's `scene:` line gives
  what each view drew and what each limit left out.

### The game's frame rate (game.frame_rate)

2026-11-03, [FRAMERATE.md](FRAMERATE.md) 7. `original`, the default, is the N64's game exactly: a frame every two
retraces at most. `60` draws a frame every retrace, and what the game steps once a frame by a constant tuned to two
retraces (a menu's slide, a blink, a fade, a camera's filter, a frame count) takes the shorter frame, so everything
keeps its speed in real time. The car physics runs every retrace either way.

- **Latched.** The game reads a copy, `pc_fps60` (`port/config/game_fps.c`, `port/include/pc_game_fps.h`), which
  `pc_game_fps_begin()` takes where the game's state changes (game_loop): a change in the overlay applies from the
  next screen (the next menu, the race's start, the results, the attract demo), never in the middle of one. Until the
  first state change it is original. `pc_game_fps_setting()` is the setting itself; `pc_game_fps_set` (main, the
  overlay) takes 60 for 60 and anything else for original.
- **Not `video.frame_rate`.** That one is how many frames are shown (frames between the game's are drawn from its
  last two); this one how many the game draws. Both can be on: the game at 60 and `video.frame_rate = 120` draw one
  frame between each two of the game's.
- **Under vsync** ([FRAMERATE.md](FRAMERATE.md) 7.6). At 60 with `video.frame_rate = original` and a swap that waits
  for vsync on a display under 100 Hz, the main loop paces the game's own frames by the display's vblanks
  (`pc_gfx_set_own_paced`), as `video.frame_rate = 60` does but with none drawn between: presented at its retrace,
  each frame's swap could hold the loop a whole refresh, and on a 60 Hz display at a bad phase the game kept 30
  frames a second. On a 144 Hz display the game's 60 frames show for 2 or 3 vblanks each; `video.frame_rate =
  display` draws one a vblank there.
- **Run limits.** `--frames`, `--screenshot-at` and `--until-state`'s count are in graphics tasks, which come twice as
  often at 60; `--seconds` is the same.
- **A host that can't keep 60** ([FRAMERATE.md](FRAMERATE.md) 7.7). In a race at 60 the game counts its frames
  and the retraces they take (`pc_game_frame_note`; the race from game_loop's state change, `pc_game_fps_state`).
  When over 4 s it keeps fewer than 0.9 frames a retrace, one line in the log says so, once a race: `game.frame_rate
  = 60: the game drew 0.79 of its frames in 4 s of the race (47 a second): this host can't keep 60 here. The game
  keeps its speed: ...`. About's Game frame rate row shows the last race's share (`pc_game_fps_describe`). There is
  no fallback: a frame of two retraces at 60 is the N64's frame already, so such a host shows more frames at 60 than
  at original, unevenly.

### Saves as files (game.saves)

2026-11-04, [SAVES.md](SAVES.md). `pak`, the default, is the `.mpk` exactly as before. With `files` the game still
sees a controller pak, built from the player's folder when it goes in; what the game writes there comes back as
files (a profile, a ghost each), what it deletes goes to the folder's `trash/`.

- **Per player**, as `game.pak2_file` is: `saves2 = files` with `pak2 = memory` gives player 2 a folder of their own.
- **What is on the pak**: the profiles, the pinned ghosts, the ghosts the game had last time, then each player's
  fastest ghost per track and way while room is left (SAVES.md 5). A change (a pin, an import, a delete from F1)
  reaches the game as a hot swap at the start prompt or the main menu with no saved profile in use, else at the next
  start (SAVES.md 5.1).
- **Moving**: the first start with files imports the `.mpk`'s notes; F1 > Saves > Export to the .mpk writes them
  back (the old one kept as `.bak`), then `saves = pak`.
- **Sharing**: F1 > Saves > a ghost > Export writes a `.r49ghost` into `export/`; a `.r49ghost` dropped into
  `import/` is taken at start or by Import ghosts, checked first (SAVES.md 4.3).

### PC SETUP in the main menu (game.pc_setup)

2026-11-05 (`port/config/start_menu.c`, `pc_start_menu.h`). `off`, the default, is the N64's main menu. With `on`
the main menu gets a sixth option below VIDEO, **PC SETUP**, and choosing it opens the settings overlay over the
menu, as F1 does; closing it leaves the menu as it was, PC SETUP still selected. F1 > Game > PC SETUP in menu turns
it on and off at once.

- **Where it comes from.** The main menu (`updateStartScreen`, `src/ovl_a/groups/start_screen/group.c`) has ten
  option slots, each valid or not (`optionValid`, `func_8038F744`, in that file and in
  `src/ovl_a/groups/runtime_a_option_eligibility/group.c`): 0 the race, 1-4 its four screens, 6 the cheat screen
  once its code is entered, 7 and 9 a debug build's, 5 and 8 never. Each slot has its own button (the same pill
  model, made for all ten by `initButtons`), and the valid ones are stacked one gap apart, so a sixth valid slot gets
  a button, a place and the turn to its selected side as the cheat screen's does. PC SETUP is slot 8: with the cheat
  screen unlocked (its code, or `game.cheats_menu`: [below](#the-cheat-screen-in-the-main-menu-gamecheats_menu-gamecheats_unlock))
  it comes after it.
- **The game's code**, three PC macros on existing lines, each giving the original tokens on the N64:
  `PC_START_VALID` on optionValid's return (both copies: slot 8 valid while the setting is on), `PC_START_LABEL` on
  the label's draw (`state_utility(..., countdown_state.indexed[indices[20] + i])`, both of the function's layouts:
  the port's "PC SETUP" for slot 8 instead of the text bank's string; the label starts 35 pixels left of the button's
  middle, so it is short: "PC SETTINGS" would run past the button), and `PC_START_EXIT` on the menu's exit test
  (`else if (exitStartScreen)`): A on slot 8 sets a request and gives 0, so the menu isn't torn down and stays in its
  state; any other choice, and B, go on as before.
- **Opening the overlay.** The request is taken by the main thread at its next tick (`pc_menu_tick`), which opens the
  overlay on its main page; the game is then held on the menu (the section below).
- **The press that closes it.** START or B closing the overlay is still held when the game runs again: on the main
  menu START starts a race. The buttons held when the overlay closes now stay idle for the game until they are let
  go: taken at the close (`pc_input_menu_closed`, from the keyboard's and controllers' state, `pc_input_live_held`,
  and the scripts'), dropped from the game's reads until released (`pc_input_poll`, port/input/input.c). Buttons
  pressed after it closed count at once (`frontend.sdl.menu`: Escape closes it, then the newly bound keys reach the
  game). This was so for F1 from any screen before.
- **Tested**: `frontend.menu.live` (the item at once: slot 8 valid as PC SETUP, the other slots and exits as they
  were, A on it asking for the overlay and the tick taking the request once, Defaults off again);
  `frontend.menu.script` (START held across the closing doesn't reach the game); `synth_boot_pcsetup`
  (`tools/port/synth/pcsetup.press`, `# realtime: only`: the virtual clock stands still while the overlay holds the
  game, and the script with it, so it runs on the host's clock and `synth_compare.py` leaves it out): five down to
  PC SETUP, A opens the overlay, START closes it without the player screen, up and A into
  the fifth option's screen; its log has the three lines in order (`tools/port/log_check.py`). On the retail
  cartridge on the Pi the same steps went the same way.

### The cheat screen in the main menu (game.cheats_menu, game.cheats_unlock)

2026-11-05 (`port/config/cheats.c`, `pc_cheats.h`). Two settings, both `off` by default: the N64's game. F1 > Game >
Cheats menu and Unlock all cheats turn them on and off at once.

- **`game.cheats_menu`**: the main menu shows the game's own CHEATS option below VIDEO, as when its button code has
  been entered there, and A on it opens the cheat screen.
- **`game.cheats_unlock`**: on the cheat screen every one of the 21 cheats is available, as when its own button code
  has been entered: LEFT and RIGHT switch it, and it is drawn as an available one. The two are independent: with
  the unlock alone, every cheat is available whenever the cheat screen is reached the cartridge's way.

How the cartridge decides:

- **The CHEATS option.** The main menu's option slot 6 (the slots: "PC SETUP" above) is the cheat screen (state
  0x8000000, `updateStartScreen`'s `case 6`, `src/ovl_a/groups/start_screen/group.c:241`). optionValid
  (`func_8038F744`, both copies) makes it valid only while `D_801164A1` is set (`start_screen/group.c:296-297`,
  `runtime_a_option_eligibility/group.c:176-177`): the byte after the 21 cheats' flags (MAT3's `gCheatActive[23]`).
  Two things set it. The main menu checks its own button code each frame while it is clear (`CheckForEgg`,
  `func_8038F648`, against the code `InitEgg(23)` points at, `D_803B41D4`; `start_screen/group.c:256-257`, 271-272).
  And LoadPlayerData (`func_800CCEFC`, `src/blob/groups/menu_save_options/group.c:335-342`) sets it when the
  profile's name matches the cartridge's developer code (`D_801211CC`), with cheat states 14-16 and the ghost save
  prompt's flag (`D_801148E4`, [SAVES.md](SAVES.md) 7.1). Nothing saves it: it lasts the session.
- **A cheat's availability.** The cheat screen (`updateCheatScreen`, `func_8039C910`,
  `src/ovl_a/groups/cheat_screen/cheatscr.c`) has 21 cheats, each with a flag in `gCheatActive` (`D_8011648C`, s8).
  A cheat's flag is set only by its own button code, entered while it is selected: `if (!gCheatActive[sel])
  gCheatActive[sel] = CheckForEgg()` (`cheatscr.c:212-213`), against `gEggs[sel]` (`D_803B3460`, which InitEgg
  points at on each move). The flag is read in two places: LEFT and RIGHT switch the selected cheat only when it is
  set (`cheatscr.c:98`), and printOption (`func_803A5350`, `cheatscr.c:339-341`) draws the cheat's value bright when
  it is set, dim (colour 4) when not. No other code reads or writes these flags; LoadPlayerData writes only the
  CHEATS option's byte after them (its loop over cheats 1-20 has no body, in the retail code as here). So a cheat's
  availability is neither the profile's progress nor anything saved: it is the codes entered in this session.
- **What a switched cheat does.** Each case of the switch writes the game's flag (`D_801427A1` super speed,
  `D_8014061A` the invisible track, ...) and the state the screen shows (`gCheatState`, `D_801164A4`); cheats 3 and
  13-16 are states that other code reads (14-16 in `net_state_validate`'s unlock tables: every car, every track and
  circuit, the other unlockables the profile's records otherwise open). They stay as set across races, until switched
  again or the game restarts (InitSystem clears them, `func_800DE45C`).

The PC's:

- **`game.cheats_menu`** adds no line to the game's code: optionValid's PC hook (`PC_START_VALID`, PC SETUP's, on
  both copies' return) gives slot 6 valid while the setting is on (`pc_start_valid`, `port/config/start_menu.c`),
  whatever `D_801164A1` says. The flag isn't written: the main menu still checks its code, and with the setting off
  again the menu is the N64's at its next frame (the cursor moves off the option, as for PC SETUP). The developer
  name's other unlocks don't come with it.
- **`game.cheats_unlock`**: `PC_CHEAT_ACTIVE` (`cheatscr.c`, a macro at file scope) on the four reads of a cheat's
  flag, the switch's test and printOption's three: `pc_cheat_active(flag)` is 1 while the setting is on, else the
  flag; on the N64 the original tokens. The code check is left as it is, so a code entered still sets its cheat's
  flag, as on the N64; the setting never writes the flags.
- **Turning the unlock off** leaves the cheats switched on, on: their game flags are the cartridge's, kept for the
  session. A cheat whose code wasn't entered then shows its state but is locked again, until the setting is on
  again, its code is entered or the game restarts. Switch cheats off before turning the setting off.
- **Tested**: `frontend.menu.live` (both items at once: slot 6 valid with the menu setting, the other slots as they
  were; a cheat's test 1 with the unlock whatever its flag, the flag itself without; Defaults off);
  `synth_boot_cheats_unlock` (`tools/port/synth/cheats_unlock.press`, both on): five down to CHEATS, A opens the
  cheat screen (0x8000000) without the main menu's code, eight down to cheat 8, the invisible track, and RIGHT
  switches it without its code; B, START and a single race on track 0's detail world: the exit's scene line has none
  of the world's parts let through (`detail_check.py --world 0`; `detail.press` has four), the cheat hiding the
  track; `synth_boot_cheats_menu` (the same presses with the menu setting alone): the cheat screen opens, RIGHT
  leaves cheat 8 locked, and the race draws the four parts; `synth_boot_cheats_hidden` (neither): the same five down
  come round to the race and A goes to the player screen, never the cheat screen. On the retail cartridge on the Pi
  (headless, `--fast`, the same presses): with the menu setting the main menu has CHEATS below VIDEO and A opens the
  cheat screen, without it the five down come round to the first option; with the unlock every cheat's value is
  drawn bright, RIGHT on the invisible track switches it on and the race shows no track (the scene line lets none
  of its parts through); without it the values are dim, RIGHT leaves it off and the race draws the track.

### Presets (N64 original, PC enhanced, PC maximum)

2026-11-04 (`port/config/presets.c`). The defaults are the N64's game, so the PC's settings are spread over the Video,
Audio and Game pages. A preset sets several in one step: F1 > Presets, or `--preset NAME` (`n64`, `enhanced`,
`maximum`) for a run or a desktop shortcut.

| Setting | `n64`: N64 original | `enhanced`: PC enhanced | `maximum`: PC maximum | Applies |
|---|---|---|---|---|
| `game.frame_rate` | original | 60 | 60 | from the next screen |
| `video.draw_distance` | original | far | max | from the game's next frame |
| `video.detail` | original | high | high | from the game's next frame (split screen's models from the next track) |
| `video.fog` | original | | off | from the game's next frame (the sky dome from the next track) |
| `video.texture_lod` | original | anisotropic | anisotropic | from the next triangle |
| `video.textures` | original | | upscaled | from the game's next frame (upscaled in the background as they appear) |
| `video.widescreen` | auto | auto | auto | from the game's next frame |
| `audio.voices` | original | 64 | 64 | after a restart |
| `audio.engines` | original | all | all | at once (the sound manager's next retrace) |
| `game.memory` | original | 64 | 64 | after a restart |

An empty cell is a setting the preset leaves as it is.

- **`n64`** puts every setting the other two set back to its default: the N64's game. Widescreen's default is `auto`,
  the window's shape, so a 4:3 window is the N64's 4:3 picture; `video.widescreen = off` keeps 4:3 in any window.
- **`enhanced`**, the recommended one: the game at 60 (its menus, HUD, cameras and effects; the car physics is the
  same), everything drawn twice as far and at its most detailed level, the far road sharp, and every drone's engine
  heard in a race of 12 (`engines = all` needs 64 voices, so both are in). 64 MiB gives the game's heap room past the
  N64's, for mods' bigger files and a race's cars, at no cost to the picture or the sound. The fog stays the player's:
  it is the track select's fog option (`video.fog = far` thins it, [DETAIL.md](DETAIL.md) 5.3); the textures stay the
  N64's art.
- **`maximum`**: `enhanced` with the whole track drawn and no fog, which would hide it ([DETAIL.md](DETAIL.md) 5.3:
  "max with fog off"), and the world's textures upscaled. Voices stay 64: at 96 a race of 12 plays no more
  ([AUDIO.md](AUDIO.md) 10.6's tables: nothing held back at 64); memory stays 64 MiB, which holds the largest files
  tried ([MEMORY.md](MEMORY.md) 5: a peak of 18.9 MiB).
- **Left out**, by every preset:
  - `game.cars`: it changes the races, not how they look or sound (and a race holds more cars only when the track
    select's Drones option asks for them).
  - the display's and the machine's own: `video.scale` (native, the default, is already the window's pixels),
    `width`/`height`, `fullscreen`, `fullscreen_mode`, `resolution`, `vsync`, `video.frame_rate` (the right value is
    the display's refresh: `display`), `api`, `audio.backend`, `volume`, `latency`.
  - the player's own choices and content: `video.hud` (the default, `edges`, is already the PC's), `texture_pack`,
    the mods, the saves, the accessories, the bindings and the stick.
- **In the overlay** (F1 > Presets, the first of the main page's actions, before Save): choosing one sets each of its
  settings as if the player had changed it on its page: it applies at once where that setting does, the pages show it
  changed (yellow), Save counts it and writes it, and the restart ones keep their `*` on their pages. The message says
  how many changed and how many need a restart ("PC enhanced: 7 changed, 2 after a restart (*)"; the log names them);
  a preset whose settings all have its values already shows "in effect", and choosing it again says so.
- **Not a setting of its own.** `rush2049pc.ini` holds the settings themselves, never a preset: a `[game] preset` would
  be a second place for the same values (which wins, the preset or the file's `draw_distance` line?), and Save would
  have to choose. The default is the same as before, every setting at its own value; with no preset chosen nothing
  changes.
- **`--preset NAME`** puts the preset's settings on the command line at its place, in order with the other options: a
  later `--set video.fog=far` wins, an earlier one loses. Like every option, it isn't saved (Save writes only what was
  changed in the overlay), so a desktop shortcut with `--preset enhanced` runs enhanced and leaves the file alone. An
  unknown name is an error (exit 2).

### Environment variables

Every variable the program (`port/`) reads. The settings' ten are a layer of their own (above); the rest are
environment-only.

| Variable | What |
|---|---|
| `RUSH2049_AUDIO`, `RUSH_GFX_SCALE`, `RUSH_GFX_API`, `RUSH_GFX_WIDESCREEN`, `RUSH_GFX_FILL`, `RUSH_GFX_FRAME_RATE`, `RUSH_GFX_TEXTURES`, `RUSH_GFX_HUD`, `RUSH_GFX_DRAW_DISTANCE`, `RUSH_GFX_FOG`, `RUSH_GFX_TEXTURE_LOD` | `audio.backend`, `video.scale`, `video.api`, `video.widescreen`, `video.fill`, `video.frame_rate`, `video.textures`, `video.hud`, `video.draw_distance`, `video.fog`, `video.texture_lod` |
| `RUSH_GFX_TEXTURE_PACK`, `RUSH_GFX_TEXTURE_DUMP`, `RUSH_GFX_TEXTURE_CACHE` | the pack's, the dump's and the upscaled textures' cache's folders, over `video.texture_pack`, `video.texture_dump` and the data folder's `texture_cache` ([TEXTURES.md](TEXTURES.md)) |
| `RUSH_GFX_UPSCALE=1\|2\|4`, `RUSH_GFX_UPSCALE_2D=1`, `RUSH_GFX_TEXTURE_SYNC=1` | the upscale factor instead of the internal scale's; 2D upscaled too; upscaling on the render thread (tests) ([TEXTURES.md](TEXTURES.md)) |
| `RUSH_GFX_HEIGHT`, `RUSH_GFX_VERBOSE`, `RUSH_GFX_TIMES`, `RUSH_GFX_TRACE`, `RUSH_GFX_DUMP`, `RUSH_GFX_HFR_DUMP` | the renderer's diagnostics ([GFX.md](GFX.md) "Backend, frame and settings", "High frame rate") |
| `RUSH2049_DUMP_AUDIO`, `RUSH2049_AUDIO_RATE`, `RUSH2049_AUDIO_STATS`, `RUSH2049_ALSA_DEVICE`, `RUSH2049_AUDIO_CAPTURE` | a WAV of the output (`--dump-audio`), the device's rate, the output's counters every N s, the ALSA device, every audio task to a file ([AUDIO.md](AUDIO.md)) |
| `RUSH2049_CONSOLE=none`, `RUSH2049_MESSAGE_BOX=print\|off`, `RUSH2049_WAIT_KEY=0\|1`, `RUSH2049_TEST_ROM_PICK` | the console, the message boxes, the wait for a key and the ROM's file dialog, for tests ([RELEASE.md](RELEASE.md)) |
| `RUSH_SIM_TIMER_STEP_US`, `RUSH_SIM_VSYNC_HZ`, `RUSH_SIM_PRESENT_MS` | Windows' timing modelled on Linux ([WINDOWS.md](WINDOWS.md) 5) |
| `RUSH_GFX_TEXTURE_MODS` | the mods' texture pack folders, one a line, first first: main sets it from the mods in use ([MODS.md](MODS.md) 2) |
| `RUSH_SIM_GFX_MS` | with `--fast`, each graphics task holds the RDP that many ms of the N64 clock: a host too slow for the game at 60 (`fps_slow_host`, [FRAMERATE.md](FRAMERATE.md) 7.7) |
| `RUSH_STATE_EXACT`, `RUSH_TRAP_FP`, `RUSH_TRACE_WAVES`, `PC_DATA_REPORT` | the state log's exact floats ([ULTRA.md](ULTRA.md) 4), a trap at the first invalid float operation, each wave's conversion logged ([LOADERS.md](LOADERS.md)), the data loader's report ([RELEASE.md](RELEASE.md) "Files") |
| `APPDATA` (Windows); `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `HOME` | the settings and data folders |
| `DISPLAY`, `WAYLAND_DISPLAY` | on Linux, without either the program renders headless unless `--window` is given |
| `XDG_CURRENT_DESKTOP`, `PATH` | the ROM's file dialog: kdialog first on KDE, zenity or kdialog found on `PATH` |
| `SDL_JOYSTICK_RAWINPUT=1` | SDL's own: on Windows, SDL's raw input for Xbox pads instead of XInput ("Windows" above) |

## The settings overlay

Phase 8 of [PORT_PLAN.md](../PORT_PLAN.md), 2026-10-27 (`port/input/menu.c`). It changes the PC settings in the
game, without editing the file. `frontend_menu_test pictures DIR` writes each of its pages as a PNG, over a test
frame, to look at.

- **Opening it.** F1 (`hotkeys.menu`), both stick clicks held together on a controller (`controller.menu`), or
  `MENU` in a `--press` script. The same closes it, and so do Escape or B on the main page, Start, and Resume.
  - `controller.menu` is a button or axis name, or several held together, joined by `+` (`guide`, `back+start`).
    A single input there is not also a game button; a combination's parts still are.
  - Both stick clicks are the default because the game's bindings leave them free, and guide alone is often the
    system's (the Xbox Game Bar, Steam's overlay). Guide alone also opened the overlay in the controller test,
    which presses every button expecting nothing but its N64 button.
- **The game is held while it is open.** `pc_input_paused` is true, so the main loop holds exactly as for the pause
  key (`loop.c`'s `hold`): no retraces, OS timers, RSP tasks or AI events, so the N64 threads stop where they wait
  for them, and the N64 clock is set back afterwards. The window's events are still handled, and the overlay is
  drawn again over the last frame after each change (`pc_gfx_redraw`). The sound runs out and is silent. The
  title says "paused".
  - This needed no change in port/ultra: port/gfx's event pump, which `hold` calls every 1/60 s, gives port/input
    a turn (`pc_gfx_set_tick_hook`). That is where scripted input reaches the overlay, a held direction repeats,
    a capture times out, and it redraws.
- **Input while it is open.** The game's pads read idle, live and scripted. Keys and buttons go to the overlay.
  Alt+Enter and the fullscreen and screenshot hotkeys still work (a screenshot has the frame without the
  overlay); the pause and quit hotkeys don't, and Escape is the overlay's back key.
- **When it closes,** keys and controller buttons still held are ignored until they are released (sticks and
  triggers until they are back in their dead zone), so the key or button that closed it doesn't reach the game.
- **In an online session** ([NETPLAY.md](NETPLAY.md) 12.4, 12.10) nothing is held and this player's pads are idle;
  the settings the game reads are locked; the Online page starts with two rows, which can't be selected: the other
  PC ("Online: the guest, 100.101.102.104:41641") and the link ("RTT 23 ms, delay 2, stalls 1, hash ok at 1234"),
  read from the session each time the overlay is drawn (`pc_menu_set_online`); the main page's Online item shows "RTT
  23 ms, delay 2", its footer the other PC.

| | Keyboard | Controller | Mouse | Script |
|---|---|---|---|---|
| move (held, it repeats) | arrows | D-pad, left stick (after 0.4 s, every 0.1 s) | point at an item | UP DOWN, STICK* |
| change a value | Left, Right | D-pad left, right | click left or right of its middle (`<`, `>`); a bar where clicked | LEFT RIGHT |
| choose; set a binding | Enter, Space | A | click | A |
| back; cancel | Escape | B | right button, back button, a click outside the panel; any click cancels a binding's wait | B |
| clear a binding | Delete, Backspace | X | | Z |
| add to a binding | Insert | Y | | |
| a page up, down | Page Up, Page Down | LB, RB | the wheel, the list's `^` `v` (3 rows) | L, R |
| close | F1 | the menu button, Start | a click outside on the first page | MENU, START |

**What it covers.** Changes apply at once where the part allows it. `*` in the overlay marks those that need a
restart.

| Page | Item | Setting | Applies |
|---|---|---|---|
| Video | Render scale: native, N64 (1x), 2x-8x, auto (whole) | `video.scale` | from the next frame (`pc_gfx_set_scale`) |
| | Widescreen: auto, off, 16:10, 16:9, 21:9 | `video.widescreen` | from the game's next frame (`pc_gfx_set_widescreen`) |
| | Fill the window | `video.fill` | from the game's next frame (`pc_gfx_set_fill`) |
| | Fullscreen | `video.fullscreen` | at once; opening the overlay takes F11's state |
| | Fullscreen mode: desktop, exclusive | `video.fullscreen_mode` | at once when in fullscreen; F11 uses it |
| | Resolution: desktop, 720p-2160p | `video.resolution` | at once: exclusive fullscreen's display mode, desktop fullscreen's drawing size |
| | Window size: 640x480 to 3840x2160, auto | `video.width`, `height` | at once when windowed, in pixels (`pc_gfx_set_window_pixels`); in fullscreen when it ends; auto from the next start; headless, the size the scales follow |
| | Vsync | `video.vsync` | at once (the swap interval) |
| | Frame rate: original, 60, 120, 144, display | `video.frame_rate` | at once (`pc_gfx_set_frame_rate`): frames between the game's from the next two it draws |
| | Pause in background | `video.pause_on_focus_loss` | at once |
| | Graphics API * | `video.api` | after a restart: the GL context and its shaders are made at start |
| | Textures: original, upscaled, pack | `video.textures` | from the game's next frame (`pc_gfx_set_textures`): the texture cache is made again; what was upscaled or loaded before is kept in memory ([TEXTURES.md](TEXTURES.md)) |
| | Race HUD (widescreen): screen edges, centered (4:3) | `video.hud` | from the game's next frame (`pc_gfx_set_hud`) |
| | Draw distance: original (N64), far (2x), max (all) | `video.draw_distance` | from the game's next frame (`pc_detail_set`, `pc_gfx_set_draw_distance`; [DETAIL.md](DETAIL.md)) |
| | Detail: original, high | `video.detail` | from the game's next frame; split screen's models and level objects from the next track |
| | Fog: original, far (2x), off | `video.fog` | from the game's next frame (`pc_gfx_set_fog`) |
| | Far textures: original (N64), sharp, anisotropic | `video.texture_lod` | from the next triangle (`pc_gfx_set_texture_lod`) |
| Audio | Volume, 0-100 in 5s | `audio.volume` | at once (`pc_audio_set_volume`) |
| | Output * | `audio.backend` | after a restart: `pc_audio_init` may not run while the game's threads call into port/audio |
| | Voices *: original (32), 48, 64, 96 | `audio.voices` | after a restart: MusyX is set up once a run (`pc_voices_set` at start; [AUDIO.md](AUDIO.md) 10.2) |
| | Drones' engines: original, nearest 2-8, all | `audio.engines` | at once: from the sound manager's next retrace (`pc_engines_set`) |
| Game | Cars in a race: original (6), 7-12 | `game.cars` | from the next race's start: the setting at once (`pc_race_cars_set`), the race's count when the game latches it ([above](#the-cars-a-race-holds-gamecars)) |
| | Memory *: original (N64), 16, 32, 64, 128, 256 MB | `game.memory` | after a restart: the RDRAM window is mapped at start ([MEMORY.md](MEMORY.md)) |
| | Game frame rate: original (30), 60 | `game.frame_rate` | from the next screen: the setting at once (`pc_game_fps_set`), the game's copy at its next change of state ([above](#the-games-frame-rate-gameframe_rate)) |
| | PC SETUP in menu: off, on | `game.pc_setup` | at once: the main menu's next frame (`pc_start_menu_set`) |
| | Ghost save prompt: off, on | `game.ghost_prompt` | at once: the next race's end (`pc_ghost_prompt_set`) |
| | Cheats menu: off, on | `game.cheats_menu` | at once: the main menu's next frame (`pc_cheats_menu_set`) |
| | Unlock all cheats: off, on | `game.cheats_unlock` | at once: the cheat screen's next frame (`pc_cheats_unlock_set`) |
| Mods | Mods *: on, off; then each mod found, in the order in effect (`1. Red Cars 1.2`): on, off (`on (unusable)` for one that can't be used); Left and Right turn it on and off, Enter or A picks it up and Up and Down move it, Enter puts it down; its footer: its author and files, or why it can't be used; the folder; Defaults (every mod on, mod.ini's order) | `game.mods`, `[mods]`, `game.mods_order` | after a restart: the files are read before the game's memory is mapped ([MODS.md](MODS.md) 1) |
| Saves | Player (P1-P4); Kept as *: pak (.mpk), files (a folder); with files the folder, the import folder's result at start, Import ghosts, Export to the .mpk, Open the saves folder (on a desktop), and each profile and ghost (on the pak, kept, pinned, import), each with its page: Pin to the pak / Unpin, Export (.r49ghost), Delete (asked twice); with pak what files gives | `game.saves`, `saves2`... | Kept as after a restart; the actions at once on the folder, and what is on the pak at the next swap ([SAVES.md](SAVES.md) 8) |
| Keyboard | the 18 N64 controls; Steering: instant, smooth; Steering time (ms), 0-1000 in 50s; the settings, quit, pause, screenshot and fullscreen keys; Defaults | `[keyboard]`, `input.keyboard_steering`, `input.keyboard_steer_time`, `[hotkeys]` | at once (`pc_input_apply`: a ramp under way goes on at the new pace) |
| Controllers | who plays each player and the N64 stick its controller gives, live (not choosable); the 18 N64 controls; the settings button; the N64 stick; dead zone; sensitivity; rumble; P1-P4 accessory: controller pak, rumble pak, auto (shown with what it gives now: "auto: rumble pak"), none; Defaults | `[controller]`, `input.deadzone`, `input.sensitivity`, `input.rumble`, `game.pak`, `game.pak2`... | at once; an accessory as a hot swap (the game sees the old one pulled at its next poll, then the new one) |
| Help | read-only: each N64 control, its keys (the keyboard's player) and its controller inputs in Xbox names (`a, righttrigger` is "A, RT", `-righty` "RS up", the N64 stick's own stick on the stick rows); the hotkeys (Alt+Enter with fullscreen); this menu's keys and the mouse | the bindings in effect, made again each time it opens | |
| About | read-only: the version (`git describe`) and build type, the commit, the build; GL vendor, renderer, version and GLSL (the log's `gfx:` line); the audio output and rate, its pulls and queue (the exit line's numbers); the memory (osMemSize) and the heap's peak so far ([MEMORY.md](MEMORY.md) 4); the mods in use ([MODS.md](MODS.md)); the game's frame rate and, after a race at 60, the share of its frames the game kept there and whether the host could keep 60 ([FRAMERATE.md](FRAMERATE.md) 7.7); players 1-4 (live); the ROM and where it was found; the settings file, saves, screenshots, log and data folder; **Open the data folder** (`SDL_OpenURL`: ShellExecute on Windows, xdg-open on Linux; only on a real display); **Copy to the clipboard** (the page as text, for a bug report; with a window) | main's facts (`pc_menu_set_about`), made each time it opens | |
| Presets | N64 original, PC enhanced, PC maximum, each "in effect" when its settings all have its values; Enter, A or a click sets its settings | the [presets'](#presets-n64-original-pc-enhanced-pc-maximum) settings | each as its own item applies it: the live ones at once, `game.frame_rate` from the next screen, `audio.voices` and `game.memory` after a restart |
| Online | Host a race (Port and Join code typed in, Host now), Join a race (Address and Join code typed in, Join now, the hosts joined before), Watch a replay (the last session's, every input log and its own page: Watch, Keep this replay); this PC's addresses, each copied by Enter, and Copy my address; Stop watching in a replay; in a session its two status rows and Leave the session (asked twice) instead of Host, Join and Watch | `net.port` and `net.code` the fields' first values; `net.recent` the hosts joined before ([NETPLAY.md](NETPLAY.md) 13) | Host now, Join now, Watch, Stop watching and Leave restart the program into the session or the replay, or out of it (lockstep starts at boot); nothing before one of them |
| (main) | Video, Audio, Keyboard, Controllers, Game, Mods, Saves, Online, Help, About; Presets; Save to rush2049pc.ini; Resume; Quit | | |

- A value that isn't one of an item's choices (a 2.39 aspect, an 800x600 window) is shown as it is; Left and Right
  then go to either end of the list.
- Changed values that aren't saved yet are yellow, and the Save item counts them (yellow too). Once saved it says
  "saved" in green, and the message names the file ("Saved 4 settings to rush2049pc.ini"); closing the overlay
  with changes not saved says how many in the log.
- **The footer's first line** says what the selected item does ("Internal resolution; native: the window's pixels"),
  in at most 49 characters; a binding gives its whole list ("N64 A: X, Space"), Save the file's path, an About row
  its whole value (a long path over both footer lines, its end kept), a Help row both bindings. A setting without a
  line of its own (one added later) shows its name, `video.key in rush2049pc.ini`. A message (a capture's result,
  Saved) takes the line until the next key, button or click, or until the mouse points at another item 1.5 s on.
- The pages' order: the settings' pages, then Help and About, then the actions (Presets first, then Save); on each
  settings page the settings, then the hotkeys or the settings button, then Defaults last; Help in the binding pages'
  order.
- **Bindings.** Enter or A waits for the next key (Keyboard page), or controller button, stick direction or
  trigger pushed past 0.6 (Controllers page), which replaces the binding. Insert or Y adds it to the binding
  instead; Delete or X clears it. Escape cancels, as do B for a key and the menu button for a controller input, and
  so does waiting 6 s. The name is written as the file takes it: SDL's names, punctuation by its alias (`Comma`).
  A key that is a hotkey, or already bound to another control, is noted at the bottom. Defaults puts the page's
  settings back to the defaults.
- **One controller mapping.** rush2049pc.ini has one `[controller]` section, which every controller uses, so the
  Controllers page edits that one; any controller can set it.
- **Save** writes the settings changed in the overlay, where they differ from the file, into the file in place
  (`pc_settings_save`, `pc_ini_update` in port/config):
  - each one's line keeps its key, its spacing and its inline comment, and only the value changes; a setting the
    file lacks goes after the last setting of its section, and a section it lacks goes at the end;
  - every other line (comments, blank lines, the order, unknown keys, bad lines, CRLF line ends, a BOM) stays byte
    for byte;
  - settings that came from the command line or the environment and weren't changed in the overlay are not
    written;
  - the file is the one read, the `--config` path, or, with `--no-config` or no file yet, the located one
    (`PcSettings.save_path`). A file that doesn't exist yet is written whole, as on the first run, with the
    changes. Through a temp file and rename, as before.
- **Drawing** (port/gfx, [GFX.md](GFX.md) "The overlay"): after the game's frame, in window pixels, so it is the
  same at every internal scale and in widescreen. A panel of 304 x 224 units in the middle of the window, a unit
  being the largest whole number of pixels that fits 320 x 240 units in the window: 2 pixels at 640 x 480, 4 at
  1280 x 960 and at 1920 x 1080, 6 at 2880 x 1620. The window's size is the drawable's, in pixels, so a high-DPI
  display gets a sharp panel of the same proportion. The frame around it is dimmed and the panel is semi-transparent (alpha 0.78). A
  title bar, up to 15 items with the selected one highlighted (the list scrolls, `^` and `v` at its right edge
  when there is more), the selected value between `<` and `>`, and at the bottom the selected item's line (or a
  message) and the keys. The text is the port's own 5 x 9 font (`gfx_font.c`).

### The mouse

`input_sdl.c` passes the mouse to the overlay while it is open (`pc_menu_mouse`), and to nothing else.

- **Where it points.** SDL's mouse events are in the window system's units. `pc_menu_mouse_pixels` turns them into
  the drawable's pixels by the drawable's size against the window's (`SDL_GL_GetDrawableSize`,
  `SDL_GetWindowSize`): the same on Windows, which is per-monitor DPI aware (a 200% 2880 × 1620 desktop is 2880 ×
  1620 of both), and on X11; 2 pixels a unit on a 2x macOS or Wayland output, 1.5 at 1.5x. The middle of the unit
  is taken, so at 2x a point lands on the second of its two pixels.
- **What is there.** `pc_menu_hit` uses the drawing's own layout (the unit, the panel's corner, the rows), so the
  hit test and the picture can't disagree at any window size, in fullscreen (desktop or exclusive: the window is the
  screen's or the mode's size), at a fullscreen resolution (the overlay is drawn at the window's pixels, not the
  frame's) or in widescreen (the panel stays in the middle of the window).
- **Pointing** selects the item under it, except the player rows and Help's headings, and not while a binding
  waits for its input (the input goes to the item chosen). Only a move selects: a mouse resting where a page puts
  another item doesn't, and the keys and buttons still move the selection as before.
- **Clicks.** Left: choose (as Enter); on a choice or a number, left of the value's middle is the previous one and
  right of it the next; on the volume's and rumble's bar, the value where it is clicked. Right, or the back
  button (X1), or a click outside the panel: back, and close on the first page. Any click while a binding waits
  cancels the wait. The second press of a double click (within 0.5 s, Windows' double-click time) on the page the
  first one opened or went back to is ignored, so a double click on Video doesn't also change its first setting.
- **The wheel** scrolls a long page (Keyboard, Controllers, Help, About) by 3 rows a notch; the selection stays on
  screen, and when the list moved the item now under the mouse is selected. It never changes a value. The list's
  `^` and `v` scroll the same way when clicked.
- **The cursor** shows while the overlay is open. During play it is hidden from the start and shows only when the
  mouse moves, for 2 s after it stops; after the overlay closes it stays 2 s and hides. An event at the same place
  isn't a move, nor is SDL's first one, which has no relative motion: Windows sends it when the window opens or
  gets the focus under a cursor at rest (Windows CI caught the cursor showing at start). Times are port/ultra's
  clock (`pc_host_ns`), which the hold sets back; a time from before is taken as now.
- **The focusing click.** A click within 250 ms after the window gets the focus back is the one that gave it the
  focus (Windows and most window managers pass it on), so it doesn't act on the overlay.
- **Never the game.** Mouse events reach no pad; a click while the overlay is closed does nothing at all.

## Building with SDL2

```
cmake -S . -B build/pc -G Ninja                              # SDL2 if installed (libsdl2-dev), else none
cmake -S . -B build/pc-sdl -G Ninja -DRUSH_FETCH_SDL2=ON     # download SDL2 2.30.9 (SHA-256 pinned), static
cmake --build build/pc-sdl -j3 && ctest --test-dir build/pc-sdl
```

- **One SDL2.** The top-level `CMakeLists.txt` finds an installed SDL2 (its config package, then pkg-config).
  With `RUSH_FETCH_SDL2=ON`, it fetches the pinned release once and builds it statically and position
  independent. That takes about 50 s on the Pi 5. It sets `RUSH_SDL2_TARGET`. port/gfx (the window), port/audio
  (the `sdl` backend) and pcfrontend (input) all link it.
- **Turning it off.** `-DRUSH_SDL2=OFF` ignores an installed SDL2. Each part has its own off switch too:
  `RUSH_GFX_SDL`, `PC_AUDIO_SDL2`.
- **Built alone.** `cmake -S port/gfx` and `cmake -S port/audio` still do their own lookup.
- **Without SDL2,** everything still builds and its tests pass: headless EGL, null/WAV (or ALSA) audio, and
  scripted input only. `pc_gfx_set_event_hook` then reports no window, and port/input's live half is no-ops.
- **What the fetched SDL2 has on this Pi.** Without X11, Wayland or gbm development headers, its video drivers
  are offscreen and dummy only. That is enough for the tests, but not for a window on a real display. Install
  those headers (or `libsdl2-dev`) for one.

## Tests

`ctest --test-dir build/pc -R frontend` runs these.

| CTest | Program | Checks |
|---|---|---|
| `frontend.ini` | `frontend_test ini` | the INI reader: BOM, CRLF, comments, inline comments, quotes, case, a key before any section, a missing final newline, and four kinds of malformed line with their line numbers |
| `frontend.settings` | `frontend_test settings` | the defaults; every type and its range checks (a bad value leaves the setting alone); the widescreen aspect (W:H, ratios, auto, off, written back as 16:9 / 16:10 / 21:9, narrower than 4:3 refused); `scale = auto`; bindings, unbound, `analog`, hotkeys; `--set` syntax; the written file reads back to the same text, quoted strings included; bad lines are counted and skipped |
| `frontend.layers` | `frontend_test layers` | the file over the defaults, the environment over the file (a bad variable is ignored), overrides over both in order, a bad override fails but the rest apply, no file, a missing `--config` file |
| `frontend.locate` | `frontend_test locate` | XDG, a relative XDG (ignored), HOME, no place at all; the first run writes the defaults (making directories) and reads them back; edits are read next time; READ never creates; a file next to the executable wins |
| `frontend.stick` | `frontend_test stick` | dead zone (radial), rescaling, full tilt in four directions, the octagon's corner and edge (22.5°: (76, 31)), a square pad's corner, the round gate, keys, monotonic travel |
| `frontend.steer` | `frontend_test steer` | smooth keyboard steering's ramp (`pc_steer_rates`, `pc_steer_step`) at 250 ms and 60 Hz: 1/15 out and 0.2 back a retrace; a key held: 5, 11, 16, 21, 27, 32, 37, 43, 48, 53, 59, 64, 69, 75, 80 (full at the 15th retrace) and held there; released: 64, 48, 32, 16, 0 and centred after; left the mirror (-80 at the 15th); full left to full right in 20 retraces (5 back, 15 out); a reversal 5 retraces out (27): 11, then -1/45 (-2: the rest of the retrace past the centre), -7; both axes to the octagon's corner (70, 70); 0 ms full tilt, a reversal and the centre each in one retrace; 1000 ms 60 out, 20 back |
| `frontend.precedence` | `rush2049pc --print-config` | the real program: the file's ROM, `RUSH2049_AUDIO` over the file, `--scale` over `RUSH_GFX_SCALE` over the file, a `--set` binding and `--set game.cars=12` |
| `frontend.memory` | `frontend_test memory` | `game.memory`: `original` the default (written commented out, with its comment), 16-256 each set and read back, 15, 8, 4, 0, -1, 257, 512, 1024, `64M`, words and an empty value refused with the range in the message, any case, `--set` syntax, the file read back, a bad line skipped; the warning only for `expansion_pak = off` with a size |
| `frontend.fps` | `frontend_test fps` | `game.frame_rate`: `original` the default (written commented out), 60 set and read back, `video.frame_rate` left alone, 30, 59, 61, 120, 0, -1, words, `display` and an empty value refused, any case, `--set` syntax, a bad line skipped; the latch: original before anything, the setting at once and the game's copy only at `pc_game_fps_begin()`, original mid-screen leaving 60 until the next; the frame's steps: a step a frame at original whatever its length, at 60 frames of one retrace half a 30 Hz frame with a whole step every other one, of two one, of three 1.5 (three steps in two), the filter e^frames; the race watch (`pc_game_fps_state`, `pc_game_fps_describe`): the countdown and a race at original not counted, a race kept at 60 (1.00), 4 s at 2/3 of 60 the host can't keep (the race's share 0.83), the share kept after the race and with the setting back at original, a new race counted afresh; `pc_fps_noise` (the in-car bob's noise kept to its spread: √(0.25/0.4375) over a retrace, 1 over two and at original) |
| `frontend.mods` | `frontend_test mods` | `game.mods`, `mods_dir`, `mods_order` and `[mods]`: the defaults (written commented out, an empty `[mods]` with its comment), each set and read back, the flags any case and set again, bad values refused, the file read back, a bad line skipped ([MODS.md](MODS.md) 8) |
| `frontend.menu.mods` | `frontend_menu_test mods` | the Mods page from mocked mods: after Game, the order from `game.mods_order` then mod.ini's priority, a mod that can't be used, the folder; Left and Right, a mod picked up and moved to the top, Save writing `game.mods_order` and `[mods]`, Defaults |
| `frontend.voices` | `frontend_test voices` | `audio.voices`: `original` the default (written commented out, as `engines`), 48, 64, 96 set and read back, 32, 33, 128, 0, -1, words and an empty value refused, `--set` syntax, the file read back; at run time: original changes neither set-up, each size gives the Expansion Pak's set-up its voices, the effects all but 16 and 128 emitters and leaves the set-up without the pak alone, past the storage capped at 96, 0 original |
| `frontend.engines` | `frontend_test engines` | `audio.engines`: every choice set and read back, `nearest`, `nearest5`, `nearest 4`, `4`, `none` refused; the choice on 11 drones' engine emitters: original chooses nothing and leaves every gain, nearest4 drones 1-4 with their gains whole and drone 5's silent, the humans' handles untouched; a drone 330 ft away against the fourth's 400 kept out (not nearer by a fifth), at 310 in, fading in over 15 passes as the fourth fades out, then held against 300; two listeners each with its four nearest, back to one; all every drone; the budget: all's engines never over and not counted against the effects' rank, nearest and original as the game's (`rank >= budget`) |
| `frontend.cars` | `frontend_test cars` | `game.cars`: `original` the default (written commented out), 6-12 each set and read back, 5, 13, 0, -1, 99, words and an empty value refused with the range in the message and the setting kept, any case, `--set` syntax, the file read back, a bad line skipped; the latch: 6 before anything, the setting at once and the race's count only at `pc_race_cars_begin()`, a change and `original` mid-race leaving the race's count alone, the next race taking it, out-of-range values taken to 6 and 12 |
| `frontend.saves` | `frontend_test saves` | `game.saves`, `saves2-4` and their folders: `pak` the default (written commented out, with its comment), `files` set and read back for each player, a bad value and an empty one refused with the choices named and the value kept, any case, `--set` (a folder with a space), the file read back, a bad line skipped and the next taken |
| `frontend.menu.saves` | `frontend_menu_test saves` | the Saves page over a mock of main's side: player 1 with files (the folder, a file refused at start, Import ghosts, Export to the .mpk, no Open headless, a profile on the pak, a ghost kept, an imported ghost on the pak), a ghost's page (kind, player, track and way, time; Pin then Unpin, main's message in the footer; Export; Delete asked once, then done, back on the list), Import and Export to the .mpk from the list, player 2 with the `.mpk` (no controller pak now, its `.mpk`, what files gives), Kept as changed from the page as a setting, Left back to player 1 |
| `frontend.detail` | `frontend_test detail` | `video.draw_distance`, `detail`, `fog`: `original` the defaults (written commented out), every value set and read back, a bad value refused with the choices named and the last value kept, `--set` in any case, the file read back, `RUSH_GFX_DRAW_DISTANCE` and `RUSH_GFX_FOG` as a layer (a bad one refused); what the game reads (port/config/detail.c): the N64's distances and the matrices' (-2048, 2048) at original, x 2 and a halved matrix to 4,096 ft at far, no limit, the first level, one player's models and any range through a power of two at max with high, unknown levels taken as original |
| `frontend.presets` | `frontend_test presets` | the presets' table (`port/config/presets.c`): three, found by name in any case and no other name; every value one the settings take, no setting twice, the footer's line in 49 characters; `n64` sets every setting the others set, each to its default, and is the one in effect over the defaults; what `enhanced` and `maximum` set (60, far or max, high, fog off for maximum, anisotropic, upscaled for maximum, auto, 64 voices, all, 64 MiB) and leave (the cars, the HUD, the scale, the display's frame rate, vsync, fullscreen, the window, the API, the volume); in effect once set and not after one setting changes; `n64` after `maximum` the defaults' whole file; widescreen off back to auto |
| `frontend.preset_option` | `rush2049pc --preset enhanced --set video.detail=original --print-config` | the real program over a file with scale 2: the preset's settings in effect, the file's scale kept, the `--set` after it winning (detail original, written commented out), widescreen auto, fog and the textures and the cars untouched |
| `frontend.menu.presets` | `frontend_menu_test presets` | the Presets page over a file with `fog = far`: after About on the main page; none in effect; PC enhanced: "in effect", "7 changed, 2 after a restart (*)", each setting's value, the fog and the rest untouched, the renderer's and the game's draw distance, detail, far textures, fog and engines at once, the game's frame rate 60 as a setting and 30 until `pc_game_fps_begin()`, the library's 32 voices kept; Save "7 changes"; the Audio and Video pages show 64 and far; PC maximum over it: 3 changed, max, fog off and upscaled at once, enhanced no longer in effect, again "in effect already"; Save writes 9 (the file's fog line in place, its comments kept, no widescreen, no preset); the file read back is PC maximum; N64 original: 9 changed, 2 after a restart, the N64's at once, every setting the default, 9 to save |
| `frontend.sdl.keyboard` | `frontend_sdl_test keyboard` | every default key, held keys, the stick from keys (cardinal, the corner, opposite keys); `keyboard_steering = smooth` set live (`pc_input_apply`), from key events a poll at a time: Right 5 at the first retrace, 75 at the 14th, 80 at the 15th; Up on top 5 (79, 5 at the gate's edge) and back; Right and Left held back to the centre (64), Left alone through it (0 after 5 retraces back, then -5), a live change of `stick_range` mid-ramp keeping the ramp (-13 at 100), released; `instant` again full tilt at once; rebinding: two keys, an alias, an unknown name skipped, the keyboard on player 2, a round gate of 100 |
| `frontend.sdl.hotkeys` | `... hotkeys` | title and minimum size; Alt+Enter toggles fullscreen and is not START; F11 back; key repeat ignored; F12 saves PNGs of the shown frame (two in one second get two names); Pause holds and the exposed window is redrawn; Escape quits and isn't a game key; rebound and cleared hotkeys; closing the window quits; fullscreen from the settings |
| `frontend.sdl.focus` | `... focus` | focus loss releases held keys; no hold by default; with `pause_on_focus_loss` it holds and focus back releases it; the pause key and focus are separate reasons |
| `frontend.sdl.controller` | `... controller` | a virtual XInput-style pad: every default button (guide and the stick clicks unbound), triggers and right-stick directions with their threshold, the left stick (dead zone, rescaled travel, full tilt, the diagonal, +y up), merging with the keyboard; rebinding: the right stick as the N64 stick, a bigger dead zone, buttons on Z and on the stick, bad names skipped |
| `frontend.sdl.generic` | `... generic` | a virtual joystick without a mapping: no player before a button press (nor from its stick), its first button takes player 2 and counts; axis 1 the stick's x, axis 2 its y (up), hat 1 the D-pad, buttons 3 and 8 x (B) and start, a trigger axis at rest either way released, axis 6 the right trigger; a joystick with one axis and no hat isn't used, even with a button |
| `input_live_drive` | `tools/port/input_check.py` | the synthetic cartridge through SDL, headless ([below](#testing-live-input)): `drive` with `--press-via controller`, `joystick`, `keyboard` and `controller:2` each the scripted run line for line, the car's steering input +1 and -1 while the stick is pushed and 0 without it; with `--rumble` the game's motor rumbles the virtual controller at full strength; `race2p` with two virtual controllers, players 1 and 2; then with a rumble pak on player 1 and player 2 on auto, each player's motor rumbles its own controller at full strength |
| `frontend.sdl.hotplug` | `... hotplug` | six pads plugged and unplugged: players filled in order, kept while others leave, a waiting pad takes a freed player, ports disconnect when unplugged; `first_controller_player = 2`; a pad present before input starts |
| `frontend.sdl.rumble` | `... rumble` | the motor drives the pad's rumble callback at full strength, only on that player; a steady rumble outlives SDL's 1 s duration; the fade over 4 retraces; a pulsed motor stays at least half; strength from the settings; another player's motor; stopped at shutdown; strength 0 |
| `frontend.sdl.script` | `... script` | `--press` merged over live input: buttons OR'ed, a scripted stick axis replaces the live one, a scripted port connected; `--log-input` runs |
| `frontend.menu.nav` | `frontend_menu_test nav` | the overlay, headless: opening holds the game and its pads read idle; the panel shows over the frame (the frame dimmed around it, the selected row's color); moving and wrapping, pages, a long page scrolled by a page, back, close; Help opens past its heading, its rows don't change, up wraps past the heading; About without main's facts; closed, the window is the frame again byte for byte, and so is the next frame; each page keeps its place; Quit stops the main loop |
| `frontend.menu.mouse` | `... mouse` | hit-testing at ten window and drawable sizes (640 × 480, 1080p, a 200% DPI-aware 2880 × 1620, 2x points, 1.5x, 1.25x square, 853 × 480, 21:9 3440 × 1440, 320 × 240, 1366 × 768): every window unit lands in its own pixels; rows 0, 7 and 14 from the window's units; the panel's edges and corners to the pixel; a row's first and last pixel lines; the footer; the scroll arrow; a value's label, left and right parts. Then by mouse at 640 × 480: pointing selects and the highlight is drawn there; clicks right and left of a value, on a label, on the title (nothing); the right button; a double click on Video (opens it, the second press changes nothing; a click 0.6 s later does); the volume's bar at its middle, end and start and right of it; the wheel (3 rows, the item under the mouse selected, at most to the last page); the up arrow; a binding's wait (pointing doesn't move it, a click cancels); at 2x points into a 1280 × 960 window, the row and its highlight; outside: back, then closed; closed, clicks don't open it nor reach the pads |
| `frontend.menu.pages` | `... pages` | About's content (`pc_menu_about_text`) from mocked main facts: version and build type, commit, build, the GL driver's own strings, audio and buffer, the memory, the game frame rate (original), four players (none headless), ROM, settings file, saves, screenshots, log, data folder; made again each time it opens (main's fill called each time); no Open or Copy headless. Help from the bindings in effect: two keys, Xbox names, the right stick as the N64 stick, a combination as the settings button, Alt+Enter with fullscreen, a binding cleared on the Keyboard page shown on return |
| `frontend.menu.live` | `... live` | widescreen 16:9 and scale 2 give the next frame a 426 x 240 and then 852 x 480 render target; the frame rate reaches port/gfx (60, then the game's own); the race HUD reaches it (centered, then edges); the window size follows (960 x 720); the volume reaches port/audio (50: 0.5); the API and the backend change only the settings; Voices from original (32) to 48 and 64, the setting only (the library's voices at the next start), and back; Drones' engines from original to nearest 4, at once (`pc_engines`), round past original to all and back to original; a key binding captured, added to, cancelled and cleared; a controller binding captured as a stick direction; a capture times out after 6 s; player 2's accessory from auto ("auto: none") to rumble pak and controller pak, each swapped at once (`pc_si_set_pak`), and back to auto, player 1's untouched; the Game page: Cars in a race from original (6) to 7 and 12, the setting at once and the race's count kept until `pc_race_cars_begin()`, past 12 to original and back, a change during a race leaving its count; Memory from original to 64 and 256 MB and round again (the setting only: it applies at the next start); the Game frame rate from original (30) to 60 and back, the setting at once and the game's copy kept until `pc_game_fps_begin()`; PC SETUP in menu, Ghost save prompt, Cheats menu and Unlock all cheats off to on, each at once (the main menu's slots 8 and 6, the ghost prompt's flag, a cheat's flag); Defaults putting the seven back; the Keyboard page's Steering after the stick's keys, instant to smooth, Steering time (ms) 250, 300, down to 0 and up to 1000 in 50s, the page's Defaults putting instant and 250 back |
| `frontend.menu.save` | `... save` | `pc_ini_update`: a value replaced in place (key case, spacing, the inline comment kept), the last of two lines, a new key after its section's last, an empty section, a new section after a last line without its end, CRLF, a binding, quoting, a BOM and bad lines kept, no changes; the overlay's Save over a hand-written file: the expected text exactly, a setting changed and back not written, the command line's scale not written, "saved" and the message naming the file, nothing more to save, a change after it counted, changed back "saved" again, the file reads back; a missing file and its directories, written whole |
| `frontend.menu.script` | `... script` | a `--press` script over time: MENU opens it, a press held across the opening doesn't act, the game's pads are idle meanwhile, DOWN, A, LEFT (the volume), B, the stick, START closes; scripted game input works after |
| `frontend.sdl.menu` | `frontend_sdl_test menu` | the overlay through the real window: F1 opens it, the game's held key released and its input suppressed, the title; (not on Windows) the panel in the window; arrows, Enter, a key captured (Space; Comma by its alias); Escape goes back and doesn't quit; the game then has the new keys; Enter that closed it and its key repeat aren't START until released; a virtual pad: one stick click doesn't open it, both do; the D-pad, A, a button and a stick direction captured, B, Start closes it and stays ignored until released, Y now A; guide as the menu button opens and closes it and isn't a game button; fullscreen from the overlay at once; F11 works in it and the overlay follows |
| `frontend.menu.online` | `frontend_menu_test online` | the Online page over a mock of main's side ([NETPLAY.md](NETPLAY.md) 13.5): its items, the Tailscale name joining it, Copy without a clipboard; the port and the code typed in (Enter, Escape, Backspace, Down, digits only, five at most), a port that isn't one; Host now calling main and stopping the loop; twelve addresses that aren't one, each with its sentence, one trimmed and joined; the recent hosts chosen; Watch the last session's, a log of another build, Keep; Stop watching; Leave asked twice in a session; the overlay's changes not saved, for the restart |
| `frontend.menu.relaunch` | `... relaunch` | `port/config/online.c` alone: the restart's command line (a session's, a record's and the scripts' options out, the ROM, `--set` changes, `--net-return` and what follows it the restart's own), Windows' quoting of 14 hard arguments parsed back by the C runtime's rules, the log's shell quoting, `net.recent` (one host once, 8 kept), the address and port checks |
| `frontend.sdl.text` | `frontend_sdl_test text` | typing into the Join page's address through the real window: SDL's text input on only while typing, SDL_TEXTINPUT, Backspace held, a key's own event typing nothing, non-ASCII refused, Ctrl+V's first line, Enter, Escape, F1 |
| `frontend.sdl.mouse` | `frontend_sdl_test mouse` | the mouse through the real window: no cursor at start, nor after SDL's first event without relative motion (the window opened under the cursor: Windows' CI showed the cursor there before); a move shows it, 2 s without one hides it, an event at the same place isn't a move; clicks and the wheel during play reach no pad, don't open the overlay and don't show the cursor; F1 shows the cursor; pointing, a click opening a page, a click starting a capture, the back button, the wheel not changing a value, the right button; the click right after the focus comes back ignored, a later one outside the panel closes it and doesn't reach the game; the cursor stays 2 s after closing; About by mouse, Copy as its last item (no Open on the offscreen driver), the clipboard holding the page (Windows' CRLF line ends taken as LF) |

- **The SDL tests** use port/gfx's real window and event loop on SDL's offscreen driver (`SDL_VIDEODRIVER=offscreen`),
  so they need no display.
  - Keys and window events are injected with `SDL_PushEvent`.
  - Controllers are SDL virtual joysticks of the GameController type (`SDL_JoystickAttachVirtualEx`, SDL 2.24+).
    Their buttons and axes are set directly, and their rumble callback records what the game sent.
  - `tests/stubs.c` stands in for port/ultra: the clock and the rumble pak's motor are the test's to set. It also
    records `pc_audio_set_volume`.
- **The menu tests** run without SDL: port/gfx renders headless, and its window is then an offscreen image of the
  window's size that `pc_gfx_read_window` reads ([GFX.md](GFX.md) "The overlay"). Input comes as the overlay's
  actions (what input_sdl.c sends for keys and buttons), captured names and `--press` scripts, with the test's
  clock. The overlay's drawing and font have their own tests in port/gfx (`gfx.ui.*`).
- **The mouse, Help and About (2026-10-28):** 193 of 193 CTests pass on rocky in Debug and RelWithDebInfo, three of
  them new (`frontend.menu.mouse`, `frontend.menu.pages`, `frontend.sdl.mouse`); the Windows cross build builds
  every test program. Mutation check: of twelve deliberate bugs, eleven were caught (window units taken as pixels,
  pointing during a capture, the wheel, a click outside, the double-click guard, the cursor shown at start or never
  hidden, the focusing click, "saved", About without main's facts, the bar); the twelfth, input_sdl.c passing a
  click on while the overlay is closed, changes nothing, since `pc_menu_mouse` ignores it then too.
- **Results on rocky (x86-64, Mesa, SDL2 2.30), 2026-10-27:** 92 of 92 CTests pass (80 before: the 5 `frontend.menu`
  and `frontend.sdl.menu` above, the 6 `gfx.ui` tests and `audio_volume` are new). Without SDL2, 82 of 82. The
  Windows cross build (mingw-w64, with and without the fetched SDL2) builds every test program.
- **Mutation check for the overlay.** Fifteen deliberate bugs, one at a time, all caught:
  - the game's pads not idle while it is open (`frontend.menu.script`);
  - the game not held (`nav`, `script`, `sdl.menu`);
  - held keys, or held controller buttons, not ignored after it closes (`sdl.menu`);
  - a captured key's name lost (`sdl.menu`);
  - Save appending a setting instead of replacing its line, or writing what didn't change (`menu.save`);
  - a script press held across the opening counting (`menu.script`);
  - widescreen not applied, or the window not resized (`menu.live`);
  - the overlay not blended (`gfx.ui.panel` and its variants, `menu.nav`);
  - glyphs mirrored (`gfx.ui.font` and its variants);
  - the redraw without the overlay (`gfx.ui.font`, `closed`, `menu.nav`, `sdl.menu`);
  - the overlay drawn into the presented image, which screenshots read (every `gfx.ui` test, `menu.nav`,
    `sdl.menu`);
  - a linear volume (`audio_volume`).
- **Results on the Pi 5.**
  - Without SDL2: 39 of 39 CTests pass (33 before, plus 6).
  - With the fetched SDL2: 48 of 48 pass. That is 35 before, including `audio_backends` and `gfx.all.sdl-window`
    now on the shared SDL2, plus 13.
  - About 300 checks in all.
- **Mutation check.** Twelve deliberate bugs were put in, one at a time, and the tests caught all twelve:
  - Alt+Enter reaching START;
  - no rumble renewal;
  - no rumble fade;
  - the stick's y not inverted;
  - controllers ignoring `first_controller_player`;
  - no reassignment on unplug;
  - held keys kept on focus loss;
  - key repeat toggling;
  - the trigger threshold ignored;
  - live input overriding a scripted stick;
  - a round gate instead of the octagon;
  - no dead-zone rescale.

## Testing live input

The user's first play with a controller on Windows (2026-10-27): "the analog stick wasn't working for steering". The
scripted presses steered, so each link between a real device and the pad the game reads was checked:

- **The device to the pads.** `--press-via KIND[:SECONDS]` sends the press scripts the way live input goes, instead of
  into the pads:
  - `controller`: an SDL virtual GameController per port the scripts use; `joystick`: a virtual joystick without a
    mapping, which gets the generic one; both plugged in at `SECONDS` on the N64 clock (0, the default: before input
    starts; later: a hot-plug, and the presses before it are lost). Each N64 control the script holds presses the
    first input bound to it; the script's stick tilts the `controller.analog` stick by the same share of its travel
    (80 is full tilt), or presses the `STICK_*` bindings with `analog = none`; `MENU` holds the parts of the first
    `controller.menu`. The state goes in with `SDL_JoystickUpdate` before the pads are read, as a device's report
    would.
  - `keyboard`: key events for the first key bound to each control, handled before the pads are read.
  - With a window or `--headless`: without one, live input pumps SDL's events itself at port/gfx's tick. The
    virtual pads log the rumble they are sent.
- **What the game does with it.** The state line (`--status`, `--until-state`) ends with the first car's steering
  input, its model record's +0x720 (-1 to 1; `test_save.c` reads the same field).
- **Results.** On the synthetic cartridge (rocky) and the retail ROM (the Pi, locally): each kind gives the scripted
  run line for line, so the device path, the mapping, the dead zone, the gate, the stick's sign and the players
  are right, and the steering input is +1 while the stick is right and -1 while it is left. On the retail ROM the
  car also goes elsewhere than without the stick; the synthetic cartridge's car physics don't turn the car, so
  there the steering input is the check (`input_check.py --expect-turn` asks for both). A second boot with a
  controller pak written by the first steers too.
- **The cause** (2026-10-28, when CI's x86-64 RelWithDebInfo job failed `input_live_drive`): the release build
  didn't steer at all, from a controller or the keys. Optimized by mingw-w64's GCC 12 (the release's toolchain) or by
  GCC 13 on x86-64, `func_800E7134` (controller_poll) stored each pad's scaled stick y and dropped its x, which is
  what the car steers by: the scaled sticks `D_80140620` were declared there as one `f32` and written through a
  `[pad][2]` view ([OPTIMIZED.md](OPTIMIZED.md) 2.7). Debug builds, rocky's GCC 14 and the arm64 builds kept the
  store, and every scripted steering check had run one of those. The declaration has its shape on the PC now; the
  scripted and the live runs steer in both build types. The release zip the user played was RelWithDebInfo.
- **Also handled,** found on the way, each a way a real pad could still fail: a pad SDL has no mapping for was
  passed over silently (the generic mapping); a device holding player 1 before the pad (generic pads take a player
  at a button press); on Windows, SDL's raw input reading an Xbox pad's sticks from window messages while its
  triggers come from XInput (XInput now); and nothing in the log said which pad was where. The log and the
  overlay's Controllers page now say which controller is on which player and what its stick gives.
- **Smooth keyboard steering** (`input.keyboard_steering`, 2026-11-04, after the player report in
  [QA.md](QA.md) "Player reports" 1): `input_check.py` then runs the path with each stick push moved to start 0.1 s
  before a whole second and end 0.05 s before one, and a push right from 6 retraces before the race's start (the
  countdown's end, state 0x400000) for 0.5 s, through `--press-via keyboard` with `--log-input`, at `instant` and
  at `smooth` (250 ms). On the synthetic cartridge (rocky, Debug, the drive path: right at 31.9-34.95 s, left at
  35.9-38.95 s): instant's stick goes 80, 0 (the push across the start), 80, 0, -80, 0; smooth's goes 5, 11, 16 ...
  75, 80, 64, 48, 32, 16, 0 for each push, a value each retrace, the one across the start 5, 11, 16, 21, 27, 32
  before the state changes and 37, 43 ... 80 and back after it. The car's steering input 0.1 s into each push is
  0.063 (right) and -0.063 (left) with smooth against 1.0 and -1.0 with instant, and 0.05 s after each release
  0.063 and -0.063 with smooth against 0 with instant. (The game's own reading of the stick is not linear: 0.4 of
  full tilt gives under a tenth of the steering.) `--no-smooth` skips these runs. The same run at
  `game.frame_rate = 60` (to 42 s): its 76 `--log-input` lines are the original rate's, line for line, and so is the
  car's steering input at each status line from 32 to 39 s.
- **CI** runs `input_check.py` as `input_live_drive` in every ctest, and on the cross-built program (the release's
  toolchain) in the windows-run job, Debug and RelWithDebInfo.

```
python3 tools/port/input_check.py --exe build/pc/rush2049pc --rom build/pc/synth/rush2049_synth.z64
python3 tools/port/input_check.py --exe build/pc/rush2049pc --rom <rom> --rom-check --press <script> --expect-turn
```

## Interfaces

- **`pc_port.h`** gains one function: `int pc_gfx_set_event_hook(int (*hook)(const void *sdl_event))`.
  - It is in `gfx_sdl.c`, with a weak `-1` default in `port/ultra/defaults.c`.
  - It returns 0 only when there is an SDL window.
  - The input section's comment now names port/input.
- **`port/ultra/ultra.h`** gains `pc_input_init`, `pc_input_shutdown`, `pc_input_paused` and `pc_input_log`.
  It also gains `pc_si_motor(channel)`: si.c's rumble motor, 1 while on, or if it was turned on since the last
  call.
- **`loop.c`**: the pause hold, and events pumped before the pads are read.
- **`main.c`**: settings resolution, and new options: `--fullscreen`, `--size`, `--scale`, `--widescreen`,
  `--config`, `--no-config`, `--set`, `--print-config` and `--log-input`. `pc_input_init` runs after `pc_gfx_init`, and
  `pc_input_shutdown` runs at exit.
- **Build.** `pcfrontend` is a new object library (port/input and port/config), and `pcultra` no longer
  compiles port/input. port/gfx and port/audio take `RUSH_SDL2_TARGET` from the top level.
- **The settings overlay** (2026-10-27) changed no game C and nothing in port/ultra.
  - `pc_port.h` gains port/gfx's drawing over the frame (`pc_gfx_set_ui_hook`, `pc_gfx_ui_rect`,
    `pc_gfx_ui_text`, `pc_gfx_font_glyph`, `pc_gfx_redraw`), the event pump's turn for port/input
    (`pc_gfx_set_tick_hook`), and `pc_audio_set_volume`. All are in parts that are always built, so they need no
    weak defaults.
  - `config.h` gains `audio.volume`, `controller.menu`, `hotkeys.menu` (`PC_HOTKEY_MENU`), `PcSettings.save_path`,
    `pc_ini_update`, `pc_settings_save` and `pc_settings_read_quiet`; `pc_settings_get` now reads the bindings,
    `analog`, `menu` and the hotkeys too.
  - `input_internal.h` has the overlay's interface (`pc_menu_*`) and the live part's new entry points.
  - `pc_input_paused` is also true while the overlay is open, and `pc_input_init` starts it; main sets the volume.
  - The pause key's redraw of an exposed or resized window now shows the presented frame again (`pc_gfx_redraw`)
    rather than presenting the last render target.
- **`game.cars`** (2026-10-29, [MORECARS.md](MORECARS.md) 10.1): `config.h` gains `PcSettings.cars` and
  `pc_race_cars_set`; `port/config/race_cars.c` is new (in pcfrontend, and in `frontend_test`); `pc_game_types.h`
  gains `pc_race_cars_begin` and `pc_cars_setting` beside `pc_race_cars`, which moves there from `port/main.c`; menu.c
  gains the Game page (`P_GAME`) after Controllers, so the menu tests step one more over the main page where they
  pass it. The game C change is the Drones option's bound (`PC_DRONES_MAX`, `n64_neutral`: shifted).
- **`game.memory`** (2026-11-03, [MEMORY.md](MEMORY.md)): `config.h` gains `PcSettings.memory` and
  `pc_settings_memory_conflict` (`pc_settings_resolve` prints its warning); config.c includes `pc_port.h` for the
  range (`PC_RCP_SEGMENT_SPAN` to `PC_RDRAM_MAX`); `input_internal.h`'s `PcAbout` gains `memory`, which main fills from
  port/ultra's `pc_heap_stats`; menu.c's Game page gains Memory after Cars in a race (restart), so the menu test steps
  over it to Defaults, and About gains its row after Audio buffer. main maps the window at the setting's size
  (`pc_mem_set_size`, `pc_os_set_mem_size`) before `pc_mem_init`.
- **The mods folder** (2026-11-04, [MODS.md](MODS.md)): `config.h` gains `PcSettings.mods`, `mods_dir`, `mods_order`
  and the `[mods]` flags (`mod_flag`, `nmod_flags`, `PC_CFG_MODS`) and `pc_settings_mod_on`; config.c's table has the
  three `[game]` settings after `memory`, and `pc_settings_set`, `pc_settings_get` and `pc_settings_format` a `[mods]`
  section of any keys (written last). `input_internal.h` gains `PcMenuMod`, `pc_menu_set_mods` and `PcAbout.mods`;
  menu.c gains the Mods page (`P_MODS`, item kind `I_MOD`) after Game, so the menu tests step one more over the main
  page where they pass it, and About's Mods row after Memory; `MAX_ITEMS` is the mods' 64 and 8. main reads the ROM
  before mapping the window (the mods' memory needs the cartridge's file table), then `mods_prepare`; after the game's
  data, `mods_install`; it sets `RUSH_GFX_TEXTURE_MODS`. No game C.
- **`game.saves`** (2026-11-04, [SAVES.md](SAVES.md)): `config.h` gains `PcSettings.saves`, `saves_dir` and
  `PC_CFG_SAVES_*`; `input_internal.h` gains the Saves page's interface (`PcSaveRow`, `PcSavesUi`,
  `pc_menu_set_saves`), which main fills over port/ultra's `pakstore.c`; menu.c gains the Saves page and a save's own
  page (`P_SAVES`, `P_SAVE_ITEM`) after Game, after Mods, so the menu tests step one more over the main page where they pass it,
  and `MAX_ITEMS` is 120 for a folder's list. No game C changed.
- **The Online page** (2026-11-07, [NETPLAY.md](NETPLAY.md) 13): `input_internal.h` gains `PcOnlineUi` (main's side:
  what this run is, this PC's addresses, the input logs, the actions), `pc_menu_set_online_ui`, `pc_menu_editing`,
  `pc_menu_text`, `pc_menu_unsaved_list` and the live part's `pc_input_live_text` (SDL's text input, off otherwise:
  `pc_input_live_init` turns it off); menu.c gains the Online pages (`P_ONLINE`, `P_ON_HOST`, `P_ON_JOIN`,
  `P_ON_REPLAYS`, `P_ON_REPLAY`) and the item kinds `I_TEXT`, `I_ON`, `I_ON_ADDR`, `I_ON_RECENT`, `I_ON_REPLAY`; the
  main page's Online item after Saves, so the menu tests step one more over the main page where they pass it, and the
  session's two rows move from the main page to the Online page. `port/config/online.c` (in pcfrontend by its glob)
  and `port/include/pc_online.h` are new; `config.h` gains `PcSettings.net_recent`. input_sdl.c handles
  SDL_TEXTINPUT, and while an item is typed in, Backspace, Delete, Enter, Escape, Ctrl+V and Shift+Insert; the
  clipboard's log line no longer names the About page. No game C, nothing in port/ultra.
- **Presets** (2026-11-04, [above](#presets-n64-original-pc-enhanced-pc-maximum)): `port/config/presets.c` is new (in
  pcfrontend by its glob, and in `frontend_test`); `config.h` gains `PcPreset`, `pc_presets`, `pc_preset_find` and
  `pc_preset_in_effect`; menu.c gains the Presets page (`P_PRESETS`, item kind `I_PRESET`) and the main page's Presets
  after About, so the menu tests step one more over the main page where they pass it; main.c gains `--preset`. No game
  C, nothing in port/ultra.
- **The mouse, Help and About** (2026-10-28) changed no game C and nothing in port/ultra.
  - `pc_port.h` gains `pc_gfx_driver` (the GL strings `gfx_gl.c` logs); `pc_audio.h`'s `PcAudioStats` gains
    `game_rate`; `app.h` gains `pc_app_rom_path`, `pc_app_version_commit`, `pc_app_version_build` and
    `pc_app_build_type` (pcapp is built with `PC_BUILD_TYPE`, which `--version` and the log now print).
  - `input_internal.h`: `pc_menu_mouse`, `pc_menu_mouse_pixels`, `pc_menu_hit`, `PcAbout`, `pc_menu_set_about`,
    `pc_menu_about_text`, `pc_menu_message`, and the live part's `pc_input_live_desktop`, `_clipboard`,
    `_open_folder`.
  - `main.c` fills About's facts (`about_fill`: the version, the audio output's stats, the ROM, the saves, the log,
    the data folder); it includes port/input's and port/audio's headers for that.
  - menu.c's `Item` gains `desc` and `row`, `Page` gains its info rows and columns; the descriptions of the
    existing settings are a table at the end of `build()`, so the pages' item lists didn't change.

## Untested

- (2026-10-28) A real controller on Windows, through XInput rather than SDL's raw input, and a pad with the generic
  mapping: only virtual ones ran. The next play test's log names the pad and its player.

- A real display (X11, Wayland, KMSDRM) and a real controller. Everything above ran on the offscreen driver and
  on virtual joysticks.
- What fullscreen does on a real window manager.
- Real rumble motors, and HIDAPI pads.
- Live input with the game running was untested when this was written, before the game reached its first
  frame; it has run since ("Testing live input" above, [QA.md](QA.md)).
- macOS. Its INI location would need `~/Library/Application Support`; Windows uses `%APPDATA%` ("Location"
  above).
- **The settings overlay:**
  - over the game's own frames, and the game resuming after it (the boot without a ROM faults before its first
    frame, 0.25 s in, so the hold was tested with the test programs' frames and clock, not the game's threads);
  - on a real display: its legibility on a monitor, high DPI (the unit follows the drawable's pixels), fullscreen
    and window resizing from it on a window manager;
  - a real controller: guide as the menu button (SDL holds back a release within 250 ms of the press; the Game Bar
    and Steam may take it first), stick drift during a capture, several pads moving it at once;
  - key names captured on other keyboard layouts (scan codes are named by their US position, as in the file);
  - its window readback on Windows: the SDL test skips the pixel check there, since WGL leaves the back buffer
    undefined after the swap; the headless pixel tests run there.
  - a real mouse: the events were SDL's, pushed into the offscreen window, and the display scales were the hit
    test's with made-up sizes (SDL's window and drawable sizes on a real 200% Windows desktop or a 2x Wayland
    output weren't seen); the cursor's hiding on a real window manager; Open the data folder (SDL_OpenURL) on
    Windows and Linux desktops, which the tests never run.
