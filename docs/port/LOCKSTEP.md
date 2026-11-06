# Lockstep's foundation: the pads gate, input record and replay, the input delay, the state hash

Phase 0.3 and 0.4 of [NETPLAY.md](NETPLAY.md) (section 5), written 2026-11-06 on f9a7177. Online play is delay-based
lockstep on the virtual N64 clock: both PCs run the same game and only exchange pads. This is what that stands on,
without a network:

- **the pads gate**: at each retrace the main loop asks a *pads source* for the retrace's four pads, and waits, the N64
  clock still, until it has them (section 2). The network layer (`port/net`, another branch) is one such source;
- **record and replay**: `--record-inputs FILE` writes each retrace's pads as the game gets them, `--replay-inputs
  FILE` plays them (section 3);
- **the input delay**: `--input-delay D`, or a schedule, with NETPLAY.md 11.2's rule (section 4);
- **the state hash**: the curated state every retrace, the RDRAM window, own storage and the decoupled data every N,
  with host pointers normalised, `--log-hash`, the desync report and the mutation check (section 5);
- **render values in the game C**: `pc_gfx_hud_dx` held at 0 in a session (`--session-rules`), and a check that no new
  one comes in (section 6);
- **which settings change the game**, measured (section 7), and the host's `powf` at 60 replaced (section 9).

Everything is off by default: a run without these options takes the main loop's old path, and `synth_compare.py
--same` finds the program's 62 virtual-clock paths the same as f9a7177's, run for run (section 8).

| Option | What |
|---|---|
| `--record-inputs F` | each retrace's final pads to F |
| `--replay-inputs F` | F's pads instead of this PC's; refused if F's header says another build, cartridge or settings |
| `--replay-force` | replay it anyway |
| `--input-delay D` | this PC's pads D retraces later; `D:FROM,D:FROM,...` a schedule (`2:0,4:600,1:1200`) |
| `--log-pads` | each change of the game's pads, with its retrace (`pads vi=N: 1 8000 +0 +0 c; 2 ...`) |
| `--log-hash N[,W]` | the state hash's line every N retraces; the window, own storage and the decoupled data every W (60) |
| `--hash-report N:PATH` | retrace N's desync report to PATH (and PATH.window, .own, .decoupled) |
| `--session-rules` | an online session's rules without the network: `pc_gfx_hud_dx` gives the game 0 |
| `RUSH_SIM_GATE=SEED,P,MS` | test: at each retrace with probability P the pads are held back up to MS ms of real time |
| `RUSH_SIM_DESYNC=N:ADDR[:XOR]` | test: the window's word at ADDR (hex) changes at retrace N, before its hash |
| `RUSH_HASH_UNNAMED=1` | diagnosis: each image pointer no table names, at the first full hash |
| `RUSH_ZERO_LOCALS` (CMake) | the game C's unset locals start as zero (section 5.4); off by default |

## 1. Retrace numbers

A retrace's number n counts the retraces the main loop sends, from 0: the `vi=` of `--log-physics`. Retrace n's pads
are latched before n's VI event, and n's hash is taken at the same moment, while every N64 thread waits (where
`snapshot()` reads the state log's values, `port/ultra/loop.c`). So hash n covers the game as the pads of retraces 0
to n-1 left it: two PCs that agree on those agree on hash n.

## 2. The pads gate (`port/include/pc_lockstep.h`, `port/ultra/lockstep.c`)

**Where.** Without anything behind it the main loop is as it was: at a retrace it pumps the window's events, polls
the pads (`pc_input_poll`) and latches them (`pc_si_set_pads`) just before the VI event. With a source, a recording,
the delay, `--log-pads` or `RUSH_SIM_GATE`, `pc_pads_gate(n)` runs when retrace n is due, before anything else of
the retrace (before the snapshot, the timers, the RSP tasks): on the virtual clock every N64 thread waits there and
nothing has been posted. It pumps the events and polls the pads once, as the loop did, puts them through the delay line,
hands them to the source, and asks the source for the four pads. Known: the loop goes on, and latches them at the old
place (`pc_pads_take`). Not known: the gate waits on a condition variable that `pc_pads_signal()` (any thread: the
network's) wakes, at most `PC_PADS_POLL_MS` (5 ms), pumps the window's events (quitting works), and the loop comes
back to the same retrace. Nothing has moved: the N64 clock stands (`pc_os_wait_main` isn't called), no timer fires, no
event goes out. Waiting in the middle of the retrace instead, after the timers and RSP tasks had posted their events,
would let threads they woke run ahead of the VI event, which on the virtual clock they never do; so the wait is at
its start.

**The API** (`pc_lockstep.h`):

```c
enum { PC_PADS_READY = 0, PC_PADS_WAIT = 1 };
typedef struct PcPadsSource {
    const char *name;
    void *user;
    void (*local)(void *user, uint32_t n, const PcPad local[4]);   /* this PC's pads at n, once, after the delay */
    int (*pads)(void *user, uint32_t n, PcPad out[4]);             /* n's four pads, or PC_PADS_WAIT */
    void (*hash)(void *user, uint32_t n, const PcStateHash *h);    /* n's state hash, when hashing is on */
} PcPadsSource;
int pc_pads_set_source(const PcPadsSource *source);   /* before pc_ultra_run; -1 if one is set (a replay) */
void pc_pads_signal(void);                            /* any thread: ask again now */
void pc_pads_stats(PcPadsStats *out);                 /* retraces, waited, wait_ns, longest_ns */
```

The pads a source gives are the final ones: buttons, sticks, and per port `connected` and `rumble` (an accessory set
to `auto` follows `rumble`, `si.c`). Everything that shapes input has happened on the PC that sampled them (NETPLAY.md
1.3). A network source sends `local`'s port for the retrace its own delay line assigns, and answers `pads` from both
players' values; `hash` gives it the hash to send. The delay line is there for it too (section 4).

**Proof** (`gate_same`, `replay_check.py gate`): race2p and stunt without the gate in the loop's path, and with
`RUSH_SIM_GATE=7,0.2,15`: pads held back at a fifth of the retraces for up to 15 ms of real time each, released by a
thread of their own that signals the gate as a network thread would. On race2p the pads were held at 570 of 2,763
retraces, 4.4 s of waiting in all; the trace (every state change, a status line each second, every retrace's physics
as exact floats) and every retrace's hash, the full one every 30, are the run's without the gate, line for line.

## 3. Record and replay (`port/ultra/replay.c`)

**The file**, little-endian, fixed layout:

| Offset | Bytes | |
|---|---|---|
| 0 | 8 | `R2PCPADS` |
| 8 | 4 | version, 1 |
| 12 | 4 | H: the header's bytes; the first record is at H |
| 16 | 4 | R: a record's bytes, 20 (a later version may make them longer; this one reads the first 20 of each) |
| 20 | 4 | the first record's retrace, 0 |
| 24 | 8 | xxHash64 of the text: the header's own check |
| 32 | 4 | T: the text's bytes |
| 36 | T | the must-match text (below), zeros to H |

A record (20 bytes, one a retrace from 0): ports 1-4, each `buttons` (u16) and the stick's x and y (s8); a byte of
connected bits and one of rumble bits (bit 0 is port 1); and the retrace's low 16 bits, a check that the records are
where they should be. 20 bytes a retrace is 72 KB a minute.

**What a replay must match.** The header's text is `key=value` lines that main.c makes just before the boot:

- `build`: the commit (and `-dirty` when the tree had changes); the compiler and build type may differ;
- `rom`: the cartridge's SHA-1; `mods`: a hash of the mods' cartridge region and the file table that points into it
  (the cartridge as the game reads it, NETPLAY.md 1.5), or `none`;
- the settings the game reads: `game.frame_rate`, `cars`, `memory`, `expansion_pak`, `pc_setup`, `ghost_prompt`,
  `cheats_menu`, `cheats_unlock`; `audio.voices`, `engines`, `latency` (section 7); `video.draw_distance`, `detail`,
  `fog`; `game.pak` to `pak4`; and the window's and osMemSize's sizes;
- `pakN.image`: a hash of each port's controller pak image as the game will find it, or `none`;
- `session.rules`, and the test models that change the game when set (`RUSH2049_TEST_*`, `RUSH_SIM_GFX_MS`).

Lines from `#` are only said: the program's version, the clock, the delay, what it replayed. A replay whose header
differs is refused, with each difference (`game.cars: original in the file, here 8`) and exit code 2;
`--replay-force` replays it anyway. A file cut short replays up to its last whole record, and a record whose check
fails ends it there; past its end the pads are idle (the last record's controllers, nothing pressed), and the log says
`replay: F ends at retrace N (cut short: a record's last bytes are missing); idle pads from here`. A damaged header is
refused. `--input-delay` with `--replay-inputs` is refused: the replay doesn't use this PC's pads.

The press scripts still run in a replay (their `pak` line, `frames`, the overlay's MENU); their pads don't reach the
game. A recording made in real time replays, but real time isn't deterministic (NETPLAY.md 1.1): it is for the
virtual clock.

**Proof.** `replay_same_<path>` (`replay_check.py same`), all 62 paths that run on the virtual clock: a process that
records, then another that replays, each with `RUSH_STATE_EXACT=1 --status 1 --log-physics 1 --log-pads --log-hash
1,30`: the same trace (3,200-12,450 lines a path) and the same hash at every retrace, the full one every 30, the two
processes' images apart (ASLR). `replay_file`: a race2p recording cut after 1,381 of its 2,763 records and 7 bytes of
the next replays the same hashes through retrace 1,381 and says where it ends; a header with a flipped byte is refused;
a recording replayed with `game.cars=8` is refused naming `game.cars`, and replays with `--replay-force`.

## 4. The input delay (`--input-delay`, the delay line)

`--input-delay D` gives the game this PC's pads D retraces later; the first D retraces are idle (nothing pressed, the
controllers as the first sample has them). `--input-delay 2:0,4:600,1:1200` is a schedule: 2 from retrace 0, 4 from
600, 1 from 1200. The rule is NETPLAY.md 11.2's, in `pc_delay_put` (exported for the network layer, whose own delay
changes as the link does): the sample of retrace n goes to slot n + d(n); slots skipped as the delay grows get the last
pads assigned again; a sample whose slot is taken as it shrinks is dropped. The game gets slot n at retrace n.
Recorded, the pads are the delayed ones, so a recording made with a delay replays without one.

**Proof** (`replay_delay`, `replay_check.py delay race2p`):

- with `--input-delay 2` the game's pads at each of 2,763 retraces are those of the run without a delay 2 retraces
  before (its 16 changes, the first at retrace 300 against 302);
- and the run is that of the press script moved 2 retraces later (each press 33.3 ms later), line for line: 5,606
  lines, every state change and every retrace's physics. A press's effects come exactly D retraces later;
- with the schedule the pads at every retrace are what the rule makes of the undelayed run's (3 samples dropped as it
  shrank, 2 retraces repeated as it grew), and its recording replays to its trace.

## 5. The state hash (`port/ultra/statehash.c`)

### 5.1 What is hashed

**Every retrace, the curated regions** (a few KiB, NETPLAY.md 1.6): `models` (`pc_models`), `cars` (`pc_cars`, the
places among them), `links` (`pc_links`), `rng` (the seed `D_8011735C`), `words` (`state_word_a`, `state_word_b`),
`tick` (`D_80143FF4`), `time` (`D_801543CC`), `pads` (`gControllerData`), `camera` (`gCamera`, four windows). `state`
is the hash of their hashes: what a session sends every retrace.

**Every W retraces, the full hash:** `window` (the RDRAM window, 8-256 MiB), `own` (each own-storage symbol of
port/data's tables, the game data kept outside the window because it holds pointers, in N64 order) and `decoupled`
(every item of `port/data/decoupled.txt`: the roster, the model and car records, the blasts, skids, sounds, MusyX
voices and sound messages; `hash_decoupled_list` checks the list is complete).

The hash is xxHash64, written from its specification (no code taken). `--log-hash N[,W]` prints:

```
rush2049pc: hash vi=1500: 254faf59b7ec9d19 models 6a02e558245cffe6 cars bb7fa9ec95d2339d ptrs=84 links e57e83b1a33ce2b5
  rng 8ec8f516bb054801 words 2e6a0b58767cf03d tick 5c381e329638eaa7 time 09a29cfa071ec12d pads 4f9a8b2d55f8976a camera
  16d80f43676e95d0 window e15cc0a957b8a9c7 ptrs=80 own 76b1b4bf885bf083 ptrs=1361 unnamed=214 decoupled
  a7e880d13e8d50ed ptrs=156
```

(one line). Cost on the build host, Debug (the hash itself compiled -O2): the curated regions every retrace add 6% to a
race2p run, the full hash 1.8 ms each (an 8 MiB window).

### 5.2 Host pointers

The window is mapped at 0x80000000 in every process, so the window's N64 addresses are the same everywhere. A host
pointer isn't: ASLR moves the executable, its heap and stacks in each process, and Windows lays the image out
otherwise. Experiment E6 found the window's host pointers: the audio manager's effect tags' `list` (a pointer to the
own-storage list head `D_80143FF8`) and `D_80156BCC`, a display-list pointer's low half. A host pointer is hashed as
what it points to, from what port/data's tables know: an own-storage symbol's N64 address and the N64 offset in it
(through the loader's runs, so a layout with 8-byte pointers or longs gives the same id on every OS); a game function's
N64 address; a decoupled item's index and offset; a runtime image's C data. Which values are host pointers depends on
what the bytes are:

| Bytes | A host pointer is | Taken as |
|---|---|---|
| N64-layout data: the window, the curated regions in it, own storage's bytes not typed as pointers | a u64 at a 4-byte offset equal to a named target's first byte or a function's address | the target's id |
| own storage's elements the tables type as pointers, or as the N64's longs (8 bytes on the PC, an `intptr_t` that may hold one: `D_801148CC`, a blit) | any value but a 32-bit one (0, a window address, a number, sign-extended) | the target's id; into the image elsewhere, "a pointer into the program"; else "a heap pointer" (a table the loader copied, a converted file: `D_8011A31C`, `D_801497C0`, `D_8017A4E0`) |
| the PC's own C structs: `pc_models`, `pc_cars`, `pc_links` and the decoupled items | a u64 at an 8-byte offset pointing into a named target | the target's id |
| the game C's sites that keep a pointer's low half (`TRUNCATED`: `D_80156BCC`) | the word | left out (zero): it holds an N64 address at the boot and a low half later, and a low half completed with the image's high half can't be told from an N64 address the image's low range covers in one process (seen: two runs of race2p hashed it otherwise at retrace 0) |

N64-layout data holds numbers and texels, and two words of them can make a value in the image's range in one process
and not in another (where the image's high half is a small number, 16-bit texel data can be it), so a looser rule
than "a named target's first byte" there can find a pointer in one process only. Every pointer the window held on the
synthetic paths was a list head's first byte. "A pointer into the program" (the PC's buffers in game
C: the frame's command lists, the blits) keeps no offset, which no two OSes share (`unnamed=` counts them,
`RUSH_HASH_UNNAMED=1` lists them).

### 5.3 Completeness: two processes, every path

`replay_same_<path>` compares two processes' hashes at every retrace, the full one every 30, on all 62 paths, and
`hash_aslr` two processes with the full hash at every retrace (race2p, battle2p, drive). Before they agreed, the
reports (`--hash-report`, compared by `hash_compare.py --reports`, which names each differing window word by
`data_symbols.txt`) found three more things, none a host pointer of the kind above:

1. **The profile's id was its handle's host address.** `CreatePlayer` (`menu_save_options/group.c`) keeps
   `(N64A(player) << 3) | pak` in the profile's record and its save file. The handles (`D_8013C128`) are own storage,
   so on the PC `N64A` cut a host address to 32 bits: the id, the save data's checksum and the bytes written to the pak
   differed in every process (and would between two PCs). Fixed: in that file `N64A(p)` is `pc_n64_identity(p)`
   (pc_prelude.h), the handle's N64 address as on the N64, from the tables. The one change to game C here: a
   file-scope macro inside `#ifdef TARGET_PC` (`n64_neutral.py` passes). A pak saved before keeps its old id; the game
   only stores and copies it.
2. **libultra's SI scratch buffers hold the host's stack.** `__osMakeMotorData` builds the rumble pak's command in a
   `__OSContRamReadFormat` on the stack, sets its header and copies all of it, 32 unset data bytes included, into
   `__osMotorPifBuf`; the Pfs code does the same into `__osPfsBuffer`. Those bytes are the host stack's leftovers
   until the next command overwrites them (rumble2p, pak4p, ghost). They are device scratch, not the game's state: the
   hash takes them as zeros (`SCRATCH`).
3. **Locals the C never sets** are the same thing at large: the game reads a few, and on the PC their bytes differ by
   process and OS where the N64's were fixed. Built with `-DRUSH_ZERO_LOCALS=ON` (GCC's `-ftrivial-auto-var-init=zero`
   for the game C), every such local starts as zero on every PC; the scratch buffers then hold zeros, and all 62 paths
   give f9a7177's traces, run for run (`synth_compare.py --same`). It is off by default; worth turning on for online
   play's builds once its cost is measured on retail.

**Across architectures** (x86-64's recordings replayed on the Pi's aarch64, the same cartridge): the curated, own
and decoupled hashes agreed from the start, and the window differed from retrace 30 in 28 words of matrices the game
fills with 0/0: **the default NaN's sign**. x86's SSE makes 0xFFC00000, ARM's FPU 0x7FC00000; nothing reads them. The
hash takes 0xFFC00000 and a double's 0xFFF80000 as the positive ones (no other value changes). With that, all 62
recordings of the build host's x86-64 Debug program replay on the Pi's aarch64 RelWithDebInfo program to the same
trace and the same hash at every retrace, the full one every 30 (`replay_check.py replay-dir --replay-force`, the
header differing only in the build's commit, which a copy of the tree without `.git` can't tell). Two more things
found on the way:

- The synthetic cartridge itself differs between Pythons: `synth_rom.py` fits the game's sine polynomial
  (`gSinCoeffs`, `gCosCoeffs`) by least squares, and the Pi's Python 3.11 and the build host's 3.12 round its last
  bits otherwise, so two machines' cartridges have other SHA-1s (a replay's header refuses the other's). CI's
  `windows-replay` replays each Linux job's recordings on that job's own cartridge.
- Windows against Linux: the hash's ids are the same on both by construction except "a pointer into the program"
  (offset dropped) and own storage whose host layout differs (a `long`, 4 bytes on Windows). CI's `windows-replay`
  (section 8) compares them; it hasn't run yet.

### 5.4 The desync report and the mutation check

`pc_hash_report(path, n)` writes retrace n's hashes, region by region, and each curated region's bytes as hashed (hex,
host pointers normalised): n's own when the session keeps them (`pc_hash_keep(K)`: the last K retraces' curated bytes,
40 KB each), else the last retrace's (the file says which). When the full hash was taken at n, PATH.window, PATH.own
and PATH.decoupled hold those bytes, and the report lists own storage's and the decoupled items' places in them.
`--hash-report N:PATH` writes it at retrace N: replay a session's input log on each OS to the first retrace the hashes
differ at (`hash_compare.py A.log B.log`), then `hash_compare.py --reports A B` names the words.

`hash_mutation` (`replay_check.py mutation`): `RUSH_SIM_DESYNC=1000:8011735C:1` flips the random seed's low bit at
retrace 1000; `hash_compare.py --expect-from 1000` finds every hash equal to retrace 999 and the state hash differing
at every retrace from 1000 on (rng, the state, the window at 1000). A window word nothing uses (0x807FFFF0) changed at
1000 is caught by the window's hash from 1000 on, and the curated hash never moves.

## 6. Render values in the game C

NETPLAY.md 1.1 and risk 13: `car_lights_render` places the coin icons and the battle and stunt HUD models at
`x + pc_gfx_hud_dx(x)`, a value of the renderer's (widescreen, the HUD setting, the window's width). Their places are
game state. `pc_gfx_hud_dx_off(1)` (`--session-rules`) makes it give the game 0: the models keep their N64 places (the
2D HUD still moves; moving the models in the renderer can come later). The matrix (section 7) shows
`video.widescreen=16:9` changing the window's hash on drive (from retrace 1470) and stunt (1650), and nothing with
`--session-rules`.

`tools/port/render_values_check.py` (ctest `render_values`) finds every place the game C names a render or per-PC
value: `pc_gfx_*`, `pc_audio_*`, `pc_input_*`, `pc_menu_*`, `pc_app_*`, `pc_cfg*`, `pc_settings_*`, the detail
settings' readers, and port/include's macros that call one (`PC_DRAW_REACH`, `PC_LOD_DIST`, `PC_DETAIL_PLAYERS`). Each
must be in `tools/port/render_values_allowed.txt` with what keeps two PCs alike: today 17, all must-match settings
(draw distance, detail, fog, voices), the session rule (`pc_gfx_hud_dx`), a compile-time check and a test hook. A new
one fails until it is read and listed.

## 7. Which settings change the game (`tools/port/hash_settings.py`)

Each path recorded once, then replayed unchanged and once with each candidate of NETPLAY.md 1.5's tables changed
(`--replay-force`); the first retrace whose hashes differ from the unchanged replay's, and the regions there (curated
every retrace, the full hash every 30). Synthetic cartridge, Debug, the build host, 2026-11-06:

| Table | Setting | race2p | drive | stunt | battle2p |
|---|---|---|---|---|---|
| must | `game.frame_rate=60` | from 30: own, window | from 30: own, window | from 30: own, window | from 30: own, window |
| must | `game.cars=8` | from 1402: cars, links, models | from 1410: decoupled, models | from 1410: decoupled, models | from 1410: decoupled, models |
| must | `game.cars=12` | from 1402: cars, links, models | from 1410: decoupled, models | from 1410: decoupled, models | from 1410: decoupled, models |
| must | `game.memory=64` | from 0: own, window | from 0: own, window | from 0: own, window | from 0: own, window |
| must | `game.expansion_pak=off` | from 0: own, window | from 0: own, window | from 0: own, window | from 0: own, window |
| must | `game.pc_setup=on` | from 510: window | from 510: window | from 510: window | from 510: window |
| must | `game.ghost_prompt=on` | same | same | same | same |
| must | `game.cheats_menu=on` | from 510: window | from 510: window | from 510: window | from 510: window |
| must | `game.cheats_unlock=on` | same | same | same | same |
| must | `audio.voices=64` | from 0: decoupled, own, window | from 0: decoupled, own, window | from 0: decoupled, own, window | from 0: decoupled, own, window |
| must | `audio.engines=all` | from 1440: decoupled, own, window | from 1440: decoupled, own, window | same | same |
| must | `video.draw_distance=far` | from 1440: own | from 1440: own | same | same |
| must | `video.draw_distance=max` | from 1410: own | from 1410: own | same | same |
| must | `video.detail=high` | same | same | same | same |
| must | `video.fog=far` | same | same | same | same |
| must | `video.fog=off` | same | same | same | same |
| must | `--pak matrix.mpk` (a memory pak) | from 30: window | from 30: window | from 30: window | from 30: window |
| must | `game.pak=rumble` | from 30: window | from 30: window | from 30: window | from 30: window |
| must | `game.pak2=rumble` | from 30: window | same | same | from 30: window |
| free | `video.scale=2` | same | same | same | same |
| free | `video.widescreen=16:9` | same | from 1470: window | from 1650: window | same |
| free | `video.widescreen=16:9 --session-rules` | same | same | same | same |
| free | `video.fill=on` | same | same | same | same |
| free | `video.frame_rate=120` | same | same | same | same |
| free | `video.hud=centered` | same | same | same | same |
| free | `video.textures=upscaled` | same | same | same | same |
| free | `video.texture_lod=sharp` | same | same | same | same |
| free | `video.vsync=off` | same | same | same | same |
| free | `video.fullscreen=on` | same | same | same | same |
| free | `audio.volume=50` | same | same | same | same |
| free | `audio.latency=100` | **from 0: window** | **from 0: window** | **from 0: window** | **from 0: window** |
| free | `--audio wav` | same | same | same | same |
| free | `input.deadzone=0.3` | same | same | same | same |
| free | `input.sensitivity=1.5` | same | same | same | same |
| free | `input.stick_range=60` | same | same | same | same |
| free | `input.keyboard_steering=smooth` | same | same | same | same |

What it says:

- **`audio.latency` is not free.** NETPLAY.md 1.5 put it with the per-PC settings, but it sets the AI's queue, which on
  the virtual clock decides when the game's audio thread runs: the window differs from the first retrace (the curated
  state not, on these paths). It is in the replay header's must-match list now. For a session either it is matched, or
  the AI's accounting runs at one fixed queue whatever the device (the audio tap of NETPLAY.md 1.8, phase 0.2).
- `video.widescreen` is free only with the session rule (section 6).
- `video.draw_distance` changes own storage (the scene walk's state) in the races, as expected; `video.detail` and
  `video.fog` change nothing on the synthetic cartridge, whose levels have no objects the detail setting adds and no
  fog; they stay must-match until retail says otherwise (NETPLAY.md 9).
- `game.ghost_prompt` and `game.cheats_unlock` change nothing on these paths (they act on screens the paths don't
  reach); they stay must-match.
- The `[input]` settings can't reach a replay (its pads are the file's); in a session they shape the pads before they
  are sent, so they are free by construction. `--audio wav` is the null output's accounting, so it changes nothing.
- The accessories and the memory/Expansion Pak change the window from the start, as the game reads them at boot.

## 8. Tests, CI and what was run

| ctest | Runs | Checks |
|---|---|---|
| `replay_same_<path>` (62) | record, replay | the same trace and hashes at every retrace, the full hash every 30, images apart |
| `replay_delay` | race2p 5 times | D = 2 shifts the pads by 2; the run is the script moved 2 retraces; the schedule follows the rule and replays |
| `replay_file` | race2p | cut short, damaged, other settings, `--replay-force` |
| `gate_same` | race2p, stunt | random waits leave no trace and no hash difference |
| `hash_mutation` | race2p 3 times | a changed word caught from its retrace on, by the curated and by the window's hash |
| `hash_aslr` | race2p, battle2p, drive | two processes, the full hash every retrace, the same |
| `hash_decoupled_list` | | the full hash takes every decoupled item |
| `render_values` | | no unlisted render or per-PC value in the game C |

An ASan or TSan build runs `replay_same_` of drive, race2p and race12, `replay_file`, and `gate_same` and
`hash_aslr` on race2p (a path runs 17-37 times slower there). All pass on the build host in Debug and RelWithDebInfo,
with the rest of the suite. Also run there:

- `hash_aslr`'s check on all 62 paths, two processes each with the full hash at every retrace, Debug: 62 of 62 the
  same, the images apart in every pair.
- `gate_same`'s check on all 62 paths: 62 of 62 the same.
- `replay_same` on the Pi (aarch64, RelWithDebInfo): 62 of 62; and the build host's x86-64 recordings replayed there:
  62 of 62 the same trace and hashes (section 5.3).
- `synth_compare.py --same` of f9a7177's program against this one's, Debug and RelWithDebInfo: 62 of 62 paths the
  same (the default is unchanged), and at `game.frame_rate=60` (section 9).
- The settings matrix (section 7).

**CI.** The linux jobs upload each path's recording and replay log (`build/pc/synth/replay/*.pads`, `*.replay.log`).
A new job, `windows-replay` (Debug and RelWithDebInfo, after `linux` and `windows-cross`), replays x86-64's and
aarch64's recordings with the cross-built Windows program (`replay_check.py replay-dir`): the same trace and hashes as
Linux's replay, path for path. A recording's header makes the Windows run refuse anything but the same commit,
cartridge and settings. The `windows` job's ctest runs the `replay_same_*` tests natively too. Not run yet: this branch
hasn't been pushed.

## 9. `powf` at 60 (NETPLAY.md 1.1, risk 6)

`pc_fps_filter(e)` (`port/config/game_fps.c`) raised e to the frame's length in 30 Hz frames with the host's `powf`,
which glibc, msvcrt and the UCRT needn't round alike. A frame is 1 to 5 retraces, so the power is 0.5, 1, 1.5, 2 or
2.5: now `sqrtf(e)`, `e`, `e * sqrtf(e)`, `e * e`, `e * e * sqrtf(e)`, made of IEEE single operations and `sqrtf`,
correctly rounded on every host. `e * sqrtf(e)` and the like can differ from glibc's `powf` in the last bit (for
e = 0.36 they do: frontend.fps checks both); `sqrtf(e)`, the power on the virtual clock at 60, is the correctly rounded
e^0.5. On the virtual clock at 60 the synthetic paths' frames are 1 retrace (18,217 of 18,225 on eight paths) or 5
(8, at loads); in real time a slow frame of 3 retraces can now step its filters a bit otherwise than before, where the
game's course isn't the same from run to run anyway. Against f9a7177's program at `game.frame_rate=60`: 62 of 62 paths the same
(`synth_compare.py --same --set game.frame_rate=60`, three of them missing their expect line alike, as the scripts are
timed for 30), and on race2p, drive, circuit, battle2p, stunt, race12 and demo every retrace's physics and every
frame's camera and car (`--log-physics 1 --log-frames`, exact floats) the same.

## 10. What isn't done or known

- **Windows against Linux** is prepared (`windows-replay`) but has not run. Own storage whose host layout differs
  between the two (a `long`) would show as `own` differing there; the curated and window hashes should agree.
- **Retail.** Hashes and replays of retail runs are for the Pi and the user's machines; nothing of them is committed.
- **The session's side**: fixed connected ports and accessories, the overlay and pause keys not holding the game,
  sending hashes: done on branch `net-session` (`port/app/netplay.c`, [NETPLAY.md](NETPLAY.md) section 12, which also
  moved the must-match list to port/config's `pc_must_match`, shared with the session). Pacing after a wait is the
  paced clock's branch.
- The window's normalisation finds host pointers only in the image. A heap or stack pointer stored in the window, or
  a pointer cut to 32 bits at a site not listed, would make two processes differ; none does on the 62 paths, and
  `hash_compare.py --reports` names such a word if one appears (retail, other paths).
- Uninitialised locals: the scratch buffers are left out of the hash; `RUSH_ZERO_LOCALS` fixes the cause for every
  local, and is off by default.
