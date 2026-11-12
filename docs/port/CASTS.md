# Pointer/integer casts in the game C

Every `-Wpointer-to-int-cast` and `-Wint-to-pointer-cast` the census counts in the game C (`src/`, `nonmatching/`),
classified, and what was fixed. 2026-10-27. Since [NATIVE.md](NATIVE.md) (wave 3, ended by step 12 on 2026-11-12) the
game keeps no 32-bit address: the RDRAM window, `PC_N64W` and `RUSH_CHECK_CASTS` (section 4) are gone, and GCC's
pointer/integer warnings in the game C are 0 (`n64_audit.py`'s `cc.*` classes). This page is the record of the
classification. Related: [POINTERS.md](POINTERS.md) (the techniques, section 4 the
earlier list), [DATA.md](DATA.md) (where data lives on the PC), [CARS.md](CARS.md) (the visual record).

## 1. Summary

| | Warnings | Files |
|---|---:|---:|
| Before (`e6d51b3`) | 331 (212 pointer-to-int, 119 int-to-pointer) | 74 |
| After | 325 (209, 116) | 69 |

Counted with gcc 12 on the Pi 5 (aarch64), as earlier sessions did; gcc 14 on x86-64 counts the same 325 in 69.

Of the 331 before:

| Class | Warnings | Sites (functions) |
|---|---:|---:|
| weak context copy: never linked (section 5) | 191 | 56 in 28 files |
| harmless: the value is always a window address or a small integer, unused, or the code never runs | 126 | 62 |
| **bug**: a host pointer passes through 32 bits, or a window address is sign-extended | 8 | 6, all fixed |
| records agent (the input records, the level-object nodes) | 5 | 5 |
| unknown: needs the ROM | 1 | 1 |

The 6 fewer are the fixed sites; `SkyUpdate`'s 2 stay, as correct zero extensions of a u32 (gone since
[NATIVE.md](NATIVE.md) step 4: `skyMatrices` holds pointers). No
harmless warning was silenced: the N64-world casts say what the code does (POINTERS.md 4.1), and the neutral
rewrite, `(u32)` to `(uintptr_t)`, would only move the truncation into an implicit conversion.

Fixed on the way, with no census warning: the high-score name table's views (`gTotalNames`, 8 files), the ghost
records and their allocator, and `AudioSfxServer`'s sound-request walk, found among the implicit conversions
`-Wno-int-conversion` hides (section 6). Tests: `assets_casts` (section 3). Debug aid: `RUSH_CHECK_CASTS`
(section 4).

**The rule.** The RDRAM window is host 0x80000000–0x80800000 (ULTRA.md 2), so these are 32-bit values: the heap
(`align32(0x8017A640)` up), everything in it (WAD files, the world, MusyX files, frame buffers, handle tables), the
overlay block 0x8038A400, and the data arenas at their N64 addresses (DATA.md 3). A u32 holds one exactly and
converts back zero-extended. An `s32` holds one too, but converting it to a pointer sign-extends it to
0xFFFFFFFF8xxxxxxx: that is a bug even though nothing is lost. Above 4 GB, and so a bug in any 32-bit integer:
own-storage data (`data_layout.txt`, "own"), C-defined data such as the environment visual pool, host functions,
the stack, malloc.

**How the list was made.** Each game file is compiled again with its compile command and `-fsyntax-only`, as
`census.py` does, and each warning is tied to the function GCC names ("In function") and to the use site of a
macro (`M2C_FIELD`'s warnings point at its definition). A function listed in the file's `build/pc/weak/*.h` is a
weak copy; `nm` over the objects confirmed that each of those has a strong owner elsewhere, and that every other
one is the linked definition (or a static of one). Each live site was then read with its callers and the storage
of what it handles (`data_symbols.txt`).

## 2. The bugs, and what was fixed

| Site | What was wrong | Fix |
|---|---|---|
| `InitBlast` (InitBlasts) | wrote the 50 blast matrices into gBlastMats `gBlastMats` as `s32` at a 4-byte stride; on the PC the table is blast.c's `tBlastMat[50]`, 8 bytes an entry (own storage), so each matrix pointer the explosion code read was two window addresses glued together | `BLAST_MAT(p, i)` / `BLAST_MAT_PTR`: pointer stores on the PC, the original tokens on the N64; the PC declaration is `void *gBlastMats[50]` |
| `UpdateEnvirons` (every frame, `GameLoop`) | walked the environment visuals (`gEnvList` -> `gEnvirons`, C-defined host storage, `struct pc_visual`) through `s32` locals and called each visual's `func` from N64 +0x14, `timeStamp` on the PC | `intptr_t` locals and links, `VIS_OFS(0x14)` (0x18) |
| `SkyUpdate` | `skyMatrices` (arena) holds each window's camera matrix, a window address from `ZOID_NewMatrix`'s pool in the arena; read through an `s32`, it was sign-extended | `MTX_WORD`: u32 on the PC |
| `UpdateModel` | a ghost handle from gGhosts (`gGhosts`, own storage) in an `s32`, sign-extended, and the ghost's header read at N64 +0x28 | `intptr_t temp_v1`, `GHOST_HEADER` (0x38 on the PC) |
| `RankScores` | stored a player's profile handle, `&gJustPlayers[n]` (own storage), into the name slot `gThisPlayer` (arena `s32[4]`) | `gThisPlayer` is `intptr_t` (own storage now); its "no name" test compares with -1 on the PC (`NO_NAME`), since `0xFFFFFFFF` no longer equals a 64-bit -1 |
| `ClearPlayerRecords` (clear a save slot) | passed the profile handle to `RemovePlayerScores` as `s32` | `(intptr_t)`, and `RemovePlayerScores(intptr_t)` |

### 2.1 The high-score name table

`gTotalNames` (`D_80151690`, 12 tracks x 3 rows x 5 places) and `gThisPlayer` hold, per slot, a name index into
`gTempNames` (0..19), -1 for none, or a profile handle. PrintScores (`func_800F0914`) tells them apart (-1, <= 20, a
pointer); `AddPlayerScores` and `RecordRace` store handles as pointers, `ControlName` copies `gThisPlayer` in,
`RemovePlayerScores` removes a handle, `FindTempNameIdx` counts name references. Natively the views disagreed:
three had 8-byte slots (1440 bytes), four `s32` slots (720), and RecordGhost' `Owner` a 0x28-byte pad and five
pointers, which gave the storage 960 bytes, so the pointer views ran past it. Every live view has pointer-sized
slots now (`intptr_t` in `InitRecords`, `FindTempNameIdx` (and its `slot` local, which a handle's low half could
make look like a name slot), `ControlName`, `RemovePlayerScores`; RecordGhost' `Owner` under `TARGET_PC`):
1440 bytes, `view_check --sym gTotalNames` 0 disagreements.

### 2.2 The ghosts

A ghost (SaveInit' `Ghost`, GC `__anon_49a9c`) is 0x2C bytes on the N64 and 64 on the PC: its link, file
handle, data handle and header handle are pointers. `_NewGhost` (`NewGhost`) kept 64 four-byte handle slots
at `ghost_file_handles` and 64 records 0x2C apart at `ghost_file_records` (game arena) and stored each handle as an 8-byte pointer,
so the second ghost overwrote the first's handle and the records overlapped. Its PC version keeps both in host
storage (nothing else references them), so ghost handles are host pointers; every place that holds one was
checked to be pointer-typed. Four live views had the N64 layout under pads or a u32: highscore.c's `Ghost`,
cleanup_colortrace's `Record` (`unknown24` is the data handle), and the race code's `Obj40`
(`car_checkpoints/PlayGame.c`) and `Ghost40` (`update_game/UpdateGame.c`).
`view_check --tag Ghost,Obj,Record,SavedRecord,ReplaySlot,Ghost40,Obj40 --n64-size 0x2C` over the 14 views: 0 live
disagreements (4 before; two weak copies still differ).

### 2.3 The sound requests (`AudioSfxServer`)

`_PeriodicMaintenance`'s request walk copied the request block's address `gSfxInstances` (a heap block) into an
`s32` and formed a finished request's address from it with an implicit conversion, which sign-extends: retiring a
finished sound wrote to 0xFFFFFFFF8xxxxxxx in the audio manager thread. The copy is `intptr_t`, and the request
nodes' `entry` it is compared with is read pointer-sized (`PcSfxNode`, MUSYX_DATA.md 8.4).

## 3. Tests

`assets_casts` (`port/assets/tests/test_casts.c`) runs the game's code on host pointers (above 4 GB, as own
storage is) and on window data in a mapped RDRAM window:

| Part | Game code | Checks | With the fix put back |
|---|---|---|---|
| blast matrices | `InitBlast` | each of the 50 entries is the pointer `ZOID_NewMatrix` returned | 50 checks fail |
| visual walk | `UpdateEnvirons` | each host visual's `func` called with it and 1, one that ends itself, one without | fault |
| camera matrix | `SkyUpdate` | a window-address matrix gets the camera's two values | fault |
| name table | `InitRecords`, `RemovePlayerScores`, `ClearPlayerRecords`, `FindTempNameIdx`, `RankScores` | all 180 slots cleared to -1; a host handle removed from every row while a value with the same low half stays; the slot clear passes the whole handle; a handle that looks like slot 19 is no reference; a profile handle kept whole, and -1 clears the name | 1 (`InitRecords`), 6 (`RemovePlayerScores`), 2 (`FindTempNameIdx`), 2 (`RankScores`); `ClearPlayerRecords`'s `(s32)` fails only on x86-64, which truncates the register |
| ghosts | `_NewGhost` | 64 separate handles and whole 64-byte records, the 65th NULL, a freed slot reused | 4 checks fail |
| sound requests | `AudioSfxServer` | a finished request in the window cleared in place, a playing one kept | fault |
| `RUSH_CHECK_CASTS` | `PC_N64W` | window addresses and small values pass, a host address calls `pc_cast_fail` with the line | |

The test passes on aarch64 (gcc 12) and x86-64 (gcc 14), and the old `ClearPlayerRecords` fails it on x86-64.

## 4. RUSH_CHECK_CASTS

Gone with [NATIVE.md](NATIVE.md) step 12 (the option, `port/ultra/castcheck.c`, `PC_N64W` and `test_casts.c`'s check of
it): no game file stored a pointer in 32 bits any more. Its successor for what the synthetic paths never reach is the
N64 guard (`RUSH2049_N64_GUARD=1`, [ULTRA.md](ULTRA.md) 2), which faults on an N64 address used as a pointer and on a
heap pointer cut to 32 bits. How it was:

The N64-world casts are harmless only while what reaches them is window data, which the ROM decides. The records
that keep their N64 layout on the PC (the heap headers, the model record, the visuals' `data`, WAD records, the
voice and DMA records) take pointers through each file's `N64W(x)`. The 39 PC definitions of `N64W` are
`PC_N64W(x)` now (`port/include/pc_prelude.h`), the same u32 conversion. Built with
`cmake -DRUSH_CHECK_CASTS=ON`, `PC_N64W` checks that the value's upper half is zero and otherwise calls
`pc_cast_fail` (`port/ultra/castcheck.c`), which prints the source line, the value and the N64 thread, the
backtrace, and aborts:

    rush2049pc: RUSH_CHECK_CASTS: .../src/blob/groups/memsys/group.c:191 stores host address 0x... as a 32-bit N64 address (N64 thread ...)

The reference is weak, so test programs that link game C without port/ultra still link. A checked build passes
ctest and the selftest and boots on a blank cartridge to the usual stop. Run it with the ROM through a race.

It doesn't see the raw `(u32)p` casts of the heap API (`DeallocateMemoryInternal((u32)p, 0)`, `PurgeableMemory`):
checking those would need a new token at each call. The heap's own entry points could check that what they get
is a heap block (`FindHeap` finds the heap of an address), which would catch a truncated host pointer with a
probability of about 1 - 8 MB / 4 GB.

## 5. Every warning

"(n)" after a count: the warnings left after the fixes, where that differs.

### 5.1 Live functions

| File | Function | Warnings (left) | Class | Why |
|---|---|---:|---|---|
| `nonmatching/game/PS_ReadPartialFile.c` | `PS_ReadPartialFile` | 1 | harmless | heap: `pdf_dealloc` frees `psfile->data`, an `AllocateHandleHeap` handle |
| `nonmatching/game/GhostEnd.c` | `GhostEnd` | 1 | harmless | heap: DeallocateMemory of the ghost's data block |
| `src/blob/MBSetupWad.c` | `MBSetupWad` | 6 | harmless | window: rebases a WAD's TOC in place; the WAD is a heap block (`SetupWaveModel`, `LoadTrack`) |
| `src/blob/UpdateEnvirons.c` | `UpdateEnvirons` | 2 (0) | **bug**, fixed | walked the environment visuals (C-defined, host) through `s32` and called `func` at N64 +0x14 |
| `src/blob/InitBlast.c` | `InitBlast` | 1 (0) | **bug**, fixed | wrote the 50 blast matrix pointers of `gBlastMats` (own storage, 8 bytes each) as `s32` at a 4-byte stride |
| `src/blob/MovePlayerFile.c` | `MovePlayerFile` | 1 | harmless | never runs: the `tPS_File` `MoveFile` hook (+12) it is installed as has no caller in the ROM's code; its N64 offsets would be wrong on the PC (section 7) |
| `src/blob/FindChunk.c` | `FindChunk` | 3 | harmless | window: the heap's handle lookup over its N64-layout headers |
| `src/blob/GetCircuitCode.c` | `GetCircuitCode` | 1 | records agent | input record `gPlayers_save` (+0x48 of `gPlayers`) at a 0x4C stride; reads the profile's save data pointer as `s32` at N64 `SLOT_DATA` |
| `src/blob/groups/waves/waves.c` | `SetupWaveModel` | 8 | harmless | window: WAD relocation, `WADP(u32)` fields, offsets between two heap addresses |
| `src/blob/groups/memsys/group.c` | `AllocateHandleHeap` | 1 | harmless | window: a heap block into a heap handle slot |
| `src/blob/groups/memsys/group.c` | `InitHeap` | 2 | harmless | window: heap header set-up |
| `src/blob/groups/memsys/group.c` | `create_heap` | 1 | harmless | window: a sub-heap's end |
| `src/blob/groups/memsys/group.c` | `locate_main_heap` | 1 | harmless | window: the heap's start, `align32(0x8017A640)` |
| `src/blob/groups/AudioInitSys/AudioInitSys.c` | `AudioInitSys` | 1 | harmless | small integer: `sample_bank_rom` is the sample bank's ROM offset 0x39B40 |
| `src/blob/groups/blast/blast.c` | `StartBlast` | 1 | harmless | ignored: EmitGameSound's `dir` reaches `AudEmitEffect`'s unread `a1` (and is the arena's unit matrix) |
| `src/blob/groups/targetpaths/group.c` | `camera_track_entry` | 1 | harmless | ignored: `AudEmitEffect`'s `a1` is never read |
| `src/blob/groups/SetupCarColor/SetupCarColor.c` | `SetupCarColor` | 1 | harmless | heap: frees its `AllocateMemoryHeap` palette buffer |
| `src/blob/groups/heap_release/zcfree.c` | `zcfree` | 1 | harmless | heap: zlib's ZFREE of a `ZALLOC` (`AllocateMemoryHeap`) block |
| `src/blob/groups/heap_release/initsystem_start.c` | `FreeBuffers_Init` | 2 | harmless | never runs: a static of the weak `InitSystem` copy |
| `src/blob/groups/heap_release/initsystem_start.c` | `_InitGfx_Init` | 2 | harmless | never runs: a static of the weak `InitSystem` copy |
| `src/blob/groups/update_model/group.c` | `UpdateModel` | 1 (0) | **bug**, fixed | held a ghost handle from `gGhosts` in an `s32` (sign extension) and read its header at N64 +0x28 |
| `src/blob/groups/destroy_heap/destroy_heap.c` | `destroy_heap` | 1 | harmless | heap: frees a sub-heap |
| `src/blob/groups/highscore_rank/RankScores.c` | `RankScores` | 1 (0) | **bug**, fixed | stored a profile handle (`&gJustPlayers[n]`, own storage) into the `s32` name slot `gThisPlayer` |
| `src/blob/groups/animRumbleText/animRumbleText.c` | `animRumbleText` | 1 | records agent | `gPlayers[i].name` held as `s32` and turned into a pointer |
| `src/blob/groups/heap_release/alloc_at.c` | `allocate_memory_at` | 6 | harmless | window: heap block arithmetic (the overlay reservation) |
| `src/blob/groups/heap_release/ClearMemoryHeap.c` | `ClearMemoryHeap` | 3 | harmless | window: heap compaction (ClearMemory) |
| `src/blob/groups/heap_release/group.c` | `unload_extra_overlay` | 1 | harmless | window: frees the overlay block 0x8038A400 |
| `src/blob/groups/heap_release/group.c` | `unload_start_overlay` | 1 | harmless | window: frees the overlay block 0x8038A400 |
| `src/blob/groups/heap_release/group.c` | `DeallocateMemoryInternal` | 6 | harmless | window: DeallocateMemory's core over N64-layout headers |
| `src/blob/groups/heap_release/initsystem.c` | `FreeBuffers_Init` | 2 | harmless | heap: `gListeners`/`gEmitters` hold `AllocateMemoryHeap` blocks |
| `src/blob/groups/heap_release/initsystem.c` | `_InitGfx_Init` | 2 | harmless | heap: the frame buffers (`AllocateMemoryHeapHigh`) as `s32`, read back through `PC_ZEXT` |
| `src/blob/groups/heap_release/overlay_loader.c` | `load_overlay` | 1 | harmless | window: `allocate_memory_at((u32)dst)` with dst = 0x8038A400 (an overlay loader) |
| `src/blob/groups/targets/group.c` | `SetTargetDefs` | 1 | records agent | the level-object node pool `gTargetList`: an `s32` GTLD key compared with a pointer slot, sign-extended |
| `src/blob/groups/loadtrack/loadtrack.c` | `LoadTrack` | 1 | harmless | window: WHDR offsets rebased on the world file, a heap block |
| `src/blob/groups/listeners/AudRemoveListener.c` | `StopListeners` | 1 | records agent | `gPlayers[i].object` held as `s32` and turned into a pointer |
| `src/blob/groups/save/NewGhost.c` | `CompressGhost` | 3 | harmless | heap: frees the zip buffer; a heap block's size |
| `src/blob/groups/save/NewGhost.c` | `ghost_free_start` | 1 | harmless | heap: frees the ghost data |
| `src/blob/groups/save/ghost_files.c` | `PS_PurgeFile` | 1 | harmless | heap: frees a slot's data handle |
| `src/blob/groups/save/ghost_files.c` | `SaveGhost` | 2 | harmless | heap: a handle's block size |
| `src/blob/groups/save/ghost_files.c` | `cart_free` | 1 | harmless | heap: frees a handle |
| `src/blob/groups/save/ghost_files.c` | `cart_free_direct` | 1 | harmless | heap: frees a handle |
| `src/blob/groups/save/ghost_files.c` | `PurgeGhostData` | 1 | harmless | heap: frees a handle |
| `src/blob/groups/save/group.c` | `CreatePlayer` | 1 | harmless | id: `(u32)player << 3 | pak` is the profile's 32-bit id in its save file, never a pointer again |
| `src/blob/groups/save/AudioInit.c` | `AudioInit` | 3 | harmless | heap: `PurgeableMemory` of `AllocateMemoryHeap` blocks |
| `src/blob/groups/save/save.c` | `SaveInit` | 2 | harmless | heap: `PurgeableMemory` of `AllocateMemoryHeap` blocks |
| `src/blob/groups/car_sounds/StartCarSounds.c` | `StartCarSounds` | 1 | harmless | dead store: `object = (s32)&gCarActive[player]` is overwritten before any use |
| `src/blob/groups/car_sounds/UpdateCarSounds.c` | `UpdateEngineSound` | 1 | harmless | ignored: `AudEmitEffect`'s `a1` |
| `src/blob/groups/car_sounds/UpdateCarSounds.c` | `UpdateCarSounds` | 1 | harmless | ignored: `AudEmitEffect`'s `a1` |
| `src/blob/groups/paksysdc/group.c` | `RemovePak` | 1 | harmless | heap: frees a pak file's data handle |
| `src/blob/groups/paksysdc/group.c` | `PS_DeleteFile` | 1 | harmless | heap: frees a pak file's data handle |
| `src/blob/groups/zoid_draw/group.c` | `ZOID_EndFrame` | 3 | harmless | window: `pal->table` is a u32 window address (WAD palettes, the car palettes in the arena), zero-extended |
| `src/blob/groups/save_limbo/SaveGhostInLimbo.c` | `SaveGhostInLimbo` | 1 | harmless | heap: frees a score record's handle |
| `src/blob/groups/state/state.c` | `LoadFrontEndOverlay` | 2 | harmless | window: frees the overlay block 0x8038A400 |
| `src/blob/groups/game_sounds/StartListeners.c` | `StartListeners` | 1 | records agent | `gPlayers[i].message` gets a pointer as `s32` |
| `src/blob/ClearPlayerRecords.c` | `ClearPlayerRecords` | 1 (0) | **bug**, fixed | passed the profile handle (host storage) to `RemovePlayerScores` as `s32` |
| `src/blob/SkyUpdate.c` | `SkyUpdate` | 2 | **bug**, fixed | read a window address (camera matrix in the arena) through an `s32`: sign-extended; a u32 now, its 2 warnings stay |
| `src/blob/musyx_alloc.c` | `musyx_alloc` | 1 | harmless | heap: `PurgeableMemory` of an `AllocateMemoryHeap` block |
| `src/rom/scheduler.c` | `__scExecTask` | 1 | harmless | window: a frame buffer (heap) into `__osScPendingSwap`, read back zero-extended |
| `src/rom/scheduler.c` | `__scHandleRetrace` | 1 | harmless | window: `(void *)(u32)` zero-extends |
| `src/rom/scheduler.c` | `__scMain` | 1 | harmless | small integer: scheduler message codes |
| `src/rom/sound/hardware.c` | `sound_hw_reset_dma_list` | 1 | harmless | D-cache cursor (no-op writebacks) over a heap block |
| `src/rom/sound/hardware.c` | `sound_hw_submit_task` | 2 | harmless | small integer: the ucode boot size, a difference of two arena addresses |
| `src/rom/sound/hardware.c` | `sound_hw_build_voice_command` | 9 | harmless | cart address: the `V->flags & 1` path, which `sound_hw_set_voice_sample`'s PC test keeps for 32-bit cart addresses |
| `src/rom/sound/hardware.c` | `sound_hw_begin_sfd_set` | 1 | harmless | D-cache cursor over a heap block |
| `src/rom/sound/hardware.c` | `sound_hw_build_subframe` | 2 | harmless | D-cache cursor over a heap block |
| `src/rom/sound/hardware.c` | `sound_hw_translate_address` | 1 | harmless | guarded: `PC_ONLY` returns a host pointer before the 32-bit test |
| `src/rom/sound/registry.c` | `sound_add_layer` | 1 | harmless | window: a registry payload, a u32 window address into a wave file (MUSYX_DATA.md) |
| `src/rom/sound/registry.c` | `sound_add_table` | 1 | harmless | window: a registry payload |
| `src/rom/sound/registry.c` | `sound_find_table` | 1 | harmless | window: a u32 payload, zero-extended |
| `src/rom/sound/registry.c` | `sound_find_layer` | 1 | harmless | window: a u32 payload, zero-extended |
| `src/rom/sound/sequencer.c` | `sound_seq_start` | 6 | harmless | window: song offsets rebased on the song, a heap block |
| `src/rom/sound/sequencer.c` | `sound_seq_play_tracks` | 10 | harmless | window: song offsets rebased on the song |
| `src/rom/speech/streams.c` | `speech_set_bank` | 1 | unknown | a stream's source tested for K0; no direct caller in the ROM's code, so only ROM data can reach it; a host pointer would take the cartridge path |
| `src/rom/libc/memset.c` | `n64_memset` | 1 | harmless | low bits: an alignment test |

### 5.2 Weak copies

| File | Functions (warnings), each a weak copy of the owner's | Owner |
|---|---|---|
| `nonmatching/game/PS_ReadPartialFile.c` | `RemovePak` (1), `PS_DeleteFile` (1) | `src/blob/groups/paksysdc/group.c` |
| `src/blob/groups/waves/group.c` | `prepare_model_dl` (1) | `src/blob/prepare_model_dl.c` |
| `src/blob/groups/StartCar/group.c` | `StartWingFlame` (1) | `src/blob/groups/StartWingFlame/group.c` |
| `src/blob/groups/AudioInitSys/ctx_func_800A4E58.c` | `AdjustTextWavePointers` (3) | `src/blob/groups/slot_sound/AdjustTextWavePointers.c` |
| `src/blob/groups/SetupCarColor/ctx_alloc_at.c` | `allocate_memory_at` (7) | `src/blob/groups/heap_release/alloc_at.c` |
| `src/blob/groups/SetupCarColor/ctx_car_damage_visual.c` | `ClearMemoryHeap` (3) | `src/blob/groups/heap_release/ClearMemoryHeap.c` |
| `src/blob/groups/SetupCarColor/ctx_group.c` | `unload_extra_overlay` (1), `unload_start_overlay` (1), `DeallocateMemoryInternal` (6), `FindChunk` (5), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/SetupCarColor/ctx_overlay_loader.c` | `load_overlay` (1) | `src/blob/groups/heap_release/overlay_loader.c` |
| `src/blob/groups/purgeable_memory/group.c` | `unload_extra_overlay` (1), `unload_start_overlay` (1), `DeallocateMemoryInternal` (6), `FindChunk` (3), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/hud_text_anims/gr3_e.c` | `updateAudioScreen` (2) | `src/blob/groups/audioscr_update/updateAudioScreen.c` |
| `src/blob/groups/free_sequence/group.c` | `ProcessSequences` (20) | `src/blob/groups/audiomgr/ProcessSequences.c` |
| `src/blob/groups/destroy_heap/heap_release_context.c` | `unload_extra_overlay` (1), `unload_start_overlay` (1), `DeallocateMemoryInternal` (6), `FindChunk` (5), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/game_music/func_800D6530.c` | `StartListeners` (1) | `src/blob/groups/game_sounds/StartListeners.c` |
| `src/blob/groups/heap_release/zip.c` | `zcfree` (1) | `src/blob/groups/heap_release/zcfree.c` |
| `src/blob/groups/heap_release/group.c` | `FindChunk` (3), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c` |
| `src/blob/groups/save/ghost_files.c` | `DeallocateMemoryInternal` (6), `FindChunk` (3), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/paksysdc/ctx_heap.c` | `unload_extra_overlay` (1), `unload_start_overlay` (1), `DeallocateMemoryInternal` (6), `FindChunk` (5), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/pointer_counters/group.c` | `FindHeap` (4) | `src/blob/FindHeap.c` |
| `src/blob/groups/resurrect/particle_context.c` | `ZOID_EndFrame` (3) | `src/blob/groups/zoid_draw/group.c` |
| `src/blob/groups/save_limbo/alloc_at.c` | `allocate_memory_at` (7) | `src/blob/groups/heap_release/alloc_at.c` |
| `src/blob/groups/save_limbo/car_damage_visual.c` | `ClearMemoryHeap` (3) | `src/blob/groups/heap_release/ClearMemoryHeap.c` |
| `src/blob/groups/save_limbo/deflate_mem.c` | `zcfree` (1) | `src/blob/groups/heap_release/zcfree.c` |
| `src/blob/groups/save_limbo/group.c` | `unload_extra_overlay` (1), `unload_start_overlay` (1), `DeallocateMemoryInternal` (6), `FindChunk` (5), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/save_limbo/overlay_loader.c` | `load_overlay` (1) | `src/blob/groups/heap_release/overlay_loader.c` |
| `src/blob/groups/state/ctx_alloc_at.c` | `allocate_memory_at` (7) | `src/blob/groups/heap_release/alloc_at.c` |
| `src/blob/groups/state/ctx_heap_group.c` | `unload_extra_overlay` (1), `unload_start_overlay` (1), `DeallocateMemoryInternal` (6), `FindChunk` (5), `FindHeap` (4) | `src/blob/FindChunk.c`, `src/blob/FindHeap.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/state/ctx_overlay_loader.c` | `load_overlay` (1) | `src/blob/groups/heap_release/overlay_loader.c` |
| `src/blob/groups/state_uninit/group.c` | `LoadWave` (2) | `src/blob/groups/waves/loadwave.c` |

The weak copies run never: gen_weak gives every call the owner's definition. Most are the heap group's context
copies, which POINTERS.md 4.1 left unconverted on purpose; the owners' versions are the ones fixed there.

## 6. What the census doesn't count: implicit conversions

The game flags include `-Wno-int-conversion`, so a pointer assigned to an integer, or an integer passed for a
pointer, without a cast, is not a warning. With `-Wint-conversion` there are 58 such conversions, 47 in live
functions:

| | Live | Class |
|---|---:|---|
| a u32 into a pointer: zero-extended window addresses (WAD texture and palette words, the texture animations' display lists, the car palettes, MusyX payloads, overlay A's map lists) | 12 | harmless |
| a pointer into a u32: window addresses into N64-layout fields (heap blocks, the car palette buffers, MusyX payloads and sample addresses) | 11 | harmless |
| `intptr_t`/`uintptr_t` into a pointer | 9 | harmless (same size) |
| arguments into prototypes of the other type: u32 fields passed for pointers (9), pointers for `intptr_t` parameters (4) | 13 | harmless |
| an `s32` into a pointer: `AudioSfxServer` (section 2.3) | 1 | **bug**, fixed |
| an `s32` into a pointer: `NewCircuit` (the input record's profile, its save data read as `s32`) | 1 | records agent |

## 7. For the other parts

- **Records agent.** The five warnings of section 5 and `NewCircuit`: `GetCircuitCode` and `NewCircuit`
  reach the input records as `gPlayers_save` + 0x4C x player and read the profile's save data through its handle
  (`SaveSlot.data`, `SLOT_DATA`) as an `s32`, which sign-extends it (the data, `gRealJustPlayerData` in the arena or a heap
  block, is window data); `animRumbleText`, `StopListeners` and `StartListeners` hold `.name`, `.object` and
  `.message` as `s32`; `SetTargetDefs` compares an `s32` GTLD key with a pointer slot, which sign-extends
  keys with bit 31 set. The texture-animation records `texture_swap_shared_frames` take WAD words zero-extended
  (`InitTextureAnimation`, `TextureAnimation`): harmless as they are. Done (POINTERS.md 10.3): the save data and the profile handle are read
  pointer-sized, `.object` and `.message` are a 32-bit packet slot converted explicitly, and the GTLD key compare
  sign-extends on purpose, as the side array holds the keys.
- **The pak file record `tPS_File`** (paksys' view: 76 bytes on the N64, 96 on the PC) has about ten views that
  keep the N64 layout under u32s and pads (state.c's and controlscr's `PSFile`, overlay A's menu `Slot`s), and raw
  N64 offsets in the save code: `CreatePlayer` and `_ReadGhostHeader` set `RemoveFile` and `MoveFile` at +8 and
  +12 (on the PC +16 and +24) and read `data` at +72 (+88); `ClearPlayerRecords` reads the pak number at +0x10
  (+0x20). The 8-byte store at +12 puts the upper half of `MovePlayerFile`'s address into the low half of the PC's
  `RemoveFile`, and `PS_DeleteFile` and `RemovePak` call `RemoveFile` when it isn't NULL: deleting a
  profile's or a ghost's file would jump there. This is the next pointer job in the front end. `MovePlayerFile`,
  installed as `MoveFile`, is never called (no indirect call through +12 in the ROM's code) and reaches profiles,
  ghosts and file records at N64 offsets. Done (POINTERS.md 9.10): every view has the 96-byte layout, the raw
  offsets go through `PSF_OFS`, and both file functions take the handle whole.
- **The controller/pak records** (POINTERS.md 9.6): `UpdateModel` clears `gPaks_rumble_request + pak * 0x304`, N64 +0x7D
  (the rumble request) at the N64 stride; on the PC the record is 0x310 bytes and the field later. Done: the PC
  stride is 0x310 (`PAK_RECORD`), and gen_data places `gPaks_rumble_request` at +0x89.
- The profile record itself (`SaveSlot`, `Player`): the profile list `gPlayerFiles` is indirect and doubly linked
  (`InitLL(&gPlayerFiles, 1, 1)`), so `InsertLLE` links a profile through pointer-sized `next` and `prev`
  at +0 and +8, where the save code's views keep the file handle at N64 +8 (`F(p, Handle, 8)`, `char pad0[20]`).
  One PC layout for it goes with the `tPS_File` work. Done (POINTERS.md 9.10): 64 bytes, the file handle at +16 and
  the data at +56 (`PROFILE_OFS`), in every live view; the ghost header keeps its N64 88 bytes, as it is file data.
  The heap's handle slots, which the save code reads through as pointers, are 8 bytes on the PC.

## 8. Needs the ROM

- `speech_set_bank` (`streams.c`, a stream's source): whether anything reaches it, through ROM data, with a host
  pointer.
- That every value the harmless "window" and "heap" rows assume is window data really is: a `RUSH_CHECK_CASTS`
  build through the front end, a race (with the blast, visual and ghost paths above) and a ghost save.
- The ghost paths (`_NewGhost`, `UpdateModel`'s header read, the high-score names with a saved profile) run only
  with pak data and races.
