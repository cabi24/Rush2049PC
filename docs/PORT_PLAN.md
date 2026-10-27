# Rush 2049 PC: native port plan

Draft, 2026-10-26. A native PC build of *San Francisco Rush 2049* (N64, USA), compiled from the
[decompilation](https://github.com/cabi24/SFRush2049-n64-decomp)'s C, the way the SM64 PC port was built from the
SM64 decompilation. No machine code is translated: the game's C is compiled for the PC, and a PC layer replaces
the N64 hardware and libultra. The player brings their own ROM, which supplies the game's data and assets.

## Where we start

Measured on 2026-10-26 by compiling the published decompilation (`09da2f0`) natively with gcc 12 on aarch64
(`-std=gnu89 -funsigned-char -fno-strict-aliasing -fwrapv -fcommon`):

| | Result |
|---|---|
| C files that compile | **1,342 of 1,346** (game 1,168/1,168; overlay A 39/40; overlay B 30/31; boot 105/107) |
| Why the 4 fail | 2 need a struct type from another file (`MultiBlit`, `Egg`, `tBlitDef`); 2 are libultra/libc files with IDO-only syntax (`__unaligned`, varargs), which the PC layer replaces anyway |
| 64-bit pointer warnings | **948 in 174 files** (661 pointer-to-int casts, 287 int-to-pointer). This is the main list of places that assume 4-byte pointers. |
| Symbols the C defines | 2,328, with no clashes, including between the two overlays that share 0x8038A400 |
| Symbols still undefined after linking everything | **2,393** (below) |

| Undefined | Count | What fills it |
|---|---:|---|
| Zero-initialized data (BSS), boot and game | 895 | Generated definitions with the right size: no ROM bytes needed |
| Initialized data, boot and game | 746 | Generated from the ROM by type (byte order fixed, pointers made into symbol references) |
| Overlay data and BSS | 540 | Same, per overlay |
| Functions with no C yet, game and overlays | 71 | The 19 + 8 near-miss drafts in `nonmatching/`, the 24 eight-byte stubs, the rest written fresh |
| Boot and libultra functions | 78 | The PC layer (threads, messages, video, DMA, controllers, pak, timers), zlib for inflate, libgsm for the GSM kernels |
| Names missing from the symbol file | 48 | Aliases to resolve (`gCamera`, `gControllerData`, ...) |

Other portability facts:

- Code mostly uses struct fields (about 13,600 `->field` accesses; about 45 raw `*(T*)(p + 0x..)` reads), so the
  compiler can redo layouts when pointers grow to 8 bytes. About 1,000 fields are still `unkNN`-style.
- `long` appears 159 times in function bodies. It is 32-bit on the N64 and 64-bit on Linux and macOS: it becomes
  `s32`.
- The game defines its own `main`, `memcpy`, `memset`, `memchr`, `strlen`, `strchr`, `sinf` and `cosf`. They
  are renamed or dropped in favor of the host's.
- Many game function names are "historical labels". Read each file's header comment before trusting a name.

## Architecture

```
Rush2049PC/
  src/  include/  asm/  Makefile   the decompilation, unchanged layout; the N64 matching build still works
  port/                            the PC layer (new)
    main.c                         startup, ROM loading, main loop
    ultra/                         libultra replacement: threads, message queues, VI timing, PI DMA, SI, timers
    gfx/                           display-list interpreter (F3DEX2) + OpenGL backend, SDL window
    audio/                         audio command interpreter (aspMain ABI) + SDL output
    input/  pak/                   SDL controllers and keyboard; .mpk controller pak (from v5's pak.cpp)
    assets/                        loaders that convert the ROM's big-endian files to native structs
  build/pc-data/                   data generated from the user's ROM (git-ignored)
  tools/port/                      census, data generator, import and parity tools
  CMakeLists.txt                   the PC build
```

- **One source tree, two targets** (as sm64-port did): game C stays buildable for the N64, and PC-only changes go
  under `#ifdef TARGET_PC`. The matching N64 build then proves every shared change (shared headers, data turned
  into C, renames) is byte-identical. It needs IDO, so it runs on the x86-64 Rocky host. The PC build runs
  anywhere.
- **Threads first, frame loop later.** The game runs idle, game, audio and render threads plus the libultra
  scheduler, and it busy-waits on variables other threads change (v5 found 14 such loops). Bring-up keeps the
  threads: each N64 thread is a host thread, only one runs at a time, and switches happen in the blocking libultra
  calls, as N64ModernRuntime does. The 14 loops are edited to yield. Folding everything into one frame loop, as
  sm64-port did, can come after the game runs.
- **Display lists with 64-bit pointers.** Under `TARGET_PC`, `Gfx` and the audio command type carry a
  `uintptr_t` second word, so the `gSP*`/`gDP*` macros store host pointers (872 macro uses, 225 raw word writes to
  review). Display lists loaded from ROM files are big-endian with 32-bit segment addresses. Either convert them on
  load, or have the interpreter read them in N64 form and resolve segments to host pointers; the decision belongs
  in Phase 3. The game's own display-list walkers (relocation at 0x80096734 and its neighbors) change with it.
- **Renderer**: a Fast3D-style interpreter (what sm64-port used), not RT64. RT64 needs the game's memory in N64
  layout, which a native build doesn't have. Candidate: libultraship's Fast3D (MIT; used by Ship of Harkinian and
  Starship; F3DEX2, OpenGL/D3D11/Metal, widescreen, interpolation). Check whether it can be used without the rest
  of libultraship before deciding. OpenGL also runs on this Pi 5.
- **Overlays**: both runtime images are linked in. An overlay "load" becomes resetting that overlay's data to its
  initial values and zeroing its BSS.
- **Licenses**: sm64-port and sm64ex have no license, so they are a model, not a source. libultraship is MIT,
  Starship is CC0. v5's `pak.cpp` and `sdl_frontend.cpp` are ours to reuse. Rush 2 Recompiled's code stays out
  (no license); its 2049 research docs describe the asset formats.

## Phases

| Phase | Goal | Done when |
|---|---|---|
| **0. Seed and census** (tonight) | Repo seeded, PC build compiles everything, census tool committed | `cmake --build` compiles 1,346 files into a library; `tools/port/census.py` reproduces the tables above |
| **1. It links** | Every undefined symbol has a definition: BSS generated, data generated from the ROM, missing functions stubbed (abort with the name) | `rush2049pc` links and runs until the first stub |
| **2. It boots** | PC libultra: threads, message queues, VI timer events, DMA from the ROM file, controllers. Busy-waits yield. Missing functions get C. | Game code reaches the first graphics task (`osSpTaskStartGo`) without a fault |
| **3. First picture** | Interpreter + OpenGL + SDL window. 64-bit `Gfx`. Pointer warnings on the boot path fixed. | Midway logo and title screen |
| **4. Assets** | List every ROM file the game loads and its format; a converting loader per format (models, textures, track geometry, collision, AI paths, car tables, fonts) | Front-end menus, then one race |
| **5. Sound** | Audio command interpreter in C (aspMain ABI); the MusyX code is already game C; SDL output | Title music; race sound |
| **6. Saves and input** | `.mpk` controller pak (from v5), SDL controllers and keyboard, rumble | Create Player and records survive a restart |
| **7. Parity with v5** | v5 is the reference: same `--press` input scripts on both; compare state probes (game state, car positions and speeds per frame), screenshots and audio | Every mode v5's scripted runs cover |
| **8. Features** | Widescreen, high frame rate, settings menu, remapping, texture packs: the V6 list, now plain C edits plus our renderer | Per feature, behind a setting |

## Phase 0 results (2026-10-26)

Done: the repository is seeded with the decompilation's history, and `cmake --build build/pc` compiles every
game file natively (about 40 s on the Pi 5) and links a test executable. Live numbers are in
[port/STATUS.md](port/STATUS.md) (`python3 tools/port/census.py --build build/pc`). At the end of the session:
1,346 of 1,346 files compile, 836 pointer warnings in 172 files, **0** symbols with two strong definitions, and
**1,515** symbols still undefined.

What was learned, and how the build handles it:

- **gnu89 is required.** With C99 rules, an `__inline` function without `extern` gets no out-of-line copy; IDO
  (and gnu89) emit one, and the ROM uses it (`audio_effect_process` in `heap_release`). CMake adds no `-std` flag
  when the compiler's default is newer, so the build passes `-std=gnu89` itself.
- **Non-owned copies are made weak.** Group files compile copies of other functions as context for IDO's
  interprocedural optimizer, and some files carry stand-ins; natively, 317 names were defined two to nine times.
  The manifests name each function's owner, so `tools/port/gen_weak.py` writes a per-file header of
  `#pragma weak` lines for the copies (788), and the build force-includes it. The linker then binds every call to
  the owner's definition, and the compiler won't inline a weak copy. The source is untouched. 32 names (mostly
  `__standin_*`, which nothing references) have no manifest owner, and the first copy is kept
  (`build/pc/weak/report.txt`).
- **BSS keeps the N64 layout.** `tools/port/gen_bss.py` defines all 895 boot and game BSS symbols at their N64
  offsets inside one zeroed array per region (assembler `.set` aliases), so code that indexes from one symbol into
  its neighbor still works. The 106 symbols declared with a pointer type get their own storage, since an 8-byte
  pointer would overwrite the next N64 slot. 82 of those are also declared as integers somewhere; they are the
  first places to check in Phase 2. Struct types that contain pointers are not detected yet.
- **The game's libc names are renamed.** `main`, `memcpy`, `memset`, `memchr`, `strlen`, `strchr`, `sinf` and
  `cosf` compile as `n64_*`, so the game keeps its N64 versions and the host's (SDL, GPU drivers) are untouched.
- **Link test.** `tools/port/gen_stubs.py` turns what's still missing into aborting function stubs (159) and zeroed
  data placeholders (1,351), and `rush2049pc_linktest` links and runs. It proves the scheme links; it is not the
  game.
- Two files needed struct definitions IDO didn't (arrays of an incomplete type), added under `#ifdef TARGET_PC`:
  `src/ovl_a/groups/start_screen/group.c` and `src/ovl_b/groups/hud_show/hudextra.c`. Check them with the N64
  matching build on the Rocky host.

## Next session

1. **N64 check**: run `make` on the Rocky host to confirm the two `#ifdef TARGET_PC` edits leave the ROM
   matching.
2. **Initialized data** (746 boot and game symbols): a generator that reads them from your ROM
   (`baserom.us.z64` in the repository root, as the N64 build expects), fixes byte order by declared type, and turns
   N64 pointers into symbol references. It can be checked against the matching build: the same values compiled for
   the N64 must give the ROM's bytes.
3. **Overlay data** (559): decide the overlay model. Both images share 0x8038A400, and an overlay load must restore
   that image's initial data.
4. **The 48 names missing from the symbol file**: resolve them to addresses.
5. **Phase 2 starts**: the PC libultra (threads, message queues, VI timer, PI DMA from the ROM file), replacing the
   boot and libultra stubs one by one.

Tools: cmake 3.25, ninja and gcc 12 are on the Pi. Install `libsdl2-dev` (missing) before Phase 3, and optionally
`clang lld`. zlib 1.2.13 is present.

## Decisions

1. **Seed** from the decompilation's history (proposed) or start a fresh copy.
2. **Keep the N64 matching build working** in this repo (proposed). Shared changes get checked on the Rocky host.
3. **Renderer** (Phase 3): libultraship's Fast3D, or our own interpreter.
4. **License** for `port/` (libultraship is MIT; nothing requires GPL now that N64ModernRuntime isn't used).
5. **Rush2049Recompiled**: v5 becomes the reference for parity testing. Its `V6_PLAN.md` is superseded by this plan.
