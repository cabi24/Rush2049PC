# The desktop frontend: input, window and settings (port/input, port/config)

Part of Phase 6 of [PORT_PLAN.md](../PORT_PLAN.md). Written 2026-10-26. It covers the keyboard and game
controllers, the window's keys (fullscreen, screenshots, pause, quit), rumble, and `rush2049pc.ini`. All of it was
checked on SDL's offscreen video driver and SDL virtual joysticks. No real display or controller was available.

| File | What |
|---|---|
| `port/input/input.c` | `pc_input_poll`: live input, then `--press` scripts over it, then rumble; `--log-input` |
| `port/input/input_sdl.c` | SDL2 builds: keyboard, controllers, hot-plug, rumble, hotkeys, focus and pause (no-ops without SDL2) |
| `port/input/stick.c` | dead zone, the N64 range, the octagonal gate |
| `port/config/config.c` | the INI reader, the settings table, the file's location, the first-run file, the layers |
| `port/input/tests/` | `frontend_test` (no SDL) and `frontend_sdl_test` (SDL2 builds) |

## Controls

The defaults. Keys are named by their position on a US keyboard, so W/A/S/D stay in place on other layouts.
Controllers use SDL's GameController layout: XInput positions, so "A" is the south button on any pad.

| N64 | Keyboard (player 1) | Controller |
|---|---|---|
| Stick | arrow keys | left stick |
| A (accelerate) | X | A (south), right trigger |
| B (brake) | C | X (west), left trigger |
| Z | Z | left shoulder (LB) |
| L | Q | Back (View) |
| R | E | right shoulder (RB) |
| START | Enter | Start (Menu) |
| C-up / C-down | I / K | right stick up / down, or Y (north) / B (east) |
| C-left / C-right | J / L | right stick left / right |
| D-pad | W / S / A / D | D-pad |

| Key | Does |
|---|---|
| Alt+Enter, F11 | toggle desktop fullscreen |
| F12 | save the shown frame as `rush2049pc-YYYYMMDD-HHMMSS.png` (in `video.screenshot_dir`) |
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
    keep that player until unplugged.
  - A pad plugged in while every player has one waits, and takes the first player that frees up.
  - A player's keyboard and controller are merged: buttons are OR'ed, and the stronger stick wins on each axis.
  - With `first_controller_player = 2`, the keyboard has player 1 to itself (v5's `--keyboard-alone`).
- **Connected.** Controller 1 is always connected. Another port is connected while a controller is assigned to
  it or a press script uses it, so unplugging a pad unplugs that N64 controller.
- **The stick.**
  - The dead zone (`input.deadzone`, 0.15) is radial. The travel past it is rescaled from 0, so movement starts
    small instead of jumping.
  - Full tilt reaches `input.stick_range` (80) left, right, up and down. On the diagonals it reaches
    (`stick_diagonal`, `stick_diagonal`) = (70, 70): the octagonal gate of an N64 stick. Between those, it
    follows the octagon's edges.
  - Keys and buttons bound to `STICK_*` give full tilt; two of them give the diagonal corner.
  - `stick_diagonal = 0` gives a round gate instead.
- **Triggers and stick directions as buttons.** They press their N64 button past `input.axis_threshold` (0.5 of
  the travel after the dead zone).
- **Rumble.** With the rumble pak in (`game.pak = rumble`, or `--rumble`), the motor that `port/ultra/si.c`
  emulates drives `SDL_GameControllerRumble` on that player's pad.
  - Both motors run at `input.rumble` × the level. The level is 1 while the motor is on. Otherwise it is the
    share of the last 4 retraces the motor was on, so a game that pulses the motor gets a steady, weaker rumble.
  - Each rumble lasts 1 s and is renewed every 0.5 s while it goes on, so it stops by itself if the game stops.
  - It stops at quit.
- **Scripts.** `--press` and `--press-file` (v5's format, [ULTRA.md](ULTRA.md)) still work. They are OR'ed over
  live input, and a scripted stick direction replaces that axis.
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
- **Mappings.** A `gamecontrollerdb.txt` next to `rush2049pc.ini` is loaded at start (SDL's format), for pads
  that SDL doesn't know.

## The window

- `port/gfx` owns the window and pumps its events in `pc_gfx_poll_events`. port/input gets every event through
  `pc_gfx_set_event_hook`, before port/gfx's own handling, so there is one event loop. Everything runs on the
  main thread, which holds the GL context.
- The main loop now pumps events first at each retrace, then reads the pads, so the pads hold that retrace's
  input rather than the previous one's.
- **Resizing.** The window is resizable, with a minimum of 320 × 240. The frame keeps its aspect: square N64
  pixels, which is 4:3 for the N64's 320 × 240, at the largest integer multiple that fits, letterboxed
  (port/gfx's present, [GFX.md](GFX.md)). The default window is 640 × 480.
- **Widescreen.** `video.widescreen` (`--widescreen`, `RUSH_GFX_WIDESCREEN`) widens the picture to a display
  aspect: `off`, `auto`, `16:9`, `16:10`, `21:9`, any `W:H` or a ratio from 4:3 to 4. The 3D view shows more at
  the sides at the same scale; menus, the HUD and sprites stay 4:3 in the middle; full-width fills and
  backgrounds span the width. The renderer does it all ([GFX.md](GFX.md) "Widescreen"); the game code is
  unchanged.
  - `auto` takes the window's aspect at each frame, so resizing the window or going fullscreen changes it. A
    640 × 480 window is 4:3, so `auto` is off there.
  - With a fixed aspect and the default window (640 × 480), the window opens at the picture's aspect (853 × 480
    at 16:9), so the frame shows at 2× as at 4:3. A window size set in the file or with `--size` is kept.
  - At 16:9 the frame is 426 × 220. A 1280 × 720 window shows it at 3×, and a 1920 × 1080 screen at 4×, with a
    border (the present uses whole multiples; see [GFX.md](GFX.md) "Hooks for later" for a scale of the window's
    size).
- **Quit.** Closing the window, the quit key, Ctrl-C and SIGTERM all end the loop the same way. The controller
  pak is flushed, rumble stops, and audio and the renderer shut down.

## Settings: rush2049pc.ini

- **Location.**
  - `rush2049pc.ini` next to the executable, when there is one (a portable install).
  - Otherwise `$XDG_CONFIG_HOME/rush2049pc/rush2049pc.ini`. That is `~/.config/rush2049pc/` when
    `XDG_CONFIG_HOME` is unset, or relative, as the XDG spec says.
  - `--config PATH` reads another file, and `--no-config` reads none.
- **First run.** When the located file doesn't exist, it is written with the defaults and every setting
  commented, and then read. `--config`, `--print-config` and `--selftest` never write it.
- **Layers.** Each overrides the one before:
  1. the defaults;
  2. the file;
  3. the environment variables other parts already read: `RUSH2049_AUDIO`, `RUSH_GFX_SCALE`, `RUSH_GFX_API`,
     `RUSH_GFX_WIDESCREEN`;
  4. a press file's `pak on|off`;
  5. command-line options, in order.
- **Bad values.** A bad line in the file, or a bad environment value, is reported with its line and skipped.
  The setting keeps its value from the layer before. A bad option is an error (exit 2).
- **`--print-config`** prints the settings in effect, as a settings file.
- **`--set SECTION.KEY=VALUE`** sets anything, for example `--set video.vsync=off` or `--set keyboard.A=Space`.
- **The format.** `[section]`, `key = value`, and `;` or `#` comments. An inline comment needs whitespace before
  its `;` or `#`. A value in double quotes keeps edge spaces and comment characters. Names are
  case-insensitive. Booleans are `on/off`, `true/false`, `yes/no` or `1/0`. A UTF-8 BOM and CRLF are fine.

| Setting | Default | Option | |
|---|---|---|---|
| `game.rom` | `baserom.us.z64` | `--rom` | the ROM; relative to the working directory |
| `game.rom_check` | on | `--no-rom-check` | refuse a ROM whose SHA-1 isn't the USA ROM's |
| `game.expansion_pak` | off | `--expansion-pak` | 8 MiB of RDRAM |
| `game.pak` | memory | `--no-pak`, `--rumble`, `--pak` | controller 1's accessory: memory, rumble, none |
| `game.pak_file` | `rush2049pc.mpk` | `--pak PATH` | the controller pak image |
| `video.width`, `video.height` | 640, 480 | `--size WxH` | window size |
| `video.fullscreen` | off | `--fullscreen` | start in desktop fullscreen |
| `video.vsync` | on | | swap interval 1 or 0 |
| `video.scale` | 1 | `--scale N` | internal resolution multiple, 1-8 (`RUSH_GFX_SCALE`) |
| `video.api` | gles | | `gles` or `gl` (`RUSH_GFX_API`) |
| `video.widescreen` | off | `--widescreen` | off, auto (the window's), 16:9, 16:10, 21:9, W:H or a ratio up to 4 (`RUSH_GFX_WIDESCREEN`; above) |
| `video.pause_on_focus_loss` | off | | hold the game in the background |
| `video.screenshot_dir` | (working directory) | | where F12 saves |
| `audio.backend` | auto | `--audio` | auto, sdl, alsa, null, wav (`RUSH2049_AUDIO`) |
| `input.keyboard_player` | 1 | | 1-4, 0 for none |
| `input.first_controller_player` | 1 | | controllers fill players from here |
| `input.deadzone` | 0.15 | | 0-0.9 |
| `input.axis_threshold` | 0.5 | | a trigger or stick direction bound to a button presses it past this |
| `input.stick_range` | 80 | | full tilt along an axis, 1-127 |
| `input.stick_diagonal` | 70 | | each axis at full diagonal tilt; 0 for round |
| `input.rumble` | 1.0 | | strength 0-1 |
| `[keyboard]` `A` ... `STICK_RIGHT` | the table above | | SDL key names, comma-separated |
| `controller.analog` | left | | the stick that is the N64 stick: left, right, none |
| `[controller]` `A` ... `STICK_RIGHT` | the table above | | SDL GameController names |
| `hotkeys.fullscreen`, `screenshot`, `pause`, `quit` | F11, F12, Pause, Escape | | empty for none |

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
- The environment variables `RUSH_GFX_HEIGHT`, `RUSH_GFX_VERBOSE`, `RUSH2049_DUMP_AUDIO`, `RUSH2049_AUDIO_RATE`
  and `RUSH2049_ALSA_DEVICE` stay environment-only, as do `--dump-audio` and the run-limit options.
- **How settings reach the parts.**
  - port/gfx's settings go through the variables it already reads: main sets `RUSH_GFX_SCALE`, `RUSH_GFX_API`
    and `RUSH_GFX_WIDESCREEN` to the resolved values before `pc_gfx_init`.
  - The window size goes to `pc_gfx_init`.
  - Vsync, fullscreen, the title and the minimum size are set by port/input on the SDL window.

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
| `frontend.settings` | `frontend_test settings` | the defaults; every type and its range checks (a bad value leaves the setting alone); the widescreen aspect (W:H, ratios, auto, off, written back as 16:9 / 16:10 / 21:9, narrower than 4:3 refused); bindings, unbound, `analog`, hotkeys; `--set` syntax; the written file reads back to the same text, quoted strings included; bad lines are counted and skipped |
| `frontend.layers` | `frontend_test layers` | the file over the defaults, the environment over the file (a bad variable is ignored), overrides over both in order, a bad override fails but the rest apply, no file, a missing `--config` file |
| `frontend.locate` | `frontend_test locate` | XDG, a relative XDG (ignored), HOME, no place at all; the first run writes the defaults (making directories) and reads them back; edits are read next time; READ never creates; a file next to the executable wins |
| `frontend.stick` | `frontend_test stick` | dead zone (radial), rescaling, full tilt in four directions, the octagon's corner and edge (22.5°: (76, 31)), a square pad's corner, the round gate, keys, monotonic travel |
| `frontend.precedence` | `rush2049pc --print-config` | the real program: the file's ROM, `RUSH2049_AUDIO` over the file, `--scale` over `RUSH_GFX_SCALE` over the file, and a `--set` binding |
| `frontend.sdl.keyboard` | `frontend_sdl_test keyboard` | every default key, held keys, the stick from keys (cardinal, the corner, opposite keys); rebinding: two keys, an alias, an unknown name skipped, the keyboard on player 2, a round gate of 100 |
| `frontend.sdl.hotkeys` | `... hotkeys` | title and minimum size; Alt+Enter toggles fullscreen and is not START; F11 back; key repeat ignored; F12 saves PNGs of the shown frame (two in one second get two names); Pause holds and the exposed window is redrawn; Escape quits and isn't a game key; rebound and cleared hotkeys; closing the window quits; fullscreen from the settings |
| `frontend.sdl.focus` | `... focus` | focus loss releases held keys; no hold by default; with `pause_on_focus_loss` it holds and focus back releases it; the pause key and focus are separate reasons |
| `frontend.sdl.controller` | `... controller` | a virtual XInput-style pad: every default button (guide and the stick clicks unbound), triggers and right-stick directions with their threshold, the left stick (dead zone, rescaled travel, full tilt, the diagonal, +y up), merging with the keyboard; rebinding: the right stick as the N64 stick, a bigger dead zone, buttons on Z and on the stick, bad names skipped |
| `frontend.sdl.hotplug` | `... hotplug` | six pads plugged and unplugged: players filled in order, kept while others leave, a waiting pad takes a freed player, ports disconnect when unplugged; `first_controller_player = 2`; a pad present before input starts |
| `frontend.sdl.rumble` | `... rumble` | the motor drives the pad's rumble callback at full strength, only on that player; a steady rumble outlives SDL's 1 s duration; the fade over 4 retraces; a pulsed motor stays at least half; strength from the settings; another player's motor; stopped at shutdown; strength 0 |
| `frontend.sdl.script` | `... script` | `--press` merged over live input: buttons OR'ed, a scripted stick axis replaces the live one, a scripted port connected; `--log-input` runs |

- **The SDL tests** use port/gfx's real window and event loop on SDL's offscreen driver (`SDL_VIDEODRIVER=offscreen`),
  so they need no display.
  - Keys and window events are injected with `SDL_PushEvent`.
  - Controllers are SDL virtual joysticks of the GameController type (`SDL_JoystickAttachVirtualEx`, SDL 2.24+).
    Their buttons and axes are set directly, and their rumble callback records what the game sent.
  - `tests/stubs.c` stands in for port/ultra: the clock and the rumble pak's motor are the test's to set.
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

## Untested

- A real display (X11, Wayland, KMSDRM) and a real controller. Everything above ran on the offscreen driver and
  on virtual joysticks.
- What fullscreen does on a real window manager.
- Real rumble motors, and HIDAPI pads.
- Live input with the game running. The game doesn't reach its first frame yet (ULTRA.md); scripted input on
  the boot path is unchanged.
- Windows and macOS. The INI location there would also need `%APPDATA%` and `~/Library/Application Support`.
