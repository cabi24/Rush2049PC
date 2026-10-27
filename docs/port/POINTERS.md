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
  - `tools/port/ptr64_u32fields.py --types T,... FILE...`: records that stay in their N64 layout on PC (the heap
    headers; usable for Phase 4's ROM records). Each pointer field becomes a 4-byte `enum n64p_X` under
    `TARGET_PC`; `N64P(x)` (a `_Generic` cast to the host pointer) and `N64W(x)` (a pointer stored as `u32`) are
    inserted where the compiler rejects a use; both are `x` on the N64. It lists `field +`/`-`/`++` sites, which
    would silently become integer arithmetic, for review.
  - `tools/port/view_audit.py` and `tools/port/view_pads.py`: one host layout for every view of a record, checked
    from DWARF against a reference view, and padding over the reference's pointers split under `TARGET_PC`
    (the car record, [CARS.md](CARS.md)).
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

A changed header is checked through every `src/rom` file. Results:
- this work, against main with the other parts merged (`--base main`): 663 files, 107 code-exact, 556 shifted,
  0 failures. The only typedef names introduced are `uintptr_t`/`intptr_t`;
- the whole branch against `d28fbb3`: 670 files, 18 fail, all from other parts' edits. Thirteen are `TARGET_PC`
  blocks added inside functions (the busy-wait yields, the overlay loaders, `src/rom`): tokens equal, lines
  inside one function moved, which this check refuses by policy. Five yields (`InitWaves.c:95`,
  `replacewave.c:46`, `wheel_params_set.c:56`, `runtime_a_c93c_closure/group.c:884`, `lib_2cf0.c:37`) split a
  one-line `while (c) {}` across lines, which changes which tokens share a line. A same-line form,
  `while (c) { PC_YIELD(); }` with `PC_YIELD()` empty on the N64, would keep both tokens and lines.
  `lib_1050_sdk.c` adds `#if defined(__APPLE__)`, presumably nested inside a `TARGET_PC` block.
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
| Now (census, after merging main `560e8fc`) | 392 in 93 files | 344 | 43 (+5 in `nonmatching/`) |

What was fixed: the Gfx work; the cast rewrite at pointer -> int -> pointer round trips, address comparisons and
alignment tests; storage that receives game-built pointers (globals `D_80149450` BlitPtrs, `D_801406B8`,
`D_801391F0`, `D_80113ED8`; 45 symbols through `ptr64_decls.py`, among them `D_80149438`, `D_80149820`,
`D_801527C8`; parameters of `control_cell_text2`, `func_800F084C`, `sign_extend_call`; the callers' prototypes of
`camera_clip_planes`, `PS_WriteFile`, `sound_stop`, `sign_extend_call`, `func_80091BA8`, `func_8008B3A0`; locals;
`Node68.p64`). Casts left where the callee ignores the value: `random_float`, `steering_sensitivity`,
`traction_control`, `camera_update_a` (first argument unused). No new warning class appears.

What is left (344):

### 4.1 The 32-bit N64 world: 239, kept on purpose

The heap allocator and its u32-address API (free, purge, size, handles), and the loaders that relocate ROM records
in place. On PC the heap is the RDRAM window at host 0x80000000 (ASSETS.md), so `(u32)` of those addresses is exact
and the casts stay. Files: `ptr64_edit.py` `N64_WORLD` (heap: `audio_heap`, the `alloc_at`/heap-group copies,
`func_80095EF4`, `car_damage_visual`, `deflate_mem` frees, `menu_save_options/ctx_slot`/`ctx_newghost`/`hud_setup`/
`save`, `paksys`, `state_audio`, `resource_alloc_init`; ROM records: `MBSetupWad`, `waves.c`, the text bank
`func_800A4E58` copies, `loadtrack`, the sample base in `car_shadow_render`).
The heap's own headers keep their N64 layout on PC: `Block` (0x20), `Heap`, `HandleTable` and the `Pool` view
have their pointer fields as 4-byte `enum n64p_X` window addresses (`ptr64_u32fields.py`) in the running heap
code: `audio_heap`, `counter_guard`, `heap_release/{group,alloc_at,car_damage_visual,func_800CB9D0}.c`,
`func_80095EF4`, `sound_play_menu`, `func_800E7A98`, `heap_max`, `loadcars/heap_total`, `heap_free_total`,
`menu_save_options/ctx_slot`. Because every view agrees on the N64, they agree on PC with no unification, and the
hard-coded 0x20 header and the raw readers (`func_80095F8C`, which now zero-extends what it loads) stay right. The
weak context copies of the heap code were not converted (they never run). Still open: the handle tables are
`u32` slots, and code that dereferences a handle as `*(void **)h` reads 8 bytes (ASSETS.md §4 #5).

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
| `SampleRecord` | `include/boot_tail_sample_contract.h`, `lib_15a10.c` | `data`: done, u32 ([MUSYX_DATA.md](MUSYX_DATA.md)) |

### 4.3 Raw N64 offsets in m2c code: need a typed PC version

m2c output that reaches fields as `*(T *)(p + 0x..)` or `M2C_FIELD(expr, T *, off)`. The warnings mark integers
holding pointers; the offsets themselves are N64 layout and wrong wherever the record has pointers before them.
`particles_spawn_emitter`,
`listener_position_set`, `func_800F42C8`, `func_80095F8C` and its copy in `pointer_counters/group.c`,
`camera_smooth_lerp` (50 pointer slots as `s32`), `audio_frame*/group.c` (`+0x28` of a node), and the `M2C_FIELD`
users `PhysicsObjectList_Update`, `entity_flag_check/group.c` (20), `func_800949D4`, `credits_scroll_grp/gr3_e.c` (a
weak copy). `UpdateActiveObjects`, `menu_confirm_dialog`, `func_8008AE2C`, `func_800A3598`/`35BC`/`35F8` and
`func_800A79DC` are done (section 9). The car record's raw offsets (`save_load_data`, `func_800B27E4`,
`func_800E56F8/group.c` and the others) go through `CAR_OFS` now, and the model record keeps its N64 layout:
[CARS.md](CARS.md).

### 4.4 Storage still 32-bit, or ambiguous

- `func_8008D870(handle, (s32)tex, -1)`: done, section 9.2.
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

- **Widening N64-world storage needs the casts widened too.** An RDRAM-window address stored as `(s32)p` into
  storage widened to `intptr_t` is sign-extended. The original 4-byte store into zeroed 8-byte storage happened to
  read back zero-extended on little-endian. `D_8002EBB0` in `game_late_init` was the one case; it now stores
  `(intptr_t)`.
- **PC-only struct changes must keep pointer fields as pointers.** gen_data infers each symbol's N64 size from the
  PC build's DWARF, with pointers counted as 4 bytes. A PC-only `u8 pad[0xA0]` standing for a native struct reads
  as 0xA0 N64 bytes and stretches the symbol over its neighbors. The frame-record views use `PcScTaskShape`, a
  struct with `OSScTask`'s field sequence, instead.
- **Weak copies can bind to an empty stub.** gen_weak binds every call to the manifest owner. Where the owner is
  a deleted-procedure stub (`func_800A51D8`: `void f(void) {}`) and a group file inlines a real helper under that
  name, the running caller gets the stub. `car_damage_visual`'s helper is renamed under `TARGET_PC`; others may
  exist.

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
| `func_800DDF28` | `0x80150000 - 0x649C + arg0` | `D_80149B64[arg0]` | arena, at its N64 address | symbol-relative under `TARGET_PC` (the literal would work too now) |
| `func_800DDEA4` | `0x80150000 - 0x648C + arg0` | `D_80149B74[arg0]` | arena, at its N64 address | symbol-relative |
| `func_8008AE2C` | `0x80130000 - 0x18EA + arg0*0x44` | `D_8012E700[arg0]` +0x16 | own (layout differs) | PC version (9.2) |
| `func_800A79DC` | `0x80140000 + 0xC06 + arg0*32` | `D_80140BF0[arg0]` +0x16 (Sprite) | own | PC version (9.3) |
| `func_800A35F8`, `func_800A35BC`, `func_800A3598` | `0x80140000 + 0x4035/0x40A8 + arg0*0x304` | `D_80144030[arg0]` +5/+0x78 (PakRecord) | own | PC versions (9.6) |

Heap and frame-buffer constants (`0x80400001` heap end, `osMemSize | 0x80000000`, `0x80000300` osTvType) rely on the
RDRAM window and are unchanged.

## 8. Boot on a blank cartridge (`rush2049pc --no-rom --no-pak --audio null --headless`)

Fixed on the way, in order of the faults:

1. `func_800CCCCC` and the save helpers read `SaveSlot.data` at its N64 offset (44; 48 natively): `SLOT_DATA` in
   the save group and the setting/filename/range helpers (12 files).
2. `func_80095F8C` (the heap that holds an address) walked the heap list through `s32` locals: sign-extended
   window addresses. Then the heap headers themselves, which the views disagreed on natively (section 4.1).
3. `car_damage_visual` (ClearMemory's compaction) called the empty `func_800A51D8` stub for its heap.
4. The frame records: five views of `D_80156BE0` (section 5).
5. Before they faulted: `dma_wait_complete`'s view of the scheduler reads `curRSPTask` at its native offset
   0x2B0; `fcvt_wrapper` (sprintf) takes pointers and formats with a real `va_list`.

Now the boot runs `main`, the threads, `game_init`, `game_late_init` (the heap, `_InitGfx`, `menu_save_options`
with the four default profiles), and `AudioInitSys` through `InitWaves` and the text bank. It stops at the first
MusyX wave load (`LoadWave(6)`, `car_shadow_render.c:84`). That stop is the blank cartridge, not pointer work.
Every wave size comes from the ROM's `gWaveSizes`, so the text bank gets a 0-byte block; its in-place fixups
parse the next block's header as the file and overwrite the heap. With a ROM the block has its size. To go
further without one, the wave loader would need a `--no-rom` guard.

## 9. Shared records: one PC layout per storage (2026-10-27)

Every game file declares its own struct for a record that other files read too. On the N64 those views agree as
long as their N64 offsets agree; on the PC they must agree on the host offsets, which pointer fields move. Byte
pads sized for the N64, `s32` words that hold pointers, and m2c code at raw N64 offsets each put a field somewhere
else, and gen_data gives the storage the size of the view it chooses: twice that was one element, because the
table's extent came from the gap to a name inside element 0. This section covers the front end's and the HUD's
shared records.

### 9.1 The rule, and the check

- **Records the game builds** (BSS, own storage, heap pools): every slot a view types as a pointer is
  pointer-sized on the PC, whatever it holds. The slots hold host pointers (game data, C-defined data such as
  overlay A's `CompLight`, stack addresses, function pointers) as well as RDRAM-window addresses (WAD textures and
  palettes, heap blocks), so a 32-bit slot would hold only some of them. A window address goes into a pointer-sized
  slot zero-extended; it must not pass through an `s32` on the way (section 5). Records loaded from the ROM keep
  their u32 fields (4.2, [LOADERS.md](LOADERS.md)); none of the records below is one. If the data arenas move into
  the RDRAM window, nothing here changes.
- **One layout in every live view.** Each live view gets the record's full typed skeleton under `#ifdef TARGET_PC`
  (or `s32` -> `intptr_t` where that is enough), with the file's own names on the fields it uses. Pointers are typed
  as pointers rather than padded, so gen_data still computes the N64 layout from the DWARF. Weak context copies
  never run and are left as they are.
- **Extents.** A table's size comes from one complete declaration under `TARGET_PC`, which must be the first
  declaration in its file: GCC's DWARF keeps the first declaration's (incomplete) type.
- **m2c code** that adds N64 strides and offsets, or addresses a field of element 0 by its own name
  (`D_8012E708` = node 0's object), gets a whole PC version over the host layout.
- **`tools/port/view_check.py`** checks it from the PC build's DWARF (gen_data's parser and layout rules). It lays
  each view out for the N64 and the host and compares, at every N64 offset, the host offset and whether the slot is
  a pointer:
  - `--sym D_8012E700 [--alias pad_config] [--also D_8012E708,...]` compares the declarations in the objects whose
    live code (strong or local functions, from the relocations) uses the symbol;
  - `--tag Blit,PadConfig --n64-size 0x40 [--in FILE,...]` compares records passed by pointer, by struct tag or
    typedef name; shorter views count as prefixes.

  Byte arrays count as pads. Two reports are benign: pointer-typed pads (the text-bank header views use `void *`
  over its u16 counts) and a u8[4] colour against a u32 one.

### 9.2 Scene nodes: ObjInstanceList `D_8012E700`

600 nodes (0x8012E700-0x80138660), 0x44 bytes on the N64. Before this, the PC storage held one 72-byte node, so
every node after the first that MBOX_NewObject made overwrote the next symbols. The layout is particle_knot's
`Node`, the model draw's view:

| N64 | PC | Field | Holds |
|---:|---:|---|---|
| 0x00 | 0x00 | `u32 flags`, `u32 lights` | |
| 0x08 | 0x08 | `obj` (View/matrix/Camera/Pose) | host pointers into game data |
| 0x0C | 0x10 | `f32 scale, w` | |
| 0x14 | 0x18 | `u16 model`, `s16 child, next, mode` | |
| 0x1C | 0x20 | `Lights2 *light` | overlay A's `CompLight` (C data) |
| 0x20 | 0x28 | `Texture *tex[4]` (LODs) | WAD records (window), any texture |
| 0x30 | 0x48 | `TexRect *rect`, `*clip` | `clip` is set to a stack rectangle by the draw |
| 0x38 | 0x58 | `Palette *pal` | WAD palettes, the car palettes `D_8013FEE0` (game arena) |
| 0x3C | 0x60 | `u8 prim[4]`, `env[4]` | |
| 0x44 | 0x68 | | |

- 50 views made to agree (struct views in 41 files; the rest go through the PC versions). The extent is declared
  in `particle_knot/group.c` and `func_8008E26C.c` (MBOX_NewObject).
- PC versions: MBOX_NewObject (its nine-word clear), ZOID_SetTexture `func_8008D870`, `func_8008D6FC`,
  `func_8008E144`, `func_8008FFD0`, `func_8009002C`, `func_8008AE10`, `func_8008AE2C` (the `0x80130000 - 0x18EA`
  address), `func_800A7884`, `car_mass_set`, `track_collision_edge`, `func_800C15FC` (camera_aspect_ratio), and the
  colour accessors of `func_8010C974`. Symbol macros for node 0's fields: `D_8012E708` (cleanup_colortrace),
  `D_8012E710` (coins), the `TBL` of `func_8008B640`.
- ZOID_SetTexture takes `void *` and keeps the whole pointer. Its 11 views that took `s32` take `intptr_t`, the
  callers' `(s32)` casts are `(intptr_t)`, and the three options-menu files read `func_800B24EC` (the texture
  lookup) as `intptr_t`. `func_8010D85C` declares MBOX_FindPalette_WORK, old-style (the call passes a fifth word),
  with a pointer-sized result. `func_800A79B8` (the light), `func_800A7830` (clip) and `func_800A785C` (palette)
  take `uintptr_t`.
- Test: `assets_zoid`.

### 9.3 Sprites `D_80140BF0` and blits

The **sprite list** (alias `pad_config`) is 200 sprites of 0x20 bytes on the N64 (to 0x801424F0); the PC storage
held 13. The layout is particle_knot's `Sprite`, 0x28 bytes: the palette at 0 and the texture (or the callback,
flags & 0x80) at 8 are pointer-sized, and everything after them moves by 8. The setter `Input_InitPadHandlers`
(both copies) took the two pointers as `u32` and stored them in a `u32` record; four flag/state views were byte
pads (`func_80109A60`, `func_803AE940`, runtime_a_c93c_closure's `ImageMeta`, DelBlit's), and `func_800A79DC` frees
a sprite through the N64 address 0x80140C06. The extent is declared in `particle_knot/group.c` and
`func_800A79F4.c`. 13 views agree.

A **Blit** (sound_control.c's) is 0x68 bytes on the PC:

| N64 | PC | Field |
|---:|---:|---|
| 0x00 | 0x00 | `Name`, `Image` (palette, or the blit itself), `Info` (texture, or the callback) |
| 0x0C | 0x18 | `TexIndex`, `X`, `Y`, `Z`, `Width`, `Height`, `Alpha`, `Flip`, `Hide`, `Init`, `Top`..`Right`, `color` |
| 0x28 | 0x38 | `AnimFunc` |
| 0x2C | 0x40 | `AnimID` |
| 0x30 | 0x48 | `intptr_t AnimDTA`: a value, or the pane group (InitPane) |
| 0x34 | 0x50 | `BLIdx` (the sprite) |
| 0x38 | 0x58 | `data`, `child` |

- `AnimDTA` is pointer-sized because InitPane (`ambient_sound_set`) keeps the pane group there. Its view had the
  group pointer at host 0x48 while the other views read `BLIdx` from 0x48: every pane's sprite index was overwritten.
- InitBlits spaced the 200 blits 64 bytes apart in the 0x3200-byte arena block `D_80146210`; the blits overlapped.
  On the PC the pool is host storage in InitBlits.c, and BlitPtrs (`D_80149450`) was pointer-sized already.
- Views made to agree: the all-`u32` ones (animControls, dust, gr3_73d8, gr3_8154), the byte-pad ones (gr3_6874,
  gr3_a53c, f6b3c, fbc84, func_80106D94, DelBlit's, runtime_a_c93c_closure's, func_803A3DA4), UpdateBlit's
  (`pad_config/group.c`) and `audio_channel_reset`'s, crowd_cheer_play's, and the views that call the `AnimFunc`
  slot `AnimDTA` or `state` and clear it. The `AnimDTA` field of the others is `intptr_t`. 40 views agree.
- `UpdateActiveObjects` (the AnimFunc pass, m2c code at `+0x28`/`+0` through an `s32`) has a PC version; its
  `sound_stop` (DelBlit) view takes `intptr_t`.
- Test: `assets_blit`.

### 9.4 Pane groups `D_80116DE4`

16 `FiveParts` (an active flag, the owner flag, two RGBA colours and five blits), 0x38 bytes on the PC with the
blits pointer-sized from +0x10 (N64: 0x20, blits from +0xC). RemovePane, `sound_handles_clear` (which also stopped
at `D_80116FE4`, a variable of its own on the PC) and ChangePaneColor were m2c code at N64 offsets; they have PC
versions. Their `sound_stop` views take `intptr_t`.

### 9.5 StatLL lists and their elements

The list header (shots.c's `StatLL`) is 40 bytes on the PC: doubly flag, element count and size, then the element
array, the active list and the free list pointer-sized. The views disagreed (24, 32 and 40 bytes; FreeStatLLE's
active and free 8 bytes early behind a 15-byte pad; `pool_linked_list_init`'s active list an `s32`), and InitStatLL
(`struct_fields_init`) and GetStatLLE (`func_8008E3C0`) were m2c code at N64 offsets. Every live header view now
has the layout, InitStatLL and GetStatLLE have PC versions, and the views that left a header 24 bytes of storage
are whole.

The elements are linked through their first pointer, and a doubly linked list's back through the second, so an
element is larger on the PC than the size its initializer passes. Those lists get the host element size
(`ELEMENT_SIZE(n64, type)`, `n64` on the N64) and an element array of that type (own storage):

| List | Elements | N64 | PC | Initializer |
|---|---|---:|---:|---|
| `D_80395EE8`, `D_80396B80` | overlay B trails, fades | 64, 28 | 72, 48 | `func_8038D1A8` |
| `D_8039A520`, `D_8039A5F8`, `D_80394F70`, `D_8039AE80` | shots, hit polys, shot objects, casings | 104, 8, 60, 72 | 120, 16, 72, 80 | `func_803908D0` |
| `D_8013F1E0` | puffs (hud_render) | 88 | 96 | `physics_collision_test` |
| `D_80151AA8` | mine links (a heap block) | 8 | 16 | `camera_lerp_position` |
| `D_80155220` | skid-mark effects (doubly linked) | 36 | 56 | `func_800B0580` |

The skid-mark views (skid_marks' `EffectObj`, `audio_voice_set`'s `Voice`, `func_800B0580`'s `Node`) have a
pointer-sized back link, and skid_marks reads the active list as the header's fourth pointer word on the PC (the
fifth `s32` on the N64). Test: `assets_pool`.

### 9.6 Controller/pak records `D_80144030`

Four 0x304-byte records (paksys' `PakRecord`) with an `OSPfs` at +0x0C, whose `queue` pointer makes the record
0x310 bytes on the PC and moves everything after +0x0C by 4 to 12 bytes (`error`/`FreeSpace` +0x80, the rumble
state +0x88, the file entries +0x90). Thirteen views were byte pads at the N64 size, so `D_80144030[port]` indexed
0x304 apart, and the rumble thread passed `pfs` at +0x0C. Every live view has the layout (an `OSPfs`-shaped member
in each); the free-space and damaged getters (`func_800A3598`/`35BC`/`35F8`, port * 0x304 added to N64 addresses)
have PC versions. 18 views agree. Test: `assets_pak`.

### 9.7 Other front-end storage

- The tDLList clear and init (`func_800A370C`/`func_800A44E8`, strong in `menu_save_options/save.c`) cleared the
  head and tail as two `s32` at +8 and +0xC: the head's two halves on the PC, and the tail at +0x10 was left. The
  `ListHead` view's head and tail are `intptr_t`.
- `hud_setup` cleared the sound handlers' ordered list's root (`D_80143F58`) as an `s32` at +8 (`intptr_t` now).
- Mode select's `SndSet` view of `D_80140420` (`func_800D5524`) read its records 4 bytes early behind `Player84`'s
  definition pointer.
- `menu_confirm_dialog` (clear a save slot) kept the slot's data pointer in an `s32` and passed the buffers to
  `PS_WriteFile` as `u32`.
- `camera_lerp_position` cleared the mine slots `D_80151528` (four target pointers and a count) as five `s32`.

### 9.8 Checks and counts

- `n64_neutral.py --base 09da2f0`: every changed file "shifted", 0 FAIL. ctest: 53 tests (4 new: `assets_zoid`,
  `assets_blit`, `assets_pool`, `assets_pak`; putting the blit stride back to 64 fails `assets_blit`).
- `view_check.py`: 0 live disagreements for every record above.
- proto_check: harmful 27 views of 9 functions before, 2 views of 2 functions after (`func_800E2D18` in
  `func_800E2F00` and `sound_position_set` in `func_8010E828`, race code; PROTOTYPES.md 6).
- Census: 373 pointer warnings before, 334 after; the same two warning classes. No overlay A file has one.
- `rush2049pc --no-rom --headless` stops where it did, after wave 8 in MusyX `sndInit`.

### 9.9 Findings, and what is left

- **The `(char *)-1` sentinel.** port/data turns an N64 pointer value 0xFFFFFFFF into the 64-bit 0x00000000FFFFFFFF
  ("not an RDRAM address: kept"), but the game compares names with `(char *)-1` (all ones). MultiBlit tables mark
  callback entries with a -1 name (sound_control.c, NewMultiBlit), so on the PC such an entry is taken for a
  texture name: InitBlit looks the name up at 0xFFFFFFFF. The loader should sign-extend -1 in pointer fields
  (port/data, not changed here). `assets_blit` uses the value the loader should produce.
- **Colours as words.** Nodes and sprites keep RGBA in a u32 that some writers fill from words and the model draw
  reads as bytes (`prim[4]`). Copies of byte-built colours are fine; the words that came from the ROM are now
  `PC_RGBA`, and colour unions in initialized data are the loader's union rule ([ENDIAN.md](ENDIAN.md) 3.2, 4).
- **Records still disagreeing in race-time code** (`view_check` over all own-storage symbols): the level-object
  kinds `D_80117530` (7 views), car lights and exhaust `D_8017A510` (4), the level-object node pool `D_80143FC8`
  (130 records of 112 bytes, several model views with pointers at different places; its element size is still the
  N64 one), the frame records `D_80156BE0`, the texture-animation frames `D_8011905C` (race_setup_1's first field).
  The player records `D_8014A118`/`input_rec0` and the cars `player_array` are another part's.
- The OSMesgQueue views that declare it without pointers (24 bytes) only take its address; harmless.
