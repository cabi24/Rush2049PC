# Names: the conventions, and how the six maps were made one

Wave 1 of the PC-only work ([PCONLY.md](../port/PCONLY.md)) named the game's C in six parts, by six agents that did
not see each other's work: the game image in five address ranges (r1 to r5) and the two runtime images with the boot
segment (ovl, rom). This page is the one set of conventions their maps now follow, every disagreement between them
and how it was decided, the aliases, the labels to delete, and what wave 2 must do by hand when it applies the maps.
The readable maps of the areas are indexed in [README.md](README.md).

## 1. The files

`tools/port/names/<area>.tsv`, one row a symbol, tab separated, `#` comments (PCONLY.md section 3):

    old    new    kind    source    confidence    note

| Map | Area | Rows | func | data | field | file | type |
|---|---|---:|---:|---:|---:|---:|---:|
| r1.tsv | game image 0x80086A50-0x8009DC50 | 614 | 251 | 150 | 49 | 164 | |
| r2.tsv | game image 0x8009DC50-0x800B4720 | 696 | 256 | 218 | 46 | 176 | |
| r3.tsv | game image 0x800B4720-0x800CD798 | 664 | 234 | 253 | 29 | 148 | |
| r4.tsv | game image 0x800CD798-0x800E847C | 759 | 233 | 268 | 98 | 158 | 2 |
| r5.tsv | game image 0x800E847C-0x8010FD7C and the data after it | 887 | 199 | 383 | 103 | 202 | |
| ovl.tsv | runtime images A (front end) and B (HUD extras), and game data only they use | 1,051 | 240 | 680 | 47 | 71 | 13 |
| rom.tsv | the boot segment (`src/rom`) and the low image's data | 1,016 | 459 | 305 | 16 | 109 | 127 |
| **all** | | **5,687** | **1,872** | **2,257** | **388** | **1,028** | **142** |
| drop.tsv | loader labels to delete, not rename (section 5) | 1,295 entries, 1,078 names | | | | | |

The agents wrote 6,447 rows for 5,633 old names: 538 old names were in more than one map (385 agreeing, 153 not), and
22 new names were given to symbols at different addresses. Now every old name has exactly one row over all seven
maps: 814 rows went as second copies of a symbol another map also named, 6 as identity rows (a name that is already
right has no row), and 60 were added: 48 for the address spelling of a mapped function (`ClearPak` beside
`ClearPak`, `pak_removed` beside `pak_removed`) and 12 for loader names the agents had not mapped (section 4).

**Where a row lives.** A symbol that several areas' functions use has its row in the area whose functions use it
most (counted over every function's body in the tree; the overlays count as ovl, `src/rom` as rom); the other areas
dropped theirs. Rows the reconciliation placed in an area that had no row for them are at the end of that area's
file, under a comment. Each map's header says that this page overrides it where they differ.

## 2. The conventions

**MAT3 first.** A name from the MAT3 decomp (`decomp/config/2049LE/symbols.txt`, `decomp/include/rush/rush_types.h`)
is used verbatim when the N64 symbol is the same thing, shown by evidence (strings, constants, callees, field offsets,
loop shapes); `sure` needs the evidence named in the note. Between two readings, a `sure` MAT3 match beats a `likely`
one and a MAT3 match beats a name from the code; a swap is settled by reading the code (section 3).

**The leading underscore goes** (C reserves it): MAT3 `_StopSkid` is `StopSkid`. When the bare name is already a
global of the same MAT3 file, the static takes the `Internal` suffix (r4's convention): `AddBattlePointInternal`,
`AudioPlayEffectInternal`, `CalcChecksumInternal`, `DeallocateMemoryInternal`, `FindWaveInstanceInternal`,
`HAL_SetFontInternal`, `InsertBSTEInternal`, `PS_FlushFileInternal`, `ReadGhostDataInternal`, `WalkBSTInternal`,
`animControllerDimInternal`. zlib's `_tr_init`, `_tr_tally`, `_tr_flush_block`, `_tr_stored_block` become `tr_*`
(which also keeps them apart from a static zlib's own `_tr_*` on Windows). One exception: libultra's internals keep
the SDK's spelling (`__osContLastCmd`, `__osSpGetStatus`, `__osDevMgrMain`), as every other `__os` name in
`src/rom/libultra` and `port/ultra` does.

**MAT3 statics that several files define** take a prefix: the screen prefix the tree already uses for that file
(`audio_`, `controls_`, `controlsys_`, `options_`, `pause_`, `video_`, `gameover_`, `highscore_`, `save_`,
`password_`, `pakerr_`, and in image A the file names `trackscr_`, `carscr_`, `playerscr_`, `circuitscr_`,
`recordscr_`, `startscr_`, `titlescr_`, `licensescr_`, `modescr_`, `pakscr_`, `warningscr_`, `cheatscr_`), else
the MAT3 file's name (`audiomgr_gCmds` and `audiosfx_gCmds`, `audiosfx_gInited`, `camera_nextSwitch`,
`hud_whichCam` beside `hudextra_whichCam`, `hud_animTrackMap` beside `trackscr_animTrackMap`). A function-local
static that recurs takes its function (`InitTrackDynamicObjs_ObjNames`, `PS_ErrorFunc_dimMultiBlit`,
`buildRaceTrackMaps_mapPos`). A static MAT3 defines once keeps its bare name (`gFreeQ`, `gCmdQ`, `lastSwitch`, `yf`).

**A family keeps one prefix.** MAT3 `stree.c`'s globals are `stree_nodes`, `stree_surf`, `stree_svert`,
`stree_slist`, `stree_vlist`, `stree_save`, `stree_savenum`: bare `surf` is a parameter of `TireSurf`, which
reads the datum (the rename would have made it read its parameter), and bare `save` a local in 14 files.

**Names from the code** (no MAT3 counterpart, or none shown): functions `lower_snake_case`, verb first; data
`lower_snake_case` nouns, tables plural or `_table`; types `CamelCase`. A code-named global is not `gCamelCase`
(r3's `gAudioLockQueue`, `gDisplayListCursor`, `gPlayerProfileHandles` are now `audio_lock_queue`, `frame_dl`,
`player_profile_handles`). An existing name that is right stays as it is (`gScheduler`, `osTvType`, `viMgrMain`).

**One name per concept.** Where the agents named one thing differently, one name, used by every map:

- The screen size is `screen_width`/`screen_height` (the game lays its views, scissors, text and clears out against
  it; it is also the colour image's width), not `framebuffer_width` or `gScreenWidth`.
- Threads: the body is `<role>_thread_entry`, the `OSThread` `<role>_thread`, its stack `<role>_thread_stack` or the
  label after it `<role>_thread_stack_top`: `main_thread`, `game_thread`, `model_thread` (thread 5, `ModelThread`),
  `bg_load_thread` (thread 8, `BGLoadWaveMain`), `rumble_thread` (2), `sample_dma_thread` (3),
  `audio_retrace_thread` (9), `audio_manager_thread` (10).
- Locks (the N64's `OSMesgQueue`s that stand for MAT3's empty `_GetSemaphore`/`_ReleaseSemaphore`):
  `<owner>_lock_queue`, `<owner>_lock_msg`, `<owner>_lock`/`<owner>_unlock`, for `audio`, `heap`, `si` (the
  serial bus: controllers, paks, rumble), `text` and `controlsys` (controlsys.c's own, a different queue from `si`).
- Message queues by who waits on them: `main_thread_queue` (the scheduler's retraces and the frame-end request),
  `game_thread_queue`, `rom_dma_queue`, `bg_load_idle_queue`, `si_event_queue`, `audio_command_queue`.
- The frame: `frame_dynamic` (the SDK's Dynamic: matrices, lookats, then the display list), `frame_dl` (its cursor),
  `frame_dl_start`, `last_frame_dl_end`, `frame_matrix_cursor`, `frame_record_index`; the 2D list is `blit_dl`.
- The runtime images: `start_overlay_*` (image A, the front end) and `extra_overlay_*` (image B), `overlay_base`;
  their loaders `load_overlay`, `load_extra_overlay`, `LoadFrontEndOverlay`.
- The sound library itself is `sound_*` (rom); the game's glue to it, the eight services and the data it hands the
  library, is `musyx_*` (`musyx_project`, `musyx_pool`, `musyx_sdir`, `musyx_samples`, `musyx_retrace_callback`,
  `musyx_task_done_queue`); the N64 scene draw is `zoid_*`.

**Labels inside an object** (an address the C reads that is not the start of a datum): `<object>_<field>` with the
MAT3 field name (`gText_single`, `gPlayerFiles_head`, `gPaks_inserted`, `ObjInstanceList_mat`), an element by its
meaning (`gObjList_tire`, `gOptions_metric`, `cheat_menu_unlocked`), `<table>_players` for rows 1-4 of a
`[5][13]` car-setting table, `<table>_biased` for a base IDO folded from `table[n - k]`. Wave 2 replaces each with
the element or field expression; the notes say which.

**Empty retail stubs** (the 8-byte `jr ra; nop` IDO leaves where it inlined a static): `<Original>_stub` when the
static is known (`SaveStuntSetting_stub`, `controlsys_lock_stub`); when the same function also exists out of line
elsewhere, the out-of-line copy takes the plain name and the stub `<Name>_stub` even if the C gave the stub a body
(`FindWaveInstance_stub`, `InitBST_stub`, `ConvertHeap_stub`, `DeallocateMemory_stub`); when the static is unknown,
`stub_before_<Next>`/`stub_after_<Previous>` or `<module>_stub_<letter>`. Wave 2 deletes them. A stub whose C is
the static's only body takes the static's plain name (r5's `EndGame`, `InitGfx`).

**Aliases.** When the loader's tables give one address two names (a loader name and the `D_`/`func_` spelling, or two
loader names), every spelling has a row and all map to the one name; the second row's note says "alias".

**Field rows** name the leaf member at the offset with its full path from the struct (`hdr.cmd`,
`tires[0].tradius`, `mpctl.xrel`, `dr_uvs[1]`, `fcam.mat.uvs[2]`), one row per offset; a union's alternatives
are in the note (`tAudioMsg.0x4`). **File rows**: a single-function file takes its function's final name
(`src/blob/audio_retrace_thread_entry.c`); a group directory the module or purpose (`src/blob/groups/stree_collide`).

**No collisions** (section 6): a new name is not an identifier in `src/`, `include/` or `port/` unless that is the
same symbol (an alias, a `#define` of the same address, a static or prototype copy of the same function), a member of
a struct (a separate namespace: `targetInfo` in a Windows display struct, `sym` in SDL's keysym), or a local or
parameter of a function that does not use the symbol.

## 3. The disagreements, decided

Every old name two maps named differently (153), and every old name that shared its new name with a symbol at
another address (44, for 22 names), with the agents' names, the decision and the evidence in one line, are in the
table at the end of this page (204 rows). 27 needed the code read; the hardest:

- **`gNumStartCmds`/`gNumStopCmds`** (`gNumStartCmds`/`gNumStopCmds`, r1 and r3 swapped): `_UpdateFx` appends the
  start messages (command 3) with `gNumStartCmds++` and the stop messages (command 5) with `gNumStopCmds++`, as MAT3
  audiomgr.c:701/709. r1 is right.
- **`gMineCones`/`gCarMines`** (`gMineCones`/`gCarMines`, r5 swapped): the cheat screen mirrors them into
  `gCheatState[1]` and `[2]` in MAT3 cheatscr.c's order, and the car-mine test in `AnimateFrame` reads `gCarMines`.
- **`gObjList`** is `gObjList`: the game image fills `gObjList[0..385]`, and between `0x8014254C` (overlay B's
  label, which the overlay map and the lead's notes took for the base) and `0x801427C0` lie `gOutOfTime`,
  `gNumDrones` and a dozen other data. `0x8014254C` is `gObjList_biased` (gObjList - 314 entries).
- **`gDefStartTimes`** (`gDefStartTimes`): the blob table names that address `gDefStartTimes` and `func_800F0914` indexes
  it by track as MAT3 does; the rom map's `build_info_tail` had no reader.
- **`bg_load_thread`** (`bg_load_thread`, the loader's `bg_load_thread`): `main_thread_entry` creates thread 8 with
  `BGLoadWaveMain` as its entry, so nothing about it is audio; `model_thread` likewise (thread 5 runs `ModelThread`).
- **`FindWaveInstance`, `InitBST`, `_ConvertHeap`** each had one MAT3 name at two addresses: the out-of-line body
  takes the name (0x800AC820, 0x800C8774, 0x80097468), the empty retail stub `<Name>_stub`.
- **`controlsys_lock_queue`** (`controlsys_lock_queue`) and **`si_lock_queue`** (`si_lock_queue`): two different locks, both called
  `si_lock_queue` by one map each; `InitControlSys` creates the first with `controlsys_lock_msg`, `si_lock` the second.
- **`main_thread_queue`/`game_thread_queue`** (`main_thread_queue`/`game_thread_queue`, three readings each): the main thread
  receives the scheduler's retraces and `UpdateFrame`'s 2750 request on the first and posts the frame start on the
  second, which only the game thread waits on.
- **`yf`** and **`last_frame_dl_end`** (`yf`, `last_frame_dl_end`): each address is both a real datum and the end
  of the array before it; the datum takes the name and wave 2 writes the end as `gOPPFuncs + 10`, `gStuntInfo + 4`.
- **`gShotObjList`**, **`gTrophyMat`** (`gShotObjList`, `gTrophyMat`): the first BSS object of each image, which the
  overlay loader also passes as the BSS start; the object takes the name, the loader gets a named bound in wave 2.
- **`taillight_colors_*`** (`taillight_colors_dark..8C..9C`) and **`headlight_colors_night`** (`headlight_colors_night`): eight palette
  colours each that `AnimateBrakeLights` and `StartHeadLights` copy into the car palette, not UVs.
- **`video_optionValid`** (`video_optionValid`): MAT3 videoscr.c has the table `optionValid[4]` and the function
  `validOption` (`video_validOption`, 0x800B59E8).
- **`gSaveGhostFiles`** (`gSaveGhostFiles`): set only for the developer profile name and tested before the ghost-save
  prompt, as MAT3 save.c:864 and highscorescr.c:145; not a cheat flag.
- **`gRenderInProgress`** (`gRenderInProgress`): `EndGraphics` increments it and `GameLoop` waits on it, as MAT3 gfx.c;
  the scheduler's own definition (`gRenderInProgress`, scheduler.c) is its alias.
- **`gspF3DEX2_NoN_fifoTextStart`** (`gspF3DEX2_NoN_fifoTextStart`): the port decodes F3DEX2 2.08 and `docs/memory_map.md` calls the
  text a NoN fifo build: neither `gspF3DEX_NoN_fifo` (F3DEX 1) nor `gspF3DEX2_fifo` (which three blob files already
  declare as `u8[]`).


## 4. Aliases: several old names, one new name

78 new names have more than one row, every one at a single address (checked): the loader's names, the C's `D_`/
`func_` spellings and the second names of one function or datum. All rows map to the one name; each extra row's note
says "alias". The data ones the lead listed and their kin:

| New name | Old names |
|---|---|
| `gGameState` | `gGameState`, `gGameState` |
| `gNewGameState` | `gNewGameState`, `gNewGameState` |
| `gNumPlayers` | `gNumPlayers`, `gNumPlayers` |
| `gRace` | `gRace`, `gRace` |
| `gPlayers` | `gPlayers`, `gPlayers` |
| `gBlitInstanceList` | `gBlitInstanceList`, `gBlitInstanceList` |
| `gText`, `gText_single` | `gText`, `gText`; `gText_single`, `gText_single` |
| `gGameCar` | `gGameCar`, `gGameCar` |
| `frame_dl` | `frame_dl`, `frame_dl` |
| `gRetraceRate`, `gRetraceTime` | `gRetraceRate`, `gRetraceRate`; `gRetraceTime`, `gRetraceTime` |
| `gSeconds`, `gFrameSeconds`, `gLastFrameCount` | `gSeconds`, `gSeconds`; `gFrameSeconds`, `gFrameSeconds`; `gLastFrameCount`, `gLastFrameCount` |
| `gRenderInProgress` | `gRenderInProgress`, `gRenderInProgress` |
| `scheduler_retrace_count` | `scheduler_retrace_count`, `scheduler_retrace_count`, `scheduler_retrace_count` |
| `scheduler_audio_task` | `scheduler_audio_task`, `scheduler_audio_task` |
| `main_thread_queue`, `game_thread_queue` | `main_thread_queue`, `main_thread_queue`; `game_thread_queue`, `game_thread_queue` |
| `rom_dma_queue`, `bg_load_idle_queue` | `rom_dma_queue`, `rom_dma_queue`; `bg_load_idle_queue`, `bg_load_idle_queue` |
| `frame_end_requested`, `frame_start_msg` | `frame_end_requested`, `frame_end_requested`; `frame_start_msg`, `frame_start_msg` |
| `model_thread`, `bg_load_thread` | `model_thread`, `model_thread`; `bg_load_thread`, `bg_load_thread` |
| `frame_cycle_count` | `frame_cycle_count`, `frame_cycle_count` |
| `game_bss_end` | `game_bss_end`, `game_bss_end` |

Functions with two loader names: `ClearRushControls` (`ClearRushControls`, `ClearRushControls`), `UpdateCameras`
(`UpdateCameras`, `UpdateCameras`), `RemoveEnvirons` (`RemoveEnvirons`, `RemoveEnvirons`),
`AudPlayEffect` (`AudPlayEffect`, `AudPlayEffect`), `SetControls` (`SetControls`, `SetControls`),
`PS_ErrorFunc` (`PS_ErrorFunc`, `PS_ErrorFunc`), and the functions whose `func_` or `D_` spelling the C
still uses beside a loader name (`ClearPak` and `ClearPak`, `SetupWaveModel` and `SetupWaveModel`, `TB_DrawText` and
`TB_DrawText`, ...). The overlay and boot maps' merges (`Rec164` to `Egg`, `SelectCarWave` to `SelectCarWave`, `ButtonInfo`
to `ButtonInfo`; the timer and scheduler globals) are of the same kind.

Two loader names at one address that are **not** one thing keep apart: a label that ends one object and starts the
next (`gStackIdle` and `audio_retrace_thread_stack_top`, `gSchedulerStack` and `sample_dma_thread_stack_top`,
`unzip_input_a`/`unzip_input_a` and `rdp_fifo_buffer_end`, `gDecompressedDataStart` and `blit_set_mode`), and the
two runtime images' symbols at 0x8038A400 and up.

## 5. drop.tsv: labels to delete

`tools/port/names/drop.tsv` (`name`, `table`, `note`; table `blob`, `boot_tail`, `ovl_a`, `ovl_b` or `*` for all
four) lists every loader label that no map renames, that is not already the right name of its address, and that
nothing uses: no line of C in `src/`, `include/`, `port/` or `nonmatching/` but extern declarations, prototypes and a
`#define` of the name itself, and no mention in `tools/` or `port/`'s data files. 1,078 names, 1,295 table entries:

| Why | Names | Entries |
|---|---:|---:|
| inside a function of the game image, not a function start | 441 | 505 |
| a second name of a function or datum the maps name | 83 | 93 |
| an unused label (data guesses, boot-segment names nothing reads) | 537 | 633 |
| stale: the maps give the name to another address | 17 | 64 |

472 of the names are the ones the agents listed (r1 61, r2 81, r3 65, r4 112, r5 9 by name, boot_tail 144); the
rest are unused labels found by the same test. 335 entries are still declared (extern lines and prototypes in the
context preambles of the C files, the arcade headers): wave 2 deletes those declarations.

**Apply drop.tsv before the maps.** Seventeen names sit at a stale address in some tables while the maps give them to
another: `gNumPlayers` (0x80166004; the maps: 0x8014A108), `gPauseState` (boot_tail 0x80114700, really
`gameover_multiblit`; the maps: 0x801170FC), `AdjustSpeed`, `CheckCPs`, `DoDrones`, `InitDrones`, `MaxPathControls`,
`find_best_gear`, `gBestLapTime`, `gBumpTime`, `gCarMass`, `gElapsedTime`, `gFirstPlaceTime`, `gInFirstPlace`,
`gScrapeState`, `gScrapeTime`, `gTireSideForce`. None of them is read by name in the C today (every use goes through
a `D_` spelling or a `#define` to it), so dropping the stale entries changes nothing; after the maps, each name has
its one right address. The drop is by name and table, so it must run first (a later drop would delete the renamed
entry too).

Every label the agents flagged is dropped or renamed. `animCountdownTimer` is dropped as a table entry inside
another function although `groups/countdown/animCountdownTimer.c` defines a C function of that name: the C function keeps it. The `__os*` names in `boot_tail` that the boot map counted as invented are libultra's and used
by `src/rom/libultra` and `port/ultra`: they stay.

## 6. Collisions with the tree

Checked against every identifier in the C of `src/`, `include/`, `port/` and `nonmatching/` (comments and strings
removed). 310 new names exist somewhere already; by kind:

| Kind | Names | What happens |
|---|---:|---|
| the loader already names the same address so | 16 | nothing: the `D_` spelling merges into it (`gCamera`, `gNumWindows`, `gTexDefPtrs`, `osTvType`) |
| `#define New old` of the same symbol | 109 | becomes `#define X X` (harmless): wave 2 deletes them |
| only in `include/game/*.h`, the arcade headers no source includes | 125 | wave 2 deletes those headers or their declarations (other signatures: `CheckVisible(v, on)`, `PickCar(s16, s16)`, `SetCountdownTimer(int)`) |
| a static copy of the same function in another file | 20 | legal (static); wave 2 deletes the copies: `ZOID_*` (12), `MBOX_NewObject`, `ClearBST`, `InsertBSTE`, `WalkBST`, `AddToEnvList`, `PlayGameSound`, `RemoveEnvirons`, `setGameState`; also `Random` and `Hidden` in several files, and loadwave.c's `_NewWaveInstance` |
| a second non-static definition of the same function | 5 | duplicate globals at link time unless flatten's dedupe removed them: `DeleteGhost` (groups/save/ctx_slot.c), `ReadGhostData` (GhostStart.c), `ZOID_SetObjectDef` (visuals/rocket.c), `Hidden` (animPause.c), `InitEgg` (func_8038FF90.c and cheat_screen/eggs.c) |
| a struct member | | separate namespace, nothing to do: `targetInfo`, `sym`, `modelrun`, `upshift`, `downshift`, `collision`, `road`, `gTrackNum` (TrackSlot) |
| a local or parameter of a function that does not use the symbol | | legal; renamed by hand only if that function ever needs the global (list in section 7) |
| only in `nonmatching/` | 5 | goes with nonmatching/ |

Resolved in the maps (the other side could not be renamed, or the shadowing would have changed what code reads):
`stree`/`surf` (the stree family, section 2), `nextSwitch` (`camera_nextSwitch`: ShowHUD has a static of that name),
`gspF3DEX2_fifo*` (three files declare that name `u8[]`), the eleven `...Internal` statics, `gCmds` and `gInited`
(file prefixes), `whichCam`, `animTrackMap`, `ObjNames`, `dimMultiBlit` (prefixes: MAT3 defines each twice),
`gControllerRemoved`, `titlePos`, `gTitlePane` (screen prefixes), `video_validOption` against `video_optionValid`,
and every new name two maps had given to different symbols (`gObjList`, `gGhostFiles`, `gPS_Files`, `gCheatActive`,
`gStoppedChannels`, `wingOffsets`, `blitPrimColor`, `gCarWings`, `gText`, `gPlayers`, `si_lock_queue`,
`si_lock_msg`, `sound_voice_count`, `FindWaveInstance`, `InitBST`, `ConvertHeap`, `gNumStartCmds`/`gNumStopCmds`,
`gMineCones`/`gCarMines`).

## 7. What wave 2 must do by hand

When the maps are applied (`tools/port/rename.py`), in this order:

1. **drop.tsv first**, then all the maps in **one simultaneous pass** (one token table, not one rename after
   another): two new names are another row's old name (`no_catchup` becomes `pak_insert_file` while the loader's
   `pak_insert_file` becomes `pak_insert_file`; `AdjustSteer` becomes `PS_PurgeFile` while `PS_PurgeFile` becomes
   `PS_PurgeFile`).
2. **`src/rom/scheduler.c`'s `PC_ALIAS` lines** name symbols inside string literals of an `__asm__`
   (`.set gRenderInProgress, gRenderInProgress`, `.set gScheduler, gScheduler`, `.set scheduler_interrupt_queue, gScheduler + 0x40`, ...).
   The rename must reach them, and the lines whose two names are now one (`gScheduler`/`gScheduler`,
   `gRenderInProgress`/`gRenderInProgress`, `gSeconds`/`gSeconds`, `gFrameSeconds`/`gFrameSeconds`,
   `gLastFrameCount`/`gLastFrameCount`, `scheduler_retrace_count`/`scheduler_retrace_count`) become `.set X, X`, which does not assemble: delete
   them. The same file defines `gRenderInProgress`, `gSeconds`, `gFrameSeconds` as its variables: they
   take the merged names.
3. **`port/assets/tests/test_roster.c`** has a static helper `init_cars(int, int, int)` that calls the game's
   `init_cars`, which the map names `init_cars`: rename the helper first (the file would declare both).
4. **`audiomgr_thread.c`'s static `_SendCmd`** is a different function from MAT3's `_SendCmd` (now `SendCmd`,
   was `SendCmd`): give it a name of its own (it posts to `audio_command_queue`).
5. **Copies**: delete the static copies and second definitions of section 6; make the callers of the stubs that
   have bodies call the real function (`FindWaveInstance_stub` in groups/loadcars, `ConvertHeap_stub` in the
   `ClearMemoryHeap.c` copies, `DeallocateMemory_stub`), then delete all `_stub`/`stub_*` functions (101).
6. **`#define` aliases** that became `#define X X` (109 names, e.g. `#define gNumPlayers gNumPlayers` in resurrect.c,
   `#define HAL_GetTextWidth HAL_GetTextWidth` in overlay B): delete. Four are function-like or casts that still
   work after the rename but should go: `HAL_GetTextWidth(s, n)` (runtime_a_text_helpers), `Random(range)`
   (anim_tire_donor.c), `gCarComponents` (cleanup_colortrace), `gFrameSeconds` (runtime_b_resource_callbacks).
7. **Declarations**: the arcade headers `include/game/*.h` (no source includes them; 125 of their names now clash
   with other signatures, e.g. `SetCountdownTimer(int)` against the real `f32` one); the extern declarations of
   dropped labels in the context preambles (335 entries); the stale `extern u8 gspF3DEX2_fifoTextStart[]` lines.
8. **Interior labels** (about 120, notes say "interior", "biased", "view of" or "_players"): replace each with the
   element or field expression (`gText_single` with `gText.single`, `gObjList_tire` with `gObjList[156]`,
   `gObjList_biased[def + 314]` with `gObjList[def]`, `carTransGearRatio_biased`, `gPlayerRects_biased`,
   `modescr_validOption_biased`), and the two end-of-array uses (`HAL_InstallOncePerFrameFunc`'s loop end:
   `gOPPFuncs + 10`, not `yf`; `InitStunt`'s: `gStuntInfo + 4`, not `last_frame_dl_end`); give the overlay loader
   named BSS bounds instead of `gShotObjList`/`gTrophyMat`. They work only while the data keeps its N64 layout.
9. **Locals and parameters** that share a new global's name (legal today, renamed if the function ever needs the
   global): `positions` (ZOID_UpdatePoly's parameter, skid_marks and vertices groups), `interp` (maxpath_controls),
   `initialize` (image A's SetFogIntensity), `humans`/`num_humans`/`num_drones` (DoDrones' group copies;
   test_roster.c, test_carshud.c), `zip` (port/mods/mods.c, test_pakstore.c), `sym` (port/ultra/statehash.c's
   `add_target`), `yf` (port/gfx/tests/interp_test.c), ShowHUD's static `nextSwitch`.
10. **Old names outside C**: `port/data/decoupled.txt`, `extents.txt` and `reserved.txt`, the sanitizer
    suppressions and `tools/port/*.py` name symbols by their old names; `port/data/test_pc_data_decls.c`'s
    `D_80010020`..`D_80010070` are test fixtures whose names are their addresses (not renamed).
11. **The loader tables lack** `best_lap_time_by_column`, `race_score_time_format`/`70`/`80` and `gObjList_biased`, which the C reads: add them
    (as their new names) when the tables are rewritten.
12. Don't rewrite member accesses that only look like a new name (`paths[i].targetInfo` in port/gfx/gfx_display.c,
    `e.key.keysym.sym`): the rename replaces old names, so these stay as they are.

## 8. How this was checked

A script (kept with the reconciliation's notes, not in the tree; `tools/port/names_check.py` is the tree's check)
over the seven maps and drop.tsv: six columns and valid kind, source and confidence in every row; each old name in
exactly one row; each new name once per namespace (functions, data and types together; fields per struct; files),
except the 78 alias groups, each at one address; no address in a new name (`func_`, `D_8`, `0x`, `_8xxxxxxx`); no
leading underscore but libultra's `__os`; no C keyword or C library name; every new name looked up in the tree's
identifiers and classified as in section 6; file rows' old paths exist, new paths are free and distinct; no name in
drop.tsv has a map row. The result: no errors; the two rename chains of section 7.

## The decisions, all

| old | the agents' names | decision | evidence |
|---|---|---|---|
| `gspF3DEX2_NoN_fifoTextStart` | r5 `gspF3DEX2_fifoTextStart`, rom `gspF3DEX_NoN_fifoTextStart` | `gspF3DEX2_NoN_fifoTextStart` | the port interprets F3DEX2 2.08 (port/gfx/gfx_gbi.h) and docs/memory_map.md calls the text a NoN fifo build: neither gspF3DEX_NoN (F3DEX 1) nor gspF3DEX2_fifo (three blob files already declare that one as u8[]) |
| `gspF3DEX2_NoN_fifoDataStart` | r5 `gspF3DEX2_fifoDataStart`, rom `gspF3DEX_NoN_fifoDataStart` | `gspF3DEX2_NoN_fifoDataStart` | as gspF3DEX2_NoN_fifoTextStart |
| `scheduler_audio_task` | r1 `audio_rsp_task`, rom `scheduler_audio_task` | `scheduler_audio_task` | scheduler.c writes it (under its invented name scheduler_audio_task) when it starts the audio task; the audio retrace thread only reads it |
| `screen_width` | r1 `screen_width`, r2 `screen_width`, r3 `gScreenWidth`, r5 `screen_width`, rom `framebuffer_width` | `screen_width` | the game lays its views, scissors, text and clears out against it; it is also the colour image width (r1, r2, r5 agree) |
| `screen_height` | r1 `screen_height`, r2 `screen_height`, r3 `gScreenHeight`, r5 `screen_height`, rom `framebuffer_height` | `screen_height` | as screen_width |
| `start_overlay_rom` | r2 `start_overlay_rom`, rom `start_image_rom` | `start_overlay_rom` | one name for the two runtime images across the maps: start_overlay_* / extra_overlay_* (r2 named the loaders) |
| `extra_overlay_rom` | r2 `extra_overlay_rom`, rom `extra_image_rom` | `extra_overlay_rom` | as start_overlay_rom |
| `rdp_init_dl` | r5 `rdp_init_dl`, rom `frame_rdp_init_dl` | `rdp_init_dl` | owner (r5 StartGraphics) name; same meaning |
| `rsp_init_dl` | r5 `rsp_init_dl`, rom `frame_rsp_init_dl` | `rsp_init_dl` | as rdp_init_dl |
| `gLargestTextWave` | r2 `gLargestTextWave`, rom `boot_wave` | `gLargestTextWave` | InitTextWave (MAT3 text.c) loads and locks it first, as MAT3 gLargestTextWave; sure beats boot_wave (likely) |
| `gDefStartTimes` | rom `build_info_tail` | `gDefStartTimes` | the blob symbol table names 0x8002E870 gDefStartTimes and func_800F0914 indexes it [track (+19)] as MAT3; "build_info_tail" had no use |
| `gScheduler` | r1 `gScheduler`, r3 `gScheduler`, r4 `gScheduler`, r5 `main_scheduler`, rom `gScheduler` | `gScheduler` | the existing name in src/rom (scheduler.c, boot.c); 4 of 5 maps |
| `scheduler_interrupt_queue` | r5 `sched_cmd_queue`, rom `scheduler_interrupt_queue` | `scheduler_interrupt_queue` | a field of gScheduler (+0x40): rom's name says which queue |
| `scheduler_task_queue` | r5 `sched_task_queue`, rom `scheduler_task_queue` | `scheduler_task_queue` | as scheduler_interrupt_queue |
| `scheduler_thread` | r5 `sched_thread`, rom `scheduler_thread` | `scheduler_thread` | as scheduler_interrupt_queue |
| `gRenderInProgress` | r5 `gRenderInProgress`, rom `scheduler_task_count` | `gRenderInProgress` | EndGraphics increments it and GameLoop spins on it, as MAT3 gfx.c gRenderInProgress (two files already #define it so); the scheduler decrements it |
| `zbuffer` | r5 `zbuffer`, rom `depth_buffer` | `zbuffer` | owner (StartGraphics, InitGfx) name |
| `game_thread_queue` | r3 `sched_frame_done_queue`, r5 `game_thread_queue`, rom `event_queue` | `game_thread_queue` | process_scheduler_message posts to it when it starts a frame and UpdateFrame drains it: it wakes the game thread |
| `main_thread_queue` | r3 `sched_command_queue`, r5 `main_thread_queue`, rom `retrace_queue` | `main_thread_queue` | the main thread's one queue: scheduler retraces and pre-NMI, and UpdateFrame's 2750 frame-end request (process_scheduler_message's switch) |
| `frame_start_pending` | r3 `sched_frame_pending`, r5 `frame_start_pending`, rom `frame_drawn` | `frame_start_pending` | set by process_scheduler_message when it starts a frame, cleared by UpdateFrame |
| `game_frame_building` | r3 `sched_game_waiting`, r5 `game_frame_building`, rom `frame_started` | `game_frame_building` | UpdateFrame: 0 while it waits for the next frame, 1 after |
| `frame_end_requested` | r3 `sched_end_frame_requested`, r5 `frame_end_requested`, rom `frame_state` | `frame_end_requested` | UpdateFrame sets it with its 2750 request, MainEndFrame clears it |
| `rumble_thread_entry` | r1 `rumble_thread` | `rumble_thread_entry` | thread convention: entry <role>_thread_entry, OSThread <role>_thread (rom does main_thread_entry / main_thread) |
| `rumble_thread` | r2 `pak_thread`, rom `thread2` | `rumble_thread` | its entry ramps the Rumble Pak motors (r1); "pak_thread"/"thread2" by role/id only |
| `rumble_thread_stack_top` | r2 `pak_thread_stack`, rom `thread2_stack_top` | `rumble_thread_stack_top` | as rumble_thread |
| `sample_dma_thread_entry` | r1 `sample_dma_thread` | `sample_dma_thread_entry` | thread convention |
| `sample_dma_thread` | r2 `audio_thread`, rom `thread3` | `sample_dma_thread` | its entry serves the sound library sample DMA list (r1) |
| `sample_dma_thread_stack_top` | r2 `audio_thread_stack`, rom `thread3_stack_top` | `sample_dma_thread_stack_top` | as sample_dma_thread |
| `audio_retrace_thread_entry` | r1 `audio_retrace_thread` | `audio_retrace_thread_entry` | thread convention |
| `audio_retrace_thread` | r2 `audio_dma_thread`, rom `thread9` | `audio_retrace_thread` | its entry is the audio retrace thread (r1); "audio_dma_thread" misread it |
| `audio_retrace_thread_stack_top` | r2 `audio_dma_thread_stack`, rom `thread9_stack_top` | `audio_retrace_thread_stack_top` | as audio_retrace_thread |
| `audio_manager_thread_entry` | r1 `audio_manager_thread` | `audio_manager_thread_entry` | thread convention |
| `audio_manager_thread` | r3 `gAudioMgrThread`, rom `thread10` | `audio_manager_thread` | thread convention (r3 gAudioMgrThread) |
| `audio_manager_thread_stack_top` | r3 `gAudioMgrThreadStack`, rom `thread10_stack_top` | `audio_manager_thread_stack_top` | thread convention |
| `bg_load_thread` | r2 `bg_load_thread`, rom `bg_load_thread` | `bg_load_thread` | main_thread_entry creates thread 8 with entry BGLoadWaveMain = BGLoadWaveMain (r2): the loader name bg_load_thread is wrong |
| `model_thread` | r2 `model_thread`, r5 `model_thread`, rom `render_thread` | `model_thread` | thread 5's entry is ModelThread = MAT3 ModelThread (r4); the model code stops it around reads of the models |
| `rom_dma_queue` | r1 `wave_dma_queue`, rom `rom_dma_queue` | `rom_dma_queue` | main_thread_entry creates it (8 messages) for cartridge PI DMA; wave_rom_read is its one user |
| `bg_load_idle_queue` | r1 `bg_load_idle_queue`, r2 `bg_load_idle_queue`, rom `wave_load_queue` | `bg_load_idle_queue` | the background loader idle token: LoadWave takes it, BGLoadWaveMain gives it back (r1, r2) |
| `si_event_queue` | r1 `si_event_queue`, r2 `si_message_queue`, r4 `si_event_queue`, rom `controller_queue` | `si_event_queue` | osSetEventMesg(OS_EVENT_SI) queue: init_controllers creates it, osContInit and the paks use it |
| `rsp_dram_stack` | r5 `rsp_dram_stack`, rom `gfx_dram_stack` | `rsp_dram_stack` | owner (EndGraphics) names, same meaning |
| `rsp_yield_buffer` | r5 `rsp_yield_buffer`, rom `gfx_yield_buffer` | `rsp_yield_buffer` | as rsp_dram_stack |
| `rdp_fifo_buffer` | r5 `rdp_fifo_buffer`, rom `gfx_fifo_buffer` | `rdp_fifo_buffer` | as rsp_dram_stack |
| `rdp_fifo_buffer_end` | r5 `rdp_fifo_buffer_end`, rom `gfx_fifo_buffer_end` | `rdp_fifo_buffer_end` | as rsp_dram_stack |
| `gSongNames` | r4 `audio_song_names`, r5 `gSongNames` | `gSongNames` | audio_animText reads it as MAT3 audioscr gSongNames (sure over a code name) |
| `gNumStartCmds` | r1 `gNumStartCmds`, r3 `gNumStopCmds` | `gNumStartCmds` | _UpdateFx (updatefx.c) appends the cmd-3 (start) messages with gNumStartCmds++ and the cmd-5 (stop) ones with gNumStopCmds++, as MAT3 audiomgr.c:701/709 |
| `gNumStopCmds` | r1 `gNumStopCmds`, r3 `gNumStartCmds` | `gNumStopCmds` | as gNumStartCmds |
| `audiosfx_gInited` | r1 `sfx_server_inited`, r2 `gSfxInited` | `audiosfx_gInited` | MAT3 statics that several files define take their file as prefix |
| `gCarModels` | r1 `car_physics_defs`, r4 `gCarModels`, r5 `gCarModels`, ovl `gCarModels` | `gCarModels` | init_cars and SetSelectTireSizes read it as MAT3 gCarModels (3 maps) |
| `gCarWings_players` | r1 `car_wings_by_player`, r2 `gCarWings` | `gCarWings_players` | gCarWings itself is gCarWings (four maps); this is its row 1 (the _players convention) |
| `car_color_choice_a` | r3 `car_default_color_0`, ovl `car_color_choice_a` | `car_color_choice_a` | s8[5][13] like the other car settings (row 0 defaults, rows 1-4 players), so not "default colour" |
| `car_color_choice_b` | r3 `car_default_color_1`, ovl `car_color_choice_b` | `car_color_choice_b` | as car_color_choice_a |
| `car_color_choice_c` | r3 `car_default_color_2`, ovl `car_color_choice_c` | `car_color_choice_c` | as car_color_choice_a |
| `gLastKeyTime` | r3 `gLastKeyTime`, r5 `last_input_tick` | `gLastKeyTime` | UpdateControls writes it and the idle timers read it, as MAT3 controlsys.c gLastKeyTime (sure over a code name) |
| `bg_texture` | r3 `backTexture`, r5 `bg_texture` | `bg_texture` | code names are lower_snake_case |
| `gSaveGhostFiles` | r3 `gCheatProfileUnlocked`, r5 `gSaveGhostFiles` | `gSaveGhostFiles` | ReadPlayerData sets it only for the developer profile name and updateHighScoreScreen gates the ghost-save prompt on it, as MAT3 save.c:864 / highscorescr.c:145 |
| `cheat_menu_unlocked` | r3 `gCheatActive`, ovl `cheat_menu_unlocked` | `cheat_menu_unlocked` | gCheatActive is 0x8011648C; this is its element 21 (the start screen code), an interior label |
| `pak_recognize_error_seen` | r4 `gPakRecognizeError`, ovl `pak_recognize_error_seen` | `pak_recognize_error_seen` | code names are lower_snake_case; the warning screen tests and clears it |
| `pause_titlePos` | r2 `titlePos`, r5 `pause_titlePos` | `pause_titlePos` | MAT3 titlePos is defined by two files: screen prefix |
| `pause_gTitlePane` | r2 `gTitlePane`, r3 `pause_gTitlePane` | `pause_gTitlePane` | MAT3 gTitlePane is defined by four files: screen prefix |
| `pause_gControllerRemoved` | r2 `gControllerRemoved`, r4 `pause_gControllerRemoved`, r5 `pause_gControllerRemoved` | `pause_gControllerRemoved` | MAT3 gControllerRemoved is defined by four files: screen prefix |
| `gCarMiddles` | r1 `gCarMiddles`, r3 `car_glow_height` | `gCarMiddles` | StartSmoke places the glow puff at it, as MAT3 smoke.c gCarMiddles (sure over likely) |
| `targetInfo` | r1 `targetInfo`, r2 `gTargetInfo`, r3 `targetInfo`, r5 `targetInfo` | `targetInfo` | MAT3 targetInfo verbatim; the port's other targetInfo is a member of a Windows DISPLAYCONFIG struct (a different namespace) |
| `video_optionValid` | r3 `video_optionValid`, r5 `video_validOption` | `video_optionValid` | MAT3 videoscr.c: optionValid is the s8[4] table, validOption the function (video_validOption = video_validOption) |
| `video_gControllerRemoved` | r3 `gControllerRemoved`, r5 `video_gControllerRemoved` | `video_gControllerRemoved` | MAT3 gControllerRemoved is defined by four files: screen prefix |
| `taillight_colors_dark` | r1 `taillight_uv_dark` | `taillight_colors_dark` | AnimateBrakeLights and StartHeadLights copy these 16-byte blocks as eight palette colours (+0x140, +0x150), not UVs |
| `taillight_colors_dim` | r1 `taillight_uv_dim`, r2 `night_car_colors_a` | `taillight_colors_dim` | the middle of the three tail-light colour blocks: lit by day, unlit at night |
| `taillight_colors_bright` | r1 `taillight_uv_bright` | `taillight_colors_bright` | as taillight_colors_dim |
| `headlight_colors_night` | r2 `night_car_colors_b` | `headlight_colors_night` | StartHeadLights (MAT3 static) writes them at +0x150 at night |
| `musyx_retrace_callback` | r1 `audio_retrace_callback`, r5 `musyx_retrace_callback` | `musyx_retrace_callback` | the game's MusyX glue is musyx_* (r5's eight service functions set/clear it) |
| `blitPrimColor_alpha` | r1 `blit_prim_alpha`, r2 `blitPrimColor` | `blitPrimColor_alpha` | blitPrimColor is blitPrimColor (r1 sure); this is its byte 3 |
| `paint_colors_rgb` | r2 `paint_colors_rgb`, ovl `car_color_palette` | `paint_colors_rgb` | InitVisuals converts these 32 RGB triples to paint_colors; one name |
| `stree_nodes` | r2 `gRoadSTree`, r3 `stree_nodes`, r4 `stree` | `stree_nodes` | MAT3 stree.c globals take the stree_ prefix as a family: bare surf is a parameter of TireSurf, which reads stree_surf, and save a local in 14 files |
| `stree_surf` | r2 `gRoadSurf`, r3 `stree_surf`, r4 `surf`, r5 `surf` | `stree_surf` | as stree_nodes |
| `stree_svert` | r2 `gRoadSVert`, r3 `stree_svert` | `stree_svert` | as stree_nodes |
| `stree_slist` | r2 `gRoadSList`, r3 `stree_slist` | `stree_slist` | as stree_nodes |
| `stree_vlist` | r2 `gRoadVList`, r3 `stree_vlist` | `stree_vlist` | as stree_nodes |
| `stree_save` | r2 `gRoadSave`, r3 `stree_save` | `stree_save` | as stree_nodes |
| `stree_savenum` | r2 `gRoadSaveNum`, r3 `stree_savenum` | `stree_savenum` | as stree_nodes |
| `zoid_node_position` | r1 `lookat_eye`, r2 `zoid_node_position` | `zoid_node_position` | it holds node->obj->pos of the node the scene draw is on; the reflection lookat only uses it as its eye |
| `frame_lookat_count` | r1 `frame_lookat_count`, r2 `lookat_count` | `frame_lookat_count` | reset each frame, counts la[] entries (sure over likely) |
| `gBgColor` | r2 `gBgColor`, r5 `clear_color` | `gBgColor` | ZOID_SetBGColor writes it as MAT3 zoidwindow.c gBgColor |
| `blit_cache_image` | r1 `blit_cache_image`, r2 `blit_cached_image` | `blit_cache_image` | the blitter's (r1) names |
| `blit_cache_tile` | r1 `blit_cache_tile`, r2 `blit_cached_tile` | `blit_cache_tile` | the blitter's (r1) names |
| `blitColors` | r1 `blitColors`, r2 `blitColors`, r3 `gBlitColorCache` | `blitColors` | MAT3 blit_pc.c blitColors (two maps sure) |
| `gPlayerFiles_head` | r1 `player_files_head`, r3 `gPlayerFilesHead`, ovl `gPlayerFiles_head` | `gPlayerFiles_head` | interior label convention <object>_<field> |
| `ObjInstanceList_mat` | r1 `obj_instance_mat`, r2 `ObjInstanceList_mat`, ovl `ObjInstanceList_mat` | `ObjInstanceList_mat` | interior label convention |
| `gFogIntensity` | r1 `fog_option`, r2 `gFogIntensity`, r3 `gFogIntensity`, r4 `gFogIntensity`, ovl `gFogIntensity` | `gFogIntensity` | four maps, MAT3 world.c gFogIntensity |
| `gMineCones` | r2 `gMineCones`, r4 `gMineCones`, r5 `gCarMines`, ovl `gMineCones` | `gMineCones` | cheatscr mirrors gMineCones into gCheatState[1] and gCarMines into [2] as MAT3 cheatscr.c:49-54; AnimateFrame's car-mine test reads gCarMines as MAT3 visuals.c:1154 reads gCarMines |
| `gCarMines` | r1 `gCarMines`, r2 `gCarMines`, r4 `gCarMines`, r5 `gMineCones`, ovl `gCarMines` | `gCarMines` | as gMineCones |
| `car_palettes` | r2 `car_palette_descs`, r3 `gCarPalettes` | `car_palettes` | code names are lower_snake_case |
| `car_palettes_palBase` | r1 `car_palette_data`, r2 `car_palette_desc_data`, r5 `car_palette_data` | `car_palettes_palBase` | interior label of car_palettes; field palBase |
| `gBlitInstanceList` | r1 `gBlitInstanceList`, r2 `gBlitInstanceList`, r5 `gBlitInstanceList`, ovl `blit_sprites` | `gBlitInstanceList` | three maps sure; gBlitInstanceList is its loader alias |
| `audio_lock_queue` | r1 `audio_cmd_lock`, r2 `audio_lock_queue`, r3 `gAudioLockQueue`, r4 `audio_lock_queue`, r5 `audio_cmd_lock` | `audio_lock_queue` | the N64 locks are OSMesgQueues: one family <owner>_lock_queue / <owner>_lock_msg (audio, heap, si, text, controlsys) |
| `audio_lock_msg` | r3 `gAudioLockMsgBuf` | `audio_lock_msg` | lock family |
| `audio_command_queue` | r1 `audio_cmd_queue`, r2 `audio_command_queue`, r3 `gAudioCommandQueue`, r4 `audio_command_queue`, r5 `audio_cmd_queue` | `audio_command_queue` | two maps; no abbreviation |
| `gObjList` | r1 `gObjList`, r2 `gObjList` | `gObjList` | the game image fills gObjList[0..385] from the model names; 0x8014254C (overlay B's label) is 314 entries lower, with gOutOfTime, gNumDrones and others in between |
| `gObjList_biased` | ovl `gObjList` | `gObjList_biased` | see gObjList |
| `gObjList_smoke` | r1 `obj_list_smoke`, r3 `smoke_animation_frames` | `gObjList_smoke` | it is gObjList + 196 entries: interior label (r2 names gObjList_hood, _tire, _rocket_* the same way) |
| `audiomgr_gCmds` | r1 `gAudioMsgs`, r3 `gCmds` | `audiomgr_gCmds` | MAT3 has two gCmds (audiomgr.c, audiosfx.c): file prefix |
| `audiosfx_gCmds` | r2 `gSfxCmds` | `audiosfx_gCmds` | as audiomgr_gCmds |
| `gTargetList_head` | r3 `gTargetListHead`, ovl `gTargetList_head` | `gTargetList_head` | interior label convention |
| `gPaks_rumble_request` | r2 `gPaks_rumble_level`, r4 `gPakRumbleRequest` | `gPaks_rumble_request` | interior label <object>_<field>; +0x7D is the request, +0x7E the current level (r1) |
| `gPaks_inserted` | r1 `paks_inserted` | `gPaks_inserted` | interior label convention |
| `gSfxInstances` | r1 `gSfxInstances`, r2 `gSfxInstances`, r5 `sound_voices` | `gSfxInstances` | two maps sure (MAT3 audiosfx.c) |
| `gPS_Files_head` | r2 `gPS_Files_head`, ovl `gPS_Files` | `gPS_Files_head` | gPS_Files is gPS_Files (r2, r3); _FindFile reads gPS_Files_head as gPS_Files[pak].head (+8) |
| `gNumInstances` | r1 `gNumInstances`, r2 `gNumInstances`, r5 `sound_voice_count` | `gNumInstances` | two maps sure (MAT3 audiosfx.c) |
| `gStoppedChannels` | r2 `gStoppedChannels` | `gStoppedChannels` | the audio init clears gStoppedChannels[i]; the shift loop stops at &gStoppedChannels_tail, its element 1 |
| `gStoppedChannels_tail` | r1 `gStoppedChannels` | `gStoppedChannels_tail` | see gStoppedChannels |
| `gOptions_metric` | r5 `option_metric`, ovl `gOptions_at_9` | `gOptions_metric` | gOptions elements: gOptions_<meaning> |
| `gOptions_show_coins` | r3 `gOptionsShowCoins` | `gOptions_show_coins` | gOptions elements: gOptions_<meaning> |
| `gOptions_fx_volume` | r4 `gOptionFxVolume`, r5 `options_sfx_volume` | `gOptions_fx_volume` | gOptions elements |
| `gOptions_fog_intensity` | ovl `gOptions_fogIntensity` | `gOptions_fog_intensity` | gOptions elements |
| `gOptions_circuit_death` | r5 `options_circuit_death` | `gOptions_circuit_death` | gOptions elements |
| `gOptions_battle_self_hit` | ovl `battle_self_hit_penalty_option` | `gOptions_battle_self_hit` | gOptions elements |
| `gFreeQ` | r1 `gFreeQ`, r2 `gSfxFreeQ` | `gFreeQ` | MAT3 audiosfx.c gFreeQ is unique in MAT3: verbatim |
| `gCmdQ` | r1 `gCmdQ`, r2 `gSfxCmdQ` | `gCmdQ` | MAT3 audiosfx.c gCmdQ is unique in MAT3: verbatim |
| `text_lock_queue` | r2 `gFontSem`, r5 `text_semaphore` | `text_lock_queue` | lock family |
| `text_lock_msg` | r5 `text_semaphore_msg` | `text_lock_msg` | lock family |
| `text_lock_inited` | r5 `text_semaphore_inited` | `text_lock_inited` | lock family |
| `blit_dl` | r1 `blit_dl`, r2 `blitDL`, r5 `text_dl_cursor` | `blit_dl` | the 2D display list the blitter (r1) and the text write; one name |
| `frame_dl` | r2 `gfx_cursor`, r3 `gDisplayListCursor`, r5 `display_list_cursor` | `frame_dl` | one family with blit_dl, frame_dl_start, last_frame_dl_end, frame_dynamic |
| `frame_dl` | r2 `gfx_cursor`, r5 `display_list_cursor` | `frame_dl` | the conventions of section 2 |
| `frame_dl_start` | r3 `gDisplayListFrameEnd`, r5 `frame_dl_start` | `frame_dl_start` | frame display list family |
| `last_frame_dl_end` | r3 `gStuntInfoEnd`, r5 `last_frame_dl_end` | `last_frame_dl_end` | a real datum (EndGraphics stores the closed cursor there); InitStunt only uses the address as gStuntInfo's end: wave 2 writes gStuntInfo + 4 |
| `yf` | r3 `gOPPFuncsEnd`, r5 `yf` | `yf` | View3Cam's per-camera blend, MAT3 camera.c static yf (the datum there); HAL_InstallOncePerFrameFunc only uses the address as gOPPFuncs' end: wave 2 writes gOPPFuncs + 10 |
| `font_image_format` | r1 `wave_debug_mode`, r2 `gFontTexFormat`, r3 `gFontTextureFormat`, r5 `font_image_format` | `font_image_format` | _SetupFontData sets G_IM_FMT_I or IA from the font header and _DrawText loads with it (r2, r3, r5); r1's compiled-out wave switch also stores it |
| `font_texel_size` | r2 `gFontTexSize`, r3 `gFontTextureSize` | `font_texel_size` | pairs with font_image_format |
| `gPlayers_save` | r2 `gPlayers`, r5 `gPlayers_save`, ovl `gPlayers_save` | `gPlayers_save` | gPlayers is gPlayers; this is gPlayers[0].save (+0x48), interior label |
| `blitMode` | r1 `blitMode`, r2 `blitMode`, r5 `gfx_cached_render_mode` | `blitMode` | two maps sure |
| `music_disabled` | r1 `music_disabled`, r2 `audio_low_memory` | `music_disabled` | AudioInitSys sets it from its argument; AudioSongPlay then does nothing |
| `song_number` | r1 `song_number`, r2 `gSongNumber` | `song_number` | code names are lower_snake_case |
| `gGhostFiles` | r1 `gGhostFiles`, r3 `gGhostFiles`, r4 `gGhostFiles`, ovl `gGhostFiles` | `gGhostFiles` | four maps; gGhostFiles_head is its head (+8) |
| `gGhostFiles_head` | r1 `ghost_files_head`, r5 `gGhostFiles`, ovl `gGhostFiles_head` | `gGhostFiles_head` | gGhostFiles is gGhostFiles (four maps); interior label |
| `musyx_project` | r1 `snd_project`, r2 `musyx_project` | `musyx_project` | the game's MusyX glue is musyx_* (r5); one prefix |
| `musyx_samples` | r1 `snd_sample_rom`, r2 `musyx_samples` | `musyx_samples` | as musyx_project |
| `musyx_sdir` | r1 `snd_sample_dir`, r2 `musyx_sdir` | `musyx_sdir` | as musyx_project |
| `musyx_pool` | r1 `snd_pool`, r2 `musyx_pool` | `musyx_pool` | as musyx_project |
| `musyx_task_done_queue` | r2 `audio_thread_queue`, r5 `musyx_task_done_queue` | `musyx_task_done_queue` | scTask.msgQ of the audio task the glue submits (musyx_submit_audio_task / musyx_wait_audio_task) |
| `musyx_task_done_msg` | r2 `audio_thread_messages` | `musyx_task_done_msg` | pairs with its queue |
| `heap_lock_queue` | r1 `heap_lock_queue`, r2 `heap_lock_queue`, r3 `gHeapSemaphore`, r4 `heap_lock_queue`, r5 `memory_lock_queue` | `heap_lock_queue` | three maps; lock family |
| `gMainHeap` | r1 `gMainHeap`, r2 `main_heap`, r4 `gMainHeap` | `gMainHeap` | two maps sure (MAT3 memsys.c) |
| `sample_dma_list` | r1 `sample_dma_list`, r5 `musyx_dma_request` | `sample_dma_list` | served by sample_dma_thread_entry (r1) |
| `HFOV` | r2 `HFOV`, r5 `field_of_view`, ovl `HFOV` | `HFOV` | two maps sure (MAT3 zoidwindow.c) |
| `expansion_pak_present` | r1 `expansion_pak_present`, r2 `all_tracks_open`, r3 `gExpansionPak`, r4 `expansion_pak_present`, r5 `expansion_pak_present`, ovl `expansion_pak_present` | `expansion_pak_present` | four maps sure |
| `bg_load_request` | r1 `bg_load_request`, r2 `bg_load_wave` | `bg_load_request` | what LoadWave posts on bg_load_request_queue (r1 family) |
| `gWaveInstance_ptr` | r1 `wave_instance_ptr`, r2 `gWaveInstance_ptr` | `gWaveInstance_ptr` | interior label convention |
| `frame_dynamic` | r1 `gfx_frame`, r2 `zoid_frame_gfx`, r5 `frame_dynamic` | `frame_dynamic` | the SDK term (a Dynamic of matrices, lookats, then glist); frame family |
| `frame_matrix_cursor` | r2 `zoid_matrix_cursor`, r5 `frame_matrices` | `frame_matrix_cursor` | it is a cursor (frame_matrix_cursor++ per node), inside frame_dynamic |
| `frame_record_index` | r2 `frame_buffer_index`, r5 `frame_record_index` | `frame_record_index` | sure over likely |
| `video_dither_filter` | r3 `gfx_video_mode`, r5 `video_dither_filter` | `video_dither_filter` | StartGraphics switches on it for OS_VI_DITHER_FILTER_OFF/ON; the video screen's reset clears it |
| `gShowPlayerArrows` | r1 `gShowPlayerArrows`, r2 `player_visibility_enabled`, r3 `gShowPlayerArrows`, r4 `gShowPlayerArrows` | `gShowPlayerArrows` | three maps sure |
| `gFadePlayerArrow` | r1 `gFadePlayerArrow`, r2 `player_visibility` | `gFadePlayerArrow` | MAT3 visuals.c gFadePlayerArrow s8[4][4] (mat3 sure over a code name) |
| `zoid_branch_slot` | r1 `scene_inline_dl`, r2 `zoid_branch_slot` | `zoid_branch_slot` | the N64 scene draw is zoid_* (r1's own function names) |
| `zoid_current_tlut` | r1 `scene_tlut_cache`, r2 `zoid_current_tlut` | `zoid_current_tlut` | zoid_* family |
| `zoid_tlut_mode` | r1 `scene_tlut_mode`, r2 `zoid_tlut_mode` | `zoid_tlut_mode` | zoid_* family |
| `gText_single` | r3 `gText`, r5 `text_single_strings`, ovl `gText_single` | `gText_single` | gText is gText; this is gText.single (+4), interior label |
| `inactiveBlit` | r2 `inactiveBlit`, r5 `idle_warning_blit` | `inactiveBlit` | InitBlits clears it as MAT3 InitBlits does and GameLoop keeps the idle-warning multiblit in it: MAT3 game.c inactiveBlit (likely over a code likely) |
| `gWprObjList_mine` | r5 `wpr_mine_object`, ovl `mine_cap_object` | `gWprObjList_mine` | interior label convention |
| `gWprObjList_mine_glow` | r5 `wpr_mine_glow_object`, ovl `mine_glow_object` | `gWprObjList_mine_glow` | interior label convention |
| `overlay_base` | r2 `overlay_base`, r3 `overlay_image_base`, ovl `overlay_area` | `overlay_base` | start_overlay/extra_overlay family |
| `gShotObjList` | r2 `extra_overlay_bss_start`, ovl `gShotObjList` | `gShotObjList` | image B's first BSS object; the overlay loader also passes the address as the BSS start: wave 2 gives the loader a named bound |
| `gTrophyMat` | r2 `start_overlay_bss_start`, ovl `gTrophyMat` | `gTrophyMat` | as gShotObjList (image A) |
| `extra_overlay_bss_end` | r2 `extra_overlay_bss_end`, ovl `extras_overlay_bss_end` | `extra_overlay_bss_end` | start_overlay/extra_overlay family |
| `start_overlay_bss_end` | r2 `start_overlay_bss_end`, ovl `frontend_overlay_bss_end` | `start_overlay_bss_end` | start_overlay/extra_overlay family |
| `start_overlay_loaded` | r2 `start_overlay_loaded`, r3 `gStartOverlayLoaded` | `start_overlay_loaded` | family; lower_snake_case |
| `extra_overlay_loaded` | r2 `extra_overlay_loaded`, r3 `gExtraOverlayLoaded` | `extra_overlay_loaded` | family; lower_snake_case |
| `controlsys_lock_queue` | r3 `gControlSemaphore`, r4 `si_lock_queue` | `controlsys_lock_queue` | InitControlSys creates controlsys_lock_queue with controlsys_lock_msg; the SI/pak lock is si_lock_queue (si_lock): two locks |
| `controlsys_lock_msg` | r4 `si_lock_msg` | `controlsys_lock_msg` | see controlsys_lock_queue |
| `si_lock_queue` | r1 `si_lock_queue`, r2 `pak_lock_queue` | `si_lock_queue` | the SI/pak lock of si_lock / si_unlock (r1) |
| `si_lock_msg` | r1 `si_lock_msg` | `si_lock_msg` | the conventions of section 2 |
| `wingOffsets` | r1 `wingOffsets` | `wingOffsets` | AnimateWing indexes wingOffsets[type][0..2]; wingOffsets_y is element [0][1] |
| `wingOffsets_y` | r2 `wingOffsets` | `wingOffsets_y` | see wingOffsets |
| `camera_nextSwitch` | r5 `nextSwitch` | `camera_nextSwitch` | MAT3 has two nextSwitch statics (camera.c [4], hud.c's ShowHUD); showhud.c's static is hud.c's |
| `FindWaveInstance` | r2 `FindWaveInstance` | `FindWaveInstance` | 0x800AC820 is the out-of-line FindWaveInstance (waves.c block); 0x8009768C is an empty retail stub the decomp gave a body |
| `FindWaveInstance_stub` | r1 `FindWaveInstance` | `FindWaveInstance_stub` | see FindWaveInstance |
| `InitBST` | r3 `InitBST` | `InitBST` | r3 has the real body (OrderFunc +0, numElements +4, root +8); 0x800986D4 is an empty stub |
| `InitBST_stub` | r1 `InitBST` | `InitBST_stub` | see InitBST |
| `ConvertHeap` | r1 `_ConvertHeap` | `ConvertHeap` | the real body (r1); 0x800A51D8 is the empty stub (r2) |
| `ConvertHeap_stub` | r2 `_ConvertHeap` | `ConvertHeap_stub` | see ConvertHeap |
| `video_validOption` | r3 `video_validOption` | `video_validOption` | the conventions of section 2 |
| `AddBattlePointInternal` | ovl `_AddBattlePoint` | `AddBattlePointInternal` | MAT3 _AddBattlePoint; AddBattlePoint is the global of the same file, so the dropped underscore takes the Internal suffix |
| `AudioPlayEffectInternal` | r1 `_AudioPlayEffect` | `AudioPlayEffectInternal` | MAT3 _AudioPlayEffect; AudioPlayEffect is the global of the same file, so the dropped underscore takes the Internal suffix |
| `CalcChecksumInternal` | r2 `_CalcChecksum` | `CalcChecksumInternal` | MAT3 _CalcChecksum; CalcChecksum is the global of the same file, so the dropped underscore takes the Internal suffix |
| `DeallocateMemoryInternal` | r1 `_DeallocateMemory` | `DeallocateMemoryInternal` | MAT3 _DeallocateMemory; DeallocateMemory is the global of the same file, so the dropped underscore takes the Internal suffix |
| `FindWaveInstanceInternal` | r1 `_FindWaveInstance` | `FindWaveInstanceInternal` | MAT3 _FindWaveInstance; FindWaveInstance is the global of the same file, so the dropped underscore takes the Internal suffix |
| `HAL_SetFontInternal` | r2 `_HAL_SetFont` | `HAL_SetFontInternal` | MAT3 _HAL_SetFont; HAL_SetFont is the global of the same file, so the dropped underscore takes the Internal suffix |
| `InsertBSTEInternal` | r1 `_InsertBSTE` | `InsertBSTEInternal` | MAT3 _InsertBSTE; InsertBSTE is the global of the same file, so the dropped underscore takes the Internal suffix |
| `PS_FlushFileInternal` | r2 `_PS_FlushFile` | `PS_FlushFileInternal` | MAT3 _PS_FlushFile; PS_FlushFile is the global of the same file, so the dropped underscore takes the Internal suffix |
| `ReadGhostDataInternal` | r3 `_ReadGhostData` | `ReadGhostDataInternal` | MAT3 _ReadGhostData; ReadGhostData is the global of the same file, so the dropped underscore takes the Internal suffix |
| `WalkBSTInternal` | r1 `_WalkBST` | `WalkBSTInternal` | MAT3 _WalkBST; WalkBST is the global of the same file, so the dropped underscore takes the Internal suffix |
| `animControllerDimInternal` | r1 `_animControllerDim` | `animControllerDimInternal` | MAT3 _animControllerDim; animControllerDim is the global of the same file, so the dropped underscore takes the Internal suffix |
| `text_lock` | r2 `_GetSemaphore_HALText` | `text_lock` | lock family: heap_lock, si_lock, audio_lock (r1) |
| `text_unlock` | r2 `_ReleaseSemaphore_HALText` | `text_unlock` | lock family |
| `text_lock_init_stub` | r5 `InitSemaphore_stub` | `text_lock_init_stub` | lock family, stub convention |
| `controlsys_lock_stub` | r3 `_GetSemaphore_ControlSys_stub` | `controlsys_lock_stub` | lock family, stub convention |
| `controlsys_unlock_stub` | r3 `_ReleaseSemaphore_ControlSys_stub` | `controlsys_unlock_stub` | lock family, stub convention |
| `gNumPlayers` | r1 `gNumPlayers`, r2 `gNumPlayers`, r3 `gNumPlayers`, r4 `gNumPlayers`, r5 `gNumPlayers`, ovl `gNumPlayers` | `gNumPlayers` | MAT3 gNumPlayers (s16; every front-end screen and resurrect.c use it as the player count); the loader's stale gNumPlayers at 0x80166004 is dropped (drop.tsv) |
| `src/blob/groups/stree_collide` | r2 `src/blob/groups/stree_surface`, r3 `src/blob/groups/stree_collide` | `src/blob/groups/stree_collide` | it holds stree.c CollideCarBody, CollideCarBodyAgainstSurface, CarApplyCollision, PointInSurf, RoadPosition (r3 sure) |
| `src/blob/groups/save` | r3 `src/blob/groups/save`, r4 `src/blob/groups/save_init` | `src/blob/groups/save` | the group holds most of MAT3 save.c (SaveServer, SaveGhost, CompressGhost, DecodeGhost, players and ghosts), not only SaveInit |
| `tAudioMsg.0x2` | r3 `cmd`, r4 `hdr.cmd` | `hdr.cmd` | field rows name the leaf member at the offset with its full path |
| `tAudioMsg.0x4` | r3 `song`, r4 `mono.mono` | `playSong.song` | a union: one row, the alternatives in the note |
| `tCarData.0x314` | r3 `mpctl`, r5 `mpctl.xrel` | `mpctl.xrel` | leaf member with its path |
| `tModelData.0x430` | r1 `tires`, r4 `tires[0].tradius` | `tires[0].tradius` | leaf member with its path |
| `tModelData.0xF4` | r4 `bodyR[0][0]`, r5 `bodyR` | `bodyR[0][0]` | leaf member with its path |
