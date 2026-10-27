# Prototypes the game's files disagree on

Every game C file carries its own declarations of what it calls, and many disagree with the definition. MIPS o32
forgave most of that: an `int` and a pointer share a register, and every argument after a leading integer or
pointer travels in a GPR whatever its type. On aarch64 and x86-64 SysV the same disagreements are wrong arguments.
This page covers the tool that finds them, what was fixed and how, and what is left. 2026-10-27. Related:
[POINTERS.md](POINTERS.md) (the N64-neutral conventions used here), [DATA.md](DATA.md) (data declarations).

## 1. The tool

`python3 tools/port/proto_check.py [--build build/pc] [--jobs 3] [--doc docs/port/PROTOTYPES.md]` (stdlib, binutils
and the build's compiler; about 8 s on the Pi 5 with 3 jobs). It writes `build/pc/proto_report.txt` (every
disagreement, nearest the boot first) and `build/pc/proto_report.json`, and with `--doc` the table in section 2.

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
  boot roots (the game's `main`, `game_init`, `game_late_init`, `AudioInitSys`, `LoadWave`, `InitWaves`,
  `LoadFrontEndOverlay`), which orders the report.

Classes, worst first. A view takes the worst of its arguments and return:

| Class | Meaning on aarch64 / x86-64 |
|---|---|
| `stub` | the owner is an empty deleted-procedure stub, while this group has the body (section 3.2) |
| `class` | float vs integer/pointer argument: different register files, so this and every later argument of either class move |
| `fwidth` | f32 vs f64 (an unprototyped call promotes float to double) |
| `width` | pointer or 64-bit integer vs an integer of 32 bits or less: the upper half is lost or garbage |
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
| `width` | 2 | 2 | harmful |
| `aggregate` | 0 | 0 | harmful |
| `count` | 0 | 0 | harmful |
| `return` | 0 | 0 | harmful |
| `narrow` | 82 | 164 | benign |
| `extra` | 8 | 12 | benign |
| `unused` | 45 | 76 | benign |
| `reviewed` | 17 | 24 | benign |
| `dead` | 42 | 166 | benign |
| `harmless` | 55 | 251 | benign |
| **harmful** | **2** | **2** | |
<!-- proto_check:end -->

Other checks on this branch: every changed file passes `n64_neutral.py --base 09da2f0` (all "shifted"); ctest 34/34;
census 390 pointer warnings in 92 files (392 in 93 before), the same two warning classes; `rush2049pc --no-rom
--headless --seconds 10` stops where it did, in `LoadWave(6)` → `AllocateMemoryHeap` on the blank cartridge's
zero-size files.

## 3. What was fixed, and how

### 3.1 Techniques (all N64-neutral)

| Technique | When | Example |
|---|---|---|
| `s32` → `intptr_t`, `u32` → `uintptr_t` | an integer that carries a pointer: parameter, declaration, local, struct field or data symbol. The files' guarded typedefs make it the same type for IDO, so no `#ifdef` | `func_800B4DA4(intptr_t arg0)` |
| `#ifdef TARGET_PC` declaration, `#else` the original line | the PC type is a real pointer, a float, or another argument list | `arb_rate_set(..., f32 c, ...)` in `ZOID_ResetWindows.c` |
| a type macro in a definition's header | a definition's parameter needs another class on the PC | `CTT_A1` (`void *` / `f32`) in `camera_target_track` |
| a function-like call macro after the declarations | the matched call passes registers IDO's IPA chose, not the C argument list | `#define track_render_process() track_render_process(forceUpdate, rumbleOnly)` |
| `#define name pc_name` before a group's inlined helper | its manifest owner is an empty stub | `pc_func_800E6AE8` |
| `PC_V0_S32` / `PC_V0_RETURN` | a `void` wrapper whose callers read the `v0` its last call leaves | `save_slot_valid` |
| a whole PC version of the function | hand-rolled MIPS varargs | `music_tempo_adjust` (as `fcvt_wrapper`) |
| `ptr64_decls.py SYMBOL` | a data symbol that holds a pointer but is declared `s32` in every user | `D_80110648`, `D_801148CC` |

### 3.2 Empty stubs that took over their group's calls (`stub`)

When umerge inlined a helper into its group, retail keeps a `jr ra` stub at the helper's address, and the decomp's
standalone file for it is `void f(void) {}`. That file is the manifest owner, so on the PC every call from the
group binds to the empty stub and the work is lost. The bodies the groups write get their own names:

- `func_800E681C`: manual gear selection `func_800E6AE8`.
- `skid_marks`: `GetSkid`/`ContinueSkid` (`func_800AFB30`, `func_800AFD54`).
- `camera_scene_manager`: three stunt trackers `func_800C2418/2420/2428`.
- `entity_helpers`: `func_800BF01C`.
- `func_800C8918`: the locked heap free `func_800960CC` maps to `audio_effect_process`, the same body under a strong
  name. `carcolor` and `carcolor/ctx_initsystem`: the heap semaphore helpers `func_80095CF4/CFC` are written in place
  (`osRecvMesg`/`osSendMesg` on `D_80152770`).

### 3.3 Float arguments (`class`)

- `camera_target_track`'s second parameter is never read, and every caller passes a pointer there. Under o32 that
  sits in a GPR whatever the type, but on the PC an `f32` takes `s0` and moves `volume`, `x` and `y` along by one
  register. Its PC view is `void *`, and the callers' views and `camera_collision_check`'s pass-through take
  `intptr_t` (this is Agent D's finding).
- `ZOID_ResetWindows` passed the field of view `D_80154188` (90.0f, declared `s32` in 412 files) as `arb_rate_set`'s
  first `f32`: front-end window setup. Its PC declarations are the `f32` prototype and an `f32` `D_80154188`.

### 3.4 Argument lists shaped by IDO's IPA

IDO's interprocedural pass moves the arguments of functions it sees whole. The matched callers then pass dummies,
or nothing, so that values land in the registers the callee reads; natively every call uses the standard ABI.

- `zlib_deflate`: `audio_reverb_update(address, tag)` reads `a1`/`a2`, so the decomp calls it `(heap, ptr, 0)`.
- `runtime_a_profile_menu`, `runtime_a_availability_refresh`: `func_80396EE8/F44` take `skip` in `a1` after a dummy
  (`move a1,s2` at the call); the group calls pass `skip` alone.
- `menu_save_options`: `SaveServer` calls `track_render_process` before it touches `a0`/`a1`, so it forwards its
  own `forceUpdate`, `rumbleOnly`; the decomp's call passes nothing.

The `extra` class was read for the same pattern (a dummy first argument of the same type). All 12 views there pass
the real value first or an ignored extra (`func_80018E2C(id, id)`, `osGetThreadId(0, pages)`).

### 3.5 Pointers through 32-bit parameters, results and storage (`width`, `return`)

- Definitions typed `s32` where every caller passes a pointer, now `intptr_t`: `func_800B4DA4` (forwarding to
  `reverb_setup`, `SavePlayerOption`), `func_800B27E4`, `task_complete_signal`, `engine_rpm_calc`,
  `camera_update_a`, `save_settings` (two strings to `control_settings`), `func_800E95DC`, `func_800E9C70`,
  `func_800EA2DC` and the `func_800E8D50` views they call, `func_800F42C8` → `tournament_trophy_award`.
- `func_800B7438` stores function pointers in its callback table `D_801551E8`, which `input_init_flag_get` walks up
  to `&D_80155210`, the next N64 variable. The table is `intptr_t[10]`, and in those two files its end is
  `&D_801551E8[10]` on the PC. `D_80155210` itself stays a variable for its other users.
- InitPane (`ambient_sound_set`) stores a `PaneAnimation` from its sixth argument into the blit. Its struct view
  (`PadConfig`) gets the blit's two pointer-sized fields (`Info`, `AnimFunc`), so position, size and the animation
  function land where `Blit` has them. The title panes it returns (`D_80110648`, `D_80116DA4`) are `intptr_t` in
  all their users, and so are `crowd_cheer_play`'s and its own views in `gr3_c.c`.
- Blits from `sound_control` (NewMultiBlit) kept in `s32`: `D_801148CC`, `D_80114624`, `save_settings`' `snd`,
  `player_state_clear`'s `handle`; `sound_stop`'s parameter is `intptr_t` in the views that pass them.
- MBOX wrappers: `save_slot_valid`, `engine_rpm_calc` and `sign_extend_call` end in a call to `func_8008E26C`
  (MBOX_NewObject), whose result stays in `v0` through their `void` return, and every caller reads it. On the PC
  they return it.
- Results read as `s32` and then used as pointers: `AllocateHandleHeap` in `DecodeGhost` (sign-extended before
  `(void **)`), `func_800A7D6C` in `camera_reset` (a game BSS address), `func_80090284`, `input_deadzone_apply`.
  `MBGetFromWad` in `LoadTrack` returns a WAD entry's 32-bit window address, read as `u32`.
- Arguments narrowed by a view: `memcpy` in `listener_position_set`, `func_8001536C` in LoadWave's
  `car_shadow_render`, `func_800F0674`, `CompressGhost`, the voice `handle` in `audio_voice_set`, `blast`'s view of
  `camera_collision_check`. `memset` in `AllocateHandleHeap` was unprototyped; its address is an
  `enum n64p_u32` window address, so the PC parameter is `uintptr_t`.
- `music_tempo_adjust` (printf at x, y) gets the host `vsprintf`, as `fcvt_wrapper` did: the N64 version walks its
  stack arguments, and its format is a pointer.

## 4. Reviewed: 32 bits that carry the value exactly

These are `width` or `return` by type, but the value is one 32 bits hold. They are listed in the tool's `REVIEWED`
table and reported as `reviewed`:

| Functions | Why |
|---|---|
| `PurgeableMemory`, `func_80095EF4`, `func_80095F8C`, `audio_reverb_update`, `audio_effect_process`, `menu_item_select`, `audio_buffer_sync`; `AllocateMemoryHeap`, `sound_play_menu` in `initgfx.c`; `format_string_parse`, `inflate_entry_alt` in `DecodeGhost` | heap block addresses are RDRAM-window addresses below 4 GB (POINTERS.md 4.1) |
| `inflate_decompress`, `lzss_decompress` | the source is a ROM offset from the file table |
| `entity_cull_check` in `waves.c`; `state_utility` in `func_803AE51C.c` | WAD and text-bank records hold 32-bit window addresses (4.2) |
| `func_800BD080`, `func_800BDA24` | a word of a display list in N64 form (texture animation, POINTERS.md 2) |
| `audio_channel_priority`, `func_800956BC` | the pointer is compared with 32-bit slots that hold the same truncation (4.4) |
| `string_copy_format`, `sign_extend_call` in `runtime_a_availability_refresh` | a model index kept in a `void *` variable |
| `func_80014D08` | empty in retail: the arguments are never read |
| `func_800C1B60` | old-style definition with a third parameter no caller passes, written before it is read |

## 5. Data declarations (DATA.md's conflict list)

`build/pc/pc-data/data_layout.txt` lists 107 symbols whose files disagree on where pointers are (397 pairs) and 85 on
element widths (190 pairs). Only two were still an integer table or scalar against a pointer:

- `D_8013C300` (`Camera *[30]`): `func_800BB7F4.c`'s tentative definition `s32[30]` cleared 120 of its 240 bytes on
  the PC (the link's "size changed" warning). Now `intptr_t[30]`.
- `D_801406B8` (`Lights2 *` vs `s32`): `nonmatching/game/display_list_flush.c`, not mine. That file should declare it
  `intptr_t`, or the pointer type.

Symbols that hold pointers but were `s32` in every user never appear in that list, because the views agree. gen_data
then gives them 4 bytes in an arena. The ones these fixes reach are now pointer-sized, checked in
`data_symbols.txt`: `D_80110648`, `D_80116DA4`, `D_801148CC` (own, 8 B), `D_801551E8` (own, 80 B), and
`D_80117480`, the six model names, which was `int[6]` and is now `char *[6]`, so the loader turns its N64 string
pointers into host pointers. `D_80154188` is `f32` in the one file that passes it as a float. `D_80114624` and
`D_80140800` were already pointer storage; their `s32` users are `intptr_t` now.

The other pointer conflicts are struct views that disagree: `tCarData` against `Car`/`Player`/`GameCar`
and others (92 pairs), `tModelData` against `Model`/`Car` (62), `ObjInstance` against `Ent`/`Model68`/`Node` (32),
the `OSMesgQueue` views (35). Making
them agree is struct unification, which is beyond prototypes. That includes the blit (`Blit` in 14 files: its
`AnimDTA` holds the pane group pointer but is `s32` in several views) and the pool headers (section 6).

## 6. What is left

Harmful views in files I may not edit:

| File | Function | What to do |
|---|---|---|
| `groups/heap_release/func_80096130.c`, `wheel_params_set.c` | `func_800960CC` (stub) | `#define func_800960CC audio_effect_process` under `TARGET_PC`, as `func_800C8918.c` does |
| `src/rom/lib_11640.c` | `bzero(void *, int)` | sizes in variables reach the host's `size_t`: declare it with `__SIZE_TYPE__` on the PC |
| `src/rom/lib_11640.c`, `lib_11050.c` | `func_8002C5E0` | **`port/ultra/misc.c` defines it `void`**, but it returns `start` advanced in 16-byte cache lines to at least `end` (`asm/us/boot_data/func_8002C5E0.s`), and `func_80010450` keeps that as its audio DMA cursor: `uint32_t func_8002C5E0(uint32_t s, uint32_t e) { do s += 16; while (s < e); return s; }` |
| `src/ovl_a/groups/runtime_a_c93c_closure/group.c` | `audio_frame_sync`, `audio_dma_sync`, `func_800960D4`, `sign_extend_call` | harmless now (constant arguments, window addresses, the wrapper returns `s32`); no change needed |
| `nonmatching/game/display_list_flush.c` | `D_801406B8` | section 5 |

Harmful views that needed a typed PC version or struct unification, not a prototype change, are done
(POINTERS.md 9): `func_8008D870`, `func_800A79B8`, `MBOX_FindPalette_WORK` and `func_800B24EC` (the scene nodes
`D_8012E700`), `sound_stop` in `RemovePane`, `sound_handles_clear` and `UpdateActiveObjects` (panes and blits),
`struct_fields_init` (the StatLL lists) and `PS_WriteFile` in `menu_confirm_dialog`. Left, in race code:
`func_800E2D18` in `func_800E2F00` (`EngineState.f40C`) and `sound_position_set` in `func_8010E828` (`+0x6C`).

Limits of the check: calls through function pointers aren't seen. `narrow` is benign for the ABI, but AAPCS64
leaves the upper bits of narrow arguments and returns unspecified, while IDO trusted the caller's extension. Behavior
that only works at -O0 (a `void` function leaving `x0` untouched, a 32-bit load zeroing the upper half) passes the
check and may break in a Release build: the fixes above remove those cases rather than rely on them.
