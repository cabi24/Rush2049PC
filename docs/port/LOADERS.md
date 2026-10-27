# Phase 4 converting loaders

The converters that put the game's files in host byte order as `_ReadWave` loads them (`port/assets/`, hooked in
`waves.c` through `pc_wave_loaded`; [ASSETS.md](ASSETS.md) §5 is the work list). Each part has its own heading.

## Text bank, font data, world, road and paths (2026-10-27)

Files: `port/assets/text.c`, `font.c`, `world.c`, `road.c`, `path.c`, and a test per format in
`port/assets/tests/`. Every converter checks that the whole file fits its block before it swaps anything, and
leaves a file that doesn't as it is (with a message): a bogus file is not half converted.

### What is converted, and how

| Format | Waves | Converter | Game edits under `TARGET_PC` |
|---|---|---|---|
| Text bank | 0–5 | swap: six u32 counts, the three u32 offset tables, the three u16 tables (padded to even counts) | `func_800A4E58` (`_AdjustTextWavePointers`) has a PC version: the offsets stay in the file, and gText's three pointer sections are native `char *` tables from `pc_text_strings`. The 19 reader files whose views of gText or its string table assumed N64 offsets get host ones (below) |
| Font data | 38–53 | none (all bytes); checks the letters and kern table fit | the 12-byte letter keeps its 4-byte kern slot as a u32 window address in `sound_update_channel` (writer), `_DrawText`, `object_manager_update`, `HAL_GetCharWidth`; `sound_update_channel` reads the u32 heap handle slot |
| World WAD | 120–138 | swap WHDR (count, offsets), WOBJ (0x68-byte nodes), GDAT (0x1C), GTLD (u32 keys). Chunks are found through the WAD TOC in either byte order | `LoadTrack` gets GTLD as a native array of the keys (`pc_world_chunk`), since race setup writes object pointers over them; `NewWorld`'s WHDR entry view is 0x14 bytes (u32 node); its comparator `pointer_offset_wrapper` takes pointers and returns its result |
| Road | 139–157 | swap by section; vlist and slist stay bytes | `listener_position_set`'s save-record cursor is `intptr_t` |
| Path | 158–182 | swap the checkpoint block, the resurrection, alternative and drone path headers and points (drone mph and hints stay bytes) | `audio_effect_apply` (`ReadPathData`) has a PC version: headers copied field by field into the native `gResPath`/`gDronePaths`, alternative paths in a native array; eleven reader files get host offsets or strides (below) |

**Text bank views.** The header gText (`D_8017A4E0`, alias `countdown_state`; `D_8017A4E4` = the single table) has
host pointers, so its fields sit at host offsets (8, 24, 32, ...) and its string tables have 8-byte entries. Views
that already declared pointer fields line up by themselves. The ones changed: header views padded with `s32[3]` or
`u8[12]` (`func_800F1210.c`, `gr3_a53c.c`, `fb7fc.c`, `func_803A4340.c`); overlays of the single table as a
struct with byte pads at N64 offsets, now `char *pad[N / 4]` (`gr3_6874.c`, `f6b3c.c`, `slp.c`, `fbc84.c`,
`gr3_8ab0.c`, `gr3_edc.c`, `fb5d0.c`, `fb9c8.c`, `func_803A6A28.c`, `func_803AF744.c`, `func_803AE51C.c`,
`runtime_a_callers_deep/group.c`, `func_803936A8.c`); string slots read as `s32` are `intptr_t` (`fb5d0.c`,
`fb7fc.c`, `fb9c8.c`); `showhud.c`'s `u32 *` tables are `char **`; `voice.c`'s raw N64 offsets go through `TXH`
(header) and `TXS` (table). `func_803AE51C` reads entry 235 as `s32`: the low word of the host pointer, which is
the window address.

**Path views.** On PC `gResPath` is 32 bytes (pts at 8, numAlt at 16, alt at 24), an alternative path 24 (pts at
16), a drone path 16. Changed: the `Header`/`Descriptor`/`Count` views of `difficulty_select.c`,
`lap_count_select.c` and `split_time_display.c`; `func_800B9740.c` (CalcResPathBounds); `func_800B9F60.c` (its
`* 0x10` stride is `ALT_PATH_SIZE`); `route_cross/group.c`; `entry_dispatch/group.c`; the `CountEntry` views of
`func_800C4CF8.c` and `camera_pair.c`; the raw `* 8` drone strides of `func_800B9338.c` and `func_800DC080.c`
(`DRONE_PATH_SIZE`). The other views (about 20) already had the native layout. The checkpoint block (812 bytes,
no pointers) is copied as it is; its views all read the swapped widths. Overlay A's track-map `loadPathData`
calls the same ReadCheckpointData and ReadPathData and needs nothing.

**Readers audited.** Road: the 20 functions that read the sections (found from the PC build's relocations: only
strong definitions count) use exactly the swapped widths and record strides. World nodes: `differential_output`,
`func_800AB53C` and `transmission_ratio_get`'s `Place` view read them at the swapped widths (`Place.w68`/`w72`
overlay the s16 pairs but are never read). Paths: every live view of the checkpoint block and the point arrays
(s16 triples; the drone points' bytes 6 and 7 are only read as bytes). Text: the u16 index views are u16 fields
and unread byte pads.

### Verification (no ROM)

- `ctest`: `assets_text`, `assets_font`, `assets_world`, `assets_road`, `assets_path` build synthetic big-endian
  files from the formats as the game's code reads them, convert them, and run the game's own code on them: the
  text parser, `func_803AE51C` and `voice_stop`, also after a language switch; `sound_update_channel` and
  `object_manager_update` with kerning, before and after a heap move of the font block; `NewWorld`'s bsearch over a
  converted world WAD (TOC big-endian or already native, with or without a WOBJ entry); `InitSTree` and
  `listener_position_set`; `ReadCheckpointData`, `ReadPathData`, `func_800B9F60`, `func_800B9338` and
  `func_800DC080`. Test files name the game sources they link on ` * game: ` lines (CMakeLists.txt compiles them
  with `game_flags`); `test_rdram()` maps the RDRAM window so the files hold window addresses as heap blocks do.
- `n64_neutral.py --base 09da2f0`: every changed game file is "shifted".
- Census: 389 → 381 pointer warnings (the text parser, `showhud`, `fb9c8`, `listener_position_set`), the same two
  classes.
- `rush2049pc --no-rom --headless --seconds 10` reaches `sndInit` and stops at the MusyX alloc callback
  (`func_80010C68`, `lib_11640.c:95`; MUSYX_DATA.md §8.1), the same point as main. Before this, the N64 text
  parser wrote into the heap on a blank cartridge.

### Open, and what to check with the ROM

- **`player_array` swallows the path globals.** gen_data lays `D_80152818` out as 8 × 0x3B8 cars (`GameCar
  player_array[8]` in 17 files), but `D_80153E68` (an `OSMesgQueue`) at +0x1650 shows it is 6 cars. The globals
  after it are placed inside its own storage through tCarData's runs, so pointer-sized ones overlap their
  neighbours: `gAltPaths` `D_80153E80` (8 bytes) over `D_80153E84`, `gPathPtList` `D_80153EF8` (32 bytes) over
  `D_80153F08` and `D_80153F10`, `gPathWave` `D_80153F20` over `D_80153F24`. ReadPathData writes all three. Fix in
  port/data: 6 elements (the C declarations, or an extent gen_data prefers over complete declarations).
- **Handle waves in `waves.c`** (not mine): `_ReadWave` (`dst = *(u8 **)r->ptr`, three times), the
  `pc_wave_loaded` hook and `SetupWaveModel` read a handle's u32 heap slot as 8 bytes. Font data (38–53) is the
  handle wave: it is read to, and converted at, the wrong address until those read `(u8 *)(uintptr_t)*(u32 *)ptr`.
  `sound_update_channel` already does.
- **`gTexDefPtrs` in `sound_update_channel`**: its `Pair {s32 first; s32 second}` view (the font textures,
  `D_80149820[i] = first + i * 36`) has to match whatever layout the WAD part gives the `{pointer, count}` tables
  (ASSETS.md §4 #19): `intptr_t first` if they stay native, as now in gen_data.
- **Weak copies** of `sound_update_channel` (in `ctx_gr3.c` and seven other context files) and of
  `func_800A4E58` keep their N64-sized views; they never run.
- With the ROM: run `RUSH_TRACE_WAVES=1` to the front end and the first race and look for the converters'
  "runs past" / "no WHDR that fits" messages. Check that the TOC count word of WOBJ is a record count (the
  converter also accepts bytes), that `WOBJ` is the tag (else the converter walks from WHDR, which covers only the
  nodes a world reaches), that GTLD keys equal the nodes' `tidx` (sign-extended both ways), that the text bank's
  u16 tables and the res/alt/drone point counts match the layout above (the walk must end within the file), and
  the menus' strings in every language.
