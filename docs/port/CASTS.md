# Pointer/integer casts in the game C

Every `-Wpointer-to-int-cast` and `-Wint-to-pointer-cast` the census counts in the game C (`src/`, `nonmatching/`),
classified, and what was fixed. 2026-10-27. Related: [POINTERS.md](POINTERS.md) (the techniques, section 4 the
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

The 6 fewer are the fixed sites; `particles_spawn_emitter`'s 2 stay, as correct zero extensions of a u32. No
harmless warning was silenced: the N64-world casts say what the code does (POINTERS.md 4.1), and the neutral
rewrite, `(u32)` to `(uintptr_t)`, would only move the truncation into an implicit conversion.

Fixed on the way, with no census warning: the high-score name table's views (`gTotalNames`, 8 files), the ghost
records and their allocator, and `camera_transform`'s sound-request walk, found among the implicit conversions
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
| `camera_smooth_lerp` (InitBlasts) | wrote the 50 blast matrices into gBlastMats `D_8013C238` as `s32` at a 4-byte stride; on the PC the table is blast.c's `tBlastMat[50]`, 8 bytes an entry (own storage), so each matrix pointer the explosion code read was two window addresses glued together | `BLAST_MAT(p, i)` / `BLAST_MAT_PTR`: pointer stores on the PC, the original tokens on the N64; the PC declaration is `void *D_8013C238[50]` |
| `PhysicsObjectList_Update` (every frame, `game_loop`) | walked the environment visuals (`D_801391F0` -> `D_80138880`, C-defined host storage, `struct pc_visual`) through `s32` locals and called each visual's `func` from N64 +0x14, `timeStamp` on the PC | `intptr_t` locals and links, `VIS_OFS(0x14)` (0x18) |
| `particles_spawn_emitter` | `D_80124FD0` (arena) holds each window's camera matrix, a window address from `func_800A7D6C`'s pool in the arena; read through an `s32`, it was sign-extended | `MTX_WORD`: u32 on the PC |
| `func_800E56F8` | a ghost handle from gGhosts (`D_80152698`, own storage) in an `s32`, sign-extended, and the ghost's header read at N64 +0x28 | `intptr_t temp_v1`, `GHOST_HEADER` (0x38 on the PC) |
| `func_800F2888` | stored a player's profile handle, `&D_80146150[n]` (own storage), into the name slot `D_801424F0` (arena `s32[4]`) | `D_801424F0` is `intptr_t` (own storage now); its "no name" test compares with -1 on the PC (`NO_NAME`), since `0xFFFFFFFF` no longer equals a 64-bit -1 |
| `menu_confirm_dialog` (clear a save slot) | passed the profile handle to `audio_channel_priority` as `s32` | `(intptr_t)`, and `audio_channel_priority(intptr_t)` |

### 2.1 The high-score name table

`gTotalNames` (`D_80151690`, 12 tracks x 3 rows x 5 places) and `D_801424F0` hold, per slot, a name index into
`D_80151968` (0..19), -1 for none, or a profile handle. PrintScores (`func_800F0914`) tells them apart (-1, <= 20, a
pointer); `draw_text` and `func_800F56E0` store handles as pointers, `func_800F207C` copies `D_801424F0` in,
`audio_channel_priority` removes a handle, `func_800F1D04` counts name references. Natively the views disagreed:
three had 8-byte slots (1440 bytes), four `s32` slots (720), and assign_drones' `Owner` a 0x28-byte pad and five
pointers, which gave the storage 960 bytes, so the pointer views ran past it. Every live view has pointer-sized
slots now (`intptr_t` in `func_800C9480`, `func_800F1D04` (and its `slot` local, which a handle's low half could
make look like a name slot), `func_800F207C`, `audio_channel_priority`; assign_drones' `Owner` under `TARGET_PC`):
1440 bytes, `view_check --sym gTotalNames` 0 disagreements.

### 2.2 The ghosts

A ghost (menu_save_options' `Ghost`, GC `__anon_49a9c`) is 0x2C bytes on the N64 and 64 on the PC: its link, file
handle, data handle and header handle are pointers. `_NewGhost` (`func_800C7200`) kept 64 four-byte handle slots
at `D_8013F1F8` and 64 records 0x2C apart at `D_8013F390` (game arena) and stored each handle as an 8-byte pointer,
so the second ghost overwrote the first's handle and the records overlapped. Its PC version keeps both in host
storage (nothing else references them), so ghost handles are host pointers; every place that holds one was
checked to be pointer-typed. Four live views had the N64 layout under pads or a u32: highscore.c's `Ghost`,
cleanup_colortrace's `Record` (`unknown24` is the data handle), and the race code's `Obj40`
(`car_checkpoints/race_tail.c`) and `Ghost40` (`update_game/countdown.c`).
`view_check --tag Ghost,Obj,Record,SavedRecord,ReplaySlot,Ghost40,Obj40 --n64-size 0x2C` over the 14 views: 0 live
disagreements (4 before; two weak copies still differ).

### 2.3 The sound requests (`camera_transform`)

`_PeriodicMaintenance`'s request walk copied the request block's address `D_80144C48` (a heap block) into an
`s32` and formed a finished request's address from it with an implicit conversion, which sign-extends: retiring a
finished sound wrote to 0xFFFFFFFF8xxxxxxx in the audio manager thread. The copy is `intptr_t`, and the request
nodes' `entry` it is compared with is read pointer-sized (`PcSfxNode`, MUSYX_DATA.md 8.4).

## 3. Tests

`assets_casts` (`port/assets/tests/test_casts.c`) runs the game's code on host pointers (above 4 GB, as own
storage is) and on window data in a mapped RDRAM window:

| Part | Game code | Checks | With the fix put back |
|---|---|---|---|
| blast matrices | `camera_smooth_lerp` | each of the 50 entries is the pointer `func_800A7D6C` returned | 50 checks fail |
| visual walk | `PhysicsObjectList_Update` | each host visual's `func` called with it and 1, one that ends itself, one without | fault |
| camera matrix | `particles_spawn_emitter` | a window-address matrix gets the camera's two values | fault |
| name table | `func_800C9480`, `audio_channel_priority`, `menu_confirm_dialog`, `func_800F1D04`, `func_800F2888` | all 180 slots cleared to -1; a host handle removed from every row while a value with the same low half stays; the slot clear passes the whole handle; a handle that looks like slot 19 is no reference; a profile handle kept whole, and -1 clears the name | 1 (`func_800C9480`), 6 (`audio_channel_priority`), 2 (`func_800F1D04`), 2 (`func_800F2888`); `menu_confirm_dialog`'s `(s32)` fails only on x86-64, which truncates the register |
| ghosts | `_NewGhost` | 64 separate handles and whole 64-byte records, the 65th NULL, a freed slot reused | 4 checks fail |
| sound requests | `camera_transform` | a finished request in the window cleared in place, a playing one kept | fault |
| `RUSH_CHECK_CASTS` | `PC_N64W` | window addresses and small values pass, a host address calls `pc_cast_fail` with the line | |

The test passes on aarch64 (gcc 12) and x86-64 (gcc 14), and the old `menu_confirm_dialog` fails it on x86-64.

## 4. RUSH_CHECK_CASTS

The N64-world casts are harmless only while what reaches them is window data, which the ROM decides. The records
that keep their N64 layout on the PC (the heap headers, the model record, the visuals' `data`, WAD records, the
voice and DMA records) take pointers through each file's `N64W(x)`. The 39 PC definitions of `N64W` are
`PC_N64W(x)` now (`port/include/pc_prelude.h`), the same u32 conversion. Built with
`cmake -DRUSH_CHECK_CASTS=ON`, `PC_N64W` checks that the value's upper half is zero and otherwise calls
`pc_cast_fail` (`port/ultra/castcheck.c`), which prints the source line, the value and the N64 thread, the
backtrace, and aborts:

    rush2049pc: RUSH_CHECK_CASTS: .../src/blob/groups/audio_heap/group.c:191 stores host address 0x... as a 32-bit N64 address (N64 thread ...)

The reference is weak, so test programs that link game C without port/ultra still link. A checked build passes
ctest and the selftest and boots on a blank cartridge to the usual stop. Run it with the ROM through a race.

It doesn't see the raw `(u32)p` casts of the heap API (`audio_reverb_update((u32)p, 0)`, `PurgeableMemory`):
checking those would need a new token at each call. The heap's own entry points could check that what they get
is a heap block (`func_80095F8C` finds the heap of an address), which would catch a truncated host pointer with a
probability of about 1 - 8 MB / 4 GB.

## 5. Every warning

"(n)" after a count: the warnings left after the fixes, where that differs.

### 5.1 Live functions

| File | Function | Warnings (left) | Class | Why |
|---|---|---:|---|---|
| `nonmatching/game/drone_set_catchup.c` | `drone_set_catchup` | 1 | harmless | heap: `pdf_dealloc` frees `psfile->data`, an `AllocateHandleHeap` handle |
| `nonmatching/game/ghost_race_setup.c` | `ghost_race_setup` | 1 | harmless | heap: DeallocateMemory of the ghost's data block |
| `src/blob/MBSetupWad.c` | `MBSetupWad` | 6 | harmless | window: rebases a WAD's TOC in place; the WAD is a heap block (`SetupWaveModel`, `LoadTrack`) |
| `src/blob/PhysicsObjectList_Update.c` | `PhysicsObjectList_Update` | 2 (0) | **bug**, fixed | walked the environment visuals (C-defined, host) through `s32` and called `func` at N64 +0x14 |
| `src/blob/camera_smooth_lerp.c` | `camera_smooth_lerp` | 1 (0) | **bug**, fixed | wrote the 50 blast matrix pointers of `D_8013C238` (own storage, 8 bytes each) as `s32` at a 4-byte stride |
| `src/blob/func_800949D4.c` | `func_800949D4` | 1 | harmless | never runs: the `tPS_File` `MoveFile` hook (+12) it is installed as has no caller in the ROM's code; its N64 offsets would be wrong on the PC (section 7) |
| `src/blob/func_80095EF4.c` | `func_80095EF4` | 3 | harmless | window: the heap's handle lookup over its N64-layout headers |
| `src/blob/func_800F42C8.c` | `func_800F42C8` | 1 | records agent | input record `D_8014A160` (+0x48 of `D_8014A118`) at a 0x4C stride; reads the profile's save data pointer as `s32` at N64 `SLOT_DATA` |
| `src/blob/groups/audio_frame_sync/waves.c` | `SetupWaveModel` | 8 | harmless | window: WAD relocation, `WADP(u32)` fields, offsets between two heap addresses |
| `src/blob/groups/audio_heap/group.c` | `AllocateHandleHeap` | 1 | harmless | window: a heap block into a heap handle slot |
| `src/blob/groups/audio_heap/group.c` | `func_800E7B44` | 2 | harmless | window: heap header set-up |
| `src/blob/groups/audio_heap/group.c` | `func_800E7C2C` | 1 | harmless | window: a sub-heap's end |
| `src/blob/groups/audio_heap/group.c` | `func_800E7D04` | 1 | harmless | window: the heap's start, `align32(0x8017A640)` |
| `src/blob/groups/audio_init_sys/car_shadow_render.c` | `car_shadow_render` | 1 | harmless | small integer: `D_00039B40` is the sample bank's ROM offset 0x39B40 |
| `src/blob/groups/blast/blast.c` | `save_write_data` | 1 | harmless | ignored: EmitGameSound's `dir` reaches `camera_target_track`'s unread `a1` (and is the arena's unit matrix) |
| `src/blob/groups/camera_aspect_ratio/group.c` | `camera_track_entry` | 1 | harmless | ignored: `camera_target_track`'s `a1` is never read |
| `src/blob/groups/carcolor/carcolor.c` | `sound_bank_unload` | 1 | harmless | heap: frees its `AllocateMemoryHeap` palette buffer |
| `src/blob/groups/carcolor/ctx_deflate_mem.c` | `zcfree` | 1 | harmless | heap: zlib's ZFREE of a `ZALLOC` (`AllocateMemoryHeap`) block |
| `src/blob/groups/carcolor/ctx_initsystem.c` | `FreeBuffers_Init` | 2 | harmless | never runs: a static of the weak `game_late_init` copy |
| `src/blob/groups/carcolor/ctx_initsystem.c` | `_InitGfx_Init` | 2 | harmless | never runs: a static of the weak `game_late_init` copy |
| `src/blob/groups/func_800E56F8/group.c` | `func_800E56F8` | 1 (0) | **bug**, fixed | held a ghost handle from `gGhosts` in an `s32` (sign extension) and read its header at N64 +0x28 |
| `src/blob/groups/func_800E7A98/func_800E7A98.c` | `func_800E7A98` | 1 | harmless | heap: frees a sub-heap |
| `src/blob/groups/func_800F2888_grp/func_800F2888.c` | `func_800F2888` | 1 (0) | **bug**, fixed | stored a profile handle (`&D_80146150[n]`, own storage) into the `s32` name slot `D_801424F0` |
| `src/blob/groups/func_8010BC84/fbc84.c` | `func_8010BC84` | 1 | records agent | `D_8014A118[i].name` held as `s32` and turned into a pointer |
| `src/blob/groups/heap_release/alloc_at.c` | `NextMaxPath` | 6 | harmless | window: heap block arithmetic (the overlay reservation) |
| `src/blob/groups/heap_release/car_damage_visual.c` | `car_damage_visual` | 3 | harmless | window: heap compaction (ClearMemory) |
| `src/blob/groups/heap_release/group.c` | `MP_TargetSpeed` | 1 | harmless | window: frees the overlay block 0x8038A400 |
| `src/blob/groups/heap_release/group.c` | `assign_default_paths` | 1 | harmless | window: frees the overlay block 0x8038A400 |
| `src/blob/groups/heap_release/group.c` | `audio_reverb_update` | 6 | harmless | window: DeallocateMemory's core over N64-layout headers |
| `src/blob/groups/heap_release/initsystem.c` | `FreeBuffers_Init` | 2 | harmless | heap: `D_8011025C`/`D_80110260` hold `AllocateMemoryHeap` blocks |
| `src/blob/groups/heap_release/initsystem.c` | `_InitGfx_Init` | 2 | harmless | heap: the frame buffers (`AllocateMemoryHeapHigh`) as `s32`, read back through `PC_ZEXT` |
| `src/blob/groups/heap_release/overlay_loader.c` | `PrevMaxPath` | 1 | harmless | window: `NextMaxPath((u32)dst)` with dst = 0x8038A400 (an overlay loader) |
| `src/blob/groups/level_objects/group.c` | `engine_sound_update` | 1 | records agent | the level-object node pool `D_80143FC8`: an `s32` GTLD key compared with a pointer slot, sign-extended |
| `src/blob/groups/loadtrack/loadtrack.c` | `LoadTrack` | 1 | harmless | window: WHDR offsets rebased on the world file, a heap block |
| `src/blob/groups/main_menu_input/object_activate.c` | `func_800D5374` | 1 | records agent | `D_8014A118[i].object` held as `s32` and turned into a pointer |
| `src/blob/groups/menu_save_options/ctx_newghost.c` | `CompressGhost` | 3 | harmless | heap: frees the zip buffer; a heap block's size |
| `src/blob/groups/menu_save_options/ctx_newghost.c` | `ghost_free_start` | 1 | harmless | heap: frees the ghost data |
| `src/blob/groups/menu_save_options/ctx_slot.c` | `AdjustSteer` | 1 | harmless | heap: frees a slot's data handle |
| `src/blob/groups/menu_save_options/ctx_slot.c` | `SaveGhost` | 2 | harmless | heap: a handle's block size |
| `src/blob/groups/menu_save_options/ctx_slot.c` | `cart_free` | 1 | harmless | heap: frees a handle |
| `src/blob/groups/menu_save_options/ctx_slot.c` | `cart_free_direct` | 1 | harmless | heap: frees a handle |
| `src/blob/groups/menu_save_options/ctx_slot.c` | `menu_transition` | 1 | harmless | heap: frees a handle |
| `src/blob/groups/menu_save_options/group.c` | `CreatePlayer` | 1 | harmless | id: `(u32)player << 3 | pak` is the profile's 32-bit id in its save file, never a pointer again |
| `src/blob/groups/menu_save_options/hud_setup.c` | `hud_setup` | 3 | harmless | heap: `PurgeableMemory` of `AllocateMemoryHeap` blocks |
| `src/blob/groups/menu_save_options/save.c` | `menu_save_options` | 2 | harmless | heap: `PurgeableMemory` of `AllocateMemoryHeap` blocks |
| `src/blob/groups/mode_select/d5e64.c` | `func_800D5E64` | 1 | harmless | dead store: `object = (s32)&D_8010FFC4[player]` is overwritten before any use |
| `src/blob/groups/mode_select/mode.c` | `func_800E0050` | 1 | harmless | ignored: `camera_target_track`'s `a1` |
| `src/blob/groups/mode_select/mode.c` | `func_800E05F0` | 1 | harmless | ignored: `camera_target_track`'s `a1` |
| `src/blob/groups/paksys/group.c` | `func_800A3640` | 1 | harmless | heap: frees a pak file's data handle |
| `src/blob/groups/paksys/group.c` | `pak_delete_file_retry` | 1 | harmless | heap: frees a pak file's data handle |
| `src/blob/groups/particle_knot/group.c` | `Input_ProcessGameplayPad` | 3 | harmless | window: `pal->table` is a u32 window address (WAD palettes, the car palettes in the arena), zero-extended |
| `src/blob/groups/score_insert/func_800CB748.c` | `func_800CB748` | 1 | harmless | heap: frees a score record's handle |
| `src/blob/groups/state/state.c` | `display_enable` | 2 | harmless | window: frees the overlay block 0x8038A400 |
| `src/blob/groups/vsync/func_800D6530.c` | `func_800D6530` | 1 | records agent | `D_8014A118[i].message` gets a pointer as `s32` |
| `src/blob/menu_confirm_dialog.c` | `menu_confirm_dialog` | 1 (0) | **bug**, fixed | passed the profile handle (host storage) to `audio_channel_priority` as `s32` |
| `src/blob/particles_spawn_emitter.c` | `particles_spawn_emitter` | 2 | **bug**, fixed | read a window address (camera matrix in the arena) through an `s32`: sign-extended; a u32 now, its 2 warnings stay |
| `src/blob/resource_alloc_init.c` | `resource_alloc_init` | 1 | harmless | heap: `PurgeableMemory` of an `AllocateMemoryHeap` block |
| `src/rom/lib_1050_sdk.c` | `__scExecTask` | 1 | harmless | window: a frame buffer (heap) into `__osScPendingSwap`, read back zero-extended |
| `src/rom/lib_1050_sdk.c` | `__scHandleRetrace` | 1 | harmless | window: `(void *)(u32)` zero-extends |
| `src/rom/lib_1050_sdk.c` | `__scMain` | 1 | harmless | small integer: scheduler message codes |
| `src/rom/lib_11640.c` | `func_80011894` | 1 | harmless | D-cache cursor (no-op writebacks) over a heap block |
| `src/rom/lib_11640.c` | `func_80011910` | 2 | harmless | small integer: the ucode boot size, a difference of two arena addresses |
| `src/rom/lib_11640.c` | `func_80012D18` | 9 | harmless | cart address: the `V->flags & 1` path, which `func_800146B4`'s PC test keeps for 32-bit cart addresses |
| `src/rom/lib_11640.c` | `func_80013964` | 1 | harmless | D-cache cursor over a heap block |
| `src/rom/lib_11640.c` | `func_800139D4` | 2 | harmless | D-cache cursor over a heap block |
| `src/rom/lib_11640.c` | `func_80014CAC` | 1 | harmless | guarded: `PC_ONLY` returns a host pointer before the 32-bit test |
| `src/rom/lib_16320.c` | `func_80015A0C` | 1 | harmless | window: a registry payload, a u32 window address into a wave file (MUSYX_DATA.md) |
| `src/rom/lib_16320.c` | `func_80015D68` | 1 | harmless | window: a registry payload |
| `src/rom/lib_16320.c` | `func_80016E68` | 1 | harmless | window: a u32 payload, zero-extended |
| `src/rom/lib_16320.c` | `func_80016F80` | 1 | harmless | window: a u32 payload, zero-extended |
| `src/rom/lib_17dc0.c` | `func_800178B0` | 6 | harmless | window: song offsets rebased on the song, a heap block |
| `src/rom/lib_17dc0.c` | `func_80018634` | 10 | harmless | window: song offsets rebased on the song |
| `src/rom/lib_25bb0.c` | `func_80025AB4` | 1 | unknown | a stream's source tested for K0; no direct caller in the ROM's code, so only ROM data can reach it; a host pointer would take the cartridge path |
| `src/rom/lib_3390.c` | `n64_memset` | 1 | harmless | low bits: an alignment test |

### 5.2 Weak copies

| File | Functions (warnings), each a weak copy of the owner's | Owner |
|---|---|---|
| `nonmatching/game/drone_set_catchup.c` | `func_800A3640` (1), `pak_delete_file_retry` (1) | `src/blob/groups/paksys/group.c` |
| `src/blob/groups/audio_frame_sync/group.c` | `entity_lod_select` (1) | `src/blob/entity_lod_select.c` |
| `src/blob/groups/audio_frame_update/group.c` | `func_800B08FC` (1) | `src/blob/groups/audio_frame/group.c` |
| `src/blob/groups/audio_init_sys/ctx_func_800A4E58.c` | `func_800A4E58` (3) | `src/blob/groups/slot_sound/func_800A4E58.c` |
| `src/blob/groups/carcolor/ctx_alloc_at.c` | `NextMaxPath` (7) | `src/blob/groups/heap_release/alloc_at.c` |
| `src/blob/groups/carcolor/ctx_car_damage_visual.c` | `car_damage_visual` (3) | `src/blob/groups/heap_release/car_damage_visual.c` |
| `src/blob/groups/carcolor/ctx_group.c` | `MP_TargetSpeed` (1), `assign_default_paths` (1), `audio_reverb_update` (6), `func_80095EF4` (5), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/carcolor/ctx_overlay_loader.c` | `PrevMaxPath` (1) | `src/blob/groups/heap_release/overlay_loader.c` |
| `src/blob/groups/counter_guard/group.c` | `MP_TargetSpeed` (1), `assign_default_paths` (1), `audio_reverb_update` (6), `func_80095EF4` (3), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/credits_scroll_grp/gr3_e.c` | `func_800D7634` (2) | `src/blob/groups/audio_options_menu/func_800D7634.c` |
| `src/blob/groups/entity_flag_check/group.c` | `func_800988D8` (20) | `src/blob/groups/engine_sound_sync/audio_effect_setup_b132.c` |
| `src/blob/groups/func_800E7A98/heap_release_context.c` | `MP_TargetSpeed` (1), `assign_default_paths` (1), `audio_reverb_update` (6), `func_80095EF4` (5), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/game_music/func_800D6530.c` | `func_800D6530` (1) | `src/blob/groups/vsync/func_800D6530.c` |
| `src/blob/groups/heap_release/deflate_mem.c` | `zcfree` (1) | `src/blob/groups/carcolor/ctx_deflate_mem.c` |
| `src/blob/groups/heap_release/group.c` | `func_80095EF4` (3), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c` |
| `src/blob/groups/menu_save_options/ctx_slot.c` | `audio_reverb_update` (6), `func_80095EF4` (3), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/paksys/ctx_heap.c` | `MP_TargetSpeed` (1), `assign_default_paths` (1), `audio_reverb_update` (6), `func_80095EF4` (5), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/pointer_counters/group.c` | `func_80095F8C` (4) | `src/blob/func_80095F8C.c` |
| `src/blob/groups/resurrect/particle_context.c` | `Input_ProcessGameplayPad` (3) | `src/blob/groups/particle_knot/group.c` |
| `src/blob/groups/score_insert/alloc_at.c` | `NextMaxPath` (7) | `src/blob/groups/heap_release/alloc_at.c` |
| `src/blob/groups/score_insert/car_damage_visual.c` | `car_damage_visual` (3) | `src/blob/groups/heap_release/car_damage_visual.c` |
| `src/blob/groups/score_insert/deflate_mem.c` | `zcfree` (1) | `src/blob/groups/carcolor/ctx_deflate_mem.c` |
| `src/blob/groups/score_insert/group.c` | `MP_TargetSpeed` (1), `assign_default_paths` (1), `audio_reverb_update` (6), `func_80095EF4` (5), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/score_insert/overlay_loader.c` | `PrevMaxPath` (1) | `src/blob/groups/heap_release/overlay_loader.c` |
| `src/blob/groups/state/ctx_alloc_at.c` | `NextMaxPath` (7) | `src/blob/groups/heap_release/alloc_at.c` |
| `src/blob/groups/state/ctx_heap_group.c` | `MP_TargetSpeed` (1), `assign_default_paths` (1), `audio_reverb_update` (6), `func_80095EF4` (5), `func_80095F8C` (4) | `src/blob/func_80095EF4.c`, `src/blob/func_80095F8C.c`, `src/blob/groups/heap_release/group.c` |
| `src/blob/groups/state/ctx_overlay_loader.c` | `PrevMaxPath` (1) | `src/blob/groups/heap_release/overlay_loader.c` |
| `src/blob/groups/state_audio/group.c` | `audio_frame_sync` (2) | `src/blob/groups/audio_frame_sync/loadwave.c` |

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
| an `s32` into a pointer: `camera_transform` (section 2.3) | 1 | **bug**, fixed |
| an `s32` into a pointer: `func_800FDF88` (the input record's profile, its save data read as `s32`) | 1 | records agent |

## 7. For the other parts

- **Records agent.** The five warnings of section 5 and `func_800FDF88`: `func_800F42C8` and `func_800FDF88`
  reach the input records as `D_8014A160` + 0x4C x player and read the profile's save data through its handle
  (`SaveSlot.data`, `SLOT_DATA`) as an `s32`, which sign-extends it (the data, `D_8013A028` in the arena or a heap
  block, is window data); `func_8010BC84`, `func_800D5374` and `func_800D6530` hold `.name`, `.object` and
  `.message` as `s32`; `engine_sound_update` compares an `s32` GTLD key with a pointer slot, which sign-extends
  keys with bit 31 set. The texture-animation records `D_8011905C` take WAD words zero-extended
  (`camera_shake_start`, `race_setup_1`): harmless as they are. Done (POINTERS.md 10.3): the save data and the profile handle are read
  pointer-sized, `.object` and `.message` are a 32-bit packet slot converted explicitly, and the GTLD key compare
  sign-extends on purpose, as the side array holds the keys.
- **The pak file record `tPS_File`** (paksys' view: 76 bytes on the N64, 96 on the PC) has about ten views that
  keep the N64 layout under u32s and pads (state.c's and controlscr's `PSFile`, overlay A's menu `Slot`s), and raw
  N64 offsets in the save code: `CreatePlayer` and `_ReadGhostHeader` set `RemoveFile` and `MoveFile` at +8 and
  +12 (on the PC +16 and +24) and read `data` at +72 (+88); `menu_confirm_dialog` reads the pak number at +0x10
  (+0x20). The 8-byte store at +12 puts the upper half of `func_800949D4`'s address into the low half of the PC's
  `RemoveFile`, and `pak_delete_file_retry` and `func_800A3640` call `RemoveFile` when it isn't NULL: deleting a
  profile's or a ghost's file would jump there. This is the next pointer job in the front end. `func_800949D4`,
  installed as `MoveFile`, is never called (no indirect call through +12 in the ROM's code) and reaches profiles,
  ghosts and file records at N64 offsets.
- **The controller/pak records** (POINTERS.md 9.6): `func_800E56F8` clears `D_801440AD + pak * 0x304`, N64 +0x7D
  (the rumble request) at the N64 stride; on the PC the record is 0x310 bytes and the field later.
- The profile record itself (`SaveSlot`, `Player`): the profile list `D_8012E6D8` is indirect and doubly linked
  (`func_800A44E8(&D_8012E6D8, 1, 1)`), so `func_80091FBC` links a profile through pointer-sized `next` and `prev`
  at +0 and +8, where the save code's views keep the file handle at N64 +8 (`F(p, Handle, 8)`, `char pad0[20]`).
  One PC layout for it goes with the `tPS_File` work.

## 8. Needs the ROM

- `func_80025AB4` (`lib_25bb0.c`, a stream's source): whether anything reaches it, through ROM data, with a host
  pointer.
- That every value the harmless "window" and "heap" rows assume is window data really is: a `RUSH_CHECK_CASTS`
  build through the front end, a race (with the blast, visual and ghost paths above) and a ghost save.
- The ghost paths (`_NewGhost`, `func_800E56F8`'s header read, the high-score names with a saved profile) run only
  with pak data and races.
