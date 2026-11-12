# Byte order in the game C

Where the game writes a value at one width and reads its bytes at another, the N64 (big-endian) and the PC
(little-endian) see different values: a word's first byte is its high byte on the N64 and its low byte here. This
document covers how such places were found (`tools/port/endian_check.py`), what was fixed, and what is left.
2026-10-27. Related: [POINTERS.md](POINTERS.md) 9 (record layouts), [DATA.md](DATA.md) (how initialized data is
filled), [LOADERS.md](LOADERS.md) (ROM files), [GFX.md](GFX.md) (what the renderer reads).

## 1. The rule

- **Memory keeps the N64's byte order wherever the game reads bytes.** A colour is R, G, B, A in memory, as the
  model draw (`prim[4]`, `env[4]`), the fill and font colours (`BlitRect`, `BlitColors`) and the renderer
  take it. A word that only moves a colour around (a copy, an equality test) is then right as it is; a word that is
  built or interpreted as a number must be converted.
- **A value held as a word and read back in parts must take its parts from where the PC keeps them.** The low half
  of an s32 is its second s16 on the N64 and its first here.
- **Initialized data must be declared the way the game reads it.** The PC loader byte-swaps every element of the
  game's initialized data by the declaration gen_data chooses (DATA.md 1, 4): a view with byte pads where other
  files read words leaves those words big-endian, and a word view of bytes reverses them.

`PC_RGBA(x)` (`port/include/pc_prelude.h`) is the one conversion macro: a colour word as the N64 holds it in a
register (`0xRRGGBBAA`) to the word whose bytes in memory are R, G, B, A. It is `__builtin_bswap32` (on the N64,
until its build was dropped, `x`).

## 2. The detector

```
python3 tools/port/endian_check.py [--build build/pc] [-j 2] [--reuse]   # build/pc/endian/report.txt
python3 tools/port/endian_check.py --reuse --only src/blob/foo.c --dump  # every access of one file
python3 tools/port/endian_check.py --selftest --check-loader            # ctest endian_check
```

A full run, which compiles the 1,300 game units again, takes about a minute with `-j 2` on the Pi 5;
`--reuse` keeps the dumps that are newer than their source, and then the analysis alone takes about 35 seconds.

### 2.1 Method

1. **Accesses.** Every live game file (the `game.dir` entries of `compile_commands.json`) is compiled again with
   its own flags plus `-O1 -fno-inline -fdump-tree-forwprop1-lineno`. GCC's GIMPLE at that pass has each memory
   access as one expression with its access type and base: `MEM[(u8 *)p_3 + 61B]`, `gBlitInstanceList[_5].prim[1]`,
   `MEM <unsigned int> [(char * {ref-all})&c]`. This sees through M2C_FIELD, casts and unions alike. Functions
   that gen_weak makes weak (context copies) are skipped.
2. **Types.** The object's DWARF (gen_data.py's parser) gives each base's declared type, so each access gets a host
   offset, an N64 position (through the view's N64 layout) and a width, and is checked against the scalar it falls
   on (section 5 of the report: puns).
3. **Storage.** Each access is put in a storage family: a data symbol (aliases at one address are one), what
   parameter k of F points to, what a pointer loaded from a given storage position points to, what one call
   returned, a local, or one function's untyped pointer. Pointers are followed within a function from `&symbol...`,
   loads, calls, parameters, casts, constant offsets and PHIs (to a fixed point). Positions in arrays of structs are
   taken in one element.
4. **Value flow.** Values are *made* where they are constants (not 0 or -1), arithmetic, conversions, or the PC
   loader's elements of initialized data (from `build/pc/pc-data/data.c`'s runs, at the width it swaps), and
   *interpreted* where they are shifted, masked, ordered, compared with a constant, converted or used as an array
   index. In between they move unchanged: loaded and stored at the same width, passed as arguments, returned, or
   inside a struct assignment or a struct passed by value (every scalar place in it, and the loader's elements of
   initialized data with them). A pointer argument connects the caller's storage with the callee's parameter
   family at the argument's offset, one way for the callee's reads and the other for its writes (values never cross
   from one caller's storage to another's). A `void *` or byte-pointer parameter given more than 12 storages
   (memcpy, bzero, buffer helpers) is not followed, nor are the parameters in `NOT_FOLLOWED` (StartBlast's `data`,
   an s16 car slot or an f32 position by its `linkToCar` argument).
5. **Conflicts.** In each storage, a value made at one (position, width) and interpreted at an overlapping other
   (position, width) is a conflict (section 1). Score 5 for a constant or ROM data, 4 for arithmetic or a
   conversion, 3 for a call's or parameter's value; +1 across files. `REVIEWED` in the tool lists the conflicts
   that were read and judged, with the reason, and `LEFT` the initialized data left to other parts (section 4
   here); they are reported last.
6. **Other sections**: every storage accessed at mixed widths with each access's flow (2); s16 reads at +2 of a
   place also accessed as a 4-byte integer, the N64's low half of a word (2b); initialized data accessed at a width
   other than the loader's element (3); copies between views whose scalars overlap at different widths (4);
   initialized data passed by address to a `u8 *` or `void *` parameter where the loader swapped words (4b).

### 2.2 How well it works

- `--selftest` builds nine synthetic cases (a word constant read as bytes in another function, bytes read as a
  shifted word, two s16 halves read as one s32, a local union, a word made in a caller's local and copied through a
  parameter, a byte-built colour copied as a word, a word moved by a struct assignment, a union passed by value,
  and an s32 handle whose second s16 is returned and used as an array index) and checks that it finds the eight
  conflicts and the two low-half reads, and nothing in the byte-built copy or the untouched flags.
- On the game: 2,500 live functions, about 47,700 accesses, 90% of them with a known storage. Run on the game C as
  it was before the fixes (the branch's base), section 1 had 72 conflicts: **24 real ones in 11 storages, all fixed
  here** (section 3); 20 real ones in the initialized data left to other parts (section 4: the loader's union rule,
  gTargInfo); 23 by design (the controller pak's page links, ULTRA.md); and **5 false positives**, a discarded
  native read in `prepare_model_dl` (4 byte positions) and route_cross's `LEN(k)`, element k+1's word at +8,
  against element 0's `count`. StartBlast's two kinds of `data`, merged in its parameter, gave 2 more before it went
  into `NOT_FOLLOWED`. Section 2b listed 25 low-half reads in 9 views, all real and all fixed (the handle views of
  3.1 and `TPos`). Sections 3 and 4b (initialized data read at another width, or passed as bytes) led to the
  rest of 3.3: the flags, the viewports, the pane colours, MaxPath's message (TB_DrawText's parameter takes too
  many storages for the value flow to follow). After the fixes: 0 open conflicts, 61 reviewed, 0 low-half reads.
  The loader's union rule (section 4) then removed the 11 conflicts left to it, with none new.
- **Limits.** No call contexts (a parameter that takes different records merges them). A pointer that comes from
  nowhere the tool follows (an integer, a pointer loaded through an untyped pointer) is one function's storage. Byte
  loops with a variable index have no position. The order of writes is not known, so ROM data that the game
  overwrites before reading still counts. A view that the PC reorders (the handle unions below) reads its parts at
  other N64 positions than the N64 view; those conflicts are marked reviewed.

## 3. What was fixed

All edits are N64-neutral (`n64_neutral.py --base 09da2f0`: every file shifted, 0 FAIL): PC views under
`#ifdef TARGET_PC` at file scope, and `PC_RGBA(x)` on existing lines.

### 3.1 Scene-node handles (`e6f3e1d`, `85e3b26`)

MBOX_NewObject returns a node index, which the game keeps as an s32 and reads back as the second s16 of
`union { s32 handle; struct { s16 high, index; } part; }`: the low half on the N64, the high half (0) here. So these
set frames, colours, textures and palettes on node 0, and removed node 0:

| View | File | Records |
|---|---|---|
| `Id` | `UpdateSmoke.c` | exhaust and smoke puffs |
| `Id`, `EntityHdr` | `AnimateCoin.c` | coins (the entity's id, the coin's model) |
| `Id` | `AnimateNewBlastRing.c` | blast rings `gBlastRingObjs` |
| `Effect.handle` | `start_smoke/StartSmoke.c` | track effects (ZOID_SetTexture) |
| `Object.handle` (`OBJECT_HANDLE`) | `ovl_b/runtime_b_resource_callbacks/group.c` | battle power-ups |
| `Handle` | `SetupCarColor/SetupCarColor.c` | the car's paint job (SetModelPalette on `gCarParts[i].root`, `.secondary`) |
| `PlayerRec` | `SetFrameColor.c` | the car's colour on its six part nodes (`gCarParts[i]` +0x0, +0x10, +0x1C..+0x28) |

Under `TARGET_PC` each view puts the low half first; the s32 and the halves keep their meaning. `Handle` and
`PlayerRec` are views of the car parts table `gCarParts`; its size and offsets are unchanged. Since 2026-11-09
SetupCarColor, SetFrameColor and the power-ups read include/game's records instead (`CarParts`, `tTarget.dlIdx`)
and take a handle's node as `(s16)handle`, its low half on either byte order.

### 3.2 Colour words from the ROM (`7aa03ff`)

Writers that copy a scene node's colour whole from initialized data the game declares as words: the ROM holds
R, G, B, A, the loader swaps the word, and the node got A, B, G, R.

| Data | Reader | What |
|---|---|---|
| `smoke_white`..`smoke_dirt_color` | `StartSmoke` | track effects' surface colours |
| `controls_initButtonColor` | `controlscr/controls_initButtons.c` | controls screen buttons |
| `controls_updateButtonColor` | `controlscr/controls_updateButtons.c` | controls screen buttons (then the alpha byte is stored over byte 3) |
| `coin_gold_glow_color`, `coin_silver_glow_color` | `AnimateCoin.c` | gold and silver coins (0xFFC800FF, 0xC8E6FFFF) |

Each read is `PC_RGBA(...)`. No game code stores a colour constant as a word into byte-read storage (the detector
found none; the only constant is 0xFFFFFFFF, which is the same in any order).

### 3.3 Initialized data declared as the game reads it (`1db4c2e`, `34a06ec`, `85e3b26`, `9f17add`)

| Symbol | View changed | Was | Now |
|---|---|---|---|
| `gBackRects` background rectangles | `updateBG.c` `Motion` | 16 pad bytes where createBG reads min/max x, y as f32 | `f32 bounds[4]` |
| `gCheatState` gCheatState | `game_sounds/InitOptions.c` `Rec164A4` | byte pads, 34 bytes, where cheatscr.c reads s16[21] | s16 throughout, 42 bytes |
| `pause_gPlayerRects` gPlayerRects | `animRumbleText/animRumbleText.c` `TPos` | 18 pad bytes and an s16 `x` at +18 (the s32 centre's low half) | pause.c's s32 layout; `x` is the whole centre, which the s16 local takes the same |
| `hud_playerPaneColors` playerPaneColors | `hud_text_anims/animResults.c` | `Pos {s32 x, y}` | R,G,B,A bytes (ChangePaneColor reads bytes) |
| `carscr_buttonInfo` front-end button slots | `ovl_a/anim_car_color_marker.c`, `carscr_animNewSlider.c` `MenuSlot` | `float matrix[16]` chosen: the alpha byte at +0x3C swapped as part of an f32 | the record's fields (as cleanup_colortrace's `Slot`) |
| `cont_bit_pattern`, `gResetLastKeyTime`, `gIgnoreCount` | `controlsys/group.c` | s32 | u8, s8, u8: the osContInit bit pattern (the boot waits while it reads 0x80), gResetTick, gSkipFrames |
| `gStopAllEffects` | `audiosfx_play/group.c` | s32 | s8, as every other file |
| `vp1`..`vplr` split-screen viewports | `ZOID_SplitScreen.c`, `ZOID_SplitMenuScreen.c` | s32: the first two halves swapped together, the other 12 bytes left big-endian | `Vp` (s16 scale[4], translate[4]) |
| `modescr_button_color` the seven-option menu's button colour | `ovl_a/runtime_a_seven_option_init/group.c` `Color4` | `{ u32 word; }`: R,G,B,A swapped as a word, then copied whole into the nodes | `u8 c[4]` |
| `target_steer_zero_dist_message` MaxPath's message (TB_DrawText at 80, 160) | `maxpath/group.c` | s32: the first four characters reversed | `PcText4 { char c[4]; }` (the rest, up to the next symbol, is copied as it is) |

`endian_check --check-loader` checks the loader's element widths at these places in `data.c`.

### 3.4 Checked and right

- The text pipeline: the font colour is built and copied as bytes (`HAL_SetTextColor`, `_SetDefaults`), and the text
  command buffer stores the colour as bytes and the alpha as an explicit big-endian float.
- ChangePaneColor, InitPane, the pane and fill colours, the sprite fills (`BlitRect`), BlitColors: bytes.
- ZOID_SetColor and its copies: a word copy of a colour built as bytes (`clr.c.r = 255` ...), right as it is.
- Light and lookat records (`lookat_reflect_fill`): bytes. Display-list words the game builds: by the GBI macros'
  shifts. The checkpoint block's `s16` reads of an `s32`-declared `gCheckpoints`: the path converter swaps the block
  by its fields, so the s16 reads are right.
- What the report's sections 3, 4 and 4b still list: in 3, only gTargInfo (`LEFT`, section 4 here); section 3
  leaves out accesses that only move narrower elements as a wider value (a word copying a colour's bytes keeps
  their order, and section 1 follows where it goes) and counts them. In 4b, all marked reviewed
  (`BYTES_REVIEWED`): the viewports handed to gSPViewport (s16 elements, which the renderer reads natively) and
  float matrices and vectors passed as `void *` (read as words). In 4, copies that move a record whole
  (InitRushControls's, the path blob's, the pak's inodes). Initialized data passed by address to a parameter with no
  pointee type (a K&R prototype) is only matrices and vectors as well (checked once, not a report section).

## 4. Left, and for other parts

- **The loader's union rule: done (port/data, [DATA.md](DATA.md) 2).** gen_data laid a union out by its widest
  member with the most non-byte elements, so the game's colour unions (`TextColor`, `tRGBA`, `RGBA`, `Color`,
  `Color4`: `{ u8 c[4]; u32 rgba; }` or `{ u32 word; struct { u8 r, g, b, a; } c; }`) were swapped as u32, and the
  bytes the game reads came out A, B, G, R. Now a union with a member made only of bytes that covers all of it is
  copied as bytes (not with a pointer or a `long` in it). Checked against every union in the layout: the 21
  colour symbols of initialized data with such a union are read as bytes, directly or after word copies, and their
  words are only copied or tested for equality (no shift, no comparison with a constant, no word constant stored),
  so none is read both ways; the other unions of initialized data (`Vtx`, `Gfx`, `MenuSlot`) have no such member
  and keep their layout, and BSS is zeroed. The 22nd, `flashColors_at_1`, is `flashColors[1]` under image A's `struct Color4 { u32 word; }` (since 2026-11-09
  the screens read `flashColors[1]` and the label has left the loader's tables): bytes now
  take part in the arena's overlap resolution, and the union's bytes rank first (DATA.md 3). Loaded as bytes now:
  `gFlashColor1`/`gFlashColor2` (the menus' flashing text colours), `flashColors`/`flashColors_at_1` (their source pairs,
  TB_FlashTextColors), `arrowColors` and `arrow_base_color` (arrows), `gShadowColor`, `SheenColors` and `Track1Colors` (the
  cars' shadow, sheen and IR colours, GetCarsIRColor), `options_buttonColor`, overlay B's `hudextra_playerColor`, `stunt_coin_color`/`A4`,
  `hitColor`, overlay A's `carscr_solidColor`/`58`, `circuitscr_init_button_color`/`5C`, `loadTrackScreen_backColor`, `race_map_vertex_color`, `trackscr_init_button_color`/`98`.
  The 11 conflicts `LEFT` held for them are gone (none new), the game C needed nothing more, and none of them takes
  `PC_RGBA`. `--check-loader` checks their widths, and `rush2049pc_datacheck` checks them against the images.
- **gTargInfo `targetInfo`** (race logic: the level-object kinds). gen_data chooses `TargInfo`, which has byte pads
  at +0x10..0x1B and +0x20..0x2F where other views read s32 and f32 (StartCone, PlayStartSound, level_objects),
  so those stay big-endian. Its seven views also disagree on pointers (POINTERS.md 9.9); the fix belongs with that
  record's PC layout. Done: every PC view now spells out each field at its width, and endian_check no longer reports
  the table (POINTERS.md 10.1).
- **The boot's `video_default_x`: done ([DATA.md](DATA.md) 1).** Declared `s16[32]` at 0x8002AFFC (and
  `video_default_y` at 0x8002AFFE), it ran over `gInitFlag` (0x8002B028) and `decompress_lock_ready` (0x8002B030), so
  the loader swapped those flags with their neighbour bytes; `main_thread_entry` reads `gInitFlag` as loaded. The two are
  columns 2 and 3 of one `s16[3][4]` table from `video_default_extra_width` to `video_pending_frames`, read as
  `[osTvType][0]`, so under `TARGET_PC` `include/m2c_types.h` declares them `s16[9]`, which reaches the last
  element read (the N64 line is under `#else`). Overlaps of this kind are now reported, bytes included
  (DATA.md 3): none open.
- **The controller pak** (`src/rom` Pfs): page links are `{bank, page}` bytes next to native u16 `ipage` tests,
  by design (ULTRA.md, controller pak); the detector's conflicts there are marked reviewed.
- `sound_hw_set_envelope` (`src/rom/sound/hardware.c`, MusyX) reads `__unaligned` u16s natively; MUSYX_DATA.md's.
- Car and model records (another part): `UpdateCarSounds.c` (`UpdateCarSounds`) and `PauseTargets` read and write car and
  level-object fields at N64 offsets inside pads; the widths agree, but the offsets are the layout work's.
- Weak context copies keep their N64 views; they never run.

### Without the ROM

- Whether the ROM's initial values of the flags above are 0 (then the old declarations were harmless).
- That the colour data above really is R, G, B, A (every reader reads it as such; the coins' are documented).
- The detector's ROM check knows only what `data.c` describes: data reached through a pointer the game loads from
  data is not checked.

## 5. Verification

- `ctest`: `assets_endian` runs the coin updater, the car colour (`SetFrameColor`) and the car paint job on
  synthetic records (the handles' low halves reach the right nodes; the gold and silver colours as the loader
  leaves them come out R, G, B, A in the node). With the N64 views the checks fail: node 0 gets the coin's object,
  its colour, the car's colour and its palettes. `endian_check` runs the detector's synthetic cases and the
  loader-width checks (21 places; 78 with the union rule and the viewport table's flags). 55 tests pass.
- `n64_neutral.py --base 09da2f0` over every changed game file: 0 FAIL.
- `rush2049pc --selftest`: 26 of 26. `rush2049pc --no-rom --headless --no-config --audio null --seconds 10` stops
  where it did (AudioSfxInit, `AudioSfxInit` → AllocateMemoryHeap, on blank data).
- Census: the same two pointer-warning classes, no new ones.
- On x86-64 (Rocky Linux 10, gcc 14, `tools/port/remote.sh`): the build, its 64 tests, the self-test and the boot
  as above. The detector runs there too (gcc 14's dumps add discriminators to locations): 0 open conflicts, 51
  reviewed, the same kinds as on the Pi. With the union rule: 40 reviewed; the 2 open ones, in the car records
  `gGameCar` (BSS: bytes +0x4D and +0x4E set by `init_view3`, the word at +0x4C read), were there before it
  and are not the loader's.
- On Windows (MSYS2, gcc 16) the detector finds none of its synthetic cases, so its GIMPLE-dump reading doesn't
  fit that compiler yet; the `endian_check` test runs only `--check-loader` there. Run the detector on Linux.
