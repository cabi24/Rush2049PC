# Phase 4 asset survey: what the game loads from the ROM, and how the PC build must read it

Survey of 2026-10-26, from code and documentation only (no ROM bytes were read). It is the work list for the
converting loaders of [PORT_PLAN.md](../PORT_PLAN.md) Phase 4.

Tags:
- **[C]** verified in this repository's C or disassembly (file:line given).
- **[M]** from the MAT3 (GameCube) decompilation. Its `waves.c`, `HAL_text.c`, `text.c`, `world.c`, `tracks.c`,
  `stree.c`, `pathcommon.c` and `targets.c` come from the same game source, and the N64 functions follow them
  statement for statement. MAT3's data is little-endian, so its `_swap_x16`/`_swap_x32` calls give the width of
  every field the PC must swap. The N64 build kept many of those loops with empty bodies
  (`read_path_data/audio_effect_apply.c:13`).
- **[R]** from `rush2recompiled/docs/rush2049_research/*.md` and `docs/rush2049_port_plan.md`, not re-checked here.

Function names are the repository's (often historical labels); the real role is given beside them.

## 1. Summary

- **There is one asset loader: MAT3's "waves".** `LoadWave` (`audio_frame_sync`, 0x80097798) loads file *n* of a
  183-entry file table into a game-heap block. `_ReadWave` (`func_80097164`) reads it raw (PI DMA), LZSS or raw
  DEFLATE. For model files, `_SetupWaveModel` (`SetupWaveModel`, 0x80096CA8) then relocates the file in place.
  Otherwise the game image only DMAs from the ROM in one other place: sound samples (MusyX). Boot inflates the
  game image and the two runtime images, which the PC links natively anyway.
- **Every loaded file lands in the game heap**, which runs from `align32(0x8017A640)` to `osMemSize | 0x80000000`.
  The overlay region 0x8038A400–0x803BB380 is a fixed-address block reserved inside that heap.
- **11 file formats** plus 2 code-image formats. The model container (WAD) carries 8 chunk types.
- **The game relocates in place with 32-bit arithmetic.** It does `+= (u32)base`, masks to 24 bits with a segment
  nibble, makes pointers with `| 0x80000000`, tests bit 31 to mean "already relocated", indexes records by
  hard-coded strides (`* 88`, `* 36`, `idx * 8`), and overlays the same records with several different struct views.
  Widening pointers inside loaded records would break all of these.
- **Recommendation: keep the asset world 32-bit.** Put the game heap inside the RDRAM window at host 0x80000000
  (`PC_RDRAM_BASE`, `port/include/pc_port.h`). Then every N64 address the game computes is a valid host address.
  Loaded records keep their N64 sizes, with their pointer fields declared `u32` under `TARGET_PC`. The converting
  loader only byte-swaps scalar fields in place, by format, in one hook in `_ReadWave` before `SetupWaveModel`. Display lists,
  vertices, texels and TLUTs stay big-endian, and the wave buffer is registered with `pc_mem_register_be`.
  About 12 game functions that walk or patch that payload get big-endian accessors.

## 2. Load paths

### 2.1 Loaders

| Loader | Address | Loads | Source | Compression | Destination | Tag |
|---|---|---|---|---|---|---|
| `inflate_entry(rom, dst, 0)` from `game_init` | 0x80006814, call 0x80002334 (`src/rom/lib_2cf0.c:91`) | game image | ROM 0xB0CB10 (`gRomCompressedData`) | raw DEFLATE | 0x80086A50 fixed; Huffman tables at fixed 0x803FD120 (0x2EE0 B) | C |
| `PrevMaxPath` (overlay load); callers `sync_maxpath_to_checkpoint` 0x800A133C ("Start" = image A), `InitMaxPath` 0x800A1244 ("Extra" = image B), `display_enable` 0x800C8FA4 | 0x800A11E4 (`src/blob/groups/heap_release/overlay_loader.c:43-69`) | runtime image A or B | ROM pointers `D_8002B020` (A, 0xB5C534) / `D_8002B024` (B, 0xB6FEC4) | raw DEFLATE via `inflate_decompress` 0x800026C0 | 0x8038A400, reserved in the heap by `NextMaxPath` (allocate at address, 0x8038A400–0x803BB380); BSS zeroed | C |
| `LoadWave` (`audio_frame_sync`) | 0x80097798 (`src/blob/groups/audio_frame_sync/loadwave.c:72`) | wave *n* | file table §2.2 | per wave | heap block of `(gWaveSizes[n] + 31) & ~31` from `AllocateMemoryHeap` 0x80097470 or `AllocateHandleHeap` 0x800974EC | C |
| `_NewWaveInstance` (`func_800972BC`, inlined) | 0x800972BC (`loadwave.c:43`) | allocates the slot in `gWaveInstance[64]` 0x80156D38 | | | | C |
| `_ReadWave` (`func_80097164`) | 0x80097164 (`src/blob/groups/audio_frame_sync/waves.c:220-255`) | the wave's bytes | `D_8011B5BC[n]` | `D_80123564[n]`: 0 raw PI DMA (inlined `func_8009715C`, queue `D_80035410`), 1 `lzss_decompress` 0x80002660, 2 `inflate_decompress` 0x800026C0 | wave buffer; then `SetupWaveModel(slot, 1)` | C |
| `BGLoadWaveMain` (`brake_force_apply`) | 0x800AC75C, thread 8 (`audio_frame_sync/group.c:3764`) | background loads (`LoadTrack(.., background=1)`) | request queue `D_80156BB0`, done queue `D_80035440` | as `_ReadWave` | wave buffer | C |
| `ReplaceWave` (`suspension_setup`) | 0x800A501C (`replacewave.c:40`) | swaps the text bank in its slot | | | reuses the buffer, sized by the first (largest) text wave | C, M (`gLargestTextWave`) |
| `SetupWaveModel` / `MBSetupWad` / `MBOffsetWad` / `MBGetFromWad` | 0x80096CA8 / 0x80096C28 / 0x80096BBC / 0x80096B5C | WAD relocation (§3.1) | | | in place | C |
| MusyX sample DMA: `struct_init_and_call` (no direct caller; reached through the MusyX callback table `D_80038000`) → `func_8008ABE4` → `osPiStartDma`; thread `task_complete_signal` | 0x8010FD1C, 0x8008ABE4 (`groups/dma_queue_next/func_8008ABE4.c:40-50`), 0x8008AC74 | sample data | cart address of ROM 0x39B40 + sample offset, from the address callback `D_80038014` (by signature `func_8010FD60`: `(a & 0x0FFFFFFF) \| 0xB0000000`) | none (ADPCM) | MusyX RAM buffers from the alloc callback `D_80038018` (by signature `resource_alloc_init` 0x8010FC80, heap) | C |
| `main` | 0x800020F0 (`lib_2cf0.c:16-20`) | 16 words at cart 0xFFB000 via `osPiReadIo`, then discarded | | | stack | C |
| `DecodeGhost` → `inflate_entry_alt` | 0x800CBE08 → 0x8000697C (`groups/menu_save_options/ctx_slot.c:209`) | ghost data from the Controller Pak, not the ROM | RAM | DEFLATE | buffer | C |
| Stream bank `func_80025AB4` / `func_800252AC` (boot library) | | 8 or 16 kHz streams, index table `u32 = rate << 24 \| offset` | | | | C: no caller in the game, image A or image B (unused) |

Decompressors [C, from `asm/us/nonmatchings/rom/lib_5610/*.s`]:
- **DEFLATE**: raw (no zlib header). The input is streamed in 4 KB PI DMA chunks through `gInflateBufferA/B`
  (0x80084A50/0x80085A50) and the output written straight to `dst`. `inflate_entry` returns the output length.
  With flag 1 its 0x2EE0-byte table scratch comes from the heap. PC: zlib `inflateInit2(-15)` from `pc_rom_bytes()`.
- **LZSS** (`lzss_decode` 0x80004AFC): a flag byte, read LSB first, where 1 marks a literal and 0 a pair `b0 b1`.
  The distance is `((b0 & 0xF0) << 4) | b1`, a back distance from the output pointer (no ring buffer). The length
  is `(b0 & 0xF) + 2`. Distance 0 with length nibble 0 ends the stream. The input is streamed in 4 KB DMA chunks.
  It is still assembly, so it needs a small C version on PC.
- The compressed size is never stored. Both decoders stream until their end marker.

### 2.2 The file table (183 waves)

| Table | Address | Element | Region | Tag |
|---|---|---|---|---|
| ROM offset | `D_8011B5BC` | u32 | game .data | C (`waves.c:235`) |
| size in RAM (after decompression) | `D_8002E580` (`gWaveSizes`) | u32 | boot .data | C (`loadwave.c:56`) |
| storage kind (0 raw, 1 LZSS, 2 DEFLATE) | `D_80123564` | s32 | game .data | C (`waves.c:229-251`) |
| model flag (1 = WAD, relocate) | `D_801234AC` | s8 | game .data | C (`audio_frame_sync/group.c:3812`) |
| initial text wave | `D_8002E860` | u8 | boot .data | C (`src/blob/func_800A5158.c:19`) |

The 183-entry count is checked by address arithmetic. `0x8011B5BC + 183*4 = 0x8011B898` is where the next table,
track 1's PVS [R], starts. `0x8002E580 + 183*4 = 0x8002E85C` and `D_8002E860` follows. `0x801234AC + 184 = 0x80123564`.
All four tables are plain integers, so `port/data` can generate them by type.

`gWaveInstance` (0x80156D38, 64 × 0x14) [C `waves.c:44-55`]: `s8 loaded, ready, locked, model; u8 mirrored;
s8 handle; u8 wave; u32 size @8; u8 *ptr @0xC; u8 *lastPtr @0x10`. With `handle` set, `ptr` is a handle, a pointer
to a heap master slot.

### 2.3 Wave map

Wave numbers come from the call sites [C]. Names come from the MAT3 call site at the same place in the same function
[M], with MAT3's wave numbers in parentheses.

| Waves | Contents | Format (§) | Loaded by | MAT3 file |
|---|---|---|---|---|
| 0–5 | text bank per language: EN, ES, IT, DE, NL, FR | Text 3.3 | `InitTextWave` (`func_800A5158` 0x800A5158) loads `D_8002E860`; `func_800A510C` calls `ReplaceWave` for the current language | `texte`..`textf.lzs` (0–5) |
| 6 | MusyX project | 3.8 | `AudioInitSys` (`car_shadow_render` 0x800A5D34, `groups/audio_init_sys/car_shadow_render.c:80`), locked | — |
| 7 | MusyX pool | 3.8 | same, `:83` | — |
| 8 | MusyX sample directory | 3.8 | same, `:86` | — |
| 9 | sample data, raw ADPCM, 0x2D2A30 B [R] | 3.8 | not loaded as a wave. The code passes ROM 0x39B40 (`D_00039B40`, `:89`) to `sndPushGroup` and DMAs on demand | — |
| 10–21 | songs 0–11 | 3.8 | `func_800979A0` (song start, `groups/slot_sound/func_800979A0.c:38`): wave `song + 10`, locked | — |
| 22–37 | 16 font texture WADs | WAD 3.1 | `_HAL_SetFont` (`slot_state_setup` 0x800B4200): wave `font + 22`, heap `D_80114740` | `*fonttex.lzs` (8–23) |
| 38–53 | 16 font metric files, loaded as handles | Font 3.7 | same: wave `font + 38`, `handle = 1` | `*fontdat.lzs` (24–39) |
| 54, 58, 59 | front-end WADs, locked | WAD | `resource_slots_clear_multiple` 0x800C937C (MAT3 `_LoadFrontEndWaves`) | `attract` (40), `selcom` (44), `selopt` (45) |
| 55 / 56 / 57 / 60 | front-end screen WADs | WAD | `playgame_state_change` 0x800CA3B4 (`groups/state/state.c:303/244/239/256`), states 0x20000000 / 0x100 / 0x4000 / 0x80 | `interface` (41) / `selcar` (42) / `selcirc` (43) / `seltrk` (46) |
| 61 | extra race WAD, with 80, when `D_80156994` (Expansion Pak) or `gTrackNum >= 6` | WAD | `LoadTrack`; image A `func_803A0FD8` | N64 only [R: effects] |
| 62 | HUD | WAD | `LoadTrack` (`groups/loadtrack/loadtrack.c:174`) | `hud` (47) |
| 63 / 67 | battle HUD (`gRace == 6`) / stunt HUD (`gRace == 4`) | WAD | `LoadTrack :189/:191` | `hudbattle` (48) / `hudstunt` (52) |
| 64 | common HUD | WAD | `:181` | `hudcom` (49) |
| 66, 65 | split-screen HUD (`gNumWindows >= 2`); 65 also needs Expansion Pak or track ≥ 6 | WAD | `:183/:185` | `hudmulti` (51), `hudlogo` (50) |
| 68 | targets | WAD | `:161` | `targetsnopak` (53) |
| 69–75 | no load or lookup site in the game, image A or image B | — | — | [R] lists effects, coins, stunt icons and flags among 61–79 |
| 76 | weapons (`gRace == 6` or tracks 6–13) | WAD | `:158` | `weapons` (55) |
| 77 | wings | WAD | `:172` | `wings` (56) |
| 78 / 79 | vehicles (track 18, or race tracks with Expansion Pak) / the variant used without it | WAD | `:163-169` | `vehicles` (57) / N64 only |
| 80 / 81 | wheels [R]: 80 with Expansion Pak or track ≥ 6, else 81 | WAD | `:176-179` | N64 only |
| 82–87 | per-track animated objects, race tracks 0–5 (Expansion Pak) | WAD | `:167`, wave `gTrackNum + 82` | `targetst1..6` (58–63) |
| 88–100 | 13 cars | WAD | `LoadCars` 0x800BB140 (`groups/loadcars/root.c:61,75`, `loadAgain = 1`); image A `func_80390BC0` (car select) | `car1..13` (64–76) |
| 101–119 | 19 track geometries: 0–5 race, 6–13 DM1–8, 14–17 stunt, 18 obstacle; with PTHD/PATH | WAD | `LoadTrack` 0x800BB9B0 (`loadtrack.c:96`), wave `track + 101` | `track1`..`obstacle1` (233–251) |
| 120–138 | worlds: placement trees | World WAD 3.4 | `loadtrack.c:106` | `*worlds` (252–270) |
| 139–157 | road: collision | Road 3.5 | `loadtrack.c:129` | `*road` (271–289) |
| 158–176 | forward AI and checkpoint paths | Path 3.6 | `loadtrack.c:142`; image A track select `loadPathData` (`func_8038CCEC`, `src/ovl_a/groups/runtime_a_c93c_closure/group.c:1276`, tracks 0–5) | `*path` (290–308) |
| 177–182 | backward paths, race tracks 0–5 (`gBackwards` `D_80152570`) | Path 3.6 | `loadtrack.c:142` | `*pathback` (309–314) |

`LoadWave` also re-runs the mirror pass on an already loaded model wave (`loadwave.c:79-81`).

### 2.4 Where data lands

| Data | Where | Tag |
|---|---|---|
| Heap | `func_800E7D04` (`groups/audio_heap/group.c:236-245`): base `((u32)D_8017A640 + 31) & ~31` (end of game BSS), end `osMemSize \| 0x80000000`; `D_80156994 = 1` (Expansion Pak features) when the end is above 0x80400000. First-fit allocator with 0x20-byte block headers, magic 0xFEDCBA98, handle tables of `u32` slots (`:159`, `count * 4`). Handle blocks can move (compaction, `groups/func_800E7A98/heap_release_context.c:151-257`). | C |
| Every wave | a heap block (`AllocateMemoryHeap` / `AllocateHandleHeap`) | C |
| Game image | 0x80086A50 fixed (PC: linked natively, skip the inflate) | C |
| Runtime images | 0x8038A400 fixed, reserved inside the heap (PC: `pc_data_overlay_load`, no inflate) | C |
| Text bank pointers, font letter pointers, GTLD pointers | written into the loaded file itself | C |
| Checkpoints, resurrection and drone path headers | copied into game BSS (`D_80151CE8`, `D_801407F0`, `D_8012E5E8`) | C |

### 2.5 What the PC `_ReadWave` does

1. Kind 0: `memcpy` from `pc_rom_bytes() + D_8011B5BC[n]`, `gWaveSizes[n]` bytes. Kind 1: C LZSS. Kind 2: zlib raw
   inflate.
2. Call the converting loader (§5) by wave range, before `SetupWaveModel`.
3. For WADs, `pc_mem_register_be(buffer, size)`. `UnloadWaveInstance` (`func_80096130`, 0x80096130) and heap
   compaction must unregister or move the registration. Only handle waves move, and those are font data, which has
   no big-endian payload.

## 3. Formats

### 3.1 WAD container (model waves 22–37, 54–119; worlds 120–138)

Table of contents [C `src/blob/MBSetupWad.c:10-44`, `MBGetFromWad.c`, `func_80096B00.c` (`MBFindInWad`); M `mb_wad.c`]:

| Off | Field | Size | Load fixup |
|---|---|---|---|
| file +0 | TOC offset | u32 | replaced by the absolute TOC address. If bit 31 is already set, the WAD counts as relocated and is skipped (`MBSetupWad.c:35`) |
| file +4 | TOC count | u32 | |
| TOC entry +0 | tag | u32 (four ASCII chars) | |
| +4 | offset | u32 | `+= (u32)file` |
| +8 | length (bytes) or record count | u32 | |

Tags fetched by name: `OBHD` 0x4F424844, `TXHD` 0x54584844, `PLHD` 0x504C4844, `PTHD` 0x50544844 (waves 101–119
only), `TXLD` 0x54584C44, `IMAG` 0x494D4147 (`waves.c:175-182`); `WHDR` 0x57484452, `GDAT` 0x47444154, `GTLD`
0x47544C44 (`loadtrack.c:116-125`). `OBJS`, `PATH` and `WOBJ` are reached only through pointers [R for their names].
The handle `{base, count, TOC}` is a 12-byte stack struct. `MBOffsetWad` (`func_80096BBC`) addresses it by raw
offsets +0/+4/+8.

Chunks:

| Chunk | Unit | Record | Pointers | Byte order on PC | Tag |
|---|---|---|---|---|---|
| OBHD | count × 0x58 | `tModelDef` (`waves.c:59-72`): `char name[16]`; `f32 radius` +0x10; `s16 unused` (kind) +0x14; `s16 lodCount` +0x16; `tLod lod[4]` +0x18, each 0x10: `u16 texidx` +0, `u16 flags` +2, `f32 lodRad` +4, `u32 *displayList` +8, `u32 *vtxList` +0xC | `displayList` and `vtxList` are file-relative and get `+= file` (`waves.c:195-196`). `vtxList` is never read [R] | swap 0x10 (32), 0x14/0x16 (16); per LOD 16, 16, 32, 32, 32 [M `waves.c:362-367`] | C |
| TXHD | count × 0x24 | `tTexture` (`waves.c:73-83`): `char name[16]`; `u16 width` +0x10, `height` +0x12; `u8 size` +0x14, `format` +0x15; `s16 palIndex` +0x16; `char *texBase` +0x18; `u32 flags` +0x1C (0x08000000 = texels, otherwise a TXLD list; clamp/mirror bits [R]); `u32 xflags` +0x20 | `texBase` is IMAG-relative, `+= IMAG` (`waves.c:185`) | 16 at 0x10, 0x12, 0x16; 32 at 0x18, 0x1C, 0x20 [M `waves.c:372`; the GC record is 0x30] | C |
| PLHD | count × 0x18 | `char name[16]`; `u8 start` +0x10, `end` +0x11; `u16 format` +0x12; `char *palBase` +0x14 (`waves.c:84-90`) | `palBase += IMAG` (`waves.c:190`) | 16 at 0x12, 32 at 0x14 [M `:377`] | C |
| PTHD | count × 0x24, waves 101–119 | `tBasicTargetPath` (`waves.c:91-99`): `char name[16]`; `u32 flags` +0x10; `s16 numPts` +0x14; `s16 collIdx` +0x16; `trigPtr` +0x18; `pts` +0x1C; `parent` +0x20 (set at run time) | `pts` and `trigPtr` are file-relative, `+= file` (`waves.c:203-204`). No reader of `trigPtr` found. `parent` is written at run time | 32 at 0x10, 0x18, 0x1C, 0x20; 16 at 0x14, 0x16 [M `:384`] | C |
| PATH | bytes | path points, numPts × 0x44: `f32 pos[3], norm[3], scale[3], quat[4], distToNext, timeToNext, speed; u32 flags` [M `rush_types.h:2435`] | none | 17 × u32 per point [M `waves.c:418`] | M |
| TXLD | bytes | texture-load display lists, 8-byte F3DEX2 commands | `G_SETTIMG` (0xFD) w1, and w1 of `G_RDPHALF_1` (0xE1) followed by 0x04, are IMAG-relative. `entity_cull_check` (0x80096A00, `src/blob/entity_cull_check.c:19-37`) rebases them to `(w1 & 0x0F000000) \| ((w1 + IMAG) & 0xFFFFFF)` | stays N64 form | C |
| OBJS | bytes | object display lists and `Vtx` (16 B: `s16 ob[3]`, `u16 flag` +6, `s16 tc[2]` +8, `u8 cn[4]` +0xC) | VTX/DL/MTX/MOVEMEM/SETxIMG w1 are file-relative with segment nibble 0. The relocator rewrites them (§3.2) | stays N64 form | C |
| IMAG | bytes | texels (N64 formats per TXHD `size`/`format`) and TLUTs (RGBA16) | none | stays N64 form (renderer) | C (use), R (contents) |

Model handle = `(waveSlot << 10) | index` (`particle_system` 0x8009C8F0, really the model draw,
`groups/particle_knot/group.c:838`). The per-slot `{pointer, count}` tables are `gRomObjDefPtrs` 0x801161F4,
`gTexDefPtrs` 0x80151AE8, `gPalDefPtrs` 0x80138670 and `gTargetPaths` 0x801392D0 (one pair).

### 3.2 Display-list relocation and the post-load passes

| Function | Address | What it does to the loaded file | Tag |
|---|---|---|---|
| `entity_lod_select` | 0x80096734 (`src/blob/entity_lod_select.c`; copy `audio_frame_sync/group.c:3818-3900`) | Walks one LOD list with a 10-deep stack. For ops 0x01, 0xDA, 0xDC, 0xDD, 0xDE, 0xE1 (before 0x04 or 0xDD) and ≥ 0xFD it sets `w1 = (w1 & 0x0F000000) \| ((w1 + base) & 0xFFFFFF)`. It rewrites the conditional `E0 01 cccc` into `DE010000` / target when load-flag bit `cccc` is clear. The flags are 2 = 1 player, 4 = 2 players, 8 = forward, 0x10 = backward (`waves.c:144-160`). It calls `func_800966D8` on the first VTX, does not enter `G_DL`, follows 0xE1+0x04, and stops at 0xDF | C |
| `func_800966D8` | 0x800966D8 | Vertex chain from that VTX: `p = addr \| 0x80000000`, stride 0x10. It reads the u16 at +6 (bit 15 = next vertex follows, bit 14 = done, bits 0–8 = texture width), sets bit 14, and when mirrored sets `s(+8) = width*32 - s` | C |
| `entity_cull_check` | 0x80096A00 | TXLD rebase (§3.1) | C |
| `entity_render_mode` (`MirrorWaveInstance`) | 0x800965BC (`groups/slot_sound/entity_render_mode.c:44`) | When the mirror state `D_80140A04` changes, walks every LOD list with `display_list_traverse(dl, 0, 0, 2, func_8008AD6C)` | C |
| `display_list_traverse` | 0x800963E8 (`src/blob/display_list_traverse.c:10-57`) | Generic walker: 0xDF ends, 0xDE calls or branches, 0xE1+0x04 branches. Its address rule: `addr &= 0xF0FFFFFF` when a segment nibble is set, else `addr += 0x80000000`, then a `u32 *` cast. With flags & 8 it replaces matching w1 | C |
| `func_8008AD6C` | 0x8008AD6C | TRI1 (0x05): swaps the 2nd and 3rd vertex-index bytes of w0. TRI2/QUAD (0x06/0x07): does the same in w0 and w1 (winding flip) | C |
| Texture animation | setup `camera_shake_start` 0x800BDAA8, update `race_setup_1` 0x800BD2C8, helpers `func_800BDA24` 0x800BDA24 (read the first SETTIMG w1), `func_800BD080` 0x800BD080 (write it), `camera_smooth_follow` 0x800BD104 (rewrite G_SETTILESIZE uls/ult) | Patches TXLD lists, TXHD `texBase`, and TLUT entries (palette kinds 0–8 store RGBA16 constants, `race_setup_1.c`) once per frame. Tables `0x8011A31C` / `0x8011A840` [R] | C, R |
| Model draw `particle_system` / texture-load builder `render_display_list` / TLUT load `func_80099B30` | 0x8009C8F0 / 0x80099BFC / 0x80099B30 (`groups/particle_knot/group.c:822/655/640`) | Read OBHD/TXHD/PLHD fields and emit native commands: `gSPDisplayList(lod.dl)` with the relocated 24-bit address, and `gSPDisplayList(tex->image)` (`:665`) for a TXLD list. Texture and TLUT loads point at IMAG (K0 addresses). `G_SETOTHERMODE_H(TEXTLUT)` once per frame | C |

Consequence for the renderer: native lists built by the game carry three kinds of address to big-endian payload.
These are physical 24-bit (`0x00xxxxxx`, from the relocator), K0 (`0x80xxxxxx`, TXHD/PLHD/IMAG) and, rarely,
segmented. All three resolve to `PC_RDRAM_BASE + (a & 0x00FFFFFF)` for segment 0. Optionally, under `TARGET_PC` the
two relocators can emit `| 0x80000000` (K0) instead. That is idempotent for `func_800966D8`, `display_list_traverse`
and the bit-31 test, and makes every relocated w1 a literal host address.

### 3.3 Text bank (waves 0–5)

[C `groups/slot_sound/func_800A4E58.c:39-71` (`_AdjustTextWavePointers`, labeled "model-table loader"); M `text.c`]

| Section | Layout | Fixup |
|---|---|---|
| 0 `single` | u32 n0, u32[n0] string offsets | `+= base` in place |
| 1 `tab` | u32 n1, u16[n1 rounded up to even] | |
| 2 `taba` | u32 n2, u32[n2] offsets | `+= base` |
| 3 `tab2` | u32 n3, u16[even] | |
| 4 `tab2a` | u32 n4, u16[even] | |
| 5 `tab2b` | u32 n5, u32[n5] offsets | `+= base` |
| strings | bytes | |

Header `gText` is at 0x8017A4E0 (0x28 on N64). 17 files read it as `char **` through at least four different struct
views (`control_settings/group.c:50-56`, `runtime_a_text_helpers/group.c:720-726`,
`runtime_a_menu_renderers/group.c:49`, `func_8010AEAC/faeac.c`). The language switch re-runs the parse after
`ReplaceWave` (`func_800A510C`).

### 3.4 World WAD (waves 120–138)

[C `loadtrack.c:104-126`, `src/blob/wheel_torque_apply.c`, `src/blob/race_setup_2.c:74-85`,
`groups/level_objects/group.c:464-475`; M `world.c:273-360`, `tracks.c:49-74`, `targets.c:694-703`]

| Chunk | Layout | Pointers / fixup |
|---|---|---|
| WHDR | u32 count; count × `{u32 woff; char wname[16]}` (0x14), sorted by name | `woff += (u32)worldptr` once (`loadtrack.c:120`). `NewWorld` (`wheel_torque_apply` 0x800AC668) bsearches it with `NameEntry {Node104 *node; char name[16]}` |
| WOBJ (world nodes) | `tModelPart` 0x68 [M `rush_types.h:2831`]: `char name[16]`; `f32 mat[12]` +0x10; `u32 dlflags` +0x40; `s16 next` +0x44, `child` +0x46, `sfx1` +0x48, `sfx2` +0x4A; `s32 tidx` +0x4C; `f32 min[3]` +0x50, `max[3]` +0x5C | none: next and child are indices. Placement: `differential_output` 0x800AC3D8 |
| GDAT | count × 0x1C `tTargetGroup`: `s16 parent; u8 unused; u8 flags; f32 minh, maxh, minv, maxv; u16 child[4]` [M]. N64 view `Tree` (`race_setup_2.c:74`) | none |
| GTLD | count × u32 target keys | **overwritten in place with `Object *`** at race setup (`level_objects/group.c:464-474`); read as `Object **` (`race_setup_2.c:155`) |

MAT3 swaps the world nodes while it walks them, with a reload flag, so a converting loader should swap the WOBJ
chunk once as an array of 0x68 records, after checking that its length is a multiple of 0x68.

### 3.5 Road: collision (waves 139–157)

[C `src/blob/func_800AB638.c:15-26` (`InitSTree`); M `stree.c:26-99`, `rush_types.h:1032, 1518, 2363-2408`]

| Section | Count | Record | Fields |
|---|---|---|---|
| header | 1 | 0x10 `stheadertyp` | `u16 pnum, tnum, snum, vnum, savenum, vlnum; u32 slnum` (N64 reads only the six u16) |
| path | pnum | 0x84 `pathtyp` | 33 × f32 (pos, uvs[3][3], ve, vl, vr, widths and slopes) |
| tree | tnum | 0x14 `streetyp` | `s16 parent; u8 unused, flags; s16 minh, maxh, minv, maxv; u16 child[4]` |
| surf | snum | 0x18 `surftyp` | `u16 flags, snd_flags; s16 uvs[9]; u16 vli` |
| svert | vnum | 8 `sverttyp` | `s16 pos[3]; u16 overflow` |
| save | savenum | 0x20 `savetyp` | `u16 object, snum; s16 uvs[9]; u16 vnum; s16 pos[3]; u16 overflow` |
| vlist, slist | vlnum, slnum | bytes | |

No pointers. Section pointers live in game BSS (`D_80152034` … `D_80152460`). This is a pure in-place swap. 24
files read these sections.

### 3.6 Path (waves 158–182)

[C `src/blob/audio_dsp_process.c:27`, `groups/read_path_data/audio_effect_apply.c:25-110` (`ReadPathData`);
M `pathcommon.c:25-80`, `rush_types.h:1329`]

| Offset | Content | Fixup |
|---|---|---|
| 0 | checkpoint block 0x32C: `s16 startTime, loopPt, finishPt, startPt, numPoints`, pad, then 10 × 0x50 `{f32 pos[3], normal[3], radius; u16 flags; s16 time[2]; s16 pathPts[20]; f32 distToNext}` | `memcpy` of 812 B to `gCheckpoints` `D_80151CE8` |
| 0x32C | `ResPath` 0x10: `u16 numPrimary, u16; pts (u32 slot); u8 numAlt, pad[3]; alt (u32 slot)` | `memcpy` of 16 B into `D_801407F0`, whose pointer fields are filled afterwards |
| +0x10 | numAlt × `AltPath` 0x10: `u8 type; …; u16 numPts +0xA; pts +0xC` | **`pts` written in place** (`:96`) |
| | u16 resurrection point count `D_801527A4`; numPrimary × `s16[3]` | |
| | alternative-path points, s16[3] each | |
| | 4 × { `DronePath` 8 B: `u16 num, u16, pts (u32 slot)`; num × 8-byte points } | `memcpy` of 8 B into `D_8012E5E8[p]` |

Byte order: s16, u16 and f32 throughout. MAT3 lists each field (`pathcommon.c:35-53`). The overlay A track-select
map reads the same waves (§2.3).

### 3.7 Font data (waves 38–53)

[C `groups/slot_sound/group.c:92-150` (`sound_update_channel` = `_SetupFontData`, 0x800B3D18); M `HAL_text.c:152-183`,
`rush_types.h:1678-1701`]

- Header 0x10, all bytes: `version, type, ascent, descent, leading, totalHeight, maxWidth, spaceWidth, spacing,
  nonProp, kern, numTextures, numChars`.
- numChars × 0xC letters: a 4-byte `kerns` slot **written in place** with a pointer
  (`D_80149800[i].ptr = p`, `:135`), then bytes `value, texIdx, uls, ult, …`.
- When `kern` is set, a numChars × numChars byte table follows.
- No byte swapping is needed. The 12-byte letter is declared `{u8 *ptr; u8 id; u8 pad[7]}` (`:108`), so the slot
  must stay 4 bytes.
- The textures are TXHD of the font WAD wave: `gFontTextures[i] = texDefs + i * 36` (`:148`).

### 3.8 MusyX sound data (waves 6–8, 10–21; samples in ROM)

The MusyX runtime is boot-segment C (`src/rom/lib_15a10.c`, `lib_16320.c`, `lib_17dc0.c`, `lib_1a660.c`,
`lib_22300.c`). It reads these files in place with native loads. Field by field, with every reader:
[MUSYX_DATA.md](MUSYX_DATA.md) §3.

| File | Layout | Writes into the file | Tag |
|---|---|---|---|
| project (6) | groups until u32 0xFFFFFFFF: `{s32 size; u16 id; u16 type; u32 offsets[]}` (`func_8001536C`), offsets relative to group + 8: u16 id lists ended by 0xFFFF (macros, samples, tables, keymaps, layers); type 1 adds an sfx table (12-byte entries), type 0 a normal page, a drum page and MIDI setups (`func_8001558C`) | byte 9 of sfx entries (`func_800164D0`, `func_80020370`) | C |
| pool (7) | 4 lists (`ResourceOffsets {u32 offsets[4]}`) of `{u32 size; u16 id; u16; payload at +8}` entries ended by size 0xFFFFFFFF (`func_80014E1C`): macros (8-byte steps of two u32, copied word by word by `func_80023E9C`), tables (byte curves; envelopes of four little-endian u16), keymaps (128 × 8 B), layers (count u16 at +0xA, payload at +12, 12 B each) | none | C |
| sample directory (8) | `SampleRecord` 0x1C until id 0xFFFF: `u16 identifier; u16 references; u32 offset; void *data; u8 descriptor[16]` (`include/boot_tail_sample_contract.h`) | **`references` counter and `data = base + offset`** (`func_800162AC`), where base is the cart address of ROM 0x39B40 (`D_801525FC`, through `func_80014CAC`) | C |
| songs (10–21) | `{u32 tracks, patterns, channel map, tempo track, tempo, loop time}`; `u32 trackTab[64]` (offsets from the song start) to 12-byte track events; a pattern table to patterns `{u32; u32 bend; u32 modulation}` + note events (u16 delta, key, velocity, u16 duration for notes) | none; resolved into sequencer state as `offset + (u32)song` (`func_800178B0`, `func_80018634`) | C |
| samples | ROM 0x39B40, raw ADPCM (format 3) or PCM16 (format 0), a 0x100-byte codebook first; `{length, source, destination}` DMA requests (`func_80012234`, `func_80012730`, `func_8008ABE4.c:21-25`) | none | C |

### 3.9 Not loaded at run time

These are game or boot .data, so `port/data` must generate them by type, with pointers turned into symbol
references:
- car physics descriptors (pointer table 0x80110D08, 0xB4-byte records) and per-car tables 0x80110D3C–0x801114E4 [R];
- PVS tables 0x8011B898… with count table 0x8011E748 [R; the start address checked in §2.2];
- texture-animation tables 0x8011A31C / 0x8011A840 [R];
- `gWorldNames` 0x8011A924 [C `loadtrack.c:66`];
- song/group pairs `D_8011F070` and sfx groups `D_8011F0A0` [C];
- the MusyX callback table `D_8011EAAC`, 8 words copied as `u32[8]` by `func_80020598` (`lib_207b0.c:471-476`) [C].

## 4. Pointer and byte-order assumptions on loaded data

Every function in this table reads loaded data with native 32- or 16-bit loads, which assumes big-endian bytes.

| # | Site | Assumption | PC fix |
|---|---|---|---|
| 1 | `MBSetupWad.c:35-41` | TOC word replaced by the absolute address; "relocated" means bit 31 set; `offset += (u32)header` | heap in the window; swap the TOC first |
| 2 | `func_80096BBC.c` (`MBOffsetWad`) | raw +0/+4/+8 into the 12-byte WAD handle | keep the handle three 32-bit words, or rewrite with fields |
| 3 | `waves.c:139, 175-182`; `MBGetFromWad.c` | chunk addresses returned and held as `s32` (`IMAGptr`, `TXLDptr`) | window |
| 4 | `waves.c:171, 195-196, 203-204` | `(u32)displayList + (u32)WADptr`; `offset = *(u8 **)ptr - (u32)lastPtr` | window; u32 fields |
| 5 | `waves.c:163`, `slot_sound/group.c:127` vs `audio_heap/group.c:159` | handles dereferenced as `*(u8 **)` while the heap's master slots are `u32` (`count * 4`) | `(u8 *)(uintptr_t)*(u32 *)h`, or 8-byte slots throughout memsys |
| 6 | `entity_lod_select` (0x80096734) | `(w1 + base) & 0xFFFFFF` plus a segment nibble; writes w0/w1 | window (RDRAM ≤ 16 MB); BE32 accessors |
| 7 | `entity_cull_check.c:19-37` | the same rebase on TXLD; iterates a local 8-byte `Gfx` | BE32; keep its local `Gfx` 8 bytes even after the `TARGET_PC` widening of `Gfx` |
| 8 | `func_800966D8` | `addr \| 0x80000000` to make a pointer; u16 at Vtx+6, s16 at +8 | window; BE16 |
| 9 | `display_list_traverse.c:19-21, 38-40` | `addr += 0x80000000` / `&= 0xF0FFFFFF`, then a `u32 *` cast | window; BE32 |
| 10 | `func_8008AD6C` | swaps index bytes at fixed bit positions of w0/w1 | BE32 load and store |
| 11 | `func_800BD080`, `func_800BDA24`, `camera_smooth_follow`, `race_setup_1` | texture animation writes w1, SETTILESIZE w0 and RGBA16 TLUT entries | BE accessors |
| 12 | `loadtrack.c:120` | `woff += (u32)worldptr` into a u32 field | window |
| 13 | `wheel_torque_apply.c:28-30` | `NameEntry {Node104 *node; char name[16]}` overlaid on 0x14-byte WHDR entries, `bsearch(.., sizeof(NameEntry))` | `node` as u32 (else the stride becomes 24) |
| 14 | `level_objects/group.c:464-474`, `race_setup_2.c:155` | GTLD u32 keys replaced in place by `Object *` and read as `void **` | native side array |
| 15 | `func_800A4E58.c:61-70` + 17 readers | text offsets `+= (u32)base`, read as `char **` | native `char *` side arrays; one shared `gText` definition |
| 16 | `slot_sound/group.c:108, 135` | 12-byte font letter with a pointer written in place | u32 slot |
| 17 | `slot_sound/group.c:148` (and its 3 copies), `entity_spawn_init.c:300` | TXHD stride `+ i * 36`; `D_80149820[]` holds pointers as s32 | TXHD stays 0x24, pointer fields u32 |
| 18 | `car_gear_shift.c:14` | OBHD stride `* 88` | OBHD stays 0x58 |
| 19 | `func_800ABB58.c:3704` | `(u8 *)&gRomObjDefPtrs + idx * 8` (the pair is 8 B); `slot_sound/group.c:109` declares the pairs `{s32 first; s32 second}` | pair tables keep a u32 pointer |
| 20 | `particle_knot/group.c:544-556` | OBHD overlaid as `LodSet`/`LodLevel`: levels start at +0x14 with stride 0x10, `Gfx *dl` | u32 `dl` |
| 21 | `runtime_a_c93c_closure/group.c:264-265` | `MapLod` 0x10 with two pointers; whole records saved and restored (`D_803BAB58`) | u32 fields |
| 22 | `audio_effect_apply.c:75, 96, 100` | `memcpy` of 16 B / 8 B file headers into structs with pointers; `AltPath.pts` written in place | field-by-field copy; u32 `pts` |
| 23 | `level_objects/group.c:59-67, 406` | PTHD as `Scene36` (0x24) with `Resource68 *resources`; byte-offset indexing `(u8 *)D_801392D0 + offset` | u32 fields |
| 24 | `func_800162AC`, `boot_tail_sample_contract.h` | `SampleRecord.data` (`void *`, 0x1C record) set in place to a cart address | done: u32 under `TARGET_PC` (MUSYX_DATA.md §4) |
| 25 | `func_80014CAC`, `func_800146B4` (`lib_11640.c`) | address class by the top byte (0x80 RAM, 0xB0 cart) | cart addresses stay 32-bit; host pointers (≥ 4 GiB) count as RAM |
| 26 | `func_800178B0`, `func_80018634` (`lib_17dc0.c`) | song offsets `+ (u32)arr` | window |
| 27 | `audio_heap/group.c:36-63, 236-245` | heap base `(u32)&D_8017A640`; `end = osMemSize \| 0x80000000`; 0x20-byte block header holding 3 pointers | `TARGET_PC`: base and end inside the window |
| 28 | `gWaveInstance` 0x80156D38 and the `{ptr,count}` tables (§3.1) | BSS/data structs with pointers, declared differently per file (`ResSlot`, `Voice`, `anon_loaded_ready_14`, `Pair`) | `gen_bss` doesn't detect pointer structs yet (PORT_PLAN Phase 0). Give them own storage and one declaration, or u32 pointers |

## 5. Work list: converting loaders

Strategy column: **swap** = byte-swap scalar fields in place, keeping the N64 size (pointer fields stay u32 and hold
window addresses); **side** = build a native array beside the file; **BE** = leave big-endian for the renderer or
audio interpreter and use BE accessors in the game functions that touch it. All conversion happens in one `TARGET_PC`
hook in `_ReadWave` just before its `SetupWaveModel` call (`waves.c:254`), keyed by wave range.

| Format | Waves | Strategy | Fields | Game edits under `TARGET_PC` |
|---|---|---|---|---|
| WAD TOC | 22–37, 54–138 | swap | header 2 × u32, entries 3 × u32 | §4 #1–3 |
| OBHD | WADs | swap | 0x10 (32), 0x14/0x16 (16), LODs 16/16/32/32/32 | u32 `displayList`/`vtxList` in `waves.c:59-72`, `particle_knot/group.c:544`, `runtime_a_c93c_closure/group.c:264` and the other OBHD readers (13 files) |
| TXHD | WADs | swap | 16 at 0x10, 0x12, 0x16; 32 at 0x18, 0x1C, 0x20 | u32 `texBase`/`image` (25 files) |
| PLHD | WADs | swap | 16 at 0x12; 32 at 0x14 | u32 `palBase`/`table` (4 files) |
| PTHD + PATH points | 101–119 | swap | §3.1 | u32 `pts`/`trigPtr`/`parent`/`resources` (2 files) |
| TXLD, OBJS DLs, Vtx, IMAG texels and TLUTs | WADs | BE | none; register the buffer | §4 #6–11 (about 12 functions) |
| WHDR | 120–138 | swap | u32 count, u32 woff | §4 #12–13 |
| World nodes (WOBJ) | 120–138 | swap | 12 f32, u32, 4 × s16, s32, 6 f32 per 0x68 | none |
| GDAT | 120–138 | swap | s16 at 0, 4 × f32, 4 × u16 | none |
| GTLD | 120–138 | swap + side | u32 keys | §4 #14 (2 files) |
| Road | 139–157 | swap | §3.5 | none |
| Path | 158–182 | swap | §3.6 | §4 #22 |
| Text bank | 0–5 | swap + side | counts u32, offset tables u32, tables u16 | §4 #15 (17 files); redo after `ReplaceWave` |
| Font data | 38–53 | none | only the 4-byte slot | §4 #16 |
| MusyX project, pool, sample directory, songs | 6–8, 10–21 | swap: done, `port/assets/musyx.c` ([MUSYX_DATA.md](MUSYX_DATA.md)). The readers use whole-field loads except a few, handled by how the fields are swapped | MUSYX_DATA.md §3 | §4 #24; registries, sample-info views, WAITMS (MUSYX_DATA.md §4) |
| Samples | ROM 0x39B40 | BE: PC DMA is a `memcpy` from the ROM | none | audio interpreter reads ADPCM bytes |
| Game image, runtime images | — | skip | — | `game_init` inflate and `PrevMaxPath` under `TARGET_PC` |
| File table (4 tables), car tables, PVS, texture-animation tables | — | `port/data` | by declared type | — |

## 6. Bring-up order

| Stage | Formats needed, in load order | Evidence |
|---|---|---|
| Boot (`InitSystem`, `game_late_init` 0x800EEA7C) | heap (§2.4) → `AudioInit` (`hud_setup` 0x800C8B8C) → `AudioInitSys`: `InitWaves` → **text bank** (`D_8002E860`, then `ReplaceWave` to the language) → **MusyX project, pool, sample directory** (6, 7, 8; converted) → MusyX init through its service callbacks (`D_8011EAAC`, not yet LP64-ready: MUSYX_DATA.md §8.1) → `sndPushGroup` of sfx groups 12–15 with samples at ROM 0x39B40 → `HAL_InitText` | `groups/heap_release/initsystem.c:144-156`, `car_shadow_render.c:65-110`, `InitWaves.c` |
| First text on screen | **font data + font texture WAD** (38 + f, 22 + f) on the first `HAL_SetFont`; **WAD** loader, TXHD, IMAG | `slot_state_setup` |
| Front end (states 0x2 / 0x4) | `InitWaves`, overlay A ("Start"), **front-end WADs** 54, 58, 59 (locked), song 6 (wave 16) when the Expansion Pak flag is set; screens 55/56/57/60 (all WAD; DL relocation, mirror pass, conditionals) | `state.c:158-196` |
| Car select / track select (image A) | **car WADs** 88–100 (`func_80390BC0`); **path** waves 158–163 for the track maps (`loadPathData`), which needs the path format before any race | `runtime_a_c93c_closure/group.c:1272-1281` |
| Race start (state 0x40000, then `LoadTrack`) | 13 fonts preloaded (`object_create` × 13, `state.c:280-292`); **track WAD** 101 + t (with PTHD/PATH) → **world WAD** 120 + t (WHDR/GDAT/GTLD, `NewWorld`) → **road** 139 + t → **path** 158 + t (or 177 + t) → race WADs (68, 78/79, 82 + t, 77, 62, 80+61 or 81, 64, 66/65, 63/67, 76) → **cars** 88 + m → **song** 10 + n; texture animation; image B only for battle, stunt or Expansion Pak | `loadtrack.c:89-228`, `state.c:260-293` |

Suggested order of work: decompressors and the file table, then the WAD (TOC, OBHD, TXHD, PLHD, the BE walkers),
text and fonts, MusyX (only to reach the menus with sound; it can be stubbed at first), path, then world, road and
GTLD for the first race. With `osMemSize` reported as 4 MB, `D_80156994` stays 0. Races then skip image B and the
Expansion Pak waves (61, 65, 78, 82–87), which is a smaller first target. Switch to 8 MB once that works.

## 7. Risks and open questions

1. **The RDRAM window is load-bearing.** The 32-bit plan needs the heap inside a fixed mapping at host 0x80000000
   (≤ 16 MB, because of the 24-bit masks). If the OS can't provide it (`pc_n64_ptr` fallback), every u32 pointer
   field needs a translating macro at each use. That is a much larger edit.
2. **Address ambiguity in the renderer.** Native lists mix host pointers with N64 physical and K0 addresses (§3.2).
   The renderer can only tell them apart if no host data sits below 4 GB except the window: build PIE, or tag the
   addresses.
3. **Multiple struct views of the same records** (OBHD three views, `gText` four, `gWaveInstance` four, the
   `{ptr,count}` pairs two). Every view must change together, or the PC layout silently diverges. `gen_bss` doesn't
   yet give pointer-bearing structs their own storage.
4. **The `Gfx` widening must not reach code that walks ROM lists** (`entity_cull_check`'s local `Gfx`, `u32 *`
   walkers): those lists stay 8-byte commands.
5. **MusyX** parses its files throughout the boot library with native loads. Audited (MUSYX_DATA.md §4): every
   reader uses whole-field loads except WAITMS's byte store (given a PC version), the little-endian envelopes,
   the layer count's halfword and two fields read at either width by record kind, which the converter swaps
   accordingly. The formats are now from the C; what to check with the ROM is MUSYX_DATA.md §7.
6. **Unidentified**: waves 9 and 69–75 (no load site), 61, 79, 80, 81 (N64-only), `trigPtr` targets (relocated, no
   reader found) and the contents of WOBJ beyond the 0x68 records [M]. The storage kind per wave is ROM data, so read
   it from the table rather than guessing.
7. **Handle blocks move** during compaction. Only font data uses handles today; a BE registration on a moving block
   would also have to move.
