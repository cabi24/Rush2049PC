# 64-bit pointers in the game C

How the game C (`src/blob`, `src/ovl_a`, `src/ovl_b`, `include`) was made 64-bit clean for the PC build without
changing what the N64 build compiles, what is left, and why. 2026-10-26. Related: [ASSETS.md](ASSETS.md) (the 32-bit
asset world), [DATA.md](DATA.md) (where data symbols live on PC), [GFX.md](GFX.md) (the renderer's side).

## 1. Mechanism

- **Prelude.** `port/include/pc_prelude.h` is force-included first into every game file on `TARGET_PC`
  (`CMakeLists.txt`, `game_flags`). It defines `uintptr_t`/`intptr_t` from `__UINTPTR_TYPE__`/`__INTPTR_TYPE__` (no
  system headers), big-endian accessors `pc_be32_get/set`, `pc_be16_get/set` for N64-form ROM data, and declares
  `pc_n64_ptr`/`pc_n64_addr` (port/ultra).
- **N64 side.** A game file that uses `uintptr_t`/`intptr_t` declares them itself, as `u32`/`s32`, under
  `#ifndef TARGET_PC`. The 427 m2c-context files that already had `typedef u32 uintptr_t; typedef s32 intptr_t;`
  got the guard; others got the guarded block after their integer typedefs. So `(u32)p` -> `(uintptr_t)p`,
  `s32 x` -> `intptr_t x` are the same types for IDO and pointer-sized on PC.
- **Other PC-only differences** are `#ifdef TARGET_PC ... #else <original> #endif` at file scope, or file-scope
  macros whose N64 expansion is the original token sequence (`DLW(w)` -> `w`, `TEXT_DL` -> `D_80124FE8`,
  `N64_LONG` -> `long`, `N64_HI_80150000` -> `0x80150000`). No line is added or removed inside a function body.
- **Scripts** (all idempotent, all checked by `n64_neutral.py`):
  - `tools/port/ptr64_edit.py guard | need FILE... | gfx | gbi | casts [FILE...]`: the typedef guards and blocks,
    the Gfx widening, the GBI address macros, and the cast rewrite at each `-Wpointer-to-int-cast`.
    `N64_WORLD` there lists the files whose casts are kept (section 4.1).
  - `tools/port/ptr64_decls.py [SYMBOL...]`: integer declarations of data symbols that other files declare as
    pointers become `intptr_t`/`uintptr_t` (from gen_data's DWARF survey, `build/pc/pc-data/data_layout.txt`).
  - The rest (ROM-list walkers, buffers, prototypes, `long`) are small hand edits, one commit each.

### The neutrality check

`tools/port/n64_neutral.py [--base REV] [FILE...]` (default: every changed `src/`/`include/` file vs `HEAD`)
preprocesses each file as the N64 build does (no `TARGET_PC`; `-Iinclude -Iinclude/PR -D_LANGUAGE_C` for `src/rom`,
nothing for game files, which IDO compiles with no `-I`/`-D`; IDO-like predefines) with `gcc -E -undef -nostdinc`,
at the base revision and in the working tree, then:

1. compares token streams after spelling-normalizing basic types (`unsigned int` == `unsigned`) and resolving
   simple file-scope typedefs from their declaration on (`uintptr_t` == `u32` where the file says so; `long` !=
   `s32`); a typedef name may not be redeclared or change type;
2. compares lines: **exact**, **code-exact** (only declarations outside functions moved), or **shifted** (each
   function body moved by one constant; same-line and order relations kept). Anything else fails. `as1` breaks
   scheduling ties by source line (COMPILER_SETTINGS.md), which depends on the order and sameness of lines, never
   on their absolute values; header comments of matched files change length all the time;
3. text rules: an added `#if`/`#ifdef`/`#ifndef`/`#elif` must test `TARGET_PC`; no added `//` comments in game files
   (IDO compiles them without `-Xcpluscomm`).

A changed header is checked through every `src/rom` file. Result over this work (`--base d28fbb3`, the game files
only): see the commit messages; every changed file passed, none needed a typedef other than `uintptr_t`/`intptr_t`.
Still to do on the x86 host: `make` and the SHA-1 check, which nothing here can replace.

## 2. Gfx

- Every local `Gfx`/`Gwords` typedef (444 files) carries `w1` as `uintptr_t`: on PC a command is
  `{u32 w0; uintptr_t w1;}`, 16 bytes, the `PcGfxWords` layout. One-line forms (`u32 w0, w1;`) got an
  `#ifdef TARGET_PC` copy.
- Address-carrying GBI macros (`gSetImage`, `gDma1p`, `gDma2p`, `gSPVertex`, `gMoveWd` for `gSPSegment`,
  `gDma1p_`) store `w1` through `(uintptr_t)`. Value macros (other modes, combine, colors, geometry mode) keep
  their 32-bit casts, so the upper half of `w1` is zero for them. The CULLDL branch patch
  (`particle_knot/group.c` and `resurrect/particle_context.c`, `D_80161430[-1].words.w1`) stores the full pointer.
- **Contract for the renderer (agent C):** in a native list, `w1` of VTX, DL, BRANCH (E1 before 04/DD), MTX,
  MOVEMEM, MOVEWORD SEGMENT and SETxIMG holds what the game stored: a host pointer, or a 32-bit N64 address the
  game computed (K0 `0x80xxxxxx`, physical from `osVirtualToPhysical`, segmented). Heap and frame buffers are in the
  RDRAM window, so masked addresses of them are fine. For every other command use `(u32)w1`.
- Code that assumed 8-byte commands:
  - the two frame command buffers (`D_800586D0`/`D_8006A090`: 0x9CC0 bytes of matrices and vertices, then 4000
    commands) are `pc_frame_dl[2][0x9CC0 + 4000*16]` on PC, in all three copies of `_InitGfx`
    (`initgraphics/initgfx.c`, `heap_release/initsystem.c` (the running `game_late_init`), `carcolor/ctx_initsystem.c`);
  - the text display-list double buffer (`D_80124FE8[2][2400]`, cursor `D_80149438`) is `pc_text_dl` (defined in
    `gfx_modes/init.c`, `sound_init`); `audio_start` and `func_8008A644`, m2c code that wrote raw words through an
    `s32` cursor with `+ 8`, have PC versions with host `Gfx`;
  - overlay A's track-map lists (5 commands a section plus 7) are sized in host bytes (`MAP_DL_*_BYTES`).
  `sizeof(Gfx)`, `gfx++` and `p - q` arithmetic needed nothing.
- **ROM display lists stay N64 form** (8-byte big-endian commands, 32-bit addresses) in registered BE ranges. The
  game's in-place walkers read and write them big-endian on PC and follow N64 addresses through `pc_n64_ptr`:
  `entity_lod_select` (0x80096734, relocation and LOD markers), `entity_cull_check` (TXLD rebase; its local `Gfx`
  stays 8 bytes), `display_list_traverse`, `func_8008AD6C` (winding flip), and `func_800966D8` (vertex pass, a
  big-endian PC version). The relocated value is `(w1 + base) & 0xFFFFFF | segment`: with the WAD in the RDRAM window
  `(s32)WADptr` is exact. The context copy of `entity_lod_select` in `audio_frame_sync/group.c` is weak and was left.
  Texture animation (`func_800BD080`, `func_800BDA24`, `camera_smooth_follow`, `race_setup_1`; ASSETS.md §4 #11)
  still needs accessors.
- **Static display lists in data** (`D_8002E4A0`, `D_8002E528`, boot data, `gSPDisplayList` targets) are `Gfx[]`, so
  gen_data gives them host size (272 B, 176 B). Their `w1` are N64 words: an address there must be translated by
  the data loader (B), or the lists left in N64 form and registered BE (C).

## 3. Acmd

There is no audio command list in the game C: no `Acmd`, no `A_*` opcodes, no aspMain ABI. The audio task
(`func_80011910`, `src/rom/lib_11640.c:402`, type 2) carries MusyX-style voice blocks: `t.data_ptr` = an array of
`Block_80012D18 {u16 count, sample; u32 changed; void *work, *loop; Command_80012D18 commands[32];}` (2576 bytes on
the N64) and `t.data_size` is a **block count**, not bytes. Each 0x50-byte `Command_80012D18` holds raw pointers
(`main.first/second`, `loop`, `u.book`, `output`), no segments or physical addresses. The builder
(`func_80012D18`, `func_800139D4`, `func_80013DEC`) and the task are boot C in `lib_11640.c`, which the PC build
replaces (`GAME_REPLACED`), so nothing was widened: the PC audio layer (agent E) owns both sides. Size assumptions to
expect there: 2576, `+= 0x50`, `+ 16`, the 12-byte DMA entries.

## 4. Pointer warnings

| | Total | Mine (`src/blob`, `src/ovl_a`, `src/ovl_b`) | `src/rom` (agent A) |
|---|---:|---:|---:|
| Start (`d28fbb3`) | 836 in 172 files | 758 | 78 |
| Now | 438 in 108 files | 360 | 78 |

What was fixed: the Gfx work; the cast rewrite at pointer -> int -> pointer round trips, address comparisons and
alignment tests; storage that receives game-built pointers (globals `D_80149450` BlitPtrs, `D_801406B8`,
`D_801391F0`, `D_80113ED8`; 45 symbols through `ptr64_decls.py`, among them `D_80149438`, `D_80149820`,
`D_801527C8`; parameters of `control_cell_text2`, `func_800F084C`, `sign_extend_call`; the callers' prototypes of
`camera_clip_planes`, `PS_WriteFile`, `sound_stop`, `sign_extend_call`, `func_80091BA8`, `func_8008B3A0`; locals;
`Node68.p64`). Casts left where the callee ignores the value: `random_float`, `steering_sensitivity`,
`traction_control`, `camera_update_a` (first argument unused). No new warning class appears.

What is left (360):

### 4.1 The 32-bit N64 world: 251, kept on purpose

The heap allocator and its u32-address API (free, purge, size, handles), and the loaders that relocate ROM records
in place. On PC the heap is the RDRAM window at host 0x80000000 (ASSETS.md), so `(u32)` of those addresses is exact
and the casts stay. Files: `ptr64_edit.py` `N64_WORLD` (heap: `audio_heap`, the `alloc_at`/heap-group copies,
`func_80095EF4`, `car_damage_visual`, `deflate_mem` frees, `menu_save_options/ctx_slot`/`ctx_newghost`/`hud_setup`/
`save`, `paksys`, `state_audio`, `resource_alloc_init`; ROM records: `MBSetupWad`, `waves.c`, the text bank
`func_800A4E58` copies, `loadtrack`, the sample base in `car_shadow_render`).
Two things this needs that are not done: the heap's 0x20-byte block header holds three pointer fields (`next`,
`prev`, ...) in several views, which grow on PC; and the handle tables are `u32` slots read as `*(u8 **)`.
Both need `u32` fields under `TARGET_PC` (ASSETS.md §4 #5, #27).

### 4.2 ROM-layout structs: not widened, for Phase 4

Records overlaid on loaded ROM data, whose pointer fields are 32-bit N64 addresses (ASSETS.md §3, §4). They are
declared with pointer types today, so on PC they are larger than the data; Phase 4 declares those fields `u32` under
`TARGET_PC` in every view:

| Record | Where declared (views) | Pointer fields |
|---|---|---|
| WAD TOC entry, handle | `MBSetupWad.c`, `MBGetFromWad.c`, `func_80096BBC.c` | offset, base, TOC |
| `tModelDef`/`tLod` (OBHD) | `waves.c:59`, `particle_knot/group.c:544` (`LodSet`), `runtime_a_c93c_closure/group.c:267` (`MapLod`) | `displayList`, `vtxList`/`verts` |
| `tTexture` (TXHD) | `waves.c:73` and ~25 readers | `texBase`/`image` |
| PLHD | `waves.c:84`, `particle_knot/group.c` (`pal->table`) | `palBase`/`table` |
| `tBasicTargetPath` (PTHD), `Scene36` | `waves.c:91`, `level_objects/group.c:59` | `trigPtr`, `pts`, `parent`, `resources` |
| text bank header `gText` (`D_8017A4E0`) | 17 files, four views (`control_settings`, `runtime_a_text_helpers`, `runtime_a_menu_renderers`, `faeac.c`) | `ptrs0/2/5`, `strings`, `msg`, `hdr->unk360` |
| WHDR entry / `NameEntry` | `loadtrack.c:120`, `wheel_torque_apply.c:28` | `woff`/`node` |
| GTLD keys | `level_objects/group.c:464`, `race_setup_2.c:155` | key -> `Object *` in place |
| font letter (12 B) | `slot_sound/group.c:108` | `ptr` |
| `ResPath`/`AltPath`/`DronePath` | `read_path_data/audio_effect_apply.c` | `pts`, `alt` |
| `SampleRecord` | `include/boot_tail_sample_contract.h` | `data` |

### 4.3 Raw N64 offsets in m2c code: need a typed PC version

m2c output that reaches fields as `*(T *)(p + 0x..)` or `M2C_FIELD(expr, T *, off)`. The warnings mark integers
holding pointers; the offsets themselves are N64 layout and wrong wherever the record has pointers before them.
`UpdateActiveObjects` (Blit `+0x28`: three pointers before it), `menu_confirm_dialog`, `particles_spawn_emitter`,
`listener_position_set`, `func_800F42C8`, `func_80095F8C` and its copy in `pointer_counters/group.c`,
`camera_smooth_lerp` (50 pointer slots as `s32`), `save_load_data` (`AnimateFrame` stored at `+0x274` as `s32`),
`func_800B27E4`, `audio_frame*/group.c` (`+0x28` of a node), and the `M2C_FIELD` users `PhysicsObjectList_Update`,
`entity_flag_check/group.c` (20), `func_8008AE2C`, `func_800949D4`, `func_800A3598`/`35BC`/`35F8`, `func_800A79DC`,
`func_800E56F8/group.c`, `credits_scroll_grp/gr3_e.c`.

### 4.4 Storage still 32-bit, or ambiguous

- `func_8008D870(handle, (s32)tex, -1)` (overlay A menus, `controlscr`): the texture pointer goes into 4-byte slots
  of the 0x44-byte table at `D_8012E700`, read back through `*(s32 *)`. The table has own storage on PC with a
  different layout (`ObjInstance`, 72 B) and other pointer-free views (`Model68`).
- `D_801424F0`, `gTotalNames`, `audio_channel_priority`: slots that hold either a name index or a profile pointer.
- `D_8014A118` (0x4C-byte player records): `.message`, `.object`, `.name` hold pointers as `s32`, through six struct
  views in 38 files.
- `d5e64.c` (`Definition32.object`, `-1` or a pointer), `menu_save_options/group.c` (`(u32)player << 3 | pak` into
  save data), `blast.c` (`camera_collision_check`'s int second argument).

### 4.5 Not mine

- 4 in the overlay loaders (`NextMaxPath((u32)dst, ...)`): agent B's files, not touched.
- 78 in `src/rom`: agent A.

## 5. Findings for the other parts

- **Sign extension.** RDRAM-window addresses have bit 31 set. An `s32` holding one sign-extends to
  `0xFFFFFFFF8xxxxxxx` when converted to a pointer (GCC extends by the integer's signedness), so only `u32` storage
  round-trips. Every explicit `(T *)s32` is a `-Wint-to-pointer-cast`; implicit ones are hidden by
  `-Wno-int-conversion`.
- **Prototype mismatches across files**, which aarch64 punishes and MIPS o32 tolerates: `camera_target_track`'s
  second parameter is `f32` in its owner, but three callers pass a pointer as `s32` (o32 passes both in `a1`; aarch64
  uses `s0` for the float). `sign_extend_call` (MBOX_NewObject) is `void` in its owner while every caller uses its
  result. The m2c contexts declare hundreds of prototypes that disagree with owners. A tool comparing each file's
  prototype with the owner's definition (param classes int/float/pointer, return) would find the rest.
- **gen_data:** its "Pointer conflicts" list now also shows `intptr_t`/`uintptr_t` declarations against pointers.
  Those are benign (same size), and could count as pointer-like. `D_800586D0`/`D_8006A090`/`D_80124FE8` are no
  longer display-list storage on PC.
- **Polygon list copy** (`particle_knot/group.c:1376`, `memcpy` of `tc`/colors from `poly`): `D_8015B268`'s polygons
  look game-built (the game writes their f32 positions), so no swap was added. Any producer that fills them from a
  WAD needs one.

## 6. long

`long` is 32-bit on the N64. There are 56 plain `long` tokens (not 159; that count included `long long`): 24 in
`src/rom` (agent A), the rest here. Under `TARGET_PC` the four zlib deflate copies use `unsigned int` for
`uLong`/`ulg` and `int` for `block_start`, and `zlib_deflate`'s `(long)` casts go through `N64_LONG`.
`D_80124FD0` and `tOrderedList.numElements` are `int`. Left: `include/static_formatter_context.h` (only
`src/rom/lib_34a0.c`, which the PC build replaces) and overlay B's `(unsigned long)&D_8002EB94`, a pointer cast that
is already right on LP64.

## 7. Hard-coded N64 addresses

Outside the overlay loaders, every address literal used as a pointer is an m2c `lui` half plus offset:

| Site | Address | Target | Storage on PC | Done |
|---|---|---|---|---|
| `func_800DDF28` | `0x80150000 - 0x649C + arg0` | `D_80149B64[arg0]` | arena | symbol-relative under `TARGET_PC` |
| `func_800DDEA4` | `0x80150000 - 0x648C + arg0` | `D_80149B74[arg0]` | arena | symbol-relative |
| `func_8008AE2C` | `0x80130000 - 0x18EA + arg0*0x44` | `D_8012E700[arg0]` +0x16 | own (layout differs) | listed (4.3) |
| `func_800A79DC` | `0x80140000 + 0xC06 + arg0*32` | `D_80140BF0[arg0]` +0x16 (Sprite) | own | listed |
| `func_800A35F8`, `func_800A35BC`, `func_800A3598` | `0x80140000 + 0x4035/0x40A8 + arg0*0x304` | `D_80144030[arg0]` +5/+0x78 (PakRecord) | own | listed |

Heap and frame-buffer constants (`0x80400001` heap end, `osMemSize | 0x80000000`, `0x80000300` osTvType) rely on the
RDRAM window and are unchanged.
