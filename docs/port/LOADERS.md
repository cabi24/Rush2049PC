# Phase 4 converting loaders

What the PC build does to each file the game loads, after `_ReadWave` has it in its heap block and before
`SetupWaveModel` relocates it ([ASSETS.md](ASSETS.md) §2.5, §5), and what the game C does differently under
`TARGET_PC` to read the result. The hook and the dispatch by wave number are in `port/assets/assets.c`; each format
has its own file in `port/assets/`. Tests: `port/assets/tests/`, run by ctest.

## WAD container (`port/assets/wad.c`), 2026-10-27

Waves 22–37 (font textures), 54–119 (models, cars, tracks) and 120–138 (worlds, before `world.c` converts their own
chunks).

### The converter

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

The display-list walkers already used big-endian accessors (POINTERS.md §2): `relocate_model_dl`,
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
arena at their N64 addresses, which also makes `heap_release`'s `RaceEntry` view of `car_palettes_palBase` right.
`SetupCarColor` copies the car's WAD palette into its working buffer as native RGBA16 (`TLUT_COPY_IN`). The
descriptor's block is then native, which port/gfx reads natively because it isn't registered.

### Tests

- **`assets_wad`** (`tests/test_wad.c`). The converter on a synthetic big-endian WAD (`tests/wad_build.h`), built
  from the format as the game's code reads it. It checks:
  - every field comes out native, with shared path points swapped once;
  - the payload is untouched and registered;
  - each release path drops the registration (one wave, the heap free, a sub-heap, a stale block);
  - non-WADs and oversized chunks are left alone.
- **`assets_wad_game`** (`tests/wad_game.c`). The game's own code, built with the game flags, over the same WAD in
  an RDRAM window the test maps at 0x80000000. It exits 77 (skipped) if it can't map it. The test:
  - loads the WAD through `_ReadWave` (a PI DMA from a synthetic cartridge, then the conversion and
    `SetupWaveModel`);
  - checks the relocated tables and records, the TXLD rebase, the display-list relocation and LOD conditionals (one
    player and two), the vertex pass, and the mirror pass with its winding flips through a call and a branch;
  - checks `MBOX_FindTexture_Sub` and `MBOffsetWad` over a moved copy;
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
  render. Check `relocate_model_dl`'s 24-bit relocations, segment nibbles and the E1/04 branch on real data.

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
  which then take the N64 size. The heap addresses stored there fit in u32.

**Record pointers kept in game structs.** The scene nodes (`ZOID_SetTexture`, `AnimateFlag`'s palette), the
blits and the sprite list now keep texture and palette pointers whole, in one host layout (POINTERS.md 9). Left:
- **Overlay B's health palettes** `healthPal` have a palette record's shape but point at game data (`healthColor`,
  `healthColor`). The arenas are now at their N64 addresses in the window ([DATA.md](DATA.md) §3), so a u32 field
  can hold that address, and the records can take the PLHD N64 layout like the car palettes. Their own declaration
  still has the host layout (own storage, `data` a pointer), which works too.

## Text bank, font data, world, road and paths (2026-10-27)

Files: `port/assets/text.c`, `font.c`, `world.c`, `road.c`, `path.c`, and a test per format in
`port/assets/tests/`. Every converter checks that the whole file fits its block before it swaps anything, and
leaves a file that doesn't as it is (with a message): a bogus file is not half converted.

### What is converted, and how

| Format | Waves | Converter | Game edits under `TARGET_PC` |
|---|---|---|---|
| Text bank | 0–5 | swap: six u32 counts, the three u32 offset tables, the three u16 tables (padded to even counts) | `AdjustTextWavePointers` (`_AdjustTextWavePointers`) has a PC version: the offsets stay in the file, and gText's three pointer sections are native `char *` tables from `pc_text_strings`. The 19 reader files whose views of gText or its string table assumed N64 offsets get host ones (below) |
| Font data | 38–53 | none (all bytes); checks the letters and kern table fit | the 12-byte letter keeps its 4-byte kern slot as a u32 window address in `SetupFontData` (writer), `_DrawText`, `HAL_GetTextWidth`, `HAL_GetCharWidth`; `SetupFontData` reads the u32 heap handle slot |
| World WAD | 120–138 | swap WHDR (count, offsets), WOBJ (0x68-byte nodes), GDAT (0x1C), GTLD (u32 keys). Chunks are found through the WAD TOC in either byte order | `LoadTrack` gets GTLD as a native array of the keys (`pc_world_chunk`), since race setup writes object pointers over them; `NewWorld`'s WHDR entry view is 0x14 bytes (u32 node); its comparator `world_name_compare` takes pointers and returns its result |
| Road | 139–157 | swap by section; vlist and slist stay bytes | `ResetCollisionObject`'s save-record cursor is `intptr_t` |
| Path | 158–182 | swap the checkpoint block, the resurrection, alternative and drone path headers and points (drone mph and hints stay bytes) | `ReadPathData` has a PC version: headers copied field by field into the native `gResPath`/`gDronePaths`, alternative paths in a native array; eleven reader files get host offsets or strides (below) |

**Text bank views.** The header gText (`gText`, alias `gText`; `gText_single` = the single table) has
host pointers, so its fields sit at host offsets (8, 24, 32, ...) and its string tables have 8-byte entries. Views
that already declared pointer fields line up by themselves. The ones changed: header views padded with `s32[3]` or
`u8[12]` (`SetHighScoreState.c`, `animCreditText.c`, `animPauseText.c`, `animStatText.c`); overlays of the single table as a
struct with byte pads at N64 offsets, now `char *pad[N / 4]` (`anim_no_rumble_panes.c`, `anim_no_rumble_text.c`, `slp.c`, `animRumbleText.c`,
`animWrongWay.c`, `animLapCount.c`, `animTitleText.c`, `animSureText.c`, `licensescr_animText.c`, `warningscr_animText.c`, `titlescr_animText.c`,
`recordscr/recordscr.c`, `animBattlePoints.c`); string slots read as `s32` are `intptr_t` (`animTitleText.c`,
`animPauseText.c`, `animSureText.c`); `showhud.c`'s `u32 *` tables are `char **`; `pause_TitlePane.c`'s raw N64 offsets go through `TXH`
(header) and `TXS` (table). `titlescr_animText` reads entry 235 as `s32`: the low word of the host pointer, which is
the window address.

**Path views.** On PC `gResPath` is 32 bytes (pts at 8, numAlt at 16, alt at 24), an alternative path 24 (pts at
16), a drone path 16. Changed: the `Header`/`Descriptor`/`Count` views of `NumPtsToCheckpoint.c`,
`TracePathPtForward.c` and `TraceToIndex.c`; `CalcResPathBounds.c` (CalcResPathBounds); `NextResPathPt.c` (its
`* 0x10` stride is `ALT_PATH_SIZE`); `pathcommon_sync/group.c`; `find_res_pt_entry/group.c`; the `CountEntry` views of
`SetDroneResPathIndex.c` and `camera_pair.c`; the raw `* 8` drone strides of `NextPathPt.c` and `PrevPathPt.c`
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
  with `game_flags`); `test_rdram()` maps the RDRAM window so the files hold window addresses as heap blocks do.
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
- **Weak copies** of `SetupFontData` (in `ctx_gr3.c` and seven other context files) and of
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
