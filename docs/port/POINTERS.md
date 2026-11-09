# 64-bit pointers in the game C

How the game C (`src/blob`, `src/ovl_a`, `src/ovl_b`, `include`) was made 64-bit clean for the PC build, what is
left, and why. 2026-10-26. Related: [ASSETS.md](ASSETS.md) (the 32-bit asset world), [DATA.md](DATA.md) (where data
symbols live on PC), [GFX.md](GFX.md) (the renderer's side).

Until 2026-11-08 the same C also built the N64 ROM, so every PC difference below was made without changing what IDO
compiled: under `#ifdef TARGET_PC` (the N64's tokens in the `#else`), or as file-scope macros whose N64 expansion was
the original tokens, and no line moved inside a function. With the N64 build gone ([PCONLY.md](PCONLY.md)) the PC
branch of every conditional is simply the code (`tools/port/flatten_pc.py`), and where this page says "under
`TARGET_PC`" it describes how a change was first made.

## 1. Mechanism

- **Prelude.** `port/include/pc_prelude.h` is force-included first into every game file (`CMakeLists.txt`,
  `game_flags`). It defines `uintptr_t`/`intptr_t` from `__UINTPTR_TYPE__`/`__INTPTR_TYPE__` (no system headers),
  big-endian accessors `pc_be32_get/set`, `pc_be16_get/set` for N64-form ROM data, and declares
  `pc_n64_ptr`/`pc_n64_addr` (port/ultra).
- **Pointer-sized integers.** `(u32)p` -> `(uintptr_t)p`, `s32 x` -> `intptr_t x` where an integer carries a pointer.
  (The 427 m2c-context files' own `typedef u32 uintptr_t;` were the N64's and are gone.)
- **PC types and macros.** Records that hold pointers have their PC layout; file-scope macros (`DLW(w)`, `TEXT_DL`,
  `N64_LONG`, `N64P`, `N64W`) are the PC's helpers, once paired with an N64 expansion that was the original tokens.
- **Scripts.** The edits were made by `tools/port/ptr64_edit.py`, `ptr64_decls.py`, `ptr64_u32fields.py` and
  `view_pads.py`, which wrote `#ifdef TARGET_PC` blocks; they went with the N64 build (they are in the history).
  `tools/port/view_audit.py` still checks one host layout for every view of a record from DWARF against a reference
  view (the car record, [CARS.md](CARS.md)). `N64P(x)` (a `_Generic` cast to the host pointer) and `N64W(x)` (a
  pointer stored as `u32`) read and write records that stay in their N64 layout on the PC (`enum n64p_X` fields).

### The neutrality check (gone)

Until 2026-11-08 every change to the game C also had to leave the N64 build byte-identical: `tools/port/ido_check.sh`
built every N64 object with IDO at a base and at HEAD, and `tools/port/n64_neutral.py` (in CI) compared the
preprocessed tokens and lines IDO would see. Both went with the N64 build ([PCONLY.md](PCONLY.md)); a change to the
game C is now proven by running the game (PCONLY.md 2).

## 2. Gfx

- Every local `Gfx`/`Gwords` typedef (444 files) carries `w1` as `uintptr_t`: on PC a command is
  `{u32 w0; uintptr_t w1;}`, 16 bytes, the `PcGfxWords` layout. One-line forms (`u32 w0, w1;`) got an
  `#ifdef TARGET_PC` copy.
- Address-carrying GBI macros (`gSetImage`, `gDma1p`, `gDma2p`, `gSPVertex`, `gMoveWd` for `gSPSegment`,
  `gDma1p_`) store `w1` through `(uintptr_t)`. Value macros (other modes, combine, colors, geometry mode) keep
  their 32-bit casts, so the upper half of `w1` is zero for them. The CULLDL branch patch
  (`zoid_draw/group.c` and `resurrect/particle_context.c`, `zoid_branch_slot[-1].words.w1`) stores the full pointer.
- **Contract for the renderer (agent C):** in a native list, `w1` of VTX, DL, BRANCH (E1 before 04/DD), MTX,
  MOVEMEM, MOVEWORD SEGMENT and SETxIMG holds what the game stored: a host pointer, or a 32-bit N64 address the
  game computed (K0 `0x80xxxxxx`, physical from `osVirtualToPhysical`, segmented). Heap and frame buffers are in the
  RDRAM window, so masked addresses of them are fine. For every other command use `(u32)w1`.
- Code that assumed 8-byte commands:
  - the two frame command buffers (`frame_command_buffer_a`/`frame_command_buffer_b`: 0x9CC0 bytes of matrices and vertices, then 4000
    commands) are `pc_frame_dl[2][0x9CC0 + 4000*16]` on PC, in all three copies of `_InitGfx`
    (`initgraphics/initgfx.c`, `heap_release/initsystem.c` (the running `InitSystem`), `SetupCarColor/ctx_initsystem.c`);
  - the text display-list double buffer (`blit_display_lists[2][2400]`, cursor `blit_dl`) is `pc_text_dl` (defined in
    `blit/init.c`, `BlitInit`); `BlitFinish` and `BlitZDist`, m2c code that wrote raw words through an
    `s32` cursor with `+ 8`, have PC versions with host `Gfx`;
  - overlay A's track-map lists (5 commands a section plus 7) are sized in host bytes (`MAP_DL_*_BYTES`).
  `sizeof(Gfx)`, `gfx++` and `p - q` arithmetic needed nothing.
- **ROM display lists stay N64 form** (8-byte big-endian commands, 32-bit addresses) in registered BE ranges. The
  game's in-place walkers read and write them big-endian on PC and follow N64 addresses through `pc_n64_ptr`:
  `relocate_model_dl` (0x80096734, relocation and LOD markers), `relocate_texture_loads` (TXLD rebase; its local `Gfx`
  stays 8 bytes), `display_list_traverse`, `flip_triangle_winding` (winding flip), and `mirror_vertex_texcoords` (vertex pass, a
  big-endian PC version). The relocated value is `(w1 + base) & 0xFFFFFF | segment`: with the WAD in the RDRAM window
  `(s32)WADptr` is exact. The context copy of `relocate_model_dl` in `waves/group.c` is weak and was left.
  Texture animation (`displaylist_set_texture_image`, `displaylist_get_texture_image`, `displaylist_set_tile_size`, `TextureAnimation`; ASSETS.md §4 #11)
  still needs accessors.
- **Static display lists in data** (`rdp_init_dl`, `rsp_init_dl`, boot data, `gSPDisplayList` targets) are `Gfx[]`, so
  gen_data gives them host size (272 B, 176 B). Their `w1` are N64 words: an address there must be translated by
  the data loader (B), or the lists left in N64 form and registered BE (C).

## 3. Acmd

There is no audio command list in the game C: no `Acmd`, no `A_*` opcodes, no aspMain ABI. The audio task
(`sound_hw_submit_task`, `src/rom/sound/hardware.c:402`, type 2) carries MusyX-style voice blocks: `t.data_ptr` = an array of
`SfdBlock {u16 count, sample; u32 changed; void *work, *loop; SfdVoice commands[32];}` (2576 bytes on
the N64) and `t.data_size` is a **block count**, not bytes. Each 0x50-byte `SfdVoice` holds raw pointers
(`main.first/second`, `loop`, `u.book`, `output`), no segments or physical addresses. The builder
(`sound_hw_build_voice_command`, `sound_hw_build_subframe`, `sound_hw_service_frame`) and the task are boot C in `hardware.c`, which the PC build
replaces (`GAME_REPLACED`), so nothing was widened: the PC audio layer (agent E) owns both sides. Size assumptions to
expect there: 2576, `+= 0x50`, `+ 16`, the 12-byte DMA entries.

## 4. Pointer warnings

2026-10-27: what is left is classified site by site in [CASTS.md](CASTS.md) (325 warnings: 191 in weak copies, the
rest window addresses, small integers or unused values, 5 on the input and level-object records, 1 that needs the
ROM), with the fixes of 4.3 and 4.4 and a run-time check of the u32 stores, `RUSH_CHECK_CASTS`.

| | Total | Mine (`src/blob`, `src/ovl_a`, `src/ovl_b`) | `src/rom` (agent A) |
|---|---:|---:|---:|
| Start (`d28fbb3`) | 836 in 172 files | 758 | 78 |
| Now (census, after merging main `560e8fc`) | 392 in 93 files | 344 | 43 (+5 in `nonmatching/`) |

What was fixed: the Gfx work; the cast rewrite at pointer -> int -> pointer round trips, address comparisons and
alignment tests; storage that receives game-built pointers (globals `BlitPtrs` BlitPtrs, `gLights`,
`gEnvList`, `controls_blit`; 45 symbols through `ptr64_decls.py`, among them `blit_dl`, `gFontTextures`,
`gMainHeap`; parameters of `DrawChoice`, `censor`, `MBOX_NewObject`; the callers' prototypes of
`AudUpdateEmitter`, `PS_WriteFile`, `RemoveBlit`, `MBOX_NewObject`, `ConvertTag`, `ZOID_FindMatrix`; locals;
`Node68.p64`). Casts left where the callee ignores the value: `setMiniFBCollisionForce`, `fHalfPipeInterp`,
`fPathInterp`, `WalkBSTInternal` (first argument unused). No new warning class appears.

What is left (344):

### 4.1 The 32-bit N64 world: 239, kept on purpose

The heap allocator and its u32-address API (free, purge, size, handles), and the loaders that relocate ROM records
in place. On PC the heap is the RDRAM window at host 0x80000000 (ASSETS.md), so `(u32)` of those addresses is exact
and the casts stay. Files: `ptr64_edit.py` `N64_WORLD` (heap: `audio_heap`, the `alloc_at`/heap-group copies,
`FindChunk`, `ClearMemoryHeap`, `deflate_mem` frees, `save/ctx_slot`/`ctx_newghost`/`AudioInit`/
`save`, `paksys`, `state_audio`, `musyx_alloc`; ROM records: `MBSetupWad`, `waves.c`, the text bank
`AdjustTextWavePointers` copies, `loadtrack`, the sample base in `AudioInitSys`).
The heap's own headers keep their N64 layout on PC: `Block` (0x20), `Heap`, `HandleTable` and the `Pool` view
have their pointer fields as 4-byte `enum n64p_X` window addresses (`ptr64_u32fields.py`) in the running heap
code: `audio_heap`, `counter_guard`, `heap_release/{group,alloc_at,ClearMemoryHeap,heap_move_block_low}.c`,
`FindChunk`, `AllocateMemoryHeapHigh`, `destroy_heap`, `heap_max`, `loadcars/heap_total`, `heap_free_total`,
`save/ctx_slot`. Because every view agrees on the N64, they agree on PC with no unification, and the
hard-coded 0x20 header and the raw readers (`FindHeap`, which now zero-extends what it loads) stay right. The
weak context copies of the heap code were not converted (they never run). The handle tables were `u32` slots,
and code that dereferences a handle as `*(void **)h` reads 8 bytes (ASSETS.md §4 #5): on the PC the slots are 8
bytes now (9.10).

### 4.2 ROM-layout structs: not widened, for Phase 4

Records overlaid on loaded ROM data, whose pointer fields are 32-bit N64 addresses (ASSETS.md §3, §4). They are
declared with pointer types today, so on PC they are larger than the data; Phase 4 declares those fields `u32` under
`TARGET_PC` in every view:

| Record | Where declared (views) | Pointer fields |
|---|---|---|
| WAD TOC entry, handle | `MBSetupWad.c`, `MBGetFromWad.c`, `MBOffsetWad.c` | offset, base, TOC |
| `tModelDef`/`tLod` (OBHD) | `waves.c:59`, `zoid_draw/group.c:544` (`LodSet`), `trackscr/trackscr.c:267` (`MapLod`) | `displayList`, `vtxList`/`verts` |
| `tTexture` (TXHD) | `waves.c:73` and ~25 readers | `texBase`/`image` |
| PLHD | `waves.c:84`, `zoid_draw/group.c` (`pal->table`) | `palBase`/`table` |
| `tBasicTargetPath` (PTHD), `Scene36` | `waves.c:91`, `targets/group.c:59` | `trigPtr`, `pts`, `parent`, `resources` |
| text bank header `gText` | 17 files, four views (`ShowPakError`, `runtime_a_text_helpers`, `runtime_a_menu_renderers`, `options_anim.c`) | `ptrs0/2/5`, `strings`, `msg`, `hdr->unk360` |
| WHDR entry / `NameEntry` | `loadtrack.c:120`, `NewWorld.c:28` | `woff`/`node` |
| GTLD keys | `targets/group.c:464`, `CheckTargetCollision.c:155` | key -> `Object *` in place |
| font letter (12 B) | `slot_sound/group.c:108` | `ptr` |
| `ResPath`/`AltPath`/`DronePath` | `pathcommon_read/ReadPathData.c` | `pts`, `alt` |
| `SampleRecord` | `include/sound_sample_record.h`, `groups.c` | `data`: done, u32 ([MUSYX_DATA.md](MUSYX_DATA.md)) |

### 4.3 Raw N64 offsets in m2c code: need a typed PC version

m2c output that reaches fields as `*(T *)(p + 0x..)` or `M2C_FIELD(expr, T *, off)`. The warnings mark integers
holding pointers; the offsets themselves are N64 layout and wrong wherever the record has pointers before them.
`SkyUpdate`, `InitBlast` (50 pointer slots as `s32`) and `UpdateEnvirons` are done
(CASTS.md 2); `GetCircuitCode` reads the input records (another part's); `MovePlayerFile` never runs;
`free_sequence/group.c` (20) and `hud_text_anims/gr3_e.c` are weak copies. `DrawBlits`, `ClearPlayerRecords`, `MBOX_GetChild`, `pak_get_free_bytes`/`35BC`/`35F8` and
`ZOID_RemoveBlit` are done (section 9), and so is animBones `animBones`, the HUD's crossbones, whose blit offsets
hid nothing on the PC ([BRINGUP.md](BRINGUP.md) 15). The car record's raw offsets (`StartFrame`, `InitCarVisuals`,
`update_model/group.c` and the others) go through `CAR_OFS` now, and the model record keeps its N64 layout:
[CARS.md](CARS.md).

### 4.4 Storage still 32-bit, or ambiguous

- `ZOID_SetTexture(handle, (s32)tex, -1)`: done, section 9.2.
- `gThisPlayer`, `gTotalNames`, `RemovePlayerScores`: slots that hold either a name index or a profile pointer.
  Done: pointer-sized in every live view (CASTS.md 2.1).
- `gPlayers` (0x4C-byte player records): `.message`, `.object`, `.name` hold pointers as `s32`, through six struct
  views in 38 files.
- `d5e64.c` (`Definition32.object`, `-1` or a pointer), `save/group.c` (`(u32)player << 3 | pak` into
  save data), `blast.c` (`EmitGameSound`'s int second argument): harmless (a dead store, a 32-bit id,
  an argument `AudEmitEffect` never reads; CASTS.md 5.1).

### 4.5 Not mine

- 4 in the overlay loaders (`allocate_memory_at((u32)dst, ...)`): agent B's files, not touched.
- 78 in `src/rom`: agent A.

## 5. Findings for the other parts

- **Sign extension.** RDRAM-window addresses have bit 31 set. An `s32` holding one sign-extends to
  `0xFFFFFFFF8xxxxxxx` when converted to a pointer (GCC extends by the integer's signedness), so only `u32` storage
  round-trips. Every explicit `(T *)s32` is a `-Wint-to-pointer-cast`; implicit ones are hidden by
  `-Wno-int-conversion`.
- **Prototype mismatches across files**, which aarch64 punishes and MIPS o32 tolerates: `AudEmitEffect`'s
  second parameter is `f32` in its owner, but three callers pass a pointer as `s32` (o32 passes both in `a1`; aarch64
  uses `s0` for the float). `MBOX_NewObject` is `void` in its owner while every caller uses its
  result. The m2c contexts declare hundreds of prototypes that disagree with owners. A tool comparing each file's
  prototype with the owner's definition (param classes int/float/pointer, return) would find the rest.
- **gen_data:** its "Pointer conflicts" list now also shows `intptr_t`/`uintptr_t` declarations against pointers.
  Those are benign (same size), and could count as pointer-like. `frame_command_buffer_a`/`frame_command_buffer_b`/`blit_display_lists` are no
  longer display-list storage on PC.
- **Polygon list copy** (`zoid_draw/group.c:1376`, `memcpy` of `tc`/colors from `poly`): `PolyInstanceList`'s polygons
  look game-built (the game writes their f32 positions), so no swap was added. Any producer that fills them from a
  WAD needs one.

- **Widening N64-world storage needs the casts widened too.** An RDRAM-window address stored as `(s32)p` into
  storage widened to `intptr_t` is sign-extended. The original 4-byte store into zeroed 8-byte storage happened to
  read back zero-extended on little-endian. `zbuffer` in `InitSystem` was the one case; it now stores
  `(intptr_t)`.
- **PC-only struct changes must keep pointer fields as pointers.** gen_data infers each symbol's N64 size from the
  PC build's DWARF, with pointers counted as 4 bytes. A PC-only `u8 pad[0xA0]` standing for a native struct reads
  as 0xA0 N64 bytes and stretches the symbol over its neighbors. The frame-record views use `PcScTaskShape`, a
  struct with `OSScTask`'s field sequence, instead.
- **Weak copies can bind to an empty stub.** gen_weak binds every call to the manifest owner. Where the owner is
  a deleted-procedure stub (`ConvertHeap_stub`: `void f(void) {}`) and a group file inlines a real helper under that
  name, the running caller gets the stub. `ClearMemoryHeap`'s helper is renamed under `TARGET_PC`; others may
  exist.

## 6. long

`long` is 32-bit on the N64. There are 56 plain `long` tokens (not 159; that count included `long long`): 24 in
`src/rom` (agent A), the rest here. Under `TARGET_PC` the four zlib deflate copies use `unsigned int` for
`uLong`/`ulg` and `int` for `block_start`, and `zlib_deflate`'s `(long)` casts go through `N64_LONG`.
`skyMatrices` and `tOrderedList.numElements` are `int`. Left: `include/static_formatter_context.h` (only
`src/rom/lib_34a0.c`, which the PC build replaces) and overlay B's `(unsigned long)&gFrameSeconds`, a pointer cast that
is already right on LP64.

## 7. Hard-coded N64 addresses

Outside the overlay loaders, every address literal used as a pointer is an m2c `lui` half plus offset:

| Site | Address | Target | Storage on PC | Done |
|---|---|---|---|---|
| `GetControllerActive` | `0x80150000 - 0x649C + arg0` | `gActiveControllers[arg0]` | arena, at its N64 address | symbol-relative under `TARGET_PC` (the literal would work too now) |
| `GetControllerStickEdges` | `0x80150000 - 0x648C + arg0` | `gStickEdges[arg0]` | arena, at its N64 address | symbol-relative |
| `MBOX_GetChild` | `0x80130000 - 0x18EA + arg0*0x44` | `ObjInstanceList[arg0]` +0x16 | own (layout differs) | PC version (9.2) |
| `ZOID_RemoveBlit` | `0x80140000 + 0xC06 + arg0*32` | `gBlitInstanceList[arg0]` +0x16 (Sprite) | own | PC version (9.3) |
| `PS_PakDamaged`, `PS_GetFreeBlocks`, `pak_get_free_bytes` | `0x80140000 + 0x4035/0x40A8 + arg0*0x304` | `gPaks[arg0]` +5/+0x78 (PakRecord) | own | PC versions (9.6) |

Heap and frame-buffer constants (`0x80400001` heap end, `osMemSize | 0x80000000`, `0x80000300` osTvType) rely on the
RDRAM window and are unchanged.

## 8. Boot on a blank cartridge (`rush2049pc --no-rom --no-pak --audio null --headless`)

Fixed on the way, in order of the faults:

1. `InitPlayer` and the save helpers read `SaveSlot.data` at its N64 offset (44; 48 natively): `SLOT_DATA` in
   the save group and the setting/filename/range helpers (12 files).
2. `FindHeap` (the heap that holds an address) walked the heap list through `s32` locals: sign-extended
   window addresses. Then the heap headers themselves, which the views disagreed on natively (section 4.1).
3. `ClearMemoryHeap` (ClearMemory's compaction) called the empty `ConvertHeap_stub` stub for its heap.
4. The frame records: five views of `frame_records` (section 5).
5. Before they faulted: `audio_retrace_thread_entry`'s view of the scheduler reads `curRSPTask` at its native offset
   0x2B0; `TB_Sprintf` (sprintf) takes pointers and formats with a real `va_list`.

Now the boot runs `main`, the threads, `main_thread_entry`, `InitSystem` (the heap, `_InitGfx`, `SaveInit`
with the four default profiles), and `AudioInitSys` through `InitWaves` and the text bank. It stops at the first
MusyX wave load (`LoadWave(6)`, `AudioInitSys.c:84`). That stop is the blank cartridge, not pointer work.
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
  overlay A's `ComponentLights`, stack addresses, function pointers) as well as RDRAM-window addresses (WAD textures and
  palettes, heap blocks), so a 32-bit slot would hold only some of them. A window address goes into a pointer-sized
  slot zero-extended; it must not pass through an `s32` on the way (section 5). Records loaded from the ROM keep
  their u32 fields (4.2, [LOADERS.md](LOADERS.md)); none of the records below is one. If the data arenas move into
  the RDRAM window, nothing here changes.
- **One layout in every live view.** Each live view gets the record's full typed skeleton (or `s32` -> `intptr_t`
  where that is enough), with the file's own names on the fields it uses. Pointers are typed as pointers rather than
  padded, so gen_data still computes the N64 layout from the DWARF. Weak context copies never run and are left as
  they are.
- **Extents.** A table's size comes from one complete declaration, which must be the first declaration in its file:
  GCC's DWARF keeps the first declaration's (incomplete) type.
- **m2c code** that adds N64 strides and offsets, or addresses a field of element 0 by its own name
  (`ObjInstanceList_mat` = node 0's object), gets a whole PC version over the host layout.
- **`tools/port/view_check.py`** checks it from the PC build's DWARF (gen_data's parser and layout rules). It lays
  each view out for the N64 and the host and compares, at every N64 offset, the host offset and whether the slot is
  a pointer:
  - `--sym ObjInstanceList [--alias gBlitInstanceList] [--also ObjInstanceList_mat,...]` compares the declarations in the objects whose
    live code (strong or local functions, from the relocations) uses the symbol;
  - `--tag Blit,PadConfig --n64-size 0x40 [--in FILE,...]` compares records passed by pointer, by struct tag or
    typedef name; shorter views count as prefixes.

  Byte arrays count as pads. Two reports are benign: pointer-typed pads (the text-bank header views use `void *`
  over its u16 counts) and a u8[4] colour against a u32 one.
- **`tools/port/layout_check.py`** checks the same rule along the code: endian_check's accesses, each put on the
  storage it reaches through pointers, parameters, returns, list walks and slots, so a view whose struct has another
  name meets the others there. It reports views off the storage's reference layout or off each other, raw offsets
  inside a moved field, strides of the N64 size and pointer-width mismatches ([LAYOUT.md](LAYOUT.md); ctest
  `layout_check`).

### 9.2 Scene nodes: ObjInstanceList `ObjInstanceList`

600 nodes (0x8012E700-0x80138660), 0x44 bytes on the N64. Before this, the PC storage held one 72-byte node, so
every node after the first that MBOX_NewObject made overwrote the next symbols. The layout is particle_knot's
`Node`, the model draw's view:

| N64 | PC | Field | Holds |
|---:|---:|---|---|
| 0x00 | 0x00 | `u32 flags`, `u32 lights` | |
| 0x08 | 0x08 | `obj` (View/matrix/Camera/Pose) | host pointers into game data |
| 0x0C | 0x10 | `f32 scale, w` | |
| 0x14 | 0x18 | `u16 model`, `s16 child, next, mode` | |
| 0x1C | 0x20 | `Lights2 *light` | overlay A's `ComponentLights` (C data) |
| 0x20 | 0x28 | `Texture *tex[4]` (LODs) | WAD records (window), any texture |
| 0x30 | 0x48 | `TexRect *rect`, `*clip` | `clip` is set to a stack rectangle by the draw |
| 0x38 | 0x58 | `Palette *pal` | WAD palettes, the car palettes `car_palettes` (game arena) |
| 0x3C | 0x60 | `u8 prim[4]`, `env[4]` | |
| 0x44 | 0x68 | | |

- 50 views made to agree (struct views in 41 files; the rest go through the PC versions). The extent is declared
  in `zoid_draw/group.c` and `ZOID_AddObject.c` (MBOX_NewObject).
- PC versions: MBOX_NewObject (its nine-word clear), ZOID_SetTexture `ZOID_SetTexture`, `ZOID_UpdateObject`,
  `ZOID_LastSibling`, `ZOID_ObjPrevObj`, `ZOID_ObjParent`, `MBOX_GetNext`, `MBOX_GetChild` (the `0x80130000 - 0x18EA`
  address), `ZOID_GetColor`, `ZOID_SetObjectParent`, `zoid_node_visible`, `ZOID_SetObjectDef_TargetPaths` (PlayStartSound), and the
  colour accessors of `AnimateMine`. Symbol macros for node 0's fields: `ObjInstanceList_mat` (cleanup_colortrace),
  `ObjInstanceListAlias` (coins), the `TBL` of `PositionObject`.
- ZOID_SetTexture takes `void *` and keeps the whole pointer. Its 11 views that took `s32` take `intptr_t`, the
  callers' `(s32)` casts are `(intptr_t)`, and the three options-menu files read `MBOX_FindTexture_WORK` (the texture
  lookup) as `intptr_t`. `AnimateFlag` declares MBOX_FindPalette_WORK, old-style (the call passes a fifth word),
  with a pointer-sized result. `ZOID_SetObjLights` (the light), `zoid_set_obj_tile_size` (clip) and `zoid_set_obj_palette` (palette)
  take `uintptr_t`. (Since wave 2c every caller goes through scene.h's one prototype: the textures, matrices and
  lights are pointers, `MBOX_FindPalette_WORK` takes four arguments and returns a `tPalette *`; PROTOTYPES.md 7.)
- Test: `assets_zoid`.

### 9.3 Sprites `gBlitInstanceList` and blits

The **sprite list** (alias `gBlitInstanceList`) is 200 sprites of 0x20 bytes on the N64 (to 0x801424F0); the PC storage
held 13. The layout is particle_knot's `Sprite`, 0x28 bytes: the palette at 0 and the texture (or the callback,
flags & 0x80) at 8 are pointer-sized, and everything after them moves by 8. The setter `ZOID_UpdateBlit`
(both copies) took the two pointers as `u32` and stored them in a `u32` record; four flag/state views were byte
pads (`animDot`, `animArrows`, runtime_a_c93c_closure's `ImageMeta`, DelBlit's), and `ZOID_RemoveBlit` frees
a sprite through the N64 address 0x80140C06. The extent is declared in `zoid_draw/group.c` and
`ZOID_AddBlit.c`. 13 views agree.

A **Blit** (NewMultiBlit.c's) is 0x68 bytes on the PC:

| N64 | PC | Field |
|---:|---:|---|
| 0x00 | 0x00 | `Name`, `Image` (palette, or the blit itself), `Info` (texture, or the callback) |
| 0x0C | 0x18 | `TexIndex`, `X`, `Y`, `Z`, `Width`, `Height`, `Alpha`, `Flip`, `Hide`, `Init`, `Top`..`Right`, `color` |
| 0x28 | 0x38 | `AnimFunc` |
| 0x2C | 0x40 | `AnimID` |
| 0x30 | 0x48 | `intptr_t AnimDTA`: a value, or the pane group (InitPane) |
| 0x34 | 0x50 | `BLIdx` (the sprite) |
| 0x38 | 0x58 | `data`, `child` |

- `AnimDTA` is pointer-sized because InitPane keeps the pane group there. Its view had the
  group pointer at host 0x48 while the other views read `BLIdx` from 0x48: every pane's sprite index was overwritten.
- InitBlits spaced the 200 blits 64 bytes apart in the 0x3200-byte arena block `Blits`; the blits overlapped.
  On the PC the pool is host storage in InitBlits.c, and BlitPtrs was pointer-sized already.
- Views made to agree: the all-`u32` ones (animControls, dust, gr3_73d8, gr3_8154), the byte-pad ones (gr3_6874,
  gr3_a53c, f6b3c, fbc84, animOdometer, DelBlit's, runtime_a_c93c_closure's, carscr_animNewSlider), UpdateBlit's
  (`update_blit/group.c`) and `animControllerDimInternal`'s, ChangePaneSize's, and the views that call the `AnimFunc`
  slot `AnimDTA` or `state` and clear it. The `AnimDTA` field of the others is `intptr_t`. 40 views agree.
- `DrawBlits` (the AnimFunc pass, m2c code at `+0x28`/`+0` through an `s32`) has a PC version; it calls
  `RemoveBlit` (DelBlit) through blit.h's prototype with the blit (PROTOTYPES.md 7).
- Test: `assets_blit`.

### 9.4 Pane groups `gPaneBlits`

16 `FiveParts` (an active flag, the owner flag, two RGBA colours and five blits), 0x38 bytes on the PC with the
blits pointer-sized from +0x10 (N64: 0x20, blits from +0xC). RemovePane, `RemovePanes` (which also stopped
at `gPasswordAlphabet`, a variable of its own on the PC) and ChangePaneColor were m2c code at N64 offsets; they have PC
versions. They call `RemoveBlit` through blit.h's prototype (PROTOTYPES.md 7).

### 9.5 StatLL lists and their elements

The list header (shots.c's `StatLL`) is 40 bytes on the PC: doubly flag, element count and size, then the element
array, the active list and the free list pointer-sized. The views disagreed (24, 32 and 40 bytes; FreeStatLLE's
active and free 8 bytes early behind a 15-byte pad; `ClearStatLL`'s active list an `s32`), and InitStatLL
(`InitStatLL`) and GetStatLLE were m2c code at N64 offsets. Every live header view now
has the layout, InitStatLL and GetStatLLE have PC versions, and the views that left a header 24 bytes of storage
are whole.

The elements are linked through their first pointer, and a doubly linked list's back through the second, so an
element is larger on the PC than the size its initializer passes. Those lists get the host element size
(`ELEMENT_SIZE(n64, type)`, `n64` on the N64) and an element array of that type (own storage):

| List | Elements | N64 | PC | Initializer |
|---|---|---:|---:|---|
| `gStreakList`, `gStreakPolyList` | overlay B trails, fades | 64, 28 | 72, 48 | `InitStreaks` |
| `gShotList`, `gHitPolyList`, `gShotObjList`, `gShotCaseList` | shots, hit polys, shot objects, casings | 104, 8, 60, 72 | 120, 16, 72, 80 | `ShotInit` |
| `gSmokeList` | puffs (UpdateSmoke) | 88 | 96 | `InitSmoke` |
| `gAlwaysColList` | mine links (a heap block) | 8 | 16 | `initTargetStructs` |
| `gSkidList` | skid-mark effects (doubly linked) | 36 | 56 | `InitSkids` |

The skid-mark views (skid_marks' `EffectObj`, `AnimateSkids`'s `Voice`, `InitSkids`'s `Node`) have a
pointer-sized back link, and skid_marks reads the active list as the header's fourth pointer word on the PC (the
fifth `s32` on the N64). Test: `assets_pool`.

### 9.6 Controller/pak records `gPaks`

Four 0x304-byte records (paksys' `PakRecord`) with an `OSPfs` at +0x0C, whose `queue` pointer makes the record
0x310 bytes on the PC and moves everything after +0x0C by 4 to 12 bytes (`error`/`FreeSpace` +0x80, the rumble
state +0x88, the file entries +0x90). Thirteen views were byte pads at the N64 size, so `gPaks[port]` indexed
0x304 apart, and the rumble thread passed `pfs` at +0x0C. Every live view has the layout (an `OSPfs`-shaped member
in each); the free-space and damaged getters (`pak_get_free_bytes`/`35BC`/`35F8`, port * 0x304 added to N64 addresses)
have PC versions. 18 views agree. Test: `assets_pak`.

### 9.7 Other front-end storage

- The tDLList clear and init (`ClearLL`/`InitLL`, strong in `save/save.c`) cleared the
  head and tail as two `s32` at +8 and +0xC: the head's two halves on the PC, and the tail at +0x10 was left. The
  `ListHead` view's head and tail are `intptr_t`.
- `AudioInit` cleared the sound handlers' ordered list's root (`gSortedFxTagTree`) as an `s32` at +8 (`intptr_t` now).
- Mode select's `SndSet` view of `D_80140420` (`StopCarSounds`) read its records 4 bytes early behind `Player84`'s
  definition pointer.
- `ClearPlayerRecords` (clear a save slot) kept the slot's data pointer in an `s32` and passed the buffers to
  `PS_WriteFile` as `u32`.
- `initTargetStructs` cleared the mine slots `gPlayerMines` (four target pointers and a count) as five `s32`.

### 9.8 Checks and counts

- `n64_neutral.py --base 09da2f0`: every changed file "shifted", 0 FAIL. ctest: 53 tests (4 new: `assets_zoid`,
  `assets_blit`, `assets_pool`, `assets_pak`; putting the blit stride back to 64 fails `assets_blit`).
- `view_check.py`: 0 live disagreements for every record above.
- proto_check: harmful 27 views of 9 functions before, 2 views of 2 functions after (`enginetorque` in
  `engine` and `RotateUV` in `AnimateKnockdown`, race code; PROTOTYPES.md 6).
- Census: 373 pointer warnings before, 334 after; the same two warning classes. No overlay A file has one.
- `rush2049pc --no-rom --headless` stops where it did, after wave 8 in MusyX `sndInit`.

### 9.9 Findings, and what is left

- **The `(char *)-1` sentinel.** port/data turns an N64 pointer value 0xFFFFFFFF into the 64-bit 0x00000000FFFFFFFF
  ("not an RDRAM address: kept"), but the game compares names with `(char *)-1` (all ones). MultiBlit tables mark
  callback entries with a -1 name (NewMultiBlit.c, NewMultiBlit), so on the PC such an entry is taken for a
  texture name: InitBlit looks the name up at 0xFFFFFFFF. The loader should sign-extend -1 in pointer fields
  (port/data, not changed here). `assets_blit` uses the value the loader should produce.
- **Colours as words.** Nodes and sprites keep RGBA in a u32 that some writers fill from words and the model draw
  reads as bytes (`prim[4]`). Copies of byte-built colours are fine; the words that came from the ROM are now
  `PC_RGBA`, and colour unions in initialized data are the loader's union rule ([ENDIAN.md](ENDIAN.md) 3.2, 4).
- **Records that still disagreed in race-time code**: the level-object kinds `targetInfo`, the ZOID windows
  `ZoidWinInfo` (named here after two of their users, car lights and exhaust), the level-object node pool
  `gTargetList`, the frame records `frame_records`, the texture-animation frames `texture_swap_shared_frames` and the input records
  `gPlayers`/`gPlayers`: all done in section 10. The cars `gGameCar` are [CARS.md](CARS.md)'s.
- The OSMesgQueue views that declare it without pointers (24 bytes) only take its address; harmless.

### 9.10 The save records: pak files, profiles, ghosts

The controller pak's file layer (paksys, MAT3 `paksysdc.c`) keeps a record per file it finds on a pak, and the
save code keeps a record per profile and per ghost that points to its file. Each is reached through a handle
(`T **`), and the lists that hold them (`gPS_Files` `gPS_Files[4]`, the free handles `pak_free_handles`, the profiles
`gPlayerFiles`, the ghosts `gGhostFiles`) are indirect: `InsertLLE` links a record through its
first two words as `{next, prev}` pointers. On the PC the views disagreed on all of them: paksys had the file
record at 96 bytes and its readers at 76, the save code wrote the file functions at raw N64 offsets, and the
profile's views put its file handle where the list keeps `prev`.

| Record | Storage | N64 | PC | On the pak |
|---|---|---:|---:|---|
| pak file record (paksys' `tPS_File`) | `pak_file_records[64]`, handles `pak_file_handles[64]` (own) | 76 | 96 | no: built from the pak's directory |
| profile (save.c's `SaveSlot`, `tPlayerProfile`) | `player_profile_records[4][16]` (own now; was arena), handles `player_profile_handles`; the defaults `gRealJustPlayers[4]`, the ghost players `gRealGhostPlayers` | 48 | 64 | no: its save data is |
| ghost (SaveInit' `Ghost`) | `_NewGhost`'s pool (CASTS.md 2.2), the limbo records `gRealLimboGhosts` (heap, `n * sizeof(Record)`) | 44 | 64 | no |
| ghost header (`GhostHeader`) | a ghost file's first 88 bytes; the race's recording headers `gRealNewGhostHeaders[4]` (arena now; was own) | 88 | 88 | yes |
| save data (`tSaveData`) | the file's data block; the defaults' `gRealJustPlayerData[4]` (arena) | 2064 | 2064 | yes |
| heap handle slot | the heap's handle tables | 4 | 8 | no |

| N64 | PC | pak file record | N64 | PC | profile | N64 | PC | ghost |
|---:|---:|---|---:|---:|---|---:|---:|---|
| 0x00 | 0x00 | `next` | 0x00 | 0x00 | `next` | 0x00 | 0x00 | `ll` |
| 0x04 | 0x08 | `prev` | 0x04 | 0x08 | `prev` | 0x04 | 0x08 | `fileh` |
| 0x08 | 0x10 | `RemoveFile` | 0x08 | 0x10 | `fileh` (NULL for a default) | 0x08 | 0x10 | `track`, `backward`, `name[13]` |
| 0x0C | 0x18 | `MoveFile` | 0x0C | 0x18 | `id[2]` | 0x18 | 0x20 | `playerId[2]` |
| 0x10 | 0x20 | `pak`, `fileNo` | 0x14 | 0x20 | `name[13]` | 0x20 | 0x28 | `finish` |
| 0x12 | 0x22 | `name[35]`, `ext[11]` | 0x21 | 0x2D | `controls[11]` (+0x2B the rumble) | 0x24 | 0x30 | `datah` |
| 0x40 | 0x50 | `size`, `blocks` | 0x2C | 0x38 | `data` (the save data's handle) | 0x28 | 0x38 | `header` |
| 0x48 | 0x58 | `data` (a heap handle) | 0x30 | 0x40 | | 0x2C | 0x40 | |
| 0x4C | 0x60 | | | | | | | |

- **Why these layouts.** Every slot a view types as a pointer holds a host pointer on the PC: the links and the file
  handles are handles in own storage (`pak_file_handles`, `player_profile_handles`, the ghost pool), the file functions are host
  functions, and the data handles are heap handles (window addresses). So the records are larger and everything
  after their first pointer moves, in N64 field order (9.1). The ghost header is the first 88 bytes of a ghost's
  file, written and read whole (`PS_ReadPartialFile(fileh, 0, 88)`, its checksum over +4..+0x44, the samples at
  `header + 1`), so it keeps its N64 layout: its `start`, the samples' buffer, is a `u32` window address in every
  view (`AllocateMemoryHeap` in GhostStart, the data handle's block in `_ReadGhostData`; `N64W` where it is stored,
  a cast where CompressGhost and the replay read it). The save data has no pointers.
- **`pc_prelude.h`**: `PSF_OFS(n)`, `PROFILE_OFS(n)`, `GHOST_OFS(n)` (the PC offset of N64 offset n; the N64 size
  gives the PC size, as `CAR_OFS` does) and `PC_PSFILE_SIZE`, `PC_PROFILE_SIZE`, `PC_GHOST_SIZE`. Game files that use
  one define the N64 side, `#define PSF_OFS(n) n`; `SLOT_DATA` is `PROFILE_OFS(0x2C)` in its eleven files.
- **The file-function table.** `RemoveFile` is `RemovePlayerFile` for profiles and ghosts and `MoveFile` is
  `MovePlayerFile`; CreatePlayer, `_ReadPlayer` (`ReadPlayer`), `_ReadGhostHeader` and SaveGhost set them,
  PS_CreateFile and PS_PakServer clear them, PS_DeleteFile and `_RemovePak` (`RemovePak`) call `RemoveFile`.
  The raw stores at +8 and +12 wrote `RemovePlayerFile` over the record's `prev` link, and the upper half of
  `MovePlayerFile` into `RemoveFile`, which deleting the file then called. `RemovePlayerFile` compared the handle as
  a `u32` with views at N64 offsets; it takes `uintptr_t` and finds a profile's handle at +16, a ghost's at +8.
  `MovePlayerFile` (never called by the game) takes `intptr_t` and uses `PROFILE_OFS`/`GHOST_OFS`.
- **Views made to agree.** The file record: pak_write_cluster's `C96File` (PS_WriteFile, PS_FlushFile, PS_FlushPak:
  its `channel`, `data_size` and `data` were 12, 12 and 8 bytes early), SaveFlush's `VisualBody`, ctx_slot's `Slot`
  (and its ghost-header `f76`, now `HeaderStart`), paksys' `prev`, and 17 views of its first fields (the pak at
  +0x10): controlscr's `PSFile`, `PS_LockFile`/`pak_file_unchanged`'s `Object`, `save_file_pak_ready`'s `Owner`,
  updateHighScoreScreen's `Q`, overlay A's `FileInfo` (4), `Item` (3), `Resource` (2), `Owner`, `ListOwner` and the file
  list's `Record`. The profile: save.c's `SaveSlot` and 28 more 48-byte views whose padding hid the links
  (`view_pads.py --ref save_option/save_option.c --name 0x0=next --name 0x4=prev --name 0x8=fileh`), and by hand
  `RecordGhost`' `Profile`, `RecordRace`'s `ResourceRoot`, the two `filename.c` `X`, `RankScores`'s, state.c's
  and track_texture_load's views with an `s32` handle at +8, controlscr's `Profile`, updateHighScoreScreen's `Player` (2),
  `CheckCameraView`'s `HObject`, the `HN` of steady_camera and `UpdateRearCamera`, overlay A's seven-option `Owner` (2) and
  callers_deep's `ListPayload`. The ghost: `GhostUpdate`'s `ReplayOwner` (the replay read its header 16 bytes
  early) and `ReplayStream`, `SaveGhostInLimbo`'s `Record44` and save.c's limbo `Record`. The ghost header: `start` in
  group.c, ctx_newghost.c, GhostStart.c and `GhostEnd`.
- **Raw offsets** (on existing lines, through the macros): CreatePlayer, `_ReadPlayer`, the check and repair
  (`CheckAndRepairPlayer`, 15 file handles), `InitPlayer`, LoadPlayerData (`ReadPlayerData`), `DeletePlayer`,
  `_ReadGhostHeader`, `_ReadGhostData` (`ReadGhostDataInternal`) and SaveServer in SaveInit' group.c; SaveGhost;
  CompressGhost (its ghost clear: 44 bytes left the PC ghost's data handle stale); GhostStart's clear of a ghost
  player; `ClearPlayerRecords`; PS_ReadFile's size; PurgePlayerData (`PurgePlayerData`, which cleared half of
  the data handle); `SavePlayerRaceType`; `DeleteGhost`; `SavePlayerCarSetting`; `GetCircuitCode` and
  `NewCircuit`; `UpdateModel`'s rumble byte; overlay A's `pakscr_FindFile` (the file number at +0x11).
- **Strides and storage.** group.c viewed `gPS_Files` as `u8[][16]` (the lists are 24 bytes: pak 1's list was
  read inside pak 0's); `UpdateModel` cleared the rumble request at the N64 stride (`PAK_RECORD`, 0x310, 9.6).
  group.c's view of the profile records is the 64-byte `ProfileRec[][16][1]` (a one-element array, so
  `player_profile_records[pak][slot]` is a pointer as the N64's byte array is), which puts them in own storage; `gRealJustPlayers` and
  `gRealGhostPlayers` follow their views (256 bytes each); `gRealNewGhostHeaders` keeps the N64's 352 bytes in the arena.
- **Heap handles** (`audio_heap`): a handle is the address of a `u32` slot, and the game reads the slot through it
  as a pointer (the files' data, the save data, the ghost headers): the read took the next slot's address as its
  upper half (4.1, ASSETS.md 4 #5). On the PC AllocateHandleHeap and the heap's init give the tables 8-byte slots
  holding the block's window address zero-extended; the slot's readers that take a `u32` (the font waves, the
  compaction's owner update, the frees) still find the address in its first four bytes. This covers every handle
  user, not only the save code.
- **On the pak, and in RAM.** On the pak are the files' contents: a profile's `tSaveData` and a ghost's header and
  zipped samples, in their N64 layouts (nothing in them is widened), and the pak's directory, which the Pfs C
  keeps in `gPaks`'s file entries (9.6). The file records, handles, lists, profiles and ghosts live only in
  RAM and are rebuilt from the directory when a pak is inserted. The game writes the save data's words (checksums,
  ids, times, sizes) natively, so the PC's `.mpk` holds them in host order like the Pfs C's own fields; an N64
  pak image still needs a converter that knows these formats (ULTRA.md, controller pak). The profile's id word
  `(player << 3) | pak` takes the low half of a host handle on the PC: it only tells profiles apart.
- **Test:** `assets_save` (`test_save.c`) runs the game's pak and save code on a pak of its own (16 notes in place
  of the SI and the Pfs C, which the selftest's "controller pak" and "pak note round trip" cover), in port 1 so the
  strides count, with the game's heap in a mapped RDRAM window: PS_InitPakSys and the defaults; PS_PakServer;
  CreatePlayer (the profile, the file record and its function table, the save file on the pak); the pak taken out
  (`_RemovePak` through `RemoveFile`) and put back (SaveServer, `_ReadPlayer`, every block's check,
  PurgePlayerData); LoadPlayerData and a car setting written back by SaveFlush; MoveFile; CompressGhost in a ghost
  slot that held another ghost; SaveGhost; `_ReadGhostHeader` and `_ReadGhostData` after a reinsertion; the replay
  playing and recording through the header; PS_DeleteFile of the ghost's and the profile's files through
  `RemoveFile`; and two handles in a row. Putting back any of the 14 views and raw offsets tried, the `gPS_Files`
  stride or the 4-byte slots fails it (a check, a fault or the build), and its static assertions check the three
  offset macros. Not caught: group.c's ghost clear in `_ReadGhostHeader`, which sets
  the bytes it misses anyway. `assets_casts`, `assets_pak` and `assets_input` model the profile and the file record
  on their PC layouts too.
- **Checks.** `view_audit --symbols gRealJustPlayers,player_profile_records,gJustPlayers,gRealGhostPlayers --size 48 --ref
  save_option/save_option.c`: 35 profile views off its layout before, none after (the 48-byte views left are
  matrices and blits). `view_check --sym` `gGhostFiles`: 1 of 4 views disagreed, none now; `gPS_Files`,
  `pak_free_handles`, `gPlayerFiles`, `pak_file_records`, `gPaks`: 0. `n64_neutral.py --base 09da2f0`: 0 FAIL. ctest, the
  selftest (26 of 26, the two pak cases among them) and the ROM-less boot (the usual stop in AudioSfxInit) pass.
- **Needs the ROM, or a real pak:** the strings the save code builds file names from (`player_file_prefix`, `ghost_file_name_format`,
  the extensions; the test uses its own), the pak font `pak_charset`, profiles and ghosts made, loaded and deleted
  through overlay A's screens and a race (GhostStart, the recording, `GhostEnd`, CompressGhost's zip), the
  rumble request with a rumble pak, and a pak image with the game's saves on it (that needs the converter above).
- **Left:** the weak context copies keep their N64 views. DecodeGhost passes the data buffer, a window address, to
  `unzipdata` and `CalcChecksumInternal` as an `s32` (PROTOTYPES.md, reviewed).

## 10. Race-time records (2026-10-27)

The records the race builds and reads every frame that section 9.9 left: the target info table, the level-object
nodes (the targets) and the records they point to, the input records, the ZOID windows, the frame records seen from
inside, and the texture-animation frames. The rule is 9.1's, with [CARS.md](CARS.md)'s refinement: a record keeps
its N64 layout, with u32 window addresses, when everything stored in its pointer fields and everything that stores
a pointer to it is in the RDRAM window (heap blocks, track WAD records, arena data); it gets a pointer-sized PC
layout when it holds host pointers (host functions, own storage, stack).

| Record | N64 | PC | Storage on the PC | Layout, and why |
|---|---:|---:|---|---|
| target info `targetInfo` (122 records) | 0x30 | **0x40** | own (initialized data) | pointer-sized: the start and visual functions are host functions |
| level-object nodes, the targets (130 at `gTargetArray`, pool `gTargetList`) | 0x70 | 0x70 | game arena (BSS), at its N64 address | N64 layout, 32-bit links and state: everything a node holds and everything that holds a node is a window address |
| their category records, the dead-reckoning records `gBBoxModelStructs` | ROM sizes, 0x40 | same | heap | N64 layout: heap blocks with the ROM's sizes (`gTargetStructSizes`) |
| input records `gPlayers` = `gPlayers` (4) | 0x4C | **0x50** | own (BSS) | the profile pointer at +0x48 points into `gJustPlayers` (own storage) |
| ZOID windows `ZoidWinInfo` (4) | 0x48 | **0x50** | own (BSS) | `first` and `bounds` pointer-sized (9.1; the storage already was) |
| frame records `frame_records` (2) | 0x80 | 0xD0 | own (BSS) | the native OSScTask (before this); one view from inside was not |
| texture-animation frames `texture_swap_shared_frames` | 0xC | 0x18 | own (initialized data) | the frame's name is a pointer (the storage already was) |

### 10.1 The target info table `targetInfo`

MAT3's `targetInfo`: one record per level-object kind, found by name (`SetupTarget` compares the place
record's name with the first `strlen` characters of each of the 122). On the PC:

| N64 | PC | Field (the views' names) | |
|---:|---:|---|---|
| 0x00 | 0x00 | `name`, `file` | strings in the game data |
| 0x08 | 0x10 | `callback`, `start`, `onHit` | the start function: `SetTargetDef` (flags 4, 0x4000) and `CheckTargetCollision` (a hit) call it with the node: `StartCone`, `StartWindow`/`StartKnockdown`, `StartFence`, `StartCactus`, `StartMine`, `StartCoin` |
| 0x0C | 0x18 | `animation`, `visFunc`, `f12`, `w0C` | the visual's function, which the start functions and `SetTargetDef` put in the visual they grab |
| 0x10 | 0x20 | `kind`, `shape`, `mode` | the collision shape: `CheckTargetCollision` indexes the hit tests `gTargetCollFuncPtrs` by it; 4 is a dead-reckoned object |
| 0x12 | 0x22 | `flags` | |
| 0x14 | 0x24 | `f20`, `id`, `s14` | the texture index; the cameras' period (361, 365) |
| 0x16, 0x17 | 0x26, 0x27 | `category`, `element` | two bytes (the camera group's view had an `s16` there) |
| 0x18 | 0x28 | `value` | |
| 0x1C | 0x2C | `sound`, `f28`, `trackA` | `target_info_start_sounds` is record 0's, placed at +0x2C |
| 0x20-0x2F | 0x30-0x3F | the cameras' sound kind, second track, `s28`, `f2C` | |

`TINFO_OFS(n)` (pc_prelude.h) is the PC offset of N64 offset n. Before: the storage was one 56-byte record (its
extent came from the gap to `target_info_start_sounds`, inside record 0), and 7 of the 8 views put the four pointers in four
places. Now `level_objects` declares the 122 records, every view has the host layout with each field spelled out at
its width (the loader converts the ROM's records by whichever view gen_data chooses, and byte pads would leave the
words big-endian: [ENDIAN.md](ENDIAN.md) 4), the m2c handlers stride by `TINFO_OFS(0x30)` (`StartMine`,
`StartCactus`, `StartFence` as a PC version, `StartCoin`), and the visual's function reaches the visual
whole (`StartCone`'s `Node.f20`, `StartPathTarget`'s `CamNode.w14`, `StartCactus`'s `BNode.stat`: `void *`).

### 10.2 The level-object nodes (the targets), their pool and their records

MAT3's targets are the level-object nodes: `SetupTarget` takes one from the StatLL pool `gTargetList`,
whose 130 elements are the arena block `gTargetArray` (0x8014D280-0x80150B60; `port/data/extents.txt` now gives its
extent), and every target function receives one. A node: link (+0), flags (+4), model key (+8), scene-node handle
(+0xC, an s32), type (+0x10, the info record), matrix and position (+0x14, +0x38), the place's transform (+0x44),
texture or animation fields (+0x50-+0x5A), car or slot (+0x5C), sound (+0x60), state (+0x64), index or owner
(+0x65), age (+0x68) and the state record (+0x6C).

**Why the N64 layout.** What a node holds: the next node (arena); its state record, a heap block of its category
(`gTargetOwnStructPtrs[c]`, `gTargetStructSizes[c]` bytes each), its place record (a track WAD record, heap) or an element of a
static per-kind table (`gTargetStructInfoPtrs[c]`, game data). What holds a node: the visuals' 32-bit `data` (CARS.md 3),
`Animation24.node`, and pointer-sized slots that don't care (the pool header, the GTLD side array `gTargetGroupList`, the
camera list `CollisionTargetPtrs`, the mine links and slots). A host layout (0x78 bytes) would not fit the arena block
(130 x 0x78 > 0x38E0), and a node anywhere else couldn't be held by a visual. Before, `level_objects`' `Node` and the
mine's `Target` had the host layout (next and state 8 bytes), so a 0x78-byte node at a 0x70 stride wrote its state
over the next node's link, while the other views had the N64 one.

- **32-bit links.** The generic StatLL code links elements through pointer-sized words (9.5). A list whose
  elements keep their N64 layout is flagged `n64` in its header's first padding byte, and InitStatLL
  (`ClearStatLL`), GetStatLLE and FreeStatLLE read and write the links
  through `pc_statll_link`/`pc_statll_set_link` (pc_prelude.h). `initTargetStructs` sets the flag before it
  builds the node pool (`PC_ONLY` on the `pool_init` line); InitStatLL clears it.
- **Node views** with `enum n64p_` fields and `N64P()`/`N64W()` (N64W is main's `PC_N64W`): `level_objects`
  (`Node`, `Matrix64`, and the camera controller's scene through `STATE_SCENE`/`SCENE_OF`), `StartCone` (the
  motion), the mine `AnimateMine` (`next`, `data`), the debris `AnimateVelRotGeneric`, the flag `AnimateFlag` (its
  place record's name), the coin `AnimateCoin`, the camera group (`Camera.ctl`, `CamCtl.scene`, `CamScene.link`,
  `keys`, `cur`: about 65 uses, wrapped with `ptr64_u32fields.py`'s error-driven fixer), `GetTargetVelocity`, `TargetCollTrigger`,
  and the dead-reckoning body in `TargetCollBBox`. The m2c functions read the state as a u32 (`StartCoin`,
  `AnimateRotGeneric`, and `AnimateKnockdown` as a PC version, which also passes it to `RotateUV`
  zero-extended: proto_check's last harmful view). The views of `StartWindow`, `StartKnockdown` and
  `CheckTargetCollision` had the N64 layout and no pointers already.
- **Walks of the node list outside `level_objects`** follow the 32-bit link too: `PauseTargets` (every node's
  state 2 and no sound; `NODE_NEXT`), the pause screen's `PauseTargets` (the same, in the nonmatching draft of
  `updatePauseScreen` the PC builds; `TARGET_NEXT`) and battle mode's power-up selection `InitBattlePowerUps` (`NEXT`), which walked
  the active list `gTargetList_head` reading the link 8 bytes wide, with the node's flags at +4 as its upper half. The
  power-ups' `Object` view (`runtime_b_resource_callbacks`: flags, handle, `resource` at +0x50) has the N64 layout,
  and `InitBattlePowerUps`'s heap block of slots holds `count` 16-byte slots (it was sized for 8-byte ones). Test:
  `assets_battle`.
- **The target visuals' views of the visual** padded over `next` or had `data` as a pointer (`AnimateObjSwapGeneric`,
  `AnimateFlag`, `AnimateCoin`): they have the visual's PC layout (CARS.md 3) with `data` a u32.
- **The handle's low half.** The flag, the frame stepper, the debris and the camera views read the node's s32 handle
  (+0xC) as an s16 at +0xE, the low half on the N64 but the high half on a little-endian host (0 for every handle).
  Their PC views read the word. (`AnimateCoin`'s `Id` union came from main's byte-order work at the same time.)
- **The records the nodes point to** keep their N64 sizes: the category records are heap blocks of
  `gTargetStructSizes[c]` bytes (ROM data), and of their views only the camera controller `CamCtl` (72 bytes) has a pointer,
  its scene (a PTHD record of the track WAD, which itself keeps its N64 layout: `link`, `keys`, `cur` u32). The
  dead-reckoning records `gBBoxModelStructs` (64 bytes, a heap block; `UpdateMiniModelStruct` strides by 64) keep their body, game
  data at `BBoxCollInfo + 32 x value`, as a u32, in each view: `level_objects`' `Matrix64`, `TargetCollBBox`'s hit
  test and the collision response `setMiniFBCollisionForce` (`Reckon`, `BODY`). The mine data, the debris physics, the cone's motion, the coin and
  battle mode's `Slot136` have no pointers.
- `CollisionTargetPtrs`, the cameras with key paths, is pointer-sized already (`intptr_t[30]`, PROTOTYPES.md 5).

### 10.3 The input records `gPlayers`

Four records (gPlayers): index, controller (+1), pad words (+4..+0x14), the controls (+0x18), flags (+0x42), a
packet slot (+0x44) and the profile pointer (+0x48, `&gJustPlayers[k]`, own storage). On the PC the record is 80
bytes and nothing before +0x48 moves. 30 of 68 views had the N64 size (padding, an `s32` or a `u32` over +0x48), so
they indexed the records 76 bytes apart: `init_cars`'s set every car's `input` pointer (only car 0's was
its record), `UpdateRushControls`' filled them every frame. Their padding is split under `TARGET_PC` (`view_pads.py`,
26 files; by hand where a named field covered the pointer: `ControlName`, `UpdateModelControls`'s group,
`AudRemoveListener`, `updateTitleScreen`), and the m2c code that adds 76-byte strides uses `INPUT_SIZE`
(`FindPlayerNum`, `ClearRushControls`, `GetCircuitCode`, `NewCircuit`, mode select's `UpdateCarSounds`, the
pause screen's `pause_TitlePane`).

- **+0x44 stays 32 bits.** `StartListeners` keeps a packet there (a heap block from `AudAddListener`, so a window
  address) or -1, `StopListeners` activates it and `UpdateListeners` hands it on. `UpdateListeners`'s view had it
  as a pointer, on top of +0x48. The readers zero-extend it and keep -1 (`OBJECT`, `NODE`, `NO_NODE`); the writer
  stores the window address (`PACKET`).
- **Through the profile.** `NewCircuit` and `GetCircuitCode` read the profile's save-data pointer at
  `SaveSlot.data` as an `s32`, which sign-extended it, and `animRumbleText` the profile handle's first word: they read
  them pointer-sized now (`intptr_t`; CASTS.md 7). The save-data block itself, at +0x6F4, is front-end work.
- `gPlayers_save` (+0x48) is declared as records seen from their profile pointer, `{pointer; the next record's 72
  bytes}`, which is 80 bytes already; `input_rec1` (record 1) is at host +0x50.

### 10.4 The ZOID windows `ZoidWinInfo`

Four 72-byte windows (`ZOID_InitializeWindow`, `ZOID_InitializeWindow`): `first`, `bounds`, the projection (`orthographic`,
the fields of view, their tangents and inverses, the viewport, `near`, `far`, `aspect`), `zoom`, `fog`, the fog
range and colour. The storage had the 80-byte layout; four views padded over the pointers or typed them `s32`
(`ZOID_WindowTo3D`, `ZOID_SetWindow`, `WorldFogDensity`, `animate_arrow`), `ZOID_ProjectWindow`'s `Port`
(`ZOID_ProjectWindow`, through the getter) padded too, and `WorldFogDensity` spaced the windows 72 bytes apart
(`VIEW_SIZE`). The m2c accessors `ZOID_SetViewport` (set `first`, `bounds`), `ZOID_GetWindow` (a window) and
`ZOID_GetFog` (the fog range and colour) have PC versions. `set_window.c` is a weak copy and keeps its view.

### 10.5 Frame records and texture-animation frames

- `ClearScreen` (clear the frame buffer) views the frame records from `frame_color_buffers`, frame 0's colour buffer
  (`frame_records` + 0x7C), as 128-byte records `{buf; ...}`. On the PC a record is 208 bytes, so frame 1's buffer was
  read 136 bytes on: the PC view runs into the next record's task, display list and count. The other three views
  of `frame_records` are arrays of half the `Segs` struct and agree with it (view_check now compares such arrays element
  by element).
- `TextureAnimation` typed the frame's name (`texture_swap_shared_frames`, `InitTextureAnimation`'s `Item12`) as a `u32`; it is a
  pointer. The host offsets were the same.

### 10.6 Tests and checks

- `assets_targets` (`test_targets.c`): the node pool's set-up, the instantiation of a cone and a camera from info
  records 5 and 9 (pool, place record, category records, the camera's controller and scene, its dead-reckoning
  record and body), the per-node set-up and the cone's visual, five start functions and a hit function, the target
  visuals (spin, rotation, frame stepper, the debris freed back into the pool), the collision pass, the camera's
  first frame, its key velocity and trigger, and a dead-reckoned collision. `assets_input` (`test_input.c`): fill,
  clear and hand-over of records 0-3, the controls through the profile pointer, the packet slot, and a save record
  reached from record 2. `assets_windows` (`test_windows.c`): window set-up, fog, the accessors, the fog regions, a
  projection undone by the unprojection in window 2, and the frame-buffer clear of frame 1. `assets_cars` has a
  second human car (its input pointer is its own record); `assets_zoid` and `assets_endian` follow the flag and coin
  views. Reverting any of 34 views, strides or PC versions makes one of them fail (17 for the targets, 10 for the
  input records with `init_cars`'s, 7 for the windows and frames; the targets' on x86-64, the others on
  aarch64).
- `tools/port/view_check.py`: an array of half-records is compared against a struct of two, and a struct that runs
  into the next element (`ClearRushControls`'s and `FindPlayerNum`'s `InputRecord`, which end at record 1's
  controller byte) field by field.
- `n64_neutral.py --base 09da2f0`: every changed file code-exact or shifted, 0 FAIL. `rush2049pc --no-rom` stops
  where it did (AudioSfxInit, `AudioSfxInit` -> `AllocateMemoryHeap` -> `AllocateMemory`).

| | Before | After |
|---|---:|---:|
| `view_check` live views disagreeing: `targetInfo` | 7 of 8 | 0 of 8 |
| `gPlayers`/`gPlayers` (with the names inside) | 25 of 62 | 0 of 62 |
| `ZoidWinInfo` | 4 of 11 | 0 of 11 |
| `frame_records` | 3 of 5 | 0 of 5 |
| `texture_swap_shared_frames` | 1 of 2 | 0 of 2 |
| `view_audit`, input record views | 30 of 68 | 0 of 68 |
| `view_audit`, node views of N64 size 0x70 (reference: `level_objects`) | 7 of 9 off its host layout | all 12 structs of that size on its N64 layout |
| `targetInfo` storage | 56 B (one record) | 7,808 B (122 x 0x40) |
| gen_data pointer conflicts, all / these records | 202 / 41 | 171 / 11 |
| gen_data width conflicts, all / these records | 345 / 21 | 356 / 39 |
| proto_check harmful | 1 view of 1 function | 0 |

The pointer conflicts left on these records are the base addresses declared as integers or bytes by code that has
a PC version (`M2C_UNK`, `u8[]`, `s32`), `intptr_t` against a pointer in the same slot, and the weak `set_window.c`.
Width conflicts went up as for the car: the views name more fields, at different places. The "all" counts of the
last column include main's work merged into this branch (10258ff).

### 10.7 What is left, and what needs the ROM

- **Needs the ROM:** that the static per-kind tables `gTargetStructInfoPtrs[c]` point to pointer-free game data in the arena
  (a node's state then holds their window address; if one were own storage, its u32 would lose the upper half: a
  `RUSH_CHECK_CASTS` build stops there, since `N64W` is `PC_N64W`); that `gTargetStructSizes`'s category sizes are what the
  N64-layout views read (72 for the cameras); that the 122 info records end at `BBoxCollInfo` and their fields have the
  widths given in 10.1 (the loader swaps them so); that the scene, key and place records are the track WAD's; and the
  targets in a real race (cones, mines, coins, flags, the scripted cameras).
- **The weapon icons** (`StartWeaponIcon`, `_AnimateWeaponIcon` = `AnimateWeaponIcon`) were only nonmatching drafts,
  so their info records' functions were NULL on the PC, and `SetTargetDef` calls a record's start function
  without a NULL check when its flag 0x4000 is set. The drafts are in the PC build now, with the car's and the info
  record's PC layouts under `TARGET_PC` (DATA.md section 4); `assets_weapons` runs them.
- **`TextureSwapLists`'s extent** (the per-track object-animation sequences): 1,316 bytes by gap, typed as
  `Sequence20 *[]`, so the unnamed sequence and frame tables after its pointers are converted as pointers. A size for
  the pointer table, and declarations for the tables, would fix it (DATA.md).
- endian_check reports two value conflicts on the car record (`init_view3`'s light-flash bytes at the PC offsets
  0x405/0x406 against words at +0x4C): the PC offsets taken modulo the N64 car stride, a car-record question.
