# Phase 4 converting loaders

What the PC build does to each file the game loads, after `_ReadWave` has it in its heap block and before
`SetupWaveModel` relocates it ([ASSETS.md](ASSETS.md) §2.5, §5), and what the game C does differently under
`TARGET_PC` to read the result. The hook and the dispatch by wave number are in `port/assets/assets.c`; each format
has its own file in `port/assets/`. Tests: `port/assets/tests/`, run by ctest.

## WAD container (`port/assets/wad.c`), 2026-10-27

Waves 22–37 (font textures), 54–119 (models, cars, tracks) and 120–138 (worlds, before `world.c` converts their own
chunks).

### Since 2026-11-09: native records beside the file

[NATIVE.md](NATIVE.md) step 7 changed what follows: the converter no longer swaps the file's table of contents and
record chunks in place. It reads them as the file has them (big-endian) and builds the four record chunks beside the
file, native, MAT3's fields with host pointers (`struct pc_wad_records`: `struct tModelDef` with its `struct tLod`
levels, `struct tTexture`, `struct tPaletteDef`, `struct tBasicTargetPath`; `port/include/pc_game_types.h`), kept by
the file's heap block (`port/assets/records.c`). Where the N64's `SetupWaveModel` added the file's or IMAG's address
to an offset in place, the converter makes the pointer: `displayList`, `vtxList` and `pts` into the file, `texBase`
and `palBase` past IMAG (32-bit arithmetic, as the game's: a texture's load list lies before IMAG), `trigPtr` to the
record of the same chunk it names (NULL for 0), `parent` NULL until `ResetPath` gives the path its target's record,
and the file's word there kept as `key`. The PATH points are still swapped in place (the game reads them through
`pts`); the display lists, texture loads, vertices and IMAG stay big-endian, and `SetupWaveModel` relocates the lists
and texture loads in place with the N64 addresses of the file and of IMAG (until step 10). Every game view of the
records went (the model draw, the texture, object and palette lookups, the blits, the fonts, the track map, the
texture animation, the target paths, the car palettes, the effects' strides: 42 game files changed); `MBSetupWad`,
`MBGetFromWad`, `MBOffsetWad`, `MBFindInWad` and `relocate_wave_model` are gone. The car palettes `car_palettes` are
palette records to the model draw, so they are `struct tPaletteDef` too (own storage, 52 of them: a race's slots or
the car select's 4 x 13), and so are the blits' own palettes `healthPal` and `n64FogPal` (they were `tBlitPalette`,
the same layout). Messages: a chunk that doesn't fit the file gives no records ("... is outside the file: none"); a
trigger that is no path record, a model with more than 4 levels, a display list that isn't an 8-aligned place in the
file, a texture without IMAG say so.

The rest of this section is the converter as it was written on 2026-10-27.

### The converter (2026-10-27)

In place, keeping every N64 record size (the game indexes records by hard-coded strides):

| Part | Swapped | Notes |
|---|---|---|
| header | u32 TOC offset, u32 count | |
| TOC | 3 × u32 per entry | then `MBSetupWad` relocates it as on the N64 |
| OBHD, count × 0x58 | 32 @0x10; 16 @0x14, @0x16; per LOD (all 4) 16, 16, 32, 32, 32 | |
| TXHD, count × 0x24 | 16 @0x10, @0x12, @0x16; 32 @0x18, @0x1C, @0x20 | |
| PLHD, count × 0x18 | 16 @0x12; 32 @0x14 | |
| PTHD, count × 0x24 | 32 @0x10; 16 @0x14, @0x16; 32 @0x18, @0x1C, @0x20 | |
| PATH points | 17 × 32 per point, numPts points from each PTHD `pts` | spans are merged, so points two paths share are swapped once |
| TXLD, display lists, Vtx, IMAG texels and TLUTs | nothing | stay big-endian |

- Only the first TOC entry of each tag is converted: `MBFindInWad` returns the first match, so later duplicates are
  never read.
- Pointer fields (`displayList`, `vtxList`, `texBase`, `palBase`, `trigPtr`, `pts`, `parent`) are swapped as u32.
  They are offsets in the file (IMAG-relative for `texBase`/`palBase`), and `SetupWaveModel` adds the file's or the
  IMAG chunk's window address. `trigPtr`'s target has no reader and is left as it is.
- The whole heap block is one `pc_mem_register_be` range. port/ultra's PI DMA and decompressors have registered the
  file already. The native records inside the range don't matter: port/gfx only asks about display lists, vertices,
  matrices, texels and TLUTs.
- A block that doesn't hold a WAD (a TOC outside the block, already relocated, a blank cartridge's empty file) is
  left alone and not registered. A chunk that runs past the block is left big-endian and reported.
- `RUSH_TRACE_WAVES=1` logs each conversion.

### Release

- Every free goes through `DeallocateMemoryInternal` (`src/blob/groups/heap_release/group.c`). Its first step poisons the
  freed block, and under `TARGET_PC` that call also reports the block to `pc_heap_block_freed` (`assets.c`).
  That drops the WAD blocks inside it (wad.c keeps a list) and the range a PI DMA or decompressor registered at its
  start. This covers `UnloadWaveInstance`, `ClearMemory`'s compaction and a sub-heap freed whole (the font heap).
- `pc_wave_released(wave, buf)` drops one wave's block on request.
- A WAD loaded over a block whose release was missed replaces the stale registration.
- WAD blocks never move. Compaction moves only handle blocks, and the only waves loaded as handles are the font data
  (38–53, `LoadWave(font + 38, .., handle = 1)`).

### The game C under `TARGET_PC`

All of these edits are N64-neutral: `n64_neutral.py --base 09da2f0` reports every file as "shifted". Types and
expressions change through file-scope macros whose N64 expansion is the original tokens (`WADP(type)` is `type *`
on the N64 and `u32` on PC), and no line inside a function body is added or removed.

The loader:
- **`waves.c`** (`SetupWaveModel`, `_ReadWave`):
  - The OBHD/TXHD/PLHD/PTHD pointer fields are u32.
  - `MBGetFromWad` returns u32, widened by zero extension. It was s32, so the 0x80xxxxxx addresses sign-extended.
  - `relocate_texture_loads` gets the `Gfx *` arguments its definition takes.
  - Handle waves read their u32 heap master slot (`HANDLE_BLOCK`) in `_ReadWave`, `SetupWaveModel` and the hook.
    `*(u8 **)` read 8 bytes, half of them the next slot. This mattered for the font data, which is loaded as handles.
- **The WAD handle** is `MBSetupWad`'s native `Table` (`{Header *, s32, Entry *}`) in every view:
  - `MBFindInWad`'s `H12` got a pointer-sized first field.
  - `MBOffsetWad` was m2c code at the N64 offsets. It has a PC version.

The display-list walkers already used big-endian accessors (POINTERS.md §2; since NATIVE.md step 10 they work on the
converter's native lists): `prepare_model_dl`,
`relocate_texture_loads`, `display_list_traverse`, `flip_triangle_winding` and `mirror_vertex_texcoords`.

The readers of the records:

| Reader | File | Change |
|---|---|---|
| mirror pass | `slot_sound/MirrorWaveInstance.c` (+ 2 copies) | `Part.dl` u32 |
| model draw, texture loads, TLUT load | `zoid_draw/group.c` (+ `resurrect/particle_context.c`) | `Texture.image`, `LodLevel.dl` u32; `Palette.table` u32 (it was s32: the TLUT address sign-extended into `gDPLoadTLUT`) |
| font draw | `hal_text/drawtext.c` | TXHD `image` u32 |
| `_SetupFontData` | `slot_sound/group.c` | `gTexDefPtrs` read as the 16-byte `{pointer, count}` pairs, not `{s32, s32}` |
| model part value | `ZOID_ChangeTexture.c` | `Part.dl` u32 |
| model radius | `MBOX_GetObjectRadius.c` | pair-table stride `sizeof` (16) |
| minimap texels | `hud_animTrackMap.c` | TXHD `data` u32, read through `TEXELS()` |
| scene objects on paths | `targets/group.c` | `Scene36.resources` (PTHD `pts`) u32, read through `PTHD_PTS()` |
| `MBOX_FindTexture_Sub`, `MBOX_FindPalette_WORK` | `ModelActive.c` | their slot test addressed `gWaveInstance` at N64 offsets (0x14 a slot). It has a PC version over the host layout |
| texture animation | `displaylist_get_texture_image.c`, `displaylist_set_texture_image.c`, `displaylist_set_tile_size.c` | big-endian reads and writes of the TXLD lists |
| | `TextureAnimation.c` | palette constants stored big-endian; `ObjectView.commands` (TXHD texBase) u32 |
| | `InitTextureAnimation.c` | `Resource32` (TXHD/PLHD) u32 fields; `displaylist_get_texture_image` returns u32 |
| car paint | `SetupCarColor/SetupCarColor.c`, `car_palette_init.c`, `AnimateBrakeLights.c`, `StartCar/startcar.c`, `loadcars/setup.c` | see below |

The car palette descriptors `car_palettes` are palette records to the model draw (`Model.palette`). So every view of
them takes the PLHD N64 layout: 24 bytes, with `data` a u32 that holds a heap block. They now live in the game
arena in its N64 layout (at their N64 addresses until [NATIVE.md](NATIVE.md) step 5, in its block since), which also
makes `heap_release`'s `RaceEntry` view of `car_palettes_palBase` right.
`SetupCarColor` copies the car's WAD palette into its working buffer as native RGBA16 (`TLUT_COPY_IN`). The
descriptor's block is then native, which port/gfx reads natively because it isn't registered.

### Tests

- **`assets_wad`** (`tests/test_wad.c`). The converter on a synthetic big-endian WAD (`tests/wad_build.h`), built
  from the format as the game's code reads it. It checks (since NATIVE.md step 7, no window):
  - every record field comes out native beside the file, its pointers into the file (past IMAG for the texels and
    colours) or to the trigger's record, with shared path points swapped once in the file;
  - the file's table of contents, records and payload are untouched, and the payload registered;
  - each release path drops the registration and the records (one wave, the heap free, a sub-heap, a stale block);
  - a world WAD keeps its WAD's records beside its world's; a missing chunk gives NULL and 0, as `MBGetFromWad` did;
  - non-WADs, oversized chunks, a trigger that is no record and a model with 7 levels.
- **`assets_wad_game`** (`tests/wad_game.c`). The game's own code, built with the game flags, over the same WAD in
  host memory (until [NATIVE.md](NATIVE.md) step 10 an RDRAM window the test mapped at 0x80000000). The test:
  - loads the WAD through `_ReadWave` (a PI DMA from a synthetic cartridge, then the conversion and
    `SetupWaveModel`);
  - checks the tables (the WAD's native records since NATIVE.md step 7), the TXLD rebase, the display-list relocation
    and LOD conditionals (one player and two), the vertex pass, and the mirror pass with its winding flips through a
    call and a branch;
  - checks `MBOX_FindTexture_Sub` (and, until step 7, `MBOffsetWad` over a moved copy);
  - runs the texture animation: set-up with `MBOX_FindPalette_WORK`, then one update (a light sequence, a texture
    scroll, flip-books on a TXLD and a texel texture).

  Reverting a view (`Part.dl`, `WADP`) or the big-endian palette stores makes it fail.

### Until the ROM is available

Checked on the retail cartridge on 2026-10-28 ([ASSETS.md](ASSETS.md) §8): every WAD converts whole. The record
chunks' count word is a record count, TXLD's and PATH's a byte length; `trigPtr` targets are PTHD records. What
was to be looked at:
- **The TOC count field.** For TXLD it is a byte length (`relocate_texture_loads(TXLD, TXLD + count)`). For the record
  chunks it is a record count. Check one real WAD: OBHD offset + count × 0x58 must not run into the next chunk.
- **Real WADs are left unconverted.** `RUSH_TRACE_WAVES=1` names any file wad.c refused (no TOC, chunk outside the
  block). Waves 69–75 and 61/79–81 have no known contents.
- **TXLD-list textures** (TXHD flags without 0x08000000). Their `texBase` is IMAG-relative like the others. In the
  test it points back at TXLD, which only works because the arithmetic is 32-bit. Check where real files put those
  lists.
- **PATH.** The points are swapped as 17 words from `pts`, as MAT3 does. A `PATH` TOC entry, if real files have
  one, isn't used.
- **`trigPtr` targets** are relocated but not swapped (no reader found).
- **The display lists.** A real model's display lists, through `--stop-at-gfx` and `--screenshot-at`, should
  render. Check `prepare_model_dl`'s 24-bit relocations, segment nibbles and the E1/04 branch on real data.

### Still open: other agents' or later work

**Reserved files.** They were not edited, so these changes are still to be made in them:
- **`groups/heap_release/UnloadWaveInstance.c`** (`UnloadWaveInstance`):
  - Its `Slot20` view of `gWaveInstance` is the N64 one (0x14 a slot, `allocation` at +0xC). On PC every other view
    has the host layout: 0x20 a slot, `ptr` at +0x10 and pointer-sized.
  - Its `memset`s of the pair tables use 8-byte entries. On PC the entries are 16 bytes.
  - It calls `DeallocateMemory_stub`, whose linked owner is an empty stub (`proto_check` class `stub`). On PC a wave's
    block is therefore never freed when it is unloaded.

  No release call is needed there: the heap free reports the block.
- **`ovl_a/groups/trackscr/trackscr.c`**: the `MapLod` view of the OBHD LOD (`u32 *displayList; void
  *verts;`) needs u32 fields under `TARGET_PC`. The records are struct-copied to and from `saved_map_lods`/`mapObj`,
  which then take the N64 size. The heap addresses stored there fit in u32. (Done otherwise since NATIVE.md step 7:
  `mapObj` and `saved_map_lods` are `struct tLod`, own storage, copied whole into the native records.)

**Record pointers kept in game structs.** The scene nodes (`ZOID_SetTexture`, `AnimateFlag`'s palette), the
blits and the sprite list now keep texture and palette pointers whole, in one host layout (POINTERS.md 9). Left:
- **Overlay B's health palettes** `healthPal` have a palette record's shape but point at game data (`healthColor`).
  Since [NATIVE.md](NATIVE.md) step 4 they and the track select's fog palette `n64FogPal` are `tBlitPalette`
  records (own storage, `palBase` a pointer), which the blit draw reads as such; since step 7 the WAD's PLHD records
  are native too, and all of them are one type, `struct tPaletteDef`.

## Text bank, font data, world, road and paths (2026-10-27)

Files: `port/assets/text.c`, `font.c`, `world.c`, `road.c`, `path.c`, and a test per format in
`port/assets/tests/`. Every converter checks that the whole file fits its block before it swaps anything, and
leaves a file that doesn't as it is (with a message): a bogus file is not half converted.

### What is converted, and how

| Format | Waves | Converter | Game edits under `TARGET_PC` |
|---|---|---|---|
| Text bank | 0–5 | swap: six u32 counts, the three u32 offset tables, the three u16 tables (padded to even counts); then the bank's `struct TextTables` beside the file (since 2026-11-09, [NATIVE.md](NATIVE.md) step 8) | `AdjustTextWavePointers` (`_AdjustTextWavePointers`) copies the bank's tables into gText (`pc_text_bank`): the offsets stay in the file, and gText's three pointer sections are native `char *` tables. The 19 reader files whose views of gText or its string table assumed N64 offsets got host ones (below), and since wave 2 every reader uses the one declaration |
| Font data | 38–53 | none (all bytes); checks the letters and kern table fit; since 2026-11-09 ([NATIVE.md](NATIVE.md) step 7) builds the letters beside the file (`struct FontLetter`, 0x10, its kerning row a pointer into a copy of the table), kept with the block as the heap moves it | until step 7 the 12-byte letter kept its 4-byte kern slot as a u32 window address in `SetupFontData` (writer), `_DrawText`, `HAL_GetTextWidth`, `HAL_GetCharWidth`; since, `SetupFontData` takes the converter's letters (`pc_font_letters`) and the readers read `kerns` as the pointer it is |
| World WAD | 120–138 | swap WHDR (count, offsets), WOBJ (0x68-byte nodes), GDAT (0x1C), GTLD (u32 keys). Chunks are found through the WAD TOC in either byte order; then the world's records beside the file (`struct pc_world_records`: the name table with host pointers to the roots, GDAT, GTLD's keys with room for their targets; since 2026-11-09) | `LoadTrack` points the world's globals at the records (`pc_world_records`) instead of finding the chunks with `MBSetupWad`/`MBGetFromWad` and relocating the names; `NewWorld` bsearches the native `NameEntry` table (24 bytes, `node` a pointer); its comparator `world_name_compare` takes the name and an entry and returns its result |
| Road | 139–157 | swap by section; vlist and slist stay bytes | `ResetCollisionObject`'s save-record cursor is `intptr_t` |
| Path | 158–182 | swap the checkpoint block, the resurrection, alternative and drone path headers and points (drone mph and hints stay bytes); then the headers decoded beside the file (`struct pc_path_records`, the points host pointers into it; since 2026-11-09) | `ReadPathData` copies the decoded headers into the native `gResPath`/`gDronePaths`, the alternative paths into its own array; eleven reader files got host offsets or strides (below) |

**Text bank views** (2026-10-27; since wave 2 every file reads gText through the one `TextTables`). The header gText (`gText`, alias `gText`; `gText_single` = the single table) has
host pointers, so its fields sit at host offsets (8, 24, 32, ...) and its string tables have 8-byte entries. Views
that already declared pointer fields line up by themselves. The ones changed: header views padded with `s32[3]` or
`u8[12]` (`SetHighScoreState.c`, `animCreditText.c`, `animPauseText.c`, `animStatText.c`); overlays of the single table as a
struct with byte pads at N64 offsets, now `char *pad[N / 4]` (`anim_no_rumble_panes.c`, `anim_no_rumble_text.c`, `video_TitlePane.c`, `animRumbleText.c`,
`animWrongWay.c`, `animLapCount.c`, `animTitleText.c`, `animSureText.c`, `licensescr_animText.c`, `warningscr_animText.c`, `titlescr_animText.c`,
`recordscr/recordscr.c`, `animBattlePoints.c`); string slots read as `s32` are `intptr_t` (`animTitleText.c`,
`animPauseText.c`, `animSureText.c`); `showhud.c`'s `u32 *` tables are `char **`; `pause_TitlePane.c`'s raw N64 offsets go through `TXH`
(header) and `TXS` (table). `titlescr_animText` read entry 235 as `s32`: the low word of the host pointer, which was
the window address (it reads the pointer since wave 2).

**Path views.** On PC `gResPath` is 32 bytes (pts at 8, numAlt at 16, alt at 24), an alternative path 24 (pts at
16), a drone path 16. Changed: the `Header`/`Descriptor`/`Count` views of `NumPtsToCheckpoint.c`,
`TracePathPtForward.c` and `TraceToIndex.c`; `CalcResPathBounds.c` (CalcResPathBounds); `NextResPathPt.c` (its
`* 0x10` stride is `ALT_PATH_SIZE`); `pathcommon_sync/group.c`; `find_res_pt_entry/group.c`; the `CountEntry` views of
`SetDroneResPathIndex.c` and `SetResurrectionData.c`; the raw `* 8` drone strides of `NextPathPt.c` and `PrevPathPt.c`
(`DRONE_PATH_SIZE`). The other views (about 20) already had the native layout. The checkpoint block (812 bytes,
no pointers) is copied as it is; its views all read the swapped widths. Overlay A's track-map `loadPathData`
calls the same ReadCheckpointData and ReadPathData and needs nothing.

**Readers audited.** Road: the 20 functions that read the sections (found from the PC build's relocations: only
strong definitions count) use exactly the swapped widths and record strides. World nodes: `NewWorldObject`,
`FindWorldGroup` and `SetupTarget`'s `Place` view read them at the swapped widths (`Place.w68`/`w72`
overlay the s16 pairs but are never read). Paths: every live view of the checkpoint block and the point arrays
(s16 triples; the drone points' bytes 6 and 7 are only read as bytes). Text: the u16 index views are u16 fields
and unread byte pads.

### Verification (no ROM)

- `ctest`: `assets_text`, `assets_font`, `assets_world`, `assets_road`, `assets_path` build synthetic big-endian
  files from the formats as the game's code reads them, convert them, and run the game's own code on them: the
  text parser, `titlescr_animText` and `pause_TitlePane`, also after a language switch; `SetupFontData` and
  `HAL_GetTextWidth` with kerning, before and after a heap move of the font block; `NewWorld`'s bsearch over a
  converted world WAD (TOC big-endian or already native, with or without a WOBJ entry); `InitSTree` and
  `ResetCollisionObject`; `ReadCheckpointData`, `ReadPathData`, `NextResPathPt`, `NextPathPt` and
  `PrevPathPt`. Test files name the game sources they link on ` * game: ` lines (CMakeLists.txt compiles them
  with `game_flags`); their heap blocks are the tests' memory (`test_memory()`, a host block: until
  [NATIVE.md](NATIVE.md) step 11 an RDRAM window the tests mapped, so the files held window addresses).
  Since 2026-11-09 (NATIVE.md step 8) `assets_text`, `assets_world` and `assets_path` also check the records each
  converter builds beside its file (`port/assets/records.c`: the text bank's tables kept where they are across a
  language switch, the world's names and target groups, the paths' headers, and each record going with its block),
  and need no window: the files sit in host memory.
- `n64_neutral.py --base 09da2f0`: every changed game file is "shifted".
- Census: 389 → 381 pointer warnings (the text parser, `showhud`, `fb9c8`, `ResetCollisionObject`), the same two
  classes.
- `rush2049pc --no-rom --headless --seconds 10` reaches `sndInit` and stops at the MusyX alloc callback
  (`sound_hw_start_output_thread`, `hardware.c:95`; MUSYX_DATA.md §8.1), the same point as main. Before this, the N64 text
  parser wrote into the heap on a blank cartridge.

### Open, and what to check with the ROM

- **`gGameCar` swallowed the path globals**: fixed. gen_data laid `gGameCar` out as 8 × 0x3B8 cars, but
  `sample_dma_pi_queue` (an `OSMesgQueue`) at +0x1650 shows it is 6 cars, and the pointer-sized globals after it overlapped
  their neighbours inside its storage (`gAltPaths` `gAltPaths`, `gPathPtList` `gPathPtList`, `gPathWave`
  `gPathWave`, which ReadPathData writes). A known size now bounds the declarations, and `extents.txt` gives
  6 cars, so each of those has storage of its own ([DATA.md](DATA.md) §1).
- **Handle waves in `waves.c`** (not mine): `_ReadWave` (`dst = *(u8 **)r->ptr`, three times), the
  `pc_wave_loaded` hook and `SetupWaveModel` read a handle's u32 heap slot as 8 bytes. Font data (38–53) is the
  handle wave: it is read to, and converted at, the wrong address until those read `(u8 *)(uintptr_t)*(u32 *)ptr`.
  `SetupFontData` already does.
- **`gTexDefPtrs` in `SetupFontData`**: its `Pair {s32 first; s32 second}` view (the font textures,
  `gFontTextures[i] = first + i * 36`) has to match whatever layout the WAD part gives the `{pointer, count}` tables
  (ASSETS.md §4 #19): `intptr_t first` if they stay native, as now in gen_data.
- **Weak copies** of `SetupFontData` (in `TB_GetFontHeight.c` and seven other context files) and of
  `AdjustTextWavePointers` keep their N64-sized views; they never run.
- Checked on the retail cartridge ([ASSETS.md](ASSETS.md) §8.1): none of the messages below appears for any wave;
  WOBJ is the tag and its count word a record count (the converter now takes it as records only, and walks from
  WHDR when it doesn't fit); the text tables and path counts end their walks at the file's end. The list was:
- With the ROM: run `RUSH_TRACE_WAVES=1` to the front end and the first race and look for the converters'
  "runs past" / "no WHDR that fits" messages. Check that the TOC count word of WOBJ is a record count (the
  converter also accepts bytes), that `WOBJ` is the tag (else the converter walks from WHDR, which covers only the
  nodes a world reaches), that GTLD keys equal the nodes' `tidx` (sign-extended both ways), that the text bank's
  u16 tables and the res/alt/drone point counts match the layout above (the walk must end within the file), and
  the menus' strings in every language.
