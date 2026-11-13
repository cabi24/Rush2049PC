# Prototypes the game's files disagree on

Every game C file carries its own declarations of what it calls, and many disagree with the definition. MIPS o32
forgave most of that: an `int` and a pointer share a register, and every argument after a leading integer or
pointer travels in a GPR whatever its type. On aarch64 and x86-64 SysV the same disagreements are wrong arguments.
This page covers the tool that finds them, what was fixed and how, and what is left. 2026-10-27. Related:
[POINTERS.md](POINTERS.md) (the N64-neutral conventions used here), [DATA.md](DATA.md) (data declarations).

## 1. The tool

`python3 tools/port/proto_check.py [--build build/pc] [--jobs 3] [--abi sysv|win64] [--doc docs/port/PROTOTYPES.md]`
(stdlib, binutils and the build's compiler; about 8 s on the Pi 5 with 3 jobs). It writes `build/pc/proto_report.txt`
(every disagreement, nearest the boot first) and `build/pc/proto_report.json`, and with `--doc` the table in section 2.
`--abi win64` judges the same DWARF of a Linux build by the Microsoft x64 convention instead (section 3.6) and writes
`proto_report_win64.txt`.

- **Definitions.** For each external function, the definition the link keeps: the strong one (nm `T`) in the game
  objects, the PC layer's for the replaced libultra units, the host's for the C library names the game calls
  (`bcopy`, `bzero`, `sqrtf`...: a built-in table). Thunk and alias names (`data.c`'s `PC_THUNK`, `aliases.ld`) map to
  their target.
- **Views.** Every other object's DWARF prototype of it: declarations (`DW_TAG_subprogram` with
  `DW_AT_declaration`), weak context copies (calls in that file go through the copy's prototype), and unprototyped
  or implicit declarations.
- **Call sites.** Files whose live code calls a suspect view are compiled again with their own command plus
  `-fsyntax-only -fdump-tree-original-raw -Wimplicit-function-declaration`. The GENERIC tree gives each call's
  promoted argument types (what an unprototyped call really passes), the operand each argument had before the
  prototype converted it ("callers pass s32"), whether the result is used, and which calls are implicit.
- **What runs.** From each object's relocations: weak copies never run (every reference binds to the owner), so a
  view only they use is `dead` and their calls don't count. The same graph gives each function's distance from the
  boot roots (the game's `main`, `main_thread_entry`, `InitSystem`, `AudioInitSys`, `LoadWave`, `InitWaves`,
  `LoadFrontEndOverlay`), which orders the report.

Classes, worst first. A view takes the worst of its arguments and return:

| Class | Meaning on aarch64 / x86-64 |
|---|---|
| `stub` | the owner is an empty deleted-procedure stub, while this group has the body (section 3.2) |
| `class` | float vs integer/pointer argument: different register files, so this and every later argument of either class move |
| `fwidth` | f32 vs f64 (an unprototyped call promotes float to double) |
| `width` | pointer or 64-bit integer vs an integer of 32 bits or less: the upper half is lost or garbage |
| `stack` | `--abi win64` only: the same in a stack slot (the fifth argument on), where even a constant 0 leaves garbage in the upper half |
| `aggregate` | a struct by value that travels differently (size, eightbyte classes, homogeneous float aggregate) |
| `count` | the caller passes fewer arguments than the definition reads |
| `return` | the return class differs and the caller reads the value |
| `narrow` (benign) | integers of 32 bits or less of different width or signedness: same register |
| `extra` (benign) | the caller passes more arguments than the definition reads |
| `unused` (benign) | the return types differ but no call reads the value |
| `reviewed` (benign) | read by hand and known not to matter (section 4; `REVIEWED` in the tool) |
| `dead` (benign) | only weak copies refer to the function in that file |
| `harmless` | same class and width (`s32` vs `u32`, `T *` vs `void *`), a constant zero-extended into its register, or a view whose file only takes the function's address |

A width mismatch is harmless when every live caller passes a non-negative constant (`NULL` spelled `0`): the
register is zero-extended. That holds in a register, or on the stack when the caller's argument is the wider one.
On Linux an argument is in a register up to the sixth integer one (x86-64; aarch64 has eight). On Windows x64 only
the first four arguments are, whatever their class, so a constant from the fifth on is `stack` there.

## 2. Results

Same tool on `3e3c58a` (before) and on this branch (after), Debug build on aarch64:

| Class | Functions before | Views before | Functions after | Views after |
|---|---:|---:|---:|---:|
| `stub` | 10 | 14 | 1 | 2 |
| `class` | 2 | 5 | 0 | 0 |
| `fwidth` | 0 | 0 | 0 | 0 |
| `width` | 37 | 70 | 8 | 24 |
| `aggregate` | 0 | 0 | 0 | 0 |
| `count` | 3 | 5 | 0 | 0 |
| `return` | 10 | 31 | 3 | 6 |
| **harmful** | **62** | **125** | **12** | **32** |

5,171 views of 1,239 functions are checked; 123 views are unprototyped. Current numbers (`--doc`):

<!-- proto_check:begin -->
| Class | Functions | Views | |
|---|---:|---:|---|
| `stub` | 0 | 0 | harmful |
| `class` | 0 | 0 | harmful |
| `fwidth` | 0 | 0 | harmful |
| `width` | 0 | 0 | harmful |
| `stack` | 0 | 0 | harmful |
| `aggregate` | 0 | 0 | harmful |
| `count` | 0 | 0 | harmful |
| `return` | 0 | 0 | harmful |
| `narrow` | 1 | 2 | benign |
| `extra` | 1 | 1 | benign |
| `unused` | 3 | 3 | benign |
| `reviewed` | 1 | 1 | benign |
| `dead` | 0 | 0 | benign |
| `harmless` | 18 | 30 | benign |
| **harmful** | **0** | **0** | |
<!-- proto_check:end -->

With `--abi win64` (2026-10-27, x86-64 Debug build): `stack` 20 views of 3 functions before section 3.6's fixes, 0
after, and no other harmful view; one more `reviewed` (the stand-in view).

Other checks on this branch: every changed file passes `n64_neutral.py --base 09da2f0` (all "shifted"); ctest 34/34;
census 390 pointer warnings in 92 files (392 in 93 before), the same two warning classes; `rush2049pc --no-rom
--headless --seconds 10` stops where it did, in `LoadWave(6)` → `AllocateMemoryHeap` on the blank cartridge's
zero-size files.

## 3. What was fixed, and how

### 3.1 Techniques (all N64-neutral)

Made while the C also built the N64 ROM (until 2026-11-08, [PCONLY.md](PCONLY.md)): the `#else` branches and the
N64 typedefs are gone since, and the PC side of each is the code.

| Technique | When | Example |
|---|---|---|
| `s32` → `intptr_t`, `u32` → `uintptr_t` | an integer that carries a pointer: parameter, declaration, local, struct field or data symbol. The files' guarded typedefs make it the same type for IDO, so no `#ifdef` | `SaveSelectPlayer(intptr_t arg0)` |
| `#ifdef TARGET_PC` declaration, `#else` the original line | the PC type is a real pointer, a float, or another argument list | `ZOID_InitializeWindow(..., f32 c, ...)` in `ZOID_ResetWindows.c` |
| a type macro in a definition's header | a definition's parameter needs another class on the PC | `CTT_A1` (`void *` / `f32`) in `AudEmitEffect` |
| a function-like call macro after the declarations | the matched call passes registers IDO's IPA chose, not the C argument list | `#define PS_PakServer() PS_PakServer(forceUpdate, rumbleOnly)` |
| `#define name pc_name` before a group's inlined helper | its manifest owner is an empty stub | `pc_ReadShifter` |
| `PC_V0_S32` / `PC_V0_RETURN` | a `void` wrapper whose callers read the `v0` its last call leaves | `InitCarObj` |
| a whole PC version of the function | hand-rolled MIPS varargs | `TB_Printf` (as `TB_Sprintf`) |
| `ptr64_decls.py SYMBOL` | a data symbol that holds a pointer but is declared `s32` in every user | `audio_gTitlePane`, `highscore_blit` |

### 3.2 Empty stubs that took over their group's calls (`stub`)

When umerge inlined a helper into its group, retail keeps a `jr ra` stub at the helper's address, and the decomp's
standalone file for it is `void f(void) {}`. That file is the manifest owner, so on the PC every call from the
group binds to the empty stub and the work is lost. The bodies the groups write get their own names:

- `UpdateModelControls`: manual gear selection `ReadShifter`.
- `skid_marks`: `GetSkid`/`ContinueSkid` (`GetSkid`, `ContinueSkid`).
- `CheckForStunt`: three stunt trackers `Check2WheelSide/2420/2428`.
- `entity_helpers`: `ValidateEmitter`.
- `AudioShutdown`: the locked heap free `DeallocateMemory_stub` maps to `DeallocateMemory`, the same body under a strong
  name. `carcolor` and `SetupCarColor/ctx_initsystem`: the heap semaphore helpers `heap_lock/CFC` are written in place
  (`osRecvMesg`/`osSendMesg` on `heap_lock_queue`).

### 3.3 Float arguments (`class`)

- `AudEmitEffect`'s second parameter is never read, and every caller passes a pointer there. Under o32 that
  sits in a GPR whatever the type, but on the PC an `f32` takes `s0` and moves `volume`, `x` and `y` along by one
  register. Its PC view is `void *`, and the callers' views and `EmitGameSound`'s pass-through take
  `intptr_t` (this is Agent D's finding).
- `ZOID_ResetWindows` passed the field of view `HFOV` (90.0f, declared `s32` in 412 files) as `ZOID_InitializeWindow`'s
  first `f32`: front-end window setup. Its PC declarations are the `f32` prototype and an `f32` `HFOV`.

### 3.4 Argument lists shaped by IDO's IPA

IDO's interprocedural pass moves the arguments of functions it sees whole. The matched callers then pass dummies,
or nothing, so that values land in the registers the callee reads; natively every call uses the standard ABI.

- `zlib_deflate`: `DeallocateMemoryInternal(address, tag)` reads `a1`/`a2`, so the decomp calls it `(heap, ptr, 0)`.
- `runtime_a_profile_menu`, `runtime_a_availability_refresh`: `playerBeingDeleted/F44` take `skip` in `a1` after a dummy
  (`move a1,s2` at the call); the group calls pass `skip` alone.
- `SaveInit`: `SaveServer` calls `PS_PakServer` before it touches `a0`/`a1`, so it forwards its
  own `forceUpdate`, `rumbleOnly`; the decomp's call passes nothing.

The `extra` class was read for the same pattern (a dummy first argument of the same type). All 12 views there pass
the real value first or an ignored extra (`sound_seq_stop(id, id)`, `osGetThreadId(0, pages)`).

### 3.5 Pointers through 32-bit parameters, results and storage (`width`, `return`)

- Definitions typed `s32` where every caller passes a pointer, now `intptr_t`: `SaveSelectPlayer` (forwarding to
  `GetPlayerOption`, `SavePlayerOption`), `InitCarVisuals`, `sample_dma_thread_entry`, `MBOX_NewObjectCam`,
  `WalkBSTInternal`, `PS_ErrorFunc` (two strings to `ShowPakError`), `steady_move_cam`, `fix_camera_in_space`,
  `UpdateCarObj` and the `point_at_car` views they call, `GetCircuitCode` → `GetPasswordString`.
- `HAL_InstallOncePerFrameFunc` stores function pointers in its callback table `gOPPFuncs`, which `HAL_CallOncePerFrameFuncs` walks up
  to `&yf`, the next N64 variable. The table is MAT3's `void (*[10])(void)` (an `intptr_t[10]` until
  [NATIVE.md](NATIVE.md) step 11), and in those two files its end is `gOPPFuncs + 10` on the PC. `yf` itself stays a
  variable for its other users.
- InitPane stores a `PaneAnimation` from its sixth argument into the blit. Its struct view
  (`PadConfig`) gets the blit's two pointer-sized fields (`Info`, `AnimFunc`), so position, size and the animation
  function land where `Blit` has them. The title panes it returns (`audio_gTitlePane`, `options_gTitlePane`) are `intptr_t` in
  all their users, and so are `ChangePaneSize`'s and its own views in `title_panes.c`.
- Blits from `NewMultiBlit` kept in `s32`: `highscore_blit`, `backBlit`, `PS_ErrorFunc`' `snd`,
  `RemoveMultiplayerHUD`'s `handle`; `RemoveBlit`'s parameter is `intptr_t` in the views that pass them.
- MBOX wrappers: `InitCarObj`, `MBOX_NewObjectCam` and `MBOX_NewObject` end in a call to `ZOID_AddObject`
  (MBOX_NewObject), whose result stays in `v0` through their `void` return, and every caller reads it. On the PC
  they return it.
- Results read as `s32` and then used as pointers: `AllocateHandleHeap` in `DecodeGhost` (sign-extended before
  `(void **)`), `ZOID_NewMatrix` in `SkyInit` (a game BSS address), `GrabEnvEntry`, `ShotToWorldCollision`.
  `MBGetFromWad` in `LoadTrack` returns a WAD entry's 32-bit window address, read as `u32`.
- Arguments narrowed by a view: `memcpy` in `ResetCollisionObject`, `sound_push_group` in LoadWave's
  `AudioInitSys`, `fixword`, `CompressGhost`, the voice `handle` in `AnimateSkids`, `blast`'s view of
  `EmitGameSound`. `memset` in `AllocateHandleHeap` was unprototyped; its address is an
  `enum n64p_u32` window address, so the PC parameter is `uintptr_t`.
- `TB_Printf` (printf at x, y) gets the host `vsprintf`, as `TB_Sprintf` did: the N64 version walks its
  stack arguments, and its format is a pointer.

### 3.6 Stack slots on Windows x64 (`stack`)

The Microsoft x64 convention passes the first four arguments in registers, whatever their class, and every later
one in an 8-byte stack slot. A caller whose prototype says `s32` stores 4 bytes there (`movl $0, 0x20(%rsp)`), so a
definition that reads a pointer gets the upper half of whatever the slot held before. In a register, as on Linux up
to the sixth integer argument, a 32-bit move zero-extends and a constant 0 arrives as NULL. A `char` or `short`
argument is extended to 32 bits and stored as 4 bytes, so `narrow` stays benign. `--abi win64` applies these rules
and reports the class `stack`. `REVIEWED`'s reasons (32 bits hold the value) don't cover it; `REVIEWED_STACK` holds
the reviewed ones.

The first run found 20 views of three functions, all passing a constant 0:

| Function | Argument | Views | Fix |
|---|---|---|---|
| `LoadWave` | 5: the heap, `void *` | `AudioInitSys.c`, `loadtrack.c`, `LoadFrontEndWaves.c` (`LockAndLoad`), image A's `cleanup_colortrace` and `runtime_a_c93c_closure` | pointer-sized on the PC |
| `InitPane` | 6: the pane's animation, `intptr_t` (section 3.5) | 14 files, one under the name `InitPane` | `intptr_t` on the PC |
| `CylinderInSurf` | 6: `f32 *` | the weak copy in `stree_tiresurf/group.c` | reviewed: its only calls are the matching stand-ins `__standin_input_process_controller_a/b`, which nothing calls |

`LockAndLoad`'s `LoadWave(id, 0, 0, 0, 0)` is what every synthetic boot reached first on Windows: LoadWave
got the heap 0x00007FF6_00000000 and faulted reading its first block ([WINDOWS.md](WINDOWS.md) 4). The views use
`intptr_t` where the file has its N64 typedef and an `#ifdef TARGET_PC` declaration elsewhere; `n64_neutral.py`
passes them. Now `--abi win64` finds no harmful view either. Calls through function pointers are not checked
(section 6).

## 4. Reviewed: 32 bits that carry the value exactly

These are `width` or `return` by type, but the value is one 32 bits hold. They are listed in the tool's `REVIEWED`
table and reported as `reviewed`:

| Functions | Why |
|---|---|
| `PurgeableMemory`, `FindChunk`, `FindHeap`, `DeallocateMemoryInternal`, `DeallocateMemory`, `ReallocateMemory`, `GetPtrSize`; `AllocateMemoryHeap`, `AllocateMemoryHeapHigh` in `initgfx.c` (`CalcChecksumInternal`, `unzipdata` in `DecodeGhost` until [NATIVE.md](NATIVE.md) step 11, which gives them pointers) | heap block addresses are RDRAM-window addresses below 4 GB (POINTERS.md 4.1) |
| `inflate_decompress`, `lzss_decompress` | the source is a ROM offset from the file table |
| `TB_DrawText` in `titlescr_animText.c` (and `relocate_texture_loads` in `waves.c` until [NATIVE.md](NATIVE.md) step 10, which removed it) | text-bank records hold 32-bit window addresses (4.2) |
| (`displaylist_set_texture_image`, `displaylist_get_texture_image`: a word of a display list in N64 form, until [NATIVE.md](NATIVE.md) step 10: they take `Gfx *` and a pointer since) | |
| `RemovePlayerScores`, `FindChangeCmd` | the pointer is compared with 32-bit slots that hold the same truncation (4.4) |
| `MBOX_FindObject_WORK`, `MBOX_NewObject` in `runtime_a_availability_refresh` | a model index kept in a `void *` variable |
| `sound_hw_remove_sample` | empty in retail: the arguments are never read |
| `AddStunt` | old-style definition with a third parameter no caller passes, written before it is read |

## 5. Data declarations (DATA.md's conflict list)

`build/pc/pc-data/data_layout.txt` lists 107 symbols whose files disagree on where pointers are (397 pairs) and 85 on
element widths (190 pairs). Only two were still an integer table or scalar against a pointer:

- `CollisionTargetPtrs` (`Camera *[30]`): `InitCollisionTargetPtrs.c`'s tentative definition `s32[30]` cleared 120 of its 240 bytes on
  the PC (the link's "size changed" warning). Now `intptr_t[30]`.
- `gLights` (`Lights2 *` vs `s32`): `nonmatching/game/PrePlay.c`, not mine. That file should declare it
  `intptr_t`, or the pointer type.

Symbols that hold pointers but were `s32` in every user never appear in that list, because the views agree. gen_data
then gives them 4 bytes in an arena. The ones these fixes reach are now pointer-sized, checked in
`data_symbols.txt`: `audio_gTitlePane`, `options_gTitlePane`, `highscore_blit` (own, 8 B), `gOPPFuncs` (own, 80 B), and
`FireTexNames`, the six model names, which was `int[6]` and is now `char *[6]`, so the loader turns its N64 string
pointers into host pointers. `HFOV` is `f32` in the one file that passes it as a float. `backBlit` and
`gGhostToSave` were already pointer storage; their `s32` users are `intptr_t` now.

The other pointer conflicts are struct views that disagree: `tCarData` against `Car`/`Player`/`GameCar`
and others (92 pairs), `tModelData` against `Model`/`Car` (62; both now one layout, [CARS.md](CARS.md)), `ObjInstance`
against `Ent`/`Model68`/`Node` (32),
the `OSMesgQueue` views (35). Making
them agree is struct unification, which is beyond prototypes. That includes the blit (`Blit` in 14 files: its
`AnimDTA` holds the pane group pointer but is `s32` in several views) and the pool headers (section 6).

## 6. What is left

Harmful views in files I may not edit:

| File | Function | What to do |
|---|---|---|
| `groups/heap_release/UnloadWaveInstance.c`, `audio_sfx_shutdown.c` | `DeallocateMemory_stub` (stub) | `#define DeallocateMemory_stub DeallocateMemory` under `TARGET_PC`, as `AudioShutdown.c` does |
| `src/rom/sound/hardware.c` | `bzero(void *, int)` | sizes in variables reach the host's `size_t`: declare it with `__SIZE_TYPE__` on the PC |
| `src/rom/sound/hardware.c`, `dma_queue.c` | `dcache_create_dirty_lines` | **`port/ultra/misc.c` defines it `void`**, but it returns `start` advanced in 16-byte cache lines to at least `end` (`asm/us/boot_data/func_8002C5E0.s`), and `sound_dma_flush_requests` keeps that as its audio DMA cursor: `uint32_t dcache_create_dirty_lines(uint32_t s, uint32_t e) { do s += 16; while (s < e); return s; }` |
| `src/ovl_a/groups/trackscr/trackscr.c` | `LoadWave`, `AllocateMemoryHeap`, `DeallocateMemory`, `MBOX_NewObject` | harmless now (constant arguments, window addresses, the wrapper returns `s32`); no change needed |
| `nonmatching/game/PrePlay.c` | `gLights` | section 5 |

Harmful views that needed a typed PC version or struct unification, not a prototype change, are done
(POINTERS.md 9): `ZOID_SetTexture`, `ZOID_SetObjLights`, `MBOX_FindPalette_WORK` and `MBOX_FindTexture_WORK` (the scene nodes
`ObjInstanceList`), `RemoveBlit` in `RemovePane`, `RemovePanes` and `DrawBlits` (panes and blits),
`InitStatLL` (the StatLL lists) and `PS_WriteFile` in `ClearPlayerRecords`. The two that were left in race
code are done too: `enginetorque` in `engine` (the torque curve, [CARS.md](CARS.md) 4) and
`RotateUV` in `AnimateKnockdown` (a target's state, a window address, passed zero-extended: POINTERS.md 10.2).
No harmful view is left.

Limits of the check: calls through function pointers aren't seen. `narrow` is benign for the ABI, but AAPCS64
leaves the upper bits of narrow arguments and returns unspecified, while IDO trusted the caller's extension. Behavior
that only works at -O0 (a `void` function leaving `x0` untouched, a 32-bit load zeroing the upper half) passes the
check and may break in a Release build: the fixes above remove those cases rather than rely on them.

## 7. One declaration each (wave 2c, 2026-11-09)

Since the headers ([PCONLY.md](PCONLY.md) 4, "Wave 2: headers") every game function has one prototype in
`include/game`, the definition's, and a call goes through it. The headers had left 72 to their files' own
declarations; they are in the headers now, and no file under `src/blob`, `src/ovl_a`, `src/ovl_b` or `nonmatching`
declares a game function but the C library's the build renames `n64_*` (`memcpy`, `memset`, `sinf`, `cosf`,
`strlen`). A structure that only the defining file completes is named in the prototype by its tag, declared in
`types.h` (`struct tPoly`, `tWindowInfo`, `ObjInstance`, `TexTile`, `N64Gfx`, `tAudioSequence`, `union tAudioMsg`
...): each file's own definition completes it, as the headers' parameters already did. The heap's `FindChunk` and
`ConvertHeap` moved from `heap.h` to `memory.h` for the heap files with views of their own.

How each disagreement was settled (from the code, the N64's instructions in the repository's history and MAT3):

| Kind | Functions | Verdict |
|---|---|---|
| a pointer kept in a 64-bit integer (`intptr_t`) by the callers, or the definition | `RemoveBlit`, `ChangePaneSize`, `ZOID_SetTexture`, `ZOID_UpdatePoly`, `ZOID_RemovePoly` (it took the polygon as `s16 *`), `ZOID_AddObject`, `PS_WriteFile`, `point_at_car`, `CompressGhost`, `GetPasswordString`, `fixword`, `SavePlayerOption`, `GetPlayerOption` (through `SaveSelectPlayer`), `enginetorque`, `AudUpdateEmitter`, `FindChangeCmd`; and, already in the headers, `SaveSelectPlayer`, `censor`, `GetCircuitCode`, `MBOX_NewObject`, `MBOX_NewObjectCam`, `ZOID_SetObjLights`, `InitCarVisuals`, `fix_camera_in_space`, `steady_move_cam`, `UpdateCarObj`, `EmitGameSound` | harmless (64 bits each way); the pointer type now |
| a pointer cut to 32 bits by the caller | `AudEmitEffect`'s `dir`: `(s32) origin`, `(s32) &unitmatrix_origin`, `(int) unitmatrix[3]` | harmless: never read; the pointer now |
| a cartridge offset the definition typed `void *` | `lzss_decompress`, `inflate_decompress` | harmless (positive offsets); `u32` now |
| more arguments than the definition reads | `dotireforce` (13: the N64 reads no 14th), `setFBCollisionForce` (5), `MBOX_FindPalette_WORK` (4), `AudioSongStop` (none), `CountdownTimeOut` (none), `sound_set_volume` (2), `sound_key_off` (1), `sound_seq_stop` (1) | harmless: the extras go |
| fewer arguments than the definition reads | `ShowPakError`'s `poll()`: MAT3 and the N64 pass the pak | harmless on the PC (`%edi` still held it); `poll(row)` now |
| a file's own view or its own forward declaration only | `AddStunt`, `relocate_texture_loads`, `musyx_submit_audio_task`, `zoid_load_palette`, `zoid_load_texture`, `zoid_draw_object`, `Traverse`, `SetupTarget` (its `fromWorld` flag is `s32`, MAT3's), `TB_FlashTextColors` (a `tRGBA`, then called `TextColor`, by value; options_anim's copy `TB_FlashTextColors_AEAC` is gone) | agree now |

`proto_check.py` (section 1, its table in section 2) went from 156 benign views of 69 functions (26
unprototyped) to 37 of 24 (5 unprototyped), all of them now the PC layer's, libultra's or the C library's. None
changed what the PC does. Four port bugs found beside them are fixed toward the original game in their own
commits: `animResults`' fifth result pane, the trophy sheen's scale, MusyX SET_PITCH at an exact octave, and
Windows builds without SDL2 ([WINDOWS.md](WINDOWS.md) 3).

Checked by compiling every game unit again with the warnings the build turns off (`-fsyntax-only`
`-Wimplicit-function-declaration -Wint-conversion -Wincompatible-pointer-types -Wbuiltin-declaration-mismatch`):
implicit declarations 8 before, 0 after (the last were the boot segment's: `src/rom/rom_tu.h` declares
`osPiReadIo`, `__osSiCreateAccessQueue`, pfschecker.c's page walks, `bcmp` and `memcpy`); built-in mismatches 1, 0
(PrintScores.c's `sprintf`); a pointer passed for an integer or back at a call (`-Wint-conversion`, "passing
argument") 57, 11. The 11 carry a pointer in 64 bits or a window address in 32 (section 4's reviewed kinds:
`blit_load_texture` 4, `displaylist_set_texture_image`, `display_list_traverse`, `sound_start_song`,
`HAL_InstallOncePerFrameFunc`, `RemovePlayerScores`, `world_name_compare`'s two `game_strcmp`), left for the memory
model's wave. `-Wincompatible-pointer-types` grew (905 to 1,177): calls that went through a file's own declaration now meet
the definition's types, and a file's view of a shared record (`Blit` for `tBlit`, `Color4` ...) is another struct
type to the compiler, as it was when the file passed it to its own declaration. Same layout, same code.
